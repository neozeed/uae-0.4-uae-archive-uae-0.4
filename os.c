 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * OS specific functions
  * 
  * (c) 1995 Bernd Schmidt
  */

#include <stdio.h>

#ifdef __unix
#include <values.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "memory.h"
#include "custom.h"
#include "os.h"

#ifdef LINUX_JOYSTICK

#include <linux/joystick.h>

static int js0;
static bool joystickpresent = false;

struct JS_DATA_TYPE jscal;

void read_joystick(UWORD *dir, bool *button)
{
    static int minx = MAXINT, maxx = MININT,
               miny = MAXINT, maxy = MININT;
    bool left = false, right = false, top = false, bot = false;
    struct JS_DATA_TYPE buffer;
    int len;
    
    *dir = 0;
    *button = false;
    if (!joystickpresent)
    	return;
    
    len = read(js0, &buffer, sizeof(buffer));
    if (len != sizeof(buffer)) 
    	return;
    
    if (buffer.x < minx) minx = buffer.x;
    if (buffer.y < miny) miny = buffer.y;
    if (buffer.x > maxx) maxx = buffer.x;
    if (buffer.y > maxy) maxy = buffer.y;
    
    if (buffer.x < (minx + (maxx-minx)/3))
    	left = true;
    else if (buffer.x > (minx + 2*(maxx-minx)/3))
    	right = true;

    if (buffer.y < (miny + (maxy-miny)/3))
    	top = true;
    else if (buffer.y > (miny + 2*(maxy-miny)/3))
    	bot = true;
    	
    if (left) top = !top;
    if (right) bot = !bot;
    *dir = bot | (right << 1) | (top << 8) | (left << 9);
    *button = (buffer.buttons & 3) != 0;
}

void init_joystick(void)
{
    js0 = open("/dev/js0", O_RDONLY);
    if (js0 < 0)
    	return;
    joystickpresent = true;
}

void close_joystick(void)
{
    if (joystickpresent)
    	close(js0);
}
#else

void read_joystick(UWORD *dir, bool *button)
{
    *dir = 0;
    *button = false;
}

void init_joystick(void)
{
}

void close_joystick(void)
{
}
#endif

CPTR audlc[4], audpt[4];
UWORD audvol[4], audper[4], audlen[4], audwlen[4];
int audwper[4];
UWORD auddat[4];
int audsnum[4];

/* Audio states. This is not an exact representation of the Audio State Machine
 * (HRM p. 166), but a simplification. To be honest, I don't completely
 * understand that picture yet.
 */
int audst[4];

#ifdef LINUX_SOUND

#include <sys/soundcard.h>

static const int n_frames = 10;

/* The buffer is too large... */
static UWORD buffer[44100], *bufpt;
static BYTE snddata[4];

static int frames = 0;
static int smplcnt = 0;

static int sfd;
static bool have_sound;

bool init_sound (void)
{
    int tmp;
    sfd = open ("/dev/dsp", O_WRONLY);
    have_sound = !(sfd < 0);
    if (!have_sound) {
	return false;
    }
    
    tmp = 16;
    ioctl(sfd, SNDCTL_DSP_SAMPLESIZE, &tmp);
    
    tmp = 0;
    ioctl(sfd, SNDCTL_DSP_STEREO, &tmp);
    
    tmp = 44100;
    ioctl(sfd, SNDCTL_DSP_SPEED, &tmp);
    
    audst[0] = audst[1] = audst[2] = audst[3] = 0;
    bufpt = buffer;
    frames = n_frames;
    smplcnt = 0;
    return true;
}

static void channel_reload (int c)
{
    audst[c] = 1;
    audpt[c] = audlc[c];
    audwper[c] = 0;
    audwlen[c] = audlen[c];
    audsnum[c] = 1;    
}

void do_sound (void)
{
    smplcnt -= 227;
    while (smplcnt < 0) {
	int i;
	smplcnt += 80;
	for(i = 0; i < 4; i++) {
	    if (dmaen (1<<i)) {
		if (audst[i] == 0) {		
		    /* DMA was turned on for this channel */
		    channel_reload (i);
		    continue;
		}
		
		if (audwper[i] <= 0) {
		    audwper[i] += audper[i];
		    if (audst[i] == 1) {
			/*  Starting a sample, cause interrupt */
			put_word (0xDFF09C, 0x8000 | (0x80 << i));
			audst[i] = 2;
		    }
		    audsnum[i] ^= 1;
		    if (audsnum[i] == 0) {
			auddat[i] = get_word (audpt[i]);
			audpt[i] += 2;
			audwlen[i]--;
			if (audwlen[i] == 0) {
			    channel_reload (i);
			}
		    }		
		}
		snddata[i] = audsnum[i] ? auddat[i] : auddat[i] >> 8;
		audwper[i] -= 80;		
	    } else 
	    	audst[i] = snddata[i] = 0;
	}
	*bufpt++ = (snddata[0] * audvol[0] / 2 + snddata[1] * audvol[1] / 2
		    + snddata[2] * audvol[2] / 2 + snddata[3] * audvol[3] / 2);
    }
}

void flush_sound (void)
{
    if (--frames == 0) {	
    	write (sfd, buffer, 2*(bufpt - buffer));
	bufpt = buffer;
	frames = n_frames;
    }
}

#else
bool init_sound (void)
{
    return true;
}

void do_sound (void)
{
}

void flush_sound (void)
{
}
#endif

