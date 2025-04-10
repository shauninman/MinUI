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
	SDL_Texture* target_layer1;
	SDL_Texture* target_layer2;
	SDL_Texture* stream_layer1;
	SDL_Texture* target_layer3;
	SDL_Texture* target_layer4;
	SDL_Texture* target;
	SDL_Texture* effect;
	SDL_Texture* overlay;
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
static uint32_t SDL_transparentBlack = 0;

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
	SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"0");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION,"1");

	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer2 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer3 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer4 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	
	vid.target	= NULL; // only needed for non-native sizes
	
	vid.screen = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);

	SDL_SetSurfaceBlendMode(vid.screen, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer2, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer3, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer4, SDL_BLENDMODE_BLEND);
	
	
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;

	SDL_transparentBlack = SDL_MapRGBA(vid.screen->format, 0, 0, 0, 0);
	
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

    if (vid.screen->format->format != SDL_PIXELFORMAT_RGBA8888) {
        fprintf(stderr, "Error: Surface is not in RGBA8888 format.\n");
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

    // Use dynamic memory allocation for the histogram
    uint32_t *color_histogram = (uint32_t *)calloc(256 * 256 * 256, sizeof(uint32_t));
    if (!color_histogram) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return 0;
    }

    for (int i = 0; i < pixel_count; i++) {
        uint32_t pixel = pixels[i];

        // Extract R, G, B from RGBA8888
        uint8_t r = (pixel >> 24) & 0xFF;
        uint8_t g = (pixel >> 16) & 0xFF;
        uint8_t b = (pixel >> 8) & 0xFF;

        uint32_t rgb = (r << 16) | (g << 8) | b;
        color_histogram[rgb]++;
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

    // Return as RGBA8888 with full alpha
    return (dominant_color << 8) | 0xFF;
}



static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, SDL_transparentBlack);
		SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);

	if (vid.target) SDL_DestroyTexture(vid.target);
	if (vid.effect) SDL_DestroyTexture(vid.effect);
	if (vid.overlay) SDL_DestroyTexture(vid.overlay);
	if (vid.target_layer3) SDL_DestroyTexture(vid.target_layer3);
	if (vid.target_layer1) SDL_DestroyTexture(vid.target_layer1);
	if (vid.target_layer2) SDL_DestroyTexture(vid.target_layer2);
	if (vid.target_layer4) SDL_DestroyTexture(vid.target_layer4);
	if (overlay_path) free(overlay_path);
	SDL_DestroyTexture(vid.stream_layer1);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
	system("cat /dev/zero > /dev/fb0 2>/dev/null");
}

void PLAT_clearVideo(SDL_Surface* screen) {
	// SDL_FillRect(screen, NULL, 0); // TODO: revisit
	SDL_FillRect(screen, NULL, SDL_transparentBlack);
}
void PLAT_clearAll(void) {
	PLAT_clearLayers(0);
	PLAT_clearVideo(vid.screen); 
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

	SDL_DestroyTexture(vid.stream_layer1);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "2", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}
	

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
				vid.overlay = SDL_CreateTextureFromSurface(vid.renderer, tmp);
				SDL_FreeSurface(tmp);
			}
		}
	}
}

void applyRoundedCorners(SDL_Surface* surface, SDL_Rect* rect, int radius) {
	if (!surface) return;

    Uint32* pixels = (Uint32*)surface->pixels;
    SDL_PixelFormat* fmt = surface->format;
	SDL_Rect target = {0, 0, surface->w, surface->h};
	if (rect)
		target = *rect;
    
    Uint32 transparent_black = SDL_MapRGBA(fmt, 0, 0, 0, 0);  // Fully transparent black

	const int xBeg = target.x;
	const int xEnd = target.x + target.w;
	const int yBeg = target.y;
	const int yEnd = target.y + target.h;
	for (int y = yBeg; y < yEnd; ++y)
	{
		for (int x = xBeg; x < xEnd; ++x) {
            int dx = (x < xBeg + radius) ? xBeg + radius - x : (x >= xEnd - radius) ? x - (xEnd - radius - 1) : 0;
            int dy = (y < yBeg + radius) ? yBeg + radius - y : (y >= yEnd - radius) ? y - (yEnd - radius - 1) : 0;
            if (dx * dx + dy * dy > radius * radius) {
                pixels[y * target.w + x] = transparent_black;  // Set to fully transparent black
            }
        }
    }
}

