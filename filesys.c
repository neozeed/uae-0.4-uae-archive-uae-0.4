 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Unix file system handler for AmigaDOS
  *
  * (c) 1996 Ed Hanway
  *
  * Version 0.1: 28-Jan-1996
  *
  * Based on example code (c) 1988 The Software Distillery
  * and published in Transactor for the Amiga, Volume 2, Issues 2-5.
  * (May - August 1989)
  *
  * Known limitations:
  * Does not support ACTION_INHIBIT (big deal).
  * Does not support any 2.0+ packet types (except ACTION_SAME_LOCK)
  * Does not actually enforce exclusive locks.
  * Does not support removable volumes.
  * May not return the correct error code in some cases.
  * Does not check for sane values passed by AmigaDOS.  May crash the emulation
  * if passed garbage values.
  *
  * TODO someday:
  * Support booting.
  * Implement real locking using flock.  Needs test cases.
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>

#ifdef __unix
#include <unistd.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef __linux
#include <sys/statfs.h>
#endif
#include <dirent.h>
#include <utime.h>
#include <errno.h>
#endif

#include "config.h"
#include "amiga.h"
#include "options.h"
#include "events.h"
#include "memory.h"
#include "custom.h"
#include "newcpu.h"
#include "xwin.h"
#include "autoconf.h"
#include "filesys.h"

#define MAKE_CASE_INSENSITIVE

typedef struct {
    char *devname; /* device name, e.g. UAE0: */
    char *volname; /* volume name, e.g. CDROM, WORK, etc. */
    char *rootdir; /* root unix directory */
    bool readonly; /* disallow write access? */
} UnitInfo;

#define MAX_UNITS 20
static int num_units = 0;
static UnitInfo ui[MAX_UNITS];

static ULONG devnameaddr[100];

static ULONG doslibname, fsdevname, filesysseglist;

/* strdup missing? */
static char*
my_strdup(const char*s)
{
    char*x = (char*)malloc(strlen(s) + 1);
    strcpy(x, s);
    return x;
}


void
add_filesys_unit(char *volname, char *rootdir, bool readonly)
{
    UnitInfo* u;
    static char buf[80];

    if (num_units >= MAX_UNITS) {
	fprintf(stderr, "Maximum number of unix file systems mounted.\n");
	return;
    }

    u = &ui[num_units];
    sprintf(buf, "UAE%d", num_units);
    u->devname = my_strdup(buf);
    u->volname = my_strdup(volname);
    u->rootdir = my_strdup(rootdir);
    u->readonly = readonly;

    num_units++;
}

#ifdef TRACING_ENABLED
#define TRACE(x)	printf x;
#define DUMPLOCK(x)	dumplock(x)
#else
#define TRACE(x)
#define DUMPLOCK(x)
#endif

/* minimal AmigaDOS definitions */

/* field offsets in DosPacket */
#define dp_Type (8)
#define dp_Res1	(12)
#define dp_Res2 (16)
#define dp_Arg1 (20)
#define dp_Arg2 (24)
#define dp_Arg3 (28)
#define dp_Arg4 (32)

/* result codes */
#define DOS_TRUE (-1L)
#define DOS_FALSE (0L)

/* packet types */
#define ACTION_CURRENT_VOLUME	7
#define ACTION_LOCATE_OBJECT	8
#define ACTION_RENAME_DISK	9
#define ACTION_FREE_LOCK	15
#define ACTION_DELETE_OBJECT	16
#define ACTION_RENAME_OBJECT	17
#define ACTION_COPY_DIR		19
#define ACTION_SET_PROTECT	21
#define ACTION_CREATE_DIR	22
#define ACTION_EXAMINE_OBJECT	23
#define ACTION_EXAMINE_NEXT	24
#define ACTION_DISK_INFO	25
#define ACTION_INFO		26
#define ACTION_FLUSH		27
#define ACTION_SET_COMMENT	28
#define ACTION_PARENT		29
#define ACTION_SET_DATE		34
#define ACTION_SAME_LOCK	40
#define ACTION_FIND_WRITE	1004
#define ACTION_FIND_INPUT	1005
#define ACTION_FIND_OUTPUT	1006
#define ACTION_END		1007
#define ACTION_SEEK		1008
#define ACTION_IS_FILESYSTEM	1027
#define ACTION_READ		'R'
#define ACTION_WRITE		'W'

#define DISK_TYPE		(0x444f5301) /* DOS\1 */

/* errors */
#define ERROR_NO_FREE_STORE     103
#define ERROR_OBJECT_IN_USE	202
#define ERROR_OBJECT_EXISTS     203
#define ERROR_DIR_NOT_FOUND     204
#define ERROR_OBJECT_NOT_FOUND  205
#define ERROR_ACTION_NOT_KNOWN  209
#define ERROR_OBJECT_WRONG_TYPE 212
#define ERROR_DISK_WRITE_PROTECTED 214
#define ERROR_DIRECTORY_NOT_EMPTY 216
#define ERROR_DEVICE_NOT_MOUNTED 218
#define ERROR_SEEK_ERROR	219
#define ERROR_DISK_FULL		221
#define ERROR_WRITE_PROTECTED 223
#define ERROR_NO_MORE_ENTRIES  232
#define ERROR_NOT_IMPLEMENTED	236

