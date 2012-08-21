#include <stdio.h>
#include "snes.h"

int spcins;
void (*spcopcodes[256])();

struct
{
        union
        {
                unsigned short ya;
                struct
                {
                        unsigned char a,y;
                } b;
        } ya;
        unsigned char x;
        unsigned char s;
        unsigned short pc;
        struct
        {
                int c,z,h,p,v,n;
        } p;
        unsigned short opc[8];
} spc;

unsigned char spurom[64]=
{
        0xCD,0xEF,0xBD,0xE8,0x00,0xC6,0x1D,0xD0,
        0xFC,0x8F,0xAA,0xF4,0x8F,0xBB,0xF5,0x78,
        0xCC,0xF4,0xD0,0xFB,0x2F,0x19,0xEB,0xF4,
        0xD0,0xFC,0x7E,0xF4,0xD0,0x0B,0xE4,0xF5,
        0xCB,0xF4,0xD7,0x00,0xFC,0xD0,0xF3,0xAB,
        0x01,0x10,0xEF,0x7E,0xF4,0x10,0xEB,0xBA,
        0xF6,0xDA,0x00,0xBA,0xF4,0xC4,0xF4,0xDD,
        0x5D,0xD0,0xDB,0x1F,0x00,0x00,0xC0,0xFF
};

unsigned char spuram[0x10080];
unsigned char spuport[4],spuportin[4]={0,0,0,0};
unsigned char spuport2[4];
unsigned char spctctrl=0x80;
unsigned char spctimer[3],spclatch[3],spccount[3];
int spctimer2[3];

int decode[64];
unsigned short diraddr;

void write214x(unsigned short addr, unsigned char v)
{
//        printf("Write %04X %02X %02X:%04X\n",addr,v,pbr,pc);
        spuportin[addr&3]=v;
}

unsigned char read214x(unsigned short a)
{
        unsigned char temp=spuport[a&3];
        spuport[a&3]=spuport2[a&3];
        return temp;
}

unsigned char readspcl(unsigned short a)
{
        unsigned char temp;
        if (a<0xF0) return spuram[a];
        switch (a)
        {
                case 0xF0: return 0xFF;
                case 0xF1: return spctctrl;
                case 0xF2: case 0xF3: return readdsp(a);
                case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                return spuportin[a&3];
                case 0xFD: temp=spccount[0]&0xF; spccount[0]=0; return temp;
                case 0xFE: temp=spccount[1]&0xF; spccount[1]=0; return temp;
                case 0xFF: temp=spccount[2]&0xF; spccount[2]=0; return temp;
        }
        printf("Bad SPC read %04X\n",a);
        dumpspcregs();
        exit(-1);
}

void writespcl(unsigned short a, unsigned char v)
{
        if (a<0xF0)
        {
                spuram[a]=v;
                return;
        }
        switch (a)
        {
                case 0xF1:
//                printf("CTRL write %02X\n",v);
                if ((spctctrl&0x80)!=(v&0x80))
                {
                        memcpy(&spuram[0x10040],&spuram[0xFFC0],64);
                        memcpy(&spuram[0xFFC0],&spuram[0x10000],64);
                        memcpy(&spuram[0x10000],&spuram[0x10040],64);
                }
                spctctrl=v;
                if (v&0x10) spuportin[0]=spuportin[1]=0;
                if (v&0x20) spuportin[2]=spuportin[3]=0;
//                printf("SPC CTRL=%02X\n",v);
                return;
                case 0xF2: case 0xF3: writedsp(a,v); return;
                case 0xF4: case 0xF5: case 0xF6: case 0xF7:
                spuport2[a&3]=v;
                return;
                case 0xF0: case 0xF8: case 0xF9: return; /*Mortal Kombat writes to F0 and F9*/
                case 0xFA: spclatch[0]=v; return;
                case 0xFB: spclatch[1]=v; return;
                case 0xFC: spclatch[2]=v; return;
                case 0xFD: case 0xFE: case 0xFF: return; /*I don't think these
                                                           are writeable*/
        }
        if ((a&0xFF00)==0x100) { spuram[a]=v; return; }
        if ((a&0xFF00)==diraddr)
        {
                if (v!=spuram[a]) decode[(a&0xFC)>>2]=1;
                spuram[a]=v;
                return;
        }
        if ((a&0xFF00)==0xFF00)
        {
                if (!(spctctrl&0x80) || (a<0xFFC0))
                   spuram[a]=v;
                return;
        }
        printf("Bad SPC write %04X %02X\n",a,v);
        dumpspcregs();
        exit(-1);
}
int spuaccess[256];

void updatespuaccess(int page)
{
        memset(spuaccess,0xFF,sizeof(spuaccess));
        spuaccess[0]=0;
        spuaccess[page]=0;
}

void dumpspcregs()
{
        FILE *f=fopen("spc.dmp","wb");
        fwrite(spuram,0x10000,1,f);
        fclose(f);
        printf("SPC700 :\n");
        printf("A=%02X Y=%02X X=%02X S=%02X PC=%04X\n",spc.ya.b.a,spc.ya.b.y,spc.x,spc.s,spc.pc);
        printf("%c%c%c%c%c%c\n",(spc.p.c)?'C':' ',
                                (spc.p.z)?'Z':' ',
                                (spc.p.h)?'H':' ',
                                (spc.p.p)?'P':' ',
                                (spc.p.v)?'V':' ',
                                (spc.p.n)?'N':' ');
        printf("In= %02X %02X %02X %02X\n",spuportin[0],spuportin[1],spuportin[2],spuportin[3]);
        printf("Out=%02X %02X %02X %02X\n",spuport[0],spuport[1],spuport[2],spuport[3]);
        printf("%i ins  %04X %04X %04X %04X %04X %04X %04X %04X\n",spcins,spc.opc[0],spc.opc[1],spc.opc[2],spc.opc[3],spc.opc[4],spc.opc[5],spc.opc[6],spc.opc[7]);
}

void resetspc()
{
        memcpy(&spuram[0xFFC0],spurom,64);
        spc.pc=0xFFC0;
        spc.p.p=0;
        memset(spuaccess,0xFF,sizeof(spuaccess));
        spuaccess[0]=0;
        spuaccess[255]=0;
        makespctables();
}

