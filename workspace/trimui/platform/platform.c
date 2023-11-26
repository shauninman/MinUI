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

static struct VID_Context {
	SDL_Surface* screen;
	GFX_Renderer* renderer;
} vid;
static int _;

// TODO: drop custom SDL, use HW surface and pan (or will we stil lose vsync without eggs TCON threading?)

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);
	memset(vid.screen->pixels, 0, FIXED_SIZE);

	#define TRIMUI_SHOW unused1
	vid.screen->TRIMUI_SHOW = 1;
	putenv("trimui_show=yes");
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	putenv("trimui_show=no");
	vid.screen->TRIMUI_SHOW = 0;
	
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
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_vsync(int remaining) {
	// ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
	if (remaining>0) SDL_Delay(remaining);
}

// taken from minarch
static void ma_scale_nn(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	uint16_t* s = src;
	uint16_t* d = dst;
	
	sp /= FIXED_BPP;
	dp /= FIXED_BPP;
	
	int rx = 0;
	int ry = 0;
	int rw = dw;
	int rh = dh;
	
	int mx = (sw << 16) / rw;
	int my = (sh << 16) / rh;
	int ox = (0 << 16);
	int sx = ox;
	int sy = (0 << 16);
	int lr = -1;
	int sr = 0;
	int dr = ry * dp;
	int cp = dp * FIXED_BPP;
	
	for (int dy=0; dy<rh; dy++) {
		sx = ox;
		sr = (sy >> 16) * sp;
		if (sr==lr) {
			memcpy(d+dr,d+dr-dp,cp);
		}
		else {
	        for (int dx=0; dx<rw; dx++) {
	            d[dr + rx + dx] = s[sr + (sx >> 16)];
				sx += mx;
	        }
		}
		lr = sr;
		sy += my;
		dr += dp;
    }
}
// taken from picoarch
static void pa_scale_nn(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp)
{
	int dy = -dh;
	unsigned lines = sh;
	int copy = 0;
	size_t cpy_w = dw * FIXED_BPP;

	while (lines) {
		int dx = -dw;
		const uint16_t *psrc16 = src;
		uint16_t *pdst16 = dst;

		if (copy) {
			copy = 0;
			memcpy(dst, dst - FIXED_PITCH, cpy_w);
			dst += FIXED_PITCH;
			dy += sh;
		} else if (dy < 0) {
			int col = sw;
			while(col--) {
				while (dx < 0) {
					*pdst16++ = *psrc16;
					dx += sw;
				}

				dx -= dw;
				psrc16++;
			}

			dst += FIXED_PITCH;
			dy += sh;
		}

		if (dy >= 0) {
			dy -= dh;
			src += sp;
			lines--;
		} else {
			copy = 1;
		}
	}
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	GFX_freeAAScaler();
	switch (renderer->scale) {
		case  6: return scale6x6_c16;
		case  5: return scale5x5_c16;
		case  4: return scale4x4_c16;
		case  3: return scale3x3_c16;
		case  2: return scale2x2_c16;
		
		// nn (both ma and pa versions) performs better than aa (~54fps vs ~48fps) in FFVI in gpsp (native is ~58fps)
		// hmm, even TLoZ:LA in gbc at native is ~58fps...
		// but M:ZM in gpsp runs at 60fps with aspect scaler, unless there's a lot onscreen

		// CIII:DC in fceumm at native runs at an unplayable 54fps
		// without blit it runs at 55fps
		// without blit and flip it's a steady 60fps (by sound)
		// without flip it's still noticeably clippy (I'd guess about 57-ish)
		// stock sdl, HWSurface + no flip = 58fps
		// 	then without snd = 59fps

		// case -1: return ma_scale_nn;
		// case -1: return pa_scale_nn;
		case -1: return GFX_getAAScaler(renderer);
		
		default: return scale1x1_c16; // this includes crop (0)
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

static int getBatteryLevel(void) { // 40-47
	// returns the average of the last 10 readings
	#define kBatteryReadings 10
	static int values[kBatteryReadings];
	static int total;
	static int i = 0;
	static int ready = 0;
	
	// get the current value
	int value = -1;
	FILE* file = fopen("/sys/devices/soc/1c23400.battery/adc", "r");
	if (file!=NULL) {
		fscanf(file, "%i", &value);
		fclose(file);
	}
	
	// first run, fill up the buffer
	if (!ready) {
		for (int i=0; i<kBatteryReadings; i++) {
			values[i] = value;
		}
		total = value * kBatteryReadings;
		ready = 1;
	}
	// subsequent calls, update average
	else {
		total -= values[i];
		values[i] = value;
		total += value;
		i += 1;
		if (i>=kBatteryReadings) i -= kBatteryReadings;
		value = total / kBatteryReadings;
	}
	return value;
}

void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	char state[256];
	getFile("/sys/class/android_usb/android0/state", state, 256);
	*is_charging = strncmp("DISCONNECTED",state,strlen("DISCONNECTED"));
	
	int i = getBatteryLevel();
	// worry less about battery and more about the game you're playing
	     if (i>45) *charge = 100;
	else if (i>44) *charge =  80;
	else if (i>43) *charge =  60;
	else if (i>42) *charge =  40;
	else if (i>41) *charge =  20;
	else           *charge =  10;
}

void PLAT_enableBacklight(int enable) {
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
	// TODO: this makes minarch crashy
	
	// uint32_t mhz = 0;
	// switch (speed) {
	// 	case CPU_SPEED_MENU: 		mhz = 0x02641021; break; // 612MHz
	// 	case CPU_SPEED_POWERSAVE:	mhz = 0x02641021; break; // 612MHz
	// 	case CPU_SPEED_NORMAL: 		mhz = 0x02d01d22; break; // 720MHz
	// 	case CPU_SPEED_PERFORMANCE: mhz = 0x03601a32; break; // 864MHz
	// }
	//
	// volatile uint32_t* mem;
	// volatile uint8_t memdev = 0;
	// memdev = open("/dev/mem", O_RDWR);
	// if (memdev>0) {
	// 	mem = (uint32_t*)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, 0x01c20000);
	// 	if (mem==MAP_FAILED) {
	// 		puts("Could not mmap CPU hardware registers!");
	// 		close(memdev);
	// 		return;
	// 	}
	// }
	// else puts("Could not open /dev/mem");
	//
	// uint32_t v = mem[0];
	// v &= 0xffff0000;
	// v |= (mhz & 0x0000ffff);
	// mem[0] = v;
	//
	// if (memdev>0) close(memdev);
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Trimui Model S";
}

int PLAT_isOnline(void) {
	return 0;
}