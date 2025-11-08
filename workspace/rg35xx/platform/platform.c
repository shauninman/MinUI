/**
 * platform.c - Anbernic RG35XX platform implementation
 *
 * Hardware details:
 * - SoC: Actions ATM7039S (quad-core ARM Cortex-A9 @ 1.5GHz)
 * - Display: 640x480 IPS LCD with OWL Display Engine hardware acceleration
 * - Memory: ION memory allocator for zero-copy video buffers
 * - Features: Hardware overlay support, bilinear/nearest-neighbor filtering
 *
 * Display Engine Architecture:
 * The OWL Display Engine (DE) provides hardware-accelerated video output with:
 * - Multiple overlay layers (video planes) for compositing
 * - Hardware scaling with configurable filtering coefficients
 * - Direct memory access to ION-allocated buffers
 * - Register-based configuration via memory-mapped I/O
 *
 * Memory Management:
 * Uses ION (In-Out-of-Order Notification) allocator for GPU-accessible memory:
 * - Physical addresses for hardware DMA
 * - Virtual addresses for CPU access
 * - Zero-copy buffer sharing between CPU and display hardware
 *
 * Double Buffering:
 * Implements page flipping between two framebuffers to prevent tearing.
 * The display engine reads from one buffer while CPU draws to the other.
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

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "ion.h"
#include "ion-owl.h"
#include "de_atm7059.h"
#include "scaler.h"

///////////////////////////////
// Input Management
///////////////////////////////

/**
 * Initializes input handling.
 *
 * The RG35XX uses SDL for input, which is initialized by the video system.
 * No additional setup is needed here.
 */
void PLAT_initInput(void) {
	// Input handled by SDL (initialized in PLAT_initVideo)
}

/**
 * Shuts down input handling.
 *
 * Input resources are cleaned up by SDL_Quit() in PLAT_quitVideo().
 */
void PLAT_quitInput(void) {
	// Input cleaned up by SDL_Quit()
}

///////////////////////////////
// ION Memory Management
///////////////////////////////

/**
 * ION allocation tracking structure.
 *
 * ION (In-Out-of-Order Notification) is Android's memory allocator for
 * sharing buffers between kernel drivers and userspace.
 */
typedef struct ion_alloc_info {
	uint32_t			size;		// Allocation size in bytes
	struct ion_handle	*handle;	// Opaque kernel handle
	int					fd;			// File descriptor for mmap
	void*				padd;		// Physical address for DMA
	void*				vadd;		// Virtual address for CPU access
} ion_alloc_info_t;

/**
 * Allocates ION memory for hardware DMA access.
 *
 * Allocates physically contiguous memory from the ION PMEM heap and
 * provides both physical and virtual address mappings. The physical
 * address is used by the Display Engine for DMA, while the virtual
 * address allows CPU access for rendering.
 *
 * Memory flow:
 * 1. Allocate from PMEM heap (physically contiguous)
 * 2. Get physical address for Display Engine registers
 * 3. Get file descriptor for mmap
 * 4. Map to virtual address for CPU access
 *
 * @param fd_ion File descriptor for /dev/ion
 * @param info Allocation info structure (size must be set, other fields filled by function)
 *
 * @note Allocation size is rounded up to page boundary
 * @note Uses PMEM heap (ION_HEAP_ID_PMEM) for physically contiguous memory
 */
static void ion_alloc(int fd_ion, ion_alloc_info_t* info) {
	struct ion_allocation_data	iad;
	struct ion_fd_data		ifd;
	struct ion_custom_data		icd;
	struct owl_ion_phys_data 	ipd;

	// Request allocation from ION PMEM heap
	iad.len = info->size;
	iad.align = sysconf(_SC_PAGESIZE); // Align to page boundary
	iad.heap_id_mask = (1<<ION_HEAP_ID_PMEM); // Physical memory heap
	iad.flags = 0;
	if (ioctl(fd_ion, ION_IOC_ALLOC, &iad)<0) fprintf(stderr, "ION_ALLOC failed %s\n",strerror(errno));

	// Get physical address (needed for Display Engine DMA registers)
	icd.cmd = OWL_ION_GET_PHY;
	icd.arg = (uintptr_t)&ipd;
	ipd.handle = iad.handle;
	if (ioctl(fd_ion, ION_IOC_CUSTOM, &icd)<0) printf("ION_GET_PHY failed %s\n",strerror(errno));

	// Get file descriptor for mmap
	ifd.handle = iad.handle;
	if (ioctl(fd_ion, ION_IOC_MAP, &ifd)<0) fprintf(stderr, "ION_MAP failed %s\n",strerror(errno));

	// Store allocation info
	info->handle = (void*)iad.handle;
	info->fd = ifd.fd;
	info->padd = (void*)ipd.phys_addr; // Physical address for hardware
	info->vadd = mmap(0, info->size, PROT_READ|PROT_WRITE, MAP_SHARED, info->fd, 0); // Virtual address for CPU

}

/**
 * Frees ION memory allocation.
 *
 * Releases resources in reverse order:
 * 1. Unmap virtual address
 * 2. Close file descriptor
 * 3. Free ION handle
 *
 * @param fd_ion File descriptor for /dev/ion
 * @param info Allocation info from ion_alloc()
 */
