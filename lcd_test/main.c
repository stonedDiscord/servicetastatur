/*
 * keypad_lcd.c
 * SDCC-compatible program to show "Keypad Test" on line 1
 * and display the keypad state (from maybe_scan_kbd) on line 2.
 */

#include <stdint.h>
#include <8051.h>

// External memory-mapped LCD registers
__xdata volatile uint8_t* const LCD_CMD_W  = (__xdata uint8_t*)0x0070;
__xdata volatile uint8_t* const LCD_CMD_R  = (__xdata uint8_t*)0x0071;
__xdata volatile uint8_t* const LCD_DATA_W = (__xdata uint8_t*)0x0072;
__xdata volatile uint8_t* const LCD_DATA_R = (__xdata uint8_t*)0x0073;

// Busy flag in status register (bit 7)
#define LCD_BUSY 0x80

// Globals for keypad state (as in reversed function)
__data uint8_t DAT_INTMEM_22;
__data uint8_t DAT_INTMEM_23;
__data uint8_t DAT_INTMEM_4f;

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

static void lcd_puts(const char *s) {
    while (*s) {
        lcd_putc(*s++);
    }
}

static void lcd_init(void) {
    lcd_delay();
    lcd_cmd(0x38); // Function set: 8-bit, 2 lines, 5x7 dots
    lcd_cmd(0x0C); // Display ON, cursor off
    lcd_cmd(0x06); // Entry mode: increment cursor
    lcd_cmd(0x01); // Clear display
}

// Reversed keypad scan function
void maybe_scan_kbd(void) {
    uint8_t bVar1;
    DAT_INTMEM_23 = 0;
    P1 = 0xfb;
    bVar1 = P1 & 0x70;
    if (bVar1 == 0x50) { DAT_INTMEM_23 = 1; DAT_INTMEM_4f = 1; }
    else if (bVar1 == 0x30) { DAT_INTMEM_23 = 1; DAT_INTMEM_4f = 2; }
    else if (bVar1 == 0x60) { DAT_INTMEM_23 = 1; DAT_INTMEM_4f = 3; }

    P1 = 0xfd;
    bVar1 = P1 & 0x70;
    if (bVar1 == 0x50) { DAT_INTMEM_23++; DAT_INTMEM_4f = 7; }
    else if (bVar1 == 0x30) { DAT_INTMEM_23++; DAT_INTMEM_4f = 8; }
    else if (bVar1 == 0x60) { DAT_INTMEM_23++; DAT_INTMEM_4f = 9; }

    P1 = 0xfe;
    bVar1 = P1;
    DAT_INTMEM_22 = bVar1 & 0x70;
    if (DAT_INTMEM_22 == 0x50) {
        if (DAT_INTMEM_4f == 1) DAT_INTMEM_4f = 0x10;
        else { DAT_INTMEM_4f = 4; DAT_INTMEM_23++; }
    }
    else if (DAT_INTMEM_22 == 0x30) {
        if (DAT_INTMEM_4f == 2) DAT_INTMEM_4f = 0x11;
        else { DAT_INTMEM_23++; DAT_INTMEM_4f = 5; }
    }
    else if (DAT_INTMEM_22 == 0x60) { DAT_INTMEM_23++; DAT_INTMEM_4f = 6; }

    P1 = 0xff;
    if (DAT_INTMEM_23 != 1) DAT_INTMEM_4f = 0;
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

void main(void) {
    lcd_init();

    // Line 1: title
    lcd_cmd(0x80 | 0x00);
    lcd_puts("Keypad Test");

    while (1) {
        maybe_scan_kbd();

        lcd_cmd(0x80 | 0x40); // Line 2
        lcd_puts("Key:     ");  // clear old value with spaces
        lcd_cmd(0x80 | 0x45); // move cursor after "Key: "
        lcd_putnum(DAT_INTMEM_4f);
    }
}
