#S9XDEBUGGER=1
#S9XNETPLAY=1
#S9XZIP=1
#S9XJMA=1

# Fairly good and special-char-safe descriptor of the os being built on.
OS         = `uname -s -r -m|sed \"s/ /-/g\"|tr \"[A-Z]\" \"[a-z]\"|tr \"/()\" \"___\"`
BUILDDIR   = .

OBJECTS    = ../apu/apu.o ../apu/SNES_SPC.o ../apu/SNES_SPC_misc.o ../apu/SNES_SPC_state.o ../apu/SPC_DSP.o ../apu/SPC_Filter.o ../bsx.o ../c4.o ../c4emu.o  ../clip.o  ../controls.o ../cpu.o ../cpuexec.o ../cpuops.o  ../dma.o ../dsp.o ../dsp1.o ../dsp2.o ../dsp3.o ../dsp4.o ../fxinst.o ../fxemu.o ../gfx.o ../globals.o  ../memmap.o  ../obc1.o ../ppu.o ../reader.o ../sa1.o ../sa1cpu.o  ../sdd1.o ../sdd1emu.o ../seta.o ../seta010.o ../seta011.o ../seta018.o  ../snes9x.o ../spc7110.o ../srtc.o ../tile.o sdlmain.o sdlinput.o sdlvideo.o sdlaudio.o


CCC        = clang++
CC         = clang
GASM       = clang
INCLUDES   = -I. -I.. -I../apu/ 

# -I../unzip/ -I../jma/ -I../filter/

CCFLAGS    = -U__linux -O3 -fomit-frame-pointer -fno-exceptions -fno-rtti -pedantic -Wall -W -Wno-unused-parameter -I/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT -DHAVE_MKSTEMP -DHAVE_STRINGS_H -DHAVE_SYS_IOCTL_H -DHAVE_STDINT_H -DRIGHTSHIFT_IS_SAR $(DEFS)  -DUSE_SDL -DSOUND
CFLAGS     = $(CCFLAGS)

.SUFFIXES: .o .cpp .c .cc .h .m .i .s .obj

all: Makefile configure snes9x-sdl

Makefile: configure Makefile.in
	@echo "Makefile is older than configure or in-file. Run configure or touch Makefile."
	exit 1

configure: configure.ac
	@echo "configure is older than in-file. Run autoconf or touch configure."
	exit 1

snes9x-sdl: $(OBJECTS)
	$(CCC) $(INCLUDES) -o $@ $(OBJECTS) -lm  -lSDL

../jma/s9x-jma.o: ../jma/s9x-jma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/7zlzma.o: ../jma/7zlzma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/crc32.o: ../jma/crc32.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/iiostrm.o: ../jma/iiostrm.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/inbyte.o: ../jma/inbyte.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/jma.o: ../jma/jma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/lzma.o: ../jma/lzma.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/lzmadec.o: ../jma/lzmadec.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@
../jma/winout.o: ../jma/winout.cpp
	$(CCC) $(INCLUDES) -c $(CCFLAGS) -fexceptions $*.cpp -o $@

.cpp.o:
	$(CCC) $(INCLUDES) -c $(CCFLAGS) $*.cpp -o $@

.c.o:
	$(CC) $(INCLUDES) -c $(CCFLAGS) $*.c -o $@

.cpp.S:
	$(GASM) $(INCLUDES) -S $(CCFLAGS) $*.cpp -o $@

.cpp.i:
	$(GASM) $(INCLUDES) -E $(CCFLAGS) $*.cpp -o $@

.S.o:
	$(GASM) $(INCLUDES) -c $(CCFLAGS) $*.S -o $@

.S.i:
	$(GASM) $(INCLUDES) -c -E $(CCFLAGS) $*.S -o $@

.s.o:
	@echo Compiling $*.s
	sh-elf-as -little $*.s -o $@

.obj.o:
	cp $*.obj $*.o

clean:
	rm -f $(OBJECTS)