static void ion_free(int fd_ion, ion_alloc_info_t* info) {
	struct ion_handle_data	ihd;
	munmap(info->vadd, info->size);
	close(info->fd);
	ihd.handle = (uintptr_t)info->handle;
	if (ioctl(fd_ion, ION_IOC_FREE, &ihd)<0) fprintf(stderr, "ION_FREE failed %s\n",strerror(errno));
	fflush(stdout);
}

///////////////////////////////
// Display Engine Hardware Control
///////////////////////////////

/**
 * Display Engine register base and size.
 *
 * The Display Engine is memory-mapped at a fixed physical address.
 * Access is via /dev/mem to directly manipulate hardware registers.
 */
#define	DE		(0xB02E0000)	// Display Engine base address
#define	DE_SIZE	(0x00002000)	// 8KB register space

/**
 * Overlay enable state (toggled by PLAT_enableOverlay).
 * Controls whether the hardware overlay layer is composited.
 */
static int de_enable_overlay = 0;

/**
 * Display Engine scaling coefficient presets.
 *
 * These control the hardware bilinear filtering behavior for upscaling
 * and downscaling. Each preset configures 8 coefficient registers that
 * define how adjacent pixels are blended during scaling.
 */
enum {
	DE_SCOEF_NONE,				// Nearest-neighbor (sharp, no filtering)
	DE_SCOEF_CRISPY,			// Minimal filtering for upscaling
	DE_SCOEF_ZOOMIN,			// Bilinear for smooth upscaling
	DE_SCOEF_HALF_ZOOMOUT,		// Balanced downscaling
	DE_SCOEF_SMALLER_ZOOMOUT,	// Aggressive downscaling filter
	DE_SCOEF_MAX
};

/**
 * Configures Display Engine scaling coefficients for a video plane.
 *
 * The hardware scaler uses 8 coefficient registers (SCOEF0-7) that define
 * how to blend adjacent pixels during scaling. Each coefficient represents
 * a fractional position between two pixels (L=left, R=right).
 *
 * Coefficient format: 0x00LLRR00
 * - LL: Left pixel weight (0x00-0x40, where 0x40 = 100%)
 * - RR: Right pixel weight (0x00-0x40)
 * - For proper interpolation: LL + RR should equal 0x40
 *
 * The 8 coefficients apply to fractional pixel positions:
 * - SCOEF0: 0.0   (100% L, 0% R)   - Exact pixel
 * - SCOEF1: 0.125 (87.5% L, 12.5% R)
 * - SCOEF2: 0.25  (75% L, 25% R)
 * - SCOEF3: 0.375 (62.5% L, 37.5% R)
 * - SCOEF4: 0.5   (50% L, 50% R)   - Midpoint
 * - SCOEF5: 0.625 (37.5% L, 62.5% R)
 * - SCOEF6: 0.75  (25% L, 75% R)
 * - SCOEF7: 0.875 (12.5% L, 87.5% R)
 *
 * @param de_mem Pointer to memory-mapped Display Engine registers
 * @param plane Video plane index (0-3)
 * @param scale Scaling coefficient preset (DE_SCOEF_*)
 *
 * @note NONE preset uses 100% left pixel (nearest-neighbor, no blending)
 * @note HALF_ZOOMOUT provides balanced bilinear filtering for most scaling
 */
