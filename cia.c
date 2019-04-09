 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * CIA chip support
  *
  * Copyright 1995 Bernd Schmidt, Alessandro Bissacco
  */

#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "cia.h"
#include "disk.h"
#include "xwin.h"
#include "keybuf.h"

#define DIV10 5 /* Yes, a bad identifier. */

static UBYTE ciaaicr,ciaaimask,ciabicr,ciabimask;
static UBYTE ciaacra,ciaacrb,ciabcra,ciabcrb;
static ULONG ciaata,ciaatb,ciabta,ciabtb;
static UWORD ciaala,ciaalb,ciabla,ciablb;
static ULONG ciaatod,ciabtod,ciaatol,ciabtol,ciaaalarm,ciabalarm;
static bool ciaatlatch,ciabtlatch;
static UBYTE ciaapra,ciaaprb,ciaadra,ciaadrb,ciaasdr;
static UBYTE ciabpra,ciabprb,ciabdra,ciabdrb,ciabsdr; 
static int div10;
static int kbstate;
static bool kback;

static void setclr(UBYTE *p, UBYTE val)
{
    if (val & 0x80) {
	*p |= val & 0x7F;
    } else {
	*p &= ~val;
    }
}

static void RethinkICRA(void)
{
    if (ciaaimask & ciaaicr) {
	ciaaicr |= 0x80;
	custom_bank.wput(0xDFF09C,0x8008);
    } else {
	ciaaicr &= 0x7F;
	custom_bank.wput(0xDFF09C,0x0008);
    }
}

static void RethinkICRB(void)
{
    if (ciabimask & ciabicr) {
	ciabicr |= 0x80;
	custom_bank.wput(0xDFF09C,0xA000);
    } else {
	ciabicr &= 0x7F;
	custom_bank.wput(0xDFF09C,0x2000);
    }
}

static int lastdiv10;

static void CIA_update(void)
{
    unsigned long int ccount = cycles - eventtab[ev_cia].oldcycles + lastdiv10;
    unsigned long int ciaclocks = ccount / DIV10;

    int aovfla = 0, aovflb = 0, bovfla = 0, bovflb = 0;

    lastdiv10 = div10;
    div10 = ccount % DIV10;
    
    /* CIA A timers */
    if ((ciaacra & 0x21) == 0x01) {
	assert((ciaata+1) >= ciaclocks);
	if ((ciaata+1) == ciaclocks) {
	    aovfla = 1;
	    if ((ciaacrb & 0x61) == 0x41) {
		if (ciaatb-- == 0) aovflb = 1;		
	    }
	} 	    
	ciaata -= ciaclocks;
    }
    if ((ciaacrb & 0x61) == 0x01) {
	assert((ciaatb+1) >= ciaclocks);
	if ((ciaatb+1) == ciaclocks) aovflb = 1;
	ciaatb -= ciaclocks;
    }
    
    /* CIA B timers */
    if ((ciabcra & 0x21) == 0x01) {
	assert((ciabta+1) >= ciaclocks);
	if ((ciabta+1) == ciaclocks) {
	    bovfla = 1;
	    if ((ciabcrb & 0x61) == 0x41) {
		if (ciabtb-- == 0) bovflb = 1;
	    }
	} 
	ciabta -= ciaclocks;
    }
    if ((ciabcrb & 0x61) == 0x01) {
	assert ((ciabtb+1) >= ciaclocks);
	if ((ciabtb+1) == ciaclocks) bovflb = 1;
	ciabtb -= ciaclocks;
    }
    if (aovfla) {
	ciaaicr |= 1; RethinkICRA();
	ciaata = ciaala;
	if (ciaacra & 0x8) ciaacra &= ~1;
    }
    if (aovflb) {
	ciaaicr |= 2; RethinkICRA();
	ciaatb = ciaalb;
	if (ciaacrb & 0x8) ciaacrb &= ~1;
    }
    if (bovfla) {
	ciabicr |= 1; RethinkICRB();
	ciabta = ciabla;
	if (ciabcra & 0x8) ciabcra &= ~1;
    }
    if (bovflb) {
	ciabicr |= 2; RethinkICRA();
	ciabtb = ciablb;
	if (ciabcrb & 0x8) ciabcrb &= ~1;
    }
}

