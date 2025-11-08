/**
 * api.c - Platform abstraction layer implementation for MinUI
 *
 * Provides cross-platform API functions for graphics (GFX_*), sound (SND_*),
 * input (PAD_*), power management (PWR_*), and vibration (VIB_*). This file
 * implements the common layer that works on all devices, while platform-specific
 * implementations are provided through PLAT_* functions defined in each platform's
 * directory.
 *
 * Key components:
 * - Graphics: SDL-based rendering, asset management, text rendering, UI helpers
 * - Sound: Audio mixing, resampling, ring buffer management
 * - Input: Button state tracking, repeat handling, analog stick support
 * - Power: Battery monitoring, sleep/wake, brightness/volume control
 * - Vibration: Rumble motor control with deferred state changes
 *
 * Memory Management:
 * - SDL surfaces are reference counted (use SDL_FreeSurface when done)
 * - Audio buffer is dynamically allocated and resized as needed
 * - Asset textures loaded once at init, freed at quit
 * - Font resources managed through TTF_CloseFont
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include <msettings.h>

#include "api.h"
#include "defines.h"
#include "utils.h"

///////////////////////////////
// Logging
///////////////////////////////

/**
 * Logs a message at the specified level to stdout/stderr.
 *
 * Supports DEBUG, INFO, WARN, and ERROR levels. Debug messages
 * are only logged when DEBUG is defined at compile time.
 *
 * @param level Log level (LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 *
 * @note Always flushes stdout to ensure messages appear immediately
 */
void LOG_note(int level, const char* fmt, ...) {
	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	switch (level) {
#ifdef DEBUG
	case LOG_DEBUG:
		printf("[DEBUG] %s", buf);
		break;
#endif
	case LOG_INFO:
		printf("[INFO] %s", buf);
		break;
	case LOG_WARN:
		fprintf(stderr, "[WARN] %s", buf);
		break;
	case LOG_ERROR:
		fprintf(stderr, "[ERROR] %s", buf);
		break;
	default:
		break;
	}
	fflush(stdout);
}

///////////////////////////////
// Graphics - Core initialization and state
///////////////////////////////

// Pre-mapped RGB color values for the current display format
// These are initialized in GFX_init() based on the screen's pixel format
uint32_t RGB_WHITE;
uint32_t RGB_BLACK;
uint32_t RGB_LIGHT_GRAY;
uint32_t RGB_GRAY;
uint32_t RGB_DARK_GRAY;

static struct GFX_Context {
	SDL_Surface* screen;
	SDL_Surface* assets;

	int mode;
	int vsync;
} gfx;

static SDL_Rect asset_rects[ASSET_COUNT];
static uint32_t asset_rgbs[ASSET_COLORS];
GFX_Fonts font;

///////////////////////////////

static struct PWR_Context {
	int initialized;

	int can_sleep;
	int can_poweroff;
	int can_autosleep;
	int requested_sleep;
	int requested_wake;

	pthread_t battery_pt;
	int is_charging;
	int charge;
	int should_warn;

	SDL_Surface* overlay;
} pwr = {0};

///////////////////////////////

// Unused variable for discarding return values
static int _;

/**
 * Initializes the graphics subsystem.
 *
 * Sets up SDL video, loads UI assets, initializes fonts, and prepares
 * the color palette. This must be called before any other GFX_ functions.
 *
 * Asset loading:
 * - Loads platform-specific asset PNG (e.g., assets@2x.png for 2x scale)
 * - Defines rectangles for each asset sprite in the texture atlas
 * - Maps asset IDs to RGB color values for fills
 *
 * Font initialization:
 * - Opens 4 font sizes (large, medium, small, tiny)
 * - Applies bold style to all fonts
 * - Font sizes are scaled based on FIXED_SCALE
 *
 * @param mode Display mode (MODE_MAIN for launcher, MODE_MENU for in-game)
 * @return Pointer to main SDL screen surface
 *
 * @note Also calls PLAT_initLid() to set up lid detection hardware
 */
SDL_Surface* GFX_init(int mode) {
	// TODO: this doesn't really belong here...
	// tried adding to PWR_init() but that was no good (not sure why)
	PLAT_initLid();

	gfx.screen = PLAT_initVideo();
	gfx.vsync = VSYNC_STRICT;
	gfx.mode = mode;

	RGB_WHITE = SDL_MapRGB(gfx.screen->format, TRIAD_WHITE);
	RGB_BLACK = SDL_MapRGB(gfx.screen->format, TRIAD_BLACK);
	RGB_LIGHT_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_LIGHT_GRAY);
	RGB_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_GRAY);
	RGB_DARK_GRAY = SDL_MapRGB(gfx.screen->format, TRIAD_DARK_GRAY);

	asset_rgbs[ASSET_WHITE_PILL] = RGB_WHITE;
	asset_rgbs[ASSET_BLACK_PILL] = RGB_BLACK;
	asset_rgbs[ASSET_DARK_GRAY_PILL] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_OPTION] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_BUTTON] = RGB_WHITE;
	asset_rgbs[ASSET_PAGE_BG] = RGB_WHITE;
	asset_rgbs[ASSET_STATE_BG] = RGB_WHITE;
	asset_rgbs[ASSET_PAGE] = RGB_BLACK;
	asset_rgbs[ASSET_BAR] = RGB_WHITE;
	asset_rgbs[ASSET_BAR_BG] = RGB_BLACK;
	asset_rgbs[ASSET_BAR_BG_MENU] = RGB_DARK_GRAY;
	asset_rgbs[ASSET_UNDERLINE] = RGB_GRAY;
	asset_rgbs[ASSET_DOT] = RGB_LIGHT_GRAY;
	asset_rgbs[ASSET_HOLE] = RGB_BLACK;

	asset_rects[ASSET_WHITE_PILL] = (SDL_Rect){SCALE4(1, 1, 30, 30)};
	asset_rects[ASSET_BLACK_PILL] = (SDL_Rect){SCALE4(33, 1, 30, 30)};
	asset_rects[ASSET_DARK_GRAY_PILL] = (SDL_Rect){SCALE4(65, 1, 30, 30)};
	asset_rects[ASSET_OPTION] = (SDL_Rect){SCALE4(97, 1, 20, 20)};
	asset_rects[ASSET_BUTTON] = (SDL_Rect){SCALE4(1, 33, 20, 20)};
	asset_rects[ASSET_PAGE_BG] = (SDL_Rect){SCALE4(64, 33, 15, 15)};
	asset_rects[ASSET_STATE_BG] = (SDL_Rect){SCALE4(23, 54, 8, 8)};
	asset_rects[ASSET_PAGE] = (SDL_Rect){SCALE4(39, 54, 6, 6)};
	asset_rects[ASSET_BAR] = (SDL_Rect){SCALE4(33, 58, 4, 4)};
	asset_rects[ASSET_BAR_BG] = (SDL_Rect){SCALE4(15, 55, 4, 4)};
	asset_rects[ASSET_BAR_BG_MENU] = (SDL_Rect){SCALE4(85, 56, 4, 4)};
	asset_rects[ASSET_UNDERLINE] = (SDL_Rect){SCALE4(85, 51, 3, 3)};
	asset_rects[ASSET_DOT] = (SDL_Rect){SCALE4(33, 54, 2, 2)};
	asset_rects[ASSET_BRIGHTNESS] = (SDL_Rect){SCALE4(23, 33, 19, 19)};
	asset_rects[ASSET_VOLUME_MUTE] = (SDL_Rect){SCALE4(44, 33, 10, 16)};
	asset_rects[ASSET_VOLUME] = (SDL_Rect){SCALE4(44, 33, 18, 16)};
	asset_rects[ASSET_BATTERY] = (SDL_Rect){SCALE4(47, 51, 17, 10)};
	asset_rects[ASSET_BATTERY_LOW] = (SDL_Rect){SCALE4(66, 51, 17, 10)};
	asset_rects[ASSET_BATTERY_FILL] = (SDL_Rect){SCALE4(81, 33, 12, 6)};
	asset_rects[ASSET_BATTERY_FILL_LOW] = (SDL_Rect){SCALE4(1, 55, 12, 6)};
	asset_rects[ASSET_BATTERY_BOLT] = (SDL_Rect){SCALE4(81, 41, 12, 6)};
	asset_rects[ASSET_SCROLL_UP] = (SDL_Rect){SCALE4(97, 23, 24, 6)};
	asset_rects[ASSET_SCROLL_DOWN] = (SDL_Rect){SCALE4(97, 31, 24, 6)};
	asset_rects[ASSET_WIFI] = (SDL_Rect){SCALE4(95, 39, 14, 10)};
	asset_rects[ASSET_HOLE] = (SDL_Rect){SCALE4(1, 63, 20, 20)};

	char asset_path[MAX_PATH];
	sprintf(asset_path, RES_PATH "/assets@%ix.png", FIXED_SCALE);
	if (!exists(asset_path))
		LOG_info("missing assets, you're about to segfault dummy!\n");
	gfx.assets = IMG_Load(asset_path);

	TTF_Init();
	font.large = TTF_OpenFont(FONT_PATH, SCALE1(FONT_LARGE));
	font.medium = TTF_OpenFont(FONT_PATH, SCALE1(FONT_MEDIUM));
	font.small = TTF_OpenFont(FONT_PATH, SCALE1(FONT_SMALL));
	font.tiny = TTF_OpenFont(FONT_PATH, SCALE1(FONT_TINY));

	TTF_SetFontStyle(font.large, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.medium, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.small, TTF_STYLE_BOLD);
	TTF_SetFontStyle(font.tiny, TTF_STYLE_BOLD);

	return gfx.screen;
}

/**
 * Shuts down the graphics subsystem and frees all resources.
 *
 * Closes all fonts, frees the asset texture, clears video memory,
 * and calls platform-specific cleanup.
 *
 * @note Should be called before program exit to prevent resource leaks
 */
void GFX_quit(void) {
	TTF_CloseFont(font.large);
	TTF_CloseFont(font.medium);
	TTF_CloseFont(font.small);
	TTF_CloseFont(font.tiny);

	SDL_FreeSurface(gfx.assets);

	GFX_freeAAScaler();

	GFX_clearAll();

	PLAT_quitVideo();
}

/**
 * Sets the display mode for UI rendering.
 *
 * @param mode MODE_MAIN (launcher) or MODE_MENU (in-game menu)
 *
 * @note Affects UI styling - main mode uses darker backgrounds
 */
void GFX_setMode(int mode) {
	gfx.mode = mode;
}

/**
 * Gets the current vsync setting.
 *
 * @return VSYNC_OFF, VSYNC_LENIENT, or VSYNC_STRICT
 */
int GFX_getVsync(void) {
	return gfx.vsync;
}

/**
 * Sets the vsync behavior for frame synchronization.
 *
 * Vsync modes:
 * - VSYNC_OFF: No frame limiting (uses SDL_Delay fallback)
 * - VSYNC_LENIENT: Skip vsync if frame took too long (default)
 * - VSYNC_STRICT: Always vsync, even if it causes slowdown
 *
 * @param vsync Vsync mode (VSYNC_OFF, VSYNC_LENIENT, VSYNC_STRICT)
 */
