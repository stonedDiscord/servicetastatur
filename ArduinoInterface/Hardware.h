#pragma once

#if defined(SERVICE_KEYBOARD_HOST_TEST)
#include <stdint.h>
#include <string.h>
#define PROGMEM
#define PSTR(s) (s)
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_ptr(p) (*(p))
#define memcpy_P memcpy
#define SERVICE_KEYBOARD_ISR_ATTR
#else
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <pgmspace.h>
#define SERVICE_KEYBOARD_ISR_ATTR IRAM_ATTR
#else
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#endif
#ifndef pgm_read_ptr
#define pgm_read_ptr(p) (*(p))
#endif
#ifndef memcpy_P
#define memcpy_P memcpy
#endif
#ifndef strlen_P
#define strlen_P strlen
#endif
#define SERVICE_KEYBOARD_ISR_ATTR
#endif
#endif

#if defined(__AVR_ATmega328P__)
#define SERVICE_KEYBOARD_AVR_SPI 1
#endif

#if !defined(SERVICE_KEYBOARD_AVR_SPI) && !defined(ARDUINO_UNOR4_MINIMA) && !defined(SERVICE_KEYBOARD_NUCLEO_C542RC) && !defined(ARDUINO_ARCH_ESP32) && !defined(SERVICE_KEYBOARD_HOST_TEST)
#error "Select an Arduino Uno/Nano ATmega328P, Uno R4 Minima, ST Nucleo C542RC or ESP32 WEMOS D1 R32."
#endif

#ifndef SERVICE_KEYBOARD_TFT
// Display type when a shield is enabled: LCD1602 keypad shield by default.
// Set to 1 here (or via a build flag) to use the RM68130 TFT/touch shield.
#define SERVICE_KEYBOARD_TFT 0
#endif

#if defined(SERVICE_KEYBOARD_AVR_SPI) && SERVICE_KEYBOARD_TFT
#error "AVR hardware SPI requires the LCD1602 or serial-console configuration."
#endif

#ifndef SERVICE_KEYBOARD_SHIELD
#define SERVICE_KEYBOARD_SHIELD 1
#endif

#ifndef SERVICE_KEYBOARD_KEYPAD
#define SERVICE_KEYBOARD_KEYPAD SERVICE_KEYBOARD_SHIELD
#endif

#ifndef SERVICE_KEYBOARD_KEY_PIN
#define SERVICE_KEYBOARD_KEY_PIN 2
#endif

// Exclusive Select/idle boundary in 10-bit ADC counts. Calibrate using K in
// the serial console; keep this below the measured released-key reading.
#ifndef SERVICE_KEYBOARD_SELECT_ADC_MAX
#if defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
// Measured on the Nucleo: Select=907..910, released=1023.
#define SERVICE_KEYBOARD_SELECT_ADC_MAX 966
#else
#define SERVICE_KEYBOARD_SELECT_ADC_MAX 900
#endif
#endif
static_assert(SERVICE_KEYBOARD_SELECT_ADC_MAX > 600 && SERVICE_KEYBOARD_SELECT_ADC_MAX <= 1023,
              "Select ADC boundary must be above Left and below full scale");

