/**
 * tg5040/platform/platform.h - Platform definitions for TG5040 and Brick variants
 *
 * The TG5040 platform supports two device variants:
 * - TG5040 standard: 1280x720 widescreen display
 * - Brick variant: 1024x768 display (4:3 aspect ratio)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1) with analog L2/R2 triggers
 * - Analog sticks (left and right)
 * - L3/R3 buttons (Brick variant only)
 * - Menu and power buttons with volume controls
 * - Uses joystick input with HAT for D-pad
 * - Runtime detection of Brick variant
 *
 * @note A/B and X/Y button mappings were swapped in first public stock release
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
// Runtime variables for hardware differences
///////////////////////////////

extern int is_brick;  // Set to 1 for Brick variant (1024x768 display)

///////////////////////////////
// SDL Keyboard Button Mappings
// TG5040 does not use SDL keyboard input
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
#define	BUTTON_POWER	116   // Direct power button code (not SDL)
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

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
#define CODE_POWER		102  // KEY_HOME

#define CODE_PLUS		128  // Volume up
#define CODE_MINUS		129  // Volume down

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices (D-pad uses HAT)
///////////////////////////////

#define JOY_UP			JOY_NA  // D-pad handled via HAT
#define JOY_DOWN		JOY_NA
#define JOY_LEFT		JOY_NA
#define JOY_RIGHT		JOY_NA

#define JOY_SELECT		6
#define JOY_START		7

// Button mappings were swapped in first public stock release
#define JOY_A			1
#define JOY_B			0
#define JOY_X			3
#define JOY_Y			2

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			JOY_NA  // Analog trigger (handled via axis)
#define JOY_R2			JOY_NA  // Analog trigger (handled via axis)
#define JOY_L3			(is_brick?9:JOY_NA)   // L3 available on Brick only
#define JOY_R3			(is_brick?10:JOY_NA)  // R3 available on Brick only

#define JOY_MENU		8
#define JOY_POWER		102                   // Matches CODE_POWER
#define JOY_PLUS		(is_brick?14:128)     // Button 14 (Brick) or code 128 (standard)
#define JOY_MINUS		(is_brick?13:129)     // Button 13 (Brick) or code 129 (standard)

///////////////////////////////
// Analog Stick and Trigger Axis Mappings
// Hardware analog axes
///////////////////////////////

#define AXIS_L2			2  // ABSZ - Left trigger analog input
#define AXIS_R2			5  // RABSZ - Right trigger analog input

#define AXIS_LX			0  // ABS_X - Left stick X-axis (-30k left to 30k right)
#define AXIS_LY			1  // ABS_Y - Left stick Y-axis (-30k up to 30k down)
#define AXIS_RX			3  // ABS_RX - Right stick X-axis (-30k left to 30k right)
#define AXIS_RY			4  // ABS_RY - Right stick Y-axis (-30k up to 30k down)

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
// Runtime-configurable for Brick variant
///////////////////////////////

#define FIXED_SCALE 	(is_brick?3:2)           // Scaling: 3x (Brick) or 2x (standard)
#define FIXED_WIDTH		(is_brick?1024:1280)     // Width: 1024px (Brick) or 1280px (standard)
#define FIXED_HEIGHT	(is_brick?768:720)       // Height: 768px (Brick) or 720px (standard)
#define FIXED_BPP		2                        // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8)          // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)     // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)  // Total framebuffer size

///////////////////////////////
// UI Layout Configuration
// Adjusted for Brick variant
///////////////////////////////

#define MAIN_ROW_COUNT (is_brick?7:8)  // Number of rows: 7 (Brick) or 8 (standard)
#define PADDING (is_brick?5:40)        // UI padding: 5px (Brick) or 40px (standard)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"  // Path to SD card mount point
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume

///////////////////////////////

#endif