void GFX_setVsync(int vsync) {
	PLAT_setVsync(vsync);
	gfx.vsync = vsync;
}

/**
 * Detects if HDMI connection state has changed.
 *
 * Tracks whether HDMI was connected/disconnected since last check.
 * Used to trigger display reconfiguration when needed.
 *
 * @return 1 if HDMI state changed, 0 otherwise
 */
int GFX_hdmiChanged(void) {
	static int had_hdmi = -1;
	int has_hdmi = GetHDMI();
	if (had_hdmi == -1)
		had_hdmi = has_hdmi;
	if (had_hdmi == has_hdmi)
		return 0;
	had_hdmi = has_hdmi;
	return 1;
}

// Target frame time in milliseconds (60fps)
#define FRAME_BUDGET 17
static uint32_t frame_start = 0;

/**
 * Marks the beginning of a new frame for timing purposes.
 *
 * Records the current timestamp to calculate frame duration.
 * Used in conjunction with GFX_sync() to maintain consistent frame rate.
 *
 * @note Call this at the start of your render loop
 */
void GFX_startFrame(void) {
	frame_start = SDL_GetTicks();
}

/**
 * Presents the rendered frame to the display.
 *
 * Decides whether to use vsync based on the current vsync mode
 * and frame timing. With VSYNC_LENIENT, skips vsync if the frame
 * took longer than FRAME_BUDGET to avoid slowdown.
 *
 * @param screen SDL surface to flip to the display
 *
 * @note Call GFX_startFrame() before rendering for proper timing
 */
void GFX_flip(SDL_Surface* screen) {
	int should_vsync = (gfx.vsync != VSYNC_OFF && (gfx.vsync == VSYNC_STRICT || frame_start == 0 ||
	                                               SDL_GetTicks() - frame_start < FRAME_BUDGET));
	PLAT_flip(screen, should_vsync);
}

/**
 * Synchronizes to maintain 60fps when not flipping this frame.
 *
 * Call this if you skip rendering a frame but still want to maintain
 * consistent timing. Waits for the remainder of the frame budget using
 * vsync or SDL_Delay depending on settings.
 *
 * This helps SuperFX games run smoother by maintaining frame timing
 * even when frames are dropped.
 */
void GFX_sync(void) {
	uint32_t frame_duration = SDL_GetTicks() - frame_start;
	if (gfx.vsync != VSYNC_OFF) {
		// this limiting condition helps SuperFX chip games
		if (gfx.vsync == VSYNC_STRICT || frame_start == 0 ||
		    frame_duration < FRAME_BUDGET) { // only wait if we're under frame budget
			PLAT_vsync(FRAME_BUDGET - frame_duration);
		}
	} else {
		if (frame_duration < FRAME_BUDGET)
			SDL_Delay(FRAME_BUDGET - frame_duration);
	}
}

/**
 * Checks if the platform supports overscan adjustment.
 *
 * Default implementation returns 0 (no overscan support).
 * Platforms can override this weak symbol to enable overscan.
 *
 * @return 1 if overscan is supported, 0 otherwise
 */
FALLBACK_IMPLEMENTATION int PLAT_supportsOverscan(void) {
	return 0;
}

/**
 * Sets the color for screen effects (scanlines, grids).
 *
 * Default implementation does nothing. Platforms with effect
 * support override this weak symbol.
 *
 * @param next_color Color index for the effect
 */
FALLBACK_IMPLEMENTATION void PLAT_setEffectColor(int next_color) {}

///////////////////////////////
// Graphics - Text rendering and formatting
///////////////////////////////

/**
 * Truncates text to fit within a maximum width, adding ellipsis if needed.
 *
 * Progressively removes characters from the end and adds "..." until
 * the text fits. Modifies out_name in place.
 *
 * @param font TTF font to measure text with
 * @param in_name Input text string
 * @param out_name Output buffer for truncated text
 * @param max_width Maximum width in pixels
 * @param padding Additional padding to account for in width
 * @return Final width of the text in pixels (including padding)
 */
int GFX_truncateText(TTF_Font* font, const char* in_name, char* out_name, int max_width,
                     int padding) {
	int text_width;
	strcpy(out_name, in_name);
	TTF_SizeUTF8(font, out_name, &text_width, NULL);
	text_width += padding;

	while (text_width > max_width) {
		int len = strlen(out_name);
		strcpy(&out_name[len - 4], "...\0");
		TTF_SizeUTF8(font, out_name, &text_width, NULL);
		text_width += padding;
	}

	return text_width;
}

/**
 * Wraps text to fit within a maximum width by inserting newlines.
 *
 * Breaks text at space characters to create wrapped lines. The last
 * line is truncated with "..." if it still exceeds max_width.
 * Modifies the input string in place by replacing spaces with newlines.
 *
 * @param font TTF font to measure text with
 * @param str String to wrap (modified in place)
 * @param max_width Maximum width per line in pixels
 * @param max_lines Maximum number of lines (0 for unlimited)
 * @return Width of the widest line in pixels
 *
 * @note Input string is modified - spaces become newlines at wrap points
 */
int GFX_wrapText(TTF_Font* font, char* str, int max_width, int max_lines) {
	if (!str)
		return 0;

	int line_width;
	int max_line_width = 0;
	char* line = str;
	char buffer[MAX_PATH];

	TTF_SizeUTF8(font, line, &line_width, NULL);
	if (line_width <= max_width) {
		line_width = GFX_truncateText(font, line, buffer, max_width, 0);
		strcpy(line, buffer);
		return line_width;
	}

	char* prev = NULL;
	char* tmp = line;
	int lines = 1;
	while (!max_lines || lines < max_lines) {
		tmp = strchr(tmp, ' ');
		if (!tmp) {
			if (prev) {
				TTF_SizeUTF8(font, line, &line_width, NULL);
				if (line_width >= max_width) {
					if (line_width > max_line_width)
						max_line_width = line_width;
					prev[0] = '\n';
					line = prev + 1;
				}
			}
			break;
		}
		tmp[0] = '\0';

		TTF_SizeUTF8(font, line, &line_width, NULL);

		if (line_width >= max_width) { // wrap
			if (line_width > max_line_width)
				max_line_width = line_width;
			tmp[0] = ' ';
			tmp += 1;
			prev[0] = '\n';
			prev += 1;
			line = prev;
			lines += 1;
		} else { // continue
			tmp[0] = ' ';
			prev = tmp;
			tmp += 1;
		}
	}

	line_width = GFX_truncateText(font, line, buffer, max_width, 0);
	strcpy(line, buffer);

	if (line_width > max_line_width)
		max_line_width = line_width;
	return max_line_width;
}

///////////////////////////////
// Graphics - Anti-aliased scaling (from picoarch)
///////////////////////////////

// Blend arguments structure for anti-aliased scaling
// Stores the ratio and blend point calculations for both dimensions
struct blend_args {
	int w_ratio_in;
	int w_ratio_out;
	uint16_t w_bp[2];
	int h_ratio_in;
	int h_ratio_out;
	uint16_t h_bp[2];
	uint16_t* blend_line; // Temporary buffer for blended scanlines
} blend_args;

// Color averaging macros using math_utils functions from utils.c
// Skip averaging if colors are identical for optimization
#define AVERAGE16_NOCHK(c1, c2) (average16((c1), (c2)))
#define AVERAGE32_NOCHK(c1, c2) (average32((c1), (c2)))

#define AVERAGE16(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE16_NOCHK((c1), (c2)))
// 1:3 weighted average (closer to c2)
#define AVERAGE16_1_3(c1, c2)                                                                      \
	((c1) == (c2) ? (c1) : (AVERAGE16_NOCHK(AVERAGE16_NOCHK((c1), (c2)), (c2))))

#define AVERAGE32(c1, c2) ((c1) == (c2) ? (c1) : AVERAGE32_NOCHK((c1), (c2)))
// 1:3 weighted average for paired RGB565 pixels
#define AVERAGE32_1_3(c1, c2)                                                                      \
	((c1) == (c2) ? (c1) : (AVERAGE32_NOCHK(AVERAGE32_NOCHK((c1), (c2)), (c2))))

/**
 * Anti-aliased scaler implementation using bilinear interpolation.
 *
 * Scales RGB565 source image to destination with anti-aliasing for smoother
 * results than nearest-neighbor. Uses quintic blending zones for smooth
 * transitions between pixels.
 *
 * Algorithm divides each source pixel into 5 zones when scaling:
 * - Zone 1 (outer): 100% source pixel A
 * - Zone 2: 75% A, 25% B (1:3 blend)
 * - Zone 3 (center): 50% A, 50% B (even blend)
 * - Zone 4: 25% A, 75% B (1:3 blend)
 * - Zone 5 (outer): 100% source pixel B
 *
 * @param src Source image data (RGB565 format)
 * @param dst Destination image buffer
 * @param w Source width in pixels
 * @param h Source height in pixels
 * @param pitch Source pitch in bytes
 * @param dst_w Destination width in pixels
 * @param dst_h Destination height in pixels
 * @param dst_p Destination pitch in bytes
 *
 * @note Requires blend_args to be initialized via GFX_getAAScaler first
 */
