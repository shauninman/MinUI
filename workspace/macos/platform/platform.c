// macos
#include <stdio.h>
#include <stdlib.h>
// #include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "msettings.h"

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"


///////////////////////////////////////

// Legacy MinUI settings
typedef struct SettingsV3 {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack;
} SettingsV3;

// First NextUI settings format
typedef struct SettingsV4 {
	int version; // future proofing
	int brightness;
	int colortemperature; // 0-20
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack; 
} SettingsV4;

// Current NextUI settings format
typedef struct SettingsV5 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV5;

// When incrementing SETTINGS_VERSION, update the Settings typedef and add
// backwards compatibility to InitSettings!
#define SETTINGS_VERSION 5
typedef SettingsV5 Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 2,
	.colortemperature = 20,
	.headphones = 4,
	.speaker = 8,
	.mute = 0,
	.jack = 0,
};
static Settings* msettings;

static char SettingsPath[256];

///////////////////////////////////////

int peekVersion(const char *filename) {
	int version = 0;
	FILE *file = fopen(filename, "r");
	if (file) {
		fread(&version, sizeof(int), 1, file);
		fclose(file);
	}
	return version;
}

void InitSettings(void){
	// We are not really using them, but we should be able to debug them
	sprintf(SettingsPath, "%s/msettings.bin", SDCARD_PATH "/.userdata");
	msettings = (Settings*)malloc(sizeof(Settings));
	
	int version = peekVersion(SettingsPath);
	if(version > 0) {
		// fopen file pointer
		int fd = open(SettingsPath, O_RDONLY);
		if(fd) {
			if (version == SETTINGS_VERSION)
			{
				printf("Found settings v5.\n");
				// changes: colortemp 0-40
				// read the rest
				read(fd, msettings, sizeof(SettingsV5));
			}
			else if(version==4) {
				printf("Found settings v4.\n");
				SettingsV4 old;
				// read old settings from fd
				read(fd, &old, sizeof(SettingsV4));
				// colortemp was 0-20 here
				msettings->colortemperature = old.colortemperature * 2;
			}
			else if(version==3) {
				printf("Found settings v3.\n");
				SettingsV3 old;
				read(fd, &old, sizeof(SettingsV3));
				// no colortemp setting yet, default value used. 
				// copy the rest
				msettings->brightness = old.brightness;
				msettings->headphones = old.headphones;
				msettings->speaker = old.speaker;
				msettings->mute = old.mute;
				msettings->jack = old.jack;
				msettings->colortemperature = 20;
			}
			else {
				printf("Found unsupported settings version: %i.\n", version);
				// load defaults
				memcpy(msettings, &DefaultSettings, sizeof(Settings));
			}

			close(fd);
		}
		else {
			printf("Unable to read settings, using defaults\n");
			// load defaults
			memcpy(msettings, &DefaultSettings, sizeof(Settings));
		}
	}
	else {
		printf("No settings found, using defaults\n");
		// load defaults
		memcpy(msettings, &DefaultSettings, sizeof(Settings));
	}
}
void QuitSettings(void){
	// dealloc settings
	free(msettings);
	msettings = NULL;
}

int GetBrightness(void) { return 0; }
int GetColortemp(void) { return 0; }
int GetContrast(void) { return 0; }
int GetSaturation(void) { return 0; }
int GetExposure(void) { return 0; }
int GetVolume(void) { return 0; }

void SetRawBrightness(int value) {}
void SetRawVolume(int value){}

void SetBrightness(int value) {}
void SetColortemp(int value) {}
void SetContrast(int value) {}
void SetSaturation(int value) {}
void SetExposure(int value) {}
void SetVolume(int value) {}

int GetJack(void) { return 0; }
void SetJack(int value) {}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return 0; }

///////////////////////////////

static SDL_Joystick *joystick;
void PLAT_initInput(void) {
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
	SDL_Texture* background;
	SDL_Texture* animationlayer;
	SDL_Texture* animationlayer2;
	SDL_Texture* foreground;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
} vid;

static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;
static uint32_t SDL_transparentBlack = 0;

SDL_Surface *PLAT_initVideo(void)
{
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
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
	//LOG_info("Available display modes:\n");
	SDL_DisplayMode mode;
	//for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	SDL_GetCurrentDisplayMode(0, &mode);
	if (mode.h>mode.w)
		rotate = 1;
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	
	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	SDL_SetRenderDrawBlendMode(vid.renderer, SDL_BLENDMODE_BLEND);
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.background = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.animationlayer = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.animationlayer2 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.foreground = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer	= SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
	vid.screen = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);
	SDL_SetSurfaceBlendMode(vid.buffer, SDL_BLENDMODE_BLEND);
	SDL_SetSurfaceBlendMode(vid.screen, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.animationlayer, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.animationlayer2, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.foreground, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.texture, SDL_BLENDMODE_BLEND);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	PWR_disablePowerOff();

	SDL_transparentBlack = SDL_MapRGBA(vid.screen->format, 0, 0, 0, 0);

	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	return vid.screen;
}

