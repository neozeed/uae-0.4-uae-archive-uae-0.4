 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Custom chip emulation
  * 
  * (c) 1995 Bernd Schmidt, Alessandro Bissacco
  */

#include <ctype.h>
#ifdef __unix
#include <sys/time.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "cia.h"
#include "disk.h"
#include "blit.h"
#include "xwin.h"
#include "os.h"

#define DMA_AUD0      0x0001
#define DMA_AUD1      0x0002
#define DMA_AUD2      0x0004
#define DMA_AUD3      0x0008
#define DMA_DISK      0x0010
#define DMA_SPRITE    0x0020
#define DMA_BLITTER   0x0040
#define DMA_COPPER    0x0080
#define DMA_BITPLANE  0x0100
#define DMA_BLITPRI   0x0400

/* These are default values for mouse emulation.
 * The first two are default values for mstepx and mstepy.
 * The second line set the orizontal and vertical offset for amiga and X 
 * pointer matching
 */
   
#define defstepx (1<<16)
#define defstepy (1<<16)
#ifndef __mac__
#define defxoffs 0
#define defyoffs 0
#else
#define defxoffs 210
#define defyoffs 85
#endif

/* Values below define mouse auto calibration process.
 * They are not critical, change them if you want.
 * The most important is calweight, which sets mouse adjustement rate */ 

static const int docal = 60, xcaloff = 40, ycaloff = 20;
static const int calweight = 3;

static int lastsampledmx, lastsampledmy;

 /*
  * Events
  */

unsigned long int cycles, nextevent, nextev_count, specialflags;
int vpos;
UWORD lof;

struct ev eventtab[ev_max];

bool copper_active;

static const int maxhpos = 227,maxvpos = 312;
static const int dskdelay = 2; /* FIXME: ??? */
static const int minfirstline = 29;

 /* 
  * hardware register values that are visible/can be written to by someone
  */

static UWORD cregs[256];

static UWORD dmacon,intena,intreq,adkcon;

static ULONG cop1lc,cop2lc,copcon;

static CPTR  bpl1pt,bpl2pt,bpl3pt,bpl4pt,bpl5pt,bpl6pt;
static ULONG bpl1dat,bpl2dat,bpl3dat,bpl4dat,bpl5dat,bpl6dat;
static UWORD bplcon0,bplcon1,bplcon2,bplcon3;
static UWORD diwstrt,diwstop,ddfstrt,ddfstop;
static WORD  bpl1mod,bpl2mod;

static UWORD sprdata[8], sprdatb[8], sprctl[8], sprpos[8];
static ULONG sprpt[8];
static bool sprarmed[8], sprptset[8];
/* Kludge. FIXME: How does sprite restart after vsync work? */
static int spron[8]; 

static UWORD bltadat,bltbdat,bltcdat,bltddat,bltafwm,bltalwm,bltsize;
static WORD  bltamod,bltbmod,bltcmod,bltdmod;
static UWORD bltcon0,bltcon1;
static ULONG bltapt,bltbpt,bltcpt,bltdpt,bltcnxlpt,bltdnxlpt;

static ULONG dskpt;
static UWORD dsklen,dsksync;

static int joy0x, joy1x, joy0y, joy1y;
static int lastspr0x,lastspr0y,lastdiffx,lastdiffy,spr0pos,spr0ctl;
static int mstepx,mstepy,xoffs=defxoffs,yoffs=defyoffs;
static int sprvbfl;

static enum { normal_mouse, dont_care_mouse, follow_mouse} mousestate;

xcolnr acolors[64];
UWORD color_regs[32];

 /*
  * "hidden" hardware registers
  */

static int dblpf_ind1[64], dblpf_ind2[64];

static ULONG coplc;
static UWORD copi1,copi2;

static enum {
    COP_stop, COP_read, COP_wait, COP_move, COP_skip
} copstate;

static UWORD vblitsize,hblitsize,blitpreva,blitprevb;
static UWORD blitlpos,blitashift,blitbshift,blinea,blineb;
static bool blitline,blitfc,blitzero,blitfill,blitife,blitdesc,blitsing;
static bool blitonedot,blitsign;
static long int bltwait;

static int dsklength;

static enum {
    BLT_done, BLT_init, BLT_read, BLT_work, BLT_write, BLT_next
} bltstate;

static int plffirstline,plflastline,plfstrt,plfstop,plflinelen;
static int diwfirstword,diwlastword;
static int plf1pri, plf2pri;

int dskdmaen; /* used in cia.c */
static UWORD dskmfm,dskbyte,dsktime;
static bool dsksynced;

static int bpldelay1;
static int bpldelay2;
static bool bplhires;
static int bplplanecnt;

static bool pfield_fullline,pfield_linedone;
static bool pfield_linedmaon;
static int pfield_lastpart_hpos;

unsigned char apixels[1000]; /* includes a lot of safety padding */
char spixels[1000]; /* for sprites */
char spixstate[1000]; /* more sprites */

 /*
  * Statistics
  */

static unsigned long int msecs = 0, frametime = 0, timeframes = 0;
static unsigned long int seconds_base;
bool bogusframe;

 /*
  * helper functions
  */

static void pfield_doline_slow(int);
static void pfield_doline(void);
static void pfield_hsync(int);

bool inhibit_frame;
static int framecnt = 0;

static __inline__ void count_frame(void)
{
    if (inhibit_frame) 
    	framecnt = 1;
    else {	
	framecnt++;
	if (framecnt == framerate)
    	    framecnt = 0;
    }
}

static __inline__ void setclr(UWORD *p, UWORD val)
{
    if (val & 0x8000) {
	*p |= val & 0x7FFF;
    } else {
	*p &= ~val;
    }
}

bool dmaen(UWORD dmamask)
{
    return (dmamask & dmacon) && (dmacon & 0x200);
}

static __inline__ int current_hpos(void)
{
    return cycles - eventtab[ev_hsync].oldcycles;
}

static void calcdiw(void)
{
    diwfirstword = (diwstrt & 0xFF) * 2 - 0x60;
    diwlastword  = (diwstop & 0xFF) * 2 + 0x200 - 0x60;
    if (diwfirstword < 0) diwfirstword = 0;

    plffirstline = diwstrt >> 8;
    plflastline = diwstop >> 8;
#if 0
    /* This happens far too often. */
    if (plffirstline < minfirstline) {
	fprintf(stderr, "Warning: Playfield begins before line %d!\n", minfirstline);
	plffirstline = minfirstline;
    }
#endif
    if ((plflastline & 0x80) == 0) plflastline |= 0x100;
#if 0 /* Turrican does this */
    if (plflastline > 313) {
	fprintf(stderr, "Warning: Playfield out of range!\n");
	plflastline = 313;
    }
#endif
    plfstrt = ddfstrt;
    plfstop = ddfstop;
    if (plfstrt < 0x18) plfstrt = 0x18;
    if (plfstop > 0xD8) plfstop = 0xD8;
    if (plfstrt > plfstop) plfstrt = plfstop;
    
    /* 
     * Prize question: What are the next lines supposed to be?
     * I can't seem to get it right.
     */
#if 0
    /* Pretty good guess, but wrong. */
    plflinelen = (plfstop-plfstrt+15) & ~7;
    plfstrt &= ~(bplhires ? 3 : 7);
    plfstop &= ~(bplhires ? 3 : 7);
#endif
    plfstrt &= ~(bplhires ? 3 : 3);
    plfstop &= ~(bplhires ? 3 : 3);
    plflinelen = (plfstop-plfstrt+15) & ~7;
}

static void pfield_may_need_update(bool colreg)
{
    int i;
    
    /* Ignore, if this happened before or after the DDF window */
    if (framecnt != 0 || !pfield_linedmaon || current_hpos() <= plfstrt
	|| vpos < plffirstline || vpos >= plflastline)
    {	
    	return;
    }
    /* 
     * If a color reg was modified, it is only important if we are within
     * the DIW.
     */
    if (current_hpos() <= (diwfirstword+0x60)/4 && colreg)
    	return;

    /*
     * If we are past the DDF window, me might as well draw the complete
     * line now.
     */
    if (current_hpos() > plfstrt + plflinelen && pfield_fullline) {
	if (!pfield_linedone)
	    pfield_doline();
	pfield_linedone = true;
	return;
    }
    	
    if (pfield_fullline) {
	pfield_lastpart_hpos = 0;
	memset(apixels, 0, sizeof(apixels));
	pfield_fullline = false;
    } else {	
	assert(pfield_lastpart_hpos <= current_hpos());
    }
    for (i = pfield_lastpart_hpos; i < current_hpos(); i++) {
	pfield_doline_slow(i);
    }
    pfield_lastpart_hpos = current_hpos();
}

/* Apparently, the DMA bit is tested by the hardware at some point,
 * presumably at the ddfstart position, to determine whether it
 * ought to draw the line.
 * This is probably not completely correct, but should not matter
 * very much.
 */
static void pfield_calclinedma(void)
{
    if (current_hpos() >= plfstrt)
    	return;
    
    pfield_linedmaon = dmaen(DMA_BITPLANE);
}

 /* 
  * register functions
  */

static UWORD DMACONR(void)
{
    return (dmacon | (bltstate==BLT_done ? 0 : 0x4000)
	    | (blitzero ? 0x2000 : 0));
}
static UWORD INTENAR(void) { return intena; }
static UWORD INTREQR(void) { return intreq; }
static UWORD ADKCONR(void) { return adkcon; }
static UWORD VPOSR(void) { return (vpos >> 8) | lof; }
static void  VPOSW(UWORD v)  { lof = v & 0x8000; }
static UWORD VHPOSR(void) { return (vpos << 8) | current_hpos(); } 

