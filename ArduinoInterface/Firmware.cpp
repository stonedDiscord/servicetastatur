/* Arduino port of firmware/src/main.c (Service-Tastatur V3.3 reconstruction).
 * Handlers retain the Ghidra names. Flash objects are relocatable; the 8051
 * register map is replaced by Hardware.cpp. EEPROM is deliberately disabled.
 */
#include "Hardware.h"
#include "LinkProtocol.h"
#include "hd44780.h"

static uint8_t legacy_gsg2 = 0xff;
static uint8_t legacy_gsg3 = 0xff;
static uint8_t legacy_i2c_control;

enum Tasten {
    T_Keine = 0, T_F1 = 1, T_F2 = 2, T_F3 = 3, T_F4 = 4,
    T_Hoch = 5, T_OK = 6, T_Links = 7, T_Runter = 8, T_Rechts = 9,
    T_F1_F4 = 0x10,   /* F1 + F4 chord */
    T_F2_Hoch = 0x11  /* F2 + HOCH chord */
};

/* ------------------------------------------------------------------ *
 *  Global state (modelled from the IDATA / register-bank layout)
 * ------------------------------------------------------------------ */
volatile uint8_t machine_mode;   /* bank3 R5 @0x1d: 1=PROFITECH, 2=old kbd, 3=none */
volatile uint8_t menu_index;     /* bank3 R6 @0x1e */
volatile uint8_t gsg_cmd;        /* bank3 R7 @0x1f */
volatile uint8_t key_code;       /* current decoded key (Tasten) */
volatile uint8_t last_key;       /* 0x50: previous key_code */
volatile uint8_t key_hold_state; /* IDATA 0x2e: held-key state used by repeat */
volatile uint8_t service_key_latch; /* IDATA 0x34: key value exchanged with PROFITECH */

volatile uint8_t gsg_rx[16];     /* 0x10..0x1f: channel latch bytes from machine (cmd index) */
volatile uint8_t gsg_tx[5];      /* 0x18..0x1c: bytes to machine / EEPROM data */

uint8_t gsg_channel = 1;         /* 0x4b: selected GSG channel/index */
uint8_t gsg_level_flags;         /* 0x4e */
uint8_t lcd_state_csum;          /* 0x55: cached checksum of gsg_rx for redraw */
uint8_t lcd_timeout_state;        /* 0x4d: timeout marker from lcd_wait_ready */

/* I2C / EEPROM working state (0x52..0x54) */
volatile uint8_t i2c_addr;                /* 0x52 */
uint8_t i2c_shift;               /* 0x53 */
uint8_t i2c_ctrl;                /* 0x54 = 0xA0 | page */

/* assorted bit flags (IDATA bit-addressable byte 0x20) */
volatile bool f_key_pending;    /* _0_0 */
volatile bool f_eeprom_write;   /* _0_3 */
volatile bool f_eeprom_read;    /* _0_4 */
volatile bool f_eeprom_done;    /* _0_5 */
volatile bool f_init;           /* _0_6 (reset to 1) */
volatile bool f_input_ready;    /* _0_7 (set by poll_machine_ready) */

/* ------------------------------------------------------------------ *
 *  Constant tables (CODE)
 * ------------------------------------------------------------------ */
/* Extracted from the EPROM's address table (originally at CODE 0x2cf8/0x2d00).
 * These are ordinary relocatable CODE objects; no __at address is required. */
static const uint8_t gsg_select_table[16] PROGMEM = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80
};

/* Extracted from the EPROM's drive tables (originally at CODE 0x2c0c..0x2c13
 * and 0x2d10..0x2d17). The linker may place these arrays anywhere in CODE. */
static const uint8_t gsg_drive_low_table[8] PROGMEM = {
    0x09, 0x05, 0x0d, 0x03, 0x0b, 0x07, 0x0f, 0x30
};
static const uint8_t gsg_drive_high_table[8] PROGMEM = {
    0x7f, 0xbf, 0xdf, 0xef, 0xf7, 0xfb, 0xfd, 0xfe
};

/* CODE 0x2c33: channel display order is least-significant byte first in the
 * machine latch area, while the LCD renders the eight digits in reverse. */
static const uint8_t lcd_channel_order[8] PROGMEM = {
    7, 6, 5, 4, 3, 2, 1, 0
};

/* Menu strings (German, from CODE) */
static const char S_NO_MACHINE[] PROGMEM =
    "Es ist kein GeldSpielGer\xe1t aktiv !!     Spiel- und Speicher Modul einsetzen !";
static const char S_OLD_KBD_BANNER[] PROGMEM =
    "RDKM HAFG    xxxx xxxx  alte Tastatur   IZSW DSMI                          Ende ";
static const char S_COMPARE_FAIL[] PROGMEM = "F E H L E R  ! Vergleich ";
static const char S_REINIT[] PROGMEM =
    "Das Geld-Spiel-Ger\xe1t wird jetzt          > NEU INITIALISIERT !!! <";
static const char S_EMPTY_TUBES[] PROGMEM = "Alle R\xefhren entleeren !!!";
static const char S_PROFITECH_TITLE[] PROGMEM =
    "Service Tastatur an    PROFITECH 3000";

/* ------------------------------------------------------------------ *
 *  Forward declarations (full set; definitions follow EPROM order)
 * ------------------------------------------------------------------ */
void lcd_cmd(uint8_t mem, uint8_t cmd);
void lcd_putc(uint8_t mem, char c);
void lcd_puts_g(uint8_t mem, const char *s);
void scroll_display_line(uint8_t mem, const char *s);
void clear_and_print(uint8_t mem, const char *s);
void lcd_init_display(void);
char lcd_render_channel_values(uint8_t mem, char addr);
void lcd_position_for_channel(void);
void delay8(uint8_t count);
void delay16(uint8_t hi, uint8_t lo);
uint8_t lcd_wait_ready(void);
void nop_ret(void);
void startup_trap(void);
uint8_t oldkbd_lookup_key_record(uint8_t id);
uint8_t gptr_read_byte(uint8_t mode, const uint8_t *address);
void clear_tx_latches_ff(void);
void fatal_compare_error(void);
void eeprom_write_block(uint8_t addr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4);
void eeprom_read_block(uint8_t addr);
void gsg_bus_idle(void);
void strobe_channel_1(void);
void strobe_selected_channel(void);
void profitech_service_loop(void);
uint8_t gsg_wait_levels(void);
void drive_gsg_channel(void);
void load_gsg_channel_address(void);
void reinit_machine(void);
void scan_keypad_matrix(void);
void poll_machine_ready(void);
void keyswitch_menu(void);
void empty_all_tubes(void);
void ejector_test(void);
void oldkbd_activate_output(void);
void oldkbd_wait_key_release(uint8_t key);
void oldkbd_handle_repeat(uint8_t held);
static void oldkbd_tail(void);
void old_keyboard_mode(void);
void edit_number_screen(uint8_t field, char type);
void settings_acceptor_limit(void);
void serial_game_payout(void);
void settings_miscellaneous(void);
void keypad_test(uint8_t seed);
void machine_output_control(uint8_t field, char mode);
void foultest_run(void);
void foul_test_screen(void);
void clock_date_screen(void);
void endurance_test(void);
void statistics_print_select(uint8_t field, uint8_t arg);
void vat_table_menu(char arg1, uint8_t arg2, char key);
void service_menu_loop(uint8_t mode);
void reset_vars(void);
static void h_edit(void);
static void h_output_toggle(void);
static void h_output_adjust(void);
static void h_table_browser(void);

/* ===== menu dispatch tables & scaffolding ===== */

/* ================================================================== *
 *  Menu dispatch
 * ================================================================== */
/* menu_dispatch_table (CODE 0x00d4): {category, handler}. The category byte
 * groups entries into the top-level menus shown by service_menu_loop. */

/* --------------------------------------------------------------------------
 * Menu page/label table  (fully reverse-engineered from CODE 0x324b)
 * --------------------------------------------------------------------------
 * service_menu_loop indexes this with menu_index: pointer = menu_index*32 + 0x324b. Each
 * 32-byte page = 4 entries of 7 bytes + a 4-byte trailer. An on-ROM entry is:
 *     [0x02, 0x02, label_lo, 0x00, p_hi, p_lo, param]
 * = a CODE label pointer (0x0200|label_lo) + an action:
 *   - p_lo == 0  -> submenu: descend into page 'param'
 *   - else       -> leaf: (p_hi:p_lo) = &menu_dispatch_table[idx].handler = 0x00D5+idx*4,
 *                   so idx = ((p_lo - 0xD5) & 0xFF) / 4, and 'param' is passed to it.
 * The 12 decoded pages below. Pages 6/7/9/10/11 are handler-internal data (the
 * param-driven handlers - machine_output_control, vat_table_menu, settings_acceptor_
 * grenze, endurance_test - render their own sub-screens); only 0,1,2,3,4,5,8 are
 * navigated by service_menu_loop. keypad_test (idx 6) is referenced by no record. */
struct menu_item {
    const char *label;
    uint8_t submenu;            /* target page, or 0xff for a leaf */
    void (*handler)(void);      /* leaf handler */
    uint8_t param;              /* record byte [6], passed to the handler via menu_arg */
};

static const char *menu_label(uint8_t page, uint8_t index);

static uint8_t menu_arg;        /* param of the leaf being invoked (record [6]) */

/* leaf wrappers for handlers that take arguments (taken from menu_arg) */
static void h_edit(void)          { edit_number_screen(0, menu_arg); } /* 1=Hallen,2=Geraete */

static void h_output_toggle(void) { machine_output_control(0, menu_arg); }

static void h_output_adjust(void) { statistics_print_select(0, menu_arg); }

static void h_table_browser(void) { vat_table_menu(2, 0, menu_arg); }
static void h_duration(void)      { endurance_test(); }