static void clearVideo(void) {
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_FillRect(vid.screen, NULL, SDL_transparentBlack);
		
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_FillRect(vid.buffer, NULL, SDL_transparentBlack);
		SDL_UnlockTexture(vid.texture);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
		
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.foreground) SDL_DestroyTexture(vid.foreground);
	if (vid.background) SDL_DestroyTexture(vid.background);
	if (vid.animationlayer) SDL_DestroyTexture(vid.animationlayer);
	if (vid.animationlayer2) SDL_DestroyTexture(vid.animationlayer2);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	//SDL_FillRect(screen, NULL, 0); // TODO: revisit
	SDL_FillRect(screen, NULL, SDL_transparentBlack);
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	
}

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	// PLAT_clearVideo(vid.screen);
	
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.texture, SDL_BLENDMODE_BLEND);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);
	
	vid.buffer = SDL_CreateRGBSurfaceWithFormat(SDL_SWSURFACE, w,h, 32, SDL_PIXELFORMAT_RGBA8888);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	
}
void PLAT_setNearestNeighbor(int enabled) {
	// always enabled?
}
void PLAT_setSharpness(int sharpness) {
	// buh
}
void PLAT_setEffect(int next_type) {
}

void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
	scale1x1_c16(
		renderer->src,renderer->dst,
		renderer->true_w,renderer->true_h,renderer->src_p,
		vid.screen->w,vid.screen->h,vid.screen->pitch // fixed in this implementation
		// renderer->dst_w,renderer->dst_h,renderer->dst_p
	);
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

void PLAT_clearForeground() {
	SDL_SetRenderTarget(vid.renderer, vid.foreground);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, NULL);
}
void PLAT_clearBackground() {
	SDL_SetRenderTarget(vid.renderer, vid.background);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, NULL);
}
void PLAT_clearAnimationLayer() {
	SDL_SetRenderTarget(vid.renderer, vid.animationlayer);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, NULL);
}
void PLAT_clearAllLayers() {
	SDL_SetRenderTarget(vid.renderer, vid.foreground);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, vid.animationlayer);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, vid.animationlayer2);
    SDL_RenderClear(vid.renderer);
	SDL_SetRenderTarget(vid.renderer, NULL);
}
void PLAT_drawBackground(SDL_Surface *inputSurface, int x, int y, int w, int h, float brightness, bool maintainAspectRatio) {
    if (!inputSurface || !vid.background || !vid.renderer) return; 

    SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
                                                 SDL_PIXELFORMAT_RGBA8888, 
                                                 SDL_TEXTUREACCESS_TARGET,  
                                                 inputSurface->w, inputSurface->h); 

    if (!tempTexture) {
        printf("Failed to create temporary texture: %s\n", SDL_GetError());
        return;
    }

    SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
    SDL_SetRenderTarget(vid.renderer, vid.background);

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
	LOG_info("drawing bg\n");
}

void PLAT_drawForeground(SDL_Surface *inputSurface, int x, int y, int w, int h) {
    if (!inputSurface || !vid.foreground || !vid.renderer) return; 
    SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
                                                 SDL_PIXELFORMAT_RGBA8888, 
                                                 SDL_TEXTUREACCESS_TARGET,  
                                                 inputSurface->w, inputSurface->h); 

    if (!tempTexture) {
        printf("Failed to create temporary texture: %s\n", SDL_GetError());
        return;
    }

    SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);
    SDL_SetRenderTarget(vid.renderer, vid.foreground);

    SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h }; 
    SDL_Rect dstRect = { x, y, w, h };  

    SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);
    SDL_SetRenderTarget(vid.renderer, NULL);
    SDL_DestroyTexture(tempTexture);
}