static long dos_errno(void)
{
    int e = errno;

    switch(e) {
     case ENOMEM:	return ERROR_NO_FREE_STORE;
     case EEXIST:	return ERROR_OBJECT_EXISTS;
     case EISDIR:	return ERROR_OBJECT_WRONG_TYPE;
     case ETXTBSY:	return ERROR_OBJECT_IN_USE;
     case EACCES:	return ERROR_WRITE_PROTECTED;
     case ENOENT:	return ERROR_OBJECT_NOT_FOUND;
     case ENOTDIR:	return ERROR_OBJECT_WRONG_TYPE;
     case EROFS:       	return ERROR_DISK_WRITE_PROTECTED;
     case ENOSPC:	return ERROR_DISK_FULL;
     case EBUSY:       	return ERROR_OBJECT_IN_USE;
     case ENOTEMPTY:	return ERROR_DIRECTORY_NOT_EMPTY;
	
     default:
	TRACE(("Unimplemented error %s\n", strerror(e)));
	return ERROR_NOT_IMPLEMENTED;
    }
}

/* handler state info */

typedef struct _unit {
    struct _unit *next;

    /* Amiga stuff */
    CPTR	dosbase;
    CPTR	volume;
    CPTR	port;	/* Our port */

    /* Native stuff */
    long	unit;	/* unit number */
    UnitInfo	ui;	/* unit startup info */
} Unit;

typedef struct {
    CPTR addr; /* addr of real packet */
    long type;
    long res1;
    long res2;
    long arg1;
    long arg2;
    long arg3;
    long arg4;
} DosPacket;

static char *
bstr(CPTR addr)
{
    static char buf[256];
    int n = get_byte(addr++);
    int i;
    for(i = 0; i < n; i++)
	buf[i] = get_byte(addr++);
    buf[i] = 0;
    return buf;
}

static Unit *units = NULL;
static int unit_num = 0;

static Unit*
find_unit(CPTR port)
{
    Unit* u;
    for(u = units; u; u = u->next)
	if(u->port == port)
	    break;

    return u;
}

static CPTR DosAllocMem(ULONG len)
{
    ULONG i;
    CPTR addr;

    regs.d[0] = len + 4;
    regs.d[1] = 1; /* MEMF_PUBLIC */
    addr = CallLib(regs.a[6], -198); /* AllocMem */

    if(addr) {
	put_long(addr, len);
	addr += 4;

	/* faster to clear memory here rather than use MEMF_CLEAR */
	for(i = 0; i < len; i += 4) 
	    put_long(addr + i, 0);
    }

    return addr;
}

static void DosFreeMem(CPTR addr)
{
    addr -= 4;
    regs.d[0] = get_long(addr) + 4;
    regs.a[1] = addr;
    CallLib(regs.a[6], -210); /* FreeMem */
}

static void
startup(DosPacket* packet)
{
    int i, namelen;
    char* devname = bstr(packet->arg1 << 2);
    char* s;
    Unit* unit;

    /* find UnitInfo with correct device name */
    s = strchr(devname, ':');
    if(s) *s = '\0';
#if 0 /* Kick 1.3 send us a bogus string. */
    for(i = 0; i < num_units; i++) {
	if(0 == strcmp(ui[i].devname, devname))
	    break;
    }
#endif
    
    i = 0;
    
    if(i == num_units || 0 != access(ui[i].rootdir, R_OK)) {
	fprintf(stderr, "Failed attempt to mount device %s\n", devname);
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DEVICE_NOT_MOUNTED;
	return;
    }

    unit = (Unit *) malloc(sizeof(Unit));
    unit->next = units;
    units = unit;

    unit->volume = 0;
    unit->port = regs.a[5];
    unit->unit = unit_num++;

    unit->ui.devname = ui[i].devname;
    unit->ui.volname = my_strdup(ui[i].volname); /* might free later for rename */
    unit->ui.rootdir = ui[i].rootdir;
    unit->ui.readonly = ui[i].readonly;

    TRACE(("**** STARTUP volume %s\n", unit->ui.volname));

    /* fill in our process in the device node */
    put_long((packet->arg3 << 2) + 8, unit->port);

    /* open dos.library */
    regs.d[0] = 0;
    regs.a[1] = doslibname;
    unit->dosbase = CallLib(regs.a[6], -552); /* OpenLibrary */

    {
	    CPTR rootnode = get_long(unit->dosbase + 34);
	    CPTR dos_info = get_long(rootnode + 24) << 2;
	    /* make new volume */
	    unit->volume = DosAllocMem(80 + 1 + 44);
	    put_long(unit->volume + 4, 2); /* Type = dt_volume */
	    put_long(unit->volume + 12, 0); /* Lock */
	    put_long(unit->volume + 16, 3800); /* Creation Date */
	    put_long(unit->volume + 20, 0);
	    put_long(unit->volume + 24, 0);
	    put_long(unit->volume + 28, 0); /* lock list */
	    put_long(unit->volume + 40, (unit->volume + 44) >> 2); /* Name */
	    namelen = strlen(unit->ui.volname);
	    put_byte(unit->volume + 44, namelen);
	    for(i = 0; i < namelen; i++)
		put_byte(unit->volume + 45 + i, unit->ui.volname[i]);
		
	    /* link into DOS list */
	    put_long(unit->volume, get_long(dos_info + 4));
	    put_long(dos_info + 4, unit->volume >> 2);
	}

    put_long(unit->volume + 8, unit->port);
    put_long(unit->volume + 32, DISK_TYPE);

    packet->res1 = DOS_TRUE;
}