static const char menu_text_0[] PROGMEM = "Test";
static const char menu_text_1[] PROGMEM = "Alttast";
static const char menu_text_2[] PROGMEM = "Einstell.";
static const char menu_text_3[] PROGMEM = "Einstell2";
static const char menu_text_5[] PROGMEM = "Foul";
static const char menu_text_6[] PROGMEM = "Uhr/Datum";
static const char menu_text_7[] PROGMEM = "Dauerlauf";
static const char menu_text_8[] PROGMEM = "Auswurf";
static const char menu_text_10[] PROGMEM = "Hallen.NR";
static const char menu_text_11[] PROGMEM = "Ger\xe1te.NR";
static const char menu_text_12[] PROGMEM = "Akzeptor";
static const char menu_text_13[] PROGMEM = "Sonstiges";
static const char menu_text_15[] PROGMEM = " 5.- DM";
static const char menu_text_16[] PROGMEM = " 2.- DM";
static const char menu_text_17[] PROGMEM = " 1.- DM";
static const char menu_text_18[] PROGMEM = " 0.1 DM";
static const char menu_text_20[] PROGMEM = "Ausdruck";
static const char menu_text_21[] PROGMEM = "";
static const char menu_text_22[] PROGMEM = "";
static const char menu_text_23[] PROGMEM = "";
static const char menu_text_25[] PROGMEM = "Serien";
static const char menu_text_26[] PROGMEM = "";
static const char menu_text_27[] PROGMEM = " Werks.";
static const char menu_text_28[] PROGMEM = "MwSteuer";
static const char menu_text_29[] PROGMEM = " 400,-DM";
static const char menu_text_30[] PROGMEM = " 300,-DM";
static const char menu_text_31[] PROGMEM = " 200,-DM";
static const char menu_text_32[] PROGMEM = "";
static const char menu_text_33[] PROGMEM = "";
static const char menu_text_34[] PROGMEM = "";
static const char menu_text_35[] PROGMEM = "";
static const char menu_text_36[] PROGMEM = "";
static const char menu_text_38[] PROGMEM = "Serien";
static const char menu_text_39[] PROGMEM = "Maschine";
static const char menu_text_40[] PROGMEM = " Werks.";
static const char menu_text_41[] PROGMEM = "MwSteuer";
static const char menu_text_42[] PROGMEM = "Maschine";
static const char menu_text_43[] PROGMEM = "Helligkeit";
static const char menu_text_44[] PROGMEM = "";
static const char menu_text_45[] PROGMEM = "Strob.AUS";
static const char menu_text_46[] PROGMEM = "Tempo";
static const char menu_text_47[] PROGMEM = "Dauerlauf";
static const char menu_text_48[] PROGMEM = "ohne Ausz";
static const char menu_text_49[] PROGMEM = "mit Ausz";
static const char menu_text_50[] PROGMEM = "L\xefschen";
static const char menu_text_51[] PROGMEM = "";
static const char menu_text_52[] PROGMEM = "MwSteuer";
static const char menu_text_53[] PROGMEM = "Neuaufstl";
static const char menu_text_54[] PROGMEM = "MwSt+1%";
static const char menu_text_55[] PROGMEM = "MwSt+0,1%";
static const char menu_text_56[] PROGMEM = "";
static const char menu_text_57[] PROGMEM = "";
static const char menu_text_58[] PROGMEM = "F1";
static const char menu_text_59[] PROGMEM = "F2";
static const char menu_text_60[] PROGMEM = "F3";
static const char menu_text_61[] PROGMEM = "F4";
static const char menu_text_62[] PROGMEM = "HOCH";
static const char menu_text_63[] PROGMEM = "OK";
static const char menu_text_64[] PROGMEM = "Links";
static const char menu_text_65[] PROGMEM = "RUNTER";
static const char menu_text_66[] PROGMEM = "RECHTS";
static const struct menu_item menu_pages[12][4] PROGMEM = {
    /* p0 top level */
    { {menu_text_0, 1, 0, 0}, {menu_text_1, 0xff, old_keyboard_mode, 0},
      {menu_text_2, 2, 0, 0}, {menu_text_3, 4, 0, 0} },
    /* p1 Test submenu */
    { {menu_text_5, 0xff, foul_test_screen, 0}, {menu_text_6, 0xff, clock_date_screen, 0},
      {menu_text_7, 0xff, h_duration, 5}, {menu_text_8, 3, 0, 0} },
    /* p2 Einstellungen submenu */
    { {menu_text_10, 0xff, h_edit, 1}, {menu_text_11, 0xff, h_edit, 2},
      {menu_text_12, 0xff, settings_acceptor_limit, 0}, {menu_text_13, 8, 0, 0} },
    /* p3 Auswurf coin selection */
    { {menu_text_15, 0xff, ejector_test, 0}, {menu_text_16, 0xff, ejector_test, 1},
      {menu_text_17, 0xff, ejector_test, 2}, {menu_text_18, 0xff, ejector_test, 3} },
    /* p4 Einstellungen 2 (a) */
    { {menu_text_20, 0xff, h_output_adjust, 0}, {menu_text_21, 5, 0, 0}, {menu_text_22, 5, 0, 0}, {menu_text_23, 5, 0, 0} },
    /* p5 Einstellungen 2 (b) */
    { {menu_text_25, 0xff, serial_game_payout, 0}, {menu_text_26, 5, 0, 0},
      {menu_text_27, 0xff, settings_miscellaneous, 0}, {menu_text_28, 0xff, h_table_browser, 0x0b} },
    /* p6 Akzeptor values (data for settings_acceptor_limit) */
    { {menu_text_29, 0xff, settings_acceptor_limit, 1}, {menu_text_30, 0xff, settings_acceptor_limit, 2},
      {menu_text_31, 0xff, settings_acceptor_limit, 3}, {menu_text_32, 6, 0, 0} },
    /* p7 router (no labels) */
    { {menu_text_33, 1, 0, 0}, {menu_text_34, 2, 0, 0}, {menu_text_35, 7, 0, 0}, {menu_text_36, 3, 0, 0} },
    /* p8 Sonstiges submenu */
    { {menu_text_38, 0xff, serial_game_payout, 0}, {menu_text_39, 0xff, h_output_toggle, 0},
      {menu_text_40, 0xff, settings_miscellaneous, 0}, {menu_text_41, 0xff, h_table_browser, 0x0b} },
    /* p9 menu_text_42 outputs (data for machine_output_control) */
    { {menu_text_43, 0xff, h_output_toggle, 1}, {menu_text_44, 9, 0, 0},
      {menu_text_45, 0xff, h_output_toggle, 3}, {menu_text_46, 0xff, h_output_toggle, 4} },
    /* p10 menu_text_47 modes (data for endurance_test) */
    { {menu_text_48, 0xff, endurance_test, 0}, {menu_text_49, 0xff, endurance_test, 1},
      {menu_text_50, 0xff, endurance_test, 2}, {menu_text_51, 10, 0, 0} },
    /* p11 menu_text_52 variants (data for vat_table_menu) */
    { {menu_text_53, 0xff, h_table_browser, 1}, {menu_text_54, 0xff, h_table_browser, 2},
      {menu_text_55, 0xff, h_table_browser, 3}, {menu_text_56, 0xff, h_table_browser, 4} },
};

static const char *menu_label(uint8_t page, uint8_t index)
{
    return (const char *)pgm_read_ptr(&menu_pages[page][index].label);
}

/* Key-name strings (CODE 0x15c..; indexed by key_code via the 3-byte-record
 * table at 0x309b in the original). */
static const char *const key_names[] PROGMEM = {
    menu_text_57, menu_text_58, menu_text_59, menu_text_60, menu_text_61, menu_text_62, menu_text_63, menu_text_64, menu_text_65, menu_text_66
};

/* ===== functions (EPROM source order) ===== */

/* ------------------------------------------------------------------ *
 *  Retained runtime helpers
 * ------------------------------------------------------------------ */

/* startup_trap (0x002f): terminal error path used by the startup/runtime stubs. */
void startup_trap(void)
{
    for (;;) {
        hardware_service();
    }
}

/* oldkbd_lookup_key_record (0x0031): scan 3-byte records and leave the selected output
 * index in the old-keyboard state. Only F1..F4 have direct output records. */
uint8_t oldkbd_lookup_key_record(uint8_t id)
{
    key_hold_state = id >= T_F1 && id <= T_F4 ? id - T_F1 : 0xff;
    return key_hold_state != 0xff;
}

/* Generic-pointer runtime helpers at 0x0057..0x00d4.  The application uses
 * CODE strings (mode 2); mode 0 and mode 1 retain the ROM's IRAM/XDATA
 * interpretations for callers that pass pointers in those spaces. */

uint8_t gptr_read_byte(uint8_t mode, const uint8_t *address)
{
    return mode == 2 ? pgm_read_byte(address) : *address;
}

/* lcd_cmd (0x04ca): wait, then write a command byte. */
void lcd_cmd(uint8_t mem, uint8_t cmd)
{
    (void)mem;
    lcd_wait_ready();
    hardware_lcd_command(uint8_t(cmd));
}

/* lcd_putc (0x04d8): spin on busy, then write a data byte. */
void lcd_putc(uint8_t mem, char c)
{
    (void)mem;
    while ((hardware_lcd_address() & 0x80) != 0)
        hardware_service();
    hardware_lcd_data(uint8_t(c));
}

/* Generic-pointer string print. The pointer remains relocatable; mem selects
 * the address space used for each byte just as it does in the ROM helper. */
void lcd_puts_g(uint8_t mem, const char *s)
{
    uint8_t c;

    while ((c = gptr_read_byte(mem, (const uint8_t *)s)) != 0) {
        lcd_putc(mem, (char)c);
        s++;
    }
}

