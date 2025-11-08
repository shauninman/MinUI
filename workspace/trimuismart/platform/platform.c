/**
 * trimuismart/platform/platform.c - Trimui Smart platform implementation
 *
 * Provides platform-specific implementations for the Trimui Smart handheld
 * gaming device powered by the Allwinner F1C100s SoC with Display Engine 2.0.
 *
 * Hardware Overview:
 * - SoC: Allwinner F1C100s (ARM926EJ-S, single core)
 * - Display: 320x240 QVGA LCD (no scaling needed)
 * - Display Engine: Allwinner DE2 with multi-layer composition
 * - Memory: ION allocator for contiguous physical memory
 * - Input: SDL keyboard events (hybrid SDL/evdev)
 * - Battery: LRADC (Low Resolution ADC) for battery monitoring
 *
 * Display Architecture (Allwinner DE2):
 * The Display Engine 2.0 provides hardware layer composition with:
 * - Multiple channels (0-2) each supporting multiple layers
 * - Channel 0/1: Video scaling support, no alpha blending
 * - Channel 2: UI overlay with alpha blending, no scaling
 * - Independent z-ordering per layer
 * - Hardware YUV to RGB conversion
 * - Hardware rotation (used for portrait display on landscape screen)
 *
 * Memory Architecture:
 * Uses ION (In-kernel Out-of-Nexus) memory allocator for DMA buffers:
 * - Provides physically contiguous memory for display hardware
 * - Returns both physical address (for DMA) and virtual address (for CPU)
 * - Required for hardware layer composition
 *
 * Layer Configuration:
 * - FB_CH (Channel 0): Stock framebuffer (disabled by MinUI)
 * - SCALER_CH (Channel 1): Main game display with rotation and scaling
 * - OVERLAY_CH (Channel 2): UI overlay (currently unused)
 * - Double buffering via page flipping
 *
 * Display Pipeline:
 * 1. Software renders to intermediate buffer (portrait orientation)
 * 2. rotate_16bpp() rotates 90 degrees to landscape
 * 3. Hardware scaler upscales to 320x240 screen
 * 4. Hardware compositor blends layers
 * 5. Output to LCD panel
 *
 * @note Rotation is needed because display is physically landscape but
 *       MinUI renders in portrait orientation for consistent UI across devices
 * @note Direct memory mapping of display registers allows zero-copy buffer flips
 */

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
// Input Management
///////////////////////////////

/**
 * Initializes input subsystem.
 *
 * Input is handled by SDL keyboard events and keymon daemon.
 * No platform-specific initialization required.
 */
void PLAT_initInput(void) {
	// Input handled by SDL and keymon daemon
}

/**
 * Cleans up input subsystem.
 *
 * No platform-specific cleanup required.
 */
void PLAT_quitInput(void) {
	// No cleanup needed
}

///////////////////////////////
// Display Layer Configuration
///////////////////////////////

/**
 * Hardware display layer channel and z-order assignments.
 *
 * Allwinner DE2 supports multiple channels, each with multiple layers.
 * Channels 0-1 support scaling but not alpha blending.
 * Channel 2 supports alpha blending but not scaling.
 *
 * Layer Management Strategy:
 * - Disable stock framebuffer (DEF_FB_CH/DEF_FB_LAYER)
 * - Use SCALER_CH for main display with hardware scaling
 * - OVERLAY_CH reserved for future UI overlay support
 * - Z-order determines compositing order (higher = on top)
 */
#define FB_CH (0)              // Unused fallback channel
#define FB_LAYER (0)           // Unused fallback layer
#define FB_ZORDER (0)          // Lowest z-order
#define SCALER_CH (1)          // Main display channel (supports scaling)
#define SCALER_LAYER (0)       // Layer 0 of scaler channel
#define SCALER_ZORDER (10)     // Base z-order for game display
#define OVERLAY_CH (2)         // UI overlay channel (supports alpha)
#define OVERLAY_LAYER (0)      // Layer 0 of overlay channel
#define OVERLAY_ZORDER (11)    // Above game display
#define DEF_FB_CH (2)          // Stock framebuffer channel (to disable)
#define DEF_FB_LAYER (0)       // Stock framebuffer layer

/**
 * Display Engine 2.0 memory-mapped register addresses.
 *
 * Direct memory mapping allows zero-copy page flipping by writing
 * the physical buffer address directly to hardware registers.
 */
