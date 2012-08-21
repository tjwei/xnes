#ifndef uint16
#define uint16 unsigned short
#endif

// COLOR_MODE 0:ansi 1:grayscale 2:xterm-color256 3: konsole 24bit

#define COLOR_MODE 3

// palette theme
//#define LINUX_CONSOLE
//#define PCMAN
#define XTERM_COLOR


#define COLS 80
#define ROWS 30

#define TRUE 1
#define FALSE 0
#if COLOR_MODE <= 1
static char gray_string[]=" .:-=+*#%$";
static unsigned ansi_color_table[65536][3];
static int ansi_color_table_ready=FALSE;

#ifdef PCMAN
static unsigned int dct[16][3] = { //default color table
        //Darker color
        {0,0,0},              //0;30m         Black
        {128,0,0},                //0;31m         Dark red
        {0,128,0},                //0;32m         Dark green
        {128,128,0},  //0;33m         Brown
        {0,0,128},                //0;34m         Dark blue
        {128,0,128},  //0;35m         Dark magenta
        {0,128,128},  //0;36m         Dark cyan
        {192,192,192},  //0;37m         Light gray
        //Bright color
        {128,128,128},    //1;30m         Gray
        {255,0,0},          //1;31m         Red
        {0,255,0},          //1;32m         Green
        {255,255,0},      //1;33m         Yellow
        {0,0,255},          //1;34m         Blue
        {255,0,255},      //1;35m         Magenta
        {0,255,255},      //1;36m         Cyan
        {255, 255,255}   //1;37m         White
};
#define LIGHT_ADJUST(x) ((x)/2)
#endif
#ifdef XTERM_COLOR
static unsigned int dct[16][3] = { 
   {0,0,0},  {205,0,0}, {0,205,0}, {205,205,0},
   {0,0,238}, {205,0,205},  {0,205,205},  {229,229,229},
   {127,127,127}, {0xff,0,0}, {0,255,0}, {0xff,0xff,0},
    {92,92,0xff}, {0xff,0,0xff}, {0,0xff,0xff}, {0xff,0xff,0xff}};
#define LIGHT_ADJUST(x) ((x)*10/16)
#endif
#ifdef TANGO_COLOR
static unsigned int dct[16][3] = { 
   {0,0,0},  {205,0,0}, {0,205,0}, {205,205,0},
   {0,0,238}, {205,0,205},  {0,205,205},  {229,229,229},
   {127,127,127}, {0xff,0,0}, {0,255,0}, {0xff,0xff,0},
    {92,92,0xff}, {0xff,0,0xff}, {0,0xff,0xff}, {0xff,0xff,0xff}};
#define LIGHT_ADJUST(x) ((x)*9/13)
#endif
#ifdef LINUX_CONSOLE
static unsigned int dct[16][3] = { 
   {0,0,0},  {0xaa,0,0}, {0,0xaa,0}, {0xaa,0x55,0},
   {0,0,0xaa}, {0xaa,0,0xaa},  {0,0xaa,0xaa},  {0xaa,0xaa,0xaa},
   {0x55,0x55,0x55}, {0xff,0x55,0x55}, {0x55,0xff,0x55}, {0xff,0xff,0x55},
    {0x55,0x55,0xff}, {0xff,0x55,0xff}, {0x55,0xff,0xff}, {0xff,0xff,0xff}};
#define LIGHT_ADJUST(x) ((x)*7/13)
#endif
static void init_ansi_color_table(){
    unsigned int c;
    for(c=0;c<65536;c++){
        int best_fg=0, best_bg=0, best_level,min_diff=0xffffff;
        int fg,bg,level, diff,r,g,b,rmean;        
        int rgb[3], rgb2[3];
        int i;
        rgb[0]=((c>>11)&0x1f)<<3;
        rgb[1]=((c>>5)&0x3f)<<2;
        rgb[2]=(c&0x1f)<<3;        
        for(fg=0;fg<16;fg++)
            for(bg=0;bg<8;bg++)
                for(level=0;level<10;level++){
                    for(i=0;i<3;i++)
                        rgb2[i]=(dct[fg][i]*level+dct[bg][i]*(18-level))/36- LIGHT_ADJUST(rgb[i]);
                    rmean = (2*rgb[0]+rgb2[0])/2;
                    r=rgb2[0];g=rgb2[1];b=rgb2[2];
                    diff=(((512+rmean)*r*r)>>8) + 4*g*g + (((767-rmean)*b*b)>>8);                    
                    if(diff < min_diff){
                        best_level=level;
                        best_fg=fg;
                        best_bg=bg;
                        min_diff=diff;
                    }                    
        }
        ansi_color_table[c][0]=best_fg;
        ansi_color_table[c][1]=best_bg;
        ansi_color_table[c][2]=best_level;
    }
    ansi_color_table_ready=TRUE;
}

