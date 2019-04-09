 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * disk support
  *
  * (c) 1995 Bernd Schmidt
  */

void DISK_init(void);
void DISK_select(UBYTE data);
UBYTE DISK_status(void);
void DISK_GetData(UWORD *mfm,UWORD *byt);
void DISK_Index(void);
void DISK_InitWrite(void);
void DISK_WriteData(void);

extern int indexpulse;
extern UWORD* mfmwrite;