/* scroll_display_line (0x04ed): print a string starting at the current DDRAM
 * position (used by callers after positioning the cursor). */
void scroll_display_line(uint8_t mem, const char *s)
{
    lcd_puts_g(mem, s);
}

/* clear_and_print (0x0519): clear display, print a (generic-pointer) string. */
void clear_and_print(uint8_t mem, const char *s)
{
    lcd_cmd(0, HD44780_CMD_CLEAR);
    lcd_puts_g(mem, s);
}

/* lcd_init_display (0x054c): if the channel-latch contents changed (checksum),
 * redraw the value line and update fill-level flags. */
void lcd_init_display(void)
{
    uint8_t csum = 0x11, i;
    for (i = 0; i < 8; i++) {
        csum = (csum & 1) ? ((csum >> 1) | 0x80) : (csum >> 1);
        csum ^= gsg_rx[i];
    }
    if (csum != lcd_state_csum) {
        lcd_state_csum = csum;
        lcd_wait_ready();
        hardware_lcd_command(uint8_t(0x8d));
        gsg_level_flags &= 0xf0;
        for (i = 0; i < 8; i++) {
            uint8_t nib = gsg_rx[pgm_read_byte(&lcd_channel_order[i])] & 0x0f;
            if (nib > 9) {
                if (i == 4 || i == 5 || i == 6)
                    gsg_level_flags++;
                else
                    gsg_level_flags = 4;
            }
            lcd_wait_ready();
            hardware_lcd_data(uint8_t(('0' + nib)));
            if (i == 3)
                lcd_putc(0, ' ');
        }
        lcd_position_for_channel();
    }
}

/* lcd_render_channel_values (0x05fb): set DDRAM addr and render 8 digits from the
 * channel-latch nibbles (gsg_rx[]) as hex/decimal characters (table @0x2c13). */
char lcd_render_channel_values(uint8_t mem, char addr)
{
    static const char digit_tbl[] PROGMEM = "0123456789:;<=>?"; /* 0x13.. region */
    uint8_t i;
    (void)mem;
    lcd_wait_ready();
    hardware_lcd_command(uint8_t(addr + 0x80));
    for (i = 0; i < 8; i++) {
        uint8_t nib = gsg_rx[pgm_read_byte(&lcd_channel_order[i])] & 0x0f;
        lcd_wait_ready();
        hardware_lcd_data(uint8_t(pgm_read_byte(&digit_tbl[nib])));
        if (i == 3)
            lcd_putc(0, ' ');
    }
    return 0;
}

/* lcd_position_for_channel (0x064f): emit a sequence of LCD positioning/shift commands
 * selected by gsg_channel (used by lcd_init_display after refreshing data). */
void lcd_position_for_channel(void)
{
    lcd_cmd(0, 0x0d);            /* display on, cursor off, blink on */
    lcd_cmd(0, HD44780_CMD_HOME);
    if (gsg_channel != 0 && gsg_channel < 9) {
        if (gsg_channel < 5)
            lcd_cmd(0, gsg_channel + 0x7f);
        else
            lcd_cmd(0, gsg_channel + 0x80);
    }
    if (gsg_channel > 8 && gsg_channel < 0x11) {
        if (gsg_channel < 0xd)
            lcd_cmd(0, gsg_channel + 0xb7);
        else
            lcd_cmd(0, gsg_channel + 0xb8);
    }
}

/* ================================================================== *
 *  Timing
 * ================================================================== */
/* delay8: byte-count busy-wait (CODE 0x06b3). */
void delay8(uint8_t count) { hardware_pause(count); }

/* delay16: 16-bit busy-wait (CODE 0x06c4). When machine_mode (bank3 R5 @0x1d)
 * == 1 (PROFITECH) the original unconditionally forces the count to 3, so all
 * delays collapse to near-instant in PROFITECH mode (the machine drives timing
 * over the bus/ISR instead). This runs on every call. */
void delay16(uint8_t hi, uint8_t lo)
{
    hardware_pause(machine_mode == 1 ? 3 : ((uint16_t)hi << 8) | lo);
}

/* ================================================================== *
 *  HD44780 LCD
 * ================================================================== */

/* lcd_wait_ready (0x06ee): poll busy flag with the EPROM's two-byte timeout. */
uint8_t lcd_wait_ready(void)
{
    hardware_service();
    return 0; // Shield R/W is grounded; the LCD driver enforces write delays.
}

/* nop_ret (0x0717): placeholder/sync point (does nothing). */
void nop_ret(void) { }

/* clear_tx_latches_ff (0x0775): on NAK the bank-3 transmit bytes are set to 0xFF. */
void clear_tx_latches_ff(void)
{
    gsg_tx[0] = gsg_tx[1] = gsg_tx[2] = gsg_tx[3] = gsg_tx[4] = 0xff;
}

/* fatal_compare_error (0x0785): show "FEHLER Vergleich" and hang. */
void fatal_compare_error(void)
{
    clear_and_print(2, S_COMPARE_FAIL);
    for (;;)
        hardware_service();
}

/* ================================================================== *
 *  Disabled EEPROM service requests
 * ================================================================== */

/* EEPROM writes are discarded; service flags are completed by the caller. */
void eeprom_write_block(uint8_t addr, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4)
{
    (void)addr; (void)b1; (void)b2; (void)b3; (void)b4;
    // EEPROM intentionally omitted: acknowledge service completion, store nothing.
}

/* Preserve the address echo and return erased bytes without accessing EEPROM. */
void eeprom_read_block(uint8_t addr)
{
    i2c_addr = uint8_t(addr << 2);
    gsg_tx[0] = addr;
    gsg_tx[1] = gsg_tx[2] = gsg_tx[3] = gsg_tx[4] = 0xff;
}

/* gsg_bus_idle (0x0a7e): release GSG bus. */
void gsg_bus_idle(void)
{
    legacy_gsg2 = 0xff;
    legacy_gsg3 = 0xff;
    gsg_tx[0] = 0;
    gsg_tx[1] = 0;
}

/* strobe_channel_1 (0x0a8f): force channel 1, then strobe. */
void strobe_channel_1(void)
{
    gsg_channel = 1;
    strobe_selected_channel();
}

/* strobe_selected_channel (0x0aa7): select current channel, settle, release, settle. */
void strobe_selected_channel(void)
{
    load_gsg_channel_address();
    delay16(0x0d, 0x48);
    gsg_bus_idle();
    delay16(0x0d, 0x48);
}

/* ================================================================== *
 *  Main loop
 * ================================================================== */

/* profitech_service_loop (0x0abc): machine_mode==1 service loop. Polls the
 * keypad, forwards key changes, and services EEPROM read/write requests raised
 * by the INT1 handler via the flag bits. */
void profitech_service_loop(void)
{
    service_key_latch = T_Keine;
    do {
        if (last_key != key_code && service_key_latch == key_code) {
            last_key = key_code;
            f_key_pending = 1;
        }
        scan_keypad_matrix();
        service_key_latch = key_code;
        if (f_eeprom_read) {
            eeprom_read_block(i2c_addr);
            f_eeprom_done = 1;
            f_eeprom_read = 0;
        }
        if (f_eeprom_write) {
            eeprom_write_block(i2c_addr, gsg_tx[1], gsg_tx[2], gsg_tx[3], gsg_tx[4]);
            f_eeprom_write = 0;
        }
        scan_keypad_matrix();
    } while (machine_mode == 1);
}

/* gsg_wait_levels (0x0af1): old-kbd only. Cycle channels 0xd/0xe/0xf and pulse
 * the bus until each channel's status low-nibble drops below threshold. */
uint8_t gsg_wait_levels(void)
{
    uint8_t saved = gsg_channel;
    uint8_t v = (uint8_t)(machine_mode ^ 2);
    if (machine_mode == 2) {
        gsg_channel = 0xd;
        /* The ROM's CODE table maps selected channels D/E/F to bank-2 R3/R2/R1,
         * i.e. receive slots 3/2/1.  They are not receive slots D/E/F. */
        while ((v = (gsg_rx[3] & 0x0f)) > 10) {
            load_gsg_channel_address(); delay16(0, 0xe8);
            gsg_tx[0] = 0; gsg_tx[1] = 0; delay16(0, 0xe8);
        }
        gsg_channel = 0xe;
        while ((v = (gsg_rx[2] & 0x0f)) > 10) {
            load_gsg_channel_address(); delay16(0, 0xe8);
            gsg_tx[0] = 0; gsg_tx[1] = 0; delay16(0, 0xe8);
        }
        gsg_channel = 0xf;
        while ((v = (gsg_rx[1] & 0x0f)) >= 0x0b) {
            load_gsg_channel_address(); delay16(0, 0xe8);
            gsg_tx[0] = 0; gsg_tx[1] = 0; delay16(0, 0xe8);
        }
        v = (uint8_t)(v - 0x0b);
    }
    gsg_channel = saved;
    gsg_level_flags = 0xf0;
    return v;
}

/* drive_gsg_channel (0x0bbb): drive the channel-specific output pattern onto
 * GSG2 (channels 1..8) or GSG3 (channels 9..16), echoing it to I2C_CTRL. */
void drive_gsg_channel(void)
{
    if (gsg_channel < 9) {
        uint8_t pat = pgm_read_byte(&gsg_drive_low_table[gsg_channel - 1]);
        legacy_gsg3 = 0xff;
        legacy_gsg2 = pat;
        legacy_i2c_control = legacy_gsg2;
        if (gsg_channel == 1)
            gsg_level_flags = 0;
    } else {
        uint8_t pat = pgm_read_byte(&gsg_drive_high_table[(gsg_channel - 9) & 7]);
        legacy_gsg2 = 0xff;
        legacy_gsg3 = pat;
        legacy_i2c_control = legacy_gsg3;
    }
}

