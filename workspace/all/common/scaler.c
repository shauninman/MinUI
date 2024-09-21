#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "platform.h" // for HAS_NEON

//
//	arm NEON / C integer scalers for ARMv7 devices
//	args/	src :	src offset		address of top left corner
//		dst :	dst offset		address	of top left corner
//		sw  :	src width		pixels
//		sh  :	src height		pixels
//		sp  :	src pitch (stride)	bytes	if 0, (src width * [2|4]) is used
//		dw  :	dst width		pixels
//		dh  :	dst height		pixels
//		dp  :	dst pitch (stride)	bytes	if 0, (src width * [2|4] * multiplier) is used
//
//	** NOTE **
//	since 32bit aligned addresses need to be processed for NEON scalers,
//	x-offset and stride pixels must be even# in the case of 16bpp,
//	if odd#, then handled by the C scaler
//

static void dummy(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {}

// 
// C scalers for Trimui Model S and GKD Pixel
//
void scale1x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=2) {
			pix = s[x];
			dpix1 = 0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) | ((pix & 0x001F0000) >> 13);			d[dx  ] = dpix1; d[dx+1] = dpix1;
			d[dx  ] = dpix1; d[dx+1] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) | ((pix16 & 0x001F) << 3);
			d[dx  ] = pix16; d[dx+1] = pix16;
		}
		dst = (uint8_t*)dst+dp;
	}
}
void scale2x_c16to32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=4) {
			pix = s[x];
			dpix1 = 0xFF000000 | ((pix & 0xF800) << 8) | ((pix & 0x07E0) << 5) | ((pix & 0x001F) << 3);
			dpix2 = 0xFF000000 | ((pix & 0xF8000000) >> 8) | ((pix & 0x07E00000) >> 11) | ((pix & 0x001F0000) >> 13);			d[dx  ] = dpix1; d[dx+1] = dpix1;
			d[dx+2] = dpix2; d[dx+3] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			pix16 = 0xFF000000 | ((pix16 & 0xF800) << 8) | ((pix16 & 0x07E0) << 5) | ((pix16 & 0x001F) << 3);
			d[dx  ] = pix16; d[dx+1] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		memcpy(dst, dstsrc, swl); dst = (uint8_t*)dst+dp;
	}
}

//
//	C scalers
//
void scale1x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ((ymul == 1)&&(swl == sp)&&(sp == dp)) memcpy(dst, src, sp*sh);
	else {
		if (swl>dp) swl = dp;
		for (; sh>0; sh--, src=(uint8_t*)src+sp) {
			for (uint32_t i=ymul; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, src, swl);
		}
	}
}

void scale1x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale1x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale1x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale1x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale1x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ((ymul == 1)&&(swl == sp)&&(sp == dp)) memcpy(dst, src, sp*sh);
	else {
		for (; sh>0; sh--, src=(uint8_t*)src+sp) {
			for (uint32_t i=ymul; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, src, swl);
		}
	}
}

void scale1x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale1x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale1x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale1x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale1x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale2x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=2) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			d[dx] = pix16|(pix16<<16);
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale2x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale2x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale2x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale2x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale2x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=2; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=2) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale2x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale2x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale2x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale2x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale2x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale3x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=3; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=3) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = pix; d[dx+2] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t *d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d16[(dx+1)*2] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale3x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale3x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale3x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale3x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale3x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=3; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=3) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale3x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale3x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale3x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale3x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale3x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale4x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=4; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=4) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix2; d[dx+3] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale4x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale4x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale4x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale4x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=4; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=4) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale4x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale4x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale4x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale4x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale4x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }

void scale5x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=5; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=5) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = pix; d[dx+3] = dpix2; d[dx+4] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t *d16 = (uint16_t*)d;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1; d16[(dx+2)*2] = pix16;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale5x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=5; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=5) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix; d[dx+4] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale5x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale6x_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, dpix1, dpix2, swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } swl*=6; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<(sw/2); x++, dx+=6) {
			pix = s[x];
			dpix1=(pix & 0x0000FFFF)|(pix<<16);
			dpix2=(pix & 0xFFFF0000)|(pix>>16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix1; d[dx+3] = dpix2; d[dx+4] = dpix2; d[dx+5] = dpix2;
		}
		if (sw&1) {
			uint16_t *s16 = (uint16_t*)s;
			uint16_t pix16 = s16[x*2];
			dpix1 = pix16|(pix16<<16);
			d[dx] = dpix1; d[dx+1] = dpix1; d[dx+2] = dpix1;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_c16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c16(src, dst, sw, sh, sp, dw, dh, dp, 6); }

void scale6x_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t x, dx, pix, swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } swl*=6; if (!dp) { dp = swl; }
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		uint32_t *s = (uint32_t* __restrict)src;
		uint32_t *d = (uint32_t* __restrict)dst;
		for (x=dx=0; x<sw; x++, dx+=6) {
			pix = s[x];
			d[dx] = pix; d[dx+1] = pix; d[dx+2] = pix; d[dx+3] = pix; d[dx+4] = pix; d[dx+5] = pix;
		}
		void* __restrict dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy(dst, dstsrc, swl);
	}
}

void scale6x1_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_c32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_c32(src, dst, sw, sh, sp, dw, dh, dp, 6); }

#ifdef HAS_NEON