static void  COP1LCH(UWORD v) { cop1lc= (cop1lc & 0xffff) | ((ULONG)v << 16); }
static void  COP1LCL(UWORD v) { cop1lc= (cop1lc & ~0xffff) | v; }
static void  COP2LCH(UWORD v) { cop2lc= (cop2lc & 0xffff) | ((ULONG)v << 16); }
static void  COP2LCL(UWORD v) { cop2lc= (cop2lc & ~0xffff) | v; }

static void  COPJMP1(UWORD a)
{
    coplc = cop1lc; copstate = COP_read; 
    eventtab[ev_copper].active = 1; eventtab[ev_copper].oldcycles = cycles;
    eventtab[ev_copper].evtime = 4; events_schedule();
    copper_active = true;
}
static void  COPJMP2(UWORD a)
{
    coplc = cop2lc; copstate = COP_read; 
    eventtab[ev_copper].active = 1; eventtab[ev_copper].oldcycles = cycles;
    eventtab[ev_copper].evtime = 4; events_schedule();
    copper_active = true;
}

static void  DMACON(UWORD v) 
{
    UWORD oldcon = dmacon;
    setclr(&dmacon,v); dmacon &= 0x1FFF;
    pfield_calclinedma();
    
    if ((dmacon & DMA_COPPER) > (oldcon & DMA_COPPER)) { 
	COPJMP1(0); /* @@@ not sure how this works */
    }
    if (copper_active && !eventtab[ev_copper].active) {
	eventtab[ev_copper].active = true;
	eventtab[ev_copper].oldcycles = cycles;
	eventtab[ev_copper].evtime = 1;
	events_schedule();
    }
}
static void  INTENA(UWORD v) { setclr(&intena,v); specialflags |= SPCFLAG_INT; }
static void  INTREQ(UWORD v) { setclr(&intreq,v); specialflags |= SPCFLAG_INT; }
static void  ADKCON(UWORD v) { setclr(&adkcon,v); }

