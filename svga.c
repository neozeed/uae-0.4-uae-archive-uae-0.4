 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * SVGAlib interface.
  * 
  * (c) 1995 Bernd Schmidt
  */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <vga.h>
#include <vgamouse.h>
#include <vgakeyboard.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "keyboard.h"
#include "xwin.h"
#include "keybuf.h"

#ifndef LINUX_SVGALIB
#error Compiling svga.c, but LINUX_SVGALIB unset. Re-edit "config.h".
#endif

/* Why doesn't SVGAlib define these !??? */

#define SCANCODE_0 11
#define SCANCODE_1 2
#define SCANCODE_2 3
#define SCANCODE_3 4
#define SCANCODE_4 5 
#define SCANCODE_5 6
#define SCANCODE_6 7 
#define SCANCODE_7 8
#define SCANCODE_8 9
#define SCANCODE_9 10

#define SCANCODE_LEFTSHIFT 42
#define SCANCODE_RIGHTSHIFT 54
#define SCANCODE_TAB 15

#define SCANCODE_F11 87
#define SCANCODE_F12 88
#define SCANCODE_NEXT 81
#define SCANCODE_PRIOR 73
#define SCANCODE_BS 14
/*
#define SCANCODE_asciicircum 1
*/
#define SCANCODE_bracketleft 26
#define SCANCODE_bracketright 27
#define SCANCODE_comma 51
#define SCANCODE_period 52
#define SCANCODE_slash 53
#define SCANCODE_semicolon 39
#define SCANCODE_grave 40
#define SCANCODE_minus 12
#define SCANCODE_equal 13
#define SCANCODE_numbersign 41
#define SCANCODE_ltgt 43
#define SCANCODE_scrolllock 70

static char pixel_buffer[3200];
char *xlinebuffer;

long int xcolors[4096];

static int next_pos;
static bool next_double;

void prepare_line(int y, bool need_double)
{
    next_pos = y;
    next_double = need_double;
    xlinebuffer = pixel_buffer;
}

void flush_line(void)
{    
    vga_drawscanline(next_pos, pixel_buffer);
    if (next_double)
    	vga_drawscanline(next_pos+1, pixel_buffer);
}

void flush_screen(void)
{
}

static __inline__ unsigned long doMask(int p, int bits, int shift)
{
    /* p is a value from 0 to 15 (Amiga color value)
     * scale to 0..255, shift to align msb with mask, and apply mask */

    unsigned long val = p * 0x11111111UL;
    val >>= (32 - bits);
    val <<= shift;

    return val;
}

static void init_colors(void)
{
#ifdef SVGALIB_16BIT_SCREEN
    int i;
    for(i=0; i<4096; i++) {
	int r = i >> 8;
	int g = (i >> 4) & 0xF;
	int b = i & 0xF;
	xcolors[i] = (doMask(r, SVGALIB_R_WEIGHT, SVGALIB_G_WEIGHT + SVGALIB_B_WEIGHT) 
		      | doMask(g, SVGALIB_G_WEIGHT, SVGALIB_B_WEIGHT)
		      | doMask(b, SVGALIB_B_WEIGHT, 0));
    }
#endif
#ifdef SVGALIB_8BIT_SCREEN
    /* This is kind of kludgy...
     * Try to allocate as many different colors as possible. */
    int step = 16, count = 0, col;
    int allocated[4096];
    memset(allocated,0,sizeof allocated);
    
    while ((step/=2) > 0) {
	int r, g, b;
	for(r = 0; r < 16; r += step) {
	    for(g = 0; g < 16; g += step) {
		for(b = 0; b < 16; b += step) {
		    int cnr = (r << 8) + (g << 4) + b;
		    if (++count == 256)
		    	goto finished;
		    if (!allocated[cnr]) {
			allocated[cnr] = true;
			xcolors[cnr] = count-1;
			vga_setpalette(count-1,(r*63+7)/15,(g*63+7)/15,(b*63+7)/15);
		    }
		}
	    }
	}
    }
    finished:
    for(col = 0; col < 4096; col++) {
	int cnr = col;
	if (!allocated[cnr]) {
	    int r = cnr >> 8;
	    int g = (cnr >> 4) & 0xF;
	    int b = cnr & 0xF;
	    int maxd = 4096,best = 0;
	    int c2;
	    for(c2 = 0; c2 < 4096; c2++)
	    	if (allocated[c2]) {
		    int r2 = c2 >> 8;
		    int g2 = (c2 >> 4) & 0xF;
		    int b2 = c2 & 0xF;
		    int dist = abs(r2-r)*2 + abs(g2-g)*3 + abs(b2-b);
		    if (dist < maxd) {
			maxd = dist;
			best = c2;
		    }
		}
	    cnr = best;
	}
	xcolors[col] = xcolors[cnr];
    }
#endif
}

