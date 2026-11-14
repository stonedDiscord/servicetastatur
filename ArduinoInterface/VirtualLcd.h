#pragma once

#include <stdint.h>
#include <string.h>

namespace ServiceKeyboard {

// Preserve two 40-character HD44780 rows; the legacy shield viewport is 16x2.
class VirtualLcd {
public:
    uint8_t ddram[80];
    uint8_t cgram[64];
    uint8_t address;
    uint8_t shift;
    uint8_t viewport;
    uint8_t displayControl;
    uint8_t entryMode;
    bool cgramSelected;
    bool cgramDirty;

    VirtualLcd() { reset(); }
    void reset() {
        memset(ddram, ' ', sizeof(ddram));
        memset(cgram, 0, sizeof(cgram));
        address = shift = viewport = 0;
        displayControl = 0x0c;
        entryMode = 0x06;
        cgramSelected = cgramDirty = false;
    }
    void command(uint8_t command) {
        if (command & 0x80) {
            address = command & 0x7f;
            cgramSelected = false;
        } else if (command & 0x40) {
            address = command & 0x3f;
            cgramSelected = true;
        } else if (command & 0x20) {
            // Original 8-bit/2-line mode is emulated on the shield's 4-bit bus.
        } else if (command & 0x10) {
            if (command & 8)
                shift = (shift + ((command & 4) ? 39 : 1)) % 40;
            else
                stepAddress(command & 4);
        } else if (command & 8) {
            displayControl = command;
        } else if (command & 4) {
            entryMode = command;
        } else if (command & 2) {
            address = shift = 0;
            cgramSelected = false;
        } else if (command & 1) {
            memset(ddram, ' ', sizeof(ddram));
            address = shift = 0;
            cgramSelected = false;
            entryMode |= 2;
        }
    }
    void data(uint8_t value) {
        if (cgramSelected) {
            cgram[address & 63] = value & 31;
            cgramDirty = true;
            address = (address + ((entryMode & 2) ? 1 : 63)) & 63;
        } else {
            const uint8_t row = address & 0x40 ? 1 : 0;
            const uint8_t col = address & 0x3f;
            if (col < 40) ddram[row * 40 + col] = value;
            stepAddress(entryMode & 2);
            if (entryMode & 1)
                shift = (shift + ((entryMode & 2) ? 1 : 39)) % 40;
        }
    }
    void scroll(bool right) {
        if (right && viewport < 24) ++viewport;
        if (!right && viewport > 0) --viewport;
    }
    uint8_t visible(uint8_t row, uint8_t col) const {
        return fullWidth(row, col + viewport);
    }
    uint8_t fullWidth(uint8_t row, uint8_t col) const {
        if (!(displayControl & 4)) return ' ';
        return ddram[row * 40 + (col + shift) % 40];
    }
    int8_t visibleCursor(uint8_t columns = 16) const {
        if (cgramSelected || !(displayControl & 4)) return -1;
        const uint8_t col = ((address & 0x3f) + 80 - shift - viewport) % 40;
        if (col >= columns) return -1;
        return ((address & 0x40) ? columns : 0) + col;
    }
private:
    void stepAddress(bool forward) {
        if (forward) {
            if (address == 0x27) address = 0x40;
            else if (address == 0x67) address = 0;
            else address = (address + 1) & 0x7f;
        } else {
            if (address == 0x40) address = 0x27;
            else if (address == 0) address = 0x67;
            else --address;
        }
    }
};

} // namespace ServiceKeyboard
