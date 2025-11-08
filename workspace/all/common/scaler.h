/**
 * scaler.h - Integer pixel scaling API for MinUI
 *
 * Provides NEON-optimized and portable C implementations of integer scaling
 * functions for RGB565 (16bpp) and RGBA8888 (32bpp) pixel formats.
 *
 * These scalers are used to upscale emulator video output to the physical
 * screen resolution. They support various integer scale factors (1x-6x)
 * in both horizontal and vertical directions.
 *
 * NEON functions (scale*_n16, scale*_n32):
 *   - Available when HAS_NEON is defined
 *   - Use ARM NEON SIMD instructions for ~2-4x speedup
 *   - Require 32-bit aligned addresses and even pixel counts
 *   - Fall back to C scalers for odd dimensions
 *
 * C functions (scale*_c16, scale*_c32):
 *   - Portable implementations
 *   - Work on any platform
 *   - Handle all pixel dimensions
 *
 * Function naming convention:
 *   scale<xmul>x<ymul>_[n|c][16|32]
 *   - xmul: horizontal scale factor (1-6)
 *   - ymul: vertical scale factor (1-6)
 *   - n/c: NEON or C implementation
 *   - 16/32: bits per pixel (16=RGB565, 32=RGBA8888)
 *
 * Parameter conventions:
 *   src: Source pixel buffer (top-left corner address)
 *   dst: Destination pixel buffer (top-left corner address)
 *   sw:  Source width in pixels
 *   sh:  Source height in pixels
 *   sp:  Source pitch/stride in bytes (0 = auto-calculate from width)
 *   dw:  Destination width in pixels
 *   dh:  Destination height in pixels
 *   dp:  Destination pitch/stride in bytes (0 = auto-calculate)
 *
 * IMPORTANT: For NEON scalers with 16bpp:
 *   - X-offset and stride must be even numbers (for 32-bit alignment)
 *   - Odd dimensions automatically fall back to C scaler
 */

#ifndef __SCALER_H__
#define __SCALER_H__
#include <stdint.h>

/**
 * Scaler function pointer type.
 *
 * All scaler functions conform to this signature for easy function
 * pointer storage and dynamic selection.
 */
typedef void (*scaler_t)(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh,
                         uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

///////////////////////////////
// Generic scaler dispatchers
///////////////////////////////

/**
 * Generic NEON scaler dispatcher for 16bpp (RGB565).
 *
 * Selects the appropriate NEON scaler based on scale factors.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-4 for xmul<5, 1-5 for xmul==5, 1-6 for xmul==6)
 * @param src Source pixel buffer
 * @param dst Destination pixel buffer
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch in bytes (0 for auto)
 * @param dw Destination width in pixels
 * @param dh Destination height in pixels
 * @param dp Destination pitch in bytes (0 for auto)
 */
#ifdef HAS_NEON
void scaler_n16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

/**
 * Generic NEON scaler dispatcher for 32bpp (RGBA8888).
 *
 * Selects the appropriate NEON scaler based on scale factors.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-4 for xmul<5, 1-5 for xmul==5, 1-6 for xmul==6)
 * @param src Source pixel buffer
 * @param dst Destination pixel buffer
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch in bytes (0 for auto)
 * @param dw Destination width in pixels
 * @param dh Destination height in pixels
 * @param dp Destination pitch in bytes (0 for auto)
 */
void scaler_n32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);
#endif

/**
 * Generic C scaler dispatcher for 16bpp (RGB565).
 *
 * Selects the appropriate C scaler based on scale factors.
 * Works on all platforms.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-6)
 * @param src Source pixel buffer
 * @param dst Destination pixel buffer
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch in bytes (0 for auto)
 * @param dw Destination width in pixels
 * @param dh Destination height in pixels
 * @param dp Destination pitch in bytes (0 for auto)
 */
void scaler_c16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

/**
 * Generic C scaler dispatcher for 32bpp (RGBA8888).
 *
 * Selects the appropriate C scaler based on scale factors.
 * Works on all platforms.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-6)
 * @param src Source pixel buffer
 * @param dst Destination pixel buffer
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch in bytes (0 for auto)
 * @param dw Destination width in pixels
 * @param dh Destination height in pixels
 * @param dp Destination pitch in bytes (0 for auto)
 */
void scaler_c32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

///////////////////////////////
// NEON-optimized functions
///////////////////////////////

#ifdef HAS_NEON

/**
 * NEON-optimized memcpy.
 *
 * Faster than standard memcpy on ARM NEON platforms.
 *
 * @param dst Destination buffer
 * @param src Source buffer
 * @param size Number of bytes to copy
 */
void memcpy_neon(void* dst, void* src, uint32_t size);

/**
 * NEON scalers with variable Y multiplier.
 *
 * These functions scale horizontally by a fixed factor (1x-6x)
 * and vertically by a variable factor specified by ymul parameter.
 *
 * Example: scale2x_n16(..., ymul=3) performs 2x3 scaling.
 */
void scale1x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale1x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale2x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale2x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale3x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale3x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale4x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale4x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale5x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale5x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale6x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale6x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);

/**
 * NEON scalers with fixed scale factors.
 *
 * These functions perform integer scaling with specific X and Y multipliers.
 * Naming: scale<X>x<Y>_n<bpp> where X is horizontal, Y is vertical scale.
 *
 * Available combinations:
 * - 1x1 through 6x6 (most combinations)
 * - Optimized for common game console resolutions
 */
void scale1x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x6_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x6_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
#endif

///////////////////////////////
// Format conversion scalers
///////////////////////////////

/**
 * 16bpp to 32bpp conversion scalers.
 *
 * These convert RGB565 to RGBA8888 while scaling.
 * Useful for platforms that require 32bpp output.
 */
void scale1x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh,
                     uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh,
                     uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp);

///////////////////////////////
// Portable C scalers
///////////////////////////////

/**
 * C scalers with variable Y multiplier.
 *
 * Portable implementations that work on all platforms.
 * Same functionality as NEON versions but without SIMD optimization.
 */
void scale1x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale1x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale2x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale2x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale3x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale3x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale4x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale4x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale5x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale5x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale6x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);
void scale6x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul);

/**
 * C scalers with fixed scale factors.
 *
 * Portable implementations with specific X and Y multipliers.
 * Same naming convention as NEON versions: scale<X>x<Y>_c<bpp>
 */
void scale1x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale1x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale5x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x6_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale6x6_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);

///////////////////////////////
// CRT effect scalers
///////////////////////////////

/**
 * Scanline effect scalers.
 *
 * These scalers add horizontal scanlines (dark lines between rows)
 * to simulate a CRT display. The _line suffix indicates scanline effect.
 *
 * Available for 1x-4x scaling factors.
 */
void scale1x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale2x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale4x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);

/**
 * Grid effect scalers.
 *
 * These scalers add a grid pattern (dark lines between both rows and columns)
 * to simulate a CRT shadow mask. The _grid suffix indicates grid effect.
 *
 * Available for 2x and 3x scaling factors.
 */
void scale2x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);
void scale3x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp);

#endif
