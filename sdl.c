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
#include <SDL/SDL.h>
#include <SDL/SDL_events.h>

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
unsigned char pixel_buffer[3200];
char *xlinebuffer;
long int xcolors[4096];
void (*DrawPixel)(int, xcolnr);

SDL_Surface *screen;
SDL_Event ev;
static int bytes_per_pixel = 1;
#define NUM_COLORS	4096
 
static void InitXColors();
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
SDL_LockSurface(screen);
	SDL_UpdateRect(screen,0,0,0,0);
SDL_UnlockSurface(screen);
}

static void DrawPixel32(int x, xcolnr col)
{
    Uint8 * pixel = (Uint8*)screen->pixels;
    pixel += (next_pos * screen->pitch) + (x * sizeof(Uint32));
    *((Uint32*)pixel) = col;
}

static void DrawPixel16(int x, xcolnr col)
{
    Uint8 * pixel = (Uint8*)screen->pixels;
    pixel += (next_pos * screen->pitch) + (x * sizeof(Uint16));
    *((Uint16*)pixel) = col;
}

static void DrawPixel8(int x, xcolnr col)
{
    Uint8 * pixel = (Uint8*)screen->pixels;
    pixel += (next_pos * screen->pitch) + (x * sizeof(Uint8));
    *((Uint8*)pixel) = col;
}

void prepare_line (int y, bool need_double)
{
    next_pos = y;
    next_double = need_double;
//    xlinebuffer = pixel_buffer;
}

void flush_line(void)
{
}

//for creating 24bit / 32bit pallete
static __inline__ int bitsInMask(unsigned long mask)
{
    /* count bits in mask */
    int n = 0;
    while(mask) {
	n += mask&1;
	mask >>= 1;
    }
    return n;
}

