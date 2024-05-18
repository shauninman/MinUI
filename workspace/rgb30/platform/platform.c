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

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////

// static SDL_Joystick *joystick;
// void PLAT_initInput(void) {
// 	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
// 	joystick = SDL_JoystickOpen(0);
// }
// void PLAT_quitInput(void) {
// 	SDL_JoystickClose(joystick);
// 	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
// }

///////////////////////////////

#define RAW_UP		544
#define RAW_DOWN	545
#define RAW_LEFT	546
#define RAW_RIGHT	547
#define RAW_A		305
#define RAW_B		304
#define RAW_X		307
#define RAW_Y		308
#define RAW_START	315
#define RAW_SELECT	314
#define RAW_MENU	139
#define RAW_L1		310
#define RAW_L2		312
#define RAW_L3		317
#define RAW_R1		311
#define RAW_R2		313
#define RAW_R3		318
#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116
#define RAW_HATY	17
#define RAW_HATX	16
#define RAW_LSY		1
#define RAW_LSX		0
#define RAW_RSY		3
#define RAW_RSX		4

#define RAW_MENU1	RAW_L3
#define RAW_MENU2	RAW_R3

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[2] = open("/dev/input/event2", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[3] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
}
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

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
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
			
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;
				LOG_info("key event: %i (%i)\n", code,pressed);
					 if (code==RAW_UP) 	{ btn = BTN_UP; 		id = BTN_ID_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DOWN; 		id = BTN_ID_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_LEFT; 		id = BTN_ID_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_RIGHT; 		id = BTN_ID_RIGHT; }
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
				LOG_info("abs event: %i (%i==%i)\n",code,value,(value * 32767) / 1800);
				
					 if (code==RAW_LSX) pad.laxis.x = (value * 32767) / 1800;
				else if (code==RAW_LSY) pad.laxis.y = (value * 32767) / 1800;
				else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 1800;
				else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 1800;
				
				btn = BTN_NONE; // already handled, force continue
			}
			
			if (btn==BTN_NONE) continue;
		
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
	}
}

int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) return 1;
		}
	}
	return 0;
}

///////////////////////////////

#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

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
SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
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
	//
	// LOG_info("Availabel audio drivers:\n");
	// for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetAudioDriver(i));
	// }
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (getInt(HDMI_STATE_PATH)) {
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		p = HDMI_PITCH;
	}
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	puts("");
	
	int renderer_width,renderer_height;
	SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// LOG_info("output size: %ix%i\n", renderer_width, renderer_height);
	if (renderer_width!=w) { // I think this can only be hdmi
		float x_scale = (float)renderer_width / w;
		float y_scale = (float)renderer_height / h;
		SDL_SetWindowSize(vid.window, w / x_scale, h / y_scale);

		SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
		x_scale = (float)renderer_width / w;
		y_scale = (float)renderer_height / h;
		SDL_RenderSetScale(vid.renderer, x_scale,y_scale);
		
		// for some reason we need to clear and present 
		// after setting the window size or we'll miss
		// the first frame
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeLinear); // we always start at device size so use linear for better upscaling over hdmi
	
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
	
	return vid.screen;
}

static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	// clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	// system("cat /dev/zero > /dev/fb0");
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

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	// PLAT_clearVideo(vid.screen);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureScaleMode(vid.texture, w==device_width&&h==device_height?SDL_ScaleModeLinear:SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	// vid.blit = NULL; // TODO: don't do this here!
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) resizeVideo(device_width,device_height,device_pitch); // !!!???
	
	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	if (vid.blit) {
		scale1x1_c16(
			vid.blit->src,vid.buffer->pixels,
			vid.blit->true_w,vid.blit->true_h,vid.blit->src_p,
			vid.buffer->w,vid.buffer->h,vid.buffer->pitch
		);
	}
	else {
		SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
	}
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
			int x = (device_width - w) / 2;
			int y = (device_height - h) / 2;
						
			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
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

			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
		}
		
		/** /
		LOG_info("true %ix%i\nsrc   %i,%i %ix%i dst   %i,%i %ix%i\nsrc_r %i,%i %ix%i dst_r %i,%i %ix%i\n",
			vid.blit->true_w,
			vid.blit->true_h,
		
			vid.blit->src_x,
			vid.blit->src_y,
			vid.blit->src_w,
			vid.blit->src_h,
		
			vid.blit->dst_x,
			vid.blit->dst_y,
			vid.blit->dst_w,
			vid.blit->dst_h,
		
			src_r.x,
			src_r.y,
			src_r.w,
			src_r.h,
		
			dst_r.x,
			dst_r.y,
			dst_r.w,
			dst_r.h
		);/**/
	}
	SDL_RenderCopy(vid.renderer, vid.texture, src_rect, dst_rect);
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

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}

void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	
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
	else strcpy(model, "RGB30");
	
	return model;
}

int PLAT_isOnline(void) {
	return online;
}