 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Mac port specific stuff
  * 
  * (c) 1996 Ernesto Corvi
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <QDOffscreen.h>
#include <Palettes.h>
#include <Profiler.h>

#include "mackbd.h"
#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "xwin.h"
#include "keyboard.h"
#include "keybuf.h"

#define kQuitError 128

extern bool left, right, top, bot, but;

KeyMap keys,keysold;
GDHandle		curDevice;
int		oldDepth=0;
static int screen;
Boolean	RM=false,Joy=false;
WindowPtr mywin;
short	gOldMBarHeight;
RgnHandle gMBarRgn;
static int bitdepth;
KeyMap theKeys;

unsigned long refresh;
char *xlinebuffer;
unsigned int *obuff;
long int xcolors[4096];

 /* Keyboard and mouse */

static bool keystate[256];

bool buttonstate[3];
int lastmx, lastmy;
bool newmousecounters;
static bool inwindow;

int GetRawCode (long set0, long set1, long set2, long set3);

static inline unsigned long doMask(int p, int bits, int shift)
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
    int n=16;
    int red_bits = 5;
    int green_bits = 5;
    int blue_bits = 5;
    int red_shift = 10;
    int green_shift = 5;
    int blue_shift = 0;
    int i=0;
    int r,g,b;
	
    for(i = 0; i < 4096; i++) {
	r = i >> 8;
	g = (i >> 4) & 0xF;
	b = i & 0xF;
	xcolors[i] = (doMask(r, red_bits, red_shift) 
		      | doMask(g, green_bits, green_shift) 
		      | doMask(b, blue_bits, blue_shift));
    }
}

static void InitToolbox(void)
{
	InitGraf (&qd.thePort);
	InitFonts ();
	FlushEvents (everyEvent,0);
	InitWindows ();
	InitMenus ();
	TEInit ();
	InitDialogs (nil);
	InitCursor ();
}

static void HideMenuBar(void)
{
    Rect mBarRect;

    /* GET RID OF THE MENU BAR */
					
    gOldMBarHeight = GetMBarHeight();
    /* make the Menu Bar's height zero */
    LMSetMBarHeight(0);
    SetRect(&mBarRect, qd.screenBits.bounds.left, qd.screenBits.bounds.top,
	    qd.screenBits.bounds.right, qd.screenBits.bounds.top + gOldMBarHeight);
    gMBarRgn = NewRgn();
    RectRgn(gMBarRgn, &mBarRect);
    UnionRgn(LMGetGrayRgn(), gMBarRgn, LMGetGrayRgn());	/* tell the desktop it covers the menu bar */

    PaintOne(nil, gMBarRgn); /* redraw desktop */
}

static void ShowMenuBar(void)
{
    LMSetMBarHeight(gOldMBarHeight); /* make the menu bar's height normal */
    DiffRgn(LMGetGrayRgn(), gMBarRgn, LMGetGrayRgn()); /* remove the menu bar from the desktop */
    DisposeRgn(gMBarRgn);
}

void graphics_init()
{	int i;
	long p1;
	long tmp;

    Rect	windowRectangle={0,0,480,640};
    InitToolbox();
	
	if (CheckForSetup()) ExitToShell();
	
	HideMenuBar();
	mywin = NewCWindow(nil, &windowRectangle, "\pUAE", true, 2, (WindowPtr)-1L, false, 0);
    SetPort(mywin);

    PaintRect(&qd.screenBits.bounds);
    
    xlinebuffer = (char *)malloc (3200);
    InitXColors();
    buttonstate[0] = buttonstate[1] = buttonstate[2] = false;
    for(i=0; i<256; i++) keystate[i] = false;
    
    lastmx = lastmy = 0; newmousecounters = false; inwindow = false;
    
    obuff=(unsigned int *)NewPtrClear(481*1280);
	{
		for(p1=0;p1 < (480*1280);p1++)
			*obuff=-1;
	}
	
	HideCursor();
    refresh=TickCount();
    
/*    tmp=ProfilerInit(collectDetailed,bestTimeBase,200,10); */
}

