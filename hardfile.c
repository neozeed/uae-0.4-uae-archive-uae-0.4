 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * AutoConfig devices
  *
  * (c) 1995 Bernd Schmidt
  */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __unix
#include <unistd.h>
#include <fcntl.h>
#endif

#include "config.h"

#ifdef __mac__
#include <unix.h>
#endif

#include "amiga.h"
#include "options.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "disk.h"
#include "xwin.h"
#include "autoconf.h"
#include "hardfile.h"

static int opencount = 0;
static int uaedevfd;
static int numtracks = 512;

static ULONG dosname, devname;

static ULONG hardfile_init(void)
{
    ULONG tmp1, tmp2, tmp3;
    bool have36 = false;
    ULONG retval = regs.d[0];

    if (!automount_uaedev)
	return retval;

    regs.d[0] = 88; regs.d[1] = 1; /* MEMF_PUBLIC */
    tmp1 = CallLib (regs.a[6], -198); /* AllocMem() */
    if (tmp1 == 0) {
	fprintf(stderr, "Not enough memory for uae.device!\n");
	return 0;
    }
    /* Open expansion.lib */
    regs.d[0] = 36; /* Let's try this... */
    regs.a[1] = explibname;
    regs.a[4] = CallLib (regs.a[6], -552); /* OpenLibrary() */
    if (regs.a[4])
	have36 = true;
    else {
	regs.d[0] = 0;
	regs.a[1] = explibname;
	regs.a[4] = CallLib (regs.a[6], -552); /* OpenLibrary() */
    }
    put_long (tmp1, dosname);
    put_long (tmp1+4, devname);
    put_long (tmp1+8, 0); /* Unit no. */
    put_long (tmp1+12, 0); /* Device flags */
    put_long (tmp1+16, 16); /* Env. size */
    put_long (tmp1+20, 128); /* 512 bytes/block */
    put_long (tmp1+24, 0); /* unused */
    put_long (tmp1+28, 1); /* heads */
    put_long (tmp1+32, 1); /* unused */
    put_long (tmp1+36, 32); /* secs per track */
    put_long (tmp1+40, 1); /* reserved blocks */
    put_long (tmp1+44, 0); /* unused */
    put_long (tmp1+48, 0); /* interleave */
    put_long (tmp1+52, 0); /* lowCyl */
    put_long (tmp1+56, 511); /* upperCyl */
    put_long (tmp1+60, 0); /* Number of buffers */
    put_long (tmp1+64, 0); /* Buffer mem type */
    put_long (tmp1+68, 0x7FFFFFFF); /* largest transfer */
    put_long (tmp1+72, ~1); /* addMask (?) */
    put_long (tmp1+76, (ULONG)-1); /* bootPri */
    if (have36)
	put_long (tmp1+80, 0x444f5301); /* DOS\1 */
    else
	put_long (tmp1+80, 0x444f5300); /* DOS\0 */
    
    put_long (tmp1+84, 0); /* pad */
    regs.a[0] = tmp1;
    tmp2 = CallLib (regs.a[4], -144); /* MakeDosNode() */
    regs.a[0] = tmp2;
    regs.d[0] = (ULONG)-1;
    regs.a[1] = 0;
    regs.d[1] = 0;
    CallLib (regs.a[4], -150); /* AddDosNode() */
#if 0
#if 0
    if (have36)
	CallLib (regs.a[4], /*-150*/-36); /* AddDosNode() */
    else {
#endif
        /* We could also try to call AddBootNode() here - but we don't have
	 * a ConfigDev. */
	regs.d[0] = 20;
	regs.d[1] = 0;
	tmp3 = CallLib (regs.a[6], -198);
	if (tmp3 == 0) {
	    fprintf(stderr, "Not enough memory for uae.device bootnode!\n");
	    return 0;
	}
	put_word (tmp3 + 14, 0);
	put_long (tmp3 + 16, tmp2);
	put_word (tmp3 + 8, 0x1005);
	put_long (tmp3 + 10, 0);
	put_long (tmp3 + 0, 0);
	put_long (tmp3 + 4, 0);
	regs.a[0] = regs.a[4] + 74; /* MountList */
	regs.a[1] = tmp3;
	CallLib (regs.a[6], -270); /* Enqueue() */
#if 0
    }
#endif
#endif
    
    regs.a[1] = tmp1;
    regs.d[0] = 88;
    CallLib (regs.a[6], -210); /* FreeMem() */
    
    regs.a[1] = regs.a[4];
    CallLib (regs.a[6], -414); /* CloseLibrary() */
    
    return retval;
}
	
