/**
 * scaler.c - Hardware-accelerated video scaling for retro handheld devices
 *
 * Provides integer pixel scaling functions optimized for ARM NEON SIMD instructions.
 * These scalers handle the common task of upscaling low-resolution game video
 * (e.g., 160x144 Game Boy) to higher-resolution device screens (e.g., 640x480).
 *
 * The scaler supports:
 * - Multiple scaling factors (1x through 6x)
 * - Both 16-bit (RGB565) and 32-bit (RGBA8888) pixel formats
 * - Format conversion (16-bit source to 32-bit destination)
 * - ARM NEON hardware acceleration with C fallbacks
 * - Non-uniform Y-axis scaling (e.g., 2x horizontal, 3x vertical)
 *
 * Platform Support:
 * - NEON versions (_n16/_n32): ARMv7 devices with NEON SIMD (most handhelds)
 * - C versions (_c16/_c32): Fallback for non-ARM or unaligned buffers
 * - Conversion versions (c16to32): Trimui Model S and GKD Pixel
 *
 * Performance: NEON implementations are 2-4x faster than C on supported hardware.
 *
 * Function naming convention: scale{X}x{Y}_{type}{bpp}
 * - X: horizontal scale factor (1-6)
 * - Y: vertical scale factor (1-6, or omitted for variable)
 * - type: 'n' (NEON), 'c' (C), or omitted
 * - bpp: '16' (RGB565), '32' (RGBA8888), or '16to32' (conversion)
 *
 * Common parameters for all scaling functions:
 * @param src Source buffer (top-left pixel address)
 * @param dst Destination buffer (top-left pixel address)
 * @param sw Source width in pixels
 * @param sh Source height in pixels
 * @param sp Source pitch (stride) in bytes, or 0 for (sw * bytes_per_pixel)
 * @param dw Destination width in pixels (often unused, for API compatibility)
 * @param dh Destination height in pixels (often unused, for API compatibility)
 * @param dp Destination pitch (stride) in bytes, or 0 for auto-calculated
 *
 * @note NEON scalers require 32-bit aligned addresses and even pixel counts
 *       for 16bpp. Unaligned inputs automatically fall back to C scalers.
 * @warning Source and destination buffers must not overlap
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform.h" // for HAS_NEON

/**
 * No-op scaler function (unused placeholder).
 */
static void dummy(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {}

///////////////////////////////
// Format Conversion Scalers (RGB565 to RGBA8888)
///////////////////////////////

/**
 * 1x scale with RGB565 to RGBA8888 color conversion (C implementation).
 *
 * Converts 16-bit RGB565 pixels to 32-bit RGBA8888 format without scaling.
 * Used on devices that require 32-bit framebuffers but receive 16-bit input
 * from libretro cores (e.g., Trimui Model S, GKD Pixel).
 *
 * RGB565 format: RRRRRGGGGGGBBBBB (16 bits)
 * RGBA8888 format: RRRRRRRRGGGGGGGGBBBBBBBB11111111 (32 bits, alpha=0xFF)
 *
 * The conversion expands:
 * - 5-bit red to 8-bit by shifting left 3 bits
 * - 6-bit green to 8-bit by shifting left 2 bits
 * - 5-bit blue to 8-bit by shifting left 3 bits
 * - Alpha channel set to 0xFF (fully opaque)
 *
 * @note Processes pixels in pairs (two RGB565 = one 32-bit read) for efficiency
 * @note Odd-width images handled separately with single-pixel processing
 */
void scale1x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh,
                     uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl; // Default source pitch
	}
	swl *= 2; // Destination line is 2x wider (32bpp vs 16bpp)
	if (!dp) {
		dp = swl; // Default destination pitch
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		// Process pixels in pairs (read 32 bits = 2 RGB565 pixels)
		for (x = dx = 0; x < (sw / 2); x++, dx += 2) {
			pix = s[x]; // Contains two RGB565 pixels
			// Convert first pixel (lower 16 bits)
			dpix1 =
			    0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			// Convert second pixel (upper 16 bits)
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) |
			        ((pix & 0x001F0000) >> 13);
			d[dx] = dpix1;
			d[dx + 1] = dpix2;
		}
		// Handle odd-width case (last pixel if width is odd)
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t pix16 = s16[x * 2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) |
			        ((pix16 & 0x001F) << 3);
			d[dx] = pix16;
			d[dx + 1] = pix16;
		}
		dst = (uint8_t*)dst + dp;
	}
}

/**
 * 2x scale with RGB565 to RGBA8888 color conversion (C implementation).
 *
 * Scales image 2x in both dimensions while converting from 16-bit to 32-bit.
 * Each source pixel becomes a 2x2 block of identical destination pixels.
 * Duplicates each output line using memcpy for efficiency.
 *
 * @note Output is 2x wider and 2x taller than input
 */
void scale2x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh,
                     uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 2;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < (sw / 2); x++, dx += 4) {
			pix = s[x];
			dpix1 =
			    0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) |
			        ((pix & 0x001F0000) >> 13);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d[dx + 2] = dpix2;
			d[dx + 3] = dpix2;
		}
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t pix16 = s16[x * 2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) |
			        ((pix16 & 0x001F) << 3);
			d[dx] = pix16;
			d[dx + 1] = pix16;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		memcpy(dst, dstsrc, swl); // Duplicate first line to create 2x vertical
		dst = (uint8_t*)dst + dp;
	}
}

///////////////////////////////
// Generic C Scalers (RGB565 and RGBA8888)
///////////////////////////////

/**
 * Generic 1x horizontal scale with variable vertical scaling (16-bit C version).
 *
 * Implements point sampling (nearest-neighbor) scaling with no horizontal scaling
 * and configurable vertical duplication. Each source line is copied to the destination
 * and optionally repeated ymul times.
 *
 * Optimization: If source and destination have matching pitches and ymul=1,
 * performs a single bulk memcpy for the entire image.
 *
 * @param ymul Vertical scale factor (number of times to repeat each line)
 *
 * @note This is the core implementation for scale1x1_c16, scale1x2_c16, etc.
 */
void scale1x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 1;
	}
	if ((ymul == 1) && (swl == sp) && (sp == dp))
		memcpy(dst, src, sp * sh);
	else {
		if (swl > dp)
			swl = dp;
		for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
			for (uint32_t i = ymul; i > 0; i--, dst = (uint8_t*)dst + dp)
				memcpy(dst, src, swl);
		}
	}
}

/**
 * 1x1 scale (no scaling, direct copy) for 16-bit pixels.
 */
void scale1x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}

/**
 * 1x2 scale (1x horizontal, 2x vertical) for 16-bit pixels.
 */
void scale1x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}

/**
 * 1x3 scale (1x horizontal, 3x vertical) for 16-bit pixels.
 */
void scale1x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}

/**
 * 1x4 scale (1x horizontal, 4x vertical) for 16-bit pixels.
 */
void scale1x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/**
 * Generic 1x horizontal scale with variable vertical scaling (32-bit C version).
 *
 * Same as scale1x_c16 but for 32-bit RGBA8888 pixels.
 *
 * @param ymul Vertical scale factor (number of times to repeat each line)
 */
void scale1x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 1;
	}
	if ((ymul == 1) && (swl == sp) && (sp == dp))
		memcpy(dst, src, sp * sh);
	else {
		for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
			for (uint32_t i = ymul; i > 0; i--, dst = (uint8_t*)dst + dp)
				memcpy(dst, src, swl);
		}
	}
}

/**
 * 1x1 scale (no scaling, direct copy) for 32-bit pixels.
 */
void scale1x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}

/**
 * 1x2 scale (1x horizontal, 2x vertical) for 32-bit pixels.
 */
void scale1x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}

/**
 * 1x3 scale (1x horizontal, 3x vertical) for 32-bit pixels.
 */
void scale1x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}

/**
 * 1x4 scale (1x horizontal, 4x vertical) for 32-bit pixels.
 */
void scale1x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/**
 * Generic 2x horizontal scale with variable vertical scaling (16-bit C version).
 *
 * Scales image 2x horizontally by duplicating each pixel, with configurable
 * vertical scaling. Each source pixel is read once and written twice horizontally.
 * Each output line is then duplicated ymul times vertically.
 *
 * Algorithm: Reads pixels in pairs (32-bit read = 2 RGB565 pixels) and
 * duplicates each to create 4 output pixels per iteration.
 *
 * @param ymul Vertical scale factor (number of times to repeat each line)
 *
 * @note Output width is 2x source width, height is ymul * source height
 */
void scale2x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 2;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		// Process pairs of RGB565 pixels (read 32 bits = 2 pixels)
		for (x = dx = 0; x < (sw / 2); x++, dx += 2) {
			pix = s[x]; // Contains pixels A and B: [B][A]
			// Create [A][A] by taking lower 16 bits and duplicating
			dpix1 = (pix & 0x0000FFFF) | (pix << 16);
			// Create [B][B] by taking upper 16 bits and duplicating
			dpix2 = (pix & 0xFFFF0000) | (pix >> 16);
			d[dx] = dpix1; // Output: [A][A]
			d[dx + 1] = dpix2; // Output: [B][B]
		}
		// Handle odd-width case (last pixel if width is odd)
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t pix16 = s16[x * 2];
			d[dx] = pix16 | (pix16 << 16); // Duplicate single pixel
		}
		// Duplicate the scaled line ymul-1 additional times
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

/**
 * 2x1 scale (2x horizontal, 1x vertical) for 16-bit pixels.
 */
void scale2x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}

/**
 * 2x2 scale (2x horizontal, 2x vertical) for 16-bit pixels.
 */
void scale2x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}

/**
 * 2x3 scale (2x horizontal, 3x vertical) for 16-bit pixels.
 */
void scale2x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}

/**
 * 2x4 scale (2x horizontal, 4x vertical) for 16-bit pixels.
 */
void scale2x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/**
 * Generic 2x horizontal scale with variable vertical scaling (32-bit C version).
 *
 * Same algorithm as scale2x_c16 but for 32-bit RGBA8888 pixels.
 * Simpler than 16-bit version since one read = one pixel (no pairs).
 *
 * @param ymul Vertical scale factor (number of times to repeat each line)
 */
void scale2x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 2;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		// Simpler than 16-bit: just duplicate each pixel directly
		for (x = dx = 0; x < sw; x++, dx += 2) {
			pix = s[x];
			d[dx] = pix;
			d[dx + 1] = pix;
		}
		// Duplicate the scaled line ymul-1 additional times
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

