// trimuismart
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

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

// TODO: flesh out based on picoarch-smart

static struct VID_Context {
	SDL_Surface* screen;
	int fd_fb;
} vid;
static int _;

int initBatteryADC();
int uninitBatteryADC();

SDL_Surface* PLAT_initVideo(void) {
	initBatteryADC();
	
	putenv("SDL_VIDEO_FBCON_ROTATION=CCW");
	putenv("SDL_USE_PAN=true");
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	vid.fd_fb = open("/dev/fb0", O_RDWR);
	vid.screen = SDL_SetVideoMode(FIXED_WIDTH,FIXED_HEIGHT,FIXED_DEPTH,SDL_SWSURFACE);
	return vid.screen;
}

void PLAT_quitVideo(void) {
	uninitBatteryADC();
	
	close(vid.fd_fb);
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0);
}
void PLAT_clearAll(void) {
	// TODO:
	PLAT_clearVideo(vid.screen);
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	// TODO: this will be similar to RG35XX
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_vsync(void) {
	// ioctl(vid.fd_fb, FBIO_WAITFORVSYNC, &_); // TODO: this slows MinUI/menu's roll but hurts FC framerate
}

void PLAT_flip(SDL_Surface* screen, int sync) {
	SDL_Flip(screen);
	if (sync) PLAT_vsync();
}

SDL_Surface* PLAT_getVideoBufferCopy(void) {
	// TODO: this is just copying the backbuffer!
	// TODO: should it be copying the frontbuffer?
	SDL_Surface* copy = SDL_CreateRGBSurface(SDL_SWSURFACE, vid.screen->w,vid.screen->h,FIXED_DEPTH,RGBA_MASK_AUTO);
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
// TODO: move to keymon?
// TODO: simplify and rewrite these
#define LRADC_BASE 0x01C22800
#define LRADC_CTRL 0x00
#define LRADC_INTC 0x04
#define LRADC_DAT0 0x0C
#define LRADC_DAT1 0x10

int lradcfd = -1;
volatile uint8_t * lradcaddr = NULL; 
int initBatteryADC(){
	uint32_t PageSize, PageMask;
	uint32_t addr_start, addr_offset;

	if ((lradcfd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) // O_RDWR
	{
        printf("initBatteryADC open mem fail: %d\n", errno);
        return -1;
    }

	PageSize = sysconf(_SC_PAGESIZE);
	PageMask = (~(PageSize - 1));

	addr_start = LRADC_BASE & PageMask;
	addr_offset = LRADC_BASE & ~PageMask;
	lradcaddr = (uint8_t*)mmap(0, PageSize * 2, PROT_READ|PROT_WRITE, MAP_SHARED, lradcfd, addr_start);
	if (lradcaddr == MAP_FAILED){
        close(lradcfd);
        lradcfd = -1;
        lradcaddr = NULL;
        printf("initBatteryADC mmap fail: %d\n", errno);
        return -1;
    }else{
        lradcaddr += addr_offset;
		// eggs pointed out this init requirement
		*(uint32_t*)lradcaddr = 0xC0004D;
    }    
    return 0;
}
int readBatteryADC(){
    if(lradcaddr){
        uint32_t val;
        val = *((uint32_t *)(lradcaddr + LRADC_DAT1));
        return val;
    }
    return -1;
}
int uninitBatteryADC(){
    uint32_t PageSize = sysconf(_SC_PAGESIZE);
    if(lradcaddr) munmap((void*)lradcaddr, PageSize * 2);
    if(lradcfd >= 0) close(lradcfd);    
    return 0;
}

///////////////////////////////

#define USB_SPEED "/sys/devices/platform/sunxi_usb_udc/udc/sunxi_usb_udc/current_speed"
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = POW_LOW_CHARGE;
	// return;
	
	char value[16]; memset(value, 0, 16);
	getFile(USB_SPEED, value, 16);
	*is_charging = !exactMatch(value, "UNKNOWN\n");
		
	int i = readBatteryADC() * 100 / 63;
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		// TODO: restore screen
		SetBrightness(GetBrightness());
		system("leds_off.sh");
	}
	else {
		// TODO: copy screen
		// TODO: clear screen
		SetRawBrightness(0);
		system("leds_on.sh");
	}
}
void PLAT_powerOff(void) {
	// buh
}

///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  504000; break;
		case CPU_SPEED_POWERSAVE:	freq = 1104000; break;
		case CPU_SPEED_NORMAL: 		freq = 1344000; break;
		case CPU_SPEED_PERFORMANCE: freq = 1536000; break;
	}

	char cmd[256];
	sprintf(cmd,"echo %i > %s\n", freq, GOVERNOR_PATH);
	system(cmd);
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Trimui Smart";
}