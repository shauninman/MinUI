/**
 * platform.c - macOS platform implementation
 *
 * Development platform for testing MinUI on macOS. Provides minimal
 * implementations of platform functions for development and debugging.
 * Most hardware-specific features (brightness, volume, power) are stubbed.
 *
 * Video: Uses SDL2 window with optional rotation
 * Input: SDL2 joystick subsystem
 * Audio/Power: No-op stubs for development
 */

#include <stdio.h>
#include <stdlib.h>
// #include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "msettings.h"

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////
// Settings (stub implementations for development)
///////////////////////////////

void InitSettings(void){}
void QuitSettings(void){}

int GetBrightness(void) { return 0; }
int GetVolume(void) { return 0; }

void SetRawBrightness(int value) {}
void SetRawVolume(int value){}

void SetBrightness(int value) {}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }

///////////////////////////////
// Input
///////////////////////////////

static SDL_Joystick *joystick;

/**
 * Initializes SDL2 joystick subsystem for development input.
 */
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}

/**
 * Closes joystick and shuts down SDL2 joystick subsystem.
 */
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////
// Video
///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
} vid;

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;

/**
 * Initializes SDL2 video subsystem with window and renderer.
 *
 * Creates a rotated window for portrait display (simulating vertical handhelds).
 * Window dimensions are swapped (h,w) and rotate flag is set for rendering.
 *
 * @return SDL surface for rendering (vid.screen)
 */
SDL_Surface* PLAT_initVideo(void) {
	// SDL_InitSubSystem(SDL_INIT_VIDEO);
	// SDL_ShowCursor(0);
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
	SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	SDL_GetCurrentDisplayMode(0, &mode);
	// Rotate display to simulate vertical handheld
	rotate = 1;
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	// Create window with swapped dimensions (h,w) for rotated display
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, h,w, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	PWR_disablePowerOff();
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	return vid.screen;
}

/**
 * Clears video output by rendering black frames.
 *
 * Presents three black frames to ensure complete screen clear.
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
 * Cleans up SDL2 video resources and quits SDL.
 */
void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

/**
 * Resizes video buffer and texture to new dimensions.
 *
 * Called when emulator core changes resolution. Recreates texture
 * and buffer at new size. No-op if dimensions haven't changed.
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param p New pitch in bytes
 */
static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;

	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	// PLAT_clearVideo(vid.screen);

	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
	scale1x1_c16(
		renderer->src,renderer->dst,
		renderer->true_w,renderer->true_h,renderer->src_p,
		vid.screen->w,vid.screen->h,vid.screen->pitch // fixed in this implementation
		// renderer->dst_w,renderer->dst_h,renderer->dst_p
	);
}

/**
 * Presents rendered frame to the display.
 *
 * Handles two rendering paths:
 * 1. Direct screen rendering (no blit): Updates texture from screen surface
 * 2. Renderer blitting: Processes aspect ratio and scaling from GFX_Renderer
 *
 * Applies 90-degree rotation when rotate flag is set (portrait orientation).
 *
 * @param IGNORED Unused SDL_Surface parameter
 * @param ignored Unused integer parameter
 */
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {

	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		if (rotate) {
			LOG_info("rotated\n");
			SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){device_height,0,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		}
		else {
			LOG_info("not rotated\n");
			SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		}
		SDL_RenderPresent(vid.renderer);
		return;
	}

	if (!vid.blit) resizeVideo(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH); // !!!???

	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
	SDL_UnlockTexture(vid.texture);

	SDL_Rect* src_rect = NULL;
	SDL_Rect* dst_rect = NULL;
	SDL_Rect src_r = {0};
	SDL_Rect dst_r = {0};
	if (vid.blit) {
		src_r.x = vid.blit->src_x;
		src_r.y = vid.blit->src_y;
		src_r.w = vid.blit->src_w;
		src_r.h = vid.blit->src_h;
		src_rect = &src_r;

		// Calculate destination rectangle based on aspect ratio mode
		if (vid.blit->aspect==0) { // native (or cropped?)
			int w = vid.blit->src_w * vid.blit->scale;
			int h = vid.blit->src_h * vid.blit->scale;
			int x = (FIXED_WIDTH - w) / 2;
			int y = (FIXED_HEIGHT - h) / 2;

			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
		}
		else if (vid.blit->aspect>0) { // aspect
			int h = FIXED_HEIGHT;
			int w = h * vid.blit->aspect;
			if (w>FIXED_WIDTH) {
				double ratio = 1 / vid.blit->aspect;
				w = FIXED_WIDTH;
				h = w * ratio;
			}
			int x = (FIXED_WIDTH - w) / 2;
			int y = (FIXED_HEIGHT - h) / 2;

			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
		}
	}
	SDL_RenderCopy(vid.renderer, vid.texture, src_rect, dst_rect);
	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

///////////////////////////////
// Overlay
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

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////
// Power and Hardware
///////////////////////////////

static int online = 1;

/**
 * Returns stub battery status for development.
 *
 * Always reports full battery and charging on macOS.
 *
 * @param is_charging Set to 1 (always charging)
 * @param charge Set to 100 (full battery)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = 1;
	*charge = 100;
	return;
}

/**
 * Stub backlight control (no-op on macOS).
 */
void PLAT_enableBacklight(int enable) {
	// No backlight control on macOS
}

/**
 * Shuts down MinUI and exits.
 *
 * Cleans up subsystems and exits process.
 */
void PLAT_powerOff(void) {
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

/**
 * Stub CPU speed control (no-op on macOS).
 */
void PLAT_setCPUSpeed(int speed) {
	// No CPU speed control on macOS
}

/**
 * Stub rumble control (no-op on macOS).
 */
void PLAT_setRumble(int strength) {
	// No rumble on macOS
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns platform model name.
 *
 * @return "macOS" string
 */
char* PLAT_getModel(void) {
	return "macOS";
}

/**
 * Returns network online status.
 *
 * @return 1 (always online for development)
 */
int PLAT_isOnline(void) {
	return online;
}