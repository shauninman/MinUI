/**
 * platform.c - Miyoo Mini platform implementation
 *
 * Provides hardware-specific implementations for the Miyoo Mini family of devices.
 * This is one of the most complex platform implementations in MinUI, featuring:
 *
 * - Hardware-accelerated blitting via MI_GFX API (zero-copy rendering)
 * - ION memory allocator for physically contiguous buffers
 * - Lid sensor support (Hall effect sensor)
 * - AXP223 power management IC (Plus model)
 * - Hardware variant detection (Mini vs Plus, 480p vs 560p)
 *
 * Supported Devices:
 * - Miyoo Mini (original) - 640x480, GPIO battery monitoring
 * - Miyoo Mini Plus - 640x480, AXP223 PMIC, improved hardware
 * - Miyoo Mini Plus (560p variant) - 752x560 resolution option
 *
 * Hardware Architecture:
 * The Miyoo Mini uses the MStar/SigmaStar SSC8336 SoC which provides:
 * - MI_GFX: Hardware graphics blitter for fast scaling/rotation/blending
 * - MI_SYS: System services including ION memory allocation
 * - Double buffering with physically contiguous memory for DMA
 *
 * Memory Management:
 * - ION allocator provides physically contiguous memory required by MI_GFX
 * - Double buffering (page flipping) for tear-free rendering
 * - Physical addresses (padd) used by hardware, virtual addresses (vadd) for CPU
 *
 * @note Based on eggs' GFXSample_rev15 implementation
 */

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

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"
#include "scaler.h"

///////////////////////////////
// MI_GFX Hardware Blitting API
///////////////////////////////

#include <mi_sys.h>
#include <mi_gfx.h>

// Hardware variant flags (detected at runtime)
int is_560p = 0;  // 1 if device supports 752x560 resolution
int is_plus = 0;  // 1 if device is Miyoo Mini Plus with AXP223 PMIC

// SDL surface extension: stores physical address for MI_GFX
#define	pixelsPa	unused1

// Align value to 4KB boundary for cache operations
#define ALIGN4K(val)	((val+4095)&(~4095))

/**
 * Determines MI_GFX color format from SDL surface pixel format.
 *
 * Maps SDL pixel format masks to MI_GFX hardware color format constants.
 * Supports 16-bit and 32-bit formats with various alpha channel layouts.
 *
 * @param surface SDL surface to analyze (may be NULL)
 * @return MI_GFX color format enum, defaults to ARGB8888 if unknown
 */
static inline MI_GFX_ColorFmt_e	GFX_ColorFmt(SDL_Surface *surface) {
	if (surface) {
		if (surface->format->BytesPerPixel == 2) {
			if (surface->format->Amask == 0x0000) return E_MI_GFX_FMT_RGB565;
			if (surface->format->Amask == 0x8000) return E_MI_GFX_FMT_ARGB1555;
			if (surface->format->Amask == 0xF000) return E_MI_GFX_FMT_ARGB4444;
			if (surface->format->Amask == 0x0001) return E_MI_GFX_FMT_RGBA5551;
			if (surface->format->Amask == 0x000F) return E_MI_GFX_FMT_RGBA4444;
			return E_MI_GFX_FMT_RGB565;
		}
		if (surface->format->Bmask == 0x000000FF) return E_MI_GFX_FMT_ARGB8888;
		if (surface->format->Rmask == 0x000000FF) return E_MI_GFX_FMT_ABGR8888;
	}
	return E_MI_GFX_FMT_ARGB8888;
}

/**
 * Flushes CPU cache for a rectangular region of a surface.
 *
 * Required before MI_GFX operations to ensure CPU writes are visible to the
 * hardware blitter. Operates on 4KB-aligned cache lines, so x/w are ignored.
 *
 * @param pixels Base address of pixel buffer
 * @param pitch Bytes per row
 * @param y Starting row
 * @param h Height in rows
 *
 * @note Cache operations work on 4KB boundaries, horizontal coordinates ignored
 */
static inline void FlushCacheNeeded(void* pixels, uint32_t pitch, uint32_t y, uint32_t h) {
	uintptr_t pixptr = (uintptr_t)pixels;
	uintptr_t startaddress = (pixptr + pitch*y)&(~4095);
	uint32_t size = ALIGN4K(pixptr + pitch*(y+h)) - startaddress;
	if (size) MI_SYS_FlushInvCache((void*)startaddress, size);
}

