// trimuismart
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

#include "sunxi_display2.h"
#include "ion.h"
#include "ion_sunxi.h"
#include "scaler.h"

///////////////////////////////

void PLAT_initInput(void) {
	// buh
}
void PLAT_quitInput(void) {
	// buh
}

///////////////////////////////
// 100% eggs

#define FB_CH (0)
#define FB_LAYER (0)
#define FB_ZORDER (0)
#define SCALER_CH (1)
#define SCALER_LAYER (0)
#define SCALER_ZORDER (10)
#define OVERLAY_CH (2)
#define OVERLAY_LAYER (0)
#define OVERLAY_ZORDER (11)
#define DEF_FB_CH (2)
#define DEF_FB_LAYER (0)

#define DE			(0x01000000)
#define RT_MIXER0		(DE+0x00100000)
#define OVL_V			(RT_MIXER0+0x2000+(SCALER_CH*0x1000))
#define OVL_V_TOP_LADD0		(0x18+(SCALER_LAYER*0x30))

typedef struct ion_alloc_info {
	uint32_t		size;
	struct ion_handle	*handle;
	int			fd;
	void*			padd;
	void*			vadd;
} ion_alloc_info_t;

//
//	allocate ion memory
//		input: info.size
//
void ion_alloc(int ion_fd, ion_alloc_info_t* info) {
	struct ion_allocation_data	iad;
	struct ion_fd_data		ifd;
	struct ion_custom_data		icd;
	sunxi_phys_data			spd;

	iad.len = info->size;
	iad.align = sysconf(_SC_PAGESIZE);
	iad.heap_id_mask = ION_HEAP_TYPE_DMA_MASK;
	iad.flags = 0;
	if (ioctl(ion_fd, ION_IOC_ALLOC, &iad)<0) fprintf(stderr, "ION_ALLOC failed %s\n",strerror(errno));
	icd.cmd = ION_IOC_SUNXI_PHYS_ADDR;
	icd.arg = (uintptr_t)&spd;
	spd.handle = iad.handle;
	if (ioctl(ion_fd, ION_IOC_CUSTOM, &icd)<0) fprintf(stderr, "ION_GET_PHY failed %s\n",strerror(errno));
	ifd.handle = iad.handle;
	if (ioctl(ion_fd, ION_IOC_MAP, &ifd)<0) fprintf(stderr, "ION_MAP failed %s\n",strerror(errno));

	info->handle = iad.handle;
	info->fd = ifd.fd;
	info->padd = (void*)spd.phys_addr;
	info->vadd = mmap(0, info->size, PROT_READ|PROT_WRITE, MAP_SHARED, info->fd, 0);
	fprintf(stderr, "allocated padd: 0x%x vadd: 0x%x size: 0x%x\n", (uintptr_t)info->padd, (uintptr_t)info->vadd, info->size);
}

//
//	free ion memory
//
void ion_free(int ion_fd, ion_alloc_info_t* info) {
	struct ion_handle_data	 ihd;
	munmap(info->vadd, info->size);
	close(info->fd);
	ihd.handle = info->handle;
	if (ioctl(ion_fd, ION_IOC_FREE, &ihd)<0) fprintf(stderr, "ION_FREE failed %s\n",strerror(errno));
}

void rotate_16bpp(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dp) {
	uint16_t* s = (uint16_t*)src;
	uint16_t* d = (uint16_t*)dst;
	int spx = sp/FIXED_BPP;
	int dpx = dp/FIXED_BPP;
	
		
	for (int y=0; y<sh; y++) {
		for (int x=0; x<sw; x++) {
			*(d + x * dpx + (dpx - y - 1)) = *(s + (sh-1-y) * spx + (sw-1-x));
		}
	}
}

///////////////////////////////

static struct VID_Context {
	SDL_Surface* video;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	SDL_Surface* special;
	
	GFX_Renderer* renderer;
	
	int disp_fd;
	int fb_fd;
	int ion_fd;
	int mem_fd;
	uint32_t* mem_map;
	