static void CIA_calctimers(void)
{
    int ciaatimea = -1, ciaatimeb = -1, ciabtimea = -1, ciabtimeb = -1;

    eventtab[ev_cia].oldcycles = cycles;
    
    if ((ciaacra & 0x21) == 0x01) {
	ciaatimea = (DIV10-div10) + DIV10*ciaata;	
    }
    if ((ciaacrb & 0x61) == 0x41) {
	/* Timer B will not get any pulses if Timer A is off. */
	if (ciaatimea >= 0) {
	    /* If Timer A is in one-shot mode, and Timer B needs more than
	     * one pulse, it will not underflow. */
	    if (ciaatb == 0 || (ciaacra & 0x8) == 0) {
		/* Otherwise, we can determine the time of the underflow. */
		ciaatimeb = ciaatimea + ciaala * DIV10 * ciaatb;
	    }
	}
    }
    if ((ciaacrb & 0x61) == 0x01) {
	ciaatimeb = (DIV10-div10) + DIV10*ciaatb;
    }

    if ((ciabcra & 0x21) == 0x01) {
	ciabtimea = (DIV10-div10) + DIV10*ciabta;	
    }
    if ((ciabcrb & 0x61) == 0x41) {
	/* Timer B will not get any pulses if Timer A is off. */
	if (ciabtimea >= 0) {
	    /* If Timer A is in one-shot mode, and Timer B needs more than
	     * one pulse, it will not underflow. */
	    if (ciabtb == 0 || (ciabcra & 0x8) == 0) {
		/* Otherwise, we can determine the time of the underflow. */
		ciabtimeb = ciabtimea + ciabla * DIV10 * ciabtb;
	    }
	}
    }
    if ((ciabcrb & 0x61) == 0x01) {
	ciabtimeb = (DIV10-div10) + DIV10*ciabtb;
    }
    eventtab[ev_cia].active = (ciaatimea != -1 || ciaatimeb != -1
			       || ciabtimea != -1 || ciabtimeb != -1);
    if (eventtab[ev_cia].active) {
	unsigned long int ciatime = ~0L;
	if (ciaatimea != -1) ciatime = ciaatimea;
	if (ciaatimeb != -1 && ciaatimeb < ciatime) ciatime = ciaatimeb;
	if (ciabtimea != -1 && ciabtimea < ciatime) ciatime = ciabtimea;
	if (ciabtimeb != -1 && ciabtimeb < ciatime) ciatime = ciabtimeb;
	eventtab[ev_cia].evtime = ciatime;
    }
    events_schedule();
}

void CIA_handler(void)
{
    CIA_update();
    CIA_calctimers();
}

void CIA_hsync_handler(void)
{
    static int keytime = 0;
    
    ciabtod++;
    ciabtod &= 0xFFFFFF;
    if (ciabtod == ciabalarm) {
	ciabicr |= 4; RethinkICRB();
    }
    if (keys_available() && kback && (++keytime & 15) == 0) {
	switch(kbstate) {
	 case 0:
	    ciaasdr = (BYTE)~0xFB; /* aaarghh... stupid compiler */
	    kbstate++;
	    break;
	 case 1:
	    kbstate++;
	    ciaasdr = (BYTE)~0xFD;
	    break;
	 case 2:
	    ciaasdr = ~get_next_key();
	    break;
	}
	ciaaicr |= 8; RethinkICRA();
    }
}

void CIA_vsync_handler()
{    
    ciaatod++;
    ciaatod &= 0xFFFFFF;
    if (ciaatod == ciaaalarm) {
	ciaaicr |= 4; RethinkICRA();
    }
}

