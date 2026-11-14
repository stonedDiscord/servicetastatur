#include "Hardware.h"
#if SERVICE_KEYBOARD_TFT
#include "OpenSmartTft.h"
#include "TouchControls.h"
#include "TftFont.h"

namespace ServiceKeyboard { namespace Tft {
namespace {
constexpr uint16_t white = 0xffff, cyan = 0x07ff, grey = 0x4208;
constexpr uint8_t columns = 40, cells = columns * 2;
constexpr uint8_t cellWidth = 5, cellHeight = 8, textLeft = 10;
static_assert(textLeft + columns * cellWidth <= TouchControls::screenWidth,
              "Machine text must fit the screen");
uint8_t shown[cells], scan, painted;
TouchInput input;
uint32_t sampled, debugAt;
int8_t oldCursor = -1;
bool oldBlink;
uint8_t oldControl;
uint8_t statusMode = 0xff, statusKey = 0xff;
bool glyphsDirty;
const char labels[13][8] PROGMEM = {
    "F1", "F2", "F3", "F4", "UP", "OK", "LEFT", "DOWN", "RIGHT",
    "F1+F4", "F2+UP", "VIEW <", "VIEW >"
};

// Index order is RD, WR, RS, CS. Preserve the AVR register fast path.
inline void control(uint8_t index, bool high) {
#if defined(ARDUINO_ARCH_ESP32)
    digitalWrite(Board::tftControlPins[index], high ? HIGH : LOW);
#else
    if (high) PORTC |= uint8_t(1 << index);
    else PORTC &= ~uint8_t(1 << index);
#endif
}

// OPEN-SMART 8-bit bus: D0..7 = 8,9,10,11,4,13,6,7.
// Masked atomic writes preserve the machine input/output and interrupt pins.
void bus(uint8_t value) {
#if defined(ARDUINO_ARCH_ESP32)
    for (uint8_t bit = 0; bit < 8; ++bit)
        digitalWrite(Board::tftDataPins[bit], (value >> bit) & 1);
#else
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        PORTB = (PORTB & ~0x2f) | (value & 0x2f);
        PORTD = (PORTD & ~0xd0) | (value & 0xd0);
    }
#endif
    control(1, false); // WR
    control(1, true);
}
void word(uint16_t value) { bus(value >> 8); bus(value); }
void command(uint8_t reg) {
    control(2, false);
    word(reg);
    control(2, true);
}
void reg(uint8_t address, uint16_t value) {
    control(3, false);
    command(address);
    word(value);
    control(3, true);
}
void window(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    // Landscape: exchange GRAM axes; AM makes logical X advance first.
    reg(0x36, y + h - 1); reg(0x37, y);
    reg(0x38, x + w - 1); reg(0x39, x);
    reg(0x20, y); reg(0x21, x);
    control(3, false);
    command(0x22);
}
void fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    window(x, y, w, h);
    uint16_t n = uint16_t(w) * h;
    while (n--) word(color);
    control(3, true);
}
uint8_t fontColumn(uint8_t value, uint8_t col) {
    // HD44780 A00 German letters, block, and a visible fallback.
    if (value == 0xe1) value = 'a';
    if (value == 0xef) value = 'o';
    if (value == 0xf5) value = 'u';
    if (value == 0xe2) value = 's';
    if (value == 0xff) return 0xff;
    if (value < 32 || value > 126) value = '?';
    return pgm_read_byte(&tftFont[(value - 32) * 5 + col]);
}
void label(uint8_t x, uint8_t y, const char *text, uint16_t color) {
    for (uint8_t i = 0; i < 36; ++i) {
        const uint8_t c = pgm_read_byte(text + i);
        if (!c) break;
        window(x + i * 6, y, 6, 8);
        for (uint8_t row = 0; row < 8; ++row)
            for (uint8_t col = 0; col < 6; ++col)
                word(col < 5 && (fontColumn(c, col) & (1 << row)) ? color : 0);
        control(3, true);
    }
}
void button(uint8_t index, bool pressed) {
    const uint8_t x = (index % 4) * TouchControls::width + 2;
    const uint8_t y = TouchControls::top + (index / 4) * TouchControls::height + 2;
    constexpr uint8_t w = TouchControls::width - 4, h = TouchControls::height - 4;
    const uint16_t color = pressed ? white : grey;
    fill(x, y, w, 1, color); fill(x, y + h - 1, w, 1, color);
    fill(x, y, 1, h, color); fill(x + w - 1, y, 1, h, color);
    uint8_t length = strlen_P(labels[index]);
    label(x + (w - length * 6) / 2, y + (h - 8) / 2, labels[index], pressed ? white : cyan);
}
void status(uint8_t mode, uint8_t key) {
    fill(0, 38, TouchControls::screenWidth, 8, 0);
    const char *modeText = mode == 1 ? PSTR("PROFITECH") :
                           mode == 2 ? PSTR("SERVICE") : PSTR("NO MACHINE");
    label(4, 38, modeText, cyan);
    label(76, 38, PSTR("Key:"), grey);
    if (key >= 1 && key <= 9) label(106, 38, labels[key - 1], white);
    else if (key == 16 || key == 17) label(106, 38, labels[key - 7], white);
    else label(106, 38, PSTR("-"), white);
    // Reinit is available before detection; PROFITECH sends the chord over J3.
    if (key == 17 && mode == 1) label(146, 38, PSTR("remote"), grey);
    if (key == 17 && mode != 1 && mode != 2) label(146, 38, PSTR("reinit"), grey);
}
uint8_t sample() {
    // Four-wire resistive panel. ADC pins must be on X- and Y+.
    // Save/restore only touch bits, keeping the interrupt-driven OutST intact.
#if !defined(ARDUINO_ARCH_ESP32)
    const uint8_t pins[] = {Board::touchXp, Board::touchYp, Board::touchXm, Board::touchYm};
    uint8_t dirs[4], levels[4];
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t mask = digitalPinToBitMask(pins[i]);
        dirs[i] = *portModeRegister(digitalPinToPort(pins[i])) & mask;
        levels[i] = *portOutputRegister(digitalPinToPort(pins[i])) & mask;
    }
#endif
    pinMode(Board::touchXp, OUTPUT); digitalWrite(Board::touchXp, LOW);
    pinMode(Board::touchYm, OUTPUT); digitalWrite(Board::touchYm, HIGH);
    pinMode(Board::touchXm, INPUT); digitalWrite(Board::touchXm, LOW);
    pinMode(Board::touchYp, INPUT); digitalWrite(Board::touchYp, LOW);
    delayMicroseconds(10);
    const int16_t z1 = analogRead(Board::touchXm), z2 = analogRead(Board::touchYp);
    const int16_t pressure = 1023 - z2 + z1;
    pinMode(Board::touchYm, INPUT); digitalWrite(Board::touchYm, LOW);
    pinMode(Board::touchXm, OUTPUT); digitalWrite(Board::touchXm, LOW);
    digitalWrite(Board::touchXp, HIGH);
    delayMicroseconds(10);
    analogRead(Board::touchYp);
    const int16_t x1 = analogRead(Board::touchYp), x2 = analogRead(Board::touchYp);
    pinMode(Board::touchXp, INPUT); digitalWrite(Board::touchXp, LOW);
    pinMode(Board::touchXm, INPUT);
    pinMode(Board::touchYp, OUTPUT); digitalWrite(Board::touchYp, HIGH);
    pinMode(Board::touchYm, OUTPUT); digitalWrite(Board::touchYm, LOW);
    delayMicroseconds(10);
    analogRead(Board::touchXm);
    const int16_t y1 = analogRead(Board::touchXm), y2 = analogRead(Board::touchXm);
#if defined(ARDUINO_ARCH_ESP32)
    // These four shared LCD pins are always outputs between samples.
    // Restore their idle levels while CS remains high.
    digitalWrite(Board::touchXp, LOW); pinMode(Board::touchXp, OUTPUT);
    digitalWrite(Board::touchYm, LOW); pinMode(Board::touchYm, OUTPUT);
    digitalWrite(Board::touchYp, HIGH); pinMode(Board::touchYp, OUTPUT);
    digitalWrite(Board::touchXm, HIGH); pinMode(Board::touchXm, OUTPUT);
#else
    for (uint8_t i = 0; i < 4; ++i) {
        const uint8_t port = digitalPinToPort(pins[i]), mask = digitalPinToBitMask(pins[i]);
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            *portOutputRegister(port) = (*portOutputRegister(port) & ~mask) | levels[i];
            *portModeRegister(port) = (*portModeRegister(port) & ~mask) | dirs[i];
        }
    }