void graphics_leave()
{
/*	ProfilerDump((unsigned char *)"\pmyProf");
	ProfilerTerm();
 */
    DisposeWindow(mywin);
    ShowCursor();
    if (oldDepth != 0) SetDepth(curDevice,oldDepth,0,0);
    ShowMenuBar();
    FlushEvents (everyEvent,0);
    ExitToShell();
}

static bool next_line_double;
static int next_line_pos = 0;

void flush_screen ()
{  
    GrafPtr oldPort;
    short	y,x,daColor=0;
    RGBColor	daColorRGB;
    unsigned char *winbaseaddr;
    double *src,*dest;
    unsigned long winrowbytes;
    PixMapHandle	ph;
    
    if (TickCount() < refresh + 4) return;
    GetPort(&oldPort);
    SetPort(mywin);
   
    ph=GetGWorldPixMap((CGrafPort *) mywin);
    LockPixels(ph);
    winbaseaddr=( unsigned char *) GetPixBaseAddr(ph);
    winrowbytes=(*ph)->rowBytes & 0x3FFF;
    winbaseaddr-=((**ph).bounds.left);
    winbaseaddr-=((**ph).bounds.top*winrowbytes);
    dest=(double *)winbaseaddr;
    src=(double *)obuff;
    
    for (y=1; y< 480; y++)
    {
	for (x=0;x < 160;x++)
	{  
	    *dest++ = *src++;
	}
	dest=(double *)(winbaseaddr+(y*winrowbytes));
    }
    UnlockPixels(ph);
    SetPort(oldPort);

}

void prepare_line (int y, bool need_double)
{
    next_line_double = need_double;
    next_line_pos = y;
}

void flush_line()
{
    short i;
    char *dst,*src;
    double *dst1,*src1;
	 
    if ((next_line_pos < 508) && (next_line_pos > 28))
    {	
	dst=(char *)obuff+((next_line_pos-28)*1280);
	src=(char *)xlinebuffer+292;
	BlockMove(src,dst,1280);  
    }
    if (next_line_double)
    {	
	if ((next_line_pos+1 < 508) && (next_line_pos+1 > 28))
	{	
	    dst1=(double *)obuff+((next_line_pos+1-28)*160);
	    src1=(double *)obuff+((next_line_pos-28)*160);
	    for (i=0;i < 160;i++)
	    {  
		*dst1++=*src1++;
	    }
	}
    }
}

/* Decode KeySyms. This function knows about all keys that are common 
 * between different keyboard languages.
 */
