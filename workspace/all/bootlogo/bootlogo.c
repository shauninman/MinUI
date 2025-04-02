#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

static bool quit = false;

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

static SDL_Surface *screen;

SDL_Surface** images;
char **image_paths;
static int selected = 0;
static int count = 0;

int loadImages()
{
    char* device = getenv("DEVICE");
    // This needs to get a bit more flexible down the line, but for now we either expect the files
    // in the pak root directory or in the "brick" subfolder.
    char basepath[MAX_PATH];
    if(exactMatch("brick", device)) {
        snprintf(basepath, sizeof(basepath), "%s/Bootlogo.pak/brick/", TOOLS_PATH);
    }
    else {
        snprintf(basepath, sizeof(basepath), "%s/Bootlogo.pak/smartpro/", TOOLS_PATH);
    }

    // grab all bmp files in the directory and load them with IMG_Load, 
    // keep them in an array of SDL_Surface pointers
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(basepath)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strstr(ent->d_name, ".bmp") != NULL) {
                char path[MAX_PATH];
                snprintf(path, sizeof(path), "%s%s", basepath, ent->d_name);
                SDL_Surface *bmp = IMG_Load(path);
                if (bmp) {
                    count++;
                    images = realloc(images, sizeof(SDL_Surface*) * count);
                    images[count-1] = bmp;
                    image_paths = realloc(image_paths, sizeof(char*) * count);
                    image_paths[count-1] = strdup(path);
                }
            }
        }
        closedir(dir);
    } else {
        // could not open directory
        LOG_error("could not open directory");
        if (CFG_getHaptics()) {
            VIB_triplePulse(5, 150, 200);
        }
        return 0;
    }
    return count;
}

void unloadImages()
{
    for (int i = 0; i < count; i++)
    {
        SDL_FreeSurface(images[i]);
    }
    free(images);
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

    loadImages();

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
        else
        {
            if (PAD_justRepeated(BTN_LEFT))
            {
                selected -= 1;
                if (selected<0)
                    selected = count - 1;
                dirty = 1;
            }
            else if (PAD_justRepeated(BTN_RIGHT))
            {
                selected += 1;
                if (selected>=count)
                    selected = 0;
                dirty = 1;
            }
            else if (PAD_justPressed(BTN_A))
            {
                // apply with system calls
                // BOOT_PATH=/mnt/boot/
                // mkdir -p $BOOT_PATH
                // mount -t vfat /dev/mmcblk0p1 $BOOT_PATH
                // cp $LOGO_PATH $BOOT_PATH
                // sync
                // umount $BOOT_PATH
                // reboot
                char* boot_path = "/mnt/boot/";
                char* logo_path = image_paths[selected];
                char cmd[256]; 
                snprintf(cmd, sizeof(cmd), "mkdir -p %s && mount -t vfat /dev/mmcblk0p1 %s && cp \"%s\" %s/bootlogo.bmp && sync && umount %s && reboot", boot_path, boot_path, logo_path, boot_path, boot_path);
                system(cmd);
            }
            else if (PAD_justPressed(BTN_B))
            {
                quit = 1;
            }
        }

        PWR_update(&dirty, NULL, NULL, NULL);

        int is_online = PLAT_isOnline();
        if (was_online != is_online)
            dirty = 1;
        was_online = is_online;

        if (dirty)
        {
            GFX_clear(screen);

            if(count > 0) {
                // render the selected image, centered on screen
                SDL_Surface *image = images[selected];
                SDL_Rect image_rect = {
                    screen->w /2 - image->w /2,
                    screen->h /2 - image->h / 2,
                    image->w,
                    image->h};
                SDL_BlitSurface(image, NULL, screen, &image_rect);
            }

            GFX_blitButtonGroup((char *[]){"L/R", "SCROLL", NULL}, 0, screen, 0);
            GFX_blitButtonGroup((char *[]){"A", "SET", "B", "BACK", NULL}, 1, screen, 1);

            GFX_flip(screen);
            dirty = 0;
        }
        else
            GFX_sync();
    }

    unloadImages();

    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();

    return EXIT_SUCCESS;
}