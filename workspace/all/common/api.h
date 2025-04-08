#ifndef __API_H__
#define __API_H__
#include "sdl.h"
#include "platform.h"
#include "scaler.h"
#include "config.h"
#include <stdbool.h>

///////////////////////////////

enum {
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_WARN,
	LOG_ERROR,
};

#define LOG_debug(fmt, ...) LOG_note(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_info(fmt, ...) LOG_note(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_warn(fmt, ...) LOG_note(LOG_WARN, fmt, ##__VA_ARGS__)
#define LOG_error(fmt, ...) LOG_note(LOG_ERROR, fmt, ##__VA_ARGS__)
void LOG_note(int level, const char* fmt, ...);

///////////////////////////////

#define PAGE_COUNT	2
#define PAGE_SCALE	3
#define PAGE_WIDTH	(FIXED_WIDTH * PAGE_SCALE)
#define PAGE_HEIGHT	(FIXED_HEIGHT * PAGE_SCALE)
#define PAGE_PITCH	(PAGE_WIDTH * FIXED_BPP)
#define PAGE_SIZE	(PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////

// TODO: these only seem to be used by a tmp.pak in trimui (model s)
// used by minarch, optionally defined in platform.h
#ifndef PLAT_PAGE_BPP
#define PLAT_PAGE_BPP 	FIXED_BPP
#endif
#define PLAT_PAGE_DEPTH (PLAT_PAGE_BPP * 8)
#define PLAT_PAGE_PITCH (PAGE_WIDTH * PLAT_PAGE_BPP)
#define PLAT_PAGE_SIZE	(PLAT_PAGE_PITCH * PAGE_HEIGHT)

///////////////////////////////

#define RGBA_MASK_AUTO	0x0, 0x0, 0x0, 0x0
#define RGBA_MASK_565	0xF800, 0x07E0, 0x001F, 0x0000
#define RGBA_MASK_8888	0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000

///////////////////////////////

#define FALLBACK_IMPLEMENTATION __attribute__((weak)) // used if platform doesn't provide an implementation

#ifndef SCREEN_FPS
#define SCREEN_FPS 60.0
#endif
///////////////////////////////

extern uint32_t RGB_WHITE;
extern uint32_t RGB_BLACK;
extern uint32_t RGB_LIGHT_GRAY;
extern uint32_t RGB_GRAY;
extern uint32_t RGB_DARK_GRAY;

// screen-mapped to RGB565
extern uint32_t THEME_COLOR1;
extern uint32_t THEME_COLOR2;
extern uint32_t THEME_COLOR3;
extern uint32_t THEME_COLOR4;
extern uint32_t THEME_COLOR5;
extern uint32_t THEME_COLOR6;
extern SDL_Color ALT_BUTTON_TEXT_COLOR;

// TODO: do we need that many free externs? This should move
// to a structure or something.
extern float currentratio;
extern int currentbufferfree;
extern int currentframecount;
extern double currentfps;
extern double currentreqfps;
extern float currentbufferms;
extern int currentbuffersize;
extern int currentsampleratein;
extern int currentsamplerateout;
extern int currentcpuspeed;
extern double currentcpuse;
extern int currentcputemp;
extern int should_rotate;
extern volatile int useAutoCpu;

enum {
	ASSET_WHITE_PILL,
	ASSET_BLACK_PILL,
	ASSET_DARK_GRAY_PILL,
	ASSET_OPTION,
	ASSET_BUTTON,
	ASSET_PAGE_BG,
	ASSET_STATE_BG,
	ASSET_PAGE,
	ASSET_BAR,
	ASSET_BAR_BG,
	ASSET_BAR_BG_MENU,
	ASSET_UNDERLINE,
	ASSET_DOT,
	ASSET_HOLE,
	
	ASSET_COLORS,
	
	ASSET_BRIGHTNESS,
	ASSET_VOLUME_MUTE,
	ASSET_VOLUME,
	ASSET_BATTERY,
	ASSET_BATTERY_LOW,
	ASSET_BATTERY_FILL,
	ASSET_BATTERY_FILL_LOW,
	ASSET_BATTERY_BOLT,
	
	ASSET_SCROLL_UP,
	ASSET_SCROLL_DOWN,
	
	ASSET_WIFI,

	ASSET_GAMEPAD,
	
	ASSET_COUNT,
};

typedef struct GFX_Fonts {
	TTF_Font* large; 	// menu
	TTF_Font* medium; 	// single char button label
	TTF_Font* small; 	// button hint
	TTF_Font* tiny; 	// multi char button label
	TTF_Font* micro; 	// icon overlay text
} GFX_Fonts;
extern GFX_Fonts font;

