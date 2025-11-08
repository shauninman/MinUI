/**
 * api.h - Platform abstraction API for MinUI
 *
 * Provides a unified interface for graphics (GFX_*), sound (SND_*), input (PAD_*),
 * power management (PWR_*), and platform-specific functionality (PLAT_*).
 *
 * This header defines the public API used by minui (launcher) and minarch (emulator).
 * Each platform implements the PLAT_* functions in their workspace/<platform>/ directory,
 * while common GFX_/SND_/PAD_/PWR_ functions are in workspace/all/common/api.c.
 */

#ifndef __API_H__
#define __API_H__
#include "defines.h"
#include "platform.h"
#include "scaler.h"
#include "sdl.h"

///////////////////////////////
// Logging
///////////////////////////////

/**
 * Log severity levels.
 */
enum {
	LOG_DEBUG = 0, // Detailed debug information
	LOG_INFO, // General informational messages
	LOG_WARN, // Warning messages
	LOG_ERROR, // Error messages
};

/**
 * Convenience macros for logging at different severity levels.
 */
#define LOG_debug(fmt, ...) LOG_note(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_info(fmt, ...) LOG_note(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_warn(fmt, ...) LOG_note(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_error(fmt, ...) LOG_note(LOG_ERROR, fmt, ##__VA_ARGS__)

/**
 * Logs a formatted message at the specified severity level.
 *
 * @param level Log severity (LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void LOG_note(int level, const char* fmt, ...);

///////////////////////////////
// Video page buffer constants
///////////////////////////////

/**
 * Double-buffered video system configuration.
 *
 * MinUI uses overscaled page buffers (FIXED_WIDTH * PAGE_SCALE) to support
 * various scaling operations. This allows UI elements to be drawn at high
 * resolution before scaling down to the physical screen.
 */
#define PAGE_COUNT 2
#ifndef PAGE_SCALE
#define PAGE_SCALE 3 // Default 3x overscaling
#endif
#define PAGE_WIDTH (FIXED_WIDTH * PAGE_SCALE)
#define PAGE_HEIGHT (FIXED_HEIGHT * PAGE_SCALE)
#define PAGE_PITCH (PAGE_WIDTH * FIXED_BPP)
#define PAGE_SIZE (PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////

/**
 * Platform-specific page buffer configuration.
 *
 * Optionally defined in platform.h for platforms requiring different
 * pixel formats (e.g., RGB888 instead of RGB565).
 */
// TODO: these only seem to be used by a tmp.pak in trimui (model s)
// used by minarch, optionally defined in platform.h
#ifndef PLAT_PAGE_BPP
#define PLAT_PAGE_BPP FIXED_BPP
#endif
#define PLAT_PAGE_DEPTH (PLAT_PAGE_BPP * 8)
#define PLAT_PAGE_PITCH (PAGE_WIDTH * PLAT_PAGE_BPP)
#define PLAT_PAGE_SIZE (PLAT_PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////
// SDL pixel format masks
///////////////////////////////

/**
 * RGBA channel masks for SDL_CreateRGBSurface.
 *
 * RGBA_MASK_AUTO: Let SDL determine format automatically
 * RGBA_MASK_565: RGB565 (16-bit, 5-6-5 bits per channel)
 * RGBA_MASK_8888: RGBA8888 (32-bit, 8 bits per channel)
 */
#define RGBA_MASK_AUTO 0x0, 0x0, 0x0, 0x0
#define RGBA_MASK_565 0xF800, 0x07E0, 0x001F, 0x0000
#define RGBA_MASK_8888 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000

///////////////////////////////
// Platform abstraction
///////////////////////////////

/**
 * Marks a function as a weak fallback implementation.
 *
 * If a platform doesn't provide its own PLAT_* implementation, the weak
 * fallback (if available) will be used instead.
 */
#define FALLBACK_IMPLEMENTATION                                                                    \
	__attribute__((weak)) // used if platform doesn't provide an implementation

///////////////////////////////
// UI colors
///////////////////////////////

/**
 * Standard UI color values (platform-specific pixel format).
 *
 * These are initialized at runtime based on the platform's pixel format.
 */
extern uint32_t RGB_WHITE;
extern uint32_t RGB_BLACK;
extern uint32_t RGB_LIGHT_GRAY;
extern uint32_t RGB_GRAY;
extern uint32_t RGB_DARK_GRAY;