/**
 * Hardware-accelerated blit using MI_GFX (replaces SDL_BlitSurface).
 *
 * Performs hardware-accelerated surface blitting with support for:
 * - Scaling (up or down)
 * - Pixel format conversion
 * - Rotation (90, 180, 270 degrees)
 * - Mirroring (horizontal, vertical, both)
 * - Alpha blending and color keying
 *
 * Falls back to SDL_BlitSurface if physical addresses are not available.
 * Automatically flushes CPU cache before DMA operations.
 *
 * @param src Source surface (must have pixelsPa set for hardware acceleration)
 * @param srcrect Source rectangle, or NULL for entire surface
 * @param dst Destination surface (must have pixelsPa set for hardware acceleration)
 * @param dstrect Destination rectangle, or NULL for entire surface
 * @param rotate Rotation: 0=none, 1=90°, 2=180°, 3=270°
 * @param mirror Mirror: 0=none, 1=horizontal, 2=vertical, 3=both
 * @param nowait 0=wait for completion, 1=don't wait (async operation)
 *
 * @note Respects SDL_SRCALPHA and SDL_SRCCOLORKEY surface flags
 */
static inline void GFX_BlitSurfaceExec(SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect, uint32_t rotate, uint32_t mirror, uint32_t nowait) {
	if ((src)&&(dst)&&(src->pixelsPa)&&(dst->pixelsPa)) {
		MI_GFX_Surface_t Src;
		MI_GFX_Surface_t Dst;
		MI_GFX_Rect_t SrcRect;
		MI_GFX_Rect_t DstRect;
		MI_GFX_Opt_t Opt;
		MI_U16 Fence;

		Src.phyAddr = src->pixelsPa;
		Src.u32Width = src->w;
		Src.u32Height = src->h;
		Src.u32Stride = src->pitch;
		Src.eColorFmt = GFX_ColorFmt(src);
		if (srcrect) {
			SrcRect.s32Xpos = srcrect->x;
			SrcRect.s32Ypos = srcrect->y;
			SrcRect.u32Width = srcrect->w;
			SrcRect.u32Height = srcrect->h;
		} else {
			SrcRect.s32Xpos = 0;
			SrcRect.s32Ypos = 0;
			SrcRect.u32Width = Src.u32Width;
			SrcRect.u32Height = Src.u32Height;
		}
		FlushCacheNeeded(src->pixels, src->pitch, SrcRect.s32Ypos, SrcRect.u32Height);

		Dst.phyAddr = dst->pixelsPa;
		Dst.u32Width = dst->w;
		Dst.u32Height = dst->h;
		Dst.u32Stride = dst->pitch;
		Dst.eColorFmt = GFX_ColorFmt(dst);
		if (dstrect) {
			DstRect.s32Xpos = dstrect->x;
			DstRect.s32Ypos = dstrect->y;
			if (dstrect->w|dstrect->h) {
				DstRect.u32Width = dstrect->w;
				DstRect.u32Height = dstrect->h;
			} else {
				DstRect.u32Width = SrcRect.u32Width;
				DstRect.u32Height = SrcRect.u32Height;
			}
		} else {
			DstRect.s32Xpos = 0;
			DstRect.s32Ypos = 0;
			DstRect.u32Width = Dst.u32Width;
			DstRect.u32Height = Dst.u32Height;
		}
		if (rotate & 1) FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Width);
		else FlushCacheNeeded(dst->pixels, dst->pitch, DstRect.s32Ypos, DstRect.u32Height);

		// Configure blending options
		memset(&Opt, 0, sizeof(Opt));

		// Handle alpha blending if requested
		if (src->flags & SDL_SRCALPHA) {
			Opt.eDstDfbBldOp = E_MI_GFX_DFB_BLD_INVSRCALPHA;
			if (src->format->alpha != SDL_ALPHA_OPAQUE) {
				// Global alpha: apply surface-level alpha to entire source
				Opt.u32GlobalSrcConstColor = (src->format->alpha << (src->format->Ashift - src->format->Aloss)) & src->format->Amask;
				Opt.eDFBBlendFlag = (MI_Gfx_DfbBlendFlags_e)
						   (E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY | E_MI_GFX_DFB_BLEND_COLORALPHA | E_MI_GFX_DFB_BLEND_ALPHACHANNEL);
			} else	Opt.eDFBBlendFlag = E_MI_GFX_DFB_BLEND_SRC_PREMULTIPLY;
		}

		// Handle color key (transparency) if requested
		if (src->flags & SDL_SRCCOLORKEY) {
			Opt.stSrcColorKeyInfo.bEnColorKey = TRUE;
			Opt.stSrcColorKeyInfo.eCKeyFmt = Src.eColorFmt;
			Opt.stSrcColorKeyInfo.eCKeyOp = E_MI_GFX_RGB_OP_EQUAL;
			Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorStart =
			Opt.stSrcColorKeyInfo.stCKeyVal.u32ColorEnd = src->format->colorkey;
		}
		Opt.eSrcDfbBldOp = E_MI_GFX_DFB_BLD_ONE;
		Opt.eRotate = (MI_GFX_Rotate_e)rotate;
		Opt.eMirror = (MI_GFX_Mirror_e)mirror;
		Opt.stClipRect.s32Xpos = dst->clip_rect.x;
		Opt.stClipRect.s32Ypos = dst->clip_rect.y;
		Opt.stClipRect.u32Width = dst->clip_rect.w;
		Opt.stClipRect.u32Height = dst->clip_rect.h;

		// Submit blit operation to hardware and optionally wait
		MI_GFX_BitBlit(&Src, &SrcRect, &Dst, &DstRect, &Opt, &Fence);
		if (!nowait) MI_GFX_WaitAllDone(FALSE, Fence);
	} else {
		// Fallback to software blit if physical addresses not available
		SDL_BlitSurface(src, srcrect, dst, dstrect);
	}
}