static void
do_info(Unit* unit, DosPacket* packet, CPTR info)
{
    struct statfs statbuf;
#ifdef __linux
    if(-1 == statfs(unit->ui.rootdir, &statbuf))
#else
    if(-1 == statfs(unit->ui.rootdir, &statbuf, sizeof(struct statfs), 0))
#endif
    {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
    }

    put_long(info, 0); /* errors */
    put_long(info + 4, unit->unit); /* unit number */
    put_long(info + 8, unit->ui.readonly ? 80 : 82); /* state  */
    put_long(info + 12, statbuf.f_blocks); /* numblocks */
#ifdef __linux
    put_long(info + 16, statbuf.f_blocks - statbuf.f_bavail); /* inuse */
#else
    put_long(info + 16, statbuf.f_blocks - statbuf.f_bfree); /* inuse */
#endif
    put_long(info + 20, statbuf.f_bsize); /* bytesperblock */
    put_long(info + 24, DISK_TYPE); /* disk type */
    put_long(info + 28, unit->volume >> 2); /* volume node */
    put_long(info + 32, 0); /* inuse */
    packet->res1 = DOS_TRUE;
}

static void
action_disk_info(Unit* unit, DosPacket* packet)
{
    TRACE(("ACTION_DISK_INFO\n"));
    do_info(unit, packet, packet->arg1 << 2);
}

static void
action_info(Unit* unit, DosPacket* packet)
{
    TRACE(("ACTION_INFO\n"));
    do_info(unit, packet, packet->arg2 << 2);
}

typedef struct {
    char *path;
    int fd;
} Key;

static void
free_key(Key*k)
{
    free(k->path);
    free(k);
}

static void
dumplock(CPTR lock)
{
    if(!lock) {
	fprintf(stderr, "LOCK: 0x0\n");
	return;
    }
    fprintf(stderr,
	    "LOCK: 0x%lx { next=0x%lx, key=%s, mode=%ld, handler=0x%lx, volume=0x%lx }\n",
	    lock,
	    get_long(lock)<<2, ((Key*)get_long(lock+4))->path, get_long(lock+8),
	    get_long(lock+12), get_long(lock+16));
}

static char*
get_path(Unit* unit, const char *base, const char *rel)
{
    static char buf[1024];
    char *s = buf;
    char *p;
    char *r;

    int i;

    TRACE(("get_path(%s,%s)\n", base, rel));

    /* root-relative path? */
    for(i = 0; rel[i] && rel[i] != '/' && rel[i] != ':'; i++);
    if(':' == rel[i]) {
	base = unit->ui.rootdir; rel += i+1;
    }

    while(*base) {
	*s++ = *base++;
    }
    p = s; /* start of relative path */
    r = buf + strlen(unit->ui.rootdir); /* end of fixed path */
	
    while(*rel) {
	/* start with a slash? go up a level. */
	if('/' == *rel) {
	    while(s > r && '/' != *s)
		s--;
	    rel++;
	} else {
	    *s++ = '/';
	    while(*rel && '/' != *rel) {
		*s++ = *rel++;
	    }
	    if('/' == *rel)
		rel++;
	}
    }
	*s = 0;

#ifdef MAKE_CASE_INSENSITIVE
    TRACE(("path=\"%s\"\n", buf));
    /* go through each section of the path and if it does not exist,
     * scan its directory for any case insensitive matches
     */
    while(*p) {
	char *p2 = strchr(p+1, '/');
	char oldp2;
	if(!p2) {
	    p2 = p+1;
	    while(*p2) p2++;
	}
	oldp2 = *p2;
	*p2 = '\0';
	if(0 != access(buf, F_OK|R_OK)) {
	    DIR* dir;
	    struct dirent* de;

	    /* does not exist -- check dir for case insensitive match */
	    *p++ = '\0'; /* was '/' */
	    dir = opendir(buf);
	    if (dir) {
		while((de = readdir(dir))) {
		    if(0 == strcasecmp(de->d_name, p))
			break;
		}
		if(de) {
		    strcpy(p, de->d_name);
		}
		closedir(dir);
	    }
	    *--p = '/';
	}
	*p2 = oldp2;
	p = p2;
    }
#endif
    TRACE(("path=\"%s\"\n", buf));

    return my_strdup(buf);
}

static Key*
make_key(Unit* unit, CPTR lock, const char *name)
{
    Key *k = (Key*)malloc(sizeof(Key));

    k->fd = 0;

    if(!lock) {
	k->path = get_path(unit, unit->ui.rootdir, name);
    } else {
	Key*oldk = (Key*)get_long(lock + 4);
	TRACE(("key: 0x%08lx", (ULONG)oldk));
	TRACE((" \"%s\"\n", oldk->path));
	k->path = get_path(unit, oldk->path, name);
    }

    TRACE(("key=\"%s\"\n", k->path));
    return k;
}

static Key*
dup_key(Key*k)
{
    Key *newk = (Key*)malloc(sizeof(Key));
    newk->path = my_strdup(k->path);
    newk->fd = 0;
    return newk;
}

static CPTR
make_lock(Unit* unit, Key *key, long mode)
{
    /* allocate lock */
    CPTR lock = DosAllocMem(20);

    put_long(lock + 4, (ULONG) key);
    put_long(lock + 8, mode);
    put_long(lock + 12, unit->port);
    put_long(lock + 16, unit->volume >> 2);

    /* prepend to lock chain */
    put_long(lock, get_long(unit->volume + 28));
    put_long(unit->volume + 28, lock >> 2);

    DUMPLOCK(lock);
    return lock;
}

