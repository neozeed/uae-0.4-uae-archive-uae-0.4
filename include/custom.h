 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * custom chip support
  *
  * (c) 1995 Bernd Schmidt
  */

extern void custom_init(void);
extern void customreset(void);
extern int intlev(void);
extern bool dmaen(UWORD dmamask);
extern void dumpcustom(void);

extern void do_disk(void);
extern void do_blitter(void);

extern bool inhibit_frame;
extern bool bogusframe;

extern unsigned long specialflags;

#define SPCFLAG_BLIT 1
#define SPCFLAG_STOP 2
#define SPCFLAG_DISK 4
#define SPCFLAG_INT  8
#define SPCFLAG_BRK  16
#define SPCFLAG_EXTRA_CYCLES 32
#define SPCFLAG_TRACE 64
#define SPCFLAG_DOTRACE 128
#define SPCFLAG_DOINT 256

extern int dskdmaen;
