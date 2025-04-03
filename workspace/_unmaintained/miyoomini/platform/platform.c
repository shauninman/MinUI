// miyoomini
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <msettings.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"
#include "scaler.h"

///////////////////////////////
// based on eggs GFXSample_rev15

#include <mi_sys.h>
#include <mi_gfx.h>

int is_plus = 0;

#define	pixelsPa	unused1
#define ALIGN4K(val)	((val+4095)&(~4095))

//
//	Get GFX_ColorFmt from SDL_Surface
//
static inline MI_GFX_ColorFmt_e	GFX_ColorFmt(SDL_Surface *surface) {
	if (surface) {
		if (surface->format->BytesPerPixel == 2) {
			if (surface->format->Amask == 0x0000) return E_MI_GFX_FMT_RGB565;
			if (surface->format->Amask == 0x8000) return E_MI_GFX_FMT_ARGB1555;
			if (surface->format->Amask == 0xF000) return E_MI_GFX_FMT_ARGB4444;
			if (surface->format->Amask == 0x0001) return E_MI_GFX_FMT_RGBA5551;
			if (surface->format->Amask == 0x000F) return E_MI_GFX_FMT_RGBA4444;
			return E_MI_GFX_FMT_RGB565;
		}
		if (surface->format->Bmask == 0x000000FF) return E_MI_GFX_FMT_ARGB8888;
		if (surface->format->Rmask == 0x000000FF) return E_MI_GFX_FMT_ABGR8888;
	}
	return E_MI_GFX_FMT_ARGB8888;
}

//
//	Flush write cache of needed segments
//		x and w are not considered since 4K units
//
static inline void FlushCacheNeeded(void* pixels, uint32_t pitch, uint32_t y, uint32_t h) {
	uintptr_t pixptr = (uintptr_t)pixels;
	uintptr_t startaddress = (pixptr + pitch*y)&(~4095);
	uint32_t size = ALIGN4K(pixptr + pitch*(y+h)) - startaddress;
	if (size) MI_SYS_FlushInvCache((void*)startaddress, size);
}

