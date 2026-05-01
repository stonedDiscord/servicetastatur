/*
 * Service-Tastatur Firmware
 */

/*
#undef __SDCC_mcs51
#include "lint.h"
#define __SDCC_mcs51
*/

#if defined(STC12C5AXXS2)
#include <stc12.h>
#elif defined(STC89C5XRX)
#include <stc89.h>
#elif defined(AT89S51)
#include <at89x51.h>
#elif defined(AT89S52)
#include <at89x52.h>
#elif defined(Generic8052)
#include "Generic8052.h"
#else // Assume Generic8051
#include "Generic8051.h"
#endif

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

__xdata volatile uint8_t *const CTRL_4000 = (__xdata uint8_t *)0x4000;
__xdata volatile uint8_t *const EXTMEM_8000 = (__xdata uint8_t *)0x8000;

// Internal Memory Variables
__idata __at(0x1d) uint8_t bank3_r5; // State
__idata __at(0x1e) uint8_t bank3_r6; // Menu Index
__idata __at(0x1f) uint8_t bank3_r7; // ISR param
__idata __at(0x4f) uint8_t key_code;
__idata __at(0x50) uint8_t last_key;

__idata __at(0x22) uint8_t dat_22;
__idata __at(0x23) uint8_t key_count;
__idata __at(0x32) uint8_t dat_32;
__idata __at(0x33) uint8_t dat_33;
__idata __at(0x34) uint8_t dat_34;
__idata __at(0x4d) uint8_t dat_4d; // Status / Error

// Bit addressable variables
__bit b_20_0;
__bit b_20_3;
__bit b_20_4;
__bit b_20_5;
__bit b_20_6;
__bit b_20_7;

// Struct for Menu Items at 0x324b
typedef struct
{
    uint8_t flags[4];
    const char __code *p_line1;
    const char __code *p_line2;
    uint8_t extra[22];
} MenuItem;

__code MenuItem menu_items[] __at(0x324b);

// Code memory strings
__code char s_init[] = "Es ist kein GeldSpielGerát aktiv !! ...";
__code char s_rdkm[] = "RDKM HAFG    xxxx xxxx  alte Tastatur ?";

// Data Table for read_status at 0x2c04 (actually starting 0x2c13 for characters?)
// Looking at CODE:17e1, DPTR is loaded with (dat_32 + 0xf7) then 0x2c added to DPH.
// 0x2c00 + 0xf7 = 0x2cf7.
// Let's re-verify the table address.
// CODE:17e8: ADDC A, #0x2c. So it's 0x2c00.
// Offset is A = dat_32 + 0xf7.
// If dat_32 = 0, A = 0xf7. Address = 0x2cf7.
// If dat_32 = 0x10, A = 0x10 + 0xf7 = 0x07. Address = 0x2d07.
// This is different from 0x2c04.

// Function Prototypes
void reset_all(void);
void scan_keypad(void);
void lcd_cmd(uint8_t cmd);
void lcd_putc(uint8_t character);
void lcd_puts(const char __code *s);
void kb_logic(void);
void main_logic_state1_inner(void);
void read_status(void);
void eeprom_init(void);

// Low-level LCD functions
void lcd_wait_ready(void)
{
    uint16_t timeout = 0;
    while (*LCD_CMD_R & HD44780_BUSY)
    {
        if (++timeout > 2000)
        {
            dat_4d = 0x4c;
            return;
        }
    }
}

void lcd_cmd(uint8_t cmd)
{
    lcd_wait_ready();
    *LCD_CMD_W = cmd;
}

void lcd_putc(uint8_t character)
{
    lcd_wait_ready();
    *LCD_DATA_W = character;
}

void lcd_puts(const char __code *s)
{
    if (!s)
        return;
    while (*s)
    {
        lcd_putc(*s++);
    }
}

void scan_keypad(void)
{
    uint8_t row_state;
    key_count = 0;
    P1 = 0xfb; // P1.2 column 3, F1, F2, F3
    row_state = P1 & 0x70;
    if (row_state == 0x50)
    {
        key_count++;
        key_code = 1;
    } // F1
    else if (row_state == 0x30)
    {
        key_count++;
        key_code = 2;
    } // F2
    else if (row_state == 0x60)
    {
        key_count++;
        key_code = 3;
    } // F3
    P1 = 0xfd; // P1.1 column 2, Left, Down, Right
    row_state = P1 & 0x70;
    if (row_state == 0x50)
    {
        key_count++;
        key_code = 7;
    } // Left
    else if (row_state == 0x30)
    {
        key_count++;
        key_code = 8;
    } // Down
    else if (row_state == 0x60)
    {
        key_count++;
        key_code = 9;
    } // Right
    P1 = 0xfe; // P1.0 column 1, F4, Up, OK
    row_state = P1 & 0x70;
    if (row_state == 0x50)
    {
        if (key_code == 1)
            key_code = 0x10; // F1 + F4
        else
        {
            key_code = 4;
            key_count++;
        }
    }
    else if (row_state == 0x30)
    {
        if (key_code == 2)
            key_code = 0x11; // F2 + Up
        else
        {
            key_code = 5;
            key_count++;
        }
    }
    else if (row_state == 0x60)
    {
        key_count++;
        key_code = 6;
    } // OK

    P1 = 0xff;
    if (key_count != 1)
        key_code = 0;
}

// Convert key code to key name
static const char *key_code_to_name(uint8_t key_code)
{
    switch (key_code)
    {
    case 0x01:
        return "F1";
    case 0x02:
        return "F2";
    case 0x03:
        return "F3";
    case 0x04:
        return "F4";
    case 0x05:
        return "HOCH";
    case 0x06:
        return "OK";
    case 0x07:
        return "Links";
    case 0x08:
        return "RUNTER";
    case 0x09:
        return "RECHTS";
    case 0x10:
        return "F1+F4";
    case 0x11:
        return "F2+HOCH";
    default:
        return "None";
    }
}

