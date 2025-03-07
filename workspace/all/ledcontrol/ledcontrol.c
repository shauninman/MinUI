#include <stdbool.h>
#include <msettings.h>

#include "sdl.h"
#include "defines.h"
#include "api.h"
#include "utils.h"


#define NUM_OPTIONS 4
#define MAX_NAME_LEN 255

const char *lightnames[] = {
    "F1 key", "F2 key", "Top bar", "L&R triggers"};
#define NROF_TRIGGERS 14
const char *triggernames[] = {
    "B", "A", "Y", "X", "L", "R", "FN1", "FN2", "MENU", "SELECT", "START", "ALL", "LR", "DPAD"};

const char *effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive"};
const char *topbar_effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive", "Topbar Rainbow", "Topbar night"};
const char *lr_effect_names[] = {
    "Linear", "Breathe", "Interval Breathe", "Static",
    "Blink 1", "Blink 2", "Blink 3", "Rainbow", "Twinkle",
    "Fire", "Glitter", "NeonGlow", "Firefly", "Aurora", "Reactive", "LR Rainbow", "LR Reactive"};



    void save_settings() {
        LOG_info("saving settings plat");
        char diskfilename[256];
        snprintf(diskfilename, sizeof(diskfilename), SHARED_USERDATA_PATH "/ledsettings.txt");
        FILE *file = fopen(diskfilename, "w");

        if (file == NULL)
        {
            perror("Unable to open settings file for writing");
        } else {
            LOG_info("saving leds!");
            for (int i = 0; i < MAX_LIGHTS; ++i)
            {
                fprintf(file, "[%s]\n", lights[i].name);
                fprintf(file, "effect=%d\n", lights[i].effect);
                fprintf(file, "color1=0x%06X\n", lights[i].color1);
                fprintf(file, "color2=0x%06X\n", lights[i].color2);
                fprintf(file, "speed=%d\n", lights[i].speed);
                fprintf(file, "brightness=%d\n", lights[i].brightness);
                fprintf(file, "trigger=%d\n", lights[i].trigger);
                fprintf(file, "filename=%s\n\n", lights[i].filename);
            }
    
            fclose(file);
        }
    }
    

void handle_light_input(LightSettings *light, SDL_Event *event, int selected_setting)
{
    const uint32_t bright_colors[] = {
        // Blues
        0x000022, 0x000044, 0x000066, 0x000088, 0x0000AA, 0x0000CC, 0x3366FF, 0x4D7AFF, 0x6699FF, 0x80B3FF, 0x99CCFF, 0xB3D9FF,
        // Cyan
        0x002222, 0x004444, 0x006666, 0x008888, 0x00AAAA, 0x00CCCC, 0x33FFFF, 0x4DFFFF, 0x66FFFF, 0x80FFFF, 0x99FFFF, 0xB3FFFF,
        // Green
        0x002200, 0x004400, 0x006600, 0x008800, 0x00AA00, 0x00CC00, 0x33FF33, 0x4DFF4D, 0x66FF66, 0x80FF80, 0x99FF99, 0xB3FFB3,
        // Magenta
        0x220022, 0x440044, 0x660066, 0x880088, 0xAA00AA, 0xCC00CC, 0xFF33FF, 0xFF4DFF, 0xFF66FF, 0xFF80FF, 0xFF99FF, 0xFFB3FF,
        // Purple
        0x110022, 0x220044, 0x330066, 0x440088, 0x5500AA, 0x6600CC, 0x8833FF, 0x994DFF, 0xAA66FF, 0xBB80FF, 0xCC99FF, 0xDDB3FF,
        // Red
        0x220000, 0x440000, 0x660000, 0x880000, 0xAA0000, 0xCC0000, 0xFF3333, 0xFF4D4D, 0xFF6666, 0xFF8080, 0xFF9999, 0xFFB3B3,
        // Yellow
        0x222200, 0x444400, 0x666600, 0x888800, 0xAAAA00, 0xCCCC00, 0xFFFF33, 0xFFFF4D, 0xFFFF66, 0xFFFF80, 0xFFFF99, 0xFFFFB3,
        // Orange
        0x221100, 0x442200, 0x663300, 0x884400, 0xAA5500, 0xCC6600, 0xFF8833, 0xFF994D, 0xFFAA66, 0xFFBB80, 0xFFCC99, 0xFFDDB3,
        // White to Black Gradient
        0x000000, 0x141414, 0x282828, 0x3C3C3C, 0x505050, 0x646464, 0x8C8C8C, 0xA0A0A0, 0xB4B4B4, 0xC8C8C8, 0xDCDCDC, 0xFFFFFF
    };

    const int num_bright_colors = sizeof(bright_colors) / sizeof(bright_colors[0]);

    switch (selected_setting)
    {

    case 0: // Effect
    if (PAD_justPressed(BTN_RIGHT))
        {
            light->effect = (light->effect % 6) + 1; // Increase effect (1 to 8)
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            light->effect = (light->effect - 2 + 6) % 6 + 1; // Decrease effect (1 to 8)
        }
        break;
    case 1: // Color
    if (PAD_justPressed(BTN_RIGHT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == light->color1)
                {
                    current_index = i;
                    break;
                }
            }
            SDL_Log("saved settings to disk and shm %d", current_index);
            light->color1 = bright_colors[(current_index + 1) % num_bright_colors];
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == light->color1)
                {
                    current_index = i;
                    break;
                }
            }
            light->color1 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    case 2: // Color2
        if (PAD_justPressed(BTN_RIGHT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == light->color2)
                {
                    current_index = i;
                    break;
                }
            }
            SDL_Log("saved settings to disk and shm %d", current_index);
            light->color2 = bright_colors[(current_index + 1) % num_bright_colors];
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            int current_index = -1;
            for (int i = 0; i < num_bright_colors; i++)
            {
                if (bright_colors[i] == light->color2)
                {
                    current_index = i;
                    break;
                }
            }
            light->color2 = bright_colors[(current_index - 1 + num_bright_colors) % num_bright_colors];
        }
        break;
    case 3: // Duration
    if (PAD_justPressed(BTN_RIGHT))
        {
            light->speed = (light->speed + 100) % 5000; // Increase duration
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            light->speed = (light->speed - 100 + 5000) % 5000; // Decrease duration
        }
        break;
    case 4: // Brightness
    if (PAD_justPressed(BTN_RIGHT))
        {
            light->brightness = (light->brightness + 5) % 105; // Increase duration
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            light->brightness = (light->brightness - 5 + 105) % 105; // Decrease duration
        }
        break;
    case 5: // trigger
    if (PAD_justPressed(BTN_RIGHT))
        {
            light->trigger = (light->trigger % NROF_TRIGGERS) + 1; // Increase effect (1 to 8)
        }
        else if (PAD_justPressed(BTN_LEFT))
        {
            light->trigger = (light->trigger - 2 + NROF_TRIGGERS) % NROF_TRIGGERS + 1; // Decrease effect (1 to 8)
        }
        break;
    }

    // Save settings after each change

    
    LEDS_updateLeds();
    LOG_debug("saving testtsttf\n");
    save_settings();
}




