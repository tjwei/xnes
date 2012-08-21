#include <stdio.h>
#include "snes.h"

int nextspc;
int slines=224;

int outit=0;
int ins2=0;
int fastrom=0;
int palf;
int c2count=0;
int spcemu;
int soundupdate;
int wai=0;
int irqon=0;
int ins=0;
uint32 oldpc[8];

int lorom=1;

unsigned char *ram;
unsigned char rom[32768*128];
unsigned char opcode;
int cycles;

char sramname[80];

/*Temporary variables*/
int tempi;
unsigned char temp,tempb;
unsigned short tempw,tempw2;
signed char offset;
uint32 templ;
unsigned char sram[0x10000];
int sramsize;
unsigned short srammask[]={0,0x7FF,0xFFF,0x1FFF};

int loadsmc(char *fn)
{
        char fn2[80],fn3[80];
        unsigned short check[2],checkinv[2];
        char title[21];
	printf("fn=%s\n",fn);
        FILE *f=fopen(fn,"rb");
        int c=0,len2, readed;
        c=0;
        while (fn[c]!='.') 
        {
                fn2[c]=fn[c];
                c++;
        }
        fn2[c]=0;
        sprintf(fn3,"%s%s",fn2,".srm");
        memcpy(sramname,fn3,80);
        c=0;
        if (!f) return -1;
        fseek(f,0,SEEK_END);
        len2=ftell(f);
        fseek(f,len2&512,SEEK_SET);        
	if(fread(rom,1, 65536,f)<=0)   exit(1);
        fseek(f,len2&512,SEEK_SET);
        check[0]=rom[0xFFDE]|(rom[0xFFDF]<<8);
        checkinv[0]=rom[0xFFDC]|(rom[0xFFDD]<<8);
        check[1]=rom[0x7FDE]|(rom[0x7FDF]<<8);
        checkinv[1]=rom[0x7FDC]|(rom[0x7FDD]<<8);
        palf=0;
        if ((check[0]+checkinv[0])==0xFFFF && (check[1]+checkinv[1])!=0xFFFF)
        {
                printf("HiROM noninterleaved\n");
                memcpy(title,&rom[0xFFC0],20);
                title[20]=0;
                printf("Game title : %s\n",title);
                printf("ROM type %01X\n",rom[0xFFD6]&0xF);
                sramsize=rom[0xFFD8]&3;
                printf("SRAM size %i kb %i %04X\n",1<<(rom[0xFFD8]),sramsize,srammask[sramsize]);
                printf("Country code %02X\n",rom[0xFFD9]);
                if ((rom[0xFFD9]&0xF)>=2 && (rom[0xFFD9]&0xF)<14) palf=1;
                printf("%s\n",(palf)?"PAL":"NTSC");
                while (!feof(f) && fread(rom+(c*32768),1, 32768,f)>0)
                        c++;                
                lorom=0;
        }
        else if (rom[0x7FD5]&1)
        {
                printf("HiROM interleaved\n");
                memcpy(title,&rom[0x7FC0],20);
                title[20]=0;
                printf("Game title : %s\n",title);
                printf("ROM type %01X\n",rom[0x7FD6]&0xF);
                sramsize=rom[0x7FD8]&3;
                printf("SRAM size %i kb %i %04X\n",1<<(rom[0x7FD8]),sramsize,srammask[sramsize]);
                printf("Country code %02X\n",rom[0x7FD9]);
                printf("%i banks\n",len2>>15);
                if ((rom[0x7FD9]&0xF)>=2) palf=1;
                printf("%s\n",(palf)?"PAL":"NTSC");
                for (c=0;c<(len2>>16);c++)
                    fread(rom+(((c<<1)+1)*32768),32768,1,f);
                for (c=0;c<(len2>>16);c++)
                    fread(rom+((c<<1)*32768),32768,1,f);
                lorom=0;
        }
        else
        {
                printf("LoROM\n");
                memcpy(title,&rom[0x7FC0],20);
                title[20]=0;
                printf("Game title : %s\n",title);
                printf("ROM type %01X\n",rom[0x7FD6]&0xF);
                sramsize=rom[0x7FD8]&3;
                printf("SRAM size %i kb %i %04X\n",1<<(rom[0x7FD8]),sramsize,srammask[sramsize]);
                printf("Country code %02X\n",rom[0x7FD9]);
                if ((rom[0x7FD9]&0xF)>=2) palf=1;
                printf("%s\n",(palf)?"PAL":"NTSC");
                while (!feof(f) && fread(rom+(c*32768),32768,1,f)>0)
                        c++;                
                lorom=1;
        }
        fclose(f);
#ifndef HEMCCHACK
        f=fopen(fn3,"rb");
        if (!f)
           memset(sram, 0, 0x10000);
        else
           fread(sram,0x10000,1,f);
        if (f) fclose(f);
#else
	 memset(sram, 0, 0x10000);
#endif	
	return 1;
}

void dumpram()
{
        FILE *f=fopen("ram.dmp","wb");
        fwrite(ram,0x20000,1,f);
        fclose(f);
        f=fopen("ram2.dmp","wb");
        fwrite(&ram[0x8000],0x8000,1,f);
        fclose(f);
/*        f=fopen(sramname,"wb");
        fwrite(sram,0x10000,1,f);
        fclose(f);*/
}

void dumpregs()
{
        printf("A=%04X X=%04X Y=%04X S=%04X PC=%02X:%04X op=%02X\n",reg_A.w,reg_X.w,reg_Y.w,reg_S.w,pbr,pc,opcode);
        printf("DP=%04X DBR=%04X mode %i  %06X %06X %06X %06X %06X %06X %06X %06X %i ins\n",dp,dbr,mode,oldpc[7],oldpc[6],oldpc[5],oldpc[4],oldpc[3],oldpc[2],oldpc[1],oldpc[0],ins);
        printf("%c%c%c%c%c%c%c%c%c\n",(struct_p.c)?'C':' ',
                                      (struct_p.z)?'Z':' ',
                                      (struct_p.i)?'I':' ',
                                      (struct_p.d)?'D':' ',
                                      (struct_p.x)?'X':' ',
                                      (struct_p.m)?'M':' ',
                                      (struct_p.v)?'V':' ',
                                      (struct_p.n)?'N':' ',
                                      (struct_p.e)?'E':' ');
}

unsigned char readmeml(unsigned char b, unsigned short addr)
{
        if (lorom)
        {
                if (b==0x70 && sramsize) { addr&=srammask[sramsize]; /*printf("R SRAM %04X = %02X\n",addr,sram[addr]); */return sram[addr]; }
                b&=0x7F;
                if (b<0x60)
                {
                        switch (addr&0xF000)
                        {
                                case 0x0000: case 0x1000:
                                return ram[addr];                                
                                case 0x2000:
                                return readppu(addr);
                                case 0x3000: return 0;                                
                                case 0x4000:
                                return readio(addr);                                
                                case 0x5000: case 0x6000: case 0x7000: return 0;
                        }
                }
                switch (b)
                {
                        case 0x66: return 0xFF;
                        case 0x70: return 0xFF;
                        case 0x79: return 0xFF;
                }
        }
        else
        {
                if ((b&0x7F)<0x40)
                {
                        switch (addr&0xF000)
                        {
                                case 0x0000: case 0x1000:
                                return ram[addr];                                
                                case 0x2000:
                                return readppu(addr);
                                case 0x3000: case 0x5000: return 0; /*SD3 reads here*/
                                case 0x4000:
                                return readio(addr);
                                case 0x6000: case 0x7000:
                                return sram[addr&srammask[sramsize]];
                        }
                }
                switch (b)
                {
                        case 0x70: case 0x71: case 0x72: case 0x73:
                        case 0x74: case 0x75: case 0x76: case 0x77:
                        return 0xFF;
                }
        }
        printf("Bad read %02X:%04X at %04X\n",b,addr,pc);
        printf("%02X %i %i\n",b,((addr)&0xFFFF)>>13,mempointv[b][((addr)&0xFFFF)>>13]);
        dumpregs();
        exit(-1);
}

void writememl(unsigned char b, unsigned short addr, unsigned char val)
{
//        if (stage==10 && pc==0xB8B5) printf("%02X %04X %02X\n",b,addr,val);
        if (lorom)
        {
                if (b==0x70 && sramsize) { addr&=srammask[sramsize]; /*printf("SRAM %04X=%02X\n",addr,val); */sram[addr]=val; return; }
                if (b==0x70 && !sramsize) return;
                b&=0x7F;
                if (b<0x60)
                {
                        switch (addr&0xF000)
                        {
                                case 0x0000: case 0x1000:
                                ram[addr]=val;
                                return;
                                case 0x2000:
                                writeppu(addr,val);
                                return;
                                case 0x3000: return; /*TMNT4 writes here*/
                                case 0x4000:
                                writeio(addr,val);
                                return;
                                case 0x5000: case 0x6000: return; /*Pushover writes here*/
                                case 0x7000: return; /*Wario's Woods writes here*/
//                                case 0xE000: return; /*Soccer Kid writes here*/
//                                case 0xF000: return; /*And here*/
//                                case 0xD000: return; /*And here*/
                                case 0x8000: case 0x9000: case 0xA000: case 0xB000:
                                case 0xC000: case 0xD000: case 0xE000: case 0xF000:
                                return;
                        }
                }
        }
        else
        {
                if (b>=0xC0) return;
                if ((b&0x7F)<0x40)
                {
                        switch (addr&0xF000)
                        {
                                case 0x0000: case 0x1000:
                                ram[addr]=val;
                                return;
                                case 0x2000:
                                writeppu(addr,val);
                                return;
                                case 0x3000: return; /*MK2 writes here*/
                                case 0x4000:
                                writeio(addr,val);
                                return;
                                case 0x6000: case 0x7000:
                                sram[addr&srammask[sramsize]]=val;
                                return;
                                case 0x8000: case 0x9000: case 0xA000: case 0xB000:
                                case 0xC000: case 0xD000: case 0xE000: case 0xF000:
                                return;
                        }
                }
                switch (b)
                {
                        case 0x70: case 0x71: case 0x72: case 0x73:
                        case 0x74: case 0x75: case 0x76: case 0x77:
                        return;
                }
        }
        printf("Bad write %02X:%04X %02X at %04X\n",b,addr,val,pc);
        dumpregs();
        exit(-1);
}

void updatemode()
{
        if (struct_p.e) 
        {
                mode=4;
                reg_S.b.h=1;
                reg_X.b.h=reg_Y.b.h=0;
        }
        else
        {
                mode=0;
                if (struct_p.x) mode|=1;
                if (struct_p.m) mode|=2;
                if (struct_p.x) reg_X.b.h=reg_Y.b.h=0;
        }
}

void (*opcodes[256][5])();

void badopcode()
{
        pc--;
        printf("Bad opcode %02X\n",opcode);
        dumpregs();
        exit(-1);
}

void reset65816()
{
/*        reg_A.w=reg_X.w=reg_Y.w=0;
        struct_p.i=struct_p.e=struct_p.x=struct_p.m=1;
        reg_S.b.h=1;
        reg_S.b.l=0xFF;
        mode=4;
        wai=0;
        pc=readmem(0,0xFFFC); pc|=(readmem(0,0xFFFD)<<8);
        pbr=dbr=0;*/
        struct_p.i=struct_p.e=struct_p.x=struct_p.m=1;
        reg_S.b.h=1;
        reg_S.b.l=0xFF;
        mode=4;
        pc=readmem(0,0xFFFC); pc|=(readmem(0,0xFFFD)<<8);

}

void makehiromtable()
{
        int c,d;
        lorom=0;
        for (c=0;c<0x100;c++)
        {
                for (d=0;d<8;d++)
                    mempointv[c][d]=0;
                for (d=0;d<8;d++)
                    mempointw[c][d]=0;
        }
        for (c=0;c<0x40;c++)
        {
                mempoint[c][0]=ram;
                mempoint[c][4]=rom+(c*65536)+0x8000;
                mempoint[c][5]=rom+(c*65536)+0xA000;
                mempoint[c][6]=rom+(c*65536)+0xC000;
                mempoint[c][7]=rom+(c*65536)+0xE000;
                mempointw[c][0]=1;
                for (d=1;d<8;d++) mempointw[c][d]=0;
                mempointv[c][0]=mempointv[c][4]=mempointv[c][5]=mempointv[c][6]=mempointv[c][7]=1;
                mempointv[c][1]=mempointv[c][2]=mempointv[c][3]=0;
                mempoint[c+0x80][0]=ram;
                mempoint[c+0x80][4]=rom+(c*65536)+0x8000;
                mempoint[c+0x80][5]=rom+(c*65536)+0xA000;
                mempoint[c+0x80][6]=rom+(c*65536)+0xC000;
                mempoint[c+0x80][7]=rom+(c*65536)+0xE000;
                mempointw[c+0x80][0]=1;
                for (d=1;d<8;d++) mempointw[c+0x80][d]=0;
                mempointv[c+0x80][0]=mempointv[c+0x80][4]=mempointv[c+0x80][5]=mempointv[c+0x80][6]=mempointv[c+0x80][7]=1;
                mempointv[c+0x80][1]=mempointv[c+0x80][2]=mempointv[c+0x80][3]=0;
        }
        for (c=0;c<0x40;c++)
        {
                mempoint[c+0xC0][0]=rom+(c*65536);
                mempoint[c+0xC0][1]=rom+(c*65536)+0x2000;
                mempoint[c+0xC0][2]=rom+(c*65536)+0x4000;
                mempoint[c+0xC0][3]=rom+(c*65536)+0x6000;
                mempoint[c+0xC0][4]=rom+(c*65536)+0x8000;
                mempoint[c+0xC0][5]=rom+(c*65536)+0xA000;
                mempoint[c+0xC0][6]=rom+(c*65536)+0xC000;
                mempoint[c+0xC0][7]=rom+(c*65536)+0xE000;
                for (d=0;d<8;d++) mempointv[c+0xC0][d]=1;
        }
        for (c=0;c<0x20;c++)
        {
                mempoint[c+0x40][0]=rom+(c*65536);
                mempoint[c+0x40][1]=rom+(c*65536)+0x2000;
                mempoint[c+0x40][2]=rom+(c*65536)+0x4000;
                mempoint[c+0x40][3]=rom+(c*65536)+0x6000;
                mempoint[c+0x40][4]=rom+(c*65536)+0x8000;
                mempoint[c+0x40][5]=rom+(c*65536)+0xA000;
                mempoint[c+0x40][6]=rom+(c*65536)+0xC000;
                mempoint[c+0x40][7]=rom+(c*65536)+0xE000;
                for (d=0;d<8;d++) mempointv[c+0x40][d]=1;
        }
        for (c=0;c<8;c++) mempointv[0x7E][c]=1;
        for (c=0;c<8;c++) mempointv[0x7F][c]=1;
        for (c=0;c<8;c++) mempointw[0x7E][c]=1;
        for (c=0;c<8;c++) mempointw[0x7F][c]=1;
        for (c=0;c<8;c++) mempoint[0x7E][c]=ram+(c*8192);
        for (c=0;c<8;c++) mempoint[0x7F][c]=ram+(c*8192)+0x10000;
}

