;--------------------------------------------------------
; File Created by SDCC : free open source ISO C Compiler
; Version 4.5.0 #15242 (Linux)
;--------------------------------------------------------
	.module main
	
	.optsdcc -mmcs51 --model-small
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _LCD_DATA_R
	.globl _LCD_DATA_W
	.globl _LCD_CMD_R
	.globl _LCD_CMD_W
	.globl _main
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; special function bits
;--------------------------------------------------------
	.area RSEG    (ABS,DATA)
	.org 0x0000
;--------------------------------------------------------
; overlayable register banks
;--------------------------------------------------------
	.area REG_BANK_0	(REL,OVR,DATA)
	.ds 8
;--------------------------------------------------------
; internal ram data
;--------------------------------------------------------
	.area DSEG    (DATA)
;--------------------------------------------------------
; overlayable items in internal ram
;--------------------------------------------------------
	.area	OSEG    (OVR,DATA)
;--------------------------------------------------------
; Stack segment in internal ram
;--------------------------------------------------------
	.area SSEG
__start__stack:
	.ds	1

;--------------------------------------------------------
; indirectly addressable internal ram data
;--------------------------------------------------------
	.area ISEG    (DATA)
;--------------------------------------------------------
; absolute internal ram data
;--------------------------------------------------------
	.area IABS    (ABS,DATA)
	.area IABS    (ABS,DATA)
;--------------------------------------------------------
; bit data
;--------------------------------------------------------
	.area BSEG    (BIT)
;--------------------------------------------------------
; paged external ram data
;--------------------------------------------------------
	.area PSEG    (PAG,XDATA)
