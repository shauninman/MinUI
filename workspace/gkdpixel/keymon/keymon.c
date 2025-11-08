/**
 * keymon.c - GKD Pixel hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the GKD Pixel handheld device. Provides
 * volume and brightness control through hardware button combinations:
 * - MENU+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Runs continuously at 60Hz polling the input device for button events.
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

#include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

static int	input_fd = 0;
static struct input_event ev;

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls input device for button events and handles:
 * - MENU+PLUS: Increase brightness
 * - MENU+MINUS: Decrease brightness
 * - PLUS alone: Increase volume
 * - MINUS alone: Decrease volume
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval)
 * and ignores stale input after system sleep (> 1 second gap).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return Never returns (runs infinite loop)
 */
int main (int argc, char *argv[]) {
	InitSettings();

	// Open gamepad input device for reading button events
	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);

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

	// TODO: tmp
	// uint32_t l1_pressed = 0;
	// uint32_t r1_pressed = 0;
	// uint32_t select_pressed = 0;
	// uint32_t start_pressed = 0;

	gettimeofday(&tod, NULL);
	then = tod.tv_sec * 1000 + tod.tv_usec / 1000; // SDL_GetTicks() equivalent
	ignore = 0;

	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;

		// Ignore stale input after system sleep to avoid spurious events
		if (now-then>1000) ignore = 1;
		
		// Read and process all available input events
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			if (ignore) continue;
			val = ev.value;

			// Only process key events (PRESSED, RELEASED, or REPEAT)
			if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;

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
				// TODO: tmp
				// case CODE_L1: l1_pressed = val; break;
				// case CODE_R1: r1_pressed = val; break;
				// case CODE_START: start_pressed = val; break;
				// case CODE_SELECT: select_pressed = val; break;
				default:
				break;
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

		// TODO: tmp
		// if (l1_pressed && r1_pressed && start_pressed && select_pressed) {
		// 	system("sync && killall -9 minarch.elf"); // TODO: tmp
		// 	system("sync && killall -9 launch.sh"); // TODO: tmp
		// }

		// Handle PLUS button (initial press or repeat after delay)
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (menu_pressed) {
				// MENU+PLUS: Brightness up
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else {
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
				// MENU+MINUS: Brightness down
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else {
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
