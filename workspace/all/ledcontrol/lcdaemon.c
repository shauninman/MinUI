#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <SDL2/SDL.h>

#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#define MAX_LIGHTS 10
#define MAX_NAME_LEN 50

typedef struct
{
    char name[MAX_NAME_LEN];
    int effect;
    int last_effect;
    int duration;
    int brightness;
    uint32_t color;
    uint32_t color2;
    bool updated;
    int current_r;
    int current_g;
    int current_b;
    float progress;
    int colorarray[10];
    int trigger;
    int running;

} LightSettings;

bool first_run = true;
bool pressed = false;
int last_pressed = 0;

float progress = 0.0f;

int current_r;
int current_g;
int current_b;

volatile sig_atomic_t running = 1;

int jsopen = 0; // Flag to keep track of whether the file is open

void chmodfile(const char *file, int writable)
{
    struct stat statbuf;
    if (stat(file, &statbuf) == 0)
    {
        mode_t newMode;
        if (writable)
        {
            // Add write permissions for all users
            newMode = statbuf.st_mode | S_IWUSR | S_IWGRP | S_IWOTH;
        }
        else
        {
            // Remove write permissions for all users
            newMode = statbuf.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
        }

        // Apply the new permissions
        if (chmod(file, newMode) != 0)
        {
            printf("chmod error %d %s", writable, file);
        }
    }
    else
    {
        printf("stat error %d %s", writable, file);
    }
}

void changePermissions(const char *path, int writable)
{
    DIR *dir;
    struct dirent *entry;

    // Open the directory
    if ((dir = opendir(path)) != NULL)
    {
        while ((entry = readdir(dir)) != NULL)
        {
            // Skip "." and ".." entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            {
                continue;
            }

            // Construct new path
            char newPath[1024];
            snprintf(newPath, sizeof(newPath), "%s/%s", path, entry->d_name);

            // Get current permissions
            struct stat statbuf;
            if (stat(newPath, &statbuf) == 0)
            {
                mode_t newMode;
                if (writable)
                {
                    // Add write permissions for all users
                    newMode = statbuf.st_mode | S_IWUSR | S_IWGRP | S_IWOTH;
                }
                else
                {
                    // Remove write permissions for all users
                    newMode = statbuf.st_mode & ~(S_IWUSR | S_IWGRP | S_IWOTH);
                }

                // Apply the new permissions
                if (chmod(newPath, newMode) != 0)
                {
                    printf("error chmod\n");
                }
            }
            else
            {
                printf("error stat\n");
            }
        }
        closedir(dir);
    }
    else
    {
        perror("opendir");
    }
}

void changebrightness(const char *dir, const LightSettings *lights)
{
    char filepath[256];
    FILE *file;
    // first set brightness
    snprintf(filepath, sizeof(filepath), "%s/max_scale", dir);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", lights[2].brightness);
        fclose(file);
    }
    chmodfile(filepath, 0);
    snprintf(filepath, sizeof(filepath), "%s/max_scale_f1f2", dir);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", lights[0].brightness);
        fclose(file);
    }
    chmodfile(filepath, 0);
    snprintf(filepath, sizeof(filepath), "%s/max_scale_lr", dir);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", lights[3].brightness);
        fclose(file);
    }
    chmodfile(filepath, 0);
}

void handle_sigterm(int sig)
{
    running = 0;
}

void handle_sigcont(int sig)
{
    changePermissions("/sys/class/led_anim", 0);
    first_run = true;
}
void handle_sigsleep()
{
    changePermissions("/sys/class/led_anim", 1);
}