#define DE			(0x01000000)                                   // Display Engine base
#define RT_MIXER0		(DE+0x00100000)                                // RT Mixer 0 (real-time compositor)
#define OVL_V			(RT_MIXER0+0x2000+(SCALER_CH*0x1000))          // Video overlay for scaler channel
#define OVL_V_TOP_LADD0		(0x18+(SCALER_LAYER*0x30))                     // Top layer address register offset

/**
 * ION memory allocation information.
 *
 * ION (In-kernel Out-of-Nexus) allocator provides physically contiguous
 * memory required by the display hardware for DMA operations.
 *
 * @field size Size of allocation in bytes
 * @field handle Opaque ION handle for kernel tracking
 * @field fd File descriptor for mmap operations
 * @field padd Physical address (for DMA by display hardware)
 * @field vadd Virtual address (for CPU access)
 */
typedef struct ion_alloc_info {
	uint32_t		size;      // Allocation size in bytes
	struct ion_handle	*handle;   // Kernel handle for this allocation
	int			fd;        // File descriptor for mmap
	void*			padd;      // Physical address (for hardware)
	void*			vadd;      // Virtual address (for CPU)
} ion_alloc_info_t;

///////////////////////////////
// ION Memory Management
///////////////////////////////

/**
 * Allocates physically contiguous memory via ION allocator.
 *
 * ION (In-kernel Out-of-Nexus) provides DMA-capable memory required by
 * Allwinner DE2 display hardware. The allocation process:
 * 1. Allocate from DMA heap (physically contiguous)
 * 2. Get physical address (for display hardware DMA)
 * 3. Create file descriptor for mmap
 * 4. Map to virtual address space (for CPU access)
 *
 * Physical address is required because display hardware uses DMA to
 * read pixel data directly from memory. Virtual addresses cannot be
 * used by hardware.
 *
 * @param ion_fd File descriptor from open("/dev/ion")
 * @param info ION allocation info (input: size; output: handle, fd, padd, vadd)
 *
 * @note info->size must be set before calling
 * @note Allocation is page-aligned
 * @note Both physical and virtual addresses are returned
 */
void ion_alloc(int ion_fd, ion_alloc_info_t* info) {
	struct ion_allocation_data	iad;
	struct ion_fd_data		ifd;
	struct ion_custom_data		icd;
	sunxi_phys_data			spd;

	// Allocate from DMA heap with page alignment
	iad.len = info->size;
	iad.align = sysconf(_SC_PAGESIZE);
	iad.heap_id_mask = ION_HEAP_TYPE_DMA_MASK;
	iad.flags = 0;
	if (ioctl(ion_fd, ION_IOC_ALLOC, &iad)<0) fprintf(stderr, "ION_ALLOC failed %s\n",strerror(errno));

	// Get physical address (Allwinner-specific ioctl)
	icd.cmd = ION_IOC_SUNXI_PHYS_ADDR;
	icd.arg = (uintptr_t)&spd;
	spd.handle = iad.handle;
	if (ioctl(ion_fd, ION_IOC_CUSTOM, &icd)<0) fprintf(stderr, "ION_GET_PHY failed %s\n",strerror(errno));

	// Create file descriptor for mmap
	ifd.handle = iad.handle;
	if (ioctl(ion_fd, ION_IOC_MAP, &ifd)<0) fprintf(stderr, "ION_MAP failed %s\n",strerror(errno));

	// Return both physical (for hardware) and virtual (for CPU) addresses
	info->handle = iad.handle;
	info->fd = ifd.fd;
	info->padd = (void*)spd.phys_addr;
	info->vadd = mmap(0, info->size, PROT_READ|PROT_WRITE, MAP_SHARED, info->fd, 0);
	fprintf(stderr, "allocated padd: 0x%x vadd: 0x%x size: 0x%x\n", (uintptr_t)info->padd, (uintptr_t)info->vadd, info->size);
}

/**
 * Frees ION-allocated memory.
 *
 * Releases all resources associated with an ION allocation:
 * 1. Unmap virtual address
 * 2. Close file descriptor
 * 3. Free ION handle
 *
 * @param ion_fd File descriptor from open("/dev/ion")
 * @param info ION allocation info to free
 */
void ion_free(int ion_fd, ion_alloc_info_t* info) {
	struct ion_handle_data	 ihd;
	munmap(info->vadd, info->size);
	close(info->fd);
	ihd.handle = info->handle;
	if (ioctl(ion_fd, ION_IOC_FREE, &ihd)<0) fprintf(stderr, "ION_FREE failed %s\n",strerror(errno));
}

