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
#include <avr/pgmspace.h>
#include <util/atomic.h>
#define SERVICE_KEYBOARD_ISR_ATTR
#endif
#endif

#if !defined(__AVR_ATmega328P__) && !defined(ARDUINO_ARCH_ESP32) && !defined(SERVICE_KEYBOARD_HOST_TEST)
#error "Select an ATmega328P Uno/Nano or an ESP32 WEMOS D1 R32."
#endif

#ifndef SERVICE_KEYBOARD_TFT
// Display type when a shield is enabled: LCD1602 keypad shield by default.
// Set to 1 here (or via a build flag) to use the RM68130 TFT/touch shield.
#define SERVICE_KEYBOARD_TFT 0
#endif

#ifndef SERVICE_KEYBOARD_SHIELD
#if defined(ARDUINO_AVR_NANO)
// Classic Nano defaults to USB serial controls and the virtual display.
#define SERVICE_KEYBOARD_SHIELD 0
#else
#define SERVICE_KEYBOARD_SHIELD 1
#endif
#endif

#ifndef SERVICE_KEYBOARD_KEYPAD
#define SERVICE_KEYBOARD_KEYPAD SERVICE_KEYBOARD_SHIELD
#endif

#ifndef SERVICE_KEYBOARD_KEY_PIN
#define SERVICE_KEYBOARD_KEY_PIN 2
#endif

// Legacy LCD1602 keypad shield: RS=8, E=9, D4..D7=4..7, buttons=A0.
// LCD1602 J3: A1=OutST OUTPUT, A2=Enable, A3=ClockIn, A4=InST INPUT.
// TFT J3 retains InST=D12, ClockIn=D3, Enable=D5, OutST=A5 on AVR.
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
#else
constexpr uint8_t keyPin = 14; // A0 on Uno and Nano
constexpr uint8_t lcdRsPin = 8, lcdEnablePin = 9;
constexpr uint8_t lcdDataPins[4] = {4, 5, 6, 7};
#endif
#if SERVICE_KEYBOARD_TFT
#if !defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t tftDataPins[8] = {8, 9, 10, 11, 4, 13, 6, 7};
constexpr uint8_t tftControlPins[4] = {14, 15, 16, 17}; // RD, WR, RS, CS
constexpr uint8_t tftResetPin = 2;
#endif
// OPEN-SMART bus uses D11 and D2: J3 OutST moves to A5, Enable to D5.
// Default resistive panel wiring. X- and Y+ must support analogRead.
#if defined(ARDUINO_ARCH_ESP32)
constexpr uint8_t touchXp = 27, touchYp = 4, touchXm = 32, touchYm = 14;
#else
constexpr uint8_t touchXp = 6, touchYp = 15, touchXm = 16, touchYm = 7;
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
