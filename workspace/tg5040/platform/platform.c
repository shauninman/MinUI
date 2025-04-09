// tg5040
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"
#include <time.h>
#include <pthread.h>
#include <string.h>

int is_brick = 0;
volatile int useAutoCpu = 1;
///////////////////////////////

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	joystick = SDL_JoystickOpen(0);
}
void PLAT_quitInput(void) {
	SDL_JoystickClose(joystick);
	SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

///////////////////////////////

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Texture* effect;
	SDL_Texture* overlay;
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

#define OVERLAYS_FOLDER SDCARD_PATH "/Overlays"
static char* overlay_path = NULL;


SDL_Surface* PLAT_initVideo(void) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	// LOG_info("DEVICE: %s is_brick: %i\n", device, is_brick);
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
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
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"1");
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
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


uint32_t PLAT_get_dominant_color() {
    if (!vid.screen) {
        fprintf(stderr, "Error: vid.screen is NULL.\n");
        return 0;
    }

    uint32_t *pixels = (uint32_t *)vid.screen->pixels;
    if (!pixels) {
        fprintf(stderr, "Error: Unable to access pixel data.\n");
        return 0;
    }

    int width = vid.screen->w;
    int height = vid.screen->h;
    int pixel_count = width * height;

    SDL_PixelFormat *format = vid.screen->format;
    if (!format) {
        fprintf(stderr, "Error: Unable to access pixel format.\n");
        return 0;
    }

    uint8_t r, g, b;

    // Use dynamic memory allocation for the histogram to avoid stack overflow
    uint32_t *color_histogram = (uint32_t *)calloc(256 * 256 * 256, sizeof(uint32_t));
    if (!color_histogram) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 0;
    }

    for (int i = 0; i < pixel_count; i++) {
        SDL_GetRGB(pixels[i], format, &r, &g, &b);
        uint32_t color = (r << 16) | (g << 8) | b;
        color_histogram[color]++;
    }

    // Find the most frequent color
    uint32_t dominant_color = 0;
    uint32_t max_count = 0;
    for (int i = 0; i < 256 * 256 * 256; i++) {
        if (color_histogram[i] > max_count) {
            max_count = color_histogram[i];
            dominant_color = i;
        }
    }

    free(color_histogram);
    return dominant_color;
}


static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, 0);
		
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, 0);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	if (vid.overlay) SDL_DestroyTexture(vid.overlay);
	if (overlay_path) free(overlay_path);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native, is this true?
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	// else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient for 640x480)
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
	// always enabled?
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
			// opacity = 96; // TODO: tmp, for white grid
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
				
				// if (r==247 && g==243 & b==247) opacity = 64;
			}
		}

		if (vid.effect) SDL_DestroyTexture(vid.effect);
		vid.effect = SDL_CreateTextureFromSurface(vid.renderer, tmp);
		SDL_SetTextureAlphaMod(vid.effect, opacity);
		SDL_FreeSurface(tmp);
		effect.live_type = effect.type;
	}
}
int screenx = 0;
int screeny = 0;
void PLAT_setOffsetX(int x) {
    if (x < 0 || x > 100) return;
    screenx = x - 50;
}
void PLAT_setOffsetY(int y) {
    if (y < 0 || y > 100) return;
    screeny = y - 50;  
}
void PLAT_setOverlay(int select, const char* tag) {
    if (vid.overlay) {
        SDL_DestroyTexture(vid.overlay);
        vid.overlay = NULL;
    }

    // Array of overlay filenames
    static const char* overlay_files[] = {
        "",
        "overlay1.png",
        "overlay2.png",
        "overlay3.png",
        "overlay4.png",
        "overlay5.png"
    };
    
    int overlay_count = sizeof(overlay_files) / sizeof(overlay_files[0]);

    if (select < 0 || select >= overlay_count) {
        printf("Invalid selection. Skipping overlay update.\n");
        return;
    }

    const char* filename = overlay_files[select];

    if (!filename || strcmp(filename, "") == 0) {
		overlay_path = strdup("");
        printf("Skipping overlay update.\n");
        return;
    }



    size_t path_len = strlen(OVERLAYS_FOLDER) + strlen(tag) + strlen(filename) + 4; // +3 for slashes and null-terminator
    overlay_path = malloc(path_len);

    if (!overlay_path) {
        perror("malloc failed");
        return;
    }

    snprintf(overlay_path, path_len, "%s/%s/%s", OVERLAYS_FOLDER, tag, filename);
    printf("Overlay path set to: %s\n", overlay_path);
}

