/**
 * minput.c - Interactive button mapping visualization
 *
 * Visual input tester that displays all available buttons on the current
 * device and highlights them in real-time as they are pressed. Useful for
 * verifying button mappings and troubleshooting input issues.
 *
 * Features:
 * - Automatically detects available buttons (L2/R2, L3/R3, volume, power, menu)
 * - Visual layout mimics physical controller layout
 * - Real-time feedback - buttons highlight when pressed
 * - Exit with SELECT + START combination
 *
 * The layout adapts based on platform capabilities defined in platform.h:
 * - Shoulder buttons (L1, L2, R1, R2)
 * - Face buttons (A, B, X, Y)
 * - D-pad (Up, Down, Left, Right)
 * - Meta buttons (Start, Select)
 * - Analog stick buttons (L3, R3)
 * - System buttons (Volume +/-, Menu, Power)
 */

#include <msettings.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "sdl.h"
#include "utils.h"

/**
 * Calculates the display width needed for a button label.
 *
 * Single-character and two-character labels fit within a square button.
 * Longer labels (like "VOL. +", "MENU") need wider pill-shaped buttons.
 *
 * @param label Button label text
 * @return Width in pixels (pre-scaled)
 */
static int getButtonWidth(char* label) {
	int w = 0;

	if (strlen(label) <= 2)
		w = SCALE1(BUTTON_SIZE);
	else {
		SDL_Surface* text = TTF_RenderUTF8_Blended(font.tiny, label, COLOR_BUTTON_TEXT);
		w = SCALE1(BUTTON_SIZE) + text->w;
		SDL_FreeSurface(text);
	}
	return w;
}

/**
 * Renders a button visual with label, showing pressed/unpressed state.
 *
 * Uses different rendering approaches based on label length:
 * - 1-2 chars: Square button with centered text
 * - 3+ chars: Pill-shaped button with wider layout
 *
 * Pressed state changes the button appearance from a hole to a raised button.
 *
 * @param label Button label text
 * @param dst Destination surface (screen)
 * @param pressed 1 if button is currently pressed, 0 otherwise
 * @param x X position (pre-scaled)
 * @param y Y position (pre-scaled)
 * @param w Width override (0 to auto-calculate)
 */
static void blitButton(char* label, SDL_Surface* dst, int pressed, int x, int y, int w) {
	SDL_Rect point = {x, y};
	SDL_Surface* text;

	int len = strlen(label);
	if (len <= 2) {
		// Short labels: use square button with larger font
		text =
		    TTF_RenderUTF8_Blended(len == 2 ? font.small : font.medium, label, COLOR_BUTTON_TEXT);
		GFX_blitAsset(pressed ? ASSET_BUTTON : ASSET_HOLE, NULL, dst, &point);
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){point.x + (SCALE1(BUTTON_SIZE) - text->w) / 2,
		                            point.y + (SCALE1(BUTTON_SIZE) - text->h) / 2});
	} else {
		// Long labels: use pill-shaped button with smaller font
		text = TTF_RenderUTF8_Blended(font.tiny, label, COLOR_BUTTON_TEXT);
		w = w ? w : SCALE1(BUTTON_SIZE) / 2 + text->w;
		GFX_blitPill(pressed ? ASSET_BUTTON : ASSET_HOLE, dst,
		             &(SDL_Rect){point.x, point.y, w, SCALE1(BUTTON_SIZE)});
		SDL_BlitSurface(text, NULL, dst,
		                &(SDL_Rect){point.x + (w - text->w) / 2,
		                            point.y + (SCALE1(BUTTON_SIZE) - text->h) / 2, text->w,
		                            text->h});
	}

	SDL_FreeSurface(text);
}

/**
 * Main entry point for the input tester application.
 *
 * Displays an interactive button map showing all available buttons on the
 * current device. Buttons highlight as they are pressed. The layout adapts
 * based on which buttons are defined in platform.h.
 *
 * @return EXIT_SUCCESS on normal exit
 */
