 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * X interface
  * 
  * Copyright 1995, 1996 Bernd Schmidt
  * Copyright 1996 Ed Hanway, Andre Beck
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>


#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "xwin.h"
#include "keyboard.h"
#include "keybuf.h"


static bool cursorOn;
static char pixel_buffer[3200];
char *xlinebuffer;
void (*DrawPixel)(int, xcolnr);
 
long int xcolors[4096];

 /* Keyboard and mouse */

static bool keystate[256];

bool buttonstate[3];
int lastmx, lastmy;
bool newmousecounters;
static bool inwindow;

static int next_pos;
static bool next_double;

void flush_screen (void)
{
//printf("flush_screen\n");

}

static void DrawPixel8(int x, xcolnr col)
{
}

void prepare_line (int y, bool need_double)
{
    next_pos = y;
    next_double = need_double;
    xlinebuffer = pixel_buffer;
}

void flush_line(void)
{
//printf("flush_line\n");
}


static void InitXColors(void)
{
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
			//vga_setpalette(count-1,(r*63+7)/15,(g*63+7)/15,(b*63+7)/15);
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

printf("initXcolors\n");
}

void graphics_init(void)
{
int i;
printf("graphics_init\n");
xlinebuffer=malloc(800*400*2);
InitXColors();
    buttonstate[0] = buttonstate[1] = buttonstate[2] = false;
    for(i = 0; i < 256; i++) 
	keystate[i] = false;

    lastmx = lastmy = 0; newmousecounters = false;
DrawPixel = DrawPixel8;
}

void graphics_leave(void)
{

}


static struct timeval lastMotionTime;

void handle_events(void)
{
//printf(".");
}

bool debuggable(void)
{
    return true;
}

bool needmousehack(void)
{
    return true;
}

void LED(int on)
{
}
