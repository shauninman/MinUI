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

///////////////////////////////////////

#include <math.h>

struct blend_args {
	int w_ratio_in;
	int w_ratio_out;
	uint16_t w_bp[2];
	int h_ratio_in;
	int h_ratio_out;
	uint16_t h_bp[2];
	uint16_t *blend_line;
} blend_args;

#if __ARM_ARCH >= 5
static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	asm ("eor %0, %2, %3\r\n"
	     "and %0, %0, %1\r\n"
	     "add %0, %3, %0\r\n"
	     "add %0, %0, %2\r\n"
	     "lsr %0, %0, #1\r\n"
	     : "=&r" (ret) : "r" (lowbits), "r" (c1), "r" (c2) : );
	return ret;
}

static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	asm ("eor %0, %3, %1\r\n"
	     "and %0, %0, %2\r\n"
	     "adds %0, %1, %0\r\n"
	     "and %1, %1, #0\r\n"
	     "movcs %1, #0x80000000\r\n"
	     "adds %0, %0, %3\r\n"
	     "rrx %0, %0\r\n"
	     "orr %0, %0, %1\r\n"
	     : "=&r" (ret), "+r" (c2) : "r" (lowbits), "r" (c1) : "cc" );

	return ret;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#else

static inline uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821))>>1;
}

static inline uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) << 31;

	return (ret >> 1) | of;
}

#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#endif


#define AVERAGE16(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE16_NOCHK((c1), (c2)))
#define AVERAGE16_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE16_NOCHK(AVERAGE16_NOCHK((c1), (c2)), (c2))))

#define AVERAGE32(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE32_NOCHK((c1), (c2)))
#define AVERAGE32_1_3(c1, c2) ((c1) == (c2) ? (c1) : (AVERAGE32_NOCHK(AVERAGE32_NOCHK((c1), (c2)), (c2))))

static inline int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

/* Generic blend based on % of dest pixel in next src pixel, using
 * rough quintiles: aaaa, aaab, aabb, abbb, bbbb. Quintile breakpoints
 * can be adjusted for sharper or smoother blending. Default 0-20%,
 * 20%-50% (round down), 50%(down)-50%(up), 50%(round up)-80%,
 * 80%-100%. An aspect scaler from picoarch */
static void scaleBlend(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h, uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_p) {
// static void scale_blend(unsigned w, unsigned h, size_t pitch, const void *src, void *dst) {
	int dy = 0;
	int lines = h;

	int rat_w = blend_args.w_ratio_in;
	int rat_dst_w = blend_args.w_ratio_out;
	uint16_t *bw = blend_args.w_bp;

	int rat_h = blend_args.h_ratio_in;
	int rat_dst_h = blend_args.h_ratio_out;
	uint16_t *bh = blend_args.h_bp;

	// dst += dst_offs;

	while (lines--) {
		while (dy < rat_dst_h) {
			uint16_t *dst16 = (uint16_t *)dst;
			uint16_t *pblend = (uint16_t *)blend_args.blend_line;
			int col = w;
			int dx = 0;

			uint16_t *pnext = (uint16_t *)(src + pitch);
			if (!lines)
				pnext -= (pitch / sizeof(uint16_t));

			if (dy > rat_dst_h - bh[0]) {
				pblend = pnext;
			} else if (dy <= bh[0]) {
				/* Drops const, won't get touched later though */
				pblend = (uint16_t *)src;
			} else {
				const uint32_t *src32 = (const uint32_t *)src;
				const uint32_t *pnext32 = (const uint32_t *)pnext;
				uint32_t *pblend32 = (uint32_t *)pblend;
				int count = w / 2;

				if (dy <= bh[1]) {
					const uint32_t *tmp = pnext32;
					pnext32 = src32;
					src32 = tmp;
				}

				if (dy > rat_dst_h - bh[1] || dy <= bh[1]) {
					while(count--) {
						*pblend32++ = AVERAGE32_1_3(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				} else {
					while(count--) {
						*pblend32++ = AVERAGE32(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				}
			}

			while (col--) {
				uint16_t a, b, out;

				a = *pblend;
				b = *(pblend+1);

				while (dx < rat_dst_w) {
					if (a == b) {
						out = a;
					} else if (dx > rat_dst_w - bw[0]) { // top quintile, bbbb
						out = b;
					} else if (dx <= bw[0]) { // last quintile, aaaa
						out = a;
					} else {
						if (dx > rat_dst_w - bw[1]) { // 2nd quintile, abbb
							a = AVERAGE16_NOCHK(a,b);
						} else if (dx <= bw[1]) { // 4th quintile, aaab
							b = AVERAGE16_NOCHK(a,b);
						}

						out = AVERAGE16_NOCHK(a, b); // also 3rd quintile, aabb
					}
					*dst16++ = out;
					dx += rat_w;
				}

				dx -= rat_dst_w;
				pblend++;
			}

			dy += rat_h;
			dst += dst_p;
		}

		dy -= rat_dst_h;
		src += pitch;
	}
}

///////////////////////////////////////

static scaler_t getScaleBlend(GFX_Renderer* renderer) {
	LOG_info("scaleBlend (%i)\n", renderer->scale);
	int gcd_w, div_w, gcd_h, div_h;
	blend_args.blend_line = calloc(renderer->src_w, sizeof(uint16_t));

	gcd_w = gcd(renderer->src_w, renderer->dst_w);
	blend_args.w_ratio_in = renderer->src_w / gcd_w;
	blend_args.w_ratio_out = renderer->dst_w / gcd_w;

	div_w = round(blend_args.w_ratio_out / 5.0);
	blend_args.w_bp[0] = div_w;
	blend_args.w_bp[1] = blend_args.w_ratio_out >> 1;

	gcd_h = gcd(renderer->src_h, renderer->dst_h);
	blend_args.h_ratio_in = renderer->src_h / gcd_h;
	blend_args.h_ratio_out = renderer->dst_h / gcd_h;

	div_h = round(blend_args.h_ratio_out / 5.0);
	blend_args.h_bp[0] = div_h;
	blend_args.h_bp[1] = blend_args.h_ratio_out >> 1;
	
	return scaleBlend;
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	if (blend_args.blend_line != NULL) {
		free(blend_args.blend_line);
		blend_args.blend_line = NULL;
	}
	switch (renderer->scale) {
		case  6: return scale6x6_n16;
		case  5: return scale5x5_n16;
		case  4: return scale4x4_n16;
		case  3: return scale3x3_n16;
		case  2: return scale2x2_n16;
		case -1: return getScaleBlend(renderer);
		default: return scale1x1_n16; // this includes crop (0)
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	void* src = renderer->src + (renderer->src_y * renderer->src_p) + (renderer->src_x * FIXED_BPP);
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
	// memcpy(dst,renderer->src, size);
	
	// native
	// int cpy_w = MIN(renderer->dst_w*2, renderer->src_w*2);
	// uint8_t* dp = dst;
	// uint8_t* sp = renderer->src;
	// for (int y=0; y<renderer->src_h; y++) {
	// 		memcpy(dp,sp,cpy_w);
	// 		dp += FIXED_PITCH; //renderer->dst_p;
	// 		sp += renderer->src_p;
	// }
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
	LOG_info("PLAT_powerOff\n");
	
	sleep(2);
	is_powering_off = 1;
	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	POW_quit();
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