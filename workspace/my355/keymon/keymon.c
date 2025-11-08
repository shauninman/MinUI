/**
 * keymon.c - MY355 hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the MY355 handheld device. Features:
 * - Volume and brightness control through button combinations
 * - Headphone jack detection
 * - HDMI output detection
 *
 * Button combinations:
 * - MENU+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Also monitors headphone jack (GPIO150) and HDMI port in a separate thread
 * and updates audio/video routing accordingly.
 *
 * Runs continuously at 60Hz polling input device and hardware status.
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

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

// #include "defines.h"

#define VOLUME_MIN 		0
#define VOLUME_MAX 		20
#define BRIGHTNESS_MIN 	0
#define BRIGHTNESS_MAX 	10

// Hardware button codes (different from SDL codes)
#define CODE_MENU		1
#define CODE_PLUS		115
#define CODE_MINUS		114

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

static int	input_fd = 0;
static struct input_event ev;

static pthread_t ports_pt;
#define JACK_STATE_PATH "/sys/class/gpio/gpio150/value"
#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"

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
 * Reads a text file into a buffer.
 *
 * @param path Path to file
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
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

/**
 * Checks if two strings are exactly equal.
 *
 * @param str1 First string
 * @param str2 Second string
 * @return 1 if strings match, 0 otherwise
 */
int exactMatch(char* str1, char* str2) {
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}

/**
 * Checks if headphones are plugged in.
 *
 * Reads GPIO150 value (inverted logic: 0 = headphones present).
 *
 * @return 1 if headphones connected, 0 otherwise
 */
static int JACK_enabled(void) {
	return !getInt(JACK_STATE_PATH);
}

/**
 * Checks if HDMI is connected.
 *
 * Reads DRM connector status file looking for "connected\n" string.
 *
 * @return 1 if HDMI connected, 0 otherwise
 */
static int HDMI_enabled(void) {
	char value[64];
	getFile(HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
}

/**
 * Background thread that monitors headphone jack and HDMI state.
 *
 * Polls the hardware interfaces every second and updates audio/video
 * routing when states change.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchPorts(void *arg) {
	int has_jack,had_jack;
	has_jack = had_jack = JACK_enabled();
	SetJack(has_jack);

	int has_hdmi,had_hdmi;
	has_hdmi = had_hdmi = HDMI_enabled();
	SetHDMI(has_hdmi);

	while(1) {
		sleep(1);

		// Check for headphone jack state changes
		has_jack = JACK_enabled();
		if (had_jack!=has_jack) {
			had_jack = has_jack;
			SetJack(has_jack);
		}

		// Check for HDMI state changes
		has_hdmi = HDMI_enabled();
		if (had_hdmi!=has_hdmi) {
			had_hdmi = has_hdmi;
			SetHDMI(has_hdmi);
		}
	}

	return 0;
}

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls input device for button events and handles:
 * - MENU+PLUS: Increase brightness
 * - MENU+MINUS: Decrease brightness
 * - PLUS alone: Increase volume
 * - MINUS alone: Decrease volume
 *
 * Also starts background thread to monitor headphone jack and HDMI state.
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

	// Start port monitoring thread
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);

	input_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

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
