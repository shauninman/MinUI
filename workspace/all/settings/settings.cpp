extern "C"
{
#include "msettings.h"

#include "defines.h"
#include "api.h"
#include "utils.h"
}

#include <map>
#include "menu.hpp"

static int quit = false;

static void sigHandler(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        quit = true;
        break;
    default:
        break;
    }
}

struct Context
{
    MenuList *menu;
    SDL_Surface *screen;
    int dirty;
    int show_setting;
};

// This is all the MinUiSettings stuff, for now just copied over from the old settings app

static const std::vector<std::any> colors = {
    0x000022U, 0x000044U, 0x000066U, 0x000088U, 0x0000AAU, 0x0000CCU, 0x1e2329U, 0x3366FFU, 0x4D7AFFU, 0x6699FFU, 0x80B3FFU, 0x99CCFFU, 0xB3D9FFU,
    0x002222U, 0x004444U, 0x006666U, 0x008888U, 0x00AAAAU, 0x00CCCCU, 0x33FFFFU, 0x4DFFFFU, 0x66FFFFU, 0x80FFFFU, 0x99FFFFU, 0xB3FFFFU,
    0x002200U, 0x004400U, 0x006600U, 0x008800U, 0x00AA00U, 0x00CC00U, 0x33FF33U, 0x4DFF4DU, 0x66FF66U, 0x80FF80U, 0x99FF99U, 0xB3FFB3U,
    0x220022U, 0x440044U, 0x660066U, 0x880088U, 0x9B2257U, 0xAA00AAU, 0xCC00CCU, 0xFF33FFU, 0xFF4DFFU, 0xFF66FFU, 0xFF80FFU, 0xFF99FFU, 0xFFB3FFU,
    0x110022U, 0x220044U, 0x330066U, 0x440088U, 0x5500AAU, 0x6600CCU, 0x8833FFU, 0x994DFFU, 0xAA66FFU, 0xBB80FFU, 0xCC99FFU, 0xDDB3FFU,
    0x220000U, 0x440000U, 0x660000U, 0x880000U, 0xAA0000U, 0xCC0000U, 0xFF3333U, 0xFF4D4DU, 0xFF6666U, 0xFF8080U, 0xFF9999U, 0xFFB3B3U,
    0x222200U, 0x444400U, 0x666600U, 0x888800U, 0xAAAA00U, 0xCCCC00U, 0xFFFF33U, 0xFFFF4DU, 0xFFFF66U, 0xFFFF80U, 0xFFFF99U, 0xFFFFB3U,
    0x221100U, 0x442200U, 0x663300U, 0x884400U, 0xAA5500U, 0xCC6600U, 0xFF8833U, 0xFF994DU, 0xFFAA66U, 0xFFBB80U, 0xFFCC99U, 0xFFDDB3U,
    0x000000U, 0x141414U, 0x282828U, 0x3C3C3CU, 0x505050U, 0x646464U, 0x8C8C8CU, 0xA0A0A0U, 0xB4B4B4U, 0xC8C8C8U, 0xDCDCDCU, 0xFFFFFFU};
// all colors above but as strings
static const std::vector<std::string> color_strings = {
    "0x000022", "0x000044", "0x000066", "0x000088", "0x0000AA", "0x0000CC", "0x1E2329", "0x3366FF", "0x4D7AFF", "0x6699FF", "0x80B3FF", "0x99CCFF", "0xB3D9FF",
    "0x002222", "0x004444", "0x006666", "0x008888", "0x00AAAA", "0x00CCCC", "0x33FFFF", "0x4DFFFF", "0x66FFFF", "0x80FFFF", "0x99FFFF", "0xB3FFFF",
    "0x002200", "0x004400", "0x006600", "0x008800", "0x00AA00", "0x00CC00", "0x33FF33", "0x4DFF4D", "0x66FF66", "0x80FF80", "0x99FF99", "0xB3FFB3",
    "0x220022", "0x440044", "0x660066", "0x880088", "0x9B2257", "0xAA00AA", "0xCC00CC", "0xFF33FF", "0xFF4DFF", "0xFF66FF", "0xFF80FF", "0xFF99FF", "0xFFB3FF",
    "0x110022", "0x220044", "0x330066", "0x440088", "0x5500AA", "0x6600CC", "0x8833FF", "0x994DFF", "0xAA66FF", "0xBB80FF", "0xCC99FF", "0xDDB3FF",
    "0x220000", "0x440000", "0x660000", "0x880000", "0xAA0000", "0xCC0000", "0xFF3333", "0xFF4D4D", "0xFF6666", "0xFF8080", "0xFF9999", "0xFFB3B3",
    "0x222200", "0x444400", "0x666600", "0x888800", "0xAAAA00", "0xCCCC00", "0xFFFF33", "0xFFFF4D", "0xFFFF66", "0xFFFF80", "0xFFFF99", "0xFFFFB3",
    "0x221100", "0x442200", "0x663300", "0x884400", "0xAA5500", "0xCC6600", "0xFF8833", "0xFF994D", "0xFFAA66", "0xFFBB80", "0xFFCC99", "0xFFDDB3",
    "0x000000", "0x141414", "0x282828", "0x3C3C3C", "0x505050", "0x646464", "0x8C8C8C", "0xA0A0A0", "0xB4B4B4", "0xC8C8C8", "0xDCDCDC", "0xFFFFFF"};

