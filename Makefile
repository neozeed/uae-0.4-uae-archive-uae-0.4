# Makefile for UAE (Un*x Amiga Emulator)
#
# Copyright 1995,1996 Bernd Schmidt
# Copyright 1996 Ed Hanway

.SUFFIXES: .o .c .h

# If you have a Pentium, you can get better code if you use the commented
# line.
# Only gcc 2.7.x understands this. Use an empty "PENTIUMOPT =" line
# if you use gcc 2.6.x, or a non-x86 architecture.
# PENTIUMOPT = -malign-jumps=0 -malign-loops=0
PENTIUMOPT =

# Don't change anything below this line, except maybe the search paths
# for X11 (X11R6 could be X11R5 on your system)

RELEASE=yes

INCLUDES=-Iinclude

OBJS = main.o newcpu.o memory.o debug.o custom.o cia.o disk.o $(GFXOBJS) \
       autoconf.o os.o ersatz.o filesys.o hardfile.o keybuf.o \
       cpu0.o cpu1.o cpu2.o cpu3.o cpu4.o cpu5.o cpu6.o cpu7.o \
       cpu8.o cpu9.o cpuA.o cpuB.o cpuC.o cpuD.o cpuE.o cpuF.o cputbl.o
       
all:
	@echo "Use one of the following:"
	@echo "make generic -- if nothing else works"
	@echo "make withgcc -- if nothing else works, but you have gcc"
	@echo "make sgi -- SGI IRIX 5.3"
	@echo "make linux -- Linux (X Window System)"
	@echo "make svga -- Linux svgalib"
	@echo "make hpux -- Try to use HP-SUX compiler. Use GCC if it's available."
	@echo "make osf -- DEC ALPHA with OSF/1"

generic:
	$(MAKE) INCDIRS='-I/usr/include/X11' \
		GFXLDFLAGS='-L/usr/X11/lib -lX11 -lXext' \
		GFXOBJS=xwin.o \
		progs

withgcc:
	$(MAKE) CC='gcc' \
	        INCDIRS='-I/usr/include/X11' \
		GFXLDFLAGS='-L/usr/X11/lib -lX11 -lXext' \
		GFXOBJS=xwin.o \
		progs

osf:
	$(MAKE) CC=gcc \
		INCDIRS='-I/usr/X11R6/include' \
		GFXLDFLAGS='-L/usr/X11R6/lib -lX11' \
		GFXOBJS=xwin.o \
		CFLAGS='-ansi -pedantic \
		-Wall -ggdb' \
		progs

hpux:
	$(MAKE) CC=$(CXX) \
		INCDIRS='-I/usr/include/X11' \
		GFXLDFLAGS='-lX11 -lXext' \
		GFXOBJS=xwin.o \
		CFLAGS='-Aa +O1' \
		progs

sgi:
	$(MAKE) CC=$(CXX) \
		INCDIRS='-I/usr/include/X11' \
		GFXLDFLAGS='-lX11 -lXext' \
		GFXOBJS=xwin.o \
		CFLAGS='-woff 3262 -O2' \
		progs

linux:
	$(MAKE) INCDIRS='-I/usr/X11R6/include' \
		GFXLDFLAGS='-L/usr/X11R6/lib -lX11 -lXext' \
		GFXOBJS=xwin.o \
		CFLAGS='-ansi -pedantic \
		-Wall -Wno-unused -W -Wmissing-prototypes -Wstrict-prototypes \
		-O3 -fomit-frame-pointer $(PENTIUMOPT)' \
		progs
		
svga:
	$(MAKE) \
		GFXLDFLAGS='-lvga' \
		GFXOBJS=svga.o \
		CFLAGS='-ansi -pedantic \
		-Wall -Wno-unused -W -Wmissing-prototypes -Wstrict-prototypes \
		-O3 -fomit-frame-pointer $(PENTIUMOPT)' \
		progs

