/**
 * platform.c - Platform implementation for Miyoo Flip (MY355)
 *
 * Implements the hardware abstraction layer for the Miyoo Flip device,
 * featuring:
 * - Hall sensor lid detection (lid open/close events)
 * - HDMI output detection and handling
 * - Display rotation (disabled when using HDMI)
 * - WiFi status monitoring
 * - Rumble support (disabled when using HDMI)
 * - Sharpness scaling (crisp/soft) with multi-pass rendering
 * - Overlay effects (scanlines/grids) with configurable scales
 *
 * Hardware specifics:
 * - Built-in screen: 640x480 display (rotatable)
 * - HDMI output: 720x720 (no rotation)
 * - Hall sensor for lid detection at /sys/devices/platform/hall-mh248/hallvalue
 * - Rumble motor controlled via GPIO20
 */

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

// Tracks whether HDMI is currently enabled
// Updated on every flip to detect hotplug events
// When enabled: disables rotation and rumble, changes resolution to HDMI_WIDTH x HDMI_HEIGHT
int on_hdmi = 0;

///////////////////////////////
// Lid Detection (Hall Sensor)
///////////////////////////////

// Hall sensor path: reports 1 when lid is open, 0 when closed
#define LID_PATH "/sys/devices/platform/hall-mh248/hallvalue"

/**
 * Initializes lid detection hardware.
 *
 * Checks for the presence of the hall sensor sysfs interface
 * to determine if the device supports lid detection.
 * Updates the global lid.has_lid flag accordingly.
 */
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}

/**
 * Checks if lid state has changed since last call.
 *
 * Polls the hall sensor to detect lid open/close events.
 * This enables automatic sleep when the lid is closed.
 *
 * @param state Output pointer to receive new lid state (1=open, 0=closed), can be NULL
 * @return 1 if lid state changed, 0 if unchanged or no lid sensor present
 *
 * @note Updates global lid.is_open state on change
 */
int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open!=lid.is_open) {
			lid.is_open = lid_open;
			if (state) *state = lid_open;
			return 1;
		}
	}
	return 0;
}

///////////////////////////////
// Input Management
///////////////////////////////

static SDL_Joystick *joystick;

/**
 * Initializes input subsystem.
 *
 * Opens the device's joystick/gamepad interface via SDL.
 */
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

/**
 * Shuts down input subsystem.
 *
 * Closes joystick handle and cleans up SDL input resources.
 */
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////
// Video Management
///////////////////////////////

#define BLANK_PATH "/sys/class/backlight/backlight/bl_power"
#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"

/**
 * Checks if HDMI cable is currently connected.
 *
 * Reads the DRM subsystem status to detect HDMI hotplug events.
 * Used during video initialization to select appropriate resolution.
 *
 * @return 1 if HDMI connected, 0 if disconnected
 *
 * @note This is the low-level hardware check; use GetHDMI() for user settings
 */
static int HDMI_enabled(void) {
	char value[64];
	getFile(HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
}

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
static int rotate = 0;

/**
 * Initializes video subsystem and creates rendering surfaces.
 *
 * Sets up SDL window, renderer, and textures for the appropriate output:
 * - Built-in LCD: 640x480 with optional 270-degree rotation
 * - HDMI output: 720x720 with no rotation
 *
 * The function automatically detects HDMI connection at startup and
 * configures resolution accordingly. Logs detailed SDL configuration
 * information for debugging.
 *
 * @return Pointer to main screen surface for rendering
 *
 * @note Rotation is applied only for built-in screen (when display is taller than wide)
 * @note HDMI detection happens before settings are loaded, uses hardware check
 */
SDL_Surface* PLAT_initVideo(void) {
	LOG_info("PLAT_initVideo\n");
	
	// char* model = getenv("RGXX_MODEL");
	// is_cubexx = exactMatch("RGcubexx", model);
	// is_rg34xx = exactMatch("RG34xx", model);
	
	SDL_version compiled;
	SDL_version linked;
	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);
	LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);

	int num_displays = SDL_GetNumVideoDisplays();
	LOG_info("SDL_GetNumVideoDisplays(): %i\n", num_displays);

	LOG_info("Available video drivers:\n");
	for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
		LOG_info("- %s\n", SDL_GetVideoDriver(i));
	}
	LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());

	LOG_info("Available render drivers:\n");
	for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
		SDL_RendererInfo info;
		SDL_GetRenderDriverInfo(i,&info);
		LOG_info("- %s\n", info.name);
	}

	LOG_info("Available display modes:\n");
	SDL_DisplayMode mode;
	for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
		SDL_GetDisplayMode(0, i, &mode);
		LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	}
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));

	// SDL_SetHint(SDL_HINT_RENDER_VSYNC,"0"); // ignored

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (HDMI_enabled()) { // can't use getHDMI() from settings because it hasn't be initialized yet
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		p = HDMI_PITCH;
		on_hdmi = 1;
	}
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	// LOG_info("window size: %ix%i\n", w,h);
	
	// SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	if (mode.h>mode.w) rotate = 3; // no longer set on 28xx (because of SDL2 rotation patch?)
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

	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1"); // linear
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
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
 * Clears the screen to black.
 *
 * Fills the screen surface and presents three black frames to ensure
 * all buffering is cleared out.
 */
