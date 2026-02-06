/*
 * Keypad test
 * compile with sdcc -mmcs51
 * Displays the current keypad state on the connected LCD
 */

#include <8051.h>
#include <stdint.h>

#include "hd44780.h"

// NC
__xdata volatile uint8_t* const GSG1_W  = (__xdata uint8_t*)0x0010;
__xdata volatile uint8_t* const GSG2_W  = (__xdata uint8_t*)0x0020;
__xdata volatile uint8_t* const GSG3_W  = (__xdata uint8_t*)0x0030;

// upper in
__xdata volatile uint8_t* const GSG4_R  = (__xdata uint8_t*)0x0040;
// lower in
__xdata volatile uint8_t* const GSG5_R  = (__xdata uint8_t*)0x0050;

//out
__xdata volatile uint8_t* const GSG6_W  = (__xdata uint8_t*)0x0060;

// lcd
__xdata volatile uint8_t* const LCD_CMD_W  = (__xdata uint8_t*)0x0070;
__xdata volatile uint8_t* const LCD_CMD_R  = (__xdata uint8_t*)0x0071;
__xdata volatile uint8_t* const LCD_DATA_W = (__xdata uint8_t*)0x0072;
__xdata volatile uint8_t* const LCD_DATA_R = (__xdata uint8_t*)0x0073;


static void spin_delay(uint16_t delay) {
    uint16_t i;
    for (i = 0; i < delay; i++);
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
    lcd_cmd(HD44780_CMD_FUNCTION_SET | HD44780_FUNCTION_8BIT | HD44780_FUNCTION_2LINE | HD44780_FUNCTION_5X7DOTS);
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
    P1 = 0xfb; // P1.2 column 3, F1, F2, F3
    row_state = P1 & 0x70;
    if (row_state == 0x50) { key_count = 1; key_code = 1; } // F1
    else if (row_state == 0x30) { key_count = 1; key_code = 2; } // F2
    else if (row_state == 0x60) { key_count = 1; key_code = 3; } // F3

    P1 = 0xfd; // P1.1 column 2, Left, Down, Right
    row_state = P1 & 0x70;
    if (row_state == 0x50) { key_count++; key_code = 7; } // Left
    else if (row_state == 0x30) { key_count++; key_code = 8; } // Down
    else if (row_state == 0x60) { key_count++; key_code = 9; } // Right

    P1 = 0xfe; // P1.0 column 1, F4, Up, OK
    row_state = P1;
    column_input = row_state & 0x70;
    if (column_input == 0x50) {
        if (key_code == 1) key_code = 0x10; // F1 + F4
        else { key_code = 4; key_count++; } // F4
    }
    else if (column_input == 0x30) {
        if (key_code == 2) key_code = 0x11; // F2 + Up
        else { key_count++; key_code = 5; } // Up
    }
    else if (column_input == 0x60) { key_count++; key_code = 6; } // OK

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

// Convert key code to key name
static const char* key_code_to_name(uint8_t key_code) {
    switch (key_code) {
        case 0x01: return "F1";
        case 0x02: return "F2";
        case 0x03: return "F3";
        case 0x04: return "F4";
        case 0x05: return "Up";
        case 0x06: return "OK";
        case 0x07: return "Left";
        case 0x08: return "Down";
        case 0x09: return "Right";
        case 0x10: return "F1+F4";
        case 0x11: return "F2+Up";
        default: return "None";
    }
}

void read_gsg(void)
{
    uint8_t direction = 1;
    uint8_t gsg_command = 0;
    if (direction == 0x03)
    {
        direction = 0x02;
    }
    gsg_command = *GSG5_R >> 1 & 7;
    if (direction == 0x01)
    {
        if ((*GSG5_R & 1) != 0)
        {
            if ((*GSG4_R & 0x20) == 0)
            {
                lcd_putc((*GSG5_R >> 4) + *GSG4_R * 0x10);
            }
            else
            {
                lcd_cmd((*GSG5_R >> 4) + *GSG4_R * 0x10);
            }
        }
    }
}

// Interrupt handler External 0
void INT0_ISR (void) __interrupt (0) {
    EA=0;
    //lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | (HD44780_LINE2_ADDR + 15));
    //lcd_puts("INT0");
    read_gsg();
}

// Interrupt handler External 1
void INT1_ISR (void) __interrupt (2) {
    EA=0;
    //lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | (HD44780_LINE1_ADDR + 15));
    //lcd_puts("INT1");
    read_gsg();
}

void main(void) {
    lcd_init();
    EX1 = 1;

    uint16_t rend = 0;

    // Line 1: title
    lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE1_ADDR | 0x00);
    lcd_puts("Keypad Test        ");
    //EA = 1;
    while (1) {
            uint8_t key = scan_kbd();
     
            lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE2_ADDR | 0x00); // Line 2
            lcd_puts("Key:          ");  // clear old value with spaces
            lcd_cmd(HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE2_ADDR | 0x05); // move cursor after "Key: "
            lcd_puts(key_code_to_name(key));
            rend++;
            *GSG6_W= (uint8_t)(rend & 0x00FF);
            EA=1;
            spin_delay(100);
            EA=0;
            
        }
}
