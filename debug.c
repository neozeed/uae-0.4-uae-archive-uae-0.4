 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Debugger
  * 
  * (c) 1995 Bernd Schmidt
  * 
  */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "debug.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "cia.h"
#include "xwin.h"

#ifdef __cplusplus
static void sigbrkhandler(...)
#else
static void sigbrkhandler(int foo)
#endif
{
    broken_in = true;
    specialflags |= SPCFLAG_BRK;
    signal(SIGINT, sigbrkhandler);
}

static void ignore_ws(char **c)
{
    while (**c && isspace(**c)) (*c)++;
}

static ULONG readhex(char **c)
{
    ULONG val = 0;
    char nc;

    ignore_ws(c);
    
    while (isxdigit(nc = **c)){
	(*c)++;
	val *= 16;
	nc = toupper(nc);
	if (isdigit(nc)) {
	    val += nc - '0';
	} else {
	    val += nc - 'A' + 10;
	}
    }
    return val;
}

static char next_char(char **c)
{
    ignore_ws(c);
    return *(*c)++;
}

static bool more_params(char **c)
{
    ignore_ws(c);
    return (**c) != 0;
}

static void dumpmem(CPTR addr, CPTR *nxmem, int lines)
{
    broken_in = false;
    for (;lines-- && !broken_in;){
	int i;
	printf("%08lx ", addr);
	for(i=0; i< 16; i++) {
	    printf("%04x ", get_word(addr)); addr += 2;
	}
	printf("\n");
    }
    *nxmem = addr;
}

void debug(void)
{
    char input[80],c;
    CPTR nextpc,nxdis,nxmem;
    
    if (debuggable())
    	signal(SIGINT, sigbrkhandler);
    
    if (!debuggable() || !use_debugger) {
	MC68000_run();
	if (!debuggable()) {	    
	    dumpcustom();
	    return;
	}
    }
    printf("debugging...\n");
    MC68000_dumpstate(&nextpc);
    nxdis = nextpc; nxmem = 0;
    for(;;){
	char cmd,*inptr;
	
	bogusframe = true;
	printf(">");
	fgets(input, 80, stdin);
	inptr = input;
	cmd = next_char(&inptr);
	switch(cmd){
	 case 'q': 
	    return;
	 case 'c':
	    dumpcia(); dumpcustom(); 
	    break;
	 case 'r':
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'd': 
	    {
		ULONG daddr;
		int count;
		
		if (more_params(&inptr))
		    daddr = readhex(&inptr);
		else 
		    daddr = nxdis;
		if (more_params(&inptr))
		    count = readhex(&inptr); 
		else
		    count = 10;
		MC68000_disasm(daddr, &nxdis, count);
	    }
	    break;
	 case 't': 
	    MC68000_step(); 
	    MC68000_dumpstate(&nextpc); 
	    break;
	 case 'z': 
	    MC68000_skip(nextpc);
	    MC68000_dumpstate(&nextpc);
	    nxdis = nextpc; 
	    break;
	 case 'f': 
	    MC68000_skip(readhex(&inptr));
	    MC68000_dumpstate(&nextpc); 
	    break;
	 case 'g':
	    if (more_params (&inptr))
		m68k_setpc (readhex (&inptr));
	    MC68000_run();
	    MC68000_dumpstate(&nextpc);
	    break;
	 case 'm':
	    {
		ULONG maddr; int lines;
		if (more_params(&inptr))
		    maddr = readhex(&inptr); 
		else 
		    maddr = nxmem;
		if (more_params(&inptr))
		    lines = readhex(&inptr); 
		else 
		    lines = 16;
		dumpmem(maddr, &nxmem, lines);
	    }
	    break;
	}
    }
}
