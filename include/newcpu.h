 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * MC68000 emulation
  *
  * (c) 1995 Bernd Schmidt
  * 
  */

/* Kludge! */
#define INLINE static __inline__

extern bool broken_in;

typedef void cpuop_func(UWORD) REGPARAM;

extern cpuop_func *cpufunctbl[65536];
extern void op_illg(UWORD) REGPARAM;

typedef char flagtype; 

extern struct regstruct 
{
    ULONG d[8];
    CPTR  a[8],usp;
    UWORD sr;
    flagtype t,s,x,n,z,v,c,stopped;
    int intmask;
    ULONG pc;
    UWORD *pc_p;
    UWORD *pc_oldp;
} regs;

extern void MC68000_oldstep(UWORD opcode);

INLINE UWORD nextiword(void)
{
    UWORD r = *regs.pc_p++;
    return r;
}

INLINE ULONG nextilong(void)
{
    ULONG r = *regs.pc_p++;
    r = (r << 16) + *regs.pc_p++;
    return r;
}

INLINE void m68k_setpc(CPTR newpc)
{
    regs.pc = newpc;
    regs.pc_p = regs.pc_oldp = get_real_address(newpc);
}

INLINE CPTR m68k_getpc(void)
{
    return regs.pc + ((char *)regs.pc_p - (char *)regs.pc_oldp);
}

INLINE void m68k_setstopped(int stop)
{
    regs.stopped = stop;
    if (stop)
    	specialflags |= SPCFLAG_STOP;
}

INLINE void setflags_add(const ULONG src, const ULONG oldv, 
			 const ULONG newv, const ULONG mask)
{
    int n=0, o=0, s=0;
    switch(mask) {
     case 0xff:
	s = (BYTE)src < 0; 
	o = (BYTE)oldv < 0;
	n = (BYTE)newv < 0;
	regs.z = (BYTE)newv == 0;
	break;
     case 0xffff:
	s = (WORD)src < 0; 
	o = (WORD)oldv < 0;
	n = (WORD)newv < 0;
	regs.z = (WORD)newv == 0;
	break;
     case 0xffffffff:
	s = (LONG)src < 0; 
	o = (LONG)oldv < 0;
	n = (LONG)newv < 0;
	regs.z = (LONG)newv == 0;
	break;
    } 
    regs.c = regs.x = (s && o) || (!n && (o || s));
    regs.n = n;
    regs.v = ((s && o && !n) || (!s && !o && n));
}

INLINE void setflags_sub(const ULONG src, const ULONG oldv,
			 const ULONG newv, const ULONG mask)
{
    int n=0, o=0, s=0;
    switch(mask) {
     case 0xff:
	s = (BYTE)src < 0; 
	o = (BYTE)oldv < 0;
	n = (BYTE)newv < 0;
	regs.z = (BYTE)newv == 0;
	break;
     case 0xffff:
	s = (WORD)src < 0; 
	o = (WORD)oldv < 0;
	n = (WORD)newv < 0;
	regs.z = (WORD)newv == 0;
	break;
     case 0xffffffff:
	s = (LONG)src < 0; 
	o = (LONG)oldv < 0;
	n = (LONG)newv < 0;
	regs.z = (LONG)newv == 0;
	break;
    } 

    regs.z = newv == 0;
    regs.c = regs.x = (s && !o) || (n && (!o || s));
    regs.n = n;
    regs.v = ((!s && o && !n) || (s && !o && n));
}

INLINE void setflags_addx(const ULONG src, const ULONG oldv, 
			  const ULONG newv, const ULONG mask)
{
    int n=0, o=0, s=0;
    switch(mask) {
     case 0xff:
	s = (BYTE)src < 0; 
	o = (BYTE)oldv < 0;
	n = (BYTE)newv < 0;
	if ((BYTE)newv != 0) regs.z = 0;
	break;
     case 0xffff:
	s = (WORD)src < 0; 
	o = (WORD)oldv < 0;
	n = (WORD)newv < 0;
	if ((WORD)newv != 0) regs.z = 0;
	break;
     case 0xffffffff:
	s = (LONG)src < 0; 
	o = (LONG)oldv < 0;
	n = (LONG)newv < 0;
	if ((LONG)newv != 0) regs.z = 0;
	break;
    } 
    
    regs.c = regs.x = (s && o) || (!n && (o || s));
    regs.n = n;
    regs.v = ((s && o && !n) || (!s && !o && n));
}

