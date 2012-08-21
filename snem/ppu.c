#include <stdio.h>
#include "snes.h"
/*struct
{*/
unsigned char ppu_vidportc,ppu_sdr;
unsigned char ppu_main,ppu_sub;
unsigned char *ppu_vram;
unsigned short *ppu_vramw;
unsigned short ppu_vaddr;
unsigned short ppu_bg[4],ppu_chr[4];
int ppu_xs[4],ppu_ys[4];
int ppu_vinc,ppu_mode;
unsigned short ppu_cgramaddr;
unsigned short ppu_cgram[256];
uint32 ppu_palette[256];
int ppu_paldirty[256];
uint32 ppu_coltable[8],ppu_coltable16[8];
uint32 ppu_bittable[2][16],ppu_masktable[2][16];
int ppu_firstread;
uint32 ppu_wramaddr;
unsigned char ppu_cgwsel,ppu_cgadsub;
uint32 ppu_fixedcol;
int ppu_fgcount,ppu_fgmask,ppu_fgshift;
unsigned char ppu_wobjsel;
int ppu_w1l,ppu_w1r,ppu_w2l,ppu_w2r;
/*} ppu;*/


struct
{
    int screenon;
    int mainena,subena;
    int bg3pri;
    int xs[4],ys[4];
    int tsize[4],xsize[4],ysize[4];
    int w1l,w1r,w2l,w2r;
#ifdef MODE7
    uint32 fixedcol;
    int bright;
    unsigned short m7a,m7b,m7c,m7d,m7x,m7y;
    int m7change;
    unsigned char m7sel;
    unsigned char mosaic,mena;
    unsigned char cgwsel,cgadsub;
    int wchg;
    unsigned char wobjsel;
#endif
} lines[224];
int lines_mode[224];
int lines_mask[224];
unsigned short lines_chr[224][4];
unsigned short lines_bg[224][4];

unsigned short objnamesel;
unsigned char window[3][256];
int wobjlog;
uint32 ppufc;

unsigned char mosaic;
int rescan[4];
unsigned short sobjaddr;
int curframe=0;
int fpsc;

int prioritybuffer[4][2];
/*Tile caching system*/
uint32 tiless[4096][2][8][2];

char tiledatm[4096][8];
char tileupdate[4096];

void updatetiles()
{
    int c,y;
    for (c=0; c<4096; c++)
    {
        if (tileupdate[c])
        {
            for (y=0; y<8; y++)
            {
                tiless[c][0][y][0]=ppu_bittable[0][ppu_vram[(c<<4)|(y<<1)]>>4]
                                   |(ppu_bittable[0][ppu_vram[(c<<4)|(y<<1)|1]>>4]<<1);
                tiless[c][0][y][1]=ppu_bittable[0][ppu_vram[(c<<4)|(y<<1)]&15]|(ppu_bittable[0][ppu_vram[(c<<4)|(y<<1)|1]&15]<<1);
                tiless[c][1][y][0]=ppu_bittable[1][ppu_vram[(c<<4)|(y<<1)]>>4]|(ppu_bittable[1][ppu_vram[(c<<4)|(y<<1)|1]>>4]<<1);
                tiless[c][1][y][1]=ppu_bittable[1][ppu_vram[(c<<4)|(y<<1)]&15]|(ppu_bittable[1][ppu_vram[(c<<4)|(y<<1)|1]&15]<<1);
                if (tiless[c][0][y][0]|tiless[c][0][y][1]) tiledatm[c][y]=1;
                else                                           tiledatm[c][y]=0;
            }
            tileupdate[c]=0;
        }
    }
}

int sblankchk;
int spcemu,palf,soundupdate;
unsigned char *ram;
int lastsetzf=1;
int ymode[4],xsize[4],bgdat[4];
uint32 matrixr;
#ifdef MODE7
unsigned short m7a,m7b;
#endif
int objsize[4]= {1,1,2,2};
int objsizes[16][2]= { {1,1}, {2,2}, {1,1}, {4,4}, {1,1}, {8,8}, {2,2}, {4,4}, {2,2}, {8,8}, {4,4}, {8,8} };

struct
{
    int x[128],y[128],xs[128],ys[128];
    int hf[128],vf[128],p[128],sx[128],sy[128];
    unsigned short t[128];
    uint32 c[128];
} sprites;

uint32 hmostable[256];
unsigned char fliptable[256];
#ifdef MODE7
unsigned short m7a,m7b,m7c,m7d,m7x,m7y;
unsigned char m7sel;
int m7write;
#endif

void initppu()
{
    ppu_vram=(unsigned char *)malloc(65536);
    ppu_vramw=(unsigned short *)ppu_vram;
    memset(ppu_vram,0,65536);
    memset(objram,0,1024);
    memset(&sprites,0,sizeof(sprites));
    maketables();
}

void resetppu()
{
    memset(ppu_vram,0,65536);
    memset(objram,0,1024);
    memset(&sprites,0,sizeof(sprites));
}

void maketables()
{
    int c;
    for (c=0; c<16; c++)
    {
        ppu_bittable[0][c]=0;
        if (c&0x8) ppu_bittable[0][c]|=0x00000001;
        if (c&0x4) ppu_bittable[0][c]|=0x00000100;
        if (c&0x2) ppu_bittable[0][c]|=0x00010000;
        if (c&0x1) ppu_bittable[0][c]|=0x01000000;
        ppu_bittable[1][c]=0;
        if (c&0x1) ppu_bittable[1][c]|=0x00000001;
        if (c&0x2) ppu_bittable[1][c]|=0x00000100;
        if (c&0x4) ppu_bittable[1][c]|=0x00010000;
        if (c&0x8) ppu_bittable[1][c]|=0x01000000;
        ppu_masktable[0][c]=0;
        if (c&0x8) ppu_masktable[0][c]|=0x000000FF;
        if (c&0x4) ppu_masktable[0][c]|=0x0000FF00;
        if (c&0x2) ppu_masktable[0][c]|=0x00FF0000;
        if (c&0x1) ppu_masktable[0][c]|=0xFF000000;
        ppu_masktable[1][c]=0;
        if (c&0x1) ppu_masktable[1][c]|=0x000000FF;
        if (c&0x2) ppu_masktable[1][c]|=0x0000FF00;
        if (c&0x4) ppu_masktable[1][c]|=0x00FF0000;
        if (c&0x8) ppu_masktable[1][c]|=0xFF000000;
    }
    for (c=0; c<256; c++)
    {
        fliptable[c]=0;
        if (c&0x01) fliptable[c]|=0x80;
        if (c&0x02) fliptable[c]|=0x40;
        if (c&0x04) fliptable[c]|=0x20;
        if (c&0x08) fliptable[c]|=0x10;
        if (c&0x10) fliptable[c]|=0x08;
        if (c&0x20) fliptable[c]|=0x04;
        if (c&0x40) fliptable[c]|=0x02;
        if (c&0x80) fliptable[c]|=0x01;
    }
    ppu_coltable[0]=0;
    ppu_coltable[1]=0x01010101;
    ppu_coltable[2]=0x02020202;
    ppu_coltable[3]=0x03030303;
    ppu_coltable[4]=0x04040404;
    ppu_coltable[5]=0x05050505;
    ppu_coltable[6]=0x06060606;
    ppu_coltable[7]=0x07070707;
    ppu_coltable16[0]=0;
    ppu_coltable16[1]=0x00010001;
    ppu_coltable16[2]=0x00020002;
    ppu_coltable16[3]=0x00030003;
    ppu_coltable16[4]=0x00040004;
    ppu_coltable16[5]=0x00050005;
    ppu_coltable16[6]=0x00060006;
    ppu_coltable16[7]=0x00070007;
#ifdef MODE7
    for (c=0; c<256; c++) hmostable[c]=c|(c<<8)|(c<<16)|(c<<24);
//        for (c=0;c<8;c++) printf("col %08X\n",ppu_coltable[c]);
#endif
}
int ins;
void dumpppuregs()
{
    printf("PPU regs :\n");
    printf("BG  : %04X %04X %04X %04X\n",ppu_bg[0],ppu_bg[1],ppu_bg[2],ppu_bg[3]);
    printf("CHR : %04X %04X %04X %04X\n",ppu_chr[0],ppu_chr[1],ppu_chr[2],ppu_chr[3]);
}

