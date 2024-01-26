#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <pthread.h>

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// #include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

#define CODE_PLUS		115
#define CODE_MINUS		114

#define CODE_SELECT		 54
#define CODE_START		 28
#define CODE_L1		 	 38
#define CODE_R1		 	 19

#define RELEASED	0
#define PRESSED		1

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/devices/virtual/switch/h2w/state" // 0 or 2

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

static void* watchPorts(void *arg) {
	int has_headphones,had_headphones;
	
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	SetJack(has_headphones);
	
	while(1) {
		sleep(1);
		
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones!=has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}
	}
	
	return 0;
}

int main (int argc, char *argv[]) {
	printf("keymon\n"); fflush(stdout);
	InitSettings();
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);
	
	char path[256];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	
	// TODO: make sure manually handling repeat is necessary on this device
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
		
		int input;
		static struct input_event event;
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
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
					case CODE_PLUS:
					case CODE_MINUS:
						system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
						SetVolume(GetVolume());
					break;
					default:
					break;
				}
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
