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
#define SHARED_USERDATA_PATH SDCARD_PATH "/.userdata/shared"
#define PAKS_PATH SYSTEM_PATH "/paks"
#define RECENT_PATH SHARED_USERDATA_PATH "/.minui/recent.txt"
#define SIMPLE_MODE_PATH SHARED_USERDATA_PATH "/enable-simple-mode"
#define AUTO_RESUME_PATH SHARED_USERDATA_PATH "/.minui/auto_resume.txt"
#define AUTO_RESUME_SLOT 9
#define GAME_SWITCHER_PERSIST_PATH SHARED_USERDATA_PATH "/.minui/game_switcher.txt"

#define FAUX_RECENT_PATH SDCARD_PATH "/Recently Played"
#define COLLECTIONS_PATH SDCARD_PATH "/Collections"

#define LAST_PATH "/tmp/last.txt" // transient
#define CHANGE_DISC_PATH "/tmp/change_disc.txt"
#define RESUME_SLOT_PATH "/tmp/resume_slot.txt"
#define NOUI_PATH "/tmp/noui"

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

// all before scale
#define PILL_SIZE 30
#define BUTTON_SIZE 20
#define BUTTON_MARGIN 5 // ((PILL_SIZE - BUTTON_SIZE) / 2)
#define BUTTON_PADDING 12
#define SETTINGS_SIZE 4
#define SETTINGS_WIDTH 80

#ifndef MAIN_ROW_COUNT
#define MAIN_ROW_COUNT 6 // FIXED_HEIGHT / (PILL_SIZE * FIXED_SCALE) - 2 (floor and subtract 1 if not an integer)
#endif

#ifndef PADDING
#define PADDING 10 // PILL_SIZE / 3 (or non-integer part of the previous calculatiom divided by three)
#endif

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

#define SCALE1(a) ((a)*FIXED_SCALE)
#define SCALE2(a,b) ((a)*FIXED_SCALE),((b)*FIXED_SCALE)
#define SCALE3(a,b,c) ((a)*FIXED_SCALE),((b)*FIXED_SCALE),((c)*FIXED_SCALE)
#define SCALE4(a,b,c,d) ((a)*FIXED_SCALE),((b)*FIXED_SCALE),((c)*FIXED_SCALE),((d)*FIXED_SCALE)

///////////////////////////////

#define HAS_POWER_BUTTON (BUTTON_POWER!=BUTTON_NA||CODE_POWER!=CODE_NA||JOY_POWER!=JOY_NA)
#define HAS_POWEROFF_BUTTON (BUTTON_POWEROFF!=BUTTON_NA)
#define HAS_MENU_BUTTON (BUTTON_MENU!=BUTTON_NA||CODE_MENU!=CODE_NA||JOY_MENU!=JOY_NA)
#define HAS_SKINNY_SCREEN (FIXED_WIDTH<320)

///////////////////////////////

#define BUTTON_NA	-1
#define CODE_NA		-1
#define JOY_NA		-1
#define AXIS_NA		-1

#ifndef BUTTON_POWEROFF
#define BUTTON_POWEROFF BUTTON_NA
#endif
#ifndef CODE_POWEROFF
#define CODE_POWEROFF CODE_NA
#endif

#ifndef BUTTON_MENU_ALT
#define BUTTON_MENU_ALT BUTTON_NA
#endif
#ifndef CODE_MENU_ALT
#define CODE_MENU_ALT CODE_NA
#endif
#ifndef JOY_MENU_ALT
#define JOY_MENU_ALT JOY_NA
#endif

#ifndef JOY_MENU_ALT2
#define JOY_MENU_ALT2 JOY_NA
#endif

#ifndef AXIS_L2
#define AXIS_L2	AXIS_NA
#define AXIS_R2	AXIS_NA
#endif 

#ifndef AXIS_LX
#define AXIS_LX	AXIS_NA
#define AXIS_LY	AXIS_NA
#define AXIS_RX	AXIS_NA
#define AXIS_RY	AXIS_NA
#endif 

#ifndef HAS_HDMI
#define HDMI_WIDTH	FIXED_WIDTH
#define HDMI_HEIGHT	FIXED_HEIGHT
#define HDMI_PITCH	FIXED_PITCH
#define HDMI_SIZE	FIXED_SIZE
#endif

#ifndef BTN_A // prevent collisions with input.h in keymon
// TODO: doesn't this belong in api.h? it's meaningless without PAD_*
enum {
	BTN_ID_NONE = -1,
	BTN_ID_DPAD_UP,
	BTN_ID_DPAD_DOWN,
	BTN_ID_DPAD_LEFT,
	BTN_ID_DPAD_RIGHT,
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
	BTN_ID_L3,
	BTN_ID_R3,
	BTN_ID_MENU,
	BTN_ID_PLUS,
	BTN_ID_MINUS,
	BTN_ID_POWER,	
	BTN_ID_POWEROFF,

	BTN_ID_ANALOG_UP,
	BTN_ID_ANALOG_DOWN,
	BTN_ID_ANALOG_LEFT,
	BTN_ID_ANALOG_RIGHT,

	BTN_ID_COUNT,
};
enum {
	BTN_NONE		= 0,
	BTN_DPAD_UP 	= 1 << BTN_ID_DPAD_UP,
	BTN_DPAD_DOWN	= 1 << BTN_ID_DPAD_DOWN,
	BTN_DPAD_LEFT	= 1 << BTN_ID_DPAD_LEFT,
	BTN_DPAD_RIGHT	= 1 << BTN_ID_DPAD_RIGHT,
	BTN_A			= 1 << BTN_ID_A,
	BTN_B			= 1 << BTN_ID_B,
	BTN_X			= 1 << BTN_ID_X,
	BTN_Y			= 1 << BTN_ID_Y,
	BTN_START		= 1 << BTN_ID_START,
	BTN_SELECT		= 1 << BTN_ID_SELECT,
	BTN_L1			= 1 << BTN_ID_L1,
	BTN_R1			= 1 << BTN_ID_R1,
	BTN_L2			= 1 << BTN_ID_L2,
	BTN_R2			= 1 << BTN_ID_R2,
	BTN_L3			= 1 << BTN_ID_L3,
	BTN_R3			= 1 << BTN_ID_R3,
	BTN_MENU		= 1 << BTN_ID_MENU,
	BTN_PLUS		= 1 << BTN_ID_PLUS,
	BTN_MINUS		= 1 << BTN_ID_MINUS,
	BTN_POWER		= 1 << BTN_ID_POWER,
	BTN_POWEROFF	= 1 << BTN_ID_POWEROFF,

	BTN_ANALOG_UP 	= 1 << BTN_ID_ANALOG_UP,
	BTN_ANALOG_DOWN	= 1 << BTN_ID_ANALOG_DOWN,
	BTN_ANALOG_LEFT	= 1 << BTN_ID_ANALOG_LEFT,
	BTN_ANALOG_RIGHT= 1 << BTN_ID_ANALOG_RIGHT,
	
	BTN_UP 		= BTN_DPAD_UP | BTN_ANALOG_UP,
	BTN_DOWN 	= BTN_DPAD_DOWN | BTN_ANALOG_DOWN,
	BTN_LEFT	= BTN_DPAD_LEFT | BTN_ANALOG_LEFT,
	BTN_RIGHT	= BTN_DPAD_RIGHT | BTN_ANALOG_RIGHT,
};
#endif

#endif