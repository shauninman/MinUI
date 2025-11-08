/**
 * sunxi_display2.h - Allwinner Display Engine 2.0 (DE2) userspace API
 *
 * Comprehensive display subsystem interface for Allwinner sunxi SoCs with
 * Display Engine 2.0 (F1C100s in Trimui Smart). Provides complete control
 * over the video output pipeline including:
 *
 * - Layer management (overlays, blending, z-order)
 * - Framebuffer configuration (RGB/YUV formats, scaling, rotation)
 * - Output device control (LCD, HDMI, TV-out)
 * - Color space conversion and color correction
 * - Video capture (screen recording)
 * - Enhancement features (contrast, saturation, gamma)
 *
 * Architecture:
 * - Managers: Output pipelines (typically 2: LCD + HDMI/TV)
 * - Layers: Compositable video planes (4+ per manager)
 * - Devices: Physical outputs (LCD panel, HDMI, etc.)
 *
 * Ioctl workflow:
 * 1. Open /dev/disp
 * 2. Configure layers via DISP_LAYER_SET_CONFIG
 * 3. Set output mode via DISP_DEVICE_SWITCH
 * 4. Update display via DISP_HWC_COMMIT or DISP_SHADOW_PROTECT
 *
 * @note This is the DE 2.0 API (newer than DE 1.0 on older Allwinner SoCs)
 * @note Trimui Smart uses this interface for framebuffer output
 * @warning Direct ioctl access bypasses Linux DRM/KMS subsystem
 */

#ifndef __SUNXI_DISPLAY_H__
#define __SUNXI_DISPLAY_H__

typedef int bool;
typedef unsigned int u32;

// Forward declarations for display subsystem structures
struct disp_manager;
struct disp_device;
struct disp_smbl;
struct disp_enhance;
struct disp_capture;

///////////////////////////////
// Basic display types
///////////////////////////////

/** ARGB color with alpha channel */
typedef struct {unsigned char  alpha;unsigned char red;unsigned char green; unsigned char blue; }disp_color;

/** Rectangle with position and size */
typedef struct {int x; int y; unsigned int width; unsigned int height;}disp_rect;

/** Size without position */
typedef struct {unsigned int width;unsigned int height;                   }disp_rectsz;

/** 2D position */
typedef struct {int x; int y;                           }disp_position;

/**
 * enum disp_pixel_format - Supported framebuffer pixel formats
 *
 * Defines all RGB and YUV pixel formats supported by the display engine.
 * Format naming: [Color][Bits]_[Type]_[Layout]
 *
 * RGB formats (0x00-0x1F):
 * - ARGB/RGBA/etc: Component order from MSB to LSB
 * - 8888: 8 bits per component (32-bit total)
 * - 888: 8 bits per component (24-bit total)
 * - 565: 5 red, 6 green, 5 blue bits (16-bit)
 * - 4444: 4 bits per component (16-bit)
 * - 1555: 1 alpha bit, 5 bits per RGB component (16-bit)
 *
 * YUV formats (0x40-0x4F):
 * - I: Interleaved (all components in single buffer)
 * - P: Planar (separate buffers for Y, U, V)
 * - SP: Semi-planar (Y in one buffer, UV interleaved in another)
 * - 444: Full chroma resolution (4:4:4)
 * - 422: Half horizontal chroma resolution (4:2:2)
 * - 420: Half horizontal and vertical chroma resolution (4:2:0)
 * - 411: Quarter horizontal chroma resolution (4:1:1)
 *
 * @note Hardware accelerates YUV to RGB conversion
 */