static void DE_setScaleCoef(uint32_t* de_mem, int plane, int scale) {
	switch(scale) {
		case DE_SCOEF_NONE:	// Nearest-neighbor (no interpolation)
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000; // L 100%  R 0%
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00400000; // L 87.5% R 12.5% (disabled, use 100% L)
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00400000; // L 75%   R 25%   (disabled, use 100% L)
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00400000; // L 62.5% R 37.5% (disabled, use 100% L)
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00004000; // L 50%   R 50%
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00004000; // L 37.5% R 62.5%
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00004000; // L 25%   R 75%
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00004000; // L 12.5% R 87.5%
			break;
		case DE_SCOEF_CRISPY:	// Minimal filtering for sharp upscaling
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00202000; // 50/50 blend at midpoint
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00004000;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00004000;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00004000;
			break;
		case DE_SCOEF_ZOOMIN:	// Full bilinear filtering for smooth upscaling
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0xFC3E07FF;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0xFA3810FE;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0xF9301BFC;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0xFA2626FA;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0xFC1B30F9;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0xFE1038FA;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0xFF073EFC;
			break;
		case DE_SCOEF_HALF_ZOOMOUT:	// Balanced bilinear for downscaling
			de_mem[DE_OVL_SCOEF0(plane)/4]= 0x00400000;
			de_mem[DE_OVL_SCOEF1(plane)/4]= 0x00380800;
			de_mem[DE_OVL_SCOEF2(plane)/4]= 0x00301000;
			de_mem[DE_OVL_SCOEF3(plane)/4]= 0x00281800;
			de_mem[DE_OVL_SCOEF4(plane)/4]= 0x00202000;
			de_mem[DE_OVL_SCOEF5(plane)/4]= 0x00182800;
			de_mem[DE_OVL_SCOEF6(plane)/4]= 0x00103000;
			de_mem[DE_OVL_SCOEF7(plane)/4]= 0x00083800;
			break;
		case DE_SCOEF_SMALLER_ZOOMOUT:	// Aggressive filtering for large downscaling
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

/**
 * Enables or disables the hardware overlay layer.
 *
 * The Display Engine supports compositing multiple video planes.
 * This controls whether plane 2 (the overlay) is enabled in the
 * display path.
 *
 * @param de_mem Pointer to memory-mapped Display Engine registers
 *
 * @note Modifies DE_PATH_CTL register while preserving other bits
 * @note 0x30300000 = both main plane and overlay enabled
 * @note 0x30100000 = only main plane enabled
 */
static void DE_enableLayer(uint32_t* de_mem) {
	de_mem[DE_PATH_CTL(0)/4] = (de_enable_overlay?0x30300000:0x30100000) | (de_mem[DE_PATH_CTL(0)/4] & 0xCF0FFFFF);
}

/**
 * Sets the output rectangle for hardware scaling.
 *
 * Configures the Display Engine to scale the input framebuffer to
 * the specified output rectangle. The hardware scaler reads the
 * input size from OVL_ISIZE and computes the scaling ratio.
 *
 * Scaling ratio calculation:
 * - SR = 0x2000 * input_size / output_size
 * - 0x2000 represents 1.0x scaling (fixed-point format)
 * - Values < 0x2000 are upscaling
 * - Values > 0x2000 are downscaling
 *
 * @param de_mem Pointer to memory-mapped Display Engine registers
 * @param x Output X position (pixels from left edge)
 * @param y Output Y position (pixels from top edge)
 * @param w Output width in pixels
 * @param h Output height in pixels
 *
 * @note All register values are 0-based (width-1, height-1)
 * @note Scaling ratio is calculated from current OVL_ISIZE register
 */
static void DE_setRect(uint32_t* de_mem, int x, int y, int w, int h) {
	// Set output size (0-based: width-1, height-1)
	de_mem[(DE_OVL_OSIZE(0))/4] = ((w-1)&0xFFFF) | ((h-1)<<16);

	// Calculate and set scaling ratio (0x2000 = 1.0x scale)
	de_mem[(DE_OVL_SR(0))/4] = ((0x2000*((de_mem[(DE_OVL_ISIZE(0))/4]&0xFFFF)+1)/w)&0xFFFF) |
						((0x2000*((de_mem[(DE_OVL_ISIZE(0))/4]>>16)+1)/h)<<16);

	// Set output position on screen
	de_mem[(DE_OVL_COOR(0,0))/4] = (y<<16) | (x&0xFFFF);
}

///////////////////////////////
// Framebuffer IOCTL Structures
///////////////////////////////

/**
 * OWL framebuffer IOCTL structures.
 *
 * These structures define the interface to the Actions OWL framebuffer driver,
 * which provides hardware overlay and display control beyond standard Linux fb.
 */

#define MAX_PRIVATE_DATA_SIZE 40

/**
 * Display device information structure.
 * Used with OWLFB_GET_DISPLAY_INFO/OWLFB_SET_DISPLAY_INFO ioctls.
 */
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

/**
 * LCD private information (stored in owlfb_disp_device.mPrivateInfo).
 */
struct display_private_info {
	int LCD_TYPE;
	int	LCD_LIGHTENESS;
	int LCD_SATURATION;
	int LCD_CONSTRAST;
};

/**
 * Display adjustment commands.
 */
enum CmdMode {
    SET_LIGHTENESS = 0,
    SET_SATURATION = 1,
    SET_CONSTRAST = 2,
	SET_DEFAULT = 3,
};

/**
 * VSync synchronization info.
 * Used with OWLFB_VSYNC_EVENT_EN ioctl.
 */
struct owlfb_sync_info {
	__u8 enabled;
	__u8 disp_id;
	__u16 reserved2;
};

/**
 * Hardware overlay types.
 */
enum owlfb_overlay_type {
	OWLFB_OVERLAY_VIDEO = 1,	// Video overlay (used for battery indicator)
	OWLFB_OVERLAY_CURSOR,		// Cursor overlay
};

/**
 * Hardware pixel format modes.
 *
 * The Display Engine supports various RGB and YUV formats.
 * MinUI uses ARGB32 for overlay (battery indicator with alpha blending).
 */
enum owl_color_mode {
	OWL_DSS_COLOR_RGB16	= 0,
	OWL_DSS_COLOR_BGR16	= 1,
	OWL_DSS_COLOR_ARGB32    = 4,	// Used for overlay
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

/**
 * Overlay request/control arguments.
 * Used with OWLFB_OVERLAY_* ioctls.
 */
struct owlfb_overlay_args {
	__u16 fb_id;
	__u16 overlay_id;
	__u16 overlay_type;
	__u32 overlay_mem_base;
	__u32 overlay_mem_size;
	__u32 uintptr_overly_info;	// Pointer to owlfb_overlay_info
};

/**
 * Overlay configuration structure.
 *
 * Defines overlay position, size, scaling, and blending parameters.
 * Passed via owlfb_overlay_args.uintptr_overly_info.
 */
struct owlfb_overlay_info {
	__u32 mem_off;				// Memory offset from framebuffer base
	__u32 mem_size;				// Buffer size
	__u32 screen_width;			// Screen width (pitch)
	enum owl_color_mode color_mode;
	__u32 img_width;			// Input image width
	__u32 img_height;			// Input image height
	__u32 xoff;					// Crop X offset
	__u32 yoff;					// Crop Y offset
	__u32 width;				// Crop width
	__u32 height;				// Crop height
	__u8 rotation;				// Rotation angle
	__u32 pos_x;				// Screen X position
	__u32 pos_y;				// Screen Y position
	__u32 out_width;			// Output width (after scaling)
	__u32 out_height;			// Output height (after scaling)
	__u8 lightness;				// Brightness adjustment
	__u8 saturation;			// Saturation adjustment
	__u8 contrast;				// Contrast adjustment
	bool global_alpha_en;		// Enable global alpha blending
	__u8 global_alpha;			// Global alpha value (0-255)
	bool pre_mult_alpha_en;		// Pre-multiplied alpha mode
	__u8 zorder;				// Layer priority (higher = on top)
};

/**
 * OWL framebuffer IOCTL commands.
 */
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
// Video Context and Initialization
///////////////////////////////

/**
 * Video subsystem state.
 *
 * Tracks all video-related resources including framebuffer, ION memory,
 * Display Engine access, and double-buffering state.
 */
static struct VID_Context {
	SDL_Surface* screen;	// SDL surface wrapping current backbuffer

	int fd_fb;				// File descriptor for /dev/fb0
	int fd_ion;				// File descriptor for /dev/ion
	int fd_mem;				// File descriptor for /dev/mem (Display Engine access)

	uint32_t* de_mem;		// Memory-mapped Display Engine registers

	struct fb_fix_screeninfo finfo;	// Fixed framebuffer info
	struct fb_var_screeninfo vinfo;	// Variable framebuffer info
	ion_alloc_info_t fb_info;		// ION allocation for double-buffered framebuffer

	int page;				// Current page (0 or 1) for double buffering
	int width;				// Current framebuffer width
	int height;				// Current framebuffer height
	int pitch;				// Current framebuffer pitch (bytes per line)
	int cleared;			// Deferred clear flag
} vid;

static int _;	// Unused variable for ioctl calls that require a pointer

/**
 * Initializes video subsystem.
 *
 * Setup process:
 * 1. Initialize SDL for input handling
 * 2. Open hardware devices (framebuffer, ION, Display Engine)
 * 3. Allocate ION memory for double-buffered framebuffer
 * 4. Configure Display Engine registers for scaling and output
 * 5. Enable VSync event notification
 *
 * Double buffering:
 * - Allocates 2 pages (PAGE_COUNT=2) of video memory
 * - CPU draws to one page while Display Engine reads from the other
 * - Pages are swapped in PLAT_flip()
 *
 * @return SDL surface for rendering
 *
 * @note Initial screen points to page 1 (frontbuffer is page 0)
 * @note Configures both plane 0 (main) and plane 2 (overlay) identically
 */
SDL_Surface* PLAT_initVideo(void) {
	// Initialize SDL (for input, not video output)
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	SDL_SetVideoMode(0,0,FIXED_DEPTH,0);

	// Open hardware devices
	vid.fd_fb = open("/dev/fb0", O_RDWR);
	vid.fd_ion = open("/dev/ion", O_RDWR);
	vid.fd_mem = open("/dev/mem", O_RDWR);
	vid.de_mem = mmap(0, DE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, vid.fd_mem, DE);

	// Get framebuffer info
	ioctl(vid.fd_fb, FBIOGET_FSCREENINFO, &vid.finfo);
	ioctl(vid.fd_fb, FBIOGET_VSCREENINFO, &vid.vinfo);

	// Enable VSync event notification
	struct owlfb_sync_info sinfo;
	sinfo.enabled = 1;
	sinfo.disp_id = 2;
	if (ioctl(vid.fd_fb, OWLFB_VSYNC_EVENT_EN, &sinfo)<0) fprintf(stderr, "VSYNC_EVENT_EN failed %s\n",strerror(errno));

	// Setup double buffering state
	vid.page = 1;	// Start with page 1 (page 0 is frontbuffer)
	vid.width = FIXED_WIDTH;
	vid.height = FIXED_HEIGHT;
	vid.pitch = FIXED_PITCH;

	// Allocate ION memory for 2-page framebuffer
	vid.fb_info.size = PAGE_SIZE * PAGE_COUNT;
	ion_alloc(vid.fd_ion, &vid.fb_info);

	// Create SDL surface wrapping page 1
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.fb_info.vadd+PAGE_SIZE, vid.width,vid.height,FIXED_DEPTH,vid.pitch, RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	// Get viewport size from Display Engine
	int vw = (vid.de_mem[DE_PATH_SIZE(0)/4]&0xFFFF)+1;
	int vh = (vid.de_mem[DE_PATH_SIZE(0)/4]>>16)+1;

	// Configure Display Engine registers for both main plane (0) and overlay plane (2)
	vid.de_mem[DE_OVL_ISIZE(0)/4] = vid.de_mem[DE_OVL_ISIZE(2)/4] = ((vid.width-1) & 0xFFFF) | ((vid.height-1) << 16);
	vid.de_mem[DE_OVL_SR(0)/4] = vid.de_mem[DE_OVL_SR(2)/4] = ((0x2000*vid.width/vw)&0xFFFF) | ((0x2000*vid.height/vh)<<16);
	vid.de_mem[DE_OVL_STR(0)/4] = vid.de_mem[DE_OVL_STR(2)/4] = vid.pitch / 8;	// Stride in 64-bit words
	vid.de_mem[DE_OVL_BA0(0)/4] = (uintptr_t)(vid.fb_info.padd + PAGE_SIZE);	// Physical address of page 1

	// Enable bilinear filtering by default
	GFX_setNearestNeighbor(0);

	return vid.screen;
}

/**
 * Shuts down video subsystem.
 *
 * Releases all video resources in reverse initialization order:
 * - ION framebuffer memory
 * - Display Engine register mapping
 * - Hardware device file descriptors
 * - SDL resources
 */
void PLAT_quitVideo(void) {
	ion_free(vid.fd_ion, &vid.fb_info);
	munmap(vid.de_mem, DE_SIZE);
	close(vid.fd_mem);
	close(vid.fd_ion);
	close(vid.fd_fb);
	SDL_FreeSurface(vid.screen);
	SDL_Quit();
}

/**
 * Clears the video buffer to black.
 *
 * @param screen SDL surface to clear (should be vid.screen)
 *
 * @note Called when buffer is offscreen (not currently displayed)
 */
void PLAT_clearVideo(SDL_Surface* screen) {
	memset(screen->pixels, 0, PAGE_SIZE);
}

/**
 * Clears both front and back buffers.
 *
 * Immediately clears the backbuffer and sets a flag to clear the
 * frontbuffer after the next flip (when it becomes offscreen).
 *
 * @note Prevents brief flash of old content when clearing during display
 */
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen);	// Clear backbuffer immediately
	vid.cleared = 1;				// Defer clearing frontbuffer until offscreen
}

/**
 * Sets VSync mode.
 *
 * @param vsync 1 to enable VSync, 0 to disable
 *
 * @note VSync is always enabled on RG35XX (controlled in PLAT_flip)
 */
void PLAT_setVsync(int vsync) {
	// VSync always enabled (see PLAT_flip)
}

/**
 * Resizes the video output.
 *
 * Reconfigures the framebuffer and Display Engine registers for a new
 * resolution. Used when switching between different emulator cores that
 * output different resolutions.
 *
 * @param w New width in pixels
 * @param h New height in pixels
 * @param pitch New pitch in bytes
 * @return New SDL surface for rendering
 *
 * @note Recalculates hardware scaling ratio to fit new resolution on screen
 */
SDL_Surface* PLAT_resizeVideo(int w, int h, int pitch) {
	vid.width = w;
	vid.height = h;
	vid.pitch = pitch;

	// Recreate SDL surface with new dimensions
	SDL_FreeSurface(vid.screen);
	vid.screen = SDL_CreateRGBSurfaceFrom(vid.fb_info.vadd + vid.page*PAGE_SIZE, vid.width,vid.height,FIXED_DEPTH,vid.pitch, RGBA_MASK_AUTO);
	memset(vid.screen->pixels, 0, vid.pitch * vid.height);

	// Get viewport size from Display Engine
	int vw = (vid.de_mem[DE_PATH_SIZE(0)/4]&0xFFFF)+1;
	int vh = (vid.de_mem[DE_PATH_SIZE(0)/4]>>16)+1;

	// Update Display Engine registers for new resolution
	vid.de_mem[DE_OVL_ISIZE(0)/4] = vid.de_mem[DE_OVL_ISIZE(2)/4] = ((vid.width-1) & 0xFFFF) | ((vid.height-1) << 16);
	vid.de_mem[DE_OVL_SR(0)/4] = vid.de_mem[DE_OVL_SR(2)/4] = ((0x2000*vid.width/vw)&0xFFFF) | ((0x2000*vid.height/vh)<<16);
	vid.de_mem[DE_OVL_STR(0)/4] = vid.de_mem[DE_OVL_STR(2)/4] = vid.pitch / 8;
	vid.de_mem[DE_OVL_BA0(0)/4] = (uintptr_t)(vid.fb_info.padd + vid.page * PAGE_SIZE);

	return vid.screen;
}

/**
 * Sets the output clipping rectangle for hardware scaling.
 *
 * Allows scaling output to a specific region of the screen instead of
 * the full display. Used for aspect ratio correction.
 *
 * @param x Output X position
 * @param y Output Y position
 * @param width Output width
 * @param height Output height
 */
void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	DE_setRect(vid.de_mem, x,y,width,height);
}