static UBYTE ReadCIAA(UWORD addr)
{
    UBYTE tmp;
    
    switch(addr & 0xf){
     case 0: 
	tmp = (DISK_status() & 0x3C);
	if (!buttonstate[0]) tmp |= 0x40;
	if (!buttonstate[1]) tmp |= 0x80;
	return tmp;
     case 1:
	return ciaaprb;
     case 2:
	return ciaadra;
     case 3:
	return ciaadrb;
     case 4:
	return ciaata & 0xff;
     case 5:
	return ciaata >> 8;
     case 6:
	return ciaatb & 0xff;
     case 7:
	return ciaatb >> 8;
     case 8:
	if (ciaatlatch) {
	    ciaatlatch = 0;
	    return ciaatol & 0xff;
	} else return ciaatod & 0xff;
     case 9:
	if (ciaatlatch) return (ciaatol >> 8) & 0xff;
	else return (ciaatod >> 8) & 0xff;
     case 10:
	ciaatlatch = 1; ciaatol = ciaatod; /* ??? only if not already latched? */
	return (ciaatol >> 16) & 0xff;
     case 12:
	return ciaasdr;
     case 13:
	tmp = ciaaicr; ciaaicr = 0; RethinkICRA(); return tmp;
     case 14:
	return ciaacra;
     case 15:
	return ciaacrb;
    }
    return 0;
}

static UBYTE ReadCIAB(UWORD addr)
{
    UBYTE tmp;
    
    switch(addr & 0xf){
     case 0: 
	return ciabpra;
     case 1:
	return ciabprb;
     case 2:
	return ciabdra;
     case 3:
	return ciabdrb;
     case 4:
	return ciabta & 0xff;
     case 5:
	return ciabta >> 8;
     case 6:
	return ciabtb & 0xff;
     case 7:
	return ciabtb >> 8;
     case 8:
	if (ciabtlatch) {
	    ciabtlatch = 0;
	    return ciabtol & 0xff;
	} else return ciabtod & 0xff;
     case 9:
	if (ciabtlatch) return (ciabtol >> 8) & 0xff;
	else return (ciabtod >> 8) & 0xff;
     case 10:
	ciabtlatch = 1; ciabtol = ciabtod;
	return (ciabtol >> 16) & 0xff;
     case 12:
	return ciabsdr;
     case 13:
	tmp = ciabicr; ciabicr = 0; RethinkICRB();
	if (!dskdmaen)
	    if (indexpulse == 0){
		tmp |= 0x10;
		DISK_Index();
		indexpulse = 100; /* whatever */
	    } else {
		indexpulse--;
	    }	    
	return tmp;
     case 14:
	return ciabcra;
     case 15:
	return ciabcrb;
    }
    return 0;
}

static void WriteCIAA(UWORD addr,UBYTE val)
{
    switch(addr & 0xf){
     case 0: 
	ciaapra = (ciaapra & ~0x3) | (val & 0x3); LED(ciaapra & 0x2); break;
     case 1:
	ciaaprb = val; break;
     case 2:
	ciaadra = val; break;
     case 3:
	ciaadrb = val; break;
     case 4:
	CIA_update();
	ciaala = (ciaala & 0xff00) | val;
	CIA_calctimers();
	break;
     case 5:
	CIA_update();
	ciaala = (ciaala & 0xff) | (val << 8);
	if ((ciaacra & 1) == 0) {
	    ciaata = ciaala;
	}
	if (ciaacra & 8) { 
	    ciaata = ciaala; 
	    ciaacra |= 1; 
	}/*??? load latch always? */
	CIA_calctimers();
	break;
     case 6:
	CIA_update();
	ciaalb = (ciaalb & 0xff00) | val;
	CIA_calctimers();
	break;
     case 7:
	CIA_update();
	ciaalb = (ciaalb & 0xff) | (val << 8);
	if ((ciaacrb & 1) == 0) ciaatb = ciaalb;
	if (ciaacrb & 8) { ciaatb = ciaalb; ciaacrb |= 1; }
	CIA_calctimers();
	break;
     case 8:
	if (ciaacrb & 0x80){
	    ciaaalarm = (ciaaalarm & ~0xff) | val;
	} else {
	    ciaatod = (ciaatod & ~0xff) | val;
	}
	break;
     case 9:
	if (ciaacrb & 0x80){
	    ciaaalarm = (ciaaalarm & ~0xff00) | (val << 8);
	} else {
	    ciaatod = (ciaatod & ~0xff00) | (val << 8);
	}
	break;
     case 10:
	if (ciaacrb & 0x80){
	    ciaaalarm = (ciaaalarm & ~0xff0000) | (val << 16);
	} else {
	    ciaatod = (ciaatod & ~0xff0000) | (val << 16);
	}
	break;
     case 12:
	ciaasdr = val; break;
     case 13:
	setclr(&ciaaimask,val); break; /* ??? call RethinkICR() ? */
     case 14:
	CIA_update();
	ciaacra = val;
	if (ciaacra & 0x10){
	    ciaacra &= ~0x10;
	    ciaata = ciaala;
	}
	if (ciaacra & 0x40) {
	    kback = true;
	}
	CIA_calctimers();
	break;
     case 15:
	CIA_update();
	ciaacrb = val; 
	if (ciaacrb & 0x10){
	    ciaacrb &= ~0x10;
	    ciaatb = ciaalb;
	}
	CIA_calctimers();
	break;
    }
}