void writeppu(unsigned short addr, unsigned char val)
{
    uint32 tempw,tempw2;
//        if (curline==32) printf("Writing to %04X %02X line 32\n",addr,val);
    switch (addr)
    {
    case 0x2100: /*Screen Display Register*/
        ppu_sdr=val;
        return;
    case 0x2101: /*Sprite Size Register*/
//                printf("Name select %i\n",(val>>3)&3);
        sprchraddr=(val&3)<<14;
        objnamesel=((val>>3)&3)<<13;
        val>>=5;
        val<<=1;
        objsize[0]=objsizes[val][0];
        objsize[1]=objsizes[val][1];
        objsize[2]=objsizes[val+1][0];
        objsize[3]=objsizes[val+1][1];
        return;
    case 0x2102:
        objaddr&=0x100;
        objaddr|=val;
//                printf("OBJ addr now %03X\n",objaddr<<1);
        sobjaddr=objaddr;
        return;
    case 0x2103:
        if (val&1)
            objaddr|=0x100;
        else
            objaddr&=0xFE;

        firstobjwrite=1;
        sobjaddr=objaddr;
        return;
    case 0x2104:

        if (objaddr>0x10F) {
            objaddr=0;
            firstobjwrite=1;
        }

        if (firstobjwrite)
            objram[objaddr<<1]=val;
        else
        {
            objram[(objaddr<<1)+1]=val;
            objaddr++;
        }
        firstobjwrite^=1;
        return;
    case 0x2105: /*Screen Mode Register*/
        ppu_mode=val;
        return;
    case 0x2106: /*Mosaic*/
        mosaic=val;
        return;
    case 0x2107: /*BG1 Address*/
        addr=ppu_bg[0];
        ppu_bg[0]=(val&0xFC)<<9;
        bgdat[0]=val&3;
        xsize[0]=val&1;
        ymode[0]=(val&2)>>1;
        if (addr==ppu_bg[0]) return;
        rescan[0]=1;

        return;
    case 0x2108: /*BG2 Address*/

        addr=ppu_bg[1];
        ppu_bg[1]=(val&0xFC)<<9;
        bgdat[1]=val&3;
        xsize[1]=val&1;
        ymode[1]=(val&2)>>1;
        if (addr==ppu_bg[1]) return;
        rescan[1]=1;
        return;
    case 0x2109: /*BG3 Address*/

        addr=ppu_bg[2];
        ppu_bg[2]=(val&0xFC)<<9;
        bgdat[2]=val&3;
        xsize[2]=val&1;
        ymode[2]=(val&2)>>1;
        if (addr==ppu_bg[2]) return;
        rescan[2]=1;
        return;
    case 0x210A: /*BG4 Address*/
        addr=ppu_bg[3];
        ppu_bg[3]=(val&0xFC)<<9;
        bgdat[3]=val&3;
        xsize[3]=val&1;
        ymode[3]=(val&2)>>1;
        if (addr==ppu_bg[3]) return;
        rescan[3]=1;

        return;
    case 0x210B: /*BG1+2 CHR Address*/
        ppu_chr[0]=(val&0xF)<<13;
        ppu_chr[1]=(val>>4)<<13;
//                printf("CHR 0 %04X 1 %04X %02X %04X\n",ppu_chr[0],ppu_chr[1],val,pc);
        return;
    case 0x210C: /*BG2+3 CHR Address*/
        ppu_chr[2]=(val&0xF)<<13;
        ppu_chr[3]=(val>>4)<<13;
        return;
    case 0x210D: /*BG1 Horizontal Scroll*/
        ppu_xs[0] = (ppu_xs[0]>>8)|(val<<8);
        return;
    case 0x210E: /*BG1 Vertical Scroll*/
        ppu_ys[0] = (ppu_ys[0]>>8)|(val<<8);
//                printf("BG1 vscroll %04X %04X\n",ppu_ys[0],pc);
        return;
    case 0x210F: /*BG2 Horizontal Scroll*/
        ppu_xs[1] = (ppu_xs[1]>>8)|(val<<8);
        return;
    case 0x2110: /*BG2 Vertical Scroll*/
        ppu_ys[1] = (ppu_ys[1]>>8)|(val<<8);
        return;
    case 0x2111: /*BG3 Horizontal Scroll*/
        ppu_xs[2] = (ppu_xs[2]>>8)|(val<<8);
        return;
    case 0x2112: /*BG3 Vertical Scroll*/
        ppu_ys[2] = (ppu_ys[2]>>8)|(val<<8);
        return;
    case 0x2113: /*BG4 Horizontal Scroll*/
        ppu_xs[3] = (ppu_xs[3]>>8)|(val<<8);
        return;
    case 0x2114: /*BG4 Vertical Scroll*/
        ppu_ys[3] = (ppu_ys[3]>>8)|(val<<8);
        return;
    case 0x2115: /*Video Port Control*/
        ppu_vidportc=val;
//                printf("Vidportc %02X\n",val);
        switch (ppu_vidportc&3)
        {
        case 0:
            ppu_vinc=1;
            break;
        case 1:
            ppu_vinc=32;
            break;
        case 2:
        case 3:
            ppu_vinc=128;
            break;
        }
        switch (ppu_vidportc&0xC)
        {
        case 0:
            ppu_fgcount=0;
            break;
        case 4:
            ppu_fgcount=0x20;
            ppu_fgmask=0xFF;
            ppu_fgshift=5;
            break;
        case 8:
            ppu_fgcount=0x40;
            ppu_fgmask=0x1FF;
            ppu_fgshift=6;
            break;
        case 12:
            ppu_fgcount=0x80;
            ppu_fgmask=0x3FF;
            ppu_fgshift=7;
            break;
        }
        return;
    case 0x2116: /*Video Addr Low*/
        ppu_firstread=1;
        ppu_vaddr=(ppu_vaddr&0x7F00)|val;
        return;
    case 0x2117: /*Video Addr High*/
        ppu_firstread=1;
        ppu_vaddr=(ppu_vaddr&0xFF)|((val&0x7F)<<8);
//                printf("Video addr %04X\n",ppu_vaddr<<1);
        return;
    case 0x2118: /*Video Data Low*/
        ppu_firstread=1;
        if (ppu_fgcount)
        {
            tempw=ppu_vaddr&ppu_fgmask;
            tempw2=(ppu_vaddr&~ppu_fgmask)+(tempw>>ppu_fgshift)+((tempw&(ppu_fgcount-1))<<3);
            ppu_vram[(tempw2&0x7FFF)<<1]=val;
            tileupdate[(tempw2&0x7FFF)>>3]=1;
        }
        else
        {
            ppu_vram[(ppu_vaddr&0x7FFF)<<1]=val;
            tileupdate[(ppu_vaddr&0x7FFF)>>3]=1;
        }
        if (!(ppu_vidportc&0x80)) ppu_vaddr+=ppu_vinc;
        return;
    case 0x2119: /*Video Data High*/
        ppu_firstread=1;
        if (ppu_fgcount)
        {
            tempw=ppu_vaddr&ppu_fgmask;
            tempw2=(ppu_vaddr&~ppu_fgmask)+(tempw>>ppu_fgshift)+((tempw&(ppu_fgcount-1))<<3)+1;
            ppu_vram[(tempw2&0x7FFF)<<1]=val;
            tileupdate[(tempw2&0x7FFF)>>3]=1;
        }
        else
        {
            ppu_vram[((ppu_vaddr&0x7FFF)<<1)+1]=val;
            tileupdate[(ppu_vaddr&0x7FFF)>>3]=1;
            tempw2=ppu_vaddr;
        }
        if (((tempw2<<1)&0xF000)==ppu_bg[0]) rescan[0]=1;
        if (((tempw2<<1)&0xF000)==ppu_bg[1]) rescan[1]=1;
        if (((tempw2<<1)&0xF000)==ppu_bg[2]) rescan[2]=1;
        if (((tempw2<<1)&0xF000)==ppu_bg[3]) rescan[3]=1;
        if (ppu_vidportc&0x80) ppu_vaddr+=ppu_vinc;
//                tileupdate[(ppu_vaddr&0x7FFF)>>3]=1;
//                tileupdate4[(ppu_vaddr&0x7FFF)>>4]=1;
        return;
#ifdef MODE7
    case 0x211A:
        m7sel=val;
//                printf("M7 sel %02X\n",val);
        return;
    case 0x211B:
        m7a=(m7a>>8)|(val<<8);
        matrixr=(short)m7a*((short)m7b>>8);
//                printf("M7A write %02X at line %i\n",val,curline);
        return;
    case 0x211C:
        m7b=(m7b>>8)|(val<<8);
        matrixr=(short)m7a*((short)m7b>>8);
//                printf("M7B write %02X at line %i\n",val,curline);
        m7write=1;
        return;
    case 0x211D:
        m7c=(m7c>>8)|(val<<8);
//                printf("M7C write %02X at line %i\n",val,curline);
        return;
    case 0x211E:
        m7d=(m7d>>8)|(val<<8);
//                printf("M7D write %02X at line %i\n",val,curline);
//                m7write=1;
        return;
    case 0x211F:
        m7x=(m7x>>8)|(val<<8);
        return;
    case 0x2120:
        m7y=(m7y>>8)|(val<<8);
        return;
#endif
    case 0x2121: /*Colour Selection Register*/
        ppu_cgramaddr=(val<<1)&0x1FF;
//                printf("CGRAM addr %02X %04X %04X\n",val,ppu_cgramaddr,pc);
        return;
    case 0x2122: /*Colour Data Register*/
        //printf("CGRAM %04X write %02X\n",ppu_cgramaddr,val);
        if(ppu_cgramaddr&1){
            tempw=ppu_cgram[ppu_cgramaddr>>1]|=val<<8;
	    ppu_palette[ppu_cgramaddr>>1]=RGB555toRGB24(tempw);
	}
        else
            ppu_cgram[ppu_cgramaddr>>1]=val;
        ppu_cgramaddr++;
        return;
    case 0x2123:
    case 0x2124:
        //printf("%x %x\n", addr, val);
        return;
    case 0x2125: /*Colour window settings*/
        ppu_wobjsel=val;
        //printf("2125 write %02X\n",val);
        return;
    case 0x2126: /*Window regs*/
        ppu_w1l=val;
//                printf("2126 write %02X\n",val);
        return;
    case 0x2127:
        ppu_w1r=val;
//                printf("2127 write %02X\n",val);
        return;
    case 0x2128:
        ppu_w2l=val;
//                printf("2128 write %02X\n",val);
        return;
    case 0x2129:
        ppu_w2r=val;
//                printf("2129 write %02X\n",val);
        return;
    case 0x212B: /*Mask logic for colour window*/
        wobjlog=val;
//                printf("WOBJLOG %02X\n",val);
        return;
    case 0x212C: /*Main Screen*/
        ppu_main=val;
//                printf("Main screen %02X %02X:%04X\n",val,pbr,pc);
        return;
    case 0x212D: /*Sub Screen*/
        ppu_sub=val;
//                printf("Sub screen %02X %02X:%04X\n",val,pbr,pc);
//                printf("Sub screen %02X\n",val);
        return;
    case 0x2130: /*Fixed colour addition*/
        ppu_cgwsel=val;
        //printf("2130 write %02X line %i\n",val,curline);
        return;
    case 0x2131: /*Addition/Subtraction*/
        ppu_cgadsub=val;
        //printf("2131 write %02X line %i\n",val,curline);
        return;
    case 0x2132: /*Fixed colour data*/        
        if (val&0x20) ppufc=(ppufc&0xffe0)|(val&0x1F);
        if (val&0x40) ppufc=(ppufc&0xfc1f)|((val&0x1F)<<5);
        if (val&0x80) ppufc=(ppufc&0x83ff)|((val&0x1F)<<10);
	ppu_fixedcol=RGB555toRGB24(ppufc);
        //printf("2132 write %02X line %i %02X:%04X %x\n",val,curline,pbr,pc, ppufc);
        return;
    case 0x2140:
    case 0x2141:
    case 0x2142:
    case 0x2143:
    case 0x2144:
    case 0x2145:
    case 0x2146:
    case 0x2147:
//                if ((pc>0x9615)&&(pc<0x963F)) printf("Write %04X %02X\n",addr,val);
#ifdef SOUND
        if (spcemu) write214x(addr&0x2143,val);
        else
#endif
            setzf=0;
        return;
    case 0x2180: /*VRAM write*/
//                if ((ppu_wramaddr)==0) printf("0 write %02X at %02X:%04X\n",val,pbr,pc);
        ram[ppu_wramaddr]=val;
        ppu_wramaddr=(ppu_wramaddr+1)&0x1FFFF;
        return;
    case 0x2181: /*VRAM addr low*/
        ppu_wramaddr=(ppu_wramaddr&0x1FF00)|val;
        return;
    case 0x2182: /*VRAM addr mid*/
        ppu_wramaddr=(ppu_wramaddr&0x100FF)|(val<<8);
        return;
    case 0x2183: /*VRAM addr high*/
        ppu_wramaddr=(ppu_wramaddr&0xFFFF)|((val&1)<<16);
        return;
    }
//        printf("Bad PPU write %04X %02X\n",addr,val);
    /*        dumpregs();
            exit(-1);*/
}

