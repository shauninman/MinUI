// heavily modified from the Onion original: https://github.com/OnionUI/Onion/tree/main/src/batteryMonitorUI
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <msettings.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#include <sqlite3.h>
#include <batmondb.h>

#define GRAPH_LINE_WIDTH 1

// Space between pixels, higher is more transparent
#define GRAPH_BACKGROUND_OPACITY 4

// A multiple of 4 is recommended
#define GRAPH_MAX_FULL_PAGES 8

#define GRAPH_SEGMENTS 9

// 4:1
#define GRAPH_SEGMENT_LOW 7200
// 2:1
#define GRAPH_SEGMENT_MED 3600
// 1:1
#define GRAPH_SEGMENT_HIGH 1800

struct GraphLayout
{
    int graph_display_size_x;
    int graph_display_size_y;
    int graph_display_start_x;
    int graph_display_start_y;

    int label_y;
    int label1_x;
    int label2_x;
    int label3_x;
    int label4_x;

    int sub_title_x;
    int sub_title_y;

    int label_session_x;
    int label_session_y;

    int label_current_x;
    int label_current_y;

    int label_left_x;
    int label_left_y;

    int label_best_x;
    int label_best_y;

    int label_size_x;
    int label_size_y;

    int icon_x;
    int icon1_y;
    int icon2_y;
    int icon3_y;
    int icon4_y;

    int graph_max_size;
};

typedef struct
{
    int pixel_height;
    bool is_charging;
    bool is_estimated;
} GraphSpot;

struct Graph
{
    struct GraphLayout layout;
    GraphSpot *graphic; // needs to be >= GRAPH_MAX_FULL_PAGES * screen_width
} graph = {0};

// One page in seconds
// (At a scale of zoom = 1:1, one segment = 30mn)
// 9 x 30min x 60secs
// 270 mn total, 16200 seconds
#define GRAPH_DISPLAY_DURATION 16200 // GRAPH_SEGMENTS * GRAPH_SEGMENT_HIGH
// Number of graph points to scroll by
#define GRAPH_PAGE_SCROLL_SMOOTHNESS 12
// Minimum amount of data available before attempting an estimation
#define GRAPH_MIN_SESSION_FOR_ESTIMATION 1200
// Cutoff at 15h standby time
#define GRAPH_MAX_PLAUSIBLE_ESTIMATION 54000
#define GRAPH_ESTIMATED_LINE_GAP 20

static bool quit = false;
static int current_zoom = 1;
static int current_page = 0;
static int current_index;

// Zoom level
static int segment_duration;

static char label[4][4];
static int estimation_line_size = 0;
static int begining_session_index;
static char session_duration[10];
static char current_percentage[10];
static char session_left[10] = "unknown";
static char session_best[10];

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

char *device_model = NULL;
sqlite3 *bat_log_db = NULL;

void secondsToHoursMinutes(int seconds, char *output)
{
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    sprintf(output, "%dh%02d", hours, minutes);
}

