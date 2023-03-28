// trimuismart/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include <SDL/SDL.h>

///////////////////////////////

#define BUTTON_UP		SDLK_UP
#define BUTTON_DOWN		SDLK_DOWN
#define BUTTON_LEFT		SDLK_LEFT
#define BUTTON_RIGHT	SDLK_RIGHT

#define BUTTON_SELECT	SDLK_RCTRL
#define BUTTON_START	SDLK_RETURN

#define BUTTON_A		SDLK_SPACE
#define BUTTON_B		SDLK_LCTRL
#define BUTTON_X		SDLK_LSHIFT
#define BUTTON_Y		SDLK_LALT

#define BUTTON_L1		SDLK_TAB
#define BUTTON_R1		SDLK_BACKSPACE
#define BUTTON_L2		SDLK_q
#define BUTTON_R2		SDLK_BACKSLASH

#define BUTTON_MENU		SDLK_ESCAPE
#define	BUTTON_POWER	SDLK_UNDEFINED

#define	BUTTON_PLUS		SDLK_UNDEFINED
#define	BUTTON_MINUS	SDLK_UNDEFINED

///////////////////////////////

#define FIXED_SCALE 	1
#define FIXED_WIDTH 	320
#define FIXED_HEIGHT 	240
#define FIXED_DEPTH 	16

///////////////////////////////

#endif
