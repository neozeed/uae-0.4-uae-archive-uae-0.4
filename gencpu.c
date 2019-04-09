/* 
 * UAE - The Un*x Amiga Emulator
 *
 * MC68000 emulation generator
 *
 * This is a fairly stupid program that generates a lot of case labels that 
 * can be #included in a switch statement.
 * As an alternative, it can generate functions that handle specific
 * MC68000 instructions, plus a prototype header file and a function pointer
 * array to look up the function for an opcode.
 * Error checking is bad, an illegal table68k file will cause the program to
 * call abort().
 * The generated code is sometimes sub-optimal, an optimizing compiler should 
 * take care of this.
 * 
 * (c) 1995 Bernd Schmidt
 * 
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "config.h"
#include "amiga.h"

typedef enum {
  Dreg, Areg, Aind, Aipi, Apdi, Ad16, Ad8r, 
  absw, absl, PC16, PC8r, imm, imm0, imm1, imm2, immi, am_unknown, am_illg
} amodes;

FILE *tablef;
int nextch = 0;

typedef enum {
    i_ILLG,
    
    i_OR, i_AND, i_EOR, i_ORSR, i_ANDSR, i_EORSR, 
    i_SUB, i_SUBA, i_SUBX, i_SBCD, 
    i_ADD, i_ADDA, i_ADDX, i_ABCD,
    i_NEG, i_NEGX, i_NBCD, i_CLR, i_NOT, i_TST, 
    i_BTST, i_BCHG, i_BCLR, i_BSET,
    i_CMP, i_CMPM, i_CMPA, 
    i_MOVEP, i_MOVE, i_MOVEA, i_MVSR2, i_MV2SR, 
    i_SWAP, i_EXG, i_EXT, i_MVMEL, i_MVMLE, 
    i_TRAP, i_MVR2USP, i_MVUSP2R, i_RESET, i_NOP, i_STOP, i_RTE, i_RTD, 
    i_LINK, i_UNLK, 
    i_RTS, i_TRAPV, i_RTR, 
    i_JSR, i_JMP, i_BSR, i_Bcc,
    i_LEA, i_PEA, i_DBcc, i_Scc, 
    i_DIVU, i_DIVS, i_MULU, i_MULS, 
    i_ASR, i_ASL, i_LSR, i_LSL, i_ROL, i_ROR, i_ROXL, i_ROXR,
    i_ASRW, i_ASLW, i_LSRW, i_LSLW, i_ROLW, i_RORW, i_ROXLW, i_ROXRW
} instrmnem;

struct mnemolookup {
    instrmnem mnemo;
    char name[10];
} lookuptab[] = {
    { i_OR, "OR" },
    { i_AND, "AND" }, 
    { i_EOR, "EOR" },
    { i_ORSR, "ORSR" }, 
    { i_ANDSR, "ANDSR" }, 
    { i_EORSR, "EORSR" }, 
    { i_SUB, "SUB" }, 
    { i_SUBA, "SUBA" }, 
    { i_SUBX, "SUBX" }, 
    { i_SBCD, "SBCD" }, 
    { i_ADD, "ADD" }, 
    { i_ADDA, "ADDA" }, 
    { i_ADDX, "ADDX" }, 
    { i_ABCD, "ABCD" },
    { i_NEG, "NEG" }, 
    { i_NEGX, "NEGX" }, 
    { i_NBCD, "NBCD" }, 
    { i_CLR, "CLR" }, 
    { i_NOT, "NOT" }, 
    { i_TST, "TST" }, 
    { i_BTST, "BTST" }, 
    { i_BCHG, "BCHG" }, 
    { i_BCLR, "BCLR" }, 
    { i_BSET, "BSET" },
    { i_CMP, "CMP" }, 
    { i_CMPM, "CMPM" }, 
    { i_CMPA, "CMPA" }, 
    { i_MOVEP, "MOVEP" },
    { i_MOVE, "MOVE" }, 
    { i_MOVEA, "MOVEA" },
    { i_MVSR2, "MVSR2" },
    { i_MV2SR, "MV2SR" }, 
    { i_SWAP, "SWAP" },
    { i_EXG, "EXG" },
    { i_EXT, "EXT" },
    { i_MVMEL, "MVMEL" }, 
    { i_MVMLE, "MVMLE" }, 
    { i_TRAP, "TRAP" },
    { i_MVR2USP, "MVR2USP" }, 
    { i_MVUSP2R, "MVUSP2R" }, 
    { i_RESET, "RESET" },
    { i_NOP, "NOP" },
    { i_STOP, "STOP" },
    { i_RTE, "RTE" },
    { i_RTD, "RTD" }, 
    { i_LINK, "LINK" },
    { i_UNLK, "UNLK" }, 
    { i_RTS, "RTS" }, 
    { i_TRAPV, "TRAPV" }, 
    { i_RTR, "RTR" }, 
    { i_JSR, "JSR" },
    { i_JMP, "JMP" },
    { i_BSR, "BSR" },
    { i_Bcc, "Bcc" },
    { i_LEA, "LEA" },
    { i_PEA, "PEA" },
    { i_DBcc, "DBcc" }, 
    { i_Scc, "Scc" }, 
    { i_DIVU, "DIVU" }, 
    { i_DIVS, "DIVS" },
    { i_MULU, "MULU" },
    { i_MULS, "MULS" }, 
    { i_ASR, "ASR" },
    { i_ASL, "ASL" },
    { i_LSR, "LSR" },
    { i_LSL, "LSL" },
    { i_ROL, "ROL" },
    { i_ROR, "ROR" },
    { i_ROXL, "ROXL" },
    { i_ROXR, "ROXR" },
    { i_ASRW, "ASRW" },
    { i_ASLW, "ASLW" },
    { i_LSRW, "LSRW" },
    { i_LSLW, "LSLW" },
    { i_ROLW, "ROLW" },
    { i_RORW, "RORW" },
    { i_ROXLW, "ROXLW" },
    { i_ROXRW, "ROXRW" }
};

char patbits[16];
char opcstr[256];

UWORD bitmask,bitpattern;

typedef enum {
    bit0, bit1, bitc, bitC, bitf, biti, bitI, bitj, bitJ, bits, bitS, bitd, bitD, bitr, bitR, bitz, lastbit
} bitvals;

typedef enum {
    sz_unknown, sz_byte, sz_word, sz_long
} wordsizes;

struct instr {
    instrmnem mnemo;
    wordsizes size;
    amodes dmode, smode;
    unsigned char dreg;
    unsigned char sreg;
    unsigned int suse:1;
    unsigned int duse:1;
    unsigned int sbtarg:1;
    unsigned int generated:1;
    unsigned int cc:4;
    signed char dpos;
    signed char spos;
    UWORD handler;
} *table;

#ifdef DEBUG
static void myabort(void)
{
    abort();
}
#else 
#define myabort abort
#endif
static void getnextch(void)
{
    do {
	nextch = fgetc(tablef);
	if (nextch == '%') {
	    do {
		nextch = fgetc(tablef);
	    } while (nextch != EOF && nextch != '\n');	
	}
    } while (nextch != EOF && isspace(nextch));	
}

static void get_bits(void)
{
    int i;
    
    bitmask = bitpattern = 0;
    for(i=0; i<16; i++) {
	bitmask <<= 1;
	bitpattern <<= 1;
	if (nextch == '0' || nextch == '1') bitmask |= 1;
	if (nextch == '1') bitpattern |= 1;
	patbits[i] = nextch;
	getnextch();
    }
}

static amodes mode_from_str(char *str)
{
    if (strncmp(str,"Dreg",4) == 0) return Dreg;
    if (strncmp(str,"Areg",4) == 0) return Areg;
    if (strncmp(str,"Aind",4) == 0) return Aind;
    if (strncmp(str,"Apdi",4) == 0) return Apdi;
    if (strncmp(str,"Aipi",4) == 0) return Aipi;
    if (strncmp(str,"Ad16",4) == 0) return Ad16;
    if (strncmp(str,"Ad8r",4) == 0) return Ad8r;
    if (strncmp(str,"absw",4) == 0) return absw;
    if (strncmp(str,"absl",4) == 0) return absl;
    if (strncmp(str,"PC16",4) == 0) return PC16;
    if (strncmp(str,"PC8r",4) == 0) return PC8r;
    if (strncmp(str,"Immd",4) == 0) return imm;
    myabort();
}

static amodes mode_from_mr(int mode, int reg) 
{
    switch(mode) {
     case 0: return Dreg; 
     case 1: return Areg;
     case 2: return Aind;
     case 3: return Aipi;
     case 4: return Apdi;
     case 5: return Ad16;
     case 6: return Ad8r;
     case 7: 
	switch(reg) {
	 case 0: return absw;
	 case 1: return absl;
	 case 2: return PC16;
	 case 3: return PC8r;
	 case 4: return imm;
	 case 5:
	 case 6:
	 case 7: return am_illg;
	}
    }
    myabort();
}

static void get_strs(void)
{
    fgets(opcstr, 250, tablef);
    getnextch();
}

static void parse_bits(void)
{
    int find;
    long int opc;
    
    for(opc = 0; opc < 65536; opc ++) {
	if ((opc & bitmask) == bitpattern) {
	    int bitcnt[lastbit];
	    int bitval[lastbit];
	    int bitpos[lastbit];
	    int i,j;
	    
	    UWORD msk = 0x8000;

	    int pos = 0;

	    int mnp = 0;
	    char mnemonic[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	    wordsizes sz = sz_unknown;
	    bool srcgather = false, dstgather = false;
	    bool usesrc = false, usedst = false;
	    bool srcbtarg = false;
	    int srcpos, dstpos;
	    
	    amodes srcmode = am_unknown, destmode = am_unknown;
	    int srcreg, destreg;
	    
	    for(i=0; i < lastbit; i++) bitcnt[i] = bitval[i] = 0;

	    for(j=0; j < 16; j++, msk >>= 1) {
		bitvals currbit;
		switch(patbits[j]) {
		 case '0': currbit = bit0; break;
		 case '1': currbit = bit1; break;
		 case 'c': currbit = bitc; break;
		 case 'C': currbit = bitC; break;
		 case 'f': currbit = bitf; break;
		 case 'i': currbit = biti; break;
		 case 'I': currbit = bitI; break;
		 case 'j': currbit = bitj; break;
		 case 'J': currbit = bitJ; break;
		 case 's': currbit = bits; break;
		 case 'S': currbit = bitS; break;
		 case 'd': currbit = bitd; break;
		 case 'D': currbit = bitD; break;
		 case 'r': currbit = bitr; break;
		 case 'R': currbit = bitR; break;
		 case 'z': currbit = bitz; break;
		 default: myabort();
		}
		bitpos[currbit] = 15 - j;
		bitcnt[currbit]++;
		bitval[currbit] = (bitval[currbit] << 1) | ((opc & msk) ? 1 : 0);
	    }
	    if (bitval[bitj] == 0) bitval[bitj] = 8;
	    /* first check whether this one does not match after all */
	    if (bitval[bitz] == 3) continue; 
	    if (bitval[bitC] == 1) continue;
	    if (bitcnt[bitI] && bitval[bitI] == 0) continue;

	    /* bitI and bitC get copied to biti and bitc */
	    if (bitcnt[bitI]) { bitval[biti] = bitval[bitI]; bitpos[biti] = bitpos[bitI]; }
	    if (bitcnt[bitC]) bitval[bitc] = bitval[bitC];
	    
	    while (isspace(opcstr[pos]) || opcstr[pos] == ':') pos++;
	    while (!isspace(opcstr[pos])) {
		if (opcstr[pos] == '.') {
		    pos++;
		    switch(opcstr[pos]) {
			
		     case 'B': sz = sz_byte; break;
		     case 'W': sz = sz_word; break;
		     case 'L': sz = sz_long; break;
		     case 'z': 
			switch(bitval[bitz]) {
			 case 0: sz = sz_byte; break;
			 case 1: sz = sz_word; break;
			 case 2: sz = sz_long; break;
			 default: myabort();
			}
			break;
		     default: myabort();
		    }
		} else {		    
		    mnemonic[mnp] = opcstr[pos];
		    if(mnemonic[mnp] == 'f') {
			switch(bitval[bitf]) {
			 case 0: mnemonic[mnp] = 'R'; break;
			 case 1: mnemonic[mnp] = 'L'; break;
			 default: myabort();
			}
		    }
		    mnp++;
		}
		pos++;		
	    }
	    
	    /* now, we have read the mnemonic and the size */
	    while (isspace(opcstr[pos])) pos++;
	    
	    /* A goto a day keeps the D******a away. */
	    if (opcstr[pos] == 0 || opcstr[pos] == ':') goto endofline;
	    
	    /* parse the source address */
	    usesrc = true;
	    switch(opcstr[pos++]) {
	     case 'D': 
		srcmode = Dreg;
		switch (opcstr[pos++]) {
		 case 'r': srcreg = bitval[bitr]; srcgather = true; srcpos = bitpos[bitr]; break;
		 case 'R': srcreg = bitval[bitR]; srcgather = true; srcpos = bitpos[bitR]; break;
		 default: myabort();
		}
		
		break;
	     case 'A': 
		srcmode = Areg;
		switch (opcstr[pos++]) {
		 case 'r': srcreg = bitval[bitr]; srcgather = true; srcpos = bitpos[bitr]; break;
		 case 'R': srcreg = bitval[bitR]; srcgather = true; srcpos = bitpos[bitR]; break;
		 default: myabort();
		}
		switch (opcstr[pos]) {
		 case 'p': srcmode = Apdi; pos++; break;
		 case 'P': srcmode = Aipi; pos++; break;
		}
		break;
	     case '#':
		switch(opcstr[pos++]) {
		 case 'z': srcmode = imm; break;
		 case '0': srcmode = imm0; break;
		 case '1': srcmode = imm1; break;
		 case '2': srcmode = imm2; break;
		 case 'i': srcmode = immi; srcreg = (LONG)(BYTE)bitval[biti]; srcbtarg = true; srcgather = true; srcpos = bitpos[biti]; break;
		 case 'j': srcmode = immi; srcreg = bitval[bitj]; break;
		 case 'J': srcmode = immi; srcreg = bitval[bitJ]; break;
		 default: myabort();
		}
		break;
	     case 'd':
		srcreg = bitval[bitD];
		srcmode = mode_from_mr(bitval[bitd],bitval[bitD]);
		if (srcmode == am_illg) continue;
		if (srcmode == Areg || srcmode == Dreg || srcmode == Aind 
		    || srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
		    || srcmode == Apdi)
		{
		    srcgather = true; srcpos = bitpos[bitD];
		}
		if (opcstr[pos] == '[') {
		    pos++;
		    if (opcstr[pos] == '!') {
			/* exclusion */
			do {			    
			    pos++;
			    if (mode_from_str(opcstr+pos) == srcmode) goto nomatch;
			    pos += 4;			    
			} while (opcstr[pos] == ',');
			pos++;
		    } else {
			if (opcstr[pos+4] == '-') {
			    /* replacement */
			    if (mode_from_str(opcstr+pos) == srcmode) 
			    	srcmode = mode_from_str(opcstr+pos+5);
			    else
			    	goto nomatch;
			    pos += 10;
			} else {
			    /* normal */
			    while(mode_from_str(opcstr+pos) != srcmode) {
				pos += 4;
				if (opcstr[pos] == ']') goto nomatch;
				pos++;
			    }
			    while(opcstr[pos] != ']') pos++;
			    pos++;
			    break;
			}
		    }
		}
		/* Some addressing modes are invalid as destination */
		if (srcmode == imm || srcmode == PC16 || srcmode == PC8r)
		    goto nomatch; 
		break;
	     case 's':
		srcreg = bitval[bitS];
		srcmode = mode_from_mr(bitval[bits],bitval[bitS]);
		
		if(srcmode == am_illg) continue;		    
		if (srcmode == Areg || srcmode == Dreg || srcmode == Aind 
		    || srcmode == Ad16 || srcmode == Ad8r || srcmode == Aipi
		    || srcmode == Apdi)
		{
		    srcgather = true; srcpos = bitpos[bitS];
		}
		if (opcstr[pos] == '[') {
		    pos++;
		    if (opcstr[pos] == '!') {
			/* exclusion */
			do {			    
			    pos++;
			    if (mode_from_str(opcstr+pos) == srcmode) goto nomatch;
			    pos += 4;			    
			} while (opcstr[pos] == ',');
			pos++;
		    } else {
			if (opcstr[pos+4] == '-') {
			    /* replacement */
			    if (mode_from_str(opcstr+pos) == srcmode) 
			    	srcmode = mode_from_str(opcstr+pos+5);
			    else
			    	goto nomatch;
			    pos += 10;
			} else {
			    /* normal */
			    while(mode_from_str(opcstr+pos) != srcmode) {
				pos += 4;
				if (opcstr[pos] == ']') goto nomatch;
				pos++;
			    }
			    while(opcstr[pos] != ']') pos++;
			    pos++;
			}
		    }
		}
		break;
	     default: myabort();
	    }
	    /* safety check - might have changed */
	    if (srcmode != Areg && srcmode != Dreg && srcmode != Aind 
		&& srcmode != Ad16 && srcmode != Ad8r && srcmode != Aipi
		&& srcmode != Apdi && srcmode != immi)
	    {
		srcgather = false;
	    }
	    if (srcmode == Areg && sz == sz_byte)
	    	goto nomatch;
	    
	    if (opcstr[pos] != ',') goto endofline;
	    pos++;
	    
	    /* parse the destination address */
	    usedst = true;		
	    switch(opcstr[pos++]) {
	     case 'D': 
		destmode = Dreg;
		switch (opcstr[pos++]) {
		 case 'r': destreg = bitval[bitr]; dstgather = true; dstpos = bitpos[bitr]; break;
		 case 'R': destreg = bitval[bitR]; dstgather = true; dstpos = bitpos[bitR]; break;
		 default: myabort();
		}
		break;
	     case 'A': 
		destmode = Areg;
		switch (opcstr[pos++]) {
		 case 'r': destreg = bitval[bitr]; dstgather = true; dstpos = bitpos[bitr]; break;
		 case 'R': destreg = bitval[bitR]; dstgather = true; dstpos = bitpos[bitR]; break;
		 default: myabort();
		}
		switch (opcstr[pos]) {
		 case 'p': destmode = Apdi; pos++; break;
		 case 'P': destmode = Aipi; pos++; break;
		}
		break;
	     case '#':
		switch(opcstr[pos++]) {
		 case 'z': destmode = imm; break;
		 case '0': destmode = imm0; break;
		 case '1': destmode = imm1; break;
		 case '2': destmode = imm2; break;
		 case 'i': destmode = immi; destreg = (LONG)(BYTE)bitval[biti]; break;
		 case 'j': destmode = immi; destreg = bitval[bitj]; break;
		 case 'J': destmode = immi; destreg = bitval[bitJ]; break;
		 default: myabort();
		}
		break;
	     case 'd':
		destreg = bitval[bitD];
		destmode = mode_from_mr(bitval[bitd],bitval[bitD]);
		if(destmode == am_illg) continue;
		if (destmode == Areg || destmode == Dreg || destmode == Aind 
		    || destmode == Ad16 || destmode == Ad8r || destmode == Aipi
		    || destmode == Apdi)
		{
		    dstgather = true; dstpos = bitpos[bitD];
		}
		
		if (opcstr[pos] == '[') {
		    pos++;
		    if (opcstr[pos] == '!') {
			/* exclusion */
			do {			    
			    pos++;
			    if (mode_from_str(opcstr+pos) == destmode) goto nomatch;
			    pos += 4;			    
			} while (opcstr[pos] == ',');
			pos++;
		    } else {
			if (opcstr[pos+4] == '-') {
			    /* replacement */
			    if (mode_from_str(opcstr+pos) == destmode) 
			    	destmode = mode_from_str(opcstr+pos+5);
			    else
			    	goto nomatch;
			    pos += 10;
			} else {
			    /* normal */
			    while(mode_from_str(opcstr+pos) != destmode) {
				pos += 4;
				if (opcstr[pos] == ']') goto nomatch;
				pos++;
			    }
			    while(opcstr[pos] != ']') pos++;
			    pos++;
			    break;
			}
		    }
		}
		/* Some addressing modes are invalid as destination */
		if (destmode == imm || destmode == PC16 || destmode == PC8r)
		    goto nomatch;
		break;
	     case 's':
		destreg = bitval[bitS];
		destmode = mode_from_mr(bitval[bits],bitval[bitS]);
		
		if(destmode == am_illg) continue;		    
		if (destmode == Areg || destmode == Dreg || destmode == Aind 
		    || destmode == Ad16 || destmode == Ad8r || destmode == Aipi
		    || destmode == Apdi)
		{
		    dstgather = true; dstpos = bitpos[bitS];
		}
		
		if (opcstr[pos] == '[') {
		    pos++;
		    if (opcstr[pos] == '!') {
			/* exclusion */
			do {			    
			    pos++;
			    if (mode_from_str(opcstr+pos) == destmode) goto nomatch;
			    pos += 4;			    
			} while (opcstr[pos] == ',');
			pos++;
		    } else {
			if (opcstr[pos+4] == '-') {
			    /* replacement */
			    if (mode_from_str(opcstr+pos) == destmode) 
			    	destmode = mode_from_str(opcstr+pos+5);
			    else
			    	goto nomatch;
			    pos += 10;
			} else {
			    /* normal */
			    while(mode_from_str(opcstr+pos) != destmode) {
				pos += 4;
				if (opcstr[pos] == ']') goto nomatch;
				pos++;
			    }
			    while(opcstr[pos] != ']') pos++;
			    pos++;
			}
		    }
		}
		break;
	     default: myabort();
	    }
	    /* safety check - might have changed */
	    if (destmode != Areg && destmode != Dreg && destmode != Aind 
		&& destmode != Ad16 && destmode != Ad8r && destmode != Aipi
		&& destmode != Apdi)
	    {
		dstgather = false;
	    }
	    
	    if (destmode == Areg && sz == sz_byte)
	    	goto nomatch;