int main(int argc, char *argv[])
{
    LOG_debug("Unatetst testtsttf\n");
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




    SDL_Color hex_to_sdl_color(uint32_t hex)
    {
        SDL_Color color;
        color.r = (hex >> 16) & 0xFF;
        color.g = (hex >> 8) & 0xFF;
        color.b = hex & 0xFF;
        color.a = 255;
        return color;
    }

    bool running = true;
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
            handle_light_input(&lights[selected_light],&event, selected_setting);
            dirty = 1;
        }

        if (dirty) {

            GFX_clear(screen);

            int ow = GFX_blitHardwareGroup(screen, show_setting);

            if (show_setting) GFX_blitHardwareHints(screen, show_setting);

            GFX_blitButtonGroup((char*[]){ "B","BACK", NULL }, 1, screen, 1);


            int max_width = screen->w - SCALE1(PADDING * 2) - ow;
            // Display light name
            char light_name_text[256];
            snprintf(light_name_text, sizeof(light_name_text), "%s", lights[selected_light].name);

            char title[256];
            int text_width = GFX_truncateText(font_med, light_name_text, title, max_width, SCALE1(BUTTON_PADDING * 2));
            max_width = MIN(max_width, text_width);

            SDL_Surface *text;
            text = TTF_RenderUTF8_Blended(font_med, title, COLOR_WHITE);
            GFX_blitPill(ASSET_BLACK_PILL, screen, &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)});
            SDL_BlitSurface(text, &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
            SDL_FreeSurface(text);

            // Display settings
            const char *settings_labels[6] = {"Effect", "Color", "Color2", "Speed", "Brightness", "Trigger"};
            int settings_values[6] = {
                lights[selected_light].effect,
                lights[selected_light].color1,
                lights[selected_light].color2,
                lights[selected_light].speed,
                lights[selected_light].brightness,
                lights[selected_light].trigger,
            };

            for (int j = 0; j < 6; ++j)
            {
                char setting_text[256];
                bool selected = (j == selected_setting);
                SDL_Color current_color = selected ? COLOR_BLACK : COLOR_WHITE;

                int y = SCALE1(PADDING + PILL_SIZE * (j + 1));

                if (j == 0)
                { // Display effect name instead of number
                    snprintf(setting_text, sizeof(setting_text), "%s: %s", settings_labels[j], selected_light == 3 ? lr_effect_names[settings_values[j] - 1] : selected_light == 2 ? topbar_effect_names[settings_values[j] - 1] : effect_names[settings_values[j] - 1]);
                    SDL_Surface *text = TTF_RenderUTF8_Blended(font_med, setting_text, current_color);
                    int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
                    GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen,
                                    &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
                    SDL_BlitSurface(text, 
                        &(SDL_Rect){0, 0, text->w, text->h}, screen, 
                        &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
                    SDL_FreeSurface(text);
                }
                else if (j < 3)
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
                else if (j == 5)
                { // Display effect name instead of number
                    snprintf(setting_text, sizeof(setting_text), "%s: %s", settings_labels[j], triggernames[settings_values[j] - 1]);

                    SDL_Surface *text = TTF_RenderUTF8_Blended(font_med, setting_text, current_color);
                    int text_width = text->w + SCALE1(BUTTON_PADDING * 2);
                    GFX_blitPill(selected ? ASSET_WHITE_PILL : ASSET_BLACK_PILL, screen, 
                        &(SDL_Rect){SCALE1(PADDING), y, text_width, SCALE1(PILL_SIZE)});
                    SDL_BlitSurface(text, 
                        &(SDL_Rect){0, 0, text->w, text->h}, screen, 
                        &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), y + SCALE1(4)});
                    SDL_FreeSurface(text);
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

    return 0;
}
