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
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifndef DONT_WANT_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "xwin.h"
#include "keyboard.h"
#include "keybuf.h"

#ifdef LINUX_SVGALIB
#error Compiling xwin.c, but LINUX_SVGALIB set. Re-edit "config.h".
#endif

static Display *display;
static int screen;
static Window rootwin, mywin;

static GC whitegc,blackgc;
static XColor black,white;
static Colormap cmap;

static XImage *img;
static Visual *vis;
static XVisualInfo visualInfo;
static int bitdepth;
#ifndef DONT_WANT_SHM
static XShmSegmentInfo shminfo;
#endif
static Cursor blankCursor, xhairCursor;
static bool cursorOn;
 
#ifdef DONT_WANT_SHM
static char pixel_buffer[3200];
#endif
static char *xlinestart;
char *xlinebuffer;
long int xcolors[4096];

 /* Keyboard and mouse */

static bool keystate[256];

bool buttonstate[3];
int lastmx, lastmy;
bool newmousecounters;
static bool inwindow;
const long int eventmask = (KeyPressMask|KeyReleaseMask|ButtonPressMask
			    |ButtonReleaseMask|PointerMotionMask
			    |FocusChangeMask|EnterWindowMask
			    |LeaveWindowMask);

static bool next_line_double;
static int next_line_pos = 0;

void flush_screen (void)
{
#ifndef DONT_WANT_SHM
    if (next_line_pos > 0) {	
	XShmPutImage(display, mywin, blackgc, img, 0, 0, 0, 0, 796, next_line_pos, 0);
	XSync(display, 0);
	next_line_pos = 0;
    }
#endif
}

void prepare_line (int y, bool need_double)
{
    next_line_double = need_double;
    next_line_pos = y;
    
#ifdef DONT_WANT_SHM
    xlinebuffer = pixel_buffer;
#else
    xlinestart = xlinebuffer = img->data + y * img->bytes_per_line;
#endif
}

void flush_line(void)
{
#ifdef DONT_WANT_SHM
    XPutImage(display, mywin, blackgc, img, 0, 0, 0, next_line_pos, 796, 1);
    if (next_line_double)
    	XPutImage(display, mywin, blackgc, img, 0, 0, 0, next_line_pos+1, 796, 1);
#else
    if (use_fast_draw && next_line_double) {	
    	memcpy (xlinestart + img->bytes_per_line, xlinestart, img->bytes_per_line);
	next_line_pos++; /* For flush_screen(). */
    }
#endif
}

#ifndef INLINE_DRAWPIXEL
void (*DrawPixel)(int, xcolnr);

static void DrawPixel16(int x, xcolnr col)
{
    *(short *)xlinebuffer = col;
    xlinebuffer += 2;
}

static void DrawPixel32(int x, xcolnr col)
{
    *(long *)xlinebuffer = col;
    xlinebuffer += 4;
}

static void DrawPixel8(int x, xcolnr col)
{
    *(char *)xlinebuffer = col;
    xlinebuffer += 1;
}

static void DrawPixelGeneric(int x, xcolnr col)
{
#ifdef DONT_WANT_SHM
    XPutPixel(img, x, 0, col);
#else
    XPutPixel(img, x, next_line_pos, col);
    if (next_line_double) {
	XPutPixel(img, x, next_line_pos+1, col);
    }
#endif
}
#endif /* INLINE_DRAWPIXEL */

#ifndef DONT_WANT_SHM
static void initShm(int depth)
{
    int scale = dont_want_aspect ? 1 : 2;
    img = XShmCreateImage(display, vis, depth,
			  ZPixmap, 0, &shminfo, 800, (313-29) * scale);
    
    shminfo.shmid = shmget(IPC_PRIVATE, 512*1024*scale, IPC_CREAT|0777);
    shminfo.shmaddr = img->data = (char *)shmat(shminfo.shmid, 0, 0);
    shminfo.readOnly = False;
    /* let the xserver attach */
    XShmAttach(display, &shminfo);
    /* NOW ! */
    XSync(display,0);
    /* now deleting means making it temporary */
    shmctl(shminfo.shmid, IPC_RMID, 0);
    
    xlinestart = xlinebuffer = img->data;
}
#endif

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

