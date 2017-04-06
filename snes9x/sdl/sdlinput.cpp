/***********************************************************************************
  Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.

  See CREDITS file to find the copyright owners of this file.

  SDL Input/Audio/Video code (many lines of code come from snes9x & drnoksnes)
  (c) Copyright 2011         Makoto Sugano (makoto.sugano@gmail.com)

  Snes9x homepage: http://www.snes9x.com/

  Permission to use, copy, modify and/or distribute Snes9x in both binary
  and source form, for non-commercial purposes, is hereby granted without
  fee, providing that this license information and copyright notice appear
  with all copies and any derived work.

  This software is provided 'as-is', without any express or implied
  warranty. In no event shall the authors be held liable for any damages
  arising from the use of this software or it's derivatives.

  Snes9x is freeware for PERSONAL USE only. Commercial users should
  seek permission of the copyright holders first. Commercial use includes,
  but is not limited to, charging money for Snes9x or software derived from
  Snes9x, including Snes9x or derivatives in commercial game bundles, and/or
  using Snes9x as a promotion for your commercial product.

  The copyright holders request that bug fixes and improvements to the code
  should be forwarded to them so everyone can benefit from the modifications
  in future versions.

  Super NES and Super Nintendo Entertainment System are trademarks of
  Nintendo Co., Limited and its subsidiary companies.
 ***********************************************************************************/

#include "sdl_snes9x.h"

#include "snes9x.h"
#include "port.h"
#include "controls.h"
#include "display.h"

// domaemon: FIXME, just collecting the essentials.
// domaemon: *) here we define the keymapping.
void S9xParseInputConfig (ConfigFile &conf, int pass)
{

	return;
}

// void S9xInitInputDevices (void)
// {
// 	S9xUnmapAllControls();
// #ifdef USE_SDL
//     S9xMapButton(SDLK_RIGHT, S9xGetCommandT("Joypad1 Right"), false);
//     S9xMapButton(SDLK_LEFT, S9xGetCommandT("Joypad1 Left"), false);
//     S9xMapButton(SDLK_DOWN, S9xGetCommandT("Joypad1 Down"), false);
//     S9xMapButton(SDLK_UP, S9xGetCommandT("Joypad1 Up"), false);
//     S9xMapButton(SDLK_RETURN, S9xGetCommandT("Joypad1 Start"), false);
//     S9xMapButton(SDLK_SPACE,  S9xGetCommandT("Joypad1 Select"), false);
//     S9xMapButton(SDLK_d, S9xGetCommandT("Joypad1 A"), false);
//     S9xMapButton(SDLK_c, S9xGetCommandT("Joypad1 B"), false);
//     S9xMapButton(SDLK_s, S9xGetCommandT("Joypad1 X"), false);
//     S9xMapButton(SDLK_x, S9xGetCommandT("Joypad1 Y"), false);
//     S9xMapButton(SDLK_a, S9xGetCommandT("Joypad1 L"), false);
//     S9xMapButton(SDLK_z, S9xGetCommandT("Joypad1 R"), false);
// #endif
// }
void S9xInitInputDevices (int r, int l, int d, int u, int st, int sl, int a, int b, int x, int y, int lb, int rb)
{
	S9xUnmapAllControls();
#ifdef USE_SDL
    S9xMapButton(r, S9xGetCommandT("Joypad1 Right"), false);
    S9xMapButton(l, S9xGetCommandT("Joypad1 Left"), false);
    S9xMapButton(d, S9xGetCommandT("Joypad1 Down"), false);
    S9xMapButton(u, S9xGetCommandT("Joypad1 Up"), false);
    S9xMapButton(st, S9xGetCommandT("Joypad1 Start"), false);
    S9xMapButton(sl,  S9xGetCommandT("Joypad1 Select"), false);
    S9xMapButton(a, S9xGetCommandT("Joypad1 A"), false);
    S9xMapButton(b, S9xGetCommandT("Joypad1 B"), false);
    S9xMapButton(x, S9xGetCommandT("Joypad1 X"), false);
    S9xMapButton(y, S9xGetCommandT("Joypad1 Y"), false);
    S9xMapButton(lb, S9xGetCommandT("Joypad1 L"), false);
    S9xMapButton(rb, S9xGetCommandT("Joypad1 R"), false);
#endif
}

void S9xProcessEvents (bool8 block)
{
    #ifdef USE_SDL
	SDL_Event event;
	bool8 quit_state = FALSE;

	while ((block) || (SDL_PollEvent (&event) != 0))
	{
		switch (event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// domaemon: not sure it's the best idea, but reserving the SDLK_q for quit.
			if (event.key.keysym.sym == SDLK_q)
			{
				quit_state = TRUE;
			}
			else
			{
				S9xReportButton(event.key.keysym.mod << 16 | // keyboard mod
						event.key.keysym.sym, // keyboard ksym
						event.type == SDL_KEYDOWN); // press or release
			}
			break;


		case SDL_QUIT:
			// domaemon: we come here when the window is getting closed.
			quit_state = TRUE;
			break;
		}
	}

	if (quit_state == TRUE)
	{
		printf ("Quit Event. Bye.\n");
		S9xExit();
	}
    #endif
}

bool S9xPollButton (uint32 id, bool *pressed)
{
	return (false);
}

bool S9xPollAxis (uint32 id, int16 *value)
{
	return (false);
}

bool S9xPollPointer (uint32 id, int16 *x, int16 *y)
{
	return (false);
}

// domaemon: needed by SNES9X
void S9xHandlePortCommand (s9xcommand_t cmd, int16 data1, int16 data2)
{
	return;
}
