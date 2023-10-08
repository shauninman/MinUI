// rgb30
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include <rga/rga.h>
#include <rga/im2d.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

typedef struct PageBuffer {
	void* vadd;
	int size;
} PageBuffer;

static struct VID_Context {
	SDL_Joystick *joystick;

	SDL_Surface* video;
	SDL_Surface* screen;
	PageBuffer buffer;
	
	int width;
	int height;
	int pitch;
	int direct;
	
	rga_buffer_t src;
	rga_buffer_t dst;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_JOYSTICK);
	LOG_info("SDL_Init error: %s\n", SDL_GetError());
	SDL_ShowCursor(0);
	
	const SDL_version* version = SDL_Linked_Version();
	LOG_info("SDL_Linked_Version: %i.%i.%i\n", version->major, version->minor, version->patch);
	LOG_info("SDL_COMPILEDVERSION: %i\n", SDL_COMPILEDVERSION);
	LOG_info("SDL_VIDEODRIVER=%s\n", SDL_getenv("SDL_VIDEODRIVER"));
	
	char driver[256];
	LOG_info("SDL_VideoDriverName: %s\n", SDL_VideoDriverName(driver, 256));

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	if (getInt(HDMI_STATE_PATH)) {
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
	}
	vid.video = SDL_SetVideoMode(w,h, FIXED_DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF);
	LOG_info("\n"); // mali debug log doesn't have a line return
	
	vid.direct = 1;
	vid.width = w;
	vid.height = h;
	vid.pitch = w * FIXED_BPP;

	vid.buffer.size = (vid.pitch*2) * (h*2);
	vid.buffer.vadd = malloc(vid.buffer.size);
	memset(vid.buffer.vadd, 0, vid.buffer.size);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd,vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);

	vid.dst = wrapbuffer_virtualaddr((void*)vid.video->pixels, vid.video->w, vid.video->h, RK_FORMAT_RGB_565); // never changes
	
	LOG_info("SDL_SetVideoMode error: %s\n", SDL_GetError());
	vid.joystick = SDL_JoystickOpen(0);
	
	LOG_info("\nPLAT_initVideo: %p (%ix%i)\n", vid.video, vid.video->w, vid.video->h);
	
	return vid.direct ? vid.video : vid.screen;
}

void PLAT_quitVideo(void) {
	LOG_info("PLAT_quitVideo\n");
	SDL_FreeSurface(vid.screen);
	free(vid.buffer.vadd);
	SDL_JoystickClose(vid.joystick);
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	SDL_FillRect(vid.video, NULL, 0);
}
void PLAT_clearAll(void) {
	
}

void PLAT_setVsync(int vsync) {
	
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	
	vid.direct = w==vid.video->w && h==vid.video->h && pitch==vid.video->pitch;

	LOG_info("PLAT_resizeVideo: %i==%i && %i==%i && %i==%i (%i)\n",w,vid.video->w,h,vid.video->h,pitch,vid.video->pitch,vid.direct);

	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;
	
	if (vid.direct) memset(vid.video->pixels, 0, vid.pitch * vid.height);
	else {
		SDL_FillRect(vid.screen, NULL, 0);
		vid.screen->pixels = NULL;
		SDL_FreeSurface(vid.screen);
		
		vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd,vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
		memset(vid.screen->pixels, 0, vid.pitch * vid.height);

		vid.src = wrapbuffer_virtualaddr((void*)vid.screen->pixels, vid.screen->w, vid.screen->h, RK_FORMAT_RGB_565);
	}
	
	return vid.direct ? vid.video : vid.screen;
	
	// SDL_FillRect(vid.video, NULL, 0);
	// return vid.video;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	
}
void PLAT_setNearestNeighbor(int enabled) {
	
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	LOG_info("PLAT_getScaler() >>> src:%ix%i (%i) dst:%i,%i %ix%i (%i)\n",
		renderer->src_w,
		renderer->src_h,
		renderer->src_p, // unused

		renderer->dst_x,
		renderer->dst_y,
		renderer->dst_w, // unused
		renderer->dst_h, // unused?
		renderer->dst_p // unused
	);
	
	switch (renderer->scale) {
		case 6:  return scale6x6_c16;
		case 5:  return scale5x5_c16;
		case 4:  return scale4x4_c16;
		case 3:  return scale3x3_c16;
		case 2:  return scale2x2_c16;
		default: return scale1x1_c16;
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	// TODO: cache these offsets? (all platforms)
	void* src = renderer->src + (renderer->src_y * renderer->src_p) + (renderer->src_x * FIXED_BPP);
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	if (!vid.direct) imresize(vid.src, vid.dst);
	SDL_Flip(vid.video);
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

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = POW_LOW_CHARGE;
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
}

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}

void PLAT_powerOff(void) {
	sleep(2);
	system("shutdown");
	while (1) pause(); // lolwat
}

///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_setspeed"

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1104000; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1992000; break;
	}

	char cmd[256];
	sprintf(cmd,"echo %i > %s", freq, GOVERNOR_PATH);
	system(cmd);
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

static char model[256];
char* PLAT_getModel(void) {
	char buffer[256];
	getFile("/proc/device-tree/model", buffer, 256);
	
	char* tmp = strrchr(buffer, ' ');
	if (tmp) strcpy(model, tmp+1);
	else strcpy(model, "RG353");
	
	return model;
}