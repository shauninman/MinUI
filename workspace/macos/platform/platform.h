/**
 * macos/platform/platform.h - Platform definitions for macOS development/testing
 *
 * This is a development platform for testing MinUI on macOS:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - Uses evdev/keyboard input codes
 * - FAKESD path for local development
 * - No joystick input support
 * - Inverted volume scale (63 = mute, 0 = max volume)
 *
 * @note This platform is for development only, not production deployment
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// macOS development platform does not use SDL keyboard input
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
#define	BUTTON_POWER	BUTTON_NA
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////
// Evdev/Keyboard Input Codes
// Maps to USB HID keycodes for keyboard input
///////////////////////////////

#define CODE_UP			82  // HID Up Arrow
#define CODE_DOWN		81  // HID Down Arrow
#define CODE_LEFT		80  // HID Left Arrow
#define CODE_RIGHT		79  // HID Right Arrow

#define CODE_SELECT		52  // HID 4
#define CODE_START		40  // HID Enter

#define CODE_A			22  // HID S
#define CODE_B			4   // HID A
#define CODE_X			26  // HID W
#define CODE_Y			20  // HID Q

#define CODE_L1			CODE_NA
#define CODE_R1			CODE_NA
#define CODE_L2			CODE_NA
#define CODE_R2			CODE_NA
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		44  // HID Space
#define CODE_POWER		42  // HID Backspace

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

///////////////////////////////
// Joystick Button Mappings
// macOS development platform uses D-pad (HAT) only
///////////////////////////////

#define JOY_UP			JOY_NA  // D-pad handled separately
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
#define FIXED_WIDTH		640            // Screen width in pixels
#define FIXED_HEIGHT	480            // Screen height in pixels (VGA)
#define FIXED_BPP		2              // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8) // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)  // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

// HDMI output support (currently disabled)
// #define HAS_HDMI	1
// #define HDMI_WIDTH 	1280
// #define HDMI_HEIGHT 720
// #define HDMI_PITCH 	(HDMI_WIDTH * FIXED_BPP)
// #define HDMI_SIZE	(HDMI_PITCH * HDMI_HEIGHT)

///////////////////////////////
// UI Layout Configuration
///////////////////////////////

#define MAIN_ROW_COUNT 6           // Number of rows visible in menu
#define PADDING 10                 // Padding for UI elements in pixels

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/Users/shauninman/Projects/Personal/MinUI/workspace/macos/FAKESD"
#define MUTE_VOLUME_RAW 63         // Volume scale is inverted: 63 = mute, 0 = max volume

///////////////////////////////

#endif
