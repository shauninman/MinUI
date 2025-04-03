// rg35xx
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

#include "ion.h"
#include "ion-owl.h"
#include "de_atm7059.h"
#include "scaler.h"

///////////////////////////////

void PLAT_initInput(void) {
	// buh
}
void PLAT_quitInput(void) {
	// buh
}

///////////////////////////////

typedef struct ion_alloc_info {
	uint32_t			size;
	struct ion_handle	*handle;
	int					fd;
	void*				padd;
	void*				vadd;
} ion_alloc_info_t;
static void ion_alloc(int fd_ion, ion_alloc_info_t* info) {
	struct ion_allocation_data	iad;
	struct ion_fd_data		ifd;
	struct ion_custom_data		icd;
	struct owl_ion_phys_data 	ipd;

	iad.len = info->size;
	iad.align = sysconf(_SC_PAGESIZE);
	iad.heap_id_mask = (1<<ION_HEAP_ID_PMEM);
	iad.flags = 0;
	if (ioctl(fd_ion, ION_IOC_ALLOC, &iad)<0) fprintf(stderr, "ION_ALLOC failed %s\n",strerror(errno));
	icd.cmd = OWL_ION_GET_PHY;
	icd.arg = (uintptr_t)&ipd;
	ipd.handle = iad.handle;
	if (ioctl(fd_ion, ION_IOC_CUSTOM, &icd)<0) printf("ION_GET_PHY failed %s\n",strerror(errno));
	ifd.handle = iad.handle;
	if (ioctl(fd_ion, ION_IOC_MAP, &ifd)<0) fprintf(stderr, "ION_MAP failed %s\n",strerror(errno));

	info->handle = (void*)iad.handle;
	info->fd = ifd.fd;
	info->padd = (void*)ipd.phys_addr;
	info->vadd = mmap(0, info->size, PROT_READ|PROT_WRITE, MAP_SHARED, info->fd, 0);

}
static void ion_free(int fd_ion, ion_alloc_info_t* info) {
	struct ion_handle_data	ihd;
	munmap(info->vadd, info->size);
	close(info->fd);
	ihd.handle = (uintptr_t)info->handle;
	if (ioctl(fd_ion, ION_IOC_FREE, &ihd)<0) fprintf(stderr, "ION_FREE failed %s\n",strerror(errno));
	fflush(stdout);
}

///////////////////////////////

#define	DE		(0xB02E0000)
#define	DE_SIZE	(0x00002000)
static int de_enable_overlay = 0;
enum {
	DE_SCOEF_NONE,
	DE_SCOEF_CRISPY,
	DE_SCOEF_ZOOMIN,
	DE_SCOEF_HALF_ZOOMOUT,
	DE_SCOEF_SMALLER_ZOOMOUT,
	DE_SCOEF_MAX
};
static void DE_setScaleCoef(uint32_t* de_mem, int plane, int scale) {
	switch(scale) {
		case DE_SCOEF_NONE:	// for integer scale	  < L R > (0x40=100%) Applies to the following pixels:
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000; // L 100%  R 0%
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00400000; // L 87.5% R 12.5%
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00400000; // L 75%   R 25%
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00400000; // L 62.5% R 37.5%
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00004000; // L 50%   R 50%
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00004000; // L 37.5% R 62.5%
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00004000; // L 25%   R 75%
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00004000; // L 12.5% R 87.5%
			break;
		case DE_SCOEF_CRISPY:	// crispy setting for upscale
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00202000;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00004000;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00004000;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00004000;
			break;
		case DE_SCOEF_ZOOMIN:
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0xFC3E07FF;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0xFA3810FE;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0xF9301BFC;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0xFA2626FA;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0xFC1B30F9;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0xFE1038FA;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0xFF073EFC;
			break;
		case DE_SCOEF_HALF_ZOOMOUT:
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00380800;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00301000;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00281800;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00202000;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00182800;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00103000;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00083800;
			break;
		case DE_SCOEF_SMALLER_ZOOMOUT:
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x10201000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x0E1E1202;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x0C1C1404;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x0A1A1606;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x08181808;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x06161A0A;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x04141C0C;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x02121E0E;
			break;
		default:
			break;
	}
}
static void DE_enableLayer(uint32_t* de_mem) {
	de_mem[DE_PATH_CTL(0)/4] = (de_enable_overlay?0x30300000:0x30100000) | (de_mem[DE_PATH_CTL(0)/4] & 0xCF0FFFFF);
}
static void DE_setRect(uint32_t* de_mem, int x, int y, int w, int h) {
	de_mem[(DE_OVL_OSIZE(0))/4] = ((w-1)&0xFFFF) | ((h-1)<<16);
	de_mem[(DE_OVL_SR(0))/4] = ((0x2000*((de_mem[(DE_OVL_ISIZE(0))/4]&0xFFFF)+1)/w)&0xFFFF) |
						((0x2000*((de_mem[(DE_OVL_ISIZE(0))/4]>>16)+1)/h)<<16);
	de_mem[(DE_OVL_COOR(0,0))/4] = (y<<16) | (x&0xFFFF);
}