///////////////////////////////
// Pixel Rotation
///////////////////////////////

/**
 * Rotates 16-bit framebuffer 90 degrees clockwise with 180-degree flip.
 *
 * The Trimui Smart display is physically landscape but MinUI renders in
 * portrait orientation for UI consistency across devices. This function
 * performs a combined 90-degree rotation and 180-degree flip to correct
 * the orientation.
 *
 * Transform: (x,y) -> (y, width-x-1) then flip vertically/horizontally
 * Result: Portrait buffer becomes landscape with correct orientation
 *
 * @param src Source buffer (portrait orientation)
 * @param dst Destination buffer (landscape orientation)
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch (row stride) in bytes
 * @param dp Destination pitch (row stride) in bytes
 *
 * @note Uses __restrict to allow compiler optimizations
 * @note Operates on RGB565 pixels (16-bit)
 */
void rotate_16bpp(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dp) {
	uint16_t* s = (uint16_t*)src;
	uint16_t* d = (uint16_t*)dst;
	int spx = sp/FIXED_BPP;  // Source pitch in pixels
	int dpx = dp/FIXED_BPP;  // Destination pitch in pixels

	// Rotate 90 degrees clockwise + flip 180 degrees
	// Reads from bottom-right to top-left, writes column-wise
	for (int y=0; y<sh; y++) {
		for (int x=0; x<sw; x++) {
			*(d + x * dpx + (dpx - y - 1)) = *(s + (sh-1-y) * spx + (sw-1-x));
		}
	}
}

///////////////////////////////
// Video Context
///////////////////////////////

/**
 * Video subsystem state.
 *
 * Manages the complete display pipeline including SDL surfaces,
 * ION-allocated DMA buffers, display layer configurations, and
 * hardware register mappings.
 *
 * Surface hierarchy:
 * - video: Stock SDL surface (640x480 rotated, unused)
 * - screen: Intermediate rendering target (variable size)
 * - buffer: ION-allocated double buffer for display hardware
 * - special: Temporary rotation buffer for scaled content
 *
 * File descriptors:
 * - disp_fd: /dev/disp (display layer control)
 * - fb_fd: /dev/fb0 (vsync and stock framebuffer)
 * - ion_fd: /dev/ion (memory allocation)
 * - mem_fd: /dev/mem (register mapping)
 *
 * @note mem_map points to display engine registers for zero-copy flips
 */
static struct VID_Context {
	SDL_Surface* video;      // Stock SDL surface (unused)
	SDL_Surface* buffer;     // Double buffer (ION-allocated, for hardware)
	SDL_Surface* screen;     // Intermediate render target
	SDL_Surface* special;    // Rotation buffer for scaled content

	GFX_Renderer* renderer;  // Current active renderer (if any)

	// File descriptors
	int disp_fd;             // Display layer control
	int fb_fd;               // Framebuffer (for vsync)
	int ion_fd;              // ION memory allocator
	int mem_fd;              // Physical memory access
	uint32_t* mem_map;       // Mapped display registers

	// Display layer configurations
	disp_layer_config fb_config;      // Stock framebuffer layer
	disp_layer_config buffer_config;  // Main display layer (scaler channel)
	disp_layer_config screen_config;  // Unused
	ion_alloc_info_t buffer_info;     // Double buffer allocation
	ion_alloc_info_t screen_info;     // Screen buffer allocation

	// Rotation offsets (for scaled content)
	int rotated_pitch;       // Pitch after rotation
	int rotated_offset;      // Destination offset in rotated buffer
	int source_offset;       // Source offset before rotation

	// Current display configuration
	int page;                // Current page (0 or 1) for double buffering
	int width;               // Current screen width
	int height;              // Current screen height
	int pitch;               // Current screen pitch

	// State flags
	int cleared;             // Screen has been cleared
	int resized;             // Screen has been resized (update hardware config)
} vid;
static int _;  // Dummy variable for ioctl calls

void ADC_init();
void ADC_quit();

///////////////////////////////
// Video Initialization and Management
///////////////////////////////

