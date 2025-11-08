/**
 * rg35xx/platform/platform.h - Platform definitions for Anbernic RG35XX handheld
 *
 * The Anbernic RG35XX is a popular retro handheld gaming device with:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons with volume controls
 * - Uses hybrid input (SDL keyboard + evdev codes)
 * - NEON SIMD optimization support
 * - Japanese-themed SDL key mappings (SDLK_KATAKANA, etc.)
 *
 * @note SDL keyboard mappings use unusual Japanese input keys as hardware quirk
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// Hardware uses Japanese input keys for button mappings
///////////////////////////////

#define	BUTTON_UP		SDLK_KATAKANA           // Japanese Katakana key
#define	BUTTON_DOWN		SDLK_HIRAGANA           // Japanese Hiragana key
#define	BUTTON_LEFT		SDLK_HENKAN             // Japanese Henkan (convert) key
#define	BUTTON_RIGHT	SDLK_KATAKANAHIRAGANA   // Japanese Katakana/Hiragana toggle

#define	BUTTON_SELECT	SDLK_PRINT              // Print Screen key
#define	BUTTON_START	SDLK_KP_DIVIDE          // Keypad Divide

#define	BUTTON_A		SDLK_MUHENKAN           // Japanese Muhenkan (no-convert) key
#define	BUTTON_B		SDLK_KP_JPCOMMA         // Japanese comma on keypad
#define	BUTTON_X		SDLK_KP_ENTER           // Keypad Enter
#define	BUTTON_Y		SDLK_RCTRL              // Right Control

#define	BUTTON_L1		SDLK_RALT               // Right Alt
#define	BUTTON_R1		SDLK_BREAK              // Break/Pause
#define	BUTTON_L2		SDLK_HOME               // Home key
#define	BUTTON_R2		SDLK_UP                 // Up Arrow (shared with D-pad up)
#define BUTTON_L3 		BUTTON_NA
#define BUTTON_R3 		BUTTON_NA

#define	BUTTON_MENU		SDLK_PAGEUP             // Page Up
#define	BUTTON_POWER	SDLK_POWER              // Power key
#define	BUTTON_PLUS		SDLK_DOWN               // Down Arrow (volume up)
#define	BUTTON_MINUS	SDLK_PAGEDOWN           // Page Down (volume down)

///////////////////////////////
// Evdev/Keyboard Input Codes
// Hardware keycodes from kernel input subsystem
///////////////////////////////

#define CODE_UP			0x5A  // 90 decimal
#define CODE_DOWN		0x5B  // 91 decimal
#define CODE_LEFT		0x5C  // 92 decimal
#define CODE_RIGHT		0x5D  // 93 decimal

#define CODE_SELECT		0x63  // 99 decimal
#define CODE_START		0x62  // 98 decimal

#define CODE_A			0x5E  // 94 decimal
#define CODE_B			0x5F  // 95 decimal
#define CODE_X			0x60  // 96 decimal
#define CODE_Y			0x61  // 97 decimal

#define CODE_L1			0x64  // 100 decimal
#define CODE_R1			0x65  // 101 decimal
#define CODE_L2			0x66  // 102 decimal
#define CODE_R2			0x67  // 103 decimal
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		0x68  // 104 decimal
#define CODE_POWER		0x74  // 116 decimal (KEY_POWER)

#define CODE_PLUS		0x6C  // 108 decimal (volume up)
#define CODE_MINUS		0x6D  // 109 decimal (volume down)

///////////////////////////////
// Joystick Button Mappings
// RG35XX does not use joystick input
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

#define SDCARD_PATH "/mnt/sdcard"  // Path to SD card mount point (lowercase)
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume
#define HAS_NEON                   // ARM NEON SIMD optimizations available

///////////////////////////////

#endif