static int kc_decode (long ks)
{	
    switch (ks)
    {
     case kAKeyMap: return AK_A;
     case kBKeyMap: return AK_B;
     case kCKeyMap: return AK_C;
     case kDKeyMap: return AK_D;
     case kEKeyMap: return AK_E;
     case kFKeyMap: return AK_F;
     case kGKeyMap: return AK_G;
     case kHKeyMap: return AK_H;
     case kIKeyMap: return AK_I;
     case kJKeyMap: return AK_J;
     case kKKeyMap: return AK_K;
     case kLKeyMap: return AK_L;
     case kMKeyMap: return AK_M;
     case kNKeyMap: return AK_N;
     case kOKeyMap: return AK_O;
     case kPKeyMap: return AK_P;
     case kQKeyMap: return AK_Q;
     case kRKeyMap: return AK_R;
     case kSKeyMap: return AK_S;
     case kTKeyMap: return AK_T;
     case kUKeyMap: return AK_U;
     case kVKeyMap: return AK_V;
     case kWKeyMap: return AK_W;
     case kXKeyMap: return AK_X;
     
     case k0KeyMap: return AK_0;
     case k1KeyMap: return AK_1;
     case k2KeyMap: return AK_2;
     case k3KeyMap: return AK_3;
     case k4KeyMap: return AK_4;
     case k5KeyMap: return AK_5;
     case k6KeyMap: return AK_6;
     case k7KeyMap: return AK_7;
     case k8KeyMap: return AK_8;
     case k9KeyMap: return AK_9;
     
     case kKP0KeyMap: return AK_NP0;
     case kKP1KeyMap: return AK_NP1;
     case kKP2KeyMap: return AK_NP2;
     case kKP3KeyMap: return AK_NP3;
     case kKP4KeyMap: return AK_NP4;
     case kKP5KeyMap: return AK_NP5;
     case kKP6KeyMap: return AK_NP6;
     case kKP7KeyMap: return AK_NP7;
     case kKP8KeyMap: return AK_NP8;
     case kKP9KeyMap: return AK_NP9;
	
     case kF1KeyMap: return AK_F1;
     case kF2KeyMap: return AK_F2;
     case kF3KeyMap: return AK_F3;
     case kF4KeyMap: return AK_F4;
     case kF5KeyMap: return AK_F5;
     case kF6KeyMap: return AK_F6;
     case kF7KeyMap: return AK_F7;
     case kF8KeyMap: return AK_F8;
     case kF9KeyMap: return AK_F9;
     case kF10KeyMap: return AK_F10;
	
     case kBackSpaceKeyMap: return AK_BS;
     case kTabKeyMap: return AK_TAB;
     case kReturnKeyMap: return AK_RET;
     case kEscapeKeyMap: return AK_ESC;
     
     case kSpaceBarMap: if (Joy) return -1; else return AK_SPC;
     
     case kUpArrowKeyMap: if (Joy) return -1; else return AK_UP;
     case kDownArrowKeyMap: if (Joy) return -1; else return AK_DN;
     case kLeftArrowKeyMap: if (Joy) return -1; else return AK_LF;
     case kRightArrowKeyMap: if (Joy) return -1; else return AK_RT;
	
     case kF11KeyMap: graphics_leave();
     case kF12KeyMap: { Joy=!Joy; SysBeep(0); return -1; }

     case kPgUpKeyMap: return AK_RAMI;
     case kPgDnKeyMap: return AK_LAMI;
     case kBackSlash: return AK_BACKSLASH;
    }
    return -1;
}

static int decode_us(long ks)
{
    switch(ks) {
	/* US specific */

     case kYKeyMap: return AK_Y;
     case kZKeyMap: return AK_Z;
     case kLBracketKeyMap: return AK_LBRACKET;
     case kRBracketKeyMap: return AK_RBRACKET;
     case kCommaKeyMap: return AK_COMMA;
     case kPeriodKeyMap: return AK_PERIOD;
     case kSlashKeyMap: return AK_SLASH;
     case kSemiColonKeyMap: return AK_SEMICOLON;
     case kMinusKeyMap: return AK_MINUS;
     case kEqualKeyMap: return AK_EQUAL;
     case kQuoteKeyMap: return AK_QUOTE;
    }

    return -1;
}

static int decode_de(long ks)
{
    switch(ks) {
/* DE specific
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
*/
    }

    return -1;
}

static int keycode2amiga(long code)
{
    long ks;
    int as;
    
    ks = (code & keyCodeMask) >> 8;
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
    return -1;
}

