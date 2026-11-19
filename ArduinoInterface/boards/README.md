# STM32C542RC board support

`nucleo_c542rc.json` uses the generic STM32C542RCT6 pin variant in
STM32duino commit `8c9c3758cf4a732fef6cdc07c5da337e381312e5`.
Application wiring is documented in the project README and `Hardware.h`.

`stm32c542rc.ld` is adapted from that commit:
`system/Drivers/CMSIS/Device/ST/STM32C5xx/Source/Templates/gcc/linker/stm32c542xc_flash.ld`.
It defines 256 KiB flash, 64 KiB SRAM and the HAL v2 startup sections.
Heap and stack reservations are increased to 8 KiB each, matching the
core's Nucleo C5 linker script rather than the minimal CMSIS defaults.
Copyright 2026 STMicroelectronics; used under the BSD-3-Clause license
reproduced in `LICENSE.STMicroelectronics`.

The generic variant lacks its own linker script and clock implementation.
The local linker script, `Stm32Clock.cpp` and
`tools/platformio_stm32c5.py` supply those PlatformIO integration pieces.
The clock is configured to the internal 144 MHz HSIS source, with flash
latency configured before switching and APB3 divided by four.
Per-edge interrupt handling is still present; a successful build does not
establish reliable operation at the required 3 MHz machine clock.

The board manifest enables Arduino only. CMSIS and STM32Cube frameworks
are not declared because the selected PlatformIO release does not package
the C5 device dependencies for those frameworks. Programming uses
the open-source stlink toolset (`st-flash --reset write`) via ST-LINK/SWD.
Install a version with STM32C5 support (v1.9.0 is installed here);
`st-flash` must be available on PATH.
