#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>

// Greatest common divisor using Euclidean algorithm
int gcd(int a, int b);

// Color averaging for 16-bit RGB565 pixels
uint32_t average16(uint32_t c1, uint32_t c2);

// Color averaging for 32-bit RGBA8888 pixels
uint32_t average32(uint32_t c1, uint32_t c2);

#endif
