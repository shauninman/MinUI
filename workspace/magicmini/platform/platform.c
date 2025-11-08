/**
 * platform.c - MagicX XU Mini M platform implementation
 *
 * Provides hardware abstraction for the MagicX XU Mini M handheld device.
 * This RK3566-based device features a 640x480 display with rotation support,
 * analog sticks (left stick via absolute events), L3/R3 buttons, and advanced
 * visual effects including scanline/grid overlays with DMG color support.
 *
 * Key Features:
 * - Analog stick support (left stick via EV_ABS events)
 * - L3/R3 buttons that also trigger MENU
 * - Display rotation support (portrait mode detection)
 * - Crisp/soft scaling modes with nearest-neighbor upscale
 * - Grid and line effects with configurable DMG color tinting
 * - Brightness-based alpha blending for low brightness compensation
 * - Wi-Fi status tracking via network interface
 *
 * @note This platform uses SDL2 for video and relies on msettings library
 *       for brightness/volume control
 */
// magicmini
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////
// Input Configuration
///////////////////////////////

#define RAW_UP		544
#define RAW_DOWN	545
#define RAW_LEFT	546
#define RAW_RIGHT	547

#define RAW_A		308
#define RAW_B		305
#define RAW_X		307
#define RAW_Y		304

#define RAW_START	315
#define RAW_SELECT	314
#define RAW_MENU	704

#define RAW_L1		310
#define RAW_L2		313
#define RAW_L3		317
#define RAW_R1		311
#define RAW_R2		312
#define RAW_R3		318

#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116

#define RAW_LSY		1
#define RAW_LSX		0
#define RAW_RSY		2
#define RAW_RSX		5

#define RAW_MENU1	RAW_L3
#define RAW_MENU2	RAW_R3

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

///////////////////////////////
// Input Initialization
///////////////////////////////

/**
 * Initializes input devices for the MagicX XU Mini M.
 *
 * Opens three input event devices:
 * - event0: Power button
 * - event2: Gamepad (buttons and analog sticks)
 * - event3: Volume buttons
 *
 * All devices are opened with O_NONBLOCK to prevent blocking reads
 * and O_CLOEXEC to prevent inheritance by child processes.
 */
void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // gamepad
	inputs[2] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // volume
}

/**
 * Closes all input device file descriptors.
 */
void PLAT_quitInput(void) {
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03

/**
 * Updates button state in the pad structure based on press/release events.
 *
 * Manages three button states:
 * - is_pressed: Currently held buttons
 * - just_pressed: Buttons pressed this frame (transient)
 * - just_repeated: Buttons eligible for repeat this frame (transient)
 * - just_released: Buttons released this frame (transient)
 *
 * @param btn Button mask (e.g., BTN_A, BTN_B)
 * @param id Button ID for repeat timing array indexing
 * @param pressed 1 if button pressed, 0 if released
 * @param tick Current tick count for scheduling next repeat
 *
 * @note If btn is BTN_NONE, this function does nothing
 */
static void updateButton(int btn, int id, int pressed, uint32_t tick) {
	if (btn==BTN_NONE) return;

	if (!pressed) {
		pad.is_pressed		&= ~btn; // unset
		pad.just_repeated	&= ~btn; // unset
		pad.just_released	|= btn; // set
	}
	else if ((pad.is_pressed & btn)==BTN_NONE) {
		pad.just_pressed	|= btn; // set
		pad.just_repeated	|= btn; // set
		pad.is_pressed		|= btn; // set
		pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
	}
}

///////////////////////////////
// Input Polling
///////////////////////////////

/**
 * Polls all input devices and updates the global pad state.
 *
 * Reads input events from all three input devices (power, gamepad, volume)
 * and translates raw input codes to button masks. Handles both digital buttons
 * (EV_KEY) and analog sticks (EV_ABS). The left analog stick generates digital
 * button presses via PAD_setAnalog().
 *
 * Special behavior:
 * - L3 and R3 buttons also trigger BTN_MENU
 * - Analog stick values are stored in pad.laxis and pad.raxis
 * - Button repeat timing is automatically handled for held buttons
 *
 * @note Transient state (just_pressed, just_released, just_repeated) is
 *       reset at the start of each poll
 */
void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	// Handle button repeat for held buttons
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// the actual poll
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;

			if (type==EV_KEY) {
				if (value>1) continue; // ignore kernel key repeats (we handle repeats ourselves)
			
				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed);
					 if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 	id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 	id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 	id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_MENU1)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_MENU2)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==RAW_L3)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_R3)		{ btn = BTN_R3; 		id = BTN_ID_R3; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			else if (type==EV_ABS) {
				// Analog stick events - left stick generates digital button presses
				LOG_info("abs event: %i (%i)\n", code,value);
					 if (code==RAW_LSX) { pad.laxis.x = value; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_LSY) { pad.laxis.y = value; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
				else if (code==RAW_RSX) pad.raxis.x = value;
				else if (code==RAW_RSY) pad.raxis.y = value;
			}
			
			if (btn==BTN_NONE) continue;

			updateButton(btn, id, pressed, tick);
			// L3/R3 buttons also trigger MENU button
			if (btn==BTN_L3||btn==BTN_R3) updateButton(BTN_MENU, BTN_ID_MENU, pressed, tick);
		}
	}
}

