#include <stdbool.h>
#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"

#define NUM_OPTIONS 3
#define MAX_NAME_LEN 50

#define fontcount 2
const char *fontnames[] = {
    "Next", "OG"};
MinUISettings uisettings;

int read_settings(const char *filename)
{

    char diskfilename[256];
    snprintf(diskfilename, sizeof(diskfilename), SHARED_USERDATA_PATH "/%s", filename);
    FILE *file = fopen(diskfilename, "r");
    if (file == NULL)
    {
        perror("Unable to open settings file");
        return 1;
    }

    char line[256];
    int current_light = -1;
    while (fgets(line, sizeof(line), file))
    {
        
            int temp_value;
            uint32_t temp_color;

            if (sscanf(line, "font=%i", &temp_value) == 1)
            {
                uisettings.font = temp_value;
                continue;
            }
            if (sscanf(line, "color1=%x", &temp_color) == 1)
            {
                uisettings.color1 = temp_color;
                continue;
            }
            if (sscanf(line, "color2=%x", &temp_color) == 1)
            {
                uisettings.color2 = temp_color;
                continue;
            }
            if (sscanf(line, "color3=%x", &temp_color) == 1)
            {
                uisettings.color3 = temp_color;
                continue;
            }
        
    }

    fclose(file);
    return 0;
}

int save_settings(const char *filename)
{
    char diskfilename[256];
    snprintf(diskfilename, sizeof(diskfilename), SHARED_USERDATA_PATH "/%s", filename);
    FILE *file = fopen(diskfilename, "w");
    if (file == NULL)
    {
        perror("Unable to open settings file for writing");
        return 1;
    }


    fprintf(file, "font=%i\n", uisettings.font);
    fprintf(file, "color1=0x%06X\n", uisettings.color1);
    fprintf(file, "color2=0x%06X\n", uisettings.color2);
    fprintf(file, "color3=0x%06X\n", uisettings.color3);
    

    fclose(file);
    return 0;
}

void handle_light_input(SDL_Event *event, int selected_setting)
{
    const uint32_t bright_colors[] = {
        // Blues
        0x000080, // Navy Blue
        0x0080FF, // Sky Blue
        0x00BFFF, // Deep Sky Blue
        0x8080FF, // Light Blue
        0x483D8B, // Dark Slate Blue
        0x7B68EE, // Medium Slate Blue

        // Cyan
        0x00FFFF, // Cyan
        0x40E0D0, // Turquoise
        0x80FFFF, // Light Cyan
        0x008080, // Teal
        0x00CED1, // Dark Turquoise
        0x20B2AA, // Light Sea Green

        // Green
        0x00FF00, // Green
        0x32CD32, // Lime Green
        0x7FFF00, // Chartreuse
        0x80FF00, // Lime
        0x80FF80, // Light Green
        0xADFF2F, // Green Yellow

        // Magenta
        0xFF00FF, // Magenta
        0xFF80C0, // Light Magenta
        0xEE82EE, // Violet
        0xDA70D6, // Orchid
        0xDDA0DD, // Plum
        0xBA55D3, // Medium Orchid
        0x9B2257, // Medium Orchid

        // Purple
        0x800080, // Purple
        0x8A2BE2, // Blue Violet
        0x9400D3, // Dark Violet
        0x9B30FF, // Purple2
        0xA020F0, // Purple
        0x9370DB, // Medium Purple

        // Red
        0xFF0000, // Red
        0xFF4500, // Red Orange
        0xFF6347, // Tomato
        0xDC143C, // Crimson
        0xFF69B4, // Hot Pink
        0xFF1493, // Deep Pink

        // Yellow and Orange
        0xFFD700, // Gold
        0xFFA500, // Orange
        0xFF8000, // Orange Red
        0xFFFF00, // Yellow
        0xFFFF80, // Light Yellow
        0xFFDAB9, // Peach Puff

        // Others
        0xFFFFFF, // White
        0xC0C0C0, // Silver
        0x000000,  // Black
        0x1E2329,  // GRAY
        0xCCCCCC    // GRAY 2
    };

    const int num_bright_colors = sizeof(bright_colors) / sizeof(bright_colors[0]);

    switch (selected_setting)
    {

    case 0: // Font
        if (PAD_justPressed(BTN_RIGHT))
        {
            uisettings.font = (uisettings.font % fontcount) + 1; // Increase effect (1 to 8)
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            uisettings.font = (uisettings.font - 2 + fontcount) % fontcount + 1; // Decrease effect (1 to 8)
        }
        break;
    case 1: // Color 1
        if (PAD_justPressed(BTN_RIGHT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color1)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color1 = bright_colors[(current_index + 1) % num_bright_colors];
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color1)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color1 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    case 2: // Color 2
        if (PAD_justPressed(BTN_RIGHT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color2)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color2 = bright_colors[(current_index + 1) % num_bright_colors];
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color2)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color2 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    case 3: // Color 3
        if (PAD_justPressed(BTN_RIGHT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color3)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color3 = bright_colors[(current_index + 1) % num_bright_colors];
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == uisettings.color3)
                {
                    current_index = i;
                    break;
                }
            }
            uisettings.color3 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    }

    // Save settings after each change

    save_settings("minuisettings.txt");
}

SDL_Color hex_to_sdl_color(uint32_t hex)
{
    SDL_Color color;
    color.r = (hex >> 16) & 0xFF;
    color.g = (hex >> 8) & 0xFF;
    color.b = hex & 0xFF;
    color.a = 255;
    return color;
}

