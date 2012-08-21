#define uint32 unsigned int
#define uint16 unsigned short
#ifdef SET_BY_SOURCE
#define USE_SDL
#define ASCII
#endif
#ifdef ASCII
#include "drawansi.h"
#endif
#ifdef USE_SDL
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifdef USE_SDL
  SDL_Surface *screen;
  Uint8 *keystates;
  uint32 *bitmap;
#else
  uint32 bitmap[288*256];  
#endif
#ifdef EMCCHACK
#define RGB555toRGB24(t) \
			( (((t)&0x1f)<<3) | (((t)&0x3e0)<<6) | (((t)&0x7c00)<<9) )
#else
#define RGB555toRGB24(t) \
			( (((t)&0x1f)<<19) | (((t)&0x3e0)<<6) | (((t)&0x7c00)>>7) )
#endif
void native_hardware_init(int spcemu){
#ifdef USE_SDL
	
	if( SDL_Init( SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_AUDIO) == -1 )  exit(-1);
	screen = SDL_SetVideoMode( 288, 256, 32, SDL_SWSURFACE|SDL_DOUBLEBUF); 
	SDL_LockSurface(screen);
	bitmap=(uint32 *)screen->pixels;
	//keystates = SDL_GetKeyState( NULL );
#ifdef EMCCHACK
	keystates = SDL_GetKeyboardState( NULL );
#else
	atexit(SDL_Quit);
	keystates = SDL_GetKeyState( NULL );
#endif
#endif
#ifdef SOUND
	if (spcemu) initsound();
        if (spcemu) install_int_ex(soundcheck,BPS_TO_TIMER(500));
#endif
}

uint32* native_set_joypad_state(uint32 state) __attribute__((used));
uint32* native_bitmap_pointer(int x, int y){ 
  return (void *)(&bitmap[(y+16)*288+x+16]);
}
static int cnt=0;
void native_bitmap_to_screen(){
#ifdef USE_SDL  
  uint32 *bp=(uint32 *)native_bitmap_pointer(-16,-16);
  int x,y;
  for(y=0;y<16*288;y++)
    *bp++=0;
  bp=(uint32 *)native_bitmap_pointer(-16,224);
  for(y=0;y<16*288;y++)
    *bp++=0;
  for(y=0;y<=224;y++){
    bp=(uint32 *)native_bitmap_pointer(-32,y);
    for(x=0;x<32;x++)
      *bp++=0;
  }
    
  SDL_UnlockSurface(screen);
  SDL_Flip(screen);
  SDL_LockSurface(screen);
#endif
  #ifdef ASCII
  if((cnt&7)==0){
  drawansi(256, 224, (void *)native_bitmap_pointer(0,0), 32, 288*4); 
  printf("%d\n",cnt);
  }
#endif

cnt++;
      
}
void native_bitmap_clear_line(int line, uint32 color){
 int x;
 uint32 *bp=(uint32 *)native_bitmap_pointer(0, line);
 for(x=0;x<256;x++)
   bp[x]=color;
 //memset(native_bitmap_pointer(0, line), 0, 264*2);
}

int native_poll_keyboard(){
#if defined(USE_SDL)
  SDL_Event event;
  while( SDL_PollEvent( &event ) ){
    if( event.type == SDL_QUIT )  {return -1;}
  }
  if(keystates[SDLK_ESCAPE]) {return -1;}
#else
 
#endif
  return 0;

}
#ifdef USE_SDL
static Uint32 my_callbackfunc(Uint32 interval, void *param)
{

       int (*func)(void);
       int stop_running;
       func=(int (*)(void))param;
       stop_running=func();
       if(stop_running)
	 return 0;
#ifdef EMCCHACK     	 
	  SDL_AddTimer( interval, my_callbackfunc, param);
#endif
       return(interval);
}
#endif
void native_tick_callback( int (*func)(void), int fps){
#ifdef USE_SDL
  SDL_AddTimer( 1000/fps, my_callbackfunc, (void *)func);
#endif
}
static uint32 joy1state=0x80000000;
uint32* native_set_joypad_state(uint32 state){
      joy1state=state;
      return &joy1state;
}
uint32 native_joypad_state(int num)
{
	uint32 joy1=0x80000000;	
	if(num!=0) return joy1;
#ifdef USE_SDL
	native_poll_keyboard();
        if (keystates[SDLK_w])     joy1|=0x0010;
        if (keystates[SDLK_q])     joy1|=0x0020;
        if (keystates[SDLK_a])     joy1|=0x0040;
        if (keystates[SDLK_s])     joy1|=0x0080;
        if (keystates[SDLK_RIGHT]) joy1|=0x0100;
        if (keystates[SDLK_LEFT])  joy1|=0x0200;
        if (keystates[SDLK_DOWN])  joy1|=0x0400;
        if (keystates[SDLK_UP])    joy1|=0x0800;
        if (keystates[SDLK_c])     joy1|=0x1000;
        if (keystates[SDLK_d])     joy1|=0x2000;
        if (keystates[SDLK_z])     joy1|=0x4000;
        if (keystates[SDLK_x])     joy1|=0x8000;
#else
	/*if(cnt%120==111)
	    joy1|=(cnt&0xffff);*/
	joy1=joy1state;

#endif
	return joy1;
}