static void scaleAA(void* __restrict src, void* __restrict dst, uint32_t w, uint32_t h,
                    uint32_t pitch, uint32_t dst_w, uint32_t dst_h, uint32_t dst_p) {
	int dy = 0;
	int lines = h;

	int rat_w = blend_args.w_ratio_in;
	int rat_dst_w = blend_args.w_ratio_out;
	const uint16_t* bw = blend_args.w_bp;

	int rat_h = blend_args.h_ratio_in;
	int rat_dst_h = blend_args.h_ratio_out;
	const uint16_t* bh = blend_args.h_bp;

	while (lines--) {
		while (dy < rat_dst_h) {
			uint16_t* dst16 = (uint16_t*)dst;
			uint16_t* pblend = (uint16_t*)blend_args.blend_line;
			int col = w;
			int dx = 0;

			uint16_t* pnext = (uint16_t*)(src + pitch);
			if (!lines)
				pnext -= (pitch / sizeof(uint16_t));

			if (dy > rat_dst_h - bh[0]) {
				pblend = pnext;
			} else if (dy <= bh[0]) {
				/* Drops const, won't get touched later though */
				pblend = (uint16_t*)src;
			} else {
				const uint32_t* src32 = (const uint32_t*)src;
				const uint32_t* pnext32 = (const uint32_t*)pnext;
				uint32_t* pblend32 = (uint32_t*)pblend;
				int count = w / 2;

				if (dy <= bh[1]) {
					const uint32_t* tmp = pnext32;
					pnext32 = src32;
					src32 = tmp;
				}

				if (dy > rat_dst_h - bh[1] || dy <= bh[1]) {
					while (count--) {
						*pblend32++ = AVERAGE32_1_3(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				} else {
					while (count--) {
						*pblend32++ = AVERAGE32(*src32, *pnext32);
						src32++;
						pnext32++;
					}
				}
			}

			while (col--) {
				uint16_t a, b, out;

				a = *pblend;
				b = *(pblend + 1);

				while (dx < rat_dst_w) {
					if (a == b) {
						out = a;
					} else if (dx > rat_dst_w - bw[0]) { // top quintile, bbbb
						out = b;
					} else if (dx <= bw[0]) { // last quintile, aaaa
						out = a;
					} else {
						if (dx > rat_dst_w - bw[1]) { // 2nd quintile, abbb
							a = AVERAGE16_NOCHK(a, b);
						} else if (dx <= bw[1]) { // 4th quintile, aaab
							b = AVERAGE16_NOCHK(a, b);
						}

						out = AVERAGE16_NOCHK(a, b); // also 3rd quintile, aabb
					}
					*dst16++ = out;
					dx += rat_w;
				}

				dx -= rat_dst_w;
				pblend++;
			}

			dy += rat_h;
			dst += dst_p;
		}

		dy -= rat_dst_h;
		src += pitch;
	}
}

/**
 * Initializes the anti-aliased scaler for a given renderer configuration.
 *
 * Calculates blend ratios and breakpoints based on the GCD of source
 * and destination dimensions. Allocates a temporary scanline buffer
 * for blending operations.
 *
 * The blend_denominator controls blend zone widths:
 * - 5.0 for downscaling (sharper)
 * - 2.5 for upscaling (smoother)
 *
 * @param renderer Renderer configuration with source/dest dimensions
 * @return Function pointer to scaleAA scaler implementation
 *
 * @note Allocates blend_line buffer - call GFX_freeAAScaler to free
 */
scaler_t GFX_getAAScaler(const GFX_Renderer* renderer) {
	int gcd_w, div_w, gcd_h, div_h;
	blend_args.blend_line = calloc(renderer->src_w, sizeof(uint16_t));

	gcd_w = gcd(renderer->src_w, renderer->dst_w);
	blend_args.w_ratio_in = renderer->src_w / gcd_w;
	blend_args.w_ratio_out = renderer->dst_w / gcd_w;

	double blend_denominator = (renderer->src_w > renderer->dst_w)
	                               ? 5
	                               : 2.5; // TODO: these values are really only good for the nano...
	// blend_denominator = 5.0; // better for trimui
	// LOG_info("blend_denominator: %f (%i && %i)\n", blend_denominator, HAS_SKINNY_SCREEN, renderer->dst_w>renderer->src_w);

	div_w = round(blend_args.w_ratio_out / blend_denominator);
	blend_args.w_bp[0] = div_w;
	blend_args.w_bp[1] = blend_args.w_ratio_out >> 1;

	gcd_h = gcd(renderer->src_h, renderer->dst_h);
	blend_args.h_ratio_in = renderer->src_h / gcd_h;
	blend_args.h_ratio_out = renderer->dst_h / gcd_h;

	div_h = round(blend_args.h_ratio_out / blend_denominator);
	blend_args.h_bp[0] = div_h;
	blend_args.h_bp[1] = blend_args.h_ratio_out >> 1;

	return scaleAA;
}

/**
 * Frees resources allocated by the anti-aliased scaler.
 *
 * Deallocates the temporary scanline buffer used for blending.
 * Safe to call even if scaler was never initialized.
 */
void GFX_freeAAScaler(void) {
	if (blend_args.blend_line != NULL) {
		free(blend_args.blend_line);
		blend_args.blend_line = NULL;
	}
}

///////////////////////////////
// Graphics - Asset and UI element rendering
///////////////////////////////

/**
 * Blits a UI asset from the asset texture to a destination surface.
 *
 * Assets are pre-defined rectangular regions in the asset PNG file.
 * The src_rect parameter allows blitting only part of an asset.
 *
 * @param asset Asset ID (e.g., ASSET_BUTTON, ASSET_BATTERY)
 * @param src_rect Optional source rectangle within the asset (NULL for full asset)
 * @param dst Destination surface
 * @param dst_rect Destination rectangle (NULL to blit at 0,0)
 */
void GFX_blitAsset(int asset, const SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect) {
	const SDL_Rect* rect = &asset_rects[asset];
	SDL_Rect adj_rect = {
	    .x = rect->x,
	    .y = rect->y,
	    .w = rect->w,
	    .h = rect->h,
	};
	if (src_rect) {
		adj_rect.x += src_rect->x;
		adj_rect.y += src_rect->y;
		adj_rect.w = src_rect->w;
		adj_rect.h = src_rect->h;
	}
	SDL_BlitSurface(gfx.assets, &adj_rect, dst, dst_rect);
}

/**
 * Renders a rounded pill-shaped UI element.
 *
 * Pills are composed of rounded ends from the asset texture with a
 * stretched middle section. Used for buttons, progress bars, and
 * status indicators.
 *
 * @param asset Asset to use for pill caps (determines color/style)
 * @param dst Destination surface
 * @param dst_rect Desired pill dimensions and position
 *
 * @note If height is 0, uses asset's default height
 */
void GFX_blitPill(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;

	if (h == 0)
		h = asset_rects[asset].h;

	int r = h / 2;
	if (w < h)
		w = h;
	w -= h;

	GFX_blitAsset(asset, &(SDL_Rect){0, 0, r, h}, dst, &(SDL_Rect){x, y});
	x += r;
	if (w > 0) {
		SDL_FillRect(dst, &(SDL_Rect){x, y, w, h}, asset_rgbs[asset]);
		x += w;
	}
	GFX_blitAsset(asset, &(SDL_Rect){r, 0, r, h}, dst, &(SDL_Rect){x, y});
}

/**
 * Renders a rounded rectangle UI element with stretched corners.
 *
 * Similar to pills but for rectangular regions. Blits the four
 * corners from the asset and fills edges and center with solid color.
 *
 * @param asset Asset to use for corner sprites and fill color
 * @param dst Destination surface
 * @param dst_rect Rectangle dimensions and position
 */
void GFX_blitRect(int asset, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	int x = dst_rect->x;
	int y = dst_rect->y;
	int w = dst_rect->w;
	int h = dst_rect->h;
	int c = asset_rgbs[asset];

	const SDL_Rect* rect = &asset_rects[asset];
	int d = rect->w;
	int r = d / 2;

	GFX_blitAsset(asset, &(SDL_Rect){0, 0, r, r}, dst, &(SDL_Rect){x, y});
	SDL_FillRect(dst, &(SDL_Rect){x + r, y, w - d, r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r, 0, r, r}, dst, &(SDL_Rect){x + w - r, y});
	SDL_FillRect(dst, &(SDL_Rect){x, y + r, w, h - d}, c);
	GFX_blitAsset(asset, &(SDL_Rect){0, r, r, r}, dst, &(SDL_Rect){x, y + h - r});
	SDL_FillRect(dst, &(SDL_Rect){x + r, y + h - r, w - d, r}, c);
	GFX_blitAsset(asset, &(SDL_Rect){r, r, r, r}, dst, &(SDL_Rect){x + w - r, y + h - r});
}

/**
 * Renders the battery status indicator.
 *
 * Displays either:
 * - Battery icon with charge level fill (when not charging)
 * - Battery icon with charging bolt (when charging)
 *
 * Battery color changes to red when charge is <= 10%.
 * The fill bar shows percentage visually.
 *
 * @param dst Destination surface
 * @param dst_rect Position for battery indicator
 *
 * @note Uses global pwr.is_charging and pwr.charge values
 */
void GFX_blitBattery(SDL_Surface* dst, const SDL_Rect* dst_rect) {
	// LOG_info("dst: %p\n", dst);
	int x = 0;
	int y = 0;
	if (dst_rect) {
		x = dst_rect->x;
		y = dst_rect->y;
	}
	SDL_Rect rect = asset_rects[ASSET_BATTERY];
	x += (SCALE1(PILL_SIZE) - (rect.w + FIXED_SCALE)) / 2;
	y += (SCALE1(PILL_SIZE) - rect.h) / 2;

	if (pwr.is_charging) {
		GFX_blitAsset(ASSET_BATTERY, NULL, dst, &(SDL_Rect){x, y});
		GFX_blitAsset(ASSET_BATTERY_BOLT, NULL, dst, &(SDL_Rect){x + SCALE1(3), y + SCALE1(2)});
	} else {
		int percent = pwr.charge;
		GFX_blitAsset(percent <= 10 ? ASSET_BATTERY_LOW : ASSET_BATTERY, NULL, dst,
		              &(SDL_Rect){x, y});

		rect = asset_rects[ASSET_BATTERY_FILL];
		SDL_Rect clip = rect;
		clip.w *= percent;
		clip.w /= 100;
		if (clip.w <= 0)
			return;
		clip.x = rect.w - clip.w;
		clip.y = 0;

		GFX_blitAsset(percent <= 20 ? ASSET_BATTERY_FILL_LOW : ASSET_BATTERY_FILL, &clip, dst,
		              &(SDL_Rect){x + SCALE1(3) + clip.x, y + SCALE1(2)});
	}
}

/**
 * Calculates the total width needed for a button with hint text.
 *
 * Buttons consist of:
 * - Icon/button label (circular or pill-shaped)
 * - Spacing
 * - Hint text
 *
 * @param hint Hint text to display next to button
 * @param button Button label (e.g., "A", "B", "START")
 * @return Total width in pixels needed for button and hint
 *
 * @note Special handling for BRIGHTNESS_BUTTON_LABEL ("+ -")
 */
int GFX_getButtonWidth(char* hint, char* button) {
	int button_width = 0;
	int width;

	int special_case = !strcmp(button, BRIGHTNESS_BUTTON_LABEL); // TODO: oof

	if (strlen(button) == 1) {
		button_width += SCALE1(BUTTON_SIZE);
	} else {
		button_width += SCALE1(BUTTON_SIZE) / 2;
		TTF_SizeUTF8(special_case ? font.large : font.tiny, button, &width, NULL);
		button_width += width;
	}
	button_width += SCALE1(BUTTON_MARGIN);

	TTF_SizeUTF8(font.small, hint, &width, NULL);
	button_width += width + SCALE1(BUTTON_MARGIN);
	return button_width;
}

/**
 * Renders a button with its label and hint text.
 *
 * Single-character labels (A, B, X, Y) are rendered in circular buttons.
 * Multi-character labels (START, SELECT) are rendered in pill-shaped buttons.
 *
 * @param hint Hint text displayed next to button (e.g., "CONFIRM")
 * @param button Button label (e.g., "A", "START")
 * @param dst Destination surface
 * @param dst_rect Position to render button at
 *
 * @note Hint text appears to the right of the button icon
 */
void GFX_blitButton(char* hint, char* button, SDL_Surface* dst, SDL_Rect* dst_rect) {
	SDL_Surface* text;
	int ox = 0;

	int special_case = !strcmp(button, BRIGHTNESS_BUTTON_LABEL); // TODO: oof

	// button
	if (strlen(button) == 1) {
		GFX_blitAsset(ASSET_BUTTON, NULL, dst, dst_rect);

		// label
		text = TTF_RenderUTF8_Blended(font.medium, button, COLOR_BUTTON_TEXT);
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){dst_rect->x + (SCALE1(BUTTON_SIZE) - text->w) / 2,
		                            dst_rect->y + (SCALE1(BUTTON_SIZE) - text->h) / 2});
		ox += SCALE1(BUTTON_SIZE);
		SDL_FreeSurface(text);
	} else {
		text = TTF_RenderUTF8_Blended(special_case ? font.large : font.tiny, button,
		                              COLOR_BUTTON_TEXT);
		GFX_blitPill(ASSET_BUTTON, dst,
		             &(SDL_Rect){dst_rect->x, dst_rect->y, SCALE1(BUTTON_SIZE) / 2 + text->w,
		                         SCALE1(BUTTON_SIZE)});
		ox += SCALE1(BUTTON_SIZE) / 4;

		int oy = special_case ? SCALE1(-2) : 0;
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){ox + dst_rect->x,
		                            oy + dst_rect->y + (SCALE1(BUTTON_SIZE) - text->h) / 2, text->w,
		                            text->h});
		ox += text->w;
		ox += SCALE1(BUTTON_SIZE) / 4;
		SDL_FreeSurface(text);
	}

	ox += SCALE1(BUTTON_MARGIN);

	// hint text
	text = TTF_RenderUTF8_Blended(font.small, hint, COLOR_WHITE);
	SDL_BlitSurface(text, NULL, dst,
	                &(SDL_Rect){ox + dst_rect->x, dst_rect->y + (SCALE1(BUTTON_SIZE) - text->h) / 2,
	                            text->w, text->h});
	SDL_FreeSurface(text);
}

