#ifndef SNEM_H
#define SNEM_H
int stage;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define uint32 unsigned
#define uint16 unsigned short
#ifdef EMCCHACK
#define RGB555toRGB24(t) \
			( (((t)&0x1f)<<3) | (((t)&0x3e0)<<6) | (((t)&0x7c00)<<9)|0xff000000 )
#else
#define RGB555toRGB24(t) \
			( (((t)&0x1f)<<19) | (((t)&0x3e0)<<6) | (((t)&0x7c00)>>7)|0xff000000 )
#endif
/*65816*/
typedef union
{
        unsigned short w;
        struct { unsigned char l,h; } b;
} reg;

reg reg_A,reg_X,reg_Y,reg_S;
unsigned short pc,dp;
unsigned char pbr,dbr;
int mode;
int nmiena;
int setzf;

struct
{
        int c,z,i,d,v,n,x,m,e;
} struct_p;

/*I/O*/
void writeio(unsigned short addr, unsigned char val);
int nmiocc;
struct
{
        unsigned short src[8],size[8];
        unsigned char srcbank[8],ctrl[8],dest[8];
} dma;
int vertint,vertline;
int horint;

/*Memory*/
unsigned char *mempoint[256][8];
int mempointv[256][8],mempointw[256][8];
#define readmem(b,a)    ((mempointv[b&255][((a)&0xFFFF)>>13])?mempoint[b&255][((a)&0xFFFF)>>13][(a)&0x1FFF]:readmeml(b,a))
#define writemem(b,a,v) {if (mempointw[b&255][((a)&0xFFFF)>>13]) {mempoint[b&255][((a)&0xFFFF)>>13][(a)&0x1FFF]=v; } else {writememl(b,a,v);} }
/*if (a==0x163B || a==0x22A) printf("%04X write %02X %02X:%04X %06X\n",a,v,pbr,pc,(ram[reg_S.w+1]|(ram[reg_S.w+2]<<8)|(ram[reg_S.w+3]<<16))+1); if (b==0x7F && (a&0xFFFF)==0x49) printf("7F0049 write %02X %02X:%04X %04X\n",v,pbr,pc,(ram[reg_S.w+1]|(ram[reg_S.w+2]<<8))+1); */
/*PPU*/
void writeppu(unsigned short addr, unsigned char val);
void initppu();
int vbl;

unsigned char hdmaena;
int curline;

unsigned short sprchraddr;
unsigned char objram[1024];
unsigned short objaddr;
int firstobjwrite;
struct
{
        int hdmaon[8];
        int hdmacount[8];
        uint32 hdmaval[8];
        unsigned char srcbank[8];
        unsigned short src[8];
        int repeat[8];
        unsigned char ibank[8];
        unsigned short ta[8];
} hdma;


/*DSP*/
unsigned char dspregs[128];
int dspaddr;
unsigned char readdsp(int a);
void vblankhdma();
void decodesprites();
unsigned char readmeml(unsigned char b, unsigned short addr);
unsigned char readspcl(unsigned short a);
void doline(int line);
void updatespuaccess(int page);
void writedsp(int a, unsigned char v);
void int65816();
unsigned char readio(unsigned short addr);
unsigned char readppu(unsigned short a);
void nmi65816();
void dumpregs();
void mainloop(int) __attribute__((used));
void reset65816();
void resetspc();
void resetppu();
void dumpspcregs();
void makeopcodetable();
void makeromtable();
void makespctables();
void maketables();
int loadsmc(char *fn);
void renderscreen() __attribute__((used));
void native_hardware_init(int);
uint32 *native_bitmap_pointer(int x, int y) __attribute__((used));
void native_bitmap_clear_line(int line, uint32);
void native_bitmap_to_screen();
void native_tick_callback( int (*func)(void), int fps);
int native_poll_keyboard() __attribute__((used));
uint32* native_set_joypad_state(uint32 state) __attribute__((used));
uint32 native_joypad_state(int num);
#ifdef SOUND
#define UPDATE_SOUND  if (soundupdate && spcemu)  { soundupdate=0; updatesound();}
#else
#define UPDATE_SOUND
#endif
#ifdef USE_SDL  
  extern Uint8 *keystates;
  extern uint32 *bitmap;
#else
  extern uint32 bitmap[288*256];  
#endif
#endif
