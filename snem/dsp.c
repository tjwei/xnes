#include <stdio.h>
#include "snes.h"
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

int decode[64];

int samppos[8],sampf[8]; // the type of samppos and sampf were the fixed type of allegro
int sampfreq[8];
int sampvol[8][3];
int sampsamp[8];
int keyed[8];
int sloop[8];

#define ATTACK  1
#define DECAY   2
#define SUSTAIN 3
#define RELEASE 4
typedef struct ADSR
{
        int sr,sl,ar,dr;
        int ena;
} ADSR;
ADSR adsr[8];
int adsrstate[8];
float voicelev[8];
float voicediff[8];
float voicetarg[8];
int realvol[8];

float ardiffs[16]=
{
        0.00406146,0.00543660,0.01110239,0.01665625,
        0.02602639,0.04383392,0.06406127,0.10463323,
        0.17350216,0.26025324,0.41640625,0.69400864,
        1.00195313,1.00312500,1.00520833,1.00000000
};
float drdiffs[8]=
{
        0.01100112,0.02250859,0.03785366,0.05743075,
        0.09253413,0.15141997,0.22508057,0.45016647
};
float srdiffs[32]=
{
        0.00000000,0.00043760,0.00059696,0.00069290,
        0.00087412,0.00118859,0.00138580,0.00176956,
        0.00234520,0.00282490,0.00354445,0.00475969,
        0.00574574,0.00693966,0.00925288,0.01110239,
        0.01387932,0.01892683,0.02250859,0.02823301,
        0.03785366,0.04501185,0.05743608,0.07571265,
        0.09253413,0.11103989,0.15141997,0.18104411,
        0.22508057,0.30283994,0.45016647,0.59486531
};

unsigned short diraddr;

void writedsp(int a, unsigned char v)
{
        int c;
        if (a&1)
        {
                dspregs[dspaddr&127]=v;
                switch (dspaddr&127)
                {
                        case 0x00: case 0x10: case 0x20: case 0x30:
                        case 0x40: case 0x50: case 0x60: case 0x70:
                        sampvol[dspaddr>>4][0]=v&127;
                        sampvol[dspaddr>>4][2]=sampvol[dspaddr>>4][0]+sampvol[dspaddr>>4][1];
                        break;
                        case 0x01: case 0x11: case 0x21: case 0x31:
                        case 0x41: case 0x51: case 0x61: case 0x71:
                        sampvol[dspaddr>>4][1]=v&127;
                        sampvol[dspaddr>>4][2]=sampvol[dspaddr>>4][0]+sampvol[dspaddr>>4][1];
                        break;
                        case 0x02: case 0x12: case 0x22: case 0x32:
                        case 0x42: case 0x52: case 0x62: case 0x72:
                        sampfreq[dspaddr>>4]&=0xFF00;
                        sampfreq[dspaddr>>4]|=v;
                        break;
                        case 0x03: case 0x13: case 0x23: case 0x33:
                        case 0x43: case 0x53: case 0x63: case 0x73:
                        sampfreq[dspaddr>>4]&=0xFF;
                        sampfreq[dspaddr>>4]|=((v&0x3F)<<8);
                        break;
                        case 0x04: case 0x14: case 0x24: case 0x34:
                        case 0x44: case 0x54: case 0x64: case 0x74:
                        sampsamp[dspaddr>>4]=v&63;
                        samppos[dspaddr>>4]=0;
                        break;
                        case 0x05: case 0x15: case 0x25: case 0x35:
                        case 0x45: case 0x55: case 0x65: case 0x75:
                        dspaddr>>=4;
                        adsr[dspaddr].ena=v>>7;
                        adsr[dspaddr].dr=(v>>4)&7;
                        adsr[dspaddr].ar=v&15;
                        if (adsrstate[dspaddr]==ATTACK) voicediff[dspaddr]=ardiffs[adsr[dspaddr].ar];
                        break;
                        case 0x06: case 0x16: case 0x26: case 0x36:
                        case 0x46: case 0x56: case 0x66: case 0x76:
                        dspaddr>>=4;
                        adsr[dspaddr].sr=v&31;
                        adsr[dspaddr].sl=v>>5;
                        break;

                        case 0x4c:
//                        printf("Key on %02X %04X\n",v,spc.pc);
                        for (c=0;c<8;c++)
                        {
                                if (v&(1<<c))
                                {
                                        keyed[c]=1;
                                        adsrstate[c]=ATTACK;
                                        voicelev[c]=0;
                                        voicediff[c]=ardiffs[adsr[c].ar&15];
                                        samppos[c]=0;
                                        sloop[c]=0;
                                }
                        }
                        break;
                        case 0x5c:
//                        printf("Key off %02X %04X\n",v,spc.pc);
                        for (c=0;c<8;c++)
                        {
                                if (v&(1<<c))
                                {
                                        keyed[c]=0;
                                        samppos[c]=0;
                                        sloop[c]=0;
                                        adsrstate[c]=0;
                                }
                        }
                        break;
                        case 0x5D:
                        diraddr=v<<8;
                        memset(decode,1,sizeof(decode));
                        updatespuaccess(v);
                        break;
                }
        }
        else
           dspaddr=v;
}