	disp_layer_config fb_config;
	disp_layer_config buffer_config;
	disp_layer_config screen_config;
	ion_alloc_info_t buffer_info;
	ion_alloc_info_t screen_info;
	
	int rotated_pitch;
	int rotated_offset;
	int source_offset;
	
	int page;
	int width;
	int height;
	int pitch;
	
	int cleared;
	int resized;
} vid;
static int _;

void ADC_init();
void ADC_quit();

SDL_Surface* PLAT_initVideo(void) {
	ADC_init();
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	vid.video = SDL_SetVideoMode(FIXED_HEIGHT, FIXED_WIDTH, FIXED_DEPTH, SDL_HWSURFACE);
	memset(vid.video->pixels, 0, FIXED_SIZE);
	
	vid.disp_fd = open("/dev/disp", O_RDWR);
	vid.fb_fd = open("/dev/fb0", O_RDWR);
	vid.ion_fd = open("/dev/ion", O_RDWR);
	vid.mem_fd = open("/dev/mem", O_RDWR);
	vid.mem_map = mmap(0, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, vid.mem_fd, OVL_V);
	
	memset(&vid.fb_config, 0, sizeof(disp_layer_config));
	memset(&vid.buffer_config, 0, sizeof(disp_layer_config));

	// wait for vsync to avoid glitch
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);

	// save and disable default FB layer first
	uint32_t args[4] = {0, (uintptr_t)&vid.fb_config, 1, 0};
	vid.fb_config.channel = DEF_FB_CH;
	vid.fb_config.layer_id = DEF_FB_LAYER;
	ioctl(vid.disp_fd, DISP_LAYER_GET_CONFIG, args);
	
	vid.fb_config.enable = 0;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);
	
	// intermediate buffer
	vid.page = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	
	vid.screen_info.size = PAGE_SIZE;
	ion_alloc(vid.ion_fd, &vid.screen_info);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.screen_info.vadd, vid.width, vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_565);
	
	// setup ch1 screen layer (RGB565, DOUBLEBUF) ch0/1 can use scaler, but cannot use alpha
	vid.buffer_info.size = PAGE_SIZE * PAGE_COUNT;
	ion_alloc(vid.ion_fd, &vid.buffer_info);

	vid.buffer = SDL_CreateRGBSurfaceFrom(vid.buffer_info.vadd + vid.page * PAGE_SIZE, PAGE_HEIGHT, PAGE_WIDTH, FIXED_DEPTH, PAGE_HEIGHT*FIXED_BPP, RGBA_MASK_565);
	vid.buffer_config.channel = SCALER_CH;
	vid.buffer_config.layer_id = SCALER_LAYER;
	vid.buffer_config.enable = 1;
	vid.buffer_config.info.fb.format = DISP_FORMAT_RGB_565;
	vid.buffer_config.info.fb.addr[0] = (uintptr_t)vid.buffer_info.padd;
	vid.buffer_config.info.fb.size[0].width  = vid.height; // PAGE_HEIGHT;
	vid.buffer_config.info.fb.size[0].height = vid.width; // PAGE_WIDTH;
	vid.buffer_config.info.mode = LAYER_MODE_BUFFER;
	vid.buffer_config.info.zorder = SCALER_ZORDER;
	vid.buffer_config.info.alpha_mode = 0; //0: pixel alpha; 1: global alpha; 2: global pixel alpha
	vid.buffer_config.info.alpha_value = 0;
	vid.buffer_config.info.screen_win.x = 0;
	vid.buffer_config.info.screen_win.y = 0;
	vid.buffer_config.info.screen_win.width  = vid.height;
	vid.buffer_config.info.screen_win.height = vid.width;
	vid.buffer_config.info.fb.pre_multiply = 0;
	vid.buffer_config.info.fb.crop.x = (int64_t)0 << 32;
	vid.buffer_config.info.fb.crop.y = (int64_t)0 << 32;
	vid.buffer_config.info.fb.crop.width  = (int64_t)vid.height << 32;
	vid.buffer_config.info.fb.crop.height = (int64_t)vid.width  << 32;
	
	args[1] = (uintptr_t)&vid.buffer_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);
	
	// lotta waiting for vsync...
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
	// return vid.buffer;
	
	// trimui's SDL pukes so much debug info
	puts("--------------------------------"); fflush(stdout);
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	puts("--------------------------------"); fflush(stdout);

	ADC_quit();
	
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
	
	memset(vid.video->pixels, 0, FIXED_SIZE);
	
	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	
	// disable all channels & revert FB channel
	vid.fb_config.enable = vid.buffer_config.enable = 0;
	uint32_t args[4] = {0, (uintptr_t)&vid.fb_config, 1, 0};
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	args[1] = (uintptr_t)&vid.buffer_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	vid.fb_config.enable = 1;
	vid.fb_config.channel = DEF_FB_CH;
	vid.fb_config.layer_id = DEF_FB_LAYER;
	args[1] = (uintptr_t)&vid.fb_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	// free & unmap & close
	ion_free(vid.ion_fd, &vid.buffer_info);
	munmap(vid.mem_map, sysconf(_SC_PAGESIZE));
	close(vid.mem_fd);
	close(vid.ion_fd);
	close(vid.fb_fd);
	close(vid.disp_fd);
	
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* IGNORED) {
	if (!vid.cleared) memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	memset(vid.buffer->pixels, 0, PAGE_SIZE); 
}
void PLAT_clearAll(void) {
	vid.cleared = 1;
	PLAT_clearVideo(vid.buffer); // clear backbuffer
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	
	SDL_FreeSurface(vid.screen);
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;
	
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.screen_info.vadd, vid.width, vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_565);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	vid.resized = 1;

	vid.rotated_pitch = 0;
	if (vid.renderer) vid.renderer->src_w = 0;
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
void PLAT_setEffect(int effect) {
	// buh
}
void PLAT_vsync(int remaining) {
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
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
	vid.renderer = renderer;
	int p = ((renderer->src_h+7)/8)*8 * FIXED_BPP;
	if (!vid.special || vid.special->w!=renderer->src_h || vid.special->h!=renderer->src_w || vid.special->pitch!=p || !vid.rotated_pitch) {
		// LOG_info("!special:%i sp_w!=src_h:%i sp_h!=src_w:%i sp_p!=src_h*FIXED_BPP:%i !rot_p: %i\n",
		// !vid.special, vid.special && vid.special->w!=renderer->src_h, vid.special && vid.special->h!=renderer->src_w, vid.special && vid.special->pitch!=p, !vid.rotated_pitch);
		
		if (vid.special) SDL_FreeSurface(vid.special);
		
		vid.special = SDL_CreateRGBSurface(SDL_SWSURFACE, renderer->src_h,renderer->src_w, FIXED_DEPTH,RGBA_MASK_565);
		vid.rotated_pitch = vid.height * FIXED_BPP; // ((vid.height+7)/8)*8 * FIXED_BPP; // TODO: this should be the fix but it's not :sob:
		vid.rotated_offset = (renderer->dst_x * vid.rotated_pitch) + (renderer->dst_y * FIXED_BPP);
		vid.source_offset = (renderer->src_x * vid.special->pitch) + (renderer->src_y * FIXED_BPP);
		
		LOG_info("PLAT_blitRenderer >> src:%p dst:%p blit:%p src:%ix%i (%i) dst:%i,%i %ix%i (%i) vid: %ix%i (%i) (%i)\n",
			vid.renderer->src,
			vid.renderer->dst, // unused
			vid.renderer->blit,

			vid.renderer->src_w,
			vid.renderer->src_h,
			vid.renderer->src_p, // unused

			vid.renderer->dst_x,
			vid.renderer->dst_y,
			vid.renderer->dst_w, // unused
			vid.renderer->dst_h, // unused?
			vid.renderer->dst_p, // unused

			vid.width,
			vid.height,
			vid.pitch,
			vid.rotated_pitch
		);
	}
	rotate_16bpp(renderer->src, vid.special->pixels, renderer->src_w,renderer->src_h,renderer->src_p,vid.special->pitch);
	((scaler_t)renderer->blit)(vid.special->pixels + vid.source_offset, vid.buffer->pixels+vid.rotated_offset, vid.special->w,vid.special->h, vid.special->pitch, vid.renderer->dst_h, vid.renderer->dst_w,vid.rotated_pitch);
	
	// LOG_info("blit(%p,%p, %i,%i,%i, %i,%i,%i)\n", vid.special->pixels, vid.buffer->pixels+vid.rotated_offset, vid.special->w,vid.special->h, vid.special->pitch, vid.renderer->dst_h, vid.renderer->dst_w,vid.rotated_pitch);
}

