#include "Hardware.h"
#include "LinkProtocol.h"
#if SERVICE_KEYBOARD_TFT
#include "OpenSmartTft.h"
#else
#include "ShieldKeys.h"
#endif
#include "VirtualLcd.h"
#include "SerialConsole.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <soc/gpio_struct.h>
#else
#include <avr/interrupt.h>
#endif

using namespace ServiceKeyboard;

volatile uint16_t linkFrameCount;
volatile uint16_t linkShortFrames;
volatile uint16_t lcdQueueOverflows;

namespace {
MachineShiftRegisters machineLink; // accessed only by non-nesting ISRs
#if defined(ARDUINO_ARCH_ESP32)
portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;
#endif
constexpr uint8_t queueSize = 64;
volatile uint16_t lcdQueue[queueSize];
volatile uint8_t queueHead;
volatile uint8_t queueTail;
VirtualLcd display;
#if !SERVICE_KEYBOARD_TFT
VirtualKeys keys;
#endif
ConsoleKeys consoleKeys;
ConsoleDisplay consoleDisplay;
const char consoleHelp[] PROGMEM =
    "\r\nService-Tastatur console (115200 baud)\r\n"
    "1 2 3 4 = F1 F2 F3 F4\r\n"
    "W = Hoch / Up   O = OK   A = Links / Left\r\n"
    "S = Runter / Down   D = Rechts / Right\r\n"
    "X = F1+F4   Y = F2+Hoch\r\n"
    "Tap: key (180ms). Hold: + then key. Release: 0, space or -.\r\n"
    "Arrows and terminal F1-F4 also work; CR/LF are ignored.\r\n"
    "? help   T toggle ANSI screen   R redraw\r\n"
    "Mode 1=PROFITECH  2=old keyboard  3=no machine\r\n";
const char *helpPosition = consoleHelp;
bool helpPaused;
#if !SERVICE_KEYBOARD_TFT
ShieldButton stableButton = NoButton;
ShieldButton candidateButton = NoButton;
uint32_t candidateSince;
uint32_t nextRepeat;
uint32_t nextKeySample;
uint32_t overlayUntil;
uint32_t lastLcdWrite;
uint8_t shown[32];
uint8_t scanPosition;
uint8_t cgramPosition;
bool cgramUploading;
int8_t shownCursor = -2;
uint8_t shownControl = 0xff;

#endif
#if SERVICE_KEYBOARD_TFT && !defined(ARDUINO_ARCH_ESP32)
volatile bool enableHigh;
#endif

inline void SERVICE_KEYBOARD_ISR_ATTR outputBit() {
#if defined(ARDUINO_ARCH_ESP32)
    // Atomic write-one registers avoid a read/modify/write of other GPIOs.
    if (machineLink.output.high()) GPIO.out_w1ts = uint32_t(1) << Board::outStPin;
    else GPIO.out_w1tc = uint32_t(1) << Board::outStPin;
#elif SERVICE_KEYBOARD_TFT
    if (machineLink.output.high()) PORTC |= _BV(PC5); // A5 / OutST
    else PORTC &= ~_BV(PC5);
#else
    if (machineLink.output.high()) PORTB |= _BV(PB3); // D11 / OutST
    else PORTB &= ~_BV(PB3);
#endif
}

#if !SERVICE_KEYBOARD_TFT
// The LCD R/W pin is tied low on the standard shield. Each physical write is
// separated by >=50us, with interrupts enabled throughout the enable pulse.
void lcdNibble(uint8_t nibble)
{
#if defined(ARDUINO_ARCH_ESP32)
    digitalWrite(Board::lcdEnablePin, LOW);
    for (uint8_t bit = 0; bit < 4; ++bit)
        digitalWrite(Board::lcdDataPins[bit], (nibble >> bit) & 1);
    delayMicroseconds(1); // RS/data setup before the enable pulse
    digitalWrite(Board::lcdEnablePin, HIGH);
    delayMicroseconds(1);
    digitalWrite(Board::lcdEnablePin, LOW);
    delayMicroseconds(1); // data hold and minimum enable-low time
#else
    // PORTD low nibble contains RX/TX and the two machine interrupts.
    // Preserve it: LCD data are D4..D7, all on PORTD's upper nibble.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        PORTD = (PORTD & 0x0f) | (nibble << 4);
    }
    PORTB |= _BV(PB1); // E = D9
    delayMicroseconds(1);
    PORTB &= ~_BV(PB1);
#endif
}