/**
 * Renders a multi-line text message centered in a rectangular area.
 *
 * Splits the message into lines and renders them vertically centered
 * within the destination rectangle. Each line is horizontally centered.
 *
 * @param font TTF font to render with
 * @param msg Message text (can contain newlines)
 * @param dst Destination surface
 * @param dst_rect Area to center message in (NULL for full surface)
 *
 * @note Maximum 16 lines supported, line height is fixed at 24 (scaled)
 */
void GFX_blitMessage(TTF_Font* font, char* msg, SDL_Surface* dst, const SDL_Rect* dst_rect) {
	if (!dst_rect)
		dst_rect = &(SDL_Rect){0, 0, dst->w, dst->h};

	// LOG_info("GFX_blitMessage: %p (%ix%i)", dst, dst_rect->w,dst_rect->h);

	SDL_Surface* text;
#define TEXT_BOX_MAX_ROWS 16
#define LINE_HEIGHT 24
	char* rows[TEXT_BOX_MAX_ROWS];
	int row_count = splitTextLines(msg, rows, TEXT_BOX_MAX_ROWS);
	if (row_count == 0)
		return;

	int rendered_height = SCALE1(LINE_HEIGHT) * row_count;
	int y = dst_rect->y;
	y += (dst_rect->h - rendered_height) / 2;

	char line[256];
	for (int i = 0; i < row_count; i++) {
		int len;
		if (i + 1 < row_count) {
			len = rows[i + 1] - rows[i] - 1;
			if (len)
				strncpy(line, rows[i], len);
			line[len] = '\0';
		} else {
			len = strlen(rows[i]);
			strcpy(line, rows[i]);
		}


		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, COLOR_WHITE);
			int x = dst_rect->x;
			x += (dst_rect->w - text->w) / 2;
			SDL_BlitSurface(text, NULL, dst, &(SDL_Rect){x, y});
			SDL_FreeSurface(text);
		}
		y += SCALE1(LINE_HEIGHT);
	}
}

/**
 * Renders the hardware status group (battery, wifi, brightness/volume).
 *
 * Displays in top-right corner of screen. Shows either:
 * - Setting adjustment UI (brightness/volume slider with icon)
 * - Status icons (battery, optional wifi)
 *
 * @param dst Destination surface
 * @param show_setting 0=status, 1=brightness, 2=volume
 * @return Width of the rendered group in pixels
 *
 * @note Does not display on HDMI output (except status icons)
 */
int GFX_blitHardwareGroup(SDL_Surface* dst, int show_setting) {
	int ox;
	int oy;
	int ow = 0;

	if (show_setting && !GetHDMI()) {
		ow = SCALE1(PILL_SIZE + SETTINGS_WIDTH + 10 + 4);
		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
		             &(SDL_Rect){ox, oy, ow, SCALE1(PILL_SIZE)});

		int setting_value;
		int setting_min;
		int setting_max;
		if (show_setting == 1) {
			setting_value = GetBrightness();
			setting_min = BRIGHTNESS_MIN;
			setting_max = BRIGHTNESS_MAX;
		} else {
			setting_value = GetVolume();
			setting_min = VOLUME_MIN;
			setting_max = VOLUME_MAX;
		}

		int asset = show_setting == 1 ? ASSET_BRIGHTNESS
		                              : (setting_value > 0 ? ASSET_VOLUME : ASSET_VOLUME_MUTE);
		int ax = ox + (show_setting == 1 ? SCALE1(6) : SCALE1(8));
		int ay = oy + (show_setting == 1 ? SCALE1(5) : SCALE1(7));
		GFX_blitAsset(asset, NULL, dst, &(SDL_Rect){ax, ay});

		ox += SCALE1(PILL_SIZE);
		oy += SCALE1((PILL_SIZE - SETTINGS_SIZE) / 2);
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_BAR_BG : ASSET_BAR_BG_MENU, dst,
		             &(SDL_Rect){ox, oy, SCALE1(SETTINGS_WIDTH), SCALE1(SETTINGS_SIZE)});

		float percent = ((float)(setting_value - setting_min) / (setting_max - setting_min));
		if (show_setting == 1 || setting_value > 0) {
			GFX_blitPill(
			    ASSET_BAR, dst,
			    &(SDL_Rect){ox, oy, SCALE1(SETTINGS_WIDTH) * percent, SCALE1(SETTINGS_SIZE)});
		}
	} else {
		// TODO: handle wifi
		int show_wifi = PLAT_isOnline(); // NOOOOO! not every frame!

		int ww = SCALE1(PILL_SIZE - 3);
		ow = SCALE1(PILL_SIZE);
		if (show_wifi)
			ow += ww;

		ox = dst->w - SCALE1(PADDING) - ow;
		oy = SCALE1(PADDING);
		GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
		             &(SDL_Rect){ox, oy, ow, SCALE1(PILL_SIZE)});
		if (show_wifi) {
			SDL_Rect rect = asset_rects[ASSET_WIFI];
			int x = ox;
			int y = oy;
			x += (SCALE1(PILL_SIZE) - rect.w) / 2;
			y += (SCALE1(PILL_SIZE) - rect.h) / 2;

			GFX_blitAsset(ASSET_WIFI, NULL, dst, &(SDL_Rect){x, y});
			ox += ww;
		}
		GFX_blitBattery(dst, &(SDL_Rect){ox, oy});
	}

	return ow;
}

/**
 * Renders hardware control button hints at bottom of screen.
 *
 * Shows appropriate button combinations for brightness/volume control
 * based on the current platform's button mapping.
 *
 * @param dst Destination surface
 * @param show_setting Which setting hint to show (1=brightness, 2=volume)
 */
void GFX_blitHardwareHints(SDL_Surface* dst, int show_setting) {
	if (BTN_MOD_VOLUME == BTN_SELECT && BTN_MOD_BRIGHTNESS == BTN_START) {
		if (show_setting == 1)
			GFX_blitButtonGroup((char*[]){"SELECT", "VOLUME", NULL}, 0, dst, 0);
		else
			GFX_blitButtonGroup((char*[]){"START", "BRIGHTNESS", NULL}, 0, dst, 0);
	} else {
		if (show_setting == 1)
			GFX_blitButtonGroup((char*[]){BRIGHTNESS_BUTTON_LABEL, "BRIGHTNESS", NULL}, 0, dst, 0);
		else
			GFX_blitButtonGroup((char*[]){"MENU", "BRIGHTNESS", NULL}, 0, dst, 0);
	}
}

/**
 * Renders a group of buttons with hints in a single pill container.
 *
 * Displays up to 2 button-hint pairs in a styled pill background.
 * On narrow screens, only shows the primary button to save space.
 *
 * @param pairs Array of [button, hint, button, hint, NULL] strings
 * @param primary Which button index is primary (0 or 1)
 * @param dst Destination surface
 * @param align_right 1 to right-align, 0 to left-align
 * @return Total width of the rendered button group in pixels
 */
int GFX_blitButtonGroup(char** pairs, int primary, SDL_Surface* dst, int align_right) {
	int ox;
	int oy;
	int ow;
	char* hint;
	char* button;

	struct Hint {
		char* hint;
		char* button;
		int ow;
	} hints[2];
	int w = 0; // individual button dimension
	int h = 0; // hints index
	ow = 0; // full pill width
	ox = align_right ? dst->w - SCALE1(PADDING) : SCALE1(PADDING);
	oy = dst->h - SCALE1(PADDING + PILL_SIZE);

	for (int i = 0; i < 2; i++) {
		if (!pairs[i * 2])
			break;
		if (HAS_SKINNY_SCREEN && i != primary)
			continue; // space saving

		button = pairs[i * 2];
		hint = pairs[i * 2 + 1];
		w = GFX_getButtonWidth(hint, button);
		hints[h].hint = hint;
		hints[h].button = button;
		hints[h].ow = w;
		h += 1;
		ow += SCALE1(BUTTON_MARGIN) + w;
	}

	ow += SCALE1(BUTTON_MARGIN);
	if (align_right)
		ox -= ow;
	GFX_blitPill(gfx.mode == MODE_MAIN ? ASSET_DARK_GRAY_PILL : ASSET_BLACK_PILL, dst,
	             &(SDL_Rect){ox, oy, ow, SCALE1(PILL_SIZE)});

	ox += SCALE1(BUTTON_MARGIN);
	oy += SCALE1(BUTTON_MARGIN);
	for (int i = 0; i < h; i++) {
		GFX_blitButton(hints[i].hint, hints[i].button, dst, &(SDL_Rect){ox, oy});
		ox += hints[i].ow + SCALE1(BUTTON_MARGIN);
	}
	return ow;
}

#define MAX_TEXT_LINES 16

/**
 * Calculates the dimensions of multi-line text.
 *
 * Splits text by newlines and measures the widest line.
 * Height is calculated as line count * leading.
 *
 * @param font TTF font to measure with
 * @param str Text to measure (may contain newlines)
 * @param leading Line height in pixels
 * @param w Output: width of widest line
 * @param h Output: total height (lines * leading)
 */
void GFX_sizeText(TTF_Font* font, char* str, int leading, int* w, int* h) {
	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	*h = count * leading;

	int mw = 0;
	char line[256];
	for (int i = 0; i < count; i++) {
		int len;
		if (i + 1 < count) {
			len = lines[i + 1] - lines[i] - 1;
			if (len)
				strncpy(line, lines[i], len);
			line[len] = '\0';
		} else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}

		if (len) {
			int lw;
			TTF_SizeUTF8(font, line, &lw, NULL);
			if (lw > mw)
				mw = lw;
		}
	}
	*w = mw;
}

