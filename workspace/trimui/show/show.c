// trimui
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

int main(int argc , char* argv[]) {
	if (argc<2) {
		puts("Usage: show.elf image [1]");
		return 0;
	}
	
	char path[256];
	strncpy(path,argv[1],256);
	int await_input = argc>2;
	
	if (SDL_Init(SDL_INIT_VIDEO)==-1) {
		puts("could not init SDL");
		puts(SDL_GetError());
	}
	
	SDL_Surface* screen = SDL_SetVideoMode(320, 240, 16, SDL_SWSURFACE);
	SDL_ShowCursor(0);
	
	SDL_Surface *image = IMG_Load(path);
	if (image==NULL) puts(IMG_GetError());
	SDL_BlitSurface(image, NULL, screen, NULL);
	SDL_Flip(screen);
	
	if (await_input) {
		int quit = 0;
		SDL_Event event;
		while (!quit) {
			while (SDL_PollEvent(&event)) {
				switch(event.type) {
					case SDL_KEYDOWN:
						quit = 1;
					break;
				}
			}
		}
	}
	
	SDL_FreeSurface(image);
	IMG_Quit();
	SDL_Quit();
	return 0;
}