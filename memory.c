 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Memory management
  *
  * (c) 1995 Bernd Schmidt
  */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "cia.h"
#include "ersatz.h"

bool buserr;

const char romfile[] = "kick.rom"; 

lget_func do_lget[256];
wget_func do_wget[256];
bget_func do_bget[256];
lput_func do_lput[256];
wput_func do_wput[256];
bput_func do_bput[256];
xlate_func do_xlateaddr[256];
check_func do_check[256];

/* Default memory access functions */

bool default_check(CPTR a, ULONG b)
{
    return false;
}

UWORD *default_xlate(CPTR a)
{
    fprintf(stderr, "Your Amiga program just did something terribly stupid\n");
    return 0;
}

/* Chip memory */

static UWORD chipmemory[chipmem_size/2];

static ULONG chipmem_lget(CPTR);
static UWORD chipmem_wget(CPTR);
static UBYTE chipmem_bget(CPTR);
static void  chipmem_lput(CPTR, ULONG);
static void  chipmem_wput(CPTR, UWORD);
static void  chipmem_bput(CPTR, UBYTE);
static bool  chipmem_check(CPTR addr, ULONG size);
static UWORD *chipmem_xlate(CPTR addr);

ULONG chipmem_lget(CPTR addr)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    return ((ULONG)chipmemory[addr >> 1] << 16) | chipmemory[(addr >> 1)+1];
}

UWORD chipmem_wget(CPTR addr)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    return chipmemory[addr >> 1];
}

UBYTE chipmem_bget(CPTR addr)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    return chipmemory[addr >> 1] >> (addr & 1 ? 0 : 8);
}

void chipmem_lput(CPTR addr, ULONG l)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    chipmemory[addr >> 1] = l >> 16;
    chipmemory[(addr >> 1)+1] = (UWORD)l;
}

void chipmem_wput(CPTR addr, UWORD w)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    chipmemory[addr >> 1] = w;
}

void chipmem_bput(CPTR addr, UBYTE b)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    if (!(addr & 1)) {
	chipmemory[addr>>1] = (chipmemory[addr>>1] & 0xff) | (((UWORD)b) << 8);
    } else {
	chipmemory[addr>>1] = (chipmemory[addr>>1] & 0xff00) | b;
    }
}

bool chipmem_check(CPTR addr, ULONG size)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    return (addr + size) < chipmem_size;
}

UWORD *chipmem_xlate(CPTR addr)
{
    addr -= chipmem_start & (chipmem_size-1);
    addr &= chipmem_size-1;
    return chipmemory + (addr >> 1);
}

/* Slow memory */

static UWORD bogomemory[bogomem_size/2];

static ULONG bogomem_lget(CPTR);
static UWORD bogomem_wget(CPTR);
static UBYTE bogomem_bget(CPTR);
static void  bogomem_lput(CPTR, ULONG);
static void  bogomem_wput(CPTR, UWORD);
static void  bogomem_bput(CPTR, UBYTE);
static bool  bogomem_check(CPTR addr, ULONG size);
static UWORD *bogomem_xlate(CPTR addr);

ULONG bogomem_lget(CPTR addr)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    return ((ULONG)bogomemory[addr >> 1] << 16) | bogomemory[(addr >> 1)+1];
}

UWORD bogomem_wget(CPTR addr)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    return bogomemory[addr >> 1];
}

UBYTE bogomem_bget(CPTR addr)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    return bogomemory[addr >> 1] >> (addr & 1 ? 0 : 8);
}

void bogomem_lput(CPTR addr, ULONG l)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    bogomemory[addr >> 1] = l >> 16;
    bogomemory[(addr >> 1)+1] = (UWORD)l;
}

void bogomem_wput(CPTR addr, UWORD w)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    bogomemory[addr >> 1] = w;
}

void bogomem_bput(CPTR addr, UBYTE b)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    if (!(addr & 1)) {
	bogomemory[addr>>1] = (bogomemory[addr>>1] & 0xff) | (((UWORD)b) << 8);
    } else {
	bogomemory[addr>>1] = (bogomemory[addr>>1] & 0xff00) | b;
    }
}

