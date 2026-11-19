#include "Hardware.h"

#if defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
// Run from the 144 MHz internal system oscillator without an external crystal.
// Clock capture still requires a hardware solution for a guaranteed 3 MHz link.
extern "C" void SystemClock_Config(void)
{
    if (HAL_RCC_HSIS_Enable() != HAL_OK) {
        Error_Handler();
    }

    hal_rcc_bus_clk_config_t bus = {};
    bus.hclk_prescaler = HAL_RCC_HCLK_PRESCALER1;
    bus.pclk1_prescaler = HAL_RCC_PCLK_PRESCALER1;
    bus.pclk2_prescaler = HAL_RCC_PCLK_PRESCALER1;
    bus.pclk3_prescaler = HAL_RCC_PCLK_PRESCALER4;
    if (HAL_RCC_SetBusClockConfig(&bus) != HAL_OK ||
        HAL_FLASH_ITF_SetLatency(HAL_FLASH, HAL_FLASH_ITF_LATENCY_4) != HAL_OK ||
        HAL_RCC_SetSYSCLKSource(HAL_RCC_SYSCLK_SRC_HSIS) != HAL_OK ||
        HAL_FLASH_ITF_SetProgrammingDelay(HAL_FLASH, HAL_FLASH_ITF_PROGRAM_DELAY_2) != HAL_OK ||
        HAL_UpdateCoreClock() != HAL_OK) {
        Error_Handler();
    }
}
#endif