unsigned char *ram;

int skip=0;

unsigned char doskipper()
{
    int temp=skip;
    skip++;
    if (skip==19) skip=0;
    switch (temp>>1)
    {
    case 0:
    case 1:
        setzf=2;
        return 0;
    case 2:
        if (temp&1) return reg_A.b.h;
        else        return reg_A.b.l;
        break;
    case 3:
        if (temp&1) return reg_X.b.h;
        else        return reg_X.b.l;
        break;
    case 4:
        if (temp&1) return reg_Y.b.h;
        else        return reg_Y.b.l;
        break;
    case 5:
        if (temp&1) return 0xBB;
        else        return 0xAA;
        break;
    case 6:
        setzf=2;
        return 0;
    case 7:
        if (temp&1) return 0xBB;
        else        return 0xAA;
        break;
    case 8:
        if (temp&1) return 0x33;
        else        return 0x33;
        break;
    case 9:
        return 0;
    }
    printf("Shouldn't have got here\n");
    exit(-1);
}

inline unsigned short getareg() {
    return reg_A.w;
}

unsigned short vlatch;

unsigned char readppu(unsigned short a)
{
    unsigned char temp;
    if (a>0x21FF) return 0x21;
    switch (a)
    {
    case 0x2134:
        return matrixr;
    case 0x2135:
        return matrixr>>8;
    case 0x2136:
        return matrixr>>16;
    case 0x2137: /*Latch*/
        vlatch=curline;
//                printf("VLATCH %i %02X %02X:%04X\n",vlatch,vlatch,pbr,pc);
        return 0xFF;
    case 0x2139: /*VRAM Read Low*/
        if (ppu_firstread)
        {
            ppu_firstread=0;
            return ppu_vram[(ppu_vaddr<<1)&0xFFFF];
        }
        temp=ppu_vram[((ppu_vaddr<<1)-2)&0xFFFF];
//                printf("Read 2139 vinc %i %i\n",ppu_vinc,ppu_vidportc&3);
        if (!(ppu_vidportc&0x80))
            ppu_vaddr+=ppu_vinc;
        return temp;
    case 0x213A: /*VRAM Read High*/
        if (ppu_firstread)
        {
            ppu_firstread=0;
            return ppu_vram[((ppu_vaddr<<1)+1)&0xFFFF];
        }
        temp=ppu_vram[((ppu_vaddr<<1)-1)&0xFFFF];
//                printf("Read 213A vinc %i %i\n",ppu_vinc,ppu_vidportc&3);
        if (ppu_vidportc&0x80)
            ppu_vaddr+=ppu_vinc;
        return temp;

    case 0x213D: /*Vertical counter*/
        temp=vlatch&0xFF;
        vlatch>>=8;
        return temp;
    case 0x213E: /*PPU time/range flags*/
//                printf("Read 213E %02X:%04X\n",pbr,pc);
        return 0;
        //S=1FEE
    case 0x213F:
        if (palf) return 0x10;
        return 0x00; /*NTSC*/ ///*PAL*/

    case 0x2140:
    case 0x2141:
    case 0x2142:
    case 0x2143:  /*SPC*/
    case 0x2144:
    case 0x2145:
    case 0x2146:
    case 0x2147:
#ifdef SOUND
        if (spcemu) return read214x(a&0x2143);
#endif

        temp=doskipper();
//                printf("%02X  %02X:%04X  %04X\n",temp,pbr,pc,a);
        return temp;

    case 0x2180: /*WRAM read*/
        temp=ram[ppu_wramaddr];
        ppu_wramaddr=(ppu_wramaddr+1)&0x1FFFF;
        return temp;
    case 0x2181: /*WRAM addr low*/
        return ppu_wramaddr;
    case 0x2182: /*WRAM addr mid*/
        return ppu_wramaddr>>8;
    case 0x2183: /*WRAM addr high*/
        return ppu_wramaddr>>16;

    default:
//                return 0;
//                printf("Bad PPU read %04X %02X:%04X %i\n",a,pbr,pc,stage);
        return 0;
//                dumpregs();
//                exit(-1);
    }
}

