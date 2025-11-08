/**
 * rg35xxplus/platform/platform.h - Platform definitions for Anbernic RG35XX Plus/H/SP variants
 *
 * The Anbernic RG35XX Plus series includes multiple device variants:
 * - RG35XX Plus: 640x480 display
 * - RG35XX H (CubeXX): 720x720 square display
 * - RG35XX SP (RG34XX): 720x480 widescreen display
 * - 1280x720 HDMI output support (all variants)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons with volume controls
 * - Uses joystick input
 * - NEON SIMD optimization support
 * - Runtime detection of device variant and HDMI
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

extern int is_cubexx;  // Set to 1 for RG35XX H (square 720x720 screen)
extern int is_rg34xx;  // Set to 1 for RG35XX SP/RG34XX (720x480 widescreen)
extern int on_hdmi;    // Set to 1 when HDMI output is active

///////////////////////////////
// SDL Keyboard Button Mappings
// RG35XX Plus does not use SDL keyboard input
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
// Hardware keycodes (power button only)
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

#define CODE_PLUS		CODE_NA
#define CODE_MINUS		CODE_NA

///////////////////////////////
// Joystick Button Mappings
// Hardware joystick indices
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
#define JOY_PLUS		18  // Volume up
#define JOY_MINUS		17  // Volume down

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
// Runtime-configurable for device variants
///////////////////////////////

#define FIXED_SCALE 	2                                   // 2x scaling factor for UI
#define FIXED_WIDTH		(is_cubexx?720:(is_rg34xx?720:640)) // Width: 720 (H/SP) or 640 (Plus)
#define FIXED_HEIGHT	(is_cubexx?720:480)                 // Height: 720 (H) or 480 (Plus/SP)
#define FIXED_BPP		2                                   // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8)                     // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)           // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)        // Total framebuffer size

///////////////////////////////
// HDMI Output Specifications
///////////////////////////////

#define HAS_HDMI	1                         // HDMI output supported
#define HDMI_WIDTH 	1280                      // HDMI width in pixels
#define HDMI_HEIGHT 720                       // HDMI height in pixels (720p)
#define HDMI_PITCH 	(HDMI_WIDTH * FIXED_BPP)  // HDMI row stride
#define HDMI_SIZE	(HDMI_PITCH * HDMI_HEIGHT) // HDMI framebuffer size

// TODO: if HDMI_HEIGHT > FIXED_HEIGHT then MAIN_ROW_COUNT will be insufficient

///////////////////////////////
// UI Layout Configuration
// Adjusted for device variant and HDMI
///////////////////////////////

#define MAIN_ROW_COUNT (is_cubexx||on_hdmi?8:6)  // Rows: 8 (H/HDMI) or 6 (Plus/SP)
#define PADDING (is_cubexx||on_hdmi?40:10)       // Padding: 40px (H/HDMI) or 10px (Plus/SP)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/sdcard"  // Path to SD card mount point (lowercase)
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume
#define HAS_NEON                   // ARM NEON SIMD optimizations available
#define SAMPLES 400                // Audio buffer size (helps reduce fceumm audio underruns)

///////////////////////////////

#endif
