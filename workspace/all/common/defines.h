#ifndef __DEFINES_H__
#define __DEFINES_H__

#include "platform.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

#define MAX_PATH 512

#define ROMS_PATH SDCARD_PATH "/Roms"
#define ROOT_SYSTEM_PATH SDCARD_PATH "/.system/"
#define SYSTEM_PATH SDCARD_PATH "/.system/" PLATFORM
#define RES_PATH SDCARD_PATH "/.system/res"
#define FONT_PATH RES_PATH "/BPreplayBold-unhinted.otf"
#define USERDATA_PATH SDCARD_PATH "/.userdata/" PLATFORM
#define PAKS_PATH SYSTEM_PATH "/paks"
#define ARCH_PATH SDCARD_PATH "/.userdata/" ARCH_TAG
#define RECENT_PATH ARCH_PATH "/.minui/recent.txt"
#define AUTO_RESUME_PATH ARCH_PATH "/.minui/auto_resume.txt"
#define AUTO_RESUME_SLOT 9

#define FAUX_RECENT_PATH SDCARD_PATH "/Recently Played"
#define COLLECTIONS_PATH SDCARD_PATH "/Collections"

#define LAST_PATH "/tmp/last.txt" // transient
#define CHANGE_DISC_PATH "/tmp/change_disc.txt"
#define RESUME_SLOT_PATH "/tmp/resume_slot.txt"

#define TRIAD_WHITE 		0xff,0xff,0xff
#define TRIAD_BLACK 		0x00,0x00,0x00
#define TRIAD_LIGHT_GRAY 	0x7f,0x7f,0x7f
#define TRIAD_GRAY 			0x99,0x99,0x99
#define TRIAD_DARK_GRAY 	0x26,0x26,0x26

#define TRIAD_LIGHT_TEXT 	0xcc,0xcc,0xcc
#define TRIAD_DARK_TEXT 	0x66,0x66,0x66

#define COLOR_WHITE			(SDL_Color){TRIAD_WHITE}
#define COLOR_GRAY			(SDL_Color){TRIAD_GRAY}
#define COLOR_BLACK			(SDL_Color){TRIAD_BLACK}
#define COLOR_LIGHT_TEXT	(SDL_Color){TRIAD_LIGHT_TEXT}
#define COLOR_DARK_TEXT		(SDL_Color){TRIAD_DARK_TEXT}
#define COLOR_BUTTON_TEXT	(SDL_Color){TRIAD_GRAY}

#define BASE_WIDTH 320
#define BASE_HEIGHT 240

#define SCREEN_WIDTH 	640
#define SCREEN_HEIGHT 	480
#define SCREEN_SCALE 	2 // SCREEN_HEIGHT / BASE_HEIGHT

#define SCREEN_DEPTH 	16
#define SCREEN_BPP 		2
#define SCREEN_PITCH 	(SCREEN_WIDTH * SCREEN_BPP)

// all before scale
#define PILL_SIZE 30
#define BUTTON_SIZE 20
#define BUTTON_MARGIN 5 // ((PILL_SIZE - BUTTON_SIZE) / 2)
#define BUTTON_PADDING 12
#define SETTINGS_SIZE 4
#define SETTINGS_WIDTH 80

#define MAIN_ROW_COUNT 6 // SCREEN_HEIGHT / (PILL_SIZE * SCREEN_SCALE) - 2 (floor and subtract 1 if not an integer)
#define PADDING 10 // PILL_SIZE / 3 (or non-integer part of the previous calculatiom divided by three)

#define FONT_LARGE 16 	// menu
#define FONT_MEDIUM 14 	// single char button label
#define FONT_SMALL 12 	// button hint
#define FONT_TINY 10  	// multi char button label

///////////////////////////////

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MAX(a, b) (a) > (b) ? (a) : (b)
#define MIN(a, b) (a) < (b) ? (a) : (b)
#define CEIL_DIV(a,b) ((a) + (b) - 1) / (b)

#define SCALE1(a) ((a)*SCREEN_SCALE)
#define SCALE2(a,b) ((a)*SCREEN_SCALE),((b)*SCREEN_SCALE)
#define SCALE4(a,b,c,d) ((a)*SCREEN_SCALE),((b)*SCREEN_SCALE),((c)*SCREEN_SCALE),((d)*SCREEN_SCALE)

///////////////////////////////

#ifndef BTN_A // prevent collisions with input.h in keymon
// TODO: doesn't this belong in api.h? it's meaningless without PAD_*
enum {
	BTN_ID_NONE = -1,
	BTN_ID_UP,
	BTN_ID_DOWN,
	BTN_ID_LEFT,
	BTN_ID_RIGHT,
	BTN_ID_A,
	BTN_ID_B,
	BTN_ID_X,
	BTN_ID_Y,
	BTN_ID_START,
	BTN_ID_SELECT,
	BTN_ID_L1,
	BTN_ID_R1,
	BTN_ID_L2,
	BTN_ID_R2,
	BTN_ID_MENU,
	BTN_ID_PLUS,
	BTN_ID_MINUS,
	BTN_ID_POWER,
	BTN_ID_COUNT,
};
enum {
	BTN_NONE	= 0,
	BTN_UP 		= 1 << BTN_ID_UP,
	BTN_DOWN	= 1 << BTN_ID_DOWN,
	BTN_LEFT	= 1 << BTN_ID_LEFT,
	BTN_RIGHT	= 1 << BTN_ID_RIGHT,
	BTN_A		= 1 << BTN_ID_A,
	BTN_B		= 1 << BTN_ID_B,
	BTN_X		= 1 << BTN_ID_X,
	BTN_Y		= 1 << BTN_ID_Y,
	BTN_START	= 1 << BTN_ID_START,
	BTN_SELECT	= 1 << BTN_ID_SELECT,
	BTN_L1		= 1 << BTN_ID_L1,
	BTN_R1		= 1 << BTN_ID_R1,
	BTN_L2		= 1 << BTN_ID_L2,
	BTN_R2		= 1 << BTN_ID_R2,
	BTN_MENU	= 1 << BTN_ID_MENU,
	BTN_PLUS	= 1 << BTN_ID_PLUS,
	BTN_MINUS	= 1 << BTN_ID_MINUS,
	BTN_POWER	= 1 << BTN_ID_POWER,
};
#endif

#endif