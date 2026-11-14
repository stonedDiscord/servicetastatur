#pragma once
#include <stdint.h>

namespace ServiceKeyboard {
// Landscape coordinates, shared by drawing and hit testing.
struct TouchControls {
    static constexpr uint8_t screenWidth = 220, screenHeight = 176;
    static constexpr uint8_t top = 52, width = 55, height = 30;
    static uint8_t hit(int16_t x, int16_t y) {
        if (x < 0 || x >= screenWidth || y < top || y >= top + 4 * height) return 0;
        // Leave a small dead area between adjacent buttons.
        if (x % width < 2 || x % width >= width - 2 ||
            (y - top) % height < 2 || (y - top) % height >= height - 2) return 0;
        const uint8_t index = (y - top) / height * 4 + x / width;
        return index < 13 ? index + 1 : 0;
    }
    static int16_t coordinate(int16_t raw, int16_t first, int16_t last, int16_t extent) {
        return int32_t(raw - first) * (extent - 1) / (last - first);
    }
    static uint8_t key(uint8_t button) {
        if (button <= 9) return button;
        if (button == 10 || button == 11) return button + 6;
        return 0; // 12/13 move the display window only.
    }
};
// Debounce presses; release immediately on loss of contact or leaving a key.
class TouchInput {
public:
    uint8_t held = 0;
    uint8_t update(uint8_t value, uint32_t now, uint16_t debounce) {
        if (value != candidate) { candidate = value; changed = now; }
        if (held && value != held) held = 0;
        if (value != held && uint32_t(now - changed) >= debounce) {
            held = value;
            if (held >= 12) { repeatAt = now + 500; return held; }
        } else if (held >= 12 && int32_t(now - repeatAt) >= 0) {
            repeatAt = now + 160;
            return held;
        }
        return 0;
    }
private:
    uint8_t candidate = 0;
    uint32_t changed = 0, repeatAt = 0;
};
}