///////////////////////////////
// Graphics assets
///////////////////////////////

/**
 * Pre-loaded graphics assets used by the UI.
 *
 * These assets are loaded from the .system/res/ directory at startup
 * and stored as SDL surfaces for efficient blitting.
 */
enum {
	ASSET_WHITE_PILL, // Rounded rectangle (white)
	ASSET_BLACK_PILL, // Rounded rectangle (black)
	ASSET_DARK_GRAY_PILL, // Rounded rectangle (dark gray)
	ASSET_OPTION, // Option indicator
	ASSET_BUTTON, // Button background
	ASSET_PAGE_BG, // Page background
	ASSET_STATE_BG, // State indicator background
	ASSET_PAGE, // Page indicator
	ASSET_BAR, // Progress bar
	ASSET_BAR_BG, // Progress bar background
	ASSET_BAR_BG_MENU, // Menu progress bar background
	ASSET_UNDERLINE, // Text underline
	ASSET_DOT, // Dot indicator
	ASSET_HOLE, // Hole/cutout graphic

	ASSET_COLORS, // Marker: end of solid color assets

	ASSET_BRIGHTNESS, // Brightness icon
	ASSET_VOLUME_MUTE, // Muted volume icon
	ASSET_VOLUME, // Volume icon
	ASSET_BATTERY, // Battery outline
	ASSET_BATTERY_LOW, // Low battery outline (red)
	ASSET_BATTERY_FILL, // Battery fill indicator
	ASSET_BATTERY_FILL_LOW, // Low battery fill (red)
	ASSET_BATTERY_BOLT, // Charging bolt icon

	ASSET_SCROLL_UP, // Scroll up indicator
	ASSET_SCROLL_DOWN, // Scroll down indicator

	ASSET_WIFI, // WiFi icon

	ASSET_COUNT, // Total number of assets
};

/**
 * TrueType fonts used throughout the UI.
 */
typedef struct GFX_Fonts {
	TTF_Font* large; // Menu items (16pt)
	TTF_Font* medium; // Single character button labels (14pt)
	TTF_Font* small; // Button hints (12pt)
	TTF_Font* tiny; // Multi-character button labels (10pt)
} GFX_Fonts;
extern GFX_Fonts font;

/**
 * Video sharpness settings for scaling.
 */
enum {
	SHARPNESS_SHARP, // Nearest-neighbor (pixelated)
	SHARPNESS_CRISP, // Slightly smoothed
	SHARPNESS_SOFT, // Bilinear filtering
};

/**
 * CRT-style visual effects.
 */
enum {
	EFFECT_NONE, // No effect
	EFFECT_LINE, // Horizontal scanlines
	EFFECT_GRID, // Grid pattern
	EFFECT_COUNT,
};

/**
 * Rendering context for video scaling operations.
 *
 * Used by minarch to configure how emulator video output is scaled and
 * positioned on the physical screen.
 */
typedef struct GFX_Renderer {
	void* src; // Source surface pixel data
	void* dst; // Destination surface pixel data
	void* blit; // Blit surface for intermediate operations
	double aspect; // 0=integer scale, -1=fullscreen, >0=aspect ratio for SDL2 accelerated scaling
	int scale; // Integer scale factor

	// TODO: document this better
	int true_w; // True source width
	int true_h; // True source height

	int src_x; // Source rectangle X offset
	int src_y; // Source rectangle Y offset
	int src_w; // Source rectangle width
	int src_h; // Source rectangle height
	int src_p; // Source pitch (bytes per scanline)

	// TODO: I think this is overscaled
	int dst_x; // Destination rectangle X offset
	int dst_y; // Destination rectangle Y offset
	int dst_w; // Destination rectangle width
	int dst_h; // Destination rectangle height
	int dst_p; // Destination pitch (bytes per scanline)
} GFX_Renderer;

///////////////////////////////
// Graphics (GFX) API
///////////////////////////////

/**
 * Video mode for initialization.
 */
enum {
	MODE_MAIN, // Main UI/launcher mode
	MODE_MENU, // In-game menu mode
};

/**
 * Initializes the graphics subsystem.
 *
 * @param mode Video mode (MODE_MAIN or MODE_MENU)
 * @return SDL surface for the screen
 */
SDL_Surface* GFX_init(int mode);

/**
 * Resizes video output to specified dimensions.
 * @param w Width in pixels
 * @param h Height in pixels
 * @param pitch Pitch in bytes
 */