void PLAT_clearLayers(int layer) {
	if(layer==0 || layer==1) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==2) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==3) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
		SDL_RenderClear(vid.renderer);
	}
	if(layer==0 || layer==4) {
		SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
		SDL_RenderClear(vid.renderer);
	}

	SDL_SetRenderTarget(vid.renderer, NULL);
}
void PLAT_drawOnLayer(SDL_Surface *inputSurface, int x, int y, int w, int h, float brightness, bool maintainAspectRatio,int layer) {
    if (!inputSurface || !vid.target_layer1 || !vid.renderer) return; 

    SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
                                                 SDL_PIXELFORMAT_RGBA8888, 
                                                 SDL_TEXTUREACCESS_TARGET,  
                                                 inputSurface->w, inputSurface->h); 

    if (!tempTexture) {
        printf("Failed to create temporary texture: %s\n", SDL_GetError());
        return;
    }

    SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
    switch (layer)
	{
	case 1:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		break;
	case 2:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		break;
	case 3:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
		break;
	case 4:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
		break;
	default:
		SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
		break;
	}

    // Adjust brightness
    Uint8 r = 255, g = 255, b = 255;
    if (brightness < 1.0f) {
        r = g = b = (Uint8)(255 * brightness);
    } else if (brightness > 1.0f) {
        r = g = b = 255;
    }

    SDL_SetTextureColorMod(tempTexture, r, g, b);

    // Aspect ratio handling
    SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h }; 
    SDL_Rect dstRect = { x, y, w, h };  

    if (maintainAspectRatio) {
        float aspectRatio = (float)inputSurface->w / (float)inputSurface->h;
    
        if (w / (float)h > aspectRatio) {
            dstRect.w = (int)(h * aspectRatio);
        } else {
            dstRect.h = (int)(w / aspectRatio);
        }
    }

    SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);
    SDL_SetRenderTarget(vid.renderer, NULL);
    SDL_DestroyTexture(tempTexture);
}


void PLAT_animateSurface(
	SDL_Surface *inputSurface,
	int x, int y,
	int target_x, int target_y,
	int w, int h,
	int duration_ms,
	int start_opacity,
	int target_opacity,
	int layer
) {
	if (!inputSurface || !vid.target_layer2 || !vid.renderer) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);  // Enable blending for opacity

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		// Interpolate opacity
		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);

		if (layer == 0)
			SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
		else
			SDL_SetRenderTarget(vid.renderer, vid.target_layer4);

		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0); 
		SDL_RenderClear(vid.renderer);

		SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h };
		SDL_Rect dstRect = { current_x, current_y, w, h };
		SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();
	}

	SDL_DestroyTexture(tempTexture);
}

