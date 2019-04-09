 /* 
  * UAE - The Un*x Amiga Emulator
  *
  * global definitions
  * 
  * (c) 1995 Bernd Schmidt
  */

typedef unsigned char UBYTE;
typedef signed char BYTE;

typedef unsigned short UWORD;
typedef short WORD;

#ifdef __alpha
typedef unsigned int ULONG;
typedef int LONG;
#else
typedef unsigned long ULONG;
typedef long LONG;
#endif
typedef ULONG CPTR;

typedef char bool;

#define true 1
#define false 0