bool buttonstate[3] = { false, false, false };
int lastmx, lastmy;
bool newmousecounters = false;

static bool keystate[256];

static int scancode2amiga(int scancode)
{
    switch(scancode) {
     case SCANCODE_A: return AK_A;
     case SCANCODE_B: return AK_B;
     case SCANCODE_C: return AK_C;
     case SCANCODE_D: return AK_D;
     case SCANCODE_E: return AK_E;
     case SCANCODE_F: return AK_F;
     case SCANCODE_G: return AK_G;
     case SCANCODE_H: return AK_H;
     case SCANCODE_I: return AK_I;
     case SCANCODE_J: return AK_J;
     case SCANCODE_K: return AK_K;
     case SCANCODE_L: return AK_L;
     case SCANCODE_M: return AK_M;
     case SCANCODE_N: return AK_N;
     case SCANCODE_O: return AK_O;
     case SCANCODE_P: return AK_P;
     case SCANCODE_Q: return AK_Q;
     case SCANCODE_R: return AK_R;
     case SCANCODE_S: return AK_S;
     case SCANCODE_T: return AK_T;
     case SCANCODE_U: return AK_U;
     case SCANCODE_V: return AK_V;
     case SCANCODE_W: return AK_W;
     case SCANCODE_X: return AK_X;
     case SCANCODE_Y: return AK_Y;
     case SCANCODE_Z: return AK_Z;
	
     case SCANCODE_0: return AK_0;
     case SCANCODE_1: return AK_1;
     case SCANCODE_2: return AK_2;
     case SCANCODE_3: return AK_3;
     case SCANCODE_4: return AK_4;
     case SCANCODE_5: return AK_5;
     case SCANCODE_6: return AK_6;
     case SCANCODE_7: return AK_7;
     case SCANCODE_8: return AK_8;
     case SCANCODE_9: return AK_9;
	
     case SCANCODE_KEYPAD0: return AK_NP0;
     case SCANCODE_KEYPAD1: return AK_NP1;
     case SCANCODE_KEYPAD2: return AK_NP2;
     case SCANCODE_KEYPAD3: return AK_NP3;
     case SCANCODE_KEYPAD4: return AK_NP4;
     case SCANCODE_KEYPAD5: return AK_NP5;
     case SCANCODE_KEYPAD6: return AK_NP6;
     case SCANCODE_KEYPAD7: return AK_NP7;
     case SCANCODE_KEYPAD8: return AK_NP8;
     case SCANCODE_KEYPAD9: return AK_NP9;
	
     case SCANCODE_F1: return AK_F1;
     case SCANCODE_F2: return AK_F2;
     case SCANCODE_F3: return AK_F3;
     case SCANCODE_F4: return AK_F4;
     case SCANCODE_F5: return AK_F5;
     case SCANCODE_F6: return AK_F6;
     case SCANCODE_F7: return AK_F7;
     case SCANCODE_F8: return AK_F8;
     case SCANCODE_F9: return AK_F9;
     case SCANCODE_F10: return AK_F10;
	
     case SCANCODE_BS: return AK_BS;
     case SCANCODE_CONTROL: return AK_CTRL;
     case SCANCODE_TAB: return AK_TAB;
     case SCANCODE_LEFTALT: return AK_LALT;
     case SCANCODE_RIGHTALT: return AK_RALT;
     case SCANCODE_ENTER: return AK_RET;
     case SCANCODE_SPACE: return AK_SPC;
     case SCANCODE_LEFTSHIFT: return AK_LSH;
     case SCANCODE_RIGHTSHIFT: return AK_RSH;
     case SCANCODE_ESCAPE: return AK_ESC;
	
     case SCANCODE_CURSORBLOCKUP: return AK_UP;
     case SCANCODE_CURSORBLOCKDOWN: return AK_DN;
     case SCANCODE_CURSORBLOCKLEFT: return AK_LF;
     case SCANCODE_CURSORBLOCKRIGHT: return AK_RT;
	
     case SCANCODE_F11: return AK_BACKSLASH;
/*
     case SCANCODE_asciicircum: return AK_00;
 */
     case SCANCODE_bracketleft: return AK_LBRACKET;
     case SCANCODE_bracketright: return AK_RBRACKET;
     case SCANCODE_comma: return AK_COMMA;
     case SCANCODE_period: return AK_PERIOD;
     case SCANCODE_slash: return AK_SLASH;
     case SCANCODE_semicolon: return AK_SEMICOLON;
     case SCANCODE_grave: return AK_QUOTE;
     case SCANCODE_minus: return AK_MINUS;
     case SCANCODE_equal: return AK_EQUAL;
	
	/* This one turns off screen updates. */
     case SCANCODE_scrolllock: return AK_inhibit;

#if 0 /* No unique scancode for these yet. I need a Win95 keyboard :-( */
     case SCANCODE_NEXT: return AK_RAMI;          /* PgUp mapped to right amiga */
     case SCANCODE_PRIOR: return AK_LAMI;         /* PgDn mapped to left amiga */ 
#endif
	
/*#ifdef KBD_LANG_DE*/
     case SCANCODE_numbersign: return AK_NUMBERSIGN;
     case SCANCODE_ltgt: return AK_LTGT;
/*#endif*/
    }
    return -1;
}

