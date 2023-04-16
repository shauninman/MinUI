#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

// TODO: revisit

int main(int argc , char* argv[]) {
	if (argc<2) {
		puts("Usage: show image.png");
		return 0;
	}
	
	char path[256];
	if (strchr(argv[1], '/')==NULL) sprintf(path, "/mnt/SDCARD/.system/res/%s", argv[1]);
	else strncpy(path,argv[1],256);
	
	if (access(path, F_OK)!=0) return 0; // nothing to show :(
	
	putenv("SDL_VIDEO_FBCON_ROTATION=CCW");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	SDL_Surface *screen = SDL_SetVideoMode(320,240,16,SDL_SWSURFACE);
	SDL_Surface* img = IMG_Load(path); // 24-bit opaque png
	SDL_BlitSurface(img, NULL, screen, NULL);
	SDL_FreeSurface(img);

	SDL_Flip(screen);

	SDL_Quit();
	return EXIT_SUCCESS;
}