static void
free_lock(Unit* unit, CPTR lock)
{
    if(!lock)
	return;

    if(lock == get_long(unit->volume + 28) << 2) {
	put_long(unit->volume + 28, get_long(lock));
    } else {
	CPTR current = get_long(unit->volume + 28);
	CPTR next = 0;
	while(current) {
	    next = get_long(current << 2);
	    if(lock == next << 2)
		break;
	    current = next;
	}
	put_long(current << 2, get_long(lock));
    }
    free_key((Key*)get_long(lock + 4));
    DosFreeMem(lock);
}

static void
action_lock(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    CPTR name = packet->arg2 << 2;
    long mode = packet->arg3;
    int access_mode = (mode == -2) ? R_OK : R_OK|W_OK;
    Key *k;

    TRACE(("ACTION_LOCK(0x%lx, \"%s\", %d)\n",lock, bstr(name), mode));
    DUMPLOCK(lock);

    k = make_key(unit, lock, bstr(name));

    if(k && 0 == access(k->path, access_mode)) {
	packet->res1 = make_lock(unit, k, mode) >> 2;
    } else {
	if(k)
	    free_key(k);
	packet->res1 = 0;
	packet->res2 = ERROR_OBJECT_NOT_FOUND;
    }
}

static void
action_free_lock(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    TRACE(("ACTION_FREE_LOCK(0x%lx)\n", lock));
    DUMPLOCK(lock);
    
    free_lock(unit, lock);
    
    packet->res1 = DOS_TRUE;
}

static void
action_dup_lock(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;

    TRACE(("ACTION_DUP_LOCK(0x%lx)\n", lock));
    DUMPLOCK(lock);

    if(!lock) {
	packet->res1 = 0;
	return;
    }

    {
	CPTR oldkey = get_long(lock + 4);
	CPTR oldmode = get_long(lock + 8);
	packet->res1 = make_lock(unit, dup_key((Key*)oldkey), oldmode) >> 2;
    }
}

/* convert time_t to/from AmigaDOS time */
const int secs_per_day = 24 * 60 * 60;
const int diff = (8 * 365 + 2) * (24 * 60 * 60);

static void
get_time(time_t t, long* days, long* mins, long* ticks)
{
    /* time_t is secs since 1-1-1970 */
    /* days since 1-1-1978 */
    /* mins since midnight */
    /* ticks past minute @ 50Hz */

    t -= diff;
    *days = t / secs_per_day;
    t -= *days * secs_per_day;
    *mins = t / 60;
    t -= *mins * 60;
    *ticks = t * 50;
}

static time_t
put_time(long days, long mins, long ticks)
{
    time_t t;
    t = ticks / 50;
    t += mins * 60;
    t += days * secs_per_day;
    t += diff;
    
    return t;
}


typedef struct {
    char *path;
    DIR* dir;
} ExamineKey;

/* Since ACTION_EXAMINE_NEXT is so braindamaged, we have to keep
 * some of these around
 */

#define EXKEYS 100
static ExamineKey examine_keys[EXKEYS];
static int next_exkey = 0;

static void
free_exkey(ExamineKey* ek)
{
    free(ek->path);
    ek->path = 0;
    if(ek->dir)
	closedir(ek->dir);
}

static ExamineKey*
new_exkey(char *path)
{
    ExamineKey* ek= &examine_keys[next_exkey++];
    if(next_exkey==EXKEYS)
	next_exkey = 0;
    if(ek->path) {
	free_exkey(ek);
    }
    ek->path = my_strdup(path);
    ek->dir = 0;
    return ek;
}

static void
get_fileinfo(DosPacket* packet, CPTR info, char *buf)
{
    struct stat statbuf;
    long days, mins, ticks;

    if(-1 == stat(buf, &statbuf)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
    } else {
	put_long(info + 4, S_ISDIR(statbuf.st_mode) ? 2 : -3);
	{
	    /* file name */
	    int i = 8;
	    int n;
	    char *x = strrchr(buf,'/');
	    if(x)
		x++;
	    else
		x = buf;
	    TRACE(("name=\"%s\"\n", x));
	    n = strlen(x);
	    if(n > 106) n = 106;
	    put_byte(info + i++, n);
	    while(n--)
		put_byte(info + i++, *x++);
	    while(i < 108)
		put_byte(info + i++, 0);
	}
	put_long(info + 116,
		 (S_IRUSR & statbuf.st_mode ? 0 : (1<<3)) |
		 (S_IWUSR & statbuf.st_mode ? 0 : (1<<2)) |
		 (S_IXUSR & statbuf.st_mode ? 0 : (1<<1)) |
		 (S_IWUSR & statbuf.st_mode ? 0 : (1<<0)));
	put_long(info + 120, S_ISDIR(statbuf.st_mode) ? 2 : -3);
	put_long(info + 124, statbuf.st_size);
	put_long(info + 128, statbuf.st_blocks);
	get_time(statbuf.st_mtime, &days, &mins, &ticks);
	put_long(info + 132, days);
	put_long(info + 136, mins);
	put_long(info + 140, ticks);
	put_long(info + 144, 0); /* no comment */
	packet->res1 = DOS_TRUE;
    }
}

