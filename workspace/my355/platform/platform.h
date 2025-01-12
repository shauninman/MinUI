// my355/platform/platform.h

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#include "sdl.h"

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

#define CODE_UP			82
#define CODE_DOWN		81
#define CODE_LEFT		80
#define CODE_RIGHT		79

#define CODE_SELECT		228
#define CODE_START		40

#define CODE_A			44
#define CODE_B			224
#define CODE_X			225
#define CODE_Y			226

#define CODE_L1			43
#define CODE_R1			42
#define CODE_L2			75
#define CODE_R2			78
#define CODE_L3			230
#define CODE_R3			229

#define CODE_MENU		41
#define CODE_POWER		102

#define CODE_PLUS		128
#define CODE_MINUS		129

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

#define AXIS_LX	0
#define AXIS_LY	1
#define AXIS_RX	4
#define AXIS_RY	3

///////////////////////////////

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

extern int on_hdmi;

#define FIXED_SCALE 	2
#define FIXED_WIDTH		640
#define FIXED_HEIGHT	480
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

#define MAIN_ROW_COUNT (on_hdmi?8:6)
#define PADDING (on_hdmi?40:10)

///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"
#define MUTE_VOLUME_RAW 0
// #define HAS_NEON
#define SAMPLES 400 // fix for (most) fceumm underruns

///////////////////////////////

#endif