static void  BPL1PTH(UWORD v) { bpl1pt= (bpl1pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL1PTL(UWORD v) { bpl1pt= (bpl1pt & ~0xffff) | v; }
static void  BPL2PTH(UWORD v) { bpl2pt= (bpl2pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL2PTL(UWORD v) { bpl2pt= (bpl2pt & ~0xffff) | v; }
static void  BPL3PTH(UWORD v) { bpl3pt= (bpl3pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL3PTL(UWORD v) { bpl3pt= (bpl3pt & ~0xffff) | v; }
static void  BPL4PTH(UWORD v) { bpl4pt= (bpl4pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL4PTL(UWORD v) { bpl4pt= (bpl4pt & ~0xffff) | v; }
static void  BPL5PTH(UWORD v) { bpl5pt= (bpl5pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL5PTL(UWORD v) { bpl5pt= (bpl5pt & ~0xffff) | v; }
static void  BPL6PTH(UWORD v) { bpl6pt= (bpl6pt & 0xffff) | ((ULONG)v << 16); }
static void  BPL6PTL(UWORD v) { bpl6pt= (bpl6pt & ~0xffff) | v; }

/*
 * I've seen the listing of an example program that changes 
 * from lo- to hires while a line is being drawn. That's
 * awful, but we want to emulate it.
 */
static void  BPLCON0(UWORD v) 
{
    pfield_may_need_update(false);
    bplcon0 = v;
    bplhires = (bplcon0 & 0x8000) == 0x8000; 
    bplplanecnt = (bplcon0 & 0x7000) >> 12; 
    calcdiw(); /* This should go away. */
}
static void  BPLCON1(UWORD v) 
{
    pfield_may_need_update(false);
    bplcon1 = v; 
    bpldelay1 = bplcon1 & 0xF; 
    bpldelay2 = (bplcon1 >> 4) & 0xF; 
}
static void  BPLCON2(UWORD v) 
{
    pfield_may_need_update(false); 
    bplcon2 = v;
    plf1pri = 1 << 2*(v & 7);
    plf2pri = 1 << 2*((v>>3) & 7);
}
static void  BPLCON3(UWORD v) { pfield_may_need_update(false); bplcon3 = v; }

static void  BPL1MOD(UWORD v) { pfield_may_need_update(false); bpl1mod = v; }
static void  BPL2MOD(UWORD v) { pfield_may_need_update(false); bpl2mod = v; }

static void  BPL1DAT(UWORD v) { bpl1dat = v; }
static void  BPL2DAT(UWORD v) { bpl2dat = v; }
static void  BPL3DAT(UWORD v) { bpl3dat = v; }
static void  BPL4DAT(UWORD v) { bpl4dat = v; }
static void  BPL5DAT(UWORD v) { bpl5dat = v; }
static void  BPL6DAT(UWORD v) { bpl6dat = v; }

/* We call pfield_may_need_update() from here. Actually, 
 * I have no idea what happens if someone changes ddf or
 * diw mid-line, and I don't really want to know. I doubt
 * that this sort of thing was ever used to create a
 * useful effect.
 */
static void  DIWSTRT(UWORD v) { pfield_may_need_update(false); diwstrt = v; calcdiw(); }
static void  DIWSTOP(UWORD v) { pfield_may_need_update(false); diwstop = v; calcdiw(); }
static void  DDFSTRT(UWORD v) { pfield_may_need_update(false); ddfstrt = v; calcdiw(); }
static void  DDFSTOP(UWORD v) { pfield_may_need_update(false); ddfstop = v; calcdiw(); }

static void  BLTADAT(UWORD v) { bltadat = v; }
static void  BLTBDAT(UWORD v) { bltbdat = v; }
static void  BLTCDAT(UWORD v) { bltcdat = v; }

static void  BLTAMOD(UWORD v) { bltamod = v; }
static void  BLTBMOD(UWORD v) { bltbmod = v; }
static void  BLTCMOD(UWORD v) { bltcmod = v; }
static void  BLTDMOD(UWORD v) { bltdmod = v; }

static void  BLTCON0(UWORD v) { bltcon0 = v; }
static void  BLTCON1(UWORD v) { bltcon1 = v; }

static void  BLTAFWM(UWORD v) { bltafwm = v; }
static void  BLTALWM(UWORD v) { bltalwm = v; }

static void  BLTAPTH(UWORD v) { bltapt= (bltapt & 0xffff) | ((ULONG)v << 16); }
static void  BLTAPTL(UWORD v) { bltapt= (bltapt & ~0xffff) | (v); }
static void  BLTBPTH(UWORD v) { bltbpt= (bltbpt & 0xffff) | ((ULONG)v << 16); }
static void  BLTBPTL(UWORD v) { bltbpt= (bltbpt & ~0xffff) | (v); }
static void  BLTCPTH(UWORD v) { bltcpt= (bltcpt & 0xffff) | ((ULONG)v << 16); }
static void  BLTCPTL(UWORD v) { bltcpt= (bltcpt & ~0xffff) | (v); }
static void  BLTDPTH(UWORD v) { bltdpt= (bltdpt & 0xffff) | ((ULONG)v << 16); }
static void  BLTDPTL(UWORD v) { bltdpt= (bltdpt & ~0xffff) | (v); }
static void  BLTSIZE(UWORD v) { bltsize = v; bltstate = BLT_init; specialflags |= SPCFLAG_BLIT; }

static void  SPRxCTL(UWORD v, int num) { pfield_may_need_update(false); sprctl[num] = v; sprarmed[num] = false; spron[num] |= 2; }
static void  SPRxPOS(UWORD v, int num) { pfield_may_need_update(false); sprpos[num] = v; }
static void  SPRxDATA(UWORD v, int num) { pfield_may_need_update(false); sprdata[num] = v; sprarmed[num] = true; }
static void  SPRxDATB(UWORD v, int num) { pfield_may_need_update(false); sprdatb[num] = v; }
static void  SPRxPTH(UWORD v, int num) { sprpt[num] &= 0xffff; sprpt[num] |= (ULONG)v << 16; spron[num] |= 1; }
static void  SPRxPTL(UWORD v, int num) { sprpt[num] &= ~0xffff; sprpt[num] |= v; spron[num] |= 1; }

static void  COLOR(UWORD v, int num)
{
    pfield_may_need_update(true);
    color_regs[num] = v;
    acolors[num] = xcolors[v];
    acolors[num+32] = xcolors[(v >> 1) & 0x777];
}

static void  DSKSYNC(UWORD v) { dsksync = v; }
static void  DSKDAT(UWORD v) { dskmfm = v; }
static void  DSKPTH(UWORD v) { dskpt = (dskpt & 0xffff) | ((ULONG)v << 16); }
static void  DSKPTL(UWORD v) { dskpt = (dskpt & ~0xffff) | (v); }

static void  DSKLEN(UWORD v) 
{
    if (v & 0x8000) { dskdmaen++; } else { dskdmaen = 0; }
    dsktime = dskdelay; dsksynced = false;
    dsklen = dsklength = v; dsklength &= 0x3fff;
    if (dskdmaen == 2 && dsksync != 0x4489 && (adkcon & 0x400)) {
	printf("Non-standard sync: %04x len: %x\n", dsksync, dsklength);
    }
    if (dsklen & 0x4000) DISK_InitWrite();
    if (dskdmaen) specialflags |= SPCFLAG_DISK;
}

static UWORD DSKBYTR(void)
{
    UWORD v = (dsklen >> 1) & 0x6000;
    v |= dskbyte;
    dskbyte &= ~0x8000;
    if (dsksync == dskmfm) v |= 0x1000;
    return v;
}

static UWORD DSKDATR(void) { return dskmfm; }
static UWORD POTGOR(void)
{
    if (!buttonstate[2]) {	
	return 0xffff;
    } else {
	return 0xbbff;
    }
}

static UWORD JOY0DAT(void) { return joy0x + (joy0y << 8); }
static UWORD JOY1DAT(void)
{
#ifdef HAVE_JOYSTICK
    UWORD dir;
    bool button;
    read_joystick(&dir, &button);
    buttonstate[1] = button;
    return dir;
#else
    return joy1x + (joy1y << 8);
#endif
}
static void JOYTEST(UWORD v)
{
    joy0x = joy1x = v & 0xFC;
    joy0y = joy1y = (v >> 8) & 0xFC;    
}
static void AUD0LCH(UWORD v) { audlc[0] = (audlc[0] & 0xffff) | ((ULONG)v << 16); }
static void AUD0LCL(UWORD v) { audlc[0] = (audlc[0] & ~0xffff) | v; }
static void AUD1LCH(UWORD v) { audlc[1] = (audlc[1] & 0xffff) | ((ULONG)v << 16); }
static void AUD1LCL(UWORD v) { audlc[1] = (audlc[1] & ~0xffff) | v; }
static void AUD2LCH(UWORD v) { audlc[2] = (audlc[2] & 0xffff) | ((ULONG)v << 16); }
static void AUD2LCL(UWORD v) { audlc[2] = (audlc[2] & ~0xffff) | v; }
static void AUD3LCH(UWORD v) { audlc[3] = (audlc[3] & 0xffff) | ((ULONG)v << 16); }
static void AUD3LCL(UWORD v) { audlc[3] = (audlc[3] & ~0xffff) | v; }
static void AUD0PER(UWORD v) { audper[0] = v; }
static void AUD1PER(UWORD v) { audper[1] = v; }
static void AUD2PER(UWORD v) { audper[2] = v; }
static void AUD3PER(UWORD v) { audper[3] = v; }
static void AUD0VOL(UWORD v) { audvol[0] = v; }
static void AUD1VOL(UWORD v) { audvol[1] = v; }
static void AUD2VOL(UWORD v) { audvol[2] = v; }
static void AUD3VOL(UWORD v) { audvol[3] = v; }
static void AUD0LEN(UWORD v) { audlen[0] = v; }
static void AUD1LEN(UWORD v) { audlen[1] = v; }
static void AUD2LEN(UWORD v) { audlen[2] = v; }
static void AUD3LEN(UWORD v) { audlen[3] = v; }

/* Custom chip memory bank */

static ULONG custom_lget(CPTR);
static UWORD custom_wget(CPTR);
static UBYTE custom_bget(CPTR);
static void  custom_lput(CPTR, ULONG);
static void  custom_wput(CPTR, UWORD);
static void  custom_bput(CPTR, UBYTE);

addrbank custom_bank = {
    custom_lget, custom_wget, custom_bget,
    custom_lput, custom_wput, custom_bput,
    default_xlate, default_check
};

UWORD custom_wget(CPTR addr)
{
#ifdef DUALCPU
    customacc = true;
#endif
    switch(addr & 0x1FE) {
     case 0x002: return DMACONR();
     case 0x004: return VPOSR();
     case 0x006: return VHPOSR();
	
     case 0x008: return DSKDATR();
     case 0x016: return POTGOR();
     case 0x01A: return DSKBYTR();
     case 0x01C: return INTENAR();
     case 0x01E: return INTREQR();
     case 0x010: return ADKCONR();
     case 0x00A: return JOY0DAT();
     case 0x00C: return JOY1DAT();
     default:
	custom_wput(addr,0);
	return 0xffff;
    }
}

UBYTE custom_bget(CPTR addr)
{
    return custom_wget(addr & 0xfffe) >> (addr & 1 ? 0 : 8);
}

ULONG custom_lget(CPTR addr)
{
    return ((ULONG)custom_wget(addr & 0xfffe) << 16) | custom_wget((addr+2) & 0xfffe);
}

void custom_wput(CPTR addr, UWORD value)
{
#ifdef DUALCPU
    customacc = true;
#endif
    addr &= 0x1FE;
    cregs[addr>>1] = value;
    switch(addr) {	
     case 0x020: DSKPTH(value); break;
     case 0x022: DSKPTL(value); break;
     case 0x024: DSKLEN(value); break;
     case 0x026: DSKDAT(value); break;
	
     case 0x02A: VPOSW(value); break;
	
     case 0x040: BLTCON0(value); break;
     case 0x042: BLTCON1(value); break;
	
     case 0x044: BLTAFWM(value); break;
     case 0x046: BLTALWM(value); break;
	
     case 0x050: BLTAPTH(value); break;
     case 0x052: BLTAPTL(value); break;
     case 0x04C: BLTBPTH(value); break;
     case 0x04E: BLTBPTL(value); break;
     case 0x048: BLTCPTH(value); break;
     case 0x04A: BLTCPTL(value); break;
     case 0x054: BLTDPTH(value); break;
     case 0x056: BLTDPTL(value); break;
	
     case 0x058: BLTSIZE(value); break;
	
     case 0x064: BLTAMOD(value); break;
     case 0x062: BLTBMOD(value); break;
     case 0x060: BLTCMOD(value); break;
     case 0x066: BLTDMOD(value); break;
	
     case 0x070: BLTCDAT(value); break;
     case 0x072: BLTBDAT(value); break;
     case 0x074: BLTADAT(value); break;
			
     case 0x07E: DSKSYNC(value); break;

     case 0x080: COP1LCH(value); break;
     case 0x082: COP1LCL(value); break;
     case 0x084: COP2LCH(value); break;
     case 0x086: COP2LCL(value); break;
	
     case 0x088: COPJMP1(value); break;
     case 0x08A: COPJMP2(value); break;
	
     case 0x08E: DIWSTRT(value); break;
     case 0x090: DIWSTOP(value); break;
     case 0x092: DDFSTRT(value); break;
     case 0x094: DDFSTOP(value); break;
	
     case 0x096: DMACON(value); break;
     case 0x09A: INTENA(value); break;
     case 0x09C: INTREQ(value); break;
     case 0x09E: ADKCON(value); break;
	
     case 0x0A0: AUD0LCH(value); break;
     case 0x0A2: AUD0LCL(value); break;
     case 0x0A4: AUD0LEN(value); break;
     case 0x0A6: AUD0PER(value); break;
     case 0x0A8: AUD0VOL(value); break;
	
     case 0x0B0: AUD1LCH(value); break;
     case 0x0B2: AUD1LCL(value); break;
     case 0x0B4: AUD1LEN(value); break;
     case 0x0B6: AUD1PER(value); break;
     case 0x0B8: AUD1VOL(value); break;
	
     case 0x0C0: AUD2LCH(value); break;
     case 0x0C2: AUD2LCL(value); break;
     case 0x0C4: AUD2LEN(value); break;
     case 0x0C6: AUD2PER(value); break;
     case 0x0C8: AUD2VOL(value); break;
	
     case 0x0D0: AUD3LCH(value); break;
     case 0x0D2: AUD3LCL(value); break;
     case 0x0D4: AUD3LEN(value); break;
     case 0x0D6: AUD3PER(value); break;
     case 0x0D8: AUD3VOL(value); break;
	
     case 0x0E0: BPL1PTH(value); break;
     case 0x0E2: BPL1PTL(value); break;
     case 0x0E4: BPL2PTH(value); break;
     case 0x0E6: BPL2PTL(value); break;
     case 0x0E8: BPL3PTH(value); break;
     case 0x0EA: BPL3PTL(value); break;
     case 0x0EC: BPL4PTH(value); break;
     case 0x0EE: BPL4PTL(value); break;
     case 0x0F0: BPL5PTH(value); break;
     case 0x0F2: BPL5PTL(value); break;
     case 0x0F4: BPL6PTH(value); break;
     case 0x0F6: BPL6PTL(value); break;
	
     case 0x100: BPLCON0(value); break;
     case 0x102: BPLCON1(value); break;
     case 0x104: BPLCON2(value); break;
     case 0x106: BPLCON3(value); break;
	
     case 0x108: BPL1MOD(value); break;
     case 0x10A: BPL2MOD(value); break;

     case 0x110: BPL1DAT(value); break;
     case 0x112: BPL2DAT(value); break;
     case 0x114: BPL3DAT(value); break;
     case 0x116: BPL4DAT(value); break;
     case 0x118: BPL5DAT(value); break;
     case 0x11A: BPL6DAT(value); break;
	
     case 0x180: case 0x182: case 0x184: case 0x186: case 0x188: case 0x18A:
     case 0x18C: case 0x18E: case 0x190: case 0x192: case 0x194: case 0x196:
     case 0x198: case 0x19A: case 0x19C: case 0x19E: case 0x1A0: case 0x1A2:
     case 0x1A4: case 0x1A6: case 0x1A8: case 0x1AA: case 0x1AC: case 0x1AE:
     case 0x1B0: case 0x1B2: case 0x1B4: case 0x1B6: case 0x1B8: case 0x1BA:
     case 0x1BC: case 0x1BE: 
	COLOR(value & 0xFFF, (addr & 0x3E) / 2);
	break;	
     case 0x120: case 0x124: case 0x128: case 0x12C: 
     case 0x130: case 0x134: case 0x138: case 0x13C:
	SPRxPTH(value, (addr - 0x120) / 4);
	break;
     case 0x122: case 0x126: case 0x12A: case 0x12E: 
     case 0x132: case 0x136: case 0x13A: case 0x13E:
	SPRxPTL(value, (addr - 0x122) / 4);
	break;
     case 0x140: case 0x148: case 0x150: case 0x158: 
     case 0x160: case 0x168: case 0x170: case 0x178:
	SPRxPOS(value, (addr - 0x140) / 8);
	break;
     case 0x142: case 0x14A: case 0x152: case 0x15A: 
     case 0x162: case 0x16A: case 0x172: case 0x17A:
	SPRxCTL(value, (addr - 0x142) / 8);
	break;
     case 0x144: case 0x14C: case 0x154: case 0x15C:
     case 0x164: case 0x16C: case 0x174: case 0x17C:
	SPRxDATA(value, (addr - 0x144) / 8);
	break;
     case 0x146: case 0x14E: case 0x156: case 0x15E: 
     case 0x166: case 0x16E: case 0x176: case 0x17E:
	SPRxDATB(value, (addr - 0x146) / 8);
	break;
	
     case 0x36: JOYTEST(value); break;	
    }
}

void custom_bput(CPTR addr, UBYTE value)
{
    /* Yes, there are programs that do this. The programmers should be shot.
     * This might actually work sometimes. */
    UWORD rval = value;
    CPTR raddr = addr & 0x1FE;
    if (addr & 1) {
	rval |= cregs[raddr >> 1] & 0xFF00;
    } else {
	rval <<= 8;
	rval |= cregs[raddr >> 1] & 0xFF;
    }
    custom_wput(raddr, rval);
}

void custom_lput(CPTR addr, ULONG value)
{
    custom_wput(addr & 0xfffe, value >> 16);
    custom_wput((addr+2) & 0xfffe, (UWORD)value);
}

static bool copcomp(void)
{
    UWORD vp = vpos & (((copi2 >> 8) & 0x7F) | 0x80);
    UWORD hp = current_hpos() & (copi2 & 0xFE);
    UWORD vcmp = copi1 >> 8;
    UWORD hcmp = copi1 & 0xFE;
    return (vp > vcmp || (vp == vcmp && hp >= hcmp)) && ((copi2 & 0x8000) || !(DMACONR() & 0x4000));
}

/*
 * Calculate the minimum number of cycles after which the
 * copper comparison becomes true. This is quite tricky. I hope it works.
 */
static int calc_copcomp_true(int currvpos, int currhpos)
{    
    UWORD vp = currvpos & (((copi2 >> 8) & 0x7F) | 0x80);
    UWORD hp = currhpos & (copi2 & 0xFE);
    UWORD vcmp = copi1 >> 8;
    UWORD hcmp = copi1 & 0xFE;

    int cycleadd = maxhpos - currhpos;
    int coptime = 0;

    if ((vp > vcmp || (vp == vcmp && hp >= hcmp)) && ((copi2 & 0x8000) || !(DMACONR() & 0x4000)))
    	return 0;    
    
    try_again:
    
    while (vp < vcmp) {
	currvpos++;
	if (currvpos > maxvpos + 1)
	    return -1;
	currhpos = 0;
	coptime += cycleadd;
	cycleadd = maxhpos;
	vp = currvpos & (((copi2 >> 8) & 0x7F) | 0x80);
    }
    hp = currhpos & (copi2 & 0xFE);
    if (!(vp > vcmp)) {	
	while (hp < hcmp) {
	    currhpos++;
	    if (currhpos > maxhpos) {
		/* Now, what? There might be a good position on the
		 * next line. But it can also be the FFFF FFFE
		 * case.
		 */
		currhpos = 0;
		currvpos++;
		vp = currvpos & (((copi2 >> 8) & 0x7F) | 0x80);
		goto try_again;
	    }
	    coptime++;
	    hp = currhpos & (copi2 & 0xFE);
	}
    }
    if (coptime == 0) /* waiting for the blitter */
    	return 1;
    
    return coptime;
}

static void copper_read(void)
{
    if (dmaen(DMA_COPPER)){
	copi1 = chipmem_bank.wget(coplc); 
	copi2 = chipmem_bank.wget(coplc+2);
	coplc += 4;
	eventtab[ev_copper].oldcycles = cycles;
	eventtab[ev_copper].evtime = (copi1 & 1) ? (copi2 & 1) ? 4 : 6 : 4;
	copstate = (copi1 & 1) ? (copi2 & 1) ? COP_skip : COP_wait : COP_move;
    } else {
	copstate = COP_read;
	eventtab[ev_copper].active = false;
    }
}

static void do_copper(void)
{    
    switch(copstate){
     case COP_read:
	copper_read();
	break;
     case COP_move:
	if (copi1 >= (copcon & 2 ? 0x40 : 0x80))
	    custom_bank.wput(copi1,copi2);
	copper_read();
	break;
     case COP_skip:
	if (copcomp())
	    coplc += 4;
	copper_read();
	break;
     case COP_wait: {	    
	 int coptime = calc_copcomp_true(vpos, current_hpos());
	 if (coptime < 0) {
	     copstate = COP_stop;
	     eventtab[ev_copper].active = false;
	     copper_active = false;
	 } else {	     
	     if (!coptime)
	     	 copper_read();
	     else {
		 eventtab[ev_copper].evtime = coptime;
		 eventtab[ev_copper].oldcycles = cycles;
	     }
	 }
	 break;
     }
     case COP_stop:
	eventtab[ev_copper].active = false;
	copper_active = false;
	break;
    }
}

static __inline__ bool blitter_read(void)
{
    if (bltcon0 & 0xe00){
	if (!dmaen(DMA_BLITTER)) return true; /* blitter stopped */
	if (!blitline){
	    if (bltcon0 & 0x800) bltadat = chipmem_bank.wget(bltapt);
	    if (bltcon0 & 0x400) bltbdat = chipmem_bank.wget(bltbpt);
	}
	if (bltcon0 & 0x200) bltcdat = chipmem_bank.wget(bltcpt);
    }
    bltstate = BLT_work;
    return (bltcon0 & 0xE00) != 0;
}

static __inline__ bool blitter_write(void)
{
    if (bltddat) blitzero = false;
    if ((bltcon0 & 0x100) || blitline){
	if (!dmaen(DMA_BLITTER)) return true;
	chipmem_bank.wput(bltdpt,bltddat);
    }
    bltstate = BLT_next;
    return (bltcon0 & 0x100) != 0;
}

static void blitter_blit(void)
{
    UWORD blitahold,blitbhold,blitchold;
    UWORD bltaold;
    
    if (blitdesc) {
	UWORD bltamask = 0xffff;
	
	if (!blitlpos) { bltamask &= bltafwm; }
	if (blitlpos == (hblitsize - 1)) { bltamask &= bltalwm; }
	bltaold = bltadat & bltamask;

	blitahold = (((ULONG)bltaold << 16) | blitpreva) >> (16-blitashift);
	blitbhold = (((ULONG)bltbdat << 16) | blitprevb) >> (16-blitbshift);
	blitchold = bltcdat;
    } else {
	UWORD bltamask = 0xffff;
	
	if (!blitlpos) { bltamask &= bltafwm; }
	if (blitlpos == (hblitsize - 1)) { bltamask &= bltalwm; }
	bltaold = bltadat & bltamask;

	blitahold = (((ULONG)blitpreva << 16) | bltaold) >> blitashift;
	blitbhold = (((ULONG)blitprevb << 16) | bltbdat) >> blitbshift;
	blitchold = bltcdat;
    }
    bltddat = 0;
    bltddat = blit_func(blitahold, blitbhold, blitchold, bltcon0 & 0xFF);
    if (blitfill){
	UWORD fillmask;
	for (fillmask = 1; fillmask; fillmask <<= 1){
	    UWORD tmp = bltddat;
	    if (blitfc) {
		if (blitife)
		    bltddat |= fillmask;
		else
		    bltddat ^= fillmask;
	    }
	    if (tmp & fillmask) blitfc = !blitfc;
	}
    }
    bltstate = BLT_write;
    blitpreva = bltaold; blitprevb = bltbdat;
}

static void blitter_nxblit(void)
{
    bltstate = BLT_read;
    if (blitdesc){
	if (++blitlpos == hblitsize) {
	    if (--vblitsize == 0) {
		bltstate = BLT_done;
#ifdef NO_FAST_BLITTER
		custom_bank.wput(0xDFF09C,0x8040);
#endif
	    }
	    blitfc = bltcon1 & 0x4;

	    blitlpos = 0;
	    if (bltcon0 & 0x800) bltapt -= 2+bltamod; 
	    if (bltcon0 & 0x400) bltbpt -= 2+bltbmod; 
	    if (bltcon0 & 0x200) bltcpt -= 2+bltcmod; 
	    if (bltcon0 & 0x100) bltdpt -= 2+bltdmod;
	} else {
	    if (bltcon0 & 0x800) bltapt -= 2; 
	    if (bltcon0 & 0x400) bltbpt -= 2; 
	    if (bltcon0 & 0x200) bltcpt -= 2; 
	    if (bltcon0 & 0x100) bltdpt -= 2;	    
	}
    } else {
	if (++blitlpos == hblitsize) {
	    if (--vblitsize == 0) { 
		bltstate = BLT_done;
#ifdef NO_FAST_BLITTER
		custom_bank.wput(0xDFF09C,0x8040);
#endif
	    }
	    blitlpos = 0;
	    if (bltcon0 & 0x800) bltapt += 2+bltamod; 
	    if (bltcon0 & 0x400) bltbpt += 2+bltbmod; 
	    if (bltcon0 & 0x200) bltcpt += 2+bltcmod; 
	    if (bltcon0 & 0x100) bltdpt += 2+bltdmod;
	} else {
	    if (bltcon0 & 0x800) bltapt += 2; 
	    if (bltcon0 & 0x400) bltbpt += 2; 
	    if (bltcon0 & 0x200) bltcpt += 2; 
	    if (bltcon0 & 0x100) bltdpt += 2;
	}
    }
}

static __inline__ void blitter_line_incx(int a)
{
    blinea >>= 1;
    if (!blinea) {
	blinea = 0x8000;
	bltcnxlpt += 2;
	bltdnxlpt += 2;
    }
}

static __inline__ void blitter_line_decx(int shm)
{
    if (shm) blineb = (blineb << 2) | (blineb >> 14);
    blinea <<= 1;
    if (!blinea) {
	blinea = 1;
	bltcnxlpt -= 2;
	bltdnxlpt -= 2;
    }
}

static __inline__ void blitter_line_decy(int shm)
{
    if (shm) blineb = (blineb >> 1) | (blineb << 15);
    bltcnxlpt -= bltcmod;
    bltdnxlpt -= bltcmod; /* ??? am I wrong or doesn't KS1.3 set bltdmod? */
    blitonedot = 0;
}

static __inline__ void blitter_line_incy(int shm)
{
    if (shm) blineb = (blineb >> 1) | (blineb << 15);
    bltcnxlpt += bltcmod;
    bltdnxlpt += bltcmod; /* ??? */
    blitonedot = 0;
}

static void blitter_line(void)
{
    UWORD blitahold = blinea, blitbhold = blineb, blitchold = bltcdat;
    bltddat = 0;
    
    if (blitsing && blitonedot) blitahold = 0;
    blitonedot = 1;
    bltddat = blit_func(blitahold, blitbhold, blitchold, bltcon0 & 0xFF);
    if (!blitsign){
	bltapt += (WORD)bltamod;
	if (bltcon1 & 0x10){
	    if (bltcon1 & 0x8)
	    	blitter_line_decy(0);
	    else
	    	blitter_line_incy(0);
	} else {
	    if (bltcon1 & 0x8)
	    	blitter_line_decx(0);
	    else 
	    	blitter_line_incx(0);
	}
    } else {
	bltapt += (WORD)bltbmod;
    }
    if (bltcon1 & 0x10){
	if (bltcon1 & 0x4)
	    blitter_line_decx(1);
	else
	    blitter_line_incx(1);
    } else {
	if (bltcon1 & 0x4)
	    blitter_line_decy(1);
	else
	    blitter_line_incy(1);
    }
    blitsign = 0 > (WORD)bltapt;
    bltstate = BLT_write;
}

static __inline__ void blitter_nxline(void)
{
    if (--vblitsize == 0) {
	bltstate = BLT_done;
#ifdef NO_FAST_BLITTER
	custom_bank.wput(0xDFF09C,0x8040);
#endif
    } else {
	bltstate = BLT_read;
	bltcpt = bltcnxlpt;
	bltdpt = bltdnxlpt;
    }
}

static void blit_init(void)
{
    vblitsize = bltsize >> 6;
    hblitsize = bltsize & 0x3F;
    if (!vblitsize) vblitsize = 1024;
    if (!hblitsize) hblitsize = 64;
    blitlpos = 0;
    blitzero = true; blitpreva = blitprevb = 0;
    blitline = bltcon1 & 1;
    blitashift = bltcon0 >> 12; blitbshift = bltcon1 >> 12;
    
    if (blitline) {
	bltcnxlpt = bltcpt;
	bltdnxlpt = bltdpt;
	blitsing = bltcon1 & 0x2;
	blinea = bltadat >> blitashift;
	blineb = (bltbdat >> blitbshift) | (bltbdat << (16-blitbshift));
	blitsign = bltcon1 & 0x40; 
	blitonedot = false;
	bltamod &= 0xfffe; bltbmod &= 0xfffe; bltapt &= 0xfffe;
    } else {
	blitfc = bltcon1 & 0x4;
	blitife = bltcon1 & 0x8;
	blitfill = bltcon1 & 0x18;

	if ((bltcon1 & 0x18) == 0x18) {
	    /* Digital "Trash" demo does this; others too. Apparently, no
	     * negative effects. */
	    static bool warn = true;
	    if (warn)
	    	fprintf(stderr, "warning: weird fill mode (further messages suppressed)\n");
	    warn = false;
	}
	blitdesc = bltcon1 & 0x2;
	if (blitfill && !blitdesc)
	    fprintf(stderr, "warning: blitter fill without desc\n");
    }
}

#ifdef DEFERRED_INT

static UWORD deferred_bits;

static void defer_int_handler(void)
{
    INTREQ(0x8000 | deferred_bits);
    deferred_bits = 0;
    eventtab[ev_deferint].active = false;
}
#endif

void do_blitter(void)
{
#ifdef NO_FAST_BLITTER
    /* I'm not sure all this bltstate stuff is really necessary.
     * Most programs should be OK if the blit is done as soon as BLTSIZE is
     * written to, and the BLTFINISH bit is set some time after that.
     * This code here is nowhere near exact.
     */
    do {	
	switch(bltstate) {
	 case BLT_init:
	    blit_init();
	    bltstate = BLT_read;
	    /* fall through */
	 case BLT_read:
	    if (blitter_read())
	    	break;
	    /* fall through */
	 case BLT_work:
	    if (blitline)
	    	blitter_line(); 
	    else 
	    	blitter_blit();
	    /* fall through */
	 case BLT_write:
	    if (blitter_write())
	    	break;
	    /* fall through */
	 case BLT_next:
	    if (blitline)
	    	blitter_nxline();
	    else 
	    	blitter_nxblit();
	    break;
	 case BLT_done:
	    specialflags &= ~SPCFLAG_BLIT;
	    break;
	}
    } while(bltstate != BLT_done && dmaen(DMA_BLITTER)
	    && dmaen(DMA_BLITPRI));  /* blitter nasty -> no time for CPU */
#else
    if (bltstate == BLT_init) {
	blit_init();
	bltstate =  BLT_read;
    }
    if (!dmaen(DMA_BLITTER))
    	return; /* gotta come back later. */

    if (blitline) {
	do {
	    blitter_read();
	    blitter_line();
	    blitter_write();
	    blitter_nxline();
	} while (bltstate != BLT_done);
    } else {
	do {	
	    blitter_read();
	    blitter_blit();
	    blitter_write();
	    blitter_nxblit();
	} while (bltstate != BLT_done);
    }
    eventtab[ev_deferint].active = true;
    eventtab[ev_deferint].oldcycles = cycles;
    eventtab[ev_deferint].evtime = 10; /* whatever */
    deferred_bits |= 0x40;
    events_schedule();

    specialflags &= ~SPCFLAG_BLIT;
#endif
}

void do_disk(void)
{
#ifdef NO_FAST_FLOPPY
    if (dskdmaen > 1 && dmaen(0x10)){
	if (--dsktime == 0) {
	    dsktime = dskdelay;
	    if (dsklen & 0x4000){
		UWORD dsksync_check;
		DISK_GetData(&dsksync_check, &dskbyte);
		dskbyte |= 0x8000;
		if (dsksynced || !(adkcon & 0x400)) {
		    *mfmwrite++ = chipmem_bank.wget(dskpt); dskpt += 2;
		    if (--dsklength == 0) {
			DISK_WriteData();
			custom_bank.wput(0xDFF09C, 0x8002); /*INTREQ->DSKBLK */
			dskdmaen = 0;
			specialflags &= ~SPCFLAG_DISK;
		    }
		}
		if (dsksync_check == dsksync) {
		    custom_bank.wput(0xDFF09C, 0x9000);
		    dsksynced = true;
		}
	    } else {
		DISK_GetData(&dskmfm, &dskbyte);
		dskbyte |= 0x8000;
		if (dsksynced || !(adkcon & 0x400)){
		    chipmem_bank.wput(dskpt, dskmfm); dskpt += 2;
		    if (--dsklength == 0) {
			custom_bank.wput(0xDFF09C, 0x8002);
			dskdmaen = 0;
			specialflags &= ~SPCFLAG_DISK;
		    }
		}
		if (dskmfm == dsksync) {
		    custom_bank.wput(0xDFF09C,0x9000);
		    dsksynced = true;
		}
	    }
	}
    }
#else
    if (dskdmaen > 1 && dmaen(0x10)){
	/* Just don't get into an infinite loop if the code below screws up */
	int ntries = 100000; 
	
	while (--ntries > 0 && (specialflags & SPCFLAG_DISK)) {
	    if (dsklen & 0x4000){
		UWORD dsksync_check;
		DISK_GetData(&dsksync_check, &dskbyte);
		dskbyte |= 0x8000;
		if (dsksynced || !(adkcon & 0x400)) {
		    *mfmwrite++ = chipmem_bank.wget(dskpt); dskpt += 2;
		    if (--dsklength == 0) {
			DISK_WriteData();
			dskdmaen = 0;
			specialflags &= ~SPCFLAG_DISK;
			eventtab[ev_deferint].active = true;
			eventtab[ev_deferint].oldcycles = cycles;
			eventtab[ev_deferint].evtime = 10; /* whatever */
			events_schedule ();
			deferred_bits |= 0x0002;
		    }
		}
		if (dsksync_check == dsksync) {
		    /* This is pretty pointless... */
		    custom_bank.wput(0xDFF09C, 0x9000);
		    dsksynced = true;
		}
	    } else {
		DISK_GetData(&dskmfm, &dskbyte);
		dskbyte |= 0x8000;
		if (dsksynced || !(adkcon & 0x400)){
		    chipmem_bank.wput(dskpt, dskmfm); dskpt += 2;
		    if (--dsklength == 0) {
			eventtab[ev_deferint].active = true;
			eventtab[ev_deferint].oldcycles = cycles;
			eventtab[ev_deferint].evtime = 10; /* whatever */
			events_schedule ();
			deferred_bits |= 0x0002;
			dskdmaen = 0;
			specialflags &= ~SPCFLAG_DISK;
		    }
		}
		if (dskmfm == dsksync) {
		    /* This is pretty pointless... */
		    custom_bank.wput(0xDFF09C,0x9000);
		    dsksynced = true;
		}
	    }
	}
    }
#endif
}

static __inline__ void pfield_fetchdata(void)
{
    if (dmaen(0x100) && pfield_linedmaon) {
	switch(bplplanecnt){
	 case 6:
	    bpl6dat = chipmem_bank.wget(bpl6pt); bpl6pt += 2; bpl6dat <<= 5;
	 case 5:
	    bpl5dat = chipmem_bank.wget(bpl5pt); bpl5pt += 2; bpl5dat <<= 4;
	 case 4:
	    bpl4dat = chipmem_bank.wget(bpl4pt); bpl4pt += 2; bpl4dat <<= 3;
	 case 3:
	    bpl3dat = chipmem_bank.wget(bpl3pt); bpl3pt += 2; bpl3dat <<= 2;
	 case 2:
	    bpl2dat = chipmem_bank.wget(bpl2pt); bpl2pt += 2; bpl2dat <<= 1;
	 case 1:
	    bpl1dat = chipmem_bank.wget(bpl1pt); bpl1pt += 2;
	}
    }
}

static void pfield_hsync(int plfline)
{
}

static void do_sprites(int currvp)
{   
    int spr;
    for(spr = 0; spr < 8; spr++) {
	int vstart = (sprpos[spr] >> 8) | ((sprctl[spr] << 6) & 0x100);
	int vstop = (sprctl[spr] >> 8) | ((sprctl[spr] << 7) & 0x100);
	if ((vstart <= currvp && vstop >= currvp) || spron[spr] == 1) {
	    if (dmaen(0x20)) {
		UWORD data1 = chipmem_bank.wget(sprpt[spr]);
		UWORD data2 = chipmem_bank.wget(sprpt[spr]+2);
		sprpt[spr] += 4;
		
		if (vstop != currvp && spron[spr] != 1) {
		    SPRxDATB(data2, spr);
		    SPRxDATA(data1, spr);
		    if (!spr && !sprvbfl && ((sprpos[0]&0xff)<<2)>0x60) {
			sprvbfl=2; 
			spr0ctl=sprctl[0]; 
			spr0pos=sprpos[0]; 
		    }
		} else {
		    SPRxPOS(data1, spr);
		    SPRxCTL(data2, spr);
		}

#if 0
		if (vstop != currvp && !sprptset[spr]) {
		    SPRxDATB(data2, spr);
		    SPRxDATA(data1, spr);
		    if (!spr && !sprvbfl && ((sprpos[0]&0xff)<<2)>0x60) {
			sprvbfl=2; 
			spr0ctl=sprctl[0]; 
			spr0pos=sprpos[0]; 
		    }
		} else {
		    SPRxPOS(data1, spr);
		    SPRxCTL(data2, spr);
		    sprptset[spr] = false;
		}
#endif
	    }
	}
    }
}

static void pfield_modulos(void)
{
    switch(bplplanecnt){
     case 6:
	bpl6pt += bpl2mod;
     case 5:
	bpl5pt += bpl1mod;
     case 4:
	bpl4pt += bpl2mod;
     case 3:
	bpl3pt += bpl1mod;
     case 2:
	bpl2pt += bpl2mod;
     case 1:
	bpl1pt += bpl1mod;
    }	 
}

static void pfield_linetoscr(int pix, int stoppos)
{
    if (bplcon0 & 0x800 && bplplanecnt == 6) {
	/* HAM */
	static UWORD lastcolor;
	while (pix < diwfirstword && pix < stoppos) {
	    DrawPixel((pix++)-16, acolors[0]);
	    lastcolor = color_regs[0];
	}
	while (pix < diwlastword && pix < stoppos) {
	    int pv = apixels[pix];
	    switch(pv & 0x30) {
	     case 0x00: lastcolor = color_regs[pv]; break;
	     case 0x10: lastcolor &= 0xFF0; lastcolor |= (pv & 0xF); break;
	     case 0x20: lastcolor &= 0x0FF; lastcolor |= (pv & 0xF) << 8; break;
	     case 0x30: lastcolor &= 0xF0F; lastcolor |= (pv & 0xF) << 4; break;
	    }
	    
	    if (spixstate[pix]) {
		DrawPixel(pix-16, acolors[spixels[pix]+16]);
		spixels[pix] = spixstate[pix] = 0;
	    } else {
		DrawPixel(pix-16, xcolors[lastcolor]);
	    }
	    pix++;
	}
	while (pix < stoppos) {
	    DrawPixel((pix++)-16, acolors[0]);
	}
    } else {	    
	if (bplcon0 & 0x400) {
	    int *lookup = (bplcon2 & 0x40) ? dblpf_ind2 : dblpf_ind1;
	    /* Dual playfield */
	    while (pix < diwfirstword && pix < stoppos) {
		DrawPixel((pix++)-16, acolors[0]);
	    }
	    while (pix < diwlastword && pix < stoppos) {
		if (spixstate[pix]) {
		    DrawPixel(pix-16, acolors[spixels[pix]+16]);
		    spixels[pix] = spixstate[pix] = 0;
		} else {
		    DrawPixel(pix-16, acolors[lookup[apixels[pix]]]);
		}
		pix++;
	    }
	    while (pix < stoppos) {
		DrawPixel((pix++)-16, acolors[0]);
	    }
	} else {	
	    while (pix < diwfirstword && pix < stoppos) {
		DrawPixel((pix++)-16, acolors[0]);
	    }
	    while (pix < diwlastword && pix < stoppos) {
		if (spixstate[pix]) {
		    if (apixels[pix] == 0 || spixstate[pix] < plf2pri)
			DrawPixel(pix-16, acolors[spixels[pix]+16]);
		    else
			DrawPixel(pix-16, acolors[apixels[pix]]);
		    spixels[pix] = spixstate[pix] = 0;
		} else {		    
		    DrawPixel(pix-16, acolors[apixels[pix]]);
		}
		pix++;
	    }
	    while (pix < stoppos) {
		DrawPixel((pix++)-16, acolors[0]);
	    }
	}
    }
}

static void pfield_sprite (int num, int sprxp, UWORD data, UWORD datb)
{
    int i;
    for(i = 15; i >= 0; i--) {
	int sprxpos = sprxp + i*2;
	if ((sprctl[num] & 0x80) && (num & 1)) {
	    /* Attached sprite */
	    int col = ((data << 2) & 4) + ((datb << 3) & 8);
	    if (col) {
		spixels[sprxpos] = col;
		spixels[sprxpos+1] = col;
		spixstate[sprxpos] = 1 << num;
		spixstate[sprxpos+1] = 1 << num;
	    }
	    spixstate[sprxpos] |= 1 << (num-1);
	    spixstate[sprxpos+1] |= 1 << (num-1);
	} else {			
	    int col = (data & 1) | ((datb << 1) & 2);
	    if (spixstate[sprxpos] & (1 << num)) {
		/* finish attached sprite */
		/* Did the upper half of the sprite have any bits set? */
		if (spixstate[sprxpos] & (1 << (num+1)))
		    col += spixels[sprxpos];
		if (!col) {
		    spixstate[sprxpos] = spixstate[sprxpos+1] &= ~(3 << num);
		}
	    } else {
		if (col) {
		    col |= (num & 6) << 1;
		}
	    }
	    if (col) {
		spixels[sprxpos] = col;
		spixels[sprxpos+1] = col;
		spixstate[sprxpos] = 1<<num;
		spixstate[sprxpos+1] = 1<<num;
	    }
	}
	data >>= 1;
	datb >>= 1;
    }   
}

void pfield_doline_slow(int currhpos)
{
    int xpos = currhpos * 4 - 0x60;
    static int nextpos = -1;
    static int linepos;
    static bool paststop = false;
    
    if (vpos < plffirstline || vpos >= plflastline) 
    	return;
    
    if (currhpos == plfstrt) {	
	nextpos = currhpos;
	linepos = 0;
	paststop = false;
    }
    if (currhpos == nextpos) {    
	if (linepos >= plflinelen) {
	    nextpos = -1;
	    pfield_modulos();
	} else {	    
	    nextpos += bplhires ? 4 : 8;
	    linepos += bplhires ? 4 : 8;
	    
	    pfield_fetchdata();
	    
	    if (bplhires) {
		int offs1 = 2 + xpos + 16 + bpldelay1*2; /* Never trust compilers, kids! */
		int offs2 = 2 + xpos + 16 + bpldelay2*2;
		
		int pix;
		for(pix = 15; pix >= 0; pix--) {
		    switch(bplplanecnt) {
		     case 4:
			apixels[pix + offs2] |= bpl4dat & 0x8; bpl4dat >>= 1;
		     case 3:
			apixels[pix + offs1] |= bpl3dat & 0x4; bpl3dat >>= 1;
		     case 2:
			apixels[pix + offs2] |= bpl2dat & 0x2; bpl2dat >>= 1;
		     case 1:
			apixels[pix + offs1] |= bpl1dat & 0x1; bpl1dat >>= 1;
		    }
		}
	    } else {
		int offs1 = 2 + xpos + 32 + bpldelay1*2;
		int offs2 = 2 + xpos + 32 + bpldelay2*2;
		
		int pix;
		for(pix = 30; pix >= 0; pix -= 2) {
		    switch(bplplanecnt) {
		     case 6:
			apixels[pix + offs2] |= bpl6dat & 0x20;
			apixels[pix + offs2 + 1] |= bpl6dat & 0x20; bpl6dat >>= 1;
		     case 5:
			apixels[pix + offs1] |= bpl5dat & 0x10;
			apixels[pix + offs1 + 1] |= bpl5dat & 0x10; bpl5dat >>= 1;
		     case 4:
			apixels[pix + offs2] |= bpl4dat & 0x8;
			apixels[pix + offs2 + 1] |= bpl4dat & 0x8; bpl4dat >>= 1;
		     case 3:
			apixels[pix + offs1] |= bpl3dat & 0x4; 
			apixels[pix + offs1 + 1] |= bpl3dat & 0x4; bpl3dat >>= 1;
		     case 2:
			apixels[pix + offs2] |= bpl2dat & 0x2; 
			apixels[pix + offs2 + 1] |= bpl2dat & 0x2; bpl2dat >>= 1;
		     case 1:
			apixels[pix + offs1] |= bpl1dat & 0x1; 
			apixels[pix + offs1 + 1] |= bpl1dat & 0x1; bpl1dat >>= 1;
		    }
		}
	    }	
	}
    }
    if (xpos > 0x60) { /* sprites can't appear that far to the left */
	int spr;
	for(spr = 7; spr >= 0; spr--) {
	    if (sprarmed[spr] && xpos == ((sprpos[spr] & 0xFF) * 4) - 0x60) {
		int sprxp = xpos + 2 + (sprctl[spr] & 1)*2;
		pfield_sprite (spr, sprxp, sprdata[spr], sprdatb[spr]);
	    }
	}
    }
    
    if (xpos >= 16 && xpos <= 812) {
	pfield_linetoscr (xpos, xpos+4);
    }
}

static __inline__ UWORD *pfield_xlateptr(CPTR plpt, int bytecount)
{
    if (!chipmem_bank.check(plpt,bytecount)) {
	fprintf(stderr, "Warning: Bad playfield pointer\n");
	return NULL;
    }
    return chipmem_bank.xlateaddr(plpt);
}

/*
 * A few hand-unrolled loops...
 */
static __inline__ void pfield_orword_hires(UWORD **plfptr, 
					   unsigned char **dataptr, int bit)
{
    UWORD data = *(*plfptr)++;
    unsigned char *pixptr = *dataptr;
    
    if (data & 0x8000) *pixptr |= bit;
    pixptr++;
    if (data & 0x4000) *pixptr |= bit;
    pixptr++;
    if (data & 0x2000) *pixptr |= bit;
    pixptr++;
    if (data & 0x1000) *pixptr |= bit;
    pixptr++;
    if (data & 0x800) *pixptr |= bit;
    pixptr++;
    if (data & 0x400) *pixptr |= bit;
    pixptr++;
    if (data & 0x200) *pixptr |= bit;
    pixptr++;
    if (data & 0x100) *pixptr |= bit;
    pixptr++;
    if (data & 0x80) *pixptr |= bit;
    pixptr++;
    if (data & 0x40) *pixptr |= bit;
    pixptr++;
    if (data & 0x20) *pixptr |= bit;
    pixptr++;
    if (data & 0x10) *pixptr |= bit;
    pixptr++;
    if (data & 0x8) *pixptr |= bit;
    pixptr++;
    if (data & 0x4) *pixptr |= bit;
    pixptr++;
    if (data & 0x2) *pixptr |= bit;
    pixptr++;
    if (data & 0x1) *pixptr |= bit;
    pixptr++;
    *dataptr = pixptr;
}

static __inline__ void pfield_orword_lores(UWORD **plfptr,
					   unsigned char **dataptr, int bit)
{
    UWORD data = *(*plfptr)++;
    unsigned char *pixptr = *dataptr;
    if (data & 0x8000) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x4000) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x2000) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x1000) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x800) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x400) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x200) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x100) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x80) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x40) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x20) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x10) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x8) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x4) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x2) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    if (data & 0x1) { *pixptr |= bit; *(pixptr+1) |= bit; }
    pixptr+=2;
    *dataptr = pixptr;
}