void makeloromtable()
{
        int c,d;
        lorom=1;
        for (c=0;c<0x100;c++)
        {
                for (d=0;d<8;d++)
                    mempointv[c][d]=0;
                for (d=0;d<8;d++)
                    mempointw[c][d]=0;
        }
        for (c=0;c<0x40;c++)
        {
                mempoint[c][0]=ram;
                mempoint[c][4]=rom+(c*32768);
                mempoint[c][5]=rom+(c*32768)+8192;
                mempoint[c][6]=rom+(c*32768)+16384;
                mempoint[c][7]=rom+(c*32768)+24576;
                mempointw[c][0]=1;
                for (d=1;d<8;d++) mempointw[c][d]=0;
                mempointv[c][0]=mempointv[c][4]=mempointv[c][5]=mempointv[c][6]=mempointv[c][7]=1;
                mempointv[c][1]=mempointv[c][2]=mempointv[c][3]=0;
                mempoint[c+0x80][0]=ram;
                mempoint[c+0x80][4]=rom+(c*32768);
                mempoint[c+0x80][5]=rom+(c*32768)+8192;
                mempoint[c+0x80][6]=rom+(c*32768)+16384;
                mempoint[c+0x80][7]=rom+(c*32768)+24576;
                mempointw[c+0x80][0]=1;
                for (d=1;d<8;d++) mempointw[c+0x80][d]=0;
                mempointv[c+0x80][0]=mempointv[c+0x80][4]=mempointv[c+0x80][5]=mempointv[c+0x80][6]=mempointv[c+0x80][7]=1;
                mempointv[c+0x80][1]=mempointv[c+0x80][2]=mempointv[c+0x80][3]=0;
        }
        for (c=0x40;c<0x60;c++)
        {
                mempoint[c][4]=rom+(c*32768);
                mempoint[c][5]=rom+(c*32768)+8192;
                mempoint[c][6]=rom+(c*32768)+16384;
                mempoint[c][7]=rom+(c*32768)+24576;
                for (d=0;d<8;d++) mempointw[c][d]=0;
                for (d=0;d<4;d++) mempointv[c][d]=0;
                mempointv[c][4]=mempointv[c][5]=mempointv[c][6]=mempointv[c][7]=1;
                mempoint[c+0x80][4]=rom+(c*32768);
                mempoint[c+0x80][5]=rom+(c*32768)+8192;
                mempoint[c+0x80][6]=rom+(c*32768)+16384;
                mempoint[c+0x80][7]=rom+(c*32768)+24576;
                for (d=0;d<8;d++) mempointw[c+0x80][d]=0;
                for (d=0;d<4;d++) mempointv[c+0x80][d]=0;
                mempointv[c+0x80][4]=mempointv[c+0x80][5]=mempointv[c+0x80][6]=mempointv[c+0x80][7]=1;
        }
        for (c=0;c<8;c++) mempointv[0x7E][c]=1;
        for (c=0;c<8;c++) mempointv[0x7F][c]=1;
        for (c=0;c<8;c++) mempointw[0x7E][c]=1;
        for (c=0;c<8;c++) mempointw[0x7F][c]=1;
        for (c=0;c<8;c++) mempoint[0x7E][c]=ram+(c*8192);
        for (c=0;c<8;c++) mempoint[0x7F][c]=ram+(c*8192)+0x10000;
        for (c=0;c<8;c++) mempointv[0xFE][c]=1;
        for (c=0;c<8;c++) mempointv[0xFF][c]=1;
        for (c=0;c<8;c++) mempointw[0xFE][c]=1;
        for (c=0;c<8;c++) mempointw[0xFF][c]=1;
        for (c=0;c<8;c++) mempoint[0xFE][c]=ram+(c*8192);
        for (c=0;c<8;c++) mempoint[0xFF][c]=ram+(c*8192)+0x10000;
}
void makeromtable(){
  if(lorom) makeloromtable();
  else makehiromtable();
}
#define setzn8(v)  { struct_p.z=(!(v)); struct_p.n=(v)&0x80; }
#define setzn16(v) { struct_p.z=(!(v)); struct_p.n=(v)&0x8000; }

static inline unsigned short getword()
{
        unsigned short tw;
        tw=readmem(pbr,pc)|(readmem(pbr,pc+1)<<8); pc+=2;
        return tw;
}

#define ADCB8()  tempw=reg_A.b.l+temp+((struct_p.c)?1:0);                          \
                struct_p.v=(!((reg_A.b.l^temp)&0x80)&&((reg_A.b.l^tempw)&0x80));       \
                reg_A.b.l=tempw&0xFF;                                       \
                setzn8(reg_A.b.l);                                          \
                struct_p.c=tempw&0x100;

#define ADCB16() templ=reg_A.w+tempw+((struct_p.c)?1:0);                           \
                struct_p.v=(!((reg_A.w^tempw)&0x8000)&&((reg_A.w^templ)&0x8000));      \
                reg_A.w=templ&0xFFFF;                                       \
                setzn16(reg_A.w);                                           \
                struct_p.c=templ&0x10000;

#define ADCBCD8()                                                       \
                tempw=(reg_A.b.l&0xF)+(temp&0xF)+(struct_p.c?1:0);                 \
                if (tempw>9)                                            \
                {                                                       \
                        tempw+=6;                                       \
                }                                                       \
                tempw+=((reg_A.b.l&0xF0)+(temp&0xF0));                      \
                if (tempw>0x9F)                                         \
                {                                                       \
                        tempw+=0x60;                                    \
                }                                                       \
                struct_p.v=(!((reg_A.b.l^temp)&0x80)&&((reg_A.b.l^tempw)&0x80));       \
                reg_A.b.l=tempw&0xFF;                                       \
                setzn8(reg_A.b.l);                                          \
                struct_p.c=tempw>0xFF;

#define ADCBCD16()                                                      \
                templ=(reg_A.w&0xF)+(tempw&0xF)+(struct_p.c?1:0);                  \
                if (templ>9)                                            \
                {                                                       \
                        templ+=6;                                       \
                }                                                       \
                templ+=((reg_A.w&0xF0)+(tempw&0xF0));                       \
                if (templ>0x9F)                                         \
                {                                                       \
                        templ+=0x60;                                    \
                }                                                       \
                templ+=((reg_A.w&0xF00)+(tempw&0xF00));                     \
                if (templ>0x9FF)                                        \
                {                                                       \
                        templ+=0x600;                                   \
                }                                                       \
                templ+=((reg_A.w&0xF000)+(tempw&0xF000));                   \
                if (templ>0x9FFF)                                       \
                {                                                       \
                        templ+=0x6000;                                  \
                }                                                       \
                struct_p.v=(!((reg_A.w^tempw)&0x8000)&&((reg_A.w^templ)&0x8000));      \
                reg_A.w=templ&0xFFFF;                                       \
                setzn16(reg_A.w);                                           \
                struct_p.c=templ>0xFFFF;

#define SBCB8()  tempw=reg_A.b.l-temp-((struct_p.c)?0:1);                          \
                struct_p.v=(((reg_A.b.l^temp)&0x80)&&((reg_A.b.l^tempw)&0x80));        \
                reg_A.b.l=tempw&0xFF;                                       \
                setzn8(reg_A.b.l);                                          \
                struct_p.c=tempw<=0xFF;

#define SBCB16() templ=reg_A.w-tempw-((struct_p.c)?0:1);                           \
                struct_p.v=(((reg_A.w^tempw)&(reg_A.w^templ))&0x8000);                 \
                reg_A.w=templ&0xFFFF;                                       \
                setzn16(reg_A.w);                                           \
                struct_p.c=templ<=0xFFFF;

#define SBCBCD8()                                                       \
                tempw=(reg_A.b.l&0xF)-(temp&0xF)-(struct_p.c?0:1);                 \
                if (tempw>9)                                            \
                {                                                       \
                        tempw-=6;                                       \
                }                                                       \
                tempw+=((reg_A.b.l&0xF0)-(temp&0xF0));                      \
                if (tempw>0x9F)                                         \
                {                                                       \
                        tempw-=0x60;                                    \
                }                                                       \
                struct_p.v=(((reg_A.b.l^temp)&0x80)&&((reg_A.b.l^tempw)&0x80));        \
                reg_A.b.l=tempw&0xFF;                                       \
                setzn8(reg_A.b.l);                                          \
                struct_p.c=tempw<=0xFF;

#define SBCBCD16()                                                      \
                templ=(reg_A.w&0xF)-(tempw&0xF)-(struct_p.c?0:1);                  \
                if (templ>9)                                            \
                {                                                       \
                        templ-=6;                                       \
                }                                                       \
                templ+=((reg_A.w&0xF0)-(tempw&0xF0));                       \
                if (templ>0x9F)                                         \
                {                                                       \
                        templ-=0x60;                                    \
                }                                                       \
                templ+=((reg_A.w&0xF00)-(tempw&0xF00));                     \
                if (templ>0x9FF)                                        \
                {                                                       \
                        templ-=0x600;                                   \
                }                                                       \
                templ+=((reg_A.w&0xF000)-(tempw&0xF000));                   \
                if (templ>0x9FFF)                                       \
                {                                                       \
                        templ-=0x6000;                                  \
                }                                                       \
                struct_p.v=(((reg_A.w^tempw)&0x8000)&&((reg_A.w^templ)&0x8000));       \
                reg_A.w=templ&0xFFFF;                                       \
                setzn16(reg_A.w);                                           \
                struct_p.c=templ<=0xFFFF;

#define jabs() (getword())
#define jabsp() (getword()|(pbr<<16))
#define jabsx()((getword()+reg_X.w)&0xFFFF)
#define jabsxp()((getword()|(pbr<<16))+reg_X.w)
#define abs()  (getword()|(dbr<<16))
#define absx() ((getword()|(dbr<<16))+reg_X.w)
#define absy() ((getword()|(dbr<<16))+reg_Y.w)
#define far()  (readmem(pbr,pc)|(readmem(pbr,pc+1)<<8)|(readmem(pbr,pc+2)<<16)); pc+=3
#define farx() (readmem(pbr,pc)|(readmem(pbr,pc+1)<<8)|(readmem(pbr,pc+2)<<16))+reg_X.w; pc+=3
#define sp()   readmem(pbr,pc)+reg_S.w; pc++
#define zp()   readmem(pbr,pc)+dp; pc++
#define zpx()  readmem(pbr,pc)+dp+reg_X.w; pc++
#define zpy()  readmem(pbr,pc)+dp+reg_Y.w; pc++

static inline uint32 indx()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+dp+reg_X.w; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(dbr<<16));
        return templ;
}

static inline uint32 indy()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+dp; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(dbr<<16))+reg_Y.w;
        return templ;
}

static inline uint32 indys()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+reg_S.w; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(dbr<<16))+reg_Y.w;
        return templ;
}

static inline uint32 indly()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+dp; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(readmem(0,t+2)<<16))+reg_Y.w;
        return templ;
}

static inline uint32 ind()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+dp; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(dbr<<16));
        return templ;
}

static inline uint32 indl()
{
        uint32 templ;
        unsigned short t=readmem(pbr,pc)+dp; pc++;
        templ=(readmem(0,t)|(readmem(0,t+1)<<8)|(readmem(0,t+2)<<16));
        return templ;
}

#define readmemw(a)    (readmem(a>>16,a)|(readmem((a+1)>>16,a+1)<<8))
#define writememw(a,v) writemem(a>>16,a,v); writemem((a+1)>>16,a+1,v>>8)

