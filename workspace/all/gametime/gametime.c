// heavily modified from the Onion original: https://github.com/OnionUI/Onion/blob/main/src/playActivity/playActivityUI.c
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#include <sqlite3.h>
#include <gametimedb.h>

struct ListLayout
{
    int list_display_size_x;
    int list_display_size_y;
    int list_display_start_x;
    int list_display_start_y;
    SDL_Rect list_display_rect;

    int sub_title_x;
    int sub_title_y;

    int items_per_page;
    int num_pages;
} layout = {0};

static bool quit = false;

static void sigHandler(int sig)
{
    switch (sig) {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}

#define BIG_PILL_SIZE 48
#define IMG_MARGIN 8
#define IMG_MAX_WIDTH BIG_PILL_SIZE - IMG_MARGIN
#define IMG_MAX_HEIGHT BIG_PILL_SIZE - IMG_MARGIN

static SDL_Surface *screen;

static PlayActivities *play_activities;

static inline SDL_Color colorFromUint(uint32_t colour)
{
	SDL_Color tempcol;
	tempcol.a = 255;
	tempcol.r = (colour >> 16) & 0xFF;
	tempcol.g = (colour >> 8) & 0xFF;
	tempcol.b = colour & 0xFF;
	return tempcol;
}

///////

int _renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect, bool right_align)
{
    int text_width = 0;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (textSurface != NULL) {
        text_width = textSurface->w;
        if (right_align)
            SDL_BlitSurface(textSurface, NULL, screen, &(SDL_Rect){rect->w - textSurface->w, rect->y, rect->w, rect->h});
        else
            SDL_BlitSurface(textSurface, NULL, screen, rect);
        SDL_FreeSurface(textSurface);
    }
    return text_width;
}

int renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, false);
}

int renderTextAlignRight(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    return _renderText(text, font, color, rect, true);
}

// Set a pixel color on the surface
void _setPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
    if (x < 0 || x >= surface->w || y < 0 || y >= surface->h) {
        return; // Out of bounds check
    }

    // Lock surface if needed
    SDL_LockSurface(surface);

    Uint8 *pixelPtr = (Uint8 *)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;

    switch (surface->format->BytesPerPixel) {
        case 1:
            *pixelPtr = color;
            break;
        case 2:
            *(Uint16 *)pixelPtr = color;
            break;
        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                pixelPtr[0] = (color >> 16) & 0xFF;
                pixelPtr[1] = (color >> 8) & 0xFF;
                pixelPtr[2] = color & 0xFF;
            } else {
                pixelPtr[0] = color & 0xFF;
                pixelPtr[1] = (color >> 8) & 0xFF;
                pixelPtr[2] = (color >> 16) & 0xFF;
            }
            break;
        case 4:
            *(Uint32 *)pixelPtr = color;
            break;
    }

    SDL_UnlockSurface(surface);
}

// Draw a filled circle
void _drawFilledCircle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                _setPixel(surface, cx + x, cy + y, color);
            }
        }
    }
}

// Draw a filled rounded rectangle
void renderRoundedRectangle(SDL_Rect rect, Uint32 color, int radius) {
    // Fill the center and straight edges
    SDL_Rect fillRect = {rect.x + radius, rect.y, rect.w - 2 * radius, rect.h};
    SDL_FillRect(screen, &fillRect, color);

    fillRect.x = rect.x;
    fillRect.y = rect.y + radius;
    fillRect.w = rect.w;
    fillRect.h = rect.h - 2 * radius;
    SDL_FillRect(screen, &fillRect, color);

    // Draw the corner circles
    _drawFilledCircle(screen, rect.x + radius, rect.y + radius, radius, color);                         // Top-left
    _drawFilledCircle(screen, rect.x + rect.w - radius - 1, rect.y + radius, radius, color);            // Top-right
    _drawFilledCircle(screen, rect.x + radius, rect.y + rect.h - radius - 1, radius, color);            // Bottom-left
    _drawFilledCircle(screen, rect.x + rect.w - radius - 1, rect.y + rect.h - radius - 1, radius, color); // Bottom-right
}

