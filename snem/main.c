//1b15a
#include <stdio.h>
#include <stdlib.h>
#include "snes.h"

int sblankchk=0;
int soundupdate=0;
static int speed_count=0;
#ifdef SOUND
static void soundcheck()
{
        soundupdate=1;
}
#endif
static int incspeedcount()
{
        speed_count++;
	//printf("%d\n",speed_count);
	return 0;
}
int spcemu,palf;
int reboot_emulator (char *) __attribute__((used));
int reboot_emulator (char *romfilename){
  if(loadsmc(romfilename)==-1){
    printf("can not find rom file %s\n",romfilename);
    exit(1);
    return 0;
  }
  makeopcodetable();
  makeromtable();
  reset65816();
  //resetppu();
  resetspc();
  initppu();
  speed_count=0;    
  return palf;
}

#ifdef EMCCHACK
#if 0
#include "romlist.h"
//char *roms[2]={"roms/smw.smc", "roms/Angel Eyes (PD).fig"};
int reboot_romnum=-1;
int js_callback(){
  if(reboot_romnum>=0 && reboot_romnum<NROMS) {    
    char *file=roms[reboot_romnum];
    reboot_romnum=-1;
    printf("reboot %s\n", file);
   reboot_emulator(file);
  }
    
  native_poll_keyboard();
  mainloop(palf ? 312 : 262);
  renderscreen();
  return 0;
}
void reboot (int i) __attribute__((used));
void reboot (int i){
  reboot_romnum=i;
}
#endif
#endif


void dumpregs();



int main(int argc, char *argv[])
{        
        int fps, end_loop;	
        spcemu=0;
	native_hardware_init(spcemu);	
#ifndef  EMCCHACK
	if(argc==1)
	  reboot_emulator("smw.smc");
	else
	  reboot_emulator(argv[1]);
	printf("palf=%d\n",palf);	
#ifdef SET_BY_SOURCE	
//#define BENCHMARK
#endif
#ifdef BENCHMARK	
	for(end_loop=0;end_loop<5000;end_loop++){	  
	  mainloop(palf ? 312 : 262);	  
	  renderscreen();	  
	}
#else
	
	native_tick_callback(incspeedcount,  palf  ? 50 : 60);
	end_loop=0;
        while (!end_loop)
        { 
           end_loop= native_poll_keyboard()==-1;	    
	   if(speed_count>0)     
	    {
	     mainloop(palf ? 312 : 262);	     
	      speed_count--;
	      vbl=0;
	      //dumpregs();
	    }
	      renderscreen();		
	      UPDATE_SOUND;
        }
#endif
#else    
   //reboot(0);
   //native_tick_callback(js_callback, 200);
#endif
	native_set_joypad_state(0x80000000);
        return 0;
}
