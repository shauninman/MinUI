// trimui
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

// TODO: not necessary
#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK);
}
void PLAT_quitInput(void) {
	close(inputs[1]);
	close(inputs[0]);
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01

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
			if (event.value>1) continue; // ignore repeats
			if (event.type!=EV_KEY) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
			
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				pressed = value;
				// LOG_info("key event: %i (%i) ticks: %i\n", code,pressed,tick);
					 if (code==CODE_UP) 		{ btn = BTN_UP; 		id = BTN_ID_UP; }
	 			else if (code==CODE_DOWN)		{ btn = BTN_DOWN; 		id = BTN_ID_DOWN; }
				else if (code==CODE_LEFT)		{ btn = BTN_LEFT; 		id = BTN_ID_LEFT; }
				else if (code==CODE_RIGHT)		{ btn = BTN_RIGHT; 		id = BTN_ID_RIGHT; }
				else if (code==CODE_A)			{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==CODE_B)			{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==CODE_X)			{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==CODE_Y)			{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==CODE_START)		{ btn = BTN_START; 		id = BTN_ID_START; }
				else if (code==CODE_SELECT)		{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==CODE_MENU)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==CODE_L1)			{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==CODE_L2)			{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==CODE_R1)			{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==CODE_R2)			{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==CODE_PLUS)		{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==CODE_MINUS)		{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==CODE_POWER)		{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
				else if (code==CODE_POWEROFF)	{ btn = BTN_POWEROFF; 	id = BTN_ID_POWEROFF; }
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
	int input = inputs[1];
	static struct input_event event;
	while (read(input, &event, sizeof(event))==sizeof(event)) {
		if (event.type==EV_KEY && event.code==CODE_POWER && event.value==0) return 1;
	}
	return 0;
}

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	GFX_Renderer* renderer;
} vid;
static int _;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_HWSURFACE | SDL_TRIPLEBUF);
	PLAT_clearVideo(vid.screen);
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	memset(vid.screen->pixels, 0, FIXED_SIZE);
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	PLAT_clearVideo(vid.screen);
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
	GFX_freeAAScaler();
	switch (renderer->scale) {
		case  6: return scale6x6_c16;
		case  5: return scale5x5_c16;
		case  4: return scale4x4_c16;
		case  3: return scale3x3_c16;
		case  2: return scale2x2_c16;
		case -1: return GFX_getAAScaler(renderer);
		default: return scale1x1_c16;
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
	// buh
}


///////////////////////////////

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/usb/online");

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
	// putInt("/sys/class/graphics/fb0/blank", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
	if (enable) SetBrightness(GetBrightness());
	else SetRawBrightness(0);
}

void PLAT_powerOff(void) {
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
	// buh
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "GKD Pixel";
}

int PLAT_isOnline(void) {
	return 0;
}