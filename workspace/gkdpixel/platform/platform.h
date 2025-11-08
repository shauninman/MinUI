/**
 * gkdpixel/platform/platform.h - Platform definitions for GKD Pixel handheld
 *
 * The GKD Pixel is a compact retro handheld gaming device with:
 * - 320x240 display (QVGA resolution)
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2)
 * - Menu and power buttons
 * - Uses evdev/keyboard input codes (no SDL keycodes or joystick)
 * - Software scaler for video output
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// SDL Keyboard Button Mappings
// GKD Pixel does not use SDL keyboard input
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
// Hardware keycodes from the kernel input subsystem
///////////////////////////////

#define CODE_UP			103  // KEY_UP
#define CODE_DOWN		108  // KEY_DOWN
#define CODE_LEFT		105  // KEY_LEFT
#define CODE_RIGHT		106  // KEY_RIGHT

#define CODE_SELECT		1    // KEY_ESC
#define CODE_START		28   // KEY_ENTER

#define CODE_A			29   // KEY_LEFTCTRL
#define CODE_B			56   // KEY_LEFTALT
#define CODE_X			57   // KEY_SPACE
#define CODE_Y			42   // KEY_LEFTSHIFT

#define CODE_L1			15   // KEY_TAB
#define CODE_R1			14   // KEY_BACKSPACE
#define CODE_L2			104  // KEY_PAGEUP
#define CODE_R2			109  // KEY_PAGEDOWN
#define CODE_L3			CODE_NA
#define CODE_R3			CODE_NA

#define CODE_MENU		102  // KEY_HOME
#define CODE_MENU_ALT	107  // KEY_END (alternate menu button)
#define CODE_POWER		116  // KEY_POWER
#define CODE_POWEROFF	68   // KEY_F10 (power off trigger)

#define CODE_PLUS		78   // KEY_KPPLUS (volume up)
#define CODE_MINUS		74   // KEY_KPMINUS (volume down)

///////////////////////////////
// Joystick Button Mappings
// GKD Pixel does not use joystick input
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

#define FIXED_SCALE 	1              // No scaling factor needed
#define FIXED_WIDTH		320            // Screen width in pixels
#define FIXED_HEIGHT	240            // Screen height in pixels (QVGA)
#define FIXED_BPP		2              // Bytes per pixel (RGB565)
#define FIXED_DEPTH		(FIXED_BPP * 8) // Bit depth (16-bit color)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)  // Row stride in bytes
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT) // Total framebuffer size

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/media/roms"  // Path to SD card mount point
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume
#define USES_SWSCALER              // Platform uses software scaler for video

///////////////////////////////

#endif
