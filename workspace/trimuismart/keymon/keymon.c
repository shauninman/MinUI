#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "defines.h"

#define RELEASED	0
#define PRESSED		1

#define INPUT_COUNT 2
static int	input_fd = 0;
static struct input_event ev;

int main (int argc, char *argv[]) {
	InitSettings();
	
	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	
	uint32_t input;
	uint32_t val;
	uint32_t start_pressed = 0;
	uint32_t select_pressed = 0;
	
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;
	
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;
	
	uint32_t now;
	struct timeval tod;
	
	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ev.type != EV_KEY ) continue;
			val = ev.value;
			
			switch (ev.code) {
				case CODE_START:
					start_pressed = val;
				break;
				case CODE_SELECT:
					select_pressed = val;
				break;
				case CODE_R1:
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300;
				break;
				case CODE_L1:
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300;
				break;
				default:
				break;
			}
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (start_pressed) {
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else if (select_pressed) {
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (start_pressed) {
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else if (select_pressed) {
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}
			
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		usleep(16666); // 60fps
	}
}
