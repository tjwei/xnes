#include "snes.h"
int slines;
int hblnk=0;
int reg4016;
unsigned char *ram;
int fastrom;
int irqon;
int padready=1;
unsigned char countena;

unsigned char mula,mulb,divb;
unsigned short mulr,divr,divc;
uint32 joy1=0x80000000,joy2;


unsigned char readio(unsigned short addr)
{
        unsigned char temp;
        if (addr==0x4017)
        {
//                printf("Old style read 2\n");
//                return 0;
                return 0xFF;
        }
        if (addr==0x4016)
        {
                return 0xFF;
        }
//        if (addr>0x42FF) return 0;
        switch (addr)
        {
                case 0x4200:
                return countena;
                case 0x420C:
//                return 0xFF;
                return hdmaena;
                case 0x4210: /*NMI Register*/
//                printf("4210 read %i line %i\n",nmiocc,curline);
                if (nmiocc)
                {
                        nmiocc=0;
                        return 0x80;
                }
                return 0;
                case 0x4211:
                if (irqon)
                {
                        irqon=0;
                        return 0x80;
                }
                return 0;
                case 0x4212: /*Status Register*/
                hblnk^=0x40;
//                printf("4212 read at %02X:%04X line %i %02X\n",pbr,pc,curline,((vbl)?0x80:0)|padready|hblnk);
                if (vbl) return 0x80|padready|hblnk;
                return padready|hblnk;
                case 0x4213: /*PIO port*/
                return 0xFF;
                case 0x4214: /*Division Result Low*/
                return divr;
                case 0x4215: /*Division Result High*/
                return divr>>8;
                case 0x4216: /*Multiplication Result Low*/
                return mulr;
                case 0x4217: /*Multiplication Result High*/
                return mulr>>8;
                case 0x4218: /*Joypad #1 low*/
//                printf("4218 read %02X at %02X:%04X\n",joy1&0xFF,pbr,pc);
                return native_joypad_state(0);
                case 0x4219: /*Joypad #1 high*/
                return native_joypad_state(0)>>8;
                return temp;
                case 0x421A: case 0x421B: /*All the other joypads*/
                case 0x421C: case 0x421D:
                case 0x421E: case 0x421F:
                return 0;
                
//                case 0x42FE: case 0x42FF: /*As read by Soccer Kid*/
//                return 0xFF;

                case 0x4300: case 0x4310: case 0x4320: case 0x4330:
                case 0x4340: case 0x4350: case 0x4360: case 0x4370:
                return dma.ctrl[(addr>>4)&7];
                case 0x4301: case 0x4311: case 0x4321: case 0x4331:
                case 0x4341: case 0x4351: case 0x4361: case 0x4371:
                return dma.dest[(addr>>4)&7];
                case 0x4302: case 0x4312: case 0x4322: case 0x4332:
                case 0x4342: case 0x4352: case 0x4362: case 0x4372:
                return dma.src[(addr>>4)&7]&0xFF;
                case 0x4303: case 0x4313: case 0x4323: case 0x4333:
                case 0x4343: case 0x4353: case 0x4363: case 0x4373:
                return dma.src[(addr>>4)&7]>>8;
                case 0x4304: case 0x4314: case 0x4324: case 0x4334:
                case 0x4344: case 0x4354: case 0x4364: case 0x4374:
                return dma.srcbank[(addr>>4)&7];
                case 0x4305: case 0x4315: case 0x4325: case 0x4335:
                case 0x4345: case 0x4355: case 0x4365: case 0x4375:
                return dma.size[(addr>>4)&7]&0xFF;
                case 0x4306: case 0x4316: case 0x4326: case 0x4336:
                case 0x4346: case 0x4356: case 0x4366: case 0x4376:
                return dma.size[(addr>>4)&7]&0xFF;
//                default:
//                printf("Bad I/O read %04X %02X:%04X %04X %02X\n",addr,pbr,pc,reg_Y.w,ram[0x287]);
        }
        return 0;
        printf("Bad I/O read %04X\n",addr);
        dumpregs();
        exit(-1);
}