/**
 * Checks if the device should wake from sleep.
 *
 * Drains the input event queue looking for a power button release event.
 * This is called during sleep mode to detect wake-up triggers.
 *
 * @return 1 if power button was released, 0 otherwise
 */
int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////
// Video System
///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;

	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
} vid;

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0; // Rotation angle (0=landscape, 3=portrait/270 degrees)

/**
 * Initializes the video subsystem for the MagicX XU Mini M.
 *
 * Sets up SDL2 video with hardware acceleration and vsync. Automatically
 * detects display orientation and enables rotation for portrait mode.
 * Creates textures for the main display and optional crisp scaling target.
 *
 * Display modes:
 * - Landscape (640x480): No rotation
 * - Portrait (480x640): 270-degree rotation (rotate=3)
 *
 * @return SDL surface for the screen buffer (640x480)
 *
 * @note This platform always starts with SHARPNESS_SOFT scaling mode
 */
SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	//
	// int num_displays = SDL_GetNumVideoDisplays();
	// LOG_info("SDL_GetNumVideoDisplays(): %i\n", num_displays);
	//
	// LOG_info("Available video drivers:\n");
	// for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetVideoDriver(i));
	// }
	// LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	//
	// LOG_info("Available render drivers:\n");
	// for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
	// 	SDL_RendererInfo info;
	// 	SDL_GetRenderDriverInfo(i,&info);
	// 	LOG_info("- %s\n", info.name);
	// }
	//
	// LOG_info("Available display modes:\n");
	// SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));

	// LOG_info("Available audio drivers:\n");
	// for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetAudioDriver(i));
	// }
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver()); // NOTE: hadn't been selected yet so will always be NULL!

	// SDL_SetHint(SDL_HINT_RENDER_VSYNC,"0"); // ignored?

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	LOG_info("window size: %ix%i\n", w,h);

	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// Auto-detect portrait mode and enable 270-degree rotation
	if (mode.h>mode.w) rotate = 3;
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	// SDL_RenderSetLogicalSize(vid.renderer, w,h); // TODO: wrong, but without and with the below it's even wrong-er
	
	// int renderer_width,renderer_height;
	// SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// LOG_info("output size: %ix%i\n", renderer_width, renderer_height);
	// if (renderer_width!=w) { // I think this can only be hdmi
	// 	float x_scale = (float)renderer_width / w;
	// 	float y_scale = (float)renderer_height / h;
	// 	SDL_SetWindowSize(vid.window, w / x_scale, h / y_scale);
	//
	// 	SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// 	LOG_info("adjusted size: %ix%i\n", renderer_width, renderer_height);
	// 	x_scale = (float)renderer_width / w;
	// 	y_scale = (float)renderer_height / h;
	// 	SDL_RenderSetScale(vid.renderer, x_scale,y_scale);
	//
	// 	// for some reason we need to clear and present
	// 	// after setting the window size or we'll miss
	// 	// the first frame
	// 	SDL_RenderClear(vid.renderer);
	// 	SDL_RenderPresent(vid.renderer);
	// }
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1"); // linear scaling for initial setup
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.texture, SDL_BLENDMODE_BLEND);
	vid.target	= NULL; // only needed for crisp scaling mode
	
	// TODO: doesn't work here
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeLinear); // we always start at device size so use linear for better upscaling over hdmi
	
	// SDL_ScaleMode scale_mode;
	// SDL_GetTextureScaleMode(vid.texture, &scale_mode);
	// LOG_info("texture scale mode: %i\n", scale_mode);
	
	// int format;
	// int access_;
	// SDL_QueryTexture(vid.texture, &format, &access_, NULL,NULL);
	// LOG_info("texture format: %s (streaming: %i)\n", SDL_GetPixelFormatName(format), access_==SDL_TEXTUREACCESS_STREAMING);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}

