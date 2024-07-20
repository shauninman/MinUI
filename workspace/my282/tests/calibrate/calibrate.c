#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <msettings.h>
#include <mstick.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#define STICK_SIZE 320

int main(int argc , char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);
	
	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();

	SDL_Event event;
	int quit = 0;
	while(!quit) {
		uint32_t frame_start = SDL_GetTicks();
		
		PAD_poll();
		if (PAD_justPressed(BTN_A)) quit = 1;
		if (PAD_justPressed(BTN_B)) GFX_clear(screen);
		
		float rx = (float)pad.laxis.x / 32768;
		float ry = (float)pad.laxis.y / 32768;
		
		SDL_FillRect(screen, &(SDL_Rect){(FIXED_WIDTH/2)+(STICK_SIZE*rx)/2-2,(FIXED_HEIGHT/2)+(STICK_SIZE*ry)/2-2,4,4}, 0xffff);
		SDL_FillRect(screen, &(SDL_Rect){0,0,FIXED_WIDTH,80}, 0);
		
		char text[256];
		sprintf(text, "%0.02f, %0.02f", rx,ry);
		SDL_Surface* txt = TTF_RenderUTF8_Blended(font.large, text, COLOR_WHITE);
		SDL_BlitSurface(txt, NULL, screen, &(SDL_Rect){8,8});
		SDL_FreeSurface(txt);
		
		GFX_flip(screen);
	}

	PAD_quit();
	GFX_quit();
	return 0;
}