void vblankhdma()
{
    int c;
    for (c=0; c<8; c++)
    {
        hdma.hdmaon[c]=1;
        hdma.hdmacount[c]=0;
        hdma.srcbank[c]=dma.srcbank[c];
        hdma.src[c]=dma.src[c];
    }
}

static inline unsigned char readf(unsigned char b, unsigned short a)
{
    if (mempointv[b][a>>13])
        return mempoint[b][a>>13][a&0x1FFF];
    else
        return 0;
}

void doline(int line)
{
    int c;
    unsigned char mask=1;
//        printf("HDMA on\n");
    for (c=0; c<8; c++, mask<<=1)
    {
        if (hdmaena&mask && hdma.hdmaon[c])
        {
            if (!hdma.hdmacount[c])
            {
                stage=162;
                hdma.hdmacount[c]=readf(hdma.srcbank[c],hdma.src[c]);
                hdma.src[c]++;
                if (!hdma.hdmacount[c])
                {
                    hdma.hdmaon[c]=0;
                    continue;
                }
                if (hdma.hdmacount[c]&0x80)
                {
                    hdma.hdmacount[c]&=0x7F;
                    hdma.repeat[c]=1;
                }
                else
                    hdma.repeat[c]=0;
//                                if (dma.dest[c]==0xD) printf("210D write line %03i src %02X:%04X ctrl %02X count %i\n",line,hdma.srcbank[c],hdma.src[c],dma.ctrl[c],hdma.hdmacount[c]);
                if (dma.ctrl[c]&0x40)
                {
                    hdma.ta[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8);
                    hdma.src[c]+=2;
                    switch (dma.ctrl[c]&7)
                    {
                    case 0: /*One reg write once*/
                        hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c]);
                        hdma.ta[c]++;
                        break;
                    case 1: /*Two regs write once*/
                        hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8);
                        hdma.ta[c]+=2;
                        break;
                    case 2: /*One reg write twice*/
                        hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8);
                        hdma.ta[c]+=2;
                        break;
                    case 3: /*Two regs write twice*/
                        hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8)|(readf(hdma.ibank[c],hdma.ta[c]+2)<<16)|(readf(hdma.ibank[c],hdma.ta[c]+3)<<24);
                        hdma.ta[c]+=4;
                        break;
                    case 4: /*Four regs write once*/
                        hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8)|(readf(hdma.ibank[c],hdma.ta[c]+2)<<16)|(readf(hdma.ibank[c],hdma.ta[c]+3)<<24);
                        hdma.ta[c]+=4;
                        break;

                    default:
                        printf("Bad HDMA indirect mode %i  count %i dest %02X  data %02X:%04X\n",dma.ctrl[c]&7,hdma.hdmacount[c],dma.dest[c],hdma.ibank[c],hdma.ta[c]);
                        dumpregs();
                        exit(-1);
                    }
                }
                else
                {
                    switch (dma.ctrl[c]&7)
                    {
                    case 0: /*One reg write once*/
                        stage=160;
                        hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c]);
                        hdma.src[c]++;
                        stage=161;
                        break;
                    case 1: /*Two regs write once*/
                        stage=160;
                        hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8);
                        hdma.src[c]+=2;
                        stage=161;
                        break;
                    case 2: /*One reg write twice*/
                        stage=160;
                        hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8);
                        hdma.src[c]+=2;
                        stage=161;
                        break;
                    case 3: /*Two regs write twice*/
                        stage=160;
                        hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8)|(readf(hdma.srcbank[c],hdma.src[c]+2)<<16)|(readf(hdma.srcbank[c],hdma.src[c]+3)<<24);
                        hdma.src[c]+=4;
                        stage=161;
                        break;
                    case 4: /*Four regs*/
                        stage=160;
                        hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8)|(readf(hdma.srcbank[c],hdma.src[c]+2)<<16)|(readf(hdma.srcbank[c],hdma.src[c]+3)<<24);
                        hdma.src[c]+=4;
                        stage=161;
                        break;

                    default:
                        printf("Bad HDMA mode %i  count %i dest %02X\n",dma.ctrl[c]&7,hdma.hdmacount[c],dma.dest[c]);
                        dumpregs();
                        exit(-1);
                    }
                }
                switch (dma.ctrl[c]&7)
                {
                case 0: /*One reg write once*/
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                    break;
                case 1: /*Two regs write once*/
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                    writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>8);
                    break;
                case 2: /*One reg write twice*/
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]>>8);
                    break;
                case 3: /*Two regs write twice*/
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]>>8);
                    writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>16);
                    writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>24);
                    break;
                case 4: /*Four regs*/
                    writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                    writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>8);
                    writeppu(0x2100|dma.dest[c]+2,hdma.hdmaval[c]>>16);
                    writeppu(0x2100|dma.dest[c]+3,hdma.hdmaval[c]>>24);
                    break;
                }
            }
            else
            {
                hdma.hdmacount[c]--;
                if (hdma.repeat[c])
                {
                    if (dma.ctrl[c]&0x40)
                    {
                        switch (dma.ctrl[c]&7)
                        {
                        case 0: /*One reg write once*/
                            hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c]);
                            hdma.ta[c]++;
                            break;
                        case 1: /*Two regs write once*/
                            hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8);
                            hdma.ta[c]+=2;
                            break;
                        case 2: /*One reg write twice*/
                            hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8);
                            hdma.ta[c]+=2;
                            break;
                        case 3: /*Two regs write twice*/
                            hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8)|(readf(hdma.ibank[c],hdma.ta[c]+2)<<16)|(readf(hdma.ibank[c],hdma.ta[c]+3)<<24);
                            hdma.ta[c]+=4;
                            break;
                        case 4: /*Four regs write once*/
                            hdma.hdmaval[c]=readf(hdma.ibank[c],hdma.ta[c])|(readf(hdma.ibank[c],hdma.ta[c]+1)<<8)|(readf(hdma.ibank[c],hdma.ta[c]+2)<<16)|(readf(hdma.ibank[c],hdma.ta[c]+3)<<24);
                            hdma.ta[c]+=4;
                            break;

                        default:
                            printf("Bad HDMA indirect mode %i  count %i dest %02X  data %02X:%04X\n",dma.ctrl[c]&7,hdma.hdmacount[c],dma.dest[c],hdma.ibank[c],hdma.ta[c]);
                            dumpregs();
                            exit(-1);
                        }
                    }
                    else
                    {
                        switch (dma.ctrl[c]&7)
                        {
                        case 0: /*One reg write once*/
                            stage=160;
                            hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c]);
                            hdma.src[c]++;
                            stage=161;
                            break;
                        case 1: /*Two regs write once*/
                            stage=160;
                            hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8);
                            hdma.src[c]+=2;
                            stage=161;
                            break;
                        case 2: /*One reg write twice*/
                            stage=160;
                            hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8);
                            hdma.src[c]+=2;
                            stage=161;
                            break;
                        case 3: /*Two regs write twice*/
                            stage=160;
                            hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8)|(readf(hdma.srcbank[c],hdma.src[c]+2)<<16)|(readf(hdma.srcbank[c],hdma.src[c]+3)<<24);
                            hdma.src[c]+=4;
                            stage=161;
                            break;
                        case 4: /*Four regs*/
                            stage=160;
                            hdma.hdmaval[c]=readf(hdma.srcbank[c],hdma.src[c])|(readf(hdma.srcbank[c],hdma.src[c]+1)<<8)|(readf(hdma.srcbank[c],hdma.src[c]+2)<<16)|(readf(hdma.srcbank[c],hdma.src[c]+3)<<24);
                            hdma.src[c]+=4;
                            stage=161;
                            break;

                        default:
                            printf("Bad HDMA mode %i  count %i dest %02X\n",dma.ctrl[c]&7,hdma.hdmacount[c],dma.dest[c]);
                            dumpregs();
                            exit(-1);
                        }
                    }
                    switch (dma.ctrl[c]&7)
                    {
                    case 0: /*One reg write once*/
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                        break;
                    case 1: /*Two regs write once*/
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                        writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>8);
                        break;
                    case 2: /*One reg write twice*/
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]>>8);
                        break;
                    case 3: /*Two regs write twice*/
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]>>8);
                        writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>16);
                        writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>24);
                        break;
                    case 4: /*Four regs*/
                        writeppu(0x2100|dma.dest[c],hdma.hdmaval[c]);
                        writeppu(0x2100|dma.dest[c]+1,hdma.hdmaval[c]>>8);
                        writeppu(0x2100|dma.dest[c]+2,hdma.hdmaval[c]>>16);
                        writeppu(0x2100|dma.dest[c]+3,hdma.hdmaval[c]>>24);
                        break;
                    }
                }
            }
        }
    }