#endif
static void print_color(uint16 c, uint16 c2){    
#if COLOR_MODE>=1 
    unsigned int r,g,b,  level;
    r=((c>>11)&0x1f)<<3;
    g=((c>>5)&0x3f)<<2;
    b=(c&0x1f)<<3;
#endif
#if COLOR_MODE >=2
    #if COLOR_MODE == 2
        level=16+(r*6/256)*36+(g*6/256)*6+(b*6/256);
        printf("\033[48;5;%d;", level);
    #else
        printf("\033[48;2;%d;%d;%d;", r,g,b);
    #endif
    r=((c2>>11)&0x1f)<<3;
    g=((c2>>5)&0x3f)<<2;
    b=(c2&0x1f)<<3;
    #if COLOR_MODE == 2
        level=16+(r*6/256)*36+(g*6/256)*6+(b*6/256);        
        printf("38;5;%dm\xe2\x96\x84", level);
    #else
        printf("38;2;%d;%d;%dm\xe2\x96\x84", r, g,b);
    #endif    
#endif    
#if COLOR_MODE == 1
    level=(((r * 0.30) + (g * 0.59) + (b * 0.11))/256.0*10.0);
    if(level>9) level=9;
    printf("%c",gray_string[level]);	
#endif
#if COLOR_MODE == 0
    static unsigned int last_fg, last_bg;
    if(!ansi_color_table_ready) init_ansi_color_table();
    if(c2 ||  last_fg!=ansi_color_table[c][0] || last_bg!=ansi_color_table[c][1])
        printf("\033[%d;3%d;4%dm", ansi_color_table[c][0]>>3,
                              ansi_color_table[c][0]&7, 
                              ansi_color_table[c][1]);
    printf("%c", gray_string[ansi_color_table[c][2]]);
    last_fg=ansi_color_table[c][0];
    last_bg=ansi_color_table[c][1];
#endif    
    

}
static uint16 RGB565FromRGB24(unsigned char *bp, int x){    
    unsigned r,g,b;    
    r=((bp[3*x]>>3)&0x1f);
    g=((bp[3*x+1]>>2)&0x3f);
    b=((bp[3*x+2]>>3)&0x1f); 
    return (r<<11)|(g<<5)|b;
    
}
static unsigned get_average_color(int x1, int y1, int x2, int y2, unsigned char * data, int bpp, int pitch){
    int x,y;
    unsigned int r=0,g=0,b=0;
    unsigned color;
    unsigned char *bp;
    int size=(y2-y1)*(x2-x1);
    if (size <=0) {y2=y1+1; x2=x1+1;size=1;}
    for(y=y1;y<y2;y++){                        
        bp=data+y*pitch;
        for(x=x1;x<x2;x++){
            color = bpp==24 ? RGB565FromRGB24(bp, x) : ((uint16 *)bp)[x];
            r+=color>>11;
            g+=(color>>5)&0x3f;
            b+=color&0x1f;                            
        }
    }
    r/=size;
    g/=size;
    b/=size;
    return (r<<11)|(g<<5)|b;
}
static void drawansi (int width, int height, void *data0, int bpp, int pitch)
{
    unsigned c1, c2;    
    unsigned char *data=(unsigned char *) data0; 
    int x,y, xstep, ystep, xstart, xend, ystart, yend;
    int cols=COLS, rows=ROWS;
    xstep=width/cols;
    ystep=height/rows;
    xstart=(width-xstep*cols)/2;
    ystart=(height-ystep*rows)/2;
    xend=xstart+cols*xstep;
    yend=ystart+rows*ystep;
    printf("\033[H");   
    uint16 reset=1;
    for(y=ystart;y<yend;y+=ystep)
        {
            if(y!=ystart) printf("\r\n");
            for(x=xstart;x<xend;x+=xstep)
            {
                #if COLOR_MODE >=2        
                    c1=get_average_color(x, y, x+xstep, y+ystep/2, data, bpp, pitch);
                    c2=get_average_color(x, y+ystep/2, x+xstep, y+ystep, data, bpp, pitch);
                    print_color(c1, c2);
                #else
                c1=get_average_color(x, y, x+xstep, y+ystep, data, bpp, pitch);
                print_color(c1, reset);
                reset=0;
                #endif
        }
       
    }
     printf("\033[0m");
    fflush(stdout);
    
}
