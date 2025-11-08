/**
 * zero28/platform/platform.c - Platform implementation for Mini Zero 28 handheld
 *
 * Provides hardware-specific implementation of the MinUI platform API for the
 * Mini Zero 28 device. Key features:
 *
 * - SDL_Joystick API for input (instead of raw evdev)
 * - Display rotation support for portrait/landscape modes
 * - Grid and line overlay effects for retro CRT simulation
 * - WiFi connectivity detection
 * - AXP2202 battery monitoring
 * - External bl_enable/bl_disable scripts for backlight control
 *
 * The Zero28 uses 640x480 VGA resolution with 2x scaling and supports both
 * soft (bilinear) and crisp (nearest neighbor + linear downscale) rendering.
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////
// Input
///////////////////////////////

// Uses SDL_Joystick API instead of raw evdev for button/axis input
static SDL_Joystick *joystick;

/**
 * Initializes joystick input subsystem.
 *
 * Opens the first available joystick device (index 0) using SDL.
 * The Zero28 uses SDL_Joystick for all input including D-pad (HAT),
 * buttons, and analog sticks.
 */
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

/**
 * Shuts down joystick input subsystem.
 *
 * Closes the joystick device and cleans up SDL joystick subsystem.
 */
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////
// Video
///////////////////////////////

/**
 * Video rendering context.
 *
 * Manages SDL rendering resources and tracks current video mode.
 * Supports both direct rendering (UI) and blit rendering (game cores).
 */
static struct VID_Context {
	SDL_Window* window;      // Main window
	SDL_Renderer* renderer;  // Hardware-accelerated renderer
	SDL_Texture* texture;    // Source texture (game output)
	SDL_Texture* target;     // Intermediate target for crisp rendering
	SDL_Texture* effect;     // Grid/line overlay texture
	SDL_Surface* buffer;     // Software surface wrapper for texture
	SDL_Surface* screen;     // Main screen surface for UI

	GFX_Renderer* blit;      // Active game renderer (non-NULL during game rendering)

	int width;               // Current texture width
	int height;              // Current texture height
	int pitch;               // Current row stride in bytes
	int sharpness;           // SHARPNESS_SOFT or SHARPNESS_CRISP
} vid;

// Device native resolution and rotation state
static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;  // Set to 1 for portrait mode (90 degree rotation)

/**
 * Initializes SDL video subsystem and creates rendering context.
 *
 * Sets up 640x480 window with hardware-accelerated renderer. Detects
 * display orientation (portrait vs landscape) and enables rotation if
 * needed. Logs detailed SDL configuration for debugging.
 *
 * @return Main screen surface for UI rendering
 */
SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);

	// Log SDL version information for debugging
	SDL_version compiled;
	SDL_version linked;
	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);
	LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);

	// Log available video and render drivers
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

	// Detect portrait mode (height > width) and enable rotation
	LOG_info("Available display modes:\n");
	SDL_DisplayMode mode;
	for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
		SDL_GetDisplayMode(0, i, &mode);
		LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	}
	SDL_GetCurrentDisplayMode(0, &mode);
	if (mode.h>mode.w) rotate = 1;  // Enable 90-degree rotation for portrait displays
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("Current render driver: %s\n", info.name);
	
	int rw,rh;
	SDL_GetRendererOutputSize(vid.renderer, &rw,&rh);
	LOG_info("renderer size: %ix%i\n", rw,rh);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	int tw,th;
	SDL_QueryTexture(vid.texture, NULL,NULL,&tw,&th);
	LOG_info("texture size: %ix%i\n", tw,th); // TODO: why is this 1024x768? :lolsob:
	
	vid.target	= NULL; // only needed for non-native sizes
	
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
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
 * Clears video output by filling screen and buffer with black.
 *
 * Performs triple-buffered clear to ensure all framebuffers are blank.
 * Used during shutdown to prevent display artifacts.
 */
static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, 0);

		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, 0);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);

		SDL_RenderPresent(vid.renderer);
	}
}

/**
 * Shuts down video subsystem and cleans up resources.
 *
 * Clears display, frees all SDL surfaces/textures, and blanks the
 * framebuffer device directly to prevent garbage on screen.
 */