/**
 * Initializes video subsystem with Allwinner DE2 multi-layer display.
 *
 * Setup process:
 * 1. Initialize SDL video (creates stock framebuffer)
 * 2. Open display, framebuffer, ION, and /dev/mem devices
 * 3. Map display engine registers for zero-copy buffer flips
 * 4. Disable stock framebuffer layer (DEF_FB_CH)
 * 5. Allocate ION buffers for screen and double-buffered display
 * 6. Configure scaler channel layer (SCALER_CH) with:
 *    - RGB565 format
 *    - Hardware scaling support
 *    - Double buffering
 *    - Portrait-to-landscape rotation
 *
 * Display configuration:
 * - Channel: SCALER_CH (supports scaling)
 * - Layer: SCALER_LAYER
 * - Z-order: SCALER_ZORDER (below potential overlays)
 * - Format: RGB565 (16-bit color)
 * - Size: 320x240 (physical screen) from 240x320 (portrait buffer)
 * - Buffers: 2 pages for double buffering
 *
 * @return SDL surface for rendering (screen buffer)
 *
 * @note ADC (battery monitoring) initialized first
 * @note Stock framebuffer disabled to prevent conflicts
 * @note Physical addresses required for display hardware DMA
 */
SDL_Surface* PLAT_initVideo(void) {
	ADC_init();
	
	// Initialize SDL (creates stock framebuffer, but we'll bypass it)
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	vid.video = SDL_SetVideoMode(FIXED_HEIGHT, FIXED_WIDTH, FIXED_DEPTH, SDL_HWSURFACE);
	memset(vid.video->pixels, 0, FIXED_SIZE);

	// Open hardware devices
	vid.disp_fd = open("/dev/disp", O_RDWR);      // Display layer control
	vid.fb_fd = open("/dev/fb0", O_RDWR);         // For vsync timing
	vid.ion_fd = open("/dev/ion", O_RDWR);        // ION memory allocator
	vid.mem_fd = open("/dev/mem", O_RDWR);        // Physical memory access

	// Map display engine video overlay registers for direct buffer address updates
	vid.mem_map = mmap(0, sysconf(_SC_PAGESIZE), PROT_READ|PROT_WRITE, MAP_SHARED, vid.mem_fd, OVL_V);

	memset(&vid.fb_config, 0, sizeof(disp_layer_config));
	memset(&vid.buffer_config, 0, sizeof(disp_layer_config));

	// Wait for vsync to avoid tearing during layer reconfiguration
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);

	// Save and disable stock framebuffer layer
	// We'll use our own ION-allocated buffers instead
	uint32_t args[4] = {0, (uintptr_t)&vid.fb_config, 1, 0};
	vid.fb_config.channel = DEF_FB_CH;
	vid.fb_config.layer_id = DEF_FB_LAYER;
	ioctl(vid.disp_fd, DISP_LAYER_GET_CONFIG, args);

	vid.fb_config.enable = 0;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	// Allocate intermediate rendering buffer (portrait orientation)
	// This is what applications render to before rotation
	vid.page = 1;
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;

	vid.screen_info.size = PAGE_SIZE;
	ion_alloc(vid.ion_fd, &vid.screen_info);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.screen_info.vadd, vid.width, vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_565);

	// Allocate double-buffered display layer (landscape orientation after rotation)
	// Channel 1 supports hardware scaling but not alpha blending
	vid.buffer_info.size = PAGE_SIZE * PAGE_COUNT;
	ion_alloc(vid.ion_fd, &vid.buffer_info);

	vid.buffer = SDL_CreateRGBSurfaceFrom(vid.buffer_info.vadd + vid.page * PAGE_SIZE, PAGE_HEIGHT, PAGE_WIDTH, FIXED_DEPTH, PAGE_HEIGHT*FIXED_BPP, RGBA_MASK_565);

	// Configure scaler channel layer for main display
	vid.buffer_config.channel = SCALER_CH;
	vid.buffer_config.layer_id = SCALER_LAYER;
	vid.buffer_config.enable = 1;
	vid.buffer_config.info.fb.format = DISP_FORMAT_RGB_565;
	vid.buffer_config.info.fb.addr[0] = (uintptr_t)vid.buffer_info.padd;  // Physical address for DMA
	vid.buffer_config.info.fb.size[0].width  = vid.height;  // Swapped for rotation
	vid.buffer_config.info.fb.size[0].height = vid.width;   // Swapped for rotation
	vid.buffer_config.info.mode = LAYER_MODE_BUFFER;
	vid.buffer_config.info.zorder = SCALER_ZORDER;
	vid.buffer_config.info.alpha_mode = 0;  // 0: pixel alpha; 1: global alpha; 2: global+pixel
	vid.buffer_config.info.alpha_value = 0;
	vid.buffer_config.info.screen_win.x = 0;
	vid.buffer_config.info.screen_win.y = 0;
	vid.buffer_config.info.screen_win.width  = vid.height;
	vid.buffer_config.info.screen_win.height = vid.width;
	vid.buffer_config.info.fb.pre_multiply = 0;
	// Crop region in fixed-point format (bits [63:32] = integer, [31:0] = fraction)
	vid.buffer_config.info.fb.crop.x = (int64_t)0 << 32;
	vid.buffer_config.info.fb.crop.y = (int64_t)0 << 32;
	vid.buffer_config.info.fb.crop.width  = (int64_t)vid.height << 32;
	vid.buffer_config.info.fb.crop.height = (int64_t)vid.width  << 32;

	// Apply scaler channel configuration
	args[1] = (uintptr_t)&vid.buffer_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	// Wait for hardware to apply configuration
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);

	// Suppress Trimui's verbose SDL debug output
	puts("--------------------------------"); fflush(stdout);

	return vid.screen;
}

