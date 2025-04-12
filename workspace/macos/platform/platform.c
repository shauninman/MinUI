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

int  GetMutedBrightness(void) { return 0; }
int  GetMutedColortemp(void) { return 0; }
int  GetMutedContrast(void) { return 0; }
int  GetMutedSaturation(void) { return 0; }
int  GetMutedExposure(void) { return 0; }

void SetMutedBrightness(int value){}
void SetMutedColortemp(int value){}
void SetMutedContrast(int value){}
void SetMutedSaturation(int value){}
void SetMutedExposure(int value){}

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
	SDL_Texture* target_layer1;
	SDL_Texture* target_layer2;
	SDL_Texture* stream_layer1;
	SDL_Texture* target_layer3;
	SDL_Texture* target_layer4;
	SDL_Texture* foreground;
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

	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer2 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer3 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	vid.target_layer4 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET , w,h);
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeNearest);

	vid.screen = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA8888);

	SDL_SetSurfaceBlendMode(vid.screen, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer2, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer3, SDL_BLENDMODE_BLEND);
	SDL_SetTextureBlendMode(vid.target_layer4, SDL_BLENDMODE_BLEND);

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
		SDL_RenderCopy(vid.renderer, vid.stream_layer1, NULL, NULL);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	clearVideo();

	SDL_FreeSurface(vid.screen);

	if (vid.target_layer3) SDL_DestroyTexture(vid.target_layer3);
	if (vid.target_layer1) SDL_DestroyTexture(vid.target_layer1);
	if (vid.target_layer2) SDL_DestroyTexture(vid.target_layer2);
	if (vid.target_layer4) SDL_DestroyTexture(vid.target_layer4);
	SDL_DestroyTexture(vid.stream_layer1);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	SDL_Quit();
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

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	LOG_info("resizeVideo(%i,%i,%i)\n",w,h,p);

	SDL_DestroyTexture(vid.stream_layer1);
	
	vid.stream_layer1 = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_SetTextureBlendMode(vid.stream_layer1, SDL_BLENDMODE_BLEND);
	
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

int screenx = 0;
int screeny = 0;
void PLAT_setOffsetX(int x) {
    if (x < 0 || x > 128) return;
    screenx = x - 64;
}
void PLAT_setOffsetY(int y) {
    if (y < 0 || y > 128) return;
    screeny = y - 64;  
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

/////////////////////

 void PLAT_wifiInit() {}
 bool PLAT_hasWifi() { return true; }
 bool PLAT_wifiEnabled() { return true; }
 void PLAT_wifiEnable(bool on) {}

 int PLAT_wifiScan(struct WIFI_network *networks, int max) {
	for (int i = 0; i < 5; i++) {
		struct WIFI_network *network = &networks[i];

		sprintf(network->ssid, "Network%d", i);
		strcpy(network->bssid, "01:01:01:01:01:01");
		network->rssi = (70 / 5) * (i + 1);
		network->freq = 2400;
		network->security = i % 2 ? SECURITY_WPA2_PSK : SECURITY_WEP;
	}
	return 5;
 }
 bool PLAT_wifiConnected() { return true; }
 int PLAT_wifiConnection(struct WIFI_connection *connection_info) {
	connection_info->freq = 2400;
	strcpy(connection_info->ip, "127.0.0.1");
	strcpy(connection_info->ssid, "Network1");
	return 0;
 }
 bool PLAT_wifiHasCredentials(char *ssid, WifiSecurityType sec) { return false; }
 void PLAT_wifiForget(char *ssid, WifiSecurityType sec) {}
 void PLAT_wifiConnect(char *ssid, WifiSecurityType sec) {}
 void PLAT_wifiConnectPass(const char *ssid, WifiSecurityType sec, const char* pass) {}
 void PLAT_wifiDisconnect() {}