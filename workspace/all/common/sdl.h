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
inline int SDLX_SetAlpha(SDL_Surface *surface, Uint32 flags, Uint8 value) {
    int result = 0;

    if (flags & SDL_SRCALPHA) {
        if (!surface->format->Amask) result = SDL_SetSurfaceAlphaMod(surface, value);
        surface->flags |= SDL_SRCALPHA;
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    } else {
        if (!surface->format->Amask) result = SDL_SetSurfaceAlphaMod(surface, 255);
        surface->flags &= ~SDL_SRCALPHA;
        SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);
    }

    return result;
}

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