/* ================================================================== *
 *  GSG parallel bus
 * ================================================================== */

/* load_gsg_channel_address (0x0c07): load the address-table byte selected by
 * gsg_channel into the bank-3 R0:R1 output pair. */
void load_gsg_channel_address(void)
{
    uint8_t pat = pgm_read_byte(&gsg_select_table[(gsg_channel - 1) & 0x0f]);
    if (gsg_channel < 9) {
        gsg_tx[0] = 0;
        gsg_tx[1] = pat;
        if (gsg_channel == 1)
            gsg_level_flags = 0;
    } else {
        gsg_tx[1] = 0;
        gsg_tx[0] = pat;
    }
}

/* ================================================================== *
 *  Machine control helpers
 * ================================================================== */

/* reinit_machine (0x0c3d): pulse a special GSG pattern and show the
 * "NEU INITIALISIERT" banner; F2+HOCH works before machine detection too. */
void reinit_machine(void)
{
    const bool recovery = machine_mode == 3;
    gsg_tx[0] = 0x81;
    gsg_tx[1] = 0x81;
    legacy_gsg2 = 0x7e;
    legacy_gsg3 = 0xff;
    clear_and_print(2, S_REINIT);
    // Detection may resume mid-pulse. Keep the full recovery interval even
    // if the ISR switches to PROFITECH, whose normal delays are shortened.
    if (recovery) hardware_pause(0xfde8);
    else delay16(0xfd, 0xe8);
    gsg_tx[0] = 0;
    gsg_tx[1] = 0;
    legacy_gsg2 = 0xff;
    legacy_gsg3 = 0xff;
    clear_and_print(2, S_OLD_KBD_BANNER);
}

/* ================================================================== *
 *  Keypad
 * ================================================================== */
/* scan_keypad_matrix (0x0c7d): read the shield's selected virtual key/chord. */
void scan_keypad_matrix(void)
{
    hardware_service();
    key_code = hardware_key();
}

/* poll_machine_ready (0x0d22): select channel 2, strobe + read; set f_input_ready
 * when gsg_rx[6] reports ready (0x0f); strobe channel 5 if the 1..3 checksum
 * exceeds a threshold. */
void poll_machine_ready(void)
{
    gsg_channel = 2;
    strobe_selected_channel();
    delay16(0x07, 0xd0);
    nop_ret();
    f_input_ready = (gsg_rx[6] == 0x0f) ? 1 : 0;
    if (gsg_rx[6] == 0x0f)
        strobe_selected_channel();
    if ((uint16_t)gsg_rx[2] + gsg_rx[1] + gsg_rx[3] >= 0x1e) {
        gsg_channel = 5;
        strobe_selected_channel();
    }
    strobe_channel_1();
}

/* keyswitch_menu (0x0d6b): "Schluesselschalterfunktion" screen. Shows
 * "1234 5678 >>>        <<<<", live-refreshes the channel digits, and on a key
 * change looks up the key's action and returns; on Hoch it flashes the keyswitch
 * outputs (GSG3 0xfd/0x7d patterns) a few times, then exits. Reached from
 * settings_miscellaneous on the F1+F4 chord. */
void keyswitch_menu(void)
{
    gsg_tx[0] = 0;
    lcd_state_csum = 0;
    clear_and_print(2, PSTR("1234 5678 >>>        <<<<"));
    lcd_cmd(0, 0x18);                 /* shift display left */
    scroll_display_line(2, PSTR(""));
    lcd_cmd(0, 0x0d);                 /* display on, cursor off, blink on */
    gsg_channel = 1;
    last_key = key_code;
    gsg_tx[1] = 8;
    do {
        scan_keypad_matrix();
        if (last_key != key_code) {
            last_key = key_code;
            /* oldkbd_lookup_key_record(key_code): resolve the key's action record */
            if (oldkbd_lookup_key_record(key_code))
                clear_tx_latches_ff(); /* only for a mapped key */
            legacy_i2c_control = 4;
            return;
        }
        lcd_init_display();
        if (key_code == T_Keine)
            gsg_tx[1] = 8;
    } while (key_code != T_Hoch);

    lcd_cmd(0, 0x0c);                 /* display on, cursor off, blink off */
    gsg_bus_idle();
    lcd_cmd(0, HD44780_CMD_CLEAR);
    /* flash the keyswitch outputs a few times */
    gsg_tx[0] = 0x40; legacy_gsg3 = 0xfd; delay16(0x0d, 0x48);
    gsg_tx[0] = 0x41; legacy_gsg3 = 0x7d; gsg_tx[1] = 0x01; delay16(0x0d, 0x48);
    gsg_tx[0] = 0x40; legacy_gsg3 = 0xfd; delay16(0x0d, 0x48);
    gsg_tx[0] = 0x41; legacy_gsg3 = 0x7d; gsg_tx[1] = 0x01; delay16(0x0d, 0x48);
    gsg_tx[0] = 0x40; legacy_gsg3 = 0xfd; delay16(0x0d, 0x48);
    do {
        scan_keypad_matrix();
    } while (key_code != T_Keine);
    scroll_display_line(2, PSTR(""));
    lcd_cmd(0, 0x18);
    scroll_display_line(2, PSTR(""));
}

/* empty_all_tubes (0x0f20): "Alle Roehren entleeren" - cycle hopper channels
 * 5..8, holding each active until a key is pressed; exit on Hoch or F4. */
void empty_all_tubes(void)
{
    for (;;) {
        gsg_channel = 5;
        while (gsg_channel < 9 && (key_code == T_Keine || key_code == T_F1_F4)) {
            uint8_t i;
            load_gsg_channel_address();
            for (i = 0; i < 0xfa; i++) {
                delay16(0x03, 0xe8);
                scan_keypad_matrix();
                if (key_code != T_Keine && key_code != T_F1_F4)
                    break;
            }
            gsg_bus_idle();
            delay16(0x03, 0xe8);
            gsg_channel++;
        }
        scan_keypad_matrix();
        if (key_code == T_Hoch || key_code == T_F4)
            return;
    }
}

/* (0x0f79): "Auswurf wird betaetigt" ejector test (mostly traced). */
void ejector_test(void)
{
    uint8_t coin = menu_arg;        /* 0..3 = 5/2/1/0.1 DM (record byte [6]) */
    clear_and_print(2, PSTR("Auswurf wird bet\xe1tigt,"));
    lcd_cmd(0, HD44780_CMD_FUNCTION_SET | HD44780_FUNCTION_8BIT | HD44780_FUNCTION_2LINE | HD44780_FUNCTION_5X7DOTS);
    scroll_display_line(2, PSTR("Funktionstaste halten f\xfcr Wiederholung"));
    if (f_init) {
        f_init = 0;
        gsg_channel = 5; strobe_selected_channel();
        gsg_wait_levels();
        delay16('N', ' ');
        gsg_channel = 5; strobe_selected_channel();
    }
    lcd_cmd(0, 0x99);
    if (coin < 4)
        lcd_puts_g(2, menu_label(3, coin));
    delay16(0x05, 0xdc);
    gsg_bus_idle();
    delay16(0x3a, 0x98);
    gsg_channel = coin + 5;          /* ejector channel for the selected coin */
    load_gsg_channel_address();
    do {
        scan_keypad_matrix();
        if (coin + T_F1 != key_code) break;   /* hold the coin's F-key to repeat */
    } while (coin != 0xff);
    oldkbd_tail();
}

/* oldkbd_activate_output (0x1011): drive the GSG channel for the held key
 * (auto-repeat) until released. */
void oldkbd_activate_output(void)
{
    delay16(0x3a, 0x98);
    gsg_channel = key_hold_state + 5;
    load_gsg_channel_address();
    do {
        scan_keypad_matrix();
        if (key_hold_state + T_F1 != key_code)
            break;
    } while (key_hold_state != 0xff);
    oldkbd_tail();
}

/* oldkbd_wait_key_release (0x1028): spin while 'key' is still held. */
void oldkbd_wait_key_release(uint8_t key)
{
    while (key == key_code) {
        scan_keypad_matrix();
        key = key_hold_state + T_F1; /* tracks held function key */
    }
    oldkbd_tail();
}

/* oldkbd_handle_repeat (0x1037): shared EPROM entry used by held-key paths.
 * It waits for the key state to change, handles the F1+F4 tube-empty chord,
 * then releases the bus after the two settle delays. */
void oldkbd_handle_repeat(uint8_t held)
{
    do {
        if (held)
            break;
        scan_keypad_matrix();
        held = (uint8_t)(0xfe < key_hold_state);
    } while ((uint8_t)(key_hold_state + T_F1) == key_code);
    if (key_code == T_F1_F4) {
        clear_and_print(2, S_EMPTY_TUBES);
        empty_all_tubes();
    }
    delay16(0xb5, 0xa4);
    gsg_bus_idle();
    delay16(0xb5, 0xa4);
}

/* Shared tail of the old-keyboard key-hold handlers (0x1037): on F1+F4 empty
 * the tubes, then settle + release the bus. */
static void oldkbd_tail(void)
{
    if (key_code == T_F1_F4) {
        clear_and_print(2, S_EMPTY_TUBES);
        empty_all_tubes();
    }
    delay16(0xb5, 0xa4);
    gsg_bus_idle();
    delay16(0xb5, 0xa4);
}

/* old_keyboard_mode (0x1065): "alte Tastatur" (old-keyboard) handler - polls the
 * keypad, refreshes the channel display, and on a key change resolves the key's
 * action and drives the corresponding old-keyboard output (auto-repeat). */