///////////////////////////////
// Lid Sensor (Hall Effect)
///////////////////////////////

#define LID_PATH "/sys/devices/soc0/soc/soc:hall-mh248/hallvalue"

/**
 * Initializes lid sensor support.
 *
 * Detects if device has a Hall effect sensor for lid open/close detection.
 * Currently only used on experimental Miyoo Mini variants.
 */
void PLAT_initLid(void) {
	lid.has_lid = exists(LID_PATH);
}

/**
 * Checks if lid state has changed.
 *
 * @param state Output: receives new lid state (1=open, 0=closed), may be NULL
 * @return 1 if state changed, 0 if unchanged or no lid sensor
 */
int PLAT_lidChanged(int* state) {
	if (lid.has_lid) {
		int lid_open = getInt(LID_PATH);
		if (lid_open!=lid.is_open) {
			lid.is_open = lid_open;
			if (state) *state = lid_open;
			return 1;
		}
	}
	return 0;
}

///////////////////////////////
// Input
///////////////////////////////

void PLAT_initInput(void) {
	// No platform-specific input initialization needed
}
void PLAT_quitInput(void) {
	// No platform-specific input cleanup needed
}

///////////////////////////////
// Video - ION Memory and MI_GFX
///////////////////////////////

/**
 * ION memory buffer with physical and virtual addresses.
 *
 * MI_GFX requires physically contiguous memory for DMA operations.
 * ION allocator provides both physical address (for hardware) and
 * virtual address (for CPU access).
 */
typedef struct HWBuffer {
	MI_PHY padd;   // Physical address (used by MI_GFX hardware)
	void* vadd;    // Virtual address (used by CPU)
} HWBuffer;

/**
 * Video subsystem context.
 *
 * Manages double-buffered rendering with ION memory allocation.
 */
static struct VID_Context {
	SDL_Surface* video;    // SDL framebuffer surface
	SDL_Surface* screen;   // Software rendering surface (may be same as video)
	HWBuffer buffer;       // ION-allocated buffer for double buffering

	int page;              // Current backbuffer page (0 or 1)
	int width;             // Current rendering width
	int height;            // Current rendering height
	int pitch;             // Current pitch in bytes

	int direct;            // 1 if rendering directly to video, 0 if using intermediate buffer
	int cleared;           // 1 if clear is deferred until offscreen
} vid;

#define MODES_PATH "/sys/class/graphics/fb0/modes"

/**
 * Checks if framebuffer supports a specific video mode.
 *
 * @param path Path to modes sysfs file
 * @param mode Mode string to search for (e.g., "752x560p")
 * @return 1 if mode is supported, 0 otherwise
 */