/** 2x1 scale (2x horizontal, 1x vertical) for 32-bit pixels. */
void scale2x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
/** 2x2 scale (2x horizontal, 2x vertical) for 32-bit pixels. */
void scale2x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
/** 2x3 scale (2x horizontal, 3x vertical) for 32-bit pixels. */
void scale2x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
/** 2x4 scale (2x horizontal, 4x vertical) for 32-bit pixels. */
void scale2x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/**
 * Generic 3x horizontal scale with variable vertical scaling (16-bit C version).
 *
 * Triplicates each pixel horizontally. For pairs of pixels [A][B], outputs
 * [A][A][A][B][B][B] using bit manipulation to minimize writes.
 *
 * @param ymul Vertical scale factor
 */
void scale3x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 3;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		// Process pairs: [A][B] -> [A][A][A][B][B][B] in 3 writes
		for (x = dx = 0; x < (sw / 2); x++, dx += 3) {
			pix = s[x]; // [B][A]
			dpix1 = (pix & 0x0000FFFF) | (pix << 16); // [A][A]
			dpix2 = (pix & 0xFFFF0000) | (pix >> 16); // [B][B]
			d[dx] = dpix1; // [A][A]
			d[dx + 1] = pix; // [B][A] (middle A, first B)
			d[dx + 2] = dpix2; // [B][B]
		}
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t* d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x * 2];
			dpix1 = pix16 | (pix16 << 16);
			d[dx] = dpix1; // First two copies
			d16[(dx + 1) * 2] = pix16; // Third copy
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

/** 3x1 scale (3x horizontal, 1x vertical) for 16-bit pixels. */
void scale3x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
/** 3x2 scale (3x horizontal, 2x vertical) for 16-bit pixels. */
void scale3x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
/** 3x3 scale (3x horizontal, 3x vertical) for 16-bit pixels. */
void scale3x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
/** 3x4 scale (3x horizontal, 4x vertical) for 16-bit pixels. */
void scale3x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/**
 * Generic 3x horizontal scale with variable vertical scaling (32-bit C version).
 *
 * Same as scale3x_c16 but for 32-bit pixels. Simpler since no paired reads needed.
 *
 * @param ymul Vertical scale factor
 */
void scale3x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 3;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < sw; x++, dx += 3) {
			pix = s[x];
			d[dx] = pix;
			d[dx + 1] = pix;
			d[dx + 2] = pix;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

/** 3x1 scale (3x horizontal, 1x vertical) for 32-bit pixels. */
void scale3x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
/** 3x2 scale (3x horizontal, 2x vertical) for 32-bit pixels. */
void scale3x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
/** 3x3 scale (3x horizontal, 3x vertical) for 32-bit pixels. */
void scale3x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
/** 3x4 scale (3x horizontal, 4x vertical) for 32-bit pixels. */
void scale3x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

/** Generic 4x horizontal scale with variable vertical scaling (16-bit C version). */
void scale4x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 4;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < (sw / 2); x++, dx += 4) {
			pix = s[x];
			dpix1 = (pix & 0x0000FFFF) | (pix << 16);
			dpix2 = (pix & 0xFFFF0000) | (pix >> 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d[dx + 2] = dpix2;
			d[dx + 3] = dpix2;
		}
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t pix16 = s16[x * 2];
			dpix1 = pix16 | (pix16 << 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale4x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale4x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale4x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

void scale4x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 4;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < sw; x++, dx += 4) {
			pix = s[x];
			d[dx] = pix;
			d[dx + 1] = pix;
			d[dx + 2] = pix;
			d[dx + 3] = pix;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale4x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale4x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale4x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}

void scale5x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 5;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < (sw / 2); x++, dx += 5) {
			pix = s[x];
			dpix1 = (pix & 0x0000FFFF) | (pix << 16);
			dpix2 = (pix & 0xFFFF0000) | (pix >> 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d[dx + 2] = pix;
			d[dx + 3] = dpix2;
			d[dx + 4] = dpix2;
		}
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t* d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x * 2];
			dpix1 = pix16 | (pix16 << 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d16[(dx + 2) * 2] = pix16;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale5x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale5x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale5x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale5x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5);
}

void scale5x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 5;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < sw; x++, dx += 5) {
			pix = s[x];
			d[dx] = pix;
			d[dx + 1] = pix;
			d[dx + 2] = pix;
			d[dx + 3] = pix;
			d[dx + 4] = pix;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale5x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale5x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale5x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale5x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5);
}

void scale6x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 6;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < (sw / 2); x++, dx += 6) {
			pix = s[x];
			dpix1 = (pix & 0x0000FFFF) | (pix << 16);
			dpix2 = (pix & 0xFFFF0000) | (pix >> 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d[dx + 2] = dpix1;
			d[dx + 3] = dpix2;
			d[dx + 4] = dpix2;
			d[dx + 5] = dpix2;
		}
		if (sw & 1) {
			const uint16_t* s16 = (const uint16_t*)s;
			uint16_t pix16 = s16[x * 2];
			dpix1 = pix16 | (pix16 << 16);
			d[dx] = dpix1;
			d[dx + 1] = dpix1;
			d[dx + 2] = dpix1;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale6x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale6x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale6x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale6x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5);
}
void scale6x6_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 6);
}

void scale6x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t x, dx, pix, swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	swl *= 6;
	if (!dp) {
		dp = swl;
	}
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		const uint32_t* s = (const uint32_t* __restrict)src;
		uint32_t* d = (uint32_t* __restrict)dst;
		for (x = dx = 0; x < sw; x++, dx += 6) {
			pix = s[x];
			d[dx] = pix;
			d[dx + 1] = pix;
			d[dx + 2] = pix;
			d[dx + 3] = pix;
			d[dx + 4] = pix;
			d[dx + 5] = pix;
		}
		const void* __restrict dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale6x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale6x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale6x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale6x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5);
}
/** 6x6 scale for 32-bit pixels. */
void scale6x6_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 6);
}

///////////////////////////////
// ARM NEON Optimized Scalers
///////////////////////////////

#ifdef HAS_NEON

/**
 * NEON-optimized memory copy using SIMD instructions.
 *
 * Performs bulk memory copy using ARM NEON vector registers (128-bit quad registers).
 * Processes data in decreasing chunk sizes: 128, 64, 32, 16, 8, then 2 bytes at a time.
 * This is significantly faster than standard memcpy for the aligned, power-of-2
 * sized buffers common in video scaling.
 *
 * Algorithm:
 * 1. Process full 128-byte blocks using 8 quad registers (q8-q15)
 * 2. Handle remainder in chunks: 64, 32, 16, 8 bytes using NEON
 * 3. Copy final 2-byte aligned remainder with scalar loads/stores
 *
 * @param dst Destination buffer (must be 32-bit aligned)
 * @param src Source buffer (must be 32-bit aligned)
 * @param size Number of bytes to copy (must be 16-bit aligned, i.e., even)
 *
 * @warning Both pointers must be 32-bit aligned, size must be even
 * @note Uses inline ARM assembly with NEON SIMD instructions
 * @note Modifies registers: r3, r4, q8-q15
 */
void memcpy_neon(void* dst, const void* src, uint32_t size) {
	asm volatile(
	    // Calculate loop boundaries
	    "	bic r4, %[sz], #127	;" // r4 = size & ~127 (128-byte aligned portion)
	    "	add r3, %[s], %[sz]	;" // r3 = src + size (end pointer)
	    "	add r4, %[s], r4	;" // r4 = src + aligned_size (128-byte loop end)
	    "	cmp %[s], r4		;"
	    "	beq 2f			;" // Skip if no 128-byte blocks
	    // Loop: Copy 128 bytes at a time using 8 quad registers
	    "1:	vldmia %[s]!, {q8-q15}	;" // Load 128 bytes (8 x 16-byte quad regs)
	    "	vstmia %[d]!, {q8-q15}	;" // Store 128 bytes
	    "	cmp %[s], r4		;"
	    "	bne 1b			;" // Repeat until all 128-byte blocks done
	    // Handle remainder: 64 bytes
	    "2:	cmp %[s], r3		;"
	    "	beq 7f			;" // Done if at end
	    "	tst %[sz], #64		;" // Check if 64-byte chunk needed
	    "	beq 3f			;"
	    "	vldmia %[s]!, {q8-q11}	;" // Load 64 bytes (4 quad regs)
	    "	vstmia %[d]!, {q8-q11}	;" // Store 64 bytes
	    "	cmp %[s], r3		;"
	    "	beq 7f			;" // Done if at end
	    // Handle remainder: 32 bytes
	    "3:	tst %[sz], #32		;" // Check if 32-byte chunk needed
	    "	beq 4f			;"
	    "	vldmia %[s]!, {q12-q13}	;" // Load 32 bytes (2 quad regs)
	    "	vstmia %[d]!, {q12-q13}	;" // Store 32 bytes
	    "	cmp %[s], r3		;"
	    "	beq 7f			;" // Done if at end
	    // Handle remainder: 16 bytes
	    "4:	tst %[sz], #16		;" // Check if 16-byte chunk needed
	    "	beq 5f			;"
	    "	vldmia %[s]!, {q14}	;" // Load 16 bytes (1 quad reg)
	    "	vstmia %[d]!, {q14}	;" // Store 16 bytes
	    "	cmp %[s], r3		;"
	    "	beq 7f			;" // Done if at end
	    // Handle remainder: 8 bytes
	    "5:	tst %[sz], #8		;" // Check if 8-byte chunk needed
	    "	beq 6f			;"
	    "	vldmia %[s]!, {d30}	;" // Load 8 bytes (1 double reg)
	    "	vstmia %[d]!, {d30}	;" // Store 8 bytes
	    "	cmp %[s], r3		;"
	    "	beq 7f			;" // Done if at end
	    // Handle final remainder: 2-byte chunks (scalar)
	    "6:	ldrh r4, [%[s]],#2	;" // Load halfword (2 bytes)
	    "	strh r4, [%[d]],#2	;" // Store halfword
	    "	cmp %[s], r3		;"
	    "	bne 6b			;" // Repeat until done
	    "7:				" // Exit
	    : [s] "+r"(src), [d] "+r"(dst)
	    : [sz] "r"(size)
	    : "r3", "r4", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory", "cc");
}

/**
 * 1x1 scale (direct copy) for 16-bit pixels using NEON.
 *
 * NEON-optimized version of scale1x1_c16. Uses memcpy_neon for fast copying.
 * Falls back to C version if buffers are not 32-bit aligned.
 *
 * Optimization: If entire image is contiguous (matching pitches), copies
 * all data in a single memcpy_neon call.
 *
 * @note Requires 32-bit aligned buffers and pitches
 * @note Falls back to C version for unaligned data
 */
void scale1x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 1;
	}
	// Check alignment requirements for NEON (32-bit aligned pointers and pitches)
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x1_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	if ((swl == sp) && (sp == dp))
		memcpy_neon(dst, src, sp * sh); // Single bulk copy for contiguous data
	else {
		if (swl > dp)
			swl = dp;
		for (; sh > 0; sh--, src = (uint8_t*)src + sp, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, src, swl);
	}
}