///////////////////////////////

#define MAX_PRIVATE_DATA_SIZE 40
struct owlfb_disp_device{
    __u32 mType;
    __u32 mState;
    __u32 mPluginState;
    __u32 mWidth;
    __u32 mHeight;
    __u32 mRefreshRate;
    __u32 mWidthScale;
    __u32 mHeightScale;
    __u32 mCmdMode;
    __u32 mIcType;
    __u32 mPrivateInfo[MAX_PRIVATE_DATA_SIZE];
};

struct display_private_info {
	int LCD_TYPE; // 1
	int	LCD_LIGHTENESS; // 128
	int LCD_SATURATION; // 7
	int LCD_CONSTRAST;  // 5
};

enum CmdMode {
    SET_LIGHTENESS = 0,
    SET_SATURATION = 1,
    SET_CONSTRAST = 2,
	SET_DEFAULT = 3,
};

struct owlfb_sync_info {
	__u8 enabled;
	__u8 disp_id;
	__u16 reserved2;
};
enum owlfb_overlay_type {
	OWLFB_OVERLAY_VIDEO = 1,
	OWLFB_OVERLAY_CURSOR,
};
enum owl_color_mode {
	OWL_DSS_COLOR_RGB16	= 0,
	OWL_DSS_COLOR_BGR16	= 1,
	OWL_DSS_COLOR_ARGB32    = 4,
	OWL_DSS_COLOR_ABGR32    = 5,
	OWL_DSS_COLOR_RGBA32    = 6,
	OWL_DSS_COLOR_BGRA32	= 7,
	OWL_DSS_COLOR_NV21	= 8,
	OWL_DSS_COLOR_NU21	= 9,
	OWL_DSS_COLOR_YU12	= 10,
	OWL_DSS_COLOR_ARGB16	= 12,
	OWL_DSS_COLOR_ABGR16	= 13,
	OWL_DSS_COLOR_RGBA16	= 14,
	OWL_DSS_COLOR_BGRA16	= 15,
	OWL_DSS_COLOR_RGB24U	= 16,
	OWL_DSS_COLOR_RGB24P	= 17,
	OWL_DSS_COLOR_RGBX32	= 18,
	OWL_DSS_COLOR_NV12	= 19,
	OWL_DSS_COLOR_XBGR32	= 20,
	OWL_DSS_COLOR_XRGB32	= 21,
};
struct owlfb_overlay_args {
	__u16 fb_id;
	__u16 overlay_id;
	__u16 overlay_type;
	__u32 overlay_mem_base;
	__u32 overlay_mem_size;
	__u32 uintptr_overly_info;
};
struct owlfb_overlay_info {
	__u32 mem_off;
	__u32 mem_size;
	__u32 screen_width;
	enum owl_color_mode color_mode;
	__u32 img_width;
	__u32 img_height;
	__u32 xoff;
	__u32 yoff;
	__u32 width;
	__u32 height;
	__u8 rotation;
	__u32 pos_x;
	__u32 pos_y;
	__u32 out_width;
	__u32 out_height;
	__u8 lightness;
	__u8 saturation;
	__u8 contrast;
	bool global_alpha_en;
	__u8 global_alpha;
	bool pre_mult_alpha_en;
	__u8 zorder;
};