INLINE void setflags_subx(const ULONG src, const ULONG oldv,
			  const ULONG newv, const ULONG mask)
{
    int n=0, o=0, s=0;
    switch(mask) {
     case 0xff:
	s = (BYTE)src < 0; 
	o = (BYTE)oldv < 0;
	n = (BYTE)newv < 0;
	if ((BYTE)newv != 0) regs.z = 0;
	break;
     case 0xffff:
	s = (WORD)src < 0; 
	o = (WORD)oldv < 0;
	n = (WORD)newv < 0;
	if ((WORD)newv != 0) regs.z = 0;
	break;
     case 0xffffffff:
	s = (LONG)src < 0; 
	o = (LONG)oldv < 0;
	n = (LONG)newv < 0;
	if ((LONG)newv != 0) regs.z = 0;
	break;
    } 

    if (newv != 0) regs.z = 0;
    regs.c = regs.x = (s && !o) || (n && (!o || s));
    regs.n = n;
    regs.v = ((!s && o && !n) || (s && !o && n));
}

INLINE void setflags_cmp(const ULONG src, const ULONG oldv, 
			 const ULONG newv, const ULONG mask)
{
    int n=0, o=0, s=0;
    switch(mask) {
     case 0xff:
	s = (BYTE)src < 0; 
	o = (BYTE)oldv < 0;
	n = (BYTE)newv < 0;
	regs.z = (BYTE)newv == 0;
	break;
     case 0xffff:
	s = (WORD)src < 0; 
	o = (WORD)oldv < 0;
	n = (WORD)newv < 0;
	regs.z = (WORD)newv == 0;
	break;
     case 0xffffffff:
	s = (LONG)src < 0; 
	o = (LONG)oldv < 0;
	n = (LONG)newv < 0;
	regs.z = (LONG)newv == 0;
	break;
    } 
    
    regs.z = newv == 0;
    regs.c = (s && !o) || (n && (!o || s));
    regs.n = n;
    regs.v = ((!s && o && !n) || (s && !o && n));
}

INLINE void setflags_logical(ULONG newv, const ULONG mask)
{
    switch(mask) {
     case 0xff: 
	regs.z = (BYTE)(newv) == 0;
	regs.n = (BYTE)(newv) < 0;
	break;
     case 0xffff: 
	regs.z = (WORD)(newv) == 0;
	regs.n = (WORD)(newv) < 0;
	break;
     case 0xffffffff: 
	regs.z = (LONG)(newv) == 0;
	regs.n = (LONG)(newv) < 0;
	break;
    }
    regs.v = regs.c = 0;
}


static __inline__ bool cctrue(const int cc)
{
    switch(cc){
     case 0: return true;                                                    /* T */
     case 1: return false;                                                   /* F */
     case 2: return !regs.c && !regs.z;                                      /* HI */
     case 3: return regs.c || regs.z;                                        /* LS */
     case 4: return !regs.c;                                                 /* CC */
     case 5: return regs.c;                                                  /* CS */
     case 6: return !regs.z;                                                 /* NE */
     case 7: return regs.z;                                                  /* EQ */
     case 8: return !regs.v;                                                 /* VC */
     case 9: return regs.v;                                                  /* VS */
     case 10:return !regs.n;                                                 /* PL */
     case 11:return regs.n;                                                  /* MI */
#if 0
     case 12:return (regs.n && regs.v) || (!regs.n && !regs.v);              /* GE */
     case 13:return (regs.n && !regs.v) || (!regs.n && regs.v);              /* LT */
     case 14:return !regs.z && ((regs.n && regs.v) || (!regs.n && !regs.v)); /* GT */
     case 15:return regs.z || ((regs.n && !regs.v) || (!regs.n && regs.v));  /* LE */
#endif 
     case 12:return regs.n == regs.v;             /* GE */
     case 13:return regs.n != regs.v;             /* LT */
     case 14:return !regs.z && (regs.n == regs.v);/* GT */
     case 15:return regs.z || (regs.n != regs.v); /* LE */
    }
    abort();
    return 0;
}

extern void MakeSR(void);
extern void MakeFromSR(void);
extern void Exception(int);

extern void MC68000_init(void);
extern void MC68000_step(void);
extern void MC68000_run(void);
extern void MC68000_skip(CPTR);
extern void MC68000_dumpstate(CPTR *);
extern void MC68000_disasm(CPTR,CPTR *,int);
extern void MC68000_reset(void);