/**
 * 1x2 scale (1x horizontal, 2x vertical) for 16-bit pixels using NEON.
 *
 * Duplicates each source line twice using NEON SIMD. Processes lines in
 * 128-byte chunks when possible, then handles remainder in smaller chunks.
 * Each 128-byte block is loaded once and stored twice (to line N and N+1).
 *
 * NEON instructions used:
 * - vldmia: Vector load multiple (128 bytes = 8 quad registers)
 * - vstmia: Vector store multiple (writes to two destination lines)
 *
 * @note Requires 32-bit aligned buffers and pitches
 * @note Falls back to C version for unaligned data
 */
void scale1x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	// Alignment check - fall back to C if not aligned
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x2_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	vstmia r9!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	vstmia r9!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale1x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x3_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	vstmia r10!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	vstmia r9!, {q14}	;"
	             "	vstmia r10!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	vstmia r9!, {d30}	;"
	             "	vstmia r10!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "	str lr, [r10]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale1x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x4_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	vstmia r11!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	vstmia r11!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	vstmia r10!, {q12-q13}	;"
	             "	vstmia r11!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	vstmia r9!, {q14}	;"
	             "	vstmia r10!, {q14}	;"
	             "	vstmia r11!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	vstmia r9!, {d30}	;"
	             "	vstmia r10!, {d30}	;"
	             "	vstmia r11!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "	str lr, [r10]		;"
	             "	str lr, [r11]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale1x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale1x1_n16, &scale1x2_n16, &scale1x3_n16, &scale1x4_n16};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}
void scale1x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 1;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x1_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	if ((swl == sp) && (sp == dp))
		memcpy_neon(dst, src, sp * sh);
	else
		for (; sh > 0; sh--, src = (uint8_t*)src + sp, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, src, swl);
}

void scale1x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x2_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	vstmia r9!, {q14}	;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	vstmia r9!, {d30}	;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale1x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x3_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	vstmia r10!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	vstmia r9!, {q14}	;"
	             "	vstmia r10!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	vstmia r9!, {d30}	;"
	             "	vstmia r10!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "	str lr, [r10]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale1x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale1x4_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x128bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q15}	;" // 128 bytes
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	vstmia r11!, {q8-q15}	;"
	             "	cmp %0, lr		;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 8f			;"
	             "	tst %3, #64		;"
	             "	beq 4f			;"
	             "	vldmia %0!, {q8-q11}	;" // 64 bytes
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	vstmia r11!, {q8-q11}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "4:	tst %3, #32		;"
	             "	beq 5f			;"
	             "	vldmia %0!, {q12-q13}	;" // 32 bytes
	             "	vstmia %1!, {q12-q13}	;"
	             "	vstmia r9!, {q12-q13}	;"
	             "	vstmia r10!, {q12-q13}	;"
	             "	vstmia r11!, {q12-q13}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "5:	tst %3, #16		;"
	             "	beq 6f			;"
	             "	vldmia %0!, {q14}	;" // 16 bytes
	             "	vstmia %1!, {q14}	;"
	             "	vstmia r9!, {q14}	;"
	             "	vstmia r10!, {q14}	;"
	             "	vstmia r11!, {q14}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "6:	tst %3, #8		;"
	             "	beq 7f			;"
	             "	vldmia %0!, {d30}	;" // 8 bytes
	             "	vstmia %1!, {d30}	;"
	             "	vstmia r9!, {d30}	;"
	             "	vstmia r10!, {d30}	;"
	             "	vstmia r11!, {d30}	;"
	             "	cmp %0, r8		;"
	             "	beq 8f			;"
	             "7:	ldr lr, [%0],#4		;" // 4 bytes
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9]		;"
	             "	str lr, [r10]		;"
	             "	str lr, [r11]		;"
	             "8:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

/**
 * Variable vertical scale dispatcher for 32-bit pixels using NEON.
 *
 * Dispatches to the appropriate scale1x{1-4}_n32 function based on ymul.
 *
 * @param ymul Vertical scale factor (1-4)
 */
void scale1x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale1x1_n32, &scale1x2_n32, &scale1x3_n32, &scale1x4_n32};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

/**
 * 2x1 scale (2x horizontal, 1x vertical) for 16-bit pixels using NEON.
 *
 * Duplicates each pixel horizontally using NEON vector duplication instructions.
 * This is the core horizontal scaling operation for 2x scaling.
 *
 * NEON technique:
 * 1. Load 64 bytes (32 RGB565 pixels) into quad registers q8-q11
 * 2. Use vdup.16 to duplicate each 16-bit pixel across a register
 * 3. Use vext.16 to extract and rearrange duplicated pixels
 * 4. Store 128 bytes (64 output pixels = each input pixel duplicated)
 *
 * Each 16-bit pixel [A] becomes [A][A] in the output.
 * Processes 32 input pixels -> 64 output pixels per 64-byte block.
 *
 * @note Requires 32-bit aligned buffers and pitches
 * @note Falls back to C version for unaligned data
 */
void scale2x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	// Alignment check - fall back to C if not aligned
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x1_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x64bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;" // Skip to remainder if no 64-byte blocks
	             // Main loop: Process 64-byte blocks (32 pixels -> 64 pixels)
	             "2:	vldmia %0!, {q8-q11}	;" // Load 64 bytes (32 RGB565 pixels)
	             // Duplicate pixels using vdup + vext pattern
	             // For each pixel [A], create [A][A] using vdup (fills register) + vext (extract)
	             "	vdup.16 d0, d23[3]	;" // d0 = [pixel31][pixel31][pixel31][pixel31]
	             "	vdup.16 d1, d23[2]	;" // d1 = [pixel30][pixel30][pixel30][pixel30]
	             "	vext.16 d31, d1,d0,#2	;" // d31 = [pixel31][pixel31][pixel30][pixel30]
	             "	vdup.16 d0, d23[1]	;"
	             "	vdup.16 d1, d23[0]	;"
	             "	vext.16 d30, d1,d0,#2	;" // Continue pattern for all 32 pixels
	             // (Pattern repeats for d29-d16, converting 32 input pixels to 64 output pixels)
	             "	vdup.16 d0, d22[3]	;"
	             "	vdup.16 d1, d22[2]	;"
	             "	vext.16 d29, d1,d0,#2	;"
	             "	vdup.16 d0, d22[1]	;"
	             "	vdup.16 d1, d22[0]	;"
	             "	vext.16 d28, d1,d0,#2	;"
	             "	vdup.16 d0, d21[3]	;"
	             "	vdup.16 d1, d21[2]	;"
	             "	vext.16 d27, d1,d0,#2	;"
	             "	vdup.16 d0, d21[1]	;"
	             "	vdup.16 d1, d21[0]	;"
	             "	vext.16 d26, d1,d0,#2	;"
	             "	vdup.16 d0, d20[3]	;"
	             "	vdup.16 d1, d20[2]	;"
	             "	vext.16 d25, d1,d0,#2	;"
	             "	vdup.16 d0, d20[1]	;"
	             "	vdup.16 d1, d20[0]	;"
	             "	vext.16 d24, d1,d0,#2	;"
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "	tst %3, #32		;"
	             "	beq 4f			;"
	             "	vldmia %0!,{q8-q9}	;" // 16 pixels
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8-q11}	;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q0", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale2x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x2_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x64bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 32 pixels 64 bytes
	             "	vdup.16 d0, d23[3]	;"
	             "	vdup.16 d1, d23[2]	;"
	             "	vext.16 d31, d1,d0,#2	;"
	             "	vdup.16 d0, d23[1]	;"
	             "	vdup.16 d1, d23[0]	;"
	             "	vext.16 d30, d1,d0,#2	;"
	             "	vdup.16 d0, d22[3]	;"
	             "	vdup.16 d1, d22[2]	;"
	             "	vext.16 d29, d1,d0,#2	;"
	             "	vdup.16 d0, d22[1]	;"
	             "	vdup.16 d1, d22[0]	;"
	             "	vext.16 d28, d1,d0,#2	;"
	             "	vdup.16 d0, d21[3]	;"
	             "	vdup.16 d1, d21[2]	;"
	             "	vext.16 d27, d1,d0,#2	;"
	             "	vdup.16 d0, d21[1]	;"
	             "	vdup.16 d1, d21[0]	;"
	             "	vext.16 d26, d1,d0,#2	;"
	             "	vdup.16 d0, d20[3]	;"
	             "	vdup.16 d1, d20[2]	;"
	             "	vext.16 d25, d1,d0,#2	;"
	             "	vdup.16 d0, d20[1]	;"
	             "	vdup.16 d1, d20[0]	;"
	             "	vext.16 d24, d1,d0,#2	;"
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "	tst %3, #32		;"
	             "	beq 4f			;"
	             "	vldmia %0!,{q8-q9}	;" // 16 pixels
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9],#4		;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q0", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale2x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x3_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x64bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 32 pixels 64 bytes
	             "	vdup.16 d0, d23[3]	;"
	             "	vdup.16 d1, d23[2]	;"
	             "	vext.16 d31, d1,d0,#2	;"
	             "	vdup.16 d0, d23[1]	;"
	             "	vdup.16 d1, d23[0]	;"
	             "	vext.16 d30, d1,d0,#2	;"
	             "	vdup.16 d0, d22[3]	;"
	             "	vdup.16 d1, d22[2]	;"
	             "	vext.16 d29, d1,d0,#2	;"
	             "	vdup.16 d0, d22[1]	;"
	             "	vdup.16 d1, d22[0]	;"
	             "	vext.16 d28, d1,d0,#2	;"
	             "	vdup.16 d0, d21[3]	;"
	             "	vdup.16 d1, d21[2]	;"
	             "	vext.16 d27, d1,d0,#2	;"
	             "	vdup.16 d0, d21[1]	;"
	             "	vdup.16 d1, d21[0]	;"
	             "	vext.16 d26, d1,d0,#2	;"
	             "	vdup.16 d0, d20[3]	;"
	             "	vdup.16 d1, d20[2]	;"
	             "	vext.16 d25, d1,d0,#2	;"
	             "	vdup.16 d0, d20[1]	;"
	             "	vdup.16 d1, d20[0]	;"
	             "	vext.16 d24, d1,d0,#2	;"
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "	tst %3, #32		;"
	             "	beq 4f			;"
	             "	vldmia %0!,{q8-q9}	;" // 16 pixels
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9],#4		;"
	             "	str lr, [r10],#4	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q0", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale2x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x3_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x64bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 32 pixels 64 bytes
	             "	vdup.16 d0, d23[3]	;"
	             "	vdup.16 d1, d23[2]	;"
	             "	vext.16 d31, d1,d0,#2	;"
	             "	vdup.16 d0, d23[1]	;"
	             "	vdup.16 d1, d23[0]	;"
	             "	vext.16 d30, d1,d0,#2	;"
	             "	vdup.16 d0, d22[3]	;"
	             "	vdup.16 d1, d22[2]	;"
	             "	vext.16 d29, d1,d0,#2	;"
	             "	vdup.16 d0, d22[1]	;"
	             "	vdup.16 d1, d22[0]	;"
	             "	vext.16 d28, d1,d0,#2	;"
	             "	vdup.16 d0, d21[3]	;"
	             "	vdup.16 d1, d21[2]	;"
	             "	vext.16 d27, d1,d0,#2	;"
	             "	vdup.16 d0, d21[1]	;"
	             "	vdup.16 d1, d21[0]	;"
	             "	vext.16 d26, d1,d0,#2	;"
	             "	vdup.16 d0, d20[3]	;"
	             "	vdup.16 d1, d20[2]	;"
	             "	vext.16 d25, d1,d0,#2	;"
	             "	vdup.16 d0, d20[1]	;"
	             "	vdup.16 d1, d20[0]	;"
	             "	vext.16 d24, d1,d0,#2	;"
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	vstmia r11!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "	tst %3, #32		;"
	             "	beq 4f			;"
	             "	vldmia %0!,{q8-q9}	;" // 16 pixels
	             "	vdup.16 d0, d19[3]	;"
	             "	vdup.16 d1, d19[2]	;"
	             "	vext.16 d23, d1,d0,#2	;"
	             "	vdup.16 d0, d19[1]	;"
	             "	vdup.16 d1, d19[0]	;"
	             "	vext.16 d22, d1,d0,#2	;"
	             "	vdup.16 d0, d18[3]	;"
	             "	vdup.16 d1, d18[2]	;"
	             "	vext.16 d21, d1,d0,#2	;"
	             "	vdup.16 d0, d18[1]	;"
	             "	vdup.16 d1, d18[0]	;"
	             "	vext.16 d20, d1,d0,#2	;"
	             "	vdup.16 d0, d17[3]	;"
	             "	vdup.16 d1, d17[2]	;"
	             "	vext.16 d19, d1,d0,#2	;"
	             "	vdup.16 d0, d17[1]	;"
	             "	vdup.16 d1, d17[0]	;"
	             "	vext.16 d18, d1,d0,#2	;"
	             "	vdup.16 d0, d16[3]	;"
	             "	vdup.16 d1, d16[2]	;"
	             "	vext.16 d17, d1,d0,#2	;"
	             "	vdup.16 d0, d16[1]	;"
	             "	vdup.16 d1, d16[0]	;"
	             "	vext.16 d16, d1,d0,#2	;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8-q11}	;"
	             "	vstmia r9!, {q8-q11}	;"
	             "	vstmia r10!, {q8-q11}	;"
	             "	vstmia r11!, {q8-q11}	;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	str lr, [r9],#4		;"
	             "	str lr, [r10],#4	;"
	             "	str lr, [r11],#4	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q0", "q8", "q9", "q10", "q11", "q12", "q13",
	               "q14", "q15", "memory", "cc");
}