static void
do_examine(DosPacket* packet, ExamineKey* ek, CPTR info)
{
    static char buf[1024];
    struct dirent* de;

    if(!ek->dir) {
	ek->dir = opendir(ek->path);
    }
    if(!ek->dir) {
	free_exkey(ek);
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_MORE_ENTRIES;
	return;
    }
    
    de = readdir(ek->dir);
    
    while(de &&
	  (0 == strcmp(".", de->d_name) ||
	   0 == strcmp("..", de->d_name))) 
    {
	de = readdir(ek->dir);
    }

    if(!de) {
	TRACE(("no more entries\n"));
	free_exkey(ek);
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_MORE_ENTRIES;
	return;
    }

    TRACE(("entry=\"%s\"\n", de->d_name));

    sprintf(buf, "%s/%s", ek->path, de->d_name);

    get_fileinfo(packet, info, buf);
}

static void
action_examine_object(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    CPTR info = packet->arg2 << 2;
    char *path;
    ExamineKey* ek;
    
    TRACE(("ACTION_EXAMINE_OBJECT(0x%lx,0x%lx)\n", lock, info));
    DUMPLOCK(lock);

    if(!lock) {
	path = unit->ui.rootdir;
    } else {
	Key*k = (Key *)get_long(lock + 4);
	path = k->path;
    }
    
    get_fileinfo(packet, info, path);
    ek = new_exkey(path);
    put_long(info, (ULONG)ek);
}

static void
action_examine_next(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    CPTR info = packet->arg2 << 2;

    TRACE(("ACTION_EXAMINE_NEXT(0x%lx,0x%lx)\n", lock, info));
    DUMPLOCK(lock);

    do_examine(packet, (ExamineKey*)get_long(info), info);
}

static void
do_find(Unit* unit, DosPacket* packet, mode_t mode)
{
    CPTR fh = packet->arg1 << 2;
    CPTR lock = packet->arg2 << 2;
    CPTR name = packet->arg3 << 2;
    Key *k;
    struct stat st;
    
    TRACE(("ACTION_FIND_*(0x%lx,0x%lx,\"%s\",%d)\n",fh,lock,bstr(name),mode));
    DUMPLOCK(lock);

    k = make_key(unit, lock, bstr(name));
    if(!k) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }

    /* Fixme: may not quite be right */
    if (0 == stat (k->path, &st)) {
	if (S_ISDIR (st.st_mode)) {
	    packet->res1 = DOS_FALSE;
	    packet->res2 = ERROR_OBJECT_WRONG_TYPE;
	    return;
	}
    }

    k->fd = open(k->path, mode, 0777);

    if(k->fd < 0) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
	free_key(k);
	return;
    }
    success:
    put_long(fh+36, (ULONG)k);

    packet->res1 = DOS_TRUE;
}

static void
action_find_input(Unit* unit, DosPacket* packet)
{
    do_find(unit, packet, O_RDONLY);
}

static void
action_find_output(Unit* unit, DosPacket* packet)
{
    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }
    do_find(unit, packet, O_WRONLY|O_CREAT|O_TRUNC);
}

static void
action_find_write(Unit* unit, DosPacket* packet)
{
    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }
    do_find(unit, packet, O_RDWR);
}

static void
action_end(Unit* unit, DosPacket* packet)
{
    Key*k;
    TRACE(("ACTION_END(0x%lx)\n", packet->arg1));

    k = (Key*)(packet->arg1);
    close(k->fd);
    free_key(k);
    packet->res1 = DOS_TRUE;
}