/**
 * Clears the video output by filling with black and presenting empty frames.
 *
 * @note Presents 3 frames to ensure all buffers are cleared
 */
static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
}

/**
 * Shuts down the video subsystem and frees all resources.
 *
 * Destroys all surfaces, textures, renderer, and window in reverse
 * order of creation. Calls SDL_Quit() to shut down SDL completely.
 */
void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

/**
 * Clears the specified screen surface to black.
 *
 * @param screen Surface to clear
 */
void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}

/**
 * Clears both the screen surface and renderer.
 */
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	SDL_RenderClear(vid.renderer);
}

/**
 * Sets vsync mode (no-op on this platform).
 *
 * @param vsync Ignored - vsync is always enabled via SDL_RENDERER_PRESENTVSYNC
 */
void PLAT_setVsync(int vsync) {
	// Vsync is always enabled via SDL_RENDERER_PRESENTVSYNC flag
}

// Integer upscale factor for crisp mode (nearest-neighbor before linear downscale)
static int hard_scale = 4;

/**
 * Resizes video buffers and textures to match new content dimensions.
 *
 * Recreates the main texture and optional crisp scaling target when content
 * size changes. Automatically determines optimal hard_scale factor based on
 * source resolution to balance quality and performance.
 *
 * Hard scale selection:
 * - Native resolution (>=640x480): 1x (no upscale)
 * - GBA and larger (height>=160): 2x
 * - Smaller systems (GB, NES, etc.): 4x
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param p New pitch in bytes
 *
 * @note Does nothing if dimensions haven't changed
 */
static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	// Determine optimal integer upscale factor for crisp mode
	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient)
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);

	// Set scaling quality: linear for soft, nearest-neighbor for crisp
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.texture, SDL_BLENDMODE_BLEND);

	// Crisp mode: create larger target texture for nearest-neighbor upscale, then linear downscale
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
		SDL_SetTextureBlendMode(vid.target, SDL_BLENDMODE_BLEND);
	}
	else {
		vid.target = NULL;
	}

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

/**
 * Resizes video to new dimensions (public API wrapper).
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param p New pitch in bytes
 * @return Screen surface (unchanged)
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

/**
 * Sets video scale clipping region (no-op on this platform).
 *
 * @param x Clip x position (ignored)
 * @param y Clip y position (ignored)
 * @param width Clip width (ignored)
 * @param height Clip height (ignored)
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// Not implemented on this platform
}

/**
 * Sets nearest-neighbor scaling mode (no-op on this platform).
 *
 * @param enabled Ignored - use PLAT_setSharpness() instead
 */
void PLAT_setNearestNeighbor(int enabled) {
	// Use PLAT_setSharpness() for scaling control
}

/**
 * Sets sharpness mode (soft linear scaling vs crisp nearest-neighbor upscale).
 *
 * Triggers a video resize to recreate textures with new scaling quality.
 * Crisp mode uses nearest-neighbor upscale to hard_scale multiplier,
 * then linear downscale to screen size.
 *
 * @param sharpness SHARPNESS_SOFT or SHARPNESS_CRISP
 */
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0; // Force resize by changing pitch
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}

///////////////////////////////
// Visual Effects System
///////////////////////////////

/**
 * Tracks the current and pending state of visual effects (scanlines/grid).
 *
 * The effect system uses a double-buffered approach: next_* fields hold
 * requested settings, which are applied by updateEffect() when changed.
 * This prevents reloading effect textures unnecessarily.
 */
static struct FX_Context {
	int scale;			// Current pixel scale factor
	int type;			// Current effect type
	int color;			// Current DMG color tint (RGB565)
	int next_scale;		// Requested pixel scale factor
	int next_type;		// Requested effect type
	int next_color;		// Requested DMG color tint
	int live_type;		// Type of currently loaded texture
	int opacity;		// Current effect opacity (0-255)
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
	.color = 0,
	.next_color = 0,
};