static void WriteCIAB(UWORD addr,UBYTE val)
{
    switch(addr & 0xf){
     case 0:
	ciabpra = (ciabpra & ~0x3) | (val & 0x3); break;
     case 1:
	ciabprb = val; DISK_select(val); break;
     case 2:
	ciabdra = val; break;
     case 3:
	ciabdrb = val; break;
     case 4:
	CIA_update();
	ciabla = (ciabla & 0xff00) | val;
	CIA_calctimers();
	break;
     case 5:
	CIA_update();
	ciabla = (ciabla & 0xff) | (val << 8);
	if ((ciabcra & 1) == 0) ciabta = ciabla;
	if (ciabcra & 8) { ciabta = ciabla; ciabcra |= 1; } 
	CIA_calctimers();
	break;
     case 6:
	CIA_update();
	ciablb = (ciablb & 0xff00) | val;
	CIA_calctimers();
	break;
     case 7:
	CIA_update();
	ciablb = (ciablb & 0xff) | (val << 8);
	if ((ciabcrb & 1) == 0) ciabtb = ciablb;
	if (ciabcrb & 8) { ciabtb = ciablb; ciabcrb |= 1; }
	CIA_calctimers();
	break;
     case 8:
	if (ciabcrb & 0x80){
	    ciabalarm = (ciabalarm & ~0xff) | val;
	} else {
	    ciabtod = (ciabtod & ~0xff) | val;
	}
	break;
     case 9:
	if (ciabcrb & 0x80){
	    ciabalarm = (ciabalarm & ~0xff00) | (val << 8);
	} else {
	    ciabtod = (ciabtod & ~0xff00) | (val << 8);
	}
	break;
     case 10:
	if (ciabcrb & 0x80){
	    ciabalarm = (ciabalarm & ~0xff0000) | (val << 16);
	} else {
	    ciabtod = (ciabtod & ~0xff0000) | (val << 16);
	}
	break;
     case 12:
	ciabsdr = val; 
	break;
     case 13:
	setclr(&ciabimask,val); 
	break;
     case 14:
	CIA_update();
	ciabcra = val;
	if (ciabcra & 0x10){
	    ciabcra &= ~0x10;
	    ciabta = ciabla;
	}
	CIA_calctimers();
	break;
     case 15:
	CIA_update();
	ciabcrb = val; 
	if (ciabcrb & 0x10){
	    ciabcrb &= ~0x10;
	    ciabtb = ciablb;
	}
	CIA_calctimers();
	break;
    }
}

void CIA_reset(void)
{
    kback = true;
    kbstate = 0;
    
    ciaatlatch = ciabtlatch = 0;
    ciaatod = ciabtod = 0;
    ciaaicr = ciabicr = ciaaimask = ciabimask = 0;
    ciaacra = ciaacrb = ciabcra = ciabcrb = 0x4; /* outmode = toggle; */
    div10 = 0;
    lastdiv10 = 0;
    CIA_calctimers();
}

