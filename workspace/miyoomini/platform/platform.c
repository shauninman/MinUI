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

#include <mi_sys.h>
#include <mi_gfx.h>

/*
	framebuffer only needs to be FIXED_WIDTH x FIXED_HEIGHT x PAGE_COUNT
	because we need to do a copy from a buffer to rotate properly anyway
	this should be vid.video and will use FBIOPAN_DISPLAY when flipping
	the buffer will be vid.screen
	still not sure how to vysnc

*/

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	int cleared;
} vid;
static int _;

SDL_Surface* PLAT_initVideo(void) {
	putenv("SDL_HIDE_BATTERY=1");
	
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);
	
	// TODO: MI_SYS_Init() & MI_GFX_Open()
	// TODO: init garbage videomode
	// TODO: allocate video buffer memory, maybe MI_SYS_MMA_Alloc()? + MI_SYS_Mmap()?
	// TODO: create our screen surface from that buffer
	// TODO: setup vsync?
	
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH,FIXED_HEIGHT,FIXED_DEPTH,SDL_HWSURFACE|SDL_DOUBLEBUF);
	return vid.screen;
}

void PLAT_quitVideo(void) {
	// TODO: release video buffer memory, maybe MI_SYS_MMA_Free()?
	// TODO: free screen surface
	// TODO: MI_GFX_Close() && MI_SYS_Exit()
	
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// this buffer is offscreen when cleared
	// memset(screen->pixels, 0, PAGE_SIZE); 
	SDL_FillRect(screen, NULL, 0);
}
void PLAT_clearAll(void) {
	GFX_clear(vid.screen); // clear backbuffer
	vid.cleared = 1; // defer clearing frontbuffer until offscreen
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	// free screen surface
	// create new screen surface from video buffer memory
	
	LOG_info("PLAT_resizeVideo(%i,%i,%i)\n", w,h,pitch);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}

void PLAT_setNearestNeighbor(int enabled) {
	// buh
}

void PLAT_vsync(void) {
	// MI_GFX_WaitAllDone(true,0); // TODO: this helps but eventually locks up?
}

void PLAT_flip(SDL_Surface* screen, int sync) {
	// TODO: FlushCacheNeeded()?
	// TODO: MI_GFX_BitBlit()?
	
	SDL_Flip(screen);
	if (sync) PLAT_vsync();
	
	if (vid.cleared) {
		// GFX_clear(vid.screen);
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