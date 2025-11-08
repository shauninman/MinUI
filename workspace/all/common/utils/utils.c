// Core timing utilities
#include "utils.h"
#include <sys/time.h>

uint64_t getMicroseconds(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}