//
//	memcpy_neon (dst/src must be aligned 4, size must be aligned 2)
//
void memcpy_neon(void* dst, void* src, uint32_t size) {
	asm volatile (
	"	bic r4, %[sz], #127	;"
	"	add r3, %[s], %[sz]	;"	// r3 = endofs
	"	add r4, %[s], r4	;"	// r4 = s128ofs
	"	cmp %[s], r4		;"
	"	beq 2f			;"
	"1:	vldmia %[s]!, {q8-q15}	;"	// 128 bytes
	"	vstmia %[d]!, {q8-q15}	;"
	"	cmp %[s], r4		;"
	"	bne 1b			;"
	"2:	cmp %[s], r3		;"
	"	beq 7f			;"
	"	tst %[sz], #64		;"
	"	beq 3f			;"
	"	vldmia %[s]!, {q8-q11}	;"	// 64 bytes
	"	vstmia %[d]!, {q8-q11}	;"
	"	cmp %[s], r3		;"
	"	beq 7f			;"
	"3:	tst %[sz], #32		;"
	"	beq 4f			;"
	"	vldmia %[s]!, {q12-q13}	;"	// 32 bytes
	"	vstmia %[d]!, {q12-q13}	;"
	"	cmp %[s], r3		;"
	"	beq 7f			;"
	"4:	tst %[sz], #16		;"
	"	beq 5f			;"
	"	vldmia %[s]!, {q14}	;"	// 16 bytes
	"	vstmia %[d]!, {q14}	;"
	"	cmp %[s], r3		;"
	"	beq 7f			;"
	"5:	tst %[sz], #8		;"
	"	beq 6f			;"
	"	vldmia %[s]!, {d30}	;"	// 8 bytes
	"	vstmia %[d]!, {d30}	;"
	"	cmp %[s], r3		;"
	"	beq 7f			;"
	"6:	ldrh r4, [%[s]],#2	;"	// rest
	"	strh r4, [%[d]],#2	;"
	"	cmp %[s], r3		;"
	"	bne 6b			;"
	"7:				"
	: [s]"+r"(src), [d]"+r"(dst)
	: [sz]"r"(size)
	: "r3","r4","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

//
//	NEON scalers
//

void scale1x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x1_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	if ((swl == sp)&&(sp == dp)) memcpy_neon(dst, src, sp*sh);
	else {
		if (swl>dp) swl = dp;
		for (; sh>0; sh--, src=(uint8_t*)src+sp, dst=(uint8_t*)dst+dp) memcpy_neon(dst, src, swl);
	}
}

void scale1x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x2_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
	"	vstmia %1!, {q8-q15}	;"
	"	vstmia r9!, {q8-q15}	;"
	"	cmp %0, lr		;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 8f			;"
	"	tst %3, #64		;"
	"	beq 4f			;"
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	vstmia r9!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	vstmia r9!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	vstmia r9!, {q14}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	vstmia r9!, {d30}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
	"	str lr, [%1],#4		;"
	"	str lr, [r9]		;"
	"8:	add %0, %0, %4		;"
	"	add %1, %1, %5		;"
	"	cmp %0, %6		;"
	"	bne 1b			"
	: "+r"(src), "+r"(dst)
	: "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x3_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
	"	vstmia %1!, {q8-q15}	;"
	"	vstmia r9!, {q8-q15}	;"
	"	vstmia r10!, {q8-q15}	;"
	"	cmp %0, lr		;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 8f			;"
	"	tst %3, #64		;"
	"	beq 4f			;"
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	vstmia r9!, {q8-q11}	;"
	"	vstmia r10!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	vstmia r9!, {q12-q13}	;"
	"	vstmia r10!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	vstmia r9!, {q14}	;"
	"	vstmia r10!, {q14}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	vstmia r9!, {d30}	;"
	"	vstmia r10!, {d30}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
	"	str lr, [%1],#4		;"
	"	str lr, [r9]		;"
	"	str lr, [r10]		;"
	"8:	add %0, %0, %4		;"
	"	add %1, %1, %5		;"
	"	cmp %0, %6		;"
	"	bne 1b			"
	: "+r"(src), "+r"(dst)
	: "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x4_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
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
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	vstmia r9!, {q8-q11}	;"
	"	vstmia r10!, {q8-q11}	;"
	"	vstmia r11!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	vstmia r9!, {q12-q13}	;"
	"	vstmia r10!, {q12-q13}	;"
	"	vstmia r11!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	vstmia r9!, {q14}	;"
	"	vstmia r10!, {q14}	;"
	"	vstmia r11!, {q14}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	vstmia r9!, {d30}	;"
	"	vstmia r10!, {d30}	;"
	"	vstmia r11!, {d30}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale1x1_n16, &scale1x2_n16, &scale1x3_n16, &scale1x4_n16 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}
void scale1x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*1; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x1_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	if ((swl == sp)&&(sp == dp)) memcpy_neon(dst, src, sp*sh);
	else for (; sh>0; sh--, src=(uint8_t*)src+sp, dst=(uint8_t*)dst+dp) memcpy_neon(dst, src, swl);
}