/**
 * Converts RGB565 color to RGB888 components.
 *
 * Expands 5-bit red, 6-bit green, and 5-bit blue to full 8-bit channels
 * by shifting and replicating high bits into low bits for smooth gradients.
 *
 * @param rgb565 RGB565 color value
 * @param r Output red component (0-255)
 * @param g Output green component (0-255)
 * @param b Output blue component (0-255)
 */
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Extract the red component (5 bits)
    uint8_t red = (rgb565 >> 11) & 0x1F;
    // Extract the green component (6 bits)
    uint8_t green = (rgb565 >> 5) & 0x3F;
    // Extract the blue component (5 bits)
    uint8_t blue = rgb565 & 0x1F;

    // Scale to 8-bit range by shifting left and replicating high bits
    *r = (red << 3) | (red >> 2);
    *g = (green << 2) | (green >> 4);
    *b = (blue << 3) | (blue >> 2);
}

/**
 * Updates the active effect texture based on pending changes.
 *
 * Loads the appropriate scanline or grid effect texture based on current
 * pixel scale. For grid effects with a DMG color specified, recolors the
 * white pixels to match the DMG palette color for authentic Game Boy look.
 *
 * Effect selection by scale:
 * - LINE: line-2.png through line-8.png based on scale
 * - GRID: grid-2.png through grid-11.png with scale-dependent opacity
 *
 * Opacity tuning:
 * - Grid effects use higher opacity for smaller scales to maintain visibility
 * - Line effects use consistent 50% opacity
 *
 * @note Does nothing if effect settings haven't changed
 * @note DMG color tinting only applies to grid effects
 */
static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type && effect.next_color==effect.color) return; // unchanged
	
	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;
	
	if (effect.type==EFFECT_NONE) return; // disabled
	if (effect.type==effect.live_type && effect.scale==live_scale && effect.color==live_color) return; // already loaded
	
	char* effect_path;
	int opacity = 128; // Default: 1 - 1/2 = 50%
	// Select appropriate effect texture based on type and scale
	if (effect.type==EFFECT_LINE) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/line-2.png";
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/line-3.png";
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/line-4.png";
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/line-5.png";
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/line-6.png";
		}
		else {
			effect_path = RES_PATH "/line-8.png";
		}
	}
	else if (effect.type==EFFECT_GRID) {
		if (effect.scale<3) {
			effect_path = RES_PATH "/grid-2.png";
			opacity = 64; // 1 - 3/4 = 25%
		}
		else if (effect.scale<4) {
			effect_path = RES_PATH "/grid-3.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<5) {
			effect_path = RES_PATH "/grid-4.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else if (effect.scale<6) {
			effect_path = RES_PATH "/grid-5.png";
			opacity = 160; // 1 - 9/25 = ~64%
		}
		else if (effect.scale<8) {
			effect_path = RES_PATH "/grid-6.png";
			opacity = 112; // 1 - 5/9 = ~44%
		}
		else if (effect.scale<11) {
			effect_path = RES_PATH "/grid-8.png";
			opacity = 144; // 1 - 7/16 = ~56%
		}
		else {
			effect_path = RES_PATH "/grid-11.png";
			opacity = 136; // 1 - 57/121 = ~52%
		}
	}
	
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		// Apply DMG color tinting to grid effects
		if (effect.type==EFFECT_GRID && effect.color) {
			uint8_t r,g,b;
			rgb565_to_rgb888(effect.color,&r,&g,&b);

			// Replace all white pixels with DMG palette color, preserving alpha
			uint32_t* pixels = (uint32_t*)tmp->pixels;
			int width = tmp->w;
			int height = tmp->h;
			for (int y = 0; y < height; ++y) {
			    for (int x = 0; x < width; ++x) {
			        uint32_t pixel = pixels[y * width + x];
			        uint8_t _,a;
			        SDL_GetRGBA(pixel, tmp->format, &_, &_, &_, &a);
			        // Recolor non-transparent pixels
			        if (a) pixels[y * width + x] = SDL_MapRGBA(tmp->format, r,g,b, a);
			    }
			}
		}
		
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureBlendMode(vid.effect, SDL_BLENDMODE_BLEND);
		effect.opacity = opacity;
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}