void writeio(unsigned short addr, unsigned char val)
{
        int c;
        int dmawsize[8]={1,2,2,4,4,0,0,0};
        int dmadone=0;
        unsigned short a,a2,len;
        unsigned char temp,val2;
        switch (addr)
        {
                case 0x4016: reg4016=val&1; return;
                case 0x4200: /*Counter enable*/
                nmiena=val&0x80;
                countena=val;
                vertint=val&0x20;
                horint=val&0x10;
                irqon=0;
                return;
                case 0x4202: /*Multiplicand A*/
                mula=val;
                mulr=mula*mulb;
                return;
                case 0x4203: /*Multiplier B*/
                mulb=val;
                mulr=mula*mulb;
                return;
                case 0x4204: /*Dividend C Low*/
                divc&=0xFF00;
                divc|=val;
                if (divb)
                {
                        divr=divc/divb;
                        mulr=divc%divb;
                }
                else
                {
                        divr=0xFFFF;
                        mulr=divc;
                }
                return;
                case 0x4205: /*Dividend C High*/
                divc&=0xFF;
                divc|=(val<<8);
                if (divb)
                {
                        divr=divc/divb;
                        mulr=divc%divb;
                }
                else
                {
                        divr=0xFFFF;
                        mulr=divc;
                }
                return;
                case 0x4206: /*Divisor B*/
                divb=val;
                if (divb)
                {
                        divr=divc/divb;
                        mulr=divc%divb;
                }
                else
                {
                        divr=0xFFFF;
                        mulr=divc;
                }
                return;
                case 0x4209: /*Vertical IRQ Position Low*/
//                printf("VIRQ low %02X %02X:%04X\n",val,pbr,pc);
                vertline=(vertline&0x100)|val;
                return;
                case 0x420A: /*Vertical IRQ Position High*/
//                printf("VIRQ high %02X %02X:%04X\n",val,pbr,pc);
                vertline=(vertline&0xFF)|((val<<8)&0x100);
                irqon=0;
                return;
                case 0x420B: /*DMA enable*/
//                printf("DMA enable %02X\n",val);
                temp=1;
                for (c=0;c<8;c++)
                {
                        if (val&temp)
                        {
                                len=dma.size[c];
                                a=dma.src[c];
                                a2=dma.dest[c]|0x2100;
                                //printf("DMA %i src %02X:%04X dest 21%02X size %04X ctrl %02X\n",c,dma.srcbank[c],dma.src[c],dma.dest[c],dma.size[c],dma.ctrl[c]);
                                do
                                {
                                        if (dma.ctrl[c]&0x80)
                                        {
                                                if (mempointv[dma.srcbank[c]][a>>13])
                                                   mempoint[dma.srcbank[c]][a>>13][a&0x1FFF]=readppu(a2);
//                                                printf("DMA 0x80\n");
//                                                dumpregs();
//                                                exit(-1);
//                                                val=readppu(a2);
//                                                writemem(dma.srcbank[c],a,val);
                                        }
                                        else
                                        {
//                                                if (c==0 && dma.dest[0]==4 && pc==0x8253) printf("%02X %04X %02X\n",dma.srcbank[c],a,a2);
                                                if (mempointv[dma.srcbank[c]][a>>13])
                                                   val2=mempoint[dma.srcbank[c]][a>>13][a&0x1FFF];
                                                else
                                                   val2=0;
//                                                val2=readmem(dma.srcbank[c],a);
                                                writeppu(a2,val2);
                                        }
                                        switch (dma.ctrl[c]&0xF)
                                        {
                                                case 0:
                                                case 2:
                                                if (dma.ctrl[c]&16) a--;
                                                else                a++;
                                                break;
                                                case 1:
                                                if (a2==(dma.dest[c]|0x2100)) a2++;
                                                else                          a2--;
                                                if (dma.ctrl[c]&16) a--;
                                                else                a++;
                                                break;
                                                case 8:
                                                case 0xA:
                                                break;
                                                case 9:
                                                if (a2==(dma.dest[c]|0x2100)) a2++;
                                                else                          a2--;
                                                break;
                                                default:
                                                printf("Bad DMA mode %02X\n",dma.ctrl[c]);
                                                dumpregs();
                                                exit(-1);
                                        }
                                        len--;
/*                                        dmadone++;
                                        dmadone&=(dmawsize[dma.ctrl[c]&7]-1);
                                        if (!dmadone && len<dmawsize[dma.ctrl[c]&7]) len=0;*/
                                }
                                while (len);
                                dma.src[c]=a;
                                dma.size[c]=0;
                        }
                        temp<<=1;
                }
                return;
                
                case 0x420C: /*HDMA Enable*/
                hdmaena=val;
//                printf("HDMAENA %02X %02X:%04X\n",val,pbr,pc);
//                if (val==0x7F && pbr==0xC2 && pc==0x841A) dumpregs();
                return;
                case 0x420D: /*ROM speed*/
                fastrom=val&1;
                return;
                
                case 0x4300: case 0x4310: case 0x4320: case 0x4330:
                case 0x4340: case 0x4350: case 0x4360: case 0x4370:
                dma.ctrl[(addr>>4)&7]=val;
                return;
                case 0x4301: case 0x4311: case 0x4321: case 0x4331:
                case 0x4341: case 0x4351: case 0x4361: case 0x4371:
                dma.dest[(addr>>4)&7]=val;
                return;
                case 0x4302: case 0x4312: case 0x4322: case 0x4332:
                case 0x4342: case 0x4352: case 0x4362: case 0x4372:
                dma.src[(addr>>4)&7]&=0xFF00;
                dma.src[(addr>>4)&7]|=val;
                return;
                case 0x4303: case 0x4313: case 0x4323: case 0x4333:
                case 0x4343: case 0x4353: case 0x4363: case 0x4373:
                dma.src[(addr>>4)&7]&=0xFF;
                dma.src[(addr>>4)&7]|=(val<<8);
                return;
                case 0x4304: case 0x4314: case 0x4324: case 0x4334:
                case 0x4344: case 0x4354: case 0x4364: case 0x4374:
                dma.srcbank[(addr>>4)&7]=val;
                return;
                case 0x4305: case 0x4315: case 0x4325: case 0x4335:
                case 0x4345: case 0x4355: case 0x4365: case 0x4375:
                dma.size[(addr>>4)&7]&=0xFF00;
                dma.size[(addr>>4)&7]|=val;
                return;
                case 0x4306: case 0x4316: case 0x4326: case 0x4336:
                case 0x4346: case 0x4356: case 0x4366: case 0x4376:
                dma.size[(addr>>4)&7]&=0xFF;
                dma.size[(addr>>4)&7]|=(val<<8);
                return;
                case 0x4307: case 0x4317: case 0x4327: case 0x4337:
                case 0x4347: case 0x4357: case 0x4367: case 0x4377:
                hdma.ibank[(addr>>4)&7]=val;
                return;
//                default:
//                printf("Bad I/O write %04X %02X\n",addr,val);
        }
}

void dumpio()
{
        printf("COUNTENA %02X\n",countena);
}
