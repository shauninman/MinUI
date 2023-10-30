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

// TODO: this might not even  be necessary since PLAT_ keeps its own copy of the screen surface
typedef struct SDLX_Screen {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Surface* bitmap;
} SDLX_Screen;

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

// TODO: these can be removed
// inline const char* copyname(const char *name, char *buffer, int len) {
//     if (name) {
//         if (buffer) {
//             strncpy(buffer, name, len);
//             return buffer;
//         }
//         return name;
//     }
//     return NULL;
// }
// 
// inline const char* SDLX_AudioDriverName(char *buffer, int len) {
//     return copyname(SDL_GetCurrentAudioDriver(), buffer, len);
// }
//
// inline const char* SDLX_VideoDriverName(char *buffer, int len) {
// 	return copyname(SDL_GetCurrentVideoDriver(), buffer, len);
// }
//
// inline const SDL_version* SDLX_Linked_Version(void) {
// 	static const SDL_version version;
// 	SDL_GetVersion((SDL_version*)&version);
// 	return &version;
// }

///////////////////////////////

#else

///////////////////////////////

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

///////////////////////////////

#define WHICH_SDL "SDL"
#define SDLX_SetAlpha SDL_SetAlpha
// #define SDLX_AudioDriverName SDL_AudioDriverName
// #define SDLX_VideoDriverName SDL_VideoDriverName
// #define SDLX_Linked_Version SDL_Linked_Version

///////////////////////////////

// TODO: this might not even  be necessary since PLAT_ keeps its own copy of the screen surface
typedef struct SDLX_Screen {
	SDL_Surface* bitmap;
} SDLX_Screen;

///////////////////////////////

#endif

#endif
