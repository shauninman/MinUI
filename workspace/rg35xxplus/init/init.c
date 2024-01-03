#include <SDL/SDL.h>
void main(void) {
	SDL_Init(SDL_INIT_VIDEO);
	SDL_SetVideoMode(0,0,0,0);
	SDL_Quit();
}
