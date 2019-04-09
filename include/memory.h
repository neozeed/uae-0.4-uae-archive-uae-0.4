 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * memory management
  * 
  * (c) 1995 Bernd Schmidt
  */

#ifdef DUALCPU
extern bool allowmem;
extern bool incpu;
extern bool customacc;
extern ULONG mempattern;
extern int memlogptr;
#endif

#define chipmem_size 0x200000
#define fastmem_size 0
#define bogomem_size 0x80000 /* C00000 crap mem */
#define kickmem_size 0x80000

#define chipmem_start 0
#define fastmem_start 0x200000
#define bogomem_start 0xC00000 
#define kickmem_start 0xF80000

typedef ULONG (*lget_func)(CPTR);
typedef UWORD (*wget_func)(CPTR);
typedef UBYTE (*bget_func)(CPTR);
typedef void (*lput_func)(CPTR,ULONG);
typedef void (*wput_func)(CPTR,UWORD);
typedef void (*bput_func)(CPTR,UBYTE);
typedef UWORD *(*xlate_func)(CPTR);
typedef bool (*check_func)(CPTR, ULONG);

typedef struct {
    lget_func lget;
    wget_func wget;
    bget_func bget;
    lput_func lput;
    wput_func wput;
    bput_func bput;
    xlate_func xlateaddr;
    check_func check;
} addrbank;

extern addrbank chipmem_bank;
extern addrbank kickmem_bank;
extern addrbank custom_bank;
extern addrbank clock_bank;
extern addrbank cia_bank;
extern addrbank rtarea_bank;

extern void rtarea_init (void);

/* Default memory access functions */

extern bool default_check(CPTR addr, ULONG size);
extern UWORD *default_xlate(CPTR addr);

extern lget_func do_lget[256];
extern wget_func do_wget[256];
extern bget_func do_bget[256];
extern lput_func do_lput[256];
extern wput_func do_wput[256];
extern bput_func do_bput[256];
extern xlate_func do_xlateaddr[256];
extern check_func do_check[256];

static __inline__ int bankindex(CPTR a)
{
#ifdef HAVE_BROKEN_SOFTWARE
    return (a>>16) & 0xFF;
#else
    return a>>16;
#endif
}

static __inline__ ULONG longget(CPTR addr)
{
    return do_lget[bankindex(addr)](addr);
}
static __inline__ UWORD wordget(CPTR addr)
{
    return do_wget[bankindex(addr)](addr);
}
static __inline__ UBYTE byteget(CPTR addr) 
{
    return do_bget[bankindex(addr)](addr);
}
static __inline__ void longput(CPTR addr, ULONG l)
{
    do_lput[bankindex(addr)](addr, l);
}
static __inline__ void wordput(CPTR addr, UWORD w)
{
    do_wput[bankindex(addr)](addr, w);
}
static __inline__ void byteput(CPTR addr, UBYTE b)
{
    do_bput[bankindex(addr)](addr, b);
}

static __inline__ bool check_addr(CPTR a)
{
#ifdef NO_EXCEPTION_3
    return true;
#else
    return (a & 1) == 0;
#endif
}
extern bool buserr;
    
extern void memory_init(void);    
extern void map_banks(addrbank bank, int first, int count);
    
#ifdef DUALCPU    
extern ULONG get_long(CPTR addr);
extern UWORD get_word(CPTR addr);
extern UBYTE get_byte(CPTR addr);
extern void put_long(CPTR a,ULONG l);
extern void put_word(CPTR a,UWORD w);
extern void put_byte(CPTR a,UBYTE b);
#else
static __inline__ ULONG get_long(CPTR addr) 
{
    if (check_addr(addr))
	return longget(addr);
    buserr = true;
    return 0;
}
static __inline__ UWORD get_word(CPTR addr) 
{
    if (check_addr(addr))
	return wordget(addr);
    buserr = true;
    return 0;
}
static __inline__ UBYTE get_byte(CPTR addr) 
{
    return byteget(addr); 
}
static __inline__ void put_long(CPTR addr, ULONG l) 
{
    if (!check_addr(addr))
	buserr = true;
    longput(addr, l);
}
static __inline__ void put_word(CPTR addr, UWORD w) 
{
    if (!check_addr(addr))
	buserr = true;
    wordput(addr, w);
}
static __inline__ void put_byte(CPTR addr, UBYTE b) 
{
    byteput(addr, b);
}
#endif

static __inline__ UWORD *get_real_address(CPTR addr)
{
    if (!check_addr(addr))
	buserr = true;
    return do_xlateaddr[bankindex(addr)](addr);
}

static __inline__ bool valid_address(CPTR addr, ULONG size)
{
    if (!check_addr(addr))
	buserr = true;
    return do_check[bankindex(addr)](addr, size);
}
