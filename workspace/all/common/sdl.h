/**
 * sdl.h - SDL version compatibility layer
 *
 * Provides a unified interface for both SDL 1.2 and SDL 2.0.
 * Platforms define USE_SDL2 in their makefile if they use SDL 2.0.
 *
 * This header handles API differences between SDL versions, particularly:
 * - Header paths (<SDL/SDL.h> vs <SDL2/SDL.h>)
 * - Alpha blending API changes
 * - Type name differences
 */

#ifndef SDL_HEADERS_H
#define SDL_HEADERS_H

#if defined(USE_SDL2)

///////////////////////////////
// SDL 2.0 configuration
///////////////////////////////

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

/**
 * SDL version identifier for runtime checks.
 */
#define WHICH_SDL "SDL2"

/**
 * SDL 1.2 compatibility: SDLKey was renamed to SDL_Keycode in SDL 2.0.
 */
#define SDLKey SDL_Keycode

/**
 * SDL 1.2 compatibility: SDL_SRCALPHA flag for alpha blending.
 *
 * In SDL 2.0, this flag doesn't exist, but we define it for compatibility.
 */
#define SDL_SRCALPHA 0x00010000

/**
 * SDL 1.2 compatibility wrapper for SDL_SetAlpha().
 *
 * In SDL 1.2, SDL_SetAlpha() controlled per-surface alpha blending.
 * In SDL 2.0, this is split into SDL_SetSurfaceAlphaMod() and
 * SDL_SetSurfaceBlendMode(). This macro provides a compatibility layer.
 *
 * @param surface SDL surface to modify
 * @param flags_ SDL_SRCALPHA to enable alpha blending, 0 to disable
 * @param value Alpha value (0-255)
 */
#define SDLX_SetAlpha(surface, flags_, value)                                                      \
	do {                                                                                           \
		if (flags_ & SDL_SRCALPHA) {                                                               \
			if (!surface->format->Amask)                                                           \
				SDL_SetSurfaceAlphaMod(surface, value);                                            \
			surface->flags |= SDL_SRCALPHA;                                                        \
			SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);                                 \
		} else {                                                                                   \
			if (!surface->format->Amask)                                                           \
				SDL_SetSurfaceAlphaMod(surface, 255);                                              \
			surface->flags &= ~SDL_SRCALPHA;                                                       \
			SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_NONE);                                  \
		}                                                                                          \
	} while (0);

///////////////////////////////
// SDL 1.2 configuration
///////////////////////////////

#else

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

/**
 * SDL version identifier for runtime checks.
 */
#define WHICH_SDL "SDL"

/**
 * SDL 1.2 uses SDL_SetAlpha() directly.
 */
#define SDLX_SetAlpha SDL_SetAlpha

#endif

#endif
