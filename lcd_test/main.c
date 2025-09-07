/*
 * hello_lcd.c
 * SDCC-compatible program to write "Hello" on line 1
 * and "World" on line 2 of a 2x40 LCD mapped at 0x0070–0x0073 in external memory.
 */

#include <stdint.h>

// External memory-mapped LCD registers
__xdata volatile uint8_t* const LCD_CMD_W  = (__xdata uint8_t*)0x0070;
__xdata volatile uint8_t* const LCD_CMD_R  = (__xdata uint8_t*)0x0071;
__xdata volatile uint8_t* const LCD_DATA_W = (__xdata uint8_t*)0x0072;
__xdata volatile uint8_t* const LCD_DATA_R = (__xdata uint8_t*)0x0073;

// Busy flag in status register (bit 7)
#define LCD_BUSY 0x80

static void lcd_delay(void) {
    unsigned int i;
    for (i = 0; i < 1000; i++);
}

static void lcd_wait_ready(void) {
    while (*LCD_CMD_R & LCD_BUSY);
}

static void lcd_cmd(uint8_t cmd) {
    lcd_wait_ready();
    *LCD_CMD_W = cmd;
}

static void lcd_putc(char c) {
    lcd_wait_ready();
    *LCD_DATA_W = c;
}

static void lcd_init(void) {
    lcd_delay();
    lcd_cmd(0x38); // Function set: 8-bit, 2 lines, 5x7 dots
    lcd_cmd(0x0C); // Display ON, cursor off
    lcd_cmd(0x06); // Entry mode: increment cursor
    lcd_cmd(0x01); // Clear display
}

static void lcd_puts(const char *s) {
    while (*s) {
        lcd_putc(*s++);
    }
}

void main(void) {
    lcd_init();

    lcd_cmd(0x80 | 0x00); // Line 1, DDRAM=0x00
    lcd_puts("Hello");

    lcd_cmd(0x80 | 0x40); // Line 2, DDRAM=0x40
    lcd_puts("World");

    while (1);
}