static void updateOverlay(void) {
	
	// LOG_info("effect: %s opacity: %i\n", effect_path, opacity);
	if(!vid.overlay) {
		if(overlay_path) {
			SDL_Surface* tmp = IMG_Load(overlay_path);
			if (tmp) {

				if (vid.overlay) SDL_DestroyTexture(vid.overlay);
				vid.overlay = SDL_CreateTextureFromSurface(vid.renderer, tmp);
				SDL_FreeSurface(tmp);
			}
		}
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

void rotate_and_render(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Rect* src_rect, SDL_Rect* dst_rect) {
	int degrees = should_rotate < 3 ? 270:90;
	if(should_rotate == 2 || should_rotate==4) {
		SDL_RenderCopyEx(renderer, texture, src_rect, dst_rect, (double)degrees, NULL, SDL_FLIP_VERTICAL);
	} else  {
		SDL_RenderCopyEx(renderer, texture, src_rect, dst_rect, (double)degrees, NULL, SDL_FLIP_NONE);
	}
    
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
    if (!vid.blit) {
        resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
        SDL_UpdateTexture(vid.texture, NULL, vid.screen->pixels, vid.screen->pitch);
        SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
        SDL_RenderPresent(vid.renderer);
        return;
    }

    SDL_UpdateTexture(vid.texture, NULL, vid.blit->src, vid.blit->src_p);

    SDL_Texture* target = vid.texture;
    int x = vid.blit->src_x;
    int y = vid.blit->src_y;
    int w = vid.blit->src_w;
    int h = vid.blit->src_h;
    if (vid.sharpness == SHARPNESS_CRISP) {
        SDL_SetRenderTarget(vid.renderer, vid.target);
        SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
        SDL_SetRenderTarget(vid.renderer, NULL);
        x *= hard_scale;
        y *= hard_scale;
        w *= hard_scale;
        h *= hard_scale;
        target = vid.target;
    }

    SDL_Rect* src_rect = &(SDL_Rect){x, y, w, h};
    SDL_Rect* dst_rect = &(SDL_Rect){0, 0, device_width, device_height};

    if (vid.blit->aspect == 0) { // native or cropped
        w = vid.blit->src_w * vid.blit->scale;
        h = vid.blit->src_h * vid.blit->scale;
        x = (device_width - w) / 2;
        y = (device_height - h) / 2;
        dst_rect->x = x +screenx;
        dst_rect->y = y +screeny;
        dst_rect->w = w;
        dst_rect->h = h;
    } else if (vid.blit->aspect > 0) { // aspect scaling mode
        if (should_rotate) {
            h = device_width; // Scale height to the screen width
            w = h * vid.blit->aspect;
            if (w > device_height) {
                double ratio = 1 / vid.blit->aspect;
                w = device_height;
                h = w * ratio;
            }
        } else {
            h = device_height;
            w = h * vid.blit->aspect;
            if (w > device_width) {
                double ratio = 1 / vid.blit->aspect;
                w = device_width;
                h = w * ratio;
            }
        }
        x = (device_width - w) / 2;
        y = (device_height - h) / 2;
        dst_rect->x = x +screenx;
        dst_rect->y = y +screeny;
        dst_rect->w = w;
        dst_rect->h = h;
    } else { // full screen mode
        if (should_rotate) {
            dst_rect->w = device_height;
            dst_rect->h = device_width;
            dst_rect->x = (device_width - dst_rect->w) / 2;
            dst_rect->y = (device_height - dst_rect->h) / 2;
        } else {
            dst_rect->x = screenx;
            dst_rect->y = screeny;
            dst_rect->w = device_width;
            dst_rect->h = device_height;
        }
    }

	// FBneo now has auto rotate but keeping this here in case we need it in the future
    // if (should_rotate) {
    //     rotate_and_render(vid.renderer, target, src_rect, dst_rect);
    // } else {
	// below rendercopy goes here
	// }
    SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
    

    updateEffect();
    if (vid.blit && effect.type != EFFECT_NONE && vid.effect) {
        SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0, 0, dst_rect->w, dst_rect->h}, dst_rect);
    }

	updateOverlay();
	SDL_RenderCopy(vid.renderer, vid.overlay, &(SDL_Rect){0, 0,device_width, device_height}, &(SDL_Rect){0, 0,device_width, device_height});
    SDL_RenderPresent(vid.renderer);
    vid.blit = NULL;
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
void PLAT_getCPUTemp() {
	currentcputemp = getInt("/sys/devices/virtual/thermal/thermal_zone0/temp")/1000;

}
void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/axp2202-usb/online");

	*charge = getInt("/sys/class/power_supply/axp2202-battery/capacity");

	// // wifi status, just hooking into the regular PWR polling
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

void PLAT_enableBacklight(int enable) {
	if (enable) {
		if (is_brick) SetRawBrightness(8);
		SetBrightness(GetBrightness());
	}
	else {
		SetRawBrightness(0);
	}
}

void PLAT_powerOff(void) {
	if (CFG_getHaptics()) {
		VIB_singlePulse(VIB_bootStrength, VIB_bootDuration_ms);
	}
	system("rm -f /tmp/nextui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("cat /dev/zero > /dev/fb0 2>/dev/null");
	touch("/tmp/poweroff");
	sync();
	exit(0);
}

int PLAT_supportsDeepSleep(void) { return 1; }

///////////////////////////////

double get_time_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}
double get_process_cpu_time_sec() {
	// this gives cpu time in nanoseconds needed to accurately calculate cpu usage in very short time frames. 
	// unfortunately about 20ms between meassures seems the lowest i can go to get accurate results
	// maybe in the future i will find and even more granual way to get cpu time, but might just be a limit of C or Linux alltogether
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9; // Convert to seconds
}

static pthread_mutex_t currentcpuinfo;
// a roling average for the display values of about 2 frames, otherwise they are unreadable jumping too fast up and down and stuff to read
#define ROLLING_WINDOW 120  

void *PLAT_cpu_monitor(void *arg) {
    struct timespec start_time, curr_time;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);

    long clock_ticks_per_sec = sysconf(_SC_CLK_TCK);

    double prev_real_time = get_time_sec();
    double prev_cpu_time = get_process_cpu_time_sec();

    const int cpu_frequencies[] = {600,650,700,750, 800,850,900,950, 1000,1050,1100,1150, 1200,1250,1300,1350, 1400,1450,1500,1550, 1600,1650,1700,1750, 1800,1850,1900,1950, 2000};
    const int num_freqs = sizeof(cpu_frequencies) / sizeof(cpu_frequencies[0]);
    int current_index = 5; 

    double cpu_usage_history[ROLLING_WINDOW] = {0};
    double cpu_speed_history[ROLLING_WINDOW] = {0};
    int history_index = 0;
    int history_count = 0; 

    while (true) {
        if (useAutoCpu) {
            double curr_real_time = get_time_sec();
            double curr_cpu_time = get_process_cpu_time_sec();

            double elapsed_real_time = curr_real_time - prev_real_time;
            double elapsed_cpu_time = curr_cpu_time - prev_cpu_time;
            double cpu_usage = 0;

            if (elapsed_real_time > 0) {
                cpu_usage = (elapsed_cpu_time / elapsed_real_time) * 100.0;
            }

            pthread_mutex_lock(&currentcpuinfo);

			// the goal here is is to keep cpu usage between 75% and 85% at the lowest possible speed so device stays cool and battery usage is at a minimum
			// if usage falls out of this range it will either scale a step down or up 
			// but if usage hits above 95% we need that max boost and we instant scale up to 2000mhz as long as needed
			// all this happens very fast like 60 times per second, so i'm applying roling averages to display values, so debug screen is readable and gives a good estimate on whats happening cpu wise
			// the roling averages are purely for displaying, the actual scaling is happening realtime each run. 
            if (cpu_usage > 95) {
                current_index = num_freqs - 1; // Instant power needed, cpu is above 95% Jump directly to max boost 2000MHz
            }
            else if (cpu_usage > 85 && current_index < num_freqs - 1) { // otherwise try to keep between 75 and 85 at lowest clock speed
                current_index++; 
            } 
            else if (cpu_usage < 75 && current_index > 0) {
                current_index--; 
            }

            PLAT_setCustomCPUSpeed(cpu_frequencies[current_index] * 1000);

            cpu_usage_history[history_index] = cpu_usage;
            cpu_speed_history[history_index] = cpu_frequencies[current_index];

            history_index = (history_index + 1) % ROLLING_WINDOW;
            if (history_count < ROLLING_WINDOW) {
                history_count++; 
            }

            double sum_cpu_usage = 0, sum_cpu_speed = 0;
            for (int i = 0; i < history_count; i++) {
                sum_cpu_usage += cpu_usage_history[i];
                sum_cpu_speed += cpu_speed_history[i];
            }

            currentcpuse = sum_cpu_usage / history_count;
            currentcpuspeed = sum_cpu_speed / history_count;

            pthread_mutex_unlock(&currentcpuinfo);

            prev_real_time = curr_real_time;
            prev_cpu_time = curr_cpu_time;
			// 20ms really seems lowest i can go, anything lower it becomes innacurate, maybe one day I will find another even more granual way to calculate usage accurately and lower this shit to 1ms haha, altough anything lower than 10ms causes cpu usage in itself so yeah
			// Anyways screw it 20ms is pretty much on a frame by frame basis anyways, so will anything lower really make a difference specially if that introduces cpu usage by itself? 
			// Who knows, maybe some CPU engineer will find my comment here one day and can explain, maybe this is looking for the limits of C and needs Assambler or whatever to call CPU instructions directly to go further, but all I know is PUSH and MOV, how did the orignal Roller Coaster Tycoon developer wrote a whole game like this anyways? Its insane..
            usleep(20000);
        } else {
            // Just measure CPU usage without changing frequency
            double curr_real_time = get_time_sec();
            double curr_cpu_time = get_process_cpu_time_sec();

            double elapsed_real_time = curr_real_time - prev_real_time;
            double elapsed_cpu_time = curr_cpu_time - prev_cpu_time;

            if (elapsed_real_time > 0) {
                double cpu_usage = (elapsed_cpu_time / elapsed_real_time) * 100.0;

                pthread_mutex_lock(&currentcpuinfo);

                cpu_usage_history[history_index] = cpu_usage;

                history_index = (history_index + 1) % ROLLING_WINDOW;
                if (history_count < ROLLING_WINDOW) {
                    history_count++;
                }

                double sum_cpu_usage = 0;
                for (int i = 0; i < history_count; i++) {
                    sum_cpu_usage += cpu_usage_history[i];
                }

                currentcpuse = sum_cpu_usage / history_count;

                pthread_mutex_unlock(&currentcpuinfo);
            }

            prev_real_time = curr_real_time;
            prev_cpu_time = curr_cpu_time;
            usleep(100000); 
        }
    }
}


