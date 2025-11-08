/**
 * my355/platform/platform.h - Platform definitions for MY355 handheld
 *
 * The MY355 is a retro handheld gaming device with:
 * - 640x480 display (VGA resolution, 2x scaled)
 * - 1280x720 HDMI output support
 * - D-pad and face buttons (A/B/X/Y)
 * - Shoulder buttons (L1/R1/L2/R2) with L3/R3 support
 * - Analog sticks (left and right)
 * - Menu and power buttons with volume controls
 * - Uses evdev input codes
 * - Runtime HDMI detection
 */

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////
// Dependencies
///////////////////////////////

#include "sdl.h"

///////////////////////////////
// Platform Variant Detection
// Runtime variables for HDMI output
///////////////////////////////

extern int on_hdmi;  // Set to 1 when HDMI output is active

///////////////////////////////
// SDL Keyboard Button Mappings
// MY355 does not use SDL keyboard input
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

#define CODE_UP			82   // HID Up Arrow
#define CODE_DOWN		81   // HID Down Arrow
#define CODE_LEFT		80   // HID Left Arrow
#define CODE_RIGHT		79   // HID Right Arrow

#define CODE_SELECT		228  // HID Left GUI
#define CODE_START		40   // HID Enter

#define CODE_A			44   // HID Space
#define CODE_B			224  // HID Left Control
#define CODE_X			225  // HID Left Shift
#define CODE_Y			226  // HID Left Alt

#define CODE_L1			43   // HID Tab
#define CODE_R1			42   // HID Backspace
#define CODE_L2			75   // HID Page Up
#define CODE_R2			78   // HID Page Down
#define CODE_L3			230  // HID Left Alt (alt mapping)
#define CODE_R3			229  // HID Right Shift

#define CODE_MENU		41   // HID Escape
#define CODE_POWER		102  // KEY_HOME

#define CODE_PLUS		128  // Volume up
#define CODE_MINUS		129  // Volume down

///////////////////////////////
// Joystick Button Mappings
// MY355 does not use joystick input
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
// Analog Stick Axis Mappings
// Hardware analog stick axes
///////////////////////////////

#define AXIS_LX	0  // Left stick X-axis
#define AXIS_LY	1  // Left stick Y-axis
#define AXIS_RX	4  // Right stick X-axis
#define AXIS_RY	3  // Right stick Y-axis

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
// Adjusted for HDMI output
///////////////////////////////

#define MAIN_ROW_COUNT (on_hdmi?8:6)  // Number of rows visible: 8 (HDMI) or 6 (LCD)
#define PADDING (on_hdmi?40:10)       // UI padding: 40px (HDMI) or 10px (LCD)

///////////////////////////////
// Platform-Specific Paths and Settings
///////////////////////////////

#define SDCARD_PATH "/mnt/SDCARD"  // Path to SD card mount point
#define MUTE_VOLUME_RAW 0          // Raw value for muted volume
// #define HAS_NEON                // NEON support commented out (not available on this SoC)
#define SAMPLES 400                // Audio buffer size (helps reduce fceumm audio underruns)

///////////////////////////////

#endif