SDL_Surface *loadRomImage(char *image_path)
{
    if(!exists(image_path))
        return NULL;

    SDL_Surface *img = IMG_Load(image_path);
    if(!img)
        return NULL;

    if(img->format->format != SDL_PIXELFORMAT_RGBA32) {
        SDL_Surface *optimized = SDL_ConvertSurfaceFormat(img, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(img); 
        img = optimized;
    }
    
    double sw = (double)SCALE1(IMG_MAX_WIDTH) / img->w;
    double sh = (double)SCALE1(IMG_MAX_HEIGHT) / img->h;
    double s = MIN(sw, sh);

    SDL_PixelFormat *ft = img->format;
    SDL_Surface *dst = SDL_CreateRGBSurface(0, (int)(s * img->w), (int)(s * img->h), ft->BitsPerPixel, ft->Rmask, ft->Gmask, ft->Bmask, ft->Amask);

    SDL_Rect src_rect = {0, 0, img->w, img->h};
    SDL_Rect dst_rect = {0, 0, dst->w, dst->h};
    SDL_SoftStretch(img, &src_rect, dst, &dst_rect);

    SDL_FreeSurface(img);

    return dst;
}

void renderList(int count, int start, int end, int selected)
{
    char num_str[12];
    char rom_name[255];
    char total[25];
    char average[25];
    char plays[25];

    int num_width = 0;

    const int elemHeight = SCALE1(BIG_PILL_SIZE);
    const int thumbMargin = SCALE1(IMG_MARGIN);
    const int textHeight = (elemHeight - thumbMargin) / 2;

    int selected_row = selected - start;
    for (int index=start,row=0; index<end; index++,row++) {
        bool isSelected = selected_row == row;

        PlayActivity *entry = play_activities->play_activity[index];
        ROM *rom = entry->rom;

        renderRoundedRectangle((SDL_Rect){
            layout.list_display_start_x, 
            layout.list_display_start_y + row * elemHeight, 
            layout.list_display_size_x, 
            elemHeight
        }, isSelected ? RGB_WHITE : RGB_BLACK, SCALE1(24));

        SDL_Surface *romImage = loadRomImage(rom->image_path);
        if (romImage) {
            SDL_Rect rectRomImage = {
                layout.list_display_start_x + num_width + thumbMargin / 2 + (SCALE1(IMG_MAX_WIDTH) - romImage->w) / 2, 
                layout.list_display_start_y + elemHeight * row + thumbMargin / 2, 
                SCALE1(IMG_MAX_WIDTH), 
                SCALE1(IMG_MAX_HEIGHT)
            };
            GFX_ApplyRounderCorners(romImage, SCALE1(18));
            SDL_BlitSurface(romImage, NULL, screen, &rectRomImage);
            SDL_FreeSurface(romImage);
        }
        else {
            SDL_Rect rectRomImage = {
                layout.list_display_start_x + num_width + thumbMargin / 2, 
                layout.list_display_start_y + elemHeight * row + thumbMargin / 2, 
                SCALE1(IMG_MAX_WIDTH), 
                SCALE1(IMG_MAX_HEIGHT)
            };

            renderRoundedRectangle(rectRomImage, RGB_DARK_GRAY, SCALE1(18));

            // TODO: no getter exposed for this right now
            //SDL_Rect rect = asset_rects[ASSET_GAMEPAD];
            SDL_Rect rect = (SDL_Rect){SCALE4(92,51,18,10)};
            int x = rectRomImage.x;
            int y = rectRomImage.y;
            x += (SCALE1(IMG_MAX_WIDTH) - rect.w) / 2;
            y += (SCALE1(IMG_MAX_HEIGHT) - rect.h) / 2;

            GFX_blitAssetColor(ASSET_GAMEPAD, NULL, screen, &(SDL_Rect){x,y}, THEME_COLOR1_255);
        }

        cleanName(rom_name, rom->name);
        SDL_Color textColor = COLOR_WHITE;
        if(isSelected) {
            //textColor = colorFromUint(THEME_COLOR1);
            textColor = COLOR_BLACK;
        }
        renderText(rom_name, font.medium, textColor, &(SDL_Rect){
            layout.list_display_start_x + num_width + thumbMargin + SCALE1(IMG_MAX_WIDTH), 
            layout.list_display_start_y + thumbMargin / 2 + elemHeight * row, 
            layout.list_display_size_x, 
            textHeight});

        serializeTime(total, entry->play_time_total);
        serializeTime(average, entry->play_time_average);
        snprintf(plays, 24, "%d", entry->play_count);

        const char *details[] = {"TOTAL ", total, "  AVERAGE ", average, "  # PLAYS ", plays};
        SDL_Rect detailsRect = {
            layout.list_display_start_x + num_width + thumbMargin + SCALE1(IMG_MAX_WIDTH), 
            layout.list_display_start_y + thumbMargin + textHeight + elemHeight * row, 
            layout.list_display_size_x, 
            textHeight
        };
        for (int i = 0; i < 6; i++) {
            SDL_Color detailCol = i % 2 == 0 ? COLOR_DARK_TEXT : colorFromUint(THEME_COLOR2_255);
            //SDL_Color detailCol = colorFromUint(i % 2 == 0 ? THEME_COLOR3_255 : THEME_COLOR2_255);
            //SDL_Color detailCol = i % 2 == 0 ? COLOR_DARK_TEXT : COLOR_LIGHT_TEXT;
            detailsRect.x += renderText(details[i], font.small, detailCol, &detailsRect);
        }
    }

    if (count>layout.items_per_page) {
        #define SCROLL_WIDTH 24
        #define SCROLL_HEIGHT 4
        int ox = (screen->w - SCALE1(SCROLL_WIDTH)) / 2;
        int oy = SCALE1((PILL_SIZE - SCROLL_HEIGHT) / 2);
        if (start>0) 
            GFX_blitAsset(ASSET_SCROLL_UP,   NULL, screen, &(SDL_Rect){ox, SCALE1(PADDING + PILL_SIZE)});
        if (end<count) 
            GFX_blitAsset(ASSET_SCROLL_DOWN, NULL, screen, &(SDL_Rect){ox, screen->h - SCALE1(PADDING + PILL_SIZE + BUTTON_SIZE) + oy});
    }
}

void initLayout()
{
    // unscaled
    int hw = screen->w;
    int hh = screen->h;

    // the title. just leave the default padding all around
    layout.sub_title_x = SCALE1(PADDING);
    layout.sub_title_y = SCALE1(PADDING);

    // the main list.
    // x: start inside the default padding, and align with the title pill.
    // y: default padding, below the title pill and some additional padding to leave some breathing room (BUTTON_MARGIN).
    layout.list_display_start_x = SCALE1(PADDING);
    layout.list_display_start_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN);
    // x: stretch whole width inside default padding + extra margin (see above).
    // y: stretch whole height below list_display_start_y, leaving room at the bottom for padding and button hints.
    layout.list_display_size_x = hw - SCALE1(PADDING * 2);
    layout.list_display_size_y = hh - SCALE1(PADDING * 2 + PILL_SIZE * 2 + BUTTON_MARGIN * 2);

    layout.list_display_rect.x = layout.list_display_start_x, 
    layout.list_display_rect.y = layout.list_display_start_y, 
    layout.list_display_rect.w = layout.list_display_size_x, 
    layout.list_display_rect.h = layout.list_display_size_y;

    layout.items_per_page = layout.list_display_size_y / SCALE1(BIG_PILL_SIZE);
    layout.num_pages = (int)ceil((double)play_activities->count / (double)layout.items_per_page);
}