static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
}

/**
 * Shuts down video subsystem and frees all resources.
 *
 * Destroys SDL surfaces, textures, renderer, and window.
 */
void PLAT_quitVideo(void) {
	// clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	// system("cat /dev/zero > /dev/fb0 2>/dev/null");
	SDL_Quit();
}

/**
 * Clears the given screen surface to black.
 *
 * @param screen Surface to clear
 */
void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}

/**
 * Clears both the screen surface and renderer.
 *
 * Used to ensure complete clearing of all video buffers.
 */
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	SDL_RenderClear(vid.renderer);
}

/**
 * Sets vsync mode (not implemented).
 *
 * @param vsync Desired vsync state (ignored)
 *
 * @note Vsync is always enabled on this platform via SDL_RENDERER_PRESENTVSYNC
 */
void PLAT_setVsync(int vsync) {
	// Vsync is always on, controlled by SDL_RENDERER_PRESENTVSYNC flag
}

// Maximum integer upscale factor for nearest-neighbor when crisp sharpness enabled
// Used for small source resolutions (e.g., GB at 160x144 can use 4x)
// Larger sources limited to 1x or 2x to avoid excessive memory usage
static int hard_scale = 4;

/**
 * Resizes video textures and surfaces to match new dimensions.
 *
 * Recreates SDL textures with new dimensions and updates scaling mode
 * based on sharpness setting. For crisp sharpness, creates intermediate
 * target texture at hard_scale multiple for nearest-neighbor upscaling.
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param p New pitch in bytes
 *
 * @note No-op if dimensions haven't changed
 * @note Automatically adjusts hard_scale based on source resolution
 */
static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient for 640x480)
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
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
 * Public API to resize video and return screen surface.
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param p New pitch in bytes
 * @return Screen surface pointer (unchanged)
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

/**
 * Sets video scale clipping region (not implemented).
 *
 * @param x X offset
 * @param y Y offset
 * @param width Clip width
 * @param height Clip height
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// Not used on this platform
}

/**
 * Sets nearest-neighbor scaling mode (not implemented).
 *
 * @param enabled Whether to enable nearest-neighbor scaling
 *
 * @note Use PLAT_setSharpness() instead for scaling control
 */
void PLAT_setNearestNeighbor(int enabled) {
	// Use sharpness setting instead
}

/**
 * Sets sharpness/scaling mode.
 *
 * Controls how video is scaled to screen:
 * - SHARPNESS_SOFT: Linear interpolation (smooth)
 * - SHARPNESS_CRISP: Nearest-neighbor upscale then linear downscale
 *
 * @param sharpness Desired sharpness mode
 *
 * @note Triggers video resize to apply new scaling textures
 */
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}

static struct FX_Context {
	int scale;
	int type;
	int color;
	int next_scale;
	int next_type;
	int next_color;
	int live_type;
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
 * Converts RGB565 pixel to RGB888 components.
 *
 * Expands 16-bit RGB565 (5-6-5 bit channels) to full 8-bit components.
 * Uses bit replication to fill lower bits for smooth gradients.
 *
 * @param rgb565 Input pixel in RGB565 format
 * @param r Output pointer for 8-bit red component
 * @param g Output pointer for 8-bit green component
 * @param b Output pointer for 8-bit blue component
 */
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Extract the red component (5 bits)
    uint8_t red = (rgb565 >> 11) & 0x1F;
    // Extract the green component (6 bits)
    uint8_t green = (rgb565 >> 5) & 0x3F;
    // Extract the blue component (5 bits)
    uint8_t blue = rgb565 & 0x1F;

    // Scale the values to 8-bit range using bit replication
    *r = (red << 3) | (red >> 2);
    *g = (green << 2) | (green >> 4);
    *b = (blue << 3) | (blue >> 2);
}