typedef enum
{
	// RGB formats - 32-bit
	DISP_FORMAT_ARGB_8888                    = 0x00,  // MSB A-R-G-B LSB (standard)
	DISP_FORMAT_ABGR_8888                    = 0x01,
	DISP_FORMAT_RGBA_8888                    = 0x02,
	DISP_FORMAT_BGRA_8888                    = 0x03,
	DISP_FORMAT_XRGB_8888                    = 0x04,  // No alpha
	DISP_FORMAT_XBGR_8888                    = 0x05,
	DISP_FORMAT_RGBX_8888                    = 0x06,
	DISP_FORMAT_BGRX_8888                    = 0x07,

	// RGB formats - 24-bit
	DISP_FORMAT_RGB_888                      = 0x08,
	DISP_FORMAT_BGR_888                      = 0x09,

	// RGB formats - 16-bit
	DISP_FORMAT_RGB_565                      = 0x0a,  // Most common 16-bit format
	DISP_FORMAT_BGR_565                      = 0x0b,
	DISP_FORMAT_ARGB_4444                    = 0x0c,
	DISP_FORMAT_ABGR_4444                    = 0x0d,
	DISP_FORMAT_RGBA_4444                    = 0x0e,
	DISP_FORMAT_BGRA_4444                    = 0x0f,
	DISP_FORMAT_ARGB_1555                    = 0x10,
	DISP_FORMAT_ABGR_1555                    = 0x11,
	DISP_FORMAT_RGBA_5551                    = 0x12,
	DISP_FORMAT_BGRA_5551                    = 0x13,

	// YUV formats - Interleaved
	DISP_FORMAT_YUV444_I_AYUV                = 0x40,  // MSB A-Y-U-V LSB (reserved)
	DISP_FORMAT_YUV444_I_VUYA                = 0x41,  // MSB V-U-Y-A LSB
	DISP_FORMAT_YUV422_I_YVYU                = 0x42,  // MSB Y-V-Y-U LSB
	DISP_FORMAT_YUV422_I_YUYV                = 0x43,  // MSB Y-U-Y-V LSB (common)
	DISP_FORMAT_YUV422_I_UYVY                = 0x44,  // MSB U-Y-V-Y LSB
	DISP_FORMAT_YUV422_I_VYUY                = 0x45,  // MSB V-Y-U-Y LSB

	// YUV formats - Planar (3 separate buffers)
	DISP_FORMAT_YUV444_P                     = 0x46,  // YYYY UUUU VVVV (reserved)
	DISP_FORMAT_YUV422_P                     = 0x47,  // YYYY UU VV
	DISP_FORMAT_YUV420_P                     = 0x48,  // YYYY U V (I420/YV12)
	DISP_FORMAT_YUV411_P                     = 0x49,  // YYYY U V

	// YUV formats - Semi-planar (Y + interleaved UV)
	DISP_FORMAT_YUV422_SP_UVUV               = 0x4a,  // Y + UVUV
	DISP_FORMAT_YUV422_SP_VUVU               = 0x4b,  // Y + VUVU
	DISP_FORMAT_YUV420_SP_UVUV               = 0x4c,  // Y + UVUV (NV12)
	DISP_FORMAT_YUV420_SP_VUVU               = 0x4d,  // Y + VUVU (NV21)
	DISP_FORMAT_YUV411_SP_UVUV               = 0x4e,  // Y + UVUV
	DISP_FORMAT_YUV411_SP_VUVU               = 0x4f,  // Y + VUVU
}disp_pixel_format;

typedef enum
{
	DISP_3D_OUT_MODE_CI_1 = 0x5,//column interlaved 1
	DISP_3D_OUT_MODE_CI_2 = 0x6,//column interlaved 2
	DISP_3D_OUT_MODE_CI_3 = 0x7,//column interlaved 3
	DISP_3D_OUT_MODE_CI_4 = 0x8,//column interlaved 4
	DISP_3D_OUT_MODE_LIRGB = 0x9,//line interleaved rgb

	DISP_3D_OUT_MODE_TB = 0x0,//top bottom
	DISP_3D_OUT_MODE_FP = 0x1,//frame packing
	DISP_3D_OUT_MODE_SSF = 0x2,//side by side full
	DISP_3D_OUT_MODE_SSH = 0x3,//side by side half
	DISP_3D_OUT_MODE_LI = 0x4,//line interleaved
	DISP_3D_OUT_MODE_FA = 0xa,//field alternative
}disp_3d_out_mode;

typedef enum
{
	DISP_BT601  = 0,
	DISP_BT709  = 1,
	DISP_YCC    = 2,
}disp_color_space;

typedef enum
{
	DISP_CSC_TYPE_RGB        = 0,
	DISP_CSC_TYPE_YUV1       = 1,//HDMI
	DISP_CSC_TYPE_YUV2       = 2,//TV
}disp_csc_type;

typedef enum
{
	DISP_COLOR_RANGE_16_255 = 0,
	DISP_COLOR_RANGE_0_255  = 1,
	DISP_COLOR_RANGE_16_235 = 2,
}disp_color_range;

/**
 * enum disp_output_type - Display output device types
 *
 * Identifies physical output devices. Values are bit flags that can
 * be combined for multi-output scenarios.
 */