/**
 * Renders multi-line text centered in a rectangular area.
 *
 * Splits text by newlines and renders each line horizontally centered.
 * Lines are spaced vertically by the leading parameter.
 *
 * @param font TTF font to render with
 * @param str Text to render (may contain newlines)
 * @param leading Line spacing in pixels
 * @param color Text color
 * @param dst Destination surface
 * @param dst_rect Area to center text in (NULL for full surface)
 *
 * @note Maximum 16 lines supported
 */
void GFX_blitText(TTF_Font* font, char* str, int leading, SDL_Color color, SDL_Surface* dst,
                  SDL_Rect* dst_rect) {
	if (dst_rect == NULL)
		dst_rect = &(SDL_Rect){0, 0, dst->w, dst->h};

	char* lines[MAX_TEXT_LINES];
	int count = splitTextLines(str, lines, MAX_TEXT_LINES);
	int x = dst_rect->x;
	int y = dst_rect->y;

	SDL_Surface* text;
	char line[256];
	for (int i = 0; i < count; i++) {
		int len;
		if (i + 1 < count) {
			len = lines[i + 1] - lines[i] - 1;
			if (len)
				strncpy(line, lines[i], len);
			line[len] = '\0';
		} else {
			len = strlen(lines[i]);
			strcpy(line, lines[i]);
		}

		if (len) {
			text = TTF_RenderUTF8_Blended(font, line, color);
			SDL_BlitSurface(text, NULL, dst,
			                &(SDL_Rect){x + ((dst_rect->w - text->w) / 2), y + (i * leading)});
			SDL_FreeSurface(text);
		}
	}
}

///////////////////////////////
// Sound system - Ring buffer-based audio mixer
// (Based on picoarch's audio implementation)
///////////////////////////////

#define MAX_SAMPLE_RATE 48000
#define BATCH_SIZE 100 // Max frames to batch per write
#ifndef SAMPLES
#define SAMPLES 512 // SDL audio buffer size (default)
#endif

#define ms SDL_GetTicks // Shorthand for timestamp

typedef int (*SND_Resampler)(const SND_Frame frame);

// Sound context manages the ring buffer and resampling
static struct SND_Context {
	int initialized;
	double frame_rate;

	int sample_rate_in;
	int sample_rate_out;

	int buffer_seconds; // current_audio_buffer_size
	SND_Frame* buffer; // buf
	size_t frame_count; // buf_len

	int frame_in; // buf_w
	int frame_out; // buf_r
	int frame_filled; // max_buf_w

	SND_Resampler resample; // Selected resampler function
} snd = {0};

/**
 * SDL audio callback - consumes samples from the ring buffer.
 *
 * This is called by SDL on the audio thread when it needs more audio data.
 * Reads samples from the ring buffer and writes them to the output stream.
 * If buffer runs dry, repeats last sample or outputs silence.
 *
 * @param userdata Unused user data pointer
 * @param stream Output audio buffer to fill
 * @param len Length of output buffer in bytes
 *
 * @note Runs on SDL's audio thread, not the main thread
 */
static void SND_audioCallback(void* userdata, uint8_t* stream, int len) { // plat_sound_callback

	// return (void)memset(stream,0,len); // TODO: tmp, silent

	if (snd.frame_count == 0)
		return;

	int16_t* out = (int16_t*)stream;
	len /= (sizeof(int16_t) * 2);
	// int full_len = len;

	// if (snd.frame_out!=snd.frame_in) LOG_info("%8i consuming samples (%i frames)\n", ms(), len);

	while (snd.frame_out != snd.frame_in && len > 0) {
		*out++ = snd.buffer[snd.frame_out].left;
		*out++ = snd.buffer[snd.frame_out].right;

		snd.frame_filled = snd.frame_out;

		snd.frame_out += 1;
		len -= 1;

		if (snd.frame_out >= snd.frame_count)
			snd.frame_out = 0;
	}

	int zero = len > 0 && len == SAMPLES;
	if (zero)
		return (void)memset(out, 0, len * (sizeof(int16_t) * 2));
	// else if (len>=5) LOG_info("%8i BUFFER UNDERRUN (%i/%i frames)\n", ms(), len,full_len);

	int16_t* in = out - 1;
	while (len > 0) {
		*out++ = (void*)in > (void*)stream ? *--in : 0;
		*out++ = (void*)in > (void*)stream ? *--in : 0;
		len -= 1;
	}
}

/**
 * Resizes the audio ring buffer based on sample rate and frame rate.
 *
 * Calculates buffer size to hold buffer_seconds worth of audio.
 * Locks audio thread during resize to prevent corruption.
 *
 * @note Called during init and when audio parameters change
 */
static void SND_resizeBuffer(void) { // plat_sound_resize_buffer
	snd.frame_count = snd.buffer_seconds * snd.sample_rate_in / snd.frame_rate;
	if (snd.frame_count == 0)
		return;

	// LOG_info("frame_count: %i (%i * %i / %f)\n", snd.frame_count, snd.buffer_seconds, snd.sample_rate_in, snd.frame_rate);
	// snd.frame_count *= 2; // no help

	SDL_LockAudio();

	int buffer_bytes = snd.frame_count * sizeof(SND_Frame);
	snd.buffer = realloc(snd.buffer, buffer_bytes);

	memset(snd.buffer, 0, buffer_bytes);

	snd.frame_in = 0;
	snd.frame_out = 0;
	snd.frame_filled = snd.frame_count - 1;

	SDL_UnlockAudio();
}

/**
 * Passthrough resampler - no conversion needed.
 *
 * Used when input and output sample rates match.
 * Simply copies frames directly to the ring buffer.
 *
 * @param frame Audio frame to write
 * @return Number of frames consumed (always 1)
 */
static int SND_resampleNone(SND_Frame frame) { // audio_resample_passthrough
	snd.buffer[snd.frame_in++] = frame;
	if (snd.frame_in >= snd.frame_count)
		snd.frame_in = 0;
	return 1;
}

/**
 * Nearest-neighbor resampler for sample rate conversion.
 *
 * Uses Bresenham-like algorithm to determine when to drop/duplicate
 * samples. Accumulates difference between input and output rates.
 *
 * @param frame Audio frame to resample
 * @return Number of frames consumed (0 or 1)
 */
static int SND_resampleNear(SND_Frame frame) { // audio_resample_nearest
	static int diff = 0;
	int consumed = 0;

	if (diff < snd.sample_rate_out) {
		snd.buffer[snd.frame_in++] = frame;
		if (snd.frame_in >= snd.frame_count)
			snd.frame_in = 0;
		diff += snd.sample_rate_in;
	}

	if (diff >= snd.sample_rate_out) {
		consumed++;
		diff -= snd.sample_rate_out;
	}

	return consumed;
}

/**
 * Selects the appropriate resampler based on sample rates.
 *
 * Chooses passthrough if rates match, otherwise uses nearest-neighbor.
 */
static void SND_selectResampler(void) { // plat_sound_select_resampler
	if (snd.sample_rate_in == snd.sample_rate_out) {
		snd.resample = SND_resampleNone;
	} else {
		snd.resample = SND_resampleNear;
	}
}

/**
 * Writes a batch of audio samples to the ring buffer.
 *
 * Pushes frames through the resampler into the ring buffer.
 * Waits if buffer is full, batching writes for efficiency.
 * This is the main entry point for emulators to submit audio.
 *
 * @param frames Array of audio frames to write
 * @param frame_count Number of frames in array
 * @return Number of frames consumed (may be resampled)
 *
 * @note May block briefly if ring buffer is full
 */
size_t SND_batchSamples(const SND_Frame* frames,
                        size_t frame_count) { // plat_sound_write / plat_sound_write_resample

	// return frame_count; // TODO: tmp, silent

	if (snd.frame_count == 0)
		return 0;

	// LOG_info("%8i batching samples (%i frames)\n", ms(), frame_count);

	SDL_LockAudio();

	int consumed = 0;
	int consumed_frames;
	while (frame_count > 0) {
		int tries = 0;
		int amount = MIN(BATCH_SIZE, frame_count);

		while (tries < 10 && snd.frame_in == snd.frame_filled) {
			tries++;
			SDL_UnlockAudio();
			SDL_Delay(1);
			SDL_LockAudio();
		}
		// if (tries) LOG_info("%8i waited %ims for buffer to get low...\n", ms(), tries);

		while (amount && snd.frame_in != snd.frame_filled) {
			consumed_frames = snd.resample(*frames);

			frames += consumed_frames;
			amount -= consumed_frames;
			frame_count -= consumed_frames;
			consumed += consumed_frames;
		}
	}
	SDL_UnlockAudio();

	return consumed;
}

/**
 * Initializes the audio subsystem.
 *
 * Sets up SDL audio, allocates the ring buffer, configures resampling.
 * Audio starts playing immediately after this call.
 *
 * @param sample_rate Input sample rate from emulator/game
 * @param frame_rate Frame rate of the game (e.g., 60.0 for 60fps)
 *
 * @note Platform may adjust sample_rate via PLAT_pickSampleRate
 */
void SND_init(double sample_rate, double frame_rate) { // plat_sound_init
	LOG_info("SND_init\n");

	SDL_InitSubSystem(SDL_INIT_AUDIO);

#if defined(USE_SDL2)
	LOG_info("Available audio drivers:\n");
	for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
		LOG_info("- %s\n", SDL_GetAudioDriver(i));
	}
	LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver());
#endif

	memset(&snd, 0, sizeof(struct SND_Context));
	snd.frame_rate = frame_rate;

	SDL_AudioSpec spec_in;
	SDL_AudioSpec spec_out;

	spec_in.freq = PLAT_pickSampleRate(sample_rate, MAX_SAMPLE_RATE);
	spec_in.format = AUDIO_S16;
	spec_in.channels = 2;
	spec_in.samples = SAMPLES;
	spec_in.callback = SND_audioCallback;

	if (SDL_OpenAudio(&spec_in, &spec_out) < 0)
		LOG_info("SDL_OpenAudio error: %s\n", SDL_GetError());

	snd.buffer_seconds = 5;
	snd.sample_rate_in = sample_rate;
	snd.sample_rate_out = spec_out.freq;

	SND_selectResampler();
	SND_resizeBuffer();

	SDL_PauseAudio(0);

	LOG_info("sample rate: %i (req) %i (rec) [samples %i]\n", snd.sample_rate_in,
	         snd.sample_rate_out, SAMPLES);
	snd.initialized = 1;
}

/**
 * Shuts down the audio subsystem and frees resources.
 *
 * Pauses audio, closes SDL audio device, frees ring buffer.
 * Safe to call even if audio was never initialized.
 */
void SND_quit(void) { // plat_sound_finish
	if (!snd.initialized)
		return;

	SDL_PauseAudio(1);
	SDL_CloseAudio();

	if (snd.buffer) {
		free(snd.buffer);
		snd.buffer = NULL;
	}
}

///////////////////////////////
// Input - Lid detection (clamshell devices)
///////////////////////////////