#endif
    if (Board::touchDebug && uint32_t(millis() - debugAt) >= 250 && Serial.availableForWrite() >= 48) {
        debugAt = millis();
        Serial.print(F("\r\nTouch x=")); Serial.print((x1 + x2) / 2);
        Serial.print(F(" y=")); Serial.print((y1 + y2) / 2);
        Serial.print(F(" z=")); Serial.println(pressure);
    }
    if (pressure < Board::touchMinPressure || pressure > Board::touchMaxPressure ||
        abs(x1 - x2) > 8 || abs(y1 - y2) > 8) return 0;
    int16_t rawX = (x1 + x2) / 2, rawY = (y1 + y2) / 2;
    if (Board::touchSwapAxes) { const int16_t t = rawX; rawX = rawY; rawY = t; }
    return TouchControls::hit(TouchControls::coordinate(rawX, Board::touchLeft, Board::touchRight, TouchControls::screenWidth),
                              TouchControls::coordinate(rawY, Board::touchTop, Board::touchBottom, TouchControls::screenHeight));
}
} // namespace

void begin() {
#if defined(ARDUINO_ARCH_ESP32)
    for (uint8_t pin : Board::tftDataPins) pinMode(pin, OUTPUT);
    for (uint8_t pin : Board::tftControlPins) {
        digitalWrite(pin, HIGH);
        pinMode(pin, OUTPUT);
    }
    const uint8_t resetPin = Board::tftResetPin;
#else
    DDRB |= 0x2f; DDRD |= 0xd0; DDRC |= 0x0f;
    PORTC |= 0x0f; // RD, WR, RS and CS idle high
    const uint8_t resetPin = 2;
#endif
    pinMode(resetPin, OUTPUT); digitalWrite(resetPin, LOW); delay(10);
    digitalWrite(resetPin, HIGH); delay(50);
    // Initialization sequence follows MCUFRIEND_kbv (FreeBSD license; LICENSE-MCUFRIEND.txt).
    // RM68130 (ID 0x6813), ILI9225-compatible 176x220 register interface.
    for (uint8_t i = 0x10; i <= 0x14; ++i) reg(i, 0);
    delay(40);
    reg(0x11, 0x0018); reg(0x12, 0x6121); reg(0x13, 0x006f);
    reg(0x14, 0x495f); reg(0x10, 0x0800); delay(10);
    reg(0x11, 0x103b); delay(50);
    reg(0x01, 0x001c); reg(0x02, 0x0100); reg(0x03, 0x1038); // 90 degrees
    reg(0x07, 0); reg(0x08, 0x0808); reg(0x0b, 0x1100);
    reg(0x0c, 0); reg(0x0f, 0x0d01); reg(0x15, 0x0020);
    reg(0x30, 0); reg(0x31, 219); reg(0x32, 0); reg(0x33, 0);
    reg(0x34, 219); reg(0x35, 0);
    static const uint16_t gamma[] PROGMEM = {
        0, 0x0808, 0x080a, 0x000a, 0x0a08, 0x0808, 0, 0x0a00, 0x0710, 0x0710
    };
    for (uint8_t i = 0; i < 10; ++i) reg(0x50 + i, pgm_read_word(&gamma[i]));
    reg(0x07, 0x0012); delay(50); reg(0x07, 0x1017);
    fill(0, 0, TouchControls::screenWidth, TouchControls::screenHeight, 0);
    for (uint8_t i = 0; i < 13; ++i) button(i, false);
    memset(shown, 0xff, sizeof(shown));
    glyphsDirty = true;
}
void poll(VirtualLcd &display, uint32_t now) {
    if (uint32_t(now - sampled) < 10) return;
    sampled = now;
    const uint8_t navigation = input.update(sample(), now, Board::debounceMs);
    if (navigation) display.scroll(navigation == 13);
}
uint8_t key() { return TouchControls::key(input.held); }

