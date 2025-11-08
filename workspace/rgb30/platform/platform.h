/**
 * rgb30/platform/platform.h - Platform definitions for Anbernic RGB30 handheld
 *
 * The Anbernic RGB30 is a retro handheld gaming device with:
 * - 720x720 square display (1:1 aspect ratio)
 * - 1280x720 HDMI output support
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu buttons (primary and alternate)
 * - Uses hybrid input (minimal SDL keyboard + joystick)
 * - Larger UI with increased row count and padding
 *
 * @note Power button uses SDL keyboard mapping, volume controls use evdev codes
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// RGB30 uses minimal SDL keyboard input (power button only)
///////////////////////////////

#define BUTTON_UP		BUTTON_NA
#define BUTTON_DOWN		BUTTON_NA
#define BUTTON_LEFT		BUTTON_NA
#define BUTTON_RIGHT	BUTTON_NA

#define BUTTON_SELECT	BUTTON_NA
#define BUTTON_START	BUTTON_NA

#define BUTTON_A		BUTTON_NA
#define BUTTON_B		BUTTON_NA
#define BUTTON_X		BUTTON_NA
#define BUTTON_Y		BUTTON_NA

#define BUTTON_L1		BUTTON_NA
#define BUTTON_R1		BUTTON_NA
#define BUTTON_L2		BUTTON_NA
#define BUTTON_R2		BUTTON_NA
#define BUTTON_L3		BUTTON_NA
#define BUTTON_R3		BUTTON_NA

#define BUTTON_MENU		BUTTON_NA
#define BUTTON_MENU_ALT	BUTTON_NA
#define	BUTTON_POWER	SDLK_POWER    // Power key mapped to SDL
#define	BUTTON_PLUS		BUTTON_NA     // Available but not used (commented: SDLK_RSUPER)
#define	BUTTON_MINUS	BUTTON_NA     // Available but not used (commented: SDLK_LSUPER)

///////////////////////////////
// Evdev/Keyboard Input Codes
// Hardware keycodes from kernel input subsystem
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
#define CODE_POWER		102  // KEY_HOME

#define CODE_PLUS		129  // Volume up (swapped from keymon)
#define CODE_MINUS		128  // Volume down (swapped from keymon)

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices
///////////////////////////////

#define JOY_UP			13
#define JOY_DOWN		14
#define JOY_LEFT		15
#define JOY_RIGHT		16

#define JOY_SELECT		8
#define JOY_START		9

#define JOY_A			1
#define JOY_B			0
#define JOY_X			2
#define JOY_Y			3

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			6
#define JOY_R2			7
#define JOY_L3			JOY_NA
#define JOY_R3			JOY_NA

#define JOY_MENU		11
#define JOY_MENU_ALT	12  // Secondary menu button
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_NA
#define JOY_MINUS		JOY_NA

///////////////////////////////
// Function Button Mappings
// System-level button combinations
///////////////////////////////

#define BTN_RESUME			BTN_X       // Button to resume from save state
#define BTN_SLEEP 			BTN_POWER   // Button to enter sleep mode
#define BTN_WAKE 			BTN_POWER   // Button to wake from sleep
#define BTN_MOD_VOLUME 		BTN_NONE    // Modifier for volume control (none - direct buttons)
#define BTN_MOD_BRIGHTNESS 	BTN_MENU    // Hold MENU for brightness control
#define BTN_MOD_PLUS 		BTN_PLUS    // Increase with PLUS
#define BTN_MOD_MINUS 		BTN_MINUS   // Decrease with MINUS

///////////////////////////////
// Display Specifications
///////////////////////////////

#define FIXED_SCALE 	2              // 2x scaling factor for UI
#define FIXED_WIDTH		720            // Screen width in pixels (square display)
#define FIXED_HEIGHT	720            // Screen height in pixels (1:1 aspect ratio)
#define FIXED_BPP		2              // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8) // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)  // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI	1                         // HDMI output supported
#define HDMI_WIDTH 	1280                      // HDMI width in pixels
#define HDMI_HEIGHT 720                       // HDMI height in pixels (720p)
#define HDMI_PITCH 	(HDMI_WIDTH * FIXED_BPP)  // HDMI row stride
#define HDMI_SIZE	(HDMI_PITCH * HDMI_HEIGHT) // HDMI framebuffer size

///////////////////////////////
// UI Layout Configuration
// Larger values for square display
///////////////////////////////

#define MAIN_ROW_COUNT 8           // Number of rows visible in menu
#define PADDING 40                 // Padding for UI elements in pixels

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/storage/roms"  // Path to SD card mount point
#define MUTE_VOLUME_RAW 0            // Raw value for muted volume
#define SAMPLES 400                  // Audio buffer size

///////////////////////////////

#endif