void scale2x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale2x1_n16, &scale2x2_n16, &scale2x3_n16, &scale2x4_n16};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale2x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x1_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x64bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 16 pixels 64 bytes
	             "	vdup.32 d31, d23[1]	;"
	             "	vdup.32 d30, d23[0]	;"
	             "	vdup.32 d29, d22[1]	;"
	             "	vdup.32 d28, d22[0]	;"
	             "	vdup.32 d27, d21[1]	;"
	             "	vdup.32 d26, d21[0]	;"
	             "	vdup.32 d25, d20[1]	;"
	             "	vdup.32 d24, d20[0]	;"
	             "	vdup.32 d23, d19[1]	;"
	             "	vdup.32 d22, d19[0]	;"
	             "	vdup.32 d21, d18[1]	;"
	             "	vdup.32 d20, d18[0]	;"
	             "	vdup.32 d19, d17[1]	;"
	             "	vdup.32 d18, d17[0]	;"
	             "	vdup.32 d17, d16[1]	;"
	             "	vdup.32 d16, d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale2x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x2_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x64bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 16 pixels 64 bytes
	             "	vdup.32 d31, d23[1]	;"
	             "	vdup.32 d30, d23[0]	;"
	             "	vdup.32 d29, d22[1]	;"
	             "	vdup.32 d28, d22[0]	;"
	             "	vdup.32 d27, d21[1]	;"
	             "	vdup.32 d26, d21[0]	;"
	             "	vdup.32 d25, d20[1]	;"
	             "	vdup.32 d24, d20[0]	;"
	             "	vdup.32 d23, d19[1]	;"
	             "	vdup.32 d22, d19[0]	;"
	             "	vdup.32 d21, d18[1]	;"
	             "	vdup.32 d20, d18[0]	;"
	             "	vdup.32 d19, d17[1]	;"
	             "	vdup.32 d18, d17[0]	;"
	             "	vdup.32 d17, d16[1]	;"
	             "	vdup.32 d16, d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale2x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x3_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x64bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 16 pixels 64 bytes
	             "	vdup.32 d31, d23[1]	;"
	             "	vdup.32 d30, d23[0]	;"
	             "	vdup.32 d29, d22[1]	;"
	             "	vdup.32 d28, d22[0]	;"
	             "	vdup.32 d27, d21[1]	;"
	             "	vdup.32 d26, d21[0]	;"
	             "	vdup.32 d25, d20[1]	;"
	             "	vdup.32 d24, d20[0]	;"
	             "	vdup.32 d23, d19[1]	;"
	             "	vdup.32 d22, d19[0]	;"
	             "	vdup.32 d21, d18[1]	;"
	             "	vdup.32 d20, d18[0]	;"
	             "	vdup.32 d19, d17[1]	;"
	             "	vdup.32 d18, d17[0]	;"
	             "	vdup.32 d17, d16[1]	;"
	             "	vdup.32 d16, d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	vstmia r10!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale2x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 2;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale2x4_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 2;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x64bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q11}	;" // 16 pixels 64 bytes
	             "	vdup.32 d31, d23[1]	;"
	             "	vdup.32 d30, d23[0]	;"
	             "	vdup.32 d29, d22[1]	;"
	             "	vdup.32 d28, d22[0]	;"
	             "	vdup.32 d27, d21[1]	;"
	             "	vdup.32 d26, d21[0]	;"
	             "	vdup.32 d25, d20[1]	;"
	             "	vdup.32 d24, d20[0]	;"
	             "	vdup.32 d23, d19[1]	;"
	             "	vdup.32 d22, d19[0]	;"
	             "	vdup.32 d21, d18[1]	;"
	             "	vdup.32 d20, d18[0]	;"
	             "	vdup.32 d19, d17[1]	;"
	             "	vdup.32 d18, d17[0]	;"
	             "	vdup.32 d17, d16[1]	;"
	             "	vdup.32 d16, d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q15}	;"
	             "	vstmia r9!, {q8-q15}	;"
	             "	vstmia r10!, {q8-q15}	;"
	             "	vstmia r11!, {q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	vstmia r10!, {d16}	;"
	             "	vstmia r11!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl64), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale2x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale2x1_n32, &scale2x2_n32, &scale2x3_n32, &scale2x4_n32};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale3x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x1_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31, d19[3]	;" //  FFFF
	             "	vdup.16 d30, d19[2]	;" //  EEEE
	             "	vdup.16 d29, d19[1]	;" //  DDDD
	             "	vdup.16 d28, d19[0]	;" //  CCCC
	             "	vext.16 d27, d30,d31,#3	;" // EFFF
	             "	vext.16 d26, d29,d30,#2	;" // DDEE
	             "	vext.16 d25, d28,d29,#1	;" // CCCD
	             "	vdup.16 d31, d18[3]	;" //  BBBB
	             "	vdup.16 d30, d18[2]	;" //  AAAA
	             "	vdup.16 d29, d18[1]	;" //  9999
	             "	vdup.16 d28, d18[0]	;" //  8888
	             "	vext.16 d24, d30,d31,#3	;" // ABBB
	             "	vext.16 d23, d29,d30,#2	;" // 99AA
	             "	vext.16 d22, d28,d29,#1	;" // 8889
	             "	vdup.16 d31, d17[3]	;" //  7777
	             "	vdup.16 d30, d17[2]	;" //  6666
	             "	vdup.16 d29, d17[1]	;" //  5555
	             "	vdup.16 d28, d17[0]	;" //  4444
	             "	vext.16 d21, d30,d31,#3	;" // 6777
	             "	vext.16 d20, d29,d30,#2	;" // 5566
	             "	vext.16 d19, d28,d29,#1	;" // 4445
	             "	vdup.16 d31, d16[3]	;" //  3333
	             "	vdup.16 d30, d16[2]	;" //  2222
	             "	vdup.16 d29, d16[1]	;" //  1111
	             "	vdup.16 d28, d16[0]	;" //  0000
	             "	vext.16 d18, d30,d31,#3	;" // 2333
	             "	vext.16 d17, d29,d30,#2	;" // 1122
	             "	vext.16 d16, d28,d29,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	strh lr, [%1],#2	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale3x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x2_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31, d19[3]	;" //  FFFF
	             "	vdup.16 d30, d19[2]	;" //  EEEE
	             "	vdup.16 d29, d19[1]	;" //  DDDD
	             "	vdup.16 d28, d19[0]	;" //  CCCC
	             "	vext.16 d27, d30,d31,#3	;" // EFFF
	             "	vext.16 d26, d29,d30,#2	;" // DDEE
	             "	vext.16 d25, d28,d29,#1	;" // CCCD
	             "	vdup.16 d31, d18[3]	;" //  BBBB
	             "	vdup.16 d30, d18[2]	;" //  AAAA
	             "	vdup.16 d29, d18[1]	;" //  9999
	             "	vdup.16 d28, d18[0]	;" //  8888
	             "	vext.16 d24, d30,d31,#3	;" // ABBB
	             "	vext.16 d23, d29,d30,#2	;" // 99AA
	             "	vext.16 d22, d28,d29,#1	;" // 8889
	             "	vdup.16 d31, d17[3]	;" //  7777
	             "	vdup.16 d30, d17[2]	;" //  6666
	             "	vdup.16 d29, d17[1]	;" //  5555
	             "	vdup.16 d28, d17[0]	;" //  4444
	             "	vext.16 d21, d30,d31,#3	;" // 6777
	             "	vext.16 d20, d29,d30,#2	;" // 5566
	             "	vext.16 d19, d28,d29,#1	;" // 4445
	             "	vdup.16 d31, d16[3]	;" //  3333
	             "	vdup.16 d30, d16[2]	;" //  2222
	             "	vdup.16 d29, d16[1]	;" //  1111
	             "	vdup.16 d28, d16[0]	;" //  0000
	             "	vext.16 d18, d30,d31,#3	;" // 2333
	             "	vext.16 d17, d29,d30,#2	;" // 1122
	             "	vext.16 d16, d28,d29,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q13}	;"
	             "	vstmia r9!, {q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	strh lr, [%1],#2	;"
	             "	str lr, [r9],#4		;"
	             "	strh lr, [r9],#2	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale3x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x3_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31, d19[3]	;" //  FFFF
	             "	vdup.16 d30, d19[2]	;" //  EEEE
	             "	vdup.16 d29, d19[1]	;" //  DDDD
	             "	vdup.16 d28, d19[0]	;" //  CCCC
	             "	vext.16 d27, d30,d31,#3	;" // EFFF
	             "	vext.16 d26, d29,d30,#2	;" // DDEE
	             "	vext.16 d25, d28,d29,#1	;" // CCCD
	             "	vdup.16 d31, d18[3]	;" //  BBBB
	             "	vdup.16 d30, d18[2]	;" //  AAAA
	             "	vdup.16 d29, d18[1]	;" //  9999
	             "	vdup.16 d28, d18[0]	;" //  8888
	             "	vext.16 d24, d30,d31,#3	;" // ABBB
	             "	vext.16 d23, d29,d30,#2	;" // 99AA
	             "	vext.16 d22, d28,d29,#1	;" // 8889
	             "	vdup.16 d31, d17[3]	;" //  7777
	             "	vdup.16 d30, d17[2]	;" //  6666
	             "	vdup.16 d29, d17[1]	;" //  5555
	             "	vdup.16 d28, d17[0]	;" //  4444
	             "	vext.16 d21, d30,d31,#3	;" // 6777
	             "	vext.16 d20, d29,d30,#2	;" // 5566
	             "	vext.16 d19, d28,d29,#1	;" // 4445
	             "	vdup.16 d31, d16[3]	;" //  3333
	             "	vdup.16 d30, d16[2]	;" //  2222
	             "	vdup.16 d29, d16[1]	;" //  1111
	             "	vdup.16 d28, d16[0]	;" //  0000
	             "	vext.16 d18, d30,d31,#3	;" // 2333
	             "	vext.16 d17, d29,d30,#2	;" // 1122
	             "	vext.16 d16, d28,d29,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q13}	;"
	             "	vstmia r9!, {q8-q13}	;"
	             "	vstmia r10!, {q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	strh lr, [%1],#2	;"
	             "	str lr, [r9],#4		;"
	             "	strh lr, [r9],#2	;"
	             "	str lr, [r10],#4	;"
	             "	strh lr, [r10],#2	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale3x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x4_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!, {q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31, d19[3]	;" //  FFFF
	             "	vdup.16 d30, d19[2]	;" //  EEEE
	             "	vdup.16 d29, d19[1]	;" //  DDDD
	             "	vdup.16 d28, d19[0]	;" //  CCCC
	             "	vext.16 d27, d30,d31,#3	;" // EFFF
	             "	vext.16 d26, d29,d30,#2	;" // DDEE
	             "	vext.16 d25, d28,d29,#1	;" // CCCD
	             "	vdup.16 d31, d18[3]	;" //  BBBB
	             "	vdup.16 d30, d18[2]	;" //  AAAA
	             "	vdup.16 d29, d18[1]	;" //  9999
	             "	vdup.16 d28, d18[0]	;" //  8888
	             "	vext.16 d24, d30,d31,#3	;" // ABBB
	             "	vext.16 d23, d29,d30,#2	;" // 99AA
	             "	vext.16 d22, d28,d29,#1	;" // 8889
	             "	vdup.16 d31, d17[3]	;" //  7777
	             "	vdup.16 d30, d17[2]	;" //  6666
	             "	vdup.16 d29, d17[1]	;" //  5555
	             "	vdup.16 d28, d17[0]	;" //  4444
	             "	vext.16 d21, d30,d31,#3	;" // 6777
	             "	vext.16 d20, d29,d30,#2	;" // 5566
	             "	vext.16 d19, d28,d29,#1	;" // 4445
	             "	vdup.16 d31, d16[3]	;" //  3333
	             "	vdup.16 d30, d16[2]	;" //  2222
	             "	vdup.16 d29, d16[1]	;" //  1111
	             "	vdup.16 d28, d16[0]	;" //  0000
	             "	vext.16 d18, d30,d31,#3	;" // 2333
	             "	vext.16 d17, d29,d30,#2	;" // 1122
	             "	vext.16 d16, d28,d29,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!, {q8-q13}	;"
	             "	vstmia r9!, {q8-q13}	;"
	             "	vstmia r10!, {q8-q13}	;"
	             "	vstmia r11!, {q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	orr lr, lr, lsl #16	;"
	             "	cmp %0, r8		;"
	             "	str lr, [%1],#4		;"
	             "	strh lr, [%1],#2	;"
	             "	str lr, [r9],#4		;"
	             "	strh lr, [r9],#2	;"
	             "	str lr, [r10],#4	;"
	             "	strh lr, [r10],#2	;"
	             "	str lr, [r11],#4	;"
	             "	strh lr, [r11],#2	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale3x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale3x1_n16, &scale3x2_n16, &scale3x3_n16, &scale3x4_n16};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale3x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x1_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15, d19[1]	;" //  7777
	             "	vdup.32 q14, d19[0]	;" //  6666
	             "	vdup.32 q1, d18[1]	;" //  5555
	             "	vdup.32 q0, d18[0]	;" //  4444
	             "	vext.32 q13, q14,q15,#3	;" // 6777
	             "	vext.32 q12, q1,q14,#2	;" // 5566
	             "	vext.32 q11, q0,q1,#1	;" // 4445
	             "	vdup.32 q15, d17[1]	;" //  3333
	             "	vdup.32 q14, d17[0]	;" //  2222
	             "	vdup.32 q1, d16[1]	;" //  1111
	             "	vdup.32 q0, d16[0]	;" //  0000
	             "	vext.32 q10, q14,q15,#3	;" // 2333
	             "	vext.32 q9, q1,q14,#2	;" // 1122
	             "	vext.32 q8, q0,q1,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	str lr, [%1],#4		;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale3x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x2_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15, d19[1]	;" //  7777
	             "	vdup.32 q14, d19[0]	;" //  6666
	             "	vdup.32 q1, d18[1]	;" //  5555
	             "	vdup.32 q0, d18[0]	;" //  4444
	             "	vext.32 q13, q14,q15,#3	;" // 6777
	             "	vext.32 q12, q1,q14,#2	;" // 5566
	             "	vext.32 q11, q0,q1,#1	;" // 4445
	             "	vdup.32 q15, d17[1]	;" //  3333
	             "	vdup.32 q14, d17[0]	;" //  2222
	             "	vdup.32 q1, d16[1]	;" //  1111
	             "	vdup.32 q0, d16[0]	;" //  0000
	             "	vext.32 q10, q14,q15,#3	;" // 2333
	             "	vext.32 q9, q1,q14,#2	;" // 1122
	             "	vext.32 q8, q0,q1,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q13}	;"
	             "	vstmia r9!,{q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	str lr, [%1],#4		;"
	             "	vstmia r9!, {d16}	;"
	             "	str lr, [r9],#4		;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale3x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x3_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15, d19[1]	;" //  7777
	             "	vdup.32 q14, d19[0]	;" //  6666
	             "	vdup.32 q1, d18[1]	;" //  5555
	             "	vdup.32 q0, d18[0]	;" //  4444
	             "	vext.32 q13, q14,q15,#3	;" // 6777
	             "	vext.32 q12, q1,q14,#2	;" // 5566
	             "	vext.32 q11, q0,q1,#1	;" // 4445
	             "	vdup.32 q15, d17[1]	;" //  3333
	             "	vdup.32 q14, d17[0]	;" //  2222
	             "	vdup.32 q1, d16[1]	;" //  1111
	             "	vdup.32 q0, d16[0]	;" //  0000
	             "	vext.32 q10, q14,q15,#3	;" // 2333
	             "	vext.32 q9, q1,q14,#2	;" // 1122
	             "	vext.32 q8, q0,q1,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q13}	;"
	             "	vstmia r9!,{q8-q13}	;"
	             "	vstmia r10!,{q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	str lr, [%1],#4		;"
	             "	vstmia r9!, {d16}	;"
	             "	str lr, [r9],#4		;"
	             "	vstmia r10!, {d16}	;"
	             "	str lr, [r10],#4	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale3x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 3;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale3x4_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 3;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15, d19[1]	;" //  7777
	             "	vdup.32 q14, d19[0]	;" //  6666
	             "	vdup.32 q1, d18[1]	;" //  5555
	             "	vdup.32 q0, d18[0]	;" //  4444
	             "	vext.32 q13, q14,q15,#3	;" // 6777
	             "	vext.32 q12, q1,q14,#2	;" // 5566
	             "	vext.32 q11, q0,q1,#1	;" // 4445
	             "	vdup.32 q15, d17[1]	;" //  3333
	             "	vdup.32 q14, d17[0]	;" //  2222
	             "	vdup.32 q1, d16[1]	;" //  1111
	             "	vdup.32 q0, d16[0]	;" //  0000
	             "	vext.32 q10, q14,q15,#3	;" // 2333
	             "	vext.32 q9, q1,q14,#2	;" // 1122
	             "	vext.32 q8, q0,q1,#1	;" // 0001
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q13}	;"
	             "	vstmia r9!,{q8-q13}	;"
	             "	vstmia r10!,{q8-q13}	;"
	             "	vstmia r11!,{q8-q13}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	str lr, [%1],#4		;"
	             "	vstmia r9!, {d16}	;"
	             "	str lr, [r9],#4		;"
	             "	vstmia r10!, {d16}	;"
	             "	str lr, [r10],#4	;"
	             "	vstmia r11!, {d16}	;"
	             "	str lr, [r11],#4	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale3x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale3x1_n32, &scale3x2_n32, &scale3x3_n32, &scale3x4_n32};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale4x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x1_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31,d19[3]	;"
	             "	vdup.16 d30,d19[2]	;"
	             "	vdup.16 d29,d19[1]	;"
	             "	vdup.16 d28,d19[0]	;"
	             "	vdup.16 d27,d18[3]	;"
	             "	vdup.16 d26,d18[2]	;"
	             "	vdup.16 d25,d18[1]	;"
	             "	vdup.16 d24,d18[0]	;"
	             "	vdup.16 d23,d17[3]	;"
	             "	vdup.16 d22,d17[2]	;"
	             "	vdup.16 d21,d17[1]	;"
	             "	vdup.16 d20,d17[0]	;"
	             "	vdup.16 d19,d16[3]	;"
	             "	vdup.16 d18,d16[2]	;"
	             "	vdup.16 d17,d16[1]	;"
	             "	vdup.16 d16,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	vdup.16 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale4x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x2_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31,d19[3]	;"
	             "	vdup.16 d30,d19[2]	;"
	             "	vdup.16 d29,d19[1]	;"
	             "	vdup.16 d28,d19[0]	;"
	             "	vdup.16 d27,d18[3]	;"
	             "	vdup.16 d26,d18[2]	;"
	             "	vdup.16 d25,d18[1]	;"
	             "	vdup.16 d24,d18[0]	;"
	             "	vdup.16 d23,d17[3]	;"
	             "	vdup.16 d22,d17[2]	;"
	             "	vdup.16 d21,d17[1]	;"
	             "	vdup.16 d20,d17[0]	;"
	             "	vdup.16 d19,d16[3]	;"
	             "	vdup.16 d18,d16[2]	;"
	             "	vdup.16 d17,d16[1]	;"
	             "	vdup.16 d16,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	vdup.16 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale4x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x3_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31,d19[3]	;"
	             "	vdup.16 d30,d19[2]	;"
	             "	vdup.16 d29,d19[1]	;"
	             "	vdup.16 d28,d19[0]	;"
	             "	vdup.16 d27,d18[3]	;"
	             "	vdup.16 d26,d18[2]	;"
	             "	vdup.16 d25,d18[1]	;"
	             "	vdup.16 d24,d18[0]	;"
	             "	vdup.16 d23,d17[3]	;"
	             "	vdup.16 d22,d17[2]	;"
	             "	vdup.16 d21,d17[1]	;"
	             "	vdup.16 d20,d17[0]	;"
	             "	vdup.16 d19,d16[3]	;"
	             "	vdup.16 d18,d16[2]	;"
	             "	vdup.16 d17,d16[1]	;"
	             "	vdup.16 d16,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	vstmia r10!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	vdup.16 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	vstmia r10!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale4x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x4_c16(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr  = x32bytes offset
	             "	add r8, %0, %3		;" // r8  = lineend offset
	             "	add r9, %1, %7		;" // r9  = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 16 pixels 32 bytes
	             "	vdup.16 d31,d19[3]	;"
	             "	vdup.16 d30,d19[2]	;"
	             "	vdup.16 d29,d19[1]	;"
	             "	vdup.16 d28,d19[0]	;"
	             "	vdup.16 d27,d18[3]	;"
	             "	vdup.16 d26,d18[2]	;"
	             "	vdup.16 d25,d18[1]	;"
	             "	vdup.16 d24,d18[0]	;"
	             "	vdup.16 d23,d17[3]	;"
	             "	vdup.16 d22,d17[2]	;"
	             "	vdup.16 d21,d17[1]	;"
	             "	vdup.16 d20,d17[0]	;"
	             "	vdup.16 d19,d16[3]	;"
	             "	vdup.16 d18,d16[2]	;"
	             "	vdup.16 d17,d16[1]	;"
	             "	vdup.16 d16,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	vstmia r10!,{q8-q15}	;"
	             "	vstmia r11!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldrh lr, [%0],#2	;" // rest
	             "	vdup.16 d16, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {d16}	;"
	             "	vstmia r9!, {d16}	;"
	             "	vstmia r10!, {d16}	;"
	             "	vstmia r11!, {d16}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

