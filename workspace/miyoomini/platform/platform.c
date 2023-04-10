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

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

///////////////////////////////

#include <mi_sys.h>
#include <mi_gfx.h>

#define SSTAR_BPP 	4
#define SSTAR_DEPTH (SSTAR_BPP * 8)
#define SSTAR_PITCH	(FIXED_WIDTH * SSTAR_BPP)
#define SSTAR_SIZE 	(SSTAR_PITCH * FIXED_HEIGHT)
#define SSTAR_COUNT 2

// from eggs GFXSample_rev15/src/gfx.c

#define	pixelsPa	unused1
#define ALIGN4K(val)	((val+4095)&(~4095))
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
static inline void FlushCacheNeeded(void* pixels, uint32_t pitch, uint32_t y, uint32_t h) {
	uintptr_t pixptr = (uintptr_t)pixels;
	uintptr_t startaddress = (pixptr + pitch*y)&(~4095);
	uint32_t size = ALIGN4K(pixptr + pitch*(y+h)) - startaddress;
	if (size) MI_SYS_FlushInvCache((void*)startaddress, size);
}

///////////////////////////////

typedef struct HWBuffer {
	MI_PHY padd;
	void* vadd;
} HWBuffer;

static struct VID_Context {
	SDL_Surface* screen;
	
	int fd_fb;
	// void* fb_map;
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	
	MI_GFX_Surface_t mi_dst;
	MI_GFX_Surface_t mi_src;
	MI_GFX_Rect_t mi_dst_rect;
	MI_GFX_Rect_t mi_src_rect;
	MI_GFX_Opt_t mi_opt;
	
	HWBuffer buffer;
		
	int page;
	int width;
	int height;
	int pitch;
	int cleared;
} vid;

SDL_Surface* PLAT_initVideo(void) {
	putenv("SDL_HIDE_BATTERY=1"); // using MiniUI's custom SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);
	
	MI_SYS_Init();
	MI_GFX_Open();
	
	SDL_Surface* video = // unused, sets up framebuffer for us
	SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, SSTAR_DEPTH, SDL_SWSURFACE);
	
	vid.fd_fb = open("/dev/fb0", O_RDWR);
	ioctl(vid.fd_fb, FBIOGET_VSCREENINFO, &vid.vinfo);
	vid.vinfo.yres_virtual = FIXED_HEIGHT * SSTAR_COUNT;
	vid.vinfo.yoffset = 0;
	ioctl(vid.fd_fb, FBIOPUT_VSCREENINFO, &vid.vinfo);
	ioctl(vid.fd_fb, FBIOGET_FSCREENINFO, &vid.finfo);
	
	vid.page = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	
	// framebuffer
	vid.mi_dst.phyAddr = vid.finfo.smem_start + vid.page * SSTAR_SIZE;
	vid.mi_dst.eColorFmt = E_MI_GFX_FMT_ARGB8888;
	vid.mi_dst.u32Width = FIXED_WIDTH;
	vid.mi_dst.u32Height = FIXED_HEIGHT;
	vid.mi_dst.u32Stride = SSTAR_PITCH;
	vid.mi_dst_rect.s32Xpos = 0;
	vid.mi_dst_rect.s32Ypos = 0;
	vid.mi_dst_rect.u32Width = FIXED_WIDTH;
	vid.mi_dst_rect.u32Height = FIXED_HEIGHT;
	
	memset(&vid.mi_opt, 0, sizeof(vid.mi_opt));
	vid.mi_opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
	vid.mi_opt.eRotate = E_MI_GFX_ROTATE_180;
	
	MI_SYS_MMA_Alloc(NULL, ALIGN4K(PAGE_SIZE), &vid.buffer.padd);
	MI_SYS_Mmap(vid.buffer.padd, ALIGN4K(PAGE_SIZE), &vid.buffer.vadd, true);
	
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd,vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, PAGE_SIZE);
	
	vid.mi_src.phyAddr = vid.buffer.padd;
	vid.mi_src.eColorFmt = GFX_ColorFmt(vid.screen);
	vid.mi_src.u32Width = vid.width;
	vid.mi_src.u32Height = vid.height;
	vid.mi_src.u32Stride = vid.pitch;
	
	vid.mi_src_rect.s32Xpos = 0;
	vid.mi_src_rect.s32Ypos = 0;
	vid.mi_src_rect.u32Width = vid.width;
	vid.mi_src_rect.u32Height = vid.height;
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);
	
	MI_SYS_Munmap(vid.buffer.vadd, ALIGN4K(PAGE_SIZE));
	MI_SYS_MMA_Free(vid.buffer.padd);

	vid.vinfo.yoffset = 0;
	ioctl(vid.fd_fb, FBIOPUT_VSCREENINFO, &vid.vinfo);
	close(vid.fd_fb);
	
	MI_GFX_Close();
	MI_SYS_Exit();
	
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// this buffer is offscreen when cleared
	memset(screen->pixels, 0, PAGE_SIZE);
}
void PLAT_clearAll(void) {
	GFX_clear(vid.screen); // clear backbuffer
	vid.cleared = 1; // defer clearing frontbuffer until offscreen
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;
	
	SDL_FreeSurface(vid.screen);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd, vid.width,vid.height,FIXED_DEPTH,vid.pitch, RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	vid.mi_src.u32Width = vid.width;
	vid.mi_src.u32Height = vid.height;
	vid.mi_src.u32Stride = vid.pitch;

	vid.mi_src_rect.s32Xpos = 0;
	vid.mi_src_rect.s32Ypos = 0;
	vid.mi_src_rect.u32Width = vid.width;
	vid.mi_src_rect.u32Height = vid.height;
	
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}

void PLAT_setNearestNeighbor(int enabled) {
	// buh
}

void PLAT_vsync(void) {
	// buh
}

void PLAT_flip(SDL_Surface* screen, int sync) {
	vid.vinfo.yoffset = vid.page * FIXED_HEIGHT;
	FlushCacheNeeded(vid.screen->pixels, vid.screen->pitch, vid.mi_src_rect.s32Ypos, vid.mi_src_rect.u32Height);
	MI_GFX_BitBlit(&vid.mi_src, &vid.mi_src_rect, &vid.mi_dst, &vid.mi_dst_rect, &vid.mi_opt, NULL);
	
	// TODO: I have a feeling blocking won't be viable in practice...nope!
	// MI_GFX_WaitAllDone(true,0);
	ioctl(vid.fd_fb, FBIOPAN_DISPLAY, &vid.vinfo);
	
	vid.page ^= 1;
	vid.mi_dst.phyAddr = vid.finfo.smem_start + vid.page * SSTAR_SIZE;
	
	if (vid.cleared) {
		GFX_clear(vid.screen);
		vid.cleared = 0;
	}
}

SDL_Surface* PLAT_getVideoBufferCopy(void) {
	// TODO: this is just copying the backbuffer!
	// TODO: should it be copying the frontbuffer?
	SDL_Surface* copy = SDL_CreateRGBSurface(SDL_SWSURFACE, vid.screen->w,vid.screen->h,FIXED_DEPTH,0,0,0,0);
	SDL_BlitSurface(vid.screen, NULL, copy, NULL);
	return copy;
}

///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000
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
	*is_charging = getInt("/sys/devices/gpiochip0/gpio/gpio59/value");
	
	int i = getInt("/tmp/battery"); // 0-100?

	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// TODO: tmp
	// *is_charging = 0;
	// *charge = POW_LOW_CHARGE;
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
	return "Miyoo Mini";
}