static int hasMode(const char *path, const char *mode) {
    FILE *f = fopen(path, "r"); if (!f) return 0;
    char s[128];
    while (fgets(s, sizeof s, f)) if (strstr(s, mode)) return fclose(f), 1;
    fclose(f); return 0;
}

/**
 * Initializes video subsystem with hardware variant detection.
 *
 * Detects hardware variant (Mini vs Plus, 480p vs 560p) and allocates
 * ION memory for double-buffered rendering. The Plus variant is identified
 * by the presence of axp_test (AXP223 PMIC utility). The 560p mode requires
 * both hardware support and user opt-in via enable-560p file.
 *
 * Memory Layout:
 * - Allocates 2 pages (PAGE_COUNT=2) of PAGE_SIZE each
 * - Each page is 4KB-aligned for cache operations
 * - Page 0 and 1 are swapped on each flip for double buffering
 *
 * @return Surface for rendering (either direct video or intermediate screen)
 *
 * @note Sets global flags: is_plus, is_560p
 */
SDL_Surface* PLAT_initVideo(void) {
	// Detect hardware variants
	is_plus = exists("/customer/app/axp_test");
	is_560p = hasMode(MODES_PATH, "752x560p") && exists(USERDATA_PATH "/enable-560p");
	LOG_info("is 560p: %i\n", is_560p);

	// Initialize SDL with custom battery handling
	putenv("SDL_HIDE_BATTERY=1"); // using MiniUI's custom SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	SDL_ShowCursor(0);

	vid.video = SDL_SetVideoMode(FIXED_WIDTH, FIXED_HEIGHT, FIXED_DEPTH, SDL_SWSURFACE);

	// Allocate ION memory for double buffering (physically contiguous for MI_GFX)
	int buffer_size = ALIGN4K(PAGE_SIZE) * PAGE_COUNT;
	MI_SYS_MMA_Alloc(NULL, ALIGN4K(buffer_size), &vid.buffer.padd);
	MI_SYS_Mmap(vid.buffer.padd, ALIGN4K(buffer_size), &vid.buffer.vadd, true);

	// Initialize rendering state
	vid.page = 1;
	vid.direct = 1;  // Start in direct mode (no intermediate buffer)
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;
	vid.cleared = 0;

	// Create screen surface backed by ION memory
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE),vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
	vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	return vid.direct ? vid.video : vid.screen;
}

/**
 * Shuts down video subsystem and frees ION memory.
 */
void PLAT_quitVideo(void) {
	SDL_FreeSurface(vid.screen);

	MI_SYS_Munmap(vid.buffer.vadd, ALIGN4K(PAGE_SIZE));
	MI_SYS_MMA_Free(vid.buffer.padd);

	SDL_Quit();
}

/**
 * Clears current video buffer using hardware memset.
 *
 * Uses MI_SYS_MemsetPa for fast hardware-accelerated clearing of the
 * physically contiguous buffer. Cache is flushed before the operation.
 *
 * @param screen Surface to clear (unused, operates on current vid.page)
 *
 * @note Direct memset() on screen->pixels can cause crashes with ION memory
 */
void PLAT_clearVideo(SDL_Surface* screen) {
	MI_SYS_FlushInvCache(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE), ALIGN4K(PAGE_SIZE));
	MI_SYS_MemsetPa(vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE), 0, PAGE_SIZE);
	SDL_FillRect(screen, NULL, 0);
}

/**
 * Clears both front and back buffers.
 *
 * Clears backbuffer immediately and defers frontbuffer clear until next
 * flip to avoid visible artifacts.
 */
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // clear backbuffer
	vid.cleared = 1; // defer clearing frontbuffer until offscreen
}

/**
 * Sets vsync behavior via custom SDL environment variables.
 *
 * Controls tearing vs flicker tradeoffs:
 * - VSYNC_OFF: No synchronization, lowest latency, potential tearing
 * - VSYNC_LENIENT: Blocking but no flip wait, tear-free but may flicker
 * - VSYNC_STRICT: Full synchronization, eliminates flicker, may introduce tearing
 *
 * @param vsync VSYNC_OFF, VSYNC_LENIENT, or VSYNC_STRICT
 *
 * @note Trade-offs are hardware-specific and somewhat counterintuitive
 */
