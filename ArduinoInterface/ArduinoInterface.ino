/* Service-Tastatur V3.3 hardware replacement.
 * Arduino IDE: select Tools > Board > Arduino UNO R4 Boards > Arduino UNO R4 Minima
 * and the connected board's port. LCD1602 keypad shield is the default.
 * Display selection is in Hardware.h (SERVICE_KEYBOARD_TFT: 0=LCD, 1=TFT).
 * Uno/Nano ATmega328P: hardware SPI slave; see README.md for wiring.
 * Uno R4 Minima, Nucleo C542RC or WEMOS D1 R32: LCD1602.
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