/*ADC abs*/   void op6D()  {templ=abs(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=5;}
/*ADC abs*/   void op6Dm() {templ=abs(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=4;}
/*ADC abs,x*/ void op7D()  {templ=absx(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=4;}
/*ADC abs,x*/ void op7Dm() {templ=absx(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC abs,y*/ void op79()  {templ=absy(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=4;}
/*ADC abs,y*/ void op79m() {templ=absy(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC far*/   void op6F()  {templ=far(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=6;}
/*ADC far*/   void op6Fm() {templ=far(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC far,x*/ void op7F()  {templ=farx(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=6;}
/*ADC far,x*/ void op7Fm() {templ=farx(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC imm*/   void op69()  {tempw=getword(); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=3;}
/*ADC imm*/   void op69m() {temp=readmem(pbr,pc); pc++; if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=2;}
/*ADC ()*/    void op72()  {templ=ind();  tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=6;}
/*ADC ()*/    void op72m() {templ=ind();  temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC (),y*/  void op71()  {templ=indy(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=6;}
/*ADC (),y*/  void op71m() {templ=indy(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=5;}
/*ADC (s),y*/ void op73()  {templ=indys();tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=8;}
/*ADC (s),y*/ void op73m() {templ=indys();temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=7;}
/*ADC []*/    void op67()  {templ=indl(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=7;}
/*ADC []*/    void op67m() {templ=indl(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=6;}
/*ADC [],y*/  void op77()  {templ=indly();tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=7;}
/*ADC [],y*/  void op77m() {templ=indly();temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=6;}
/*ADC sp*/    void op63()  {templ=sp(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=5;}
/*ADC sp*/    void op63m() {templ=sp(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=4;}
/*ADC zp*/    void op65()  {templ=zp(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=4;}
/*ADC zp*/    void op65m() {templ=zp(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=3;}
/*ADC zp,x*/  void op75()  {templ=zpx(); tempw=readmemw(templ); if (struct_p.d) {ADCBCD16()} else {ADCB16()} cycles-=5;}
/*ADC zp,x*/  void op75m() {templ=zpx(); temp=readmem(templ>>16,templ); if (struct_p.d) {ADCBCD8()} else {ADCB8()} cycles-=4;}

/*AND abs*/   void op2D()  {templ=abs();  tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*AND abs*/   void op2Dm() {templ=abs();  temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=4;}
/*AND abs,x*/ void op3D()  {templ=absx(); tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*AND abs,x*/ void op3Dm() {templ=absx(); temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=4;}
/*AND abs,y*/ void op39()  {templ=absy(); tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*AND abs,y*/ void op39m() {templ=absy(); temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=4;}
/*AND far*/   void op2F()  {templ=far();  tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*AND far*/   void op2Fm() {templ=far();  temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=5;}
/*AND far,x*/ void op3F()  {templ=farx(); tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*AND far,x*/ void op3Fm() {templ=farx(); temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=5;}
/*AND imm*/   void op29()  {reg_A.w&=getword(); setzn16(reg_A.w); cycles-=3;}
/*AND imm*/   void op29m() {reg_A.b.l&=readmem(pbr,pc); pc++; setzn8(reg_A.b.l); cycles-=2;}
/*AND (),y*/  void op31()  {templ=indy(); tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*AND (),y*/  void op31m() {templ=indy(); temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=5;}
/*AND []*/    void op27()  {templ=indl(); tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*AND []*/    void op27m() {templ=indl(); temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=6;}
/*AND [],y*/  void op37()  {templ=indly();tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*AND [],y*/  void op37m() {templ=indly();temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=6;}
/*AND sp*/    void op23()  {templ=sp();   tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*AND sp*/    void op23m() {templ=sp();   temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=4;}
/*AND zp*/    void op25()  {templ=zp();   tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=4;}
/*AND zp*/    void op25m() {templ=zp();   temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=3;}
/*AND zp,x*/  void op35()  {templ=zpx();  tempw=readmemw(templ);         reg_A.w&=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*AND zp,x*/  void op35m() {templ=zpx();  temp=readmem(templ>>16,templ); reg_A.b.l&=temp; setzn8(reg_A.b.l); cycles-=4;}

/*ASL A*/     void op0A()  {struct_p.c=reg_A.w&0x8000; reg_A.w<<=1; setzn16(reg_A.w); cycles-=2;}
/*ASL A*/     void op0Am() {struct_p.c=reg_A.b.l&0x80; reg_A.b.l<<=1; setzn8(reg_A.b.l); cycles-=2;}
/*ASL abs*/   void op0E()  {templ=abs(); tempw=readmemw(templ);         struct_p.c=tempw&0x8000; tempw<<=1; writememw(templ,tempw);         setzn16(tempw); cycles-=8;}
/*ASL abs*/   void op0Em() {templ=abs(); temp=readmem(templ>>16,templ); struct_p.c=temp&0x80;    temp<<=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=6;}
/*ASL abs,x*/ void op1E()  {templ=absx();tempw=readmemw(templ);         struct_p.c=tempw&0x8000; tempw<<=1; writememw(templ,tempw);         setzn16(tempw); cycles-=9;}
/*ASL abs,x*/ void op1Em() {templ=absx();temp=readmem(templ>>16,templ); struct_p.c=temp&0x80;    temp<<=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=7;}
/*ASL zp*/    void op06()  {templ=zp();  tempw=readmemw(templ);         struct_p.c=tempw&0x8000; tempw<<=1; writememw(templ,tempw);         setzn16(tempw); cycles-=7;}
/*ASL zp*/    void op06m() {templ=zp();  temp=readmem(templ>>16,templ); struct_p.c=temp&0x80;    temp<<=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=5;}
/*ASL zp,x*/  void op16()  {templ=zpx(); tempw=readmemw(templ);         struct_p.c=tempw&0x8000; tempw<<=1; writememw(templ,tempw);         setzn16(tempw); cycles-=8;}
/*ASL zp,x*/  void op16m() {templ=zpx(); temp=readmem(templ>>16,templ); struct_p.c=temp&0x80;    temp<<=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=6;}

/*BCC*/ void op90() {offset=(signed char)readmem(pbr,pc); pc++; if (!struct_p.c) {pc+=offset; cycles--;} cycles-=3;}
/*BCS*/ void opB0() {offset=(signed char)readmem(pbr,pc); pc++; if (struct_p.c) {pc+=offset; cycles--;} cycles-=3;}
/*BEQ*/ void opF0() {if (setzf>0) struct_p.z=0; else if (setzf<0) struct_p.z=1; offset=(signed char)readmem(pbr,pc); pc++; if (struct_p.z) {pc+=offset; cycles--;} cycles-=3; setzf=0;}
/*BMI*/ void op30() {offset=(signed char)readmem(pbr,pc); pc++; if (struct_p.n) {pc+=offset; cycles--;} cycles-=3;}
/*BNE*/ void opD0() {if (setzf>0) struct_p.z=1; else if (setzf<0) struct_p.z=0; offset=(signed char)readmem(pbr,pc); pc++; if (!struct_p.z) {pc+=offset; cycles--;} cycles-=3; setzf=0;}
/*BPL*/ void op10() {offset=(signed char)readmem(pbr,pc); pc++; if (!struct_p.n) {pc+=offset; cycles--;} cycles-=3;}
/*BRA*/ void op80() {offset=(signed char)readmem(pbr,pc); pc++; pc+=offset; cycles-=3;}
/*BRL*/ void op82() {tempw=getword(); pc+=tempw; cycles-=4;}
/*BVC*/ void op50() {offset=(signed char)readmem(pbr,pc); pc++; if (!struct_p.v){pc+=offset; cycles--;} cycles-=3;}
/*BVS*/ void op70() {offset=(signed char)readmem(pbr,pc); pc++; if (struct_p.v) {pc+=offset; cycles--;} cycles-=3;}

/*BIT abs*/   void op2C()  {templ=abs(); tempw=readmemw(templ);         struct_p.z=!(reg_A.w&tempw);  struct_p.v=tempw&0x4000; struct_p.n=tempw&0x8000; cycles-=5;}
/*BIT abs*/   void op2Cm() {templ=abs(); temp=readmem(templ>>16,templ); struct_p.z=!(reg_A.b.l&temp); struct_p.v=temp&0x40;    struct_p.n=temp&0x80;    cycles-=4;}
/*BIT abs,x*/ void op3C()  {templ=absx();tempw=readmemw(templ);         struct_p.z=!(reg_A.w&tempw);  struct_p.v=tempw&0x4000; struct_p.n=tempw&0x8000; cycles-=5;}
/*BIT abs,x*/ void op3Cm() {templ=absx();temp=readmem(templ>>16,templ); struct_p.z=!(reg_A.b.l&temp); struct_p.v=temp&0x40;    struct_p.n=temp&0x80;    cycles-=4;}
/*BIT #*/     void op89()  {tempw=getword(); struct_p.z=!(reg_A.w&tempw); struct_p.v=tempw&0x4000; struct_p.n=tempw&0x8000; cycles-=3; setzf=0;}
/*BIT #*/     void op89m() {temp=readmem(pbr,pc); pc++; struct_p.z=!(reg_A.b.l&temp); struct_p.v=temp&0x40; struct_p.n=temp&0x80; cycles-=2;}
/*BIT zp*/    void op24()  {templ=zp();  tempw=readmemw(templ);         struct_p.z=!(reg_A.w&tempw);  struct_p.v=tempw&0x4000; struct_p.n=tempw&0x8000; cycles-=4;}
/*BIT zp*/    void op24m() {templ=zp();  temp=readmem(templ>>16,templ); struct_p.z=!(reg_A.b.l&temp); struct_p.v=temp&0x40;    struct_p.n=temp&0x80;    cycles-=3;}
/*BIT zp,x*/  void op34()  {templ=zpx(); tempw=readmemw(templ);         struct_p.z=!(reg_A.w&tempw);  struct_p.v=tempw&0x4000; struct_p.n=tempw&0x8000; cycles-=5;}
/*BIT zp,x*/  void op34m() {templ=zpx(); temp=readmem(templ>>16,templ); struct_p.z=!(reg_A.b.l&temp); struct_p.v=temp&0x40;    struct_p.n=temp&0x80;    cycles-=4;}

/*CLC*/ void op18() {struct_p.c=0; cycles-=2;}
/*CLD*/ void opD8() {struct_p.d=0; cycles-=2;}
/*CLI*/ void op58() {struct_p.i=0; cycles-=2;}
/*CLV*/ void opB8() {struct_p.v=0; cycles-=2;}

/*CMP abs*/   void opCD()  {templ=abs();  tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=5;}
/*CMP abs*/   void opCDm() {templ=abs();  temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=4;}
/*CMP abs,x*/ void opDD()  {templ=absx(); tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=5;}
/*CMP abs,x*/ void opDDm() {templ=absx(); temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=4;}
/*CMP abs,y*/ void opD9()  {templ=absy(); tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=5;}
/*CMP abs,y*/ void opD9m() {templ=absy(); temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=4;}
/*CMP far*/   void opCF()  {templ=far();  tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=6;}
/*CMP far*/   void opCFm() {templ=far();  temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=5;}
/*CMP far,x*/ void opDF()  {templ=farx(); tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=8;}
/*CMP far,x*/ void opDFm() {templ=farx(); temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=7;}
/*CMP imm*/   void opC9()  {tempw=getword(); setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw); cycles-=3;}
/*CMP imm*/   void opC9m() {temp=readmem(pbr,pc); pc++; setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=2;}
/*CMP ()*/    void opD2()  {templ=ind();  tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=6;}
/*CMP ()*/    void opD2m() {templ=ind();  temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=5;}
/*CMP (),y*/  void opD1()  {templ=indy(); tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=6;}
/*CMP (),y*/  void opD1m() {templ=indy(); temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=5;}
/*CMP []*/    void opC7()  {templ=indl(); tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=7;}
/*CMP []*/    void opC7m() {templ=indl(); temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=6;}
/*CMP [],y*/  void opD7()  {templ=indly();tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=7;}
/*CMP [],y*/  void opD7m() {templ=indly();temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=6;}
/*CMP sp*/    void opC3()  {templ=sp();   tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=4;}
/*CMP sp*/    void opC3m() {templ=sp();   temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=3;}
/*CMP zp*/    void opC5()  {templ=zp();   tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=5;}
/*CMP zp*/    void opC5m() {templ=zp();   temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=4;}
/*CMP zp,x*/  void opD5()  {templ=zpx();  tempw=readmemw(templ);         setzn16(reg_A.w-tempw); struct_p.c=(reg_A.w>=tempw);  cycles-=5;}
/*CMP zp,x*/  void opD5m() {templ=zpx();  temp=readmem(templ>>16,templ); setzn8(reg_A.b.l-temp); struct_p.c=(reg_A.b.l>=temp); cycles-=4;}

/*CPX abs*/ void opEC()  {templ=abs();  tempw=readmemw(templ);         setzn16(reg_X.w-tempw); struct_p.c=(reg_X.w>=tempw);  cycles-=5;}
/*CPX abs*/ void opECx() {templ=abs();  temp=readmem(templ>>16,templ); setzn8(reg_X.b.l-temp); struct_p.c=(reg_X.b.l>=temp); cycles-=4;}
/*CPX imm*/ void opE0()  {tempw=getword(); setzn16(reg_X.w-tempw); struct_p.c=(reg_X.w>=tempw); cycles-=3;}
/*CPX imm*/ void opE0x() {temp=readmem(pbr,pc); pc++; setzn8(reg_X.b.l-temp); struct_p.c=(reg_X.b.l>=temp); cycles-=2;}
/*CPX zp*/  void opE4()  {templ=zp();   tempw=readmemw(templ);         setzn16(reg_X.w-tempw); struct_p.c=(reg_X.w>=tempw);  cycles-=4;}
/*CPX zp*/  void opE4x() {templ=zp();   temp=readmem(templ>>16,templ); setzn8(reg_X.b.l-temp); struct_p.c=(reg_X.b.l>=temp); cycles-=3;}

/*CPY abs*/ void opCC()  {templ=abs();  tempw=readmemw(templ);         setzn16(reg_Y.w-tempw); struct_p.c=(reg_Y.w>=tempw);  cycles-=5;}
/*CPY abs*/ void opCCx() {templ=abs();  temp=readmem(templ>>16,templ); setzn8(reg_Y.b.l-temp); struct_p.c=(reg_Y.b.l>=temp); cycles-=4;}
/*CPY imm*/ void opC0()  {tempw=getword(); setzn16(reg_Y.w-tempw); struct_p.c=(reg_Y.w>=tempw); cycles-=3;}
/*CPY imm*/ void opC0x() {temp=readmem(pbr,pc); pc++; setzn8(reg_Y.b.l-temp); struct_p.c=(reg_Y.b.l>=temp); cycles-=2;}
/*CPY zp*/  void opC4()  {templ=zp();   tempw=readmemw(templ);         setzn16(reg_Y.w-tempw); struct_p.c=(reg_Y.w>=tempw);  cycles-=4;}
/*CPY zp*/  void opC4x() {templ=zp();   temp=readmem(templ>>16,templ); setzn8(reg_Y.b.l-temp); struct_p.c=(reg_Y.b.l>=temp); cycles-=3;}

/*DEC abs*/    void opCE()  {templ=abs();  tempw=readmemw(templ);         tempw--; setzn16(tempw); writememw(templ,tempw); cycles-=8;}
/*DEC abs*/    void opCEm() {templ=abs();  temp=readmem(templ>>16,templ); temp--;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=6;}
/*DEC abs,x*/  void opDE()  {templ=absx(); tempw=readmemw(templ);         tempw--; setzn16(tempw); writememw(templ,tempw); cycles-=9;}
/*DEC abs,x*/  void opDEm() {templ=absx(); temp=readmem(templ>>16,templ); temp--;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=7;}
/*DEC zp*/     void opC6()  {templ=zp();   tempw=readmemw(templ);         tempw--; setzn16(tempw); writememw(templ,tempw); cycles-=7;}
/*DEC zp*/     void opC6m() {templ=zp();   temp=readmem(templ>>16,templ); temp--;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=5;}
/*DEC zp,x*/   void opD6()  {templ=zpx();  tempw=readmemw(templ);         tempw--; setzn16(tempw); writememw(templ,tempw); cycles-=8;}
/*DEC zp,x*/   void opD6m() {templ=zpx();  temp=readmem(templ>>16,templ); temp--;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=6;}
/*DEC A*/      void op3A()  {reg_A.w--; setzn16(reg_A.w); cycles-=2;}
/*DEC A*/      void op3Am() {reg_A.b.l--; setzn8(reg_A.b.l); cycles-=2;}

/*DEX*/ void opCA()  {reg_X.w--;   setzn16(reg_X.w);  cycles-=2;}
/*DEX*/ void opCAx() {reg_X.b.l--; setzn8(reg_X.b.l); cycles-=2;}
/*DEY*/ void op88()  {reg_Y.w--;   setzn16(reg_Y.w);  cycles-=2;}
/*DEY*/ void op88x() {reg_Y.b.l--; setzn8(reg_Y.b.l); cycles-=2;}

/*EOR abs*/   void op4D()  {templ=abs();  tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*EOR abs*/   void op4Dm() {templ=abs();  temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=4;}
/*EOR abs,x*/ void op5D()  {templ=absx(); tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*EOR abs,x*/ void op5Dm() {templ=absx(); temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=4;}
/*EOR abs,y*/ void op59()  {templ=absy(); tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*EOR abs,y*/ void op59m() {templ=absy(); temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=4;}
/*EOR far*/   void op4F()  {templ=far();  tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*EOR far*/   void op4Fm() {templ=far();  temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=5;}
/*EOR far,x*/ void op5F()  {templ=farx(); tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*EOR far,x*/ void op5Fm() {templ=farx(); temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=5;}
/*EOR imm*/   void op49()  {reg_A.w^=getword(); setzn16(reg_A.w); cycles-=3;}
/*EOR imm*/   void op49m() {reg_A.b.l^=readmem(pbr,pc); pc++; setzn8(reg_A.b.l); cycles-=2;}
/*EOR []*/    void op47()  {templ=indl(); tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*EOR []*/    void op47m() {templ=indl(); temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=6;}
/*EOR [],y*/  void op57()  {templ=indly();tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*EOR [],y*/  void op57m() {templ=indly();temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=6;}
/*EOR (),y*/  void op51()  {templ=indy(); tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*EOR (),y*/  void op51m() {templ=indy(); temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=5;}
/*EOR (s),y*/ void op53()  {templ=indys();tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*EOR (s),y*/ void op53m() {templ=indys();temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=5;}
/*EOR sp*/    void op43()  {templ=sp();   tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*EOR sp*/    void op43m() {templ=sp();   temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=4;}
/*EOR zp*/    void op45()  {templ=zp();   tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=4;}
/*EOR zp*/    void op45m() {templ=zp();   temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=3;}
/*EOR zp,x*/  void op55()  {templ=zpx();  tempw=readmemw(templ);         reg_A.w^=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*EOR zp,x*/  void op55m() {templ=zpx();  temp=readmem(templ>>16,templ); reg_A.b.l^=temp; setzn8(reg_A.b.l); cycles-=4;}

/*INC abs*/    void opEE()  {templ=abs();  tempw=readmemw(templ);         tempw++; setzn16(tempw); writememw(templ,tempw); cycles-=8;}
/*INC abs*/    void opEEm() {templ=abs();  temp=readmem(templ>>16,templ); temp++;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=6;}
/*INC abs,x*/  void opFE()  {templ=absx(); tempw=readmemw(templ);         tempw++; setzn16(tempw); writememw(templ,tempw); cycles-=9;}
/*INC abs,x*/  void opFEm() {templ=absx(); temp=readmem(templ>>16,templ); temp++;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=7;}
/*INC zp*/     void opE6()  {templ=zp();   tempw=readmemw(templ);         tempw++; setzn16(tempw); writememw(templ,tempw); cycles-=7;}
/*INC zp*/     void opE6m() {templ=zp();   temp=readmem(templ>>16,templ); temp++;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=5;}
/*INC zp,x*/   void opF6()  {templ=zpx();  tempw=readmemw(templ);         tempw++; setzn16(tempw); writememw(templ,tempw); cycles-=8;}
/*INC zp,x*/   void opF6m() {templ=zpx();  temp=readmem(templ>>16,templ); temp++;  setzn8(temp);   writemem(templ>>16,templ,temp); cycles-=6;}
/*INC A*/      void op1A()  {reg_A.w++; setzn16(reg_A.w); cycles-=2;}
/*INC A*/      void op1Am() {reg_A.b.l++; setzn8(reg_A.b.l); cycles-=2;}

/*INX*/ void opE8()  {reg_X.w++;   setzn16(reg_X.w);  cycles-=2;}
/*INX*/ void opE8x() {reg_X.b.l++; setzn8(reg_X.b.l); cycles-=2;}
/*INY*/ void opC8()  {reg_Y.w++;   setzn16(reg_Y.w);  cycles-=2;}
/*INY*/ void opC8x() {reg_Y.b.l++; setzn8(reg_Y.b.l); cycles-=2;}

/*JML*/ void op5C() {tempw=getword(); pbr=readmem(pbr,pc); pc=tempw; cycles-=4;}
/*JML*/ void opDC() {templ=jabs(); pc=readmemw(templ); pbr=readmem(templ>>16,templ+2); cycles-=6;/* printf("JML %02X %02X %02X\n",readmem(pbr,pc),readmem(pbr,pc+1),readmem(pbr,pc+2)); exit(0);*/}
/*JMP*/ void op4C() {pc=getword(); cycles-=3;}
/*JMP*/ void op6C() {templ=jabs(); pc=readmemw(templ); cycles-=5;}
/*JMP*/ void op7C() {templ=jabsxp();pc=readmemw(templ); cycles-=6;}

/*JSL*/ void op22()  {tempw=getword(); temp=readmem(pbr,pc); writemem(0,reg_S.w,pbr); writemem(0,reg_S.w-1,pc>>8); writemem(0,reg_S.w-2,pc&0xFF); reg_S.w-=3; pc=tempw; pbr=temp; cycles-=8;}
/*JSL*/ void op22e() {tempw=getword(); temp=readmem(pbr,pc); writemem(0,reg_S.w,pbr); writemem(0,reg_S.w-1,pc>>8); writemem(0,reg_S.w-2,pc&0xFF); reg_S.b.l-=3; pc=tempw; pbr=temp; cycles-=8;}

/*JSR*/ void op20()  {tempw=getword(); pc--; writemem(0,reg_S.w,pc>>8); writemem(0,reg_S.w-1,pc&0xFF); reg_S.w-=2; pc=tempw; cycles-=6;}
/*JSR*/ void op20e() {tempw=getword(); pc--; writemem(0,reg_S.w,pc>>8); writemem(0,reg_S.w-1,pc&0xFF); reg_S.b.l-=2; pc=tempw; cycles-=6;}

/*JSR*/ void opFC()  {templ=jabsxp(); tempw=readmemw(templ); pc--; writemem(0,reg_S.w,pc>>8); writemem(0,reg_S.w-1,pc&0xFF); reg_S.w-=2;   pc=tempw; cycles-=8;}
/*JSR*/ void opFCe() {templ=jabsxp(); tempw=readmemw(templ); pc--; writemem(0,reg_S.w,pc>>8); writemem(0,reg_S.w-1,pc&0xFF); reg_S.b.l-=2; pc=tempw; cycles-=8;}

/*LDA imm*/   void opA9()  {reg_A.b.l=readmem(pbr,pc); reg_A.b.h=readmem(pbr,pc+1); pc+=2; setzn16(reg_A.w); cycles-=3;}
/*LDA imm*/   void opA9m() {reg_A.b.l=readmem(pbr,pc); pc++; setzn8(reg_A.b.l); cycles-=2;}

/*LDA abs*/   void opAD()  {templ=abs();  reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=5;}
/*LDA abs*/   void opADm() {templ=abs();  reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=4;}
/*LDA abs,x*/ void opBD()  {templ=absx(); reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=5;}
/*LDA abs,x*/ void opBDm() {templ=absx(); reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=4;}
/*LDA abs,y*/ void opB9()  {templ=absy(); reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=5;}
/*LDA abs,y*/ void opB9m() {templ=absy(); reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=4;}
/*LDA far*/   void opAF()  {templ=far();  reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=6;}
/*LDA far*/   void opAFm() {templ=far();  reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=5;}
/*LDA far,x*/ void opBF()  {templ=farx(); reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=6;}
/*LDA far,x*/ void opBFm() {templ=farx(); reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=5;}
/*LDA sp*/    void opA3()  {templ=sp();   reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=5;}
/*LDA sp*/    void opA3m() {templ=sp();   reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=4;}
/*LDA zp*/    void opA5()  {templ=zp();   reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=4;}
/*LDA zp*/    void opA5m() {templ=zp();   reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=3;}
/*LDA zp,x*/  void opB5()  {templ=zpx();  reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=5;}
/*LDA zp,x*/  void opB5m() {templ=zpx();  reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=4;}
/*LDA ()*/    void opB2()  {templ=ind();  reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=6;}
/*LDA ()*/    void opB2m() {templ=ind();  reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=5;}
/*LDA (),y*/  void opB1()  {templ=indy(); reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=6;}
/*LDA (),y*/  void opB1m() {templ=indy(); reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=5;}
/*LDA (s),y*/ void opB3()  {templ=indys();reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=8;}
/*LDA (s),y*/ void opB3m() {templ=indys();reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=7;}
/*LDA [],y*/  void opB7()  {templ=indly();reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=7;}
/*LDA [],y*/  void opB7m() {templ=indly();reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=6;}
/*LDA []*/    void opA7()  {templ=indl(); reg_A.w=readmemw(templ);            setzn16(reg_A.w);  cycles-=7;}
/*LDA []*/    void opA7m() {templ=indl(); reg_A.b.l=readmem(templ>>16,templ); setzn8(reg_A.b.l); cycles-=6;}

/*LDX imm*/   void opA2()  {reg_X.b.l=readmem(pbr,pc); reg_X.b.h=readmem(pbr,pc+1); pc+=2; setzn16(reg_X.w); cycles-=3;}
/*LDX imm*/   void opA2x() {reg_X.b.l=readmem(pbr,pc); pc++; setzn8(reg_X.b.l); cycles-=2;}
/*LDX abs*/   void opAE()  {templ=abs();  reg_X.w=readmemw(templ);            setzn16(reg_X.w);  cycles-=5;}
/*LDX abs*/   void opAEx() {templ=abs();  reg_X.b.l=readmem(templ>>16,templ); setzn8(reg_X.b.l); cycles-=4;}
/*LDX abs,y*/ void opBE()  {templ=absy(); reg_X.w=readmemw(templ);            setzn16(reg_X.w);  cycles-=5;}
/*LDX abs,y*/ void opBEx() {templ=absy(); reg_X.b.l=readmem(templ>>16,templ); setzn8(reg_X.b.l); cycles-=4;}
/*LDX zp*/    void opA6()  {templ=zp();  reg_X.w=readmemw(templ);            setzn16(reg_X.w);  cycles-=4;}
/*LDX zp*/    void opA6x() {templ=zp();  reg_X.b.l=readmem(templ>>16,templ); setzn8(reg_X.b.l); cycles-=3;}
/*LDX zp,y*/  void opB6()  {templ=zpy(); reg_X.w=readmemw(templ);            setzn16(reg_X.w);  cycles-=5;}
/*LDX zp,y*/  void opB6x() {templ=zpy(); reg_X.b.l=readmem(templ>>16,templ); setzn8(reg_X.b.l); cycles-=4;}

/*LDY imm*/   void opA0()  {reg_Y.b.l=readmem(pbr,pc); reg_Y.b.h=readmem(pbr,pc+1); pc+=2; setzn16(reg_Y.w); cycles-=3;}
/*LDY imm*/   void opA0x() {reg_Y.b.l=readmem(pbr,pc); pc++; setzn8(reg_Y.b.l); cycles-=2;}
/*LDY abs*/   void opAC()  {templ=abs();  reg_Y.w=readmemw(templ);            setzn16(reg_Y.w);  cycles-=5;}
/*LDY abs*/   void opACx() {templ=abs();  reg_Y.b.l=readmem(templ>>16,templ); setzn8(reg_Y.b.l); cycles-=4;}
/*LDY abs,x*/ void opBC()  {templ=absx(); reg_Y.w=readmemw(templ);            setzn16(reg_Y.w);  cycles-=5;}
/*LDY abs,x*/ void opBCx() {templ=absx(); reg_Y.b.l=readmem(templ>>16,templ); setzn8(reg_Y.b.l); cycles-=4;}
/*LDY zp*/    void opA4()  {templ=zp();   reg_Y.w=readmemw(templ);            setzn16(reg_Y.w);  cycles-=4;}
/*LDY zp*/    void opA4x() {templ=zp();   reg_Y.b.l=readmem(templ>>16,templ); setzn8(reg_Y.b.l); cycles-=3;}
/*LDY zp,x*/  void opB4()  {templ=zpx();  reg_Y.w=readmemw(templ);            setzn16(reg_Y.w);  cycles-=5;}
/*LDY zp,x*/  void opB4x() {templ=zpx();  reg_Y.b.l=readmem(templ>>16,templ); setzn8(reg_Y.b.l); cycles-=4;}

/*LSR A*/     void op4A()  {struct_p.c=reg_A.w&1; reg_A.w>>=1; setzn16(reg_A.w); cycles-=2;}
/*LSR A*/     void op4Am() {struct_p.c=reg_A.b.l&1; reg_A.b.l>>=1; setzn8(reg_A.b.l); cycles-=2;}
/*LSR abs*/   void op4E()  {templ=abs(); tempw=readmemw(templ);         struct_p.c=tempw&1; tempw>>=1; writememw(templ,tempw);         setzn16(tempw); cycles-=8;}
/*LSR abs*/   void op4Em() {templ=abs(); temp=readmem(templ>>16,templ); struct_p.c=temp&1;  temp>>=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=6;}
/*LSR abs,x*/ void op5E()  {templ=absx();tempw=readmemw(templ);         struct_p.c=tempw&1; tempw>>=1; writememw(templ,tempw);         setzn16(tempw); cycles-=9;}
/*LSR abs,x*/ void op5Em() {templ=absx();temp=readmem(templ>>16,templ); struct_p.c=temp&1;  temp>>=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=7;}
/*LSR zp*/    void op46()  {templ=zp();  tempw=readmemw(templ);         struct_p.c=tempw&1; tempw>>=1; writememw(templ,tempw);         setzn16(tempw); cycles-=7;}
/*LSR zp*/    void op46m() {templ=zp();  temp=readmem(templ>>16,templ); struct_p.c=temp&1;  temp>>=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=5;}
/*LSR zp,x*/  void op56()  {templ=zpx(); tempw=readmemw(templ);         struct_p.c=tempw&1; tempw>>=1; writememw(templ,tempw);         setzn16(tempw); cycles-=8;}
/*LSR zp,x*/  void op56m() {templ=zpx(); temp=readmem(templ>>16,templ); struct_p.c=temp&1;  temp>>=1;  writemem(templ>>16,templ,temp); setzn8(temp);   cycles-=6;}

/*MVN*/ void op54() {temp=readmem(pbr,pc);
                     dbr=temp;
                     tempb=readmem(pbr,pc+1);
                     pc+=2;
                     temp=readmem(tempb,reg_X.w);
                     writemem(dbr,reg_Y.w,temp);
                     reg_X.w++;
                     reg_Y.w++;
                     reg_A.w--;
                     if (reg_A.w!=0xFFFF) pc-=3;
                     cycles-=7;}
/*MVP*/ void op44() {temp=readmem(pbr,pc); dbr=temp; tempb=readmem(pbr,pc+1); pc+=2; tempw=readmem(tempb,reg_X.w); writemem(temp,reg_Y.w,tempw); reg_X.w--; reg_Y.w--; reg_A.w--; if (reg_A.w!=0xFFFF) pc-=3; cycles-=7;}

/*NOP*/ void opEA() {cycles-=2;}

/*ORA abs*/   void op0D()  {templ=abs();  tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*ORA abs*/   void op0Dm() {templ=abs();  temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=4;}
/*ORA abs,x*/ void op1D()  {templ=absx(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*ORA abs,x*/ void op1Dm() {templ=absx(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=4;}
/*ORA abs,y*/ void op19()  {templ=absy(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*ORA abs,y*/ void op19m() {templ=absy(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=4;}
/*ORA far*/   void op0F()  {templ=far();  tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*ORA far*/   void op0Fm() {templ=far();  temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=5;}
/*ORA far,x*/ void op1F()  {templ=farx(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*ORA far,x*/ void op1Fm() {templ=farx(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=5;}
/*ORA []*/    void op07()  {templ=indl(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*ORA []*/    void op07m() {templ=indl(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=6;}
/*ORA [],y*/  void op17()  {templ=indly();tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=7;}
/*ORA [],y*/  void op17m() {templ=indly();temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=6;}
/*ORA ()*/    void op12()  {templ=ind();  tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*ORA ()*/    void op12m() {templ=ind();  temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=5;}
/*ORA (),y*/  void op11()  {templ=indy(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*ORA (),y*/  void op11m() {templ=indy(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=5;}
/*ORA (,x)*/  void op01()  {templ=indx(); tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=6;}
/*ORA (,x)*/  void op01m() {templ=indx(); temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=5;}
/*ORA sp*/    void op03()  {templ=sp();   tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*ORA sp*/    void op03m() {templ=sp();   temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=4;}
/*ORA zp*/    void op05()  {templ=zp();   tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=4;}
/*ORA zp*/    void op05m() {templ=zp();   temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=3;}
/*ORA zp,x*/  void op15()  {templ=zpx();  tempw=readmemw(templ);         reg_A.w|=tempw;  setzn16(reg_A.w);  cycles-=5;}
/*ORA zp,x*/  void op15m() {templ=zpx();  temp=readmem(templ>>16,templ); reg_A.b.l|=temp; setzn8(reg_A.b.l); cycles-=4;}
/*ORA imm*/   void op09()  {reg_A.w|=getword(); setzn16(reg_A.w); cycles-=3;}
/*ORA imm*/   void op09m() {reg_A.b.l|=readmem(pbr,pc); pc++; setzn8(reg_A.b.l); cycles-=2;}

/*PEA*/ void opF4()  {tempw=getword(); writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.w-=2; cycles-=5;}
/*PEA*/ void opF4e() {tempw=getword(); writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.b.l-=2; cycles-=5;}

/*PEI*/ void opD4()  {tempw=ind(); writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.w-=2; cycles-=6;}
/*PEI*/ void opD4e() {tempw=ind(); writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.b.l-=2; cycles-=6;}

/*PER*/ void op62()  {tempw=getword(); tempw+=pc; writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.w-=2; cycles-=6;}
/*PER*/ void op62e() {tempw=getword(); tempw+=pc; writemem(0,reg_S.w,tempw>>8); writemem(0,reg_S.w-1,tempw); reg_S.b.l-=2; cycles-=6;}

/*PHA*/ void op48()  {writemem(0,reg_S.w,reg_A.b.h); writemem(0,reg_S.w-1,reg_A.b.l); reg_S.w-=2; cycles-=4;}
/*PHA*/ void op48m() {writemem(0,reg_S.w,reg_A.b.l); reg_S.w--; cycles-=3;}
/*PHA*/ void op48e() {writemem(0,reg_S.w,reg_A.b.l); reg_S.b.l--; cycles-=3;}

/*PHB*/ void op8B()  {writemem(0,reg_S.w,dbr); reg_S.w--;   cycles-=3;}
/*PHB*/ void op8Be() {writemem(0,reg_S.w,dbr); reg_S.b.l--; cycles-=3;}

/*PHD*/ void op0B()  {writemem(0,reg_S.w,dp>>8); writemem(0,reg_S.w-1,dp); reg_S.w-=2; cycles-=4;}

/*PHK*/ void op4B()  {writemem(0,reg_S.w,pbr); reg_S.w--;   cycles-=3;}
/*PHK*/ void op4Be() {writemem(0,reg_S.w,pbr); reg_S.b.l--; cycles-=3;}

/*PHP*/ void op08()  {temp=0; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4; 
                      if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                      if (struct_p.x) temp|=0x10; if (struct_p.m) temp|=0x20;
                      writemem(0,reg_S.w,temp); reg_S.w--; cycles-=3;}
/*PHP*/ void op08e() {temp=0x30; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4; 
                      if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                      writemem(0,reg_S.w,temp); reg_S.b.l--; cycles-=3; }

/*PHX*/ void opDA()  {writemem(0,reg_S.w,reg_X.b.h); writemem(0,reg_S.w-1,reg_X.b.l); reg_S.w-=2; cycles-=4;}
/*PHX*/ void opDAx() {writemem(0,reg_S.w,reg_X.b.l); reg_S.w--; cycles-=3;}
/*PHX*/ void opDAe() {writemem(0,reg_S.w,reg_X.b.l); reg_S.b.l--; cycles-=3;}

/*PHY*/ void op5A()  {writemem(0,reg_S.w,reg_Y.b.h); writemem(0,reg_S.w-1,reg_Y.b.l); reg_S.w-=2; cycles-=4;}
/*PHY*/ void op5Ax() {writemem(0,reg_S.w,reg_Y.b.l); reg_S.w--; cycles-=3;}
/*PHY*/ void op5Ae() {writemem(0,reg_S.w,reg_Y.b.l); reg_S.b.l--; cycles-=3;}

/*PLA*/ void op68()  {reg_A.b.l=readmem(0,reg_S.w+1); reg_A.b.h=readmem(0,reg_S.w+2); reg_S.w+=2; setzn16(reg_A.w); cycles-=5;}
/*PLA*/ void op68m() {reg_A.b.l=readmem(0,reg_S.w+1); reg_S.w++; setzn8(reg_A.b.l); cycles-=4;}
/*PLA*/ void op68e() {reg_A.b.l=readmem(0,reg_S.w+1); reg_S.b.l++; setzn8(reg_A.b.l); cycles-=4;}

/*PLB*/ void opAB()  {reg_S.w++;   dbr=readmem(0,reg_S.w); cycles-=4;}
/*PLB*/ void opABe() {reg_S.b.l++; dbr=readmem(0,reg_S.w); cycles-=4;}

/*PLD*/ void op2B()  {dp=readmem(0,reg_S.w+1); dp|=(readmem(0,reg_S.w+2)<<8); reg_S.w+=2; cycles-=5;}

/*PLP*/ void op28()  {temp=readmem(0,reg_S.w+1); reg_S.w++; struct_p.c=temp&1; struct_p.z=temp&2;
                      struct_p.i=temp&4; struct_p.d=temp&8; struct_p.v=temp&0x40; struct_p.n=temp&0x80;
                      struct_p.x=temp&0x10; struct_p.m=temp&0x20; updatemode(); cycles-=4;}
/*PLP*/ void op28e() {temp=readmem(0,reg_S.w+1); reg_S.b.l++; struct_p.c=temp&1; struct_p.z=temp&2;
                      struct_p.i=temp&4; struct_p.d=temp&8; struct_p.v=temp&0x40; struct_p.n=temp&0x80;
                      updatemode(); cycles-=4;}
                      
/*PLX*/ void opFA()  {reg_X.b.l=readmem(0,reg_S.w+1); reg_X.b.h=readmem(0,reg_S.w+2); reg_S.w+=2; setzn16(reg_X.w); cycles-=5;}
/*PLX*/ void opFAx() {reg_X.b.l=readmem(0,reg_S.w+1); reg_S.w++; setzn8(reg_X.b.l); cycles-=4;}
/*PLX*/ void opFAe() {reg_X.b.l=readmem(0,reg_S.w+1); reg_S.b.l++; setzn8(reg_X.b.l); cycles-=4;}

/*PLY*/ void op7A()  {reg_Y.b.l=readmem(0,reg_S.w+1); reg_Y.b.h=readmem(0,reg_S.w+2); reg_S.w+=2; setzn16(reg_Y.w); cycles-=5;}
/*PLY*/ void op7Ax() {reg_Y.b.l=readmem(0,reg_S.w+1); reg_S.w++; setzn8(reg_Y.b.l); cycles-=4;}
/*PLY*/ void op7Ae() {reg_Y.b.l=readmem(0,reg_S.w+1); reg_S.b.l++; setzn8(reg_Y.b.l); cycles-=4;}

/*REP*/ void opC2() {temp=readmem(pbr,pc); pc++;
                     if (temp&1) struct_p.c=0; if (temp&2) struct_p.z=0; if (temp&4) struct_p.z=0; if (temp&8) struct_p.n=0;
                     if (temp&0x40) struct_p.z=0; if (temp&0x80) struct_p.n=0;
                     if (!struct_p.e) { if (temp&0x10) struct_p.x=0; if (temp&0x20) struct_p.m=0; }
                     updatemode();
                     cycles-=3;}

/*ROL A*/     void op2A()  {tempi=struct_p.c; struct_p.c=reg_A.w&0x8000; reg_A.w<<=1; if (tempi) reg_A.w|=1; setzn16(reg_A.w); cycles-=2;}
/*ROL A*/     void op2Am() {tempi=struct_p.c; struct_p.c=reg_A.b.l&0x80; reg_A.b.l<<=1; if (tempi) reg_A.b.l|=1; setzn8(reg_A.b.l); cycles-=2;}
/*ROL abs*/   void op2E()  {templ=abs(); tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&0x8000; tempw<<=1; if (tempi) tempw|=1; setzn16(reg_A.w); writememw(templ,tempw); cycles-=8;}
/*ROL abs*/   void op2Em() {templ=abs(); temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&0x80;    temp<<=1;  if (tempi) temp|=1;  setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=6;}
/*ROL abs,x*/ void op3E()  {templ=absx();tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&0x8000; tempw<<=1; if (tempi) tempw|=1; setzn16(reg_A.w); writememw(templ,tempw); cycles-=9;}
/*ROL abs,x*/ void op3Em() {templ=absx();temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&0x80;    temp<<=1;  if (tempi) temp|=1;  setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=7;}
/*ROL zp*/    void op26()  {templ=zp();  tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&0x8000; tempw<<=1; if (tempi) tempw|=1; setzn16(reg_A.w); writememw(templ,tempw); cycles-=7;}
/*ROL zp*/    void op26m() {templ=zp();  temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&0x80;    temp<<=1;  if (tempi) temp|=1;  setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=5;}
/*ROL zp,x*/  void op36()  {templ=zpx(); tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&0x8000; tempw<<=1; if (tempi) tempw|=1; setzn16(reg_A.w); writememw(templ,tempw); cycles-=8;}
/*ROL zp,x*/  void op36m() {templ=zpx(); temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&0x80;    temp<<=1;  if (tempi) temp|=1;  setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=6;}

/*ROR A*/     void op6A()  {tempi=struct_p.c; struct_p.c=reg_A.w&1; reg_A.w>>=1; if (tempi) reg_A.w|=0x8000; setzn16(reg_A.w); cycles-=2;}
/*ROR A*/     void op6Am() {tempi=struct_p.c; struct_p.c=reg_A.b.l&1; reg_A.b.l>>=1; if (tempi) reg_A.b.l|=0x80; setzn8(reg_A.b.l); cycles-=2;}
/*ROR abs*/   void op6E()  {templ=abs(); tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&1; tempw>>=1; if (tempi) tempw|=0x8000; setzn16(reg_A.w); writememw(templ,tempw); cycles-=8;}
/*ROR abs*/   void op6Em() {templ=abs(); temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&1;  temp>>=1;  if (tempi) temp|=0x80;    setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=6;}
/*ROR abs,x*/ void op7E()  {templ=absx();tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&1; tempw>>=1; if (tempi) tempw|=0x8000; setzn16(reg_A.w); writememw(templ,tempw); cycles-=9;}
/*ROR abs,x*/ void op7Em() {templ=absx();temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&1;  temp>>=1;  if (tempi) temp|=0x80;    setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=7;}
/*ROR zp*/    void op66()  {templ=zp();  tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&1; tempw>>=1; if (tempi) tempw|=0x8000; setzn16(reg_A.w); writememw(templ,tempw); cycles-=7;}
/*ROR zp*/    void op66m() {templ=zp();  temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&1;  temp>>=1;  if (tempi) temp|=0x80;    setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=5;}
/*ROR zp,x*/  void op76()  {templ=zpx(); tempw=readmemw(templ);         tempi=struct_p.c; struct_p.c=tempw&1; tempw>>=1; if (tempi) tempw|=0x8000; setzn16(reg_A.w); writememw(templ,tempw); cycles-=8;}
/*ROR zp,x*/  void op76m() {templ=zpx(); temp=readmem(templ>>16,templ); tempi=struct_p.c; struct_p.c=temp&1;  temp>>=1;  if (tempi) temp|=0x80;    setzn8(reg_A.b.l); writemem(templ>>16,templ,temp); cycles-=6;}

/*RTI*/ void op40()  {temp=readmem(0,reg_S.w+1); reg_S.w++; struct_p.c=temp&1; struct_p.z=temp&2;
                      struct_p.i=temp&4; struct_p.d=temp&8; struct_p.v=temp&0x40; struct_p.n=temp&0x80;
                      struct_p.x=temp&0x10; struct_p.m=temp&0x20;
                      pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8)); reg_S.w+=2;
                      pbr=readmem(0,reg_S.w+1); reg_S.w++; cycles-=7; updatemode();}
/*RTI*/ void op40e() {temp=readmem(0,reg_S.w+1); reg_S.b.l++; struct_p.c=temp&1; struct_p.z=temp&2;
                      struct_p.i=temp&4; struct_p.d=temp&8; struct_p.v=temp&0x40; struct_p.n=temp&0x80;
                      pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8)); reg_S.b.l+=2; cycles-=6; }
                      
/*RTL*/ void op6B()  {pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8))+1; pbr=readmem(0,reg_S.w+3); reg_S.w+=3; cycles-=6;}
/*RTL*/ void op6Be() {pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8))+1; pbr=readmem(0,reg_S.w+3); reg_S.b.l+=3; cycles-=6;}

/*RTS*/ void op60()  {pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8))+1; reg_S.w+=2; cycles-=6;}
/*RTS*/ void op60e() {pc=(readmem(0,reg_S.w+1)|(readmem(0,reg_S.w+2)<<8))+1; reg_S.b.l+=2; cycles-=6;}

/*SBC abs*/   void opED()  {templ=abs();  tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=5;}
/*SBC abs*/   void opEDm() {templ=abs();  temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=4;}
/*SBC abs,x*/ void opFD()  {templ=absx(); tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=5;}
/*SBC abs,x*/ void opFDm() {templ=absx(); temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=4;}
/*SBC abs,y*/ void opF9()  {templ=absy(); tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=5;}
/*SBC abs,y*/ void opF9m() {templ=absy(); temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=4;}
/*SBC far*/   void opEF()  {templ=far();  tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=6;}
/*SBC far*/   void opEFm() {templ=far();  temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=5;}
/*SBC far,x*/ void opFF()  {templ=farx(); tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=6;}
/*SBC far,x*/ void opFFm() {templ=farx(); temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=5;}
/*SBC imm*/   void opE9()  {tempw=getword(); if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=3;}
/*SBC imm*/   void opE9m() {temp=readmem(pbr,pc); pc++; if (struct_p.d) {SBCBCD8()} else {SBCB8()} cycles-=2;}
/*SBC ()*/    void opF2()  {templ=ind();  tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=6;}
/*SBC ()*/    void opF2m() {templ=ind();  temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=5;}
/*SBC (),y*/  void opF1()  {templ=indy(); tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=6;}
/*SBC (),y*/  void opF1m() {templ=indy(); temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=5;}
/*SBC []*/    void opE7()  {templ=indl(); tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=7;}
/*SBC []*/    void opE7m() {templ=indl(); temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=6;}
/*SBC [],y*/  void opF7()  {templ=indly();tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=7;}
/*SBC [],y*/  void opF7m() {templ=indly();temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=6;}
/*SBC sp*/    void opE3()  {templ=sp();   tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=5;}
/*SBC sp*/    void opE3m() {templ=sp();   temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=4;}
/*SBC zp*/    void opE5()  {templ=zp();   tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=4;}
/*SBC zp*/    void opE5m() {templ=zp();   temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=3;}
/*SBC zp,x*/  void opF5()  {templ=zpx();  tempw=readmemw(templ);         if (struct_p.d) {SBCBCD16()} else {SBCB16()} cycles-=5;}
/*SBC zp,x*/  void opF5m() {templ=zpx();  temp=readmem(templ>>16,templ); if (struct_p.d) {SBCBCD8()}  else {SBCB8()}  cycles-=4;}

/*SEC*/ void op38() {struct_p.c=1; cycles-=2;}
/*SED*/ void opF8() {struct_p.d=1; cycles-=2;}
/*SEI*/ void op78() {struct_p.i=1; cycles-=2;}

/*SEP*/ void opE2() {temp=readmem(pbr,pc); pc++;
                     if (temp&1) struct_p.c=1; if (temp&2) struct_p.z=1; if (temp&4) struct_p.z=1; if (temp&8) struct_p.n=1;
                     if (temp&0x40) struct_p.z=1; if (temp&0x80) struct_p.n=1;                     
                     if (!struct_p.e) { if (temp&0x10) struct_p.x=1; if (temp&0x20) struct_p.m=1; }
                     updatemode();
                     cycles-=3;}

/*STA abs*/   void op8D()  {templ=abs();  writememw(templ,reg_A.w);            cycles-=5;}
/*STA abs*/   void op8Dm() {templ=abs();  writemem(templ>>16,templ,reg_A.b.l); cycles-=4;}
/*STA abs,x*/ void op9D()  {templ=absx(); writememw(templ,reg_A.w);            cycles-=6;}
/*STA abs,x*/ void op9Dm() {templ=absx(); writemem(templ>>16,templ,reg_A.b.l); cycles-=5;}
/*STA abs,y*/ void op99()  {templ=absy(); writememw(templ,reg_A.w);            cycles-=6;}
/*STA abs,y*/ void op99m() {templ=absy(); writemem(templ>>16,templ,reg_A.b.l); cycles-=5;}
/*STA far*/   void op8F()  {templ=far();  writememw(templ,reg_A.w);            cycles-=6;}
/*STA far*/   void op8Fm() {templ=far();  writemem(templ>>16,templ,reg_A.b.l); cycles-=5;}
/*STA far,x*/ void op9F()  {templ=farx(); writememw(templ,reg_A.w);            cycles-=6;}
/*STA far,x*/ void op9Fm() {templ=farx(); writemem(templ>>16,templ,reg_A.b.l); cycles-=5;}
/*STA ()*/    void op92()  {templ=ind();  writememw(templ,reg_A.w);            cycles-=6;}
/*STA ()*/    void op92m() {templ=ind();  writemem(templ>>16,templ,reg_A.b.l); cycles-=5;}
/*STA (,x)*/  void op81()  {templ=indx(); writememw(templ,reg_A.w);            cycles-=7;}
/*STA (,x)*/  void op81m() {templ=indx(); writemem(templ>>16,templ,reg_A.b.l); cycles-=6;}
/*STA (),y*/  void op91()  {templ=indy(); writememw(templ,reg_A.w);            cycles-=7;}
/*STA (),y*/  void op91m() {templ=indy(); writemem(templ>>16,templ,reg_A.b.l); cycles-=6;}
/*STA (s),y*/ void op93()  {templ=indys();writememw(templ,reg_A.w);            cycles-=8;}
/*STA (s),y*/ void op93m() {templ=indys();writemem(templ>>16,templ,reg_A.b.l); cycles-=7;}
/*STA []*/    void op87()  {templ=indl(); writememw(templ,reg_A.w);            cycles-=7;}
/*STA []*/    void op87m() {templ=indl(); writemem(templ>>16,templ,reg_A.b.l); cycles-=6;}
/*STA [],y*/  void op97()  {templ=indly();writememw(templ,reg_A.w);            cycles-=7;}
/*STA [],y*/  void op97m() {templ=indly();writemem(templ>>16,templ,reg_A.b.l); cycles-=6;}
/*STA sp*/    void op83()  {templ=sp();   writememw(templ,reg_A.w);            cycles-=5;}
/*STA sp*/    void op83m() {templ=sp();   writemem(templ>>16,templ,reg_A.b.l); cycles-=4;}
/*STA zp*/    void op85()  {templ=zp();   writememw(templ,reg_A.w);            cycles-=5;}
/*STA zp*/    void op85m() {templ=zp();   writemem(templ>>16,templ,reg_A.b.l); cycles-=4;}
/*STA zp,x*/  void op95()  {templ=zpx();  writememw(templ,reg_A.w);            cycles-=5;}
/*STA zp,x*/  void op95m() {templ=zpx();  writemem(templ>>16,templ,reg_A.b.l); cycles-=4;}

/*STX abs*/   void op8E()  {templ=abs();  writememw(templ,reg_X.w);            cycles-=5;}
/*STX abs*/   void op8Ex() {templ=abs();  writemem(templ>>16,templ,reg_X.b.l); cycles-=4;}
/*STX zp*/    void op86()  {templ=zp();   writememw(templ,reg_X.w);            cycles-=4;}
/*STX zp*/    void op86x() {templ=zp();   writemem(templ>>16,templ,reg_X.b.l); cycles-=3;}
/*STX zp,y*/  void op96()  {templ=zpy();  writememw(templ,reg_X.w);            cycles-=5;}
/*STX zp,y*/  void op96x() {templ=zpy();  writemem(templ>>16,templ,reg_X.b.l); cycles-=4;}

/*STY abs*/   void op8C()  {templ=abs();  writememw(templ,reg_Y.w);            cycles-=5;}
/*STY abs*/   void op8Cx() {templ=abs();  writemem(templ>>16,templ,reg_Y.b.l); cycles-=4;}
/*STY zp*/    void op84()  {templ=zp();   writememw(templ,reg_Y.w);            cycles-=4;}
/*STY zp*/    void op84x() {templ=zp();   writemem(templ>>16,templ,reg_Y.b.l); cycles-=3;}
/*STY zp,x*/  void op94()  {templ=zpx();  writememw(templ,reg_Y.w);            cycles-=5;}
/*STY zp,x*/  void op94x() {templ=zpx();  writemem(templ>>16,templ,reg_Y.b.l); cycles-=4;}

/*STZ abs*/   void op9C()  {templ=abs();  writememw(templ,0);          cycles-=5;}
/*STZ abs*/   void op9Cm() {templ=abs();  writemem(templ>>16,templ,0); cycles-=4;}
/*STZ abs,x*/ void op9E()  {templ=absx(); writememw(templ,0);          cycles-=6;}
/*STZ abs,x*/ void op9Em() {templ=absx(); writemem(templ>>16,templ,0); cycles-=5;}
/*STZ zp*/    void op64()  {templ=zp();   writememw(templ,0);          cycles-=4;}
/*STZ zp*/    void op64m() {templ=zp();   writemem(templ>>16,templ,0); cycles-=3;}
/*STZ zp,x*/  void op74()  {templ=zpx();  writememw(templ,0);          cycles-=5;}
/*STZ zp,x*/  void op74m() {templ=zpx();  writemem(templ>>16,templ,0); cycles-=4;}

/*TRB zp*/  void op14()  {templ=zp();  tempw=readmemw(templ);         struct_p.z=!(tempw&reg_A.w);  tempw&=~reg_A.w;  writememw(templ,tempw);         cycles-=7;}
/*TRB zp*/  void op14m() {templ=zp();  temp=readmem(templ>>16,templ); struct_p.z=!(temp&reg_A.b.l); temp&=~reg_A.b.l; writemem(templ>>16,templ,temp); cycles-=5;}
/*TRB abs*/ void op1C()  {templ=abs(); tempw=readmemw(templ);         struct_p.z=!(tempw&reg_A.w);  tempw&=~reg_A.w;  writememw(templ,tempw);         cycles-=8;}
/*TRB abs*/ void op1Cm() {templ=abs(); temp=readmem(templ>>16,templ); struct_p.z=!(temp&reg_A.b.l); temp&=~reg_A.b.l; writemem(templ>>16,templ,temp); cycles-=6;}

/*TSB zp*/  void op04()  {templ=zp(); tempw=readmemw(templ);         struct_p.z=!(tempw&reg_A.w);  tempw|=reg_A.w;  writememw(templ,tempw);         cycles-=7;}
/*TSB zp*/  void op04m() {templ=zp(); temp=readmem(templ>>16,templ); struct_p.z=!(temp&reg_A.b.l); temp|=reg_A.b.l; writemem(templ>>16,templ,temp); cycles-=5;}
/*TSB abs*/ void op0C()  {templ=abs();tempw=readmemw(templ);         struct_p.z=!(tempw&reg_A.w);  tempw|=reg_A.w;  writememw(templ,tempw);         cycles-=8;}
/*TSB abs*/ void op0Cm() {templ=abs();temp=readmem(templ>>16,templ); struct_p.z=!(temp&reg_A.b.l); temp|=reg_A.b.l; writemem(templ>>16,templ,temp); cycles-=6;}

/*TCD*/ void op5B()  {dp=reg_A.w; cycles-=2;}
/*TCS*/ void op1B()  {reg_S.w=reg_A.w; cycles-=2;}
/*TDC*/ void op7B()  {reg_A.w=dp; setzn16(reg_A.w); cycles-=2;}
/*TSC*/ void op3B()  {reg_A.w=reg_S.w; cycles-=2;}
/*TXA*/ void op8A()  {reg_A.w=reg_X.w; setzn16(reg_A.w); cycles-=2;}
/*TXA*/ void op8Am() {reg_A.b.l=reg_X.b.l; setzn8(reg_A.b.l); cycles-=2;}
/*TYA*/ void op98()  {reg_A.w=reg_Y.w; setzn16(reg_A.w); cycles-=2;}
/*TYA*/ void op98m() {reg_A.b.l=reg_Y.b.l; setzn8(reg_A.b.l); cycles-=2;}
/*TAX*/ void opAA()  {reg_X.w=reg_A.w; setzn16(reg_X.w); cycles-=2;}
/*TAX*/ void opAAx() {reg_X.b.l=reg_A.b.l; setzn8(reg_X.b.l); cycles-=2;}
/*TAY*/ void opA8()  {reg_Y.w=reg_A.w; setzn16(reg_Y.w); cycles-=2;}
/*TAY*/ void opA8x() {reg_Y.b.l=reg_A.b.l; setzn8(reg_Y.b.l); cycles-=2;}
/*TSX*/ void opBA()  {reg_X.w=reg_S.w; if (struct_p.x) reg_X.b.h=0; cycles-=2;}
/*TXS*/ void op9A()  {reg_S.w=reg_X.w; if (struct_p.e) reg_S.b.h=1; cycles-=2;}
/*TXY*/ void op9B()  {reg_Y.w=reg_X.w; setzn16(reg_Y.w); cycles-=2;}
/*TXY*/ void op9Bx() {reg_Y.b.l=reg_X.b.l; setzn8(reg_Y.b.l); cycles-=2;}
/*TYX*/ void opBB()  {reg_X.w=reg_Y.w; setzn16(reg_X.w); cycles-=2;}
/*TYX*/ void opBBx() {reg_X.b.l=reg_Y.b.l; setzn8(reg_X.b.l); cycles-=2;}

/*WAI*/ void opCB() {wai=1; pc--; cycles-=3;}

/*XBA*/ void opEB() {reg_A.w=(reg_A.w>>8)|(reg_A.w<<8); setzn8(reg_A.b.l); cycles-=3;}
/*XCE*/ void opFB() {tempi=struct_p.c; struct_p.c=struct_p.e; struct_p.e=tempi; updatemode(); cycles-=2;}


/*BRK*/
void op00()
{
        pc++;
        if (struct_p.e)
        {
                writemem(0,reg_S.w,pc>>8); reg_S.b.l--;
                writemem(0,reg_S.w,pc);    reg_S.b.l--;
                reg_S.b.h=1;
                temp=0x30; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                writemem(0,reg_S.w,temp);  reg_S.b.l--;
                pbr=0;
                pc=readmem(0,0xFFFE)|(readmem(0,0xFFFF)<<8);
                struct_p.i=1;
                dbr=0;
                cycles-=4;
        }
        else
        {
                writemem(0,reg_S.w,pbr);   reg_S.w--;
                writemem(0,reg_S.w,pc>>8); reg_S.w--;
                writemem(0,reg_S.w,pc);    reg_S.w--;
                temp=0; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                if (struct_p.x) temp|=0x10; if (struct_p.m) temp|=0x20;
                writemem(0,reg_S.w,temp);  reg_S.w--;
                pbr=0;
                pc=readmem(0,0xFFE6)|(readmem(0,0xFFE7)<<8);
                struct_p.i=1;
                cycles-=5;
        }
}

/*COP*/
void op02()
{
        pc++;
        if (struct_p.e)
        {
                writemem(0,reg_S.w,pc>>8); reg_S.b.l--;
                writemem(0,reg_S.w,pc);    reg_S.b.l--;
                reg_S.b.h=1;
                temp=0x30; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                writemem(0,reg_S.w,temp);  reg_S.b.l--;
                pbr=0;
                dbr=0;
                pc=readmem(0,0xFFF4)|(readmem(0,0xFFF5)<<8);
                struct_p.i=1;
                cycles-=4;
        }
        else
        {
                writemem(0,reg_S.w,pbr);   reg_S.w--;
                writemem(0,reg_S.w,pc>>8); reg_S.w--;
                writemem(0,reg_S.w,pc);    reg_S.w--;
                temp=0; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                if (struct_p.x) temp|=0x10; if (struct_p.m) temp|=0x20;
                writemem(0,reg_S.w,temp);  reg_S.w--;
                pbr=0;
                pc=readmem(0,0xFFE4)|(readmem(0,0xFFE5)<<8);
                struct_p.i=1;
                cycles-=5;
        }
}

void makeopcodetable()
{
        int c,d;
        for (c=0;c<256;c++)
        {
                for (d=0;d<5;d++)
                    opcodes[c][d]=badopcode;
        }
        for (c=0;c<5;c++)
        {
                opcodes[0x00][c]=op00;
                opcodes[0x01][c]=op01;
                opcodes[0x02][c]=op02;
                opcodes[0x03][c]=op03;
                opcodes[0x04][c]=op04;
                opcodes[0x05][c]=op05;
                opcodes[0x06][c]=op06;
                opcodes[0x07][c]=op07;
                opcodes[0x08][c]=op08;
                opcodes[0x09][c]=op09;
                opcodes[0x0A][c]=op0A;
                opcodes[0x0B][c]=op0B;
                opcodes[0x0C][c]=op0C;
                opcodes[0x0D][c]=op0D;
                opcodes[0x0E][c]=op0E;
                opcodes[0x0F][c]=op0F;
                opcodes[0x10][c]=op10;
                opcodes[0x11][c]=op11;
                opcodes[0x12][c]=op12;
                opcodes[0x14][c]=op14;
                opcodes[0x15][c]=op15;
                opcodes[0x16][c]=op16;
                opcodes[0x17][c]=op17;
                opcodes[0x18][c]=op18;
                opcodes[0x19][c]=op19;
                opcodes[0x1A][c]=op1A;
                opcodes[0x1B][c]=op1B;
                opcodes[0x1C][c]=op1C;
                opcodes[0x1D][c]=op1D;
                opcodes[0x1E][c]=op1E;
                opcodes[0x1F][c]=op1F;
                opcodes[0x20][c]=op20;
                opcodes[0x22][c]=op22;
                opcodes[0x23][c]=op23;
                opcodes[0x24][c]=op24;
                opcodes[0x25][c]=op25;
                opcodes[0x26][c]=op26;
                opcodes[0x27][c]=op27;
                opcodes[0x28][c]=op28;
                opcodes[0x29][c]=op29;
                opcodes[0x2A][c]=op2A;
                opcodes[0x2B][c]=op2B;
                opcodes[0x2C][c]=op2C;
                opcodes[0x2D][c]=op2D;
                opcodes[0x2E][c]=op2E;
                opcodes[0x2F][c]=op2F;
                opcodes[0x30][c]=op30;
                opcodes[0x31][c]=op31;
                opcodes[0x34][c]=op34;
                opcodes[0x35][c]=op35;
                opcodes[0x36][c]=op36;
                opcodes[0x37][c]=op37;
                opcodes[0x38][c]=op38;
                opcodes[0x39][c]=op39;
                opcodes[0x3A][c]=op3A;
                opcodes[0x3B][c]=op3B;
                opcodes[0x3C][c]=op3C;
                opcodes[0x3D][c]=op3D;
                opcodes[0x3E][c]=op3E;
                opcodes[0x3F][c]=op3F;
                opcodes[0x40][c]=op40;
                opcodes[0x43][c]=op43;
                opcodes[0x44][c]=op44;
                opcodes[0x45][c]=op45;
                opcodes[0x46][c]=op46;
                opcodes[0x47][c]=op47;
                opcodes[0x48][c]=op48;
                opcodes[0x49][c]=op49;
                opcodes[0x4A][c]=op4A;
                opcodes[0x4B][c]=op4B;
                opcodes[0x4C][c]=op4C;
                opcodes[0x4D][c]=op4D;
                opcodes[0x4E][c]=op4E;
                opcodes[0x4F][c]=op4F;
                opcodes[0x50][c]=op50;
                opcodes[0x51][c]=op51;
                opcodes[0x53][c]=op53;
                opcodes[0x54][c]=op54;
                opcodes[0x55][c]=op55;
                opcodes[0x56][c]=op56;
                opcodes[0x57][c]=op57;
                opcodes[0x58][c]=op58;
                opcodes[0x59][c]=op59;
                opcodes[0x5A][c]=op5A;
                opcodes[0x5B][c]=op5B;
                opcodes[0x5C][c]=op5C;
                opcodes[0x5D][c]=op5D;
                opcodes[0x5E][c]=op5E;
                opcodes[0x5F][c]=op5F;
                opcodes[0x60][c]=op60;
                opcodes[0x62][c]=op62;
                opcodes[0x63][c]=op63;
                opcodes[0x64][c]=op64;
                opcodes[0x65][c]=op65;
                opcodes[0x66][c]=op66;
                opcodes[0x67][c]=op67;
                opcodes[0x68][c]=op68;
                opcodes[0x69][c]=op69;
                opcodes[0x6A][c]=op6A;
                opcodes[0x6B][c]=op6B;
                opcodes[0x6C][c]=op6C;
                opcodes[0x6D][c]=op6D;
                opcodes[0x6E][c]=op6E;
                opcodes[0x6F][c]=op6F;
                opcodes[0x70][c]=op70;
                opcodes[0x71][c]=op71;
                opcodes[0x72][c]=op72;
                opcodes[0x73][c]=op73;
                opcodes[0x74][c]=op74;
                opcodes[0x75][c]=op75;
                opcodes[0x76][c]=op76;
                opcodes[0x77][c]=op77;
                opcodes[0x78][c]=op78;
                opcodes[0x79][c]=op79;
                opcodes[0x7A][c]=op7A;
                opcodes[0x7B][c]=op7B;
                opcodes[0x7C][c]=op7C;
                opcodes[0x7D][c]=op7D;
                opcodes[0x7E][c]=op7E;
                opcodes[0x7F][c]=op7F;
                opcodes[0x80][c]=op80;
                opcodes[0x81][c]=op81;
                opcodes[0x82][c]=op82;
                opcodes[0x83][c]=op83;
                opcodes[0x84][c]=op84;
                opcodes[0x85][c]=op85;
                opcodes[0x86][c]=op86;
                opcodes[0x87][c]=op87;
                opcodes[0x88][c]=op88;
                opcodes[0x89][c]=op89;
                opcodes[0x8A][c]=op8A;
                opcodes[0x8B][c]=op8B;
                opcodes[0x8C][c]=op8C;
                opcodes[0x8D][c]=op8D;
                opcodes[0x8E][c]=op8E;
                opcodes[0x8F][c]=op8F;
                opcodes[0x90][c]=op90;
                opcodes[0x91][c]=op91;
                opcodes[0x92][c]=op92;
                opcodes[0x93][c]=op93;
                opcodes[0x94][c]=op94;
                opcodes[0x95][c]=op95;
                opcodes[0x96][c]=op96;
                opcodes[0x97][c]=op97;
                opcodes[0x98][c]=op98;
                opcodes[0x99][c]=op99;
                opcodes[0x9A][c]=op9A;
                opcodes[0x9B][c]=op9B;
                opcodes[0x9C][c]=op9C;
                opcodes[0x9D][c]=op9D;
                opcodes[0x9E][c]=op9E;
                opcodes[0x9F][c]=op9F;
                opcodes[0xA0][c]=opA0;
                opcodes[0xA2][c]=opA2;
                opcodes[0xA3][c]=opA3;
                opcodes[0xA4][c]=opA4;
                opcodes[0xA5][c]=opA5;
                opcodes[0xA6][c]=opA6;
                opcodes[0xA7][c]=opA7;
                opcodes[0xA8][c]=opA8;
                opcodes[0xA9][c]=opA9;
                opcodes[0xAA][c]=opAA;
                opcodes[0xAB][c]=opAB;
                opcodes[0xAC][c]=opAC;
                opcodes[0xAD][c]=opAD;
                opcodes[0xAE][c]=opAE;
                opcodes[0xAF][c]=opAF;
                opcodes[0xB0][c]=opB0;
                opcodes[0xB1][c]=opB1;
                opcodes[0xB2][c]=opB2;
                opcodes[0xB3][c]=opB3;
                opcodes[0xB4][c]=opB4;
                opcodes[0xB5][c]=opB5;
                opcodes[0xB6][c]=opB6;
                opcodes[0xB7][c]=opB7;
                opcodes[0xB8][c]=opB8;
                opcodes[0xB9][c]=opB9;
                opcodes[0xBA][c]=opBA;
                opcodes[0xBB][c]=opBB;
                opcodes[0xBC][c]=opBC;
                opcodes[0xBD][c]=opBD;
                opcodes[0xBE][c]=opBE;
                opcodes[0xBF][c]=opBF;
                opcodes[0xC0][c]=opC0;
                opcodes[0xC2][c]=opC2;
                opcodes[0xC3][c]=opC3;
                opcodes[0xC4][c]=opC4;
                opcodes[0xC5][c]=opC5;
                opcodes[0xC6][c]=opC6;
                opcodes[0xC7][c]=opC7;
                opcodes[0xC8][c]=opC8;
                opcodes[0xC9][c]=opC9;
                opcodes[0xCA][c]=opCA;
                opcodes[0xCB][c]=opCB;
                opcodes[0xCC][c]=opCC;
                opcodes[0xCD][c]=opCD;
                opcodes[0xCE][c]=opCE;
                opcodes[0xCF][c]=opCF;
                opcodes[0xD0][c]=opD0;
                opcodes[0xD1][c]=opD1;
                opcodes[0xD2][c]=opD2;
                opcodes[0xD4][c]=opD4;
                opcodes[0xD5][c]=opD5;
                opcodes[0xD6][c]=opD6;
                opcodes[0xD7][c]=opD7;
                opcodes[0xD8][c]=opD8;
                opcodes[0xD9][c]=opD9;
                opcodes[0xDA][c]=opDA;
                opcodes[0xDC][c]=opDC;
                opcodes[0xDD][c]=opDD;
                opcodes[0xDE][c]=opDE;
                opcodes[0xDF][c]=opDF;
                opcodes[0xE0][c]=opE0;
                opcodes[0xE2][c]=opE2;
                opcodes[0xE3][c]=opE3;
                opcodes[0xE4][c]=opE4;
                opcodes[0xE5][c]=opE5;
                opcodes[0xE6][c]=opE6;
                opcodes[0xE7][c]=opE7;
                opcodes[0xE8][c]=opE8;
                opcodes[0xE9][c]=opE9;
                opcodes[0xEA][c]=opEA;
                opcodes[0xEB][c]=opEB;
                opcodes[0xEC][c]=opEC;
                opcodes[0xED][c]=opED;
                opcodes[0xEE][c]=opEE;
                opcodes[0xEF][c]=opEF;
                opcodes[0xF0][c]=opF0;
                opcodes[0xF1][c]=opF1;
                opcodes[0xF2][c]=opF2;
                opcodes[0xF4][c]=opF4;
                opcodes[0xF5][c]=opF5;
                opcodes[0xF6][c]=opF6;
                opcodes[0xF7][c]=opF7;
                opcodes[0xF8][c]=opF8;
                opcodes[0xF9][c]=opF9;
                opcodes[0xFA][c]=opFA;
                opcodes[0xFB][c]=opFB;
                opcodes[0xFC][c]=opFC;
                opcodes[0xFD][c]=opFD;
                opcodes[0xFE][c]=opFE;
                opcodes[0xFF][c]=opFF;
        }
        opcodes[0x08][4]=op08e;
        opcodes[0x20][4]=op20e;
        opcodes[0x22][4]=op22e;
        opcodes[0x28][4]=op28e;
        opcodes[0x40][4]=op40e;
        opcodes[0x48][4]=op48e;
        opcodes[0x4B][4]=op4Be;
        opcodes[0x5A][4]=op5Ae;
        opcodes[0x60][4]=op60e;
        opcodes[0x62][4]=op62e;
        opcodes[0x68][4]=op68e;
        opcodes[0x6B][4]=op6Be;
        opcodes[0x7A][4]=op7Ae;
        opcodes[0x8B][4]=op8Be;
        opcodes[0xAB][4]=opABe;
        opcodes[0xD4][4]=opD4e;
        opcodes[0xDA][4]=opDAe;
        opcodes[0xF4][4]=opF4e;
        opcodes[0xFA][4]=opFAe;
        opcodes[0xFC][4]=opFCe;
        opcodes[0x01][2]=opcodes[0x01][3]=opcodes[0x01][4]=op01m;
        opcodes[0x03][2]=opcodes[0x03][3]=opcodes[0x03][4]=op03m;
        opcodes[0x04][2]=opcodes[0x04][3]=opcodes[0x04][4]=op04m;
        opcodes[0x05][2]=opcodes[0x05][3]=opcodes[0x05][4]=op05m;
        opcodes[0x06][2]=opcodes[0x06][3]=opcodes[0x06][4]=op06m;
        opcodes[0x07][2]=opcodes[0x07][3]=opcodes[0x07][4]=op07m;
        opcodes[0x09][2]=opcodes[0x09][3]=opcodes[0x09][4]=op09m;
        opcodes[0x0A][2]=opcodes[0x0A][3]=opcodes[0x0A][4]=op0Am;
        opcodes[0x0C][2]=opcodes[0x0C][3]=opcodes[0x0C][4]=op0Cm;
        opcodes[0x0D][2]=opcodes[0x0D][3]=opcodes[0x0D][4]=op0Dm;
        opcodes[0x0E][2]=opcodes[0x0E][3]=opcodes[0x0E][4]=op0Em;
        opcodes[0x0F][2]=opcodes[0x0F][3]=opcodes[0x0F][4]=op0Fm;
        opcodes[0x11][2]=opcodes[0x11][3]=opcodes[0x11][4]=op11m;
        opcodes[0x12][2]=opcodes[0x12][3]=opcodes[0x12][4]=op12m;
        opcodes[0x14][2]=opcodes[0x14][3]=opcodes[0x14][4]=op14m;
        opcodes[0x15][2]=opcodes[0x15][3]=opcodes[0x15][4]=op15m;
        opcodes[0x16][2]=opcodes[0x16][3]=opcodes[0x16][4]=op16m;
        opcodes[0x17][2]=opcodes[0x17][3]=opcodes[0x17][4]=op17m;
        opcodes[0x19][2]=opcodes[0x19][3]=opcodes[0x19][4]=op19m;
        opcodes[0x1A][2]=opcodes[0x1A][3]=opcodes[0x1A][4]=op1Am;
        opcodes[0x1C][2]=opcodes[0x1C][3]=opcodes[0x1C][4]=op1Cm;
        opcodes[0x1D][2]=opcodes[0x1D][3]=opcodes[0x1D][4]=op1Dm;
        opcodes[0x1E][2]=opcodes[0x1E][3]=opcodes[0x1E][4]=op1Em;
        opcodes[0x1F][2]=opcodes[0x1F][3]=opcodes[0x1F][4]=op1Fm;
        opcodes[0x23][2]=opcodes[0x23][3]=opcodes[0x23][4]=op23m;
        opcodes[0x24][2]=opcodes[0x24][3]=opcodes[0x24][4]=op24m;
        opcodes[0x25][2]=opcodes[0x25][3]=opcodes[0x25][4]=op25m;
        opcodes[0x26][2]=opcodes[0x26][3]=opcodes[0x26][4]=op26m;
        opcodes[0x27][2]=opcodes[0x27][3]=opcodes[0x27][4]=op27m;
        opcodes[0x29][2]=opcodes[0x29][3]=opcodes[0x29][4]=op29m;
        opcodes[0x2A][2]=opcodes[0x2A][3]=opcodes[0x2A][4]=op2Am;
        opcodes[0x2C][2]=opcodes[0x2C][3]=opcodes[0x2C][4]=op2Cm;
        opcodes[0x2D][2]=opcodes[0x2D][3]=opcodes[0x2D][4]=op2Dm;
        opcodes[0x2E][2]=opcodes[0x2E][3]=opcodes[0x2E][4]=op2Em;
        opcodes[0x2F][2]=opcodes[0x2F][3]=opcodes[0x2F][4]=op2Fm;
        opcodes[0x31][2]=opcodes[0x31][3]=opcodes[0x31][4]=op31m;
        opcodes[0x34][2]=opcodes[0x34][3]=opcodes[0x34][4]=op34m;        
        opcodes[0x35][2]=opcodes[0x35][3]=opcodes[0x35][4]=op35m;
        opcodes[0x36][2]=opcodes[0x36][3]=opcodes[0x36][4]=op36m;
        opcodes[0x37][2]=opcodes[0x37][3]=opcodes[0x37][4]=op37m;
        opcodes[0x39][2]=opcodes[0x39][3]=opcodes[0x39][4]=op39m;
        opcodes[0x3A][2]=opcodes[0x3A][3]=opcodes[0x3A][4]=op3Am;
        opcodes[0x3C][2]=opcodes[0x3C][3]=opcodes[0x3C][4]=op3Cm;
        opcodes[0x3D][2]=opcodes[0x3D][3]=opcodes[0x3D][4]=op3Dm;
        opcodes[0x3E][2]=opcodes[0x3E][3]=opcodes[0x3E][4]=op3Em;
        opcodes[0x3F][2]=opcodes[0x3F][3]=opcodes[0x3F][4]=op3Fm;
        opcodes[0x43][2]=opcodes[0x43][3]=opcodes[0x43][4]=op43m;
        opcodes[0x45][2]=opcodes[0x45][3]=opcodes[0x45][4]=op45m;
        opcodes[0x46][2]=opcodes[0x46][3]=opcodes[0x46][4]=op46m;
        opcodes[0x47][2]=opcodes[0x47][3]=opcodes[0x47][4]=op47m;
        opcodes[0x48][2]=opcodes[0x48][3]=op48m;
        opcodes[0x49][2]=opcodes[0x49][3]=opcodes[0x49][4]=op49m;
        opcodes[0x4A][2]=opcodes[0x4A][3]=opcodes[0x4A][4]=op4Am;
        opcodes[0x4D][2]=opcodes[0x4D][3]=opcodes[0x4D][4]=op4Dm;
        opcodes[0x4E][2]=opcodes[0x4E][3]=opcodes[0x4E][4]=op4Em;
        opcodes[0x4F][2]=opcodes[0x4F][3]=opcodes[0x4F][4]=op4Fm;
        opcodes[0x51][2]=opcodes[0x51][3]=opcodes[0x51][4]=op51m;
        opcodes[0x53][2]=opcodes[0x53][3]=opcodes[0x53][4]=op53m;
        opcodes[0x55][2]=opcodes[0x55][3]=opcodes[0x55][4]=op55m;
        opcodes[0x56][2]=opcodes[0x56][3]=opcodes[0x56][4]=op56m;
        opcodes[0x57][2]=opcodes[0x57][3]=opcodes[0x57][4]=op57m;
        opcodes[0x59][2]=opcodes[0x59][3]=opcodes[0x59][4]=op59m;
        opcodes[0x5A][1]=opcodes[0x5A][3]=op5Ax;
        opcodes[0x5D][2]=opcodes[0x5D][3]=opcodes[0x5D][4]=op5Dm;
        opcodes[0x5E][2]=opcodes[0x5E][3]=opcodes[0x5E][4]=op5Em;
        opcodes[0x5F][2]=opcodes[0x5F][3]=opcodes[0x5F][4]=op5Fm;
        opcodes[0x63][2]=opcodes[0x63][3]=opcodes[0x63][4]=op63m;
        opcodes[0x64][2]=opcodes[0x64][3]=opcodes[0x64][4]=op64m;
        opcodes[0x65][2]=opcodes[0x65][3]=opcodes[0x65][4]=op65m;
        opcodes[0x66][2]=opcodes[0x66][3]=opcodes[0x66][4]=op66m;
        opcodes[0x67][2]=opcodes[0x67][3]=opcodes[0x67][4]=op67m;
        opcodes[0x68][2]=opcodes[0x68][3]=op68m;
        opcodes[0x69][2]=opcodes[0x69][3]=opcodes[0x69][4]=op69m;
        opcodes[0x6A][2]=opcodes[0x6A][3]=opcodes[0x6A][4]=op6Am;
        opcodes[0x6D][2]=opcodes[0x6D][3]=opcodes[0x6D][4]=op6Dm;
        opcodes[0x6E][2]=opcodes[0x6E][3]=opcodes[0x6E][4]=op6Em;
        opcodes[0x6F][2]=opcodes[0x6F][3]=opcodes[0x6F][4]=op6Fm;
        opcodes[0x71][2]=opcodes[0x71][3]=opcodes[0x71][4]=op71m;
        opcodes[0x72][2]=opcodes[0x72][3]=opcodes[0x72][4]=op72m;
        opcodes[0x73][2]=opcodes[0x73][3]=opcodes[0x73][4]=op73m;
        opcodes[0x74][2]=opcodes[0x74][3]=opcodes[0x74][4]=op74m;
        opcodes[0x75][2]=opcodes[0x75][3]=opcodes[0x75][4]=op75m;
        opcodes[0x76][2]=opcodes[0x76][3]=opcodes[0x76][4]=op76m;
        opcodes[0x77][2]=opcodes[0x77][3]=opcodes[0x77][4]=op77m;
        opcodes[0x79][2]=opcodes[0x79][3]=opcodes[0x79][4]=op79m;
        opcodes[0x7A][1]=opcodes[0x7A][3]=op7Ax;
        opcodes[0x7D][2]=opcodes[0x7D][3]=opcodes[0x7D][4]=op7Dm;
        opcodes[0x7E][2]=opcodes[0x7E][3]=opcodes[0x7E][4]=op7Em;
        opcodes[0x7F][2]=opcodes[0x7F][3]=opcodes[0x7F][4]=op7Fm;
        opcodes[0x81][2]=opcodes[0x81][3]=opcodes[0x81][4]=op81m;
        opcodes[0x83][2]=opcodes[0x83][3]=opcodes[0x83][4]=op83m;
        opcodes[0x84][1]=opcodes[0x84][3]=opcodes[0x84][4]=op84x;
        opcodes[0x85][2]=opcodes[0x85][3]=opcodes[0x85][4]=op85m;
        opcodes[0x86][1]=opcodes[0x86][3]=opcodes[0x86][4]=op86x;
        opcodes[0x87][2]=opcodes[0x87][3]=opcodes[0x87][4]=op87m;
        opcodes[0x88][1]=opcodes[0x88][3]=opcodes[0x88][4]=op88x;
        opcodes[0x89][2]=opcodes[0x89][3]=opcodes[0x89][4]=op89m;
        opcodes[0x8A][2]=opcodes[0x8A][3]=opcodes[0x8A][4]=op8Am;
        opcodes[0x8C][1]=opcodes[0x8C][3]=opcodes[0x8C][4]=op8Cx;
        opcodes[0x8D][2]=opcodes[0x8D][3]=opcodes[0x8D][4]=op8Dm;
        opcodes[0x8E][1]=opcodes[0x8E][3]=opcodes[0x8E][4]=op8Ex;
        opcodes[0x8F][2]=opcodes[0x8F][3]=opcodes[0x8F][4]=op8Fm;
        opcodes[0x91][2]=opcodes[0x91][3]=opcodes[0x91][4]=op91m;
        opcodes[0x92][2]=opcodes[0x92][3]=opcodes[0x92][4]=op92m;
        opcodes[0x93][2]=opcodes[0x93][3]=opcodes[0x93][4]=op93m;
        opcodes[0x94][1]=opcodes[0x94][3]=opcodes[0x94][4]=op94x;
        opcodes[0x95][2]=opcodes[0x95][3]=opcodes[0x95][4]=op95m;
        opcodes[0x96][1]=opcodes[0x96][3]=opcodes[0x96][4]=op96x;
        opcodes[0x97][2]=opcodes[0x97][3]=opcodes[0x97][4]=op97m;
        opcodes[0x98][2]=opcodes[0x98][3]=opcodes[0x98][4]=op98m;
        opcodes[0x99][2]=opcodes[0x99][3]=opcodes[0x99][4]=op99m;
        opcodes[0x9B][1]=opcodes[0x9B][3]=opcodes[0x9B][4]=op9Bx;
        opcodes[0x9C][2]=opcodes[0x9C][3]=opcodes[0x9C][4]=op9Cm;
        opcodes[0x9D][2]=opcodes[0x9D][3]=opcodes[0x9D][4]=op9Dm;
        opcodes[0x9E][2]=opcodes[0x9E][3]=opcodes[0x9E][4]=op9Em;
        opcodes[0x9F][2]=opcodes[0x9F][3]=opcodes[0x9F][4]=op9Fm;
        opcodes[0xA0][1]=opcodes[0xA0][3]=opcodes[0xA0][4]=opA0x;
        opcodes[0xA2][1]=opcodes[0xA2][3]=opcodes[0xA2][4]=opA2x;
        opcodes[0xA3][2]=opcodes[0xA3][3]=opcodes[0xA3][4]=opA3m;
        opcodes[0xA4][1]=opcodes[0xA4][3]=opcodes[0xA4][4]=opA4x;
        opcodes[0xA5][2]=opcodes[0xA5][3]=opcodes[0xA5][4]=opA5m;
        opcodes[0xA6][1]=opcodes[0xA6][3]=opcodes[0xA6][4]=opA6x;
        opcodes[0xA7][2]=opcodes[0xA7][3]=opcodes[0xA7][4]=opA7m;
        opcodes[0xA8][1]=opcodes[0xA8][3]=opcodes[0xA8][4]=opA8x;
        opcodes[0xA9][2]=opcodes[0xA9][3]=opcodes[0xA9][4]=opA9m;
        opcodes[0xAA][1]=opcodes[0xAA][3]=opcodes[0xAA][4]=opAAx;
        opcodes[0xAC][1]=opcodes[0xAC][3]=opcodes[0xAC][4]=opACx;
        opcodes[0xAD][2]=opcodes[0xAD][3]=opcodes[0xAD][4]=opADm;
        opcodes[0xAE][1]=opcodes[0xAE][3]=opcodes[0xAE][4]=opAEx;
        opcodes[0xAF][2]=opcodes[0xAF][3]=opcodes[0xAF][4]=opAFm;
        opcodes[0xB1][2]=opcodes[0xB1][3]=opcodes[0xB1][4]=opB1m;
        opcodes[0xB2][2]=opcodes[0xB2][3]=opcodes[0xB2][4]=opB2m;
        opcodes[0xB3][2]=opcodes[0xB3][3]=opcodes[0xB3][4]=opB3m;
        opcodes[0xB4][1]=opcodes[0xB4][3]=opcodes[0xB4][4]=opB4x;
        opcodes[0xB5][2]=opcodes[0xB5][3]=opcodes[0xB5][4]=opB5m;
        opcodes[0xB6][1]=opcodes[0xB6][3]=opcodes[0xB6][4]=opB6x;
        opcodes[0xB7][2]=opcodes[0xB7][3]=opcodes[0xB7][4]=opB7m;
        opcodes[0xB9][2]=opcodes[0xB9][3]=opcodes[0xB9][4]=opB9m;
        opcodes[0xBB][1]=opcodes[0xBB][3]=opcodes[0xBB][4]=opBBx;
        opcodes[0xBC][1]=opcodes[0xBC][3]=opcodes[0xBC][4]=opBCx;
        opcodes[0xBD][2]=opcodes[0xBD][3]=opcodes[0xBD][4]=opBDm;
        opcodes[0xBE][1]=opcodes[0xBE][3]=opcodes[0xBE][4]=opBEx;
        opcodes[0xBF][2]=opcodes[0xBF][3]=opcodes[0xBF][4]=opBFm;
        opcodes[0xC0][1]=opcodes[0xC0][3]=opcodes[0xC0][4]=opC0x;
        opcodes[0xC3][2]=opcodes[0xC3][3]=opcodes[0xC3][4]=opC3m;
        opcodes[0xC4][1]=opcodes[0xC4][3]=opcodes[0xC4][4]=opC4x;
        opcodes[0xC5][2]=opcodes[0xC5][3]=opcodes[0xC5][4]=opC5m;
        opcodes[0xC6][2]=opcodes[0xC6][3]=opcodes[0xC6][4]=opC6m;
        opcodes[0xC7][2]=opcodes[0xC7][3]=opcodes[0xC7][4]=opC7m;
        opcodes[0xC8][1]=opcodes[0xC8][3]=opcodes[0xC8][4]=opC8x;
        opcodes[0xC9][2]=opcodes[0xC9][3]=opcodes[0xC9][4]=opC9m;
        opcodes[0xCA][1]=opcodes[0xCA][3]=opcodes[0xCA][4]=opCAx;
        opcodes[0xCC][1]=opcodes[0xCC][3]=opcodes[0xCC][4]=opCCx;
        opcodes[0xCD][2]=opcodes[0xCD][3]=opcodes[0xCD][4]=opCDm;
        opcodes[0xCE][2]=opcodes[0xCE][3]=opcodes[0xCE][4]=opCEm;
        opcodes[0xCF][2]=opcodes[0xCF][3]=opcodes[0xCF][4]=opCFm;
        opcodes[0xD1][2]=opcodes[0xD1][3]=opcodes[0xD1][4]=opD1m;
        opcodes[0xD2][2]=opcodes[0xD2][3]=opcodes[0xD2][4]=opD2m;
        opcodes[0xD5][2]=opcodes[0xD5][3]=opcodes[0xD5][4]=opD5m;
        opcodes[0xD6][2]=opcodes[0xD6][3]=opcodes[0xD6][4]=opD6m;
        opcodes[0xD7][2]=opcodes[0xD7][3]=opcodes[0xD7][4]=opD7m;
        opcodes[0xD9][2]=opcodes[0xD9][3]=opcodes[0xD9][4]=opD9m;
        opcodes[0xDA][1]=opcodes[0xDA][3]=opDAx;        
        opcodes[0xDD][2]=opcodes[0xDD][3]=opcodes[0xDD][4]=opDDm;
        opcodes[0xDE][2]=opcodes[0xDE][3]=opcodes[0xDE][4]=opDEm;
        opcodes[0xDF][2]=opcodes[0xDF][3]=opcodes[0xDF][4]=opDFm;
        opcodes[0xE0][1]=opcodes[0xE0][3]=opcodes[0xE0][4]=opE0x;
        opcodes[0xE3][2]=opcodes[0xE3][3]=opcodes[0xE3][4]=opE3m;
        opcodes[0xE4][1]=opcodes[0xE4][3]=opcodes[0xE4][4]=opE4x;
        opcodes[0xE5][2]=opcodes[0xE5][3]=opcodes[0xE5][4]=opE5m;
        opcodes[0xE6][2]=opcodes[0xE6][3]=opcodes[0xE6][4]=opE6m;
        opcodes[0xE7][2]=opcodes[0xE7][3]=opcodes[0xE7][4]=opE7m;
        opcodes[0xE8][1]=opcodes[0xE8][3]=opcodes[0xE8][4]=opE8x;
        opcodes[0xE9][2]=opcodes[0xE9][3]=opcodes[0xE9][4]=opE9m;
        opcodes[0xEC][1]=opcodes[0xEC][3]=opcodes[0xEC][4]=opECx;
        opcodes[0xED][2]=opcodes[0xED][3]=opcodes[0xED][4]=opEDm;
        opcodes[0xEE][2]=opcodes[0xEE][3]=opcodes[0xEE][4]=opEEm;
        opcodes[0xEF][2]=opcodes[0xEF][3]=opcodes[0xEF][4]=opEFm;
        opcodes[0xF1][2]=opcodes[0xF1][3]=opcodes[0xF1][4]=opF1m;
        opcodes[0xF2][2]=opcodes[0xF2][3]=opcodes[0xF2][4]=opF2m;
        opcodes[0xF5][2]=opcodes[0xF5][3]=opcodes[0xF5][4]=opF5m;
        opcodes[0xF6][2]=opcodes[0xF6][3]=opcodes[0xF6][4]=opF6m;
        opcodes[0xF7][2]=opcodes[0xF7][3]=opcodes[0xF7][4]=opF7m;
        opcodes[0xF9][2]=opcodes[0xF9][3]=opcodes[0xF9][4]=opF9m;
        opcodes[0xFA][1]=opcodes[0xFA][3]=opFAx;
        opcodes[0xFD][2]=opcodes[0xFD][3]=opcodes[0xFD][4]=opFDm;
        opcodes[0xFE][2]=opcodes[0xFE][3]=opcodes[0xFE][4]=opFEm;
        opcodes[0xFF][2]=opcodes[0xFF][3]=opcodes[0xFF][4]=opFFm;
        ram=(unsigned char *)malloc(0x20000);
        memset(ram,0,131072);
//        atexit(dumpram);
}

void int65816()
{
        if (struct_p.i)
        {
                if (wai) pc++;
                wai=0;
                return;
        }
//        printf("INT\n");
        if (wai) pc++;
        wai=0;
//        irqon=0;
        if (struct_p.e)
        {
                writemem(0,reg_S.w,pc>>8); reg_S.b.l--;
                writemem(0,reg_S.w,pc);    reg_S.b.l--;
                reg_S.b.h=1;
                temp=0x30; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                writemem(0,reg_S.w,temp);  reg_S.b.l--;
                pbr=0;
                dbr=0;
                pc=readmem(0,0xFFFE)|(readmem(0,0xFFFF)<<8);
                struct_p.i=1;
                cycles-=4;
        }
        else
        {
                writemem(0,reg_S.w,pbr);   reg_S.w--;
                writemem(0,reg_S.w,pc>>8); reg_S.w--;
                writemem(0,reg_S.w,pc);    reg_S.w--;
                temp=0; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                if (struct_p.x) temp|=0x10; if (struct_p.m) temp|=0x20;
                writemem(0,reg_S.w,temp);  reg_S.w--;                
                pbr=0;
                pc=readmem(0,0xFFEE)|(readmem(0,0xFFEF)<<8);
                struct_p.i=1;
                cycles-=5;
        }
}

void nmi65816()
{
        if (wai)
        {
                pc++;
                wai=0;
//                return;
        }
        if (struct_p.e)
        {
                writemem(0,reg_S.w,pc>>8); reg_S.b.l--;
                writemem(0,reg_S.w,pc);    reg_S.b.l--;
                reg_S.b.h=1;
                temp=0x30; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                writemem(0,reg_S.w,temp);  reg_S.b.l--;
                pbr=0;
                dbr=0;
                pc=readmem(0,0xFFFA)|(readmem(0,0xFFFB)<<8);
                struct_p.i=1;
                cycles-=4;
        }
        else
        {
                writemem(0,reg_S.w,pbr);   reg_S.w--;
                writemem(0,reg_S.w,pc>>8); reg_S.w--;
                writemem(0,reg_S.w,pc);    reg_S.w--;
                temp=0; if (struct_p.c) temp|=1; if (struct_p.z) temp|=2; if (struct_p.i) temp|=4;
                if (struct_p.d) temp|=8; if (struct_p.v) temp|=0x40; if (struct_p.n) temp|=0x80;
                if (struct_p.x) temp|=0x10; if (struct_p.m) temp|=0x20;
                writemem(0,reg_S.w,temp);  reg_S.w--;                
                pbr=0;
                pc=readmem(0,0xFFEA)|(readmem(0,0xFFEB)<<8);
                struct_p.i=1;
                cycles-=5;
        }
}

int nmipend=0;