int read_settings(const char *filename, LightSettings *lights, int max_lights)
{
    FILE *file;
    if (first_run == 1)
    {
        // first run read settings from disk and write to shm
        printf("First run\n");
        char diskfilename[256];
        snprintf(diskfilename, sizeof(diskfilename), "/etc/LedControl/%s", filename);
        FILE *diskfile = fopen(diskfilename, "r");
        if (diskfile == NULL)
        {
            perror("Unable to open settings file");
            return 1;
        }

        char shmfile[256];
        snprintf(shmfile, sizeof(shmfile), "/dev/shm/%s", filename);
        file = fopen(shmfile, "w");
        if (file == NULL)
        {
            perror("Unable to open /dev/shm/ file");
            return 1;
        }

        char buffer[1024];
        size_t bytesRead;
        while ((bytesRead = fread(buffer, 1, sizeof(buffer), diskfile)) > 0)
        {
            fwrite(buffer, 1, bytesRead, file);
        }

        printf("File contents copied to /dev/shm/%s\n", filename);
        fclose(file);
        fclose(diskfile);
    }

    char shmfile[256];
    snprintf(shmfile, sizeof(shmfile), "/dev/shm/%s", filename);
    file = fopen(shmfile, "r");
    if (file == NULL)
    {
        perror("Unable to open /dev/shm/ file");
        fclose(file);
        return 1;
    }

    char line[256];
    int current_light = -1;
    while (fgets(line, sizeof(line), file))
    {
        if (line[0] == '[')
        {
            // Section header
            char light_name[MAX_NAME_LEN];
            if (sscanf(line, "[%49[^]]]", light_name) == 1)
            {
                current_light++;
                if (current_light < max_lights)
                {
                    strncpy(lights[current_light].name, light_name, MAX_NAME_LEN - 1);
                    lights[current_light].name[MAX_NAME_LEN - 1] = '\0'; // Ensure null-termination
                    lights[current_light].updated = false;               // Initialize updated flag
                }
                else
                {
                    current_light = -1; // Reset if max_lights exceeded
                }
            }
        }
        else if (current_light >= 0 && current_light < max_lights)
        {
            int temp_value;
            uint32_t temp_color;

            if (sscanf(line, "effect=%d", &temp_value) == 1)
            {
                if (lights[current_light].effect != temp_value)
                {
                    printf("effect changed\n");
                    lights[current_light].effect = temp_value;
                    lights[current_light].updated = true;
                }
                continue;
            }
            if (sscanf(line, "color=%x", &temp_color) == 1)
            {
                if (lights[current_light].color != temp_color)
                {
                    lights[current_light].color = temp_color;
                    lights[current_light].updated = true;
                }
                continue;
            }
            if (sscanf(line, "color2=%x", &temp_color) == 1)
            {
                if (lights[current_light].color2 != temp_color)
                {
                    lights[current_light].color2 = temp_color;
                    lights[current_light].updated = true;
                }
                continue;
            }
            if (sscanf(line, "duration=%d", &temp_value) == 1)
            {
                if (lights[current_light].duration != temp_value)
                {
                    lights[current_light].duration = temp_value;
                    lights[current_light].updated = true;
                }
                continue;
            }
            if (sscanf(line, "brightness=%d", &temp_value) == 1)
            {
                if (lights[current_light].brightness != temp_value)
                {
                    lights[current_light].brightness = temp_value;
                    lights[current_light].updated = true;
                }
                continue;
            }
            if (sscanf(line, "trigger=%d", &temp_value) == 1)
            {
                if (lights[current_light].trigger != temp_value)
                {
                    lights[current_light].trigger = temp_value;
                    lights[current_light].updated = true;
                }
                continue;
            }
        }
    }

    fclose(file);
    return 0;
}

// Function to convert integer hex code to SDL_Color
SDL_Color HexIntToColor(unsigned int hexValue)
{
    SDL_Color color = {0, 0, 0, 255}; // Default to black with full opacity

    // Extract RGB values from the integer hex code
    color.r = (hexValue >> 16) & 0xFF;
    color.g = (hexValue >> 8) & 0xFF;
    color.b = hexValue & 0xFF;

    return color;
}