void scale4x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale4x1_n16, &scale4x2_n16, &scale4x3_n16, &scale4x4_n16};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale4x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x1_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15,d19[1]	;"
	             "	vdup.32 q14,d19[0]	;"
	             "	vdup.32 q13,d18[1]	;"
	             "	vdup.32 q12,d18[0]	;"
	             "	vdup.32 q11,d17[1]	;"
	             "	vdup.32 q10,d17[0]	;"
	             "	vdup.32 q9,d16[1]	;"
	             "	vdup.32 q8,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 q8, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale4x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x2_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 2 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15,d19[1]	;"
	             "	vdup.32 q14,d19[0]	;"
	             "	vdup.32 q13,d18[1]	;"
	             "	vdup.32 q12,d18[0]	;"
	             "	vdup.32 q11,d17[1]	;"
	             "	vdup.32 q10,d17[0]	;"
	             "	vdup.32 q9,d16[1]	;"
	             "	vdup.32 q8,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 q8, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8}	;"
	             "	vstmia r9!, {q8}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15", "memory",
	               "cc");
}

void scale4x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x3_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 3 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15,d19[1]	;"
	             "	vdup.32 q14,d19[0]	;"
	             "	vdup.32 q13,d18[1]	;"
	             "	vdup.32 q12,d18[0]	;"
	             "	vdup.32 q11,d17[1]	;"
	             "	vdup.32 q10,d17[0]	;"
	             "	vdup.32 q9,d16[1]	;"
	             "	vdup.32 q8,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	vstmia r10!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 q8, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8}	;"
	             "	vstmia r9!, {q8}	;"
	             "	vstmia r10!, {q8}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "q15",
	               "memory", "cc");
}

