#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <SDL/SDL.h>


//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];
static struct input_event ev;

#define O_CLOEXEC 0

void raw_input(void) {
	puts("raw"); fflush(stdout);
	char path[32];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
		printf("path %i: %s\n", i, path); fflush(stdout);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	
	int count = 0;
	
	uint32_t input;
	while (1) {
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ev.type!=EV_KEY && ev.type!=EV_ABS) continue;
				printf("input: %i type:%i code:%i value:%i\n", i, ev.type,ev.code,ev.value); fflush(stdout);
				
				count += 1;
				if (count>10) return;
			}
		}
		usleep(16666);
	}
}

void sdl_input(void) {
	SDL_Init(SDL_INIT_VIDEO);
	puts("sdl"); fflush(stdout);
	
	SDL_SetVideoMode(0,0,0,0);
	int count = 0;
	
	while (1) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type==SDL_KEYDOWN || event.type==SDL_KEYUP) {
				uint8_t code = event.key.keysym.scancode;
				uint8_t pressed = event.type==SDL_KEYDOWN;
				printf("key event: %i (%i)\n", code,pressed); fflush(stdout);
			}
			
			count += 1;
			if (count>10) return;
		}
	}
	SDL_Quit();
}

int main (int argc, char *argv[]) {
	// printf("input.elf\n"); fflush(stdout);
	// raw_input();
	sdl_input();
}