typedef enum
{
	DISP_OUTPUT_TYPE_NONE   = 0,  // No output
	DISP_OUTPUT_TYPE_LCD    = 1,  // LCD panel (most common on handhelds)
	DISP_OUTPUT_TYPE_TV     = 2,  // Composite/component video output
	DISP_OUTPUT_TYPE_HDMI   = 4,  // HDMI digital output
	DISP_OUTPUT_TYPE_VGA    = 8,  // VGA analog output
}disp_output_type;

typedef enum
{
	DISP_TV_MOD_480I                = 0,
	DISP_TV_MOD_576I                = 1,
	DISP_TV_MOD_480P                = 2,
	DISP_TV_MOD_576P                = 3,
	DISP_TV_MOD_720P_50HZ           = 4,
	DISP_TV_MOD_720P_60HZ           = 5,
	DISP_TV_MOD_1080I_50HZ          = 6,
	DISP_TV_MOD_1080I_60HZ          = 7,
	DISP_TV_MOD_1080P_24HZ          = 8,
	DISP_TV_MOD_1080P_50HZ          = 9,
	DISP_TV_MOD_1080P_60HZ          = 0xa,
	DISP_TV_MOD_1080P_24HZ_3D_FP    = 0x17,
	DISP_TV_MOD_720P_50HZ_3D_FP     = 0x18,
	DISP_TV_MOD_720P_60HZ_3D_FP     = 0x19,
	DISP_TV_MOD_1080P_25HZ          = 0x1a,
	DISP_TV_MOD_1080P_30HZ          = 0x1b,
	DISP_TV_MOD_PAL                 = 0xb,
	DISP_TV_MOD_PAL_SVIDEO          = 0xc,
	DISP_TV_MOD_NTSC                = 0xe,
	DISP_TV_MOD_NTSC_SVIDEO         = 0xf,
	DISP_TV_MOD_PAL_M               = 0x11,
	DISP_TV_MOD_PAL_M_SVIDEO        = 0x12,
	DISP_TV_MOD_PAL_NC              = 0x14,
	DISP_TV_MOD_PAL_NC_SVIDEO       = 0x15,
	DISP_TV_MOD_3840_2160P_30HZ     = 0x1c,
	DISP_TV_MOD_3840_2160P_25HZ     = 0x1d,
	DISP_TV_MOD_3840_2160P_24HZ     = 0x1e,
	DISP_TV_MODE_NUM                = 0x1f,
}disp_tv_mode;


//FIXME:still need?
typedef enum
{
	DISP_EXIT_MODE_CLEAN_ALL    = 0,
	DISP_EXIT_MODE_CLEAN_PARTLY = 1,//only clean interrupt temply
}disp_exit_mode;

typedef enum
{
	DISP_BF_NORMAL        = 0,//non-stereo
	DISP_BF_STEREO_TB     = 1 << 0,//stereo top-bottom
	DISP_BF_STEREO_FP     = 1 << 1,//stereo frame packing
	DISP_BF_STEREO_SSH    = 1 << 2,//stereo side by side half
	DISP_BF_STEREO_SSF    = 1 << 3,//stereo side by side full
	DISP_BF_STEREO_LI     = 1 << 4,//stereo line interlace
}disp_buffer_flags;

typedef enum
{
	LAYER_MODE_BUFFER = 0,
	LAYER_MODE_COLOR = 1,
}disp_layer_mode;

typedef enum
{
	DISP_SCAN_PROGRESSIVE                 = 0,//non interlace
	DISP_SCAN_INTERLACED_ODD_FLD_FIRST    = 1 << 0,//interlace ,odd field first
	DISP_SCAN_INTERLACED_EVEN_FLD_FIRST   = 1 << 1,//interlace,even field first
}disp_scan_flags;

typedef struct
{
	unsigned int type;
	unsigned int mode;
}disp_output;

typedef struct
{
	long long x;
	long long y;
	long long width;
	long long height;
}disp_rect64;

typedef struct
{
	unsigned long long   addr[3];          /* address of frame buffer,
																						single addr for interleaved fomart,
																						double addr for semi-planar fomart
																						triple addr for planar format */
	disp_rectsz           size[3];          //size for 3 component,unit: pixels
	unsigned int          align[3];         //align for 3 comonent,unit: bytes(align=2^n,i.e. 1/2/4/8/16/32..)
	disp_pixel_format     format;
	disp_color_space      color_space;      //color space
	unsigned int          trd_right_addr[3];/* right address of 3d fb,
																						used when in frame packing 3d mode */
	bool                  pre_multiply;     //true: pre-multiply fb
	disp_rect64           crop;             //crop rectangle boundaries
	disp_buffer_flags     flags;            //indicate stereo or non-stereo buffer
	disp_scan_flags       scan;             //scan type & scan order
}disp_fb_info;

