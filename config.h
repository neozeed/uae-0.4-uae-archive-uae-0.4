 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * User configuration options
  *
  * (c) 1995 Bernd Schmidt
  */

/*
 * This is just a performance hack. It should work automatically in all
 * circumstances. If you get compilation errors because of this, use a
 * single line saying "#define REGPARAM".
 */
#undef REGPARAM
#if defined(__i386__) && defined(__GNUC_MINOR__) 
#if (__GNUC__ > 2) || (__GNUC_MINOR__ > 6)
#define REGPARAM __attribute__((regparm(3)))
#endif
#endif
#ifndef REGPARAM
#define REGPARAM
#endif

/*
 * If you encounter compilation problems of the sort "IPC_whatever undefined",
 * or if you get SHM errors when running UAE, try defining this.
#define DONT_WANT_SHM
 */

/*
 * Define if you have a Mac. No, wrong. Define if you are compiling on a Mac.
*/
#define __mac__

/***************************************************************************
 * The following options are run-time configurable. However, you can
 * change the default values by adding defines here. You can also simply
 * skip this block: in that case you'll get the default behaviour.
 */

/*
 * You can select your keyboard language. US is the default when neither is
 * selected.
 * The run-time option for this is "-l lang", where lang is US or DE.
#define DEFAULT_KBD_LANG_DE
*/
#define DEFAULT_KBD_LANG_US
/*
 * The frame rate. Setting this to a higher value will speed up the
 * emulation by a large factor, but animations will not be smooth.
 * I normally use a frame rate of 3-5 myself (use odd frame rates for
 * interlaced screens).
 * The run-time option is "-f n" where n is the frame rate.
#define DEFAULT_FRAMERATE 5
 */

/*
 * Define this if you want every line to be drawn twice. This is required
 * if you want circles to be circles and not ellipses. On the other hand,
 * non-aspect screens will be drawn twice as fast.
 * (To get a correct aspect, you could also try to fiddle with your
 * XF86Config file. I won't pay your broken monitor, though.)
 * The run-time option is "-d" to enable correct-aspect drawing.
*/
#define DEFAULT_WANT_ASPECT

/*
 * It is not necessary to select one of these: UAE will figure this out
 * at run-time. However, selecting the right one will result in a faster
 * executable.
 * Selecting the wrong one will result in a non-working executable.
#define X_8BIT_SCREEN
#define X_16BIT_SCREEN
 */

#ifdef __linux /* This line protects you if you don't use Linux */
/***************************************************************************
 * Linux specific options. Ignore these if you are using another OS.
 */

/*
 * Set this if you want to use SVGAlib instead of X. Don't forget to say
 * "make svga" instead of "make linux".
#define LINUX_SVGALIB
 */

/*
 * If you have defined LINUX_SVGALIB, select either SVGALIB_16BIT_SCREEN 
 * or SVGALIB_8BIT_SCREEN. If the combination of your graphics card and
 * SVGAlib supports 800x600 with 16 bit color, you should use the 16 bit 
 * screen to get better colors.
 * You may have to change the ?_WEIGHT defines if you
 * use a 16 bit screen.
#define SVGALIB_8BIT_SCREEN
 */
#define SVGALIB_16BIT_SCREEN
#define SVGALIB_R_WEIGHT 5
#define SVGALIB_G_WEIGHT 6
#define SVGALIB_B_WEIGHT 5

/*
 * Define if you have installed the joystick driver module.
#define LINUX_JOYSTICK
 */

/*
 * Define if you have installed the Linux sound driver and if you have read
 * the section about sound in the README.
 * Turn off sound at run-time with the "-S" option.
#define LINUX_SOUND
 */

/*
 * Try defining this if you don't get steady sound output. 
#define LINUX_SOUND_SLOW_MACHINE
 */

#endif /* __linux */

/***************************************************************************
 * Support for broken software. These options are set to default values
 * that are reasonable for most uses. You should not need to change these.
 */

/*
 * Some STUPID programs access a longword at an odd address and expect to
 * end up at the routine given in the vector for exception 3.
 * (For example, Katakis does this)
 * If you leave this commented in, memory accesses will be faster,
 * but some programs may fail for an obscure reason.
 */
#define NO_EXCEPTION_3

/*
 * If you use Kickstart 1.3, you had better define the following.
 * Useful for other programs too. It causes the emulator to
 * prevent a segfault if an address outside the 16M range is 
 * accessed.
 * Again, some programs may fail for some obscure reason.
 */
#define HAVE_BROKEN_SOFTWARE

/*
 * If you want to see the "Hardwired" demo, you need to define this.
 * Otherwise, it will say "This demo don't like Axel" - apparently, Axel
 * has a 68040.
#define WANT_SLOW_MULTIPLY
 */

/***************************************************************************
 * From here, you should not need to change anything.
 */

/*
 * The blitter emulator contains an optimization that is, strictly
 * speaking, invalid, but very unlikely to break anything. Leaving
 * this commented out will enable it.
#define NO_FAST_BLITTER
 */

/*
 * Similar: Disk accesses can be sped up. This isn't such a big win, though.
 * It hasn't been extensively tested and is turned off by default.
 */
#define NO_FAST_DISK

/*
 * This means more or less what it says. It will produce much
 * larger .cc files which you can only compile if you have lots
 * of physical memory. On my machine, I can't even preprocess them.
 * The CPU emulator might be a little faster if you really can get
 * this to compile.
#define HAVE_ONE_GIG_OF_MEMORY_TO_COMPILE_THIS
 */

/*
 * Better leave these untouched.
 */
#define SVGALIB_MODE_16 21
#define SVGALIB_MODE_8 11