void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	// Directly blank framebuffer to prevent visual artifacts
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

/**
 * Clears a surface to black.
 *
 * @param screen Surface to clear
 */
void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}

/**
 * Clears both screen surface and renderer.
 *
 * Used to completely clear all rendering state.
 */
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	SDL_RenderClear(vid.renderer);
}

/**
 * Sets vsync mode (no-op on Zero28).
 *
 * The renderer is created with SDL_RENDERER_PRESENTVSYNC flag,
 * so vsync is always enabled and cannot be changed at runtime.
 *
 * @param vsync Ignored
 */
void PLAT_setVsync(int vsync) {
	// Vsync always enabled via renderer flags
}

// Integer upscale factor for crisp rendering (NN before linear downscale)
// Higher values for small source resolutions (e.g., GB 160x144 uses 4x)
static int hard_scale = 4;

/**
 * Resizes video texture to match source dimensions.
 *
 * Recreates texture and buffer at new dimensions. For crisp rendering,
 * also creates intermediate target texture scaled by hard_scale factor.
 * The hard_scale value is determined by source resolution:
 * - Native/large resolutions (>=640x480): 1x
 * - Small resolutions (GB, GBC, etc.): 4x
 *
 * @param w Width in pixels
 * @param h Height in pixels
 * @param p Pitch (row stride) in bytes
 */
static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	// Determine integer upscale factor based on source resolution
	if (w>=device_width && h>=device_height) hard_scale = 1;
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	// Recreate texture at new dimensions
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);

	// Set scaling quality based on sharpness mode
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);

	// For crisp rendering, create intermediate target at integer-scaled size
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
 * Resizes video mode (public API wrapper).
 *
 * @param w Width in pixels
 * @param h Height in pixels
 * @param p Pitch in bytes
 * @return Screen surface (unchanged)
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

/**
 * Sets video scaling clip region (not implemented).
 *
 * @param x X offset (ignored)
 * @param y Y offset (ignored)
 * @param width Width (ignored)
 * @param height Height (ignored)
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// Not implemented
}

/**
 * Enables nearest neighbor scaling (not used).
 *
 * Scaling mode is controlled by sharpness setting instead.
 *
 * @param enabled Ignored
 */
void PLAT_setNearestNeighbor(int enabled) {
	// Scaling controlled by sharpness mode
}

/**
 * Sets rendering sharpness mode.
 *
 * - SHARPNESS_SOFT: Bilinear scaling
 * - SHARPNESS_CRISP: Nearest neighbor upscale + bilinear downscale
 *
 * @param sharpness SHARPNESS_SOFT or SHARPNESS_CRISP
 */
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;  // Force resize
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}

///////////////////////////////
// Visual Effects (Grid/Line Overlays)
///////////////////////////////

/**
 * Visual effect state.
 *
 * Tracks current and pending effect configuration. Effects simulate
 * CRT scanlines (line) or pixel grids for retro aesthetic.
 */
static struct FX_Context {
	int scale;        // Current scale factor (determines effect size)
	int type;         // Current effect type (EFFECT_NONE/LINE/GRID)
	int color;        // Current tint color (RGB565, 0 = no tint)
	int next_scale;   // Pending scale factor
	int next_type;    // Pending effect type
	int next_color;   // Pending tint color
	int live_type;    // Currently loaded effect type
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
 * Converts RGB565 color to RGB888.
 *
 * Expands 16-bit color (5-6-5 bits) to full 8-bit components.
 * Uses bit replication to fill low bits for accurate conversion.
 *
 * @param rgb565 Input color in RGB565 format
 * @param r Output red component (0-255)
 * @param g Output green component (0-255)
 * @param b Output blue component (0-255)
 */
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
	// Extract color components from packed RGB565
	uint8_t red = (rgb565 >> 11) & 0x1F;   // 5 bits
	uint8_t green = (rgb565 >> 5) & 0x3F;  // 6 bits
	uint8_t blue = rgb565 & 0x1F;          // 5 bits

	// Scale to 8-bit range by replicating high bits into low bits
	*r = (red << 3) | (red >> 2);
	*g = (green << 2) | (green >> 4);
	*b = (blue << 3) | (blue >> 2);
}

