/**
 * miyoomini/platform/platform.h - Platform definitions for Miyoo Mini handheld
 *
 * The Miyoo Mini is one of the most popular retro handheld devices with:
 * - 640x480 or 752x560 display (VGA or higher, runtime detected)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons with volume controls
 * - Uses hybrid input (SDL keyboard + evdev codes)
 * - NEON SIMD optimization support
 * - Runtime detection of "Plus" variant with extra buttons
 * - Dynamic screen resolution support (560p variant)
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

extern int is_plus;   // Set to 1 for Miyoo Mini Plus variant
extern int is_560p;   // Set to 1 for 560p screen variant

///////////////////////////////
// SDL Keyboard Button Mappings
// Maps physical buttons to SDL key codes
///////////////////////////////

#define BUTTON_UP 		SDLK_UP
#define BUTTON_DOWN 	SDLK_DOWN
#define BUTTON_LEFT 	SDLK_LEFT
#define BUTTON_RIGHT 	SDLK_RIGHT

#define BUTTON_SELECT 	SDLK_RCTRL
#define BUTTON_START 	SDLK_RETURN

#define BUTTON_A 		SDLK_SPACE
#define BUTTON_B 		SDLK_LCTRL
#define BUTTON_X 		SDLK_LSHIFT
#define BUTTON_Y 		SDLK_LALT

#define BUTTON_L1 		SDLK_e
#define BUTTON_R1 		SDLK_t
#define BUTTON_L2 		SDLK_TAB
#define BUTTON_R2 		SDLK_BACKSPACE
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define BUTTON_MENU	 	SDLK_ESCAPE
#define BUTTON_POWER 	SDLK_POWER
#define	BUTTON_PLUS		SDLK_RSUPER  // Plus model only
#define	BUTTON_MINUS	SDLK_LSUPER  // Plus model only

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

#define CODE_L1			18   // KEY_E
#define CODE_R1			20   // KEY_T
#define CODE_L2			15   // KEY_TAB
#define CODE_R2			14   // KEY_BACKSPACE
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		1    // KEY_ESC
#define CODE_POWER		116  // KEY_POWER

#define CODE_PLUS		115  // KEY_VOLUMEUP (Plus model)
#define CODE_MINUS		114  // KEY_VOLUMEDOWN (Plus model)

///////////////////////////////
// Joystick Button Mappings
// Miyoo Mini does not use joystick input
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
// System-level button combinations (variant-aware)
///////////////////////////////

#define BTN_RESUME 			BTN_X                           // Button to resume from save state
#define BTN_SLEEP 			BTN_POWER                       // Button to enter sleep mode
#define BTN_WAKE 			BTN_POWER                       // Button to wake from sleep
#define BTN_MOD_VOLUME 		(is_plus ? BTN_NONE : BTN_SELECT)  // Hold SELECT for volume (standard) or none (Plus)
#define BTN_MOD_BRIGHTNESS 	(is_plus ? BTN_MENU : BTN_START)   // Hold START/MENU for brightness
#define BTN_MOD_PLUS 		(is_plus ? BTN_PLUS : BTN_R1)      // Increase with dedicated button (Plus) or R1
#define BTN_MOD_MINUS 		(is_plus ? BTN_MINUS : BTN_L1)     // Decrease with dedicated button (Plus) or L1

///////////////////////////////
// Display Specifications
// Runtime-configurable for 560p variant
///////////////////////////////

#define FIXED_SCALE 	2                      // 2x scaling factor for UI
#define FIXED_WIDTH		(is_560p?752:640)      // Screen width: 752px (560p) or 640px (standard)
#define FIXED_HEIGHT	(is_560p?560:480)      // Screen height: 560px (560p) or 480px (standard)
#define FIXED_BPP		2                      // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8)        // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)   // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

///////////////////////////////
// UI Layout Configuration
// Adjusted for 560p variant
///////////////////////////////

#define MAIN_ROW_COUNT (is_560p?8:6)  // Number of rows visible: 8 (560p) or 6 (standard)
#define PADDING (is_560p?5:10)        // UI padding: 5px (560p) or 10px (standard)
#define PAGE_SCALE (is_560p?2:3)      // Memory scaling: tighter on 560p to reduce usage

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"   // Path to SD card mount point
#define MUTE_VOLUME_RAW -60         // Raw value for muted volume (negative scale)
#define HAS_NEON                    // ARM NEON SIMD optimizations available

///////////////////////////////

#endif