void lcdWrite(bool data, uint8_t value)
{
#if defined(ARDUINO_ARCH_ESP32)
    digitalWrite(Board::lcdRsPin, data ? HIGH : LOW);
#else
    if (data) PORTB |= _BV(PB0); // RS = D8
    else PORTB &= ~_BV(PB0);
#endif
    lcdNibble(value >> 4);
    lcdNibble(value & 15);
#if defined(ARDUINO_ARCH_ESP32)
    // Match LiquidCrystal's conservative settling time on the faster CPU.
    // Clear and home need much longer than normal commands.
    if (!data && (value == 0x01 || (value & 0xfe) == 0x02))
        delayMicroseconds(2000);
    else
        delayMicroseconds(100);
#else
    delayMicroseconds(Board::lcdStepUs);
#endif
}

bool overlayActive(uint32_t now)
{
    return int32_t(overlayUntil - now) > 0;
}

void buttonAction(ShieldButton button, uint32_t now)
{
    if (button == UpButton || button == DownButton) {
        keys.choose(button == DownButton);
        overlayUntil = now + 900;
    } else if (button == LeftButton || button == RightButton) {
        display.scroll(button == RightButton);
        overlayUntil = 0;
    }
}

void pollKeys(uint32_t now)
{
    if (int32_t(now - nextKeySample) < 0) return;
    nextKeySample = now + 5;
#if defined(ARDUINO_ARCH_ESP32)
    // Use measurements from this board, not another MCU's example ranges.
    const uint16_t adc = analogRead(Board::keyPin);
    uint16_t millivolts = 0;
    const bool wideSample = adc >= 946;
    if (wideSample) {
        // Resolve the saturated Select/idle states on every keypad poll.
        analogSetPinAttenuation(Board::keyPin, ADC_11db);
        analogRead(Board::keyPin);
        millivolts = analogReadMilliVolts(Board::keyPin);
        analogSetPinAttenuation(Board::keyPin, ADC_0db);
        analogRead(Board::keyPin);
    }
    const ShieldButton sample = decodeMeasuredEsp32Button(adc, millivolts);
#else
    const uint16_t adc = analogRead(Board::keyPin);
    const ShieldButton sample = decodeShieldExampleButton(adc);
#endif
    if (sample != candidateButton) {
        candidateButton = sample;
        candidateSince = now;
    }
    if (sample != stableButton && uint32_t(now - candidateSince) >= Board::debounceMs) {
        stableButton = sample;
        buttonAction(sample, now);
        nextRepeat = now + 500;
        if (sample == SelectButton) overlayUntil = 0;
    } else if (sample == stableButton && sample != NoButton && sample != SelectButton
               && int32_t(now - nextRepeat) >= 0) {
        buttonAction(sample, now);
        nextRepeat = now + 160;
    }
}

uint8_t overlayChar(uint8_t pos)
{
    static const char names[11][9] PROGMEM = {
        "F1", "F2", "F3", "F4", "Hoch", "OK", "Links", "Runter", "Rechts",
        "F1+F4", "F2+Hoch"
    };
    static const char prefix[] PROGMEM = "Taste: ";
    if (pos < 7) return pgm_read_byte(&prefix[pos]);
    return pgm_read_byte(&names[keys.selected][pos - 7]);
}