enum {
	SHARPNESS_SHARP,
	SHARPNESS_CRISP,
	SHARPNESS_SOFT,
};

enum {
	EFFECT_NONE,
	EFFECT_LINE,
	EFFECT_GRID,
	EFFECT_COUNT,
};

typedef struct GFX_Renderer {
	void* src;
	void* dst;
	void* blit;
	double aspect; // 0 for integer, -1 for fullscreen, otherwise aspect ratio, used for SDL2 accelerated scaling
	int scale;
	
	// TODO: document this better
	int true_w;
	int true_h;

	int src_x;
	int src_y;
	int src_w;
	int src_h;
	int src_p;
	
	// TODO: I think this is overscaled
	int dst_x;
	int dst_y;
	int dst_w;
	int dst_h;
	int dst_p;
} GFX_Renderer;

typedef struct
{
    char name[255];
    char filename[255];
    int effect;
    int speed;
    int brightness;
    uint32_t color1;
    uint32_t color2;
    int updated;
    int colorFrames[255];
    int trigger;
    int inbrightness;
    int cycles;

} LightSettings;

extern LightSettings lightsDefault[MAX_LIGHTS];

enum {
	MODE_MAIN,
	MODE_MENU,
};

SDL_Surface* GFX_init(int mode);
#define GFX_resize PLAT_resizeVideo				// (int w, int h, int pitch);
#define GFX_setScaleClip PLAT_setVideoScaleClip // (int x, int y, int width, int height)
#define GFX_setNearestNeighbor PLAT_setNearestNeighbor // (int enabled)
#define GFX_setSharpness PLAT_setSharpness // (int sharpness)
#define GFX_setEffectColor PLAT_setEffectColor // (int color)
#define GFX_setEffect PLAT_setEffect // (int effect)
#define GFX_setOverlay PLAT_setOverlay// (int effect)
#define GFX_setOffsetX PLAT_setOffsetX// (int effect)
#define GFX_setOffsetY PLAT_setOffsetY// (int effect)
void GFX_setMode(int mode);
int GFX_hdmiChanged(void);
SDL_Color /*GFX_*/uintToColour(uint32_t colour);

#define GFX_clear PLAT_clearVideo // (SDL_Surface* screen)
#define GFX_clearAll PLAT_clearAll // (void)

void GFX_startFrame(void);
void audioFPS(void);
void GFX_flip(SDL_Surface* screen);
void GFX_flip_fixed_rate(SDL_Surface* screen, double target_fps); // if target_fps is 0, then use the native screen FPS
#define GFX_supportsOverscan PLAT_supportsOverscan // (void)
void GFX_sync(void); // call this to maintain 60fps when not calling GFX_flip() this frame
void GFX_sync_fixed_rate(double target_fps);
void GFX_delay(void); // gfx_sync() is only for everywhere where there is no audio buffer to rely on for delaying, stupid so doing gfx_delay() for like waiting for input loop in binding menu. Need to remove gfx_sync() everwhere eventually
void GFX_quit(void);

enum {
	VSYNC_OFF = 0,
	VSYNC_LENIENT, // default
	VSYNC_STRICT,
};

int GFX_getVsync(void);
void GFX_setVsync(int vsync);

int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding); // returns final width
int GFX_resetScrollText(TTF_Font* font, const char* in_name,int max_width);
void GFX_scrollTextSurface(TTF_Font* font, const char* in_name, SDL_Surface** out_surface, int max_width, int height, int padding, SDL_Color color,float heightratio); // returns final width
int GFX_getTextWidth(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding); // returns final width
int GFX_getTextHeight(TTF_Font* font, const char* in_name, char* out_name, int max_width, int padding); // returns final width
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines);

#define GFX_getScaler PLAT_getScaler		// scaler_t:(GFX_Renderer* renderer)
#define GFX_blitRenderer PLAT_blitRenderer	// void:(GFX_Renderer* renderer)

scaler_t GFX_getAAScaler(GFX_Renderer* renderer);
void GFX_freeAAScaler(void);

// calls the appropriate scale function based on the enum value.
// returns the SDL_Rect of the resulting image in screen coordinates.
SDL_Rect GFX_blitScaled(int scale, SDL_Surface *src, SDL_Surface *dst);
// blits to the destination and stretches to fit.
SDL_Rect GFX_blitStretch(SDL_Surface *src, SDL_Surface *dst);
// blits to the destination while keeping the aspect ratio.
SDL_Rect GFX_blitScaleAspect(SDL_Surface *src, SDL_Surface *dst);
// same as GFX_blitScaledAspect, but fills both dimensions.
SDL_Rect GFX_blitScaleToFill(SDL_Surface *src, SDL_Surface *dst);