void PLAT_revealSurface(
	SDL_Surface *inputSurface,
	int x, int y,
	int w, int h,
	int duration_ms,
	const char* direction,
	int opacity,
	int layer
) {
	if (!inputSurface || !vid.target_layer2 || !vid.renderer) return;

	SDL_Surface* formatted = SDL_CreateRGBSurfaceWithFormat(0, inputSurface->w, inputSurface->h, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!formatted) {
		printf("Failed to create formatted surface: %s\n", SDL_GetError());
		return;
	}
	SDL_FillRect(formatted, NULL, SDL_MapRGBA(formatted->format, 0, 0, 0, 0));
	SDL_SetSurfaceBlendMode(inputSurface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(inputSurface,&(SDL_Rect){0,0,w,h}, formatted, &(SDL_Rect){0,0,w,h});

	SDL_Texture* tempTexture = SDL_CreateTextureFromSurface(vid.renderer, formatted);
	SDL_FreeSurface(formatted);

	if (!tempTexture) {
		printf("Failed to create texture: %s\n", SDL_GetError());
		return;
	}

	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(tempTexture, opacity);

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {
		float t = (float)frame / total_frames;
		if (t > 1.0f) t = 1.0f;

		int reveal_w = w;
		int reveal_h = h;
		int src_x = 0;
		int src_y = 0;

		if (strcmp(direction, "left") == 0) {
			reveal_w = (int)(w * t + 0.5f);
		}
		else if (strcmp(direction, "right") == 0) {
			reveal_w = (int)(w * t + 0.5f);
			src_x = w - reveal_w;
		}
		else if (strcmp(direction, "up") == 0) {
			reveal_h = (int)(h * t + 0.5f);
		}
		else if (strcmp(direction, "down") == 0) {
			reveal_h = (int)(h * t + 0.5f);
			src_y = h - reveal_h;
		}

		SDL_Rect srcRect = { src_x, src_y, reveal_w, reveal_h };
		SDL_Rect dstRect = { x + src_x, y + src_y, reveal_w, reveal_h };

		SDL_SetRenderTarget(vid.renderer, (layer == 0) ? vid.target_layer2 : vid.target_layer4);

		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);

		if (reveal_w > 0 && reveal_h > 0)
			SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(tempTexture);
}

static int text_offset = 0;

int PLAT_resetScrollText(TTF_Font* font, const char* in_name,int max_width) {
	int text_width, text_height;
	
    TTF_SizeUTF8(font, in_name, &text_width, &text_height);

	text_offset = 0;

	if (text_width <= max_width) {
		return 0;
	} else {
		return 1;
	}
}
void PLAT_scrollTextTexture(
    TTF_Font* font,
    const char* in_name,
    int x, int y,      // Position on target layer
    int w, int h,      // Clipping width and height
    int padding,
    SDL_Color color,
    float transparency
) {
   
    static int frame_counter = 0;

    if (transparency < 0.0f) transparency = 0.0f;
    if (transparency > 1.0f) transparency = 1.0f;
    color.a = (Uint8)(transparency * 255);

    char scroll_text[1024];
    snprintf(scroll_text, sizeof(scroll_text), "%s  %s", in_name, in_name);

	SDL_Surface* tempSur = TTF_RenderUTF8_Blended(font, scroll_text, color);
	SDL_Surface* text_surface = SDL_CreateRGBSurfaceWithFormat(0,
		tempSur->w, tempSur->h, 32, SDL_PIXELFORMAT_RGBA8888);
	
	SDL_FillRect(text_surface, NULL, THEME_COLOR1);
	SDL_BlitSurface(tempSur, NULL, text_surface, NULL);

    SDL_Texture* full_text_texture = SDL_CreateTextureFromSurface(vid.renderer, text_surface);
    int full_text_width = text_surface->w;
	int full_text_height = text_surface->h;
    SDL_FreeSurface(text_surface);
    SDL_FreeSurface(tempSur);

    if (!full_text_texture) {
        return;
    }

    SDL_SetTextureBlendMode(full_text_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(full_text_texture, color.a);

    SDL_SetRenderTarget(vid.renderer, vid.target_layer4);

    SDL_Rect src_rect = { text_offset, 0, w, full_text_height };
    SDL_Rect dst_rect = { x, y, w, full_text_height };

    SDL_RenderCopy(vid.renderer, full_text_texture, &src_rect, &dst_rect);

    SDL_SetRenderTarget(vid.renderer, NULL);
    SDL_DestroyTexture(full_text_texture);

    if (full_text_width > w + padding) {
        frame_counter++;
        if (frame_counter >= 1) {
            text_offset += 3;
            if (text_offset >= full_text_width / 2) {
                text_offset = 0;
            }
            frame_counter = 0;
        }
    } else {
        text_offset = 0;
    }

	PLAT_GPU_Flip();
}

// super fast without update_texture to draw screen
void PLAT_GPU_Flip() {
	SDL_RenderClear(vid.renderer);
	SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
	SDL_RenderPresent(vid.renderer);
}

void PLAT_GPU_core_flip(const void *data,size_t pitch,int width,int height) {

	if (vid.width != width || vid.height != height) {
		if (vid.stream_layer1) SDL_DestroyTexture(vid.stream_layer1);
		vid.stream_layer1 = SDL_CreateTexture(
			vid.renderer,
			SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_STREAMING,
			width,
			height
		);
		vid.width = width;
		vid.height = height;
	}

	
	SDL_RenderClear(vid.renderer);
	SDL_UpdateTexture(vid.stream_layer1, NULL, data, (int)pitch);
	SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
	SDL_RenderPresent(vid.renderer);
}

void PLAT_animateAndRevealSurfaces(
	SDL_Surface* inputMoveSurface,
	SDL_Surface* inputRevealSurface,
	int move_start_x, int move_start_y,
	int move_target_x, int move_target_y,
	int move_w, int move_h,
	int reveal_x, int reveal_y,
	int reveal_w, int reveal_h,
	const char* reveal_direction,
	int duration_ms,
	int move_start_opacity,
	int move_target_opacity,
	int reveal_opacity,
	int layer1,
	int layer2
) {
	if (!inputMoveSurface || !inputRevealSurface || !vid.renderer || !vid.target_layer2) return;

	SDL_Texture* moveTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputMoveSurface->w, inputMoveSurface->h);
	if (!moveTexture) {
		printf("Failed to create move texture: %s\n", SDL_GetError());
		return;
	}
	SDL_UpdateTexture(moveTexture, NULL, inputMoveSurface->pixels, inputMoveSurface->pitch);
	SDL_SetTextureBlendMode(moveTexture, SDL_BLENDMODE_BLEND);

	SDL_Surface* formatted = SDL_CreateRGBSurfaceWithFormat(0, inputRevealSurface->w, inputRevealSurface->h, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!formatted) {
		SDL_DestroyTexture(moveTexture);
		printf("Failed to create formatted surface for reveal: %s\n", SDL_GetError());
		return;
	}
	SDL_FillRect(formatted, NULL, SDL_MapRGBA(formatted->format, 0, 0, 0, 0));
	SDL_SetSurfaceBlendMode(inputRevealSurface, SDL_BLENDMODE_BLEND);
	SDL_BlitSurface(inputRevealSurface, &(SDL_Rect){0, 0, reveal_w, reveal_h}, formatted, &(SDL_Rect){0, 0, reveal_w, reveal_h});
	SDL_Texture* revealTexture = SDL_CreateTextureFromSurface(vid.renderer, formatted);
	SDL_FreeSurface(formatted);
	if (!revealTexture) {
		SDL_DestroyTexture(moveTexture);
		printf("Failed to create reveal texture: %s\n", SDL_GetError());
		return;
	}
	SDL_SetTextureBlendMode(revealTexture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(revealTexture, reveal_opacity);

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	for (int frame = 0; frame <= total_frames; ++frame) {
		float t = (float)frame / total_frames;
		if (t > 1.0f) t = 1.0f;

		int current_x = move_start_x + (int)((move_target_x - move_start_x) * t);
		int current_y = move_start_y + (int)((move_target_y - move_start_y) * t);
		int current_opacity = move_start_opacity + (int)((move_target_opacity - move_start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;
		SDL_SetTextureAlphaMod(moveTexture, current_opacity);

		int reveal_src_x = 0, reveal_src_y = 0;
		int reveal_draw_w = reveal_w, reveal_draw_h = reveal_h;

		if (strcmp(reveal_direction, "left") == 0) {
			reveal_draw_w = (int)(reveal_w * t + 0.5f);
		}
		else if (strcmp(reveal_direction, "right") == 0) {
			reveal_draw_w = (int)(reveal_w * t + 0.5f);
			reveal_src_x = reveal_w - reveal_draw_w;
		}
		else if (strcmp(reveal_direction, "up") == 0) {
			reveal_draw_h = (int)(reveal_h * t + 0.5f);
		}
		else if (strcmp(reveal_direction, "down") == 0) {
			reveal_draw_h = (int)(reveal_h * t + 0.5f);
			reveal_src_y = reveal_h - reveal_draw_h;
		}

		SDL_Rect revealSrc = { reveal_src_x, reveal_src_y, reveal_draw_w, reveal_draw_h };
		SDL_Rect revealDst = { reveal_x + reveal_src_x, reveal_y + reveal_src_y, reveal_draw_w, reveal_draw_h };

		SDL_SetRenderTarget(vid.renderer, (layer1 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(vid.renderer, (2 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_NONE);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);
		SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);

		SDL_SetRenderTarget(vid.renderer, (layer1 == 0) ? vid.target_layer3 : vid.target_layer4);
		SDL_Rect moveDst = { current_x, current_y, move_w, move_h };
		SDL_RenderCopy(vid.renderer, moveTexture, NULL, &moveDst);

		SDL_SetRenderTarget(vid.renderer, (layer2 == 0) ? vid.target_layer3 : vid.target_layer4);

		if (reveal_draw_w > 0 && reveal_draw_h > 0)
			SDL_RenderCopy(vid.renderer, revealTexture, &revealSrc, &revealDst);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(moveTexture);
	SDL_DestroyTexture(revealTexture);
}


void PLAT_animateSurfaceOpacity(
	SDL_Surface *inputSurface,
	int x, int y, int w, int h,
	int start_opacity, int target_opacity,
	int duration_ms,
	int layer
) {
	if (!inputSurface) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND); 

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	SDL_Texture* target_layer = (layer == 0) ? vid.target_layer2 : vid.target_layer4;
	if (!target_layer) {
		SDL_DestroyTexture(tempTexture);
		return;
	}

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;
		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);
		SDL_SetRenderTarget(vid.renderer, target_layer);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect dstRect = { x, y, w, h };
		SDL_RenderCopy(vid.renderer, tempTexture, NULL, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_flip(vid.screen,0);

	}

	SDL_DestroyTexture(tempTexture);
}
void PLAT_animateSurfaceOpacityAndScale(
	SDL_Surface *inputSurface,
	int x, int y,                         // Center position
	int start_w, int start_h,
	int target_w, int target_h,
	int start_opacity, int target_opacity,
	int duration_ms,
	int layer
) {
	if (!inputSurface || !vid.renderer) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
	SDL_SetTextureBlendMode(tempTexture, SDL_BLENDMODE_BLEND); 

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	SDL_Texture* target_layer = (layer == 0) ? vid.target_layer2 : vid.target_layer4;
	if (!target_layer) {
		SDL_DestroyTexture(tempTexture);
		return;
	}

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		int current_w = start_w + (int)((target_w - start_w) * t);
		int current_h = start_h + (int)((target_h - start_h) * t);

		SDL_SetTextureAlphaMod(tempTexture, current_opacity);

		SDL_SetRenderTarget(vid.renderer, target_layer);
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect dstRect = {
			x - current_w / 2,
			y - current_h / 2,
			current_w,
			current_h
		};

		SDL_RenderCopy(vid.renderer, tempTexture, NULL, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(tempTexture);
}



SDL_Surface* PLAT_captureRendererToSurface() {
	if (!vid.renderer) return NULL;

	int width, height;
	SDL_GetRendererOutputSize(vid.renderer, &width, &height);

	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!surface) {
		printf("Failed to create surface: %s\n", SDL_GetError());
		return NULL;
	}

	Uint32 black = SDL_MapRGBA(surface->format, 0, 0, 0, 255);
	SDL_FillRect(surface, NULL, black);

	if (SDL_RenderReadPixels(vid.renderer, NULL, SDL_PIXELFORMAT_RGBA8888, surface->pixels, surface->pitch) != 0) {
		printf("Failed to read pixels from renderer: %s\n", SDL_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}

	// remove transparancy
	Uint32* pixels = (Uint32*)surface->pixels;
	int total_pixels = (surface->pitch / 4) * surface->h;

	for (int i = 0; i < total_pixels; i++) {
		Uint8 r, g, b, a;
		SDL_GetRGBA(pixels[i], surface->format, &r, &g, &b, &a);
		pixels[i] = SDL_MapRGBA(surface->format, r, g, b, 255);
	}

	SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
	return surface;
}

void PLAT_animateAndFadeSurface(
	SDL_Surface *inputSurface,
	int x, int y, int target_x, int target_y, int w, int h, int duration_ms,
	SDL_Surface *fadeSurface,
	int fade_x, int fade_y, int fade_w, int fade_h,
	int start_opacity, int target_opacity,int layer
) {
	if (!inputSurface || !vid.renderer) return;

	SDL_Texture* moveTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!moveTexture) {
		printf("Failed to create move texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(moveTexture, NULL, inputSurface->pixels, inputSurface->pitch);

	SDL_Texture* fadeTexture = NULL;
	if (fadeSurface) {
		fadeTexture = SDL_CreateTextureFromSurface(vid.renderer, fadeSurface);
		if (!fadeTexture) {
			printf("Failed to create fade texture: %s\n", SDL_GetError());
			SDL_DestroyTexture(moveTexture);
			return;
		}
		SDL_SetTextureBlendMode(fadeTexture, SDL_BLENDMODE_BLEND);
	}

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	Uint32 start_time = SDL_GetTicks();

	for (int frame = 0; frame <= total_frames; ++frame) {

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		switch (layer)
		{
		case 1:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
			break;
		case 2:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer2);
			break;
		case 3:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer3);
			break;
		case 4:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer4);
			break;
		default:
			SDL_SetRenderTarget(vid.renderer, vid.target_layer1);
			break;
		}
		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0);
		SDL_RenderClear(vid.renderer);

		SDL_Rect moveSrcRect = { 0, 0, inputSurface->w, inputSurface->h };
		SDL_Rect moveDstRect = { current_x, current_y, w, h };
		SDL_RenderCopy(vid.renderer, moveTexture, &moveSrcRect, &moveDstRect);

		if (fadeTexture) {
			SDL_SetTextureAlphaMod(fadeTexture, current_opacity);
			SDL_Rect fadeDstRect = { fade_x, fade_y, fade_w, fade_h };
			SDL_RenderCopy(vid.renderer, fadeTexture, NULL, &fadeDstRect);
		}

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_GPU_Flip();

	}

	SDL_DestroyTexture(moveTexture);
	if (fadeTexture) SDL_DestroyTexture(fadeTexture);
}



void PLAT_present() {
	SDL_RenderPresent(vid.renderer);
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

void PLAT_flipHidden() {
	SDL_RenderClear(vid.renderer);
	resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
	SDL_UpdateTexture(vid.stream_layer1, NULL, vid.screen->pixels, vid.screen->pitch);
	SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
	//  SDL_RenderPresent(vid.renderer); // no present want to flip  hidden
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	// dont think we need this here tbh
	// SDL_RenderClear(vid.renderer);
    if (!vid.blit) {
        resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
        SDL_UpdateTexture(vid.stream_layer1, NULL, vid.screen->pixels, vid.screen->pitch);
		SDL_RenderCopy(vid.renderer, vid.target_layer1, NULL, NULL);
        SDL_RenderCopy(vid.renderer, vid.target_layer2, NULL, NULL);
        SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
		SDL_RenderCopy(vid.renderer, vid.target_layer3, NULL, NULL);
		SDL_RenderCopy(vid.renderer, vid.target_layer4, NULL, NULL);
        SDL_RenderPresent(vid.renderer);
        return;
    }

    SDL_UpdateTexture(vid.stream_layer1, NULL, vid.blit->src, vid.blit->src_p);

    SDL_Texture* target = vid.stream_layer1;
    int x = vid.blit->src_x;
    int y = vid.blit->src_y;
    int w = vid.blit->src_w;
    int h = vid.blit->src_h;
    if (vid.sharpness == SHARPNESS_CRISP) {
		
        SDL_SetRenderTarget(vid.renderer, vid.target);
        SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
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
	
    SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
    

    updateEffect();
    if (vid.blit && effect.type != EFFECT_NONE && vid.effect) {
        SDL_RenderCopy(vid.renderer, vid.effect, &(SDL_Rect){0, 0, dst_rect->w, dst_rect->h}, dst_rect);
    }

	updateOverlay();
	if(vid.overlay) {
		SDL_RenderCopy(vid.renderer, vid.overlay, &(SDL_Rect){0, 0,device_width, device_height}, &(SDL_Rect){0, 0,device_width, device_height});
	}
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