#if 0
	    if (sz == sz_byte && (destmode == Aipi || destmode == Apdi)) {
		dstgather = false;
	    }
#endif
endofline:
	    /* now, we have a match */
	    if (table[opc].mnemo != i_ILLG)
	    	fprintf(stderr, "Double match: %x: %s\n", opc, opcstr);
	    for(find = 0;; find++) {
		if (strcmp(mnemonic, lookuptab[find].name) == 0) {
		    table[opc].mnemo = lookuptab[find].mnemo;
		    break;
		}
		if (strlen(lookuptab[find].name) == 0) myabort();
	    }
	    table[opc].cc = bitval[bitc];
	    if (table[opc].mnemo == i_BTST
		|| table[opc].mnemo == i_BSET
		|| table[opc].mnemo == i_BCLR
		|| table[opc].mnemo == i_BCHG) 
	    {
		sz = destmode == Dreg ? sz_long : sz_byte;
	    }
	    table[opc].generated = false;
	    table[opc].size = sz;
	    table[opc].sreg = srcreg;
	    table[opc].dreg = destreg;
	    table[opc].smode = srcmode;
	    table[opc].dmode = destmode;
	    table[opc].spos = srcgather ? srcpos : -1;
	    table[opc].dpos = dstgather ? dstpos : -1;
	    table[opc].suse = usesrc;
	    table[opc].duse = usedst;
	    table[opc].sbtarg = srcbtarg;
