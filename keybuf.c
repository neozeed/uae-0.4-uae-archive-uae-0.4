 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Keyboard buffer. Not really needed for X, but for SVGAlib and possibly
  * Mac and DOS ports.
  * 
  * (c) 1995 Bernd Schmidt
  */

#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "keybuf.h"

static int kpb_first, kpb_last;

static int keybuf[256];

bool keys_available (void)
{
    return kpb_first != kpb_last;
}

int get_next_key (void)
{
    int key;
    
    assert (kpb_first != kpb_last);
    
    key = keybuf[kpb_last];
    if (++kpb_last == 256) 
	kpb_last = 0;
    return key;    
}

void record_key (int kc)
{
    int kpb_next = kpb_first + 1;

    if (kpb_next == 256)
	kpb_next = 0;
    if (kpb_next == kpb_last) {
	fprintf(stderr, "Keyboard buffer overrun. Congratulations.\n");
	return;
    }
    keybuf[kpb_first] = kc;
    kpb_first = kpb_next;
}

void keybuf_init (void)
{
    kpb_first = kpb_last = 0;
}