void PLAT_setVsync(int vsync) {
	if (vsync==VSYNC_OFF) {
		putenv("GFX_FLIPWAIT=0");
		putenv("GFX_BLOCKING=0");
	}
	else if (vsync==VSYNC_LENIENT) {
		putenv("GFX_FLIPWAIT=0");
		putenv("GFX_BLOCKING=1");
	}
	else if (vsync==VSYNC_STRICT) {
		putenv("GFX_FLIPWAIT=1");
		putenv("GFX_BLOCKING=1");
	}
	SDL_GetVideoInfo();  // Apply environment changes
}

/**
 * Resizes rendering surface (switches between direct and indirect rendering).
 *
 * If requested size matches native resolution, renders directly to framebuffer.
 * Otherwise, creates an intermediate surface and uses MI_GFX to scale during flip.
 *
 * @param w Width in pixels
 * @param h Height in pixels
 * @param pitch Pitch in bytes
 * @return Surface to render to (either vid.video or vid.screen)
 *
 * @note Recreates vid.screen if switching to indirect mode
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	// Determine if we can render directly to framebuffer
	vid.direct = w==FIXED_WIDTH && h==FIXED_HEIGHT && pitch==FIXED_PITCH;
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;

	if (vid.direct) {
		memset(vid.video->pixels, 0, vid.pitch * vid.height);
	}
	else {
		// Recreate screen surface with new dimensions
		vid.screen->pixels = NULL;
		vid.screen->pixelsPa = NULL; // Prevent custom SDL from freeing ION memory
		SDL_FreeSurface(vid.screen);

		vid.screen = SDL_CreateRGBSurfaceFrom(vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE),vid.width,vid.height,FIXED_DEPTH,vid.pitch,RGBA_MASK_AUTO);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
		memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	}

	return vid.direct ? vid.video : vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// No clipping support needed
}
void PLAT_setNearestNeighbor(int enabled) {
	// Filtering handled by scaler selection
}

///////////////////////////////
// Pixel Effects and Scaling
///////////////////////////////

static int next_effect = EFFECT_NONE;  // Effect to apply on next render
static int effect_type = EFFECT_NONE;  // Currently active effect

/**
 * Forces scaler reload when sharpness settings change.
 *
 * Triggered when scaling factor changes to ensure effect is reapplied
 * with the new scaler.
 *
 * @param sharpness Sharpness value (unused, but triggers reload)
 */
void PLAT_setSharpness(int sharpness) {
	if (effect_type>=EFFECT_NONE) next_effect = effect_type;
	effect_type = -1;  // Force reload
}

/**
 * Sets pixel effect for next render.
 *
 * @param effect EFFECT_NONE, EFFECT_LINE, or EFFECT_GRID
 */
void PLAT_setEffect(int effect) {
	next_effect = effect;
}

/**
 * Waits for remaining frame time to maintain target framerate.
 *
 * @param remaining Milliseconds remaining in frame
 */
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

/**
 * Selects appropriate scaler function based on scale factor and effect.
 *
 * @param renderer Renderer context containing scale factor
 * @return Function pointer to scaler implementation
 */
scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	// Scanline effect scalers
	if (effect_type==EFFECT_LINE) {
		switch (renderer->scale) {
			case 4:  return scale4x_line;
			case 3:  return scale3x_line;
			case 2:  return scale2x_line;
			default: return scale1x_line;
		}
	}
	// Grid effect scalers
	else if (effect_type==EFFECT_GRID) {
		switch (renderer->scale) {
			case 3:  return scale3x_grid;
			case 2:  return scale2x_grid;
		}
	}

	// Default nearest-neighbor scalers (NEON-optimized)
	switch (renderer->scale) {
		case 6:  return scale6x6_n16;
		case 5:  return scale5x5_n16;
		case 4:  return scale4x4_n16;
		case 3:  return scale3x3_n16;
		case 2:  return scale2x2_n16;
		default: return scale1x1_n16;
	}
}

