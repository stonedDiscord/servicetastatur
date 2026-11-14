#!/usr/bin/env python3
"""Build with an installed Arduino AVR core/toolchain; no network or cache writes."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--board', choices=('uno', 'nano'), default='uno')
display = parser.add_mutually_exclusive_group()
display.add_argument('--lcd1602', action='store_true', help='Build the LCD1602 keypad version (default)')
display.add_argument('--tft', action='store_true', help='Build the RM68130 TFT/touch version')
parser.add_argument('--avr-bin', type=Path,
                    default=Path.home() / '.platformio/packages/toolchain-atmelavr/bin')
parser.add_argument('--arduino-core', type=Path,
                    default=Path.home() / '.platformio/packages/framework-arduino-avr')
parser.add_argument('--output', type=Path, help='Directory for the ELF and HEX (default: a new /tmp directory)')
args = parser.parse_args()
project = Path(__file__).resolve().parents[1]
output = args.output or Path(tempfile.mkdtemp(prefix=f'service-keyboard-{args.board}-'))
output.mkdir(parents=True, exist_ok=True)
# Some packaged toolchains retain only the versioned plugin filename. Give
# GCC its expected name in the build directory without changing the install.
plugins = sorted((args.avr_bin.parent / 'libexec/gcc/avr').glob('*/liblto_plugin.so*'))
if not plugins:
    raise SystemExit('AVR LTO plugin not found in the toolchain')
plugin = output / 'liblto_plugin.so'
if not plugin.exists():
    plugin.symlink_to(plugins[0].resolve())
tool_prefix = f'-B{output.resolve()}/'
core = args.arduino_core / 'cores/arduino'
variant = args.arduino_core / 'variants' / ('standard' if args.board == 'uno' else 'eightanaloginputs')
common = ['-mmcu=atmega328p', '-DF_CPU=16000000L', '-DARDUINO=10819',
          f'-DARDUINO_AVR_{args.board.upper()}', '-DARDUINO_ARCH_AVR', '-Os', '-flto',
          tool_prefix, '-ffunction-sections', '-fdata-sections', f'-I{core}', f'-I{variant}', f'-I{project}']

if args.lcd1602:
    common.append('-DSERVICE_KEYBOARD_TFT=0')
elif args.tft:
    common.append('-DSERVICE_KEYBOARD_TFT=1')

def run(tool, *flags, capture=False):
    return subprocess.run([str(args.avr_bin / tool), *map(str, flags)], check=True,
                          text=True, stdout=subprocess.PIPE if capture else None).stdout

def compile_source(source, destination, application=False):
    if source.suffix == '.c':
        tool, flags = 'avr-gcc', ['-std=gnu11']
    elif source.suffix == '.S':
        tool, flags = 'avr-gcc', ['-x', 'assembler-with-cpp']
    else:
        tool, flags = 'avr-g++', ['-x', 'c++', '-std=gnu++11', '-fno-exceptions', '-fno-threadsafe-statics']
    if application:
        flags += ['-Wall', '-Wextra', '-Werror']
    run(tool, *common, *flags, '-c', source, '-o', destination)

core_objects = []
for source in sorted(core.iterdir()):
    if source.suffix not in ('.c', '.cpp', '.S'):
        continue
    obj = output / ('core-' + source.name + '.o')
    compile_source(source, obj)
    core_objects.append(obj)
archive = output / 'core.a'
run('avr-ar', 'rcs', f'--plugin={plugins[0]}', archive, *core_objects)
objects = []
for source in sorted(project.iterdir()):
    # PlatformIO may temporarily create this beside the sketch during a build.
    if source.name.endswith('.ino.cpp'):
        continue
    if source.suffix not in ('.ino', '.cpp'):
        continue
    obj = output / (source.name + '.o')
    compile_source(source, obj, application=True)
    objects.append(obj)
elf = output / 'ArduinoInterface.elf'
run('avr-g++', tool_prefix, '-mmcu=atmega328p', '-Os', '-flto', '-Wl,--gc-sections',
    '-o', elf, *objects, archive, '-lm')
run('avr-objcopy', '-O', 'ihex', '-R', '.eeprom', elf, output / 'ArduinoInterface.hex')
size = run('avr-size', '-A', elf, capture=True)
sections = {}
for line in size.splitlines():
    parts = line.split()
    if len(parts) >= 2 and parts[0].startswith('.'):
        sections[parts[0]] = int(parts[1])
flash = sections.get('.text', 0) + sections.get('.data', 0)
sram = sections.get('.data', 0) + sections.get('.bss', 0) + sections.get('.noinit', 0)
limit = 32256 if args.board == 'uno' else 30720
print(f'{args.board}: flash {flash}/{limit} bytes; static SRAM {sram}/2048 bytes; '
      f'{2048 - sram} bytes available for stack')
if flash > limit or sram >= 2048:
    raise SystemExit('Sketch exceeds board capacity')
print(f'ELF and HEX: {output}')