void scale1x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x2_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
	"	vstmia %1!, {q8-q15}	;"
	"	cmp %0, lr		;"
	"	vstmia r9!, {q8-q15}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 8f			;"
	"	tst %3, #64		;"
	"	beq 4f			;"
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	vstmia r9!, {q8-q11}	;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	vstmia r9!, {q12-q13}	;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	cmp %0, r8		;"
	"	vstmia r9!, {q14}	;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	cmp %0, r8		;"
	"	vstmia r9!, {d30}	;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
	"	str lr, [%1],#4		;"
	"	str lr, [r9]		;"
	"8:	add %0, %0, %4		;"
	"	add %1, %1, %5		;"
	"	cmp %0, %6		;"
	"	bne 1b			"
	: "+r"(src), "+r"(dst)
	: "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x3_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
	"	vstmia %1!, {q8-q15}	;"
	"	vstmia r9!, {q8-q15}	;"
	"	vstmia r10!, {q8-q15}	;"
	"	cmp %0, lr		;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 8f			;"
	"	tst %3, #64		;"
	"	beq 4f			;"
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	vstmia r9!, {q8-q11}	;"
	"	vstmia r10!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	vstmia r9!, {q12-q13}	;"
	"	vstmia r10!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	vstmia r9!, {q14}	;"
	"	vstmia r10!, {q14}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	vstmia r9!, {d30}	;"
	"	vstmia r10!, {d30}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
	"	str lr, [%1],#4		;"
	"	str lr, [r9]		;"
	"	str lr, [r10]		;"
	"8:	add %0, %0, %4		;"
	"	add %1, %1, %5		;"
	"	cmp %0, %6		;"
	"	bne 1b			"
	: "+r"(src), "+r"(dst)
	: "r"(swl128), "r"(swl), "r"(sadd), "r"(dadd), "r"(finofs), "r"(dp)
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw*sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale1x4_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl128 = swl & ~127;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x128bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q15}	;"	// 128 bytes
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
	"	vldmia %0!, {q8-q11}	;"	// 64 bytes
	"	vstmia %1!, {q8-q11}	;"
	"	vstmia r9!, {q8-q11}	;"
	"	vstmia r10!, {q8-q11}	;"
	"	vstmia r11!, {q8-q11}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"4:	tst %3, #32		;"
	"	beq 5f			;"
	"	vldmia %0!, {q12-q13}	;"	// 32 bytes
	"	vstmia %1!, {q12-q13}	;"
	"	vstmia r9!, {q12-q13}	;"
	"	vstmia r10!, {q12-q13}	;"
	"	vstmia r11!, {q12-q13}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"5:	tst %3, #16		;"
	"	beq 6f			;"
	"	vldmia %0!, {q14}	;"	// 16 bytes
	"	vstmia %1!, {q14}	;"
	"	vstmia r9!, {q14}	;"
	"	vstmia r10!, {q14}	;"
	"	vstmia r11!, {q14}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"6:	tst %3, #8		;"
	"	beq 7f			;"
	"	vldmia %0!, {d30}	;"	// 8 bytes
	"	vstmia %1!, {d30}	;"
	"	vstmia r9!, {d30}	;"
	"	vstmia r10!, {d30}	;"
	"	vstmia r11!, {d30}	;"
	"	cmp %0, r8		;"
	"	beq 8f			;"
	"7:	ldr lr, [%0],#4		;"	// 4 bytes
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale1x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale1x1_n32, &scale1x2_n32, &scale1x3_n32, &scale1x4_n32 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale2x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x1_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x64bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 32 pixels 64 bytes
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
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"	tst %3, #32		;"
	"	beq 4f			;"
	"	vldmia %0!,{q8-q9}	;"	// 16 pixels
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","lr","q0","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x2_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x64bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 32 pixels 64 bytes
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
	"	vldmia %0!,{q8-q9}	;"	// 16 pixels
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","lr","q0","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x3_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x64bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 32 pixels 64 bytes
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
	"	vldmia %0!,{q8-q9}	;"	// 16 pixels
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","lr","q0","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x3_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x64bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 32 pixels 64 bytes
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
	"	vldmia %0!,{q8-q9}	;"	// 16 pixels
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","r11","lr","q0","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale2x1_n16, &scale2x2_n16, &scale2x3_n16, &scale2x4_n16 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale2x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x1_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x64bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 16 pixels 64 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x2_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x64bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 16 pixels 64 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x3_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x64bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 16 pixels 64 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*2; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale2x4_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl64 = swl & ~63;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*2;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x64bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q11}	;"	// 16 pixels 64 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale2x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale2x1_n32, &scale2x2_n32, &scale2x3_n32, &scale2x4_n32 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale3x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x1_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q9}	;"	// 16 pixels 32 bytes
	"	vdup.16 d31, d19[3]	;"	//  FFFF
	"	vdup.16 d30, d19[2]	;"	//  EEEE
	"	vdup.16 d29, d19[1]	;"	//  DDDD
	"	vdup.16 d28, d19[0]	;"	//  CCCC
	"	vext.16 d27, d30,d31,#3	;"	// EFFF
	"	vext.16 d26, d29,d30,#2	;"	// DDEE
	"	vext.16 d25, d28,d29,#1	;"	// CCCD
	"	vdup.16 d31, d18[3]	;"	//  BBBB
	"	vdup.16 d30, d18[2]	;"	//  AAAA
	"	vdup.16 d29, d18[1]	;"	//  9999
	"	vdup.16 d28, d18[0]	;"	//  8888
	"	vext.16 d24, d30,d31,#3	;"	// ABBB
	"	vext.16 d23, d29,d30,#2	;"	// 99AA
	"	vext.16 d22, d28,d29,#1	;"	// 8889
	"	vdup.16 d31, d17[3]	;"	//  7777
	"	vdup.16 d30, d17[2]	;"	//  6666
	"	vdup.16 d29, d17[1]	;"	//  5555
	"	vdup.16 d28, d17[0]	;"	//  4444
	"	vext.16 d21, d30,d31,#3	;"	// 6777
	"	vext.16 d20, d29,d30,#2	;"	// 5566
	"	vext.16 d19, d28,d29,#1	;"	// 4445
	"	vdup.16 d31, d16[3]	;"	//  3333
	"	vdup.16 d30, d16[2]	;"	//  2222
	"	vdup.16 d29, d16[1]	;"	//  1111
	"	vdup.16 d28, d16[0]	;"	//  0000
	"	vext.16 d18, d30,d31,#3	;"	// 2333
	"	vext.16 d17, d29,d30,#2	;"	// 1122
	"	vext.16 d16, d28,d29,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!, {q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x2_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q9}	;"	// 16 pixels 32 bytes
	"	vdup.16 d31, d19[3]	;"	//  FFFF
	"	vdup.16 d30, d19[2]	;"	//  EEEE
	"	vdup.16 d29, d19[1]	;"	//  DDDD
	"	vdup.16 d28, d19[0]	;"	//  CCCC
	"	vext.16 d27, d30,d31,#3	;"	// EFFF
	"	vext.16 d26, d29,d30,#2	;"	// DDEE
	"	vext.16 d25, d28,d29,#1	;"	// CCCD
	"	vdup.16 d31, d18[3]	;"	//  BBBB
	"	vdup.16 d30, d18[2]	;"	//  AAAA
	"	vdup.16 d29, d18[1]	;"	//  9999
	"	vdup.16 d28, d18[0]	;"	//  8888
	"	vext.16 d24, d30,d31,#3	;"	// ABBB
	"	vext.16 d23, d29,d30,#2	;"	// 99AA
	"	vext.16 d22, d28,d29,#1	;"	// 8889
	"	vdup.16 d31, d17[3]	;"	//  7777
	"	vdup.16 d30, d17[2]	;"	//  6666
	"	vdup.16 d29, d17[1]	;"	//  5555
	"	vdup.16 d28, d17[0]	;"	//  4444
	"	vext.16 d21, d30,d31,#3	;"	// 6777
	"	vext.16 d20, d29,d30,#2	;"	// 5566
	"	vext.16 d19, d28,d29,#1	;"	// 4445
	"	vdup.16 d31, d16[3]	;"	//  3333
	"	vdup.16 d30, d16[2]	;"	//  2222
	"	vdup.16 d29, d16[1]	;"	//  1111
	"	vdup.16 d28, d16[0]	;"	//  0000
	"	vext.16 d18, d30,d31,#3	;"	// 2333
	"	vext.16 d17, d29,d30,#2	;"	// 1122
	"	vext.16 d16, d28,d29,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!, {q8-q13}	;"
	"	vstmia r9!, {q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x3_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q9}	;"	// 16 pixels 32 bytes
	"	vdup.16 d31, d19[3]	;"	//  FFFF
	"	vdup.16 d30, d19[2]	;"	//  EEEE
	"	vdup.16 d29, d19[1]	;"	//  DDDD
	"	vdup.16 d28, d19[0]	;"	//  CCCC
	"	vext.16 d27, d30,d31,#3	;"	// EFFF
	"	vext.16 d26, d29,d30,#2	;"	// DDEE
	"	vext.16 d25, d28,d29,#1	;"	// CCCD
	"	vdup.16 d31, d18[3]	;"	//  BBBB
	"	vdup.16 d30, d18[2]	;"	//  AAAA
	"	vdup.16 d29, d18[1]	;"	//  9999
	"	vdup.16 d28, d18[0]	;"	//  8888
	"	vext.16 d24, d30,d31,#3	;"	// ABBB
	"	vext.16 d23, d29,d30,#2	;"	// 99AA
	"	vext.16 d22, d28,d29,#1	;"	// 8889
	"	vdup.16 d31, d17[3]	;"	//  7777
	"	vdup.16 d30, d17[2]	;"	//  6666
	"	vdup.16 d29, d17[1]	;"	//  5555
	"	vdup.16 d28, d17[0]	;"	//  4444
	"	vext.16 d21, d30,d31,#3	;"	// 6777
	"	vext.16 d20, d29,d30,#2	;"	// 5566
	"	vext.16 d19, d28,d29,#1	;"	// 4445
	"	vdup.16 d31, d16[3]	;"	//  3333
	"	vdup.16 d30, d16[2]	;"	//  2222
	"	vdup.16 d29, d16[1]	;"	//  1111
	"	vdup.16 d28, d16[0]	;"	//  0000
	"	vext.16 d18, d30,d31,#3	;"	// 2333
	"	vext.16 d17, d29,d30,#2	;"	// 1122
	"	vext.16 d16, d28,d29,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!, {q8-q13}	;"
	"	vstmia r9!, {q8-q13}	;"
	"	vstmia r10!, {q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x4_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!, {q8-q9}	;"	// 16 pixels 32 bytes
	"	vdup.16 d31, d19[3]	;"	//  FFFF
	"	vdup.16 d30, d19[2]	;"	//  EEEE
	"	vdup.16 d29, d19[1]	;"	//  DDDD
	"	vdup.16 d28, d19[0]	;"	//  CCCC
	"	vext.16 d27, d30,d31,#3	;"	// EFFF
	"	vext.16 d26, d29,d30,#2	;"	// DDEE
	"	vext.16 d25, d28,d29,#1	;"	// CCCD
	"	vdup.16 d31, d18[3]	;"	//  BBBB
	"	vdup.16 d30, d18[2]	;"	//  AAAA
	"	vdup.16 d29, d18[1]	;"	//  9999
	"	vdup.16 d28, d18[0]	;"	//  8888
	"	vext.16 d24, d30,d31,#3	;"	// ABBB
	"	vext.16 d23, d29,d30,#2	;"	// 99AA
	"	vext.16 d22, d28,d29,#1	;"	// 8889
	"	vdup.16 d31, d17[3]	;"	//  7777
	"	vdup.16 d30, d17[2]	;"	//  6666
	"	vdup.16 d29, d17[1]	;"	//  5555
	"	vdup.16 d28, d17[0]	;"	//  4444
	"	vext.16 d21, d30,d31,#3	;"	// 6777
	"	vext.16 d20, d29,d30,#2	;"	// 5566
	"	vext.16 d19, d28,d29,#1	;"	// 4445
	"	vdup.16 d31, d16[3]	;"	//  3333
	"	vdup.16 d30, d16[2]	;"	//  2222
	"	vdup.16 d29, d16[1]	;"	//  1111
	"	vdup.16 d28, d16[0]	;"	//  0000
	"	vext.16 d18, d30,d31,#3	;"	// 2333
	"	vext.16 d17, d29,d30,#2	;"	// 1122
	"	vext.16 d16, d28,d29,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!, {q8-q13}	;"
	"	vstmia r9!, {q8-q13}	;"
	"	vstmia r10!, {q8-q13}	;"
	"	vstmia r11!, {q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale3x1_n16, &scale3x2_n16, &scale3x3_n16, &scale3x4_n16 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale3x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x1_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
	"	vdup.32 q15, d19[1]	;"	//  7777
	"	vdup.32 q14, d19[0]	;"	//  6666
	"	vdup.32 q1, d18[1]	;"	//  5555
	"	vdup.32 q0, d18[0]	;"	//  4444
	"	vext.32 q13, q14,q15,#3	;"	// 6777
	"	vext.32 q12, q1,q14,#2	;"	// 5566
	"	vext.32 q11, q0,q1,#1	;"	// 4445
	"	vdup.32 q15, d17[1]	;"	//  3333
	"	vdup.32 q14, d17[0]	;"	//  2222
	"	vdup.32 q1, d16[1]	;"	//  1111
	"	vdup.32 q0, d16[0]	;"	//  0000
	"	vext.32 q10, q14,q15,#3	;"	// 2333
	"	vext.32 q9, q1,q14,#2	;"	// 1122
	"	vext.32 q8, q0,q1,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!,{q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x2_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
	"	vdup.32 q15, d19[1]	;"	//  7777
	"	vdup.32 q14, d19[0]	;"	//  6666
	"	vdup.32 q1, d18[1]	;"	//  5555
	"	vdup.32 q0, d18[0]	;"	//  4444
	"	vext.32 q13, q14,q15,#3	;"	// 6777
	"	vext.32 q12, q1,q14,#2	;"	// 5566
	"	vext.32 q11, q0,q1,#1	;"	// 4445
	"	vdup.32 q15, d17[1]	;"	//  3333
	"	vdup.32 q14, d17[0]	;"	//  2222
	"	vdup.32 q1, d16[1]	;"	//  1111
	"	vdup.32 q0, d16[0]	;"	//  0000
	"	vext.32 q10, q14,q15,#3	;"	// 2333
	"	vext.32 q9, q1,q14,#2	;"	// 1122
	"	vext.32 q8, q0,q1,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!,{q8-q13}	;"
	"	vstmia r9!,{q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x3_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
	"	vdup.32 q15, d19[1]	;"	//  7777
	"	vdup.32 q14, d19[0]	;"	//  6666
	"	vdup.32 q1, d18[1]	;"	//  5555
	"	vdup.32 q0, d18[0]	;"	//  4444
	"	vext.32 q13, q14,q15,#3	;"	// 6777
	"	vext.32 q12, q1,q14,#2	;"	// 5566
	"	vext.32 q11, q0,q1,#1	;"	// 4445
	"	vdup.32 q15, d17[1]	;"	//  3333
	"	vdup.32 q14, d17[0]	;"	//  2222
	"	vdup.32 q1, d16[1]	;"	//  1111
	"	vdup.32 q0, d16[0]	;"	//  0000
	"	vext.32 q10, q14,q15,#3	;"	// 2333
	"	vext.32 q9, q1,q14,#2	;"	// 1122
	"	vext.32 q8, q0,q1,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!,{q8-q13}	;"
	"	vstmia r9!,{q8-q13}	;"
	"	vstmia r10!,{q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*3; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale3x4_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*3;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
	"	vdup.32 q15, d19[1]	;"	//  7777
	"	vdup.32 q14, d19[0]	;"	//  6666
	"	vdup.32 q1, d18[1]	;"	//  5555
	"	vdup.32 q0, d18[0]	;"	//  4444
	"	vext.32 q13, q14,q15,#3	;"	// 6777
	"	vext.32 q12, q1,q14,#2	;"	// 5566
	"	vext.32 q11, q0,q1,#1	;"	// 4445
	"	vdup.32 q15, d17[1]	;"	//  3333
	"	vdup.32 q14, d17[0]	;"	//  2222
	"	vdup.32 q1, d16[1]	;"	//  1111
	"	vdup.32 q0, d16[0]	;"	//  0000
	"	vext.32 q10, q14,q15,#3	;"	// 2333
	"	vext.32 q9, q1,q14,#2	;"	// 1122
	"	vext.32 q8, q0,q1,#1	;"	// 0001
	"	cmp %0, lr		;"
	"	vstmia %1!,{q8-q13}	;"
	"	vstmia r9!,{q8-q13}	;"
	"	vstmia r10!,{q8-q13}	;"
	"	vstmia r11!,{q8-q13}	;"
	"	bne 2b			;"
	"3:	cmp %0, r8		;"
	"	beq 5f			;"
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale3x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale3x1_n32, &scale3x2_n32, &scale3x3_n32, &scale3x4_n32 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale4x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x1_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 16 pixels 32 bytes
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x2_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 16 pixels 32 bytes
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x3_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 16 pixels 32 bytes
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint16_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x4_c16(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr  = x32bytes offset
	"	add r8, %0, %3		;"	// r8  = lineend offset
	"	add r9, %1, %7		;"	// r9  = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 16 pixels 32 bytes
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
	"4:	ldrh lr, [%0],#2	;"	// rest
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale4x1_n16, &scale4x2_n16, &scale4x3_n16, &scale4x4_n16 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale4x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x1_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x2_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*2 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x3_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*3 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	if (!sw||!sh) return;
	uint32_t swl = sw * sizeof(uint32_t);
	if (!sp) { sp = swl; } if (!dp) { dp = swl*4; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale4x4_c32(src,dst,sw,sh,sp,dw,dh,dp); return; }
	uint32_t swl32 = swl & ~31;
	uint32_t sadd = sp - swl;
	uint32_t dadd = dp*4 - swl*4;
	uint8_t* finofs = (uint8_t*)src + (sp*sh);
	asm volatile (
	"1:	add lr, %0, %2		;"	// lr = x32bytes offset
	"	add r8, %0, %3		;"	// r8 = lineend offset
	"	add r9, %1, %7		;"	// r9 = 2x line offset
	"	add r10, r9, %7		;"	// r10 = 3x line offset
	"	add r11, r10, %7	;"	// r11 = 4x line offset
	"	cmp %0, lr		;"
	"	beq 3f			;"
	"2:	vldmia %0!,{q8-q9}	;"	// 8 pixels 32 bytes
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
	"4:	ldr lr, [%0],#4		;"	// rest
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
	: "r8","r9","r10","r11","lr","q8","q9","q10","q11","q12","q13","q14","q15","memory","cc"
	);
}