bool bogomem_check(CPTR addr, ULONG size)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    return (addr + size) < bogomem_size;
}

UWORD *bogomem_xlate(CPTR addr)
{
    addr -= bogomem_start & (bogomem_size-1);
    addr &= bogomem_size-1;
    return bogomemory + (addr >> 1);
}

/* Kick memory */

static UWORD kickmemory[kickmem_size/2];

static ULONG kickmem_lget(CPTR);
static UWORD kickmem_wget(CPTR);
static UBYTE kickmem_bget(CPTR);
static void  kickmem_lput(CPTR, ULONG);
static void  kickmem_wput(CPTR, UWORD);
static void  kickmem_bput(CPTR, UBYTE);
static bool  kickmem_check(CPTR addr, ULONG size);
static UWORD *kickmem_xlate(CPTR addr);

ULONG kickmem_lget(CPTR addr)
{
    addr -= kickmem_start & (kickmem_size-1);
    addr &= kickmem_size-1;
    return ((ULONG)kickmemory[addr >> 1] << 16) | kickmemory[(addr >> 1)+1];
}

UWORD kickmem_wget(CPTR addr)
{
    addr -= kickmem_start & (kickmem_size-1);
    addr &= kickmem_size-1;
    return kickmemory[addr >> 1];
}

UBYTE kickmem_bget(CPTR addr)
{
    addr -= kickmem_start & (kickmem_size-1);
    addr &= kickmem_size-1;
    return kickmemory[addr >> 1] >> (addr & 1 ? 0 : 8);
}

void kickmem_lput(CPTR a, ULONG b)
{
}

void kickmem_wput(CPTR a, UWORD b)
{
}

void kickmem_bput(CPTR a, UBYTE b)
{
}

bool kickmem_check(CPTR addr, ULONG size)
{
    addr -= kickmem_start & (kickmem_size-1);
    addr &= kickmem_size-1;
    return (addr + size) < kickmem_size;
}

UWORD *kickmem_xlate(CPTR addr)
{
    addr -= kickmem_start & (kickmem_size-1);
    addr &= kickmem_size-1;
    return kickmemory + (addr >> 1);
}

static bool load_kickstart(void)
{
    int i;
    ULONG cksum = 0, prevck = 0;
    
    FILE *f = fopen(romfile, "rb");
    
    if (f == NULL) {	
    	fprintf(stderr, "No Kickstart ROM found.\n");
	return false;
    }
    
    for(i = 0; i < kickmem_size/2; i++) {
	unsigned char buffer[2];
	if (fread(buffer, 1, 2, f) < 2) {
	    if (feof(f) && i == kickmem_size/4) {
		fprintf(stderr, "Warning: Kickstart is only 256K.\n");
#ifndef __mac__
		memcpy (kickmemory + kickmem_size/4, kickmemory, kickmem_size/4);
#else
		memcpy (kickmemory + kickmem_size/4, kickmemory, kickmem_size/2);
#endif
		break;
	    } else {
		fprintf(stderr, "Error while reading Kickstart.\n");
		return false;
	    }
	}
	kickmemory[i] = buffer[0]*256 + buffer[1];
    }
    fclose (f);
    
    for (i = 0; i < kickmem_size/4; i++) {
	ULONG data = kickmemory[i*2]*65536 + kickmemory[i*2+1];
	cksum += data;
	if (cksum < prevck)
	    cksum++;
	prevck = cksum;
    }
    if (cksum != 0xFFFFFFFF) {
	fprintf(stderr, "Warning: Kickstart checksum incorrect. You probably have a corrupted ROM image.\n");
    }
    return true;
}

/* Address banks */

addrbank chipmem_bank = {
    chipmem_lget, chipmem_wget, chipmem_bget,
    chipmem_lput, chipmem_wput, chipmem_bput,
    chipmem_xlate, chipmem_check
};

