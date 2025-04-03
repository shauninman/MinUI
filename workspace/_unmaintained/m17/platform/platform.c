// m17
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

#define RAW_UP		103
#define RAW_DOWN	108
#define RAW_LEFT	105
#define RAW_RIGHT	106
#define RAW_A		48
#define RAW_B		30
#define RAW_X		21
#define RAW_Y		45
#define RAW_START	28
#define RAW_SELECT	54
#define RAW_MENU	115
#define RAW_L1		38
#define RAW_L2		44
#define RAW_R1		19
#define RAW_R2		46
#define RAW_PLUS	115
#define RAW_MINUS	114

#define RAW_MENU1	RAW_PLUS
#define RAW_MENU2	RAW_MINUS

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	char path[256];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
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
				// LOG_info("key event: %i (%i)\n", code,pressed); // no L3/R3
					 if (code==RAW_UP) 		{ btn = BTN_DPAD_UP; 		id = BTN_ID_DPAD_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DPAD_DOWN; 		id = BTN_ID_DPAD_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_DPAD_LEFT; 		id = BTN_ID_DPAD_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_DPAD_RIGHT; 	id = BTN_ID_DPAD_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A; 				id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 				id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 				id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 				id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START; 			id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 		id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
				else if (code==RAW_MENU1)	{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
				else if (code==RAW_MENU2)	{ btn = BTN_MENU; 			id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 			id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 			id = BTN_ID_L2; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 			id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 			id = BTN_ID_R2; }
			}
			else if (type==EV_ABS) {
				// LOG_info("axis: %i (%i)\n",code,value);
				// else if (code==RAW_LSX) pad.laxis.x = (value * 32767) / 4096;
				// else if (code==RAW_LSY) pad.laxis.y = (value * 32767) / 4096;
				// else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 4096;
				// else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 4096;
				
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
			if (event.type==EV_KEY && (event.code==RAW_MENU1 || event.code==RAW_MENU2) && event.value==0) return 1;
		}
	}
	return 0;
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("We compiled against SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("But we linked against SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	
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
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE); // we default to soft
	// SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "1", SDL_HINT_OVERRIDE); // TODO: not doing anything
	
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	// SDL_RenderSetVSync(vid.renderer, 0); // added SDL 2.0.18, m17 has 2.0.7
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565); // is this necessary any more?
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
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
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);
	SDL_Quit();

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	clearVideo();
}

void PLAT_setVsync(int vsync) {
	
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4 o

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	if (w>=FIXED_WIDTH && h>=FIXED_HEIGHT) {
		hard_scale = 1;
	}
	else if (h>=160) {
		hard_scale = 2;
	}
	else {
		hard_scale = 4;
	}

	// TODO: figure out how to ignore minarch upscale requests
	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i\n",w,h,p, hard_scale);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target	= SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}
void PLAT_setEffect(int effect) {
	// buh
}

void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_n16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) {
		resizeVideo(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH); // !!!???
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	((scaler_t)vid.blit->blit)(
		vid.blit->src,vid.buffer->pixels,
		vid.blit->src_w,vid.blit->src_h,vid.blit->src_p,
		// vid.blit->true_w,vid.blit->true_h,vid.blit->src_p, // TODO: fix to be confirmed, issue may not present on this platform
		vid.buffer->w,vid.buffer->h,vid.buffer->pitch
	);
	SDL_UnlockTexture(vid.texture);
	
	SDL_Texture* target = vid.texture;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){0,0,w,h};
	SDL_Rect* dst_rect = NULL;
	if (vid.blit->aspect==0) { // native or cropped
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (FIXED_WIDTH - w) / 2;
		int y = (FIXED_HEIGHT - h) / 2;
		dst_rect = &(SDL_Rect){x,y,w,h};
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
		dst_rect = &(SDL_Rect){x,y,w,h};
	}
	SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
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

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);

	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	char state[256];
	
	// works with old model
	getFile("/sys/class/udc/10180000.usb/state", state, 256);
	*is_charging = strncmp("not attached",state,strlen("not attached")); // I don't understand how this works, if it's a match it would equal 0 which is false...
	
	// nothing works with new model :sob:
	// getFile("/sys/class/power_supply/battery/status", state, 256);
	// *is_charging = exactMatch(state,"Charging\n");

	*charge = getInt("/sys/class/power_supply/battery/capacity");
}

void PLAT_enableBacklight(int enable) {
	// haven't figured out how to turn it off (or change brightness)
	if (!enable) {
		putInt("/sys/class/graphics/fb0/blank", 1); // clear
		SetRawBrightness(8001); // off
	}
	else {
		SetBrightness(GetBrightness());
	}
}

void PLAT_powerOff(void) {
	// system("leds_on");
	sleep(2);

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
	// M17 can go any speed you like as long as that speed is 1200000
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "M17";
}

int PLAT_isOnline(void) {
	return 0;
}