nomatch:
	    /* FOO! */;
	}
    }
}

static int n_braces = 0;

static void start_brace(void)
{
    n_braces++;
    printf("{");
}

static void close_brace(void)
{
    assert (n_braces > 0);
    n_braces--;
    printf("}");
}

static void finish_braces(void)
{
    while (n_braces > 0)
	close_brace();
}

static void pop_braces(int to)
{
    while (n_braces > to)
	close_brace();
}

static void genamode(amodes mode, char *reg, wordsizes size, char *name, bool getv, bool movem)
{
    start_brace ();
    switch(mode) {
     case Dreg:
	if (movem)
	    abort();
	if (getv)
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = regs.d[%s];\n", name, reg);
		break;
	     case sz_word:
		printf("\tWORD %s = regs.d[%s];\n", name, reg);
		break;
	     case sz_long:
		printf("\tLONG %s = regs.d[%s];\n", name, reg);
		break;
	     default: myabort();
	    }
	break;
     case Areg:
	if (movem)
	    abort();
	if (getv)
	    switch(size) {	  
	     case sz_word:
		printf("\tWORD %s = regs.a[%s];\n", name, reg);
		break;
	     case sz_long:
		printf("\tLONG %s = regs.a[%s];\n", name, reg);
		break;
	     default: myabort();
	    }
	break;
     case Aind:
	printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	if (getv)
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	break;
     case Aipi:
	printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	switch(size) {
	 case sz_byte:	    
	    if (getv) printf("\tBYTE %s = get_byte(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += (%s == 7) ? 2 : 1;\n", reg, reg);
	    }
	    break;
	 case sz_word:
	    if (getv) printf("\tWORD %s = get_word(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += 2;\n", reg);
	    }
	    break;
	 case sz_long:
	    if (getv) printf("\tLONG %s = get_long(%sa);\n", name, name);
	    if (!movem) {
		start_brace();
		printf("\tregs.a[%s] += 4;\n", reg);
	    }
	    break;
	 default: myabort();
	}
	break;
     case Apdi:
	switch(size) {	  
	 case sz_byte:
	    if (!movem) printf("\tregs.a[%s] -= (%s == 7) ? 2 : 1;\n", reg, reg);
	    start_brace();
	    printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tBYTE %s = get_byte(%sa);\n", name, name);
	    break;
	 case sz_word:
	    if (!movem) printf("\tregs.a[%s] -= 2;\n", reg);
	    start_brace();
	    printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tWORD %s = get_word(%sa);\n", name, name);
	    break;
	 case sz_long:
	    if (!movem) printf("\tregs.a[%s] -= 4;\n", reg);
	    start_brace();
	    printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	    if (getv) printf("\tLONG %s = get_long(%sa);\n", name, name);
	    break;
	 default: myabort();
	}
	break;
     case Ad16:
	printf("\tCPTR %sa = regs.a[%s] + (LONG)(WORD)nextiword();\n", name, reg);
	if (getv) 
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	break;
     case Ad8r:
	printf("\tCPTR %sa = regs.a[%s];\n", name, reg);
	printf("\tUWORD %sdp = nextiword();\n", name);
	printf("\t%sa += (LONG)(BYTE)(%sdp & 0xFF);\n", name, name);
	start_brace();
	printf("\tULONG %sdpr = %sdp & 0x8000 ? regs.a[(%sdp & 0x7000) >> 12] : regs.d[(%sdp & 0x7000) >> 12];\n", name, name, name, name);
	printf("\tif (!(%sdp & 0x800)) %sdpr = (LONG)(WORD)%sdpr;\n", name, name, name);
	printf("\t%sa += %sdpr;\n", name, name);
	if (getv) {
	    start_brace();
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	}
	break;
     case PC16:
	printf("\tCPTR %sa = m68k_getpc();\n", name);
	printf("\t%sa += (LONG)(WORD)nextiword();\n", name);
	if (getv) {
	    start_brace();
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	}
	break;
     case PC8r:
	printf("\tCPTR %sa = m68k_getpc();\n", name);
	printf("\tUWORD %sdp = nextiword();\n", name);
	printf("\t%sa += (LONG)(BYTE)(%sdp & 0xFF);\n", name, name);
	start_brace();
	printf("\tULONG %sdpr = %sdp & 0x8000 ? regs.a[(%sdp & 0x7000) >> 12] : regs.d[(%sdp & 0x7000) >> 12];\n", name, name, name, name);
	printf("\tif (!(%sdp & 0x800)) %sdpr = (LONG)(WORD)%sdpr;\n", name, name, name);
	printf("\t%sa += %sdpr;\n", name, name);
	if (getv) {
	    start_brace();
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	}
	break;
     case absw:
	printf("\tCPTR %sa = (LONG)(WORD)nextiword();\n", name);
	if (getv) 
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	break;
     case absl:
	printf("\tCPTR %sa = nextilong();\n", name);
	if (getv) 
	    switch(size) {	  
	     case sz_byte:
		printf("\tBYTE %s = get_byte(%sa);\n", name, name);
		break;
	     case sz_word:
		printf("\tWORD %s = get_word(%sa);\n", name, name);
		break;
	     case sz_long:
		printf("\tLONG %s = get_long(%sa);\n", name, name);
		break;
	     default: myabort();
	    }
	break;
     case imm:
	if (getv) 
	    switch(size) {
	     case sz_byte:
		printf("\tBYTE %s = nextiword();\n", name);
		break;
	     case sz_word:
		printf("\tWORD %s = nextiword();\n", name);
		break;
	     case sz_long:
		printf("\tLONG %s = nextilong();\n", name);
		break;
	     default: myabort();
	    }
	break;
     case imm0:
	if (!getv) myabort();
	printf("\tBYTE %s = nextiword();\n", name);
	break;
     case imm1:
	if (!getv) myabort();
	printf("\tWORD %s = nextiword();\n", name);
	break;
     case imm2:
	if (!getv) myabort();
        printf("\tLONG %s = nextilong();\n", name);
	break;
     case immi:
	if (!getv) myabort();
	printf("\tULONG %s = %s;\n", name, reg);
	break;
     default: 
	myabort();
    }
}

