#include "snes.h"

int padready;
unsigned short oldspcpc;
int spcpchi=1;
unsigned char spuram[0x10080];
int cycs;
int soundupdate;
unsigned short sobjaddr;
int curline;
int curframe;
int spcemu;
unsigned char spctctrl;
unsigned char spctimer[3],spclatch[3],spccount[3];
int spctimer2[3];

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

unsigned char spuram[0x10080];

void (*opcodes[256][5])();
void (*spcopcodes[256])();

int slines;
int irqon,fastrom,nmipend,palf;
int cycles=0,spccycles=0;
unsigned char opcode;
int ins;
int gcycs=0,spccycs=0;

#define readspc(a) ((a&0xFF00)?spuram[a]:readspcl(a))

int cyclookup[3][10]=
{
        {0,8,16,24,32,40,48,56,64,72},
        {0,6,12,18,24,30,36,42,48,54},
        {0,10,20,30,40,50,60,70,80,90},
};

/*Main loop - switches between 65816,SPC,and PPU
  VERY loose timing here*/
void mainloop(int linenum)
{
        int lines;
        int ocycles;
        for (lines=0;lines<linenum;lines++)
        {
                /*Deal with IRQs*/
                curline=slines;
                gcycs+=1366; //best timing I could find
                spccycs+=1366;
                /*Execute two instructions before processing NMI - helps Chrono Trigger and Puzzle Bobble*/
                opcode=readmem(pbr,pc); pc++;
                opcodes[opcode][mode]();                
                opcode=readmem(pbr,pc); pc++;
                opcodes[opcode][mode]();
                opcode=readmem(pbr,pc); pc++;
                opcodes[opcode][mode]();
                opcode=readmem(pbr,pc); pc++;
                opcodes[opcode][mode]();
                ins+=2;
//                if (nmipend&&nmiena) { nmipend=0; nmi65816(); }
                while (gcycs>0)
                {
                        cycles=0;
                        opcode=readmem(pbr,pc); pc++;
                        opcodes[opcode][mode]();
                        gcycs-=cyclookup[fastrom][-cycles];
                        if (irqon && !struct_p.i) int65816();
                        if (spcemu)
                        {
                                while (spccycs>gcycs)
                                {
                                        opcode=readspc(spc.pc); spc.pc++;
                                        spcopcodes[opcode]();
                                        spccycs-=cyclookup[2][cycs];
                                }
                        }
                }
		UPDATE_SOUND;
                if (spcemu)
                {
                        if (spctctrl&1)
                        {
                                spctimer2[0]++;
                                if (spctimer2[0]==2)
                                {
                                        spctimer2[0]=0;
                                        spctimer[0]++;
                                        if (spctimer[0]==spclatch[0])
                                        {
                                                spctimer[0]=0;
                                                spccount[0]++;
                                        }
                                }
                        }
                        if (spctctrl&2)
                        {
                                spctimer2[1]++;
                                if (spctimer2[1]==2)
                                {
                                        spctimer2[1]=0;
                                        spctimer[1]++;
                                        if (spctimer[1]==spclatch[1])
                                        {
                                                spctimer[1]=0;
                                                spccount[1]++;
                                        }
                                }
                        }
                        if (spctctrl&4)
                        {
                                spctimer[2]+=4;
                                if (spctimer[2]>=spclatch[2])
                                {
                                        spctimer[2]=0;
                                        spccount[2]++;
                                }
                        }
                }
                if (slines==231)           nmiocc=nmipend=1;
                if (slines==231 && nmiena) nmi65816();
                if (slines==224)
                {
                        vbl=1;
                        vblankhdma();
                        curframe++;                        
			padready=1;
                }
                if (slines==227) padready=0;
                if (slines<224) doline(slines);
                if ((vertint && (slines==vertline)) || (horint && !vertint))
                   irqon=1;
                else
                   irqon=0;
                slines++;
                if (palf)
                {
                        if (slines==312)
                        {
                                slines=0;
                                objaddr=sobjaddr;
                                vbl=nmiocc=nmipend=0;
                                decodesprites();
                        }
                }
                else
                {
                        if (slines==262)
                        {
                                slines=0;
                                objaddr=sobjaddr;
                                vbl=nmiocc=nmipend=0;
                                decodesprites();
                        }
                }
        }
}