/**
 * Shuts down video subsystem and restores stock framebuffer.
 *
 * Cleanup process:
 * 1. Wait for vsync to complete current frame
 * 2. Clear display to avoid artifacts
 * 3. Free SDL surfaces
 * 4. Disable custom display layers
 * 5. Re-enable stock framebuffer layer
 * 6. Free ION allocations
 * 7. Unmap and close all device file descriptors
 * 8. Shutdown SDL
 *
 * @note Restores system to state before PLAT_initVideo
 */
void PLAT_quitVideo(void) {
	puts("--------------------------------"); fflush(stdout);

	ADC_quit();

	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);

	memset(vid.video->pixels, 0, FIXED_SIZE);

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);

	// Disable all custom layers
	vid.fb_config.enable = vid.buffer_config.enable = 0;
	uint32_t args[4] = {0, (uintptr_t)&vid.fb_config, 1, 0};
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	args[1] = (uintptr_t)&vid.buffer_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	// Re-enable stock framebuffer layer
	vid.fb_config.enable = 1;
	vid.fb_config.channel = DEF_FB_CH;
	vid.fb_config.layer_id = DEF_FB_LAYER;
	args[1] = (uintptr_t)&vid.fb_config;
	ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);

	// Free ION allocations and close devices
	ion_free(vid.ion_fd, &vid.buffer_info);
	munmap(vid.mem_map, sysconf(_SC_PAGESIZE));
	close(vid.mem_fd);
	close(vid.ion_fd);
	close(vid.fb_fd);
	close(vid.disp_fd);

	SDL_Quit();
}

/**
 * Clears video buffers to black.
 *
 * Clears both the screen buffer (if not already cleared) and the
 * current page of the double buffer.
 *
 * @param IGNORED Unused (kept for API compatibility)
 */
void PLAT_clearVideo(SDL_Surface* IGNORED) {
	if (!vid.cleared) memset(vid.screen->pixels, 0, vid.pitch * vid.height);
	memset(vid.buffer->pixels, 0, PAGE_SIZE);
}

/**
 * Marks screen as cleared and clears backbuffer.
 *
 * Sets cleared flag to skip redundant clears on next frame.
 */
void PLAT_clearAll(void) {
	vid.cleared = 1;
	PLAT_clearVideo(vid.buffer);
}

/**
 * Configures vsync mode.
 *
 * Vsync is always enabled on Trimui Smart (hardware requirement).
 *
 * @param vsync Ignored
 */
void PLAT_setVsync(int vsync) {
	// Always vsync on Trimui Smart
}

/**
 * Resizes screen surface for dynamic resolution changes.
 *
 * Creates a new SDL surface wrapper around the existing ION-allocated
 * screen buffer with updated dimensions. Used when games change resolution.
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param pitch New pitch (row stride) in bytes
 * @return Resized SDL surface
 *
 * @note Resets rotation state to force recalculation
 * @note Sets resized flag to update hardware layer config on next flip
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {

	SDL_FreeSurface(vid.screen);
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;

	vid.screen = SDL_CreateRGBSurfaceFrom(vid.screen_info.vadd, vid.width, vid.height, FIXED_DEPTH, vid.pitch, RGBA_MASK_565);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	vid.resized = 1;

	// Reset rotation state (will be recalculated in PLAT_blitRenderer)
	vid.rotated_pitch = 0;
	if (vid.renderer) vid.renderer->src_w = 0;
	return vid.screen;
}

/**
 * Sets video scaling clip region.
 *
 * Not supported on Trimui Smart (hardware scaler is full-screen only).
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// Not supported
}

/**
 * Enables nearest-neighbor filtering.
 *
 * Not configurable on Trimui Smart (hardware scaler behavior is fixed).
 */