static void InitXColors(void)
{
#ifdef INLINE_DRAWPIXEL

#ifdef X_8BIT_SCREEN
    if (visualInfo.depth != 8) {	
    	fprintf(stderr, "Unsupported bit depth (%d)\n", visualInfo.depth);
	exit(1);
    }
#endif

#ifdef X_16BIT_SCREEN
    if (visualInfo.depth != 12 && visualInfo.depth != 16) {
    	fprintf(stderr, "Unsupported bit depth (%d)\n", visualInfo.depth);
	exit(1);
    }
#endif

#else /* not INLINE_DRAWPIXEL */
    DrawPixel = DrawPixelGeneric;

    if(use_fast_draw) {
	/* this is probably a lot faster but makes nonportable assumptions
	 * about image format */
	
	switch(visualInfo.depth) {
	 case 12:
	    DrawPixel = DrawPixel16;
	    break;
	    
	 case 24:
	    DrawPixel = DrawPixel32;
	    break;

	 case 16:
	    DrawPixel = DrawPixel16;
	    break;

	 case 8:
	    DrawPixel = DrawPixel8;
	    break;

	 default:
	    fprintf(stderr, "Unsupported bit depth (%d)\n", visualInfo.depth);
	    exit(1);
	}
    }
#endif /* not INLINE_DRAWPIXEL */
    
#ifdef __cplusplus
    switch(visualInfo.c_class) {
#else
    switch(visualInfo.class) {
#endif
     case TrueColor: 
	{	    
	    int red_bits = bitsInMask(visualInfo.red_mask);
	    int green_bits = bitsInMask(visualInfo.green_mask);
	    int blue_bits = bitsInMask(visualInfo.blue_mask);
	    int red_shift = maskShift(visualInfo.red_mask);
	    int green_shift = maskShift(visualInfo.green_mask);
	    int blue_shift = maskShift(visualInfo.blue_mask);
	    
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
	break;

     case GrayScale:
     case PseudoColor:
	{
	    /* This is kind of kludgy...
	     * Try to allocate as many different colors as possible. */
	    int col;
	    int step = 16;
	    int allocated[4096];
	    memset(allocated,0,sizeof allocated);
	    
	    while ((step/=2) > 0) {
		int r, g, b;
		for(r=0; r<16; r += step) {
		    for(g=0; g<16; g += step) {
			for(b=0; b<16; b += step) {
			    int cnr = (r << 8) + (g << 4) + b;
			    if (!allocated[cnr]) {
				XColor col;
				char str[10];
				sprintf(str, "rgb:%x/%x/%x", r,g,b);
				XParseColor(display,cmap,str,&col);
				if (XAllocColor(display,cmap,&col)) {			    
				    allocated[cnr] = true;
				    xcolors[cnr] = col.pixel;
				} 
			    }
			}
		    }
		}
	    }

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
	}
	break;
	
     default:
#ifdef __cplusplus
	fprintf(stderr, "Unsupported class (%d)\n", visualInfo.c_class);
#else
	fprintf(stderr, "Unsupported class (%d)\n", visualInfo.class);
#endif
	exit(1);
    }
}

void graphics_init(void)
{
    int i;
    char *display_name = 0;
    XSetWindowAttributes wattr;
    int vscale = dont_want_aspect ? 1 : 2;

    display = XOpenDisplay(display_name);
    if (display == 0)  {
	fprintf(stderr, "Can't connect to X server %s\n", XDisplayName(display_name));
	exit(-1);
    }
    screen = XDefaultScreen(display);
    rootwin = XRootWindow(display,screen);

    /* try for a 12 bit visual first, then a 16 bit, then a 24 bit, then 8 bit */
    if (XMatchVisualInfo(display, screen, 12, TrueColor, &visualInfo)) {
    } else if (XMatchVisualInfo(display, screen, 16, TrueColor, &visualInfo)) {
    } else if (XMatchVisualInfo(display, screen, 24, TrueColor, &visualInfo)) {
    } else if (XMatchVisualInfo(display, screen, 8, PseudoColor, &visualInfo)) {
      /* for our HP boxes */
    } else if (XMatchVisualInfo(display, screen, 8, GrayScale, &visualInfo)) {
    } else {
	fprintf(stderr, "Can't obtain appropriate X visual\n");
	exit(1);
    }

    vis = visualInfo.visual;
    bitdepth = visualInfo.depth;

    fprintf(stderr, "Using %d bit visual\n", bitdepth);

    cmap = XCreateColormap(display, rootwin, vis, AllocNone);
    XParseColor(display, cmap, "#000000", &black);
    if (!XAllocColor(display, cmap, &black))
	fprintf(stderr, "Whoops??\n");
    XParseColor(display, cmap, "#ffffff", &white);
    if (!XAllocColor(display, cmap, &white))
	fprintf(stderr, "Whoops??\n");

    wattr.event_mask = eventmask;
    wattr.background_pixel = black.pixel;
    wattr.backing_store = Always;
    wattr.backing_planes = bitdepth;
    wattr.border_pixmap = None;
    wattr.border_pixel = black.pixel;
    wattr.colormap = cmap;

    mywin = XCreateWindow(display,rootwin,0,0,796,vscale*(313-29),0,
			  bitdepth, InputOutput, vis,
			  CWEventMask|CWBackPixel|CWBorderPixel|CWBackingStore
			  |CWBackingPlanes|CWColormap,
			  &wattr);
    XMapWindow(display,mywin);
    
    blankCursor = XCreatePixmapCursor(display,
				      XCreatePixmap(display, mywin, 1, 1, 1),
				      XCreatePixmap(display, mywin, 1, 1, 1), 
				      &black, &white, 0, 0);
    xhairCursor = XCreateFontCursor(display, XC_crosshair);

    whitegc = XCreateGC(display,mywin,0,0);
    blackgc = XCreateGC(display,mywin,0,0);
    
    XSetForeground(display,blackgc,black.pixel);
    XSetForeground(display,whitegc,white.pixel);

#ifdef DONT_WANT_SHM
    img = XCreateImage(display, vis, bitdepth, ZPixmap, 0, pixel_buffer, 800, 
		       1, 32, 0);
#else
    initShm (bitdepth);
#endif
    InitXColors();
    buttonstate[0] = buttonstate[1] = buttonstate[2] = false;
    for(i=0; i<256; i++) keystate[i] = false;
    
    lastmx = lastmy = 0; newmousecounters = false; inwindow = false;
	XDefineCursor(display, mywin, xhairCursor);
	cursorOn = true;
}

void graphics_leave(void)
{
    XAutoRepeatOn(display);
}

/* Decode KeySyms. This function knows about all keys that are common 
 * between different keyboard languages. */
static int kc_decode (KeySym ks)
{
    switch (ks) {	
     case XK_A: case XK_a: return AK_A;
     case XK_B: case XK_b: return AK_B;
     case XK_C: case XK_c: return AK_C;
     case XK_D: case XK_d: return AK_D;
     case XK_E: case XK_e: return AK_E;
     case XK_F: case XK_f: return AK_F;
     case XK_G: case XK_g: return AK_G;
     case XK_H: case XK_h: return AK_H;
     case XK_I: case XK_i: return AK_I;
     case XK_J: case XK_j: return AK_J;
     case XK_K: case XK_k: return AK_K;
     case XK_L: case XK_l: return AK_L;
     case XK_M: case XK_m: return AK_M;
     case XK_N: case XK_n: return AK_N;
     case XK_O: case XK_o: return AK_O;
     case XK_P: case XK_p: return AK_P;
     case XK_Q: case XK_q: return AK_Q;
     case XK_R: case XK_r: return AK_R;
     case XK_S: case XK_s: return AK_S;
     case XK_T: case XK_t: return AK_T;
     case XK_U: case XK_u: return AK_U;
     case XK_V: case XK_v: return AK_V;
     case XK_W: case XK_w: return AK_W;
     case XK_X: case XK_x: return AK_X;
	
     case XK_0: return AK_0;
     case XK_1: return AK_1;
     case XK_2: return AK_2;
     case XK_3: return AK_3;
     case XK_4: return AK_4;
     case XK_5: return AK_5;
     case XK_6: return AK_6;
     case XK_7: return AK_7;
     case XK_8: return AK_8;
     case XK_9: return AK_9;
	
     case XK_KP_0: return AK_NP0;
     case XK_KP_1: return AK_NP1;
     case XK_KP_2: return AK_NP2;
     case XK_KP_3: return AK_NP3;
     case XK_KP_4: return AK_NP4;
     case XK_KP_5: return AK_NP5;
     case XK_KP_6: return AK_NP6;
     case XK_KP_7: return AK_NP7;
     case XK_KP_8: return AK_NP8;
     case XK_KP_9: return AK_NP9;
	
     case XK_F1: return AK_F1;
     case XK_F2: return AK_F2;
     case XK_F3: return AK_F3;
     case XK_F4: return AK_F4;
     case XK_F5: return AK_F5;
     case XK_F6: return AK_F6;
     case XK_F7: return AK_F7;
     case XK_F8: return AK_F8;
     case XK_F9: return AK_F9;
     case XK_F10: return AK_F10;
	    
     case XK_BackSpace: case XK_Delete: return AK_BS;
     case XK_Control_L: return AK_CTRL;
     case XK_Tab: return AK_TAB;
     case XK_Alt_L: case XK_Meta_L: return AK_LALT;
     case XK_Alt_R: case XK_Meta_R: return AK_RALT;
     case XK_Return: return AK_RET;
     case XK_space: return AK_SPC;
     case XK_Shift_L: return AK_LSH;
     case XK_Shift_R: return AK_RSH;
     case XK_Escape: return AK_ESC;

     case XK_Up: return AK_UP;
     case XK_Down: return AK_DN;
     case XK_Left: return AK_LF;
     case XK_Right: return AK_RT;
	
     case XK_F11: return AK_BACKSLASH;
     case XK_F12: return AK_mousestuff;
     case XK_Scroll_Lock: return AK_inhibit;

#ifdef XK_Page_Up /* These are missing occasionally */
     case XK_Page_Up: return AK_RAMI;          /* PgUp mapped to right amiga */
     case XK_Page_Down: return AK_LAMI;        /* PgDn mapped to left amiga */
#endif
    }
    return -1;
}

static int decode_us(KeySym ks)
{
    switch(ks) {	/* US specific */	
     case XK_Y: case XK_y: return AK_Y;
     case XK_Z: case XK_z: return AK_Z;
     case XK_bracketleft: return AK_LBRACKET;
     case XK_bracketright: return AK_RBRACKET;
     case XK_comma: return AK_COMMA;
     case XK_period: return AK_PERIOD;
     case XK_slash: return AK_SLASH;
     case XK_semicolon: return AK_SEMICOLON;
     case XK_minus: return AK_MINUS;
     case XK_equal: return AK_EQUAL;
	/* this doesn't work: */
     case XK_grave: return AK_QUOTE;
    }

    return -1;
}

static int decode_de(KeySym ks)
{
    switch(ks) {
	/* DE specific */
     case XK_Y: case XK_y: return AK_Z;
     case XK_Z: case XK_z: return AK_Y;
     case XK_Odiaeresis: case XK_odiaeresis: return AK_SEMICOLON;
     case XK_Adiaeresis: case XK_adiaeresis: return AK_QUOTE;
     case XK_Udiaeresis: case XK_udiaeresis: return AK_LBRACKET;
     case XK_plus: case XK_asterisk: return AK_RBRACKET;
     case XK_comma: return AK_COMMA;
     case XK_period: return AK_PERIOD;
     case XK_less: case XK_greater: return AK_LTGT;
     case XK_numbersign: return AK_NUMBERSIGN;
     case XK_ssharp: return AK_MINUS;
     case XK_apostrophe: return AK_EQUAL;
     case XK_asciicircum: return AK_00;
     case XK_minus: return AK_SLASH;	    
    }

    return -1;
}

static int keycode2amiga(XKeyEvent *event)
{
    KeySym ks;
    int as;
    int index = 0;
    
    do {
	ks = XLookupKeysym(event, index);
	as = kc_decode (ks);
	
	if (as == -1) {	    
	    switch(keyboard_lang) {
	     case KBD_LANG_US:
		as = decode_us(ks);
		break;
		
	     case KBD_LANG_DE:
		as = decode_de(ks);
		break;
		
	     default:
		as = -1;
		break;
	    }
	}
	if(-1 != as)
		return as;
	index++;
    } while (ks != NoSymbol);
    return -1;
}

static struct timeval lastMotionTime;

void handle_events(void)
{
    bool repeat;
    newmousecounters = false;
    do {
	XEvent event;
	if (!XCheckMaskEvent(display, eventmask, &event)) break;
	repeat = false;
	
	switch(event.type) {
	 case KeyPress: {		
	     int kc = keycode2amiga((XKeyEvent *)&event);
	     if (kc == -1) break;
	     switch (kc) {
	      case AK_mousestuff:
#if 0
	         if (keystate[AK_CTRL])
		     mousesetup();
		 else 
#endif
		     togglemouse();
		 break;

	      case AK_inhibit:
		 inhibit_frame ^= 1;
		 break;

	      default:
	     	 if (!keystate[kc]) {
		     keystate[kc] = true;
		     record_key (kc << 1);
		 }
		 break;
	     }
	     break;
	 }
	 case KeyRelease: {	     
	     int kc = keycode2amiga((XKeyEvent *)&event);
	     if (kc == -1) break;
	     keystate[kc] = false;
	     record_key ((kc << 1) | 1);
	     break;
	 }
	 case ButtonPress:
	    buttonstate[((XButtonEvent *)&event)->button-1] = true;
	    break;
	 case ButtonRelease:
	    buttonstate[((XButtonEvent *)&event)->button-1] = false;
	    break;
	 case EnterNotify:
	    newmousecounters = true;
	    lastmx = ((XCrossingEvent *)&event)->x;
	    lastmy = ((XCrossingEvent *)&event)->y;
	    repeat = true;
	    inwindow = true;
	    break;
	 case LeaveNotify:
	    inwindow = false;
	    repeat = true;
	    break;
	 case FocusIn:
	    XAutoRepeatOff(display);
	    repeat = true;
	    break;
	 case FocusOut:
	    XAutoRepeatOn(display);
	    repeat = true;
	    break;
	 case MotionNotify:
	    if (inwindow) {		
		lastmx = ((XMotionEvent *)&event)->x;
		lastmy = ((XMotionEvent *)&event)->y;
		if(!cursorOn) {
		    XDefineCursor(display, mywin, xhairCursor);
		    cursorOn = true;
		}
		gettimeofday(&lastMotionTime, NULL);
	    }
	    repeat = true;
	    break;
	}
    } while (repeat);
    
    if(cursorOn) {
	struct timeval now;
	int diff;
	gettimeofday(&now, NULL);
	diff = (now.tv_sec - lastMotionTime.tv_sec) * 1000000 + 
	    (now.tv_usec - lastMotionTime.tv_usec);
	if(diff > 1000000) {
	    XDefineCursor(display, mywin, blankCursor);
	    cursorOn = false;
	}
    }
	
    /* "Affengriff" */
    if(keystate[AK_CTRL] && keystate[AK_LAMI] && keystate[AK_RAMI])
    	MC68000_reset();
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
	XKeyboardControl control;
	control.led = 1; /* implementation defined */
	control.led_mode = on ? LedModeOn : LedModeOff;
	XChangeKeyboardControl(display, KBLed | KBLedMode, &control);
}
