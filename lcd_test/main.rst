                                      1 ;--------------------------------------------------------
                                      2 ; File Created by SDCC : free open source ISO C Compiler
                                      3 ; Version 4.5.0 #15242 (Linux)
                                      4 ;--------------------------------------------------------
                                      5 	.module main
                                      6 	
                                      7 	.optsdcc -mmcs51 --model-small
                                      8 ;--------------------------------------------------------
                                      9 ; Public variables in this module
                                     10 ;--------------------------------------------------------
                                     11 	.globl _LCD_DATA_R
                                     12 	.globl _LCD_DATA_W
                                     13 	.globl _LCD_CMD_R
                                     14 	.globl _LCD_CMD_W
                                     15 	.globl _main
                                     16 ;--------------------------------------------------------
                                     17 ; special function registers
                                     18 ;--------------------------------------------------------
                                     19 	.area RSEG    (ABS,DATA)
      000000                         20 	.org 0x0000
                                     21 ;--------------------------------------------------------
                                     22 ; special function bits
                                     23 ;--------------------------------------------------------
                                     24 	.area RSEG    (ABS,DATA)
      000000                         25 	.org 0x0000
                                     26 ;--------------------------------------------------------
                                     27 ; overlayable register banks
                                     28 ;--------------------------------------------------------
                                     29 	.area REG_BANK_0	(REL,OVR,DATA)
      000000                         30 	.ds 8
                                     31 ;--------------------------------------------------------
                                     32 ; internal ram data
                                     33 ;--------------------------------------------------------
                                     34 	.area DSEG    (DATA)
                                     35 ;--------------------------------------------------------
                                     36 ; overlayable items in internal ram
                                     37 ;--------------------------------------------------------
                                     38 	.area	OSEG    (OVR,DATA)
                                     39 ;--------------------------------------------------------
                                     40 ; Stack segment in internal ram
                                     41 ;--------------------------------------------------------
                                     42 	.area SSEG
      000008                         43 __start__stack:
      000008                         44 	.ds	1
                                     45 
                                     46 ;--------------------------------------------------------
                                     47 ; indirectly addressable internal ram data
                                     48 ;--------------------------------------------------------
                                     49 	.area ISEG    (DATA)
                                     50 ;--------------------------------------------------------
                                     51 ; absolute internal ram data
                                     52 ;--------------------------------------------------------
                                     53 	.area IABS    (ABS,DATA)
                                     54 	.area IABS    (ABS,DATA)
                                     55 ;--------------------------------------------------------
                                     56 ; bit data
                                     57 ;--------------------------------------------------------
                                     58 	.area BSEG    (BIT)
                                     59 ;--------------------------------------------------------
                                     60 ; paged external ram data
                                     61 ;--------------------------------------------------------
                                     62 	.area PSEG    (PAG,XDATA)
                                     63 ;--------------------------------------------------------
                                     64 ; uninitialized external ram data
                                     65 ;--------------------------------------------------------
                                     66 	.area XSEG    (XDATA)
                                     67 ;--------------------------------------------------------
                                     68 ; absolute external ram data
                                     69 ;--------------------------------------------------------
                                     70 	.area XABS    (ABS,XDATA)
                                     71 ;--------------------------------------------------------
                                     72 ; initialized external ram data
                                     73 ;--------------------------------------------------------
                                     74 	.area XISEG   (XDATA)
                                     75 	.area HOME    (CODE)
                                     76 	.area GSINIT0 (CODE)
                                     77 	.area GSINIT1 (CODE)
                                     78 	.area GSINIT2 (CODE)
                                     79 	.area GSINIT3 (CODE)
                                     80 	.area GSINIT4 (CODE)
                                     81 	.area GSINIT5 (CODE)
                                     82 	.area GSINIT  (CODE)
                                     83 	.area GSFINAL (CODE)
                                     84 	.area CSEG    (CODE)
                                     85 ;--------------------------------------------------------
                                     86 ; interrupt vector
                                     87 ;--------------------------------------------------------
                                     88 	.area HOME    (CODE)
      000000                         89 __interrupt_vect:
      000000 02 00 4C         [24]   90 	ljmp	__sdcc_gsinit_startup
                                     91 ; restartable atomic support routines
      000003                         92 	.ds	5
      000008                         93 sdcc_atomic_exchange_rollback_start::
      000008 00               [12]   94 	nop
      000009 00               [12]   95 	nop
      00000A                         96 sdcc_atomic_exchange_pdata_impl:
      00000A E2               [24]   97 	movx	a, @r0
      00000B FB               [12]   98 	mov	r3, a
      00000C EA               [12]   99 	mov	a, r2
      00000D F2               [24]  100 	movx	@r0, a
      00000E 80 2C            [24]  101 	sjmp	sdcc_atomic_exchange_exit
      000010 00               [12]  102 	nop
      000011 00               [12]  103 	nop
      000012                        104 sdcc_atomic_exchange_xdata_impl:
      000012 E0               [24]  105 	movx	a, @dptr
      000013 FB               [12]  106 	mov	r3, a
      000014 EA               [12]  107 	mov	a, r2
      000015 F0               [24]  108 	movx	@dptr, a
      000016 80 24            [24]  109 	sjmp	sdcc_atomic_exchange_exit
      000018                        110 sdcc_atomic_compare_exchange_idata_impl:
      000018 E6               [12]  111 	mov	a, @r0
      000019 B5 02 02         [24]  112 	cjne	a, ar2, .+#5
      00001C EB               [12]  113 	mov	a, r3
      00001D F6               [12]  114 	mov	@r0, a
      00001E 22               [24]  115 	ret
      00001F 00               [12]  116 	nop
      000020                        117 sdcc_atomic_compare_exchange_pdata_impl:
      000020 E2               [24]  118 	movx	a, @r0
      000021 B5 02 02         [24]  119 	cjne	a, ar2, .+#5
      000024 EB               [12]  120 	mov	a, r3
      000025 F2               [24]  121 	movx	@r0, a
      000026 22               [24]  122 	ret
      000027 00               [12]  123 	nop
      000028                        124 sdcc_atomic_compare_exchange_xdata_impl:
      000028 E0               [24]  125 	movx	a, @dptr
      000029 B5 02 02         [24]  126 	cjne	a, ar2, .+#5
      00002C EB               [12]  127 	mov	a, r3
      00002D F0               [24]  128 	movx	@dptr, a
      00002E 22               [24]  129 	ret
      00002F                        130 sdcc_atomic_exchange_rollback_end::
                                    131 
      00002F                        132 sdcc_atomic_exchange_gptr_impl::
      00002F 30 F6 E0         [24]  133 	jnb	b.6, sdcc_atomic_exchange_xdata_impl
      000032 A8 82            [24]  134 	mov	r0, dpl
      000034 20 F5 D3         [24]  135 	jb	b.5, sdcc_atomic_exchange_pdata_impl
      000037                        136 sdcc_atomic_exchange_idata_impl:
      000037 EA               [12]  137 	mov	a, r2
      000038 C6               [12]  138 	xch	a, @r0
      000039 F5 82            [12]  139 	mov	dpl, a
      00003B 22               [24]  140 	ret
      00003C                        141 sdcc_atomic_exchange_exit:
      00003C 8B 82            [24]  142 	mov	dpl, r3
      00003E 22               [24]  143 	ret
      00003F                        144 sdcc_atomic_compare_exchange_gptr_impl::
      00003F 30 F6 E6         [24]  145 	jnb	b.6, sdcc_atomic_compare_exchange_xdata_impl
      000042 A8 82            [24]  146 	mov	r0, dpl
      000044 20 F5 D9         [24]  147 	jb	b.5, sdcc_atomic_compare_exchange_pdata_impl
      000047 80 CF            [24]  148 	sjmp	sdcc_atomic_compare_exchange_idata_impl
                                    149 ;--------------------------------------------------------
                                    150 ; global & static initialisations
                                    151 ;--------------------------------------------------------
                                    152 	.area HOME    (CODE)
                                    153 	.area GSINIT  (CODE)
                                    154 	.area GSFINAL (CODE)
                                    155 	.area GSINIT  (CODE)
                                    156 	.globl __sdcc_gsinit_startup
                                    157 	.globl __sdcc_program_startup
                                    158 	.globl __start__stack
                                    159 	.globl __mcs51_genXINIT
                                    160 	.globl __mcs51_genXRAMCLEAR
                                    161 	.globl __mcs51_genRAMCLEAR
                                    162 	.area GSFINAL (CODE)
      0000A5 02 00 49         [24]  163 	ljmp	__sdcc_program_startup
                                    164 ;--------------------------------------------------------
                                    165 ; Home
                                    166 ;--------------------------------------------------------
                                    167 	.area HOME    (CODE)
                                    168 	.area HOME    (CODE)
      000049                        169 __sdcc_program_startup:
      000049 02 01 43         [24]  170 	ljmp	_main
                                    171 ;	return from main will return to caller
                                    172 ;--------------------------------------------------------
                                    173 ; code
                                    174 ;--------------------------------------------------------
                                    175 	.area CSEG    (CODE)
                                    176 ;------------------------------------------------------------
                                    177 ;Allocation info for local variables in function 'lcd_delay'
                                    178 ;------------------------------------------------------------
                                    179 ;i             Allocated to registers r6 r7 
                                    180 ;------------------------------------------------------------
                                    181 ;	main.c:18: static void lcd_delay(void) {
                                    182 ;	-----------------------------------------
                                    183 ;	 function lcd_delay
                                    184 ;	-----------------------------------------
      0000A8                        185 _lcd_delay:
                           000007   186 	ar7 = 0x07
                           000006   187 	ar6 = 0x06
                           000005   188 	ar5 = 0x05
                           000004   189 	ar4 = 0x04
                           000003   190 	ar3 = 0x03
                           000002   191 	ar2 = 0x02
                           000001   192 	ar1 = 0x01
                           000000   193 	ar0 = 0x00
                                    194 ;	main.c:20: for (i = 0; i < 1000; i++);
      0000A8 7E E8            [12]  195 	mov	r6,#0xe8
      0000AA 7F 03            [12]  196 	mov	r7,#0x03
      0000AC                        197 00104$:
      0000AC 1E               [12]  198 	dec	r6
      0000AD BE FF 01         [24]  199 	cjne	r6,#0xff,00115$
      0000B0 1F               [12]  200 	dec	r7
      0000B1                        201 00115$:
      0000B1 EE               [12]  202 	mov	a,r6
      0000B2 4F               [12]  203 	orl	a,r7
      0000B3 70 F7            [24]  204 	jnz	00104$
                                    205 ;	main.c:21: }
      0000B5 22               [24]  206 	ret
                                    207 ;------------------------------------------------------------
                                    208 ;Allocation info for local variables in function 'lcd_wait_ready'
                                    209 ;------------------------------------------------------------
                                    210 ;	main.c:23: static void lcd_wait_ready(void) {
                                    211 ;	-----------------------------------------
                                    212 ;	 function lcd_wait_ready
                                    213 ;	-----------------------------------------
      0000B6                        214 _lcd_wait_ready:
                                    215 ;	main.c:24: while (*LCD_CMD_R & LCD_BUSY);
      0000B6                        216 00101$:
      0000B6 90 01 88         [24]  217 	mov	dptr,#_LCD_CMD_R
      0000B9 E4               [12]  218 	clr	a
      0000BA 93               [24]  219 	movc	a,@a+dptr
      0000BB FE               [12]  220 	mov	r6,a
      0000BC 74 01            [12]  221 	mov	a,#0x01
      0000BE 93               [24]  222 	movc	a,@a+dptr
      0000BF FF               [12]  223 	mov	r7,a
      0000C0 8E 82            [24]  224 	mov	dpl,r6
      0000C2 8F 83            [24]  225 	mov	dph,r7
      0000C4 E0               [24]  226 	movx	a,@dptr
      0000C5 20 E7 EE         [24]  227 	jb	acc.7,00101$
                                    228 ;	main.c:25: }
      0000C8 22               [24]  229 	ret
                                    230 ;------------------------------------------------------------
                                    231 ;Allocation info for local variables in function 'lcd_cmd'
                                    232 ;------------------------------------------------------------
                                    233 ;cmd           Allocated to registers r7 
                                    234 ;------------------------------------------------------------
                                    235 ;	main.c:27: static void lcd_cmd(uint8_t cmd) {
                                    236 ;	-----------------------------------------
                                    237 ;	 function lcd_cmd
                                    238 ;	-----------------------------------------
      0000C9                        239 _lcd_cmd:
      0000C9 AF 82            [24]  240 	mov	r7, dpl
                                    241 ;	main.c:28: lcd_wait_ready();
      0000CB C0 07            [24]  242 	push	ar7
      0000CD 12 00 B6         [24]  243 	lcall	_lcd_wait_ready
      0000D0 D0 07            [24]  244 	pop	ar7
                                    245 ;	main.c:29: *LCD_CMD_W = cmd;
      0000D2 90 01 86         [24]  246 	mov	dptr,#_LCD_CMD_W
      0000D5 E4               [12]  247 	clr	a
      0000D6 93               [24]  248 	movc	a,@a+dptr
      0000D7 FD               [12]  249 	mov	r5,a
      0000D8 74 01            [12]  250 	mov	a,#0x01
      0000DA 93               [24]  251 	movc	a,@a+dptr
      0000DB FE               [12]  252 	mov	r6,a
      0000DC 8D 82            [24]  253 	mov	dpl,r5
      0000DE 8E 83            [24]  254 	mov	dph,r6
      0000E0 EF               [12]  255 	mov	a,r7
      0000E1 F0               [24]  256 	movx	@dptr,a
                                    257 ;	main.c:30: }
      0000E2 22               [24]  258 	ret
                                    259 ;------------------------------------------------------------
                                    260 ;Allocation info for local variables in function 'lcd_putc'
                                    261 ;------------------------------------------------------------
                                    262 ;c             Allocated to registers r7 
                                    263 ;------------------------------------------------------------
                                    264 ;	main.c:32: static void lcd_putc(char c) {
                                    265 ;	-----------------------------------------
                                    266 ;	 function lcd_putc
                                    267 ;	-----------------------------------------
      0000E3                        268 _lcd_putc:
      0000E3 AF 82            [24]  269 	mov	r7, dpl
                                    270 ;	main.c:33: lcd_wait_ready();
      0000E5 C0 07            [24]  271 	push	ar7
      0000E7 12 00 B6         [24]  272 	lcall	_lcd_wait_ready
      0000EA D0 07            [24]  273 	pop	ar7
                                    274 ;	main.c:34: *LCD_DATA_W = c;
      0000EC 90 01 8A         [24]  275 	mov	dptr,#_LCD_DATA_W
      0000EF E4               [12]  276 	clr	a
      0000F0 93               [24]  277 	movc	a,@a+dptr
      0000F1 FD               [12]  278 	mov	r5,a
      0000F2 74 01            [12]  279 	mov	a,#0x01
      0000F4 93               [24]  280 	movc	a,@a+dptr
      0000F5 FE               [12]  281 	mov	r6,a
      0000F6 8D 82            [24]  282 	mov	dpl,r5
      0000F8 8E 83            [24]  283 	mov	dph,r6
      0000FA EF               [12]  284 	mov	a,r7
      0000FB F0               [24]  285 	movx	@dptr,a
                                    286 ;	main.c:35: }
      0000FC 22               [24]  287 	ret
                                    288 ;------------------------------------------------------------
                                    289 ;Allocation info for local variables in function 'lcd_init'
                                    290 ;------------------------------------------------------------
                                    291 ;	main.c:37: static void lcd_init(void) {
                                    292 ;	-----------------------------------------
                                    293 ;	 function lcd_init
                                    294 ;	-----------------------------------------
      0000FD                        295 _lcd_init:
                                    296 ;	main.c:38: lcd_delay();
      0000FD 12 00 A8         [24]  297 	lcall	_lcd_delay
                                    298 ;	main.c:39: lcd_cmd(0x38); // Function set: 8-bit, 2 lines, 5x7 dots
      000100 75 82 38         [24]  299 	mov	dpl, #0x38
      000103 12 00 C9         [24]  300 	lcall	_lcd_cmd
                                    301 ;	main.c:40: lcd_cmd(0x0C); // Display ON, cursor off
      000106 75 82 0C         [24]  302 	mov	dpl, #0x0c
      000109 12 00 C9         [24]  303 	lcall	_lcd_cmd
                                    304 ;	main.c:41: lcd_cmd(0x06); // Entry mode: increment cursor
      00010C 75 82 06         [24]  305 	mov	dpl, #0x06
      00010F 12 00 C9         [24]  306 	lcall	_lcd_cmd
                                    307 ;	main.c:42: lcd_cmd(0x01); // Clear display
      000112 75 82 01         [24]  308 	mov	dpl, #0x01
                                    309 ;	main.c:43: }
      000115 02 00 C9         [24]  310 	ljmp	_lcd_cmd
                                    311 ;------------------------------------------------------------
                                    312 ;Allocation info for local variables in function 'lcd_puts'
                                    313 ;------------------------------------------------------------
                                    314 ;s             Allocated to registers 
                                    315 ;------------------------------------------------------------
                                    316 ;	main.c:45: static void lcd_puts(const char *s) {
                                    317 ;	-----------------------------------------
                                    318 ;	 function lcd_puts
                                    319 ;	-----------------------------------------
      000118                        320 _lcd_puts:
      000118 AD 82            [24]  321 	mov	r5, dpl
      00011A AE 83            [24]  322 	mov	r6, dph
      00011C AF F0            [24]  323 	mov	r7, b
                                    324 ;	main.c:46: while (*s) {
      00011E                        325 00101$:
      00011E 8D 82            [24]  326 	mov	dpl,r5
      000120 8E 83            [24]  327 	mov	dph,r6
      000122 8F F0            [24]  328 	mov	b,r7
      000124 12 01 66         [24]  329 	lcall	__gptrget
      000127 FC               [12]  330 	mov	r4,a
      000128 60 18            [24]  331 	jz	00104$
                                    332 ;	main.c:47: lcd_putc(*s++);
      00012A 8C 82            [24]  333 	mov	dpl,r4
      00012C 0D               [12]  334 	inc	r5
      00012D BD 00 01         [24]  335 	cjne	r5,#0x00,00120$
      000130 0E               [12]  336 	inc	r6
      000131                        337 00120$:
      000131 C0 07            [24]  338 	push	ar7
      000133 C0 06            [24]  339 	push	ar6
      000135 C0 05            [24]  340 	push	ar5
      000137 12 00 E3         [24]  341 	lcall	_lcd_putc
      00013A D0 05            [24]  342 	pop	ar5
      00013C D0 06            [24]  343 	pop	ar6
      00013E D0 07            [24]  344 	pop	ar7
      000140 80 DC            [24]  345 	sjmp	00101$
      000142                        346 00104$:
                                    347 ;	main.c:49: }
      000142 22               [24]  348 	ret
                                    349 ;------------------------------------------------------------
                                    350 ;Allocation info for local variables in function 'main'
                                    351 ;------------------------------------------------------------
                                    352 ;	main.c:51: void main(void) {
                                    353 ;	-----------------------------------------
                                    354 ;	 function main
                                    355 ;	-----------------------------------------
      000143                        356 _main:
                                    357 ;	main.c:52: lcd_init();
      000143 12 00 FD         [24]  358 	lcall	_lcd_init
                                    359 ;	main.c:54: lcd_cmd(0x80 | 0x00); // Line 1, DDRAM=0x00
      000146 75 82 80         [24]  360 	mov	dpl, #0x80
      000149 12 00 C9         [24]  361 	lcall	_lcd_cmd
                                    362 ;	main.c:55: lcd_puts("Hello");
      00014C 90 01 8E         [24]  363 	mov	dptr,#___str_0
      00014F 75 F0 80         [24]  364 	mov	b, #0x80
      000152 12 01 18         [24]  365 	lcall	_lcd_puts
                                    366 ;	main.c:57: lcd_cmd(0x80 | 0x40); // Line 2, DDRAM=0x40
      000155 75 82 C0         [24]  367 	mov	dpl, #0xc0
      000158 12 00 C9         [24]  368 	lcall	_lcd_cmd
                                    369 ;	main.c:58: lcd_puts("World");
      00015B 90 01 94         [24]  370 	mov	dptr,#___str_1
      00015E 75 F0 80         [24]  371 	mov	b, #0x80
      000161 12 01 18         [24]  372 	lcall	_lcd_puts
                                    373 ;	main.c:60: while (1);
      000164                        374 00102$:
                                    375 ;	main.c:61: }
      000164 80 FE            [24]  376 	sjmp	00102$
                                    377 	.area CSEG    (CODE)
                                    378 	.area CONST   (CODE)
                                    379 	.area CONST   (CODE)
      000186                        380 _LCD_CMD_W:
      000186 70 00                  381 	.byte #0x70,#0x00
                                    382 	.area CSEG    (CODE)
                                    383 	.area CONST   (CODE)
      000188                        384 _LCD_CMD_R:
      000188 71 00                  385 	.byte #0x71,#0x00
                                    386 	.area CSEG    (CODE)
                                    387 	.area CONST   (CODE)
      00018A                        388 _LCD_DATA_W:
      00018A 72 00                  389 	.byte #0x72,#0x00
                                    390 	.area CSEG    (CODE)
                                    391 	.area CONST   (CODE)
      00018C                        392 _LCD_DATA_R:
      00018C 73 00                  393 	.byte #0x73,#0x00
                                    394 	.area CSEG    (CODE)
                                    395 	.area CONST   (CODE)
      00018E                        396 ___str_0:
      00018E 48 65 6C 6C 6F         397 	.ascii "Hello"
      000193 00                     398 	.db 0x00
                                    399 	.area CSEG    (CODE)
                                    400 	.area CONST   (CODE)
      000194                        401 ___str_1:
      000194 57 6F 72 6C 64         402 	.ascii "World"
      000199 00                     403 	.db 0x00
                                    404 	.area CSEG    (CODE)
                                    405 	.area XINIT   (CODE)
                                    406 	.area CABS    (ABS,CODE)