unsigned char readdsp(int a)
{
        if (a&1) return dspregs[dspaddr&127];
        return dspaddr;
}

unsigned short samples[64][32768],sampler[64][32768];
int samp[64][2];
int samplen[64][2],samplenr[64];
int samploop[64],samploopp[64];


int decodedelay=60;

signed short lastsamp[2];
int range,filter;
inline signed short decodebrr(int v)
{
        signed short temp=v&0xF;
        float tempf;
        if (temp&8) temp|=0xFFF0;
        temp<<=range;
        switch (filter)
        {
                case 0: break;
                case 1: tempf=(float)lastsamp[0]*((float)15/(float)16); temp+=tempf; break;
                case 2: tempf=((float)lastsamp[0]*((float)61/(float)32))-((float)lastsamp[1]*((float)15/(float)16)); temp+=tempf; break;
                case 3: tempf=((float)lastsamp[0]*((float)115/(float)64))-((float)lastsamp[1]*((float)13/(float)16)); temp+=tempf; break;
                default:
                printf("Unimplemented filter type %i\n",filter);
                exit(-1);
        }
        lastsamp[1]=lastsamp[0];
        lastsamp[0]=temp;
        return temp;
}
#ifdef SOUND
int decodecalls=0;
void decodesamples()
{
        unsigned char *block;
        int c;
        unsigned short addr,addr2=diraddr,laddr,faddr;
        signed short temp;
        int bnum;
        unsigned short saddr;
        decodecalls++;
        decodecalls&=15;
        for (c=0;c<64;c++)
        {
                if (decodecalls==15) decode[c]=5;
                if (decode[c]==5)
                {
                        samp[c][0]=samp[c][1]=samploop[c]=0;
                        faddr=addr=spuram[addr2]|(spuram[addr2+1]<<8);
                        laddr=spuram[addr2+2]|(spuram[addr2+3]<<8);
                        addr2+=4;
                        if (addr)
                        {
                                samp[c][0]=1;
                                block=&spuram[addr];
                                saddr=0;
                                bnum=0;
                                lastsamp[0]=lastsamp[1]=0;
                                while (saddr<0x7000)
                                {
                                        if (block[0]&2) samploop[c]=1;
//                                        if (c==0) printf("Block 0 : %02X saddr %04X\n",block[0],saddr);
                                        range=block[0]>>4;
                                        filter=(block[0]>>2)&3;
                                        temp=decodebrr(block[1]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[1]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[2]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[2]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[3]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[3]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[4]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[4]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[5]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[5]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[6]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[6]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[7]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[7]&0xF); samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[8]>>4);  samples[c][saddr++]=(signed short)temp;
                                        temp=decodebrr(block[8]&0xF); samples[c][saddr++]=(signed short)temp;
                                        bnum+=16;
                                        if (block[0]&1) goto donesmp1;
                                        addr+=9;
                                        block=&spuram[addr];
                                }
                                donesmp1:
                                samplen[c][0]=bnum;
                                if (laddr)
                                {
                                        samp[c][1]=1;
                                        addr=laddr;
                                        block=&spuram[addr];
                                        bnum=0;
                                        saddr=0;
                                        while ((block[0]&2) && saddr<0x7000)
                                        {
                                                range=block[0]>>4;
                                                filter=(block[0]>>2)&3;
                                                temp=decodebrr(block[1]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[1]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[2]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[2]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[3]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[3]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[4]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[4]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[5]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[5]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[6]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[6]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[7]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[7]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[8]>>4);  sampler[c][saddr++]=(signed short)temp;
                                                temp=decodebrr(block[8]&0xF); sampler[c][saddr++]=(signed short)temp;
                                                bnum+=16;
                                                if (block[0]&1) goto donesmp2;
                                                addr+=9;
                                                block=&spuram[addr];
                                        }
                                        donesmp2:
                                        samplen[c][1]=bnum;
                                }
//                                printf("Sample %i decoded len %i looplen %i\n",c,samplen[c][0],samplen[c][1]);
                        }
                        decode[c]=0;
                }
                else
                {
                        if (decode[c]) decode[c]++;
                        addr2+=4;
                }
        }
}