static void
action_read(Unit* unit, DosPacket* packet)
{
    Key*k = (Key*)(packet->arg1);
    CPTR addr = packet->arg2;
    long size = packet->arg3;
    int actual;
    char *buf;
    
    TRACE(("ACTION_READ(%s,0x%lx,%ld)\n",k->path,addr,size));
    
    /* ugh this is inefficient but easy */
    buf = (char *)malloc(size);
    if(!buf) {
	packet->res1 = -1;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    
    actual = read(k->fd, buf, size);

    packet->res1 = actual;

    if(actual > 0) {
	int i;
	unsigned char *bp = (unsigned char *)buf;
	UWORD *realpt;
	if (addr & 1) { /* Eeek! */
	    actual--;
	    put_byte(addr, *bp++);
	    addr++;
	}
	if (valid_address (addr, actual)) {
	    realpt = get_real_address (addr);
	    for (i = 0; i < (actual & ~1); i+=2) {
		UWORD data = ((UWORD)(*bp++) << 8);
		data |= *bp++;
		*realpt++ = data;
	    }
	    if (actual & 1)
		put_byte (addr + actual - 1, *bp);
	} else {
	   /* fprintf (stderr, "unixfs warning: Bad pointer passed for read\n");*/
	    for(i = 0; i < actual; i++) 
		put_byte(addr + i, buf[i]);
	}
    } else {
	packet->res2 = dos_errno();
    }
    free(buf);
}

static void
action_write(Unit* unit, DosPacket* packet)
{
    Key*k = (Key*)(packet->arg1);
    CPTR addr = packet->arg2;
    long size = packet->arg3;
    char *buf;
    int i;

    TRACE(("ACTION_WRITE(%s,0x%lx,%ld)\n",k->path,addr,size));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    /* ugh this is inefficient but easy */
    buf = (char *)malloc(size);
    if(!buf) {
	packet->res1 = -1;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    
    for(i = 0; i < size; i++)
	buf[i] = get_byte(addr + i);

    packet->res1 = write(k->fd, buf, size);
    if(packet->res1 != size)
	packet->res2 = dos_errno();

    free(buf);
}

static void
action_seek(Unit* unit, DosPacket* packet)
{
    Key* k = (Key*) (packet->arg1);
    long pos = packet->arg2;
    long mode = packet->arg3;
    off_t res;
    long old;
    int whence = SEEK_CUR;
    if(mode > 0) whence = SEEK_END;
    if(mode < 0) whence = SEEK_SET;

    TRACE(("ACTION_SEEK(%s,%d,%d)\n",k->path,pos,mode));

    old = lseek(k->fd, 0, SEEK_CUR);
    res = lseek(k->fd, pos, whence);

    if(-1 == res)
	packet->res1 = res;
    else
	packet->res1 = old;
}

static void
action_set_protect(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg2 << 2;
    CPTR name = packet->arg3 << 2;
    ULONG mask = packet->arg4;
    struct stat statbuf;
    mode_t mode;
    Key *k;

    TRACE(("ACTION_SET_PROTECT(0x%lx,\"%s\",0x%lx)\n",lock,bstr(name),mask));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    k = make_key(unit, lock, bstr(name));
    if(!k) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
		
    if(-1 == stat(k->path, &statbuf)) {
	free_key(k);
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_OBJECT_NOT_FOUND;
	return;
    }

    mode = statbuf.st_mode;

    if(mask & (1 << 3))
	mode &= ~S_IRUSR;
    else
	mode |= S_IRUSR;
    
    if(mask & (1 << 2) || mask & (1 << 0))
	mode &= ~S_IWUSR;
    else
	mode |= S_IWUSR;

    if(mask & (1 << 1))
	mode &= ~S_IXUSR;
    else
	mode |= S_IXUSR;

    if(-1 == chmod(k->path, mode)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
    } else {
	packet->res1 = DOS_TRUE;
    }
    free_key(k);
}

static void
action_same_lock(Unit* unit, DosPacket* packet)
{
    CPTR lock1 = packet->arg1 << 2;
    CPTR lock2 = packet->arg2 << 2;

    TRACE(("ACTION_SAME_LOCK(0x%lx,0x%lx)\n",lock1,lock2));
    DUMPLOCK(lock1); DUMPLOCK(lock2);

    if(!lock1 || !lock2) {
	packet->res1 = (lock1 == lock2) ? DOS_TRUE : DOS_FALSE;
    } else {
	Key* key1 = (Key*) get_long(lock1 + 4);
	Key* key2 = (Key*) get_long(lock2 + 4);
	packet->res1 = (0 == strcmp(key1->path, key2->path)) ? DOS_TRUE : DOS_FALSE;
    }
}

static void
action_parent(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    Key*k;

    TRACE(("ACTION_PARENT(0x%lx)\n",lock));
    
    if(!lock) {
	packet->res1 = 0;
	packet->res2 = 0;
	return;
    }

    k = dup_key((Key*) get_long(lock + 4));
    if(0 == strcmp(k->path, unit->ui.rootdir)) {
	free_key(k);
	packet->res1 = 0;
	packet->res2 = 0;
	return;
    }
    {
	char *x = strrchr(k->path,'/');
	if(!x) {
	    free_key(k);
	    packet->res1 = 0;
	    packet->res2 = 0;
	    return;
	} else {
	    *x = '\0';
	}
    }
    packet->res1 = make_lock(unit, k, -2) >> 2;
}

static void
action_create_dir(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    CPTR name = packet->arg2 << 2;
    Key* k;

    TRACE(("ACTION_CREATE_DIR(0x%lx,\"%s\")\n",lock,bstr(name)));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    k = make_key(unit, lock, bstr(name));

    if(!k) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    if(-1 == mkdir(k->path, 0777)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
	free_key(k);
	return;
    }
    packet->res1 = make_lock(unit, k, -2) >> 2;
}

static void
action_delete_object(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg1 << 2;
    CPTR name = packet->arg2 << 2;
    Key* k;
    struct stat statbuf;

    TRACE(("ACTION_DELETE_OBJECT(0x%lx,\"%s\")\n",lock,bstr(name)));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    k = make_key(unit, lock, bstr(name));

    if(!k) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    if(-1 == stat(k->path, &statbuf)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
	free_key(k);
	return;
    }
    if(S_ISDIR(statbuf.st_mode)) {
	if(-1 == rmdir(k->path)) {
	    packet->res1 = DOS_FALSE;
	    packet->res2 = dos_errno();
	    free_key(k);
	    return;
	}
    } else {
	if(-1 == unlink(k->path)) {
	    packet->res1 = DOS_FALSE;
	    packet->res2 = dos_errno();
	    free_key(k);
	    return;
	}
    }
    free_key(k);
    packet->res1 = DOS_TRUE;
}

static void
action_set_date(Unit* unit, DosPacket* packet)
{
    CPTR lock = packet->arg2 << 2;
    CPTR name = packet->arg3 << 2;
    CPTR date = packet->arg4;
    Key* k;
    struct utimbuf ut;
    
    TRACE(("ACTION_SET_DATE(0x%lx,\"%s\")\n",lock,bstr(name)));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    ut.actime = ut.modtime = put_time(get_long(date),get_long(date+4),get_long(date+8));
    k = make_key(unit, lock, bstr(name));

    if(!k) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    if(-1 == utime(k->path, &ut)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
	free_key(k);
	return;
    }
    free_key(k);
    packet->res1 = DOS_TRUE;
}

static void
action_rename_object(Unit* unit, DosPacket* packet)
{
    CPTR lock1 = packet->arg1 << 2;
    CPTR name1 = packet->arg2 << 2;
    Key* k1;
    CPTR lock2 = packet->arg3 << 2;
    CPTR name2 = packet->arg4 << 2;
    Key* k2;

    TRACE(("ACTION_RENAME_OBJECT(0x%lx,\"%s\",",lock1,bstr(name1)));
    TRACE(("0x%lx,\"%s\")\n",lock2,bstr(name2)));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    k1 = make_key(unit, lock1, bstr(name1));
    if(!k1) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    k2 = make_key(unit, lock2, bstr(name2));
    if(!k2) {
	free_key(k1);
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_NO_FREE_STORE;
	return;
    }
    
    if(-1 == rename(k1->path, k2->path)) {
	packet->res1 = DOS_FALSE;
	packet->res2 = dos_errno();
	free_key(k1);
	free_key(k2);
	return;
    }
    free_key(k1);
    free_key(k2);
    packet->res1 = DOS_TRUE;
}

static void
action_current_volume(Unit* unit, DosPacket* packet)
{
    packet->res1 = unit->volume >> 2;
}

static void
action_rename_disk(Unit* unit, DosPacket* packet)
{
    CPTR name = packet->arg1 << 2;
    int i;
    int namelen;

    TRACE(("ACTION_RENAME_DISK(\"%s\")\n", bstr(name)));

    if(unit->ui.readonly) {
	packet->res1 = DOS_FALSE;
	packet->res2 = ERROR_DISK_WRITE_PROTECTED;
	return;
    }

    /* get volume name */
    namelen = get_byte(name++);
    free(unit->ui.volname);
    unit->ui.volname = (char *) malloc(namelen + 1);
    for(i = 0; i < namelen; i++)
	unit->ui.volname[i] = get_byte(name++);
    unit->ui.volname[i] = 0;
    
    put_byte(unit->volume + 44, namelen);
    for(i = 0; i < namelen; i++)
	put_byte(unit->volume + 45 + i, unit->ui.volname[i]);
    
    packet->res1 = DOS_TRUE;
}

static void
action_is_filesystem(Unit* unit, DosPacket* packet)
{
    packet->res1 = DOS_TRUE;
}

static void
action_flush(Unit* unit, DosPacket* packet)
{
    /* sync(); */ /* pretty drastic, eh */
    packet->res1 = DOS_TRUE;
}

static ULONG
filesys_handler(void)
{
    DosPacket packet;
    Unit *unit = find_unit(regs.a[5]);
	
    /* got DosPacket in A4 */
    packet.addr = regs.a[4];
    packet.type = get_long(packet.addr + dp_Type);
    packet.res1 = get_long(packet.addr + dp_Res1);
    packet.res2 = get_long(packet.addr + dp_Res2);
    packet.arg1 = get_long(packet.addr + dp_Arg1);
    packet.arg2 = get_long(packet.addr + dp_Arg2);
    packet.arg3 = get_long(packet.addr + dp_Arg3);
    packet.arg4 = get_long(packet.addr + dp_Arg4);

    if(!unit) {
	startup(&packet);
	put_long(packet.addr + dp_Res1, packet.res1);
	put_long(packet.addr + dp_Res2, packet.res2); 
	return 0;
    }

    if(!unit->volume) {
	printf("no volume\n");
	return 0;
    }

    switch(packet.type) {
     case ACTION_LOCATE_OBJECT:
	action_lock(unit, &packet);
	break;

     case ACTION_FREE_LOCK:
	action_free_lock(unit, &packet);
	break;

     case ACTION_COPY_DIR:
	action_dup_lock(unit, &packet);
	break;

     case ACTION_DISK_INFO:
	action_disk_info(unit, &packet);
	break;

     case ACTION_INFO:
	action_info(unit, &packet);
	break;

     case ACTION_EXAMINE_OBJECT:
	action_examine_object(unit, &packet);
	break;

     case ACTION_EXAMINE_NEXT:
	action_examine_next(unit, &packet);
	break;

     case ACTION_FIND_INPUT:
	action_find_input(unit, &packet);
	break;

     case ACTION_FIND_WRITE:
	action_find_write(unit, &packet);
	break;

     case ACTION_FIND_OUTPUT:
	action_find_output(unit, &packet);
	break;

     case ACTION_END:
	action_end(unit, &packet);
	break;

     case ACTION_READ:
	action_read(unit, &packet);
	break;

     case ACTION_WRITE:
	action_write(unit, &packet);
	break;
	
     case ACTION_SEEK:
	action_seek(unit, &packet);
	break;
	
     case ACTION_SET_PROTECT:
	action_set_protect(unit, &packet);
	break;
	
     case ACTION_SAME_LOCK:
	action_same_lock(unit, &packet);
	break;
	
     case ACTION_PARENT:
	action_parent(unit, &packet);
	break;
	
     case ACTION_CREATE_DIR:
	action_create_dir(unit, &packet);
	break;
	
     case ACTION_DELETE_OBJECT:
	action_delete_object(unit, &packet);
	break;
	
     case ACTION_RENAME_OBJECT:
	action_rename_object(unit, &packet);
	break;
	
     case ACTION_SET_DATE:
	action_set_date(unit, &packet);
	break;
	
     case ACTION_CURRENT_VOLUME:
	action_current_volume(unit, &packet);
	break;
	
     case ACTION_RENAME_DISK:
	action_rename_disk(unit, &packet);
	break;
	
     case ACTION_IS_FILESYSTEM:
	action_is_filesystem(unit, &packet);
	break;
	
     case ACTION_FLUSH:
	action_flush(unit, &packet);
	break;
	
     default:
	TRACE(("*** UNSUPPORTED PACKET %ld\n", packet.type));
	packet.res1 = DOS_FALSE;
	packet.res2 = ERROR_ACTION_NOT_KNOWN;
	break;
    }

    put_long(packet.addr + dp_Res1, packet.res1);
    put_long(packet.addr + dp_Res2, packet.res2); 
    TRACE(("reply: %8lx, %ld\n", packet.res1, packet.res2));

    return 0;
}

static ULONG
filesys_init(void)
{
    ULONG tmp1, tmp2;
    bool have36 = false;
    int i;

    regs.d[0] = 88; regs.d[1] = 1; /* MEMF_PUBLIC */
    tmp1 = CallLib (regs.a[6], -198); /* AllocMem() */
    if (tmp1 == 0) {
	fprintf(stderr, "Not enough memory for filesystem!\n");
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
    put_long (tmp1+4, fsdevname);
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

    /* re-use the same parameter packet to make each
     * dos node, which will then get tweaked
     */
    for(i = 0; i < num_units; i++) {
	put_long (tmp1, devnameaddr[i]);
	put_long (tmp1+8, i); /* Unit no. (not really necessary) */
	regs.a[0] = tmp1;
	tmp2 = CallLib (regs.a[4], -144); /* MakeDosNode() */
	put_long(tmp2+8, 0x0); /* dn_Task */
	put_long(tmp2+16, 0x0); /* dn_Handler */
	put_long(tmp2+20, 4000); /* dn_StackSize */
	put_long(tmp2+32, filesysseglist >> 2); /* dn_SegList */
	put_long(tmp2+36, (ULONG)-1); /* dn_GlobalVec */
	
	regs.a[0] = tmp2;
	regs.d[0] = (ULONG)-1;
	regs.a[1] = 0;
	regs.d[1] = 0;
	CallLib (regs.a[4], -150); /* AddDosNode() */
    }

    regs.a[1] = tmp1;
    regs.d[0] = 88;
    CallLib (regs.a[6], -210); /* FreeMem() */

    regs.a[1] = regs.a[4];
    CallLib (regs.a[6], -414); /* CloseLibrary() */

    return 0;
}

void
filesys_install(void)
{
    ULONG begin, end, resname, resid;
    int i;

    if(0 == num_units)
	return;

    resname = ds("UAEunixfs.resource");
    resid = ds("UAE unixfs 0.1");

    doslibname = ds("dos.library");
    fsdevname = ds("unixfs.device"); /* does not really exist */

    for(i = 0; i < num_units; i++) {
	devnameaddr[i] = ds(ui[i].devname);
    }

    begin = here();
    dw(0x4AFC); /* RTC_MATCHWORD */
    dl(begin); /* our start address */
    dl(0); /* Continue scan here */
    dw(0x0101); /* RTF_COLDSTART; Version 1 */
    dw(0x0805); /* NT_RESOURCE; pri 5 */
    dl(resname); /* name */
    dl(resid); /* ID */
    dl(here() + 4); /* Init area: directly after this */

    calltrap(deftrap(filesys_init)); dw(RTS);

    /* align */
    align(4);
    /* Fake seglist */
    dl(16);
    filesysseglist = here();
    dl(0); /* NextSeg */

    /* start of code */

    /* I don't trust calling functions that Wait() directly,
     * so here's a little bit of 68000 code to receive and send our
     * DosPackets
     */
    dw(0x2c79); dl(4);		/* move.l	$4,a6 */
    dw(0x2279); dl(0);		/* move.l	0,a1 */
    dw(0x4eae); dw(0xfeda);	/* jsr		FindTask(a6) */
    dw(0x2040);				/* move.l	d0,a0 */
    dw(0x4be8); dw(0x005c);	/* lea.l	pr_MsgPort(a0),a5 */
					/* loop: */
    dw(0x204d);				/* move.l	a5,a0 */
    dw(0x4eae); dw(0xfe80);	/* jsr		WaitPort(a6) */
    dw(0x204d);				/* move.l	a5,a0 */
    dw(0x4eae); dw(0xfe8c);	/* jsr		GetMsg(a6) */
    dw(0x2840);				/* move.l	d0,a4 */
    dw(0x286c); dw(10);		/* move.l	LN_NAME(a4),a4 */
    calltrap(deftrap(filesys_handler));
    dw(0x226c);	dw(0);		/* move.l	dp_Link(a4),a1 */
    dw(0x206c); dw(4);		/* move.l	dp_Port(a4),a0 */
    dw(0x294d); dw(4);		/* move.l	a5,dp_Port(a4) */
    dw(0x4eae); dw(0xfe92);	/* jsr		PutMsg(a6) */
    dw(0x60d6); 			/* bra.s	loop */

    end = here();
    org(begin + 6);
    dl(end);

    org(end);
}

