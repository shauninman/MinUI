/**
 * clock.c - Date and time configuration application
 *
 * Interactive clock setting tool for MinUI. Allows users to adjust
 * system date and time with a visual interface showing year, month,
 * day, hour, minute, and second fields. Supports both 12-hour and
 * 24-hour time formats.
 *
 * Navigation:
 * - LEFT/RIGHT: Move between fields (year, month, day, hour, min, sec)
 * - UP/DOWN: Increment/decrement selected field with wraparound
 * - SELECT: Toggle between 12-hour and 24-hour display
 * - A: Save changes and exit
 * - B: Cancel and exit without saving
 *
 * Loosely based on https://github.com/gameblabla/clock_sdl_app
 */

#include <msettings.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "api.h"
#include "defines.h"
#include "utils.h"

/**
 * Cursor position indices for the clock interface.
 * Determines which date/time field is currently selected.
 */
enum {
	CURSOR_YEAR,
	CURSOR_MONTH,
	CURSOR_DAY,
	CURSOR_HOUR,
	CURSOR_MINUTE,
	CURSOR_SECOND,
	CURSOR_AMPM,
};

/**
 * Main entry point for the clock setting application.
 *
 * Creates a graphical interface for adjusting system date and time.
 * Pre-renders digit sprites for efficient display updates, then enters
 * a main loop handling user input until the user saves or cancels.
 *
 * @return EXIT_SUCCESS on normal exit
 */
int main(int argc, char* argv[]) {
	PWR_setCPUSpeed(CPU_SPEED_MENU);

	SDL_Surface* screen = GFX_init(MODE_MAIN);
	PAD_init();
	PWR_init();
	InitSettings();

	// Pre-render all digit and separator characters into a sprite sheet
	// for fast blitting during the main loop
	SDL_Surface* digits =
	    SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(120, 16), FIXED_DEPTH, RGBA_MASK_AUTO);
	SDL_FillRect(digits, NULL, RGB_BLACK);

	SDL_Surface* digit;
	char* chars[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "/", ":", NULL};
	char* c;
	int i = 0;
#define DIGIT_WIDTH 10
#define DIGIT_HEIGHT 16

