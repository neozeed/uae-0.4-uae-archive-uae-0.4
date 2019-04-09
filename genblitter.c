 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * Optimized blitter minterm function generator
  * 
  * (c) 1995 Bernd Schmidt
  */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "amiga.h"

static bool bitset(int mt, int bit)
{
    return mt & (1<<bit);
}

int main(void)
{
    int minterm;
    printf("static __inline__ UWORD blit_func(UWORD srca, UWORD srcb, UWORD srcc, UBYTE mt)\n{\nswitch(mt){\n");
    for (minterm = 0; minterm < 256; minterm++) {
	bool firstor = true;
	int bits = 0;
	int i;
	printf("case 0x%x:\n", minterm);
	printf("\treturn ");
	for(i=0; i<8; i++) {
	    if (bitset(minterm, i) && !bitset(bits,i)) {
		int j;
		int dontcare = 0;
		bool firstand = true;
		int bitbucket[8], bitcount;
		
		bits |= 1<<i;
		bitcount = 1; bitbucket[0] = i;
		for(j=1; j<8; j *= 2) {
		    bool success = true;
		    int k;
		    for(k=0; k < bitcount; k++) {			
			if (!bitset(minterm, bitbucket[k] ^ j)) {
			    success = false;
			}
		    }
		    if (success) {
			int l;
			dontcare |= j;
			for(l=bitcount; l < bitcount*2; l++) {
			    bitbucket[l] = bitbucket[l-bitcount] ^ j;
			    bits |= 1 << bitbucket[l];
			}
			bitcount *= 2;
		    }
		}
		if (firstor) {
		    firstor = false;
		} else {
		    printf(" | ");
		}
		for (j=1; j<8; j *= 2) {
		    if (!(dontcare & j)) {
			if (firstand) {
			    firstand = false;
			    printf("(");
			} else {
			    printf(" & ");
			}
			if (!(i & j))
			    printf("~");
			printf("src%c", (j == 1 ? 'c' : j == 2 ? 'b' : 'a'));
		    }
		}
		if (!firstand) {		    
		    printf(")");
		} else {
		    printf("0xFFFF");
		}
	    }
	}
	if (firstor)
	    printf("0");
	printf(";\n");
    }
    printf("}\n");
    printf("return 0;\n"); /* No, sir, it doesn't! */
    printf("}\n");
    return 0;
}