int main(int argc, char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);

	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

	// Detect which buttons are available on this platform
	// Each button can be mapped via scancode, keycode, joystick button, or axis
	int has_L2 =
	    (BUTTON_L2 != BUTTON_NA || CODE_L2 != CODE_NA || JOY_L2 != JOY_NA || AXIS_L2 != AXIS_NA);
	int has_R2 =
	    (BUTTON_R2 != BUTTON_NA || CODE_R2 != CODE_NA || JOY_R2 != JOY_NA || AXIS_R2 != AXIS_NA);
	int has_L3 = (BUTTON_L3 != BUTTON_NA || CODE_L3 != CODE_NA || JOY_L3 != JOY_NA);
	int has_R3 = (BUTTON_R3 != BUTTON_NA || CODE_R3 != CODE_NA || JOY_R3 != JOY_NA);

	int has_volume = (BUTTON_PLUS != BUTTON_NA || CODE_PLUS != CODE_NA || JOY_PLUS != JOY_NA);
	int has_power = HAS_POWER_BUTTON;
	int has_menu = HAS_MENU_BUTTON;
	int has_both = (has_power && has_menu);

	// Adjust vertical offset if L3/R3 not present (reclaim space)
	int oy = SCALE1(PADDING);
	if (!has_L3 && !has_R3)
		oy += SCALE1(PILL_SIZE);

	int quit = 0;
	int dirty = 1;

	// Main event loop - redraw on any button press/release
	while (!quit) {
		PAD_poll();

		// Mark dirty if any input detected
		if (PAD_anyPressed() || PAD_anyJustReleased())
			dirty = 1;

		// Exit on SELECT + START combination
		if (PAD_isPressed(BTN_SELECT) && PAD_isPressed(BTN_START))
			quit = 1;

		// Redraw screen if anything changed
		if (dirty) {
			GFX_clear(screen);

			///////////////////////////////
			// Left shoulder buttons (L1, L2)
			///////////////////////////////
			{
				int x = SCALE1(BUTTON_MARGIN + PADDING);
				int y = oy;
				int w = 0;
				int ox = 0;

				w = getButtonWidth("L1") + SCALE1(BUTTON_MARGIN) * 2;
				ox = w;

				if (has_L2)
					w += getButtonWidth("L2") + SCALE1(BUTTON_MARGIN);
				// Offset if L2 not present to maintain visual balance
				if (!has_L2)
					x += SCALE1(PILL_SIZE);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, w});

				blitButton("L1", screen, PAD_isPressed(BTN_L1), x + SCALE1(BUTTON_MARGIN),
				           y + SCALE1(BUTTON_MARGIN), 0);
				if (has_L2)
					blitButton("L2", screen, PAD_isPressed(BTN_L2), x + ox,
					           y + SCALE1(BUTTON_MARGIN), 0);
			}

			///////////////////////////////
			// Right shoulder buttons (R1, R2)
			///////////////////////////////
			{
				int x = 0;
				int y = oy;
				int w = 0;
				int ox = 0;

				w = getButtonWidth("R1") + SCALE1(BUTTON_MARGIN) * 2;
				ox = w;

				if (has_R2)
					w += getButtonWidth("R2") + SCALE1(BUTTON_MARGIN);

				// Right-align the button group
				x = FIXED_WIDTH - w - SCALE1(BUTTON_MARGIN + PADDING);
				if (!has_R2)
					x -= SCALE1(PILL_SIZE);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, w});

				// R2 comes first visually (left position), then R1
				blitButton(has_R2 ? "R2" : "R1", screen, PAD_isPressed(has_R2 ? BTN_R2 : BTN_R1),
				           x + SCALE1(BUTTON_MARGIN), y + SCALE1(BUTTON_MARGIN), 0);
				if (has_R2)
					blitButton("R1", screen, PAD_isPressed(BTN_R1), x + ox,
					           y + SCALE1(BUTTON_MARGIN), 0);
			}

			///////////////////////////////
			// D-pad (Up, Down, Left, Right)
			///////////////////////////////
			{
				int x = SCALE1(PADDING + PILL_SIZE);
				int y = oy + SCALE1(PILL_SIZE * 2);
				int o = SCALE1(BUTTON_MARGIN);

				// Vertical bar connecting Up and Down buttons
				SDL_FillRect(screen,
				             &(SDL_Rect){x, y + SCALE1(PILL_SIZE / 2), SCALE1(PILL_SIZE),
				                         SCALE1(PILL_SIZE * 2)},
				             RGB_DARK_GRAY);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("U", screen, PAD_isPressed(BTN_DPAD_UP), x + o, y + o, 0);

				y += SCALE1(PILL_SIZE * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("D", screen, PAD_isPressed(BTN_DPAD_DOWN), x + o, y + o, 0);

				x -= SCALE1(PILL_SIZE);
				y -= SCALE1(PILL_SIZE);

				// Horizontal bar connecting Left and Right buttons
				SDL_FillRect(screen,
				             &(SDL_Rect){x + SCALE1(PILL_SIZE / 2), y, SCALE1(PILL_SIZE * 2),
				                         SCALE1(PILL_SIZE)},
				             RGB_DARK_GRAY);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("L", screen, PAD_isPressed(BTN_DPAD_LEFT), x + o, y + o, 0);

				x += SCALE1(PILL_SIZE * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("R", screen, PAD_isPressed(BTN_DPAD_RIGHT), x + o, y + o, 0);
			}

			///////////////////////////////
			// Face buttons (A, B, X, Y)
			///////////////////////////////
			{
				int x = FIXED_WIDTH - SCALE1(PADDING + PILL_SIZE * 3) + SCALE1(PILL_SIZE);
				int y = oy + SCALE1(PILL_SIZE * 2);
				int o = SCALE1(BUTTON_MARGIN);

				// X (top)
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("X", screen, PAD_isPressed(BTN_X), x + o, y + o, 0);

				// B (bottom)
				y += SCALE1(PILL_SIZE * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("B", screen, PAD_isPressed(BTN_B), x + o, y + o, 0);

				x -= SCALE1(PILL_SIZE);
				y -= SCALE1(PILL_SIZE);

				// Y (left)
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("Y", screen, PAD_isPressed(BTN_Y), x + o, y + o, 0);

				// A (right)
				x += SCALE1(PILL_SIZE * 2);
				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("A", screen, PAD_isPressed(BTN_A), x + o, y + o, 0);
			}

			///////////////////////////////
			// Volume buttons (if available)
			///////////////////////////////
			if (has_volume) {
				int x = (FIXED_WIDTH - SCALE1(99)) / 2;
				int y = oy + SCALE1(PILL_SIZE);
				int w = SCALE1(42);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, SCALE1(98)});
				x += SCALE1(BUTTON_MARGIN);
				y += SCALE1(BUTTON_MARGIN);
				blitButton("VOL. -", screen, PAD_isPressed(BTN_MINUS), x, y, w);
				x += w + SCALE1(BUTTON_MARGIN);
				blitButton("VOL. +", screen, PAD_isPressed(BTN_PLUS), x, y, w);
			}

			///////////////////////////////
			// System buttons (Menu, Power)
			///////////////////////////////
			if (has_power || has_menu) {
				int bw = 42;
				int pw = has_both ? (bw * 2 + BUTTON_MARGIN * 3) : (bw + BUTTON_MARGIN * 2);

				int x = (FIXED_WIDTH - SCALE1(pw)) / 2;
				int y = oy + SCALE1(PILL_SIZE * 3);
				int w = SCALE1(bw);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, SCALE1(pw)});
				x += SCALE1(BUTTON_MARGIN);
				y += SCALE1(BUTTON_MARGIN);
				if (has_menu) {
					blitButton("MENU", screen, PAD_isPressed(BTN_MENU), x, y, w);
					x += w + SCALE1(BUTTON_MARGIN);
				}
				if (has_power) {
					blitButton("POWER", screen, PAD_isPressed(BTN_POWER), x, y, w);
				}
			}

			///////////////////////////////
			// Meta buttons (Select, Start) with quit hint
			///////////////////////////////
			{
				int x = (FIXED_WIDTH - SCALE1(99)) / 2;
				int y = oy + SCALE1(PILL_SIZE * 5);
				int w = SCALE1(42);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, SCALE1(130)});
				x += SCALE1(BUTTON_MARGIN);
				y += SCALE1(BUTTON_MARGIN);
				blitButton("SELECT", screen, PAD_isPressed(BTN_SELECT), x, y, w);
				x += w + SCALE1(BUTTON_MARGIN);
				blitButton("START", screen, PAD_isPressed(BTN_START), x, y, w);
				x += w + SCALE1(BUTTON_MARGIN);

				// Display "QUIT" hint - press both together to exit
				SDL_Surface* text = TTF_RenderUTF8_Blended(font.tiny, "QUIT", COLOR_LIGHT_TEXT);
				SDL_BlitSurface(text, NULL, screen,
				                &(SDL_Rect){x, y + (SCALE1(BUTTON_SIZE) - text->h) / 2});
				SDL_FreeSurface(text);
			}

			///////////////////////////////
			// Analog stick buttons (if available)
			///////////////////////////////
			if (has_L3) {
				int x = SCALE1(PADDING + PILL_SIZE);
				int y = oy + SCALE1(PILL_SIZE * 6);
				int o = SCALE1(BUTTON_MARGIN);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("L3", screen, PAD_isPressed(BTN_L3), x + o, y + o, 0);
			}

			if (has_R3) {
				int x = FIXED_WIDTH - SCALE1(PADDING + PILL_SIZE * 3) + SCALE1(PILL_SIZE);
				int y = oy + SCALE1(PILL_SIZE * 6);
				int o = SCALE1(BUTTON_MARGIN);

				GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){x, y, 0});
				blitButton("R3", screen, PAD_isPressed(BTN_R3), x + o, y + o, 0);
			}

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();
	}

	// Cleanup
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	return EXIT_SUCCESS;
}