unsigned short addr,addr2,tempw;
unsigned char temp,temp2,opcode;
uint32 templ;
signed char tmps;
int cycs;

#define readspc(a) (((a)&0xFF00)?spuram[(a)]:readspcl(a))
#define writespc(a,v) if (spuaccess[(a)>>8]) spuram[(a)]=v; else writespcl(a,v)

#define getdp()  addr=readspc(spc.pc)|spc.p.p; spc.pc++
#define getdpx() addr=((readspc(spc.pc)+spc.x)&0xFF)|spc.p.p; spc.pc++
#define getdpy() addr=((readspc(spc.pc)+spc.ya.b.y)&0xFF)|spc.p.p; spc.pc++
#define getabs()  addr=readspc(spc.pc)|(readspc(spc.pc+1)<<8); spc.pc+=2

#define setspczn(v)   spc.p.z=!(v); spc.p.n=(v)&0x80
#define setspczn16(v) spc.p.z=!(v); spc.p.n=(v)&0x8000

#define ADC(ac) tempw=ac+temp+((spc.p.c)?1:0);                  \
                spc.p.v=(!((ac^temp)&0x80)&&((ac^tempw)&0x80)); \
                ac=tempw&0xFF;                                  \
                setspczn(ac);                                   \
                spc.p.c=tempw&0x100;

#define SBC(ac) tempw=ac-temp-((spc.p.c)?0:1);                 \
                spc.p.v=(((ac^temp)&0x80)&&((ac^tempw)&0x80)); \
                ac=tempw&0xFF;                                 \
                setspczn(ac);                                  \
                spc.p.c=tempw<=0xFF;

#define CMP(ac) spc.p.c=(ac>=temp);                            \
                spc.p.z=!(ac-temp);                            \
                spc.p.n=(ac-temp)&0x80

/*NOP*/ void sop00() { cycs=2; }

/*SET*/
void sop02() { getdp(); temp=readspc(addr)|0x01; writespc(addr,temp); cycs=4; }
void sop22() { getdp(); temp=readspc(addr)|0x02; writespc(addr,temp); cycs=4; }
void sop42() { getdp(); temp=readspc(addr)|0x04; writespc(addr,temp); cycs=4; }
void sop62() { getdp(); temp=readspc(addr)|0x08; writespc(addr,temp); cycs=4; }
void sop82() { getdp(); temp=readspc(addr)|0x10; writespc(addr,temp); cycs=4; }
void sopa2() { getdp(); temp=readspc(addr)|0x20; writespc(addr,temp); cycs=4; }
void sopc2() { getdp(); temp=readspc(addr)|0x40; writespc(addr,temp); cycs=4; }
void sope2() { getdp(); temp=readspc(addr)|0x80; writespc(addr,temp); cycs=4; }

/*CLR*/
void sop12() { getdp(); temp=readspc(addr)&~0x01; writespc(addr,temp); cycs=4; }
void sop32() { getdp(); temp=readspc(addr)&~0x02; writespc(addr,temp); cycs=4; }
void sop52() { getdp(); temp=readspc(addr)&~0x04; writespc(addr,temp); cycs=4; }
void sop72() { getdp(); temp=readspc(addr)&~0x08; writespc(addr,temp); cycs=4; }
void sop92() { getdp(); temp=readspc(addr)&~0x10; writespc(addr,temp); cycs=4; }
void sopb2() { getdp(); temp=readspc(addr)&~0x20; writespc(addr,temp); cycs=4; }
void sopd2() { getdp(); temp=readspc(addr)&~0x40; writespc(addr,temp); cycs=4; }
void sopf2() { getdp(); temp=readspc(addr)&~0x80; writespc(addr,temp); cycs=4; }

/*BEQ*/ void sopf0() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (spc.p.z) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BNE*/ void sopd0() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (!spc.p.z) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BCS*/ void sopb0() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (spc.p.c) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BCC*/ void sop90() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (!spc.p.c) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BRA*/ void sop2f() { tmps=(signed char)readspc(spc.pc); spc.pc++; spc.pc+=tmps; cycs=4; }
/*BVS*/ void sop70() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (spc.p.v) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BVC*/ void sop50() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (!spc.p.v) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BMI*/ void sop30() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (spc.p.n) { spc.pc+=tmps; cycs=4; } else cycs=2; }
/*BPL*/ void sop10() { tmps=(signed char)readspc(spc.pc); spc.pc++; if (!spc.p.n) { spc.pc+=tmps; cycs=4; } else cycs=2; }

/*MOV (X),A*/   void sopc6() { addr=spc.x|spc.p.p; writespc(addr,spc.ya.b.a); cycs=4; }
/*MOV (X)+,A*/  void sopaf() { addr=spc.x|spc.p.p; writespc(addr,spc.ya.b.a); spc.x++; cycs=4; }
/*MOV dp,A*/    void sopc4() { getdp(); writespc(addr,spc.ya.b.a); cycs=4; }
/*MOV dpx,A*/   void sopd4() { getdpx(); writespc(addr,spc.ya.b.a); cycs=5; }
/*MOV abs,A*/   void sopc5() { getabs(); writespc(addr,spc.ya.b.a); cycs=5; }
/*MOV absx,A*/  void sopd5() { getabs(); addr+=spc.x; writespc(addr,spc.ya.b.a); cycs=5; }
/*MOV absy,A*/  void sopd6() { getabs(); addr+=spc.ya.b.y; writespc(addr,spc.ya.b.a); cycs=5; }
/*MOV (dpx),A*/ void sopc7() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); writespc(addr2,spc.ya.b.a); cycs=7; }
/*MOV (dp)y,A*/ void sopd7() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; writespc(addr2,spc.ya.b.a); cycs=7; }

/*MOV dp,X*/    void sopd8() { getdp(); writespc(addr,spc.x); cycs=4; }
/*MOV dpy,X*/   void sopd9() { getdpy(); writespc(addr,spc.x); cycs=5; }
/*MOV abs,X*/   void sopc9() { getabs(); writespc(addr,spc.x); cycs=5; }