typedef struct
{
	disp_layer_mode           mode;
	unsigned char             zorder;      /*specifies the front-to-back ordering of the layers on the screen,
											 the top layer having the highest Z value
											 can't set zorder, but can get */
	unsigned char             alpha_mode;  //0: pixel alpha;  1: global alpha;  2: global pixel alpha
	unsigned char             alpha_value; //global alpha value
	disp_rect                 screen_win;  //display window on the screen
	bool                      b_trd_out;   //3d display
	disp_3d_out_mode          out_trd_mode;//3d display mode
	union {
		unsigned int            color;       //valid when LAYER_MODE_COLOR
		disp_fb_info            fb;          //framebuffer, valid when LAYER_MODE_BUFFER
	};

	unsigned int              id;          /* frame id, can get the id of frame display currently
																						by DISP_LAYER_GET_FRAME_ID */
}disp_layer_info;

typedef struct
{
	disp_layer_info info;
	bool enable;
	unsigned int channel;
	unsigned int layer_id;
}disp_layer_config;

typedef struct
{
    disp_color      ck_max;
    disp_color      ck_min;
    unsigned int    red_match_rule;//0/1:always match; 2:match if min<=color<=max; 3:match if color>max or color<min
    unsigned int    green_match_rule;//0/1:always match; 2:match if min<=color<=max; 3:match if color>max or color<min
    unsigned int    blue_match_rule;//0/1:always match; 2:match if min<=color<=max; 3:match if color>max or color<min
}disp_colorkey;

typedef struct
{
	disp_pixel_format format;
	disp_rectsz size[3];
	disp_rect crop;
	unsigned long long addr[3];
}disp_s_frame;

typedef struct
{
	disp_rect window;  // capture window, rectangle of screen to be captured
	                          //capture the whole screen if window eq ZERO
	disp_s_frame out_frame;
}disp_capture_info;

typedef struct
{
	unsigned int    vic;  //video infomation code
	unsigned int	tv_mode;
	unsigned int    pixel_clk;
	unsigned int    pixel_repeat;//pixel repeat (pixel_repeat+1) times
	unsigned int    x_res;
	unsigned int    y_res;
	unsigned int    hor_total_time;
	unsigned int    hor_back_porch;
	unsigned int    hor_front_porch;
	unsigned int    hor_sync_time;
	unsigned int    ver_total_time;
	unsigned int    ver_back_porch;
	unsigned int    ver_front_porch;
	unsigned int    ver_sync_time;
	unsigned int    hor_sync_polarity;//0: negative, 1: positive
	unsigned int    ver_sync_polarity;//0: negative, 1: positive
	bool            b_interlace;
	unsigned int    vactive_space;
	unsigned int    trd_mode;
}disp_video_timings;

typedef enum
{
	FB_MODE_SCREEN0 = 0,
	FB_MODE_SCREEN1 = 1,
	FB_MODE_SCREEN2 = 2,
	FB_MODE_DUAL_SAME_SCREEN_TB = 3,//two screen, top buffer for screen0, bottom buffer for screen1
	FB_MODE_DUAL_DIFF_SCREEN_SAME_CONTENTS = 4,//two screen, they have same contents;
}disp_fb_mode;

typedef struct
{
	disp_fb_mode       fb_mode;
	disp_layer_mode    mode;
	unsigned int       buffer_num;
	unsigned int       width;
	unsigned int       height;

	unsigned int       output_width;//used when scaler mode
	unsigned int       output_height;//used when scaler mode
}disp_fb_create_info;

typedef enum
{
	DISP_INIT_MODE_SCREEN0 = 0,//fb0 for screen0
	DISP_INIT_MODE_SCREEN1 = 1,//fb0 for screen1
	DISP_INIT_MODE_SCREEN2 = 2,//fb0 for screen1
	DISP_INIT_MODE_TWO_DIFF_SCREEN = 3,//fb0 for screen0 and fb1 for screen1
	DISP_INIT_MODE_TWO_SAME_SCREEN = 4,//fb0(up buffer for screen0, down buffer for screen1)
	DISP_INIT_MODE_TWO_DIFF_SCREEN_SAME_CONTENTS = 5,//fb0 for two different screen(screen0 layer is normal layer, screen1 layer is scaler layer);
}disp_init_mode;

