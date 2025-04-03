// m17

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

#define BUTTON_UP		SDLK_UP
#define BUTTON_DOWN		SDLK_DOWN
#define BUTTON_LEFT		SDLK_LEFT
#define BUTTON_RIGHT	SDLK_RIGHT

#define BUTTON_SELECT	SDLK_RSHIFT
#define BUTTON_START	SDLK_RETURN

#define BUTTON_A		SDLK_B
#define BUTTON_B		SDLK_A
#define BUTTON_X		SDLK_Y
#define BUTTON_Y		SDLK_X

#define BUTTON_L1		SDLK_L
#define BUTTON_R1		SDLK_R
#define BUTTON_L2		SDLK_z
#define BUTTON_R2		SDLK_c
#define BUTTON_L3		BUTTON_NA
#define BUTTON_R3		BUTTON_NA

#define BUTTON_MENU		BUTTON_NA
#define BUTTON_MENU_ALT	BUTTON_NA
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////

#define CODE_UP			CODE_NA
#define CODE_DOWN		CODE_NA
#define CODE_LEFT		CODE_NA
#define CODE_RIGHT		CODE_NA

#define CODE_SELECT		CODE_NA
#define CODE_START		CODE_NA

#define CODE_A			CODE_NA
#define CODE_B			CODE_NA
#define CODE_X			CODE_NA
#define CODE_Y			CODE_NA

#define CODE_L1			CODE_NA
#define CODE_R1			CODE_NA
#define CODE_L2			CODE_NA
#define CODE_R2			CODE_NA
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		CODE_NA
#define CODE_MENU_ALT	CODE_NA
#define CODE_POWER		CODE_NA

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

///////////////////////////////

#define JOY_UP			11
#define JOY_DOWN		14
#define JOY_LEFT		12
#define JOY_RIGHT		13

#define JOY_SELECT		10
#define JOY_START		3

#define JOY_A			9
#define JOY_B			4
#define JOY_X			2
#define JOY_Y			7

#define JOY_L1			5
#define JOY_R1			1
#define JOY_L2			6
#define JOY_R2			8
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA

#define JOY_MENU		15
#define JOY_MENU_ALT	16
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_NA
#define JOY_MINUS		JOY_NA

///////////////////////////////

#define BTN_RESUME 			BTN_X
#define BTN_SLEEP 			BTN_MENU
#define BTN_WAKE 			BTN_MENU
#define BTN_MOD_VOLUME 		BTN_SELECT
#define BTN_MOD_BRIGHTNESS 	BTN_START
#define BTN_MOD_PLUS 		BTN_R1
#define BTN_MOD_MINUS 		BTN_L1

///////////////////////////////

#define FIXED_SCALE 	1
#define FIXED_WIDTH		480
#define FIXED_HEIGHT	273
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define SDCARD_PATH "/sdcard"
#define MUTE_VOLUME_RAW 0
#define HAS_NEON // maybe?

#define MAIN_ROW_COUNT 7

// this should be set to the devices native screen refresh rate
#define SCREEN_FPS 60.0
///////////////////////////////

#endif