/*MOV dp,Y*/    void sopcb() { getdp(); writespc(addr,spc.ya.b.y); cycs=4; }
/*MOV dpx,Y*/   void sopdb() { getdpx(); writespc(addr,spc.ya.b.y); cycs=5; }
/*MOV abs,Y*/   void sopcc() { getabs(); writespc(addr,spc.ya.b.y); cycs=5; }

/*MOV A,#*/     void sope8() { getdp(); spc.ya.b.a=addr&0xFF; setspczn(spc.ya.b.a); cycs=2; }
/*MOV A,(X)*/   void sope6() { addr=spc.x|spc.p.p; spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=3; }
/*MOV A,(X)+*/  void sopbf() { addr=spc.x|spc.p.p; spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); spc.x++; cycs=4; }
/*MOV A,dp*/    void sope4() { getdp(); spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=3; }
/*MOV A,dpx*/   void sopf4() { getdpx(); spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=4; }
/*MOV A,abs*/   void sope5() { getabs(); spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=4; }
/*MOV A,absx*/  void sopf5() { getabs(); addr+=spc.x; spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=5; }
/*MOV A,absy*/  void sopf6() { getabs(); addr+=spc.ya.b.y; spc.ya.b.a=readspc(addr); setspczn(spc.ya.b.a); cycs=5; }
/*MOV A,(dpx)*/ void sope7() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); spc.ya.b.a=readspc(addr2); setspczn(spc.ya.b.a); cycs=6; }
/*MOV A,(dp)y*/ void sopf7() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; spc.ya.b.a=readspc(addr2); setspczn(spc.ya.b.a); cycs=6; }

/*MOV dp,dp*/   void sopfa() { getdp(); tempw=addr; getdp(); temp=readspc(tempw); writespc(addr,temp); setspczn(temp); cycs=5; }

/*MOV X,#*/     void sopcd() { getdp(); spc.x=addr&0xFF; setspczn(spc.x); cycs=2; }
/*MOV X,dp*/    void sopf8() { getdp(); spc.x=readspc(addr); setspczn(spc.x); cycs=3; }
/*MOV X,dpx*/   void sopf9() { getdpx(); spc.x=readspc(addr); setspczn(spc.x); cycs=4; }
/*MOV X,abs*/   void sope9() { getabs(); spc.x=readspc(addr); setspczn(spc.x); cycs=4; }

/*MOV Y,#*/     void sop8d() { getdp(); spc.ya.b.y=addr&0xFF; setspczn(spc.ya.b.y); cycs=2; }
/*MOV Y,dp*/    void sopeb() { getdp(); spc.ya.b.y=readspc(addr); setspczn(spc.ya.b.y); cycs=3; }
/*MOV Y,dpx*/   void sopfb() { getdpx(); spc.ya.b.y=readspc(addr); setspczn(spc.ya.b.y); cycs=4; }
/*MOV Y,abs*/   void sopec() { getabs(); spc.ya.b.y=readspc(addr); setspczn(spc.ya.b.y); cycs=4; }

/*MOV A,X*/     void sop7d() { spc.ya.b.a=spc.x;      setspczn(spc.ya.b.a); cycs=2; }
/*MOV A,Y*/     void sopdd() { spc.ya.b.a=spc.ya.b.y; setspczn(spc.ya.b.a); cycs=2; }
/*MOV X,A*/     void sop5d() { spc.x     =spc.ya.b.a; setspczn(spc.x);      cycs=2; }
/*MOV Y,A*/     void sopfd() { spc.ya.b.y=spc.ya.b.a; setspczn(spc.ya.b.y); cycs=2; }
/*MOV X,SP*/    void sop9d() { spc.x=spc.s; setspczn(spc.x); cycs=2; }
/*MOV SP,X*/    void sopbd() { spc.s=spc.x; cycs=2; }

/*ADC #*/       void sop88() { getdp(); temp=addr&0xFF; ADC(spc.ya.b.a); cycs=2; }
/*ADC (X)*/     void sop86() { addr=spc.x|spc.p.p; temp=readspc(addr); ADC(spc.ya.b.a); cycs=3; }
/*ADC dp*/      void sop84() { getdp(); temp=readspc(addr); ADC(spc.ya.b.a); cycs=3; }
/*ADC dpx*/     void sop94() { getdpx(); temp=readspc(addr); ADC(spc.ya.b.a); cycs=4; }
/*ADC abs*/     void sop85() { getabs(); temp=readspc(addr); ADC(spc.ya.b.a); cycs=4; }
/*ADC absx*/    void sop95() { getabs(); addr+=spc.x; temp=readspc(addr); ADC(spc.ya.b.a); cycs=5; }
/*ADC absy*/    void sop96() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); ADC(spc.ya.b.a); cycs=5; }
/*ADC (dpx)*/   void sop87() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); ADC(spc.ya.b.a); cycs=6; }
/*ADC (dp)y*/   void sop97() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); ADC(spc.ya.b.a); cycs=6; }
/*ADC dp,#*/    void sop98() { getdp(); temp=addr&0xFF; getdp(); temp2=readspc(addr); ADC(temp2); writespc(addr,temp2); cycs=5; }
/*ADC dp,dp*/   void sop89() { getdp(); temp=readspc(addr); getdp(); temp2=readspc(addr); ADC(temp2); writespc(addr,temp2); cycs=5; }

/*SBC #*/       void sopa8() { getdp(); temp=addr&0xFF; SBC(spc.ya.b.a); cycs=2; }
/*SBC (X)*/     void sopa6() { addr=spc.x|spc.p.p; temp=readspc(addr); SBC(spc.ya.b.a); cycs=3; }
/*SBC dp*/      void sopa4() { getdp(); temp=readspc(addr); SBC(spc.ya.b.a); cycs=3; }
/*SBC dpx*/     void sopb4() { getdpx(); temp=readspc(addr); SBC(spc.ya.b.a); cycs=4; }
/*SBC abs*/     void sopa5() { getabs(); temp=readspc(addr); SBC(spc.ya.b.a); cycs=4; }
/*SBC absx*/    void sopb5() { getabs(); addr+=spc.x; temp=readspc(addr); SBC(spc.ya.b.a); cycs=5; }
/*SBC absy*/    void sopb6() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); SBC(spc.ya.b.a); cycs=5; }
/*SBC (dpx)*/   void sopa7() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); SBC(spc.ya.b.a); cycs=6; }
/*SBC (dp)y*/   void sopb7() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); SBC(spc.ya.b.a); cycs=6; }
/*SBC dp,#*/    void sopb8() { getdp(); temp=addr&0xFF; getdp(); temp2=readspc(addr); SBC(temp2); writespc(addr,temp2); cycs=5; }