static __inline__ void pfield_setword_hires(UWORD **plfptr, 
					    unsigned char **dataptr, int bit)
{
    UWORD data = *(*plfptr)++;
    unsigned char *pixptr = *dataptr; 
    if (data & 0x8000) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x4000) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x2000) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x1000) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x800) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x400) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x200) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x100) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x80) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x40) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x20) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x10) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x8) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x4) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x2) *pixptr++ = bit;
    else *pixptr++ = 0;
    if (data & 0x1) *pixptr++ = bit;
    else *pixptr++ = 0;
    *dataptr = pixptr;
}
	
static __inline__ void pfield_setword_lores(UWORD **plfptr,
					    unsigned char **dataptr, int bit)
{
    UWORD data = *(*plfptr)++;
    unsigned char *pixptr = *dataptr;
    
    if (data & 0x8000) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x4000) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x2000) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x1000) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x800) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x400) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x200) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x100) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x80) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x40) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x20) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x10) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x8) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x4) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x2) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    if (data & 0x1) { *pixptr++ = bit; *pixptr++ = bit; }
    else { *pixptr++ = 0; *pixptr++ = 0; }
    
    *dataptr = pixptr;
}

static void pfield_doline(void)
{
    int xpos = plfstrt * 4 - 0x60;
    int spr;
    int linelen;

    if (vpos < plffirstline || vpos >= plflastline) 
    	return;

    if (dmaen(0x100) && pfield_linedmaon) {	
	/*
	 * We want to use real pointers, but we don't want to get segfaults
         * because of them.
         */
	UWORD *r_bpl1pt,*r_bpl2pt,*r_bpl3pt,*r_bpl4pt,*r_bpl5pt,*r_bpl6pt;
	int bytecount = plflinelen / (bplhires ? 4 : 8) * 2;
	switch (bplplanecnt) {	    
	 case 6:
	    r_bpl6pt = pfield_xlateptr(bpl6pt, bytecount);
	    if (r_bpl6pt == NULL)
		return;
	 case 5: 
	    r_bpl5pt = pfield_xlateptr(bpl5pt, bytecount);
	    if (r_bpl5pt == NULL)
		return;
	 case 4:
	    r_bpl4pt = pfield_xlateptr(bpl4pt, bytecount);
	    if (r_bpl4pt == NULL)
		return;
	 case 3:
	    r_bpl3pt = pfield_xlateptr(bpl3pt, bytecount);
	    if (r_bpl3pt == NULL)
		return;
	 case 2: 
	    r_bpl2pt = pfield_xlateptr(bpl2pt, bytecount);
	    if (r_bpl2pt == NULL)
		return;
	 case 1:
	    r_bpl1pt = pfield_xlateptr(bpl1pt, bytecount);
	    if (r_bpl1pt == NULL)
		return;
	} 
	switch (bplplanecnt) {	    
	 case 6: bpl6pt += bytecount + bpl2mod;
	 case 5: bpl5pt += bytecount + bpl1mod;
	 case 4: bpl4pt += bytecount + bpl2mod;
	 case 3: bpl3pt += bytecount + bpl1mod;
	 case 2: bpl2pt += bytecount + bpl2mod;
	 case 1: bpl1pt += bytecount + bpl1mod;
	}
 
	if (bplhires) {	
	    int xpos1 = 2 + xpos + 16 + bpldelay1*2;
	    int xpos2 = 2 + xpos + 16 + bpldelay2*2;
	    unsigned char *xp1ptr = apixels+xpos1;
	    unsigned char *xp2ptr = apixels+xpos2;

	    if (bplplanecnt > 0) {
		unsigned char *app = xp1ptr;
		for (linelen = plflinelen; linelen > 0; linelen-=4) {
		    pfield_setword_hires(&r_bpl1pt, &app, 1);
		}
		if (bplplanecnt > 2) {
		    unsigned char *app = xp1ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=4) {
			pfield_orword_hires(&r_bpl3pt, &app, 4);
		    }
		}
		if (bplplanecnt > 1) {
		    unsigned char *app = xp2ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=4) {
			pfield_orword_hires(&r_bpl2pt, &app, 2);
		    }
		}
		if (bplplanecnt > 3) {
		    unsigned char *app = xp2ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=4) {
			pfield_orword_hires(&r_bpl4pt, &app, 8);
		    }
		}
	    } else {
		memset(apixels, 0, sizeof(apixels));
	    }
	} else {
	    int xpos1 = 2 + xpos + 32 + bpldelay1*2;
	    int xpos2 = 2 + xpos + 32 + bpldelay2*2;
	    unsigned char *xp1ptr = apixels+xpos1;
	    unsigned char *xp2ptr = apixels+xpos2;
	    if (bplplanecnt > 0) {
		unsigned char *app = xp1ptr;
		for (linelen = plflinelen; linelen > 0; linelen-=8) {
		    pfield_setword_lores(&r_bpl1pt, &app, 1);
		}
		if (bplplanecnt > 2) {
		    unsigned char *app = xp1ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=8) {
			pfield_orword_lores(&r_bpl3pt, &app, 4);
		    }
		}
		if (bplplanecnt > 4) {
		    unsigned char *app = xp1ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=8) {
			pfield_orword_lores(&r_bpl5pt, &app, 16);
		    }
		}
		if (bplplanecnt > 1) {
		    unsigned char *app = xp2ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=8) {
			pfield_orword_lores(&r_bpl2pt, &app, 2);
		    }
		}
		if (bplplanecnt > 3) {
		    unsigned char *app = xp2ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=8) {
			pfield_orword_lores(&r_bpl4pt, &app, 8);
		    }
		}
		if (bplplanecnt > 5) {
		    unsigned char *app = xp2ptr;
		    for (linelen = plflinelen; linelen > 0; linelen-=8) {
			pfield_orword_lores(&r_bpl6pt, &app, 32);
		    }
		}
	    } else {
		memset(apixels, 0, sizeof(apixels));
	    }
	}
    }
    for(spr = 7; spr >= 0; spr--) {
	if (sprarmed[spr]) {
	    int sprxp = ((sprpos[spr] & 0xFF) * 4) - 0x60 + 2 + (sprctl[spr] & 1)*2;
	    int i;
	    /* Ugh. Nasty bug. Let's rather lose some sprites than trash
	     * memory. */
	    if (sprxp < 0)
		continue;
	    pfield_sprite (spr, sprxp, sprdata[spr], sprdatb[spr]);
	}
    }
    
    pfield_linetoscr (16, 812);
}