/**
 * Sets the visual effect type for next frame.
 *
 * @param next_type EFFECT_NONE, EFFECT_LINE, or EFFECT_GRID
 */
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}

/**
 * Sets the DMG color tint for grid effects.
 *
 * @param next_color RGB565 color value (e.g., from DMG palette)
 *
 * @note Only affects EFFECT_GRID, ignored for EFFECT_LINE
 */
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}

/**
 * Delays for remaining frame time to maintain consistent frame rate.
 *
 * @param remaining Milliseconds to delay (0 or negative = no delay)
 */
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

/**
 * Gets the appropriate scaler function for the renderer.
 *
 * Updates the effect scale based on renderer scale for proper effect alignment.
 *
 * @param renderer Renderer with scale information
 * @return Scaler function (always scale1x1_c16 on this platform)
 */
scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

/**
 * Prepares to blit a renderer's output to screen.
 *
 * Stores renderer reference and resizes video buffers to match renderer's
 * true dimensions. Clears the renderer in preparation for drawing.
 *
 * @param renderer Renderer to blit
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

/**
 * Combines two alpha values using proper alpha blending math.
 *
 * @param a First alpha value (0-255)
 * @param b Second alpha value (0-255)
 * @return Combined alpha value (0-255)
 */
static uint8_t combine_alpha(uint8_t a, uint8_t b) {
    return (a * b + 255) / 255;
}

/**
 * Flips the video buffer to screen, applying scaling, rotation, and effects.
 *
 * This is the main rendering function that handles:
 * - Brightness-based alpha blending (compensates for low brightness)
 * - Aspect ratio correction (native, cropped, or aspect fit)
 * - Crisp mode rendering (nearest-neighbor upscale, then linear downscale)
 * - Display rotation for portrait mode
 * - Visual effects (scanlines/grid) with opacity blending
 *
 * Brightness-based alpha blending:
 * - At brightness < 5: Applies fading effect to compensate for backlight
 * - Alpha = 255 - (192 - (brightness * 192) / 5)
 * - This prevents content from becoming too dim at low brightness settings
 *
 * @param IGNORED Unused surface parameter (uses vid.screen internally)
 * @param ignored Unused integer parameter
 */
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	// Apply brightness-based alpha compensation for low brightness levels
	int alpha = GetBrightness();
	if (alpha<5) alpha = 255 - (192 - (alpha * 192) / 5);
	else alpha = 255;

	// Simple case: no renderer, just display the screen surface
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH);
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		SDL_SetTextureAlphaMod(vid.texture, alpha);
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){0,device_width,device_width,device_height},rotate*90,NULL,SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}

	// Update texture with renderer's pixel data
	SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);

	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;

	// Crisp mode: render to larger target with nearest-neighbor, then downscale with linear
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_SetTextureAlphaMod(vid.texture, 255);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		// Scale source rect to match target texture size
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};

	// Calculate destination rectangle based on aspect ratio mode
	if (vid.blit->aspect==0) { // Native or cropped (integer scale, centered)
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	else if (vid.blit->aspect>0) { // Aspect fit (scale to fit, maintain aspect ratio)
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	// else aspect < 0: fullscreen stretch (dst_rect already set to full screen)
	
	// Calculate rotation offsets for portrait mode
	int ox,oy;
	oy = (device_width-device_height)/2;
	ox = -oy;

	// Render main content with brightness-based alpha
	SDL_SetTextureAlphaMod(target, alpha);
	if (rotate) SDL_RenderCopyEx(vid.renderer,target,src_rect,&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
	else SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);

	// Apply visual effects (scanlines/grid) if enabled
	updateEffect();
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect) {
		// Align effect to pixel grid
		ox = effect.scale - (dst_rect->x % effect.scale);
		oy = effect.scale - (dst_rect->y % effect.scale);
		if (ox==effect.scale) ox = 0;
		if (oy==effect.scale) oy = 0;

		// Combine brightness alpha with effect opacity for proper layering
		int opacity = combine_alpha(alpha, effect.opacity);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.effect,dst_rect,&(SDL_Rect){oy,ox+device_width,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.effect, dst_rect,dst_rect);
	}

	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

///////////////////////////////
// Overlay (unused on this platform)
///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

/**
 * Initializes overlay surface (unused on this platform).
 *
 * @return Overlay surface
 */
SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}