void HSVtoRGB(float h, float s, float v, int *r, int *g, int *b)
{
    int i = floor(h / 60);
    float f = h / 60 - i;
    float p = v * (1 - s);
    float q = v * (1 - s * f);
    float t = v * (1 - s * (1 - f));
    switch (i)
    {
    case 0:
        *r = v * 255;
        *g = t * 255;
        *b = p * 255;
        break;
    case 1:
        *r = q * 255;
        *g = v * 255;
        *b = p * 255;
        break;
    case 2:
        *r = p * 255;
        *g = v * 255;
        *b = t * 255;
        break;
    case 3:
        *r = p * 255;
        *g = q * 255;
        *b = v * 255;
        break;
    case 4:
        *r = t * 255;
        *g = p * 255;
        *b = v * 255;
        break;
    default:
        *r = v * 255;
        *g = p * 255;
        *b = q * 255;
        break;
    }
}
void CycleBetweenTwoColors(float progress, int r1, int g1, int b1, int r2, int g2, int b2, int *r, int *g, int *b)
{
    *r = r1 + (r2 - r1) * progress;
    *g = g1 + (g2 - g1) * progress;
    *b = b1 + (b2 - b1) * progress;
}
void PulseEffect(float progress, int base_r, int base_g, int base_b, int *r, int *g, int *b)
{
    float factor = (sin(progress * 2 * M_PI) + 1) / 2; // Create a sine wave effect
    *r = base_r * factor;
    *g = base_g * factor;
    *b = base_b * factor;
}
void GradientShift(float progress, int r1, int g1, int b1, int r2, int g2, int b2, int r3, int g3, int b3, int *r, int *g, int *b)
{
    float section = fmod(progress * 3.0, 3.0);
    if (section < 1)
    {
        CycleBetweenTwoColors(section, r1, g1, b1, r2, g2, b2, r, g, b);
    }
    else if (section < 2)
    {
        CycleBetweenTwoColors(section - 1, r2, g2, b2, r3, g3, b3, r, g, b);
    }
    else
    {
        CycleBetweenTwoColors(section - 2, r3, g3, b3, r1, g1, b1, r, g, b);
    }
}

void TwinkleEffect(float progress, int base_r, int base_g, int base_b, int *r, int *g, int *b)
{
    float factor = ((rand() % 100) / 100.0f) * (sin(progress * M_PI * 2) + 1) / 2;
    *r = base_r * factor;
    *g = base_g * factor;
    *b = base_b * factor;
}

void FireEffect(float progress, int *r, int *g, int *b)
{
    float section = fmod(progress * 3.0, 3.0);
    if (section < 1)
    {
        CycleBetweenTwoColors(section, 255, 0, 0, 255, 165, 0, r, g, b); // Red to Orange
    }
    else if (section < 2)
    {
        CycleBetweenTwoColors(section - 1, 255, 165, 0, 255, 255, 0, r, g, b); // Orange to Yellow
    }
    else
    {
        CycleBetweenTwoColors(section - 2, 255, 255, 0, 255, 0, 0, r, g, b); // Yellow to Red
    }
}

void GlitterEffect(float progress, int base_r, int base_g, int base_b, int *r, int *g, int *b)
{
    float factor = ((rand() % 100) / 100.0f) * (sin(progress * M_PI * 2) + 1) / 2;
    if (rand() % 2)
    {
        *r = base_r * factor;
        *g = base_g * factor;
        *b = base_b * factor;
    }
    else
    {
        *r = base_r;
        *g = base_g;
        *b = base_b;
    }
}

void NeonGlowEffect(float progress, int base_r, int base_g, int base_b, int *r, int *g, int *b)
{
    float factor = (sin(progress * M_PI * 2) + 1) / 2;
    *r = base_r * factor;
    *g = base_g * factor;
    *b = base_b * factor;
}

void FireflyEffect(float progress, int base_r, int base_g, int base_b, int *r, int *g, int *b)
{
    float factor = ((rand() % 100) / 100.0f) * (sin(progress * M_PI * 2) + 1) / 2;
    if (rand() % 2)
    {
        *r = base_r * factor;
        *g = base_g * factor;
        *b = base_b * factor;
    }
    else
    {
        *r = 0;
        *g = 0;
        *b = 0;
    }
}