/**
 * Updates effect texture based on pending configuration.
 *
 * Loads appropriate grid or line overlay image based on scale factor
 * and applies color tinting if specified. Different scale factors use
 * different effect images with tuned opacity values.
 *
 * Line effects: Horizontal scanlines at varying scales (2-8 pixel height)
 * Grid effects: Pixel grids simulating LCD/CRT at varying scales (2-11 pixel)
 */
static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type && effect.next_color==effect.color) return;

	int live_scale = effect.scale;
	int live_color = effect.color;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	effect.color = effect.next_color;

	if (effect.type==EFFECT_NONE) return;
	if (effect.type==effect.live_type && effect.scale==live_scale && effect.color==live_color) return;

	// Select effect image and opacity based on type and scale
	char* effect_path;
	int opacity = 128;  // Default 50% opacity (1 - 1/2)
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
			// opacity = 96; // TODO: tmp, for white grid
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

	// Load effect image and apply color tinting if needed
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		// Apply color tinting for grid effects (e.g., DMG green tint)
		if (effect.type==EFFECT_GRID) {
			if (effect.color) {
				uint8_t r,g,b;
				rgb565_to_rgb888(effect.color,&r,&g,&b);

				// Tint all non-transparent pixels with the specified color
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

		// Create texture from tinted/untinted surface
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}

/**
 * Sets the visual effect type.
 *
 * @param next_type EFFECT_NONE, EFFECT_LINE, or EFFECT_GRID
 */
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}

/**
 * Sets the effect tint color.
 *
 * Used for grid effects to simulate colored displays (DMG green, etc).
 *
 * @param next_color RGB565 color value, or 0 for no tint
 */
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}

/**
 * Waits for remaining frame time to maintain target framerate.
 *
 * @param remaining Milliseconds to wait
 */
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

/**
 * Gets software scaler function for renderer.
 *
 * The Zero28 uses SDL hardware scaling, so always returns identity
 * scaler. Updates effect scale based on renderer scale factor.
 *
 * @param renderer Active renderer
 * @return Identity scaler (scale1x1_c16)
 */
scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

/**
 * Activates game renderer and resizes video to match source.
 *
 * Called when switching from UI rendering to game rendering.
 * Stores renderer pointer and resizes texture to match game output.
 *
 * @param renderer Game renderer with source dimensions
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

/**
 * Flips the framebuffer, presenting rendered content to screen.
 *
 * Handles two rendering paths:
 * 1. UI rendering: Direct screen->texture->display
 * 2. Game rendering: Scaled game output with aspect ratio correction
 *
 * For game rendering, applies:
 * - Crisp rendering (NN upscale + linear downscale) if enabled
 * - Aspect ratio correction (native, cropped, or stretched)
 * - Display rotation if in portrait mode
 * - Grid/line effects if enabled
 *
 * @param IGNORED Unused (legacy SDL_Surface parameter)
 * @param ignored Unused parameter
 */
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {

	// UI rendering path (no active game renderer)
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH);
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		// Apply rotation for portrait displays
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){device_height,0,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}

	// Game rendering path: Update texture with game framebuffer
	SDL_UpdateTexture(vid.texture,NULL,vid.blit->src,vid.blit->src_p);

	// For crisp rendering: NN upscale to intermediate target
	SDL_Texture* target = vid.texture;
	int x = vid.blit->src_x;
	int y = vid.blit->src_y;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		// Scale source rect for intermediate target
		x *= hard_scale;
		y *= hard_scale;
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}

	// Calculate source and destination rectangles
	SDL_Rect* src_rect = &(SDL_Rect){x,y,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};

	// Native or cropped aspect ratio (integer scaling, centered)
	if (vid.blit->aspect==0) {
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	// Aspect ratio correction (fit to screen, maintain aspect)
	else if (vid.blit->aspect>0) {
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

	// Render game output with rotation if needed
	int ox,oy;
	oy = (device_width-device_height)/2;
	ox = -oy;
	if (rotate) SDL_RenderCopyEx(vid.renderer,target,src_rect,&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
	else SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);

	// Apply grid/line effect overlay if enabled
	updateEffect();
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect) {
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.effect,&(SDL_Rect){0,0,dst_rect->w,dst_rect->h},&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,dst_rect->w,dst_rect->h},dst_rect);
	}

	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

