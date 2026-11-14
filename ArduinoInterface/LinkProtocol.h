#pragma once

#include <stdint.h>

// Keep the small shift/decode helpers inside ESP32 IRAM interrupt handlers.
#if defined(ARDUINO_ARCH_ESP32)
#define SERVICE_LINK_INLINE inline __attribute__((always_inline))
#else
#define SERVICE_LINK_INLINE inline
#endif

namespace ServiceKeyboard {

SERVICE_LINK_INLINE uint8_t reverseByte(uint8_t value)
{
    value = ((value & 0x55) << 1) | ((value & 0xaa) >> 1);
    value = ((value & 0x33) << 2) | ((value & 0xcc) >> 2);
    return (value << 4) | (value >> 4);
}

SERVICE_LINK_INLINE uint8_t swapU19Bits(uint8_t value)
{
    const uint8_t different = ((value >> 1) ^ (value >> 3)) & 1;
    return value ^ (different << 1) ^ (different << 3);
}

// First eight serial bits reach U19 (GSG5); the last eight reach U20 (GSG4).
SERVICE_LINK_INLINE uint8_t frameGsg4(uint16_t wire) { return reverseByte(uint8_t(wire)); }
SERVICE_LINK_INLINE uint8_t frameGsg5(uint16_t wire) { return swapU19Bits(reverseByte(wire >> 8)); }
SERVICE_LINK_INLINE uint8_t frameData(uint8_t gsg4, uint8_t gsg5)
{
    return uint8_t((gsg4 << 4) | (gsg5 >> 4));
}

// Rolling 16-bit shift state matches cascaded 4094s, including excess clocks.
struct ReceiveShift {
    uint16_t bits;
    uint8_t clocks;

    ReceiveShift() : bits(0), clocks(0) {}
    SERVICE_LINK_INLINE void clock(bool high) {
        bits = uint16_t((bits << 1) | high);
        if (clocks != 255) ++clocks;
    }
    SERVICE_LINK_INLINE bool latch(uint16_t &frame) {
        frame = bits;
        const bool complete = clocks >= 16;
        clocks = 0;
        // A strobe copies the storage latch; it does not clear the 4094s.
        return complete;
    }
};

// U13 shifts MSB first and shifts in zeros (DS is grounded on the PCB).
struct TransmitShift {
    uint8_t bits;
    TransmitShift() : bits(0) {}
    SERVICE_LINK_INLINE void load(uint8_t reply) { bits = reply; }
    SERVICE_LINK_INLINE bool high() const { return bits & 0x80; }
    SERVICE_LINK_INLINE void clock() { bits <<= 1; }
};

// The same external clock drives both input 4094s and the output 165.
struct MachineShiftRegisters {
    ReceiveShift input;
    TransmitShift output;
    SERVICE_LINK_INLINE void clock(bool inST) {
        input.clock(inST);
        output.clock();
    }
};

} // namespace ServiceKeyboard

#undef SERVICE_LINK_INLINE
