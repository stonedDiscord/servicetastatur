/*
 * Keypad test
 * compile with sdcc -mmcs51
 * Displays the current keypad state on the connected LCD
 */

#include <8051.h>
#include <stdint.h>

#include "hd44780.h"

// External memory-mapped LCD registers
__xdata volatile uint8_t* const LCD_CMD_W  = (__xdata uint8_t*)0x0070;
__xdata volatile uint8_t* const LCD_CMD_R  = (__xdata uint8_t*)0x0071;
__xdata volatile uint8_t* const LCD_DATA_W = (__xdata uint8_t*)0x0072;
__xdata volatile uint8_t* const LCD_DATA_R = (__xdata uint8_t*)0x0073;

static void lcd_delay(void) {
    unsigned int i;
    for (i = 0; i < 1000; i++);
}

static void lcd_wait_ready(void) {
    while (*LCD_CMD_R & HD44780_BUSY);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_wait_ready();
    *LCD_CMD_W = cmd;
}

static void lcd_putc(char c) {
    lcd_wait_ready();
    *LCD_DATA_W = c;
}

static void lcd_puts(const char *s) {
    while (*s) {
        lcd_putc(*s++);
    }
}

static void lcd_init(void) {
    lcd_delay();
    lcd_cmd(HD44780_CMD_FUNCTION_SET | HD44780_FUNCTION_8BIT | HD44780_FUNCTION_2LINE | HD44780_FUNCTION_5x7DOTS);
    lcd_cmd(HD44780_CMD_DISPLAY_CTRL | HD44780_DISPLAY_ON | HD44780_DISPLAY_CURSOR_OFF | HD44780_DISPLAY_BLINK_OFF);
    lcd_cmd(HD44780_CMD_ENTRY_MODE   | HD44780_ENTRY_LTR);
    lcd_cmd(HD44780_CMD_CLEAR);
}

// Keypad scan function
uint8_t scan_kbd(void) {
    uint8_t row_state;
    uint8_t key_count = 0;
    uint8_t key_code = 0;
    uint8_t column_input;
    P1 = 0xfb; // P1.2 column 3, F3, F1, F2
    row_state = P1 & 0x70;
    if (row_state == 0x50) { key_count = 1; key_code = 1; } // F3
    else if (row_state == 0x30) { key_count = 1; key_code = 2; } // F1
    else if (row_state == 0x60) { key_count = 1; key_code = 3; } // F2

    P1 = 0xfd; // P1.1 column 2, Right, Left, Down
    row_state = P1 & 0x70;
    if (row_state == 0x50) { key_count++; key_code = 7; } // Right
    else if (row_state == 0x30) { key_count++; key_code = 8; } // Left
    else if (row_state == 0x60) { key_count++; key_code = 9; } // Down

    P1 = 0xfe; // P1.0 column 1, OK, F4, Up
    row_state = P1;
    column_input = row_state & 0x70;
    if (column_input == 0x50) {
        if (key_code == 1) key_code = 0x10; // F3 + OK
        else { key_code = 4; key_count++; } // OK
    }
    else if (column_input == 0x30) {
        if (key_code == 2) key_code = 0x11; // F1 + F4
        else { key_count++; key_code = 5; } // F4
    }
    else if (column_input == 0x60) { key_count++; key_code = 6; } // Up

    P1 = 0xff;
    if (key_count != 1) key_code = 0; // No key or multiple keys pressed
    return key_code;
}

// Convert a small number to decimal string and display it
static void lcd_putnum(uint8_t n) {
    char buf[4];
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';
    if (n == 0) {
        *--p = '0';
    } else {
        while (n > 0) {
            *--p = '0' + (n % 10);
            n /= 10;
        }
    }
    lcd_puts(p);
}

// Interrupt handler External 0
void ext0_isr(void) __interrupt(0) {
    // Set cursor to bottom right (assuming 16-char display, position 13-15 for "INT")
    lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | (HD44780_LINE2_ADDR + 13));
    lcd_puts("INT");
}

void main(void) {
    lcd_init();

    // Line 1: title
    lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE1_ADDR | 0x00);
    lcd_puts("Keypad Test");

    while (1) {
            uint8_t key = scan_kbd();
    
            lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE2_ADDR | 0x00); // Line 2
            lcd_puts("Key:     ");  // clear old value with spaces
            lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE2_ADDR | 0x05); // move cursor after "Key: "
            lcd_putnum(key);
        }
}