AUDIOSTREAM *as;
FILE *sf;

void initsound()
{
        install_sound(DIGI_AUTODETECT,MIDI_NONE,0);
        as=play_audio_stream(533,16,0,30000,255,128);
///        sf=fopen("sound.pcm","wb");
}
int decodedelay2=5;
void updatesound()
{
        unsigned short *buf;
        int c,d;
        uint32 templ;
        
        buf=0;
              buf=(unsigned short *)get_audio_stream_buffer(as);
        if (buf)
        {
                decodesamples();
                for (c=0;c<8;c++)
                    sampf[c]=sampfreq[c]<<4;
                for (c=0;c<8;c++)
                {
                        if (adsr[c].ena)
                        {
                        switch (adsrstate[c])
                        {
                                case ATTACK:
                                voicelev[c]+=voicediff[c];
                                realvol[c]=(int)(voicelev[c]*(float)sampvol[c][2]);
                                if (voicelev[c]>=1)
                                {
                                        voicelev[c]=1;
                                        adsrstate[c]=DECAY;
                                        voicediff[c]=drdiffs[adsr[c].dr&7];
                                        voicetarg[c]=((float)((adsr[c].sl&7)+1))/8;
                                }
                                break;
                                case DECAY:
                                voicelev[c]-=(voicediff[c]*(1-voicetarg[c]));
                                if (voicelev[c]<=voicetarg[c])
                                {
                                        voicelev[c]=1;
                                        adsrstate[c]=SUSTAIN;
                                        voicediff[c]=srdiffs[adsr[c].sr&31];
                                }
                                realvol[c]=(int)(voicelev[c]*(float)sampvol[c][2]);
                                break;
                                case SUSTAIN:
                                voicelev[c]-=voicediff[c];
                                if (voicelev[c]==0)
                                {
                                        adsrstate[c]=0;
                                }
                                realvol[c]=(int)((voicelev[c]*voicetarg[c])*(float)sampvol[c][2]);
                                break;
                        }
                        }
                        else
                           realvol[c]=sampvol[c][2];
                }
                for (c=0;c<533;c++)
                {
                        buf[c]=0;
                        for (d=0;d<8;d++)
                        {
                                if (keyed[d] && samp[sampsamp[d]][sloop[d]])
                                {
                                        if (sloop[d]) templ=sampler[sampsamp[d]][(samppos[d]>>16)&32767];
                                        else          templ=samples[sampsamp[d]][(samppos[d]>>16)&32767];
                                        if (templ&0x8000) templ|=0xFFFF0000;
                                        templ*=(realvol[d]);//sampvol[d][2];
                                        templ>>=8;
                                        buf[c]+=(templ&0xFFFF);
                                        samppos[d]+=sampf[d];
                                        if ((samppos[d]>>16)>=samplen[sampsamp[d]][sloop[d]])
                                        {
                                                if (samp[sampsamp[d]][1] && samploop[sampsamp[d]])
                                                {
                                                        sloop[d]=1;
                                                        samppos[d]=0;
                                                }
                                                else
                                                   keyed[d]=0;
                                        }
                                }
                        }

                }

                for (c=0;c<533;c++) buf[c]^=0x8000;
                free_audio_stream_buffer(as);
                for (c=0;c<8;c++) dspregs[(c<<4)|8]=voicelev[c]*256;

        }
}
#else
void updatesound(){}

#endif