// Global lid state for devices with flip-lid hardware
LID_Context lid = {
    .has_lid = 0,
    .is_open = 1,
};

/**
 * Initializes lid detection hardware.
 *
 * Default implementation does nothing. Platforms with lid hardware
 * (e.g., GKD Pixel) override this to set up GPIO or sensors.
 */
FALLBACK_IMPLEMENTATION void PLAT_initLid(void) {}

/**
 * Checks if lid state has changed.
 *
 * Default implementation returns 0 (no lid). Platforms override
 * this to detect lid open/close events.
 *
 * @param state Output: current lid state (1=open, 0=closed), may be NULL
 * @return 1 if state changed since last call, 0 otherwise
 */
FALLBACK_IMPLEMENTATION int PLAT_lidChanged(int* state) {
	return 0;
}

///////////////////////////////
// Input - Button and analog stick handling
///////////////////////////////

// Global input state, polled each frame
PAD_Context pad;

// Analog stick deadzone (threshold for registering input)
#define AXIS_DEADZONE 0x4000

/**
 * Processes analog stick movement and updates button state.
 *
 * Converts analog axis value to digital button presses (up/down/left/right).
 * Handles deadzone, button repeat, and opposite direction cancellation.
 *
 * @param neg_id Button ID for negative direction (left/up)
 * @param pos_id Button ID for positive direction (right/down)
 * @param value Analog axis value (-32768 to 32767)
 * @param repeat_at Timestamp when button should start repeating
 *
 * @note Called internally by PLAT_pollInput for analog stick axes
 */
void PAD_setAnalog(int neg_id, int pos_id, int value, int repeat_at) {
	// LOG_info("neg %i pos %i value %i\n", neg_id, pos_id, value);
	int neg = 1 << neg_id;
	int pos = 1 << pos_id;
	if (value > AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed & pos)) { // not pressing
			pad.is_pressed |= pos; // set
			pad.just_pressed |= pos; // set
			pad.just_repeated |= pos; // set
			pad.repeat_at[pos_id] = repeat_at;

			if (pad.is_pressed & neg) { // was pressing opposite
				pad.is_pressed &= ~neg; // unset
				pad.just_repeated &= ~neg; // unset
				pad.just_released |= neg; // set
			}
		}
	} else if (value < -AXIS_DEADZONE) { // pressing
		if (!(pad.is_pressed & neg)) { // not pressing
			pad.is_pressed |= neg; // set
			pad.just_pressed |= neg; // set
			pad.just_repeated |= neg; // set
			pad.repeat_at[neg_id] = repeat_at;

			if (pad.is_pressed & pos) { // was pressing opposite
				pad.is_pressed &= ~pos; // unset
				pad.just_repeated &= ~pos; // unset
				pad.just_released |= pos; // set
			}
		}
	} else { // not pressing
		if (pad.is_pressed & neg) { // was pressing
			pad.is_pressed &= ~neg; // unset
			pad.just_repeated &= neg; // unset
			pad.just_released |= neg; // set
		}
		if (pad.is_pressed & pos) { // was pressing
			pad.is_pressed &= ~pos; // unset
			pad.just_repeated &= pos; // unset
			pad.just_released |= pos; // set
		}
	}
}

/**
 * Resets all button states to unpressed.
 *
 * Clears all button press/release/repeat flags.
 * Call this when changing contexts (e.g., entering/exiting sleep).
 */
void PAD_reset(void) {
	// LOG_info("PAD_reset");
	pad.just_pressed = BTN_NONE;
	pad.is_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;
}

/**
 * Polls input devices and updates global button state.
 *
 * Default implementation handles:
 * - SDL keyboard events (mapped to buttons)
 * - SDL joystick/gamepad button events
 * - SDL joystick hat (d-pad) events
 * - SDL analog stick axis events
 * - Button repeat timing
 * - Lid close detection (triggers BTN_SLEEP)
 *
 * Called once per frame. Updates pad.just_pressed, pad.is_pressed,
 * pad.just_released, and pad.just_repeated bitmasks.
 *
 * Button repeat behavior:
 * - Initial delay: PAD_REPEAT_DELAY (300ms)
 * - Repeat interval: PAD_REPEAT_INTERVAL (100ms)
 *
 * @note Platforms can override this to handle custom input hardware
 */
FALLBACK_IMPLEMENTATION void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	// the actual poll
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		int btn = BTN_NONE;
		int pressed = 0; // 0=up,1=down
		int id = -1;
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			pressed = event.type == SDL_KEYDOWN;
			// LOG_info("key event: %i (%i)\n", code,pressed);
			if (code == CODE_UP) {
				btn = BTN_DPAD_UP;
				id = BTN_ID_DPAD_UP;
			} else if (code == CODE_DOWN) {
				btn = BTN_DPAD_DOWN;
				id = BTN_ID_DPAD_DOWN;
			} else if (code == CODE_LEFT) {
				btn = BTN_DPAD_LEFT;
				id = BTN_ID_DPAD_LEFT;
			} else if (code == CODE_RIGHT) {
				btn = BTN_DPAD_RIGHT;
				id = BTN_ID_DPAD_RIGHT;
			} else if (code == CODE_A) {
				btn = BTN_A;
				id = BTN_ID_A;
			} else if (code == CODE_B) {
				btn = BTN_B;
				id = BTN_ID_B;
			} else if (code == CODE_X) {
				btn = BTN_X;
				id = BTN_ID_X;
			} else if (code == CODE_Y) {
				btn = BTN_Y;
				id = BTN_ID_Y;
			} else if (code == CODE_START) {
				btn = BTN_START;
				id = BTN_ID_START;
			} else if (code == CODE_SELECT) {
				btn = BTN_SELECT;
				id = BTN_ID_SELECT;
			} else if (code == CODE_MENU) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (code == CODE_MENU_ALT) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (code == CODE_L1) {
				btn = BTN_L1;
				id = BTN_ID_L1;
			} else if (code == CODE_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
			} else if (code == CODE_L3) {
				btn = BTN_L3;
				id = BTN_ID_L3;
			} else if (code == CODE_R1) {
				btn = BTN_R1;
				id = BTN_ID_R1;
			} else if (code == CODE_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
			} else if (code == CODE_R3) {
				btn = BTN_R3;
				id = BTN_ID_R3;
			} else if (code == CODE_PLUS) {
				btn = BTN_PLUS;
				id = BTN_ID_PLUS;
			} else if (code == CODE_MINUS) {
				btn = BTN_MINUS;
				id = BTN_ID_MINUS;
			} else if (code == CODE_POWER) {
				btn = BTN_POWER;
				id = BTN_ID_POWER;
			} else if (code == CODE_POWEROFF) {
				btn = BTN_POWEROFF;
				id = BTN_ID_POWEROFF;
			} // nano-only
		} else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			pressed = event.type == SDL_JOYBUTTONDOWN;
			// LOG_info("joy event: %i (%i)\n", joy,pressed);
			if (joy == JOY_UP) {
				btn = BTN_DPAD_UP;
				id = BTN_ID_DPAD_UP;
			} else if (joy == JOY_DOWN) {
				btn = BTN_DPAD_DOWN;
				id = BTN_ID_DPAD_DOWN;
			} else if (joy == JOY_LEFT) {
				btn = BTN_DPAD_LEFT;
				id = BTN_ID_DPAD_LEFT;
			} else if (joy == JOY_RIGHT) {
				btn = BTN_DPAD_RIGHT;
				id = BTN_ID_DPAD_RIGHT;
			} else if (joy == JOY_A) {
				btn = BTN_A;
				id = BTN_ID_A;
			} else if (joy == JOY_B) {
				btn = BTN_B;
				id = BTN_ID_B;
			} else if (joy == JOY_X) {
				btn = BTN_X;
				id = BTN_ID_X;
			} else if (joy == JOY_Y) {
				btn = BTN_Y;
				id = BTN_ID_Y;
			} else if (joy == JOY_START) {
				btn = BTN_START;
				id = BTN_ID_START;
			} else if (joy == JOY_SELECT) {
				btn = BTN_SELECT;
				id = BTN_ID_SELECT;
			} else if (joy == JOY_MENU) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_MENU_ALT) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_MENU_ALT2) {
				btn = BTN_MENU;
				id = BTN_ID_MENU;
			} else if (joy == JOY_L1) {
				btn = BTN_L1;
				id = BTN_ID_L1;
			} else if (joy == JOY_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
			} else if (joy == JOY_L3) {
				btn = BTN_L3;
				id = BTN_ID_L3;
			} else if (joy == JOY_R1) {
				btn = BTN_R1;
				id = BTN_ID_R1;
			} else if (joy == JOY_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
			} else if (joy == JOY_R3) {
				btn = BTN_R3;
				id = BTN_ID_R3;
			} else if (joy == JOY_PLUS) {
				btn = BTN_PLUS;
				id = BTN_ID_PLUS;
			} else if (joy == JOY_MINUS) {
				btn = BTN_MINUS;
				id = BTN_ID_MINUS;
			} else if (joy == JOY_POWER) {
				btn = BTN_POWER;
				id = BTN_ID_POWER;
			}
		} else if (event.type == SDL_JOYHATMOTION) {
			int hats[4] = {-1, -1, -1, -1}; // -1=no change,0=up,1=down,2=left,3=right btn_ids
			int hat = event.jhat.value;
			// LOG_info("hat event: %i\n", hat);
			// TODO: safe to assume hats will always be the primary dpad?
			// TODO: this is literally a bitmask, make it one (oh, except there's 3 states...)
			switch (hat) {
			case SDL_HAT_UP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 0;
				break;
			case SDL_HAT_DOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 0;
				hats[3] = 0;
				break;
			case SDL_HAT_LEFT:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_RIGHT:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_LEFTUP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_LEFTDOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 1;
				hats[3] = 0;
				break;
			case SDL_HAT_RIGHTUP:
				hats[0] = 1;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_RIGHTDOWN:
				hats[0] = 0;
				hats[1] = 1;
				hats[2] = 0;
				hats[3] = 1;
				break;
			case SDL_HAT_CENTERED:
				hats[0] = 0;
				hats[1] = 0;
				hats[2] = 0;
				hats[3] = 0;
				break;
			default:
				break;
			}

			for (id = 0; id < 4; id++) {
				int state = hats[id];
				btn = 1 << id;
				if (state == 0) {
					pad.is_pressed &= ~btn; // unset
					pad.just_repeated &= ~btn; // unset
					pad.just_released |= btn; // set
				} else if (state == 1 && (pad.is_pressed & btn) == BTN_NONE) {
					pad.just_pressed |= btn; // set
					pad.just_repeated |= btn; // set
					pad.is_pressed |= btn; // set
					pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
				}
			}
			btn = BTN_NONE; // already handled, force continue
		} else if (event.type == SDL_JOYAXISMOTION) {
			int axis = event.jaxis.axis;
			int val = event.jaxis.value;
			// LOG_info("axis: %i (%i)\n", axis,val);

			// triggers on tg5040
			if (axis == AXIS_L2) {
				btn = BTN_L2;
				id = BTN_ID_L2;
				pressed = val > 0;
			} else if (axis == AXIS_R2) {
				btn = BTN_R2;
				id = BTN_ID_R2;
				pressed = val > 0;
			}

			else if (axis == AXIS_LX) {
				pad.laxis.x = val;
				PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, val,
				              tick + PAD_REPEAT_DELAY);
			} else if (axis == AXIS_LY) {
				pad.laxis.y = val;
				PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, val, tick + PAD_REPEAT_DELAY);
			} else if (axis == AXIS_RX)
				pad.raxis.x = val;
			else if (axis == AXIS_RY)
				pad.raxis.y = val;

			// axis will fire off what looks like a release
			// before the first press but you can't release
			// a button that wasn't pressed
			if (!pressed && btn != BTN_NONE && !(pad.is_pressed & btn)) {
				// LOG_info("cancel: %i\n", axis);
				btn = BTN_NONE;
			}
		}
		// else if (event.type==SDL_QUIT) PWR_powerOff(); // added for macOS debug

		if (btn == BTN_NONE)
			continue;

		if (!pressed) {
			pad.is_pressed &= ~btn; // unset
			pad.just_repeated &= ~btn; // unset
			pad.just_released |= btn; // set
		} else if ((pad.is_pressed & btn) == BTN_NONE) {
			pad.just_pressed |= btn; // set
			pad.just_repeated |= btn; // set
			pad.is_pressed |= btn; // set
			pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
		}
	}

	if (lid.has_lid && PLAT_lidChanged(NULL))
		pad.just_released |= BTN_SLEEP;
}