#define GFX_resize PLAT_resizeVideo

/**
 * Sets clipping rectangle for scaled video output.
 * @param x X offset
 * @param y Y offset
 * @param width Width in pixels
 * @param height Height in pixels
 */
#define GFX_setScaleClip PLAT_setVideoScaleClip

/**
 * Enables/disables nearest-neighbor filtering.
 * @param enabled 1 to enable, 0 to disable
 */
#define GFX_setNearestNeighbor PLAT_setNearestNeighbor

/**
 * Sets video sharpness level.
 * @param sharpness SHARPNESS_SHARP, SHARPNESS_CRISP, or SHARPNESS_SOFT
 */
#define GFX_setSharpness PLAT_setSharpness

/**
 * Sets color tint for CRT effects.
 * @param color RGB color value
 */
#define GFX_setEffectColor PLAT_setEffectColor

/**
 * Sets CRT-style visual effect.
 * @param effect EFFECT_NONE, EFFECT_LINE, or EFFECT_GRID
 */
#define GFX_setEffect PLAT_setEffect

/**
 * Switches video mode (main UI vs in-game menu).
 *
 * @param mode MODE_MAIN or MODE_MENU
 */
void GFX_setMode(int mode);

/**
 * Checks if HDMI connection state has changed.
 *
 * @return 1 if HDMI state changed, 0 otherwise
 */
int GFX_hdmiChanged(void);

/**
 * Clears the screen surface to black.
 * @param screen SDL surface to clear
 */
#define GFX_clear PLAT_clearVideo

/**
 * Clears all video buffers (front and back).
 */
#define GFX_clearAll PLAT_clearAll

/**
 * Starts a new frame for rendering.
 *
 * Call this at the beginning of each frame before drawing operations.
 */
void GFX_startFrame(void);

/**
 * Flips the back buffer to the screen.
 *
 * @param screen SDL surface to flip
 */
void GFX_flip(SDL_Surface* screen);

/**
 * Checks if platform supports overscan adjustment.
 * @return 1 if supported, 0 otherwise
 */
#define GFX_supportsOverscan PLAT_supportsOverscan

/**
 * Maintains 60fps timing when not calling GFX_flip() this frame.
 *
 * Use this to keep consistent frame timing during pause/sleep.
 */
void GFX_sync(void);

/**
 * Shuts down the graphics subsystem.
 */
void GFX_quit(void);

/**
 * VSync modes for frame pacing.
 */
enum {
	VSYNC_OFF = 0, // No frame pacing
	VSYNC_LENIENT, // Default, allows slight timing variance
	VSYNC_STRICT, // Strict 60fps timing
};

/**
 * Gets current VSync mode.
 *
 * @return VSYNC_OFF, VSYNC_LENIENT, or VSYNC_STRICT
 */
int GFX_getVsync(void);

/**
 * Sets VSync mode.
 *
 * @param vsync VSYNC_OFF, VSYNC_LENIENT, or VSYNC_STRICT
 */
void GFX_setVsync(int vsync);

/**
 * Truncates text to fit within maximum width, adding ellipsis if needed.
 *
 * @param font Font to use for measuring
 * @param in_name Input text
 * @param out_name Output buffer for truncated text
 * @param max_width Maximum width in pixels
 * @param padding Additional padding to account for
 * @return Final width of truncated text in pixels
 */
int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width,
                     int padding);

/**
 * Wraps text to fit within maximum width and line count.
 *
 * @param font Font to use for measuring
 * @param str String to wrap (modified in place)
 * @param max_width Maximum width in pixels
 * @param max_lines Maximum number of lines
 * @return Number of lines after wrapping
 */
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines);

/**
 * Gets the appropriate scaler function for a renderer configuration.
 * @param renderer Rendering context
 * @return Scaler function pointer
 */
#define GFX_getScaler PLAT_getScaler

/**
 * Blits a renderer's output to the screen.
 * @param renderer Rendering context
 */
#define GFX_blitRenderer PLAT_blitRenderer

/**
 * Gets anti-aliased scaler for smooth scaling operations.
 *
 * @param renderer Rendering context
 * @return Anti-aliased scaler function pointer
 */
scaler_t GFX_getAAScaler(const GFX_Renderer* renderer);

/**
 * Frees resources allocated for anti-aliased scaling.
 */
void GFX_freeAAScaler(void);

