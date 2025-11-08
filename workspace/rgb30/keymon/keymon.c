/**
 * keymon.c - RGB30 hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles
 * system-level shortcuts on the RGB30 handheld device. Features:
 * - Volume and brightness control through button combinations
 * - Headphone jack detection
 * - HDMI output detection
 *
 * Button combinations:
 * - L3/R3 (MENU)+PLUS/MINUS: Adjust brightness
 * - PLUS/MINUS alone: Adjust volume
 *
 * Also monitors headphone jack and HDMI state in a separate thread and
 * updates audio/video routing accordingly.
 *
 * Uses L3 or R3 analog stick buttons as menu modifier (CODE_MENU 317 or CODE_MENU_ALT 318).
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

#include <msettings.h>

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

// L3 or R3 analog stick button codes
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
 * Background thread that monitors headphone jack and HDMI state.
 *
 * Polls the hardware interfaces every second and updates audio/video
 * routing when states change.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* watchPorts(void *arg) {
	int has_headphones,had_headphones;
	int has_hdmi,had_hdmi;

	// Initialize headphone and HDMI state
	has_headphones = had_headphones = getInt(JACK_STATE_PATH);
	has_hdmi = had_hdmi = getInt(HDMI_STATE_PATH);
	SetJack(has_headphones);
	SetHDMI(has_hdmi);

	while(1) {
		sleep(1);

		// Check for headphone jack state changes
		has_headphones = getInt(JACK_STATE_PATH);
		if (had_headphones!=has_headphones) {
			had_headphones = has_headphones;
			SetJack(has_headphones);
		}

		// Check for HDMI state changes
		has_hdmi = getInt(HDMI_STATE_PATH);
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
 * Continuously polls multiple input devices for button events and handles:
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
 *
 * @note Uses event0-4 and js0 (joystick) input devices
 * @note MENU modifier is L3 or R3 analog stick button press
 */
int main (int argc, char *argv[]) {
	printf("keymon\n"); fflush(stdout);
	InitSettings();

	// Start port monitoring thread
	pthread_create(&ports_pt, NULL, &watchPorts, NULL);

	// Open multiple input devices including joystick
	char path[32];
	for (int i=0; i<INPUT_COUNT-1; i++) {
		sprintf(path, "/dev/input/event%i", i);
		inputs[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	}
	inputs[5] = open("/dev/input/js0", O_RDONLY | O_NONBLOCK | O_CLOEXEC);

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

		// Poll all input devices for button events
		for (int i=0; i<INPUT_COUNT; i++) {
			input = inputs[i];
			while(read(input, &ev, sizeof(ev))==sizeof(ev)) {
				if (ignore) continue;
				val = ev.value;

				// Only process key events (PRESSED, RELEASED, or REPEAT)
				if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;

				// Process hardware button events
				switch (ev.code) {
					case CODE_MENU:
					case CODE_MENU_ALT:
						// L3 or R3 analog stick button as menu modifier
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