addrbank bogomem_bank = {
    bogomem_lget, bogomem_wget, bogomem_bget,
    bogomem_lput, bogomem_wput, bogomem_bput,
    bogomem_xlate, bogomem_check
};

addrbank kickmem_bank = {
    kickmem_lget, kickmem_wget, kickmem_bget,
    kickmem_lput, kickmem_wput, kickmem_bput,
    kickmem_xlate, kickmem_check
};


#ifdef DUALCPU
ULONG mempattern;
bool incpu;
struct memreads {
    CPTR addr;
    ULONG value;
} readlog[200];

int memlogptr = 0;

ULONG get_long(CPTR addr)
{
    ULONG result;
    mempattern ^= addr ^ 0x99999999;
    if (allowmem || !incpu) {
    	result = longget(addr);
	if (incpu) {	    
	    readlog[memlogptr].addr = addr;
	    readlog[memlogptr++].value = result;
	}
    } else {	
	for(int i=0;i<memlogptr;i++) {
	    if (readlog[i].addr == addr) {
		result = readlog[i].value;
	    }
	}
    }
    return result;
}

UWORD get_word(CPTR addr)
{
    UWORD result;
    mempattern ^= addr ^ 0xAAAAAAAA;
    if (allowmem || !incpu) {
    	result = wordget(addr);
	if (incpu) {	    
	    readlog[memlogptr].addr = addr;
	    readlog[memlogptr++].value = result;
	}
    } else {	
	for(int i=0;i<memlogptr;i++) {
	    if (readlog[i].addr == addr) {
		result = readlog[i].value;
	    }
	}
    }
    return result;
}

UBYTE get_byte(CPTR addr)
{
    UBYTE result;
    mempattern ^= addr ^ 0x55555555;
    if (allowmem || !incpu) {
	result = byteget(addr);
	if (incpu) {	    
	    readlog[memlogptr].addr = addr;
	    readlog[memlogptr++].value = result;
	}
    } else {	
	for(int i=0;i<memlogptr;i++) {
	    if (readlog[i].addr == addr) {
		result = readlog[i].value;
	    }
	}
    }
    return result;
}

void put_long(CPTR addr, ULONG l)
{
    mempattern ^= addr ^ l ^ 0x09090909;
    longput(addr,l);
}

void put_word(CPTR addr, UWORD w)
{
    mempattern ^= addr ^ w ^ 0x0A0A0A0A;
    wordput(addr,w);
}

void put_byte(CPTR addr, UBYTE b)
{
    mempattern ^= addr ^ b ^ 0x05050505;
    byteput(addr,b);
}

#endif

void memory_init(void)
{
#ifdef DUALCPU
    allowmem = true;
#endif
    buserr = false;
    
    map_banks(chipmem_bank, 0, 256);
    
    map_banks(custom_bank, 0xC0, 0x20);    

    if (use_slow_mem && bogomem_size > 0) {
    	map_banks(bogomem_bank, 0xC0, bogomem_size >> 16);
    }
#if 0
    if (fastmem_size > 0)
    	map_banks(expmem_config_bank, 0xE8, 1);
#endif
    map_banks(kickmem_bank, 0xF8, 8);
    map_banks(rtarea_bank, 0xF0, 1); rtarea_init ();
    map_banks(cia_bank, 0xBF, 1);
    map_banks(clock_bank, 0xDC, 1);

    if (!load_kickstart()) {
	init_ersatz_rom(kickmemory);
    }
}

void map_banks(addrbank bank, int start, int size)
{
    int bnr;
    for(bnr=start; bnr < start+size; bnr++) {
	do_lget[bnr] = bank.lget;
	do_wget[bnr] = bank.wget;
	do_bget[bnr] = bank.bget;
	do_lput[bnr] = bank.lput;
	do_wput[bnr] = bank.wput;
	do_bput[bnr] = bank.bput;
	do_xlateaddr[bnr] = bank.xlateaddr;
	do_check[bnr] = bank.check;
    }
}
