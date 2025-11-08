/**
 * trimuismart/platform/platform.h - Platform definitions for Trimui Smart handheld
 *
 * The Trimui Smart is a compact retro handheld gaming device with:
 * - 320x240 display (QVGA resolution, 1x scale - no scaling)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1 only, no L2/R2)
 * - Menu and power buttons
 * - Uses hybrid input (SDL keyboard + evdev codes)
 * - NEON SIMD optimization support
 * - Compact form factor with smaller screen
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// Maps physical buttons to SDL key codes
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
#define BUTTON_L2		BUTTON_NA  // No L2 button on hardware
#define BUTTON_R2		BUTTON_NA  // No R2 button on hardware
#define	BUTTON_L3		BUTTON_NA
#define	BUTTON_R3		BUTTON_NA

#define BUTTON_MENU		SDLK_ESCAPE
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// Hardware keycodes from kernel input subsystem
///////////////////////////////

#define CODE_UP			103  // KEY_UP
#define CODE_DOWN		108  // KEY_DOWN
#define CODE_LEFT		105  // KEY_LEFT
#define CODE_RIGHT		106  // KEY_RIGHT

#define CODE_SELECT		97   // KEY_RIGHTCTRL
#define CODE_START		28   // KEY_ENTER

#define CODE_A			57   // KEY_SPACE
#define CODE_B			29   // KEY_LEFTCTRL
#define CODE_X			42   // KEY_LEFTSHIFT
#define CODE_Y			56   // KEY_LEFTALT

#define CODE_L1			15   // KEY_TAB
#define CODE_R1			14   // KEY_BACKSPACE
#define CODE_L2			CODE_NA
#define CODE_R2			CODE_NA
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		1    // KEY_ESC
#define CODE_POWER		CODE_NA

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

///////////////////////////////
// Joystick Button Mappings
// Trimui Smart does not use joystick input
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
// Function Button Mappings
// System-level button combinations
///////////////////////////////

#define BTN_RESUME 			BTN_X       // Button to resume from save state
#define BTN_SLEEP 			BTN_MENU    // Button to enter sleep mode
#define BTN_WAKE 			BTN_MENU    // Button to wake from sleep
#define BTN_MOD_VOLUME 		BTN_SELECT  // Hold SELECT for volume control
#define BTN_MOD_BRIGHTNESS 	BTN_START   // Hold START for brightness control
#define BTN_MOD_PLUS 		BTN_R1      // Increase with R1
#define BTN_MOD_MINUS 		BTN_L1      // Decrease with L1

///////////////////////////////
// Display Specifications
///////////////////////////////

#define FIXED_SCALE 	1              // No scaling (1:1 pixel mapping)
#define FIXED_WIDTH		320            // Screen width in pixels
#define FIXED_HEIGHT	240            // Screen height in pixels (QVGA)
#define FIXED_BPP		2              // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8) // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)  // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"  // Path to SD card mount point
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume
#define HAS_NEON                   // ARM NEON SIMD optimizations available

///////////////////////////////

#endif