#define OWL_IOW(num, dtype) _IOW('O', num, dtype)
#define OWL_IOR(num, dtype)	_IOR('O', num, dtype)
#define OWLFB_WAITFORVSYNC      OWL_IOW(57, long long)
#define OWLFB_GET_DISPLAY_INFO	OWL_IOW(74, struct owlfb_disp_device)
#define OWLFB_SET_DISPLAY_INFO	OWL_IOW(75, struct owlfb_disp_device)
#define OWLFB_VSYNC_EVENT_EN	OWL_IOW(67, struct owlfb_sync_info)
#define OWLFB_OVERLAY_REQUEST	OWL_IOR(41, struct owlfb_overlay_args)
#define OWLFB_OVERLAY_RELEASE	OWL_IOR(42, struct owlfb_overlay_args)
#define OWLFB_OVERLAY_ENABLE	OWL_IOR(43, struct owlfb_overlay_args)
#define OWLFB_OVERLAY_DISABLE	OWL_IOR(45, struct owlfb_overlay_args)
#define OWLFB_OVERLAY_GETINFO	OWL_IOW(46, struct owlfb_overlay_args)
#define OWLFB_OVERLAY_SETINFO   OWL_IOW(47, struct owlfb_overlay_args)

///////////////////////////////

static struct VID_Context {
	SDL_Surface* screen;
	
	int fd_fb;
	int fd_ion;
	int fd_mem;
	
	uint32_t* de_mem;
	
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;
	ion_alloc_info_t fb_info;
	
	int page;
	int width;
	int height;
	int pitch;
	int cleared;
} vid;
static int _;