void scale4x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	void (* const func[4])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)
		 = { &scale4x1_n32, &scale4x2_n32, &scale4x3_n32, &scale4x4_n32 };
	if (--ymul < 4) func[ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scale5x_n16line(void* src, void* dst, uint32_t swl) {
	asm volatile (
	"	bic r4, %2, #15		;"	// r4 = swl16
	"	add r3, %0, %2		;"	// r3 = lineend offset
	"	add r4, %0, r4		;"	// r4 = x16bytes offset
	"	cmp %0, r4		;"
	"	beq 2f			;"
	"1:	vldmia %0!, {q8}	;"	// 8 pixels 16 bytes
	"	vdup.16 d25, d17[3]	;"	//  7777
	"	vdup.16 d27, d17[2]	;"	//  6666
	"	vdup.16 d26, d17[1]	;"	//  5555
	"	vdup.16 d21, d17[0]	;"	//  4444
	"	vext.16 d24, d27,d25,#1	;"	// 6667
	"	vext.16 d23, d26,d27,#2	;"	// 5566
	"	vext.16 d22, d21,d26,#3	;"	// 4555
	"	vdup.16 d20, d16[3]	;"	//  3333
	"	vdup.16 d27, d16[2]	;"	//  2222
	"	vdup.16 d26, d16[1]	;"	//  1111
	"	vdup.16 d16, d16[0]	;"	//  0000
	"	vext.16 d19, d27,d20,#1	;"	// 2223
	"	vext.16 d18, d26,d27,#2	;"	// 1122
	"	vext.16 d17, d16,d26,#3	;"	// 0111
	"	cmp %0, r4		;"
	"	vstmia %1!, {q8-q12}	;"
	"	bne 1b			;"
	"2:	cmp %0, r3		;"
	"	beq 4f			;"
	"3:	ldrh r4, [%0],#2	;"	// rest
	"	orr r4, r4, lsl #16	;"
	"	cmp %0, r3		;"
	"	str r4, [%1],#4		;"
	"	str r4, [%1],#4		;"
	"	strh r4, [%1],#2	;"
	"	bne 3b			;"
	"4:				"
	: "+r"(src), "+r"(dst)
	: "r"(swl)
	: "r3","r4","q8","q9","q10","q11","q12","q13","memory","cc"
	);
}

void scale5x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw * sizeof(uint16_t);
	uint32_t dwl = swl*5;
	if (!sp) { sp = swl; } if (!dp) { dp = dwl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale5x_c16(src,dst,sw,sh,sp,dw,dh,dp,ymul); return; }
	void* __restrict dstsrc;
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		scale5x_n16line(src, dst, swl);
		dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale5x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n16(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale5x_n32line(void* src, void* dst, uint32_t swl) {
	asm volatile (
	"	bic r4, %2, #15		;"	// r4 = swl16
	"	add r3, %0, %2		;"	// r3 = lineend offset
	"	add r4, %0, r4		;"	// r4 = x16bytes offset
	"	cmp %0, r4		;"
	"	beq 2f			;"
	"1:	vldmia %0!,{q8}		;"	// 4 pixels 16 bytes
	"	vdup.32 q12, d17[1]	;"	// 3333
	"	vdup.32 q14, d17[0]	;"	//  2222
	"	vdup.32 q13, d16[1]	;"	//  1111
	"	vdup.32 q8, d16[0]	;"	// 0000
	"	vext.32 q11, q14,q12,#1	;"	// 2223
	"	vext.32 q10, q13,q14,#2	;"	// 1122
	"	vext.32 q9, q8,q13,#3	;"	// 0111
	"	cmp %0, r4		;"
	"	vstmia %1!,{q8-q12}	;"
	"	bne 1b			;"
	"2:	cmp %0, r3		;"
	"	beq 4f			;"
	"3:	ldr r4, [%0],#4		;"	// rest
	"	vdup.32 q8, r4		;"
	"	cmp %0, r3		;"
	"	vstmia %1!, {q8}	;"
	"	str r4, [%1],#4		;"
	"	bne 3b			;"
	"4:				"
	: "+r"(src), "+r"(dst)
	: "r"(swl)
	: "r3","r4","q8","q9","q10","q11","q12","q13","q14","memory","cc"
	);
}

void scale5x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw * sizeof(uint32_t);
	uint32_t dwl = swl*5;
	if (!sp) { sp = swl; } if (!dp) { dp = dwl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale5x_c32(src,dst,sw,sh,sp,dw,dh,dp,ymul); return; }
	void* __restrict dstsrc;
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		scale5x_n32line(src, dst, swl);
		dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale5x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale5x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale5x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale5x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale5x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale5x_n32(src, dst, sw, sh, sp, dw, dh, dp, 5); }

void scale6x_n16line(void* src, void* dst, uint32_t swl) {
	asm volatile (
	"	bic r4, %2, #15		;"	// r4 = swl16
	"	add r3, %0, %2		;"	// r3 = lineend offset
	"	add r4, %0, r4		;"	// r4 = x16bytes offset
	"	cmp %0, r4		;"
	"	beq 2f			;"
	"1:	vldmia %0!, {q8}	;"	// 8 pixels 16 bytes
	"	vdup.16 d27, d17[3]	;"	//  7777
	"	vdup.16 d25, d17[2]	;"	//  6666
	"	vdup.16 d24, d17[1]	;"	//  5555
	"	vdup.16 d22, d17[0]	;"	//  4444
	"	vext.16 d26, d25,d27,#2	;"	// 6677
	"	vext.16 d23, d22,d24,#2	;"	// 4455
	"	vdup.16 d21, d16[3]	;"	//  3333
	"	vdup.16 d19, d16[2]	;"	//  2222
	"	vdup.16 d18, d16[1]	;"	//  1111
	"	vdup.16 d16, d16[0]	;"	//  0000
	"	vext.16 d20, d19,d21,#2	;"	// 2233
	"	vext.16 d17, d16,d18,#2	;"	// 0011
	"	cmp %0, r4		;"
	"	vstmia %1!, {q8-q13}	;"
	"	bne 1b			;"
	"2:	cmp %0, r3		;"
	"	beq 4f			;"
	"3:	ldrh r4, [%0],#2	;"	// rest
	"	orr r4, r4, lsl #16	;"
	"	vdup.32 d16, r4		;"
	"	cmp %0, r3		;"
	"	vstmia %1!, {d16}	;"
	"	str r4, [%1],#4		;"
	"	bne 3b			;"
	"4:				"
	: "+r"(src), "+r"(dst)
	: "r"(swl)
	: "r3","r4","q8","q9","q10","q11","q12","q13","memory","cc"
	);
}

void scale6x_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw * sizeof(uint16_t);
	uint32_t dwl = swl*6;
	if (!sp) { sp = swl; } if (!dp) { dp = dwl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale6x_c16(src,dst,sw,sh,sp,dw,dh,dp,ymul); return; }
	void* __restrict dstsrc;
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		scale6x_n16line(src, dst, swl);
		dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale6x1_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_n16(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n16(src, dst, sw, sh, sp, dw, dh, dp, 6); }

void scale6x_n32line(void* src, void* dst, uint32_t swl) {
	asm volatile (
	"	bic r4, %2, #15		;"	// r4 = swl16
	"	add r3, %0, %2		;"	// r3 = lineend offset
	"	add r4, %0, r4		;"	// r4 = x16bytes offset
	"	cmp %0, r4		;"
	"	beq 2f			;"
	"1:	vldmia %0!,{q8}		;"	// 4 pixels 16 bytes
	"	vdup.32 q13, d17[1]	;"	// 3333
	"	vdup.32 q11, d17[0]	;"	// 2222
	"	vdup.32 q10, d16[1]	;"	// 1111
	"	vdup.32 q8, d16[0]	;"	// 0000
	"	vext.32 q12, q11,q13,#2	;"	// 2233
	"	vext.32 q9, q8,q10,#2	;"	// 0011
	"	cmp %0, r4		;"
	"	vstmia %1!,{q8-q13}	;"
	"	bne 1b			;"
	"2:	cmp %0, r3		;"
	"	beq 4f			;"
	"3:	ldr r4, [%0],#4		;"	// rest
	"	vdup.32 q8, r4		;"
	"	vmov d18, d16		;"
	"	cmp %0, r3		;"
	"	vstmia %1!, {d16-d18}	;"
	"	bne 3b			;"
	"4:				"
	: "+r"(src), "+r"(dst)
	: "r"(swl)
	: "r3","r4","q8","q9","q10","q11","q12","q13","memory","cc"
	);
}

void scale6x_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp, uint32_t ymul) {
	if (!sw||!sh||!ymul) return;
	uint32_t swl = sw * sizeof(uint32_t);
	uint32_t dwl = swl*6;
	if (!sp) { sp = swl; } if (!dp) { dp = dwl; }
	if ( ((uintptr_t)src&3)||((uintptr_t)dst&3)||(sp&3)||(dp&3) ) { scale6x_c32(src,dst,sw,sh,sp,dw,dh,dp,ymul); return; }
	void* __restrict dstsrc;
	for (; sh>0; sh--, src=(uint8_t*)src+sp) {
		scale6x_n32line(src, dst, swl);
		dstsrc = dst; dst = (uint8_t*)dst+dp;
		for (uint32_t i=ymul-1; i>0; i--, dst=(uint8_t*)dst+dp) memcpy_neon(dst, dstsrc, dwl);
	}
}