/*CMP A,#*/     void sop68() { getdp(); temp=addr&0xFF; CMP(spc.ya.b.a); cycs=2; }
/*CMP A,(X)*/   void sop66() { addr=spc.x|spc.p.p; temp=readspc(addr); CMP(spc.ya.b.a); cycs=3; }
/*CMP A,dp*/    void sop64() { getdp(); temp=readspc(addr); CMP(spc.ya.b.a); cycs=3; }
/*CMP A,dpx*/   void sop74() { getdpx(); temp=readspc(addr); CMP(spc.ya.b.a); cycs=4; }
/*CMP A,abs*/   void sop65() { getabs(); temp=readspc(addr); CMP(spc.ya.b.a); cycs=4; }
/*CMP A,absx*/  void sop75() { getabs(); addr+=spc.x; temp=readspc(addr); CMP(spc.ya.b.a); cycs=5; }
/*CMP A,absy*/  void sop76() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); CMP(spc.ya.b.a); cycs=5; }
/*CMP A,(dpx)*/ void sop67() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); CMP(spc.ya.b.a); cycs=6; }
/*CMP A,(dp)y*/ void sop77() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); CMP(spc.ya.b.a); cycs=6; }

/*CMP X,#*/     void sopc8() { getdp(); temp=addr&0xFF; CMP(spc.x); cycs=2; }
/*CMP X,dp*/    void sop3e() { getdp(); temp=readspc(addr); CMP(spc.x); cycs=3; }
/*CMP X,abs*/   void sop1e() { getabs(); temp=readspc(addr); CMP(spc.x); cycs=4; }

/*CMP Y,#*/     void sopad() { getdp(); temp=addr&0xFF; CMP(spc.ya.b.y); cycs=2; }
/*CMP Y,dp*/    void sop7e() { getdp(); temp=readspc(addr); CMP(spc.ya.b.y); cycs=3; }
/*CMP Y,abs*/   void sop5e() { getabs(); temp=readspc(addr); CMP(spc.ya.b.y); cycs=4; }

/*CMP dp,dp*/   void sop69() { getdp(); tempw=addr; getdp(); temp2=readspc(tempw); temp=readspc(addr); CMP(temp2); cycs=6; }

/*AND #*/       void sop28() { getdp(); temp=addr&0xFF; spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=2; }
/*AND (X)*/     void sop26() { addr=spc.x|spc.p.p; temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=3; }
/*AND dp*/      void sop24() { getdp(); temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=3; }
/*AND dpx*/     void sop34() { getdpx(); temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=4; }
/*AND abs*/     void sop25() { getabs(); temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=4; }
/*AND absx*/    void sop35() { getabs(); addr+=spc.x; temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=5; }
/*AND absy*/    void sop36() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=5; }
/*AND (dpx)*/   void sop27() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=6; }
/*AND (dp)y*/   void sop37() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); spc.ya.b.a&=temp; setspczn(spc.ya.b.a); cycs=6; }
/*AND dp,#*/    void sop38() { getdp(); temp=addr&0xFF; getdp(); temp2=readspc(addr); temp&=temp2; setspczn(temp); writespc(addr,temp); cycs=5; }
/*AND dp,dp*/   void sop29() { getdp(); tempw=addr; getdp(); temp=readspc(tempw)&readspc(addr); writespc(addr,temp); setspczn(temp); cycs=6; }

/*OR #*/        void sop08() { getdp(); temp=addr&0xFF; spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=2; }
/*OR (X)*/      void sop06() { addr=spc.x|spc.p.p; temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=3; }
/*OR dp*/       void sop04() { getdp(); temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=3; }
/*OR dpx*/      void sop14() { getdpx(); temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=4; }
/*OR abs*/      void sop05() { getabs(); temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=4; }
/*OR absx*/     void sop15() { getabs(); addr+=spc.x; temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=5; }
/*OR absy*/     void sop16() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=5; }
/*OR (dpx)*/    void sop07() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=6; }
/*OR (dp)y*/    void sop17() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); spc.ya.b.a|=temp; setspczn(spc.ya.b.a); cycs=6; }
/*OR dp,dp*/    void sop09() { getdp(); tempw=addr; getdp(); temp=readspc(tempw)|readspc(addr); writespc(addr,temp); setspczn(temp); cycs=6; }
/*OR dp,#*/     void sop18() { getdp(); temp=addr&0xFF; getdp(); temp2=readspc(addr); temp|=temp2; setspczn(temp); writespc(addr,temp); cycs=5; }

/*EOR #*/       void sop48() { getdp(); temp=addr&0xFF; spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=2; }
/*EOR (X)*/     void sop46() { addr=spc.x|spc.p.p; temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=3; }
/*EOR dp*/      void sop44() { getdp(); temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=3; }
/*EOR dpx*/     void sop54() { getdpx(); temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=4; }
/*EOR abs*/     void sop45() { getabs(); temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=4; }
/*EOR absx*/    void sop55() { getabs(); addr+=spc.x; temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=5; }
/*EOR absy*/    void sop56() { getabs(); addr+=spc.ya.b.y; temp=readspc(addr); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=5; }
/*EOR (dpx)*/   void sop47() { getdpx(); addr2=readspc(addr)|(readspc(addr+1)<<8); temp=readspc(addr2); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=6; }
/*EOR (dp)y*/   void sop57() { getdp(); addr2=(readspc(addr)|(readspc(addr+1)<<8))+spc.ya.b.y; temp=readspc(addr2); spc.ya.b.a^=temp; setspczn(spc.ya.b.a); cycs=6; }
/*EOR dp,dp*/   void sop49() { getdp(); tempw=addr; getdp(); temp=readspc(tempw)^readspc(addr); writespc(addr,temp); setspczn(temp); cycs=6; }
/*EOR dp,#*/    void sop58() { getdp(); temp=addr&0xFF; getdp(); temp2=readspc(addr); temp^=temp2; setspczn(temp); writespc(addr,temp); cycs=5; }