static void genastore(char *from, amodes mode, char *reg, wordsizes size, char *to)
{
    switch(mode) {
     case Dreg:
	switch(size) {	  
	 case sz_byte:
	    printf("\tregs.d[%s] &= ~0xff; regs.d[%s] |= (%s) & 0xff;\n", reg, reg, from);
	    break;
	 case sz_word:
	    printf("\tregs.d[%s] &= ~0xffff; regs.d[%s] |= (%s) & 0xffff;\n", reg, reg, from);
	    break;
	 case sz_long:
	    printf("\tregs.d[%s] = (%s);\n", reg, from);
	    break;
	 default: myabort();
	}
	break;
     case Areg:
	switch(size) {	  
	 case sz_word:
	    printf("\tregs.a[%s] = (LONG)(WORD)(%s);\n", reg, from);
	    break;
	 case sz_long:
	    printf("\tregs.a[%s] = (%s);\n", reg, from);
	    break;
	 default: myabort();
	}
	break;
     case Aind:
     case Aipi:
     case Apdi:
     case Ad16:
     case Ad8r:
     case absw:
     case absl:
	switch(size) {
	 case sz_byte:
	    printf("\tput_byte(%sa,%s);\n", to, from);
	    break;
	 case sz_word:
	    printf("\tput_word(%sa,%s);\n", to, from);
	    break;
	 case sz_long:
	    printf("\tput_long(%sa,%s);\n", to, from);
	    break;
	 default: myabort();
	}
	break;
     case PC16:
     case PC8r:
	switch(size) {
	 case sz_byte:
	    printf("\tput_byte(%sa,%s);\n", to, from);
	    break;
	    /* These are only used for byte-sized bit ops */
	 default: myabort();
	}
	break;
     case imm:
     case imm0:
     case imm1:
     case imm2:
     case immi:
	myabort();
	break;
     default: 
	myabort();
    }
}

static void genmovemel(UWORD opcode)
{
    char getcode[100];
    int size = table[opcode].size == sz_long ? 4 : 2;
    
    if (table[opcode].size == sz_long) {	
    	strcpy(getcode, "get_long(srca)");
    } else {	    
    	strcpy(getcode, "(LONG)(WORD)get_word(srca)");
    }
    
    printf("\tUWORD mask = nextiword(), bitmask = mask;\n");
    genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, true);
    start_brace();
    printf("\tint i, bitcnt = 0;\n");
    printf("\tfor(i=0;i<16;i++) { bitcnt += bitmask & 1; bitmask >>= 1; }\n");
    
    printf("\tfor(i=0;i<8;i++) { if (mask & 1) { regs.d[i] = %s; srca += %d; } mask >>= 1; }\n", getcode, size);
    printf("\tfor(i=0;i<8;i++) { if (mask & 1) { regs.a[i] = %s; srca += %d; } mask >>= 1; }\n", getcode, size);
    
    if (table[opcode].smode == Aipi)
    	printf("\tregs.a[srcreg] = srca;\n");
}