/**
 * Configures hardware pixel filtering mode.
 *
 * @param enabled 1 for nearest-neighbor (sharp), 0 for bilinear (smooth)
 *
 * @note Applies to all 4 video planes
 */
void PLAT_setNearestNeighbor(int enabled) {
	int scale_coef = enabled ? DE_SCOEF_NONE : DE_SCOEF_HALF_ZOOMOUT;
	DE_setScaleCoef(vid.de_mem, 0, scale_coef);
	DE_setScaleCoef(vid.de_mem, 1, scale_coef);
	DE_setScaleCoef(vid.de_mem, 2, scale_coef);
	DE_setScaleCoef(vid.de_mem, 3, scale_coef);
}

///////////////////////////////
// Rendering Effects and Scaling
///////////////////////////////

/**
 * Pixel effect state tracking.
 *
 * MinUI supports optional scanline/grid effects for retro aesthetics.
 * These are implemented via CPU-based scalers rather than hardware.
 */
static int next_effect = EFFECT_NONE;		// Effect requested for next frame
static int effect_type = EFFECT_NONE;		// Currently active effect

/**
 * Resets scaling sharpness.
 *
 * When sharpness changes (e.g., switching between hardware and software scaling),
 * this forces the effect to reload to ensure consistency.
 *
 * @param sharpness New sharpness value (unused on RG35XX)
 *
 * @note Forces effect reload by invalidating current effect_type
 */