void refresh(VirtualLcd &display, uint32_t now) {
    const uint8_t mode = machine_mode, currentKey = hardware_key();
    if (statusMode != mode || statusKey != currentKey) {
        statusMode = mode; statusKey = currentKey;
        status(mode, currentKey);
        return;
    }
    if (painted != input.held) {
        if (painted) button(painted - 1, false);
        if (input.held) button(input.held - 1, true);
        painted = input.held;
        return;
    }
    if (display.cgramDirty) {
        display.cgramDirty = false; glyphsDirty = true; scan = 0;
    }
    const int8_t cursor = display.visibleCursor(columns);
    const bool blink = (display.displayControl & 1) && (now % 1000 < 500);
    if (cursor != oldCursor || blink != oldBlink || display.displayControl != oldControl) {
        if (oldCursor >= 0) shown[oldCursor] = display.visible(oldCursor / columns, oldCursor % columns) ^ 0xff;
        if (cursor >= 0) shown[cursor] = display.visible(cursor / columns, cursor % columns) ^ 0xff;
        oldCursor = cursor; oldBlink = blink; oldControl = display.displayControl;
    }
    // One unscaled 5x8 cell per service call; interrupts stay enabled during pixels.
    for (uint8_t n = 0; n < cells; ++n) {
        const uint8_t pos = scan;
        scan = (scan + 1) % cells;
        const uint8_t c = display.visible(pos / columns, pos % columns);
        const bool dirty = glyphsDirty;
        if (!scan) glyphsDirty = false;
        if (shown[pos] == c && !dirty) continue;
        shown[pos] = c;
        window(textLeft + (pos % columns) * cellWidth, 6 + (pos / columns) * 12, cellWidth, cellHeight);
        for (uint8_t y = 0; y < cellHeight; ++y) {
            for (uint8_t x = 0; x < cellWidth; ++x) {
                const uint8_t bits = c < 16 ? 0 : fontColumn(c, x);
                bool pixel = c < 16 ? (display.cgram[(c & 7) * 8 + y] & (1 << (4 - x))) : (bits & (1 << y));
                if (pos == cursor && (blink || ((display.displayControl & 2) && y == cellHeight - 1))) pixel = true;
                word(pixel ? white : 0);
            }
        }
        control(3, true);
        return;
    }
}
} }
#endif
