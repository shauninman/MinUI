/**
 * keymon.c - M17 hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the M17 handheld device. Provides volume
 * and brightness control through button combinations:
 * - START+L1/R1: Adjust brightness
 * - SELECT+L1/R1: Adjust volume
 *
 * Also monitors headphone jack state in a separate thread and updates
 * audio routing accordingly.
 *
 * Runs continuously at 60Hz polling multiple input devices.
 */

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

// Input event values
#define RELEASED	0
#define PRESSED		1

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT];
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/devices/virtual/switch/h2w/state" // 0 or 2

/**
 * Reads an integer value from a sysfs file.
 *
 * Used for reading hardware state from kernel interfaces.
 *
 * @param path Path to sysfs file
 * @return Integer value read from file, or 0 if file cannot be opened
 */
int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

/**
 * Background thread that monitors headphone jack state.
 *
 * Polls the headphone jack sysfs interface every second and updates
 * audio routing when the state changes (headphones plugged/unplugged).
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchPorts(void *arg) {
	int has_headphones,had_headphones;

	// Initialize headphone state
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	SetJack(has_headphones);

	while(1) {
		sleep(1);

		// Check for headphone state changes
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones!=has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}
	}

	return 0;
}

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls multiple input devices for button events and handles:
 * - START+R1: Increase brightness
 * - START+L1: Decrease brightness
 * - SELECT+R1: Increase volume
 * - SELECT+L1: Decrease volume
 * - PLUS/MINUS: Re-applies volume setting (hardware workaround)
 *
 * Also starts background thread to monitor headphone jack state.
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return Never returns (runs infinite loop)
 */
int main (int argc, char *argv[]) {
	printf("keymon\n"); fflush(stdout);
	InitSettings();

	// Start headphone jack monitoring thread
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);

	// Open all input devices (event0-event3)
	char path[256];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}

	// TODO: make sure manually handling repeat is necessary on this device
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

		int input;
		static struct input_event event;

		// Poll all input devices for button events
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];

			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
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
					case CODE_PLUS:
					case CODE_MINUS:
						// Hardware volume buttons: reset scaled flag and re-apply volume
						// This is a platform-specific workaround for hardware quirks
						system("echo 0 > /sys/devices/platform/0gpio-keys/scaled");
						SetVolume(GetVolume());
					break;
					default:
					break;
				}
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
