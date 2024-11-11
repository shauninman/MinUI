// tg5040
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

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* effect;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	SDL_version compiled;
	SDL_version linked;
	SDL_VERSION(&compiled);
	SDL_GetVersion(&linked);
	LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	
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
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	
	SDL_RendererInfo info;
	SDL_GetRendererInfo(vid.renderer, &info);
	LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	return vid.screen;
}

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

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("cat /dev/zero > /dev/fb0");
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

static struct FX_Context {
	int scale;
	int type;
	int next_scale;
	int next_type;
	int live_type;
} effect = {
	.scale = 1,
	.next_scale = 1,
	.type = EFFECT_NONE,
	.next_type = EFFECT_NONE,
	.live_type = EFFECT_NONE,
};
static void updateEffect(void) {
	if (effect.next_scale==effect.scale && effect.next_type==effect.type) return; // unchanged
	
	int live_scale = effect.scale;
	effect.scale = effect.next_scale;
	effect.type = effect.next_type;
	
	if (effect.type==EFFECT_NONE) return; // disabled
	if (effect.type==effect.live_type && effect.scale==live_scale) return; // already loaded
	
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
	
	LOG_info("effect: %s opacity: %i\n", effect_path, opacity);
	SDL_Surface* tmp = IMG_Load(effect_path);
	if (tmp) {
		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	LOG_info("getScaler for scale: %i\n", renderer->scale);
	effect.next_scale = renderer->scale;
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

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
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
	
	updateEffect();
	if (vid.blit && effect.type!=EFFECT_NONE && vid.effect) {
		if (!dst_rect) dst_rect = &dst_r;
		SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0,0,FIXED_WIDTH,FIXED_HEIGHT},&(SDL_Rect){dst_rect->x%effect.scale,dst_rect->y%effect.scale,FIXED_WIDTH,FIXED_HEIGHT});
	}

	SDL_RenderPresent(vid.renderer);
	vid.blit = NULL;
}

///////////////////////////////

// TODO: 
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

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	int i = getInt("/sys/class/power_supply/axp2202-battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// // wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define LED_PATH1 "/sys/class/led_anim/max_scale"
#define LED_PATH2 "/sys/class/led_anim/max_scale_lr"
#define LED_PATH3 "/sys/class/led_anim/max_scale_f1f2" // front facing
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		putInt(LED_PATH1,0);
		// putInt(LED_PATH2,0);
		putInt(LED_PATH3,0);
	}
	else {
		SetRawBrightness(0);
		putInt(LED_PATH1,60);
		// putInt(LED_PATH2,60);
		putInt(LED_PATH3,60);
	}
}

void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	
	system("cat /dev/zero > /dev/fb0");
	system("poweroff");
	while (1) pause(); // lolwat
}

///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1200000; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; break;
		case CPU_SPEED_PERFORMANCE: freq = 2000000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"
void PLAT_setRumble(int strength) {
	putInt(RUMBLE_PATH, (strength && !GetMute())?1:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Trimui Brick";
}

int PLAT_isOnline(void) {
	return online;
}