SDL_Surface* PLAT_initVideo(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	SDL_SetVideoMode(0,0,FIXED_DEPTH,0);
	
	vid.fd_fb = open("/dev/fb0", O_RDWR);
	vid.fd_ion = open("/dev/ion", O_RDWR);
	vid.fd_mem = open("/dev/mem", O_RDWR);
	vid.de_mem = mmap(0, DE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, vid.fd_mem, DE);
	
	ioctl(vid.fd_fb, FBIOGET_FSCREENINFO, &vid.finfo);
	ioctl(vid.fd_fb, FBIOGET_VSCREENINFO, &vid.vinfo);

	struct owlfb_sync_info sinfo;
	sinfo.enabled = 1;
	sinfo.disp_id = 2;
	if (ioctl(vid.fd_fb, OWLFB_VSYNC_EVENT_EN, &sinfo)<0) fprintf(stderr, "VSYNC_EVENT_EN failed %s\n",strerror(errno));
	
	vid.page = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	
	vid.fb_info.size = PAGE_SIZE * PAGE_COUNT;
	ion_alloc(vid.fd_ion, &vid.fb_info);

	vid.screen = SDL_CreateRGBSurfaceFrom(vid.fb_info.vadd+PAGE_SIZE, vid.width,vid.height,FIXED_DEPTH,vid.pitch, RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	
	int vw = (vid.de_mem[DE_PATH_SIZE(0)/4]&0xFFFF)+1;
	int vh = (vid.de_mem[DE_PATH_SIZE(0)/4]>>16)+1;
	
	vid.de_mem[DE_OVL_ISIZE(0)/4] = vid.de_mem[DE_OVL_ISIZE(2)/4] = ((vid.width-1) & 0xFFFF) | ((vid.height-1) << 16);
	vid.de_mem[DE_OVL_SR(0)/4] = vid.de_mem[DE_OVL_SR(2)/4] = ((0x2000*vid.width/vw)&0xFFFF) | ((0x2000*vid.height/vh)<<16);
	vid.de_mem[DE_OVL_STR(0)/4] = vid.de_mem[DE_OVL_STR(2)/4] = vid.pitch / 8;
	vid.de_mem[DE_OVL_BA0(0)/4] = (uintptr_t)(vid.fb_info.padd + PAGE_SIZE);
	
	GFX_setNearestNeighbor(0); // false?  
	
	return vid.screen;
}

void PLAT_quitVideo(void) {
	ion_free(vid.fd_ion, &vid.fb_info);
	munmap(vid.de_mem, DE_SIZE);
	close(vid.fd_mem);
	close(vid.fd_ion);
	close(vid.fd_fb);
	SDL_FreeSurface(vid.screen);
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// this buffer is offscreen when cleared
	memset(screen->pixels, 0, PAGE_SIZE); 
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // clear backbuffer
	vid.cleared = 1; // defer clearing frontbuffer until offscreen
}

void PLAT_setVsync(int vsync) {
	// buh
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;

	SDL_FreeSurface(vid.screen);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.fb_info.vadd + vid.page*PAGE_SIZE, vid.width,vid.height,FIXED_DEPTH,vid.pitch, RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	
	int vw = (vid.de_mem[DE_PATH_SIZE(0)/4]&0xFFFF)+1;
	int vh = (vid.de_mem[DE_PATH_SIZE(0)/4]>>16)+1;
	
	vid.de_mem[DE_OVL_ISIZE(0)/4] = vid.de_mem[DE_OVL_ISIZE(2)/4] = ((vid.width-1) & 0xFFFF) | ((vid.height-1) << 16);
	vid.de_mem[DE_OVL_SR(0)/4] = vid.de_mem[DE_OVL_SR(2)/4] = ((0x2000*vid.width/vw)&0xFFFF) | ((0x2000*vid.height/vh)<<16);
	vid.de_mem[DE_OVL_STR(0)/4] = vid.de_mem[DE_OVL_STR(2)/4] = vid.pitch / 8;
	vid.de_mem[DE_OVL_BA0(0)/4] = (uintptr_t)(vid.fb_info.padd + vid.page * PAGE_SIZE);
	
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	DE_setRect(vid.de_mem, x,y,width,height);
}

void PLAT_setNearestNeighbor(int enabled) {
	int scale_coef = enabled ? DE_SCOEF_NONE : DE_SCOEF_HALF_ZOOMOUT;
	DE_setScaleCoef(vid.de_mem, 0, scale_coef);
	DE_setScaleCoef(vid.de_mem, 1, scale_coef);
	DE_setScaleCoef(vid.de_mem, 2, scale_coef);
	DE_setScaleCoef(vid.de_mem, 3, scale_coef);
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
	if (ioctl(vid.fd_fb, OWLFB_WAITFORVSYNC, &_)) LOG_info("OWLFB_WAITFORVSYNC failed %s\n", strerror(errno));
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
	vid.de_mem[DE_OVL_BA0(0)/4] = vid.de_mem[DE_OVL_BA0(2)/4] = (uintptr_t)(vid.fb_info.padd + vid.page * PAGE_SIZE);
	DE_enableLayer(vid.de_mem);

	if (sync) PLAT_vsync(0);

	// swap backbuffer
	vid.page ^= 1;
	vid.screen->pixels = vid.fb_info.vadd + vid.page * PAGE_SIZE;

	if (vid.cleared) {
		PLAT_clearVideo(vid.screen);
		vid.cleared = 0;
	}
}

///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 32
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
#define OVERLAY_FB 0
#define OVERLAY_ID 1
static struct OVL_Context {
	SDL_Surface* overlay;
	struct owlfb_overlay_args oargs;
	struct owlfb_overlay_info oinfo;
	ion_alloc_info_t ov_info;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	// setup surface
	ovl.overlay = SDL_CreateRGBSurfaceFrom(NULL,SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,SCALE1(OVERLAY_PITCH), OVERLAY_RGBA_MASK);
	uint32_t size = ovl.overlay->h * ovl.overlay->pitch;
	ovl.ov_info.size = size;
	ion_alloc(vid.fd_ion, &ovl.ov_info);
	ovl.overlay->pixels = ovl.ov_info.vadd;
	memset(ovl.overlay->pixels, 0xff, size);
	
	// setup overlay
	memset(&ovl.oargs, 0, sizeof(struct owlfb_overlay_args));
	ovl.oargs.fb_id = OVERLAY_FB;
	ovl.oargs.overlay_id = OVERLAY_ID;
	ovl.oargs.overlay_type = OWLFB_OVERLAY_VIDEO;
	ovl.oargs.uintptr_overly_info = (uintptr_t)&ovl.oinfo;

	int x,y,w,h;
	w = h = ovl.overlay->w;
	x = FIXED_WIDTH - SCALE1(PADDING) - w;
	y = SCALE1(PADDING);
	
	ovl.oinfo.mem_off = (uintptr_t)ovl.ov_info.padd - vid.finfo.smem_start;
	ovl.oinfo.mem_size = size;
	ovl.oinfo.screen_width = PAGE_WIDTH; // ???
	ovl.oinfo.color_mode = OWL_DSS_COLOR_ARGB32;
	ovl.oinfo.img_width = w;
	ovl.oinfo.img_height = h;
	ovl.oinfo.xoff = 0;
	ovl.oinfo.yoff = 0;
	ovl.oinfo.width = w;
	ovl.oinfo.height = h;
	ovl.oinfo.rotation = 0;
	ovl.oinfo.pos_x = x;	// position
	ovl.oinfo.pos_y = y;	//
	ovl.oinfo.out_width = w;	// scaled size
	ovl.oinfo.out_height = h;	//
	ovl.oinfo.global_alpha_en = 0;
	ovl.oinfo.global_alpha = 0;
	ovl.oinfo.pre_mult_alpha_en = 0;
	ovl.oinfo.zorder = 3;
	if (ioctl(vid.fd_fb, OWLFB_OVERLAY_SETINFO, &ovl.oargs)<0) printf("SETINFO failed %s\n",strerror(errno));
	if (ioctl(vid.fd_fb, OWLFB_OVERLAY_ENABLE, &ovl.oargs)<0) printf("ENABLE failed %s\n",strerror(errno));
	DE_enableLayer(vid.de_mem); // we don't want it to show immediately
	
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
	ion_free(vid.fd_ion, &ovl.ov_info);

	memset(&ovl.oargs, 0, sizeof(struct owlfb_overlay_args));
	ovl.oargs.fb_id = OVERLAY_FB;
	ovl.oargs.overlay_id = OVERLAY_ID;
	ovl.oargs.overlay_type = OWLFB_OVERLAY_VIDEO;
	ovl.oargs.uintptr_overly_info = 0;
	ioctl(vid.fd_fb, OWLFB_OVERLAY_DISABLE, &ovl.oargs);
}
void PLAT_enableOverlay(int enable) {
	de_enable_overlay = enable;
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
	*is_charging = getInt("/sys/class/power_supply/battery/charger_online");
	
	*charge = getInt("/sys/class/power_supply/battery/voltage_now") / 10000; // 310-410
	*charge -= 310; 	// ~0-100

	// TODO: tmp
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
}

void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight.2/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
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
}

///////////////////////////////

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

#define RUMBLE_PATH "/sys/class/power_supply/battery/moto"
void PLAT_setRumble(int strength) {
	int val = MAX(0, MIN((100 * strength)>>16, 100));
	// LOG_info("strength: %8i (%3i/100)\n", strength, val);
	putInt(RUMBLE_PATH, val);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Anbernic RG35XX";
}

int PLAT_isOnline(void) {
	return 0;
}