///////////////////////////////
// Overlay (Status Icons)
///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE     // Unscaled width
#define OVERLAY_HEIGHT PILL_SIZE    // Unscaled height
#define OVERLAY_BPP 4               // Bytes per pixel (ARGB32)
#define OVERLAY_DEPTH 16            // Bit depth
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)  // Row stride
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000  // ARGB

/**
 * Overlay rendering context.
 *
 * Provides surface for rendering status icons (battery, wifi, etc).
 */
static struct OVL_Context {
	SDL_Surface* overlay;  // ARGB surface for status icons
} ovl;

/**
 * Initializes overlay surface for status icons.
 *
 * Creates ARGB surface at 2x scale for status indicators.
 *
 * @return Overlay surface
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
 * Enables or disables overlay (not implemented).
 *
 * @param enable Ignored
 */
void PLAT_enableOverlay(int enable) {
	// Not implemented
}

///////////////////////////////
// Power Management
///////////////////////////////

// WiFi connectivity state (updated during battery polling)
static int online = 0;

/**
 * Gets battery and charging status.
 *
 * Reads battery level from AXP2202 power management IC via sysfs.
 * The Zero28 uses different battery paths than other platforms:
 * - /sys/class/power_supply/axp2202-battery/capacity
 * - /sys/class/power_supply/axp2202-usb/online
 *
 * Also polls WiFi status as a convenience (updated during regular
 * battery polling to avoid separate polling).
 *
 * @param is_charging Set to 1 if USB power connected, 0 otherwise
 * @param charge Set to battery level (10-100 in 20% increments)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// Check USB power connection (AXP2202-specific path)
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	// Read battery capacity and round to nearest 20%
	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// Update WiFi status (polled here to avoid separate polling loop)
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define BLANK_PATH "/sys/class/graphics/fb0/blank"

/**
 * Enables or disables backlight.
 *
 * The Zero28 uses external bl_enable/bl_disable scripts for backlight
 * control in addition to standard brightness and blanking controls.
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		system("bl_enable");  // Platform-specific backlight enable script
		putInt(BLANK_PATH,FB_BLANK_UNBLANK);
	}
	else {
		SetRawBrightness(0);
		system("bl_disable");  // Platform-specific backlight disable script
		putInt(BLANK_PATH,FB_BLANK_POWERDOWN);
	}
}

/**
 * Powers off the device.
 *
 * Performs clean shutdown sequence:
 * 1. Remove exec file and sync filesystem
 * 2. Mute audio and disable backlight
 * 3. Shutdown subsystems
 * 4. Clear framebuffer
 * 5. Power off system
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

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
	system("poweroff");
	exit(0);
}

///////////////////////////////
// CPU and Hardware Control
///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"

/**
 * Sets CPU frequency based on performance profile.
 *
 * CPU frequencies:
 * - MENU: 600 MHz (UI navigation)
 * - POWERSAVE: 816 MHz (low-demand games)
 * - NORMAL: 1416 MHz (most games)
 * - PERFORMANCE: 1800 MHz (demanding games)
 *
 * @param speed CPU_SPEED_* constant
 */
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq =  816000; break;
		case CPU_SPEED_NORMAL: 		freq = 1416000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1800000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"

/**
 * Sets rumble motor strength (not implemented).
 *
 * @param strength Rumble strength (0-100, ignored)
 */
void PLAT_setRumble(int strength) {
	// Not implemented
}

/**
 * Selects appropriate audio sample rate.
 *
 * @param requested Requested sample rate
 * @param max Maximum supported sample rate
 * @return Lesser of requested or max
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Gets device model name.
 *
 * @return "Mini Zero 28"
 */
char* PLAT_getModel(void) {
	return "Mini Zero 28";
}

/**
 * Checks if device is connected to WiFi.
 *
 * Status is updated during battery polling (see PLAT_getBatteryStatus).
 *
 * @return 1 if WiFi connected, 0 otherwise
 */
int PLAT_isOnline(void) {
	return online;
}