void AuroraEffect(float progress, int *r, int *g, int *b)
{
    float section = fmod(progress * 2.0, 2.0);
    if (section < 1)
    {
        CycleBetweenTwoColors(section, 0, 255, 128, 0, 255, 255, r, g, b); // Green to Cyan
    }
    else
    {
        CycleBetweenTwoColors(section - 1, 0, 255, 255, 0, 128, 255, r, g, b); // Cyan to Blue
    }
}

void ColorWave(float progress, int *r, int *g, int *b)
{
    float h = fmod(progress * 360.0, 360.0);
    float s = 1.0, v = 1.0; // Saturation and brightness are constant
    HSVtoRGB(h, s, v, r, g, b);
}

void FadeToBlack(int *r, int *g, int *b, float fadeAmount)
{

    fadeAmount = fadeAmount * 5.0f;
    if (fadeAmount < 0)
        fadeAmount = 0;
    if (fadeAmount > 1)
        fadeAmount = 1;

    // Calculate the faded RGB values based on fadeAmount
    *r = *r * (1 - fadeAmount);
    *g = *g * (1 - fadeAmount);
    *b = *b * (1 - fadeAmount);
}

float mapSpeedToProgress(int speed)
{
    float progress;
    if (speed <= 500)
    {
        // Map speed from 0 to 1000 to progress from 0.05 to 0.01
        float maxSpeedSegment1 = 500.0f;
        float minSpeedSegment1 = 0.0f;
        float maxProgressSegment1 = 1.1f;
        float minProgressSegment1 = 0.1f;

        progress = maxProgressSegment1 - ((speed - minSpeedSegment1) / (maxSpeedSegment1 - minSpeedSegment1)) * (maxProgressSegment1 - minProgressSegment1);
    }
    else if (speed <= 1000)
    {
        // Map speed from 0 to 1000 to progress from 0.05 to 0.01
        float maxSpeedSegment1 = 1000.0f;
        float minSpeedSegment1 = 500.0f;
        float maxProgressSegment1 = 0.1f;
        float minProgressSegment1 = 0.01f;

        progress = maxProgressSegment1 - ((speed - minSpeedSegment1) / (maxSpeedSegment1 - minSpeedSegment1)) * (maxProgressSegment1 - minProgressSegment1);
    }
    else if (speed <= 4900)
    {
        // Map speed from 1000 to 4900 to progress from 0.01 to 0.001
        float maxSpeedSegment2 = 4900.0f;
        float minSpeedSegment2 = 1000.0f;
        float maxProgressSegment2 = 0.01f;
        float minProgressSegment2 = 0.001f;

        progress = maxProgressSegment2 - ((speed - minSpeedSegment2) / (maxSpeedSegment2 - minSpeedSegment2)) * (maxProgressSegment2 - minProgressSegment2);
    }
    else
    {
        // Handle out-of-range values gracefully
        progress = 0.001f; // Assuming the slowest value if speed > 4900
    }

    return progress;
}

void shiftColors(int colors[], int size)
{
    int last = colors[size - 1];
    for (int i = size - 1; i > 0; i--)
    {
        colors[i] = colors[i - 1];
    }
    colors[0] = last;
}