/**
 * Blits scaled renderer output to destination surface.
 *
 * Applies the current pixel effect and scaler, reloading if effect changed.
 * Uses NEON-optimized scalers from scaler.c for performance.
 *
 * @param renderer Renderer context with source/dest buffers and dimensions
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	// Reload scaler if effect changed
	if (effect_type!=next_effect) {
		effect_type = next_effect;
		renderer->blit = PLAT_getScaler(renderer);
	}

	// Calculate destination pointer with offset
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);
	((scaler_t)renderer->blit)(renderer->src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}

/**
 * Flips display buffer (presents rendered frame).
 *
 * In indirect mode, uses MI_GFX to scale from intermediate buffer to framebuffer.
 * Implements double buffering by swapping page on each flip.
 * Handles deferred clear if PLAT_clearAll() was called.
 *
 * @param IGNORED Surface parameter (unused)
 * @param sync Sync parameter (unused, vsync controlled via PLAT_setVsync)
 */
void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	// Scale to framebuffer if using intermediate buffer
	if (!vid.direct) GFX_BlitSurfaceExec(vid.screen, NULL, vid.video, NULL, 0,0,1);

	SDL_Flip(vid.video);

	// Swap to other page for double buffering
	if (!vid.direct) {
		vid.page ^= 1;
		vid.screen->pixels = vid.buffer.vadd + ALIGN4K(vid.page*PAGE_SIZE);
		vid.screen->pixelsPa = vid.buffer.padd + ALIGN4K(vid.page*PAGE_SIZE);
	}

	// Complete deferred clear if pending
	if (vid.cleared) {
		PLAT_clearVideo(vid.screen);
		vid.cleared = 0;
	}
}

///////////////////////////////
// Overlay (On-Screen Display)
///////////////////////////////

#define OVERLAY_WIDTH PILL_SIZE
#define OVERLAY_HEIGHT PILL_SIZE
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000  // ARGB

static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

/**
 * Initializes overlay surface for on-screen indicators.
 *
 * @return Overlay surface for rendering status indicators
 */
SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}

/**
 * Cleans up overlay surface.
 */
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}

/**
 * Enables or disables overlay display.
 *
 * @param enable 1 to enable, 0 to disable
 *
 * @note Currently no-op, overlay always composited by upper layers
 */
void PLAT_enableOverlay(int enable) {
	// Overlay composited in software, no hardware control needed
}

///////////////////////////////
// Power Management - AXP223 PMIC (Plus Model)
///////////////////////////////

// I2C device and address for AXP223 power management IC
#define	AXPDEV	"/dev/i2c-1"
#define	AXPID	(0x34)

/**
 * Writes a register on the AXP223 PMIC (Miyoo Mini Plus only).
 *
 * Used for power control including shutdown. Key registers:
 * - 0x32 bit7: Shutdown control
 *
 * @param address Register address
 * @param val Value to write
 * @return 0 on success, -1 on failure
 *
 * @note Only available on Mini Plus with AXP223 PMIC
 */
int axp_write(unsigned char address, unsigned char val) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[2];
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	buf[0] = address;
	buf[1] = val;
	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	packets.nmsgs = 1;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0) return -1;
	return 0;
}

/**
 * Reads a register from the AXP223 PMIC (Miyoo Mini Plus only).
 *
 * Key registers:
 * - 0x00: Power status
 *   - bit7: ACIN presence (1=connected)
 *   - bit6: ACIN usable
 *   - bit4: VBUS usable
 *   - bit2: Battery current direction (0=discharging, 1=charging)
 *   - bit0: Boot source (ACIN or VBUS)
 * - 0x01: Charge status
 *   - bit6: Charging indication (1=charging, 0=not charging/finished)
 * - 0xB9: Battery percentage (mask with 0x7F)
 *
 * @param address Register address
 * @return Register value (0-255) on success, -1 on failure
 *
 * @note Only available on Mini Plus with AXP223 PMIC
 */
int axp_read(unsigned char address) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char val;
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;
	msg[1].addr = AXPID;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &val;

	packets.nmsgs = 2;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if(ret < 0) return -1;
	return val;
}

///////////////////////////////
// Battery and Power Status
///////////////////////////////

static int online = 0;  // WiFi connection status