void scale6x1_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 1); }
void scale6x2_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 2); }
void scale6x3_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 3); }
void scale6x4_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 4); }
void scale6x5_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 5); }
void scale6x6_n32(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	scale6x_n32(src, dst, sw, sh, sp, dw, dh, dp, 6); }

void scaler_n16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_n16, &scale1x2_n16, &scale1x3_n16, &scale1x4_n16, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_n16, &scale2x2_n16, &scale2x3_n16, &scale2x4_n16, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_n16, &scale3x2_n16, &scale3x3_n16, &scale3x4_n16, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_n16, &scale4x2_n16, &scale4x3_n16, &scale4x4_n16, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_n16, &scale5x2_n16, &scale5x3_n16, &scale5x4_n16, &scale5x5_n16, &dummy, &dummy, &dummy },
			{ &scale6x1_n16, &scale6x2_n16, &scale6x3_n16, &scale6x4_n16, &scale6x5_n16, &scale6x6_n16, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scaler_n32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_n32, &scale1x2_n32, &scale1x3_n32, &scale1x4_n32, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_n32, &scale2x2_n32, &scale2x3_n32, &scale2x4_n32, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_n32, &scale3x2_n32, &scale3x3_n32, &scale3x4_n32, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_n32, &scale4x2_n32, &scale4x3_n32, &scale4x4_n32, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_n32, &scale5x2_n32, &scale5x3_n32, &scale5x4_n32, &scale5x5_n32, &dummy, &dummy, &dummy },
			{ &scale6x1_n32, &scale6x2_n32, &scale6x3_n32, &scale6x4_n32, &scale6x5_n32, &scale6x6_n32, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

#endif

void scaler_c16(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_c16, &scale1x2_c16, &scale1x3_c16, &scale1x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_c16, &scale2x2_c16, &scale2x3_c16, &scale2x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_c16, &scale3x2_c16, &scale3x3_c16, &scale3x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_c16, &scale4x2_c16, &scale4x3_c16, &scale4x4_c16, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_c16, &scale5x2_c16, &scale5x3_c16, &scale5x4_c16, &scale5x5_c16, &dummy, &dummy, &dummy },
			{ &scale6x1_c16, &scale6x2_c16, &scale6x3_c16, &scale6x4_c16, &scale6x5_c16, &scale6x6_c16, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}

void scaler_c32(uint32_t xmul, uint32_t ymul, void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	void (* const func[6][8])(void* __restrict, void* __restrict, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) = {
			{ &scale1x1_c32, &scale1x2_c32, &scale1x3_c32, &scale1x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale2x1_c32, &scale2x2_c32, &scale2x3_c32, &scale2x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale3x1_c32, &scale3x2_c32, &scale3x3_c32, &scale3x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale4x1_c32, &scale4x2_c32, &scale4x3_c32, &scale4x4_c32, &dummy, &dummy, &dummy, &dummy },
			{ &scale5x1_c32, &scale5x2_c32, &scale5x3_c32, &scale5x4_c32, &scale5x5_c32, &dummy, &dummy, &dummy },
			{ &scale6x1_c32, &scale6x2_c32, &scale6x3_c32, &scale6x4_c32, &scale6x5_c32, &scale6x6_c32, &dummy, &dummy }
		   };
	if ((--xmul < 6)&&(--ymul < 6)) func[xmul][ymul](src, dst, sw, sh, sp, dw, dh, dp);
	return;
}


// from gambatte-dms
//from RGB565
#define cR(A) (((A) & 0xf800) >> 11)
#define cG(A) (((A) & 0x7e0) >> 5)
#define cB(A) ((A) & 0x1f)
//to RGB565
#define Weight2_3(A, B)  (((((cR(A) << 1) + (cR(B) * 3)) / 5) & 0x1f) << 11 | ((((cG(A) << 1) + (cG(B) * 3)) / 5) & 0x3f) << 5 | ((((cB(A) << 1) + (cB(B) * 3)) / 5) & 0x1f))
#define Weight3_1(A, B)  ((((cR(B) + (cR(A) * 3)) >> 2) & 0x1f) << 11 | (((cG(B) + (cG(A) * 3)) >> 2) & 0x3f) << 5 | (((cB(B) + (cB(A) * 3)) >> 2) & 0x1f))
#define Weight3_2(A, B)  (((((cR(B) << 1) + (cR(A) * 3)) / 5) & 0x1f) << 11 | ((((cG(B) << 1) + (cG(A) * 3)) / 5) & 0x3f) << 5 | ((((cB(B) << 1) + (cB(A) * 3)) / 5) & 0x1f))

#define MIN(a, b) (a) < (b) ? (a) : (b)
void scale1x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	// pitch of src image not src buffer!
	// eg. gb has a 160 pixel wide image but 
	// gambatte uses a 256 pixel wide buffer
	// (only matters when using memcpy) 
	int ip = sw * FIXED_BPP; 
	int src_stride = 2 * sp / FIXED_BPP;
	int dst_stride = 2 * dp / FIXED_BPP;
	int cpy_pitch = MIN(ip, dp);
	
	uint16_t k = 0x0000;
	uint16_t* restrict src_row = (uint16_t*)src;
	uint16_t* restrict dst_row = (uint16_t*)dst;
	for (int y=0; y<sh; y+=2) {
		memcpy(dst_row, src_row, cpy_pitch);
		dst_row += dst_stride;
		src_row += src_stride;
		for (unsigned x=0; x<sw; x++) {
			uint16_t s = *(src_row + x);
			*(dst_row + x) = Weight3_1(s, k);
		}
	}
}
void scale2x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			*(dst_row     ) = c1;
			*(dst_row + 1 ) = c1;
			
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c2;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
void scale3x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row             ) = c2;
			*(dst_row          + 1) = c2;
			*(dst_row          + 2) = c2;

			// row 2
			*(dst_row + dw * 1    ) = c1;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2    ) = c1;
			*(dst_row + dw * 2 + 1) = c1;
			*(dst_row + dw * 2 + 2) = c1;

			src_row += 1;
			dst_row += 3;
		}
	}
}
void scale4x_line(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	int row3 = dw * 2;
	int row4 = dw * 3;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 4;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			
			// row 1
			*(dst_row    ) = c1;
			*(dst_row + 1) = c1;
			*(dst_row + 2) = c1;
			*(dst_row + 3) = c1;
			
			// row 2
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c2;
			*(dst_row + dw + 2) = c2;
			*(dst_row + dw + 3) = c2;

			// row 3
			*(dst_row + row3    ) = c1;
			*(dst_row + row3 + 1) = c1;
			*(dst_row + row3 + 2) = c1;
			*(dst_row + row3 + 3) = c1;

			// row 4
			*(dst_row + row4    ) = c2;
			*(dst_row + row4 + 1) = c2;
			*(dst_row + row4 + 2) = c2;
			*(dst_row + row4 + 3) = c2;

			src_row += 1;
			dst_row += 4;
		}
	}
}