void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	if (!vid.renderer) rotate_16bpp(vid.screen->pixels, vid.buffer->pixels, vid.width, vid.height,vid.pitch,vid.height*FIXED_BPP);
	
	vid.buffer_config.info.fb.addr[0] = (uintptr_t)vid.buffer_info.padd + vid.page * PAGE_SIZE;
	vid.mem_map[OVL_V_TOP_LADD0/4] = (uintptr_t)vid.buffer_info.padd + vid.page * PAGE_SIZE;
	
	if (vid.resized) {
		vid.buffer_config.info.fb.size[0].width  = vid.height;
		vid.buffer_config.info.fb.size[0].height = vid.width;
		vid.buffer_config.info.fb.crop.width  = (int64_t)vid.height << 32;
		vid.buffer_config.info.fb.crop.height = (int64_t)vid.width  << 32;
		uint32_t args[4] = {0, (uintptr_t)&vid.buffer_config, 1, 0};
		ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);
		vid.resized = 0;
	}
	
	vid.page ^= 1;
	vid.buffer->pixels = vid.buffer_info.vadd + vid.page * PAGE_SIZE;
	
	if (sync) PLAT_vsync(0);
	
	if (vid.cleared) {
		PLAT_clearVideo(vid.buffer);
		vid.cleared = 0;
	}
	
	vid.renderer = NULL;
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