void drawLine(int x1, int y1, int x2, int y2, Uint32 color)
{
    int dx, dy, sx, sy, err, e2;

    dx = abs(x2 - x1);
    dy = abs(y2 - y1);

    if (x1 < x2)
    {
        sx = 1;
    }
    else
    {
        sx = -1;
    }

    if (y1 < y2)
    {
        sy = 1;
    }
    else
    {
        sy = -1;
    }

    err = dx - dy;

    while (1)
    {
        SDL_Rect pixel = {x1, y1, SCALE1(1), SCALE1(1)};
        SDL_FillRect(screen, &pixel, color);

        if (x1 == x2 && y1 == y2)
        {
            break;
        }

        e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }

        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

int _renderText(const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect, bool right_align)
{
    int text_width = 0;
    SDL_Surface *textSurface = TTF_RenderUTF8_Blended(font, text, color);
    if (textSurface != NULL)
    {
        text_width = textSurface->w;
        if (right_align)
            SDL_BlitSurface(textSurface, NULL, screen, &(SDL_Rect){rect->x - textSurface->w, rect->y, rect->w, rect->h});
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

void switch_zoom_profile(int segment_duration)
{
    // TODO: no reason this cant be dynamic
    switch (segment_duration)
    {
    case GRAPH_SEGMENT_LOW:
        // A segment is 120 minutes
        sprintf(label[0], "%s", "4h");
        sprintf(label[1], "%s", "8h");
        sprintf(label[2], "%s", "12h");
        sprintf(label[3], "%s", "16h");
        break;
    case GRAPH_SEGMENT_MED:
        // A segment is 60 minutes
        sprintf(label[0], "%s", "2h");
        sprintf(label[1], "%s", "4h");
        sprintf(label[2], "%s", "6h");
        sprintf(label[3], "%s", "8h");
        break;
    case GRAPH_SEGMENT_HIGH:
        // A segment is 30 minutes
        sprintf(label[0], "%s", "1h");
        sprintf(label[1], "%s", "2h");
        sprintf(label[2], "%s", "3h");
        sprintf(label[3], "%s", "4h");
        break;
    default:
        sprintf(label[0], "%s", "");
        sprintf(label[1], "%s", "");
        sprintf(label[2], "%s", "");
        sprintf(label[3], "%s", "");
        break;
    }
}

int battery_to_pixel(int battery_perc)
{
    // Converts a battery percentage to a pixel coordinate
    int y = (int)((graph.layout.graph_display_size_y * battery_perc) / 100);

    if ((y < 0) || (y > graph.layout.graph_display_size_y))
    {
        return -1;
    }
    else
    {
        return y;
    }
}

int duration_to_pixel(int duration)
{
    // Convert a duration to a number of pixel
    return (int)((graph.layout.graph_display_size_x * duration) / GRAPH_DISPLAY_DURATION);
}

void compute_graph(void)
{
    int total_duration = 0;
    int previous_index = graph.layout.graph_max_size - 1;
    bool is_estimation_computed = false;
    
    bat_log_db = open_battery_log_db();
    secondsToHoursMinutes(get_best_session_time(bat_log_db, device_model), session_best);

    if (bat_log_db != NULL)
    {
        const char *sql = "SELECT * FROM bat_activity WHERE device_serial = ? ORDER BY id DESC;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(bat_log_db, sql, -1, &stmt, 0);

        if (rc == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, device_model, -1, SQLITE_STATIC);
            bool b_quit = false;

            while ((sqlite3_step(stmt) == SQLITE_ROW) && (!b_quit))
            {

                int bat_perc = sqlite3_column_int(stmt, 2);
                int duration = sqlite3_column_int(stmt, 3);
                bool is_charging = sqlite3_column_int(stmt, 4);

                if (total_duration == 0)
                {
                    sprintf(current_percentage, "%d%%", bat_perc);
                }

                current_index = (graph.layout.graph_max_size - 1) - duration_to_pixel(total_duration);
                graph.graphic[current_index].is_charging = is_charging;

                if (bat_perc > 100)
                    bat_perc = 100;

                graph.graphic[current_index].pixel_height = battery_to_pixel(bat_perc);

                int segment = previous_index - current_index;
                if (segment > 1)
                {
                    for (int i = 1; i < segment; i++)
                    {
                        graph.graphic[current_index + i].pixel_height = graph.graphic[previous_index].pixel_height;
                        graph.graphic[current_index + i].is_charging = graph.graphic[previous_index].is_charging;
                    }
                }

                if ((is_charging) && (!is_estimation_computed))
                {
                    secondsToHoursMinutes(total_duration, session_duration);
                    if (previous_index < (graph.layout.graph_max_size - duration_to_pixel(GRAPH_MIN_SESSION_FOR_ESTIMATION)))
                    {
                        float slope = (float)(graph.graphic[graph.layout.graph_max_size - 1].pixel_height - graph.graphic[previous_index].pixel_height) / (float)(graph.layout.graph_max_size - 1 - previous_index);

                        if (slope < 0)
                        {

                            estimation_line_size = (int)-(graph.graphic[graph.layout.graph_max_size - 1].pixel_height) / slope;

                            int estimated_playtime = (int)(estimation_line_size)*GRAPH_DISPLAY_DURATION / graph.layout.graph_display_size_x;

                            if (estimated_playtime < GRAPH_MAX_PLAUSIBLE_ESTIMATION)
                            {
                                secondsToHoursMinutes(estimated_playtime, session_left);
                                // shift of the existing logs to make room for the estimation line
                                int room_to_make = estimation_line_size + GRAPH_ESTIMATED_LINE_GAP;
                                if (current_index - room_to_make >= 0)
                                {
                                    for (int i = current_index; i < graph.layout.graph_max_size; i++)
                                    {
                                        graph.graphic[i - room_to_make].pixel_height = graph.graphic[i].pixel_height;
                                        graph.graphic[i - room_to_make].is_charging = graph.graphic[i].is_charging;
                                        graph.graphic[i].pixel_height = 0;
                                        graph.graphic[i].is_charging = false;
                                    }
                                }
                                total_duration += estimated_playtime + (int)(GRAPH_ESTIMATED_LINE_GAP)*GRAPH_DISPLAY_DURATION / graph.layout.graph_display_size_x;
                                current_index -= room_to_make;
                                previous_index -= room_to_make;
                                begining_session_index = previous_index;
                                for (int x = (graph.layout.graph_max_size - room_to_make); x < graph.layout.graph_max_size; x++)
                                {
                                    int y = graph.graphic[previous_index].pixel_height + (int)(slope * (x - previous_index));
                                    if (y > 0)
                                    {
                                        graph.graphic[x].pixel_height = y;
                                        graph.graphic[x].is_estimated = true;
                                    }
                                    else
                                        break;
                                }
                            }
                            else
                            {
                                estimation_line_size = 0;
                                estimated_playtime = 0;
                            }
                        }
                    }

                    is_estimation_computed = true;
                }

                total_duration += duration;
                if (duration_to_pixel(total_duration) > graph.layout.graph_max_size)
                    b_quit = true;

                previous_index = current_index;
            }
        }
        sqlite3_finalize(stmt);
        close_battery_log_db(bat_log_db);
    }
}

void drawBatteryIcon(int percent, SDL_Rect dst) {
    // taken from GFX_blitBattery

    int x = dst.x;
    int y = dst.y;
    //SDL_Rect rect = asset_rects[ASSET_BATTERY];
    SDL_Rect rect = (SDL_Rect){SCALE4(47,51,17,10)};
    GFX_blitAsset(ASSET_BATTERY, NULL, screen, &(SDL_Rect){x,y});
    //rect = asset_rects[ASSET_BATTERY_FILL];
    rect = (SDL_Rect){SCALE4(81,33,12,6)};
    SDL_Rect clip = rect;
    clip.w *= percent;
    clip.w /= 100;
    if (clip.w<=0) return;
    clip.x = rect.w - clip.w;
    clip.y = 0;
    
    GFX_blitAsset(ASSET_BATTERY_FILL, &clip, screen, &(SDL_Rect){x+SCALE1(3)+clip.x,y+SCALE1(2)});
}

void renderPage()
{
    const struct SDL_Point tl = {graph.layout.graph_display_start_x, graph.layout.graph_display_start_y};
    const struct SDL_Point br = {graph.layout.graph_display_start_x + graph.layout.graph_display_size_x, graph.layout.graph_display_start_y + graph.layout.graph_display_size_y};
    const int grid_step_x = 2 * (int)(graph.layout.graph_display_size_x / GRAPH_SEGMENTS); // every second "segment"
    const int grid_step_y = graph.layout.graph_display_size_y / 4;

    // grid verticals
    for (int x = tl.x; x <= br.x; x+= grid_step_x)
        drawLine(x, tl.y, x, br.y, RGB_DARK_GRAY);
    drawLine(br.x, tl.y, br.x, br.y, RGB_DARK_GRAY); // close the last segment early
    // grid horizontals
    for (int y = tl.y; y <= br.y; y += grid_step_y)
        drawLine(tl.x, y, br.x, y, RGB_DARK_GRAY);

    switch (current_zoom)
    {
    case 0:
        segment_duration = GRAPH_SEGMENT_LOW;
        break;
    case 1:
        segment_duration = GRAPH_SEGMENT_MED;
        break;
    case 2:
        segment_duration = GRAPH_SEGMENT_HIGH;
        break;
    default:
        segment_duration = GRAPH_SEGMENT_MED;
        break;
    }

    if (estimation_line_size == 0)
        current_index = graph.layout.graph_max_size - (graph.layout.graph_display_size_x * (int)(segment_duration / GRAPH_SEGMENT_HIGH));
    else
        current_index = begining_session_index;

    current_index -= (int)(current_page * (graph.layout.graph_display_size_x * (int)(segment_duration / GRAPH_SEGMENT_HIGH)) / GRAPH_PAGE_SCROLL_SMOOTHNESS);

    if (current_index < 0)
        current_index = 0;

    switch_zoom_profile(segment_duration);

    // x axis labels
    renderText(label[0], font.small, COLOR_WHITE, &(SDL_Rect){graph.layout.label1_x, graph.layout.label_y, 32, 32});
    renderText(label[1], font.small, COLOR_WHITE, &(SDL_Rect){graph.layout.label2_x, graph.layout.label_y, 32, 32});
    renderText(label[2], font.small, COLOR_WHITE, &(SDL_Rect){graph.layout.label3_x, graph.layout.label_y, 32, 32});
    renderText(label[3], font.small, COLOR_WHITE, &(SDL_Rect){graph.layout.label4_x, graph.layout.label_y, 32, 32});

    // y axis "labels"
    drawBatteryIcon(100, (SDL_Rect){graph.layout.icon_x, graph.layout.icon1_y});
    drawBatteryIcon(66, (SDL_Rect){graph.layout.icon_x, graph.layout.icon2_y});
    drawBatteryIcon(33, (SDL_Rect){graph.layout.icon_x, graph.layout.icon3_y});
    drawBatteryIcon(0, (SDL_Rect){graph.layout.icon_x, graph.layout.icon4_y});

    char text_line[255];
    sprintf(text_line, "Since Charge: %s", session_duration);
    renderText(text_line, font.medium, COLOR_WHITE, &(SDL_Rect){graph.layout.label_session_x, graph.layout.label_session_y, graph.layout.label_size_x, graph.layout.label_size_y});

    sprintf(text_line, "Current: %s", current_percentage);
    renderText(text_line, font.medium, COLOR_WHITE, &(SDL_Rect){graph.layout.label_current_x, graph.layout.label_current_y, graph.layout.label_size_x, graph.layout.label_size_y});

    sprintf(text_line, "Remaining: %s", session_left);
    renderTextAlignRight(text_line, font.medium, COLOR_WHITE, &(SDL_Rect){graph.layout.label_left_x, graph.layout.label_left_y, graph.layout.label_size_x, graph.layout.label_size_y});

    sprintf(text_line, "Longest: %s", session_best);
    renderTextAlignRight(text_line, font.medium, COLOR_WHITE, &(SDL_Rect){graph.layout.label_best_x, graph.layout.label_best_y, graph.layout.label_size_x, graph.layout.label_size_y});

    int half_line_width = (int)(GRAPH_LINE_WIDTH) / 2;

    Uint32 white_pixel_color = RGB_GRAY;
    Uint32 red_pixel_color = RGB_WHITE;
    Uint32 blue_pixel_color = RGB_LIGHT_GRAY;
    Uint32 pixel_color = white_pixel_color;

    // monochrome is more MinUI, but some colours are nice
    const bool ilikeitcolourful = true;
    if(ilikeitcolourful) {
        white_pixel_color = SDL_MapRGB(screen->format, 255, 255, 255);
        red_pixel_color = SDL_MapRGB(screen->format, 255, 170, 170);
        blue_pixel_color = SDL_MapRGB(screen->format, 89, 167, 255);
        pixel_color = white_pixel_color;
    }

    int x;
    int y;
    int x_end = 0;
    int y_end = 0;

    int zoom_level = (int)segment_duration / GRAPH_SEGMENT_HIGH;

    if (SDL_LockSurface(screen) == 0)
    {
        // some constants to make it more readable
        const int graph_display_right = graph.layout.graph_display_size_x + graph.layout.graph_display_start_x;
        const int graph_display_bottom = graph.layout.graph_display_size_y + graph.layout.graph_display_start_y;

        for (int i = 0; i < graph.layout.graph_max_size - current_index; i += zoom_level)
        {
            x = graph.layout.graph_display_start_x + (int)(i / zoom_level);
            y = graph.graphic[i + current_index].pixel_height;

            bool is_charging = graph.graphic[i + current_index].is_charging;
            bool is_estimated = graph.graphic[i + current_index].is_estimated;
            // if ((!is_charging)
            if ((!is_charging) && (!is_estimated))
                pixel_color = white_pixel_color;
            else if (is_charging)
                pixel_color = red_pixel_color;
            else if (is_estimated)
            {
                pixel_color = blue_pixel_color;
                // magic numbers everywhere...
                if (y < 5 && x < graph_display_right)
                {
                    x_end = x - 12;
                    y_end = graph_display_bottom - 45;
                }
            }

            if (x < graph_display_right && y > 0)
            {
                // Actual graph line
                if (half_line_width >= 0)
                {
                    for (int k = -half_line_width; k <= half_line_width; k++)
                    {
                        int index = (graph_display_bottom - y + k) * screen->pitch + x * screen->format->BytesPerPixel;
                        *((Uint32 *)((Uint8 *)screen->pixels + index)) = pixel_color;
                    }
                }

                // Area under the graph
                if ((x % GRAPH_BACKGROUND_OPACITY) == 0)
                {
                    for (int k = y; k > 0; k--)
                    {
                        if ((k % GRAPH_BACKGROUND_OPACITY) == 0)
                        {
                            int index = (graph_display_bottom - k) * screen->pitch + x * screen->format->BytesPerPixel;
                            *((Uint32 *)((Uint8 *)screen->pixels + index)) = pixel_color;
                        }
                    }
                }
            }
        }
        SDL_UnlockSurface(screen);
        if (x_end != 0)
            GFX_blitAsset(ASSET_BATTERY_LOW, NULL, screen, &(SDL_Rect){x_end, y_end});
    }
}

void initLayout()
{
    // unscaled
    int hw = screen->w;
    int hh = screen->h;

    // the title. just leave the default padding all around
    graph.layout.sub_title_x = SCALE1(PADDING);
    graph.layout.sub_title_y = SCALE1(PADDING);

#define GRAPH_MARGIN 12
#define STATS_MARGIN 12
#define AXIS_MARGIN 8
#define AXIS_WIDTH 16
#define STATS_HEIGHT 31

    // the diagram.
    // x: start inside the default padding, and align with the text inside the pill above and below (BUTTON_MARGIN).
    // y: default padding, below the title pill and some additional padding to leave some breathing room (BUTTON_MARGIN + GRAPH_MARGIN).
    graph.layout.graph_display_start_x = SCALE1(PADDING + BUTTON_MARGIN);
    graph.layout.graph_display_start_y = SCALE1(PADDING + PILL_SIZE + BUTTON_MARGIN + GRAPH_MARGIN);

    // x: stretch whole width inside default padding + extra margin (see above), leaving space top the right for icons.
    // y: stretch whole height below graph_display_start_y, leaving room at the bottom for padding, button hints, stats, axis labels (TODO)
    graph.layout.graph_display_size_x = hw - SCALE1(PADDING * 2 + BUTTON_MARGIN * 2 + AXIS_MARGIN + AXIS_WIDTH);
    graph.layout.graph_display_size_y = hh - SCALE1(PADDING * 2 + PILL_SIZE * 2 + BUTTON_MARGIN * 2 + GRAPH_MARGIN * 2 + STATS_MARGIN * 2 + STATS_HEIGHT);

    // y coord of x axis labels
    graph.layout.label_y = graph.layout.graph_display_start_y + graph.layout.graph_display_size_y;

    // x axis: time
    const int seglen = graph.layout.graph_display_size_x / GRAPH_SEGMENTS;
    graph.layout.label1_x = graph.layout.graph_display_start_x + 2 * seglen - SCALE1(FONT_SMALL / 2);
    graph.layout.label2_x = graph.layout.graph_display_start_x + 4 * seglen - SCALE1(FONT_SMALL / 2);
    graph.layout.label3_x = graph.layout.graph_display_start_x + 6 * seglen - SCALE1(FONT_SMALL / 2);
    graph.layout.label4_x = graph.layout.graph_display_start_x + 8 * seglen - SCALE1(FONT_SMALL / 2);

    // y axis: charge
    graph.layout.icon_x = hw - SCALE1(PADDING + BUTTON_MARGIN + AXIS_WIDTH);
    const int inc = graph.layout.graph_display_size_y / 4;
    const int assetH = 10;
    graph.layout.icon1_y = graph.layout.graph_display_start_y + inc / 2 - assetH / 2;
    graph.layout.icon2_y = graph.layout.icon1_y + inc;
    graph.layout.icon3_y = graph.layout.icon2_y + inc;
    graph.layout.icon4_y = graph.layout.icon3_y + inc;

    graph.layout.label_current_x = SCALE1(PADDING + BUTTON_MARGIN);
    graph.layout.label_current_y = graph.layout.label_y + SCALE1(GRAPH_MARGIN + STATS_MARGIN);

    graph.layout.label_session_x = SCALE1(PADDING + BUTTON_MARGIN);
    graph.layout.label_session_y = graph.layout.label_y + SCALE1(GRAPH_MARGIN + STATS_MARGIN + BUTTON_MARGIN + FONT_MEDIUM);

    graph.layout.label_left_x = hw - SCALE1(PADDING + BUTTON_MARGIN + AXIS_WIDTH + AXIS_MARGIN); // right-aligned!
    graph.layout.label_left_y = graph.layout.label_y + SCALE1(GRAPH_MARGIN + STATS_MARGIN);

    graph.layout.label_best_x = hw - SCALE1(PADDING + BUTTON_MARGIN + AXIS_WIDTH + AXIS_MARGIN); // right-aligned!
    graph.layout.label_best_y = graph.layout.label_y + SCALE1(GRAPH_MARGIN + STATS_MARGIN + BUTTON_MARGIN + FONT_MEDIUM);

    graph.layout.label_size_x = graph.layout.graph_display_size_x / 2;
    graph.layout.label_size_y = FONT_MEDIUM; // true for all? better make sure!

    graph.layout.graph_max_size = GRAPH_MAX_FULL_PAGES * graph.layout.graph_display_size_x;

    graph.graphic = (GraphSpot *)malloc(graph.layout.graph_max_size * sizeof(GraphSpot));
    for (int i = 0; i < graph.layout.graph_max_size; i++)
    {
        graph.graphic[i].pixel_height = 0;
        graph.graphic[i].is_charging = false;
        graph.graphic[i].is_estimated = false;
    }
}

void pre_sleep_callback(int reason)
{
    LOG_info("System going to sleep reason: %i\n", reason);
}

int main(int argc, char *argv[])
{
    InitSettings();

    PWR_setCPUSpeed(CPU_SPEED_MENU);
    device_model = PLAT_getModel();

    screen = GFX_init(MODE_MAIN);
    PAD_init();
    PWR_init();
    // TODO: remove. Just a crutch to keep it from sleeping for validation!
    PWR_disableAutosleep();

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    initLayout();
    compute_graph();
    renderPage();

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
                dirty = 1;
                int page_max = (int)(((GRAPH_MAX_FULL_PAGES) * (GRAPH_PAGE_SCROLL_SMOOTHNESS)) / (segment_duration / GRAPH_SEGMENT_HIGH));
                page_max -= GRAPH_PAGE_SCROLL_SMOOTHNESS;

                if (page_max > current_page)
                    current_page++;
            }
            else if (PAD_justRepeated(BTN_RIGHT))
            {
                dirty = 1;
                if (current_page > 0)
                    current_page--;
            }
            else if (PAD_justPressed(BTN_B))
            {
                quit = 1;
            }
            else if (PAD_justPressed(BTN_L1) || PAD_justPressed(BTN_L2))
            {
                if (current_zoom > 0)
                {
                    current_page = 0;
                    current_zoom--;
                    dirty = 1;
                }
            }
            else if (PAD_justPressed(BTN_R1) || PAD_justPressed(BTN_R2))
            {
                if (current_zoom < 2)
                {
                    current_page = 0;
                    current_zoom++;
                    dirty = 1;
                }
            }
        }

        PWR_update(&dirty, &show_setting, pre_sleep_callback, NULL);

        int is_online = PLAT_isOnline();
        if (was_online != is_online)
            dirty = 1;
        was_online = is_online;

        if (dirty)
        {
            GFX_clear(screen);

            // title pill
            {
                int ow = GFX_blitHardwareGroup(screen, show_setting);
                int max_width = screen->w - SCALE1(PADDING * 2) - ow;

                char display_name[256];

                switch (current_zoom)
                {
                case 0:
                    sprintf(display_name, "Battery usage: Last %s", "16 hours");
                    break;
                case 1:
                    sprintf(display_name, "Battery usage: Last %s", "8 hours");
                    break;
                case 2:
                    sprintf(display_name, "Battery usage: Last %s", "4 hours");
                    break;
                default:
                    sprintf(display_name, "Battery usage: Last %s", "8 hours");
                    break;
                }

                char title[256];
                int text_width = GFX_truncateText(font.large, display_name, title, max_width, SCALE1(BUTTON_PADDING * 2));
                max_width = MIN(max_width, text_width);

                SDL_Surface *text;
                text = TTF_RenderUTF8_Blended(font.large, title, COLOR_WHITE);
                GFX_blitPill(ASSET_DARK_GRAY_PILL, screen, &(SDL_Rect){SCALE1(PADDING), SCALE1(PADDING), max_width, SCALE1(PILL_SIZE)});
                SDL_BlitSurface(text, &(SDL_Rect){0, 0, max_width - SCALE1(BUTTON_PADDING * 2), text->h}, screen, &(SDL_Rect){SCALE1(PADDING + BUTTON_PADDING), SCALE1(PADDING + 4)});
                SDL_FreeSurface(text);
            }

            renderPage();

            if (show_setting)
                GFX_blitHardwareHints(screen, show_setting);
            else
                GFX_blitButtonGroup((char *[]){"L/R", "SCROLL", "L1/R1", "ZOOM", NULL}, 0, screen, 0);

            GFX_blitButtonGroup((char *[]){"B", "BACK", NULL}, 1, screen, 1);

            GFX_flip(screen);
            dirty = 0;
        }
        else
            GFX_sync();
    }

    QuitSettings();
    PWR_quit();
    PAD_quit();
    GFX_quit();

    return EXIT_SUCCESS;
}