static void setdontcare(void)
{
    printf("Don't care mouse mode set\n");
    mousestate=dont_care_mouse;
    lastspr0x=lastmx; lastspr0y=lastmy;
    mstepx=defstepx; mstepy=defstepy;
}

static void setfollow(void)
{
    printf("Follow sprite mode set\n");
    mousestate=follow_mouse;
    lastdiffx=lastdiffy=0;
    sprvbfl=0;
    spr0ctl=spr0pos=0;
    mstepx=defstepx; mstepy=defstepy;
}

void togglemouse(void)
{
    switch(mousestate) {
     case dont_care_mouse: setfollow(); break;
     case follow_mouse: setdontcare(); break;
    }
}	    

#if 0 /* Mouse calibrations works well enough without this. */
void mousesetup(void)
{
    if (mousestate != follow_mouse) return;
    char buf[80];
    printf("\nMouse pointer centering.\n\n");
    printf("Current values: xoffs=0x%x yoffs=0x%x\n", xoffs, yoffs);
    printf("New values (y/n)?";
    cin.get(buf,80,'\n');
    if (toupper(buf[0]) != 'Y') return;
    cout << "\nxoffs= "; cin>>xoffs;
    cout << "yoffs= "; cin>>yoffs;
    cout << "\n";
}
#endif

static __inline__ int adjust(int val)
{
    if (val>127)
	return 127; 
    else if (val<-127)
	return -127;
    return val;
}

static void vsync_handler(void)
{
    int spr0x = ((spr0pos & 0xff) << 2) | ((spr0ctl & 1) << 1);
    int spr0y = ((spr0pos >> 8) | ((spr0ctl & 4) << 6)) << 1;
    int diffx, diffy;
    int i;
    
    handle_events();
#ifdef HAVE_JOYSTICK
    {
	UWORD dir;
	bool button;
	read_joystick(&dir, &button);
	buttonstate[1] = button;
    }
#endif

    if (produce_sound)
	flush_sound ();
    
    switch (mousestate) {
     case normal_mouse:
	diffx = lastmx - lastsampledmx;
	diffy = lastmy - lastsampledmy;
	if (!newmousecounters) {	
	    if (diffx > 127) diffx = 127;
	    if (diffx < -127) diffx = -127;
	    joy0x += diffx;
	    if (diffy > 127) diffy = 127;
	    if (diffy < -127) diffy = -127;
	    joy0y += diffy;
	}
	lastsampledmx += diffx; lastsampledmy += diffy;
	break;

     case dont_care_mouse:
	diffx = adjust (((lastmx-lastspr0x) * mstepx) >> 16);
	diffy = adjust (((lastmy-lastspr0y) * mstepy) >> 16);
	lastspr0x=lastmx; lastspr0y=lastmy;
	joy0x+=diffx; joy0y+=diffy;
	break;
	
     case follow_mouse:
	if (sprvbfl && sprvbfl-- >1) {
	    int mouseypos;
	    
	    if ((lastdiffx > docal || lastdiffx < -docal) && lastspr0x != spr0x 
		&& spr0x > plfstrt*4+34+xcaloff && spr0x < plfstop*4-xcaloff)
	    {  
		int val = (lastdiffx << 16) / (spr0x - lastspr0x);
		if (val>=0x8000) mstepx=(mstepx*(calweight-1)+val)/calweight;
	    }
	    if ((lastdiffy > docal || lastdiffy < -docal) && lastspr0y != spr0y
		&& spr0y>plffirstline+ycaloff && spr0y<plflastline-ycaloff) 
	    { 
		int val = (lastdiffy<<16) / (spr0y-lastspr0y);
		if (val>=0x8000) mstepy=(mstepy*(calweight-1)+val)/calweight;
	    }
	    mouseypos = lastmy;

	    /* Calculations might be incorrect */
	    if(dont_want_aspect)
	    	mouseypos *= 2;

	    /* cout<<" mstepx= "<<mstepx<<" mstepy= "<<mstepy<<"\n";*/
	    diffx = adjust ((((lastmx + 0x70 + xoffs - spr0x) & ~1) * mstepx) >> 16);
	    diffy = adjust ((((mouseypos + yoffs - spr0y+minfirstline*2) & ~1) * mstepy) >> 16);
	    lastspr0x=spr0x; lastspr0y=spr0y;
	    lastdiffx=diffx; lastdiffy=diffy;
	    joy0x+=diffx; joy0y+=diffy; 
	}
	break;
    }
 
    INTREQ(0x8020);
    if (bplcon0 & 4) lof ^= 0x8000;
    COPJMP1(0);
    
    for (i = 0; i < 8; i++)
	spron[i] = 0;

    if (framecnt == 0)
    	flush_screen ();
    count_frame();    
#ifdef __unix
    {
	struct timeval tv;
	unsigned long int newtime;
	
	gettimeofday(&tv,NULL);	
	newtime = (tv.tv_sec-seconds_base) * 1000 + tv.tv_usec / 1000;
	
	if (!bogusframe) {	
	    frametime += newtime - msecs;
	    timeframes++;
	}
	msecs = newtime;
	bogusframe = false;
    }
#endif
    CIA_vsync_handler();
}

static void hsync_handler(void)
{
    eventtab[ev_hsync].oldcycles = cycles;
    CIA_hsync_handler();
    
    if(produce_sound)
	do_sound ();

    if (framecnt == 0 && vpos >= plffirstline && vpos < plflastline 
	&& vpos >= minfirstline) 
    {
	/* Finish the line, if we started doing it with the slow update.
	 * Otherwise, draw it entirely. */
	if (pfield_fullline) {	    
	    if (!pfield_linedone) {		
	    	pfield_doline();
	    }
	} else {
	    int i;
	    for(i = pfield_lastpart_hpos; i < maxhpos; i++)
	    	pfield_doline_slow(i);
	}
	flush_line ();
    }
    pfield_calclinedma();
    pfield_fullline = true;
    pfield_linedone = false;
    pfield_lastpart_hpos = 0;

    if (++vpos == (maxvpos + (lof != 0))) {
	vpos = 0;
	vsync_handler();
    }
    
    /* Tell the graphics code about the next line. */
    if (framecnt == 0 && vpos >= plffirstline && vpos < plflastline
	&& vpos >= minfirstline) 
    {
	if (dont_want_aspect) {
	    prepare_line(vpos - minfirstline, false);
	} else {
	    if (bplcon0 & 4) {
		if(lof) {
		    prepare_line((vpos-minfirstline)*2, false);
		} else {
		    prepare_line((vpos-minfirstline)*2+1, false);
		}
	    } else {
		prepare_line((vpos-minfirstline)*2, true);
	    }
	}
    }

    /* do_sprites must be called after pfield_lastpart_hpos
     * is set to 0. */
    if (vpos > 0) do_sprites(vpos);
}

void customreset(void)
{
    int i;
#ifdef __unix
    struct timeval tv;
#endif
    inhibit_frame = 0;
    CIA_reset();
    cycles = 0; specialflags = 0;
    
    vpos = 0; lof = 0;
    
    if (needmousehack()) {
    	if (mousestate != follow_mouse) setfollow();
    } else {
	mousestate = normal_mouse;
    }

    memset(spixels, 0, sizeof(spixels));
    memset(spixstate, 0, sizeof(spixstate));
    
    prepare_line(0, false); /* Get the graphics stuff in a sane state */
    
    dmacon = intena = 0;
    bltstate = BLT_done;
    copstate = COP_stop;
    copcon = 0;
    dskdmaen = 0;
    cycles = 0;
    for(i = ev_copper; i < ev_max; i++) {
	eventtab[i].active = false;
	eventtab[i].oldcycles = 0;
    }
    copper_active = false;
    eventtab[ev_cia].handler = CIA_handler;
    eventtab[ev_copper].handler = do_copper;
    eventtab[ev_hsync].handler = hsync_handler;
    eventtab[ev_hsync].evtime = maxhpos;
    eventtab[ev_hsync].active = true;
#ifdef DEFERRED_INT
    eventtab[ev_deferint].handler = defer_int_handler;
    eventtab[ev_deferint].active = false;
    deferred_bits = 0;
#endif
    events_schedule();
    
#ifdef __unix
    gettimeofday(&tv,NULL);
    seconds_base = tv.tv_sec;
    bogusframe = true;
#endif
}

void dumpcustom(void)
{

    printf("DMACON: %x INTENA: %x INTREQ: %x VPOS: %x HPOS: %x\n", DMACONR(),
	   intena, intreq, vpos, current_hpos());
    if (timeframes) { 
	printf("Average frame time: %d ms [frames: %d time: %d]\n", 
	       frametime/timeframes, timeframes, frametime);
    }
}

int intlev(void)
{
    UWORD imask = intreq & intena;
    if (imask && (intena & 0x4000)){
	if (imask & 0x2000) return 6;
	if (imask & 0x1800) return 5;
	if (imask & 0x0780) return 4;
	if (imask & 0x0070) return 3;
	if (imask & 0x0008) return 2;
	if (imask & 0x0007) return 1;
    }
    return -1;
}

void custom_init(void)
{
    int num;
    setfollow();
    customreset();
    for (num = 0; num < 64; num++) {	
	int plane1 = (num & 1) | ((num >> 1) & 2) | ((num >> 2) & 4);
	int plane2 = ((num >> 1) & 1) | ((num >> 2) & 2) | ((num >> 3) & 4);
	if (plane2 > 0) plane2 += 8;
	dblpf_ind1[num] = plane1 == 0 ? plane2 : plane1;
	dblpf_ind2[num] = plane2 == 0 ? plane1 : plane2;
    }
}
