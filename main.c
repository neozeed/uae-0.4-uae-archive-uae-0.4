 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Main program 
  * 
  * (c) 1995 Bernd Schmidt, Ed Hanway
  */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "disk.h"
#include "debug.h"
#include "xwin.h"
#include "os.h"
#include "filesys.h"
#include "keybuf.h"

int framerate = DEFAULT_FRAMERATE;
bool dont_want_aspect = DEFAULT_NO_ASPECT;
bool use_fast_draw = true;
bool use_debugger = false;
bool use_slow_mem = false;
bool automount_uaedev = true;
bool produce_sound = true;
KbdLang keyboard_lang = DEFAULT_KBD_LANG;

#ifdef __unix

static bool mount_seen = false;

static void parse_cmdline(int argc, char **argv)
{
    int c;
    extern char *optarg;

    while(((c = getopt(argc, argv, "l:Df:dxasSm:M:")) != EOF)) switch(c) {
	case 'm':
	case 'M':
	{
	    /* mount file system (repeatable)
	     * syntax: [-m | -M] VOLNAME:/mount_point
	     * example: -M CDROM:/cdrom -m UNIXFS:./disk
	     */
	    static char buf[256];
	    char *s2;
	    bool readonly = (c == 'M');
	    
	    if (mount_seen)
		fprintf (stderr, "Multiple mounts not supported right now, sorry.\n");
	    else {
		mount_seen = true;
		strcpy(buf, optarg);
		s2 = strchr(buf, ':');
		if(s2) {
		    *s2++ = '\0';
		    add_filesys_unit(buf, s2, readonly);
		} else {
		    fprintf(stderr, "Usage: [-m | -M] VOLNAME:/mount_point\n");
		}
	    }
	}
	break;
	
     case 'S':
	produce_sound = false;
	break;

     case 'f':
	framerate = atoi(optarg);
	break;
	
     case 'd':
	dont_want_aspect = false;
	break;
	
     case 'x':
	use_fast_draw = false;
	break;

     case 'D':
	use_debugger = true;
	break;

     case 'l':
	if (0 == strcasecmp(optarg, "de"))
	    keyboard_lang = KBD_LANG_DE;
	else if (0 == strcasecmp(optarg, "us"))
	    keyboard_lang = KBD_LANG_US;
	break;
	
     case 'a':
	automount_uaedev = false;
	break;
	
     case 's':
	use_slow_mem = true;
	break;
    }
}
#endif

int main(int argc, char **argv)
{
    parse_cmdline(argc, argv);
    
    if (produce_sound && !init_sound()) {	
    	fprintf(stderr, "Sound driver unavailable: Sound output disabled\n");
	produce_sound = false;
    }

    init_joystick();
    keybuf_init ();    
    graphics_init();
    memory_init();
    custom_init();
    DISK_init();
    MC68000_init();
    MC68000_reset();

    debug();
    
    graphics_leave();
    close_joystick();
#if 0
    extern void dump_counts();
    dump_counts();
#endif
    return 0;
}
