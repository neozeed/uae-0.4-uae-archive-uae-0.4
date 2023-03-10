 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Keyboard buffer. Not really needed for X, but for SVGAlib and possibly
  * Mac and DOS ports.
  * 
  * (c) 1996 Bernd Schmidt
  */

extern int get_next_key (void);
extern bool keys_available (void);
extern void record_key (int);
extern void keybuf_init (void);