//
//	GFX BlitSurface (MI_GFX ver) / in place of SDL_BlitSurface
//		with scale/bpp convert and rotate/mirror
//		rotate : 1 = 90 / 2 = 180 / 3 = 270
//		mirror : 1 = Horizontal / 2 = Vertical / 3 = Both
//		nowait : 0 = wait until done / 1 = no wait
//
static inline void GFX_BlitSurfaceExec(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, uint32_t rotate, uint32_t mirror, uint32_t nowait) {
	if ((src)&&(dst)&&(src->pixelsPa)&&(dst->pixelsPa)) {
		MI_GFX_Surface_t Src;
		MI_GFX_Surface_t Dst;
		MI_GFX_Rect_t SrcRect;
		MI_GFX_Rect_t DstRect;
		MI_GFX_Opt_t Opt;
		MI_U16 Fence;

		Src.phyAddr = src->pixelsPa;
		Src.u32Width = src->w;
		Src.u32Height = src->h;
		Src.u32Stride = src->pitch;
		Src.eColorFmt = GFX_ColorFmt(src);
		if (srcrect) {
			SrcRect.s32Xpos = srcrect->x;
			SrcRect.s32Ypos = srcrect->y;
			SrcRect.u32Width = srcrect->w;
			SrcRect.u32Height = srcrect->h;
		} else {
			SrcRect.s32Xpos = 0;
			SrcRect.s32Ypos = 0;
			SrcRect.u32Width = Src.u32Width;
			SrcRect.u32Height = Src.u32Height;
		}
		FlushCacheNeeded(src->pixels, src->pitch, SrcRect.s32Ypos, SrcRect.u32Height);

		Dst.phyAddr = dst->pixelsPa;
		Dst.u32Width = dst->w;
		Dst.u32Height = dst->h;
		Dst.u32Stride = dst->pitch;
		Dst.eColorFmt = GFX_ColorFmt(dst);
		if (dstrect) {
			DstRect.s32Xpos = dstrect->x;
			DstRect.s32Ypos = dstrect->y;
			if (dstrect->w|dstrect->h) {
				DstRect.u32Width = dstrect->w;
				DstRect.u32Height = dstrect->h;
			} else {
				DstRect.u32Width = SrcRect.u32Width;
				DstRect.u32Height = SrcRect.u32Height;
			}
		} else {
			DstRect.s32Xpos = 0;
			DstRect.s32Ypos = 0;
			DstRect.u32Width = Dst.u32Width;
			DstRect.u32Height = Dst.u32Height;
		}
		if (rotate & 1) FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Width);
		else FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Height);

		memset(&Opt, 0, sizeof(Opt));
		if (src->flags & SDL_SRCALPHA) {
			Opt.eDstDfbBldOp = E_MI_GFX_DFB_BLD_INVSRCALPHA;
			if (src->format->alpha != SDL_ALPHA_OPAQUE) {
				Opt.u32GlobalSrcConstColor = (src->format->alpha << (src->format->Ashift - src->format->Aloss)) & src->format->Amask;
				Opt.eDFBBlendFlag = (MI_Gfx_DfbBlendFlags_e)
						   (E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY | E_MI_GFX_DFB_BLEND_COLORALPHA | E_MI_GFX_DFB_BLEND_ALPHACHANNEL);
			} else	Opt.eDFBBlendFlag = E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY;
		}
		if (src->flags & SDL_SRCCOLORKEY) {
			Opt.stSrcColorKeyInfo.bEnColorKey = TRUE;
			Opt.stSrcColorKeyInfo.eCKeyFmt = Src.eColorFmt;
			Opt.stSrcColorKeyInfo.eCKeyOp = E_MI_GFX_RGB_OP_EQUAL;
			Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorStart =
			Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorEnd = src->format->colorkey;
		}
		Opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
		Opt.eRotate = (MI_GFX_Rotate_e)rotate;
		Opt.eMirror = (MI_GFX_Mirror_e)mirror;
		Opt.stClipRect.s32Xpos = dst->clip_rect.x;
		Opt.stClipRect.s32Ypos = dst->clip_rect.y;
		Opt.stClipRect.u32Width = dst->clip_rect.w;
		Opt.stClipRect.u32Height = dst->clip_rect.h;

		MI_GFX_BitBlit(&Src, &SrcRect, &Dst, &DstRect, &Opt, &Fence);
		if (!nowait) MI_GFX_WaitAllDone(FALSE, Fence);
	} else SDL_BlitSurface(src, srcrect, dst, dstrect);
}

///////////////////////////////

void PLAT_initInput(void) {
	// buh
}
void PLAT_quitInput(void) {
	// buh
}

///////////////////////////////

typedef struct HWBuffer {
	MI_PHY padd;
	void* vadd;
} HWBuffer;

static struct VID_Context {
	SDL_Surface* video;
	SDL_Surface* screen;
	HWBuffer buffer;
	
	int page;
	int width;
	int height;
	int pitch;
	
	int direct;
	int cleared;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	is_plus = exists("/customer/app/axp_test");
	
	putenv("SDL_HIDE_BATTERY=1"); // using MiniUI's custom SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);
	