void PLAT_setNearestNeighbor(int enabled) {
	// Not configurable
}

/**
 * Sets display sharpness.
 *
 * Not supported on Trimui Smart.
 */
void PLAT_setSharpness(int sharpness) {
	// Not supported
}

/**
 * Sets display effect (scanlines, etc.).
 *
 * Not supported on Trimui Smart.
 */
void PLAT_setEffect(int effect) {
	// Not supported
}

/**
 * Waits for vertical blank (vsync).
 *
 * Blocks until next vsync to prevent tearing.
 *
 * @param remaining Ignored
 */
void PLAT_vsync(int remaining) {
	ioctl(vid.fb_fd, FBIO_WAITFORVSYNC, &_);
}

///////////////////////////////
// Hardware Scaling and Rendering
///////////////////////////////

/**
 * Selects appropriate NEON-optimized scaler for integer scale factor.
 *
 * Returns a function pointer to the optimal NEON scaler implementation
 * for the requested scale factor. All scalers are ARM NEON optimized
 * for 16-bit RGB565 pixels.
 *
 * @param renderer Renderer containing scale factor
 * @return Function pointer to scaler implementation
 *
 * @note All scalers use NEON SIMD instructions for performance
 * @note Scale factors > 6 fall back to 1x1 (no scaling)
 */
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

/**
 * Blits renderer content with rotation and scaling.
 *
 * This function handles the complete pipeline for rendering scaled game content:
 * 1. Allocate/resize rotation buffer if needed (8-byte aligned pitch)
 * 2. Calculate rotation offsets for source and destination
 * 3. Rotate source buffer 90 degrees + flip to match display orientation
 * 4. Apply NEON-optimized integer scaling
 * 5. Write to hardware double buffer
 *
 * The rotation buffer (vid.special) is recreated when:
 * - Source dimensions change (different game resolution)
 * - First time renderer is used
 *
 * Offsets account for:
 * - Source clipping (renderer->src_x, renderer->src_y)
 * - Destination position (renderer->dst_x, renderer->dst_y)
 * - Portrait-to-landscape rotation
 *
 * @param renderer Renderer containing source buffer, dimensions, and scaler
 *
 * @note Pitch is 8-byte aligned for NEON optimizations
 * @note Source and destination coordinates are swapped due to rotation
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.renderer = renderer;
	int p = ((renderer->src_h+7)/8)*8 * FIXED_BPP;  // 8-byte aligned pitch for NEON

	// Recreate rotation buffer if dimensions changed
	if (!vid.special || vid.special->w!=renderer->src_h || vid.special->h!=renderer->src_w || vid.special->pitch!=p || !vid.rotated_pitch) {

		if (vid.special) SDL_FreeSurface(vid.special);

		// Rotation buffer (swapped dimensions: width becomes height)
		vid.special = SDL_CreateRGBSurface(SDL_SWSURFACE, renderer->src_h,renderer->src_w, FIXED_DEPTH,RGBA_MASK_565);
		vid.rotated_pitch = vid.height * FIXED_BPP;
		// Calculate offsets accounting for rotation
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

	// Step 1: Rotate source buffer into special buffer
	rotate_16bpp(renderer->src, vid.special->pixels, renderer->src_w,renderer->src_h,renderer->src_p,vid.special->pitch);

	// Step 2: Scale rotated buffer into display buffer (NEON optimized)
	((scaler_t)renderer->blit)(vid.special->pixels + vid.source_offset, vid.buffer->pixels+vid.rotated_offset, vid.special->w,vid.special->h, vid.special->pitch, vid.renderer->dst_h, vid.renderer->dst_w,vid.rotated_pitch);
}

/**
 * Flips display buffer to screen (page flip).
 *
 * Performs double-buffered page flip using zero-copy hardware buffer swap:
 * 1. Rotate screen buffer if not using renderer (UI/launcher mode)
 * 2. Update buffer address in both layer config and direct register write
 * 3. Update layer configuration if screen was resized
 * 4. Swap pages for next frame
 * 5. Optionally wait for vsync
 * 6. Clear backbuffer if needed
 *
 * Zero-copy flip technique:
 * - Writes physical buffer address directly to display hardware register
 * - Also updates layer config for consistency
 * - Hardware automatically switches to new buffer on next vsync
 *
 * Double buffering:
 * - Two pages in ION allocation
 * - Page 0/1 toggled each frame
 * - Prevents tearing and allows rendering while displaying
 *
 * @param IGNORED Unused (kept for API compatibility)
 * @param sync If true, wait for vsync before returning
 *
 * @note Direct register write (mem_map) enables zero-copy page flip
 * @note Renderer is cleared after flip (one-shot rendering)
 */