void dumpcia(void)
{
    printf("A: CRA: %02x, CRB: %02x, IMASK: %02x, TOD: %08lx %7s TA: %04lx, TB: %04lx\n",
	   (int)ciaacra, (int)ciaacrb, (int)ciaaimask, ciaatod, 
	   ciaatlatch ? " latched" : "", ciaata, ciaatb);
    printf("B: CRA: %02x, CRB: %02x, IMASK: %02x, TOD: %08lx %7s TA: %04lx, TB: %04lx\n",
	   (int)ciabcra, (int)ciabcrb, (int)ciabimask, ciabtod, 
	   ciabtlatch ? " latched" : "", ciabta, ciabtb);
}

/* CIA memory access */

static ULONG cia_lget(CPTR);
static UWORD cia_wget(CPTR);
static UBYTE cia_bget(CPTR);
static void  cia_lput(CPTR, ULONG);
static void  cia_wput(CPTR, UWORD);
static void  cia_bput(CPTR, UBYTE);

addrbank cia_bank = {
    cia_lget, cia_wget, cia_bget,
    cia_lput, cia_wput, cia_bput,
    default_xlate, default_check
};

ULONG cia_lget(CPTR addr)
{
    return cia_bget(addr+3);
}

UWORD cia_wget(CPTR addr)
{
    return cia_bget(addr+1);
}

UBYTE cia_bget(CPTR addr)
{
#ifdef DUALCPU
    customacc = true;
#endif
    if ((addr & 0xF0FF) == 0xE001)
    	return ReadCIAA((addr & 0xF00) >> 8);
    if ((addr & 0xF0FF) == 0xD000)
    	return ReadCIAB((addr & 0xF00) >> 8);
    return 0;
}

void cia_lput(CPTR addr, ULONG value)
{
    cia_bput(addr+3,value); /* FIXME ? */
}

void cia_wput(CPTR addr, UWORD value)
{
    cia_bput(addr+1,value);
}

void cia_bput(CPTR addr, UBYTE value)
{
#ifdef DUALCPU
    customacc = true;
#endif
    if ((addr & 0xF0FF) == 0xE001)
    	WriteCIAA((addr & 0xF00) >> 8,value);
    if ((addr & 0xF0FF) == 0xD000)
    	WriteCIAB((addr & 0xF00) >> 8,value);
}

/* battclock memory access */

static ULONG clock_lget(CPTR);
static UWORD clock_wget(CPTR);
static UBYTE clock_bget(CPTR);
static void  clock_lput(CPTR, ULONG);
static void  clock_wput(CPTR, UWORD);
static void  clock_bput(CPTR, UBYTE);

addrbank clock_bank = {
    clock_lget, clock_wget, clock_bget,
    clock_lput, clock_wput, clock_bput,
    default_xlate, default_check
};

ULONG clock_lget(CPTR addr)
{
    return clock_bget(addr+3);
}

UWORD clock_wget(CPTR addr)
{
    return clock_bget(addr+1);
}

UBYTE clock_bget(CPTR addr)
{
#ifdef DUALCPU
    customacc = true;
#endif
    
    time_t t=time(0);
    struct tm *ct;
    ct=localtime(&t);
    switch (addr & 0x3f)
    {
     case 0x03: return ct->tm_sec % 10;
     case 0x07: return ct->tm_sec / 10;
     case 0x0b: return ct->tm_min % 10;
     case 0x0f: return ct->tm_min / 10;
     case 0x13: return ct->tm_hour % 10;
     case 0x17: return ct->tm_hour / 10;
     case 0x1b: return ct->tm_mday % 10;
     case 0x1f: return ct->tm_mday / 10;
     case 0x23: return (ct->tm_mon+1) % 10;
     case 0x27: return (ct->tm_mon+1) / 10;
     case 0x2b: return ct->tm_year % 10;
     case 0x2f: return ct->tm_year / 10;
    }
    return 0;
}

void clock_lput(CPTR addr, ULONG value)
{
    /* No way */
}

void clock_wput(CPTR addr, UWORD value)
{
    /* No way */
}

void clock_bput(CPTR addr, UBYTE value)
{
    /* No way */
}