static void genmovemle(UWORD opcode)
{
    char putcode[100], shiftcode[] = ">>";
    int size = table[opcode].size == sz_long ? 4 : 2;
    int mask = 1;
    if (table[opcode].size == sz_long) {
    	strcpy(putcode, "put_long(srca,");
    } else {	    
    	strcpy(putcode, "put_word(srca,");
    }
    
    printf("\tUWORD mask = nextiword(), bitmask = mask;\n");
    genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, true);
    start_brace();
    printf("\tint i, bitcnt = 0;\n");
    printf("\tULONG rd[8], ra[8];\n");
    printf("\tfor(i=0;i<16;i++) { bitcnt += bitmask & 1; bitmask >>= 1; }\n");
    printf("\tfor(i=0;i<8;i++) { rd[i] = regs.d[i]; ra[i] = regs.a[i]; }\n");
    if (table[opcode].smode == Apdi) {
	printf("\tsrca -= %d*bitcnt;\n", size);
	printf("\tregs.a[srcreg] = srca;\n");
	strcpy(shiftcode, "<<");
	mask = 0x8000;
    }

    printf("\tfor(i=0;i<8;i++) { if (mask & %d) { %s rd[i]); srca += %d; } mask %s= 1; }\n", 
	   mask, putcode, size, shiftcode);
    printf("\tfor(i=0;i<8;i++) { if (mask & %d) { %s ra[i]); srca += %d; } mask %s= 1; }\n",
	   mask, putcode, size, shiftcode);    
}

typedef enum {
    flag_logical, flag_add, flag_sub, flag_cmp, flag_addx, flag_subx, flag_zn
} flagtypes;

static void genflags(flagtypes type, wordsizes size, char *value, char *src, char *dst)
{
    char vstr[100],sstr[100],dstr[100];
    char usstr[100],udstr[100];
    char unsstr[100],undstr[100];
    ULONG mask;
    switch(size) {
     case sz_byte: mask = 0xFF; break;
     case sz_word: mask = 0xFFFF; break;
     case sz_long: mask = 0xFFFFFFFF; break;
     default: myabort();
    }
#ifndef NO_INLINE_FLAGS
    switch(size) {	    
     case sz_byte:
	strcpy(vstr, "((BYTE)(");
	strcpy(usstr, "((UBYTE)(");
	break;
     case sz_word:
	strcpy(vstr, "((WORD)(");
	strcpy(usstr, "((UWORD)(");
	break;
     case sz_long:
	strcpy(vstr, "((LONG)(");
	strcpy(usstr, "((ULONG)(");
	break;
     default:
	myabort();
    }
    strcpy(unsstr, usstr); 

    strcpy(sstr, vstr);
    strcpy(dstr, vstr);
    strcat(vstr, value); strcat(vstr,"))");
    strcat(dstr, dst); strcat(dstr,"))");
    strcat(sstr, src); strcat(sstr,"))");
    
    strcpy(udstr, usstr);
    strcat(udstr, dst); strcat(udstr,"))");
    strcat(usstr, src); strcat(usstr,"))");
    
    strcpy(undstr, unsstr);
    strcat(unsstr, "-");
    strcat(undstr, "~");
    strcat(undstr, dst); strcat(undstr,"))");
    strcat(unsstr, src); strcat(unsstr,"))");
    
    switch(type) {
     case flag_logical:
	printf("\tregs.v = regs.c = 0;\n");
	/* fall through */
     case flag_zn:
	switch(size) {
	 case sz_byte:
	    printf("\tregs.z = %s == 0;\n", vstr);
	    printf("\tregs.n = %s < 0;\n", vstr);
	    break;
	 case sz_word:
	    printf("\tregs.z = %s == 0;\n", vstr);
	    printf("\tregs.n = %s < 0;\n", vstr);
	    break;
	 case sz_long:
	    printf("\tregs.z = %s == 0;\n", vstr);
	    printf("\tregs.n = %s < 0;\n", vstr);
	    break;
	 default:
	    myabort();
	}
	break;
     case flag_add:
	start_brace();
	printf("\tbool flgs = %s < 0;\n", sstr);
	printf("\tbool flgo = %s < 0;\n", dstr);
	printf("\tbool flgn = %s < 0;\n", vstr);
	printf("\tregs.z = %s == 0;\n", vstr);
	printf("\tregs.v = (flgs == flgo) && (flgn != flgo);\n");
	printf("\tregs.c = regs.x = %s < %s;\n", undstr, usstr);
	printf("\tregs.n = flgn != 0;\n");
	break;
     case flag_sub:
	start_brace();
	printf("\tbool flgs = %s < 0;\n", sstr);
	printf("\tbool flgo = %s < 0;\n", dstr);
	printf("\tbool flgn = %s < 0;\n", vstr);
	printf("\tregs.z = %s == 0;\n", vstr);
	printf("\tregs.v = (flgs != flgo) && (flgn != flgo);\n");
	printf("\tregs.c = regs.x = %s > %s;\n", usstr, udstr);
	printf("\tregs.n = flgn != 0;\n");
	break;
     case flag_addx:
	start_brace();
	printf("\tbool flgs = %s < 0;\n", sstr);
	printf("\tbool flgo = %s < 0;\n", dstr);
	printf("\tbool flgn = %s < 0;\n", vstr);
	printf("\tif ((flgs && flgo && !flgn) || (!flgs && !flgo && flgn)) regs.v = 1;\n");
	printf("\tif ((flgs && flgo) || (!flgn && (flgo || flgs))) regs.c = regs.x = 1;\n");
	break;
     case flag_subx:
	start_brace();
	printf("\tbool flgs = %s < 0;\n", sstr);
	printf("\tbool flgo = %s < 0;\n", dstr);
	printf("\tbool flgn = %s < 0;\n", vstr);
	printf("\tif ((!flgs && flgo && !flgn) || (flgs && !flgo && flgn)) regs.v = 1;\n");
	printf("\tif ((flgs && !flgo) || (flgn && (!flgo || flgs))) regs.c = regs.x = 1;\n");
	break;
     case flag_cmp:
	start_brace();
	printf("\tbool flgs = %s < 0;\n", sstr);
	printf("\tbool flgo = %s < 0;\n", dstr);
	printf("\tbool flgn = %s < 0;\n", vstr);
	printf("\tregs.z = %s == 0;\n", vstr);
	printf("\tregs.v = (flgs != flgo) && (flgn != flgo);\n");
	printf("\tregs.c = %s > %s;\n", usstr, udstr);
	printf("\tregs.n = flgn != 0;\n");
	break;
    }
#else
    switch(type) {
     case flag_logical:
	printf("\tsetflags_logical(%s,%s);\n", value, mask);
	break;
     case flag_add:
	printf("\tsetflags_add(%s,%s,%s);\n", src, dest, value);
	break;
     case flag_sub:
	printf("\tsetflags_sub(%s,%s,%s);\n", src, dest, value);
	break;
     case flag_addx:
	printf("\tsetflags_addx(%s,%s,%s);\n", src, dest, value);
	break;
     case flag_subx:
	printf("\tsetflags_subx(%s,%s,%s);\n", src, dest, value);
	break;
     case flag_cmp:
	printf("\tsetflags_cmp(%s,%s,%s);\n", src, dest, value);
	break;
    }
#endif    
}