/*CLRC*/        void sop60() { spc.p.c=0; cycs=2; }
/*SETC*/        void sop80() { spc.p.c=1; cycs=2; }
/*NOTC*/        void soped() { spc.p.c=!spc.p.c; cycs=2; }
/*CLRP*/        void sop20() { spc.p.p=0; cycs=2; }
/*SETP*/        void sop40() { spc.p.p=0x100; cycs=2; }

/*DEC X*/       void sop1d() { spc.x--; setspczn(spc.x); cycs=2; }
/*DEC Y*/       void sopdc() { spc.ya.b.y--; setspczn(spc.ya.b.y); cycs=2; }
/*DEC A*/       void sop9c() { spc.ya.b.a--; setspczn(spc.ya.b.a); cycs=2; }

/*INC X*/       void sop3d() { spc.x++; setspczn(spc.x); cycs=2; }
/*INC Y*/       void sopfc() { spc.ya.b.y++; setspczn(spc.ya.b.y); cycs=2; }
/*INC A*/       void sopbc() { spc.ya.b.a++; setspczn(spc.ya.b.a); cycs=2; }

/*MOV dp,#*/    void sop8f() { temp=readspc(spc.pc); spc.pc++; getdp(); writespc(addr,temp); cycs=5; }

/*CMP dp,#*/    void sop78() { temp=readspc(spc.pc); spc.pc++; getdp(); temp2=readspc(addr); CMP(temp2); cycs=5; }

/*MOVW YA,dp*/  void sopba() { getdp(); spc.ya.b.a=readspc(addr); spc.ya.b.y=readspc(addr+1); setspczn16(spc.ya.ya); cycs=5; }
/*MOVW dp,YA*/  void sopda() { getdp(); writespc(addr,spc.ya.b.a); writespc(addr+1,spc.ya.b.y); cycs=4; }

/*INC dp*/      void sopab() { getdp();  temp=readspc(addr)+1; writespc(addr,temp); setspczn(temp); cycs=4; }
/*INC dpx*/     void sopbb() { getdpx(); temp=readspc(addr)+1; writespc(addr,temp); setspczn(temp); cycs=5; }
/*INC abs*/     void sopac() { getabs(); temp=readspc(addr)+1; writespc(addr,temp); setspczn(temp); cycs=5; }

/*DEC dp*/      void sop8b() { getdp();  temp=readspc(addr)-1; writespc(addr,temp); setspczn(temp); cycs=4; }
/*DEC dpx*/     void sop9b() { getdpx(); temp=readspc(addr)-1; writespc(addr,temp); setspczn(temp); cycs=5; }
/*DEC abs*/     void sop8c() { getabs(); temp=readspc(addr)-1; writespc(addr,temp); setspczn(temp); cycs=5; }

/*JMP abs*/     void sop5f() { getabs(); spc.pc=addr; cycs=3; }
/*JMP [,x]*/    void sop1f() { getabs(); addr+=spc.x; spc.pc=readspc(addr)|(readspc(addr+1)<<8); cycs=6; }

/*CALL*/        void sop3f() { getabs(); writespc(0x100|spc.s,spc.pc>>8); spc.s--; writespc(0x100|spc.s,spc.pc&0xFF); spc.s--; spc.pc=addr; cycs=8; }
/*PCALL*/       void sop4f() { addr=readspc(spc.pc)|0xFF00; spc.pc++; writespc(0x100|spc.s,spc.pc>>8); spc.s--; writespc(0x100|spc.s,spc.pc&0xFF); spc.s--; spc.pc=addr; cycs=6; }
/*RET*/         void sop6f() { spc.s++; addr=readspc(0x100|spc.s); spc.s++; addr|=readspc(0x100|spc.s)<<8; spc.pc=addr; cycs=5; }

/*PUSH A*/      void sop2d() { spuram[0x100|spc.s]=spc.ya.b.a; spc.s--; cycs=4; }
/*PUSH X*/      void sop4d() { spuram[0x100|spc.s]=spc.x; spc.s--; cycs=4; }
/*PUSH Y*/      void sop6d() { spuram[0x100|spc.s]=spc.ya.b.y; spc.s--; cycs=4; }

/*POP A*/       void sopae() { spc.s++; spc.ya.b.a=spuram[0x100|spc.s]; cycs=4; }
/*POP X*/       void sopce() { spc.s++; spc.x=spuram[0x100|spc.s]; cycs=4; }
/*POP Y*/       void sopee() { spc.s++; spc.ya.b.y=spuram[0x100|spc.s]; cycs=4; }

/*MUL YA*/      void sopcf() { spc.ya.ya=spc.ya.b.a*spc.ya.b.y; setspczn16(spc.ya.ya); cycs=9; }
/*DIV YA,X*/    void sop9e() { if (!spc.x) { spc.p.v=1; spc.ya.ya=0xFFFF; } else { spc.p.v=0; temp=spc.ya.ya/spc.x; spc.ya.b.y=spc.ya.ya%spc.x; spc.ya.b.a=temp; setspczn(spc.ya.b.a); } cycs=12; }