void PLAT_setSharpness(int sharpness) {
	// Force effect to reload on next frame
	if (effect_type>=EFFECT_NONE) next_effect = effect_type;
	effect_type = -1;
}

/**
 * Sets the visual effect for rendering.
 *
 * @param effect Effect type (EFFECT_NONE, EFFECT_LINE, EFFECT_GRID)
 *
 * @note Effect is applied on next PLAT_blitRenderer call
 */
void PLAT_setEffect(int effect) {
	next_effect = effect;
}

/**
 * Waits for vertical sync.
 *
 * Blocks until the Display Engine completes the current scanout and
 * enters the vertical blanking interval. Prevents tearing by ensuring
 * buffer swaps happen during VBlank.
 *
 * @param remaining Unused (for compatibility with other platforms)
 *
 * @note Uses OWLFB_WAITFORVSYNC ioctl to the framebuffer driver
 */
void PLAT_vsync(int remaining) {
	if (ioctl(vid.fd_fb, OWLFB_WAITFORVSYNC, &_)) LOG_info("OWLFB_WAITFORVSYNC failed %s\n", strerror(errno));
}

/**
 * Selects the appropriate pixel scaler function.
 *
 * Returns a function pointer to a NEON-optimized scaler that handles
 * the requested effect and scaling factor. Effects include:
 * - NONE: Plain upscaling
 * - LINE: Scanline effect (horizontal lines)
 * - GRID: Grid effect (horizontal and vertical lines)
 *
 * @param renderer Renderer context with scale factor
 * @return Function pointer to scaler implementation
 *
 * @note Scanline/grid effects only available for 2x-4x scaling
 * @note All scalers are NEON-optimized (_n16 suffix = RGB565)
 */
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

	// Standard scaling (no effect)
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
 * Renders a frame using software scaling.
 *
 * Called by the graphics renderer to blit a scaled frame into the
 * framebuffer. Handles effect transitions and invokes the appropriate
 * NEON-optimized scaler.
 *
 * @param renderer Renderer context with source/dest buffers and dimensions
 *
 * @note Refreshes scaler function if effect type changed since last call
 * @note Calculates destination pointer with proper alignment for NEON
 */