void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	// If no renderer was used, rotate screen buffer directly (UI mode)
	if (!vid.renderer) rotate_16bpp(vid.screen->pixels, vid.buffer->pixels, vid.width, vid.height,vid.pitch,vid.height*FIXED_BPP);

	// Update buffer address for page flip (zero-copy via register write)
	vid.buffer_config.info.fb.addr[0] = (uintptr_t)vid.buffer_info.padd + vid.page * PAGE_SIZE;
	vid.mem_map[OVL_V_TOP_LADD0/4] = (uintptr_t)vid.buffer_info.padd + vid.page * PAGE_SIZE;

	// Update hardware layer config if screen was resized
	if (vid.resized) {
		vid.buffer_config.info.fb.size[0].width  = vid.height;
		vid.buffer_config.info.fb.size[0].height = vid.width;
		vid.buffer_config.info.fb.crop.width  = (int64_t)vid.height << 32;
		vid.buffer_config.info.fb.crop.height = (int64_t)vid.width  << 32;
		uint32_t args[4] = {0, (uintptr_t)&vid.buffer_config, 1, 0};
		ioctl(vid.disp_fd, DISP_LAYER_SET_CONFIG, args);
		vid.resized = 0;
	}

	// Toggle page for double buffering (0->1, 1->0)
	vid.page ^= 1;
	vid.buffer->pixels = vid.buffer_info.vadd + vid.page * PAGE_SIZE;

	if (sync) PLAT_vsync(0);

	// Clear backbuffer if clearAll was called
	if (vid.cleared) {
		PLAT_clearVideo(vid.buffer);
		vid.cleared = 0;
	}

	// Reset renderer (one-shot)
	vid.renderer = NULL;
}

///////////////////////////////
// UI Overlay (Unused)
///////////////////////////////

/**
 * Overlay configuration for UI elements.
 *
 * Reserved for future use with OVERLAY_CH (channel 2).
 * Would support alpha blending for on-screen UI elements.
 *
 * @note Currently unused - overlay layer not enabled
 */
#define OVERLAY_WIDTH PILL_SIZE       // Width in pixels
#define OVERLAY_HEIGHT PILL_SIZE      // Height in pixels
#define OVERLAY_BPP 4                 // 32-bit ARGB
#define OVERLAY_DEPTH 16              // Bit depth
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB

static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

/**
 * Initializes overlay surface.
 *
 * Creates SDL surface for UI overlay elements. Currently unused
 * as overlay layer is not enabled in display configuration.
 *
 * @return SDL surface for overlay rendering
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
 * Enables or disables overlay layer.
 *
 * Not implemented - overlay layer not used.
 *
 * @param enable Ignored
 */
void PLAT_enableOverlay(int enable) {
	// Not implemented
}

///////////////////////////////
// Battery Monitoring (LRADC)
///////////////////////////////

/**
 * LRADC (Low Resolution ADC) hardware addresses.
 *
 * The Allwinner F1C100s includes a low-resolution ADC used for
 * battery voltage monitoring. Accessed via /dev/mem mapping.
 */
#define LRADC 0x01C22800       // LRADC base address
#define LRADC_VALUE 0x10       // Data register offset

/**
 * ADC context for battery monitoring.
 */
static struct ADC_Context {
	int	mem_fd;        // /dev/mem file descriptor
	int page_size;     // System page size
	void* mem_map;     // Mapped memory region
	void* adc_addr;    // LRADC register base
} adc;

/**
 * Initializes LRADC for battery voltage monitoring.
 *
 * Maps LRADC hardware registers via /dev/mem and configures
 * the ADC for continuous reading.
 *
 * Configuration value 0xC0004D:
 * - Enables ADC
 * - Sets sampling rate
 * - Configures voltage reference
 *
 * @note Called before video initialization
 */