#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
void PLAT_setCustomCPUSpeed(int speed) {
    FILE *fp = fopen(GOVERNOR_PATH, "w");
    if (fp == NULL) {
        perror("Failed to open scaling_setspeed");
        return;
    }

    fprintf(fp, "%d\n", speed);
    fclose(fp);
}
void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  600000; currentcpuspeed = 600; break;
		case CPU_SPEED_POWERSAVE:	freq = 1200000; currentcpuspeed = 1200; break;
		case CPU_SPEED_NORMAL: 		freq = 1608000; currentcpuspeed = 1600; break;
		case CPU_SPEED_PERFORMANCE: freq = 2000000; currentcpuspeed = 2000; break;
	}
	putInt(GOVERNOR_PATH, freq);
}

#define MAX_STRENGTH 0xFFFF
#define MIN_VOLTAGE 500000
#define MAX_VOLTAGE 3300000
#define RUMBLE_PATH "/sys/class/gpio/gpio227/value"
#define RUMBLE_VOLTAGE_PATH "/sys/class/motor/voltage"

void PLAT_setRumble(int strength) {
	int voltage = MAX_VOLTAGE;

	if(strength > 0 && strength < MAX_STRENGTH) {
		voltage = MIN_VOLTAGE + (int)(strength * ((long long)(MAX_VOLTAGE - MIN_VOLTAGE) / MAX_STRENGTH));
		putInt(RUMBLE_VOLTAGE_PATH, voltage);
	}
	else {
		putInt(RUMBLE_VOLTAGE_PATH, MAX_VOLTAGE);
	}

	// enable rumble - removed the FN switch disabling haptics
	// did not make sense 
	putInt(RUMBLE_PATH, (strength) ? 1 : 0);
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	char* model = getenv("TRIMUI_MODEL");
	if (model) return model;
	return "Trimui Smart Pro";
}