static ULONG hardfile_open(void)
{
    CPTR tmp1 = regs.a[1]; /* IOReq */
    
    /* Check unit number */
    if (regs.d[0] == 0) {	    
	opencount++;
	put_word (regs.a[6]+32, get_word (regs.a[6]+32) + 1);
	put_long (tmp1+24, 0); /* io_Unit */
	put_byte (tmp1+31, 0); /* io_Error */
	put_byte (tmp1+8, 7); /* ln_type = NT_REPLYMSG */
	return 0;
    }

    put_long (tmp1+20, (ULONG)-1);
    put_byte (tmp1+31, (UBYTE)-1);
    return (ULONG)-1;
}

static ULONG hardfile_close(void)
{
    opencount--;
    put_word (regs.a[6]+32, get_word (regs.a[6]+32) - 1);

    return regs.d[0];
}
	
static ULONG hardfile_expunge(void)
{
    return 0; /* Simply ignore this one... */
}
	
static ULONG hardfile_beginio(void)
{
    ULONG tmp1, tmp2, dataptr, offset;
    ULONG retval = regs.d[0];

    tmp1 = regs.a[1];
    put_byte (tmp1+8, 5); /* set ln_type to NT_MESSAGE */
    put_byte (tmp1+31, 0); /* no error yet */
    tmp2 = get_word (tmp1+28); /* io_Command */
    switch (tmp2) {
     case 2: /* Read */
	dataptr = get_long (tmp1 + 40);
	if (dataptr & 1)
	    goto bad_command;
	offset = get_long (tmp1 + 44);
	if (offset & 511)
	    goto bad_command;
	tmp2 = get_long (tmp1 + 36); /* io_Length */
	if (tmp2 & 511)
	    goto bad_command;
	if (tmp2 + offset > (ULONG)numtracks * 32 * 512)
	    goto bad_command;
	
	put_long (tmp1 + 32, tmp2); /* set io_Actual */
	lseek (uaedevfd, offset, SEEK_SET);
	while (tmp2) {
	    int i;
	    char buffer[512];
	    read (uaedevfd, buffer, 512);
	    for (i = 0; i < 512; i++, dataptr++)
		put_byte(dataptr, buffer[i]);
	    tmp2 -= 512;
	}
	break;
	    
     case 3: /* Write */
     case 11: /* Format */
	dataptr = get_long (tmp1 + 40);
	if (dataptr & 1)
	    goto bad_command;
	offset = get_long (tmp1 + 44);
	if (offset & 511)
	    goto bad_command;
	tmp2 = get_long (tmp1 + 36); /* io_Length */
	if (tmp2 & 511)
	    goto bad_command;
	if (tmp2 + offset > (ULONG)numtracks * 32 * 512)
	    goto bad_command;
	
	put_long (tmp1 + 32, tmp2); /* set io_Actual */
	lseek (uaedevfd, offset, SEEK_SET);
	while (tmp2) {
	    char buffer[512];
	    int i;
	    for (i=0; i < 512; i++, dataptr++)
		buffer[i] = get_byte(dataptr);
	    write (uaedevfd, buffer, 512);
	    tmp2 -= 512;
	}
	break;
	
	bad_command:
	break;
	
     case 18: /* GetDriveType */
	put_long (tmp1 + 32, 1); /* not exactly a 3.5" drive, but... */
	break;
	
     case 19: /* GetNumTracks */
	put_long (tmp1 + 32, numtracks); 
	break;
	
	/* Some commands that just do nothing and return zero */
     case 4: /* Update */
     case 5: /* Clear */
     case 9: /* Motor */
     case 10: /* Seek */
     case 12: /* Remove */
     case 13: /* ChangeNum */
     case 14: /* ChangeStatus */
     case 15: /* ProtStatus */
     case 20: /* AddChangeInt */
     case 21: /* RemChangeInt */
	put_long (tmp1+32, 0); /* io_Actual */
	retval = 0;
	break;
	
     default:
	/* Command not understood. */
	put_byte (tmp1+31, (UBYTE)-3); /* io_Error */
	retval = 0;
	break;
    }
    if ((get_byte (tmp1+30) & 1) == 0) {
	/* Not IOF_QUICK -- need to ReplyMsg */
	regs.a[1] = tmp1;
	CallLib (get_long(4), -378);
    }
    return retval;
}
	
