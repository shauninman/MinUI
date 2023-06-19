#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <poll.h>

#include <msettings.h>
#include <pthread.h>

#include "defines.h"

//	for keyshm
#define VOLUME		0
#define BRIGHTNESS	1
#define VOLMAX		20
#define BRIMAX		10

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

//	for button_flag
#define SELECT_BIT	0
#define START_BIT	1
#define SELECT		(1<<SELECT_BIT)
#define START		(1<<START_BIT)

#define KEY_PRESS 0x01

//
//	Main
//
void main(void) {
	InitSettings();
	
	// based on https://stackoverflow.com/a/55932967
	struct pollfd poll_fd;
	poll_fd.fd = open("/dev/input/event0", O_RDONLY|O_NONBLOCK);
	poll_fd.events = POLLIN;
	
    const int input_size = 4096;
    unsigned char input_data[input_size];
    memset(input_data,0,input_size);
	
	// Main Loop
	register uint32_t type;
	register uint32_t val;
	register uint32_t code;
	register uint32_t pressedbuttons = 0;
	register uint32_t button_flag = 0;
	uint32_t repeat_LR = 0;
	
	while (1) {
		if (poll(&poll_fd,1,5000)<0 || !poll_fd.revents) continue;
		if (read(poll_fd.fd,input_data,input_size)<0) break; // TODO: or continue?
		
		type = input_data[8]; // ev.type
		code = input_data[10]; // ev.code
		val = input_data[12]; // ev.value
		if (( type==KEY_PRESS ) || ( val <= REPEAT )) {
			if ( val < REPEAT ) {
				pressedbuttons += val;
				if (( val == RELEASED )&&( pressedbuttons > 0 )) pressedbuttons--;
			}
			switch (code) {
				case CODE_SELECT:
					if ( val != REPEAT ) {
						button_flag = button_flag & (~SELECT) | (val<<SELECT_BIT);
					}
					break;
				case CODE_START:
					if ( val != REPEAT ) {
						button_flag = button_flag & (~START) | (val<<START_BIT);
					} 
					break;
				case CODE_L1:
					if ( val == REPEAT ) {
						// Adjust repeat speed to 1/2
						val = repeat_LR;
						repeat_LR ^= PRESSED;
					} else {
						repeat_LR = 0;
					}
					if ( val == PRESSED ) {
						switch (button_flag) {
						case SELECT:
							// SELECT + L : volume down
							val = GetVolume();
							if (val>0) SetVolume(--val);
							break;
						case START:
							// START + L : brightness down
							val = GetBrightness();
							if (val>0) SetBrightness(--val);
							break;
						default:
							break;
						}
					}
					break;
				case CODE_R1:
					if ( val == REPEAT ) {
						// Adjust repeat speed to 1/2
						val = repeat_LR;
						repeat_LR ^= PRESSED;
					} else {
						repeat_LR = 0;
					}
					if ( val == PRESSED ) {
						switch (button_flag) {
						case SELECT:
							// SELECT + R : volume up
							val = GetVolume();
							if (val<VOLMAX) SetVolume(++val);
							break;
						case START:
							// START + R : brightness up
							val = GetBrightness();
							if (val<BRIMAX) SetBrightness(++val);
							break;
						default:
							break;
						}
					}
					break;
				default:
					break;
			}
		}
		memset(input_data,0,input_size);
	}
	close(poll_fd.fd);
}