int main(int argc, char *argv[])
{
    InitSettings();

    PWR_setCPUSpeed(CPU_SPEED_MENU);

    screen = GFX_init(MODE_MAIN);
    PAD_init();
    PWR_init();

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    play_activities = play_activity_find_all();
    LOG_debug("found %d roms\n", play_activities->count);

    initLayout();
    int count = play_activities->count;
    int selected = 0;
    int start = 0;
    int end = MIN(count, layout.items_per_page);
    int visible_rows = end;

    int dirty = 1;
    int show_setting = 0;
    int was_online = PLAT_isOnline();
    while (!quit)
    {
        uint32_t frame_start = SDL_GetTicks();

        PAD_poll();

        // This might be too harsh, but ignore all combos with MENU (most likely a shortcut for someone else)
        if (PAD_justPressed(BTN_MENU))
        {
            // ?
        }
        else {
            if (PAD_justRepeated(BTN_UP))
            {
                selected -= 1;
                if (selected<0) {
                    selected = count - 1;
                    start = MAX(0,count - layout.items_per_page);
                    end = count;
                }
                else if (selected<start) {
                    start -= 1;
                    end -= 1;
                }
                dirty = 1;
            }
            else if (PAD_justRepeated(BTN_DOWN))
            {
                selected += 1;
                if (selected>=count) {
                    selected = 0;
                    start = 0;
                    end = visible_rows;
                }
                else if (selected>=end) {
                    start += 1;
                    end += 1;
                }
                dirty = 1;
            }
            else if (PAD_justPressed(BTN_B))
            {
                quit = 1;
            }
        }

        PWR_update(&dirty, &show_setting, NULL, NULL);

        if (dirty) {
            GFX_clear(screen);

            // title pill
            {
                int max_width = screen->w - SCALE1(PADDING * 2);
                if(screen->w >= SCALE1(320)) {
                    int ow = GFX_blitHardwareGroup(screen, show_setting);
                    max_width = screen->w - SCALE1(PADDING * 2) - ow;
                }

                int play_time_total = play_activities->play_time_total;
                char play_time_total_formatted[255];
                serializeTime(play_time_total_formatted, play_time_total);
                char display_name[256];
                sprintf(display_name, "Time spent having fun: %s", play_time_total_formatted);

                char title[256];
                int text_width = GFX_truncateText(font.large, display_name, title, max_width, SCALE1(BUTTON_PADDING * 2));
                max_width = MIN(max_width, text_width);

                SDL_Surface *text;
                text = TTF_RenderUTF8_Blended(font.large, title, COLOR_WHITE);
                GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)});
                SDL_BlitSurface(text, &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                SDL_FreeSurface(text);
            }

            renderList(count, start, end, selected);

            if (show_setting)
                GFX_blitHardwareHints(screen, show_setting);
            else
                GFX_blitButtonGroup((char *[]){"U/D", "SCROLL", NULL}, 0, screen, 0);

            GFX_blitButtonGroup((char *[]){"B", "BACK", NULL}, 1, screen, 1);

            GFX_flip(screen);
            dirty = 0;
        }
        else {
            GFX_sync();
        }
    }

    free_play_activities(play_activities);

    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();

    return EXIT_SUCCESS;
}