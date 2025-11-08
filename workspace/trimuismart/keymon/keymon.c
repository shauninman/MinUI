/**
 * keymon.c - Trimui Smart hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the Trimui Smart handheld device. Provides
 * volume and brightness control through button combinations:
 * - START+R1: Adjust brightness up
 * - START+L1: Adjust brightness down
 * - SELECT+R1: Adjust volume up
 * - SELECT+L1: Adjust volume down
 *
 * This keymon implementation is simpler than others, with no additional
 * hardware monitoring (no jack detection, no HDMI, no power button).
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

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1

#define INPUT_COUNT 2
static int	input_fd = 0;
static struct input_event ev;

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls input device for button events and handles:
 * - START+R1: Increase brightness
 * - START+L1: Decrease brightness
 * - SELECT+R1: Increase volume
 * - SELECT+L1: Decrease volume
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval).
 * No stale input detection (simpler implementation for this device).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return Never returns (runs infinite loop)
 *
 * @note Uses event0 input device
 */
int main (int argc, char *argv[]) {
	InitSettings();

	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

	uint32_t val;
	uint32_t start_pressed = 0;
	uint32_t select_pressed = 0;

	// Track R1 button state for repeat handling
	uint32_t up_pressed = 0;
	uint32_t up_just_pressed = 0;
	uint32_t up_repeat_at = 0;

	// Track L1 button state for repeat handling
	uint32_t down_pressed = 0;
	uint32_t down_just_pressed = 0;
	uint32_t down_repeat_at = 0;

	uint32_t now;
	struct timeval tod;

	while (1) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;

		// Read and process all available input events
		while(read(input_fd, &ev, sizeof(ev))==sizeof(ev)) {
			// Only process key events
			if (ev.type != EV_KEY ) continue;
			val = ev.value;

			// Process hardware button events
			switch (ev.code) {
				case CODE_START:
					start_pressed = val;
				break;
				case CODE_SELECT:
					select_pressed = val;
				break;
				case CODE_R1:
					// R1 button (brightness/volume up when combined)
					up_pressed = up_just_pressed = val;
					if (val) up_repeat_at = now + 300; // 300ms initial delay
				break;
				case CODE_L1:
					// L1 button (brightness/volume down when combined)
					down_pressed = down_just_pressed = val;
					if (val) down_repeat_at = now + 300; // 300ms initial delay
				break;
				default:
				break;
			}
		}

		// Handle R1 button (initial press or repeat after delay)
		if (up_just_pressed || (up_pressed && now>=up_repeat_at)) {
			if (start_pressed) {
				// START+R1: Brightness up
				val = GetBrightness();
				if (val<BRIGHTNESS_MAX) SetBrightness(++val);
			}
			else if (select_pressed) {
				// SELECT+R1: Volume up
				val = GetVolume();
				if (val<VOLUME_MAX) SetVolume(++val);
			}

			if (up_just_pressed) up_just_pressed = 0;
			else up_repeat_at += 100; // 100ms repeat interval
		}

		// Handle L1 button (initial press or repeat after delay)
		if (down_just_pressed || (down_pressed && now>=down_repeat_at)) {
			if (start_pressed) {
				// START+L1: Brightness down
				val = GetBrightness();
				if (val>BRIGHTNESS_MIN) SetBrightness(--val);
			}
			else if (select_pressed) {
				// SELECT+L1: Volume down
				val = GetVolume();
				if (val>VOLUME_MIN) SetVolume(--val);
			}

			if (down_just_pressed) down_just_pressed = 0;
			else down_repeat_at += 100; // 100ms repeat interval
		}

		usleep(16666); // 60Hz polling rate
	}
}
