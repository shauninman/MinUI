// nano
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	GFX_Renderer* renderer;
} vid;
static int _;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);
	memset(vid.screen->pixels, 0, FIXED_SIZE);
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	SDL_FillRect(vid.screen, NULL, 0);
	// if (!vid.cleared) memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	// memset(vid.buffer->pixels, 0, PAGE_SIZE);
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
	// vid.cleared = 1;
	// PLAT_clearVideo(vid.buffer); // clear backbuffer
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	// TODO: new, uncomment
	// sc_info.fb.src_win.x = 0;
	// sc_info.fb.src_win.y = 0;
	// sc_info.fb.src_win.width  = w;
	// sc_info.fb.src_win.height = h;
	// // TODO: set a flag and do this in flip?
	// uint32_t args[4] = {0, LAYER1, (uintptr_t)&sc_info, 0};
	// ioctl(disp_fd, DISP_CMD_LAYER_SET_INFO, &args);
	
	// old, delete
	// SDL_FreeSurface(vid.screen);
	// vid.width = w;
	// vid.height = h;
	// vid.pitch = pitch;
	//
	// vid.screen = SDL_CreateRGBSurfaceFrom(vid.screen_info.vadd, vid.width, vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_565);
	// memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	//
	// vid.resized = 1;
	//
	// vid.rotated_pitch = 0;
	// if (vid.renderer) vid.renderer->src_w = 0;
	SDL_FillRect(vid.screen, NULL, 0);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_vsync(int remaining) {
	// ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	GFX_freeAAScaler();
	switch (renderer->scale) {
		case  6: return scale6x6_n16;
		case  5: return scale5x5_n16;
		case  4: return scale4x4_n16;
		case  3: return scale3x3_n16;
		case  2: return scale2x2_n16;
		case -1: return GFX_getAAScaler(renderer);
		default: return scale1x1_n16; // this includes crop (0)
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	void* src = renderer->src + (renderer->src_y * renderer->src_p) + (renderer->src_x * FIXED_BPP);
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	SDL_Flip(vid.screen);
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
	// setup surface
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}


///////////////////////////////

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/axp20x-usb/online");
	// if (!*is_charging) system("test-led 0"); // TODO: only turn off if not sleeping!
	int i = getInt("/sys/class/power_supply/axp20x-battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

// TODO: better handle led status (eg. connect usb, sleep, wake, disconnect and led stays on)
static int is_powering_off = 0;
void PLAT_enableBacklight(int enable) {
	// we have bl_power but it doesn't do anything :sob:
	if (enable) {
		SDL_Flip(vid.screen); // restore screen
		SetBrightness(GetBrightness());
		if (!getInt("/sys/class/power_supply/axp20x-usb/online")) system("test-led 0");
	}
	else {
		system("dd if=/dev/zero of=/dev/fb0 bs=115200 count=1"); // clear screen
		if (!is_powering_off) system("test-led 1");
		SetRawBrightness(0);
	}
}

void PLAT_powerOff(void) {
	sleep(2);
	is_powering_off = 1;
	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	touch("/tmp/poweroff");
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// TODO: 
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "RG Nano"; // TODO: I don't think there's a way to differentiate the devices since the fw is on the sd
}

int PLAT_isOnline(void) {
	return 0;
}