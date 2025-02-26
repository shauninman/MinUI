// rg35xx/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

#define	BUTTON_UP		SDLK_KATAKANA
#define	BUTTON_DOWN		SDLK_HIRAGANA
#define	BUTTON_LEFT		SDLK_HENKAN
#define	BUTTON_RIGHT	SDLK_KATAKANAHIRAGANA

#define	BUTTON_SELECT	SDLK_PRINT
#define	BUTTON_START	SDLK_KP_DIVIDE

#define	BUTTON_A		SDLK_MUHENKAN
#define	BUTTON_B		SDLK_KP_JPCOMMA
#define	BUTTON_X		SDLK_KP_ENTER
#define	BUTTON_Y		SDLK_RCTRL

#define	BUTTON_L1		SDLK_RALT
#define	BUTTON_R1		SDLK_BREAK
#define	BUTTON_L2		SDLK_HOME
#define	BUTTON_R2		SDLK_UP
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define	BUTTON_MENU		SDLK_PAGEUP
#define	BUTTON_POWER	SDLK_POWER
#define	BUTTON_PLUS		SDLK_DOWN
#define	BUTTON_MINUS	SDLK_PAGEDOWN

///////////////////////////////

#define CODE_UP			0x5A
#define CODE_DOWN		0x5B
#define CODE_LEFT		0x5C
#define CODE_RIGHT		0x5D

#define CODE_SELECT		0x63
#define CODE_START		0x62

#define CODE_A			0x5E
#define CODE_B			0x5F
#define CODE_X			0x60
#define CODE_Y			0x61

#define CODE_L1			0x64
#define CODE_R1			0x65
#define CODE_L2			0x66
#define CODE_R2			0x67
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		0x68
#define CODE_POWER		0x74

#define CODE_PLUS		0x6C
#define CODE_MINUS		0x6D

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

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		640
#define FIXED_HEIGHT	480
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0
#define HAS_NEON

// this should be set to the devices native screen refresh rate
#define SCREEN_FPS 60.0
///////////////////////////////

#endif
