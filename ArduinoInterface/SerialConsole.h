#pragma once

#include "Hardware.h"
#include "VirtualLcd.h"

namespace ServiceKeyboard {

// Terminal keystrokes and Serial Monitor text use the same original key codes.
// A small FIFO preserves taps sent together by a line-oriented monitor.
class ConsoleKeys {
public:
    enum Action : uint8_t { None, Help, Redraw, ToggleTerminal };
    ConsoleKeys() : head(0), tail(0), active(0), held(false), holdNext(false),
                    escape(0), parameter(0), escapeSince(0), deadline(0) {}
    Action input(uint8_t c, uint32_t now) {
        if (escape && uint32_t(now - escapeSince) > 100) escape = 0;
        if (c == 27) { escape = 1; parameter = 0; escapeSince = now; return None; }
        if (escape) {
            if (escape == 1 && (c == '[' || c == 'O')) { escape = 2; return None; }
            if (escape == 2 && c >= '0' && c <= '9') {
                parameter = parameter * 10 + c - '0'; return None;
            }
            escape = 0;
            if (c == 'A') push(5);
            else if (c == 'B') push(8);
            else if (c == 'C') push(9);
            else if (c == 'D') push(7);
            else if (c >= 'P' && c <= 'S') push(c - 'P' + 1); // SS3 F1..F4
            else if (c == '~' && parameter >= 11 && parameter <= 14) push(parameter - 10);
            return None;
        }
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c == '+') { holdNext = true; return None; }
        if (c == '0' || c == ' ' || c == '-') {
            // Release is immediate and cancels queued taps/hold prefixes.
            head = tail = 0; active = 0; held = holdNext = false;
            deadline = now + 60;
        } else if (c >= '1' && c <= '4') push(c - '0');
        else if (c == 'w') push(5);
        else if (c == 'o') push(6);
        else if (c == 'a') push(7);
        else if (c == 's') push(8);
        else if (c == 'd') push(9);
        else if (c == 'x') push(16);
        else if (c == 'y') push(17);
        else if (c == '?') return Help;
        else if (c == 'r') return Redraw;
        else if (c == 't') return ToggleTerminal;
        // Ignore CR/LF: sending "1" with a monitor's line ending is one tap.
        return None;
    }
    void tick(uint32_t now) {
        if (active) {
            if ((held && head == tail) || (!held && int32_t(now - deadline) < 0)) return;
            active = 0; held = false; deadline = now + 60;
            return;
        }
        if (head == tail || int32_t(now - deadline) < 0) return;
        const uint8_t event = queue[tail];
        tail = (tail + 1) & 7;
        active = event & 0x7f;
        held = event & 0x80;
        deadline = now + 180;
    }
    uint8_t key() const { return active; }
private:
    uint8_t queue[8];
    uint8_t head, tail, active;
    bool held, holdNext;
    uint8_t escape, parameter;
    uint32_t escapeSince, deadline;
    void push(uint8_t key) {
        const uint8_t next = (head + 1) & 7;
        if (next != tail) { queue[head] = key | (holdNext ? 0x80 : 0); head = next; }
        holdNext = false;
    }
};

// One bounded frame, drained only when the UART has room. No LCD/serial work
// runs in the machine ISRs and a slow console cannot block the firmware loop.
class ConsoleDisplay {
public:
    char frame[288];
    uint16_t length, sent;
    bool terminal;
    ConsoleDisplay() : length(0), sent(0), terminal(false), force(true), restoreCursor(false),
                       lastMode(255), lastKey(255), lastAddress(255),
                       lastControl(255), nextUpdate(0) {
        memset(previous, 0xff, sizeof(previous));
    }
    void redraw() { force = true; }
    void toggle() { restoreCursor = terminal; terminal = !terminal; force = true; }
    bool busy() const { return sent != length; }
    bool update(const VirtualLcd &lcd, uint8_t mode, uint8_t key, uint32_t now) {
        if (busy() || (!force && int32_t(now - nextUpdate) < 0)) return false;
        nextUpdate = now + 100;
        bool changed = force || mode != lastMode || key != lastKey
                     || lcd.address != lastAddress || lcd.displayControl != lastControl;
        for (uint8_t i = 0; i < 80; ++i) {
            const uint8_t value = lcd.fullWidth(i / 40, i % 40);
            if (previous[i] != value) changed = true;
            previous[i] = value;
        }
        if (!changed) return false;
        length = sent = 0;
        if (terminal) {
            if (force) text(PSTR("\x1b[2J"));
            text(PSTR("\x1b[H\x1b[?25l"));
        }
        else text(PSTR("\r\n"));
        text(PSTR("LCD 40x2 mode: ")); append('0' + mode);
        text(PSTR("\r\n"));
        if (terminal) text(PSTR("\x1b[37;44m"));
        border();
        for (uint8_t row = 0; row < 2; ++row) {
            append('|');
            for (uint8_t col = 0; col < 40; ++col) {
                const bool cursor = terminal && !lcd.cgramSelected && (lcd.displayControl & 4)
                    && (lcd.displayControl & 3) && ((lcd.address & 0x40) ? 1 : 0) == row
                    && (lcd.address & 0x3f) == (col + lcd.shift) % 40;
                if (cursor) text((lcd.displayControl & 1) ? PSTR("\x1b[5;7m") : PSTR("\x1b[4m"));
                append(printable(previous[row * 40 + col]));
                if (cursor) text(PSTR("\x1b[0;37;44m"));
            }
            text(PSTR("|\r\n"));
        }
        border();
        if (terminal) text(PSTR("\x1b[0m"));
        text(PSTR("Key: "));
        append(hex(key >> 4)); append(hex(key & 15));
        text(PSTR("  ? help  t ANSI  r redraw\r\n"));
        if (restoreCursor) { text(PSTR("\x1b[?25h")); restoreCursor = false; }
        lastMode = mode; lastKey = key; lastAddress = lcd.address;
        lastControl = lcd.displayControl;
        force = false;
        return true;
    }
private:
    uint8_t previous[80];
    bool force, restoreCursor;
    uint8_t lastMode, lastKey, lastAddress, lastControl;
    uint32_t nextUpdate;
    static char hex(uint8_t n) { return n < 10 ? '0' + n : 'A' + n - 10; }
    static char printable(uint8_t c) {
        // ASCII keeps each LCD cell exactly one console column in any monitor.
        if (c >= 32 && c < 127) return c;
        if (c == 0xe1) return 'a'; // HD44780 A00 umlauts
        if (c == 0xef) return 'o';
        if (c == 0xf5) return 'u';
        if (c == 0xe2) return 's';
        if (c == 0xff) return '#';
        return '?'; // Custom CGRAM and other character-ROM glyphs
    }
    void append(char c) { if (length < sizeof(frame) - 1) frame[length++] = c; }
    void text(const char *p) { char c; while ((c = pgm_read_byte(p++))) append(c); }
    void border() {
        append('+'); for (uint8_t i = 0; i < 40; ++i) append('-');
        text(PSTR("+\r\n"));
    }
};

} // namespace ServiceKeyboard
