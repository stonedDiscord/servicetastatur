/* Service-Tastatur V3.3 hardware replacement.
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