static void my_kbd_handler(int scancode, int newstate)
{
    int akey = scancode2amiga(scancode);
    
    assert(scancode >= 0 && scancode < 0x100);
    if (scancode == SCANCODE_F12)
    	specialflags |= SPCFLAG_BRK;
    if (keystate[scancode] == newstate)
    	return;
    keystate[scancode] = newstate;

    if (akey == -1)
    	return;

    if (newstate == KEY_EVENTPRESS) {
	if (akey == AK_inhibit)
	    inhibit_frame ^= 1;
	else
	    record_key (akey << 1);
    } else
    	record_key ((akey << 1) | 1);
    
    /* "Affengriff" */
    if(keystate[AK_CTRL] && keystate[AK_LAMI] && keystate[AK_RAMI])
    	MC68000_reset();
}

void graphics_init(void)
{
    int i;
    
    vga_init();
    vga_setmousesupport(1);
    mouse_init("/dev/mouse",vga_getmousetype(),10);
#ifdef SVGALIB_8BIT_SCREEN
    vga_setmode(SVGALIB_MODE_8);
#endif
#ifdef SVGALIB_16BIT_SCREEN
    vga_setmode(SVGALIB_MODE_16);
#endif
    if (keyboard_init() != 0) 
    	abort();
    keyboard_seteventhandler(my_kbd_handler);
    
    init_colors();
    
    buttonstate[0] = buttonstate[1] = buttonstate[2] = false;
    for(i = 0; i < 256; i++) 
	keystate[i] = false;

    lastmx = lastmy = 0; newmousecounters = false;

    mouse_setxrange(-1000,1000);
    mouse_setyrange(-1000,1000);
    mouse_setposition(0,0);
}

void graphics_leave(void)
{
    sleep(1); /* Maybe this will fix the "screen full of garbage" problem */
    vga_setmode(TEXT);
    keyboard_close();
}

void handle_events(void)
{
    int button = mouse_getbutton();
    
    keyboard_update();
    mouse_update();
    lastmx += mouse_getx();
    lastmy += mouse_gety();
    mouse_setposition(0,0);

    buttonstate[0] = button & 4;
    buttonstate[1] = button & 2;
    buttonstate[2] = button & 1;
}

bool debuggable(void)
{
    return false;
}

bool needmousehack(void)
{
    return false;
}

void LED(int on)
{
}