void handle_events()
{
    Boolean repeat;
    Boolean itHappened;
    Point   mpos;
    EventRecord event;
    int kc,i,count;
	
    SetEventMask(-1);
    SelectWindow(mywin);
    HideCursor();
	
    GetKeys(keys);
    if (BitTst(&keys, kCommandRawKey))
	RM=true;
    else
	RM=false;
    
    if (BitTst(&keys, kShiftRawKey))
    {	
	if (!keystate[AK_LSH]) {
	    keystate[AK_LSH] = true;
	    record_key (AK_LSH << 1);
	    goto label1;
	}
    } else {
	if (keystate[AK_LSH]) {
	    keystate[AK_LSH] = false;
	    record_key ((AK_LSH << 1) | 1);
	    goto label1;
	}
    }
    if (BitTst(&keys, kControlRawKey))
    {	
	if (!keystate[AK_CTRL]) {
	    keystate[AK_CTRL] = true;
	    record_key (AK_CTRL << 1);
	    goto label1;
	}
    } else {	
	if (keystate[AK_CTRL]) {
	    keystate[AK_CTRL] = false;
	    record_key ((AK_CTRL << 1) | 1);
	    goto label1;
	}
    }
    if (BitTst(&keys, kOptionRawKey))
    {	
	if (!keystate[AK_LALT]) {
	    keystate[AK_LALT] = true;
	    record_key (AK_LALT << 1);
	    goto label1;
	}
    } else {
	if (keystate[AK_LALT]) {
	    keystate[AK_LALT] = false;
	    record_key ((AK_LALT << 1) | 1);
	    goto label1;
	}
    }
    do {
	repeat = false;
	newmousecounters = false;
	itHappened=WaitNextEvent(-1,&event,0L,0L);

	switch(event.what) {
	 case keyDown:
	 case autoKey: {	
	     int kc = keycode2amiga(event.message);
	     if (kc == -1) break;
	     if (RM == true && kc == AK_Q) graphics_leave();
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
	 case keyUp: {	     
	     if (Joy) break;
	     kc = keycode2amiga(event.message);
	     if (kc == -1) break;
	     keystate[kc] = false;
	     record_key ((kc << 1) | 1);
	     break;
	 }
	 case mouseDown:
	    if (RM == true) buttonstate[2] = true;
	    else buttonstate[0] = true;
	    break;
	 case mouseUp:
	    buttonstate[0] = false;
	    buttonstate[2] = false;
	    break;
	}
	GetMouse(&mpos);
	if (mpos.h != lastmx) { lastmx=mpos.h; repeat = true; }
	if (mpos.v != lastmy) { lastmy=mpos.v; repeat = true; }
    } while (repeat);
    
label1:
    /* "Affengriff" */
    if(keystate[AK_CTRL] && keystate[AK_LAMI] && keystate[AK_RAMI])
    	MC68000_reset();
}

bool debuggable()
{
    return true;
}

bool needmousehack()
{
    return true;
}

void LED(int on)
{
}

void parse_cmdline ()
{
    /* No commandline on the Mac. Implemented on Menus soon!*/
}

// Check Minimal System Configuration and Setup
static Boolean CheckForSetup (void)
{	Boolean			retvalue=FALSE;
	SysEnvRec		env;
	
	SysEnvirons( 2, &env );
	
	if ( env.systemVersion < 0x0700 )
	{	ParamAString("\pUAE requires System 7 or later!");
		DisplayError(kQuitError);
		retvalue=TRUE;
	}
	if ( !env.hasColorQD)
	{	ParamAString("\pUAE requires Color QuickDraw!");
		DisplayError(kQuitError);
		retvalue=TRUE;
	}
	if ( env.processor < 3)
	{	ParamAString("\pUAE requires a 68020 or better processor to run!");
		DisplayError(kQuitError);
		retvalue=TRUE;
	}
	curDevice = GetMainDevice();
	if ((*curDevice)->gdPMap == NULL || (*(*curDevice)->gdPMap)->pixelSize != 16)
	{	if (HasDepth(curDevice,16,0,0) == 0)
		{	ParamAString("\pUAE requires 16 bit color and your display doesn't support it!");
			DisplayError(kQuitError);
			retvalue=TRUE;
		}
		else
		{
		oldDepth=(*(*curDevice)->gdPMap)->pixelSize;
		SetDepth(curDevice,16,0,0);
		}
	}
	return(retvalue);
}

// Shows up the standard error alert;
int DisplayError(int ID)
{	int	ret=0;

	InitCursor();
	ret=Alert(ID,0);

	return (ret);
}

// Parses a Pascal string for error display
void ParamAString( ConstStr255Param theStr )
{
	ParamText(theStr, "\p", "\p", "\p");
}
