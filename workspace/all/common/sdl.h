#ifndef SDL_HEADERS_H
#define SDL_HEADERS_H

#if defined(USE_SDL2)

///////////////////////////////

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

///////////////////////////////

#define WHICH_SDL "SDL2"
#define SDLKey SDL_Keycode

///////////////////////////////

#define SDL_SRCALPHA 0x00010000
#define SDLX_SetAlpha(surface,flags_,value) do { \
    if (flags_ & SDL_SRCALPHA) { \
        if (!surface->format->Amask) SDL_SetSurfaceAlphaMod(surface, value); \
        surface->flags |= SDL_SRCALPHA; \
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND); \
    } else { \
        if (!surface->format->Amask) SDL_SetSurfaceAlphaMod(surface, 255); \
        surface->flags &= ~SDL_SRCALPHA; \
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE); \
    } \
} while(0);

///////////////////////////////

#else

///////////////////////////////

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

///////////////////////////////

#define WHICH_SDL "SDL"
#define SDLX_SetAlpha SDL_SetAlpha

///////////////////////////////

#endif

#endif
