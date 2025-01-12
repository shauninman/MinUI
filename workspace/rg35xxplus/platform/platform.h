// rg35xx/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

///////////////////////////////

extern int is_cubexx;
extern int is_rg34xx;
extern int on_hdmi;

///////////////////////////////

#define	BUTTON_UP		BUTTON_NA
#define	BUTTON_DOWN		BUTTON_NA
#define	BUTTON_LEFT		BUTTON_NA
#define	BUTTON_RIGHT	BUTTON_NA

#define	BUTTON_SELECT	BUTTON_NA
#define	BUTTON_START	BUTTON_NA

#define	BUTTON_A		BUTTON_NA
#define	BUTTON_B		BUTTON_NA
#define	BUTTON_X		BUTTON_NA
#define	BUTTON_Y		BUTTON_NA

#define	BUTTON_L1		BUTTON_NA
#define	BUTTON_R1		BUTTON_NA
#define	BUTTON_L2		BUTTON_NA
#define	BUTTON_R2		BUTTON_NA
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define	BUTTON_MENU		BUTTON_NA
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
#define CODE_POWER		102

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

///////////////////////////////

#define JOY_UP			13
#define JOY_DOWN		16
#define JOY_LEFT		14
#define JOY_RIGHT		15

#define JOY_SELECT		6
#define JOY_START		7

#define JOY_A			0
#define JOY_B			1
#define JOY_X			3
#define JOY_Y			2

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			9
#define JOY_R2			10
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA

#define JOY_MENU		8
#define JOY_POWER		JOY_NA
#define JOY_PLUS		18
#define JOY_MINUS		17

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
#define FIXED_WIDTH		(is_cubexx?720:(is_rg34xx?720:640))
#define FIXED_HEIGHT	(is_cubexx?720:480)
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define HAS_HDMI	1
#define HDMI_WIDTH 	1280
#define HDMI_HEIGHT 720
#define HDMI_PITCH 	(HDMI_WIDTH * FIXED_BPP)
#define HDMI_SIZE	(HDMI_PITCH * HDMI_HEIGHT)

// TODO: if HDMI_HEIGHT > FIXED_HEIGHT then MAIN_ROW_COUNT will be insufficient


///////////////////////////////

#define MAIN_ROW_COUNT (is_cubexx||on_hdmi?8:6)
#define PADDING (is_cubexx||on_hdmi?40:10)

///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard"
#define MUTE_VOLUME_RAW 0
#define HAS_NEON
#define SAMPLES 400 // fix for (most) fceumm underruns

///////////////////////////////

#endif