typedef struct
{
	int (*hdmi_open)(void);
	int (*hdmi_close)(void);
	int (*hdmi_set_mode)(disp_tv_mode mode);
	int (*hdmi_mode_support)(disp_tv_mode mode);
	int (*hdmi_get_HPD_status)(void);
	int (*hdmi_set_pll)(unsigned int pll, unsigned int clk);
	int (*hdmi_dvi_enable)(unsigned int mode);
	int (*hdmi_dvi_support)(void);
	int (*hdmi_get_input_csc)(void);
	int (*hdmi_get_hdcp_enable)(void);
	int (*hdmi_get_video_timing_info)(disp_video_timings **video_info);
	int (*hdmi_suspend)(void);
	int (*hdmi_resume)(void);
	int (*hdmi_early_suspend)(void);
	int (*hdmi_late_resume)(void);
	int (*hdmi_get_edid)(void);
}disp_hdmi_func;
typedef struct
{
		int (*tv_enable)(u32 sel);
		int (*tv_disable)(u32 sel);
		int (*tv_suspend)(void);
		int (*tv_resume)(void);
		int (*tv_get_mode)(u32 sel);
		int (*tv_set_mode)(u32 sel, disp_tv_mode tv_mod);
		int (*tv_get_input_csc) (void);
		int (* tv_get_video_timing_info) (u32 sel, disp_video_timings **video_info);
		int (*tv_mode_support) (disp_tv_mode mode);
		int (*tv_hot_plugging_detect)(u32 state);
		int (*tv_set_enhance_mode)(u32 sel, u32 mode);

}disp_tv_func;

/* disp_vdevice_interface_para - vdevice interaface parameter
 *
 * @intf:interface
 * 	0:hv, 1:cpu, 3:lvds, 4:dsi
 * @sub_intf:  sub interface
 * 	rgb interface: 0:parallel hv, 8:serial hv, 10:dummy rgb, 11: rgb dummy, 12: ccir656
 *	cpu interface: 0:18 pin, 10:9pin, 12:6pin, 8:16pin, 14:8pin
 *	lvds interface:0:single link, 1:dual link
 *	dsi inerafce:   0:video mode, 1:command mode, 2: video burst mode
 * @sequence:output sequence
 * 	rgb output: 0:rgb rgb, 1:rgb brg, 2:rgb gbr, 4:brg rgb, 5:brg brg, 6:brg gbr
 *	8:grb rgb, 9:grb brg, 10:grb gbr
 *	yuv output:0:yuyv, 1: yvyu, 2:uyvy, 3:vyuy
 * @fdelay:yuv eav/sav F line delay
 * 	0: F toggle right after active video line
 *	1: delay 2 line(CCIR NTSC)
 *	2: delay 3 line(CCIR PAL)
 * @clk_phase:clk phase
 * 	0: 0 degree, 1:90 degree, 2: 180 degree, 3:270 degree
 * @sync_polarity:sync signals polarity
 * 	0: vsync active low,hsync active low
 *	1: vsync active high,hsync active low
 *	2: vsync active low,hsync active high
 *	3: vsync active high,hsync active high
 */
typedef struct {
	unsigned int intf;
	unsigned int sub_intf;
	unsigned int sequence;
	unsigned int fdelay;
	unsigned int clk_phase;
	unsigned int sync_polarity;
}disp_vdevice_interface_para;

typedef struct
{
	int (*tcon_enable)(struct disp_device *dispdev);
	int (*tcon_disable)(struct disp_device *dispdev);
	int (*tcon_simple_enable)(struct disp_device *dispdev);
	int (*tcon_simple_disable)(struct disp_device *dispdev);
}disp_vdevice_source_ops;

typedef struct
{
	int (*enable)(void);
	int (*disable)(void);
	int (*set_mode)(u32 mode);
	int (*mode_support)(u32 mode);
	int (*get_HPD_status)(void);
	int (*get_input_csc)(void);
	int (*get_video_timing_info)(disp_video_timings **video_info);
	int (*suspend)(void);
	int (*resume)(void);
	int (*early_suspend)(void);
	int (*late_resume)(void);
	int (*get_interface_para)(void *para);
}disp_device_func;