/**
 * Loads and updates scanline/grid overlay texture.
 *
 * Selects appropriate overlay image based on effect type and scale factor.
 * For grid effects, applies color tinting if specified (e.g., DMG green).
 * Adjusts alpha opacity based on grid size to maintain consistent appearance.
 *
 * Only updates when effect parameters change to avoid redundant loading.
 *
 * @note Updates effect.live_type to track currently loaded effect
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
	int opacity = 128; // 1 - 1/2 = 50%
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
	
	// LOG_info("effect: %s opacity: %i\n", effect_path, opacity);
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		if (effect.type==EFFECT_GRID) {
			if (effect.color) {
				// LOG_info("dmg color grid...\n");
			
				uint8_t r,g,b;
				rgb565_to_rgb888(effect.color,&r,&g,&b);
				// LOG_info("rgb %i,%i,%i\n",r,g,b);
				
				uint32_t* pixels = (uint32_t*)tmp->pixels;
				int width = tmp->w;
				int height = tmp->h;
				for (int y = 0; y < height; ++y) {
				    for (int x = 0; x < width; ++x) {
				        uint32_t pixel = pixels[y * width + x];
				        uint8_t _,a;
				        SDL_GetRGBA(pixel, tmp->format, &_, &_, &_, &a);
				        if (a) pixels[y * width + x] = SDL_MapRGBA(tmp->format, r,g,b, a);
				    }
				}
			}
		}
		
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
	}

/**
 * Queues a scanline/grid overlay effect for next frame.
 *
 * @param next_type Effect type (EFFECT_NONE, EFFECT_LINE, EFFECT_GRID)
 */
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}

/**
 * Sets the color tint for grid effects.
 *
 * Used to colorize grid overlay (e.g., for DMG green tint).
 *
 * @param next_color RGB565 color value, or 0 for white
 */
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}

/**
 * Delays for remaining frame time to maintain target framerate.
 *
 * @param remaining Milliseconds remaining in frame budget
 */
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

/**
 * Returns software scaler function for renderer.
 *
 * Always returns simple 1x1 scaler as this platform uses hardware
 * scaling via SDL renderer. Updates effect scale for overlay sizing.
 *
 * @param renderer Renderer context
 * @return Software scaler function pointer
 */
scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

/**
 * Prepares to blit from a GFX_Renderer.
 *
 * Saves renderer pointer and resizes video to match source dimensions.
 * The actual blit happens in PLAT_flip().
 *
 * @param renderer Renderer containing source image and scaling info
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

/**
 * Presents rendered frame to display.
 *
 * Handles multiple rendering paths:
 * 1. Direct screen surface blit (launcher/menu)
 * 2. GFX_Renderer blit with scaling and effects (games)
 *
 * For renderer path:
 * - Applies sharpness (crisp uses 2-pass: NN upscale -> linear downscale)
 * - Handles aspect ratio modes (native/cropped/aspect-correct)
 * - Applies rotation for built-in screen (disabled on HDMI)
 * - Overlays scanline/grid effects if enabled
 *
 * Updates HDMI status on every flip to detect hotplug events.
 *
 * @param IGNORED Unused (kept for API compatibility)
 * @param ignored Unused (kept for API compatibility)
 */
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	
	on_hdmi = GetHDMI(); // use settings instead of getInt(HDMI_STATE_PATH)
	
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		if (rotate && !on_hdmi) SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){0,device_width,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
	// uint32_t then = SDL_GetTicks();
	SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);
	// LOG_info("blit blocked for %ims (%i,%i)\n", SDL_GetTicks()-then,vid.buffer->w,vid.buffer->h);
	
	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};
	if (vid.blit->aspect==0) { // native or cropped
		// LOG_info("src_rect %i,%i %ix%i\n",src_rect->x,src_rect->y,src_rect->w,src_rect->h);

		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
		
		// LOG_info("dst_rect %i,%i %ix%i\n",dst_rect->x,dst_rect->y,dst_rect->w,dst_rect->h);
	}
	else if (vid.blit->aspect>0) { // aspect
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	
	int ox,oy;
	oy = (device_width-device_height)/2;
	ox = -oy;
	if (rotate && !on_hdmi) SDL_RenderCopyEx(vid.renderer,target,src_rect,&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
	else SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
	
	updateEffect();
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect) {
		// ox = effect.scale - (dst_rect->x % effect.scale);
		// oy = effect.scale - (dst_rect->y % effect.scale);
		// if (ox==effect.scale) ox = 0;
		// if (oy==effect.scale) oy = 0;
		// LOG_info("rotate: %i ox: %i oy: %i\n", rotate, ox,oy);
		if (rotate && !on_hdmi) SDL_RenderCopyEx(vid.renderer,vid.effect,&(SDL_Rect){0,0,dst_rect->w,dst_rect->h},&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,dst_rect->w,dst_rect->h},dst_rect);
	}
	
	// uint32_t then = SDL_GetTicks();
	SDL_RenderPresent(vid.renderer);
	// LOG_info("SDL_RenderPresent blocked for %ims\n", SDL_GetTicks()-then);
	vid.blit = NULL;
}