int PLAT_isOnline(void) {
	return online;
}





void PLAT_chmod(const char *file, int writable)
{
    struct stat statbuf;
    if (stat(file, &statbuf) == 0)
    {
        mode_t newMode;
        if (writable)
        {
            // Add write permissions for all users
            newMode = statbuf.st_mode | S_IWUSR | S_IWGRP | S_IWOTH;
        }
        else
        {
            // Remove write permissions for all users
            newMode = statbuf.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
        }

        // Apply the new permissions
        if (chmod(file, newMode) != 0)
        {
            printf("chmod error %d %s", writable, file);
        }
    }
    else
    {
        printf("stat error %d %s", writable, file);
    }
}




void PLAT_initDefaultLeds() {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	if(is_brick) {
	lightsDefault[0] = (LightSettings) {
		"FN 1 key",
		"f1",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[1] = (LightSettings) {
		"FN 2 key",
		"f2",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[2] = (LightSettings) {
		"Topbar",
		"m",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[3] = (LightSettings) {
		"L/R triggers",
		"lr",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
} else {
	lightsDefault[0] = (LightSettings) {
		"Joysticks",
		"lr",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
	lightsDefault[1] = (LightSettings) {
		"Logo",
		"m",
		4,
		1000,
		100,
		0xFFFFFF,
		0xFFFFFF,
		0,
		{},
		1,
		100,
		0
	};
}
}
void PLAT_initLeds(LightSettings *lights) {
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);

	PLAT_initDefaultLeds();
	FILE *file;
	if(is_brick) {
		file = PLAT_OpenSettings("ledsettings_brick.txt");
	}
	else {
		file = PLAT_OpenSettings("ledsettings.txt");
	}

    if (file == NULL)
    {
		
        LOG_info("Unable to open led settings file\n");
	
    }
	else {
		char line[256];
		int current_light = -1;
		while (fgets(line, sizeof(line), file))
		{
			if (line[0] == '[')
			{
				// Section header
				char light_name[255];
				if (sscanf(line, "[%49[^]]]", light_name) == 1)
				{
					current_light++;
					if (current_light < MAX_LIGHTS)
					{
						strncpy(lights[current_light].name, light_name, 255 - 1);
						lights[current_light].name[255 - 1] = '\0'; // Ensure null-termination
						lights[current_light].cycles = -1; // cycles (times animation loops) should basically always be -1 for unlimited unless specifically set
					}
					else
					{
						current_light = -1; // Reset if max_lights exceeded
					}
				}
			}
			else if (current_light >= 0 && current_light < MAX_LIGHTS)
			{
				int temp_value;
				uint32_t temp_color;
				char filename[255];

				if (sscanf(line, "filename=%s", &filename) == 1)
				{
					strncpy(lights[current_light].filename, filename, 255 - 1);
					continue;
				}
				if (sscanf(line, "effect=%d", &temp_value) == 1)
				{
					lights[current_light].effect = temp_value;
					continue;
				}
				if (sscanf(line, "color1=%x", &temp_color) == 1)
				{
					lights[current_light].color1 = temp_color;
					continue;
				}
				if (sscanf(line, "color2=%x", &temp_color) == 1)
				{
					lights[current_light].color2 = temp_color;
					continue;
				}
				if (sscanf(line, "speed=%d", &temp_value) == 1)
				{
					lights[current_light].speed = temp_value;
					continue;
				}
				if (sscanf(line, "brightness=%d", &temp_value) == 1)
				{
					lights[current_light].brightness = temp_value;
					continue;
				}
				if (sscanf(line, "trigger=%d", &temp_value) == 1)
				{
					lights[current_light].trigger = temp_value;
					continue;
				}
				if (sscanf(line, "inbrightness=%d", &temp_value) == 1)
				{
					lights[current_light].inbrightness = temp_value;
					continue;
				}
			}
		}

		fclose(file);
	}

	
	LOG_info("lights setup\n");
}

#define LED_PATH1 "/sys/class/led_anim/max_scale"
#define LED_PATH2 "/sys/class/led_anim/max_scale_lr"
#define LED_PATH3 "/sys/class/led_anim/max_scale_f1f2" 

void PLAT_setLedInbrightness(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
	if(is_brick) {
		if (strcmp(led->filename, "m") == 0) {
			snprintf(filepath, sizeof(filepath), LED_PATH1);
		} else if (strcmp(led->filename, "f1") == 0) {
			snprintf(filepath, sizeof(filepath),LED_PATH3);
		} else  {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_%s", led->filename);
		}
	} else {
		snprintf(filepath, sizeof(filepath), LED_PATH1);
	}
	if (strcmp(led->filename, "f2") != 0) {
		// do nothhing for f2
		PLAT_chmod(filepath, 1);
		file = fopen(filepath, "w");
		if (file != NULL)
		{
			fprintf(file, "%i\n", led->inbrightness);
			fclose(file);
		}
		PLAT_chmod(filepath, 0);
	}
}
void PLAT_setLedBrightness(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
	if(is_brick) {
		if (strcmp(led->filename, "m") == 0) {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale");
		} else if (strcmp(led->filename, "f1") == 0) {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_f1f2");
		} else  {
			snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale_%s", led->filename);
		}
	} else {
		snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/max_scale");
	}
	if (strcmp(led->filename, "f2") != 0) {
		// do nothhing for f2
		PLAT_chmod(filepath, 1);
		file = fopen(filepath, "w");
		if (file != NULL)
		{
			fprintf(file, "%i\n", led->brightness);
			fclose(file);
		}
		PLAT_chmod(filepath, 0);
	}
}
void PLAT_setLedEffect(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->effect);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedEffectCycles(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_cycles_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->cycles);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedEffectSpeed(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_duration_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%i\n", led->speed);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}
void PLAT_setLedColor(LightSettings *led)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_rgb_hex_%s", led->filename);
    PLAT_chmod(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%06X\n", led->color1);
        fclose(file);
    }
    PLAT_chmod(filepath, 0);
}

//////////////////////////////////////////////

int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	sprintf(cmd, "date -s '%d-%d-%d %d:%d:%d'; hwclock -u -w", y,m,d,h,i,s);
	system(cmd);
	return 0; // why does this return an int?
}

#define MAX_LINE_LENGTH 200
#define ZONE_PATH "/usr/share/zoneinfo"
#define ZONE_TAB_PATH ZONE_PATH "/zone.tab"

static char cached_timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
static int cached_tz_count = -1;

int compare_timezones(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

void PLAT_initTimezones() {
    if (cached_tz_count != -1) { // Already initialized
        return;
    }
    
    FILE *file = fopen(ZONE_TAB_PATH, "r");
    if (!file) {
        LOG_info("Error opening file %s\n", ZONE_TAB_PATH);
        return;
    }
    
    char line[MAX_LINE_LENGTH];
    cached_tz_count = 0;
    
    while (fgets(line, sizeof(line), file)) {
        // Skip comment lines
        if (line[0] == '#' || strlen(line) < 3) {
            continue;
        }
        
        char *token = strtok(line, "\t"); // Skip country code
        if (!token) continue;
        
        token = strtok(NULL, "\t"); // Skip latitude/longitude
        if (!token) continue;
        
        token = strtok(NULL, "\t\n"); // Extract timezone
        if (!token) continue;
        
        // Check for duplicates before adding
        int duplicate = 0;
        for (int i = 0; i < cached_tz_count; i++) {
            if (strcmp(cached_timezones[i], token) == 0) {
                duplicate = 1;
                break;
            }
        }
        
        if (!duplicate && cached_tz_count < MAX_TIMEZONES) {
            strncpy(cached_timezones[cached_tz_count], token, MAX_TZ_LENGTH - 1);
            cached_timezones[cached_tz_count][MAX_TZ_LENGTH - 1] = '\0'; // Ensure null-termination
            cached_tz_count++;
        }
    }
    
    fclose(file);
    
    // Sort the list alphabetically
    qsort(cached_timezones, cached_tz_count, MAX_TZ_LENGTH, compare_timezones);
}

void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count) {
    if (cached_tz_count == -1) {
        LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        *tz_count = 0;
        return;
    }
    
    memcpy(timezones, cached_timezones, sizeof(cached_timezones));
    *tz_count = cached_tz_count;
}

char *PLAT_getCurrentTimezone() {

	char *output = (char *)malloc(256);
	if (!output) {
		return false;
	}
	FILE *fp = popen("uci get system.@system[0].zonename", "r");
	if (!fp) {
		free(output);
		return false;
	}
	fgets(output, 256, fp);
	pclose(fp);
	trimTrailingNewlines(output);

	return output;
}

void PLAT_setCurrentTimezone(const char* tz) {
	if (cached_tz_count == -1) {
		LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        return;
    }

	// This makes it permanent
	char *zonename = (char *)malloc(256);
	if (!zonename)
		return;
	snprintf(zonename, 256, "uci set system.@system[0].zonename=\"%s\"", tz);
	system(zonename);
	//system("uci set system.@system[0].zonename=\"Europe/Berlin\"");
	system("uci del -q system.@system[0].timezone");
	system("uci commit system");
	free(zonename);

	// This fixes the timezone until the next reboot
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return;
	}
	snprintf(tz_path, 256, ZONE_PATH "/%s", tz);
	// replace existing symlink
	if (unlink("/tmp/localtime") == -1) {
		LOG_error("Failed to remove existing symlink: %s\n", strerror(errno));
	}
	if (symlink(tz_path, "/tmp/localtime") == -1) {
		LOG_error("Failed to set timezone: %s\n", strerror(errno));
	}
	free(tz_path);

	// apply timezone to kernel
	system("date -k");
}

bool PLAT_getNetworkTimeSync(void) {
	char *output = (char *)malloc(256);
	if (!output) {
		return false;
	}
	FILE *fp = popen("uci get system.ntp.enable", "r");
	if (!fp) {
		free(output);
		return false;
	}
	fgets(output, 256, fp);
	pclose(fp);
	bool result = (output[0] == '1');
	free(output);
	return result;
}

void PLAT_setNetworkTimeSync(bool on) {
	// note: this is not the service residing at /etc/init.d/ntpd - that one has hardcoded time server URLs and does not interact with UCI.
	if (on) {
		// permanment
		system("uci set system.ntp.enable=1");
		system("uci commit system");
		system("/etc/init.d/ntpd reload");
	} else {
		// permanment
		system("uci set system.ntp.enable=0");
		system("uci commit system");
		system("/etc/init.d/ntpd stop");
	}
}

/////////////////////////

bool PLAT_supportSSH() { return true; }

// wifi check /etc/rc.d/S20network