/*CBNE dp*/     void sop2e() { getdp();  tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (spc.ya.b.a!=temp) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*CBNE dpx*/    void sopde() { getdpx(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (spc.ya.b.a!=temp) { spc.pc+=tmps; cycs=8; } else cycs=6; }
/*DBNZ dp*/     void sop6e() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); temp--; writespc(addr,temp); if (temp) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*DBNZ Y*/      void sopfe() { tmps=(signed char)readspc(spc.pc); spc.pc++; spc.ya.b.y--; if (spc.ya.b.y) { spc.pc+=tmps; cycs=6; } else cycs=4; }

/*ASL A*/       void sop1c() { spc.p.c=spc.ya.b.a&0x80; spc.ya.b.a<<=1; setspczn(spc.ya.b.a); cycs=2; }
/*ASL dp*/      void sop0b() { getdp();  temp=readspc(addr); spc.p.c=temp&0x80; temp<<=1; setspczn(temp); writespc(addr,temp); cycs=4; }
/*ASL dpx*/     void sop1b() { getdpx(); temp=readspc(addr); spc.p.c=temp&0x80; temp<<=1; setspczn(temp); writespc(addr,temp); cycs=5; }
/*ASL abs*/     void sop0c() { getabs(); temp=readspc(addr); spc.p.c=temp&0x80; temp<<=1; setspczn(temp); writespc(addr,temp); cycs=5; }

/*LSR A*/       void sop5c() { spc.p.c=spc.ya.b.a&1; spc.ya.b.a>>=1; setspczn(spc.ya.b.a); cycs=2; }
/*LSR dp*/      void sop4b() { getdp();  temp=readspc(addr); spc.p.c=temp&1; temp>>=1; setspczn(temp); writespc(addr,temp); cycs=4; }
/*LSR dpx*/     void sop5b() { getdpx(); temp=readspc(addr); spc.p.c=temp&1; temp>>=1; setspczn(temp); writespc(addr,temp); cycs=5; }
/*LSR abs*/     void sop4c() { getabs(); temp=readspc(addr); spc.p.c=temp&1; temp>>=1; setspczn(temp); writespc(addr,temp); cycs=5; }

/*ROL A*/       void sop3c() { templ=spc.p.c; spc.p.c=spc.ya.b.a&0x80; spc.ya.b.a<<=1; if (templ) spc.ya.b.a|=1; setspczn(spc.ya.b.a); cycs=2; }
/*ROL dp*/      void sop2b() { getdp();  temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&0x80; temp<<=1; if (templ) temp|=1; setspczn(temp); writespc(addr,temp); cycs=4; }
/*ROL dpx*/     void sop3b() { getdpx(); temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&0x80; temp<<=1; if (templ) temp|=1; setspczn(temp); writespc(addr,temp); cycs=5; }
/*ROL abs*/     void sop2c() { getabs(); temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&0x80; temp<<=1; if (templ) temp|=1; setspczn(temp); writespc(addr,temp); cycs=5; }

/*ROR A*/       void sop7c() { templ=spc.p.c; spc.p.c=spc.ya.b.a&1;    spc.ya.b.a>>=1; if (templ) spc.ya.b.a|=0x80; setspczn(spc.ya.b.a); cycs=2; }
/*ROR dp*/      void sop6b() { getdp();  temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&1; temp>>=1; if (templ) temp|=0x80; setspczn(temp); writespc(addr,temp); cycs=4; }
/*ROR dpx*/     void sop7b() { getdpx(); temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&1; temp>>=1; if (templ) temp|=0x80; setspczn(temp); writespc(addr,temp); cycs=5; }
/*ROR abs*/     void sop6c() { getabs(); temp=readspc(addr); templ=spc.p.c; spc.p.c=temp&1; temp>>=1; if (templ) temp|=0x80; setspczn(temp); writespc(addr,temp); cycs=5; }

/*BBC 0*/       void sop13() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x01)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 1*/       void sop33() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x02)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 2*/       void sop53() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x04)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 3*/       void sop73() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x08)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 4*/       void sop93() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x10)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 5*/       void sopb3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x20)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 6*/       void sopd3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x40)) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBC 7*/       void sopf3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (!(temp&0x80)) { spc.pc+=tmps; cycs=7; } else cycs=5; }

/*BBS 0*/       void sop03() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x01) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 1*/       void sop23() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x02) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 2*/       void sop43() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x04) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 3*/       void sop63() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x08) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 4*/       void sop83() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x10) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 5*/       void sopa3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x20) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 6*/       void sopc3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x40) { spc.pc+=tmps; cycs=7; } else cycs=5; }
/*BBS 7*/       void sope3() { getdp(); tmps=(signed char)readspc(spc.pc); spc.pc++; temp=readspc(addr); if (temp&0x80) { spc.pc+=tmps; cycs=7; } else cycs=5; }

/*ADDW YA,dp*/  void sop7a() { getdp(); tempw=readspc(addr); tempw|=readspc(addr+1)<<8; templ=spc.ya.ya+tempw; spc.p.v=(!((spc.ya.ya^tempw)&0x8000)&&((spc.ya.ya^templ)&0x8000)); spc.ya.ya=templ&0xFFFF; setspczn16(spc.ya.ya); spc.p.c=templ&0x10000; cycs=5; }
/*SUBW YA,dp*/  void sop9a() { getdp(); tempw=readspc(addr); tempw|=readspc(addr+1)<<8; templ=spc.ya.ya-tempw; spc.p.v=(((spc.ya.ya^tempw)&0x8000)&&((spc.ya.ya^templ)&0x8000));  spc.ya.ya=templ&0xFFFF; spc.p.c=(spc.ya.ya>=tempw); setspczn16(spc.ya.ya); cycs=5; }
/*CMPW YA,dp*/  void sop5a() { getdp(); tempw=readspc(addr); tempw|=readspc(addr+1)<<8; spc.p.c=(spc.ya.ya>=tempw); setspczn16(spc.ya.ya-tempw); cycs=4; }

/*DECW dp*/     void sop1a() { getdp(); tempw=readspc(addr); tempw|=readspc(addr+1)<<8; tempw--; writespc(addr,tempw); writespc(addr+1,tempw>>8); setspczn16(tempw); cycs=6; }
/*INCW dp*/     void sop3a() { getdp(); tempw=readspc(addr); tempw|=readspc(addr+1)<<8; tempw++; writespc(addr,tempw); writespc(addr+1,tempw>>8); setspczn16(tempw); cycs=6; }

/*XCN A*/       void sop9f() { spc.ya.b.a=(spc.ya.b.a>>4)|(spc.ya.b.a<<4); setspczn(spc.ya.b.a); cycs=2; }

/*TCLR1*/       void sop4e() { getabs(); temp=readspc(addr); setspczn(spc.ya.b.a&temp); temp&=~spc.ya.b.a; writespc(addr,temp); cycs=6; }
/*TSET1*/       void sop0e() { getabs(); temp=readspc(addr); setspczn(spc.ya.b.a&temp); temp|=spc.ya.b.a;  writespc(addr,temp); cycs=6; }

