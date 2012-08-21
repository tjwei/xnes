#S9XDEBUGGER=1
#S9XNETPLAY=1
#S9XZIP=1
#S9XJMA=1

# Fairly good and special-char-safe descriptor of the os being built on.
OS         = `uname -s -r -m|sed \"s/ /-/g\"|tr \"[A-Z]\" \"[a-z]\"|tr \"/()\" \"___\"`
BUILDDIR   = .

OBJECTS    =  ../apu/apu.bc ../apu/SNES_SPC.bc ../apu/SNES_SPC_misc.bc ../apu/SNES_SPC_state.bc ../apu/SPC_DSP.bc ../apu/SPC_Filter.bc ../bsx.bc ../c4.bc ../c4emu.bc  ../clip.bc  ../controls.bc ../cpu.bc ../cpuexec.bc ../cpuops.bc ../dma.bc ../dsp.bc ../dsp1.bc ../dsp2.bc ../dsp3.bc ../dsp4.bc ../fxinst.bc ../fxemu.bc ../gfx.bc ../globals.bc  ../memmap.bc  ../obc1.bc ../ppu.bc ../reader.bc ../sa1.bc ../sa1cpu.bc  ../sdd1.bc ../sdd1emu.bc ../seta.bc ../seta010.bc ../seta011.bc ../seta018.bc  ../snes9x.bc ../spc7110.bc ../srtc.bc ../tile.bc 

GUISRC = sdlmain.cpp sdlinput.cpp sdlvideo.cpp sdlaudio.cpp



CCC        = emcc
CC         = emcc
GASM       = emcc
INCLUDES   = -I. -I.. -I../apu/ 

CCFLAGS    =  --llvm-opts 3 -O1 -DLSB_FIRST -U__linux -fomit-frame-pointer -fno-exceptions -fno-rtti -pedantic -Wall -W -Wno-unused-parameter -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT -DHAVE_MKSTEMP -DHAVE_STRINGS_H -DHAVE_SYS_IOCTL_H -DHAVE_STDINT_H -DRIGHTSHIFT_IS_SAR $(DEFS)
CFLAGS     = $(CCFLAGS)

.SUFFIXES: .bc .cpp .c .cc .h .m .i .s .bcbj

all: snes9x.js

snes9x.js: $(OBJECTS)
	$(CCC)  $(INCLUDES) $(CCFLAGS) -O2 --closure 0 --embed-file smw.smc $(INCLUDES) -o $@ $(OBJECTS) $(GUISRC) -lm -lSDL

html: $(OBJECTS)
	$(CCC) -O1 --closure 0 --embed-file smw.smc $(INCLUDES) -o xnes9x.html -DUSE_SDL -DHTML $(INCLUDES) $(CCFLAGS) $(GUISRC) $(OBJECTS) -lm -lSDL

../jma/s9x-jma.bc: ../jma/s9x-jma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/7zlzma.bc: ../jma/7zlzma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/crc32.bc: ../jma/crc32.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/iiostrm.bc: ../jma/iiostrm.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/inbyte.bc: ../jma/inbyte.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/jma.bc: ../jma/jma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/lzma.bc: ../jma/lzma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/lzmadec.bc: ../jma/lzmadec.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/winout.bc: ../jma/winout.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@

.cpp.bc:
	$(CCC) $(INCLUDES) -c $(CCFLAGS) $*.cpp -o $@

.c.bc:
	$(CC) $(INCLUDES) -c $(CCFLAGS) $*.c -o $@

.cpp.S:
	$(GASM) $(INCLUDES) -S $(CCFLAGS) $*.cpp -o $@

.cpp.i:
	$(GASM) $(INCLUDES) -E $(CCFLAGS) $*.cpp -o $@

.S.bc:
	$(GASM) $(INCLUDES) -c $(CCFLAGS) $*.S -o $@

.S.i:
	$(GASM) $(INCLUDES) -c -E $(CCFLAGS) $*.S -o $@

.s.bc:
	@echo Compiling $*.s
	sh-elf-as -little $*.s -o $@

.bcbj.bc:
	cp $*.bcbj $*.bc

clean:
	rm -f $(OBJECTS)
