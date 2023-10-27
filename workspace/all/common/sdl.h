#ifndef SDL_HEADERS_H
#define SDL_HEADERS_H

#if defined(USE_SDL2)
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#define WHICH_SDL "SDL2"
#else
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#define WHICH_SDL "SDL"
#endif

#endif