void ADC_init(void) {
	adc.page_size = sysconf(_SC_PAGESIZE);
	int page_mask = (~(adc.page_size - 1));
	int addr_start = LRADC & page_mask;
	int addr_offset = LRADC & ~page_mask;

	adc.mem_fd = open("/dev/mem",O_RDWR);
	adc.mem_map = mmap(0, adc.page_size*2, PROT_READ|PROT_WRITE, MAP_SHARED, adc.mem_fd, addr_start);
	adc.adc_addr = adc.mem_map + addr_offset;
	*(uint32_t*)adc.adc_addr = 0xC0004D;  // Configure and enable LRADC
}

/**
 * Reads current battery voltage from LRADC.
 *
 * @return Raw ADC value (higher = more voltage)
 *
 * @note Value ranges roughly 38-44 for typical battery levels
 */
int ADC_read(void) {
	return *((uint32_t *)(adc.adc_addr + LRADC_VALUE));
}

/**
 * Shuts down LRADC.
 *
 * Unmaps registers and closes /dev/mem.
 */
void ADC_quit(void) {
	munmap(adc.mem_map, adc.page_size*2);
	close(adc.mem_fd);
}

///////////////////////////////
// Power Management
///////////////////////////////

#define USB_SPEED "/sys/devices/platform/sunxi_usb_udc/udc/sunxi_usb_udc/current_speed"

/**
 * Gets battery charge level and charging status.
 *
 * Charging detection:
 * - Reads USB device current_speed sysfs file
 * - "UNKNOWN" = not connected/charging
 * - Any other value = USB connected (charging)
 *
 * Battery level estimation (via LRADC):
 * - Raw values 38-44 map to battery percentage
 * - Thresholds are conservative to avoid premature low battery warnings
 * - Values calibrated for Trimui Smart battery characteristics
 *
 * @param is_charging Output: 1 if charging, 0 otherwise
 * @param charge Output: Battery percentage (10-100)
 *
 * @note "Worry less about battery and more about the game you're playing"
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	char value[16]; memset(value, 0, 16);
	getFile(USB_SPEED, value, 16);
	*is_charging = !exactMatch(value, "UNKNOWN\n");

	int i = ADC_read();
	// Map raw ADC values to battery percentage
	     if (i>43) *charge = 100;
	else if (i>41) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>39) *charge =  40;
	else if (i>38) *charge =  20;
	else           *charge =  10;
}

/**
 * Enables or disables display backlight.
 *
 * Enable: Restores brightness and turns off LED indicator
 * Disable: Sets brightness to 0 and turns on LED indicator
 *
 * @param enable 1 to enable backlight, 0 to disable
 *
 * @note LED provides visual feedback when screen is off
 */
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		system("leds_off");
	}
	else {
		SetRawBrightness(0);
		system("leds_on");
	}
}

/**
 * Powers off the device.
 *
 * Shutdown sequence:
 * 1. Turn on LED indicator
 * 2. Wait for user feedback
 * 3. Mute audio
 * 4. Disable backlight
 * 5. Shutdown subsystems (sound, vibration, power, graphics)
 * 6. Signal poweroff to system
 * 7. Exit process
 *
 * @note Creates /tmp/poweroff for init system to detect
 */
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
// CPU Frequency Scaling
///////////////////////////////

#define GOVERNOR_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"

/**
 * Sets CPU frequency for power/performance tradeoff.
 *
 * Frequency profiles:
 * - MENU (504 MHz): Lowest power for UI navigation
 * - POWERSAVE (1104 MHz): Extended battery for light games
 * - NORMAL (1344 MHz): Balanced performance for most games
 * - PERFORMANCE (1536 MHz): Maximum performance for demanding games
 *
 * @param speed CPU speed profile (CPU_SPEED_*)
 *
 * @note Uses userspace governor with fixed frequencies
 */
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

///////////////////////////////
// Miscellaneous Platform Functions
///////////////////////////////

/**
 * Sets rumble/haptic feedback strength.
 *
 * Not supported on Trimui Smart (no rumble motor).
 *
 * @param strength Ignored
 */
void PLAT_setRumble(int strength) {
	// No rumble hardware
}

/**
 * Picks appropriate audio sample rate.
 *
 * Returns the requested rate clamped to maximum supported rate.
 *
 * @param requested Desired sample rate
 * @param max Maximum supported sample rate
 * @return Selected sample rate
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

/**
 * Returns device model name.
 *
 * @return "Trimui Smart"
 */
char* PLAT_getModel(void) {
	return "Trimui Smart";
}

/**
 * Checks if device has network connectivity.
 *
 * Trimui Smart has no WiFi hardware.
 *
 * @return 0 (never online)
 */
int PLAT_isOnline(void) {
	return 0;
}