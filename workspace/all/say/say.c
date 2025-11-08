/**
 * say.c - Simple text message display utility
 *
 * Displays a full-screen message dialog with a single "OKAY" button.
 * Used by system scripts to show notifications, warnings, or status
 * messages to the user. The message text is passed as a command-line
 * argument.
 *
 * Example: say "SD card safely removed"
 */

#include <msettings.h>
#include <stdio.h>

#include "api.h"
#include "defines.h"
#include "utils.h"

/**
 * Displays a full-screen message dialog and waits for user confirmation.
 *
 * The message is centered on screen with a single "OKAY" button at the
 * bottom. The application exits when the user presses A or B button.
 *
 * @param argc Argument count (must be at least 2)
 * @param argv Argument vector - argv[1] contains the message text
 * @return EXIT_SUCCESS on normal exit
 */
int main(int argc, const char* argv[]) {
	char msg[1024];
	sprintf(msg, "%s", argv[1]);

	// Set low CPU speed for power efficiency (no heavy processing needed)
	PWR_setCPUSpeed(CPU_SPEED_MENU);

	// Initialize platform subsystems
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

	int quit = 0;

	int dirty = 1; // Needs redraw
	while (!quit) {
		PAD_poll();
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) {
			quit = 1;
		}

		// PWR_update(&dirty, NULL, NULL,NULL);

		if (dirty) {
			GFX_clear(screen);

			// Display message centered, leaving room for button at bottom
			GFX_blitMessage(
			    font.large, msg, screen,
			    &(SDL_Rect){0, 0, screen->w, screen->h - SCALE1(PADDING + PILL_SIZE + PADDING)});
			GFX_blitButtonGroup((char*[]){"A", "OKAY", NULL}, 1, screen, 1);

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();
	}

	// Clean up subsystems in reverse order of initialization
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