	vid.video = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);
	
	int buffer_size = ALIGN4K(PAGE_SIZE) * PAGE_COUNT;
	MI_SYS_MMA_Alloc(NULL, ALIGN4K(buffer_size), &vid.buffer.padd);
	MI_SYS_Mmap(vid.buffer.padd, ALIGN4K(buffer_size), &vid.buffer.vadd, true);
	// memset(vid.buffer.vadd, 0, PAGE_SIZE);
	
	vid.page = 1;
	vid.direct = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	vid.cleared = 0;
	
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE),vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
	vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	
	return vid.direct ? vid.video : vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	
	MI_SYS_Munmap(vid.buffer.vadd, ALIGN4K(PAGE_SIZE));
	MI_SYS_MMA_Free(vid.buffer.padd);
	
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	MI_SYS_FlushInvCache(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE), ALIGN4K(PAGE_SIZE));
	MI_SYS_MemsetPa(vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE), 0, PAGE_SIZE);
	SDL_FillRect(screen, NULL, 0);
	// memset(screen->pixels, 0, PAGE_SIZE); // this causes crashing
}
void PLAT_clearAll(void) {
	// buh
	// MI_SYS_FlushInvCache(vid.buffer.vadd, ALIGN4K(PAGE_SIZE));
	// MI_SYS_MemsetPa(vid.buffer.padd, 0, PAGE_SIZE);
	
	PLAT_clearVideo(vid.screen); // clear backbuffer
	vid.cleared = 1; // defer clearing frontbuffer until offscreen
}

void PLAT_setVsync(int vsync) {
	// TODO: Prevent Tearing/Vsync
	// isn't a 1:1 mapping of what's happening here...
	if (vsync==VSYNC_OFF) {
		putenv("GFX_FLIPWAIT=0");
		putenv("GFX_BLOCKING=0");
	}
	else if (vsync==VSYNC_LENIENT) { // this is tear free but results in flicker issues with settings overlay
		putenv("GFX_FLIPWAIT=0");
		putenv("GFX_BLOCKING=1");
	}
	else if (vsync==VSYNC_STRICT) { // this actually introduces tearing but reduces/eliminates judder and fixes flicker issues with settings overlay
		putenv("GFX_FLIPWAIT=1");
		putenv("GFX_BLOCKING=1");
	}
	SDL_GetVideoInfo();
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	vid.direct = w==FIXED_WIDTH && h==FIXED_HEIGHT && pitch==FIXED_PITCH;
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;
	
	if (vid.direct) memset(vid.video->pixels, 0, vid.pitch * vid.height);
	else {
		vid.screen->pixels = NULL;
		vid.screen->pixelsPa = NULL; // otherwise custom SDL will try to free it?
		SDL_FreeSurface(vid.screen);
		
		vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE),vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
		memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	}
	
	return vid.direct ? vid.video : vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
static int next_effect = EFFECT_NONE;
static int effect_type = EFFECT_NONE;
void PLAT_setSharpness(int sharpness) {
	// force effect to reload
	// on scaling change
	if (effect_type>=EFFECT_NONE) next_effect = effect_type;
	effect_type = -1;
}

void PLAT_setEffect(int effect) {
	next_effect = effect;
}

void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	if (effect_type==EFFECT_LINE) {
		switch (renderer->scale) {
			case 4:  return scale4x_line;
			case 3:  return scale3x_line;
			case 2:  return scale2x_line;
			default: return scale1x_line;
		}
	}
	else if (effect_type==EFFECT_GRID) {
		switch (renderer->scale) {
			case 3:  return scale3x_grid;
			case 2:  return scale2x_grid;
		}
	}
	
	switch (renderer->scale) {
		case 6:  return scale6x6_n16;
		case 5:  return scale5x5_n16;
		case 4:  return scale4x4_n16;
		case 3:  return scale3x3_n16;
		case 2:  return scale2x2_n16;
		default: return scale1x1_n16;
	}
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	if (effect_type!=next_effect) {
		effect_type = next_effect;
		renderer->blit = PLAT_getScaler(renderer); // refresh the scaler
	}
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(renderer->src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}


void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	if (!vid.direct) GFX_BlitSurfaceExec(vid.screen, NULL, vid.video, NULL, 0,0,1); // TODO: handle aspect clipping
	SDL_Flip(vid.video);
	
	// swap backbuffer
	if (!vid.direct) {
		vid.page ^= 1;
		vid.screen->pixels = vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
	}
	
	if (vid.cleared) {
		PLAT_clearVideo(vid.screen);
		vid.cleared = 0;
	}
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