/**
 * Checks if device should wake from sleep.
 *
 * Polls for wake button (BTN_POWER or BTN_MENU depending on platform).
 * Also checks lid state - if lid is closed, wake button is ignored.
 *
 * @return 1 if device should wake, 0 otherwise
 *
 * @note Consumes the wake button event to prevent double-triggering
 */
FALLBACK_IMPLEMENTATION int PLAT_shouldWake(void) {
	int lid_open = 1; // assume open by default
	if (lid.has_lid && PLAT_lidChanged(&lid_open) && lid_open)
		return 1;


	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_KEYUP) {
			uint8_t code = event.key.keysym.scancode;
			if ((BTN_WAKE == BTN_POWER && code == CODE_POWER) ||
			    (BTN_WAKE == BTN_MENU && (code == CODE_MENU || code == CODE_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open)
					return 0; // do it here so we eat the input
				return 1;
			}
		} else if (event.type == SDL_JOYBUTTONUP) {
			uint8_t joy = event.jbutton.button;
			if ((BTN_WAKE == BTN_POWER && joy == JOY_POWER) ||
			    (BTN_WAKE == BTN_MENU && (joy == JOY_MENU || joy == JOY_MENU_ALT))) {
				// ignore input while lid is closed
				if (lid.has_lid && !lid.is_open)
					return 0; // do it here so we eat the input
				return 1;
			}
		}
	}
	return 0;
}

/**
 * Checks if any button was just pressed this frame.
 *
 * @return 1 if any button was just pressed, 0 otherwise
 */
int PAD_anyJustPressed(void) {
	return pad.just_pressed != BTN_NONE;
}

/**
 * Checks if any button is currently held down.
 *
 * @return 1 if any button is pressed, 0 otherwise
 */
int PAD_anyPressed(void) {
	return pad.is_pressed != BTN_NONE;
}

/**
 * Checks if any button was just released this frame.
 *
 * @return 1 if any button was just released, 0 otherwise
 */
int PAD_anyJustReleased(void) {
	return pad.just_released != BTN_NONE;
}

/**
 * Checks if a specific button was just pressed this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just pressed, 0 otherwise
 */
int PAD_justPressed(int btn) {
	return pad.just_pressed & btn;
}

/**
 * Checks if a specific button is currently held down.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button is pressed, 0 otherwise
 */
int PAD_isPressed(int btn) {
	return pad.is_pressed & btn;
}

/**
 * Checks if a specific button was just released this frame.
 *
 * @param btn Button bitmask (e.g., BTN_A, BTN_START)
 * @return 1 if button was just released, 0 otherwise
 */
int PAD_justReleased(int btn) {
	return pad.just_released & btn;
}

/**
 * Checks if a specific button is repeating (held for repeat interval).
 *
 * @param btn Button bitmask (e.g., BTN_DPAD_UP)
 * @return 1 if button is repeating this frame, 0 otherwise
 */
int PAD_justRepeated(int btn) {
	return pad.just_repeated & btn;
}

/**
 * Detects a quick tap of the menu button.
 *
 * Returns true if menu button was pressed and released within MENU_DELAY (250ms)
 * without any brightness adjustment (PLUS/MINUS) being triggered.
 *
 * @param now Current timestamp in milliseconds
 * @return 1 if menu was tapped (not held), 0 otherwise
 *
 * @note Used to distinguish menu tap from menu+brightness adjustment
 */
int PAD_tappedMenu(uint32_t now) {
#define MENU_DELAY 250 // also in PWR_update()
	static uint32_t menu_start = 0;
	static int ignore_menu = 0;
	if (PAD_justPressed(BTN_MENU)) {
		ignore_menu = 0;
		menu_start = now;
	} else if (PAD_isPressed(BTN_MENU) && BTN_MOD_BRIGHTNESS == BTN_MENU &&
	           (PAD_justPressed(BTN_MOD_PLUS) || PAD_justPressed(BTN_MOD_MINUS))) {
		ignore_menu = 1;
	}
	return (!ignore_menu && PAD_justReleased(BTN_MENU) && now - menu_start < MENU_DELAY);
}

///////////////////////////////
// Vibration - Rumble motor control
///////////////////////////////

// Vibration context with deferred state changes to minimize motor wear
static struct VIB_Context {
	int initialized;
	pthread_t pt;
	int queued_strength;
	int strength; // Current applied strength
} vib = {0};

/**
 * Vibration worker thread that applies deferred strength changes.
 *
 * Defers strength changes for 3 frames to prevent rapid on/off cycling
 * which can damage rumble motors. Runs continuously at ~60Hz.
 *
 * @param arg Unused thread argument
 * @return Never returns (infinite loop)
 */
static void* VIB_thread(void* arg) {
#define DEFER_FRAMES 3
	static int defer = 0;
	while (1) {
		SDL_Delay(17);
		if (vib.queued_strength != vib.strength) {
			if (defer < DEFER_FRAMES &&
			    vib.queued_strength ==
			        0) { // minimize vacillation between 0 and some number (which this motor doesn't like)
				defer += 1;
				continue;
			}
			vib.strength = vib.queued_strength;
			defer = 0;

			PLAT_setRumble(vib.strength);
		}
	}
	return 0;
}

/**
 * Initializes the vibration subsystem.
 *
 * Starts the vibration worker thread. Call this before using vibration.
 */
void VIB_init(void) {
	vib.queued_strength = vib.strength = 0;
	pthread_create(&vib.pt, NULL, &VIB_thread, NULL);
	vib.initialized = 1;
}

/**
 * Shuts down the vibration subsystem.
 *
 * Stops vibration and terminates the worker thread.
 * Safe to call even if not initialized.
 */
void VIB_quit(void) {
	if (!vib.initialized)
		return;

	VIB_setStrength(0);
	pthread_cancel(vib.pt);
	pthread_join(vib.pt, NULL);
}

/**
 * Queues a vibration strength change.
 *
 * Change is deferred by 3 frames to prevent rapid motor cycling.
 * No-op if strength hasn't changed.
 *
 * @param strength Vibration strength (0=off, higher=stronger)
 */
void VIB_setStrength(int strength) {
	if (vib.queued_strength == strength)
		return;
	vib.queued_strength = strength;
}

/**
 * Gets the current applied vibration strength.
 *
 * @return Current strength (not queued strength)
 */
int VIB_getStrength(void) {
	return vib.strength;
}

///////////////////////////////
// Power management - Battery, sleep, brightness, volume
///////////////////////////////

/**
 * Initializes the low battery warning overlay.
 *
 * Creates an overlay surface showing a low battery icon.
 * The overlay is toggled on/off based on battery charge level.
 */
static void PWR_initOverlay(void) {
	// setup surface
	pwr.overlay = PLAT_initOverlay();

	// draw battery
	SDLX_SetAlpha(gfx.assets, 0, 0);
	GFX_blitAsset(ASSET_BLACK_PILL, NULL, pwr.overlay, NULL);
	SDLX_SetAlpha(gfx.assets, SDL_SRCALPHA, 0);
	GFX_blitBattery(pwr.overlay, NULL);
}

/**
 * Updates battery charging state and charge level.
 *
 * Queries platform for current battery status and updates overlay
 * visibility based on charge level and warning state.
 */
static void PWR_updateBatteryStatus(void) {
	PLAT_getBatteryStatus(&pwr.is_charging, &pwr.charge);
	PLAT_enableOverlay(pwr.should_warn && pwr.charge <= PWR_LOW_CHARGE);
}

/**
 * Battery monitoring worker thread.
 *
 * Polls battery status every 5 seconds and updates the UI overlay.
 * Runs continuously in the background.
 *
 * @param arg Unused thread argument
 * @return Never returns (infinite loop)
 */
static void* PWR_monitorBattery(void* arg) {
	while (1) {
		// TODO: the frequency of checking could depend on whether
		// we're in game (less frequent) or menu (more frequent)
		sleep(5);
		PWR_updateBatteryStatus();
	}
	return NULL;
}

/**
 * Initializes the power management subsystem.
 *
 * Sets up battery monitoring, initializes overlay, starts background thread.
 * Configures default power management flags (sleep/poweroff enabled).
 */
void PWR_init(void) {
	pwr.can_sleep = 1;
	pwr.can_poweroff = 1;
	pwr.can_autosleep = 1;

	pwr.requested_sleep = 0;
	pwr.requested_wake = 0;

	pwr.should_warn = 0;
	pwr.charge = PWR_LOW_CHARGE;

	PWR_initOverlay();

	PWR_updateBatteryStatus();
	pthread_create(&pwr.battery_pt, NULL, &PWR_monitorBattery, NULL);
	pwr.initialized = 1;
}

/**
 * Shuts down the power management subsystem.
 *
 * Terminates battery monitoring thread and frees overlay resources.
 * Safe to call even if not initialized.
 */
void PWR_quit(void) {
	if (!pwr.initialized)
		return;

	PLAT_quitOverlay();

	// cancel battery thread
	pthread_cancel(pwr.battery_pt);
	pthread_join(pwr.battery_pt, NULL);
}

/**
 * Enables or disables low battery warning overlay.
 *
 * @param enable 1 to show warning when battery low, 0 to hide
 */
void PWR_warn(int enable) {
	pwr.should_warn = enable;
	PLAT_enableOverlay(pwr.should_warn && pwr.charge <= PWR_LOW_CHARGE);
}

/**
 * Checks if a button press should be ignored during settings adjustment.
 *
 * Returns true for PLUS/MINUS buttons when brightness/volume overlay is showing.
 * Prevents these buttons from affecting the game/app while adjusting settings.
 *
 * @param btn Button that was pressed
 * @param show_setting Current settings overlay state (1=brightness, 2=volume)
 * @return 1 if button should be ignored, 0 otherwise
 */