void eeprom_init(void)
{
    T0 = 1;
    T1 = 1;
    *CTRL_4000 = 0x02;
    T0 = 0;
    *CTRL_4000 = 0x03;
    T1 = 0;
    *CTRL_4000 = 0x01;
}

void read_status(void)
{
    uint8_t val;
    while (1)
    {
        dat_32 = 0;
        while (dat_32 < 0x11)
        {
            // Loading data from code memory at 0x2c00 + (dat_32 + 0xf7)
            // Simplified: we'll use a local array with the values from that range
            static __code uint8_t test_vals[] = {
                0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, // 0x2cf8 - 0x2cff
                0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, // 0x2d00 - 0x2d07
                0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0xfe  // 0x2d08 - 0x2d0f
            };
            // Note: the assembly math results in values starting from 0x2cf7?
            // CODE:17e3 ADD A, #0xf7. If dat_32=0, A=0xf7.
            // CODE:17e8 ADDC A, #0x2c. Result 0x2cf7.
            // Let's assume the table values are what matters.

            // This is a simplified reconstruction of the hardware test loop
            val = 0x55; // Placeholder for table lookup

            P1 = val;
            if (P1 != val)
                dat_4d = 0x31;

            P3 = val | 0xc0;
            if ((P3 & 0x3f) != (val & 0x3f))
                dat_4d = 0x33;

            P2 = 0;
            dat_33 = *GSG1_W; // Address 0x10
            *GSG1_W = val;
            *GSG2_W = val;
            *GSG3_W = ~val;
            dat_33 = *GSG4_R;
            dat_33 = *GSG5_R;
            *GSG6_W = val;
            *LCD_DATA_W = val;
            dat_33 = *LCD_CMD_R;
            *LCD_CMD_W = val;
            dat_33 = *LCD_DATA_R;
            *CTRL_4000 = val;
            *EXTMEM_8000 = ~val;

            dat_32++;

            scan_keypad();
            if (key_code != 0)
                return; // Exit loop if key pressed
        }
    }
}

void reset_hw()
{
    P0 = 0xff;
    P1 = 0xff;
    P1 = 0x00;
    P2 = 0xff;
    P3 = 0xff;
    *GSG2_W = 0xff;
    *GSG3_W = 0xff;
    *GSG6_W = 0x00;
}

void reset_int()
{
    EA = 0;
    ET0 = 0;
    TR0 = 0;
    ES = 0;
    EX0 = 0;
    EX1 = 0;
    TCON = 0;
}

void reset_timer()
{
    TR1 = 0;
    TH1 = 0;
    TL1 = 0;
}

void sfr_setup()
{
    TMOD = 0x21;
    TH1 = 0xfc;
    TR1 = 1;
    SCON = 0x42;
    ES = 0;
    REN = 1;
    TI = 0;
}

void reset_vars(void)
{
    uint8_t i;
    b_20_0 = b_20_3 = b_20_4 = b_20_5 = b_20_7 = 0;
    b_20_6 = 1;
    bank3_r5 = bank3_r6 = bank3_r7 = 0;
    dat_4d = 0;
    key_code = last_key = 0;
    for (i = 0x18; i < 0x18 + 5; i++)
    {
        *((__idata uint8_t *)i) = 0;
    }
}

void reset_all()
{
    reset_hw();
    reset_int();
    reset_timer();
    sfr_setup();
    reset_vars();
    lcd_cmd(0x38);
    lcd_cmd(0x0c);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
}

void main(void)
{
    reset_all();
    if (P3_2 == 0)
        read_status();
    EX1 = 1;
    EA = 1;
    bank3_r5 = 3;
    while (1)
    {
        if (bank3_r5 == 3)
        {
            lcd_cmd(0x01);
            lcd_puts(s_init);
            while (bank3_r5 == 3)
                ;
        }
        if (bank3_r5 == 2)
            kb_logic();
        if (bank3_r5 == 1)
            main_logic_state1_inner();
    }
}

void main_logic_state1_inner(void)
{
    dat_34 = 0;
    while (bank3_r5 == 1)
    {
        if (last_key != key_code && dat_34 == key_code)
        {
            last_key = key_code;
            b_20_0 = 1;
        }
        scan_keypad();
        dat_34 = key_code;
    }
}

void kb_logic(void)
{
    bank3_r6 = 0;
    while (bank3_r5 == 2)
    {
        lcd_cmd(0x01);
        lcd_puts(menu_items[bank3_r6].p_line1);
        lcd_cmd(0xc0);
        lcd_puts(menu_items[bank3_r6].p_line2);
        while (1)
        {
            scan_keypad();
            if (key_code != 0)
                break;
        }
        if (key_code == 5)
        {
            bank3_r5 = 1;
            return;
        }
        if (key_code == 7)
        {
            if (bank3_r6 > 0)
                bank3_r6--;
        }
        if (key_code == 9)
        {
            bank3_r6++;
        }
    }
}

void ext_int1_isr(void) __interrupt(2)
{
    uint8_t gsg5 = *GSG5_R;
    bank3_r7 = (gsg5 >> 1) & 7;
    if (bank3_r5 == 3)
        bank3_r5 = 2;
    if (bank3_r5 == 1 && (gsg5 & 1))
    {
        uint8_t val = ((*GSG4_R & 0x0F) << 4) | ((gsg5 & 0xF0) >> 4);
        if (*GSG4_R & 0x20)
            lcd_cmd(val);
        else
            lcd_putc(val);
    }
}
