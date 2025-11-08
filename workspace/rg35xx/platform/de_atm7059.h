/**
 * de_atm7059.h - Actions ATM7059 Display Engine (DE) register definitions
 *
 * Defines hardware registers and bit positions for the ATM7059 SoC display
 * engine, used in the RG35xx handheld gaming device. This header provides
 * low-level access to the video output subsystem, including:
 * - Video overlay management (layers, blending, scaling)
 * - Color space conversion (RGB/YUV)
 * - Display paths and output routing
 * - Memory Management Unit (MMU) for video buffers
 * - Gamma correction tables
 *
 * Based on Actions Semiconductor kernel driver.
 *
 * @note This file should only be included by de_atm7059.c or related display driver code.
 * @warning Register addresses are memory-mapped I/O. Direct access requires kernel privileges.
 *
 * Copyright (C) 2014 Actions
 * Author: lipeng<lipeng@actions-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _DE_ATM7059_H_
#define _DE_ATM7059_H_

//#include "de.h"

/*================================================================
 *           Definition of registers and bit position
 *==============================================================*/

/**
 * DE_SIZE_BIT_WIDTH - Bit width for size fields in display engine registers
 *
 * Maximum size values use 12-bit fields (max value 4095).
 */
#define DE_SIZE_BIT_WIDTH			12

///////////////////////////////
// DE common registers - Global display engine control
///////////////////////////////

#define DE_IRQSTATUS				0x0004  // Interrupt status register
#define DE_IRQENABLE				0x0000  // Interrupt enable register
#define DE_IF_CON				0x000c  // Interface configuration

// Memory Management Unit registers for video buffer address translation
#define DE_MMU_EN				0x0010  // MMU enable control
#define DE_MMU_BASE				0x0014  // MMU page table base address

// Output and writeback control
#define DE_OUTPUT_CON				0x1000  // Output configuration (path routing)
#define DE_OUTPUT_STAT				0x100c  // Output status
#define DE_WB_CON				0x1004  // Writeback control (capture to memory)
#define DE_WB_ADDR				0x1008  // Writeback destination address

/**
 * DE_PATH_DITHER - Dither control for display path 0
 *
 * Only available on ATM7059A revision. Reduces color banding on
 * displays with limited color depth.
 */
#define DE_PATH_DITHER				0x150

///////////////////////////////
// Display path registers - Per-path output configuration
// Each display path (n) has its own register block at offset n*0x100
///////////////////////////////

#define DE_PATH_BASE				0x0100
#define DE_PATH_CTL(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0000)

// Path enable control (shares register with DE_PATH_CTL)
#define DE_PATH_EN(n)				DE_PATH_CTL(n)
#define DE_PATH_ENABLE_BIT			28  // Bit 28 enables this display path

// Frame rate control (shares register with DE_PATH_CTL)
#define DE_PATH_FCR(n)				DE_PATH_CTL(n)

// Path visual properties
#define DE_PATH_BK(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0020)  // Background color
#define DE_PATH_SIZE(n)				(DE_PATH_BASE + (n) * 0x100  +  0x0024)  // Output resolution
#define DE_PATH_E_COOR(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0028)  // End coordinates

// Gamma correction table access
#define DE_PATH_GAMMA_IDX(n)			(DE_PATH_BASE + (n) * 0x100  +  0x002C)  // Gamma table index register
#define DE_PATH_GAMMA_IDX_BUSY_BIT		(14)      // Gamma operation in progress
#define DE_PATH_GAMMA_IDX_OP_SEL_BEGIN_BIT	(12)      // Operation select (read/write) start bit
#define DE_PATH_GAMMA_IDX_OP_SEL_END_BIT	(13)      // Operation select end bit
#define DE_PATH_GAMMA_IDX_INDEX_BEGIN_BIT	(0)       // Table index start bit (0-255)
#define DE_PATH_GAMMA_IDX_INDEX_END_BIT		(7)       // Table index end bit