void scale4x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw || !sh)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = swl * 4;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale4x4_c32(src, dst, sw, sh, sp, dw, dh, dp);
		return;
	}
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp * 4 - swl * 4;
	const uint8_t* finofs = (const uint8_t*)src + (sp * sh);
	asm volatile("1:	add lr, %0, %2		;" // lr = x32bytes offset
	             "	add r8, %0, %3		;" // r8 = lineend offset
	             "	add r9, %1, %7		;" // r9 = 2x line offset
	             "	add r10, r9, %7		;" // r10 = 3x line offset
	             "	add r11, r10, %7	;" // r11 = 4x line offset
	             "	cmp %0, lr		;"
	             "	beq 3f			;"
	             "2:	vldmia %0!,{q8-q9}	;" // 8 pixels 32 bytes
	             "	vdup.32 q15,d19[1]	;"
	             "	vdup.32 q14,d19[0]	;"
	             "	vdup.32 q13,d18[1]	;"
	             "	vdup.32 q12,d18[0]	;"
	             "	vdup.32 q11,d17[1]	;"
	             "	vdup.32 q10,d17[0]	;"
	             "	vdup.32 q9,d16[1]	;"
	             "	vdup.32 q8,d16[0]	;"
	             "	cmp %0, lr		;"
	             "	vstmia %1!,{q8-q15}	;"
	             "	vstmia r9!,{q8-q15}	;"
	             "	vstmia r10!,{q8-q15}	;"
	             "	vstmia r11!,{q8-q15}	;"
	             "	bne 2b			;"
	             "3:	cmp %0, r8		;"
	             "	beq 5f			;"
	             "4:	ldr lr, [%0],#4		;" // rest
	             "	vdup.32 q8, lr		;"
	             "	cmp %0, r8		;"
	             "	vstmia %1!, {q8}	;"
	             "	vstmia r9!, {q8}	;"
	             "	vstmia r10!, {q8}	;"
	             "	vstmia r11!, {q8}	;"
	             "	bne 4b			;"
	             "5:	add %0, %0, %4		;"
	             "	add %1, %1, %5		;"
	             "	cmp %0, %6		;"
	             "	bne 1b			"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl32), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	             : "r8", "r9", "r10", "r11", "lr", "q8", "q9", "q10", "q11", "q12", "q13", "q14",
	               "q15", "memory", "cc");
}

/** Variable vertical scale dispatcher for 4x horizontal with 1-4x vertical (32-bit NEON). */
void scale4x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (*const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                      uint32_t, uint32_t,
	                      uint32_t) = {&scale4x1_n32, &scale4x2_n32, &scale4x3_n32, &scale4x4_n32};
	if (--ymul < 4)
		func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

/**
 * Helper: 5x horizontal scale for a single line (16-bit pixels using NEON).
 *
 * Scales one scanline 5x horizontally using a different NEON technique than 2x/3x/4x.
 * For 5x scaling, uses vdup + vext to create patterns like [0][1][1][1][2].
 *
 * NEON technique for 5x:
 * - Load 8 pixels (16 bytes) at a time
 * - Use vdup.16 to replicate each pixel across registers
 * - Use vext.16 with different offsets to create the 5x pattern
 * - Each input pixel [A] becomes [A][A][A][A][A] distributed across output
 *
 * Example: pixels [0][1][2][3] become:
 * Output: [0][0][0][0][0][1][1][1][1][1][2][2][2][2][2][3][3][3][3][3]
 *
 * @param src Source line pointer
 * @param dst Destination line pointer
 * @param swl Source line width in bytes
 *
 * @note Called by scale5x_n16 for each output line
 */