#define LRADC 0x01C22800
#define LRADC_VALUE 0x10
static struct ADC_Context {
	int	mem_fd;
	int page_size;
	void* mem_map;
	void* adc_addr;
} adc;

void ADC_init(void) {
	adc.page_size = sysconf(_SC_PAGESIZE);
	int page_mask = (~(adc.page_size - 1));
	int addr_start = LRADC & page_mask;
	int addr_offset = LRADC & ~page_mask;
	
	adc.mem_fd = open("/dev/mem",O_RDWR);
	adc.mem_map = mmap(0, adc.page_size*2, PROT_READ|PROT_WRITE, MAP_SHARED, adc.mem_fd, addr_start);
	adc.adc_addr = adc.mem_map + addr_offset;
	*(uint32_t*)adc.adc_addr = 0xC0004D;
}
int ADC_read(void) {
	return *((uint32_t *)(adc.adc_addr + LRADC_VALUE));
}
void ADC_quit(void) {
	munmap(adc.mem_map, adc.page_size*2);
	close(adc.mem_fd);
}

///////////////////////////////

#define USB_SPEED "/sys/devices/platform/sunxi_usb_udc/udc/sunxi_usb_udc/current_speed"
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);
}

void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	char value[16]; memset(value, 0, 16);
	getFile(USB_SPEED, value, 16);
	*is_charging = !exactMatch(value, "UNKNOWN\n");
	
	int i = ADC_read();
	// worry less about battery and more about the game you're playing
	     if (i>43) *charge = 100;
	else if (i>41) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>39) *charge =  40;
	else if (i>38) *charge =  20;
	else           *charge =  10;
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		// TODO: restore screen
		SetBrightness(GetBrightness());
		system("leds_off");
	}
	else {
		// TODO: copy screen
		// TODO: clear screen
		SetRawBrightness(0);
		system("leds_on");
	}
}

void PLAT_powerOff(void) {
	system("leds_on");
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
	sprintf(cmd,"echo %i > %s", freq, GOVERNOR_PATH);
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

int PLAT_isOnline(void) {
	return 0;
}