#define DE_PATH_GAMMA_RAM(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0030)  // Gamma correction data

// Hardware cursor support
#define DE_PATH_CURSOR_FB(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0034)  // Cursor framebuffer address
#define DE_PATH_CURSOR_STR(n)			(DE_PATH_BASE + (n) * 0x100  +  0x0038)  // Cursor stride (bytes per row)

///////////////////////////////
// Video overlay registers - Layer composition and scaling
// Each overlay (n) has its own register block at offset n*0x100
// Overlays are composited together to create the final output
///////////////////////////////

#define DE_OVL_BASE				0x0400

// Overlay configuration and sizing
#define DE_OVL_CFG(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0000)  // Config (format, CSC, flip)
#define DE_OVL_ISIZE(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0004)  // Input size
#define DE_OVL_OSIZE(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0008)  // Output size (after scaling)
#define DE_OVL_SR(n)				(DE_OVL_BASE + (n) * 0x100  +  0x000c)  // Scaling ratio

// Scaling coefficients (8 coefficient registers for filter kernel)
#define DE_OVL_SCOEF0(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0010)
#define DE_OVL_SCOEF1(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0014)
#define DE_OVL_SCOEF2(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0018)
#define DE_OVL_SCOEF3(n)			(DE_OVL_BASE + (n) * 0x100  +  0x001c)
#define DE_OVL_SCOEF4(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0020)
#define DE_OVL_SCOEF5(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0024)
#define DE_OVL_SCOEF6(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0028)
#define DE_OVL_SCOEF7(n)			(DE_OVL_BASE + (n) * 0x100  +  0x002c)

// Framebuffer addresses (planar formats use multiple buffers)
#define DE_OVL_BA0(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0030)  // Y or RGB buffer
#define DE_OVL_BA1UV(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0034)  // U/UV buffer
#define DE_OVL_BA2V(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0038)  // V buffer

// 3D stereo support (right eye framebuffers for frame-packed 3D)
#define DE_OVL_3D_RIGHT_BA0(n)			(DE_OVL_BASE + (n) * 0x100  +  0x003C)
#define DE_OVL_3D_RIGHT_BA1UV(n)		(DE_OVL_BASE + (n) * 0x100  +  0x0040)
#define DE_OVL_3D_RIGHT_BA2V(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0044)

// Overlay properties
#define DE_OVL_STR(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0048)  // Stride (pitch)
#define DE_OVL_CRITICAL_CFG(n)			(DE_OVL_BASE + (n) * 0x100  +  0x004c)  // Critical watermark
#define DE_OVL_REMAPPING(n)			(DE_OVL_BASE + (n) * 0x100  +  0x0050)  // Color remapping
#define DE_OVL_COOR(m, n)			(DE_OVL_BASE + (n) * 0x100  +  0x0054)  // Screen coordinates

// Alpha blending and transparency
#define DE_OVL_ALPHA_CFG(m, n)			(DE_OVL_BASE + (n) * 0x100  +  0x0058)  // Alpha configuration
#define DE_OVL_CKMAX(n)				(DE_OVL_BASE + (n) * 0x100  +  0x005c)  // Color key max value
#define DE_OVL_CKMIN(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0060)  // Color key min value
#define DE_OVL_BLEND(n)				(DE_OVL_BASE + (n) * 0x100  +  0x0064)  // Blend mode
#define DE_OVL_ALPHA_ENABLE(m, n)		DE_OVL_BLEND(n)              // Alpha enable (alias)

// Gamma enable control (shares register with path control)
#define DE_PATH_GAMMA_ENABLE(n)			DE_PATH_CTL(n)
#define DE_PATH_GAMMA_ENABLE_BIT		(9)  // Bit 9 of path control

///////////////////////////////
// Bit field definitions for overlay and path registers
///////////////////////////////

