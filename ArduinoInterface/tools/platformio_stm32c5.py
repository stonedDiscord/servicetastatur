"""Add the HAL v2 CMSIS startup include path missing from the PIO builder."""
from pathlib import Path

Import('env')

framework = Path(env.PioPlatform().get_package_dir('framework-arduinoststm32'))
env.Append(CPPPATH=[str(framework / 'system/Drivers/CMSIS/Device/ST/STM32C5xx/Source')])
