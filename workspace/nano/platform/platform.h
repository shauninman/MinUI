// nano

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include <SDL/SDL.h>

///////////////////////////////

#define BUTTON_UP		SDLK_u
#define BUTTON_DOWN		SDLK_d
#define BUTTON_LEFT		SDLK_l
#define BUTTON_RIGHT	SDLK_r

#define BUTTON_SELECT	SDLK_k
#define BUTTON_START	SDLK_s

#define BUTTON_A		SDLK_a
#define BUTTON_B		SDLK_b
#define BUTTON_X		SDLK_x
#define BUTTON_Y		SDLK_y

#define BUTTON_L1		SDLK_m
#define BUTTON_R1		SDLK_n
#define BUTTON_L2		BUTTON_NA
#define BUTTON_R2		BUTTON_NA
#define	BUTTON_L3		BUTTON_NA
#define	BUTTON_R3		BUTTON_NA

#define BUTTON_MENU		SDLK_q
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_POWEROFF	SDLK_ESCAPE

#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////

#define CODE_UP			22
#define CODE_DOWN		32
#define CODE_LEFT		38
#define CODE_RIGHT		19

#define CODE_SELECT		37
#define CODE_START		31

#define CODE_A			30
#define CODE_B			48
#define CODE_X			45
#define CODE_Y			21

#define CODE_L1			50
#define CODE_R1			49
#define CODE_L2			CODE_NA
#define CODE_R2			CODE_NA
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		16
#define CODE_POWER		CODE_NA
#define CODE_POWEROFF	1

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

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
#define JOY_POWEROFF	JOY_NA

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
#define FIXED_WIDTH		240
#define FIXED_HEIGHT	240
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define SDCARD_PATH "/mnt"
#define MUTE_VOLUME_RAW 0
#define HAS_NEON
#define USES_SWSCALER

///////////////////////////////

#endif
