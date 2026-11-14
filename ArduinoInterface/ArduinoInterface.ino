/* Service-Tastatur V3.3 hardware replacement.
 * Arduino IDE: select Tools > Board > Arduino AVR Boards > Arduino Uno
 * and the connected board's port. LCD1602 keypad shield is the default.
 * Display selection is in Hardware.h (SERVICE_KEYBOARD_TFT: 0=LCD, 1=TFT).
 * Uno / classic Nano or WEMOS D1 R32, with RM68130 TFT/touch or LCD1602.
 * See README.md for J3 wiring and the touch controls.
 */
#include "Hardware.h"

void setup()
{
    hardware_begin();
    firmware_begin();
}

void loop()
{
    firmware_run();
}