//        printf("HDMA off\n");
    lines_bg[line][0]=ppu_bg[0]>>1;
    lines_chr[line][0]=ppu_chr[0]>>1;
    lines_bg[line][1]=ppu_bg[1]>>1;
    lines_chr[line][1]=ppu_chr[1]>>1;
    lines_bg[line][2]=ppu_bg[2]>>1;
    lines_chr[line][2]=ppu_chr[2]>>1;
    lines_bg[line][3]=ppu_bg[3]>>1;
    lines_chr[line][3]=ppu_chr[3]>>1;
    lines_mode[line]=ppu_mode&7;
    lines[line].tsize[3]=(ppu_mode&0x80)>>7;
    lines[line].tsize[2]=(ppu_mode&0x40)>>6;
    lines[line].tsize[1]=(ppu_mode&0x20)>>5;
    lines[line].tsize[0]=(ppu_mode&0x10)>>4;
    lines[line].screenon=!(ppu_sdr&0x80);
    if (sblankchk) lines[line].screenon=1;
    lines[line].mainena=ppu_main;
    lines[line].subena=ppu_sub;
    lines[line].bg3pri=(ppu_mode&8)>>3;
    lines[line].xs[0]=ppu_xs[0];
    lines[line].xs[1]=ppu_xs[1];
    lines[line].xs[2]=ppu_xs[2];
    lines[line].xs[3]=ppu_xs[3];
    lines[line].ys[0]=ppu_ys[0];
    lines[line].ys[1]=ppu_ys[1];
    lines[line].ys[2]=ppu_ys[2];
    lines[line].ys[3]=ppu_ys[3];
    lines[line].xsize[0]=xsize[0]&1;
    lines[line].ysize[0]=ymode[0]&1;
    lines[line].xsize[1]=bgdat[1]&1;
    lines[line].ysize[1]=(bgdat[1]&2)>>1;
    lines[line].xsize[2]=bgdat[2]&1;
    lines[line].ysize[2]=(bgdat[2]&2)>>1;
    lines[line].xsize[3]=bgdat[3]&1;
    lines[line].ysize[3]=(bgdat[3]&2)>>1;

    lines[line].w1l=ppu_w1l;
    lines[line].w1r=ppu_w1r;
    lines[line].w2l=ppu_w2l;
    lines[line].w2r=ppu_w2r;