/**
 * Blits a graphics asset to the destination surface.
 *
 * @param asset Asset ID (ASSET_* enum)
 * @param src_rect Source rectangle (NULL for full asset)
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 *
 * @note All dimensions should be pre-scaled by FIXED_SCALE
 */
void GFX_blitAsset(int asset, const SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect);

/**
 * Blits a pill-shaped background (rounded rectangle).
 *
 * @param asset ASSET_WHITE_PILL, ASSET_BLACK_PILL, or ASSET_DARK_GRAY_PILL
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 */
void GFX_blitPill(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect);

/**
 * Blits a rectangular asset (stretched to fit destination).
 *
 * @param asset Asset ID
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 */
void GFX_blitRect(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect);

/**
 * Blits the battery indicator (outline, fill, and bolt if charging).
 *
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 */
void GFX_blitBattery(SDL_Surface* dst, const SDL_Rect* dst_rect);

/**
 * Calculates the width of a button with hint text.
 *
 * @param hint Button hint text (e.g., "Resume")
 * @param button Button label (e.g., "A")
 * @return Width in scaled pixels
 */
int GFX_getButtonWidth(char* hint, char* button);

/**
 * Blits a button with hint text and label.
 *
 * @param hint Button hint text
 * @param button Button label (single or multi-character)
 * @param dst Destination surface
 * @param dst_rect Destination rectangle (updated with actual size)
 */
void GFX_blitButton(char* hint, char* button, SDL_Surface* dst, SDL_Rect* dst_rect);

/**
 * Blits centered message text.
 *
 * @param font Font to use
 * @param msg Message text
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 */
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, const SDL_Rect* dst_rect);

/**
 * Blits hardware status indicators (brightness, volume, battery).
 *
 * @param dst Destination surface
 * @param show_setting Currently active setting (0=none, 1=brightness, 2=volume)
 * @return Width of the indicator group in pixels
 */
int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting);

/**
 * Blits hardware adjustment hints (+ - buttons).
 *
 * @param dst Destination surface
 * @param show_setting Currently active setting (1=brightness, 2=volume)
 */
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting);

/**
 * Blits a group of button hints.
 *
 * @param hints Array of hint strings (NULL-terminated)
 * @param primary Index of primary (highlighted) button
 * @param dst Destination surface
 * @param align_right 1 to align right, 0 to align left
 * @return Width of the button group in pixels
 */
int GFX_blitButtonGroup(char** hints, int primary, SDL_Surface* dst, int align_right);

/**
 * Calculates the size of rendered text.
 *
 * @param font Font to use
 * @param str Text to measure
 * @param leading Line spacing in pixels
 * @param w Output: width in pixels (may be NULL)
 * @param h Output: height in pixels (may be NULL)
 */
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h);

/**
 * Blits multi-line text with custom leading.
 *
 * @param font Font to use
 * @param str Text to render (may contain newlines)
 * @param leading Line spacing in pixels
 * @param color Text color
 * @param dst Destination surface
 * @param dst_rect Destination rectangle
 */
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst,
                  SDL_Rect* dst_rect);

///////////////////////////////
// Sound (SND) API
///////////////////////////////

/**
 * Stereo audio frame (left and right channels).
 */
typedef struct SND_Frame {
	int16_t left; // Left channel sample (-32768 to 32767)
	int16_t right; // Right channel sample (-32768 to 32767)
} SND_Frame;

/**
 * Initializes the audio subsystem.
 *
 * @param sample_rate Audio sample rate in Hz (e.g., 44100.0)
 * @param frame_rate Video frame rate in Hz (e.g., 60.0)
 */
void SND_init(double sample_rate, double frame_rate);

/**
 * Submits a batch of audio samples for playback.
 *
 * @param frames Array of stereo audio frames
 * @param frame_count Number of frames in the array
 * @return Number of frames actually queued
 */
size_t SND_batchSamples(const SND_Frame* frames, size_t frame_count);

/**
 * Shuts down the audio subsystem.
 */
void SND_quit(void);

///////////////////////////////
// Lid sensor (LID) API
///////////////////////////////

/**
 * Lid sensor state for devices with flip covers.
 */
typedef struct LID_Context {
	int has_lid; // 1 if device has a lid sensor, 0 otherwise
	int is_open; // 1 if lid is open, 0 if closed
} LID_Context;
extern LID_Context lid;

/**
 * Initializes lid sensor (if available).
 */
void PLAT_initLid(void);