void PLAT_blitRenderer(GFX_Renderer* renderer) {
	// Update scaler if effect changed
	if (effect_type!=next_effect) {
		effect_type = next_effect;
		renderer->blit = PLAT_getScaler(renderer);
	}

	// Calculate destination pointer
	void* dst = renderer->dst + (renderer->dst_y * renderer->dst_p) + (renderer->dst_x * FIXED_BPP);

	// Invoke scaler
	((scaler_t)renderer->blit)(renderer->src,dst,renderer->src_w,renderer->src_h,renderer->src_p,renderer->dst_w,renderer->dst_h,renderer->dst_p);
}

/**
 * Flips the framebuffer (presents rendered frame).
 *
 * Double buffering flow:
 * 1. Point Display Engine to current backbuffer (make it frontbuffer)
 * 2. Enable hardware overlay layer if requested
 * 3. Optionally wait for VSync
 * 4. Swap page index (old frontbuffer becomes new backbuffer)
 * 5. Update screen->pixels to point to new backbuffer
 * 6. Clear new backbuffer if deferred clear flag was set
 *
 * @param IGNORED SDL surface parameter (unused, vid.screen is always used)
 * @param sync 1 to wait for VSync, 0 to flip immediately
 *
 * @note Updates DE_OVL_BA0 register with physical address of new frontbuffer
 * @note Page index toggles between 0 and 1 using XOR
 */
void PLAT_flip(SDL_Surface* IGNORED, int sync) {
	// Point Display Engine to current page (physical address)
	vid.de_mem[DE_OVL_BA0(0)/4] = vid.de_mem[DE_OVL_BA0(2)/4] = (uintptr_t)(vid.fb_info.padd + vid.page * PAGE_SIZE);
	DE_enableLayer(vid.de_mem);

	// Wait for VSync if requested
	if (sync) PLAT_vsync(0);

	// Swap to other page for rendering next frame
	vid.page ^= 1;
	vid.screen->pixels = vid.fb_info.vadd + vid.page * PAGE_SIZE;

	// Apply deferred clear if needed
	if (vid.cleared) {
		PLAT_clearVideo(vid.screen);
		vid.cleared = 0;
	}
}

///////////////////////////////
// Hardware Overlay (Battery Indicator)
///////////////////////////////