void old_keyboard_mode(void)
{
    uint8_t changed_key;

    clear_and_print(2, S_OLD_KBD_BANNER);
    lcd_cmd(0, 0x0f);            /* display on, cursor on, blink on */
    lcd_state_csum = 0;
    for (;;) {
        scan_keypad_matrix();
        changed_key = (uint8_t)(last_key ^ key_code);
        if (changed_key != T_Keine)
            break;
        lcd_init_display();
        if (gsg_level_flags == 3)
            gsg_wait_levels();
        delay16(0, 200);
        if (key_code == T_Keine)
            gsg_bus_idle();
    }
    last_key = key_code;
    oldkbd_lookup_key_record(key_code);
    oldkbd_wait_key_release(changed_key);
    oldkbd_activate_output();
}

/* edit_number_screen (0x119a): "Nr:" 6-digit Hallen-/Geraete-Nummer entry.
 * Reads the value over the GSG bus (channel 3, plus channel 2 when type==2),
 * shows it as 6 digits (gsg_rx[5..0] + '0'), and edits via the keypad:
 *   Links/OK/Rechts move the cursor (1..6), Hoch/Runter strobe channel
 *   field+2 / field+10 to inc/dec the selected digit, F4 exits. */
void edit_number_screen(uint8_t field, char type)
{
    uint8_t cursor = 1;
    (void)field;

    lcd_cmd(0, HD44780_CMD_CLEAR);
    if (type == 1)
        scroll_display_line(2, PSTR("  Einstellungen-Hallen-Nummer :"));
    else
        scroll_display_line(2, PSTR("  Einstellungen-Ger\xe1te-Nummer:"));

    gsg_channel = 3;
    strobe_selected_channel();
    delay16(0x0d, 0x48);
    while (gsg_rx[6] != 0x0a)
        hardware_service();
    if (type == 2) {
        delay16('N', ' ');
        gsg_channel = 2;
        delay16(0x0d, 0x48);
        strobe_selected_channel();
        while (gsg_rx[7] != 0x0a)
            hardware_service();
    }

    lcd_cmd(0, 0x0f);                 /* display on, cursor on, blink on */
    do {
        lcd_cmd(0, 0x18);            /* shift display left */
        lcd_putc(0, 'N'); lcd_putc(0, 'r'); lcd_putc(0, ':'); lcd_putc(0, ' ');
        lcd_putc(0, gsg_rx[5] + '0');
        lcd_putc(0, gsg_rx[4] + '0');
        lcd_putc(0, gsg_rx[3] + '0');
        lcd_putc(0, gsg_rx[2] + '0');
        lcd_putc(0, gsg_rx[1] + '0');
        lcd_putc(0, gsg_rx[0] + '0');
        lcd_cmd(0, cursor + 0x80 - 0x14);   /* position cursor over selected digit */

        do {
            scan_keypad_matrix();
        } while (key_code == last_key);
        last_key = key_code;

        if (key_code == T_Links) {
            if (cursor > 1) cursor--;
        } else if (key_code == T_OK || key_code == T_Rechts) {
            if (cursor < 6) cursor++;
        } else if (key_code == T_Hoch) {
            gsg_channel = cursor + 2;
            strobe_selected_channel();
        } else if (key_code == T_Runter) {
            gsg_channel = cursor + 10;
            strobe_selected_channel();
        }
    } while (key_code != T_F4);

    lcd_cmd(0, HD44780_CMD_DISPLAY_CTRL | HD44780_DISPLAY_ON); /* cursor off */
    strobe_channel_1();
}

/* settings_acceptor_limit (0x12f6): "Akzeptorsperrgrenze" (acceptor lock-limit)
 * settings. Probes the machine (GSG channels 0xf then 5). If it has no
 * configurable limits, shows that message and bails. Otherwise lets the operator
 * pick one of three limit values with Links/Rechts/F1-F3 (200/300/400 DM, or the
 * high range 350/450/550 DM when the machine reports the 5-DM variant), strobes
 * channel 6 to apply, then shows the resulting "Fuellhoehe" until confirmed with
 * OK. Finishes with a "Bitte warten" delay. */
void settings_acceptor_limit(void)
{
    static const char digit_tbl[] PROGMEM = "0123456789:;<=>?";
    uint8_t sel, i;
    uint8_t no_limits = 0;   /* _1_4: machine has no acceptor limits */
    uint8_t range_hi = 0;    /* _1_1: high value range (350/450/550) */

    gsg_bus_idle();
    clear_and_print(2, PSTR("Einstell.:Akzeptorsperrgrenze"));
    gsg_channel = 0xf;
    load_gsg_channel_address();
    delay16(0x0f, 0xa0);
    do {
        if (gsg_rx[2] != 0) break;
    } while (gsg_rx[3] == 0);
    gsg_bus_idle();
    delay16(0x1a, 0x90);
    gsg_channel = 5;
    load_gsg_channel_address();
    delay16(0x1a, 0x90);
    do {
        if ((uint16_t)gsg_rx[2] + gsg_rx[1] + gsg_rx[3] >= 0x1e)
            no_limits = 1;
    } while (gsg_rx[3] != 10 && !no_limits);
    gsg_bus_idle();
    do {
        if ((uint16_t)gsg_rx[2] + gsg_rx[1] + gsg_rx[3] >= 0x1e)
            no_limits = 1;
        sel = gsg_rx[2];
    } while ((gsg_rx[2] < 2 || gsg_rx[2] > 5) && !no_limits);

    if (!no_limits) {
        scan_keypad_matrix();
        lcd_cmd(0, 0x18);
        if (gsg_rx[1] == 5) {
            scroll_display_line(2, PSTR("  350,-DM   450,-DM   550,-DM      Ende"));
            range_hi = 1;
            sel--;
        } else {
            scroll_display_line(2, PSTR("  200,-DM   300,-DM   400,-DM      Ende"));
        }
        do {
            if (key_code != last_key) {
                last_key = key_code;
                if (key_code == T_Links) {
                    if (sel > 2) sel--;
                } else if (key_code == T_Rechts) {
                    if (sel < 4) sel++;
                } else if (key_code >= T_F1 && key_code <= T_F3) {
                    sel = key_code + 1;          /* key_code + T_F1 maps F1->2.. */
                }
            }
            lcd_cmd(0, 0x18);
            /* strobe channel 6 until the machine echoes the chosen value */
            if (!range_hi) {
                do {
                    if (gsg_rx[2] != sel) { gsg_channel = 6; strobe_selected_channel(); }
                    delay16(0x27, 0x10);
                } while (gsg_rx[2] != sel);
                scroll_display_line(2, PSTR("  200,-DM   300,-DM   400,-DM      Ende"));
            } else {
                do {
                    if ((uint8_t)(sel + 1) != gsg_rx[2] || sel == 0xff) {
                        gsg_channel = 6; strobe_selected_channel();
                    }
                } while ((uint8_t)((sel + 1) ^ gsg_rx[2]) != 0);
                scroll_display_line(2, PSTR("  350,-DM   450,-DM   550,-DM      Ende"));
            }
            scan_keypad_matrix();
            /* bracket the selected value with two block chars on line 2 at
             * columns (sel-2)*10 + 1 and +9 (DDRAM 0xc1/0xc9 + (sel-2)*10) */
            lcd_cmd(0, 0xc1 + (uint8_t)((sel - 2) * 10));
            lcd_putc(0, (char)0xff);
            lcd_cmd(0, 0xc9 + (uint8_t)((sel - 2) * 10));
            lcd_putc(0, (char)0xff);
            delay16(0x0d, 0x48);
        } while (key_code != T_F4);

        gsg_channel = 5;
        load_gsg_channel_address();
        lcd_cmd(0, HD44780_CMD_CLEAR);
        delay16(0x1a, 0x90);
        lcd_cmd(0, 0x18);
        scroll_display_line(2, PSTR(""));
        gsg_bus_idle();
        do {
            lcd_cmd(0, 0x80);
            scroll_display_line(2, PSTR("F\xfcllh\xefhe : "));
            for (i = 5; i < 8; i++)
                lcd_putc(0, pgm_read_byte(&digit_tbl[gsg_rx[i] & 0x0f]));
            scroll_display_line(2, PSTR(",-DM    mit OK einstellen"));
            delay16(0, 200);
            scan_keypad_matrix();
            if (key_code != last_key) {
                last_key = key_code;
                if (key_code == T_OK) { gsg_channel = 6; strobe_selected_channel(); }
                gsg_bus_idle();
            }
        } while (gsg_rx[2] != 0);
    } else {
        gsg_bus_idle();
        clear_and_print(2, PSTR("Bei diesem Ger\xe1t keine Akzeptorgrenzen"));
        gsg_channel = 5; strobe_selected_channel();
        delay16(0x07, 0xd0);
        gsg_channel = 1; strobe_selected_channel();
        delay16(0x1a, 0x90);
        if ((uint16_t)gsg_rx[2] + gsg_rx[1] + gsg_rx[3] != 0) {
            gsg_channel = 5; strobe_selected_channel();
            delay16(0x1a, 0x90);
        }
    }

    clear_and_print(2, PSTR("Bitte warten !"));
    delay16('}', 0); scroll_display_line(2, PSTR(""));
    delay16('}', 0); scroll_display_line(2, PSTR(""));
    delay16('}', 0); scroll_display_line(2, PSTR(""));
    delay16('}', 0);
    menu_index = 0;
}

/* serial_game_payout (0x15ea): "Serienspiele aufbuchen" payout screen. Selects GSG
 * channel 0xb, waits for the machine to settle, then F1/F2/F3 choose a count
 * (1/10/50) of serial games to book; each is dispensed with a strobe and a
 * progress block on the LCD. F4 aborts the current run, Hoch exits. */