/**
 * Checks if lid state has changed.
 *
 * @param state Output: new lid state (1=open, 0=closed)
 * @return 1 if state changed, 0 otherwise
 */
int PLAT_lidChanged(int* state);

///////////////////////////////
// Input/Gamepad (PAD) API
///////////////////////////////

/**
 * Analog stick axis values.
 */
typedef struct PAD_Axis {
	int x; // X-axis value (-32768 to 32767)
	int y; // Y-axis value (-32768 to 32767)
} PAD_Axis;

/**
 * Input state tracking context.
 */
typedef struct PAD_Context {
	int is_pressed; // Bitmask of currently pressed buttons
	int just_pressed; // Bitmask of buttons pressed this frame
	int just_released; // Bitmask of buttons released this frame
	int just_repeated; // Bitmask of buttons auto-repeated this frame
	uint32_t repeat_at[BTN_ID_COUNT]; // Timestamp for next repeat per button
	PAD_Axis laxis; // Left analog stick state
	PAD_Axis raxis; // Right analog stick state
} PAD_Context;
extern PAD_Context pad;

/**
 * Auto-repeat timing constants.
 */
#define PAD_REPEAT_DELAY 300 // Milliseconds before first repeat
#define PAD_REPEAT_INTERVAL 100 // Milliseconds between repeats

/**
 * Initializes the input subsystem.
 */
#define PAD_init PLAT_initInput

/**
 * Shuts down the input subsystem.
 */
#define PAD_quit PLAT_quitInput

/**
 * Polls input devices and updates PAD_Context.
 * Call this once per frame before checking button states.
 */
#define PAD_poll PLAT_pollInput

/**
 * Checks if any input should wake the device from sleep.
 * @return 1 if device should wake, 0 otherwise
 */
#define PAD_wake PLAT_shouldWake

/**
 * Sets analog stick state (internal use by platform implementations).
 *
 * @param neg Negative direction button bit
 * @param pos Positive direction button bit
 * @param value Axis value
 * @param repeat_at Timestamp for next auto-repeat
 */
void PAD_setAnalog(int neg, int pos, int value, int repeat_at);

/**
 * Resets all button states to unpressed.
 */
void PAD_reset(void);

/**
 * Checks if any button was just pressed this frame.
 *
 * @return 1 if any button pressed, 0 otherwise
 */
int PAD_anyJustPressed(void);

/**
 * Checks if any button is currently held.
 *
 * @return 1 if any button pressed, 0 otherwise
 */
int PAD_anyPressed(void);

/**
 * Checks if any button was just released this frame.
 *
 * @return 1 if any button released, 0 otherwise
 */
int PAD_anyJustReleased(void);

/**
 * Checks if a specific button was just pressed this frame.
 *
 * @param btn Button bitmask (BTN_A, BTN_B, etc.)
 * @return 1 if button was just pressed, 0 otherwise
 */
int PAD_justPressed(int btn);

/**
 * Checks if a specific button is currently held.
 *
 * @param btn Button bitmask (BTN_A, BTN_B, etc.)
 * @return 1 if button is pressed, 0 otherwise
 */
int PAD_isPressed(int btn);

/**
 * Checks if a specific button was just released this frame.
 *
 * @param btn Button bitmask (BTN_A, BTN_B, etc.)
 * @return 1 if button was just released, 0 otherwise
 */
int PAD_justReleased(int btn);

/**
 * Checks if a specific button auto-repeated this frame.
 *
 * @param btn Button bitmask (BTN_A, BTN_B, etc.)
 * @return 1 if button auto-repeated, 0 otherwise
 */
int PAD_justRepeated(int btn);

/**
 * Detects a quick tap of the menu button.
 *
 * Returns 1 if BTN_MENU was released within 250ms and BTN_PLUS/BTN_MINUS
 * haven't been pressed (to distinguish from menu+volume combos).
 *
 * @param now Current timestamp in milliseconds
 * @return 1 if menu was tapped, 0 otherwise
 */
int PAD_tappedMenu(uint32_t now);

///////////////////////////////
// Vibration (VIB) API
///////////////////////////////

/**
 * Initializes the vibration subsystem.
 */
void VIB_init(void);

/**
 * Shuts down the vibration subsystem.
 */
void VIB_quit(void);

/**
 * Sets vibration motor strength.
 *
 * @param strength Vibration strength (0=off, platform-specific maximum)
 */