/**
 * Overlay configuration.
 *
 * The RG35XX Display Engine supports multiple hardware overlay planes
 * that can be composited on top of the main framebuffer. MinUI uses
 * overlay plane 1 to display the battery indicator (pill icon) in the
 * top-right corner.
 *
 * Hardware overlay advantages:
 * - No CPU overhead (composited by Display Engine)
 * - Per-pixel alpha blending
 * - Independent positioning and scaling
 * - Can be toggled without affecting main framebuffer
 */
#define OVERLAY_WIDTH PILL_SIZE		// Unscaled overlay width
#define OVERLAY_HEIGHT PILL_SIZE	// Unscaled overlay height
#define OVERLAY_BPP 4				// 32-bit ARGB
#define OVERLAY_DEPTH 32
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP)
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000	// ARGB format
#define OVERLAY_FB 0				// Framebuffer device index
#define OVERLAY_ID 1				// Overlay plane ID

/**
 * Overlay subsystem state.
 */
static struct OVL_Context {
	SDL_Surface* overlay;				// SDL surface for CPU rendering
	struct owlfb_overlay_args oargs;	// IOCTL arguments
	struct owlfb_overlay_info oinfo;	// Overlay configuration
	ion_alloc_info_t ov_info;			// ION memory allocation
} ovl;

/**
 * Initializes hardware overlay for battery indicator.
 *
 * Setup process:
 * 1. Allocate SDL surface (scaled by platform SCALE factor)
 * 2. Allocate ION memory for hardware access
 * 3. Configure overlay parameters (position, size, format, z-order)
 * 4. Enable overlay via framebuffer IOCTL
 *
 * The overlay is positioned in the top-right corner with PADDING offset.
 * It uses ARGB32 format for per-pixel alpha blending, allowing the battery
 * icon to appear semi-transparent over game content.
 *
 * @return SDL surface for rendering battery indicator
 *
 * @note Overlay is enabled but not visible until de_enable_overlay is set
 * @note Z-order is 3 (on top of main framebuffer)
 * @note Uses per-pixel alpha (not global alpha)
 */
SDL_Surface* PLAT_initOverlay(void) {
	// Create SDL surface (scaled by SCALE2 macro)
	ovl.overlay = SDL_CreateRGBSurfaceFrom(NULL,SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,SCALE1(OVERLAY_PITCH), OVERLAY_RGBA_MASK);
	uint32_t size = ovl.overlay->h * ovl.overlay->pitch;

	// Allocate ION memory for overlay buffer
	ovl.ov_info.size = size;
	ion_alloc(vid.fd_ion, &ovl.ov_info);
	ovl.overlay->pixels = ovl.ov_info.vadd;
	memset(ovl.overlay->pixels, 0xff, size);	// Initialize to white (will be overwritten)

	// Setup IOCTL arguments
	memset(&ovl.oargs, 0, sizeof(struct owlfb_overlay_args));
	ovl.oargs.fb_id = OVERLAY_FB;
	ovl.oargs.overlay_id = OVERLAY_ID;
	ovl.oargs.overlay_type = OWLFB_OVERLAY_VIDEO;
	ovl.oargs.uintptr_overly_info = (uintptr_t)&ovl.oinfo;

	// Calculate overlay position (top-right corner)
	int x,y,w,h;
	w = h = ovl.overlay->w;
	x = FIXED_WIDTH - SCALE1(PADDING) - w;
	y = SCALE1(PADDING);

	// Configure overlay info
	ovl.oinfo.mem_off = (uintptr_t)ovl.ov_info.padd - vid.finfo.smem_start;	// Offset from FB base
	ovl.oinfo.mem_size = size;
	ovl.oinfo.screen_width = PAGE_WIDTH;
	ovl.oinfo.color_mode = OWL_DSS_COLOR_ARGB32;	// 32-bit ARGB with alpha
	ovl.oinfo.img_width = w;
	ovl.oinfo.img_height = h;
	ovl.oinfo.xoff = 0;				// No cropping
	ovl.oinfo.yoff = 0;
	ovl.oinfo.width = w;
	ovl.oinfo.height = h;
	ovl.oinfo.rotation = 0;
	ovl.oinfo.pos_x = x;			// Top-right corner position
	ovl.oinfo.pos_y = y;
	ovl.oinfo.out_width = w;		// No scaling (1:1)
	ovl.oinfo.out_height = h;
	ovl.oinfo.global_alpha_en = 0;	// Use per-pixel alpha, not global
	ovl.oinfo.global_alpha = 0;
	ovl.oinfo.pre_mult_alpha_en = 0;
	ovl.oinfo.zorder = 3;			// Top layer (above main framebuffer)

	// Configure and enable overlay via IOCTL
	if (ioctl(vid.fd_fb, OWLFB_OVERLAY_SETINFO, &ovl.oargs)<0) printf("SETINFO failed %s\n",strerror(errno));
	if (ioctl(vid.fd_fb, OWLFB_OVERLAY_ENABLE, &ovl.oargs)<0) printf("ENABLE failed %s\n",strerror(errno));

	// Overlay is enabled but not visible until PLAT_enableOverlay(1) is called
	DE_enableLayer(vid.de_mem);

	return ovl.overlay;
}