typedef struct
{
	char name[32];
	u32 disp;
	u32 fix_timing;
	disp_output_type type;
	disp_device_func func;
}disp_vdevice_init_data;

typedef enum
{
    DISP_TV_DAC_SRC_COMPOSITE       = 0,
    DISP_TV_DAC_SRC_LUMA            = 1,
    DISP_TV_DAC_SRC_CHROMA          = 2,
    DISP_TV_DAC_SRC_Y               = 4,
    DISP_TV_DAC_SRC_PB              = 5,
    DISP_TV_DAC_SRC_PR              = 6,
    DISP_TV_DAC_SRC_NONE            = 7,
}disp_tv_dac_source;

typedef enum
{
    DISP_TV_NONE    = 0,
    DISP_TV_CVBS    = 1,
    DISP_TV_YPBPR   = 2,
    DISP_TV_SVIDEO  = 4,
}disp_tv_output;

/**
 * enum __DISP_t - Display subsystem ioctl commands
 *
 * Comprehensive set of ioctl commands for /dev/disp device. Commands are
 * grouped by functional area with reserved ranges for future expansion.
 *
 * Command groups:
 * - 0x00-0x3F: Global display control (output routing, vsync, blanking)
 * - 0x40-0x7F: Layer management (enable, configure, z-order)
 * - 0xC0-0xFF: HDMI/TV output control
 * - 0x100-0x13F: LCD panel control (brightness, backlight, gamma)
 * - 0x140-0x17F: Screen capture (framebuffer readback)
 * - 0x180-0x1FF: Image enhancement (contrast, saturation)
 * - 0x200-0x23F: Smart backlight management
 * - 0x280-0x2BF: Framebuffer allocation (testing)
 * - 0x2C0-0x2FF: Memory management (testing)
 *
 * Usage: ioctl(fd, command, argument_pointer)
 */