float wastriggered = 0.0f;
void update_light_settings(LightSettings *light, const char *dir)
{
    char filepath[256];
    char filepath2[256];
    FILE *file;
    FILE *file2;
    light->progress += mapSpeedToProgress(light->duration);

    if (light->progress > 1.0f)
        light->progress = 0.0f;

    // Update effect and other settings
    snprintf(filepath, sizeof(filepath), "%s/effect_rgb_hex_%s", dir, light->name);
    snprintf(filepath2, sizeof(filepath2), "%s/frame_hex", dir, light->name);

    chmodfile(filepath, 1);
    chmodfile(filepath2, 1);
    file = fopen(filepath, "w");
    file2 = fopen(filepath2, "w");

    if (file != NULL && file2 != NULL)
    {
        SDL_Color tempcolor = HexIntToColor(light->color);
        int r, g, b;
        if (light->effect == 8)
        {
            ColorWave(light->progress, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }

        else if (light->effect == 9)
        {
            TwinkleEffect(light->progress, tempcolor.r, tempcolor.g, tempcolor.b, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 10)
        {
            FireEffect(light->progress, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 11)
        {
            GlitterEffect(light->progress, tempcolor.r, tempcolor.g, tempcolor.b, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 12)
        {
            NeonGlowEffect(light->progress, tempcolor.r, tempcolor.g, tempcolor.b, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 13)
        {
            FireflyEffect(light->progress, tempcolor.r, tempcolor.g, tempcolor.b, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 14)
        {
            AuroraEffect(light->progress, &r, &g, &b);
            fprintf(file, "%02X%02X%02X\n", r, g, b);
        }
        else if (light->effect == 15)
        {
            printf("pressed: %d, trigger setting: %d\n", pressed, light->trigger);
            if (pressed)
            {
                int doit = 0;
                if (light->trigger == 12 || last_pressed == light->trigger - 1)
                {
                    doit = 1;
                }
                if (light->trigger == 13 && (last_pressed == 4 || last_pressed == 5))
                {
                    doit = 1;
                }
                if (light->trigger == 14 && last_pressed == 100)
                {
                    doit = 1;
                }
                if (doit == 1)
                {
                    light->current_r = light->color >> 16 & 0xFF;
                    light->current_g = light->color >> 8 & 0xFF;
                    light->current_b = light->color & 0xFF;
                    light->progress = 0.0f;
                    light->running = 1;
                    fprintf(file, "%06X\n", light->color);
                }
            }
            else
            {
                if (light->duration > 0 && light->running > 0)
                {
                    const int colorr = light->color2 >> 16 & 0xFF;
                    const int colorg = light->color2 >> 8 & 0xFF;
                    const int colorb = light->color2 & 0xFF;
                    const float speed = (5000 / light->duration) * 1;
                    // FadeToBlack(&light->current_r, &light->current_g, &light->current_b, light->progress);
                    if (light->current_r > colorr)
                    {
                        light->current_r = light->current_r - speed;
                    }
                    if (light->current_g > colorg)
                    {
                        light->current_g = light->current_g - speed;
                    }
                    if (light->current_b > colorb)
                    {
                        light->current_b = light->current_b - speed;
                    }
                    if (light->current_r < colorr)
                    {
                        light->current_r = light->current_r + speed;
                    }
                    if (light->current_g < colorg)
                    {
                        light->current_g = light->current_g + speed;
                    }
                    if (light->current_b < colorb)
                    {
                        light->current_b = light->current_b + speed;
                    }
                    int faded_color = (light->current_r << 16) | (light->current_g << 8) | light->current_b;
                    if (light->current_r >= colorr - speed && light->current_g >= colorg - speed && light->current_b >= colorb - speed && light->current_r <= colorr + speed && light->current_g <= colorg + speed && light->current_b <= colorb + speed)
                    {
                        light->running = 0;
                    }
                    fprintf(file, "%06X\n", faded_color);
                }
                else
                {
                    fprintf(file, "%06X\n", light->color2);
                    light->running = 0;
                }
            }
        }
        else if (light->effect == 16)
        {

            ColorWave(light->progress, &r, &g, &b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);

            ColorWave(light->progress + 0.1, &r, &g, &b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);

            ColorWave(light->progress + 0.2, &r, &g, &b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);

            ColorWave(light->progress + 0.3, &r, &g, &b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);

            ColorWave(light->progress + 0.4, &r, &g, &b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
            fprintf(file2, "%02X%02X%02X ", r, g, b);
        }
        else if (light->effect == 17)
        {

            for (int i = 1; i < 10; i++)
            {
                fprintf(file2, "%06X ", light->colorarray[i]);
            }
            if (light->progress == 0.0)
                shiftColors(light->colorarray, 10);
        }
        else
        {
            fprintf(file, "%06X\n", light->color);
        }

        fclose(file);
        fclose(file2);
    }

    chmodfile(filepath, 0);
    chmodfile(filepath2, 0);

    snprintf(filepath, sizeof(filepath), "%s/effect_cycles_%s", dir, light->name);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", -1);
        fclose(file);
    }
    chmodfile(filepath, 0);

    snprintf(filepath, sizeof(filepath), "%s/effect_duration_%s", dir, light->name);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", light->duration);
        fclose(file);
    }
    chmodfile(filepath, 0);

    snprintf(filepath, sizeof(filepath), "%s/effect_%s", dir, light->name);
    chmodfile(filepath, 1);
    file = fopen(filepath, "w");
    if (file != NULL)
    {
        fprintf(file, "%d\n", light->effect >= 8 ? light->effect >= 16 ? 0 : 4 : light->effect);
        fclose(file);
    }
    chmodfile(filepath, 0);
}

bool checkIfEffectChanged(LightSettings *light)
{
    if (light->effect < 8)
    {
        char filepath[256];
        FILE *file;
        snprintf(filepath, sizeof(filepath), "/sys/class/led_anim/effect_%s", light->name);
        file = fopen(filepath, "r");
        if (file != NULL)
        {
            char current_value[20];
            if (fgets(current_value, sizeof(current_value), file))
            {
                int current_effect;
                sscanf(current_value, "%d", &current_effect);
                fclose(file);
                return (light->effect != current_effect);
            }
        }
    }
    else
    {
        if (light->effect != light->last_effect)
        {
            light->last_effect = light->effect;
            return true;
        }
        else
        {
            return false;
        }
    }
}

int main()
{

    int fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        perror("Failed joystick device");
    }
    else
    {
        jsopen = 1;
    }

    LightSettings lights[MAX_LIGHTS] = {0};

    signal(SIGTERM, handle_sigterm);
    signal(SIGCONT, handle_sigcont);
    signal(SIGSTOP, handle_sigsleep);

    changePermissions("/sys/class/led_anim", 0);

    while (running)
    {
        if (!jsopen)
        {
            // Attempt to open the device if it is not already opened
            fd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
            if (fd < 0)
            {
                perror("Failed joystick device");
            }
            else
            {
                printf("Joystick device opened successfully.\n");
                jsopen = 1; // Set the flag to indicate the device is opened
            }
        }

        struct js_event event;
        if (read(fd, &event, sizeof(event)) > 0)
        {
            if (event.type == JS_EVENT_AXIS || event.type == JS_EVENT_BUTTON)
            {
                pressed = event.value ? true : false;
                if (event.type == JS_EVENT_BUTTON)
                {
                    last_pressed = event.number;
                }
                else
                {
                    last_pressed = 100;
                }
            }
        }

        if (read_settings("settings.txt", lights, MAX_LIGHTS) != 0)
        {
            return 1;
        }

        for (int i = 0; i < MAX_LIGHTS; i++)
        {
            // Check current effect before updating
            if (checkIfEffectChanged(&lights[i]))
            {
                lights[i].updated = true;
            }

            if (lights[i].updated || first_run || lights[i].effect >= 8)
            {
                if (first_run || lights[i].updated)
                {
                    int initialColorArray[10] = {lights[i].color, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000};
                    for (int j = 0; j < 8; j++)
                    {
                        lights[i].colorarray[j] = initialColorArray[j];
                    }
                }
                changebrightness("/sys/class/led_anim", lights);
                update_light_settings(&lights[i], "/sys/class/led_anim");
                lights[i].updated = false;
            }
        }

        first_run = false; // Set to false after the first iteration
        usleep(50000);
    }
    close(fd);
    printf("Received SIGTERM, exiting color app...\n");

    return 0;
}