progs: uae readdisk

install:

readdisk: readdisk.o
	$(CC) readdisk.o -o readdisk $(LDFLAGS) $(DEBUGFLAGS)

uae: $(OBJS)
	$(CC) $(OBJS) -o uae $(GFXLDFLAGS) $(LDFLAGS) $(DEBUGFLAGS)

clean:
	-rm -f *.o uae readdisk
	-rm -f gencpu genblitter
	-rm -f cpu?.c blit.h
	-rm -f cputbl.h cputbl.c

blit.h: genblitter
	genblitter >blit.h

genblitter: genblitter.o
	$(CC) -o $@ $?
gencpu: gencpu.o
	$(CC) -o $@ $?

custom.o: blit.h

cputbl.c: gencpu
	gencpu t >cputbl.c
cputbl.h: gencpu
	gencpu h >cputbl.h

cpu0.c: gencpu
	gencpu f 0 >cpu0.c
cpu1.c: gencpu
	gencpu f 1 >cpu1.c
cpu2.c: gencpu
	gencpu f 2 >cpu2.c
cpu3.c: gencpu
	gencpu f 3 >cpu3.c
cpu4.c: gencpu
	gencpu f 4 >cpu4.c
cpu5.c: gencpu
	gencpu f 5 >cpu5.c
cpu6.c: gencpu
	gencpu f 6 >cpu6.c
cpu7.c: gencpu
	gencpu f 7 >cpu7.c
cpu8.c: gencpu
	gencpu f 8 >cpu8.c
cpu9.c: gencpu
	gencpu f 9 >cpu9.c
cpuA.c: gencpu
	gencpu f 10 >cpuA.c
cpuB.c: gencpu
	gencpu f 11 >cpuB.c
cpuC.c: gencpu
	gencpu f 12 >cpuC.c
cpuD.c: gencpu
	gencpu f 13 >cpuD.c
cpuE.c: gencpu
	gencpu f 14 >cpuE.c
cpuF.c: gencpu
	gencpu f 15 >cpuF.c

# Compiling these without debugging info speeds up the compilation
# if you have limited RAM (<= 8M)
cpu0.o: cpu0.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu1.o: cpu1.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu2.o: cpu2.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu3.o: cpu3.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu4.o: cpu4.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu5.o: cpu5.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu6.o: cpu6.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu7.o: cpu7.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu8.o: cpu8.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpu9.o: cpu9.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuA.o: cpuA.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuB.o: cpuB.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuC.o: cpuC.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuD.o: cpuD.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuE.o: cpuE.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
cpuF.o: cpuF.c
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $*.c
       
.c.o:
	$(CC) $(INCLUDES) -c $(INCDIRS) $(CFLAGS) $(DEBUGFLAGS) $*.c
.c.s:
	$(CC) $(INCLUDES) -S $(INCDIRS) $(CFLAGS) $*.c

# Some more dependencies...
cpu0.o: cputbl.h
cpu1.o: cputbl.h
cpu2.o: cputbl.h
cpu3.o: cputbl.h
cpu4.o: cputbl.h
cpu5.o: cputbl.h
cpu6.o: cputbl.h
cpu7.o: cputbl.h
cpu8.o: cputbl.h
cpu9.o: cputbl.h
cpuA.o: cputbl.h
cpuB.o: cputbl.h
cpuC.o: cputbl.h
cpuD.o: cputbl.h
cpuE.o: cputbl.h
cpuF.o: cputbl.h
cputbl.o: cputbl.h

ifeq ($(RELEASE),yes)
main.o: config.h
cia.o: config.h
custom.o: config.h
newcpu.o: config.h
autoconf.o: config.h
xwin.o: config.h
svga.o: config.h
os.o: config.h
memory.o: config.h
gencpu.o: config.h
genblitter.o: config.h
debug.o: config.h
ersatz.o: config.h
disk.o: config.h
endif