/*MOV1 C,mem.bit*/ void sopaa() { getabs(); tempw=addr>>13; addr&=0x1FFF; temp=readspc(addr); spc.p.c=temp&(1<<tempw); cycs=4; }
/*MOV1 mem.bit,C*/ void sopca() { getabs(); tempw=addr>>13; addr&=0x1FFF; temp=readspc(addr); if (spc.p.c) temp|=(1<<tempw); else temp&=~(1<<tempw); writespc(addr,temp); cycs=6; }
/*EOR1 C,mem.bit*/ void sop8a() { getabs(); tempw=addr>>13; addr&=0x1FFF; temp=readspc(addr); if (spc.p.c) spc.p.c=1; spc.p.c^=((temp&(1<<tempw))?1:0); cycs=4; }
/*NOT1 mem.bit*/   void sopea() { getabs(); tempw=addr>>13; addr&=0x1FFF; temp=readspc(addr); temp^=(1<<tempw); writespc(addr,temp); cycs=4; }

/*PUSH PSW*/     void sop0d() { temp=(spc.p.c)?1:0; temp|=(spc.p.z)?2:0;
                                temp|=(spc.p.h)?8:0; temp|=(spc.p.p)?32:0;
                                temp|=(spc.p.v)?64:0; temp|=(spc.p.n)?128:0;
                                spuram[0x100|spc.s]=temp; spc.s--; cycs=4; }
/*POP PSW*/      void sop8e() { spc.s++; temp=spuram[0x100|spc.s];
                                spc.p.c=temp&1; spc.p.z=temp&2;
                                spc.p.h=temp&8; spc.p.p=(temp&32)?0x100:0;
                                spc.p.v=temp&64; spc.p.n=temp&128; cycs=4; }

void sopbad() { printf("Illegal SPC opcode %02X\n",opcode); dumpspcregs(); exit(-1); }