/**
 * Checks if platform supports overscan adjustment.
 *
 * @return 0 (overscan not supported on this platform)
 */
int PLAT_supportsOverscan(void) { return 0; }

///////////////////////////////
// Overlay (HUD Icons)
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
 * Initializes overlay surface for HUD icons.
 *
 * Creates ARGB surface for battery, volume, and other status icons.
 *
 * @return Overlay surface pointer
 */
SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}

/**
 * Cleans up overlay resources.
 */
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}

/**
 * Enables or disables overlay rendering (not implemented).
 *
 * @param enable Whether to enable overlay
 *
 * @note Overlay is always composited with main screen
 */
void PLAT_enableOverlay(int enable) {
	// Overlay always enabled
}

///////////////////////////////
// Power and Battery Management
///////////////////////////////

static int online = 0;

/**
 * Gets battery charge level and charging status.
 *
 * Also monitors WiFi connection status as a side effect (updates online flag).
 * Battery percentage is simplified into 6 levels to reduce icon flickering.
 *
 * @param is_charging Output pointer for charging state (1=charging, 0=on battery)
 * @param charge Output pointer for charge level (10/20/40/60/80/100)
 *
 * @note Also updates global 'online' flag from wlan0 interface state
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/ac/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/leds/work/brightness"

/**
 * Enables or disables screen backlight.
 *
 * When disabling:
 * - Powers down display completely (FB_BLANK_POWERDOWN)
 * - Sets brightness to 0
 * - Turns on LED indicator (255 = full brightness)
 *
 * When enabling:
 * - Wakes display (FB_BLANK_UNBLANK)
 * - Restores user brightness setting
 * - Turns off LED indicator
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH,0);
	}
	else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH,255);
	}
}

/**
 * Performs graceful system shutdown.
 *
 * Shutdown sequence:
 * 1. Remove exec marker and sync filesystem
 * 2. Mute audio
 * 3. Turn off backlight and enable LED
 * 4. Clean up all subsystems (SND, VIB, PWR, GFX)
 * 5. Exit process (shutdown handled by system)
 */
void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 255 > /sys/class/leds/work/brightness");
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	// system("cat /dev/zero > /dev/fb0 2>/dev/null");
	// system("poweroff");
	// while (1) pause(); // lolwat
	
	// touch("/tmp/poweroff");
	// sync();
	// system("touch /tmp/poweroff && sync");
	exit(0);
}

///////////////////////////////
// CPU and Performance
///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

/**
 * Sets CPU clock speed based on performance level.
 *
 * Speed mappings:
 * - CPU_SPEED_MENU:        600 MHz (minimal power for menus)
 * - CPU_SPEED_POWERSAVE:  1104 MHz (battery-friendly gaming)
 * - CPU_SPEED_NORMAL:     1608 MHz (default gaming)
 * - CPU_SPEED_PERFORMANCE: 1992 MHz (demanding games)
 *
 * @param speed CPU speed constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1104000; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1992000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

///////////////////////////////
// Rumble
///////////////////////////////

#define RUMBLE_PATH "/sys/class/gpio/gpio20/value"

/**
 * Controls rumble motor.
 *
 * Disabled when HDMI is active (assumes external controller is in use).
 *
 * @param strength Rumble strength (0=off, non-zero=on, binary control only)
 */
void PLAT_setRumble(int strength) {
	if (GetHDMI()) return; // Disable rumble on HDMI (likely using controller)
	putInt(RUMBLE_PATH, strength?1:0);
}

///////////////////////////////
// Audio
///////////////////////////////

/**
 * Selects audio sample rate based on constraints.
 *
 * @param requested Desired sample rate
 * @param max Maximum supported sample rate
 * @return Selected sample rate (minimum of requested and max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

///////////////////////////////
// Platform Info
///////////////////////////////

/**
 * Returns platform model name.
 *
 * @return "Miyoo Flip"
 */
char* PLAT_getModel(void) {
	return "Miyoo Flip";
}

/**
 * Checks if WiFi is currently connected.
 *
 * @return 1 if WiFi connected, 0 if disconnected
 *
 * @note Updated by PLAT_getBatteryStatus() which polls wlan0 state
 */
int PLAT_isOnline(void) {
	return online;
}