// Color Space Conversion (CSC) bit fields in DE_OVL_CFG
#define DE_OVL_CSC(n)				DE_OVL_CFG(n)           // CSC register (alias)
#define DE_OVL_CSC_CON_BEGIN_BIT		4   // Contrast adjustment start bit
#define DE_OVL_CSC_CON_END_BIT			7   // Contrast adjustment end bit
#define DE_OVL_CSC_STA_BEGIN_BIT		8   // Saturation adjustment start bit
#define DE_OVL_CSC_STA_END_BIT			11  // Saturation adjustment end bit
#define DE_OVL_CSC_BRI_BEGIN_BIT		12  // Brightness adjustment start bit
#define DE_OVL_CSC_BRI_END_BIT			19  // Brightness adjustment end bit
#define DE_OVL_CSC_BYPASS_BIT			0   // Bypass CSC (use raw input)

// Overlay configuration bit fields
#define DE_OVL_CFG_FLIP_BIT			20  // Vertical flip enable
#define DE_OVL_CFG_FMT_BEGIN_BIT		0   // Pixel format select start bit
#define DE_OVL_CFG_FMT_END_BIT			2   // Pixel format select end bit
#define DE_OVL_CFG_BYPASS_BIT			3   // Bypass overlay processing
#define DE_OVL_CFG_CONTRAST_BEGIN_BIT       4   // Contrast value start bit
#define DE_OVL_CFG_CONTRAST_END_BIT         7   // Contrast value end bit
#define DE_OVL_CFG_SATURATION_BEGIN_BIT     8   // Saturation value start bit
#define DE_OVL_CFG_SATURATION_END_BIT       11  // Saturation value end bit
#define DE_OVL_CFG_LIGHTNESS_BEGIN_BIT      12  // Lightness/brightness start bit
#define DE_OVL_CFG_LIGHTNESS_END_BIT        19  // Lightness/brightness end bit
#define DE_OVL_CFG_CRITICAL_CTL_BEGIN_BIT	26  // Critical timing control start bit
#define DE_OVL_CFG_CRITICAL_CTL_END_BIT		27  // Critical timing control end bit

// Alpha blending configuration bit fields
#define DE_OVL_ALPHA_CFG_PRE_MUTI_BIT		8   // Pre-multiplied alpha mode
#define DE_OVL_ALPHA_CFG_VALUE_BEGIN_BIT	0   // Global alpha value start bit (0-255)
#define DE_OVL_ALPHA_CFG_VALUE_END_BIT		7   // Global alpha value end bit
#define DE_OVL_ALPHA_CFG_ENABLE_BEGIN_BIT	0   // Alpha enable start bit
#define DE_OVL_ALPHA_CFG_ENABLE_END_BIT		0   // Alpha enable end bit

// Output path routing bit fields (which device each path connects to)
#define DE_OUTPUT_PATH1_DEVICE_BEGIN_BIT	0   // Path 1 output device select start bit
#define DE_OUTPUT_PATH1_DEVICE_END_BIT		2   // Path 1 output device select end bit
#define DE_OUTPUT_PATH2_DEVICE_BEGIN_BIT	4   // Path 2 output device select start bit
#define DE_OUTPUT_PATH2_DEVICE_END_BIT		6   // Path 2 output device select end bit

// Display path control bit fields
#define DE_PATH_CTL_IYUV_QEN_BIT		16  // YUV queue enable
#define DE_PATH_CTL_YUV_FMT_BIT			15  // YUV format select
#define DE_PATH_CTL_ILACE_BIT			11  // Interlaced mode enable
#define DE_PATH_CTL_GAMMA_ENABLE_BIT		9   // Gamma correction enable

// Panel and cursor control bit fields
#define DE_PANEL_ENABLE_BIT			20  // LCD panel enable
#define DE_PANEL_CURSOR_ENABLE_BIT	24  // Hardware cursor enable
#define DE_PATH_FCR_BIT				29  // Frame rate control enable

#endif