static ULONG hardfile_abortio(void)
{
    return (ULONG)-3;
}

void hardfile_install(void)
{
    ULONG devid, functable, datatable, inittable, begin, end;
    ULONG initroutine, openfunc, closefunc, expungefunc;
    ULONG nullfunc, beginiofunc, abortiofunc;

    uaedevfd = open ("hardfile", O_RDWR);
    
    if (uaedevfd < 0)
    	return;
    
    devname = ds("uae.device");
    devid = ds("uae 0.4"); /* ID */
    dosname = ds("UAEHF"); /* This is the DOS name */

    begin = here();
    dw(0x4AFC); /* RTC_MATCHWORD */
    dl(begin); /* our start address */
    dl(0); /* Continue scan here */
    dw(0x8101); /* RTF_AUTOINIT|RTF_COLDSTART; Version 1 */
    dw(0x0305); /* NT_DEVICE; pri 5 */
    dl(devname); /* name */
    dl(devid); /* ID */
    dl(here() + 4); /* Init area: directly after this */
    /* Init area starts here */
    dw(0x0000); /* Not really sure what to put here */
    dw(0x0100);
    inittable = here();
    dl(0); dl(0); dl(0); /* skip 3 longs */

    /* InitRoutine */
    initroutine = here();
    calltrap(deftrap(hardfile_init)); dw(RTS);

    /* Open */
    openfunc = here();
    calltrap(deftrap(hardfile_open)); dw(RTS);

    /* Close */
    closefunc = here();
    calltrap(deftrap(hardfile_close)); dw(RTS);

    /* Expunge */
    expungefunc = here();
    calltrap(deftrap(hardfile_expunge)); dw(RTS);
    
    /* Null */
    nullfunc = here();
    dw(0x7000); /* return 0; */
    dw(RTS);

    /* BeginIO */
    beginiofunc = here();
    calltrap(deftrap(hardfile_beginio)); dw(RTS);

    /* AbortIO */
    abortiofunc = here();
    calltrap(deftrap(hardfile_abortio)); dw(RTS);
    
    /* FuncTable */
    functable = here();
    dl(openfunc); /* Open */
    dl(closefunc); /* Close */
    dl(expungefunc); /* Expunge */
    dl(nullfunc); /* Null */
    dl(beginiofunc); /* BeginIO */
    dl(abortiofunc); /* AbortIO */
    dl(0xFFFFFFFF); /* end of table */

    /* DataTable */
    datatable = here();
    dw(0xE000); /* INITBYTE */
    dw(0x0008); /* LN_TYPE */
    dw(0x0300); /* NT_DEVICE */
    dw(0xC000); /* INITLONG */
    dw(0x000A); /* LN_NAME */
    dl(devname);
    dw(0xE000); /* INITBYTE */
    dw(0x000E); /* LIB_FLAGS */
    dw(0x0600); /* LIBF_SUMUSED | LIBF_CHANGED */
    dw(0xD000); /* INITWORD */
    dw(0x0014); /* LIB_VERSION */
    dw(0x0004); /* 0.4 */
    dw(0xD000);
    dw(0x0016); /* LIB_REVISION */
    dw(0x0000);
    dw(0xC000);
    dw(0x0018); /* LIB_IDSTRING */
    dl(devid);
    dw(0x0000); /* end of table */

    end = here();

    org(inittable);
    dl(functable);
    dl(datatable);
    dl(initroutine);

    org(begin + 6);
    dl(end);

    org(end);
}