void serial_game_payout(void)
{
    uint8_t count;
    clear_and_print(2, PSTR("Bitte warten !"));
    delay16(0x05, 0xdc);
    gsg_channel = 0xb;
    strobe_selected_channel();
    gsg_wait_levels();
    do {
        delay16(0x05, 0xdc);
        gsg_channel = 0xb;
        strobe_selected_channel();
        delay16(0x05, 0xdc);
    } while (gsg_rx[2] == 0x0a);

    lcd_cmd(0, 0x80);
    scroll_display_line(2, PSTR("Serienspiele aufbuchen !             "));
    lcd_cmd(0, 0x18);
    scroll_display_line(2, PSTR("  + 1       +10       +50        Stop"));
    do {
        scan_keypad_matrix();
        if (key_code != last_key) {
            last_key = key_code;
            count = 0;
            if (key_code == T_F1) count = 1;
            else if (key_code == T_F2) count = 10;
            else if (key_code == T_F3) count = 0x32;
            lcd_cmd(0, 0x9e);
            scroll_display_line(2, PSTR("Warten!"));           /* CODE 0x302d */
            for (; count != 0 && count < 0x40; count--) {
                strobe_selected_channel();
                lcd_cmd(0, 0x9d);
                lcd_putc(0, (char)0xff);       /* full-block progress char */
                delay16(0x0d, 0xac);
                delay16(0x1b, 'X');
                lcd_cmd(0, 0x9d);
                lcd_putc(0, ' ');
                delay16(0x09, 0xc4);
                scan_keypad_matrix();
                if (key_code == T_F4)
                    count = 0;                 /* abort */
                else
                    delay16(0x1b, 'X');
            }
            lcd_cmd(0, 0x9e);
            for (count = 0; count < 10; count++)
                lcd_putc(0, ' ');
        }
    } while (key_code != T_Hoch);
    strobe_channel_1();
}

/* settings_miscellaneous (0x1704): "Einstell.:Sonstiges:Werks." settings screen.
 * Selects GSG channel 0xf, waits, then F1..F4 drive distinct GSG3 patterns
 * (relay/strobe combinations); F1+F4 opens keyswitch_menu; Hoch exits. */
void settings_miscellaneous(void)
{
    clear_and_print(2, PSTR("Einstell.:Sonstiges:Werks."));
    gsg_channel = 0xf;
    load_gsg_channel_address();
    do {
        lcd_render_channel_values(0, 0x1e);
    } while (gsg_rx[2] == 0);
    delay16('N', ' ');
    gsg_bus_idle();
    lcd_render_channel_values(0, 0x1e);
    delay16('N', ' ');
    lcd_cmd(0, 0x18);
    scroll_display_line(2, PSTR("M\xfcnzG/Z.   Li&EMP   Auff\xfcllpr Sicherheit"));
    gsg_tx[0] = 0x40;
    legacy_gsg3 = 0xfd;                            /* CODE 0x2d16 */
    do {
        lcd_render_channel_values(0, 0x1e);
        scan_keypad_matrix();
        if (key_code == T_F1) { gsg_tx[0] = 0x41; legacy_gsg3 = 0x7d; }
        if (key_code == T_F2) { gsg_tx[0] = 0x42; legacy_gsg3 = 0xbd; }
        if (key_code == T_F3) { gsg_tx[0] = 0x44; legacy_gsg3 = 0xed; }
        if (key_code == T_F4) { gsg_tx[0] = 0x48; legacy_gsg3 = 0xed; }
        if (key_code == T_Keine) { gsg_tx[0] = 0x40; legacy_gsg3 = 0xfd; }
        if (key_code == T_F1_F4)
            keyswitch_menu();
    } while (key_code != T_Hoch);
    gsg_bus_idle();
    lcd_cmd(0, 0x18);
    scroll_display_line(2, PSTR("mit Service-Schalter bet\xe1tigen Ende")); /* CODE 0x2f22 */
    while (gsg_rx[2] != 0)
        hardware_service();
    menu_index = 2;
}

/* keypad_test (0x1d99): "Test" keypad screen. Shows the seed key 'F<n>', then
 * on each press displays the key's numeric code and its name; F1+F4 and F2+HOCH
 * chords are spelled out. Waits for release between presses. */
void keypad_test(uint8_t seed)
{
    lcd_cmd(0, HD44780_CMD_CLEAR);
    lcd_putc(0, 'F');
    lcd_putc(0, seed + '0');
    delay16(0xc3, 'P');
    scan_keypad_matrix();
    if (key_code != T_Keine) {
        lcd_cmd(0, 0x8a);                       /* line 1, col 10 */
        /* (label printed here in original) */
        if (key_code < 10) {
            lcd_putc(0, key_code + '0');
        } else {
            lcd_putc(0, key_code / 10 + '0');
            lcd_putc(0, key_code % 10 + '0');
        }
        lcd_cmd(0, 0x94);                       /* line 2 */
        if (key_code == T_F1_F4) {
            lcd_puts_g(2, PSTR("F1+F4"));
        } else if (key_code == T_F2_Hoch) {
            lcd_puts_g(2, PSTR("F2+HOCH"));
        } else if (key_code >= T_F1 && key_code <= T_Rechts) {
            lcd_puts_g(2, (const char *)pgm_read_ptr(&key_names[key_code]));
        }
        delay16(0xc3, 'P');
        do {
            scan_keypad_matrix();
        } while (key_code != T_Keine);
    }
}

/* machine_output_control (0x1ec6): Test-menu handler. Manually drives a single GSG
 * output bit (patterns from CODE 0x2cf8: 0x01, 0x2cf9: 0x02) under F1/F3/F4;
 * exits on Hoch. mode==0 = brief init pulse, else interactive loop. */
void machine_output_control(uint8_t field, char mode)
{
    (void)field;
    if (mode == 0) {
        gsg_tx[0] = 0;    gsg_tx[1] = 0x01;
        delay16(0x0d, 0x48);
        gsg_tx[0] = 0x01; gsg_tx[1] = 0x01;
        delay16(0x0d, 0x48);
        gsg_tx[0] = 0;
        lcd_render_channel_values(0, 0x1f);
        menu_index = 9;
    } else {
        do {
            lcd_render_channel_values(0, 0x1f);
            scan_keypad_matrix();
            if (key_code == T_F1)
                gsg_tx[0] = 0x02;
            if (key_code == T_F3) {
                do {
                    gsg_tx[0] = 0x01; delay16(0x0d, 0x48);
                    gsg_tx[0] = 0;    delay16(0x0d, 0x48);
                } while (gsg_rx[6] != 0x0a);
            }
            if (key_code == T_F4)
                gsg_tx[0] = 0x01;
            if (key_code == T_Keine)
                gsg_tx[0] = 0;
        } while (key_code != T_Hoch);
        strobe_channel_1();
        gsg_tx[0] = 0;
        gsg_tx[1] = 0;
        menu_index = 8;
    }
}

/* foultest_run (0x1f6a): Foultest worker (called from foul_test_screen). Drives the GSG
 * bus, waits for the machine's status bytes, verifies a 4-byte checksum (the
 * received bytes must sum to 0). On pass it shows the OK result; otherwise it
 * enters an interactive loop: F1 pulses the foul output (unless the special
 * "all 8" pattern was seen, _1_0), F3 runs an auto sequence, F4/Hoch exit. */
void foultest_run(void)
{
    f_eeprom_done = 0; /* _1_0 cleared (special-pattern flag) */
    if (machine_mode == 2)
        load_gsg_channel_address();
    else
        drive_gsg_channel();
    delay16(0x0d, 0x48);
    while (gsg_rx[7] != 0x0e)
        hardware_service();
    legacy_gsg2 = 0xff;
    legacy_gsg3 = 0xff;
    delay16(0x13, 0x88);
    while (gsg_rx[6] != 0x0e)
        hardware_service();
    delay16(0x13, 0x88);
    lcd_cmd(0, 0x80);
    scroll_display_line(2, PSTR("Fehler speicher:"));      /* CODE 0x3111 */
    delay16(0x13, 0x88);

    if ((uint16_t)gsg_rx[0] + gsg_rx[1] + gsg_rx[2] + gsg_rx[3] != 0)
        delay16(0, 'd');

    if ((uint16_t)gsg_rx[0] + gsg_rx[1] + gsg_rx[2] + gsg_rx[3] == 0) {
        /* checksum OK -> error memory empty */
        scroll_display_line(2, PSTR("ist leer !"));        /* CODE 0x3122 */
        delay16(0xfd, 0xe8);
        delay16(0xfd, 0xe8);
        strobe_channel_1();
        delay16(0xfd, 0xe8);
    } else {
        /* checksum mismatch -> interactive foul control */
        lcd_cmd(0, 0x18);
        scroll_display_line(2, PSTR("siehe Foul-Tabelle !"));
        do {
            lcd_render_channel_values(0, 0x17);
            scan_keypad_matrix();
            if (gsg_rx[1] == 8 && gsg_rx[3] == 8 && gsg_rx[2] == 8 && gsg_rx[4] == 8) {
                f_eeprom_done = 1;          /* _1_0 = special pattern seen */
                lcd_cmd(0, 0xd5);
                scroll_display_line(2, PSTR("L\xefschen"));      /* CODE 0x3156 */
            }
            if (key_code != last_key) {
                last_key = key_code;
                if (key_code == T_F1) {
                    if (!f_eeprom_done) {
                        gsg_channel = 7;
                        if (machine_mode == 2)
                            load_gsg_channel_address();
                        else
                            drive_gsg_channel();
                        delay16(0x0d, 0x48);
                        do {
                            scan_keypad_matrix();
                        } while (key_code == T_F1);
                        gsg_bus_idle();
                    } else {
                        strobe_channel_1();
                        key_code = T_F4;
                    }
                } else if (key_code == T_F4) {
                    strobe_channel_1();
                } else if (key_code == T_F3) {
                    if (f_eeprom_done) {
                        gsg_channel = 7;
                        do {
                            do {
                                strobe_selected_channel();
                                delay16(0x13, 0x88);
                            } while (gsg_rx[2] != 8);
                        } while (gsg_rx[4] != 8);
                    }
                    strobe_channel_1();
                }
            }
        } while (key_code != T_F4);
    }
}

