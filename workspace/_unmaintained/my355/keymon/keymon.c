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

// uses different codes from SDL
#define CODE_MENU		1
#define CODE_PLUS		115
#define CODE_MINUS		114

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
void getFile(char* path, char* buffer, size_t buffer_size) {
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size>buffer_size-1) size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}
int exactMatch(char* str1, char* str2) {
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}

static int	input_fd = 0;
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/class/gpio/gpio150/value"
#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"

static int JACK_enabled(void) {
	return !getInt(JACK_STATE_PATH);
}
static int HDMI_enabled(void) {
	char value[64];
	getFile(HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
}

static void* watchPorts(void *arg) {
	int has_jack,had_jack;
	had_jack = had_jack = JACK_enabled();
	SetJack(has_jack);

	int has_hdmi,had_hdmi;
	has_hdmi = had_hdmi = HDMI_enabled();
	SetHDMI(has_hdmi);
	
	while(1) {
		sleep(1);

		has_jack = JACK_enabled();
		if (had_jack!=has_jack) {
			had_jack = has_jack;
			SetJack(has_jack);
		}
		
		has_hdmi = HDMI_enabled();
		if (had_hdmi!=has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}
	
	return 0;
}

int main (int argc, char *argv[]) {
	InitSettings();
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);
	
	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	
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
		// TODO: check if if necessary
		if (now-then>1000) ignore = 1; // ignore input that arrived during sleep
		
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ignore) continue;
			val = ev.value;

			if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
			// printf("code: %i (%i)\n", ev.code, val); fflush(stdout);
			switch (ev.code) {
				case CODE_MENU:
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
		
		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}
		
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness up\n"); fflush(stdout);
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				// printf("volume up\n"); fflush(stdout);
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}
			
			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100;
		}
		
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				// printf("brightness down\n"); fflush(stdout);
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				// printf("volume down\n"); fflush(stdout);
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