int main(int argc, char *argv[])
{
    PWR_setCPUSpeed(CPU_SPEED_MENU);

    SDL_Surface* screen = GFX_init(MODE_MAIN);
    TTF_Font *font_med = TTF_OpenFont("main.ttf", SCALE1(FONT_MEDIUM));
    if (!font_med)
    {
        LOG_debug("Unable to load main.ttf\n");
        GFX_quit();
        return EXIT_FAILURE;
    }

	PAD_init();
	PWR_init();
	InitSettings();

    // Read initial settings
    if (read_settings("minuisettings.txt") != 0)
    {
        LOG_debug("Unable to load minuisettings.txt\n");
        QuitSettings();
        PWR_quit();
        PAD_quit();
        GFX_quit();
        return EXIT_FAILURE;
    }

    int selected_light = 0;
    int selected_setting = 0;
    SDL_Event event;
    int quit = 0;
	int dirty = 1;
    int show_setting = 0; // 1=brightness,2=volume,3=colortemp
    int was_online = PLAT_isOnline();

    while (!quit)
    {
        GFX_startFrame();
        uint32_t frame_start = SDL_GetTicks();
		
		PAD_poll();

        PWR_update(&dirty, &show_setting, NULL, NULL);
        
        int is_online = PLAT_isOnline();
		if (was_online!=is_online) dirty = 1;
		was_online = is_online;
        
        if (PAD_justPressed(BTN_B)) {
            quit = 1;
        }
        else if(PAD_justPressed(BTN_DOWN)) {
            selected_setting = (selected_setting + 1) % 6;
            dirty = 1;
        }
        else if(PAD_justPressed(BTN_UP)) {
            selected_setting = (selected_setting - 1 + 6) % 6;
            dirty = 1;
        }
        else if(PAD_justPressed(BTN_L1)) {
            selected_light = (selected_light - 1 + NUM_OPTIONS) % NUM_OPTIONS;
            dirty = 1;
        }
        else if(PAD_justPressed(BTN_R1)) {
            selected_light = (selected_light + 1) % NUM_OPTIONS;
            dirty = 1;
        }
        else if(PAD_justPressed(BTN_LEFT) || PAD_justPressed(BTN_RIGHT)) {
            handle_light_input(&event, selected_setting);
            dirty = 1;
        }

        if (dirty) {
            GFX_clear(screen);

            int ow = GFX_blitHardwareGroup(screen, show_setting);

            if (show_setting) GFX_blitHardwareHints(screen, show_setting);

			GFX_blitButtonGroup((char*[]){ "B","BACK", NULL }, 1, screen, 1);

            // Display settings
            const char *settings_labels[4] = {"Font", "Color1", "Color2", "Color3"};
            int settings_values[4] = {
                uisettings.font,
                uisettings.color1,
                uisettings.color2,
                uisettings.color3,
            };

            // title pill
            {
                int max_width = screen->w - SCALE1(PADDING * 2) - ow;

                char display_name[256];
                sprintf(display_name, "%s", "MinUI Next Settings");
                char title[256];
                int text_width = GFX_truncateText(font_med, display_name, title, max_width, SCALE1(BUTTON_PADDING * 2));
                max_width = MIN(max_width, text_width);

                SDL_Surface *text;
                text = TTF_RenderUTF8_Blended(font_med, title, COLOR_WHITE);
                GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)});
                SDL_BlitSurface(text, &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                SDL_FreeSurface(text);
            }

            for (int j = 0; j < 4; ++j)
            {
                char setting_text[256];
                bool selected = (j == selected_setting);
                SDL_Color current_color = selected ? COLOR_BLACK : COLOR_WHITE;

                int y = SCALE1(PADDING + PILL_SIZE * (j + 1));

                // saving this for font names
                if (j == 0)
                { // Display font name instead of number
            
                    snprintf(setting_text, sizeof(setting_text), "%s: %s", settings_labels[j], fontnames[settings_values[j] - 1]);

                    SDL_Surface *text = TTF_RenderUTF8_Blended(font_med, setting_text, current_color);
                    int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
                    GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
                                 &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
                    SDL_BlitSurface(text, 
                        &(SDL_Rect){0, 0, text->w, text->h}, screen, 
                        &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
                    SDL_FreeSurface(text);
                }
                else if (j < 5)
                { // Display color as hex code
                    snprintf(setting_text, sizeof(setting_text), "%s", settings_labels[j]);

                    SDL_Surface *text = TTF_RenderUTF8_Blended(font_med, setting_text, current_color);
                    int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
                    GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, 
                        &(SDL_Rect){SCALE1(PADDING), y, text_width + SCALE1(BUTTON_MARGIN + BUTTON_SIZE), SCALE1(PILL_SIZE)});
                    SDL_BlitSurface(text, 
                        &(SDL_Rect){0, 0, text->w, text->h}, screen, 
                        &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
                    SDL_FreeSurface(text);

                    GFX_blitAssetColor(ASSET_BUTTON, NULL, screen, &(SDL_Rect){
                        SCALE1(PADDING) + text_width,
                        y + SCALE1(BUTTON_MARGIN)
                    }, settings_values[j]);
                }
                else
                {
                    snprintf(setting_text, sizeof(setting_text), "%s: %d", settings_labels[j], settings_values[j]);

                    SDL_Surface *text = TTF_RenderUTF8_Blended(font_med, setting_text, current_color);
                    int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
                    GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, 
                        &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
                    SDL_BlitSurface(text, 
                        &(SDL_Rect){0, 0, text->w, text->h}, screen, 
                        &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
                    SDL_FreeSurface(text);
                }
            }

            GFX_flip(screen);
            dirty = 0;
        }
        else GFX_sync();
    }

    QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();

    return EXIT_SUCCESS;
}
