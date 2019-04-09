 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Interface to the graphics system (X, SVGAlib)
  * 
  * (c) 1995 Bernd Schmidt
  */

extern void graphics_init(void);
extern void graphics_leave(void);
extern void handle_events(void);

extern void prepare_line(int, bool);
extern void flush_line(void);
extern void flush_screen(void);

extern bool debuggable(void);
extern bool needmousehack(void);
extern void togglemouse(void);
extern void LED(int);

typedef long int xcolnr;

extern xcolnr xcolors[4096];

extern bool buttonstate[3];
extern bool newmousecounters;
extern int lastmx, lastmy;

#ifdef INLINE_DRAWPIXEL

extern char *xlinebuffer;

static __inline__ void DrawPixel(int x, xcolnr col)
{
#ifdef INLINE_DRAWPIXEL16
    *(short *)xlinebuffer = col;
    xlinebuffer+=2;
#endif
#ifdef INLINE_DRAWPIXEL8
    *(char *)xlinebuffer = col;
    xlinebuffer++;
#endif
}

#else
#ifdef __mac__
extern char *xlinebuffer;

static inline void DrawPixel(int x, xcolnr col)
{
    ((short *)xlinebuffer)[x] = col;
}
#else /* not INLINE_DRAWPIXEL */

extern void (*DrawPixel)(int, xcolnr);

#endif
#endif