void makespctables()
{
        int c;
        for (c=0;c<256;c++) spcopcodes[c]=sopbad;
        spcopcodes[0x02]=sop02;
        spcopcodes[0x22]=sop22;
        spcopcodes[0x42]=sop42;
        spcopcodes[0x62]=sop62;
        spcopcodes[0x82]=sop82;
        spcopcodes[0xa2]=sopa2;
        spcopcodes[0xc2]=sopc2;
        spcopcodes[0xe2]=sope2;
        spcopcodes[0x12]=sop12;
        spcopcodes[0x32]=sop32;
        spcopcodes[0x52]=sop52;
        spcopcodes[0x72]=sop72;
        spcopcodes[0x92]=sop92;
        spcopcodes[0xb2]=sopb2;
        spcopcodes[0xd2]=sopd2;
        spcopcodes[0xf2]=sopf2;
        spcopcodes[0xf0]=sopf0;
        spcopcodes[0xd0]=sopd0;
        spcopcodes[0xb0]=sopb0;
        spcopcodes[0x90]=sop90;
        spcopcodes[0x70]=sop70;
        spcopcodes[0x50]=sop50;
        spcopcodes[0x30]=sop30;
        spcopcodes[0x10]=sop10;
        spcopcodes[0xc6]=sopc6;
        spcopcodes[0xaf]=sopaf;
        spcopcodes[0xc4]=sopc4;
        spcopcodes[0xd4]=sopd4;
        spcopcodes[0xc5]=sopc5;
        spcopcodes[0xd5]=sopd5;
        spcopcodes[0xd6]=sopd6;
        spcopcodes[0xc7]=sopc7;
        spcopcodes[0xd7]=sopd7;
        spcopcodes[0xd8]=sopd8;
        spcopcodes[0xd9]=sopd9;
        spcopcodes[0xc9]=sopc9;
        spcopcodes[0xcb]=sopcb;
        spcopcodes[0xdb]=sopdb;
        spcopcodes[0xcc]=sopcc;

        spcopcodes[0xe8]=sope8;
        spcopcodes[0xe6]=sope6;
        spcopcodes[0xbf]=sopbf;
        spcopcodes[0xe4]=sope4;
        spcopcodes[0xf4]=sopf4;
        spcopcodes[0xe5]=sope5;
        spcopcodes[0xf5]=sopf5;
        spcopcodes[0xf6]=sopf6;
        spcopcodes[0xe7]=sope7;
        spcopcodes[0xf7]=sopf7;

        spcopcodes[0xcd]=sopcd;
        spcopcodes[0xf8]=sopf8;
        spcopcodes[0xf9]=sopf9;
        spcopcodes[0xe9]=sope9;

        spcopcodes[0x8d]=sop8d;
        spcopcodes[0xeb]=sopeb;
        spcopcodes[0xfb]=sopfb;
        spcopcodes[0xec]=sopec;

        spcopcodes[0x7d]=sop7d;
        spcopcodes[0xdd]=sopdd;
        spcopcodes[0x5d]=sop5d;
        spcopcodes[0xfd]=sopfd;
        spcopcodes[0x9d]=sop9d;
        spcopcodes[0xbd]=sopbd;

        spcopcodes[0x88]=sop88;
        spcopcodes[0x86]=sop86;
        spcopcodes[0x84]=sop84;
        spcopcodes[0x94]=sop94;
        spcopcodes[0x85]=sop85;
        spcopcodes[0x95]=sop95;
        spcopcodes[0x96]=sop96;
        spcopcodes[0x87]=sop87;
        spcopcodes[0x97]=sop97;
        spcopcodes[0x98]=sop98;
        spcopcodes[0x89]=sop89;

        spcopcodes[0xa8]=sopa8;
        spcopcodes[0xa6]=sopa6;
        spcopcodes[0xa4]=sopa4;
        spcopcodes[0xb4]=sopb4;
        spcopcodes[0xa5]=sopa5;
        spcopcodes[0xb5]=sopb5;
        spcopcodes[0xb6]=sopb6;
        spcopcodes[0xa7]=sopa7;
        spcopcodes[0xb7]=sopb7;
        spcopcodes[0xb8]=sopb8;

        spcopcodes[0x68]=sop68;
        spcopcodes[0x66]=sop66;
        spcopcodes[0x64]=sop64;
        spcopcodes[0x74]=sop74;
        spcopcodes[0x65]=sop65;
        spcopcodes[0x75]=sop75;
        spcopcodes[0x76]=sop76;
        spcopcodes[0x67]=sop67;
        spcopcodes[0x77]=sop77;

        spcopcodes[0x28]=sop28;
        spcopcodes[0x26]=sop26;
        spcopcodes[0x24]=sop24;
        spcopcodes[0x34]=sop34;
        spcopcodes[0x25]=sop25;
        spcopcodes[0x35]=sop35;
        spcopcodes[0x36]=sop36;
        spcopcodes[0x27]=sop27;
        spcopcodes[0x37]=sop37;
        spcopcodes[0x38]=sop38;
        spcopcodes[0x29]=sop29;

        spcopcodes[0x08]=sop08;
        spcopcodes[0x06]=sop06;
        spcopcodes[0x04]=sop04;
        spcopcodes[0x14]=sop14;
        spcopcodes[0x05]=sop05;
        spcopcodes[0x15]=sop15;
        spcopcodes[0x16]=sop16;
        spcopcodes[0x07]=sop07;
        spcopcodes[0x17]=sop17;
        spcopcodes[0x09]=sop09;

        spcopcodes[0x48]=sop48;
        spcopcodes[0x46]=sop46;
        spcopcodes[0x44]=sop44;
        spcopcodes[0x54]=sop54;
        spcopcodes[0x45]=sop45;
        spcopcodes[0x55]=sop55;
        spcopcodes[0x56]=sop56;
        spcopcodes[0x47]=sop47;
        spcopcodes[0x57]=sop57;
        spcopcodes[0x49]=sop49;
        spcopcodes[0x58]=sop58;

        spcopcodes[0x1d]=sop1d;
        spcopcodes[0x9c]=sop9c;
        spcopcodes[0xdc]=sopdc;

        spcopcodes[0x3d]=sop3d;
        spcopcodes[0xbc]=sopbc;
        spcopcodes[0xfc]=sopfc;

        spcopcodes[0x8f]=sop8f;
        spcopcodes[0x78]=sop78;
        spcopcodes[0x2f]=sop2f;

        spcopcodes[0xba]=sopba;
        spcopcodes[0xda]=sopda;

        spcopcodes[0xc8]=sopc8;
        spcopcodes[0x3e]=sop3e;
        spcopcodes[0x1e]=sop1e;

        spcopcodes[0xad]=sopad;
        spcopcodes[0x7e]=sop7e;
        spcopcodes[0x5e]=sop5e;

        spcopcodes[0xab]=sopab;
        spcopcodes[0xbb]=sopbb;
        spcopcodes[0xac]=sopac;

        spcopcodes[0x8b]=sop8b;
        spcopcodes[0x9b]=sop9b;
        spcopcodes[0x8c]=sop8c;

        spcopcodes[0x1f]=sop1f;
        spcopcodes[0x5f]=sop5f;

        spcopcodes[0x60]=sop60;
        spcopcodes[0x80]=sop80;
        spcopcodes[0xed]=soped;
        spcopcodes[0x20]=sop20;
        spcopcodes[0x40]=sop40;

        spcopcodes[0x3f]=sop3f;
        spcopcodes[0x6f]=sop6f;

        spcopcodes[0x2d]=sop2d;
        spcopcodes[0x4d]=sop4d;
        spcopcodes[0x6d]=sop6d;

        spcopcodes[0xae]=sopae;
        spcopcodes[0xce]=sopce;
        spcopcodes[0xee]=sopee;

        spcopcodes[0xcf]=sopcf;
        spcopcodes[0x9e]=sop9e;

        spcopcodes[0x2e]=sop2e;
        spcopcodes[0xde]=sopde;
        spcopcodes[0x6e]=sop6e;
        spcopcodes[0xfe]=sopfe;

        spcopcodes[0x1c]=sop1c;

        spcopcodes[0x5c]=sop5c;

        spcopcodes[0x03]=sop03;
        spcopcodes[0x23]=sop23;
        spcopcodes[0x43]=sop43;
        spcopcodes[0x63]=sop63;
        spcopcodes[0x83]=sop83;
        spcopcodes[0xa3]=sopa3;
        spcopcodes[0xc3]=sopc3;
        spcopcodes[0xe3]=sope3;

        spcopcodes[0x13]=sop13;
        spcopcodes[0x33]=sop33;
        spcopcodes[0x53]=sop53;
        spcopcodes[0x73]=sop73;
        spcopcodes[0x93]=sop93;
        spcopcodes[0xb3]=sopb3;
        spcopcodes[0xd3]=sopd3;
        spcopcodes[0xf3]=sopf3;

        spcopcodes[0x1a]=sop1a;
        spcopcodes[0x3a]=sop3a;
        spcopcodes[0x7a]=sop7a;
        spcopcodes[0x9a]=sop9a;
        spcopcodes[0x5a]=sop5a;

        spcopcodes[0x9f]=sop9f;

        spcopcodes[0x4b]=sop4b;
        spcopcodes[0x5b]=sop5b;
        spcopcodes[0x4c]=sop4c;

        spcopcodes[0x0b]=sop0b;
        spcopcodes[0x1b]=sop1b;
        spcopcodes[0x0c]=sop0c;

        spcopcodes[0x2b]=sop2b;
        spcopcodes[0x3b]=sop3b;
        spcopcodes[0x2c]=sop2c;

        spcopcodes[0x6b]=sop6b;
        spcopcodes[0x7b]=sop7b;
        spcopcodes[0x6c]=sop6c;

        spcopcodes[0x3c]=sop3c;
        spcopcodes[0x7c]=sop7c;

        spcopcodes[0x0e]=sop0e;
        spcopcodes[0x4e]=sop4e;

        spcopcodes[0xfa]=sopfa;

        spcopcodes[0x00]=sop00;
        spcopcodes[0xc0]=sop00;

        spcopcodes[0x69]=sop69;

        spcopcodes[0x8a]=sop8a;
        spcopcodes[0xaa]=sopaa;
        spcopcodes[0xca]=sopca;
        spcopcodes[0xea]=sopea;

        spcopcodes[0x18]=sop18;

        spcopcodes[0x0d]=sop0d;
        spcopcodes[0x8e]=sop8e;

        spcopcodes[0x4f]=sop4f;
}