int PWR_ignoreSettingInput(int btn, int show_setting) {
	return show_setting && (btn == BTN_MOD_PLUS || btn == BTN_MOD_MINUS);
}

/**
 * Main power management update function, called each frame.
 *
 * Handles:
 * - Autosleep after 30 seconds of inactivity
 * - Manual sleep button (lid close or sleep button)
 * - Power button hold detection (1 second to power off)
 * - Brightness/volume overlay display timing
 * - Charging state change detection
 * - Display refresh dirty flag management
 *
 * @param _dirty Pointer to dirty flag (set to 1 when display needs refresh)
 * @param _show_setting Pointer to settings overlay state (0/1/2)
 * @param before_sleep Callback invoked before entering sleep
 * @param after_sleep Callback invoked after waking from sleep
 *
 * @note All pointer parameters may be NULL if not needed
 */
void PWR_update(int* _dirty, int* _show_setting, PWR_callback_t before_sleep,
                PWR_callback_t after_sleep) {
	int dirty = _dirty ? *_dirty : 0;
	int show_setting = _show_setting ? *_show_setting : 0;

	static uint32_t last_input_at = 0; // timestamp of last input (autosleep)
	static uint32_t checked_charge_at = 0; // timestamp of last time checking charge
	static uint32_t setting_shown_at = 0; // timestamp when settings started being shown
	static uint32_t power_pressed_at = 0; // timestamp when power button was just pressed
	static uint32_t mod_unpressed_at =
	    0; // timestamp of last time settings modifier key was NOT down
	static uint32_t was_muted = -1;
	if (was_muted == -1)
		was_muted = GetMute();

	static int was_charging = -1;
	if (was_charging == -1)
		was_charging = pwr.is_charging;

	uint32_t now = SDL_GetTicks();
	if (was_charging || PAD_anyPressed() || last_input_at == 0)
		last_input_at = now;

#define CHARGE_DELAY 1000
	if (dirty || now - checked_charge_at >= CHARGE_DELAY) {
		int is_charging = pwr.is_charging;
		if (was_charging != is_charging) {
			was_charging = is_charging;
			dirty = 1;
		}
		checked_charge_at = now;
	}

	if (PAD_justReleased(BTN_POWEROFF) || (power_pressed_at && now - power_pressed_at >= 1000)) {
		if (before_sleep)
			before_sleep();
		PWR_powerOff();
	}

	if (PAD_justPressed(BTN_POWER)) {
		power_pressed_at = now;
	}

#define SLEEP_DELAY 30000 // 30 seconds
	if (now - last_input_at >= SLEEP_DELAY && PWR_preventAutosleep())
		last_input_at = now;

	if (pwr.requested_sleep || // hardware requested sleep
	    now - last_input_at >= SLEEP_DELAY || // autosleep
	    (pwr.can_sleep && PAD_justReleased(BTN_SLEEP)) // manual sleep
	) {
		pwr.requested_sleep = 0;
		if (before_sleep)
			before_sleep();
		PWR_fauxSleep();
		if (after_sleep)
			after_sleep();

		last_input_at = now = SDL_GetTicks();
		power_pressed_at = 0;
		dirty = 1;
	}

	// TODO: only delay hiding setting changes if that setting didn't require a modifier button be held, otherwise release as soon as modifier is released

	int delay_settings =
	    BTN_MOD_BRIGHTNESS ==
	    BTN_MENU; // when both volume and brighness require a modifier hide settings as soon as it is released
#define SETTING_DELAY 500
	if (show_setting && (now - setting_shown_at >= SETTING_DELAY || !delay_settings) &&
	    !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		show_setting = 0;
		dirty = 1;
	}

	if (!show_setting && !PAD_isPressed(BTN_MOD_VOLUME) && !PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
		mod_unpressed_at = now; // this feels backwards but is correct
	}

#define MOD_DELAY 250
	if (((PAD_isPressed(BTN_MOD_VOLUME) || PAD_isPressed(BTN_MOD_BRIGHTNESS)) &&
	     (!delay_settings || now - mod_unpressed_at >= MOD_DELAY)) ||
	    ((!BTN_MOD_VOLUME || !BTN_MOD_BRIGHTNESS) &&
	     (PAD_justRepeated(BTN_MOD_PLUS) || PAD_justRepeated(BTN_MOD_MINUS)))) {
		setting_shown_at = now;
		if (PAD_isPressed(BTN_MOD_BRIGHTNESS)) {
			show_setting = 1;
		} else {
			show_setting = 2;
		}
	}

	int muted = GetMute();
	if (muted != was_muted) {
		was_muted = muted;
		show_setting = 2;
		setting_shown_at = now;
	}

	if (show_setting)
		dirty = 1; // shm is slow or keymon is catching input on the next frame
	if (_dirty)
		*_dirty = dirty;
	if (_show_setting)
		*_show_setting = show_setting;
}

/**
 * Disables manual sleep (sleep button/lid close).
 *
 * Prevents device from sleeping when sleep button is pressed.
 * Autosleep and power off may still occur.
 */
void PWR_disableSleep(void) {
	pwr.can_sleep = 0;
}
/**
 * Re-enables manual sleep.
 */
void PWR_enableSleep(void) {
	pwr.can_sleep = 1;
}

/**
 * Disables power off functionality.
 *
 * Prevents device from powering off completely.
 * Useful during critical operations.
 */
void PWR_disablePowerOff(void) {
	pwr.can_poweroff = 0;
}

/**
 * Powers off the device.
 *
 * Displays "powering off" message and calls platform power off.
 * Checks for auto-resume quicksave and adjusts message accordingly.
 * Only works if power off hasn't been disabled.
 *
 * @note This function does not return if power off succeeds
 */
void PWR_powerOff(void) {
	if (pwr.can_poweroff) {
		int w = FIXED_WIDTH;
		int h = FIXED_HEIGHT;
		int p = FIXED_PITCH;
		if (GetHDMI()) {
			w = HDMI_WIDTH;
			h = HDMI_HEIGHT;
			p = HDMI_PITCH;
		}
		gfx.screen = GFX_resize(w, h, p);

		char* msg;
		if (HAS_POWER_BUTTON || HAS_POWEROFF_BUTTON)
			msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npowering off" : "Powering off";
		else
			msg = exists(AUTO_RESUME_PATH) ? "Quicksave created,\npower off now" : "Power off now";

		// LOG_info("PWR_powerOff %s (%ix%i)\n", gfx.screen, gfx.screen->w, gfx.screen->h);

		// TODO: for some reason screen's dimensions end up being 0x0 in GFX_blitMessage...
		PLAT_clearVideo(gfx.screen);
		GFX_blitMessage(font.large, msg, gfx.screen,
		                &(SDL_Rect){0, 0, gfx.screen->w, gfx.screen->h}); //, NULL);
		GFX_flip(gfx.screen);
		PLAT_powerOff();
	}
}

/**
 * Enters sleep mode (low power state).
 *
 * On HDMI: Clears screen
 * On device screen: Mutes audio, disables backlight
 * Pauses keymon daemon and syncs filesystem.
 */
static void PWR_enterSleep(void) {
	SDL_PauseAudio(1);
	if (GetHDMI()) {
		PLAT_clearVideo(gfx.screen);
		PLAT_flip(gfx.screen, 0);
	} else {
		SetRawVolume(MUTE_VOLUME_RAW);
		PLAT_enableBacklight(0);
	}
	system("killall -STOP keymon.elf");

	sync();
}

/**
 * Exits sleep mode and restores normal operation.
 *
 * Restores backlight, volume, resumes keymon, and unpauses audio.
 */
static void PWR_exitSleep(void) {
	system("killall -CONT keymon.elf");
	if (GetHDMI()) {
		// buh
	} else {
		PLAT_enableBacklight(1);
		SetVolume(GetVolume());
	}
	SDL_PauseAudio(0);

	sync();
}

/**
 * Waits in sleep mode until wake condition occurs.
 *
 * Polls wake button every 200ms. If sleeping for > 2 minutes and not
 * charging, powers off automatically. Charging extends wait time.
 */
static void PWR_waitForWake(void) {
	uint32_t sleep_ticks = SDL_GetTicks();
	while (!PAD_wake()) {
		if (pwr.requested_wake) {
			pwr.requested_wake = 0;
			break;
		}
		SDL_Delay(200);
		if (pwr.can_poweroff &&
		    SDL_GetTicks() - sleep_ticks >= 120000) { // increased to two minutes
			if (pwr.is_charging)
				sleep_ticks += 60000; // check again in a minute
			else
				PWR_powerOff();
		}
	}

	return;
}

/**
 * Performs a "fake sleep" by entering and exiting sleep mode.
 *
 * Clears screen, resets input, enters sleep, waits for wake,
 * then exits sleep and resets input again. This is the main
 * sleep function called by applications.
 */
void PWR_fauxSleep(void) {
	GFX_clear(gfx.screen);
	PAD_reset();
	PWR_enterSleep();
	PWR_waitForWake();
	PWR_exitSleep();
	PAD_reset();
}

/**
 * Disables automatic sleep after 30 seconds of inactivity.
 */
void PWR_disableAutosleep(void) {
	pwr.can_autosleep = 0;
}

/**
 * Re-enables automatic sleep.
 */
void PWR_enableAutosleep(void) {
	pwr.can_autosleep = 1;
}

/**
 * Checks if autosleep should be prevented.
 *
 * Autosleep is prevented when:
 * - Device is charging
 * - Autosleep has been disabled
 * - HDMI is connected
 *
 * @return 1 if autosleep should be prevented, 0 otherwise
 */
int PWR_preventAutosleep(void) {
	return pwr.is_charging || !pwr.can_autosleep || GetHDMI();
}

/**
 * Checks if device is currently charging.
 *
 * @return 1 if charging, 0 otherwise
 *
 * @note Value updated every 5 seconds by battery monitoring thread
 */
int PWR_isCharging(void) {
	return pwr.is_charging;
}

/**
 * Gets current battery charge level.
 *
 * @return Charge percentage (10-100 in 10-20% increments)
 *
 * @note Value updated every 5 seconds by battery monitoring thread
 */
int PWR_getBattery(void) { // 10-100 in 10-20% fragments
	return pwr.charge;
}

///////////////////////////////
// Platform utility functions
///////////////////////////////

/**
 * Sets the system date and time.
 *
 * Executes date command and syncs to hardware clock.
 * This is a temporary implementation - platforms may override.
 *
 * @param y Year
 * @param m Month (1-12)
 * @param d Day (1-31)
 * @param h Hour (0-23)
 * @param i Minute (0-59)
 * @param s Second (0-59)
 * @return Always returns 0
 */
int PLAT_setDateTime(int y, int m, int d, int h, int i, int s) {
	char cmd[512];
	sprintf(cmd, "date -s '%d-%d-%d %d:%d:%d'; hwclock --utc -w", y, m, d, h, i, s);
	system(cmd);
	return 0; // why does this return an int?
}
