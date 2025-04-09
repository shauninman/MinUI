#ifndef __TOOL_H
#define __TOOL_H
#include <time.h>
#include <sys/wait.h>
#include <stdint.h>
#include "wmg_debug.h"
#include <stdbool.h>

#if __cplusplus
extern "C" {
#endif

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef long sys_time_t;

struct sys_time {
	sys_time_t sec;
	sys_time_t usec;
};

extern int get_process_state(const char *process_nae,int length);
extern int sys_get_time(struct sys_time *t);
void * wgos_zalloc(size_t size);
void * wgos_malloc(size_t size);

void ms_sleep(unsigned long ms);

#if __cplusplus
};  // extern "C"
#endif

#endif