void scale2x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 2;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_1( c1, k);
			
			*(dst_row     ) = c2;
			*(dst_row + 1 ) = c2;
			
			*(dst_row + dw    ) = c2;
			*(dst_row + dw + 1) = c1;
			
			src_row += 1;
			dst_row += 2;
		}
	}
}
void scale3x_grid(void* __restrict src, void* __restrict dst, uint32_t sw, uint32_t sh, uint32_t sp, uint32_t dw, uint32_t dh, uint32_t dp) {
	dw = dp / 2;
	uint16_t k = 0x0000;
	for (unsigned y=0; y<sh; y++) {
		uint16_t* restrict src_row = (void*)src + y * sp;
		uint16_t* restrict dst_row = (void*)dst + y * dp * 3;
		for (unsigned x=0; x<sw; x++) {
			uint16_t c1 = *src_row;
			uint16_t c2 = Weight3_2( c1, k);
			uint16_t c3 = Weight2_3( c1, k);
			
			// row 1
			*(dst_row                       ) = c2;
			*(dst_row                    + 1) = c1;
			*(dst_row                    + 2) = c1;

			// row 2
			*(dst_row + dw * 1    ) = c2;
			*(dst_row + dw * 1 + 1) = c1;
			*(dst_row + dw * 1 + 2) = c1;

			// row 3
			*(dst_row + dw * 2    ) = c3;
			*(dst_row + dw * 2 + 1) = c2;
			*(dst_row + dw * 2 + 2) = c2;

			src_row += 1;
			dst_row += 3;
		}
	}
}