void refreshDisplay(uint32_t now)
{
    if (uint32_t(micros() - lastLcdWrite) < Board::lcdStepUs) return;
    const bool overlay = overlayActive(now);
    if (display.cgramDirty) {
        // Restart after a model change so early rows cannot retain an obsolete
        // value after the last write in a CGRAM burst.
        display.cgramDirty = false;
        cgramPosition = 0;
        cgramUploading = true;
    }
    if (cgramUploading) {
        // Upload at most one CGRAM row per call. Leave serial interrupts enabled.
        lcdWrite(false, 0x40 | cgramPosition);
        lcdWrite(true, display.cgram[cgramPosition]);
        if (++cgramPosition == 64) {
            cgramPosition = 0;
            cgramUploading = false;
        }
        shownCursor = -2;
    } else {
        for (uint8_t i = 0; i < 32; ++i) {
            const uint8_t pos = scanPosition;
            scanPosition = (scanPosition + 1) & 31;
            uint8_t value = overlay && pos < 16 ? overlayChar(pos) : display.visible(pos >> 4, pos & 15);
            if (overlay && pos < 16 && value == 0) value = ' ';
            if (shown[pos] == value) continue;
            shown[pos] = value;
            lcdWrite(false, 0x80 | ((pos >= 16) ? 0x40 | (pos & 15) : pos));
            lcdWrite(true, value);
            shownCursor = -2;
            lastLcdWrite = micros();
            return;
        }
        const int8_t cursor = overlay ? -1 : display.visibleCursor();
        const uint8_t control = 0x0c | ((cursor >= 0) ? display.displayControl & 3 : 0);
        if (shownControl != control) {
            lcdWrite(false, control);
            shownControl = control;
        }
        if (cursor >= 0 && cursor != shownCursor) {
            lcdWrite(false, 0x80 | ((cursor >= 16) ? 0x40 | (cursor & 15) : cursor));
        }
        shownCursor = cursor;
    }
    lastLcdWrite = micros();
}

#endif

void serviceConsole(uint32_t now)
{
    for (uint8_t n = 0; n < 16 && Serial.available(); ++n) {
        const uint8_t c = Serial.read();
        if (helpPaused && c != '\r' && c != '\n') {
            helpPaused = false;
            consoleDisplay.redraw();
        }
        switch (consoleKeys.input(c, now)) {
        case ConsoleKeys::Help:
            helpPosition = consoleHelp;
            helpPaused = consoleDisplay.terminal;
            consoleDisplay.redraw();
            break;
        case ConsoleKeys::Redraw: consoleDisplay.redraw(); break;
        case ConsoleKeys::ToggleTerminal: consoleDisplay.toggle(); break;
        default: break;
        }
    }
    consoleKeys.tick(now);
    if (!helpPosition && !helpPaused)
        consoleDisplay.update(display, machine_mode, hardware_key(), now);
    // Never call a blocking Serial.print: only fill space already in the UART
    // transmit ring and keep each service call short for key/LCD processing.
    for (uint8_t n = 0; n < 16 && Serial.availableForWrite() > 0; ++n) {
        if (consoleDisplay.busy()) {
            Serial.write(consoleDisplay.frame[consoleDisplay.sent++]);
        } else if (helpPosition) {
            const uint8_t c = pgm_read_byte(helpPosition++);
            if (!c) { helpPosition = nullptr; break; }
            Serial.write(c);
        } else break;
    }
}
} // namespace

// ClockIn rising edge clocks both 4094s and U13, whose Q7 changes after the
// edge. The machine samples the previously presented bit at that edge.
#if defined(ARDUINO_ARCH_ESP32)
static void SERVICE_KEYBOARD_ISR_ATTR clockInterrupt()
{
    machineLink.clock(GPIO.in & (uint32_t(1) << Board::inStPin));
    outputBit();
}