typedef enum tag_DISP_CMD
{
	///////////////////////////////
	// Global display control
	///////////////////////////////
	DISP_RESERVE0 = 0x00,
	DISP_RESERVE1 = 0x01,
	DISP_SET_BKCOLOR = 0x03,           // Set background color
	DISP_GET_BKCOLOR = 0x04,           // Get background color
	DISP_SET_COLORKEY = 0x05,          // Set transparent color key
	DISP_GET_COLORKEY = 0x06,          // Get color key settings
	DISP_GET_SCN_WIDTH = 0x07,         // Get screen width
	DISP_GET_SCN_HEIGHT = 0x08,        // Get screen height
	DISP_GET_OUTPUT_TYPE = 0x09,       // Get current output type (LCD/HDMI/etc)
	DISP_SET_EXIT_MODE = 0x0A,         // Set cleanup mode on exit
	DISP_VSYNC_EVENT_EN = 0x0B,        // Enable vsync interrupt events
	DISP_BLANK = 0x0C,                 // Blank/unblank display
	DISP_SHADOW_PROTECT = 0x0D,        // Atomic update protection
	DISP_HWC_COMMIT = 0x0E,            // Commit hardware composer changes
	DISP_DEVICE_SWITCH = 0x0F,         // Switch output device
	DISP_GET_OUTPUT = 0x10,            // Get output configuration
	DISP_SET_COLOR_RANGE = 0x11,       // Set RGB color range (full/limited)
	DISP_GET_COLOR_RANGE = 0x12,       // Get color range

	///////////////////////////////
	// Layer management
	///////////////////////////////
	DISP_LAYER_ENABLE = 0x40,          // Enable layer
	DISP_LAYER_DISABLE = 0x41,         // Disable layer
	DISP_LAYER_SET_INFO = 0x42,        // Set layer properties
	DISP_LAYER_GET_INFO = 0x43,        // Get layer properties
	DISP_LAYER_TOP = 0x44,             // Move layer to top (highest z-order)
	DISP_LAYER_BOTTOM = 0x45,          // Move layer to bottom (lowest z-order)
	DISP_LAYER_GET_FRAME_ID = 0x46,    // Get current frame ID
	DISP_LAYER_SET_CONFIG = 0x47,      // Set layer configuration (preferred)
	DISP_LAYER_GET_CONFIG = 0x48,      // Get layer configuration

	///////////////////////////////
	// HDMI/TV control
	///////////////////////////////
	DISP_HDMI_SUPPORT_MODE = 0xc4,     // Query supported HDMI modes
	DISP_SET_TV_HPD = 0xc5,            // Set hotplug detect
	DISP_HDMI_GET_EDID = 0xc6,         // Read EDID from connected display

	///////////////////////////////
	// LCD panel control
	///////////////////////////////
	DISP_LCD_ENABLE = 0x100,           // Enable LCD output
	DISP_LCD_DISABLE = 0x101,          // Disable LCD output
	DISP_LCD_SET_BRIGHTNESS = 0x102,   // Set backlight brightness
	DISP_LCD_GET_BRIGHTNESS = 0x103,   // Get backlight brightness
	DISP_LCD_BACKLIGHT_ENABLE  = 0x104,// Enable backlight
	DISP_LCD_BACKLIGHT_DISABLE  = 0x105,// Disable backlight
	DISP_LCD_SET_SRC = 0x106,          // Set LCD data source
	DISP_LCD_SET_FPS  = 0x107,         // Set refresh rate
	DISP_LCD_GET_FPS  = 0x108,         // Get refresh rate
	DISP_LCD_GET_SIZE = 0x109,         // Get panel resolution
	DISP_LCD_GET_MODEL_NAME = 0x10a,   // Get panel model string
	DISP_LCD_SET_GAMMA_TABLE = 0x10b,  // Upload gamma correction table
	DISP_LCD_GAMMA_CORRECTION_ENABLE = 0x10c,  // Enable gamma
	DISP_LCD_GAMMA_CORRECTION_DISABLE = 0x10d, // Disable gamma
	DISP_LCD_USER_DEFINED_FUNC = 0x10e,// Call custom panel function
	DISP_LCD_CHECK_OPEN_FINISH = 0x10f,// Wait for panel initialization
	DISP_LCD_CHECK_CLOSE_FINISH = 0x110,// Wait for panel shutdown

	///////////////////////////////
	// Screen capture
	///////////////////////////////
	DISP_CAPTURE_START = 0x140,        // Start screen capture to memory
	DISP_CAPTURE_STOP = 0x141,         // Stop capture
	DISP_CAPTURE_COMMIT = 0x142,       // Commit capture operation

	///////////////////////////////
	// Image enhancement
	///////////////////////////////
	DISP_ENHANCE_ENABLE = 0x180,       // Enable enhancement engine
	DISP_ENHANCE_DISABLE = 0x181,      // Disable enhancement
	DISP_ENHANCE_GET_EN = 0x182,       // Get enhancement state
	DISP_ENHANCE_SET_WINDOW = 0x183,   // Set enhancement region
	DISP_ENHANCE_GET_WINDOW = 0x184,   // Get enhancement region
	DISP_ENHANCE_SET_MODE = 0x185,     // Set enhancement mode/strength
	DISP_ENHANCE_GET_MODE = 0x186,     // Get enhancement mode
	DISP_ENHANCE_DEMO_ENABLE = 0x187,  // Enable split-screen demo
	DISP_ENHANCE_DEMO_DISABLE = 0x188, // Disable demo mode

	///////////////////////////////
	// Smart backlight
	///////////////////////////////
	DISP_SMBL_ENABLE = 0x200,          // Enable adaptive backlight
	DISP_SMBL_DISABLE = 0x201,         // Disable adaptive backlight
	DISP_SMBL_GET_EN = 0x202,          // Get backlight state
	DISP_SMBL_SET_WINDOW = 0x203,      // Set analysis window
	DISP_SMBL_GET_WINDOW = 0x204,      // Get analysis window

	///////////////////////////////
	// Testing/debugging
	///////////////////////////////
	DISP_FB_REQUEST = 0x280,           // Allocate framebuffer
	DISP_FB_RELEASE = 0x281,           // Free framebuffer

	DISP_MEM_REQUEST = 0x2c0,          // Request memory allocation
	DISP_MEM_RELEASE = 0x2c1,          // Release memory
	DISP_MEM_GETADR = 0x2c2,           // Get memory address
}__DISP_t;

/**
 * Framebuffer device ioctl commands
 *
 * Used with /dev/fb0, /dev/fb1 to get layer handles for display configuration.
 * These allow mapping framebuffer devices to display engine layers.
 */
#define FBIOGET_LAYER_HDL_0 0x4700  // Get layer handle for primary framebuffer
#define FBIOGET_LAYER_HDL_1 0x4701  // Get layer handle for secondary framebuffer

#endif