/**
 * Frees overlay surface.
 */
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}

/**
 * Enables or disables overlay (no-op on this platform).
 *
 * @param enable Ignored
 */
void PLAT_enableOverlay(int enable) {
	// Not implemented on this platform
}

///////////////////////////////
// Power Management
///////////////////////////////

static int online = 0; // Wi-Fi connection status

/**
 * Gets battery charging status and charge level.
 *
 * Reads battery status from sysfs. Charge level is bucketed into 6 levels
 * (10%, 20%, 40%, 60%, 80%, 100%) to reduce UI flicker from minor fluctuations.
 *
 * @param is_charging Output: 1 if charging, 0 if not
 * @param charge Output: Battery percentage (10, 20, 40, 60, 80, or 100)
 *
 * @note Wi-Fi status tracking is currently disabled but could be polled here
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// Bucket charge level to reduce UI flicker from minor fluctuations
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

#define BACKLIGHT_PATH "/sys/class/backlight/backlight/bl_power"

/**
 * Enables or disables the backlight.
 *
 * When disabling, sets brightness to 0 and powers down the backlight.
 * When enabling, restores previous brightness and powers up the backlight.
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(BACKLIGHT_PATH, FB_BLANK_UNBLANK);
	}
	else {
		SetRawBrightness(0);
		system("dd if=/dev/zero of=/dev/fb0"); // Clear framebuffer (may not work on all kernels)
		putInt(BACKLIGHT_PATH, FB_BLANK_POWERDOWN);
	}
}

/**
 * Powers off the device.
 *
 * Performs cleanup sequence:
 * 1. Removes exec flag and syncs filesystem
 * 2. Waits 2 seconds for sync to complete
 * 3. Mutes audio and disables backlight
 * 4. Shuts down subsystems
 * 5. Exits process (system will handle power-off)
 */
void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////
// CPU/GPU Control
///////////////////////////////

#define CPU_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"
#define GPU_PATH "/sys/devices/platform/ff400000.gpu/devfreq/ff400000.gpu/governor"
#define DMC_PATH "/sys/devices/platform/dmc/devfreq/dmc/governor"

/**
 * Sets CPU/GPU/memory frequency based on performance mode.
 *
 * RK3566 frequency settings:
 * - MENU: 600MHz (lowest power for UI)
 * - POWERSAVE: 816MHz (conservative for simple games)
 * - NORMAL: 1.416GHz (balanced for most games)
 * - PERFORMANCE: 2.016GHz (maximum, GPU/DMC also set to performance)
 *
 * @param speed CPU_SPEED_MENU, CPU_SPEED_POWERSAVE, CPU_SPEED_NORMAL,
 *              or CPU_SPEED_PERFORMANCE
 *
 * @note PERFORMANCE mode may not be stable on all chips (depends on binning)
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq =  816000; break;
		case CPU_SPEED_NORMAL: 		freq = 1416000; break;
		case CPU_SPEED_PERFORMANCE: freq = 2016000; break; // not viable on lower binned chips
	}

	// Performance mode: maximize GPU and memory controller
	if (speed==CPU_SPEED_PERFORMANCE) {
		putFile(GPU_PATH, "performance");
		putFile(DMC_PATH, "performance");
	}
	else {
		putFile(GPU_PATH, "simple_ondemand");
		putFile(DMC_PATH, "dmc_ondemand");
	}
	putInt(CPU_PATH, freq);
}

/**
 * Sets rumble/vibration strength (not supported on this platform).
 *
 * @param strength Ignored (no vibration motor)
 */
void PLAT_setRumble(int strength) {
	// Not supported on this platform
}

/**
 * Picks appropriate audio sample rate for the platform.
 *
 * @param requested Requested sample rate
 * @param max Maximum allowed sample rate
 * @return Chosen sample rate (minimum of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Gets the device model name.
 *
 * @return Device model string
 */
char* PLAT_getModel(void) {
	return "MagicX XU Mini M";
}

/**
 * Checks if device is online (Wi-Fi connected).
 *
 * @return 1 if online, 0 if offline
 *
 * @note Currently always returns 0 (Wi-Fi status polling is disabled)
 */
int PLAT_isOnline(void) {
	return online;
}