#ifdef MODE7
    lines[line].fixedcol=ppufc;
    lines[line].bright=ppu_sdr&0xF;
    if (ppu_sdr&0x80) lines[line].bright=0;
    lines[line].m7a=m7a;
    lines[line].m7c=m7c;
    lines[line].m7x=m7x;
    lines[line].m7y=m7y;
    lines[line].m7sel=m7sel;
    lines[line].m7b=m7b;
    lines[line].m7d=m7d;
    lines[line].mosaic=mosaic>>4;
    lines[line].mena=mosaic&0xF;
    if (m7write || !line)
        lines[line].m7change=1;
    else
        lines[line].m7change=0;
    m7write=0;
    lines[line].cgwsel=ppu_cgwsel;
    lines[line].cgadsub=ppu_cgadsub;
    lines[line].wobjsel=ppu_wobjsel;
    if (line)
    {
        if ((ppu_w1l!=lines[line-1].w1l) ||
                (ppu_w1r!=lines[line-1].w1r) ||
                (ppu_w2l!=lines[line-1].w2l) ||
                (ppu_w2r!=lines[line-1].w2r))
            lines[line].wchg=1;
        else
            lines[line].wchg=0;
    }
#endif
}

unsigned short ylookup[2][64]=
{
    {   0x000,0x020,0x040,0x060,0x080,0x0A0,0x0C0,0x0E0,
        0x100,0x120,0x140,0x160,0x180,0x1A0,0x1C0,0x1E0,
        0x200,0x220,0x240,0x260,0x280,0x2A0,0x2C0,0x2E0,
        0x300,0x320,0x340,0x360,0x380,0x3A0,0x3C0,0x3E0,
        0x000,0x020,0x040,0x060,0x080,0x0A0,0x0C0,0x0E0,
        0x100,0x120,0x140,0x160,0x180,0x1A0,0x1C0,0x1E0,
        0x200,0x220,0x240,0x260,0x280,0x2A0,0x2C0,0x2E0,
        0x300,0x320,0x340,0x360,0x380,0x3A0,0x3C0,0x3E0
    },
    {   0x000,0x020,0x040,0x060,0x080,0x0A0,0x0C0,0x0E0,
        0x100,0x120,0x140,0x160,0x180,0x1A0,0x1C0,0x1E0,
        0x200,0x220,0x240,0x260,0x280,0x2A0,0x2C0,0x2E0,
        0x300,0x320,0x340,0x360,0x380,0x3A0,0x3C0,0x3E0,
        0x800,0x820,0x840,0x860,0x880,0x8A0,0x8C0,0x8E0,
        0x900,0x920,0x940,0x960,0x980,0x9A0,0x9C0,0x9E0,
        0xA00,0xA20,0xA40,0xA60,0xA80,0xAA0,0xAC0,0xAE0,
        0xB00,0xB20,0xB40,0xB60,0xB80,0xBA0,0xBC0,0xBE0
    }
};

unsigned short xlookup[2][64]=
{
    {   0x000,0x001,0x002,0x003,0x004,0x005,0x006,0x007,
        0x008,0x009,0x00A,0x00B,0x00C,0x00D,0x00E,0x00F,
        0x010,0x011,0x012,0x013,0x014,0x015,0x016,0x017,
        0x018,0x019,0x01A,0x01B,0x01C,0x01D,0x01E,0x01F,
        0x000,0x001,0x002,0x003,0x004,0x005,0x006,0x007,
        0x008,0x009,0x00A,0x00B,0x00C,0x00D,0x00E,0x00F,
        0x010,0x011,0x012,0x013,0x014,0x015,0x016,0x017,
        0x018,0x019,0x01A,0x01B,0x01C,0x01D,0x01E,0x01F
    },
    {   0x000,0x001,0x002,0x003,0x004,0x005,0x006,0x007,
        0x008,0x009,0x00A,0x00B,0x00C,0x00D,0x00E,0x00F,
        0x010,0x011,0x012,0x013,0x014,0x015,0x016,0x017,
        0x018,0x019,0x01A,0x01B,0x01C,0x01D,0x01E,0x01F,
        0x400,0x401,0x402,0x403,0x404,0x405,0x406,0x407,
        0x408,0x409,0x40A,0x40B,0x40C,0x40D,0x40E,0x40F,
        0x410,0x411,0x412,0x413,0x414,0x415,0x416,0x417,
        0x418,0x419,0x41A,0x41B,0x41C,0x41D,0x41E,0x41F
    },
};