void VIB_setStrength(int strength);

/**
 * Gets current vibration strength setting.
 *
 * @return Current strength value
 */
int VIB_getStrength(void);

///////////////////////////////
// Power management (PWR) API
///////////////////////////////

/**
 * Button label for brightness adjustment.
 */
#define BRIGHTNESS_BUTTON_LABEL "+ -"

/**
 * Callback function type for sleep/wake events.
 */
typedef void (*PWR_callback_t)(void);

/**
 * Initializes the power management subsystem.
 */
void PWR_init(void);

/**
 * Shuts down the power management subsystem.
 */
void PWR_quit(void);

/**
 * Enables/disables low battery warning display.
 *
 * @param enable 1 to enable warning, 0 to disable
 */
void PWR_warn(int enable);

/**
 * Checks if a button press should be ignored when adjusting settings.
 *
 * Used to prevent menu navigation while adjusting brightness/volume.
 *
 * @param btn Button that was pressed
 * @param show_setting Currently active setting (1=brightness, 2=volume)
 * @return 1 if button should be ignored, 0 otherwise
 */
int PWR_ignoreSettingInput(int btn, int show_setting);

/**
 * Updates power management state (handles sleep, settings, battery).
 *
 * Call this once per frame in the main loop.
 *
 * @param dirty Output: set to 1 if screen needs redraw
 * @param show_setting Input/Output: currently active setting (0=none, 1=brightness, 2=volume)
 * @param before_sleep Callback to invoke before entering sleep
 * @param after_sleep Callback to invoke after waking from sleep
 */
void PWR_update(int* dirty, int* show_setting, PWR_callback_t before_sleep,
                PWR_callback_t after_sleep);

/**
 * Disables power-off functionality (used during critical operations).
 */
void PWR_disablePowerOff(void);

/**
 * Powers off the device.
 */
void PWR_powerOff(void);

/**
 * Checks if device is in the process of powering off.
 *
 * @return 1 if powering off, 0 otherwise
 */
int PWR_isPoweringOff(void);

/**
 * Enters a low-power state without actually sleeping.
 *
 * Used to reduce power consumption during idle periods.
 */
void PWR_fauxSleep(void);

/**
 * Disables sleep functionality (prevents auto-sleep and sleep button).
 */
void PWR_disableSleep(void);

/**
 * Re-enables sleep functionality.
 */
void PWR_enableSleep(void);

/**
 * Disables automatic sleep timer.
 */
void PWR_disableAutosleep(void);

/**
 * Re-enables automatic sleep timer.
 */
void PWR_enableAutosleep(void);

/**
 * Checks if autosleep should be prevented this frame.
 *
 * @return 1 if autosleep should be prevented, 0 otherwise
 */
int PWR_preventAutosleep(void);

/**
 * Checks if device is currently charging.
 *
 * @return 1 if charging, 0 otherwise
 */
int PWR_isCharging(void);

/**
 * Gets current battery charge percentage.
 *
 * @return Battery charge (0, 10, 20, 40, 60, 80, or 100)
 */
int PWR_getBattery(void);

/**
 * CPU speed presets for power management.
 */
enum {
	CPU_SPEED_MENU, // Low speed for menu navigation
	CPU_SPEED_POWERSAVE, // Reduced speed for battery saving
	CPU_SPEED_NORMAL, // Default speed
	CPU_SPEED_PERFORMANCE, // Maximum speed for demanding games
};

/**
 * Sets CPU clock speed.
 * @param speed CPU_SPEED_* enum value
 */
#define PWR_setCPUSpeed PLAT_setCPUSpeed

///////////////////////////////
// Platform implementation (PLAT) functions
///////////////////////////////

/**
 * Platform-specific input initialization.
 */
void PLAT_initInput(void);

/**
 * Platform-specific input cleanup.
 */
void PLAT_quitInput(void);

/**
 * Platform-specific input polling.
 */
void PLAT_pollInput(void);

/**
 * Platform-specific wake detection.
 *
 * @return 1 if device should wake from sleep, 0 otherwise
 */
int PLAT_shouldWake(void);

/**
 * Platform-specific video initialization.
 *
 * @return SDL surface for the screen
 */
SDL_Surface* PLAT_initVideo(void);

/**
 * Platform-specific video cleanup.
 */
void PLAT_quitVideo(void);

/**
 * Platform-specific screen clearing.
 *
 * @param screen SDL surface to clear
 */
