// rg35xxplus
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

int is_cubexx = 0;
int is_rg34xx = 0;
int on_hdmi = 0;

///////////////////////////////

#define RAW_UP		103
#define RAW_DOWN	108
#define RAW_LEFT	105
#define RAW_RIGHT	106
#define RAW_A		304
#define RAW_B		305
#define RAW_X		307
#define RAW_Y		306
#define RAW_START	311
#define RAW_SELECT	310
#define RAW_MENU	312
#define RAW_L1		308
#define RAW_L2		314
#define RAW_L3		313
#define RAW_R1		309
#define RAW_R2		315
#define RAW_R3		316
#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116
#define RAW_HATY	17
#define RAW_HATX	16
#define RAW_LSY		3
#define RAW_LSX		2
#define RAW_RSY		5
#define RAW_RSX		4

#define RAW_MENU1	RAW_L3
#define RAW_MENU2	RAW_R3

// TODO: thanks I hate it
// RG P01
#define RGP01_A			305
#define RGP01_B			304
#define RGP01_X			308
#define RGP01_Y			307
#define RGP01_START		315
#define RGP01_SELECT	314
#define RGP01_MENU		316
#define RGP01_L1		310
#define RGP01_L2		312
#define RGP01_L3		317
#define RGP01_R1		311
#define RGP01_R2		313
#define RGP01_R3		318
#define RGP01_LSY		1
#define RGP01_LSX		0
#define RGP01_RSY		5
#define RGP01_RSX		2
#define RGP01_MENU1		RGP01_L3
#define RGP01_MENU2		RGP01_R3

// X-box (8BitDo SN30 Pro)
#define XBOX_A		305
#define XBOX_B		304
#define XBOX_X		308
#define XBOX_Y		307
#define XBOX_START	315
#define XBOX_SELECT	314
#define XBOX_MENU	316
#define XBOX_L1		310
#define XBOX_L2		2
#define XBOX_L3		317
#define XBOX_R1		311
#define XBOX_R2		5
#define XBOX_R3		318
#define XBOX_LSY	1
#define XBOX_LSX	0
#define XBOX_RSY	4
#define XBOX_RSX	3
#define XBOX_MENU1	XBOX_L3
#define XBOX_MENU2	XBOX_R3

typedef enum GamepadType {
	kGamepadTypeUnknown,
	kGamepadTypeRGP01,
	kGamepadTypeXbox,
} GamepadType;

#define INPUT_COUNT 3
static int inputs[INPUT_COUNT];

#define kPadIndex 2
static GamepadType pad_type = kGamepadTypeUnknown;

#define LID_PATH "/sys/class/power_supply/axp2202-battery/hallkey"
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}
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

