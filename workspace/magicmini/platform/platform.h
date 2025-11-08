/**
 * magicmini/platform/platform.h - Platform definitions for Magic Mini handheld
 *
 * The Magic Mini is a retro handheld gaming device with:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons with volume controls
 * - Uses hybrid input (evdev codes + joystick, joystick currently unused)
 * - NEON SIMD optimization support
 * - Secondary SD card mount at /storage/TF2
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// Magic Mini does not use SDL keyboard input
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
#define CODE_POWER		116  // KEY_POWER

#define CODE_PLUS		115  // KEY_VOLUMEUP
#define CODE_MINUS		114  // KEY_VOLUMEDOWN

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices (currently unused)
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

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/storage/TF2"  // Path to secondary SD card slot
#define MUTE_VOLUME_RAW 0           // Raw value for muted volume
#define HAS_NEON                    // ARM NEON SIMD optimizations available
#define SAMPLES 400                 // Audio buffer size (helps reduce fceumm audio underruns)

///////////////////////////////

#endif