/**
 * Gets battery charge level and charging status.
 *
 * On Mini Plus: Reads charging status from AXP223 PMIC register 0x00, bit 2
 * On Mini: Reads charging status from GPIO 59
 *
 * Battery level is read from /tmp/battery (updated by system daemon) and
 * quantized to 6 levels to reduce visual noise: 100%, 80%, 60%, 40%, 20%, 10%
 *
 * Also updates WiFi connection status as a side effect.
 *
 * @param is_charging Output: receives charging status (1=charging, 0=not)
 * @param charge Output: receives battery level (10, 20, 40, 60, 80, 100)
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// Check charging status (hardware-dependent)
	*is_charging = is_plus ? (axp_read(0x00) & 0x4) > 0 : getInt("/sys/devices/gpiochip0/gpio/gpio59/value");

	// Read battery percentage from system daemon
	int i = getInt("/tmp/battery"); // 0-100

	// Quantize to reduce visual noise in battery indicator
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// Update WiFi connection status
	char status[16];
	getFile("/sys/class/net/wlan0/operstate", status,16);
	online = prefixMatch("up", status);
}

/**
 * Enables or disables the backlight via PWM control.
 *
 * Enable: Configures PWM0 for backlight control
 * Disable: Configures GPIO4 to turn off backlight
 *
 * @param enable 1 to enable backlight, 0 to disable
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		// Restore PWM control
		putInt("/sys/class/gpio/gpio4/value", 1);
		putInt("/sys/class/gpio/unexport", 4);
		putInt("/sys/class/pwm/pwmchip0/export", 0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",0);
		putInt("/sys/class/pwm/pwmchip0/pwm0/enable",1);
	}
	else {
		// Use GPIO to turn off backlight
		putInt("/sys/class/gpio/export", 4);
		putFile("/sys/class/gpio/gpio4/direction", "out");
		putInt("/sys/class/gpio/gpio4/value", 0);
	}
}

/**
 * Performs graceful system shutdown.
 *
 * Sequence:
 * 1. Wait 2 seconds (debounce)
 * 2. Mute audio
 * 3. Disable backlight
 * 4. Shut down all subsystems
 * 5. Execute system shutdown command
 * 6. Pause indefinitely (wait for power off)
 */
void PLAT_powerOff(void) {
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();

	system("shutdown");
	while (1) pause();  // Wait for kernel to power off
}

///////////////////////////////
// CPU Speed Control
///////////////////////////////

/**
 * Sets CPU frequency using overclock.elf utility.
 *
 * Frequency mapping:
 * - CPU_SPEED_MENU:        504 MHz (power saving for menus)
 * - CPU_SPEED_POWERSAVE:  1104 MHz (light games)
 * - CPU_SPEED_NORMAL:     1296 MHz (most games)
 * - CPU_SPEED_PERFORMANCE: 1488 MHz (demanding games)
 *
 * @param speed One of the CPU_SPEED_* constants
 */
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

///////////////////////////////
// Rumble/Vibration
///////////////////////////////

/**
 * Controls vibration motor via GPIO 48.
 *
 * Only updates GPIO state when value changes to minimize I/O overhead.
 *
 * @param strength 0 to disable, non-zero to enable
 */
void PLAT_setRumble(int strength) {
    static char lastvalue = 0;
    const char str_export[2] = "48";
    const char str_direction[3] = "out";
    char value[1];
    int fd;

    value[0] = (strength == 0 ? 0x31 : 0x30); // '0' (off) : '1' (on)
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

///////////////////////////////
// Audio Configuration
///////////////////////////////

/**
 * Selects audio sample rate.
 *
 * @param requested Requested sample rate (Hz)
 * @param max Maximum supported sample rate (Hz)
 * @return Sample rate to use (always returns max)
 */
int PLAT_pickSampleRate(int requested, int max) {
	return max;
}

///////////////////////////////
// Device Identification
///////////////////////////////

/**
 * Returns human-readable device model string.
 *
 * Detects between:
 * - Miyoo Mini Flip (MY285 - clamshell variant)
 * - Miyoo Mini Plus (Plus model with AXP223)
 * - Miyoo Mini (original model)
 *
 * @return Model string (do not free)
 */
char* PLAT_getModel(void) {
	char* model = getenv("MY_MODEL");
	if (exactMatch(model,"MY285")) return "Miyoo Mini Flip";
	else if (is_plus) return "Miyoo Mini Plus";
	else return "Miyoo Mini";
}

/**
 * Checks if device is connected to WiFi.
 *
 * @return 1 if online, 0 if offline
 *
 * @note Status updated as side effect of PLAT_getBatteryStatus()
 */
int PLAT_isOnline(void) {
	return online;
}