;--------------------------------------------------------
; uninitialized external ram data
;--------------------------------------------------------
	.area XSEG    (XDATA)
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area XABS    (ABS,XDATA)
;--------------------------------------------------------
; initialized external ram data
;--------------------------------------------------------
	.area XISEG   (XDATA)
	.area HOME    (CODE)
	.area GSINIT0 (CODE)
	.area GSINIT1 (CODE)
	.area GSINIT2 (CODE)
	.area GSINIT3 (CODE)
	.area GSINIT4 (CODE)
	.area GSINIT5 (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area CSEG    (CODE)
;--------------------------------------------------------
; interrupt vector
;--------------------------------------------------------
	.area HOME    (CODE)
__interrupt_vect:
	ljmp	__sdcc_gsinit_startup
; restartable atomic support routines
	.ds	5
sdcc_atomic_exchange_rollback_start::
	nop
	nop
sdcc_atomic_exchange_pdata_impl:
	movx	a, @r0
	mov	r3, a
	mov	a, r2
	movx	@r0, a
	sjmp	sdcc_atomic_exchange_exit
	nop
	nop
sdcc_atomic_exchange_xdata_impl:
	movx	a, @dptr
	mov	r3, a
	mov	a, r2
	movx	@dptr, a
	sjmp	sdcc_atomic_exchange_exit
sdcc_atomic_compare_exchange_idata_impl:
	mov	a, @r0
	cjne	a, ar2, .+#5
	mov	a, r3
	mov	@r0, a
	ret
	nop
sdcc_atomic_compare_exchange_pdata_impl:
	movx	a, @r0
	cjne	a, ar2, .+#5
	mov	a, r3
	movx	@r0, a
	ret
	nop
sdcc_atomic_compare_exchange_xdata_impl:
	movx	a, @dptr
	cjne	a, ar2, .+#5
	mov	a, r3
	movx	@dptr, a
	ret
sdcc_atomic_exchange_rollback_end::

sdcc_atomic_exchange_gptr_impl::
	jnb	b.6, sdcc_atomic_exchange_xdata_impl
	mov	r0, dpl
	jb	b.5, sdcc_atomic_exchange_pdata_impl
sdcc_atomic_exchange_idata_impl:
	mov	a, r2
	xch	a, @r0
	mov	dpl, a
	ret
sdcc_atomic_exchange_exit:
	mov	dpl, r3
	ret
sdcc_atomic_compare_exchange_gptr_impl::
	jnb	b.6, sdcc_atomic_compare_exchange_xdata_impl
	mov	r0, dpl
	jb	b.5, sdcc_atomic_compare_exchange_pdata_impl
	sjmp	sdcc_atomic_compare_exchange_idata_impl
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area HOME    (CODE)
	.area GSINIT  (CODE)
	.area GSFINAL (CODE)
	.area GSINIT  (CODE)
	.globl __sdcc_gsinit_startup
	.globl __sdcc_program_startup
	.globl __start__stack
	.globl __mcs51_genXINIT
	.globl __mcs51_genXRAMCLEAR
	.globl __mcs51_genRAMCLEAR
	.area GSFINAL (CODE)
	ljmp	__sdcc_program_startup
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area HOME    (CODE)
	.area HOME    (CODE)
__sdcc_program_startup:
	ljmp	_main
;	return from main will return to caller
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area CSEG    (CODE)
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_delay'
;------------------------------------------------------------
;i             Allocated to registers r6 r7 
;------------------------------------------------------------
;	main.c:18: static void lcd_delay(void) {
;	-----------------------------------------
;	 function lcd_delay
;	-----------------------------------------
_lcd_delay:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
;	main.c:20: for (i = 0; i < 1000; i++);
	mov	r6,#0xe8
	mov	r7,#0x03
00104$:
	dec	r6
	cjne	r6,#0xff,00115$
	dec	r7
00115$:
	mov	a,r6
	orl	a,r7
	jnz	00104$
;	main.c:21: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_wait_ready'
;------------------------------------------------------------
;	main.c:23: static void lcd_wait_ready(void) {
;	-----------------------------------------
;	 function lcd_wait_ready
;	-----------------------------------------
_lcd_wait_ready:
;	main.c:24: while (*LCD_CMD_R & LCD_BUSY);
00101$:
	mov	dptr,#_LCD_CMD_R
	clr	a
	movc	a,@a+dptr
	mov	r6,a
	mov	a,#0x01
	movc	a,@a+dptr
	mov	r7,a
	mov	dpl,r6
	mov	dph,r7
	movx	a,@dptr
	jb	acc.7,00101$
;	main.c:25: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_cmd'
;------------------------------------------------------------
;cmd           Allocated to registers r7 
;------------------------------------------------------------
;	main.c:27: static void lcd_cmd(uint8_t cmd) {
;	-----------------------------------------
;	 function lcd_cmd
;	-----------------------------------------
_lcd_cmd:
	mov	r7, dpl
;	main.c:28: lcd_wait_ready();
	push	ar7
	lcall	_lcd_wait_ready
	pop	ar7
;	main.c:29: *LCD_CMD_W = cmd;
	mov	dptr,#_LCD_CMD_W
	clr	a
	movc	a,@a+dptr
	mov	r5,a
	mov	a,#0x01
	movc	a,@a+dptr
	mov	r6,a
	mov	dpl,r5
	mov	dph,r6
	mov	a,r7
	movx	@dptr,a
;	main.c:30: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_putc'
;------------------------------------------------------------
;c             Allocated to registers r7 
;------------------------------------------------------------
;	main.c:32: static void lcd_putc(char c) {
;	-----------------------------------------
;	 function lcd_putc
;	-----------------------------------------
_lcd_putc:
	mov	r7, dpl
;	main.c:33: lcd_wait_ready();
	push	ar7
	lcall	_lcd_wait_ready
	pop	ar7
;	main.c:34: *LCD_DATA_W = c;
	mov	dptr,#_LCD_DATA_W
	clr	a
	movc	a,@a+dptr
	mov	r5,a
	mov	a,#0x01
	movc	a,@a+dptr
	mov	r6,a
	mov	dpl,r5
	mov	dph,r6
	mov	a,r7
	movx	@dptr,a
;	main.c:35: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_init'
;------------------------------------------------------------
;	main.c:37: static void lcd_init(void) {
;	-----------------------------------------
;	 function lcd_init
;	-----------------------------------------
_lcd_init:
;	main.c:38: lcd_delay();
	lcall	_lcd_delay
;	main.c:39: lcd_cmd(0x38); // Function set: 8-bit, 2 lines, 5x7 dots
	mov	dpl, #0x38
	lcall	_lcd_cmd
;	main.c:40: lcd_cmd(0x0C); // Display ON, cursor off
	mov	dpl, #0x0c
	lcall	_lcd_cmd
;	main.c:41: lcd_cmd(0x06); // Entry mode: increment cursor
	mov	dpl, #0x06
	lcall	_lcd_cmd
;	main.c:42: lcd_cmd(0x01); // Clear display
	mov	dpl, #0x01
;	main.c:43: }
	ljmp	_lcd_cmd
;------------------------------------------------------------
;Allocation info for local variables in function 'lcd_puts'
;------------------------------------------------------------
;s             Allocated to registers 
;------------------------------------------------------------
;	main.c:45: static void lcd_puts(const char *s) {
;	-----------------------------------------
;	 function lcd_puts
;	-----------------------------------------
_lcd_puts:
	mov	r5, dpl
	mov	r6, dph
	mov	r7, b
;	main.c:46: while (*s) {
00101$:
	mov	dpl,r5
	mov	dph,r6
	mov	b,r7
	lcall	__gptrget
	mov	r4,a
	jz	00104$
;	main.c:47: lcd_putc(*s++);
	mov	dpl,r4
	inc	r5
	cjne	r5,#0x00,00120$
	inc	r6
00120$:
	push	ar7
	push	ar6
	push	ar5
	lcall	_lcd_putc
	pop	ar5
	pop	ar6
	pop	ar7
	sjmp	00101$
00104$:
;	main.c:49: }
	ret
;------------------------------------------------------------
;Allocation info for local variables in function 'main'
;------------------------------------------------------------
;	main.c:51: void main(void) {
;	-----------------------------------------
;	 function main
;	-----------------------------------------
_main:
;	main.c:52: lcd_init();
	lcall	_lcd_init
;	main.c:54: lcd_cmd(0x80 | 0x00); // Line 1, DDRAM=0x00
	mov	dpl, #0x80
	lcall	_lcd_cmd
;	main.c:55: lcd_puts("Hello");
	mov	dptr,#___str_0
	mov	b, #0x80
	lcall	_lcd_puts
;	main.c:57: lcd_cmd(0x80 | 0x40); // Line 2, DDRAM=0x40
	mov	dpl, #0xc0
	lcall	_lcd_cmd
;	main.c:58: lcd_puts("World");
	mov	dptr,#___str_1
	mov	b, #0x80
	lcall	_lcd_puts
;	main.c:60: while (1);
00102$:
;	main.c:61: }
	sjmp	00102$
	.area CSEG    (CODE)
	.area CONST   (CODE)
	.area CONST   (CODE)
_LCD_CMD_W:
	.byte #0x70,#0x00
	.area CSEG    (CODE)
	.area CONST   (CODE)
_LCD_CMD_R:
	.byte #0x71,#0x00
	.area CSEG    (CODE)
	.area CONST   (CODE)
_LCD_DATA_W:
	.byte #0x72,#0x00
	.area CSEG    (CODE)
	.area CONST   (CODE)
_LCD_DATA_R:
	.byte #0x73,#0x00
	.area CSEG    (CODE)
	.area CONST   (CODE)
___str_0:
	.ascii "Hello"
	.db 0x00
	.area CSEG    (CODE)
	.area CONST   (CODE)
___str_1:
	.ascii "World"
	.db 0x00
	.area CSEG    (CODE)
	.area XINIT   (CODE)
	.area CABS    (ABS,CODE)
