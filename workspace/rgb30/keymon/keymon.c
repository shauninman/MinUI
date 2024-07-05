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

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

// L3 or R3
#define CODE_MENU 317 // 11 in SDL for some reason...
#define CODE_MENU_ALT 318 // 12 in SDL...

#define CODE_PLUS 114
#define CODE_MINUS 115

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

#define INPUT_COUNT 6
static int inputs[INPUT_COUNT];
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/bus/platform/devices/singleadc-joypad/hp"
#define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

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
	int has_hdmi,had_hdmi;
	
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	has_hdmi = had_hdmi = getInt(HDMI_STATE_PATH);
	SetJack(has_headphones);
	SetHDMI(has_hdmi);
	
	while(1) {
		sleep(1);
		
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones!=has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}
		
		has_hdmi = getInt(HDMI_STATE_PATH);
		if (had_hdmi!=has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}
	
	return 0;
}


int main (int argc, char *argv[]) {
	printf("keymon\n"); fflush(stdout);
	InitSettings();
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);
	
	char path[32];
	for (int i=0; i<INPUT_COUNT-1; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	inputs[5] = open("/dev/input/js0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	
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
				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
				// printf("value: %i code: %i\n", val, ev.code); fflush(stdout);
				switch (ev.code) {
					case CODE_MENU:
					case CODE_MENU_ALT:
						menu_pressed = val;
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
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
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