static void SERVICE_KEYBOARD_ISR_ATTR enableInterrupt()
{
#else
ISR(INT1_vect)
{
    machineLink.clock(PINB & _BV(PB4)); // D12 / InST; U13 DS is grounded
    outputBit();
}

#if SERVICE_KEYBOARD_TFT
ISR(PCINT2_vect)
{
    const bool high = PIND & _BV(PD5);
    const bool rising = high && !enableHigh;
    enableHigh = high;
    if (!rising) return;
#else
ISR(INT0_vect)
{
#endif
#endif
    uint16_t frame;
    if (!machineLink.input.latch(frame)) ++linkShortFrames;
    ++linkFrameCount;
    // Even a short transfer latches the physical register contents. A reply
    // write pulses U13 /PL; commands without a write must not reload U13.
    uint8_t reply;
    if (firmware_receive(frameGsg4(frame), frameGsg5(frame), reply))
        machineLink.output.load(reply);
    outputBit();
}

void SERVICE_KEYBOARD_ISR_ATTR hardware_lcd_enqueue_isr(bool data, uint8_t value)
{
#if defined(ARDUINO_ARCH_ESP32)
    portENTER_CRITICAL_ISR(&queueMux);
#endif
    const uint8_t next = (queueHead + 1) & (queueSize - 1);
    if (next == queueTail) {
        ++lcdQueueOverflows;
    } else {
        lcdQueue[queueHead] = uint16_t(value) | (data ? 0x100 : 0);
        queueHead = next;
    }
#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL_ISR(&queueMux);
#endif
}

void hardware_begin()
{
    Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
    analogReadResolution(10); // retain the LCD1602 ladder's 0..1023 thresholds
    analogSetAttenuation(ADC_11db);
    pinMode(Board::inStPin, INPUT_PULLUP);
    pinMode(Board::clockPin, INPUT_PULLUP);
    pinMode(Board::enablePin, INPUT_PULLUP);
    digitalWrite(Board::outStPin, LOW);
    pinMode(Board::outStPin, OUTPUT);
#endif
#if SERVICE_KEYBOARD_TFT
    if (Board::shieldEnabled) Tft::begin();
#if !defined(ARDUINO_ARCH_ESP32)
    DDRC |= _BV(PC5); // A5 OutST
    PORTC &= ~_BV(PC5);
    DDRB &= ~_BV(PB4); PORTB |= _BV(PB4); // D12 InST
    DDRD &= ~(_BV(PD3) | _BV(PD5));
    PORTD |= _BV(PD3) | _BV(PD5); // ClockIn, Enable
#endif
#else
    // Initialize the shield without touching D10: some backlight circuits short
    // a driven-high D10 through the backlight transistor base.
    if (Board::shieldEnabled) {
#if defined(ARDUINO_ARCH_ESP32)
        pinMode(Board::lcdRsPin, OUTPUT);
        pinMode(Board::lcdEnablePin, OUTPUT);
        digitalWrite(Board::lcdRsPin, LOW);
        digitalWrite(Board::lcdEnablePin, LOW);
        for (uint8_t pin : Board::lcdDataPins) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, LOW);
        }
        if (Board::keypadEnabled) {
            pinMode(Board::keyPin, INPUT);
            // Put the reported 1.1V idle near full scale on this channel.
            analogSetPinAttenuation(Board::keyPin, ADC_0db);
        }
#else
        DDRD |= 0xf0; // LCD D4..D7
        DDRB |= _BV(PB0) | _BV(PB1); // RS, E
        PORTB &= ~(_BV(PB0) | _BV(PB1));
#endif
    }
#if !defined(ARDUINO_ARCH_ESP32)
    DDRB |= _BV(PB3); // OutST
    DDRB &= ~_BV(PB4);                     // InST
    DDRD &= ~(_BV(PD2) | _BV(PD3));         // Enable, ClockIn
    PORTD |= _BV(PD2) | _BV(PD3);
    PORTB |= _BV(PB4);
    PORTB &= ~_BV(PB3);
#endif
    if (Board::shieldEnabled) {
#if defined(ARDUINO_ARCH_ESP32)
        delay(100); // allow the shield's 5V supply and LCD oscillator to settle
        lcdNibble(3); delay(5);
        lcdNibble(3); delay(5);
        lcdNibble(3); delayMicroseconds(150);
        lcdNibble(2); delayMicroseconds(150);
        lcdWrite(false, 0x28); // 4-bit, two lines
        lcdWrite(false, 0x08); // display off while clearing
        lcdWrite(false, 0x01);
        lcdWrite(false, 0x06);
        lcdWrite(false, 0x0c);
#else
        delay(50);
        lcdNibble(3); delay(5);
        lcdNibble(3); delayMicroseconds(150);
        lcdNibble(3); delayMicroseconds(150);
        lcdNibble(2); delayMicroseconds(150);
        lcdWrite(false, 0x28); // 4-bit, two lines
        lcdWrite(false, 0x0c);
        lcdWrite(false, 0x06);
        lcdWrite(false, 0x01); delay(2);
#endif
    }
    memset(shown, 0xff, sizeof(shown));
#endif
    // Interrupts are enabled by firmware_begin after all protocol state is reset.
}

