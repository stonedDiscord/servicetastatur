#include "Hardware.h"
#include "LinkProtocol.h"
#if SERVICE_KEYBOARD_TFT
#include "OpenSmartTft.h"
#else
#include "ShieldKeys.h"
#include <LiquidCrystal.h>
#include <stdio.h>
#endif
#include "VirtualLcd.h"
#include "SerialConsole.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <soc/gpio_struct.h>
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
bool keypadDebug;
uint16_t keypadAdc;
uint32_t keypadDebugAt;
LiquidCrystal &physicalLcd()
{
    // Construct after Arduino initializes GPIO/timers, and only with a shield.
    static LiquidCrystal lcd(Board::lcdRsPin, Board::lcdEnablePin,
                             Board::lcdDataPins[0], Board::lcdDataPins[1],
                             Board::lcdDataPins[2], Board::lcdDataPins[3]);
    return lcd;
}
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
#if !SERVICE_KEYBOARD_TFT
    "K toggle keypad ADC diagnostics (0..1023)\r\n"
#endif
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

inline void SERVICE_KEYBOARD_ISR_ATTR outputBit() {
#if defined(ARDUINO_ARCH_ESP32)
    // Atomic write-one registers avoid a read/modify/write of other GPIOs.
    if (machineLink.output.high()) GPIO.out_w1ts = uint32_t(1) << Board::outStPin;
    else GPIO.out_w1tc = uint32_t(1) << Board::outStPin;
#elif defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
#if SERVICE_KEYBOARD_TFT
    GPIOC->BSRR = machineLink.output.high() ? (1UL << 0) : (1UL << 16); // PC0
#else
    GPIOA->BSRR = machineLink.output.high() ? (1UL << 1) : (1UL << 17); // PA1
#endif
#elif defined(ARDUINO_UNOR4_MINIMA)
#if SERVICE_KEYBOARD_TFT
    if (machineLink.output.high()) R_PORT1->POSR = 1U << 0; // A5 / P100
    else R_PORT1->PORR = 1U << 0;
#else
    if (machineLink.output.high()) R_PORT0->POSR = 1U << 0; // A1 / P000
    else R_PORT0->PORR = 1U << 0;
#endif
#else
    digitalWrite(Board::outStPin, machineLink.output.high() ? HIGH : LOW);
#endif
}

#if !SERVICE_KEYBOARD_TFT
// Physical bus timing belongs to LiquidCrystal; refresh stays incremental.
void lcdWrite(bool data, uint8_t value)
{
    if (data) physicalLcd().write(value);
    else physicalLcd().command(value);
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
    const ShieldButton sample = decodeShieldExampleButton(adc, SERVICE_KEYBOARD_SELECT_ADC_MAX);
#endif
    keypadAdc = adc;
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
#if !SERVICE_KEYBOARD_TFT
        if (c == 'k' || c == 'K') {
            keypadDebug = !keypadDebug;
            if (!keypadDebug) consoleDisplay.redraw();
            continue;
        }
#endif
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
    if (!helpPosition && !helpPaused
#if !SERVICE_KEYBOARD_TFT
        && !keypadDebug
#endif
    )
        consoleDisplay.update(display, machine_mode, hardware_key(), now);
#if !SERVICE_KEYBOARD_TFT
    if (keypadDebug && !consoleDisplay.busy() && !helpPosition &&
        uint32_t(now - keypadDebugAt) >= 250) {
        char line[64];
        const int length = snprintf(line, sizeof(line),
            "\r\nADC=%u button=%u key=%u selectMax=%u\r\n",
            unsigned(keypadAdc), unsigned(stableButton), unsigned(hardware_key()),
            unsigned(SERVICE_KEYBOARD_SELECT_ADC_MAX));
        if (length > 0 && length < int(sizeof(line)) && Serial.availableForWrite() >= length) {
            Serial.write(reinterpret_cast<const uint8_t *>(line), length);
            keypadDebugAt = now;
        }
    }
#endif
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
static void SERVICE_KEYBOARD_ISR_ATTR clockInterrupt()
{
#if !defined(ARDUINO_ARCH_ESP32)
    const uint32_t interruptState = __get_PRIMASK();
    __disable_irq();
#endif
#if defined(ARDUINO_ARCH_ESP32)
#if SERVICE_KEYBOARD_TFT
    machineLink.clock(GPIO.in & (uint32_t(1) << Board::inStPin));
#else
    machineLink.clock(GPIO.in1.val & (uint32_t(1) << (Board::inStPin - 32)));
#endif
#elif defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
#if SERVICE_KEYBOARD_TFT
    machineLink.clock(GPIOA->IDR & (1UL << 6)); // PA6
#else
    machineLink.clock(GPIOC->IDR & (1UL << 5)); // PC5
#endif
#elif defined(ARDUINO_UNOR4_MINIMA)
#if SERVICE_KEYBOARD_TFT
    machineLink.clock(R_PORT1->PIDR & (1U << 10)); // D12 / P110
#else
    machineLink.clock(R_PORT1->PIDR & (1U << 1)); // A4 / P101
#endif
#else
    machineLink.clock(digitalRead(Board::inStPin) == HIGH);
#endif
    outputBit();
#if !defined(ARDUINO_ARCH_ESP32)
    __set_PRIMASK(interruptState);
#endif
}

static void SERVICE_KEYBOARD_ISR_ATTR enableInterrupt()
{
#if !defined(ARDUINO_ARCH_ESP32)
    // The machine handlers share state and must not nest on ARM.
    const uint32_t interruptState = __get_PRIMASK();
    __disable_irq();
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
#if !defined(ARDUINO_ARCH_ESP32)
    __set_PRIMASK(interruptState);
#endif
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
    analogReadResolution(10); // retain the LCD1602 ladder's 0..1023 thresholds
#if defined(ARDUINO_ARCH_ESP32)
    analogSetAttenuation(ADC_11db);
    // GPIO34..39 do not provide internal pull-ups.
    const auto inputMode = SERVICE_KEYBOARD_TFT ? INPUT_PULLUP : INPUT;
#else
    const auto inputMode = INPUT_PULLUP;
#endif
    pinMode(Board::inStPin, inputMode);
    pinMode(Board::clockPin, inputMode);
    pinMode(Board::enablePin, inputMode);
    digitalWrite(Board::outStPin, LOW);
    pinMode(Board::outStPin, OUTPUT);
#if SERVICE_KEYBOARD_TFT
    if (Board::shieldEnabled) Tft::begin();
#else
    // The six-pin constructor leaves the shield's D10 backlight untouched.
    if (Board::shieldEnabled) {
        physicalLcd().begin(16, 2);
#if defined(ARDUINO_ARCH_ESP32)
        if (Board::keypadEnabled) {
            pinMode(Board::keyPin, INPUT);
            analogSetPinAttenuation(Board::keyPin, ADC_0db);
        }
#endif
    }
    memset(shown, 0xff, sizeof(shown));
#endif
    // Interrupts are enabled by firmware_begin after all protocol state is reset.
}

void hardware_link_begin()
{
    detachInterrupt(digitalPinToInterrupt(Board::clockPin));
    detachInterrupt(digitalPinToInterrupt(Board::enablePin));
    machineLink = MachineShiftRegisters();
    outputBit();
    // ESP32 dispatches both handlers on the Arduino loop core. ARM handlers
    // use interrupt masking to serialize access to the shift registers.
    attachInterrupt(digitalPinToInterrupt(Board::clockPin), clockInterrupt, RISING);
    attachInterrupt(digitalPinToInterrupt(Board::enablePin), enableInterrupt, RISING);
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
