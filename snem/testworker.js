function snes_init(){
           reboot_emulator=Module.cwrap('reboot_emulator', 'number', ['string'])
	   native_set_joypad_state=Module._native_set_joypad_state
	   native_bitmap_pointer=Module._native_bitmap_pointer
	   mainloop=Module._mainloop
	   renderscreen=Module._renderscreen
	   palf=reboot_emulator("/_.smc")	
	   native_set_joypad_state(0x80000000)
	   frameskip=0

           }
function snes_mainloop(){		
		for(var _i=0;_i<=frameskip;_i++)
		  mainloop(palf ? 312 :262)		
		renderscreen()		  
		var bitmap=native_bitmap_pointer(-16,0)>>2		
		var src=i.subarray(bitmap,bitmap+288*224)  // // Unstable Hack: i is Heap32 compiled by closure
		var buffer=new ArrayBuffer(4*288*224)
		var src2=new Uint32Array(buffer)			// WW2
		for(var _i=0;_i<288*224;_i++) src2[_i]=src[_i]		// WW2
		postMessage({cmd:"render2", src:buffer}, [buffer])    // WW2
		postMessage({cmd:"render", src:src})			// WW1
		setTimeout("snes_mainloop()", 0);	      
}
           
onmessage=function(event) {  
  var data=event.data
  switch(data.cmd){
    case "loadfile":      
	Module.FS_createDataFile("/", "_.smc", new Uint8Array(data.buffer) , true, true)	
	postMessage({cmd: "print", txt:"file loaded"})
	break
    case "start":
      snes_init()
      snes_mainloop()
      break
    case "joy1":
      native_set_joypad_state(data.state)
      break
    case "frameskip":
      frameskip=data.value
      break
    default:
      postMessage({cmd:"print", txt:"unknown command "+data.cmd})
  }
}