void decodesprites()
{
    int c;
    int sprsize;
    int addrtemp=543;
    int tempval,temppos=0;
    tempval=objram[addrtemp];
    for (c=127; c>=0; c--)
    {
        sprites.x[c]=objram[c<<2];
        if (tempval&0x40) sprites.x[c]|=256;
        sprites.y[c]=objram[(c<<2)+1];
        sprites.t[c]=objram[(c<<2)+2]|((objram[(c<<2)+3]&1)<<8);
        sprites.c[c]=(ppu_coltable[(objram[(c<<2)+3]>>1)&7]|0x08080808)<<4;
        if ((objram[(c<<2)+3]>>1)&4) sprites.c[c]|=0x80008000;
//                else       sprites.c[c]=(ppu_coltable[(objram[(c<<2)+3]>>1)&7]|0x08080808)<<4;
        sprites.p[c]=(objram[(c<<2)+3]>>4)&3;
        sprites.hf[c]=(objram[(c<<2)+3]&0x40)>>4;
        sprites.vf[c]=(8-((objram[(c<<2)+3]&0x80)>>7))&7;
        sprsize=tempval&0x80;
        if (sprsize)
        {
            sprites.sx[c]=objsize[2];
            sprites.sy[c]=objsize[3];
        }
        else
        {
            sprites.sx[c]=objsize[0];
            sprites.sy[c]=objsize[1];
        }
        tempval<<=2;
        temppos++;
        if (temppos==4)
        {
            addrtemp--;
            tempval=objram[addrtemp];
        }
        temppos&=3;
    }
}

void dosprites(const int pri, const int ms)
{
    int c,x,y,yy;
    int hstart,hend,hdir;
    int vstart,vend,vdir;
    unsigned short temptile;
    uint32 *tilep;
    uint32 *p, *p2;
    uint32 t1,t2;
    if (ms) c=lines[0].subena;
    else    c=lines[0].mainena;
    if (!(c&0x10)) return;
    for (c=127; c>=0; c--)
    {
        if (pri==sprites.p[c])
        {
            sprites.x[c]&=511;
            sprites.y[c]&=511;
            if (sprites.x[c]&256) sprites.x[c]-=512;
            if (sprites.y[c]>240) sprites.y[c]-=256;
            if ((sprites.x[c]<264) && (sprites.y[c]<240) && ((sprites.x[c]+(sprites.sx[c]<<3))>=0) && ((sprites.y[c]+(sprites.sy[c]<<3))>=-16)) /*Clip offscreen sprites*/
            {
                if (sprites.vf[c])
                {
                    vend=-8;
                    vstart=(sprites.sy[c]-1)<<3;
                    vdir=-8;
                    temptile=(sprites.t[c]+(sprchraddr>>5))<<1;
                }
                else
                {
                    vstart=0;
                    vend=sprites.sy[c]<<3;
                    vdir=8;
                    temptile=(sprites.t[c]+(sprchraddr>>5))<<1;
                }
                if (sprites.t[c]&256) temptile+=(objnamesel>>5)<<1;
                if (sprites.hf[c])
                {
                    hend=-8;
                    hstart=(sprites.sx[c]-1)<<3;
                    hdir=-8;
                }
                else
                {
                    hstart=0;
                    hend=sprites.sx[c]<<3;
                    hdir=8;
                }
                for (y=vstart; y!=vend; y+=vdir)
                {
                    if ((y+sprites.y[c]+16)>=0 && (y+sprites.y[c]+24)<=247)
                    {
                        for (x=hstart; x!=hend; x+=hdir)
                        {
                            if ((x+sprites.x[c]+16)>=0 && (x+sprites.x[c]+24)<=279)
                            {
                                for (yy=0; yy<8; yy++)
                                {

                                    p=(uint32 *)native_bitmap_pointer(x+sprites.x[c], y+yy+sprites.y[c]+1);
                                    tilep=tiless[temptile][sprites.hf[c]>>2][yy^sprites.vf[c]];
                                    t1=tilep[0]|tilep[32];
                                    t2=tilep[0]|(tilep[32]<<2)|sprites.c[c];
                                    p2=p+sprites.hf[c];
                                    for(; t1; p2++, t1>>=8, t2>>=8)  if (t1&0xff) *p2=ppu_palette[t2&0xff];
                                    t1=tilep[1]|tilep[33];
                                    t2=tilep[1]|(tilep[33]<<2)|sprites.c[c];
                                    p2=p+4-sprites.hf[c];
                                    for(; t1; p2++, t1>>=8, t2>>=8)  if(t1&0xff) *p2=ppu_palette[t2&0xff];

                                }
                            }
                            temptile+=2;
                        }
                    }
                    else
                        temptile+=(sprites.sx[c]<<1);
                    if (vdir>0) temptile+=0x20;
                    else        temptile+=0x20;
                    temptile-=(sprites.sx[c]<<1);
                }
            }
        }
    }
}


int bgs[4][8]=
{
    {2,4,4,8,8,4,4,7},
    {2,4,4,4,2,2,0,7},
    {2,2,0,0,0,0,0,0},
    {2,0,0,0,0,0,0,0}
};

#define getm7pixel(x2,y2) temp=ppu_vram[(((y2&~7)<<5)|((x2&~7)>>2))&0xFFFF]; col=ppu_vram[((temp<<7)+((y2&7)<<4)+((x2&7)<<1)+1)&0xFFFF]