static void gen_opcode(unsigned long int opcode) 
{
    start_brace ();
    switch(table[opcode].mnemo) {
     case i_OR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	printf("\tsrc |= dst;\n");
	genflags(flag_logical, table[opcode].size, "src", "", "");
	genastore("src", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_AND:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	printf("\tsrc &= dst;\n");
	genflags(flag_logical, table[opcode].size, "src", "", "");
	genastore("src", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_EOR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	printf("\tsrc ^= dst;\n");
	genflags(flag_logical, table[opcode].size, "src", "", "");
	genastore("src", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_ORSR:
	if (table[opcode].size != sz_byte) {
	    printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else\n");
	}
	start_brace ();
	printf("\tMakeSR();\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	if (table[opcode].size == sz_byte) {
	    printf("\tsrc &= 0xFF;\n");
	}
	printf("\tregs.sr |= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_ANDSR: 
	if (table[opcode].size != sz_byte) {
	    printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else\n");
	}
	start_brace();
	
	printf("\tMakeSR();\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	if (table[opcode].size == sz_byte) {
	    printf("\tsrc |= 0xFF00;\n");
	}
	printf("\tregs.sr &= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_EORSR:
	if (table[opcode].size != sz_byte) {
	    printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else\n");
	}
	start_brace ();
	printf("\tMakeSR();\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	if (table[opcode].size == sz_byte) {
	    printf("\tsrc &= 0xFF;\n");
	}
	printf("\tregs.sr ^= src;\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_SUB: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst - src;\n");
	genflags(flag_sub, table[opcode].size, "newv", "src", "dst");
	genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_SUBA:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_long, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst - src;\n");
	genastore("newv", table[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_SUBX:
	{
	    int old_brace_level;
	    genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	    genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	    start_brace ();
	    printf("\tbool xflg = regs.x;\n");
	    printf("\tregs.v = regs.c = regs.x = 0;\n");
	    start_brace ();
	    printf("\tULONG newv = dst - src;\n");
	    genflags(flag_subx, table[opcode].size, "newv", "src", "dst");
	    /* FIXME: I'm not entirely sure how to handle this */
	    printf("\tif (xflg)\n"); 
	    old_brace_level = n_braces;
	    start_brace ();
	    genflags(flag_subx, table[opcode].size, "(newv-1)", "1", "newv");
	    printf("\tnewv--;\n");
	    pop_braces(old_brace_level);
	    
	    genflags(flag_zn, table[opcode].size, "newv", "", "");
	    genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	}
	break;
     case i_SBCD:
	/* Let's hope this works... */
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	start_brace ();
	printf("\tUWORD newv = dst - src - regs.x;\n");
	printf("\tif ((newv & 0xF) > 9) newv-=6;\n");
	printf("\tregs.c = regs.x = (newv & 0x1F0) > 0x90;\n");
	printf("\tif (regs.c) newv -= 0x60;\n");
	printf("\tif (newv != 0) regs.z = 0;\n");
	genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_ADD:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst + src;\n");
	genflags(flag_add, table[opcode].size, "newv", "src", "dst");
	genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_ADDA: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_long, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst + src;\n");
	genastore("newv", table[opcode].dmode, "dstreg", sz_long, "dst");
	break;
     case i_ADDX:
	{
	    int old_brace_level;
	    genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	    genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	    start_brace ();
	    printf("\tbool xflg = regs.x;\n");
	    printf("\tregs.v = regs.c = regs.x = 0;\n");
	    start_brace ();
	    printf("\tULONG newv = dst + src;\n");
	    genflags(flag_addx, table[opcode].size, "newv", "src", "dst");
	    printf("\tif (xflg)\n");
	    old_brace_level = n_braces;
	    start_brace ();
	    genflags(flag_addx, table[opcode].size, "(newv+1)", "newv", "1");
	    printf("\tnewv++;\n");
	    pop_braces(old_brace_level);
	    genflags(flag_zn, table[opcode].size, "newv", "", "");
	    genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	}
	break;
     case i_ABCD:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	start_brace ();
	printf("\tUWORD newv = src + dst + regs.x;\n");
	printf("\tif ((newv & 0xF) > 9) newv+=6;\n");
	printf("\tregs.c = regs.x = (newv & 0x1F0) > 0x90;\n");
	printf("\tif (regs.c) newv += 0x60;\n");
	printf("\tif (newv != 0) regs.z = 0;\n");
	genastore("newv", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_NEG:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	printf("\tULONG dst = -src;\n");
	genflags(flag_sub, table[opcode].size, "dst", "src", "0");
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_NEGX:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	printf("\tULONG dst = -src-regs.x;\n");
	genflags(flag_sub, table[opcode].size, "dst", "src", "0");
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_NBCD: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	printf("\tUWORD newv = - src - regs.x;\n");
	printf("\tif ((newv & 0xF) > 9) newv-=6;\n");
	printf("\tregs.c = regs.x = (newv & 0x1F0) > 0x90;\n");
	printf("\tif (regs.c) newv -= 0x60;\n");
	printf("\tif (newv != 0) regs.z = 0;\n");
	genastore("newv", table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_CLR: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	genflags(flag_logical, table[opcode].size, "0", "", "");
	genastore("0",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_NOT: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	printf("\tULONG dst = ~src;\n");
	genflags(flag_logical, table[opcode].size, "dst", "", "");
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_TST:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genflags(flag_logical, table[opcode].size, "src", "", "");
	break;
     case i_BTST:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	if (table[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tregs.z = !(dst & (1 << src));\n");
	break;
     case i_BCHG:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	if (table[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tregs.z = !(dst & (1 << src));\n");
	printf("\tdst ^= (1 << src);\n");
	genastore("dst", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_BCLR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	if (table[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tregs.z = !(dst & (1 << src));\n");
	printf("\tdst &= ~(1 << src);\n");
	genastore("dst", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_BSET:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	if (table[opcode].size == sz_byte)
	    printf("\tsrc &= 7;\n");
	else
	    printf("\tsrc &= 31;\n");
	printf("\tregs.z = !(dst & (1 << src));\n");
	printf("\tdst |= (1 << src);\n");
	genastore("dst", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_CMPM:
     case i_CMP:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst - src;\n");
	genflags(flag_cmp, table[opcode].size, "newv", "src", "dst");
	break;
     case i_CMPA: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_long, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = dst - src;\n");
	genflags(flag_cmp, sz_long, "newv", "src", "dst");
	break;
     case i_MOVEP: 
	printf("\tfprintf(stderr, \"MOVEP\\n\");\n");
	break;
     case i_MOVE:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", false, false);
	genflags(flag_logical, table[opcode].size, "src", "", "");
	genastore("src", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_MOVEA:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", false, false);
	genastore("src", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_MVSR2: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	printf("\tMakeSR();\n");
	genastore("regs.sr", table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_MV2SR:
	if (table[opcode].size != sz_byte) {
	    printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else\n");
	}
	start_brace ();
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	if (table[opcode].size == sz_byte)
	    printf("\tMakeSR();\n\tregs.sr &= 0xFF00;\n\tregs.sr |= src & 0xFF;\n");
	else {		    
	    printf("\tregs.sr = src;\n");
	}
	printf("\tMakeFromSR();\n");
	break;
     case i_SWAP: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	printf("\tULONG dst = ((src >> 16)&0xFFFF) | ((src&0xFFFF)<<16);\n");
	genflags(flag_logical, table[opcode].size, "dst", "", "");
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_EXG:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", true, false);
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	genastore("src",table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_EXT:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_word: printf("\tULONG dst = (LONG)(BYTE)src;\n"); break;
	 case sz_long: printf("\tULONG dst = (LONG)(WORD)src;\n"); break;
	 default: myabort();
	}
	genflags(flag_logical, table[opcode].size, "dst", "", "");
	genastore("dst",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_MVMEL:
	genmovemel(opcode);
	break;
     case i_MVMLE:
	genmovemle(opcode);
	break;
     case i_TRAP:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	printf("\tException(src+32);\n");
	break;
     case i_MVR2USP:
	printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else {\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	printf("\tregs.usp = src;\n");
	printf("\t}\n");
	break;
     case i_MVUSP2R: 
	printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else {\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	genastore("regs.usp", table[opcode].smode, "srcreg", table[opcode].size, "src");
	printf("\t}\n");
	break;
     case i_RESET:
	printf("\tcustomreset();\n");
	break;
     case i_NOP:
	break;
     case i_STOP:
	printf("\tif (!regs.s) { regs.pc_p--; Exception(8); } else {\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	printf("\tregs.sr = src;\n");
	printf("\tMakeFromSR();\n");
	printf("\tm68k_setstopped(1);\n");
	printf("\t}\n");
	break;
     case i_RTE:
	genamode(Aipi, "7", sz_word, "sr", true, false);
	genamode(Aipi, "7", sz_long, "pc", true, false);
	printf("\tregs.sr = sr; m68k_setpc(pc);\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_RTD:
	break;
     case i_LINK:
	genamode(Apdi, "7", sz_long, "old", false, false);
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genastore("src", Apdi, "7", sz_long, "old");
	genastore("regs.a[7]", table[opcode].smode, "srcreg", table[opcode].size, "src");
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "offs", true, false);
	printf("\tregs.a[7] += offs;\n");
	break;
     case i_UNLK:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	printf("\tregs.a[7] = src;\n");
	genamode(Aipi, "7", sz_long, "old", true, false);
	genastore("old", table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_RTS:
	genamode(Aipi, "7", sz_long, "pc", true, false);
	printf("\tm68k_setpc(pc);\n");
	break;
     case i_TRAPV:
	printf("\tif(regs.v) Exception(7);\n");
	break;
     case i_RTR: 
	printf("\tMakeSR();\n");
	genamode(Aipi, "7", sz_word, "sr", true, false);
	genamode(Aipi, "7", sz_long, "pc", true, false);
	printf("\tregs.sr &= 0xFF00; sr &= 0xFF;\n");
	printf("\tregs.sr |= sr; m68k_setpc(pc);\n");
	printf("\tMakeFromSR();\n");
	break;
     case i_JSR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	genamode(Apdi, "7", sz_long, "sp", false, false);
	genastore("m68k_getpc()", Apdi, "7", sz_long, "sp");
	printf("\tm68k_setpc(srca);\n");
	break;
     case i_JMP: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	printf("\tm68k_setpc(srca);\n");
	break;
     case i_BSR:
	printf("\tchar *oldpcp = (char *)regs.pc_p;\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(Apdi, "7", sz_long, "sp", false, false);
	genastore("m68k_getpc()", Apdi, "7", sz_long, "sp");
	printf("\tregs.pc_p = (UWORD *)(oldpcp + (LONG)src);\n");
	break;
     case i_Bcc:
	printf("\tchar *oldpcp = (char *)regs.pc_p;\n");
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	printf("\tif (cctrue(%d)) regs.pc_p = (UWORD *)(oldpcp + (LONG)src);\n", table[opcode].cc);
	break;
     case i_LEA:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "dst", false, false);
	genastore("srca", table[opcode].dmode, "dstreg", table[opcode].size, "dst");
	break;
     case i_PEA:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	genamode(Apdi, "7", sz_long, "dst", false, false);
	genastore("srca", Apdi, "7", sz_long, "dst");
	break;
     case i_DBcc:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "offs", true, false);
	printf("\tif (!cctrue(%d)) {\n", table[opcode].cc);
	printf("\tif (src--) regs.pc_p = (UWORD *)((char *)regs.pc_p + (LONG)offs - 2);\n");
	genastore("src", table[opcode].smode, "srcreg", table[opcode].size, "src");
	printf("\t}\n");
	break;
     case i_Scc: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "src", false, false);
	start_brace ();
	printf("\tint val = cctrue(%d) ? 0xff : 0;\n", table[opcode].cc);
	genastore("val",table[opcode].smode, "srcreg", table[opcode].size, "src");
	break;
     case i_DIVU:
	genamode(table[opcode].smode, "srcreg", sz_word, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_long, "dst", true, false);
	printf("\tif(src != 0){\n");
	printf("\tULONG newv = (ULONG)dst / (UWORD)src;\n");
	printf("\tULONG rem = (ULONG)dst %% (UWORD)src;\n");
	/* The N flag appears to be set each time there is an overflow.
	 * Weird. */
	printf("\tif (newv > 0xffff) { regs.v = regs.n = 1; } else\n\t{\n");
	genflags(flag_logical, sz_word, "newv", "", "");
	printf("\tnewv = (newv & 0xffff) | ((ULONG)rem << 16);\n");
	genastore("newv",table[opcode].dmode, "dstreg", sz_long, "dst");
	printf("\t}\n");
	printf("\t}\n");
	break;
     case i_DIVS: 
	genamode(table[opcode].smode, "srcreg", sz_word, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_long, "dst", true, false);
	printf("\tif(src != 0){\n");
	printf("\tLONG newv = (LONG)dst / (WORD)src;\n");
	printf("\tUWORD rem = (LONG)dst %% (WORD)src;\n");
	printf("\tif ((newv & 0xffff0000) && (newv & 0xffff0000) != 0xffff0000) { regs.v = regs.n = 1; } else\n\t{\n");
	printf("\tif (((WORD)rem < 0) != ((LONG)dst < 0)) rem = -rem;\n");
	genflags(flag_logical, sz_word, "newv", "", "");
	printf("\tnewv = (newv & 0xffff) | ((ULONG)rem << 16);\n");
	genastore("newv",table[opcode].dmode, "dstreg", sz_long, "dst");
	printf("\t}\n");
	printf("\t}\n");
	break;
     case i_MULU: 
	genamode(table[opcode].smode, "srcreg", sz_word, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_word, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = (ULONG)(UWORD)dst * (ULONG)(UWORD)src;\n");
	genflags(flag_logical, sz_long, "newv", "", "");
	genastore("newv",table[opcode].dmode, "dstreg", sz_long, "dst");
#ifdef WANT_SLOW_MULTIPLY
	printf("\tspecialflags |= SPCFLAG_EXTRA_CYCLES;\n");
#endif
	break;
     case i_MULS:
	genamode(table[opcode].smode, "srcreg", sz_word, "src", true, false);
	genamode(table[opcode].dmode, "dstreg", sz_word, "dst", true, false);
	start_brace ();
	printf("\tULONG newv = (LONG)(WORD)dst * (LONG)(WORD)src;\n");
	genflags(flag_logical, sz_long, "newv", "", "");
	genastore("newv",table[opcode].dmode, "dstreg", sz_long, "dst");
#ifdef WANT_SLOW_MULTIPLY
	printf("\tspecialflags |= SPCFLAG_EXTRA_CYCLES;\n");
#endif
	break;
     case i_ASR: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	printf("\tregs.v = 0;\n");
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tULONG sign = cmask & val;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tregs.c=regs.x=val&1; val = ((ULONG)val >> 1) | sign;\n");
	printf("\t}\n\tregs.n = sign != 0;\n");
	printf("\tregs.z = val == 0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ASL:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	printf("\tregs.v = 0;\n");
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tULONG sign = cmask & val;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tregs.c=regs.x=(val&cmask)!=0; val <<= 1;\n");
	printf("\tif ((val&cmask)!=sign)regs.v=1;\n");
	printf("\t}\n\tregs.n = (val&cmask) != 0;\n");
	printf("\tregs.z = val == 0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_LSR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = regs.x = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_LSL:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = regs.x = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ROL:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\tif(carry)  val |= 1;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ROR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val = (ULONG)val >> 1;\n");
	printf("\tif(carry) val |= cmask;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("\tregs.c = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ROXL: 
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&cmask; val <<= 1;\n");
	printf("\tif(regs.x) val |= 1;\n");    
	printf("\tregs.x = carry != 0;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("\tregs.x = regs.c = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ROXR:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "cnt", true, false);
	genamode(table[opcode].dmode, "dstreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tcnt &= 63;\n");
	start_brace ();
	printf("\tint carry = 0;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tfor(;cnt;--cnt){\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	printf("\tif(regs.x) val |= cmask;\n");
	printf("\tregs.x = carry != 0;\n");
	printf("\t}\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("\tregs.x = regs.c = carry!=0;\n");
	genastore("val", table[opcode].dmode, "dstreg", table[opcode].size, "data");
	break;
     case i_ASRW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	printf("\tregs.v = 0;\n");
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tULONG sign = cmask & val;\n");
	printf("\tregs.c=regs.x=val&1; val = (val >> 1) | sign;\n");
	printf("\tregs.n = sign != 0;\n");
	printf("\tregs.z = val == 0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_ASLW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	printf("\tregs.v = 0;\n");
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tULONG sign = cmask & val;\n");
	printf("\tregs.c=regs.x=(val&cmask)!=0; val <<= 1;\n");
	printf("\tif ((val&cmask)!=sign)regs.v=1;\n");
	printf("\tregs.n = (val&cmask) != 0;\n");
	printf("\tregs.z = val == 0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_LSRW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&1;\n");
	printf("\tcarry=val&1; val >>= 1;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = regs.x = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_LSLW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = regs.x = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_ROLW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	printf("\tif(carry)  val |= 1;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_RORW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&1;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tval >>= 1;\n");
	printf("\tif(carry) val |= cmask;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.c = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_ROXLW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&cmask;\n");
	printf("\tval <<= 1;\n");
	printf("\tif(regs.x) val |= 1;\n");
	printf("\tregs.x = carry != 0;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.x = regs.c = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     case i_ROXRW:
	genamode(table[opcode].smode, "srcreg", table[opcode].size, "data", true, false);
	start_brace ();
	switch(table[opcode].size) {
	 case sz_byte: printf("\tUBYTE val = data;\n"); break;
	 case sz_word: printf("\tUWORD val = data;\n"); break;
	 case sz_long: printf("\tULONG val = data;\n"); break;
	 default: myabort();
	}
	printf("\tint carry = val&1;\n");
	switch(table[opcode].size) {
	 case sz_byte: printf("\tULONG cmask = 0x80;\n"); break;
	 case sz_word: printf("\tULONG cmask = 0x8000;\n"); break;
	 case sz_long: printf("\tULONG cmask = 0x80000000;\n"); break;
	 default: myabort();
	}
	printf("\tval >>= 1;\n");
	printf("\tif(regs.x) val |= cmask;\n");
	printf("\tregs.x = carry != 0;\n");
	genflags(flag_logical, table[opcode].size, "val", "", "");
	printf("regs.x = regs.c = carry!=0;\n");
	genastore("val", table[opcode].smode, "srcreg", table[opcode].size, "data");
	break;
     default:
	myabort();
	break;
    };
    finish_braces ();
}

static int mismatch;

static void handle_merges(long int opcode)
{
    UWORD smsk; 
    UWORD dmsk;
    int sbitsrc, sbitdst;
    int srcreg, dstreg;
    
    smsk = (table[opcode].sbtarg ? 255 : 7) << table[opcode].spos;
    dmsk = 7 << table[opcode].dpos;
    sbitsrc = table[opcode].spos == -1 ? 0 : 0;
    sbitdst = table[opcode].spos == -1 ? 1 : (table[opcode].sbtarg ? 256 : 8);
    for (srcreg=sbitsrc; srcreg < sbitdst; srcreg++) {
	for (dstreg=0; dstreg < (table[opcode].dpos == -1 ? 1 : 8); dstreg++) {
	    UWORD code = opcode;		

	    if (table[opcode].spos != -1) code &= ~(smsk << table[opcode].spos);
	    if (table[opcode].dpos != -1) code &= ~(dmsk << table[opcode].dpos);
	    
	    if (table[opcode].spos != -1) code |= srcreg << table[opcode].spos;
	    if (table[opcode].dpos != -1) code |= dstreg << table[opcode].dpos;
	    
	    /* Check whether this is in fact the same instruction.
	     * The instructions should never differ, except for the
	     * Bcc.(BW) case. */
	    if (table[code].mnemo != table[opcode].mnemo
		|| table[code].size != table[opcode].size
		|| table[code].suse != table[opcode].suse
		|| table[code].duse != table[opcode].duse) 
	    {
		mismatch++; continue;
	    }
	    if (table[opcode].suse 
		&& (table[opcode].spos != table[code].spos
		    || table[opcode].smode != table[code].smode
		    || table[opcode].sbtarg != table[code].sbtarg)) 
	    {
		mismatch++; continue;
	    }
	    if (table[opcode].duse 
		&& (table[opcode].dpos != table[code].dpos
		    || table[opcode].dmode != table[code].dmode))
	    {
		mismatch++; continue;
	    }
	    
	    table[code].generated = true;
	    table[code].handler = opcode;
	}
    }
}
	       
static void generate_func(long int from, long int to)
{
    int illg = 0;
    long int opcode;
    UWORD smsk; 
    UWORD dmsk;    
    mismatch = 0;

    printf("#include <stdlib.h>\n");
    printf("#include \"config.h\"\n");
    printf("#include \"amiga.h\"\n");
    printf("#include \"options.h\"\n");
    printf("#include \"memory.h\"\n");
    printf("#include \"custom.h\"\n");    
    printf("#include \"newcpu.h\"\n");
    printf("#include \"cputbl.h\"\n");
    for(opcode=from; opcode < to; opcode++) {
	if (table[opcode].mnemo == i_ILLG) {
	    illg++;
	    continue;
	}
	if (table[opcode].generated) {
	    continue;
	}
#ifndef HAVE_ONE_GIG_OF_MEMORY_TO_COMPILE_THIS
    
	handle_merges (opcode);
	
	smsk = (table[opcode].sbtarg ? 255 : 7) << table[opcode].spos;
	dmsk = 7 << table[opcode].dpos;

	printf("void op_%lx(UWORD opcode)\n{\n", opcode);
	if (table[opcode].suse
	    && table[opcode].smode != imm  && table[opcode].smode != imm0
	    && table[opcode].smode != imm1 && table[opcode].smode != imm2
	    && table[opcode].smode != absw && table[opcode].smode != absl
	    && table[opcode].smode != PC8r && table[opcode].smode != PC16)
	{
	    if (table[opcode].spos == -1) {
		printf("\tULONG srcreg = %d;\n", (int)table[opcode].sreg);
	    } else {
		printf("\tULONG srcreg = (LONG)(BYTE)((opcode & %d) >> %d);\n", smsk, (int)table[opcode].spos);
	    }
	}
	if (table[opcode].duse
	    /* Yes, the dmode can be imm, in case of LINK or DBcc */
	    && table[opcode].dmode != imm  && table[opcode].dmode != imm0
	    && table[opcode].dmode != imm1 && table[opcode].dmode != imm2
	    && table[opcode].dmode != absw && table[opcode].dmode != absl) {
	    if (table[opcode].dpos == -1) {		
		printf("\tULONG dstreg = %d;\n", (int)table[opcode].dreg);
	    } else {
		printf("\tULONG dstreg = (opcode & %d) >> %d;\n", dmsk, (int)table[opcode].dpos);
	    }
	}
#else
	printf("void op_%lx(UWORD opcode)\n{\n", opcode);
	if (table[opcode].suse)
	    printf("\tULONG srcreg = %d;\n", (int)table[opcode].sreg);
	if (table[opcode].duse)
	    printf("\tULONG dstreg = %d;\n", (int)table[opcode].dreg);
#endif
	gen_opcode(opcode);
        printf("}\n");
    }

    fprintf (stderr, "%d illegals generated.\n", illg);
    if (mismatch) fprintf(stderr, "%d mismatches.\n", mismatch);
}

static void generate_table(void)
{
    int illg = 0;
    long int opcode;
    
    mismatch = 0;
    
    printf("#include <stdlib.h>\n");
    printf("#include \"config.h\"\n");
    printf("#include \"amiga.h\"\n");
    printf("#include \"options.h\"\n");
    printf("#include \"memory.h\"\n");
    printf("#include \"custom.h\"\n");
    printf("#include \"newcpu.h\"\n");
    printf("#include \"cputbl.h\"\n");
    
    printf("cpuop_func *cpufunctbl[65536] = {\n");
    for(opcode=0; opcode < 65536; opcode++) {
	if (table[opcode].mnemo == i_ILLG) {
	    printf("op_illg");
	    illg++;
	    goto loop_end;
	}
	if (table[opcode].generated) {
	    printf("op_%x", table[opcode].handler);
	    goto loop_end;
	}
#ifndef HAVE_ONE_GIG_OF_MEMORY_TO_COMPILE_THIS
	handle_merges (opcode);
#endif
	printf("op_%lx", opcode);
	
	loop_end:
	if (opcode < 65535) printf(",");
	if ((opcode & 7) == 7) printf("\n");
    }
    printf("\n};\n");
    fprintf (stderr, "%d illegals generated.\n", illg);
    if (mismatch) fprintf(stderr, "%d mismatches.\n", mismatch);
}

static void generate_header(void)
{
    int illg = 0;
    long int opcode;
    
    mismatch = 0;
    
//    printf("extern void op_illg(UWORD) REGPARAM;\n");
    printf("extern void op_illg(UWORD);\n");
    
    for(opcode=0; opcode < 65536; opcode++) {
	if (table[opcode].mnemo == i_ILLG) {
	    illg++;
	    continue;
	}
	if (table[opcode].generated) 
	    continue;
	    
#ifndef HAVE_ONE_GIG_OF_MEMORY_TO_COMPILE_THIS
	handle_merges (opcode);
#endif
	printf("extern cpuop_func op_%lx;\n", opcode);
    }
    
    fprintf (stderr, "%d illegals generated.\n", illg);
    if (mismatch) fprintf(stderr, "%d mismatches.\n", mismatch);
}

int main(int argc, char **argv)
{
    long int range = -1;
    char mode = 'n';
    int i;
    
    if (argc == 2)
    	mode = *argv[1];

    if (argc == 3) {
	range = atoi(argv[2]);
	mode = *argv[1];
    }
    tablef = fopen("table68k","r");
    if (tablef == NULL) {
	fprintf(stderr, "table68k not found\n");
	exit(1);
    }
    table = (struct instr *) malloc (sizeof(struct instr) * 65536);
    for(i = 0; i < 65536; i++) {
	table[i].mnemo = i_ILLG;
    }
    getnextch();
    while (nextch != EOF) {
	get_bits();
	get_strs();
	parse_bits();
    }
    switch(mode) {
     case 'f':
    	generate_func(range * 0x1000, (range + 1) * 0x1000);
	break;
     case 'h':
    	generate_header();
	break;
     case 't':
	generate_table();
	break;
     default:
	myabort();
    }
    fclose(tablef);
    free(table);
    return 0;
}