// NOTE: all dimensions should be pre-scaled
void GFX_blitAssetColor(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect, uint32_t asset_color);
void GFX_blitAsset(int asset, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitPillColor(int asset, SDL_Surface* dst, SDL_Rect* dst_rect, uint32_t asset_color, uint32_t fill_color);
void GFX_blitPill(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitPillLight(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitPillDark(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitRect(int asset, SDL_Surface* dst, SDL_Rect* dst_rect);
int GFX_blitBattery(SDL_Surface* dst, SDL_Rect* dst_rect);
int GFX_getButtonWidth(char* hint, char* button);
void GFX_blitButton(char* hint, char*button, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, SDL_Rect* dst_rect);

int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting);
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting);
int GFX_blitButtonGroup(char** hints, int primary, SDL_Surface* dst, int align_right);

void GFX_sizeText(TTF_Font* font, const char* str, int leading, int* w, int* h);
void GFX_blitText(TTF_Font* font, const char* str, int leading, SDL_Color color, SDL_Surface* dst, SDL_Rect* dst_rect);
void GFX_setAmbientColor(const void *data, unsigned width, unsigned height, size_t pitch,int mode);

void GFX_ApplyRoundedCorners(SDL_Surface* surface, SDL_Rect* rect, int radius);
void GFX_ApplyRoundedCorners16(SDL_Surface* surface, SDL_Rect* rect, int radius);
void GFX_ApplyRoundedCorners_RGBA4444(SDL_Surface* surface, SDL_Rect* rect, int radius);
void BlitRGBA4444toRGB565(SDL_Surface* src, SDL_Surface* dest, SDL_Rect* dest_rect);
///////////////////////////////

typedef struct SND_Frame {
	int16_t left;
	int16_t right;
} SND_Frame;

typedef struct {
	SND_Frame* frames;
	int frame_count;
} ResampledFrames;

void SND_init(double sample_rate, double frame_rate);
size_t SND_batchSamples(const SND_Frame* frames, size_t frame_count);
size_t SND_batchSamples_fixed_rate(const SND_Frame* frames, size_t frame_count);
void SND_quit(void);
void SND_resetAudio(double sample_rate, double frame_rate);
void SND_setQuality(int quality);

///////////////////////////////

typedef struct LID_Context {
	int has_lid;
	int is_open;
} LID_Context;
extern LID_Context lid;

void PLAT_initLid(void);
int PLAT_lidChanged(int* state);
void PLAT_getCPUTemp();
///////////////////////////////

typedef struct PAD_Axis {
		int x;
		int y;
} PAD_Axis;
typedef struct PAD_Context {
	int is_pressed;
	int just_pressed;
	int just_released;
	int just_repeated;
	uint32_t repeat_at[BTN_ID_COUNT];
	PAD_Axis laxis;
	PAD_Axis raxis;
} PAD_Context;
extern PAD_Context pad;

#define PAD_REPEAT_DELAY	300
#define PAD_REPEAT_INTERVAL 100

#define PAD_init PLAT_initInput
#define PAD_quit PLAT_quitInput
#define PAD_poll PLAT_pollInput
#define PAD_wake PLAT_shouldWake

void PAD_setAnalog(int neg, int pos, int value, int repeat_at); // internal

void PAD_reset(void);
int PAD_anyJustPressed(void);
int PAD_anyPressed(void);
int PAD_anyJustReleased(void);

int PAD_justPressed(int btn);
int PAD_isPressed(int btn);
int PAD_justReleased(int btn);
int PAD_justRepeated(int btn);

int PAD_tappedMenu(uint32_t now); // special case, returns 1 on release of BTN_MENU within 250ms if BTN_PLUS/BTN_MINUS haven't been pressed

///////////////////////////////
#define VIB_sleepStrength 4
#define VIB_sleepDuration_ms 100
#define VIB_bootStrength 5
#define VIB_bootDuration_ms 100

void VIB_init(void);
void VIB_quit(void);
void VIB_setStrength(int strength);
int VIB_getStrength(void);
int VIB_scaleStrength(int strength);
void VIB_singlePulse(int strength, int duration_ms);
void VIB_doublePulse(int strength, int duration_ms, int gap_ms);
void VIB_triplePulse(int strength, int duration_ms, int gap_ms);

///////////////////////////////

#define BRIGHTNESS_BUTTON_LABEL "+ -" // ew

typedef void (*PWR_callback_t)();
void PWR_init(void);
void PWR_quit(void);
void PWR_warn(int enable);

int PWR_ignoreSettingInput(int btn, int show_setting);
void PWR_update(int* dirty, int* show_setting, PWR_callback_t before_sleep, PWR_callback_t after_sleep);

void PWR_disablePowerOff(void);
void PWR_powerOff(void);
int PWR_isPoweringOff(void);

void PWR_sleep(void);
int PWR_deepSleep(void);

void PWR_disableSleep(void);
void PWR_enableSleep(void);

void PWR_disableAutosleep(void);
void PWR_enableAutosleep(void);
int PWR_preventAutosleep(void);

int PWR_isCharging(void);
int PWR_getBattery(void);

void LEDS_initLeds();
void LEDS_updateLeds();
void LEDS_SaveSettings();
void LEDS_setEffect(int);
void LEDS_setColor(uint32_t color);
void LED_setColor(uint32_t color,int ledindex);
void LEDS_setIndicator(int effect,uint32_t color, int cycles);
void LED_setIndicator(int effect,uint32_t color,int cycles,int ledindex);

enum {
	CPU_SPEED_MENU,
	CPU_SPEED_POWERSAVE,
	CPU_SPEED_NORMAL,
	CPU_SPEED_PERFORMANCE,
};
#define CPU_SWITCH_DELAY_MS 500
#define PWR_setCPUSpeed PLAT_setCPUSpeed

///////////////////////////////

FILE *PLAT_OpenSettings(const char *filename);
FILE *PLAT_WriteSettings(const char *filename);
void PLAT_initInput(void);
void PLAT_quitInput(void);
void PLAT_pollInput(void);
int PLAT_shouldWake(void);

SDL_Surface* PLAT_initVideo(void);
void PLAT_quitVideo(void);
uint32_t PLAT_get_dominant_color(void);
void PLAT_clearVideo(SDL_Surface* screen);
void PLAT_clearAll(void);
void PLAT_setVsync(int vsync);
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch);
void PLAT_setVideoScaleClip(int x, int y, int width, int height);
void PLAT_setNearestNeighbor(int enabled);
void PLAT_setSharpness(int sharpness);
void PLAT_setEffectColor(int color);
void PLAT_setEffect(int effect);
void PLAT_setOverlay(int select, const char* tag);
void PLAT_setOffsetX(int x);
void PLAT_setOffsetY(int y);
void PLAT_vsync(int remaining);
scaler_t PLAT_getScaler(GFX_Renderer* renderer);
void PLAT_blitRenderer(GFX_Renderer* renderer);
void PLAT_flip(SDL_Surface* screen, int sync);
int PLAT_supportsOverscan(void);

SDL_Surface* PLAT_initOverlay(void);
void PLAT_quitOverlay(void);
void PLAT_enableOverlay(int enable);
	
#define PWR_LOW_CHARGE 10
void PLAT_getBatteryStatus(int* is_charging, int* charge); // 0,1 and 0,10,20,40,60,80,100
void PLAT_getBatteryStatusFine(int* is_charging, int* charge); // 0,1 and 0-100
void PLAT_enableBacklight(int enable);
int PLAT_supportsDeepSleep(void);
int PLAT_deepSleep(void);
void PLAT_powerOff(void);

void *PLAT_cpu_monitor(void *arg);
void PLAT_setCPUSpeed(int speed); // enum
void PLAT_setCustomCPUSpeed(int speed);
void PLAT_setRumble(int strength);
int PLAT_pickSampleRate(int requested, int max);

char* PLAT_getModel(void);
int PLAT_isOnline(void);
int PLAT_setDateTime(int y, int m, int d, int h, int i, int s);

void PLAT_initLeds(LightSettings *lights);
void PLAT_setLedEffect(LightSettings *led);
void PLAT_setLedColor(LightSettings *led);
void PLAT_setLedBrightness(LightSettings *led);
void PLAT_setLedInbrightness(LightSettings *led);
void PLAT_setLedEffectSpeed(LightSettings *led);
void PLAT_setLedEffectCycles(LightSettings *led);

///////////////////

#define MAX_TIMEZONES 500
#define MAX_TZ_LENGTH 100

void PLAT_initTimezones();
void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count);
char *PLAT_getCurrentTimezone();
void PLAT_setCurrentTimezone(const char*);
bool PLAT_getNetworkTimeSync(void);
void PLAT_setNetworkTimeSync(bool on);

#define TIME_init PLAT_initTimezones
#define TIME_getTimezones PLAT_getTimezones
#define TIME_getCurrentTimezone PLAT_getCurrentTimezone
#define TIME_setCurrentTimezone PLAT_setCurrentTimezone
#define TIME_getNetworkTimeSync PLAT_getNetworkTimeSync
#define TIME_setNetworkTimeSync PLAT_setNetworkTimeSync

#endif