void foul_test_screen(void)
{
    clear_and_print(2, PSTR("Foultest !"));
    foultest_run();
}

/* clock_date_screen (0x21a5): "Uhrzeit und Datum" screen. Reads the time, then the
 * date, from GSG channel 10 (6 BCD digits each in gsg_rx[5..0]); shows time as
 * HH:MM:SS and date as DD.MM.YY, then live-updates the clock forever (the
 * original ends in an infinite loop; exit is via device reset). */
void clock_date_screen(void)
{
    uint8_t i, d;
    clear_and_print(2, PSTR(" Uhrzeit  und  Datum"));
    gsg_channel = 10;
    strobe_selected_channel();
    while (gsg_rx[7] != 0x0c)
        hardware_service();
    lcd_cmd(0, 0x18);
    for (i = 0; i < 6; i++) {
        d = gsg_rx[5 - i];
        if (i == 0 && d == 0) lcd_putc(0, ' ');
        else                  lcd_putc(0, d + '0');
        if (i == 1 || i == 3) lcd_putc(0, ':');
    }
    gsg_channel = 10;
    strobe_selected_channel();
    while (gsg_rx[7] != 0x0d)
        hardware_service();
    lcd_cmd(0, 0xce);
    delay16(0x13, 0x88);
    for (i = 0; i < 6; i++) {
        d = gsg_rx[5 - i];
        if ((i == 0 || i == 2) && d == 0) lcd_putc(0, ' ');
        else                              lcd_putc(0, d + '0');
        if (i == 1 || i == 3) lcd_putc(0, '.');
    }
    while (gsg_rx[7] != 0)
        hardware_service();
    gsg_channel = 10;
    strobe_selected_channel();
    while (gsg_rx[7] != 0x0c)
        hardware_service();
    do {
        lcd_cmd(0, 0x18);
        for (i = 0; i < 6; i++) {
            d = gsg_rx[5 - i];
            if (i == 0 && d == 0) lcd_putc(0, ' ');
            else                  lcd_putc(0, d + '0');
            if (i == 1 || i == 3) lcd_putc(0, ':');
        }
        scan_keypad_matrix();
    } while (key_code == T_Keine);
    gsg_channel = 10;
    strobe_selected_channel();
    lcd_cmd(0, 0x96);
    scroll_display_line(2, PSTR("Auf Ende warten !"));   /* CODE 0x3173 */
    while (gsg_rx[7] != 0x0d)
        hardware_service();
    for (;;)            /* clock keeps running until reset */
        hardware_service();
}

/* endurance_test (0x2302): "Test:Dauerlauf" endurance/continuous-run test.
 * Entered with seed key Hoch(5). Strobes GSG channel 2 until the machine reports
 * ready (gsg_rx[6]==0x0f), then continuously monitors run state (rx[0..3] all
 * zero = idle) and drives the test outputs: F1 pulses channel 2, F2 fires the
 * run output (pattern differs old-kbd vs PROFITECH), F3 (while running) toggles
 * an extra output. Updates the status line; exits on Hoch. */
void endurance_test(void)
{
    uint8_t running = 0;         /* _1_3 */

    /* The dispatch record passes 5 for the real Dauerlauf entry.  Other
     * records share this handler as data and return immediately in the ROM. */
    if (menu_arg != 5)
        return;
    {
        clear_and_print(2, PSTR("Test:Dauerlauf"));
        scan_keypad_matrix();
        lcd_render_channel_values(0, 0x19);
        gsg_channel = 2;
        do {
            strobe_selected_channel();
        } while (gsg_rx[6] != 0x0f);
        do {
            lcd_render_channel_values(0, 0x19);
            scan_keypad_matrix();
        } while (key_code != T_Keine);
        last_key = key_code;
        lcd_cmd(0, 0x18);
        scroll_display_line(2, PSTR("ohne-Ausz  mit-Ausz"));   /* CODE 0x0193 */
        running = 0;
        do {
            do {
                if (gsg_rx[6] == 0x0f) {
                    if (gsg_rx[0] == 0 && gsg_rx[1] == 0 && gsg_rx[2] == 0 && gsg_rx[3] == 0) {
                        if (running) {
                            lcd_cmd(0, 0xd4);
                            scroll_display_line(2, PSTR("         "));   /* CODE 0x31a8 (blank = idle) */
                            running = 0;
                        }
                    } else if (running != 1) {
                        lcd_cmd(0, 0xd4);
                        scroll_display_line(2, PSTR(""));       /* "running" */
                        running = 1;
                    }
                }
                lcd_render_channel_values(0, 0x19);
                scan_keypad_matrix();
                if (key_code != last_key) {
                    last_key = key_code;
                    if (key_code == T_F1) {
                        gsg_channel = 2;
                        if (machine_mode == 2) load_gsg_channel_address();
                        else                   drive_gsg_channel();
                        delay16(0x1a, 0x90);
                    } else if (key_code == T_F2) {
                        if (machine_mode == 2) {
                            gsg_tx[0] = 0x80;            /* CODE 0x2cff */
                            delay16(0x1a, 0x90);
                            gsg_tx[1] = 0x02;            /* CODE 0x2cf9 */
                        } else {
                            legacy_gsg3 = 0xfe;             /* CODE 0x2d0f */
                            delay16(0x0d, 0x48);
                            legacy_gsg2 = 0xbf;             /* CODE 0x2d09 */
                            delay16(0x0d, 0x48);
                            if (gsg_rx[6] == 0) {
                                legacy_gsg2 = 0xff;
                                delay16(0x1a, 0x90);
                                legacy_gsg2 = 0xbf;
                            }
                        }
                        delay16(0x1a, 0x90);
                    } else if (key_code == T_F3 && running == 1) {
                        legacy_gsg2 = 0xbf;  gsg_tx[1] = 0x02;
                        delay16(0x1a, 0x90);
                        legacy_gsg2 = 0xbf & 0x7f;  gsg_tx[1] = 0x02 | 0x01;
                        delay16(0x1a, 0x90);
                    }
                    delay16(0x0d, 0x48);
                    legacy_gsg3 = 0xff;
                    delay16(0x0d, 0x48);
                    legacy_gsg2 = 0xff;
                    gsg_tx[0] = 0;
                    gsg_tx[1] = 0;
                    delay16(0x0d, 0x48);
                    if (gsg_rx[6] == 0 && gsg_rx[7] == 0)
                        last_key = 0x14;
                }
            } while (last_key == 0x14 && key_code != T_Hoch);
            if (gsg_rx[6] == 0x0f) {
                lcd_cmd(0, 0x18);
                scroll_display_line(2, PSTR(""));
            } else {
                lcd_cmd(0, 0x18);
                scroll_display_line(2, PSTR(""));
                scroll_display_line(2, PSTR(""));
                scroll_display_line(2, PSTR(""));
            }
        } while (key_code != T_Hoch);
    }
}

/* statistics_print_select (0x24fd): "Statistikausdruck" (statistics printout) screen.
 * Shows "Wechsel mit < oder >" and lets the operator pick which report to print
 * with Links/Rechts (1+5 Kassierungen vs. letzte Kassierung), driving the printer
 * via the GSG2/GSG3 latches (patterns CODE 0x2d07=0x80 / 0x2d08=0x7F / 0x2d17=0xFE);
 * exits on Hoch. */
void statistics_print_select(uint8_t field, uint8_t arg)
{
    (void)field; (void)arg;
    lcd_cmd(0, HD44780_CMD_CLEAR);
    scroll_display_line(2, PSTR("Wechsel mit < oder >   "));          /* CODE 0x01a7 */
    gsg_tx[0] = 0x80;
    legacy_gsg3 = 0xfe;
    delay16(0x1a, 0x90);
    do {
        lcd_render_channel_values(0, 0x1e);
        delay16(0x1a, 0x90);
        if (gsg_rx[7] == 0x00) {
            lcd_cmd(0, 0x18);                 /* shift display left */
            scroll_display_line(2, PSTR("Statistikausdruck 1+5 Kassierungen     ")); /* 0x01bf */
        }
        if (gsg_rx[7] == 0x0c) {
            lcd_cmd(0, 0x18);
            scroll_display_line(2, PSTR("Statistikausdruck letzte Kassierung    ")); /* 0x01e7 */
        }
        scan_keypad_matrix();
        if (key_code == T_Links || key_code == T_Rechts) {
            gsg_tx[0] = 0x80; gsg_tx[1] = 0x01;
            legacy_gsg3 = 0xfe;   legacy_gsg2 = 0x7f;
            delay16(0x1a, 0x90);
        }
        if (key_code == T_Keine) {
            gsg_tx[1] = 0;
            legacy_gsg2 = 0xff;
        }
    } while (key_code != T_Hoch);
    gsg_bus_idle();
}

/* vat_table_menu (0x25b0): record-table menu handler (Test category). First
 * renders the colon-separated field labels (line 1) and 4 entries (line 2) from
 * the 7-byte record table - the same gptr/array_elem_addr machinery as service_menu_loop,
 * summarised here. Then runs a manual output-select loop: F1/F2/F3 drive
 * distinct GSG2 patterns (CODE 0x2cf8=0x01 / 0x2cfd=0x20 / 0x2cfe=0x40 with
 * GSG2 masks 0x7F/0xFB/0xFD); Hoch exits. 'key' is the initial selection. */