void PLAT_animateSurface(SDL_Surface *inputSurface, int x, int y, int target_x, int target_y, int w, int h, int duration_ms,int layer) {
	if (!inputSurface || !vid.animationlayer || !vid.renderer) return;

	SDL_Texture* tempTexture = SDL_CreateTexture(vid.renderer,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		inputSurface->w, inputSurface->h);

	if (!tempTexture) {
		printf("Failed to create temporary texture: %s\n", SDL_GetError());
		return;
	}

	SDL_UpdateTexture(tempTexture, NULL, inputSurface->pixels, inputSurface->pitch);

	const int fps = 60;
	const int frame_delay = 1000 / fps;
	const int total_frames = duration_ms / frame_delay;

	Uint32 start_time = SDL_GetTicks();

	for (int frame = 0; frame <= total_frames; ++frame) {
		Uint32 frame_start = SDL_GetTicks();

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		if(layer==0)
			SDL_SetRenderTarget(vid.renderer, vid.animationlayer);
		else
			SDL_SetRenderTarget(vid.renderer, vid.animationlayer2);

		SDL_SetRenderDrawColor(vid.renderer, 0, 0, 0, 0); 
		SDL_RenderClear(vid.renderer);

		SDL_Rect srcRect = { 0, 0, inputSurface->w, inputSurface->h };
		SDL_Rect dstRect = { current_x, current_y, w, h };
		SDL_RenderCopy(vid.renderer, tempTexture, &srcRect, &dstRect);

		SDL_SetRenderTarget(vid.renderer, NULL);
		PLAT_flip(NULL, 0);

		// Frame timing control to avoid busy waiting
		Uint32 frame_time = SDL_GetTicks() - frame_start;
		if (frame_time < frame_delay) {
			SDL_Delay(frame_delay - frame_time);
		}
	}

	SDL_DestroyTexture(tempTexture);
}

SDL_Surface* PLAT_captureRendererToSurface() {
	if (!vid.renderer) return NULL;

	int width, height;
	SDL_GetRendererOutputSize(vid.renderer, &width, &height);

	// Create a surface to hold the renderer output
	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
	if (!surface) {
		printf("Failed to create surface: %s\n", SDL_GetError());
		return NULL;
	}

	// Read pixels from the renderer into the surface
	if (SDL_RenderReadPixels(vid.renderer, NULL, SDL_PIXELFORMAT_RGBA8888, surface->pixels, surface->pitch) != 0) {
		printf("Failed to read pixels from renderer: %s\n", SDL_GetError());
		SDL_FreeSurface(surface);
		return NULL;
	}

	return surface;
}
void PLAT_animateAndFadeSurface(
	SDL_Surface *inputSurface,
	int x, int y, int target_x, int target_y, int w, int h, int duration_ms,
	SDL_Surface *fadeSurface,
	int fade_x, int fade_y, int fade_w, int fade_h,
	int start_opacity, int target_opacity,int layer
) {
	if (!inputSurface || !vid.animationlayer || !vid.renderer) return;

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
		Uint32 frame_start = SDL_GetTicks();

		float t = (float)frame / total_frames;

		int current_x = x + (int)((target_x - x) * t);
		int current_y = y + (int)((target_y - y) * t);

		int current_opacity = start_opacity + (int)((target_opacity - start_opacity) * t);
		if (current_opacity < 0) current_opacity = 0;
		if (current_opacity > 255) current_opacity = 255;

		SDL_SetRenderTarget(vid.renderer, vid.animationlayer);
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
		PLAT_flip(NULL, 0);

		// Frame timing control
		Uint32 frame_time = SDL_GetTicks() - frame_start;
		if (frame_time < frame_delay) {
			SDL_Delay(frame_delay - frame_time);
		}
	}

	SDL_DestroyTexture(moveTexture);
	if (fadeTexture) SDL_DestroyTexture(fadeTexture);
}



void PLAT_present() {
	SDL_RenderPresent(vid.renderer);
}

void PLAT_flipHidden() {
	SDL_RenderClear(vid.renderer);
	resizeVideo(device_width, device_height, FIXED_PITCH); // !!!???
	SDL_UpdateTexture(vid.texture, NULL, vid.screen->pixels, vid.screen->pitch);
	SDL_RenderCopy(vid.renderer, vid.background, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.animationlayer, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.foreground, NULL, NULL);
	SDL_RenderCopy(vid.renderer, vid.animationlayer2, NULL, NULL);
	//  SDL_RenderPresent(vid.renderer); // no present want to flip  hidden
}