/*Inaccuratly named - this actually draws an entire BG*/
void drawbackground(const int bg, const int pri, const int bg3p, const int ms)
{
    int line,bgbpp;
    int y,xx, y0;
    unsigned short tile,temptile;
    uint32 *p, *p2;
    unsigned short baddr;
    int x=0;
    uint32 col=0;
    int lin;
    int hflip;
    int tilemode;
    int yhalf;
    uint32 *tilep;
    unsigned short *xlk;
    uint32 t1,t2;
    static int showerr=-1;

    for (line=0; line<224; line++)
    {
        bgbpp=bgs[bg][lines_mode[line]];
        if (bgbpp==7 && bg==0 && pri) continue;
        if (!lines[line].screenon) continue;
        if (!((ms ? lines[line].subena : lines[line].mainena)&(1<<bg))) continue;
        if (bg==2 &&  pri && lines_mode[line]==1  &&  (bg3p != lines[line].bg3pri)) continue;
        tilemode=lines[line].tsize[bg];
        baddr=lines_bg[line][bg];
        if(tilemode) {
            lin=(line+lines[line].ys[bg])&1023;
            p=(uint32 *)native_bitmap_pointer(-(lines[line].xs[bg]&15), line);
            baddr+=ylookup[lines[line].ysize[bg]][(lin>>4)&63];
            x=(lines[line].xs[bg]>>4)&63;
        }
        else {
            lin=(line+lines[line].ys[bg])&511;
            p=(uint32 *)native_bitmap_pointer(-(lines[line].xs[bg]&7), line);
            baddr+=ylookup[lines[line].ysize[bg]][(lin>>3)&63];
            x=(lines[line].xs[bg]>>3)&63;
        }
        y=lin&7;
        yhalf = (tilemode && (lin&8)) ? 1 : 0;
        xlk=xlookup[lines[line].xsize[bg]];

        switch (bgbpp|tilemode)
        {
        case 2:
            for (xx=0; xx<264; xx+=8, x++)
            {
                tile=ppu_vramw[baddr+xlk[x&63]];
                if ((tile&0x2000)==pri)
                {
                    temptile=(tile&1023)+(lines_chr[line][bg]>>3);
                    y0=y^((8-(tile>>15))&7);
                    if (!lines_mask[line] || tiledatm[temptile][y0])
                    {
                        col=ppu_coltable[(tile>>10)&7]<<2;
                        hflip=(tile&0x4000)>>12;
                        tilep=tiless[temptile][hflip>>2][y0];
                        t1=tilep[0]|col;
                        t2=tilep[0];
                        p2=p+xx+hflip;
                        for(; t2; p2++, t1>>=8, t2>>=8)
                            if(t2&0xff)
                                *p2=ppu_palette[t1&0xff];
                        t1=tilep[1]|col;
                        t2=tilep[1];
                        p2=p+xx+4-hflip;
                        for(; t2; p2++, t1>>=8, t2>>=8)
                            if(t2&0xff)
                                *p2=ppu_palette[t1&0xff];
                    }
                }
            }
            break;
        case 4:
            for (xx=0; xx<264; xx+=8, x++)
            {
                tile=ppu_vramw[baddr+xlk[x&63]];
                if ((tile&0x2000)==pri)
                {
                    temptile=((tile&1023)+(lines_chr[line][bg]>>4))<<1;
                    y0=y^((8-(tile>>15))&7);
                    if (!lines_mask[line] || (tiledatm[temptile][y0]|tiledatm[temptile+1][y0]))
                    {
                        col=ppu_coltable[(tile>>10)&7]<<4;
                        hflip=(tile&0x4000)>>12;
                        tilep=tiless[temptile][hflip>>2][y0];
                        t1=tilep[0]|(tilep[32]<<2)|col;
                        t2=tilep[0]|tilep[32];
                        p2=p+xx+hflip;
                        for(; t2; p2++, t1>>=8, t2>>=8)
                            if(t2&0xff)
                                *p2=ppu_palette[t1&0xff];
                        t1=tilep[1]|(tilep[33]<<2)|col;
                        t2=tilep[1]|tilep[33];
                        p2=p+xx+4-hflip;
                        for(; t2; p2++, t1>>=8, t2>>=8)
                            if(t2&0xff)
                                *p2=ppu_palette[t1&0xff];
                    }
                }
            }
            break;

        default:
            if(showerr!=ppu_mode)
                printf("unsupported mode %d %d %d\n", ppu_mode, bgbpp, tilemode);
            showerr=ppu_mode;
        }

        lines_mask[line]=1;

    }
}

void renderscreen()
{
    unsigned int line,c,d,s;
    uint32 baddr,tempw;
    for (c=0; c<4; c++)
    {
        if (rescan[c])
        {
            rescan[c]=0;
            switch (bgdat[c])
            {
            case 0:
                s=1024;
                break;
            case 1:
            case 2:
                s=2048;
                break;
            case 3:
                s=4096;
                break;
            }
            baddr=ppu_bg[c]>>1;
            prioritybuffer[c][0]=prioritybuffer[c][1]=0;
            for (d=0; d<s; d++)
            {
                if (ppu_vramw[baddr]&0x2000) prioritybuffer[c][1]=1;
                else                         prioritybuffer[c][0]=1;
                if (prioritybuffer[c][0] && prioritybuffer[c][1]) d=s;
                baddr++;
            }
        }
    }
//        printf("VBL\n");
//        decodesprites();
    updatetiles();
    if( (ppu_cgadsub&0x20) && ((ppu_cgwsel&0x30) !=0x30) ){
      
        tempw=ppu_fixedcol;
    }
    else
        tempw=0;
    for (line=0; line<224; line++)
    {
        if (lines_mode[line]==7) lines_mask[line]=1;
        else                     lines_mask[line]=0;
        native_bitmap_clear_line(line, tempw);

    }
#define PRI 0x2000
    if (prioritybuffer[3][0]) drawbackground(3,0,0,1);
    if (prioritybuffer[2][0]) drawbackground(2,0,0,1);
    dosprites(0,1);
    UPDATE_SOUND;
    if (prioritybuffer[3][1]) drawbackground(3,PRI,0,1);
    if (prioritybuffer[2][1]) drawbackground(2,PRI,0,1);
    dosprites(1,1);
    UPDATE_SOUND;
    if (prioritybuffer[1][0]) drawbackground(1,0,0,1);
    if (prioritybuffer[0][0]) drawbackground(0,0,0,1);
    dosprites(2,1);
    UPDATE_SOUND;
    if (prioritybuffer[1][1]) drawbackground(1,PRI,0,1);
    if (prioritybuffer[0][1]) drawbackground(0,PRI,0,1);
    dosprites(3,1);
    UPDATE_SOUND;
    for (line=0; line<224; line++) lines_mask[line]=1;
    drawbackground(2,PRI,1,1);
    if (prioritybuffer[3][0]) drawbackground(3,0,0,0);
    if (prioritybuffer[2][0]) drawbackground(2,0,0,0);
    dosprites(0,0);
    UPDATE_SOUND;
    if (prioritybuffer[3][1]) drawbackground(3,PRI,0,0);
    if (prioritybuffer[2][1]) drawbackground(2,PRI,0,0);
    dosprites(1,0);
    UPDATE_SOUND;
    if (prioritybuffer[1][0]) drawbackground(1,0,0,0);
    if (prioritybuffer[0][0]) drawbackground(0,0,0,0);
    dosprites(2,0);
    UPDATE_SOUND;
    if (prioritybuffer[1][1]) drawbackground(1,PRI,0,0);
    if (prioritybuffer[0][1]) drawbackground(0,PRI,0,0);
    dosprites(3,0);
    UPDATE_SOUND;
    for (line=0; line<224; line++) lines_mask[line]=1;
    drawbackground(2,PRI,1,0);
    //window 1 clip hack
    if( ppu_w1l>0 && (ppu_wobjsel&0x33)==0x23) {
        for (line=0; line<224; line++)
        {
            uint32 *p=native_bitmap_pointer(0,line);
            int x;
            for(x=0; x<lines[line].w1l; x++) p[x]=0;
            for(x=lines[line].w1r; x<256; x++) p[x]=0;
        }
    }    
    native_bitmap_to_screen();
    UPDATE_SOUND;

}