void scale5x_n16line(const void* src, void* dst, uint32_t swl) {
	asm volatile("	bic r4, %2, #15		;" // r4 = swl16
	             "	add r3, %0, %2		;" // r3 = lineend offset
	             "	add r4, %0, r4		;" // r4 = x16bytes offset
	             "	cmp %0, r4		;"
	             "	beq 2f			;" // Skip to remainder if no 16-byte blocks
	             // Main loop: Process 16-byte blocks (8 pixels -> 40 pixels)
	             "1:	vldmia %0!, {q8}	;" // Load 16 bytes (8 RGB565 pixels: 0-7)
	             // Create 5x duplication pattern using vdup + vext
	             // Input pixels [0][1][2][3][4][5][6][7] -> 40 output pixels
	             "	vdup.16 d25, d17[3]	;" // d25 = 7777 (pixel 7 replicated)
	             "	vdup.16 d27, d17[2]	;" // d27 = 6666
	             "	vdup.16 d26, d17[1]	;" // d26 = 5555
	             "	vdup.16 d21, d17[0]	;" // d21 = 4444
	             "	vext.16 d24, d27,d25,#1	;" // d24 = 6667 (extract creates transition)
	             "	vext.16 d23, d26,d27,#2	;" // d23 = 5566 (2 of pixel 5, 2 of pixel 6)
	             "	vext.16 d22, d21,d26,#3	;" // d22 = 4555 (1 of pixel 4, 3 of pixel 5)
	             "	vdup.16 d20, d16[3]	;" // d20 = 3333
	             "	vdup.16 d27, d16[2]	;" // d27 = 2222
	             "	vdup.16 d26, d16[1]	;" // d26 = 1111
	             "	vdup.16 d16, d16[0]	;" // d16 = 0000
	             "	vext.16 d19, d27,d20,#1	;" // d19 = 2223
	             "	vext.16 d18, d26,d27,#2	;" // d18 = 1122
	             "	vext.16 d17, d16,d26,#3	;" // d17 = 0111
	             // Result in q8-q12: 80 bytes = 40 pixels (5x scaling of 8 input pixels)
	             "	cmp %0, r4		;"
	             "	vstmia %1!, {q8-q12}	;" // Store 80 bytes
	             "	bne 1b			;"
	             // Handle remainder pixels one at a time
	             "2:	cmp %0, r3		;"
	             "	beq 4f			;"
	             "3:	ldrh r4, [%0],#2	;" // Load one pixel
	             "	orr r4, r4, lsl #16	;" // Duplicate to both halves of register
	             "	cmp %0, r3		;"
	             "	str r4, [%1],#4		;" // Store 2 copies (4 bytes)
	             "	str r4, [%1],#4		;" // Store 2 more copies
	             "	strh r4, [%1],#2	;" // Store 1 final copy (total: 5 copies)
	             "	bne 3b			;"
	             "4:				" // Exit
	             : "+r"(src), "+r"(dst)
	             : "r"(swl)
	             : "r3", "r4", "q8", "q9", "q10", "q11", "q12", "q13", "memory", "cc");
}

/**
 * Generic 5x horizontal scale with variable vertical scaling (16-bit NEON version).
 *
 * Scales each line 5x horizontally using scale5x_n16line, then duplicates
 * each scaled line ymul times vertically. Uses memcpy_neon for line duplication.
 *
 * @param ymul Vertical scale factor
 *
 * @note Falls back to C version for unaligned data
 */
void scale5x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	uint32_t dwl = swl * 5;
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = dwl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, ymul);
		return;
	}
	const void* __restrict dstsrc;
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		scale5x_n16line(src, dst, swl);
		dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, dstsrc, dwl);
	}
}

/** 5x1 scale (5x horizontal, 1x vertical) for 16-bit pixels using NEON. */
void scale5x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
/** 5x2 scale for 16-bit pixels using NEON. */
void scale5x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
/** 5x3 scale for 16-bit pixels using NEON. */
void scale5x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
/** 5x4 scale for 16-bit pixels using NEON. */
void scale5x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
/** 5x5 scale for 16-bit pixels using NEON. */
void scale5x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 5);
}

/** Helper: 5x horizontal scale for a single line (32-bit pixels using NEON). */
void scale5x_n32line(const void* src, void* dst, uint32_t swl) {
	asm volatile("	bic r4, %2, #15		;" // r4 = swl16
	             "	add r3, %0, %2		;" // r3 = lineend offset
	             "	add r4, %0, r4		;" // r4 = x16bytes offset
	             "	cmp %0, r4		;"
	             "	beq 2f			;"
	             "1:	vldmia %0!,{q8}		;" // 4 pixels 16 bytes
	             "	vdup.32 q12, d17[1]	;" // 3333
	             "	vdup.32 q14, d17[0]	;" //  2222
	             "	vdup.32 q13, d16[1]	;" //  1111
	             "	vdup.32 q8, d16[0]	;" // 0000
	             "	vext.32 q11, q14,q12,#1	;" // 2223
	             "	vext.32 q10, q13,q14,#2	;" // 1122
	             "	vext.32 q9, q8,q13,#3	;" // 0111
	             "	cmp %0, r4		;"
	             "	vstmia %1!,{q8-q12}	;"
	             "	bne 1b			;"
	             "2:	cmp %0, r3		;"
	             "	beq 4f			;"
	             "3:	ldr r4, [%0],#4		;" // rest
	             "	vdup.32 q8, r4		;"
	             "	cmp %0, r3		;"
	             "	vstmia %1!, {q8}	;"
	             "	str r4, [%1],#4		;"
	             "	bne 3b			;"
	             "4:				"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl)
	             : "r3", "r4", "q8", "q9", "q10", "q11", "q12", "q13", "q14", "memory", "cc");
}

void scale5x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	uint32_t dwl = swl * 5;
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = dwl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, ymul);
		return;
	}
	const void* __restrict dstsrc;
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		scale5x_n32line(src, dst, swl);
		dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale5x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale5x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale5x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale5x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale5x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 5);
}

void scale6x_n16line(const void* src, void* dst, uint32_t swl) {
	asm volatile("	bic r4, %2, #15		;" // r4 = swl16
	             "	add r3, %0, %2		;" // r3 = lineend offset
	             "	add r4, %0, r4		;" // r4 = x16bytes offset
	             "	cmp %0, r4		;"
	             "	beq 2f			;"
	             "1:	vldmia %0!, {q8}	;" // 8 pixels 16 bytes
	             "	vdup.16 d27, d17[3]	;" //  7777
	             "	vdup.16 d25, d17[2]	;" //  6666
	             "	vdup.16 d24, d17[1]	;" //  5555
	             "	vdup.16 d22, d17[0]	;" //  4444
	             "	vext.16 d26, d25,d27,#2	;" // 6677
	             "	vext.16 d23, d22,d24,#2	;" // 4455
	             "	vdup.16 d21, d16[3]	;" //  3333
	             "	vdup.16 d19, d16[2]	;" //  2222
	             "	vdup.16 d18, d16[1]	;" //  1111
	             "	vdup.16 d16, d16[0]	;" //  0000
	             "	vext.16 d20, d19,d21,#2	;" // 2233
	             "	vext.16 d17, d16,d18,#2	;" // 0011
	             "	cmp %0, r4		;"
	             "	vstmia %1!, {q8-q13}	;"
	             "	bne 1b			;"
	             "2:	cmp %0, r3		;"
	             "	beq 4f			;"
	             "3:	ldrh r4, [%0],#2	;" // rest
	             "	orr r4, r4, lsl #16	;"
	             "	vdup.32 d16, r4		;"
	             "	cmp %0, r3		;"
	             "	vstmia %1!, {d16}	;"
	             "	str r4, [%1],#4		;"
	             "	bne 3b			;"
	             "4:				"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl)
	             : "r3", "r4", "q8", "q9", "q10", "q11", "q12", "q13", "memory", "cc");
}

void scale6x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint16_t);
	uint32_t dwl = swl * 6;
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = dwl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, ymul);
		return;
	}
	const void* __restrict dstsrc;
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		scale6x_n16line(src, dst, swl);
		dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale6x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale6x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale6x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale6x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale6x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 5);
}
void scale6x6_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 6);
}

void scale6x_n32line(const void* src, void* dst, uint32_t swl) {
	asm volatile("	bic r4, %2, #15		;" // r4 = swl16
	             "	add r3, %0, %2		;" // r3 = lineend offset
	             "	add r4, %0, r4		;" // r4 = x16bytes offset
	             "	cmp %0, r4		;"
	             "	beq 2f			;"
	             "1:	vldmia %0!,{q8}		;" // 4 pixels 16 bytes
	             "	vdup.32 q13, d17[1]	;" // 3333
	             "	vdup.32 q11, d17[0]	;" // 2222
	             "	vdup.32 q10, d16[1]	;" // 1111
	             "	vdup.32 q8, d16[0]	;" // 0000
	             "	vext.32 q12, q11,q13,#2	;" // 2233
	             "	vext.32 q9, q8,q10,#2	;" // 0011
	             "	cmp %0, r4		;"
	             "	vstmia %1!,{q8-q13}	;"
	             "	bne 1b			;"
	             "2:	cmp %0, r3		;"
	             "	beq 4f			;"
	             "3:	ldr r4, [%0],#4		;" // rest
	             "	vdup.32 q8, r4		;"
	             "	vmov d18, d16		;"
	             "	cmp %0, r3		;"
	             "	vstmia %1!, {d16-d18}	;"
	             "	bne 3b			;"
	             "4:				"
	             : "+r"(src), "+r"(dst)
	             : "r"(swl)
	             : "r3", "r4", "q8", "q9", "q10", "q11", "q12", "q13", "memory", "cc");
}

void scale6x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                 uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw || !sh || !ymul)
		return;
	uint32_t swl = sw * sizeof(uint32_t);
	uint32_t dwl = swl * 6;
	if (!sp) {
		sp = swl;
	}
	if (!dp) {
		dp = dwl;
	}
	if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3) || (sp & 3) || (dp & 3)) {
		scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, ymul);
		return;
	}
	const void* __restrict dstsrc;
	for (; sh > 0; sh--, src = (uint8_t*)src + sp) {
		scale6x_n32line(src, dst, swl);
		dstsrc = dst;
		dst = (uint8_t*)dst + dp;
		for (uint32_t i = ymul - 1; i > 0; i--, dst = (uint8_t*)dst + dp)
			memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale6x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 1);
}
void scale6x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 2);
}
void scale6x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 3);
}
void scale6x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 4);
}
void scale6x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 5);
}
void scale6x6_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 6);
}