/**
 * Shuts down hardware overlay.
 *
 * Disables the overlay plane and releases associated resources.
 */
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
	ion_free(vid.fd_ion, &ovl.ov_info);

	// Disable overlay via IOCTL
	memset(&ovl.oargs, 0, sizeof(struct owlfb_overlay_args));
	ovl.oargs.fb_id = OVERLAY_FB;
	ovl.oargs.overlay_id = OVERLAY_ID;
	ovl.oargs.overlay_type = OWLFB_OVERLAY_VIDEO;
	ovl.oargs.uintptr_overly_info = 0;
	ioctl(vid.fd_fb, OWLFB_OVERLAY_DISABLE, &ovl.oargs);
}

/**
 * Enables or disables overlay visibility.
 *
 * Controls whether the hardware overlay (battery indicator) is composited
 * on top of the main framebuffer. The overlay layer is configured during
 * init but not shown until this is called with enable=1.
 *
 * @param enable 1 to show overlay, 0 to hide
 *
 * @note Actual compositing happens in PLAT_flip via DE_enableLayer
 */
void PLAT_enableOverlay(int enable) {
	de_enable_overlay = enable;
}

///////////////////////////////
// Power Management
///////////////////////////////

/**
 * Retrieves battery status.
 *
 * Reads battery voltage from sysfs and converts it to a percentage.
 * The RG35XX battery voltage ranges approximately 310-410 (3.1V-4.1V).
 *
 * Voltage mapping (intentionally coarse to reduce flicker):
 * - >390 (>3.9V): 100%
 * - >370 (>3.7V): 80%
 * - >350 (>3.5V): 60%
 * - >330 (>3.3V): 40%
 * - >320 (>3.2V): 20%
 * - ≤320 (≤3.2V): 10%
 *
 * @param is_charging Output: 1 if charging, 0 if not
 * @param charge Output: Battery percentage (10, 20, 40, 60, 80, or 100)
 *
 * @note Coarse granularity prevents battery indicator from flickering
 * @note Values are read from /sys/class/power_supply/battery/
 */
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	*is_charging = getInt("/sys/class/power_supply/battery/charger_online");

	// Read voltage (in 10µV units, so divide by 10000 to get centivolts)
	int i = getInt("/sys/class/power_supply/battery/voltage_now") / 10000; // 310-410
	i -= 310; 	// Normalize to ~0-100

	// Map to coarse percentage levels (prevents flicker)
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;
}

/**
 * Enables or disables the LCD backlight.
 *
 * @param enable 1 to turn backlight on, 0 to turn off
 *
 * @note Uses FB_BLANK_UNBLANK/FB_BLANK_POWERDOWN constants
 */
void PLAT_enableBacklight(int enable) {
	putInt("/sys/class/backlight/backlight.2/bl_power", enable ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN);
}

/**
 * Initiates system shutdown.
 *
 * Shutdown sequence:
 * 1. Sleep 2 seconds (allow user to release power button)
 * 2. Mute audio
 * 3. Turn off backlight
 * 4. Cleanup subsystems (sound, vibration, power, graphics)
 * 5. Execute system shutdown command
 *
 * @note This function does not return
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
}

///////////////////////////////
// Performance and Hardware Control
///////////////////////////////

/**
 * Sets CPU frequency for power/performance balance.
 *
 * Frequency levels:
 * - CPU_SPEED_MENU: 504MHz (lowest power, UI only)
 * - CPU_SPEED_POWERSAVE: 1.104GHz (balanced)
 * - CPU_SPEED_NORMAL: 1.296GHz (recommended for most games)
 * - CPU_SPEED_PERFORMANCE: 1.488GHz (maximum, for demanding games)
 *
 * @param speed CPU_SPEED_* constant
 *
 * @note Uses overclock.elf utility to set frequency
 * @note Frequency is in Hz (e.g., 1296000 = 1.296GHz)
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

/**
 * Controls vibration motor strength.
 *
 * The RG35XX vibration motor is controlled via the battery subsystem's
 * "moto" (motor) interface. Strength is specified as a percentage.
 *
 * @param strength Vibration strength (fixed-point 16.16 format, 0-65536)
 *
 * @note Converts from 16.16 fixed-point to 0-100 percentage
 * @note Motor control via /sys/class/power_supply/battery/moto
 */
#define RUMBLE_PATH "/sys/class/power_supply/battery/moto"
void PLAT_setRumble(int strength) {
	// Convert from 16.16 fixed-point to 0-100 percentage
	int val = MAX(0, MIN((100 * strength)>>16, 100));
	putInt(RUMBLE_PATH, val);
}

/**
 * Selects optimal audio sample rate.
 *
 * The RG35XX audio hardware can handle any sample rate up to the maximum,
 * so this simply returns the requested rate if within limits.
 *
 * @param requested Desired sample rate (Hz)
 * @param max Maximum supported sample rate (Hz)
 * @return Sample rate to use
 */
int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

///////////////////////////////
// Platform Identification
///////////////////////////////

/**
 * Returns the device model name.
 *
 * @return Model string for display in UI
 */
char* PLAT_getModel(void) {
	return "Anbernic RG35XX";
}

/**
 * Checks if device has network connectivity.
 *
 * @return 1 if online, 0 if offline
 *
 * @note RG35XX has no WiFi hardware
 */
int PLAT_isOnline(void) {
	return 0;
}