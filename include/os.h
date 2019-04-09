 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * OS specific functions
  * 
  * (c) 1995 Bernd Schmidt
  */

extern void read_joystick(UWORD *dir, bool *button);
extern void init_joystick(void);
extern void close_joystick(void);

extern CPTR audlc[4], audpt[4];
extern UWORD audvol[4], audper[4], audlen[4];

extern bool init_sound (void);
extern void do_sound (void);
extern void flush_sound (void);