// Legacy LCD1602 keypad shield: RS=8, E=9, D4..D7=4..7, buttons=A0.
// LCD1602 J3: A1=OutST OUTPUT, A2=Enable, A3=ClockIn, A4=InST INPUT.
// TFT J3: InST=D12, ClockIn=D3, Enable=D2, OutST=A5; reset moves to D5.
namespace Board {
// Set false to use USB serial alone, with no LCD/keypad shield attached.
constexpr bool shieldEnabled = SERVICE_KEYBOARD_SHIELD != 0;
constexpr bool keypadEnabled = shieldEnabled && SERVICE_KEYBOARD_KEYPAD != 0;
#if defined(ARDUINO_ARCH_ESP32)
// WEMOS D1 R32: Arduino APIs use native GPIO numbers, not Uno D numbers.
// Plug-on LCD1602 shield A0 connects to GPIO2. Verify voltage before use.
constexpr uint8_t keyPin = SERVICE_KEYBOARD_KEY_PIN;
#if SERVICE_KEYBOARD_TFT
constexpr uint8_t inStPin = 19, clockPin = 25;
constexpr uint8_t enablePin = 16, outStPin = 22;
// Header A2/A3/A5 are input-only: rewire TFT RS/CS and J3 OutST.
constexpr uint8_t tftDataPins[8] = {12, 13, 5, 23, 17, 18, 27, 14};
constexpr uint8_t tftControlPins[4] = {2, 4, 32, 33}; // RD, WR, RS, CS
constexpr uint8_t tftResetPin = 26;
#else
// D1 R32 analog header: A1=GPIO4, A2=GPIO35, A3=GPIO34, A4=GPIO36.
// These inputs have no internal pull-ups; provide external bias if needed.
constexpr uint8_t inStPin = 36, clockPin = 34;
constexpr uint8_t enablePin = 35, outStPin = 4;
#endif
constexpr uint8_t lcdRsPin = 12, lcdEnablePin = 13;
constexpr uint8_t lcdDataPins[4] = {17, 16, 27, 14};
#elif defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
// Digital indices from STM32duino's GENERIC_C542RCTX variant. PIN_Ax aliases
// do not fit GFX's int8_t pin arguments. Wire by the MCU pin names below,
// not by Arduino shield header numbers.
constexpr uint8_t keyPin = 0; // PA0
constexpr uint8_t lcdRsPin = 9, lcdEnablePin = 37; // PA9, PC6
constexpr uint8_t lcdDataPins[4] = {32, 20, 26, 8}; // PC1, PB4, PB10, PA8
#if SERVICE_KEYBOARD_TFT
constexpr uint8_t inStPin = 6, clockPin = 19; // PA6, PB3 (EXTI3)
constexpr uint8_t enablePin = 10, outStPin = 31; // PA10 (EXTI10), PC0
constexpr uint8_t tftDataPins[8] = {9, 37, 40, 7, 32, 5, 26, 8};
constexpr uint8_t tftControlPins[4] = {0, 1, 4, 16}; // PA0, PA1, PA4, PB0
constexpr uint8_t tftResetPin = 20; // PB4
#else
constexpr uint8_t inStPin = 36, clockPin = 16; // PC5, PB0 (EXTI0)
constexpr uint8_t enablePin = 4, outStPin = 1; // PA4 (EXTI4), PA1
#endif
#else
#if defined(SERVICE_KEYBOARD_HOST_TEST)
constexpr uint8_t A0 = 14, A1 = 15, A2 = 16, A3 = 17, A4 = 18, A5 = 19;
#endif
constexpr uint8_t keyPin = A0;
#if defined(SERVICE_KEYBOARD_AVR_SPI)
// Hardware SPI: MOSI input, SCK input, MISO output. SS (D10) stays low.
constexpr uint8_t inStPin = 11, clockPin = 13;
constexpr uint8_t enablePin = 2, outStPin = 12;
#elif SERVICE_KEYBOARD_TFT
constexpr uint8_t inStPin = 12, clockPin = 3;
constexpr uint8_t enablePin = 2, outStPin = A5;
#else
constexpr uint8_t inStPin = A4, clockPin = A3;
constexpr uint8_t enablePin = A2, outStPin = A1;
#endif
constexpr uint8_t lcdRsPin = 8, lcdEnablePin = 9;
constexpr uint8_t lcdDataPins[4] = {4, 5, 6, 7};
#endif
#if SERVICE_KEYBOARD_TFT
#if !defined(ARDUINO_ARCH_ESP32) && !defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
constexpr uint8_t tftDataPins[8] = {8, 9, 10, 11, 4, 13, 6, 7};
constexpr uint8_t tftControlPins[4] = {A0, A1, A2, A3}; // RD, WR, RS, CS
constexpr uint8_t tftResetPin = 5;
#endif
// D2 and D3 provide independent external interrupts on the Uno R4 Minima.
// Default resistive panel wiring. X- and Y+ must support analogRead.
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t touchXp = 27, touchYp = 4, touchXm = 32, touchYm = 14;
#elif defined(SERVICE_KEYBOARD_NUCLEO_C542RC)
constexpr uint8_t touchXp = 26, touchYp = 1, touchXm = 4, touchYm = 8; // PB10, PA1, PA4, PA8
#else
constexpr uint8_t touchXp = 6, touchYp = A1, touchXm = A2, touchYm = 7;
#endif
constexpr bool touchEnabled = true;
constexpr bool touchSwapAxes = true; // landscape: raw Y controls screen X
// Raw ADC readings at the visible left/right/top/bottom; reverse to mirror.
// Reverse the reported portrait touch direction, then rotate with the LCD.
constexpr int16_t touchLeft = 920, touchRight = 100; // swapped raw Y
constexpr int16_t touchTop = 120, touchBottom = 900; // swapped raw X
constexpr int16_t touchMinPressure = 80, touchMaxPressure = 1000;
// Prints raw coordinates to USB serial for calibration (115200 baud).
constexpr bool touchDebug = false;
static_assert(touchLeft != touchRight && touchTop != touchBottom, "Invalid touch calibration");
#endif
constexpr uint16_t debounceMs = 25;
constexpr uint16_t lcdStepUs = 50;
// Estimate for the reconstructed 8051 busy loops; tune against a hardware trace.
constexpr uint8_t delayTickUs = 10;
}

void hardware_begin();
void hardware_service();
void hardware_link_begin();
uint8_t hardware_key();
void hardware_lcd_command(uint8_t command);
void hardware_lcd_data(uint8_t value);
uint8_t hardware_lcd_address();
void SERVICE_KEYBOARD_ISR_ATTR hardware_lcd_enqueue_isr(bool data, uint8_t value);
void hardware_pause(uint16_t ticks);

void firmware_begin();
void firmware_run();
// Called at ENST. True means the firmware wrote GSG6: parallel-load reply
// into U13. False leaves its current shift state intact. LCD work is queued.
bool SERVICE_KEYBOARD_ISR_ATTR firmware_receive(uint8_t gsg4, uint8_t gsg5, uint8_t &reply);

extern volatile uint16_t linkFrameCount;
extern volatile uint16_t linkShortFrames;
extern volatile uint16_t lcdQueueOverflows;
extern volatile uint8_t machine_mode;
extern volatile uint8_t gsg_rx[16];
extern volatile uint8_t gsg_tx[5];