static __inline__ int maskShift(unsigned long mask)
{
    /* determine how far mask is shifted */
    int n = 0;
    while(!(mask&1)) {
	n++;
	mask >>= 1;
    }
    return n;
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


static unsigned int doAlpha (int alpha, int bits, int shift)
{
	return (alpha & ((1 << bits) - 1)) << shift;
}

void alloc_colors64k (int rw, int gw, int bw, int rs, int gs, int bs, int aw, int as, int alpha) 
{ 
int i,j; 

j=256;

for (i = 0; i < 4096; i++) { 
int r = ((i >> 8) << 4) | (i >> 8); 
int g = (((i >> 4) & 0xf) << 4) | ((i >> 4) & 0x0f); 
int b = ((i & 0xf) << 4) | (i & 0x0f); 
xcolors[i] = doMask(r, rw, rs) | doMask(g, gw, gs) | doMask(b, bw, bs) | doAlpha (alpha, aw, as); 

	} 
}


static void InitXColors(void)
{
SDL_Color palette[NUM_COLORS];
int allocated[4096];

static int red_bits, green_bits, blue_bits;
static int red_shift, green_shift, blue_shift;

//only 24/32bit deep!
	{	    
        red_bits    = bitsInMask (screen->format->Rmask);
        green_bits  = bitsInMask (screen->format->Gmask);
        blue_bits   = bitsInMask (screen->format->Bmask);
        red_shift   = maskShift (screen->format->Rmask);
        green_shift = maskShift (screen->format->Gmask);
        blue_shift  = maskShift (screen->format->Bmask);
	    
	    int i;
	    for(i = 0; i < 4096; i++) {
		int r = i >> 8;
		int g = (i >> 4) & 0xF;
		int b = i & 0xF;
		xcolors[i] = (doMask(r, red_bits, red_shift) 
			      | doMask(g, green_bits, green_shift) 
			      | doMask(b, blue_bits, blue_shift));
	    }
	}
printf("initXcolors\n");
}

void graphics_init(void)
{
int i;
int flags;
int vscale = dont_want_aspect ? 1 : 2;

printf("graphics_init\n");

    buttonstate[0] = buttonstate[1] = buttonstate[2] = false;
    for(i = 0; i < 256; i++) 
	keystate[i] = false;

    lastmx = lastmy = 0; newmousecounters = false;

	/* Set the video mode */
        if(SDL_Init(SDL_INIT_VIDEO)<0)
		{printf("There was an issue with SDL trying to initalize video.\n");
                exit(0);}
        flags=SDL_SWSURFACE;//(SDL_SWSURFACE|SDL_HWPALETTE);
        if (!(screen = SDL_SetVideoMode(796,vscale*(313-29), 32, flags)))
        printf("VID: Couldn't set video mode: %s\n", SDL_GetError());
        SDL_WM_SetCaption("UAE 0.4 (SDL build)","UAE 0.4");
InitXColors();
DrawPixel=DrawPixel32;
}

void graphics_leave(void)
{
printf("graphics_leave\n");
	SDL_Quit();
	exit(0);
}


//static struct timeval lastMotionTime;
int ak_no_repeat;
static int keycode2amiga(int j)
{

    int as;
    int index = 0;
    ak_no_repeat=1;
switch(j){
	case SDLK_a:		return AK_A;
	case SDLK_b:		return AK_B;
	case SDLK_c:		return AK_C;
	case SDLK_d:		return AK_D;
	case SDLK_e:		return AK_E;
	case SDLK_f:		return AK_F;
	case SDLK_g:		return AK_G;
	case SDLK_h:		return AK_H;
	case SDLK_i:		return AK_I;
	case SDLK_j:		return AK_J;
	case SDLK_k:		return AK_K;
	case SDLK_l:		return AK_L;
	case SDLK_m:		return AK_M;
	case SDLK_n:		return AK_N;
	case SDLK_o:		return AK_O;
	case SDLK_p:		return AK_P;
	case SDLK_q:		return AK_Q;
	case SDLK_r:		return AK_R;
	case SDLK_s:		return AK_S;
	case SDLK_t:		return AK_T;
	case SDLK_u:		return AK_U;
	case SDLK_v:		return AK_V;
	case SDLK_w:		return AK_W;
	case SDLK_y:		return AK_Y;
	case SDLK_x:		return AK_X;
	case SDLK_z:		return AK_Z;

	case SDLK_LEFTBRACKET:	return AK_LBRACKET;
	case SDLK_RIGHTBRACKET:	return AK_RBRACKET;
	case SDLK_COMMA:	return AK_COMMA;
	case SDLK_PERIOD:	return AK_PERIOD;
	case SDLK_SLASH:	return AK_SLASH;
	case SDLK_SEMICOLON:	return AK_SEMICOLON;
	case SDLK_MINUS:	return AK_MINUS;
	case SDLK_EQUALS:	return AK_EQUAL;
	case SDLK_QUOTE:	return AK_QUOTE;
	case SDLK_BACKQUOTE:	return AK_QUOTE;	//AK_BACKQUOTE;
	case SDLK_BACKSLASH:	return AK_BACKSLASH;

	case SDLK_0:		return AK_0;
	case SDLK_1:		return AK_1;
	case SDLK_2:		return AK_2;
	case SDLK_3:		return AK_3;
	case SDLK_4:		return AK_4;
	case SDLK_5:		return AK_5;
	case SDLK_6:		return AK_6;
	case SDLK_7:		return AK_7;
	case SDLK_8:		return AK_8;
	case SDLK_9:		return AK_9;

	case SDLK_F1: 		return AK_F1;
	case SDLK_F2: 		return AK_F2;
	case SDLK_F3: 		return AK_F3;
	case SDLK_F4: 		return AK_F4;
	case SDLK_F5: 		return AK_F5;
	case SDLK_F6: 		return AK_F6;
	case SDLK_F7: 		return AK_F7;
	case SDLK_F8: 		return AK_F8;
	case SDLK_F9: 		return AK_F9;
	case SDLK_F10: 		return AK_F10;

	case SDLK_BACKSPACE: 	return AK_BS;
//	case SDLK_DELETE: 	return AK_DEL;
	case SDLK_LCTRL: 	ak_no_repeat=0; return AK_CTRL;
//	case SDLK_RCTRL: 	ak_no_repeat=0; return AK_RCTRL;
	case SDLK_TAB: 		return AK_TAB;
	case SDLK_LALT: 	ak_no_repeat=0; return AK_LALT;
	case SDLK_RALT: 	ak_no_repeat=0; return AK_RALT;
	case SDLK_RMETA: 	ak_no_repeat=0; return AK_RAMI;
	case SDLK_LMETA: 	ak_no_repeat=0; return AK_LAMI;
	case SDLK_RETURN: 	return AK_RET;
	case SDLK_SPACE: 	return AK_SPC;
	case SDLK_LSHIFT: 	ak_no_repeat=0; return AK_LSH;
	case SDLK_RSHIFT: 	ak_no_repeat=0; return AK_RSH;
	case SDLK_ESCAPE: 	return AK_ESC;

	case SDLK_INSERT: 	return AK_HELP;
//	case SDLK_HOME: 	return AK_NPLPAREN;
//	case SDLK_END: 		return AK_NPRPAREN;
//	case SDLK_CAPSLOCK: 	return AK_CAPSLOCK;

	case SDLK_UP: 		return AK_UP;
	case SDLK_DOWN: 	return AK_DN;
	case SDLK_LEFT: 	return AK_LF;
	case SDLK_RIGHT: 	return AK_RT;

	case SDLK_PAGEUP:
	case SDLK_RSUPER:	return AK_RAMI;

	case SDLK_PAGEDOWN:
	case SDLK_LSUPER:	return AK_LAMI;

#ifdef SDL2
//	case SDLK_PAUSE: 	return AKS_PAUSE;
//	case SDLK_SCROLLOCK:	return AKS_INHIBITSCREEN;
//	case SDLK_PRINT: 	return AKS_SCREENSHOT;
	case SDLK_KP0:		return AK_NP0;
	case SDLK_KP1:		return AK_NP1;
	case SDLK_KP2:		return AK_NP2;
	case SDLK_KP3:		return AK_NP3;
	case SDLK_KP4:		return AK_NP4;
	case SDLK_KP5:		return AK_NP5;
	case SDLK_KP6:		return AK_NP6;
	case SDLK_KP7:		return AK_NP7;
	case SDLK_KP8:		return AK_NP8;
	case SDLK_KP9:		return AK_NP9;
	case SDLK_KP_DIVIDE:	return AK_NPDIV;
	case SDLK_KP_MULTIPLY:	return AK_NPMUL;
	case SDLK_KP_MINUS:	return AK_NPSUB;
	case SDLK_KP_PLUS:	return AK_NPADD;
	case SDLK_KP_PERIOD:	return AK_NPDEL;
	case SDLK_KP_ENTER:	return AK_ENT;
#endif
	default:
	break;
	}

return -1;

}

void handle_events(void)
{
SDL_PollEvent(&ev);
	switch(ev.type) {
	case SDL_MOUSEBUTTONDOWN:
		switch(ev.button.button){
			case SDL_BUTTON_LEFT:
				buttonstate[0]=true;
				break;
			case SDL_BUTTON_RIGHT:
				buttonstate[2]=true;
				break;
			default:
				break;
			}
	break;
	case SDL_MOUSEBUTTONUP:
		switch(ev.button.button){
			case SDL_BUTTON_LEFT:
				buttonstate[0]=false;
				break;
			case SDL_BUTTON_RIGHT:
				buttonstate[2]=false;
				break;
			default:
				break;
			}
	break;
	case SDL_MOUSEMOTION:
		lastmx = ev.motion.x;
		lastmy = ev.motion.y;
		break;
	case SDL_KEYDOWN: {
		int kc = keycode2amiga(ev.key.keysym.sym);
	     	 if (!keystate[kc]) {
		     keystate[kc] = true;
		     record_key (kc << 1);
			if(ak_no_repeat)
			     record_key ((kc << 1) | 1);	//hack because the keyboard is way too fast
		 }
//    printf("scancode %d sym %d\n",ev.key.keysym.scancode,ev.key.keysym.sym);
		}
		break;
	case SDL_KEYUP: {
		     int kc = keycode2amiga(ev.key.keysym.sym);
		     if (kc == -1) break;
		     keystate[kc] = false;
		     record_key ((kc << 1) | 1);
		}
		break;
	case SDL_QUIT:
	exit(0);
	break;

	default:
	break;
	}
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
