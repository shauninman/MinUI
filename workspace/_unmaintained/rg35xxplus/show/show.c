// rg35xxplus
#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)

#define RGBA_MASK_565	0xF800, 0x07E0, 0x001F, 0x0000

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
	
	int w = 0;
	int h = 0;
	int p = 0;
	SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	
	int rotate = 0;
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	if (mode.h>mode.w) rotate = 3;
	w = mode.w;
	h = mode.h;
	p = mode.w * FIXED_BPP;
	
	SDL_Renderer* renderer = SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	SDL_Texture* texture = SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	SDL_Surface* screen = SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	
	SDL_LockTexture(texture,NULL,&screen->pixels,&screen->pitch);
	SDL_FillRect(screen, NULL, 0);
	SDL_Surface* img = IMG_Load(path);
	SDL_BlitSurface(img, NULL, screen, &(SDL_Rect){(screen->w-img->w)/2,(screen->h-img->h)/2});
	SDL_FreeSurface(img);
	SDL_UnlockTexture(texture);

	if (rotate) SDL_RenderCopyEx(renderer,texture,NULL,&(SDL_Rect){0,w,w,h},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
	else SDL_RenderCopy(renderer, texture, NULL,NULL);
	SDL_RenderPresent(renderer);
	
	sleep(delay);
	
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
