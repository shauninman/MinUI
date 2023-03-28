#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>

#include <mi_ao.h>

#define FREQ		48000
#define	BUSY_THRESHOLD	((FREQ<<2)/(1000/20))	// 20ms

MI_S32 MI_AO_SendFrame(MI_AUDIO_DEV AoDevId, MI_AO_CHN AoChn, MI_AUDIO_Frame_t *pstData, MI_S32 s32MilliSec) {
	static void* mi_ao_handle = NULL;
	static MI_S32 (*_MI_AO_SendFrame)(MI_AUDIO_DEV AoDevId, MI_AO_CHN AoChn, MI_AUDIO_Frame_t *pstData, MI_S32 s32MilliSec);
	static MI_S32 (*_MI_AO_QueryChnStat)(MI_AUDIO_DEV AoDevId , MI_AO_CHN AoChn, MI_AO_ChnState_t *pstStatus);

	if (!mi_ao_handle) {
		mi_ao_handle = dlopen("/config/lib/libmi_ao.so", RTLD_LAZY);
		_MI_AO_SendFrame = dlsym(mi_ao_handle, "MI_AO_SendFrame");
		_MI_AO_QueryChnStat = dlsym(mi_ao_handle, "MI_AO_QueryChnStat");
	}

	MI_S32 ret = _MI_AO_SendFrame(AoDevId, AoChn, pstData, 0);

	if (s32MilliSec) {
		MI_AO_ChnState_t status;
		_MI_AO_QueryChnStat(AoDevId, AoChn, &status);
		if (status.u32ChnBusyNum > BUSY_THRESHOLD) {
			useconds_t usleepclock = (uint64_t)(status.u32ChnBusyNum - BUSY_THRESHOLD) * 1000000 / (FREQ << 2);
			if (usleepclock) usleep(usleepclock);
		}
	}

	return ret;
}
