#pragma once

#include <stdint.h>

namespace ServiceKeyboard {

enum ShieldButton : uint8_t { NoButton, RightButton, UpButton, DownButton, LeftButton, SelectButton };

inline ShieldButton decodeButton(uint16_t adc)
{
    // Standard DFRobot-style ladder; clone thresholds can be changed here.
    if (adc < 50) return RightButton;
    if (adc < 195) return UpButton;
    if (adc < 380) return DownButton;
    if (adc < 555) return LeftButton;
    if (adc < 790) return SelectButton;
    return NoButton;
}

inline ShieldButton decodeShieldExampleButton(uint16_t adc)
{
    // The example's second independent if overrides Right for 80..99.
    if (adc < 80) return RightButton;
    if (adc < 200) return UpButton;
    if (adc < 400) return DownButton;
    if (adc < 600) return LeftButton;
    if (adc < 900) return SelectButton;
    return NoButton;
}

inline ShieldButton decodeMeasuredEsp32Button(uint16_t adc, uint16_t wideMillivolts)
{
    // Measured at 0dB: Right=0, Up=337, Down=672, Left=868.
    // Select and idle both clip at 1023. At 11dB they measured 1075mV
    // and 1235mV respectively; their midpoint is 1155mV.
    if (adc < 169) return RightButton;
    if (adc < 505) return UpButton;
    if (adc < 770) return DownButton;
    if (adc < 946) return LeftButton;
    return wideMillivolts < 1155 ? SelectButton : NoButton;
}

class VirtualKeys {
public:
    VirtualKeys() : selected(0) {}
    uint8_t selected;
    void choose(bool down) {
        selected = (selected + (down ? 1 : 10)) % 11;
    }
    uint8_t code() const {
        // Original F1..F4, Hoch, OK, Links, Runter, Rechts, then the two chords.
        return selected < 9 ? selected + 1 : selected + 7;
    }
    uint8_t held(ShieldButton button) const {
        return button == SelectButton ? code() : 0;
    }
};

} // namespace ServiceKeyboard
