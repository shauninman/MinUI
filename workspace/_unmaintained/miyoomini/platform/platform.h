// miyoomini/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

#define BUTTON_UP 		SDLK_UP
#define BUTTON_DOWN 	SDLK_DOWN
#define BUTTON_LEFT 	SDLK_LEFT
#define BUTTON_RIGHT 	SDLK_RIGHT

#define BUTTON_SELECT 	SDLK_RCTRL
#define BUTTON_START 	SDLK_RETURN

#define BUTTON_A 		SDLK_SPACE
#define BUTTON_B 		SDLK_LCTRL
#define BUTTON_X 		SDLK_LSHIFT
#define BUTTON_Y 		SDLK_LALT

#define BUTTON_L1 		SDLK_e
#define BUTTON_R1 		SDLK_t
#define BUTTON_L2 		SDLK_TAB
#define BUTTON_R2 		SDLK_BACKSPACE
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define BUTTON_MENU	 	SDLK_ESCAPE
#define BUTTON_POWER 	SDLK_POWER
#define	BUTTON_PLUS		SDLK_RSUPER
#define	BUTTON_MINUS	SDLK_LSUPER

///////////////////////////////

#define CODE_UP			103
#define CODE_DOWN		108
#define CODE_LEFT		105
#define CODE_RIGHT		106

#define CODE_SELECT		97
#define CODE_START		28

#define CODE_A			57
#define CODE_B			29
#define CODE_X			42
#define CODE_Y			56

#define CODE_L1			18
#define CODE_R1			20
#define CODE_L2			15
#define CODE_R2			14
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		1
#define CODE_POWER		116

#define CODE_PLUS		115
#define CODE_MINUS		114

///////////////////////////////

#define JOY_UP			JOY_NA
#define JOY_DOWN		JOY_NA
#define JOY_LEFT		JOY_NA
#define JOY_RIGHT		JOY_NA

#define JOY_SELECT		JOY_NA
#define JOY_START		JOY_NA

#define JOY_A			JOY_NA
#define JOY_B			JOY_NA
#define JOY_X			JOY_NA
#define JOY_Y			JOY_NA

#define JOY_L1			JOY_NA
#define JOY_R1			JOY_NA
#define JOY_L2			JOY_NA
#define JOY_R2			JOY_NA
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA

#define JOY_MENU		JOY_NA
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_NA
#define JOY_MINUS		JOY_NA

///////////////////////////////

extern int is_plus;
#define BTN_RESUME 			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		(is_plus ? BTN_NONE : BTN_SELECT)
#define BTN_MOD_BRIGHTNESS 	(is_plus ? BTN_MENU : BTN_START)
#define BTN_MOD_PLUS 		(is_plus ? BTN_PLUS : BTN_R1)
#define BTN_MOD_MINUS 		(is_plus ? BTN_MINUS : BTN_L1)

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		640
#define FIXED_HEIGHT	480
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"
#define MUTE_VOLUME_RAW -60
#define HAS_NEON

// this should be set to the devices native screen refresh rate
#define SCREEN_FPS 60.0
///////////////////////////////

#endif