#define CHAR_SLASH 10
#define CHAR_COLON 11
	while (c = chars[i]) {
		digit = TTF_RenderUTF8_Blended(font.large, c, COLOR_WHITE);
		// Colon sits too low naturally, adjust vertically
		int y = i == CHAR_COLON ? SCALE1(-1.5) : 0;
		SDL_BlitSurface(
		    digit, NULL, digits,
		    &(SDL_Rect){(i * SCALE1(DIGIT_WIDTH)) + (SCALE1(DIGIT_WIDTH) - digit->w) / 2,
		                y + (SCALE1(DIGIT_HEIGHT) - digit->h) / 2});
		SDL_FreeSurface(digit);
		i += 1;
	}

	SDL_Event event;
	int quit = 0;
	int save_changes = 0;
	int select_cursor = 0;
	// Check if user prefers 24-hour format (stored as a flag file)
	int show_24hour = exists(USERDATA_PATH "/show_24hour");

	// Initialize with current system time
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	int32_t day_selected = tm.tm_mday;
	int32_t month_selected = tm.tm_mon + 1;
	uint32_t year_selected = tm.tm_year + 1900;
	int32_t hour_selected = tm.tm_hour;
	int32_t minute_selected = tm.tm_min;
	int32_t seconds_selected = tm.tm_sec;
	int32_t am_selected = tm.tm_hour < 12;

	/**
	 * Blits a single digit from the pre-rendered sprite sheet.
	 *
	 * @param i Index of character in sprite sheet (0-9, CHAR_SLASH, CHAR_COLON)
	 * @param x X position (pre-scaled)
	 * @param y Y position (pre-scaled)
	 * @return New x position after blitting (x + digit width)
	 */
	int blit(int i, int x, int y) {
		SDL_BlitSurface(digits, &(SDL_Rect){i * SCALE1(10), 0, SCALE2(10, 16)}, screen,
		                &(SDL_Rect){x, y});
		return x + SCALE1(10);
	}

	/**
	 * Draws the selection underline bar beneath the current field.
	 *
	 * @param x X position (pre-scaled)
	 * @param y Y position (pre-scaled)
	 * @param w Width of bar (pre-scaled)
	 */
	void blitBar(int x, int y, int w) {
		GFX_blitPill(ASSET_UNDERLINE, screen, &(SDL_Rect){x, y, w});
	}

	/**
	 * Renders a numeric value using digit sprites.
	 *
	 * Supports 2-digit and 4-digit numbers (days/months/hours/minutes and years).
	 *
	 * @param num Number to render (0-9999)
	 * @param x Starting X position (pre-scaled)
	 * @param y Y position (pre-scaled)
	 * @return New x position after rendering all digits
	 */
	int blitNumber(int num, int x, int y) {
		int n;
		if (num > 999) {
			n = num / 1000;
			num -= n * 1000;
			x = blit(n, x, y);

			n = num / 100;
			num -= n * 100;
			x = blit(n, x, y);
		}
		n = num / 10;
		num -= n * 10;
		x = blit(n, x, y);

		n = num;
		x = blit(n, x, y);

		return x;
	}

	/**
	 * Validates and wraps date/time values after user modification.
	 * Delegates to validateDateTime() in utils.c.
	 */
	void validate(void) {
		validateDateTime(&year_selected, &month_selected, &day_selected, &hour_selected,
		                 &minute_selected, &seconds_selected);
	}

	// In 12-hour mode, option_count includes AM/PM field
	int option_count = 7;

	int dirty = 1;
	int show_setting = 0;
	int was_online = PLAT_isOnline();

	// Main event loop
	while (!quit) {
		uint32_t frame_start = SDL_GetTicks();

		PAD_poll();

		// UP: Increment current field
		if (PAD_justRepeated(BTN_UP)) {
			dirty = 1;
			switch (select_cursor) {
			case CURSOR_YEAR:
				year_selected++;
				break;
			case CURSOR_MONTH:
				month_selected++;
				break;
			case CURSOR_DAY:
				day_selected++;
				break;
			case CURSOR_HOUR:
				hour_selected++;
				break;
			case CURSOR_MINUTE:
				minute_selected++;
				break;
			case CURSOR_SECOND:
				seconds_selected++;
				break;
			case CURSOR_AMPM:
				// Toggle AM/PM by adding 12 hours
				hour_selected += 12;
				break;
			default:
				break;
			}
		} else if (PAD_justRepeated(BTN_DOWN)) {
			dirty = 1;
			switch (select_cursor) {
			case CURSOR_YEAR:
				year_selected--;
				break;
			case CURSOR_MONTH:
				month_selected--;
				break;
			case CURSOR_DAY:
				day_selected--;
				break;
			case CURSOR_HOUR:
				hour_selected--;
				break;
			case CURSOR_MINUTE:
				minute_selected--;
				break;
			case CURSOR_SECOND:
				seconds_selected--;
				break;
			case CURSOR_AMPM:
				// Toggle AM/PM by subtracting 12 hours
				hour_selected -= 12;
				break;
			default:
				break;
			}
		} else if (PAD_justRepeated(BTN_LEFT)) {
			// Move cursor left with wraparound
			dirty = 1;
			select_cursor--;
			if (select_cursor < 0)
				select_cursor += option_count;
		} else if (PAD_justRepeated(BTN_RIGHT)) {
			// Move cursor right with wraparound
			dirty = 1;
			select_cursor++;
			if (select_cursor >= option_count)
				select_cursor -= option_count;
		} else if (PAD_justPressed(BTN_A)) {
			// Accept changes and exit
			save_changes = 1;
			quit = 1;
		} else if (PAD_justPressed(BTN_B)) {
			// Cancel and exit without saving
			quit = 1;
		} else if (PAD_justPressed(BTN_SELECT)) {
			// Toggle between 12-hour and 24-hour display
			dirty = 1;
			show_24hour = !show_24hour;
			option_count = (show_24hour ? CURSOR_SECOND : CURSOR_AMPM) + 1;
			if (select_cursor >= option_count)
				select_cursor -= option_count;

			// Persist preference as a flag file
			if (show_24hour) {
				system("touch " USERDATA_PATH "/show_24hour");
			} else {
				system("rm " USERDATA_PATH "/show_24hour");
			}
		}

		// Check for power state changes (battery, charging, etc.)
		PWR_update(&dirty, NULL, NULL, NULL);

		// Redraw if network status changed (affects status icons)
		int is_online = PLAT_isOnline();
		if (was_online != is_online)
			dirty = 1;
		was_online = is_online;

		// Redraw screen if anything changed
		if (dirty) {
			// Validate and wrap date/time values
			validate();

			GFX_clear(screen);

			// Draw hardware status indicators (battery, WiFi, etc.)
			GFX_blitHardwareGroup(screen, show_setting);

			if (show_setting)
				GFX_blitHardwareHints(screen, show_setting);
			else
				GFX_blitButtonGroup((char*[]){"SELECT", show_24hour ? "12 HOUR" : "24 HOUR", NULL},
				                    0, screen, 0);

			GFX_blitButtonGroup((char*[]){"B", "CANCEL", "A", "SET", NULL}, 1, screen, 1);

			// Center the date/time display
			// Width is 188 pixels (@1x) in 24-hour mode, 223 pixels in 12-hour mode
			int ox = (screen->w - (show_24hour ? SCALE1(188) : SCALE1(223))) / 2;

			// Render date/time in format: YYYY/MM/DD HH:MM:SS [AM/PM]
			int x = ox;
			int y = SCALE1((((FIXED_HEIGHT / FIXED_SCALE) - PILL_SIZE - DIGIT_HEIGHT) / 2));

			x = blitNumber(year_selected, x, y);
			x = blit(CHAR_SLASH, x, y);
			x = blitNumber(month_selected, x, y);
			x = blit(CHAR_SLASH, x, y);
			x = blitNumber(day_selected, x, y);
			x += SCALE1(10); // space between date and time

			am_selected = hour_selected < 12;
			if (show_24hour) {
				x = blitNumber(hour_selected, x, y);
			} else {
				// Convert to 12-hour format for display
				int hour = convertTo12Hour(hour_selected);
				x = blitNumber(hour, x, y);
			}
			x = blit(CHAR_COLON, x, y);
			x = blitNumber(minute_selected, x, y);
			x = blit(CHAR_COLON, x, y);
			x = blitNumber(seconds_selected, x, y);

			int ampm_w;
			if (!show_24hour) {
				x += SCALE1(10); // space before AM/PM
				SDL_Surface* text =
				    TTF_RenderUTF8_Blended(font.large, am_selected ? "AM" : "PM", COLOR_WHITE);
				ampm_w = text->w + SCALE1(2);
				SDL_BlitSurface(text, NULL, screen, &(SDL_Rect){x, y - SCALE1(3)});
				SDL_FreeSurface(text);
			}

			// Draw selection cursor underline
			x = ox;
			y += SCALE1(19);
			if (select_cursor != CURSOR_YEAR) {
				x += SCALE1(50); // Width of "YYYY/"
				x += (select_cursor - 1) * SCALE1(30);
			}
			blitBar(x, y,
			        (select_cursor == CURSOR_YEAR
			             ? SCALE1(40)
			             : (select_cursor == CURSOR_AMPM ? ampm_w : SCALE1(20))));

			GFX_flip(screen);
			dirty = 0;
		} else
			GFX_sync();
	}

	// Cleanup
	SDL_FreeSurface(digits);

	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

	// Apply changes to system clock if user confirmed
	if (save_changes)
		PLAT_setDateTime(year_selected, month_selected, day_selected, hour_selected,
		                 minute_selected, seconds_selected);

	return EXIT_SUCCESS;
}
