/**
 * keymon.c - TG5040 hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the TG5040 handheld device. Features:
 * - Volume and brightness control through button combinations
 * - Hardware mute switch monitoring
 * - Headphone jack detection via EV_SW events
 *
 * Button combinations:
 * - MENU+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Also monitors hardware mute switch state (GPIO243) and headphone jack
 * switch events in a separate thread.
 *
 * Supports SIGTERM signal for graceful shutdown.
 *
 * Runs continuously at 60Hz polling multiple input devices.
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
#include <pthread.h>
#include <signal.h>

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

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

#define MUTE_STATE_PATH "/sys/class/gpio/gpio243/value"

#define INPUT_COUNT 4
static int inputs[INPUT_COUNT] = {};
static struct input_event ev;

static volatile int quit = 0;

/**
 * SIGTERM signal handler for graceful shutdown.
 *
 * @param sig Signal number
 */
static void on_term(int sig) { quit = 1; }

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

static pthread_t mute_pt;

/**
 * Background thread that monitors hardware mute switch.
 *
 * Polls GPIO243 mute switch every 200ms and updates mute state when
 * it changes (mute switch toggled).
 *
 * @param arg Thread argument (unused)
 * @return NULL on thread termination
 */
static void* watchMute(void *arg) {
	int is_muted,was_muted;

	// Initialize mute state
	is_muted = was_muted = getInt(MUTE_STATE_PATH);
	SetMute(is_muted);

	while(!quit) {
		usleep(200000); // Poll 5 times per second

		// Check for mute switch state changes
		is_muted = getInt(MUTE_STATE_PATH);
		if (was_muted!=is_muted) {
			was_muted = is_muted;
			SetMute(is_muted);
		}
	}

	return NULL;
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
 * - EV_SW CODE_MUTE: Hardware mute switch events
 *
 * Also starts background thread to monitor GPIO mute switch state.
 *
 * Implements repeat functionality (initial 300ms delay, then 100ms interval)
 * and ignores stale input after system sleep (> 1 second gap).
 *
 * Handles SIGTERM for graceful shutdown.
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return 0 on clean exit
 *
 * @note Uses event0-3 input devices
 * @note Supports multiple MENU button codes (314, 315, 316)
 */
int main (int argc, char *argv[]) {
	// Install SIGTERM handler for graceful shutdown
	struct sigaction sa = {0};
	sa.sa_handler = on_term;
	sigaction(SIGTERM, &sa, NULL);

	InitSettings();

	// Start mute switch monitoring thread
	pthread_create(&mute_pt, NULL, &watchMute, NULL);

	// Open all input devices
	char path[32];
	for (int i=0; i<INPUT_COUNT; i++) {
		sprintf(path, "/dev/input/event%i", i);
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

	while (!quit) {
		gettimeofday(&tod, NULL);
		now = tod.tv_sec * 1000 + tod.tv_usec / 1000;

		// Ignore stale input after system sleep to avoid spurious events
		if (now-then>1000) ignore = 1;

		// Poll all input devices for events
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;

				// Process switch events (headphone jack, mute)
				if (ev.type==EV_SW) {
					printf("switch: %i\n", ev.code);
					if (ev.code==CODE_JACK) {
					printf("jack: %u\n", val);
					SetJack(val);
				}
					else if (ev.code==CODE_MUTE) {
						printf("mute: %u\n", val);
						SetMute(val);
					}
				}

				// Only process key events (PRESSED, RELEASED, or REPEAT)
				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;

				printf("code: %i (%u)\n", ev.code, val); fflush(stdout);

				// Process hardware button events
				switch (ev.code) {
					case CODE_MENU0:
					case CODE_MENU1:
					case CODE_MENU2:
						// Multiple MENU button codes supported
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

	// Clean shutdown
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}

	pthread_cancel(mute_pt);
	pthread_join(mute_pt, NULL);
}
