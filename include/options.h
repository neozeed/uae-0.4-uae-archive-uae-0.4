 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Run-time configuration options
  * 
  * (c) 1995 Bernd Schmidt, Ed Hanway
  */

typedef enum { KBD_LANG_US, KBD_LANG_DE } KbdLang;

extern int framerate;
extern bool produce_sound;
extern bool dont_want_aspect;
extern bool use_fast_draw;
extern bool use_debugger;
extern bool use_slow_mem;
extern bool automount_uaedev;
extern KbdLang keyboard_lang;

#undef DEFAULT_NO_ASPECT
#ifdef DEFAULT_WANT_ASPECT
#define DEFAULT_NO_ASPECT false
#else
#define DEFAULT_NO_ASPECT true
#endif

#undef DEFAULT_KBD_LANG
#ifdef DEFAULT_KBD_LANG_DE
#define DEFAULT_KBD_LANG KBD_LANG_DE
#else
#define DEFAULT_KBD_LANG KBD_LANG_US
#endif

#ifndef DEFAULT_FRAMERATE
#define DEFAULT_FRAMERATE 5
#endif

#undef HAVE_JOYSTICK
#ifdef LINUX_JOYSTICK
#define HAVE_JOYSTICK
#endif

#ifdef __mac__
#define __inline__ inline
#else
#ifndef __GNUC__
#ifdef __cplusplus
#define __inline__ inline
#else
#define __inline__
#endif
#endif
#endif

#ifndef __unix
extern void parse_cmdline(int argc, char **argv);
#endif

/* Try to figure out whether we can __inline__ DrawPixel() */

#undef INLINE_DRAWPIXEL8
#undef INLINE_DRAWPIXEL16
#undef INLINE_DRAWPIXEL

#ifdef LINUX_SVGALIB

#define INLINE_DRAWPIXEL

#ifdef SVGALIB_8BIT_SCREEN
#define INLINE_DRAWPIXEL8
#else

#ifdef SVGALIB_16BIT_SCREEN
#define INLINE_DRAWPIXEL16
#else
#error You need to define either SVGALIB_8BIT_SCREEN or SVGALIB_16BIT_SCREEN.
#endif

#endif

#else /* not LINUX_SVGALIB */

#ifdef X_8BIT_SCREEN
#define INLINE_DRAWPIXEL8
#define INLINE_DRAWPIXEL
#else
#ifdef X_16BIT_SCREEN
#define INLINE_DRAWPIXEL16
#define INLINE_DRAWPIXEL
#endif
#endif

#endif /* not LINUX_SVGALIB */

#ifdef __mac__
/* Apparently, no memcpy :-/ */
/*
static __inline__ void *memcpy(void *to, void *from, int size)
{
    BlockMove(from, to, size);
}
*/
#endif
