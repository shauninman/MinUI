#include <msettings.h>
#include <stdio.h>

#include "api.h"
#include "defines.h"
#include "utils.h"

int main(int argc, char* argv[]) {
	char msg[1024];
	sprintf(msg, "%s", argv[1]);

	PWR_setCPUSpeed(CPU_SPEED_MENU);

	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

	SDL_Event event;
	int quit = 0;

	int dirty = 1;
	while (!quit) {
		uint32_t frame_start = SDL_GetTicks();

		PAD_poll();
		if (PAD_justPressed(BTN_A) || PAD_justPressed(BTN_B)) {
			quit = 1;
		}

		// PWR_update(&dirty, NULL, NULL,NULL);

		if (dirty) {
			GFX_clear(screen);

			GFX_blitMessage(
			    font.large, msg, screen,
			    &(SDL_Rect){0, 0, screen->w, screen->h - SCALE1(PADDING + PILL_SIZE + PADDING)});
			GFX_blitButtonGroup((char*[]){"A", "OKAY", NULL}, 1, screen, 1);

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();
	}

	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