static const std::vector<std::string> font_names = {"OG", "Next"};

static const std::vector<std::any> timeout_secs = {0U, 5U, 10U, 15U, 30U, 45U, 60U, 90U, 120U, 240U, 360U, 600U};
static const std::vector<std::string> timeout_labels = {"Never", "5s", "10s", "15s", "30s", "45s", "60s", "90s", "2m", "4m", "6m", "10m"};

static const std::vector<std::string> on_off = {"Off", "On"};

static const std::vector<std::string> scaling_strings = {"Fullscreen", "Fit", "Fill"};
static const std::vector<std::any> scaling = {(int)GFX_SCALE_FULLSCREEN, (int)GFX_SCALE_FIT, (int)GFX_SCALE_FILL};

int main(int argc, char *argv[])
{
    try
    {
        InitSettings();

        PWR_setCPUSpeed(CPU_SPEED_MENU);

        Context ctx = {0};
        ctx.dirty = 1;
        ctx.show_setting = 0;
        ctx.screen = GFX_init(MODE_MAIN);
        PAD_init();
        PWR_init();

        TIME_init();

        signal(SIGINT, sigHandler);
        signal(SIGTERM, sigHandler);

        char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH];
        int tz_count = 0;
        TIME_getTimezones(timezones, &tz_count);
        
        std::vector<std::any> tz_values;
        std::vector<std::string> tz_labels;
        for (int i = 0; i < tz_count; ++i) {
            //LOG_info("Timezone: %s\n", timezones[i]);
            tz_values.push_back(std::string(timezones[i]));
            // Todo: beautify, remove underscores and so on
            tz_labels.push_back(std::string(timezones[i]));
        }

        auto appearanceMenu = new MenuList(MenuItemType::Fixed, "Appearance",
        {
            MenuItem{Generic, "Font", "The font to render all UI text.", {0, 1}, font_names, []() -> std::any
                    { return CFG_getFontId(); },
                    [](const std::any &value)
                    {
                        CFG_setFontId(std::any_cast<int>(value));
                    }},
            MenuItem{Color, "Main Color", "The color used to render main UI elements.", colors, color_strings, []() -> std::any
                    { return CFG_getColor(1); }, [](const std::any &value)
                    { CFG_setColor(1, std::any_cast<uint32_t>(value)); }},
            MenuItem{Color, "Primary Accent Color", "The color used to highlight important things in the user interface.", colors, color_strings, []() -> std::any
                    { return CFG_getColor(2); }, [](const std::any &value)
                    { CFG_setColor(2, std::any_cast<uint32_t>(value)); }},
            MenuItem{Color, "Secondary Accent Color", "A secondary highlight color.", colors, color_strings, []() -> std::any
                    { return CFG_getColor(3); }, [](const std::any &value)
                    { CFG_setColor(3, std::any_cast<uint32_t>(value)); }},
            MenuItem{Color, "Hint info Color", "Color for button hints and info", colors, color_strings, []() -> std::any
                    { return CFG_getColor(6); }, [](const std::any &value)
                    { CFG_setColor(6, std::any_cast<uint32_t>(value)); }},
            MenuItem{Color, "List Text", "List text color", colors, color_strings, []() -> std::any
                    { return CFG_getColor(4); }, [](const std::any &value)
                    { CFG_setColor(4, std::any_cast<uint32_t>(value)); }},
            MenuItem{Color, "List Text Selected", "List selected text color", colors, color_strings, []() -> std::any
                    { return CFG_getColor(5); }, [](const std::any &value)
                    { CFG_setColor(5, std::any_cast<uint32_t>(value)); }},
            MenuItem{Generic, "Brightness", "Display brightness (0-10)", 0, 10, []() -> std::any
                    { return GetBrightness(); }, [](const std::any &value)
                    { SetBrightness(std::any_cast<int>(value)); }},
            MenuItem{Generic, "Volume", "Speaker volume (0-20)", 0, 20, []() -> std::any
                    { return GetVolume(); }, [](const std::any &value)
                    { SetVolume(std::any_cast<int>(value)); }},
            MenuItem{Generic, "Color temperature", "Color temperature (0-40)", 0, 40, []() -> std::any
                    { return GetColortemp(); }, [](const std::any &value)
                    { SetColortemp(std::any_cast<int>(value)); }},
            MenuItem{Generic, "Show battery percentage", "Show battery level as percent in the status pill", {false, true}, on_off, []() -> std::any
                    { return CFG_getShowBatteryPercent(); },
                    [](const std::any &value)
                    { CFG_setShowBatteryPercent(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Show menu animations", "Enable or disable menu animations", {false, true}, on_off, []() -> std::any
                    { return CFG_getMenuAnimations(); },
                    [](const std::any &value)
                    { CFG_setMenuAnimations(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Thumb radius", "Set the thumb radius for the status pill", 0, 24, []() -> std::any
                    { return CFG_getThumbnailRadius(); }, [](const std::any &value)
                    { CFG_setThumbnailRadius(std::any_cast<int>(value)); }},
            MenuItem{Generic, "Show recents", "Show \"Recently Played\" menu entry.\nThis also disables Game Switcher.", {false, true}, on_off, []() -> std::any
                    { return CFG_getShowRecents(); },
                    [](const std::any &value)
                    { CFG_setShowRecents(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Show game art", "Show game artwork in the main menu", {false, true}, on_off, []() -> std::any
                    { return CFG_getShowGameArt(); },
                    [](const std::any &value)
                    { CFG_setShowGameArt(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Use folder background for ROMs", "If enabled, used the emulator background image. Otherwise uses the default.", {false, true}, on_off, []() -> std::any
                    { return CFG_getRomsUseFolderBackground(); },
                    [](const std::any &value)
                    { CFG_setRomsUseFolderBackground(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Game switcher scaling", "The scaling algorithm used to display the savegame image.", scaling, scaling_strings, []() -> std::any
                    { return CFG_getGameSwitcherScaling(); },
                    [](const std::any &value)
                    { CFG_setGameSwitcherScaling(std::any_cast<int>(value)); }},
        });

        auto systemMenu = new MenuList(MenuItemType::Fixed, "System",
        {
            MenuItem{Generic, "Screen timeout", "Time before screen turns off (0-600s)", timeout_secs, timeout_labels, []() -> std::any
                    { return CFG_getScreenTimeoutSecs(); }, [](const std::any &value)
                    { CFG_setScreenTimeoutSecs(std::any_cast<uint32_t>(value)); }},
            MenuItem{Generic, "Suspend timeout", "Time before device goes to sleep (0-600s)", timeout_secs, timeout_labels, []() -> std::any
                    { return CFG_getSuspendTimeoutSecs(); }, [](const std::any &value)
                    { CFG_setSuspendTimeoutSecs(std::any_cast<uint32_t>(value)); }},
            MenuItem{Generic, "Haptic feedback", "Enable or disable haptic feedback on certain actions in the OS", {false, true}, on_off, []() -> std::any
                    { return CFG_getHaptics(); }, [](const std::any &value)
                    { CFG_setHaptics(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Show 24h time format", "Show clock in the 24hrs time format", {false, true}, on_off, []() -> std::any
                    { return CFG_getClock24H(); },
                    [](const std::any &value)
                    { CFG_setClock24H(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Show clock", "Show clock in the status pill", {false, true}, on_off, []() -> std::any
                    { return CFG_getShowClock(); },
                    [](const std::any &value)
                    { CFG_setShowClock(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Set time and date automatically", "Automatically adjust system time\nwith NTP (requires internet access)", {false, true}, on_off, []() -> std::any
                    { return TIME_getNetworkTimeSync(); }, [](const std::any &value)
                    { TIME_setNetworkTimeSync(std::any_cast<bool>(value)); }},
            MenuItem{Generic, "Time zone", "Your time zone", tz_values, tz_labels, []() -> std::any
                    { return std::string(TIME_getCurrentTimezone()); }, [](const std::any &value)
                    { TIME_setCurrentTimezone(std::any_cast<std::string>(value).c_str()); }},
            MenuItem{Generic, "Save format", "The save format to use.", {(int)SAVE_FORMAT_SAV, (int)SAVE_FORMAT_SRM}, {".sav", ".srm"}, []() -> std::any
                    { return CFG_getSaveFormat(); }, [](const std::any &value)
                    { CFG_setSaveFormat(std::any_cast<int>(value)); }},
        });

        ctx.menu = new MenuList(MenuItemType::List, "Main",
        {
            MenuItem{Generic, "Appearance", "UI customization", {}, {}, nullptr, nullptr, DeferToSubmenu, appearanceMenu},
            MenuItem{Generic, "System", "", {}, {}, nullptr, nullptr, DeferToSubmenu, systemMenu},
        });

        const bool showTitle = false;
        const bool showIndicator = true;
        const bool showHints = false;

        SDL_Surface* bgbmp = IMG_Load(SDCARD_PATH "/bg.png");
        SDL_Surface* convertedbg = SDL_ConvertSurfaceFormat(bgbmp, SDL_PIXELFORMAT_RGB565, 0);
        if (convertedbg) {
            SDL_FreeSurface(bgbmp); 
            SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, ctx.screen->w, ctx.screen->h, 32, SDL_PIXELFORMAT_RGB565);
            GFX_blitScaleToFill(convertedbg, scaled);
            bgbmp = scaled;
        }

        // main content (list)
        // PADDING all around
        SDL_Rect listRect = {SCALE1(PADDING), SCALE1(PADDING), ctx.screen->w - SCALE1(PADDING * 2), ctx.screen->h - SCALE1(PADDING * 2)};
        // PILL_SIZE above (if showing title)
        if (showTitle || showIndicator)
            listRect = dy(listRect, SCALE1(PILL_SIZE));
        // BUTTON_SIZE below (if showing hints)
        if (showHints)
            listRect.h -= SCALE1(BUTTON_SIZE);
        ctx.menu->performLayout(listRect);

        while (!quit)
        {
            GFX_startFrame();
            uint32_t now = SDL_GetTicks();
            PAD_poll();

            bool handled = ctx.menu->handleInput(ctx.dirty, quit);

            PWR_update(&ctx.dirty, &ctx.show_setting, nullptr, nullptr);

            if (ctx.dirty)
            {
                GFX_clear(ctx.screen);
                if(bgbmp) {
                    SDL_Rect image_rect = {0, 0, ctx.screen->w, ctx.screen->h};
                    SDL_BlitSurface(bgbmp, NULL, ctx.screen, &image_rect);
                }

                int ow = 0;

                // indicator area top right
                if (showIndicator)
                {
                    ow = GFX_blitHardwareGroup(ctx.screen, ctx.show_setting);
                }
                int max_width = ctx.screen->w - SCALE1(PADDING * 2) - ow;

                // title pill
                if (showTitle)
                {
                    char display_name[256];
                    int text_width = GFX_truncateText(font.large, "Some title", display_name, max_width, SCALE1(BUTTON_PADDING * 2));
                    max_width = MIN(max_width, text_width);

                    SDL_Surface *text;
                    text = TTF_RenderUTF8_Blended(font.large, display_name, COLOR_WHITE);
                    SDL_Rect target = {SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)};
                    GFX_blitPillLight(ASSET_WHITE_PILL, ctx.screen, &target);
                    SDL_BlitSurfaceCPP(text, {0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, ctx.screen, {SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                    SDL_FreeSurface(text);
                }

                // bottom area, button hints
                if (showHints)
                {
                    if (ctx.show_setting && !GetHDMI())
                        GFX_blitHardwareHints(ctx.screen, ctx.show_setting);
                    else
                    {
                        char *hints[] = {(char *)("MENU"), (char *)("SLEEP"), NULL};
                        GFX_blitButtonGroup(hints, 0, ctx.screen, 0);
                    }
                    char *hints[] = {(char *)("B"), (char *)("BACK"), (char *)("A"), (char *)("OKAY"), NULL};
                    GFX_blitButtonGroup(hints, 1, ctx.screen, 1);
                }

                ctx.menu->draw(ctx.screen, listRect);

                // present
                GFX_flip(ctx.screen);
                ctx.dirty = false;

                // hdmimon();
            }
        }

        delete ctx.menu;
        delete appearanceMenu;
        delete systemMenu;
        ctx.menu = NULL;

        QuitSettings();
        PWR_quit();
        PAD_quit();
        GFX_quit();

        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        LOG_error("%s", e.what());
        QuitSettings();
        PWR_quit();
        PAD_quit();
        GFX_quit();

        return EXIT_FAILURE;
    }
}