void scaler_n16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (*const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                         uint32_t, uint32_t, uint32_t) = {
	    {&scale1x1_n16, &scale1x2_n16, &scale1x3_n16, &scale1x4_n16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale2x1_n16, &scale2x2_n16, &scale2x3_n16, &scale2x4_n16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale3x1_n16, &scale3x2_n16, &scale3x3_n16, &scale3x4_n16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale4x1_n16, &scale4x2_n16, &scale4x3_n16, &scale4x4_n16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale5x1_n16, &scale5x2_n16, &scale5x3_n16, &scale5x4_n16, &scale5x5_n16, &dummy, &dummy,
	     &dummy},
	    {&scale6x1_n16, &scale6x2_n16, &scale6x3_n16, &scale6x4_n16, &scale6x5_n16, &scale6x6_n16,
	     &dummy, &dummy}};
	if ((--xmul < 6) && (--ymul < 6))
		func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scaler_n32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (*const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                         uint32_t, uint32_t, uint32_t) = {
	    {&scale1x1_n32, &scale1x2_n32, &scale1x3_n32, &scale1x4_n32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale2x1_n32, &scale2x2_n32, &scale2x3_n32, &scale2x4_n32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale3x1_n32, &scale3x2_n32, &scale3x3_n32, &scale3x4_n32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale4x1_n32, &scale4x2_n32, &scale4x3_n32, &scale4x4_n32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale5x1_n32, &scale5x2_n32, &scale5x3_n32, &scale5x4_n32, &scale5x5_n32, &dummy, &dummy,
	     &dummy},
	    {&scale6x1_n32, &scale6x2_n32, &scale6x3_n32, &scale6x4_n32, &scale6x5_n32, &scale6x6_n32,
	     &dummy, &dummy}};
	if ((--xmul < 6) && (--ymul < 6))
		func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

#endif // HAS_NEON

///////////////////////////////
// High-Level Scaler Dispatchers
///////////////////////////////

/**
 * Generic scaler dispatcher for 16-bit pixels (C implementation).
 *
 * Selects the appropriate scale function based on horizontal (xmul) and
 * vertical (ymul) scale factors. Uses function pointer table for dispatch.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-6, up to 8 for some scales)
 *
 * @note This is the main entry point for C-based scaling (fallback or non-NEON platforms)
 */
void scaler_c16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (*const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                         uint32_t, uint32_t, uint32_t) = {
	    {&scale1x1_c16, &scale1x2_c16, &scale1x3_c16, &scale1x4_c16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale2x1_c16, &scale2x2_c16, &scale2x3_c16, &scale2x4_c16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale3x1_c16, &scale3x2_c16, &scale3x3_c16, &scale3x4_c16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale4x1_c16, &scale4x2_c16, &scale4x3_c16, &scale4x4_c16, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale5x1_c16, &scale5x2_c16, &scale5x3_c16, &scale5x4_c16, &scale5x5_c16, &dummy, &dummy,
	     &dummy},
	    {&scale6x1_c16, &scale6x2_c16, &scale6x3_c16, &scale6x4_c16, &scale6x5_c16, &scale6x6_c16,
	     &dummy, &dummy}};
	if ((--xmul < 6) && (--ymul < 6))
		func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

/**
 * Generic scaler dispatcher for 32-bit pixels (C implementation).
 *
 * Same as scaler_c16 but for 32-bit RGBA8888 pixels.
 *
 * @param xmul Horizontal scale factor (1-6)
 * @param ymul Vertical scale factor (1-6, up to 8 for some scales)
 */
void scaler_c32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst,
                uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (*const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t,
	                         uint32_t, uint32_t, uint32_t) = {
	    {&scale1x1_c32, &scale1x2_c32, &scale1x3_c32, &scale1x4_c32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale2x1_c32, &scale2x2_c32, &scale2x3_c32, &scale2x4_c32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale3x1_c32, &scale3x2_c32, &scale3x3_c32, &scale3x4_c32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale4x1_c32, &scale4x2_c32, &scale4x3_c32, &scale4x4_c32, &dummy, &dummy, &dummy,
	     &dummy},
	    {&scale5x1_c32, &scale5x2_c32, &scale5x3_c32, &scale5x4_c32, &scale5x5_c32, &dummy, &dummy,
	     &dummy},
	    {&scale6x1_c32, &scale6x2_c32, &scale6x3_c32, &scale6x4_c32, &scale6x5_c32, &scale6x6_c32,
	     &dummy, &dummy}};
	if ((--xmul < 6) && (--ymul < 6))
		func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

///////////////////////////////
// Advanced Scaling Algorithms (from gambatte-dms)
///////////////////////////////

// RGB565 color component extraction macros
#define cR(A) (((A) & 0xf800) >> 11) // Extract 5-bit red component
#define cG(A) (((A) & 0x7e0) >> 5) // Extract 6-bit green component
#define cB(A) ((A) & 0x1f) // Extract 5-bit blue component

// RGB565 weighted blending macros for smooth scaling
// These create intermediate colors by blending two RGB565 pixels
#define Weight2_3(A, B)                                                                            \
	(((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | /* 2/5 A + 3/5 B */                       \
	 ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 |                                            \
	 ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_1(A, B)                                                                            \
	((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | /* 3/4 A + 1/4 B (optimized) */                 \
	 (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight3_2(A, B)                                                                            \
	(((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | /* 3/5 A + 2/5 B */                       \
	 ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 |                                            \
	 ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))

#define MIN(a, b) (a) < (b) ? (a) : (b)

/**
 * Deinterlaced line-doubling scaler with blending.
 *
 * Special scaler for Game Boy emulation that handles interlaced output.
 * Copies even lines directly, blends odd lines from adjacent even lines
 * for smoother scaling. This reduces flicker in games that alternate
 * between scanlines.
 *
 * Algorithm:
 * - Even lines (y % 2 == 0): Direct copy
 * - Odd lines: Weighted blend of surrounding even lines using Weight3_1/Weight3_2
 *
 * @note From gambatte-dms emulator
 * @note Uses FIXED_BPP macro (bytes per pixel)
 */
void scale1x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	// pitch of src image not src buffer!
	// eg. gb has a 160 pixel wide image but
	// gambatte uses a 256 pixel wide buffer
	// (only matters when using memcpy)
	int ip = sw * FIXED_BPP;
	int src_stride = 2 * sp / FIXED_BPP;
	int dst_stride = 2 * dp / FIXED_BPP;
	int cpy_pitch = MIN(ip, dp);

	uint16_t k = 0x0000; // Black pixel for blending
	uint16_t* restrict src_row = (uint16_t*)src;
	uint16_t* restrict dst_row = (uint16_t*)dst;
	for (int y = 0; y < sh; y += 2) {
		// Copy even line directly
		memcpy(dst_row, src_row, cpy_pitch);
		dst_row += dst_stride;
		src_row += src_stride;
		// Blend odd line (75% current pixel + 25% black for scanline effect)
		for (unsigned x = 0; x < sw; x++) {
			uint16_t s = *(src_row + x);
			*(dst_row + x) = Weight3_1(s, k); // Blend with black
		}
	}
}

/**
 * 2x line-doubling scaler with blending.
 *
 * Similar to scale1x_line but with 2x horizontal scaling.
 * Creates scanline effect by blending duplicate lines.
 *
 * @note From gambatte-dms emulator
 */
void scale2x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x = 0; x < sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2(c1, k);

			*(dst_row) = c1;
			*(dst_row + 1) = c1;

			*(dst_row + dw) = c2;
			*(dst_row + dw + 1) = c2;

			src_row += 1;
			dst_row += 2;
		}
	}
}
/**
 * 3x line-tripling scaler with blending.
 *
 * Scales 3x with scanline effect. Each source row creates 3 destination rows
 * with different blending weights for a CRT-like appearance.
 *
 * @note From gambatte-dms emulator
 */
void scale3x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x = 0; x < sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2(c1, k);

			// row 1
			*(dst_row) = c2;
			*(dst_row + 1) = c2;
			*(dst_row + 2) = c2;

			// row 2
			*(dst_row + dw * 1) = c1;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2) = c1;
			*(dst_row + dw * 2 + 1) = c1;
			*(dst_row + dw * 2 + 2) = c1;

			src_row += 1;
			dst_row += 3;
		}
	}
}
void scale4x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	int row3 = dw * 2;
	int row4 = dw * 3;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 4;
		for (unsigned x = 0; x < sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2(c1, k);

			// row 1
			*(dst_row) = c1;
			*(dst_row + 1) = c1;
			*(dst_row + 2) = c1;
			*(dst_row + 3) = c1;

			// row 2
			*(dst_row + dw) = c2;
			*(dst_row + dw + 1) = c2;
			*(dst_row + dw + 2) = c2;
			*(dst_row + dw + 3) = c2;

			// row 3
			*(dst_row + row3) = c1;
			*(dst_row + row3 + 1) = c1;
			*(dst_row + row3 + 2) = c1;
			*(dst_row + row3 + 3) = c1;

			// row 4
			*(dst_row + row4) = c2;
			*(dst_row + row4 + 1) = c2;
			*(dst_row + row4 + 2) = c2;
			*(dst_row + row4 + 3) = c2;

			src_row += 1;
			dst_row += 4;
		}
	}
}

void scale2x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x = 0; x < sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_1(c1, k);

			*(dst_row) = c2;
			*(dst_row + 1) = c2;

			*(dst_row + dw) = c2;
			*(dst_row + dw + 1) = c1;

			src_row += 1;
			dst_row += 2;
		}
	}
}
void scale3x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp,
                  uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y = 0; y < sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x = 0; x < sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2(c1, k);
			uint16_t c3 = Weight2_3(c1, k);

			// row 1
			*(dst_row) = c2;
			*(dst_row + 1) = c1;
			*(dst_row + 2) = c1;

			// row 2
			*(dst_row + dw * 1) = c2;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2) = c3;
			*(dst_row + dw * 2 + 1) = c2;
			*(dst_row + dw * 2 + 2) = c2;

			src_row += 1;
			dst_row += 3;
		}
	}
}