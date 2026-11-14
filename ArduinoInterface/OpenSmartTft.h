#pragma once
#include "Hardware.h"
#include "VirtualLcd.h"

namespace ServiceKeyboard {
namespace Tft {
void begin();
void refresh(VirtualLcd &display, uint32_t now);
void poll(VirtualLcd &display, uint32_t now);
uint8_t key();
}
}
