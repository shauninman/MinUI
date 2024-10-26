// tg3040
#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

SDL_Window* window;
SDL_Surface* screen;

int main(int argc , char* argv[]) {
	if (argc<2) {
		puts("Usage: show.elf image.png delay");
		return 0;
	}
	
	char path[256];
	strncpy(path,argv[1],256);
	if (access(path, F_OK)!=0) return 0; // nothing to show :(
	
	int delay = argc>2 ? atoi(argv[2]) : 2;
	
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	int w = 1024;
	int h = 768;
	window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	puts("");
	
	screen = SDL_GetWindowSurface(window); 
	SDL_FillRect(screen, NULL, 0);
	 
	SDL_Surface* img = IMG_Load(path); 
	SDL_BlitSurface(img, NULL, screen, &(SDL_Rect){(screen->w-img->w)/2,(screen->h-img->h)/2});
	
	SDL_UpdateWindowSurface(window);
	sleep(delay);
	
	SDL_FreeSurface(img);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
