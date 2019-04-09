 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * AutoConfig devices
  *
  * (c) 1995 Bernd Schmidt
  * (c) 1996 Ed Hanway
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "disk.h"
#include "xwin.h"
#include "autoconf.h"
#include "filesys.h"
#include "hardfile.h"

/* Commonly used autoconfig strings */

ULONG explibname;

/* ROM tag area memory access */

static UWORD rtarea[32768];

static ULONG rtarea_lget(CPTR) REGPARAM;
static UWORD rtarea_wget(CPTR) REGPARAM;
static UBYTE rtarea_bget(CPTR) REGPARAM;
static void  rtarea_lput(CPTR, ULONG) REGPARAM;
static void  rtarea_wput(CPTR, UWORD) REGPARAM;
static void  rtarea_bput(CPTR, UBYTE) REGPARAM;
static UWORD *rtarea_xlate(CPTR) REGPARAM;

addrbank rtarea_bank = {
    rtarea_lget, rtarea_wget, rtarea_bget,
    rtarea_lput, rtarea_wput, rtarea_bput,
    rtarea_xlate, default_check
};

UWORD *rtarea_xlate(CPTR addr)
{
    addr &= 0xFFFF;
    return rtarea + (addr >> 1);
}

ULONG rtarea_lget(CPTR addr)
{
    addr &= 0xFFFF;
    return (ULONG)(rtarea_wget(addr) << 16) + rtarea_wget(addr+2);
}

UWORD rtarea_wget(CPTR addr)
{
    addr &= 0xFFFF;
    return rtarea[addr >> 1];
}

UBYTE rtarea_bget(CPTR addr)
{
    UWORD data;
    addr &= 0xFFFF;
    data = rtarea[addr >> 1];
    return addr & 1 ? data : data >> 8;
}

void rtarea_lput(CPTR addr, ULONG value) { }
void rtarea_bput(CPTR addr, UBYTE value) { }

/* Don't start at 0 -- can get bogus writes there. */
static ULONG trap_base_addr = 0x00F00180; 

TrapFunction traps[256];
static int max_trap = 0;

void rtarea_wput(CPTR addr, UWORD value) 
{
    /* Save all registers */
    struct regstruct backup_regs = regs;
    ULONG retval = 0;

    ULONG func = ((addr  - trap_base_addr) & 0xFFFF) >> 1;
    if(func < max_trap) {
	retval = (*traps[func])();
    } else {
	fprintf(stderr, "illegal emulator trap\n");
    }
    regs = backup_regs;
    regs.d[0] = retval;
}

/* some quick & dirty code to fill in the rt area and save me a lot of
 * scratch paper
 */

static int rt_addr = 0;
static int rt_straddr = 0xF000/2 - 1;

ULONG
addr(int ptr)
{
    return ((ULONG)ptr << 1) + 0x00F00000;
}

void
dw(UWORD data)
{
    rtarea[rt_addr++] = data;
}

void
dl(ULONG data)
{
    rtarea[rt_addr++] = data >> 16;
    rtarea[rt_addr++] = data;
}

/* store strings starting at the end of the rt area and working
 * backward.  store pointer at current address
 */

ULONG
ds(char *str)
{
    UWORD data;
    char c;
    int start;

    int len = (strlen(str) + 1) >> 1;
    rt_straddr -= len;
    start = rt_straddr;
    
    do {
	data = c = *str++;
	if (c) {
	    data <<= 8;
	    c = *str++;
	    data |= c;
	}
	rtarea[start++] = data;
    } while (c);
    
    return addr(rt_straddr--);
}

void
calltrap(ULONG n)
{
    dw(0x33C0);	/* MOVE.W D0,abs32 */
    dl(n*2 + trap_base_addr);
}

void
org(ULONG a)
{
    rt_addr = (a - 0x00F00000) >> 1;
}

ULONG
here(void)
{
    return addr(rt_addr);
}

int
deftrap(TrapFunction func)
{
    int num = max_trap++;
    traps[num] = func;
    return num;
}

void
align(int b)
{
    rt_addr = (rt_addr + (b-1)) & ~(b-1);
}

ULONG CallLib(CPTR base, WORD offset)
{
    /* Make tracing through this possible */
    int oldspc = specialflags & SPCFLAG_BRK;
    
    CPTR olda6 = regs.a[6];
    CPTR oldpc = m68k_getpc();
    regs.a[6] = base;
    regs.a[7] -= 4;
    put_long (regs.a[7], 0xF0FFFE);
    m68k_setpc (base + offset);
    MC68000_skip (0xF0FFFE);
    regs.a[6] = olda6;
    m68k_setpc (oldpc);
    specialflags |= oldspc;
    return regs.d[0];
}

void
rtarea_init()
{
    explibname = ds("expansion.library");

    hardfile_install();
//    filesys_install(); Not Implemented in Mac yet.
}