void hardware_link_begin()
{
#if defined(ARDUINO_ARCH_ESP32)
    detachInterrupt(digitalPinToInterrupt(Board::clockPin));
    detachInterrupt(digitalPinToInterrupt(Board::enablePin));
    machineLink = MachineShiftRegisters();
    outputBit();
    // Both handlers are installed on the Arduino loop core and dispatched by
    // the same GPIO interrupt service, so they cannot run concurrently.
    attachInterrupt(digitalPinToInterrupt(Board::clockPin), clockInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(Board::enablePin), enableInterrupt, RISING);
#else
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        machineLink = MachineShiftRegisters();
        outputBit();
#if SERVICE_KEYBOARD_TFT
        enableHigh = PIND & _BV(PD5);
        EICRA = (EICRA & ~(_BV(ISC10) | _BV(ISC11))) | _BV(ISC10) | _BV(ISC11);
        EIFR = _BV(INTF1);
        PCMSK2 |= _BV(PCINT21); // D5 ENST: pin-change interrupt, rising only
        PCIFR = _BV(PCIF2);
        PCICR |= _BV(PCIE2);
        EIMSK |= _BV(INT1);
#else
        // INT0 rising (ENST/Enable), INT1 rising (ClockIn).
        EICRA = (EICRA & ~(_BV(ISC00) | _BV(ISC01) | _BV(ISC10) | _BV(ISC11)))
              | _BV(ISC00) | _BV(ISC01) | _BV(ISC10) | _BV(ISC11);
        EIFR = _BV(INTF0) | _BV(INTF1);
        EIMSK |= _BV(INT0) | _BV(INT1);
#endif
    }
#endif
}

void hardware_service()
{
    // A single producer (machine ISR), single consumer (foreground) queue.
    // queueHead is published only after its entire 16-bit slot is written.
    for (uint8_t n = 0; n < queueSize; ++n) {
#if defined(ARDUINO_ARCH_ESP32)
        portENTER_CRITICAL(&queueMux);
        const bool empty = queueTail == queueHead;
        const uint16_t item = empty ? 0 : lcdQueue[queueTail];
        if (!empty) queueTail = (queueTail + 1) & (queueSize - 1);
        portEXIT_CRITICAL(&queueMux);
        if (empty) break;
#else
        if (queueTail == queueHead) break;
        const uint16_t item = lcdQueue[queueTail];
        queueTail = (queueTail + 1) & (queueSize - 1);
#endif
        if (item & 0x100) display.data(item);
        else display.command(item);
    }
    const uint32_t now = millis();
#if SERVICE_KEYBOARD_TFT
    if (Board::shieldEnabled && Board::touchEnabled) Tft::poll(display, now);
#else
    if (Board::keypadEnabled) pollKeys(now);
#endif
    serviceConsole(now);
#if SERVICE_KEYBOARD_TFT
    if (Board::shieldEnabled) Tft::refresh(display, now);
#else
    if (Board::shieldEnabled) refreshDisplay(now);
#endif
#if defined(ARDUINO_ARCH_ESP32)
    // Menu handlers may stay in their own loops; allow FreeRTOS idle tasks to
    // run there as well as in loop(), so the task watchdog stays serviced.
    static uint32_t lastYield;
    if (uint32_t(now - lastYield) >= 5) {
        lastYield = now;
        delay(1);
    }
#endif
}

uint8_t hardware_key() {
#if SERVICE_KEYBOARD_TFT
    return consoleKeys.key() ? consoleKeys.key() : Tft::key();
#else
    return consoleKeys.key() ? consoleKeys.key() : keys.held(stableButton);
#endif
}

void hardware_lcd_command(uint8_t command)
{
    hardware_service();
    display.command(command);
}

void hardware_lcd_data(uint8_t value)
{
    display.data(value);
}

uint8_t hardware_lcd_address() { return display.address & 0x7f; }

void hardware_pause(uint16_t ticks)
{
    const uint32_t start = micros();
    const uint32_t duration = uint32_t(ticks) * Board::delayTickUs;
    do { hardware_service(); } while (uint32_t(micros() - start) < duration);
}