void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	// dont think we need this here tbh
	// SDL_RenderClear(vid.renderer);
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_UpdateTexture(vid.texture,NULL,vid.screen->pixels,vid.screen->pitch);
		if (rotate) {
			//LOG_info("rotated\n");

			SDL_RenderCopyEx(vid.renderer, vid.background, NULL,
				&(SDL_Rect){device_height, 0, device_width, device_height}, 
				rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
			SDL_RenderCopyEx(vid.renderer, vid.animationlayer, NULL,
				&(SDL_Rect){device_height, 0, device_width, device_height}, 
				rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
			SDL_RenderCopyEx(vid.renderer, vid.texture, NULL, 
				&(SDL_Rect){device_height, 0, device_width, device_height}, 
				rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
			SDL_RenderCopyEx(vid.renderer, vid.foreground, NULL, 
				&(SDL_Rect){device_height, 0, device_width, device_height}, 
				rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
			SDL_RenderCopyEx(vid.renderer, vid.animationlayer2, NULL,
				&(SDL_Rect){device_height, 0, device_width, device_height}, 
				rotate * 90, &(SDL_Point){0, 0}, SDL_FLIP_NONE);
		}
		else {
			//LOG_info("not rotated\n");
			SDL_RenderCopy(vid.renderer, vid.background, NULL, NULL);
			SDL_RenderCopy(vid.renderer, vid.animationlayer, NULL, NULL);
			SDL_RenderCopy(vid.renderer, vid.texture, NULL, NULL);
			SDL_RenderCopy(vid.renderer, vid.foreground, NULL, NULL);
			SDL_RenderCopy(vid.renderer, vid.animationlayer2, NULL, NULL);
		}
		SDL_RenderPresent(vid.renderer);

		return;
	}
	
	if (!vid.blit) resizeVideo(FIXED_WIDTH,FIXED_HEIGHT,FIXED_PITCH); // !!!???
	
	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
	SDL_UnlockTexture(vid.texture);
	
	SDL_Rect* src_rect = NULL;
	SDL_Rect* dst_rect = NULL;
	SDL_Rect src_r = {0};
	SDL_Rect dst_r = {0};
	if (vid.blit) {
		src_r.x = vid.blit->src_x;
		src_r.y = vid.blit->src_y;
		src_r.w = vid.blit->src_w;
		src_r.h = vid.blit->src_h;
		src_rect = &src_r;
		
		if (vid.blit->aspect==0) { // native (or cropped?)
			int w = vid.blit->src_w * vid.blit->scale;
			int h = vid.blit->src_h * vid.blit->scale;
			int x = (FIXED_WIDTH - w) / 2;
			int y = (FIXED_HEIGHT - h) / 2;
						
			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
		}
		else if (vid.blit->aspect>0) { // aspect
			int h = FIXED_HEIGHT;
			int w = h * vid.blit->aspect;
			if (w>FIXED_WIDTH) {
				double ratio = 1 / vid.blit->aspect;
				w = FIXED_WIDTH;
				h = w * ratio;
			}
			int x = (FIXED_WIDTH - w) / 2;
			int y = (FIXED_HEIGHT - h) / 2;

			dst_r.x = x;
			dst_r.y = y;
			dst_r.w = w;
			dst_r.h = h;
			dst_rect = &dst_r;
		}
	}
	SDL_RenderCopy(vid.renderer, vid.texture, src_rect, dst_rect);
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

static int online = 1;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);
}

void PLAT_getBatteryStatusFine(int* is_charging, int* charge)
{
	*is_charging = 1;
	*charge = 100;
}

void PLAT_enableBacklight(int enable) {
	// buh
}

void PLAT_powerOff(void) {
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	// buh
}

void PLAT_setRumble(int strength) {
	// buh
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "macOS";
}

int PLAT_isOnline(void) {
	return online;
}

/////////////////////////////////
// Remove, just for debug


#define MAX_LINE_LENGTH 200
#define ZONE_PATH "/var/db/timezone/zoneinfo"
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
	// call readlink -f /tmp/localtime to get the current timezone path, and
	// then remove /usr/share/zoneinfo/ from the beginning of the path to get the timezone name.
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return NULL;
	}
	if (readlink("/etc/localtime", tz_path, 256) == -1) {
		free(tz_path);
		return NULL;
	}
	tz_path[255] = '\0'; // Ensure null-termination
	char *tz_name = strstr(tz_path, ZONE_PATH "/");
	if (tz_name) {
		tz_name += strlen(ZONE_PATH "/");
		return strdup(tz_name);
	} else {
		return strdup(tz_path);
	}
}

void PLAT_setCurrentTimezone(const char* tz) {
	return;
	if (cached_tz_count == -1)
	{
		LOG_warn("Error: Timezones not initialized. Call PLAT_initTimezones first.\n");
        return;
	}

	// tzset()

	// tz will be in format Asia/Shanghai
	char *tz_path = (char *)malloc(256);
	if (!tz_path) {
		return;
	}
	snprintf(tz_path, 256, ZONE_PATH "/%s", tz);
	if (unlink("/tmp/localtime") == -1) {
		LOG_error("Failed to remove existing symlink: %s\n", strerror(errno));
	}
	if (symlink(tz_path, "/tmp/localtime") == -1) {
		LOG_error("Failed to set timezone: %s\n", strerror(errno));
	}
	free(tz_path);
}