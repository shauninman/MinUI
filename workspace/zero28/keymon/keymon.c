/**
 * keymon.c - Zero28 hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the Zero28 handheld device. Features:
 * - Volume and brightness control through button combinations
 * - Headphone jack detection via EV_SW events
 *
 * Button combinations:
 * - MENU+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Also monitors headphone jack switch events from input device.
 *
 * Runs continuously at 60Hz polling multiple input devices (event2 and event3).
 * Ignores stale input after system sleep to prevent spurious events.
 */

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

// #include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

#define CODE_MENU		158
#define CODE_PLUS		115
#define CODE_MINUS		114
#define CODE_JACK		2

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT] = {};
static struct input_event ev;

/**
 * Reads an integer value from a sysfs file.
 *
 * Used for reading hardware state from kernel interfaces.
 *
 * @param path Path to sysfs file
 * @return Integer value read from file, or 0 if file cannot be opened
 */
static int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls multiple input devices for button events and switch events,
 * handling:
 * - MENU+PLUS: Increase brightness
 * - MENU+MINUS: Decrease brightness
 * - PLUS alone: Increase volume
 * - MINUS alone: Decrease volume
 * - EV_SW CODE_JACK: Headphone jack switch events
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval)
 * and ignores stale input after system sleep (> 1 second gap).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return Never returns (runs infinite loop)
 *
 * @note Uses event2 and event3 input devices (starts at event1, opens event2-3)
 */
int main (int argc, char *argv[]) {
	InitSettings();

	// Open input devices (event2 and event3)
	char path[32];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i+1); // Opens event2 and event3
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}

	uint32_t input;
	uint32_t val;
	uint32_t menu_pressed = 0;

	// Track PLUS button state for repeat handling
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;

	// Track MINUS button state for repeat handling
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;

	uint8_t ignore;
	uint32_t then;
	uint32_t now;
	struct timeval tod;

	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000; // SDL_GetTicks() equivalent
	ignore = 0;

	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;

		// Ignore stale input after system sleep to avoid spurious events
		if (now-then>1000) ignore = 1;

		// Poll both input devices for events
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;

				// Process switch events (headphone jack)
				if (ev.type==EV_SW) {
					printf("switch: %i\n", ev.code);
					if (ev.code==CODE_JACK) {
						printf("jack: %u\n", val);
						SetJack(val);
					}
				}

				// Only process key events (PRESSED, RELEASED, or REPEAT)
				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;

				printf("code: %i (%u)\n", ev.code, val); fflush(stdout);

				// Process hardware button events
				switch (ev.code) {
					case CODE_MENU:
						menu_pressed = val;
					break;
					case CODE_PLUS:
						// Volume up button (or brightness up if MENU held)
						up_pressed = up_just_pressed = val;
						if (val) up_repeat_at = now + 300; // 300ms initial delay
					break;
					case CODE_MINUS:
						// Volume down button (or brightness down if MENU held)
						down_pressed = down_just_pressed = val;
						if (val) down_repeat_at = now + 300; // 300ms initial delay
					break;
					default:
					break;
				}
			}
		}

		// Reset button state after ignoring stale input
		if (ignore) {
			menu_pressed = 0;
			up_pressed = up_just_pressed = 0;
			down_pressed = down_just_pressed = 0;
			up_repeat_at = 0;
			down_repeat_at = 0;
		}

		// Handle PLUS button (initial press or repeat after delay)
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				printf("brightness up\n"); fflush(stdout);
				// MENU+PLUS: Brightness up
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
				printf("volume up\n"); fflush(stdout);
				// PLUS alone: Volume up
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}

			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100; // 100ms repeat interval
		}

		// Handle MINUS button (initial press or repeat after delay)
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (menu_pressed) {
				printf("brightness down\n"); fflush(stdout);
				// MENU+MINUS: Brightness down
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
				printf("volume down\n"); fflush(stdout);
				// MINUS alone: Volume down
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}

			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100; // 100ms repeat interval
		}

		then = now;
		ignore = 0;

		usleep(16666); // 60Hz polling rate
	}
}
