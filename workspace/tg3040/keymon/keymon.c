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

#define CODE_MENU0		314
#define CODE_MENU1		315
#define CODE_MENU2		316
#define CODE_PLUS		115
#define CODE_MINUS		114
#define CODE_MUTE		1
#define CODE_JACK		2

// keymon and api might need different codes

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define MUTE_STATE_PATH "/sys/class/gpio/gpio243/value"

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT] = {};
static struct input_event ev;

static int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

static pthread_t mute_pt;
static void* watchMute(void *arg) {
	int is_muted,was_muted;
	
	is_muted = was_muted = getInt(MUTE_STATE_PATH);
	SetMute(is_muted);
	
	while(1) {
		usleep(200000); // 5 times per second
		
		is_muted = getInt(MUTE_STATE_PATH);
		if (was_muted!=is_muted) {
			was_muted = is_muted;
			SetMute(is_muted);
		}
	}
	
	return 0;
}

int main (int argc, char *argv[]) {
	InitSettings();
	pthread_create(&mute_pt, NULL, &watchMute, NULL);
	
	char path[32];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	
	uint32_t input;
	uint32_t val;
	uint32_t menu_pressed = 0;
	
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;
	
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;
	
	uint8_t ignore;
	uint32_t then;
	uint32_t now;
	struct timeval tod;
	
	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000; // essential SDL_GetTicks()
	ignore = 0;
	
	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;
		if (now-then>1000) ignore = 1; // ignore input that arrived during sleep
		
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;
				if (ev.type==EV_SW) {
					printf("switch: %i\n", ev.code);
					if (ev.code==CODE_JACK) {
						printf("jack: %i\n", val);
						SetJack(val);
					}
					else if (ev.code==CODE_MUTE) {
						printf("mute: %i\n", val);
						SetMute(val);
					}
				}
				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
				printf("code: %i (%i)\n", ev.code, val); fflush(stdout);
				switch (ev.code) {
					case CODE_MENU0:
					case CODE_MENU1:
					case CODE_MENU2:
						menu_pressed = val;
					break;
					break;
					case CODE_PLUS:
						up_pressed = up_just_pressed = val;
						if (val) up_repeat_at = now + 300;
					break;
					case CODE_MINUS:
						down_pressed = down_just_pressed = val;
						if (val) down_repeat_at = now + 300;
					break;
					default:
					break;
				}
			}
		}
		
		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				printf("brightness up\n"); fflush(stdout);
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				printf("volume up\n"); fflush(stdout);
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				printf("brightness down\n"); fflush(stdout);
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				printf("volume down\n"); fflush(stdout);
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}
			
			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100;
		}
		
		then = now;
		ignore = 0;
		
		usleep(16666); // 60fps
	}
}