static void menu_table_render_text(void)
{
    uint8_t i;

    /* The ROM record set has three headings followed by four selectable
     * strings. Keep the source pointers in CODE so the linker may relocate the
     * table without changing the display reader. */
    lcd_cmd(0, HD44780_CMD_SET_DDRAM_ADDR);
    for (i = 0; i < 3; i++) {
        if (i != 0)
            lcd_putc(0, ':');
        lcd_puts_g(2, menu_label(11, i));
    }
    lcd_cmd(0, HD44780_CMD_SET_DDRAM_ADDR | HD44780_LINE2_ADDR);
    for (i = 0; i < 4; i++) {
        if (i != 0)
            lcd_putc(0, ' ');
        lcd_puts_g(2, menu_label(11, i));
    }
}

void vat_table_menu(char arg1, uint8_t arg2, char key)
{
    uint8_t sel = (uint8_t)key;
    (void)arg1; (void)arg2;

    menu_index = 0x0b;
    key_code = 0x14;                 /* original sentinel while rendering */

    /* --- render record fields (label row + 4 entries), see service_menu_loop --- */
    menu_table_render_text();

    gsg_tx[0] = 0x01;            /* CODE 0x2d00 */
    legacy_gsg3 = 0x7f;             /* CODE 0x2d10 */
    delay16(0x1a, 0x90);
    lcd_render_channel_values(0, 0x1e);
    delay16(0x1a, 0x90);
    last_key = key_code;
    do {
        if (sel == T_F1)      { gsg_tx[1] = 0x01; legacy_gsg2 = 0x7f; }
        else if (sel == T_F2) { gsg_tx[1] = 0x20; legacy_gsg2 = 0xfb; }
        else if (sel == T_F3) { gsg_tx[1] = 0x40; legacy_gsg2 = 0xfd; }
        scan_keypad_matrix();
        delay16(0x1a, 0x90);
        lcd_render_channel_values(0, 0x1e);
        if (key_code != last_key && (last_key = key_code, key_code != T_Keine)
            && key_code < T_Hoch) {
            sel = key_code;
            gsg_tx[1] = 0;
            legacy_gsg2 = 0xff;
            delay16(0x1a, 0x90);
        }
    } while (key_code != T_Hoch);
    gsg_bus_idle();
    delay16(0x1a, 0x90);
    menu_index = (machine_mode == 2) ? 8 : 5;
}

/* ================================================================== *
 *  Menu engine
 * ================================================================== */

/* service_menu_loop (0x27b9): top-level menu engine for machine_mode 2.
 *
 * menu_index selects a 32-byte page in the menu table at CODE 0x324b
 * (pointer = menu_index*32 + 0x324b). menu_index 0 = top level: show
 * "Bitte warten !", poll the GSG bus showing "Service-Schalter !" until input is
 * ready (or the machine IDs as PROFITECH -> mode 1), then "Service Tastatur an
 * PROFITECH 3000". Each page renders its 4 entry labels on line 2.
 * Navigation: Hoch = back/up, F1..F4 = select entry 0..3, F2+HOCH = reinit.
 *
 * HOW THE ORIGINAL DISPATCHES (recovered from disassembly @0x2a51, since the
 * decompiler hid it as "gptr_read_byte; RET"): on a leaf selection it builds a
 * pointer from the record's action byte that indexes the handler-address table
 * at CODE 0x00d4 (menu_dispatch_table below), reads the 2-byte handler address,
 * PUSHes it and RETs (8051 computed-call), with 0x2a81 pushed as the return
 * address so the handler returns into the menu loop. The menu_pages table below
 * reproduces this behaviourally with a direct handler call per entry. Returns
 * when machine_mode becomes 1. */
void service_menu_loop(uint8_t mode)
{
    uint8_t i;
    (void)mode;
    menu_index = 0;
    do {
        lcd_cmd(0, HD44780_CMD_CLEAR);
        if (menu_index == 0) {
            /* top level: show title on line 1; poll the GSG bus until ready
             * (or the machine identifies itself as PROFITECH -> mode 1) */
            lcd_cmd(0, HD44780_CMD_DISPLAY_CTRL | HD44780_DISPLAY_ON);
            clear_and_print(2, PSTR("}Bitte warten !"));
            last_key = T_Keine;
            do {
                poll_machine_ready();
                if (!f_input_ready)
                    clear_and_print(2, PSTR("Service-Schalter !                     "));
                scan_keypad_matrix();
                if (key_code == T_F2_Hoch)
                    reinit_machine();
            } while (!f_input_ready && machine_mode != 1);
            if (machine_mode == 1) return;
            lcd_cmd(0, HD44780_CMD_CLEAR);
            clear_and_print(2, S_PROFITECH_TITLE);
        }

        /* render the 4 entry labels of the current page on line 2 (cols 0,10,20,30) */
        for (i = 0; i < 4; i++) {
            lcd_cmd(0, HD44780_CMD_SET_DDRAM_ADDR | (HD44780_LINE2_ADDR + i * 10));
            lcd_puts_g(2, menu_label(menu_index, i));
        }

        /* wait for the previous key to be released */
        do {
            scan_keypad_matrix();
            if (key_code == T_Keine)
                break;
        } while (machine_mode != 1);
        delay16(0x27, 0x10);

        /* while idle, keep strobing any active GSG output channel */
        do {
            for (i = 0; i < 8; i++) {
                if (gsg_rx[i] != 0) {
                    gsg_channel = 1;
                    strobe_selected_channel();
                }
            }
            scan_keypad_matrix();
        } while (key_code == last_key && machine_mode != 1);
        last_key = key_code;

        if (key_code == T_F2_Hoch) {
            reinit_machine();
        } else if (key_code == T_Hoch) {
            /* back up to the top level (one-shot strobe the first time) */
            if (!f_init) {
                strobe_channel_1();
                delay16(0xb5, 0xa4);
                f_init = 1;
            }
            menu_index = 0;
        } else if (key_code >= T_F1 && key_code <= T_F4) {
            /* F1..F4 select entry 0..3 of the current page */
            struct menu_item item;
            memcpy_P(&item, &menu_pages[menu_index][key_code - T_F1], sizeof(item));
            const struct menu_item *it = &item;
            if (it->submenu != 0xff) {
                menu_index = it->submenu;       /* descend into submenu page */
            } else if (it->handler) {
                menu_arg = it->param;           /* record [6] -> handler parameter */
                it->handler();                  /* leaf: run the handler */
                menu_index = 0;                 /* return to top afterwards */
            }
        }

        if (machine_mode == 1)
            return;
    } while (1);
}

/* ================================================================== *
 *  Reset / init
 * ================================================================== */

/* reset_vars (0x2b88): clear all state. */
void reset_vars(void)
{
    uint8_t i;
    f_key_pending = 0;
    f_eeprom_write = 0;
    f_eeprom_read = 0;
    f_eeprom_done = 0;
    f_init = 1;
    f_input_ready = 0;
    machine_mode = 0;
    menu_index = 0;
    gsg_cmd = 0;
    gsg_channel = 1;
    gsg_level_flags = 0;
    key_code = T_Keine;
    last_key = 0;
    key_hold_state = 0;
    service_key_latch = T_Keine;
    lcd_timeout_state = 0;
    for (i = 0; i < 16; i++) gsg_rx[i] = 0;
    for (i = 0; i < 5; i++) gsg_tx[i] = 0;
    i2c_addr = 0; i2c_shift = 0; i2c_ctrl = 0;
    lcd_state_csum = 0;
}

void firmware_begin()
{
    reset_vars();
    machine_mode = 3;
    clear_tx_latches_ff();
    gsg_tx[0] = gsg_tx[1] = 0;
    clear_and_print(2, S_NO_MACHINE);
    hardware_link_begin();
}

void firmware_run()
{
    hardware_service();
    if (machine_mode == 1)
        profitech_service_loop();
    else if (machine_mode == 2)
        service_menu_loop(2);
    else if (machine_mode == 3) {
        // A stuck machine may not identify itself. Offer the reinit pattern
        // over J3 without waiting for detection, once per chord press.
        scan_keypad_matrix();
        const bool reinit = key_code == T_F2_Hoch && last_key != T_F2_Hoch;
        last_key = key_code;
        if (reinit) {
            reinit_machine();
            // Receive interrupts stay enabled during the pulse. Preserve any
            // machine detection that happened while it was being sent.
            if (machine_mode == 3)
                clear_and_print(2, S_NO_MACHINE);
        }
    }
}

bool SERVICE_KEYBOARD_ISR_ATTR firmware_receive(uint8_t gsg4, uint8_t gsg5, uint8_t &reply)
{
    const uint8_t cmd = (gsg5 >> 1) & 7;
    const uint8_t data = ServiceKeyboard::frameData(gsg4, gsg5);
    bool written = false;
    gsg_cmd = cmd;
    if (machine_mode == 3) machine_mode = 2;
    if (machine_mode == 1) {
        if (gsg5 & 1)
            hardware_lcd_enqueue_isr(!(gsg4 & 0x20), data);
        if (gsg4 & 0x10) {
            gsg_rx[cmd] = data;
            if (cmd == 0) i2c_addr = data;
            if (cmd == 4) f_eeprom_write = true;
            if (gsg4 & 0x20) { f_eeprom_read = true; f_eeprom_done = false; }
        }
        if (cmd <= 4) { reply = gsg_tx[cmd]; written = true; }
        if (cmd == 7 && (gsg4 & 0x80)) machine_mode = 2;
        if (cmd == 7 && f_key_pending) {
            reply = last_key | 0x80;
            written = true;
            f_key_pending = false;
        }
    } else {
        gsg_rx[cmd] = data;
        if (cmd == 6) { reply = gsg_tx[1]; written = true; }
        if (cmd == 7) {
            reply = gsg_tx[0];
            written = true;
            if (!(gsg4 & 0x80)) machine_mode = 1;
        }
    }
    if (cmd == 5) { reply = 0x30; written = true; }
    return written;
}
