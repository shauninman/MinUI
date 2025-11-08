// Math utility functions
#include "math_utils.h"

// Greatest common divisor using Euclidean algorithm
int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

// Color averaging for 16-bit RGB565 pixels
// Handles the special case where colors have different low bits
#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__APPLE__)
// ARM assembly optimized version
uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	__asm__("eor %0, %2, %3\r\n"
	        "and %0, %0, %1\r\n"
	        "add %0, %3, %0\r\n"
	        "add %0, %0, %2\r\n"
	        "lsr %0, %0, #1\r\n"
	        : "=&r"(ret)
	        : "r"(lowbits), "r"(c1), "r"(c2)
	        :);
	return ret;
}
#else
// C fallback version
uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821)) >> 1;
}
#endif

// Color averaging for 32-bit RGBA8888 pixels
// Handles overflow correctly
#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__APPLE__)
// ARM assembly optimized version
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	__asm__("eor %0, %3, %1\r\n"
	        "and %0, %0, %2\r\n"
	        "adds %0, %1, %0\r\n"
	        "and %1, %1, #0\r\n"
	        "movcs %1, #0x80000000\r\n"
	        "adds %0, %0, %3\r\n"
	        "rrx %0, %0\r\n"
	        "orr %0, %0, %1\r\n"
	        : "=&r"(ret), "+r"(c2)
	        : "r"(lowbits), "r"(c1)
	        : "cc");

	return ret;
}
#else
// C fallback version
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) ? 0x80000000 : 0;

	return (ret >> 1) | of;
}
#endif
