#include "Hardware.h"
#if SERVICE_KEYBOARD_TFT
#include "OpenSmartTft.h"
#include "TouchControls.h"
#include "TftFont.h"
#include <Arduino_GFX_Library.h>

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

// SWPAR8 handles the data pins. On AVR, digitalWrite protects the shared
// control port from the J3 ISR updating OutST on A5 between read and write.
class ShieldBus : public Arduino_SWPAR8 {
public:
    using Arduino_SWPAR8::Arduino_SWPAR8;
    void beginWrite() override {
        digitalWrite(Board::tftControlPins[3], LOW);
        digitalWrite(Board::tftControlPins[2], HIGH);
    }
    void endWrite() override { digitalWrite(Board::tftControlPins[3], HIGH); }
    void writeCommand(uint8_t value) override { writeCommand16(value); }
    // The parallel RM68130 bus requires a 16-bit register index, even when
    // Arduino_ILI9225 supplies an 8-bit command through its SPI-style API.
    void writeC8D16(uint8_t command, uint16_t value) override {
        writeCommand(command);
        Arduino_SWPAR8::write16(value);
    }
    void writeCommand16(uint16_t value) override {
        digitalWrite(Board::tftControlPins[2], LOW);
        Arduino_SWPAR8::write16(value);
        digitalWrite(Board::tftControlPins[2], HIGH);
    }
    void writeCommandBytes(uint8_t *data, uint32_t length) override {
        digitalWrite(Board::tftControlPins[2], LOW);
        while (length--) Arduino_SWPAR8::write(*data++);
        digitalWrite(Board::tftControlPins[2], HIGH);
    }
};

ShieldBus bus(Board::tftControlPins[2], Board::tftControlPins[3],
                   Board::tftControlPins[1], Board::tftControlPins[0],
                   Board::tftDataPins[0], Board::tftDataPins[1],
                   Board::tftDataPins[2], Board::tftDataPins[3],
                   Board::tftDataPins[4], Board::tftDataPins[5],
                   Board::tftDataPins[6], Board::tftDataPins[7]);
Arduino_ILI9225 lcd(&bus, Board::tftResetPin, 1);
uint16_t pixels[6 * 8];
// One glyph buffer, rather than a framebuffer: fits in the Uno's 2KB SRAM.
void drawGlyph(uint8_t x, uint8_t y, uint8_t width) {
    lcd.draw16bitRGBBitmap(x, y, pixels, width, cellHeight);
}
void fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
    lcd.fillRect(x, y, w, h, color);
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
        for (uint8_t row = 0; row < 8; ++row)
            for (uint8_t col = 0; col < 6; ++col)
                pixels[row * 6 + col] = col < 5 && (fontColumn(c, col) & (1 << row)) ? color : 0;
        drawGlyph(x + i * 6, y, 6);
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
    lcd.begin(); // ILI9225-compatible RM68130, 220x176 landscape
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

        for (uint8_t y = 0; y < cellHeight; ++y) {
            for (uint8_t x = 0; x < cellWidth; ++x) {
                const uint8_t bits = c < 16 ? 0 : fontColumn(c, x);
                bool pixel = c < 16 ? (display.cgram[(c & 7) * 8 + y] & (1 << (4 - x))) : (bits & (1 << y));
                if (pos == cursor && (blink || ((display.displayControl & 2) && y == cellHeight - 1))) pixel = true;
                pixels[y * cellWidth + x] = pixel ? white : 0;
            }
        }
        drawGlyph(textLeft + (pos % columns) * cellWidth, 6 + (pos / columns) * 12, cellWidth);
        return;
    }
}
} }
#endif