//	mmplus axp223 (via eggs)
#define	AXPDEV	"/dev/i2c-1"
#define	AXPID	(0x34)

//
//	AXP223 write (plus)
//		32 .. bit7: Shutdown Control
//
int axp_write(unsigned char address, unsigned char val) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[2];
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	buf[0] = address;
	buf[1] = val;
	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	packets.nmsgs = 1;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0) return -1;
	return 0;
}

//
//	AXP223 read (plus)
//		00 .. C4/C5(USBDC connected) 00(discharging)
//			bit7: ACIN presence indication 0:ACIN not exist, 1:ACIN exists
//			bit6: Indicating whether ACIN is usable (used by axp_test)
//			bit4: Indicating whether VBUS is usable (used by axp_test)
//			bit2: Indicating the Battery current direction 0: discharging, 1: charging
//			bit0: Indicating whether the boot source is ACIN or VBUS
//		01 .. 70(charging) 30(non-charging)
//			bit6: Charge indication 0:not charge or charge finished, 1: in charging
//		B9 .. (& 0x7F) battery percentage
//
int axp_read(unsigned char address) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char val;
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;
	msg[1].addr = AXPID;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &val;

	packets.nmsgs = 2;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if(ret < 0) return -1;
	return val;
}

///////////////////////////////

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
	*is_charging = is_plus ? (axp_read(0x00) & 0x4) > 0 : getInt("/sys/devices/gpiochip0/gpio/gpio59/value");
	
	*charge = getInt("/tmp/battery"); // 0-100?

	// TODO: tmp
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		putInt("/sys/class/gpio/gpio4/value", 1);
		putInt("/sys/class/gpio/unexport", 4);
		putInt("/sys/class/pwm/pwmchip0/export", 0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",1);
	}
	else {
		putInt("/sys/class/gpio/export", 4);
		putFile("/sys/class/gpio/gpio4/direction", "out");
		putInt("/sys/class/gpio/gpio4/value", 0);
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
	
	system("shutdown");
	while (1) pause(); // lolwat
}

///////////////////////////////

// #define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
// void PLAT_setCPUSpeed(int speed) {
// 	// TODO: this isn't quite right
// 	if (speed==CPU_SPEED_MENU) putFile(GOVERNOR_PATH, "powersave");
// 	else if (speed==CPU_SPEED_POWERSAVE) putFile(GOVERNOR_PATH, "ondemand");
// 	else putFile(GOVERNOR_PATH, "performance");
// }

// copy/paste of 35XX version now that we have our own overclock.elf
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  504000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1104000; break;
		case CPU_SPEED_NORMAL: 		freq = 1296000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1488000; break;
	}
	
	char cmd[32];
	sprintf(cmd,"overclock.elf %d\n", freq);
	system(cmd);
}

void PLAT_setRumble(int strength) {
    static char lastvalue = 0;
    const char str_export[2] = "48";
    const char str_direction[3] = "out";
    char value[1];
    int fd;

    value[0] = (strength == 0 ? 0x31 : 0x30); // '0' : '1'
    if (lastvalue != value[0]) {
       fd = open("/sys/class/gpio/export", O_WRONLY);
       if (fd > 0) { write(fd, str_export, 2); close(fd); }
       fd = open("/sys/class/gpio/gpio48/direction", O_WRONLY);
       if (fd > 0) { write(fd, str_direction, 3); close(fd); }
       fd = open("/sys/class/gpio/gpio48/value", O_WRONLY);
       if (fd > 0) { write(fd, value, 1); close(fd); }
       lastvalue = value[0];
    }
}

int PLAT_pickSampleRate(int requested, int max) {
	return max;
}

char* PLAT_getModel(void) {
	return is_plus ? "Miyoo Mini Plus" : "Miyoo Mini";
}

int PLAT_isOnline(void) {
	return 0;
}