static void checkForGamepad(void) {
	uint32_t now = SDL_GetTicks();
	static uint32_t last_check = 0;
	if (last_check==0 || now-last_check>2000) {
		last_check = now;
		int connected = exists("/dev/input/event3");
		if (inputs[kPadIndex]<0 && connected) {
			LOG_info("Connecting gamepad: ");
			char pad_name[256];
			getFile("/sys/class/input/event3/device/name", pad_name, 256);
			if (containsString(pad_name,"Anbernic")) {
				LOG_info("P01\n");
				pad_type = kGamepadTypeRGP01;
			}
			else if (containsString(pad_name, "Microsoft")) {
				LOG_info("Xbox\n");
				pad_type = kGamepadTypeXbox;
			}
			else {
				LOG_info("Unknown\n");
				pad_type = kGamepadTypeUnknown;
			}
			
			inputs[kPadIndex] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		}
		else if (inputs[kPadIndex]>=0 && !connected) {
			LOG_info("Gamepad disconnected\n");
			close(inputs[kPadIndex]);
			inputs[kPadIndex] = -1;
			pad_type = kGamepadTypeUnknown;
		}
	}
}

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[1] = open("/dev/input/event1", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	inputs[kPadIndex] = -1; 
	checkForGamepad();
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
	
	checkForGamepad();
	
	// the actual poll
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		if (input<0) continue;
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
				// LOG_info("key event: %i (%i)\n", code,pressed);
				if (i==kPadIndex) {
					if (pad_type==kGamepadTypeRGP01) {
							 if (code==RGP01_A)			{ btn = BTN_A; 			id = BTN_ID_A; }
						else if (code==RGP01_B)			{ btn = BTN_B; 			id = BTN_ID_B; }
						else if (code==RGP01_X)			{ btn = BTN_X; 			id = BTN_ID_X; }
						else if (code==RGP01_Y)			{ btn = BTN_Y; 			id = BTN_ID_Y; }
						else if (code==RGP01_START)		{ btn = BTN_START; 		id = BTN_ID_START; }
						else if (code==RGP01_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
						else if (code==RGP01_MENU)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==RGP01_MENU1)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==RGP01_MENU2)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==RGP01_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
						else if (code==RGP01_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
						else if (code==RGP01_L3)		{ btn = BTN_L3; 		id = BTN_ID_L3; }
						else if (code==RGP01_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
						else if (code==RGP01_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
						else if (code==RGP01_R3)		{ btn = BTN_R3; 		id = BTN_ID_R3; }
					}
					else if (pad_type==kGamepadTypeXbox) {
							 if (code==XBOX_A)			{ btn = BTN_A; 			id = BTN_ID_A; }
						else if (code==XBOX_B)			{ btn = BTN_B; 			id = BTN_ID_B; }
						else if (code==XBOX_X)			{ btn = BTN_X; 			id = BTN_ID_X; }
						else if (code==XBOX_Y)			{ btn = BTN_Y; 			id = BTN_ID_Y; }
						else if (code==XBOX_START)		{ btn = BTN_START; 		id = BTN_ID_START; }
						else if (code==XBOX_SELECT)		{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
						else if (code==XBOX_MENU)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==XBOX_MENU1)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==XBOX_MENU2)		{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
						else if (code==XBOX_L1)			{ btn = BTN_L1; 		id = BTN_ID_L1; }
						else if (code==XBOX_L3)			{ btn = BTN_L3; 		id = BTN_ID_L3; }
						else if (code==XBOX_R1)			{ btn = BTN_R1; 		id = BTN_ID_R1; }
						else if (code==XBOX_R3)			{ btn = BTN_R3; 		id = BTN_ID_R3; }
					}
				}
				else {
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
			}
			else if (type==EV_ABS) {
				// LOG_info("abs event: %i (%i)\n", code,value);
				// { up, down, left, right }
				if (code==RAW_HATY || code==RAW_HATX) {
					if (value>1) continue; // ignore repeats
			
					int hats[4] = {-1,-1,-1,-1}; // -1=no change,1=pressed,0=released
					if (code==RAW_HATY) {
						hats[0] = value==-1; // up
						hats[1] = value==1; // down
					}
					else if (code==RAW_HATX) { // left/right
						hats[2] = value==-1; // left
						hats[3] = value==1; // right
					}
				
					for (id=0; id<4; id++) {
						int state = hats[id];
						btn = 1 << id;
						if (state==0) {
							pad.is_pressed		&= ~btn; // unset
							pad.just_repeated	&= ~btn; // unset
							pad.just_released	|= btn; // set
						}
						else if (state==1 && (pad.is_pressed & btn)==BTN_NONE) {
							pad.just_pressed	|= btn; // set
							pad.just_repeated	|= btn; // set
							pad.is_pressed		|= btn; // set
							pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
						}
					}
					
					btn = BTN_NONE; // already handled, force continue
				}
				else if (i==kPadIndex) {
					if (pad_type==kGamepadTypeRGP01) {
							 if (code==RGP01_LSX) { pad.laxis.x = ((value-128) * 32767) / 128; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
						else if (code==RGP01_LSY) { pad.laxis.y = ((value-128) * 32767) / 128; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
						else if (code==RGP01_RSX) pad.raxis.x = ((value-128) * 32767) / 128;
						else if (code==RGP01_RSY) pad.raxis.y = ((value-128) * 32767) / 128;
					}
					else if (pad_type==kGamepadTypeXbox) {
							 if (code==XBOX_LSX) { pad.laxis.x = value; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
						else if (code==XBOX_LSY) { pad.laxis.y = value; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
						else if (code==XBOX_RSX) pad.raxis.x = value;
						else if (code==XBOX_RSY) pad.raxis.y = value;
						else if (code==XBOX_L2) { pressed = value>0; btn = BTN_L2; id = BTN_ID_L2; }
						else if (code==XBOX_R2) { pressed = value>0; btn = BTN_R2; id = BTN_ID_R2; }
					}
				}
				else {
						 if (code==RAW_LSX) { pad.laxis.x = (value * 32767) / 4096; PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick+PAD_REPEAT_DELAY); }
					else if (code==RAW_LSY) { pad.laxis.y = (value * 32767) / 4096; PAD_setAnalog(BTN_ID_ANALOG_UP,   BTN_ID_ANALOG_DOWN,  pad.laxis.y, tick+PAD_REPEAT_DELAY); }
					else if (code==RAW_RSX) pad.raxis.x = (value * 32767) / 4096;
					else if (code==RAW_RSY) pad.raxis.y = (value * 32767) / 4096;
				}
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
	
	if (lid.has_lid && PLAT_lidChanged(NULL)) pad.just_released |= BTN_SLEEP;
}

int PLAT_shouldWake(void) {
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open) return 1;
	
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open) return 0;  // do it here so we eat the input
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////

// based on rgb30 + tg5040 + m17
#define HDMI_STATE_PATH "/sys/class/switch/hdmi/cable.0/state" // TODO: can detect but doesn't update automatically
#define BLANK_PATH "/sys/class/graphics/fb0/blank"

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
SDL_Surface* PLAT_initVideo(void) {
	// LOG_info("PLAT_initVideo\n");
	
	char* model = getenv("RGXX_MODEL"); // TODO: use device?
	is_cubexx = exactMatch("RGcubexx", model);
	is_rg34xx = exactMatch("RG34xx", model);
	
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

	// SDL_SetHint(SDL_HINT_RENDER_VSYNC,"0"); // ignored

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	if (getInt(HDMI_STATE_PATH)) { // can't use getHDMI() from settings because it hasn't be initialized yet
		w = HDMI_WIDTH;
		h = HDMI_HEIGHT;
		p = HDMI_PITCH;
		on_hdmi = 1;
	}
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	// LOG_info("window size: %ix%i\n", w,h);
	
	SDL_DisplayMode mode;
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
	//
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
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
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	// system("cat /dev/zero > /dev/fb0 2>/dev/null");
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
	// buh
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

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
static void rgb565_to_rgb888(uint32_t rgb565, uint8_t *r, uint8_t *g, uint8_t *b) {
    // Extract the red component (5 bits)
    uint8_t red = (rgb565 >> 11) & 0x1F;
    // Extract the green component (6 bits)
    uint8_t green = (rgb565 >> 5) & 0x3F;
    // Extract the blue component (5 bits)
    uint8_t blue = rgb565 & 0x1F;

    // Scale the values to 8-bit range
    *r = (red << 3) | (red >> 2);
    *g = (green << 2) | (green >> 4);
    *b = (blue << 3) | (blue >> 2);
}
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
void PLAT_setEffect(int next_type) {
	effect.next_type = next_type;
}
void PLAT_setEffectColor(int next_color) {
	effect.next_color = next_color;
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// LOG_info("getScaler for scale: %i\n", renderer->scale);
	effect.next_scale = renderer->scale;
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

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

int PLAT_supportsOverscan(void) { return is_cubexx; }

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
	PLAT_getBatteryStatusFine(is_charging, charge);

	// worry less about battery and more about the game you're playing
	     if (*charge>80) *charge = 100;
	else if (*charge>60) *charge =  80;
	else if (*charge>40) *charge =  60;
	else if (*charge>20) *charge =  40;
	else if (*charge>10) *charge =  20;
	else           		 *charge =  10;
}

void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	*charge = getInt("/sys/class/power_supply/axp2202-battery/capacity");

	// wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/power_supply/axp2202-battery/work_led"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt(BLANK_PATH, FB_BLANK_UNBLANK); // wake
		SetBrightness(GetBrightness());
		putInt(LED_PATH,0);
	}
	else {
		putInt(BLANK_PATH, FB_BLANK_POWERDOWN); // sleep
		SetRawBrightness(0);
		putInt(LED_PATH,1);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 1 > /sys/class/power_supply/axp2202-battery/work_led");
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	// system("cat /dev/zero > /dev/fb0 2>/dev/null");
	// system("shutdown");
	// while (1) pause(); // lolwat
	
	// touch("/tmp/poweroff");
	// sync();
	// system("touch /tmp/poweroff && sync");
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// TODO: why wasn't this ever implemented?
}

#define RUMBLE_PATH "/sys/class/power_supply/axp2202-battery/moto"
void PLAT_setRumble(int strength) {
	if (GetHDMI()) return; // assume we're using a controller?
	putInt(RUMBLE_PATH, strength?1:0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

static char model[256];
char* PLAT_getModel(void) {
	// firmware "strings /mnt/vendor/bin/dmenu.bin | grep ^20"
	char* _model = getenv("RGXX_MODEL");
	if (_model!=NULL) {
		if (exactMatch(_model,"RGcubexx")) _model = "RG CubeXX";
		
		sprintf(model, "Anbernic %s", _model);
		char* tmp = strrchr(model, '_');
		if (tmp) *tmp = '\0';
		return model;
	}
	return "Anbernic RG*XX";
}

int PLAT_isOnline(void) {
	return online;
}