void PLAT_clearVideo(SDL_Surface* screen);

/**
 * Platform-specific clearing of all video buffers.
 */
void PLAT_clearAll(void);

/**
 * Platform-specific VSync configuration.
 *
 * @param vsync VSync mode
 */
void PLAT_setVsync(int vsync);

/**
 * Platform-specific video mode change.
 *
 * @param w Width in pixels
 * @param h Height in pixels
 * @param pitch Pitch in bytes
 * @return New SDL surface
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch);

/**
 * Platform-specific video scaling clip region.
 *
 * @param x X offset
 * @param y Y offset
 * @param width Width in pixels
 * @param height Height in pixels
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height);

/**
 * Platform-specific nearest-neighbor filtering control.
 *
 * @param enabled 1 to enable, 0 to disable
 */
void PLAT_setNearestNeighbor(int enabled);

/**
 * Platform-specific sharpness control.
 *
 * @param sharpness Sharpness level
 */
void PLAT_setSharpness(int sharpness);

/**
 * Platform-specific effect color control.
 *
 * @param color RGB color value
 */
void PLAT_setEffectColor(int color);

/**
 * Platform-specific visual effect control.
 *
 * @param effect Effect type
 */
void PLAT_setEffect(int effect);

/**
 * Platform-specific VSync wait.
 *
 * @param remaining Microseconds remaining in frame
 */
void PLAT_vsync(int remaining);

/**
 * Platform-specific scaler selection.
 *
 * @param renderer Rendering context
 * @return Appropriate scaler function
 */
scaler_t PLAT_getScaler(GFX_Renderer* renderer);

/**
 * Platform-specific renderer blitting.
 *
 * @param renderer Rendering context
 */
void PLAT_blitRenderer(GFX_Renderer* renderer);

/**
 * Platform-specific screen flip.
 *
 * @param screen SDL surface to flip
 * @param sync 1 to wait for VSync, 0 to flip immediately
 */
void PLAT_flip(SDL_Surface* screen, int sync);

/**
 * Platform-specific overscan support check.
 *
 * @return 1 if platform supports overscan adjustment, 0 otherwise
 */
int PLAT_supportsOverscan(void);

/**
 * Platform-specific overlay initialization (for on-screen indicators).
 *
 * @return SDL surface for overlay
 */
SDL_Surface* PLAT_initOverlay(void);

/**
 * Platform-specific overlay cleanup.
 */
void PLAT_quitOverlay(void);

/**
 * Platform-specific overlay enable/disable.
 *
 * @param enable 1 to enable overlay, 0 to disable
 */
void PLAT_enableOverlay(int enable);

/**
 * Battery charge threshold for low battery warning.
 */
#define PWR_LOW_CHARGE 10

/**
 * Platform-specific battery status query.
 *
 * @param is_charging Output: 1 if charging, 0 otherwise
 * @param charge Output: battery percentage (0, 10, 20, 40, 60, 80, or 100)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge);

/**
 * Platform-specific backlight control.
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable);

/**
 * Platform-specific power off implementation.
 */
void PLAT_powerOff(void);

/**
 * Platform-specific CPU speed control.
 *
 * @param speed CPU_SPEED_* enum value
 */
void PLAT_setCPUSpeed(int speed);

/**
 * Platform-specific rumble/vibration control.
 *
 * @param strength Rumble strength (0 to disable)
 */
void PLAT_setRumble(int strength);

/**
 * Platform-specific audio sample rate selection.
 *
 * Picks the best available sample rate for the platform.
 *
 * @param requested Requested sample rate
 * @param max Maximum acceptable sample rate
 * @return Actual sample rate to use
 */
int PLAT_pickSampleRate(int requested, int max);

/**
 * Gets platform model identifier string.
 *
 * @return Model name (e.g., "Miyoo Mini", "RG35XX")
 */
char* PLAT_getModel(void);

/**
 * Checks if device is connected to a network.
 *
 * @return 1 if online, 0 otherwise
 */
int PLAT_isOnline(void);

/**
 * Sets system date and time.
 *
 * @param y Year
 * @param m Month (1-12)
 * @param d Day (1-31)
 * @param h Hour (0-23)
 * @param i Minute (0-59)
 * @param s Second (0-59)
 * @return 0 on success, -1 on failure
 */
int PLAT_setDateTime(int y, int m, int d, int h, int i, int s);

#endif
