# MinUI API Reference

Complete reference for MinUI's common APIs and platform abstraction layer.

## Graphics API (GFX_*)

Defined in: `/workspace/all/common/api.c`, `/workspace/all/common/api.h`

### Initialization

#### `void GFX_init(void)`

Initializes the graphics subsystem.

**Description:**
- Sets up SDL video mode
- Loads UI assets
- Initializes fonts
- Prepares rendering surfaces

**Called by:** MinUI, Minarch, utilities
**Must call:** Before any other GFX functions

---

#### `void GFX_quit(void)`

Shuts down graphics subsystem.

**Description:**
- Frees all loaded assets
- Closes fonts
- Releases video resources

**Called by:** Application exit
**Must call:** After all GFX operations complete

---

### Display Management

#### `void GFX_clear(void)`

Clears the screen to black.

**Description:**
- Fills screen with color 0x0000 (black)
- Typically called at start of frame

---

#### `void GFX_flip(void)`

Presents the rendered frame.

**Description:**
- Swaps front/back buffers (double-buffered mode)
- Or copies buffer to display (single-buffered)
- Equivalent to SDL_Flip

**Frame timing:** Should be called once per frame (~60 Hz)

---

#### `void GFX_sync(void)`

Waits for vertical sync.

**Description:**
- Blocks until next v-sync
- Prevents screen tearing
- May delegate to `PLAT_vsync()`

---

### Asset Loading

#### `SDL_Surface* GFX_loadImage(const char* path)`

Loads a PNG image.

**Parameters:**
- `path` - File path to PNG image

**Returns:**
- `SDL_Surface*` - Loaded image surface
- `NULL` - On error

**Description:**
- Loads PNG from disk
- Converts to screen format
- Caches for performance
- Caller must NOT free (managed internally)

**Example:**
```c
SDL_Surface* icon = GFX_loadImage("icon.png");
if (icon) {
    GFX_blitImage(icon, x, y);
}
```

---

#### `void GFX_freeImage(SDL_Surface* image)`

Frees a loaded image.

**Parameters:**
- `image` - Surface to free

**Description:**
- Releases image memory
- Removes from cache
- Safe to call with NULL

---

#### `void GFX_blitImage(SDL_Surface* src, int x, int y)`

Draws an image to screen.

**Parameters:**
- `src` - Source surface
- `x` - X coordinate
- `y` - Y coordinate

**Description:**
- Blits entire source surface
- Top-left corner at (x, y)
- Respects alpha channel

---

### Text Rendering

#### `TTF_Font* GFX_openFont(const char* path, int size)`

Opens a TrueType font.

**Parameters:**
- `path` - Font file path
- `size` - Point size (16, 20, 24, 32, 40)

**Returns:**
- `TTF_Font*` - Loaded font
- `NULL` - On error

**Description:**
- Loads TTF/OTF font
- Opens at specified point size
- Caches fonts by path+size
- Caller must NOT free

**Example:**
```c
TTF_Font* font = GFX_openFont("font.otf", 20);
```

---

#### `void GFX_closeFont(TTF_Font* font)`

Closes a font.

**Parameters:**
- `font` - Font to close

**Description:**
- Releases font resources
- Removes from cache
- Safe to call with NULL

---

#### `void GFX_drawText(int x, int y, const char* text, TTF_Font* font, uint32_t color)`

Renders text to screen.

**Parameters:**
- `x` - X coordinate
- `y` - Y coordinate (baseline)
- `text` - UTF-8 string
- `font` - Font to use
- `color` - RGB888 color (0xRRGGBB)

**Description:**
- Renders text with anti-aliasing
- Color converted to screen format
- Alpha blending enabled

**Example:**
```c
GFX_drawText(10, 30, "Hello World", font, 0xFFFFFF);
```

---

#### `int GFX_getTextWidth(const char* text, TTF_Font* font)`

Measures text width.

**Parameters:**
- `text` - String to measure
- `font` - Font to use

**Returns:**
- `int` - Width in pixels

**Description:**
- Calculates rendered width
- Useful for centering/alignment

**Example:**
```c
int width = GFX_getTextWidth("Hello", font);
int x = (FIXED_WIDTH - width) / 2;  // Center
GFX_drawText(x, y, "Hello", font, color);
```

---

### UI Elements

#### `void GFX_drawPill(int x, int y, int w, int h, uint32_t color)`

Draws a rounded rectangle (pill shape).

**Parameters:**
- `x`, `y` - Top-left corner
- `w`, `h` - Width and height
- `color` - RGB888 color

**Description:**
- Filled rounded rectangle
- Used for buttons, selections
- Corner radius automatic based on height

---

#### `void GFX_drawButton(int x, int y, const char* text, int selected)`

Draws a UI button.

**Parameters:**
- `x`, `y` - Position
- `text` - Button label
- `selected` - 1 if selected, 0 otherwise

**Description:**
- Draws pill background
- Renders text centered
- Different colors for selected/unselected

---

#### `void GFX_drawBattery(int x, int y, int level, int charging)`

Draws battery indicator.

**Parameters:**
- `x`, `y` - Position
- `level` - Battery percentage (0-100)
- `charging` - 1 if charging, 0 otherwise

**Description:**
- Draws battery icon
- Fill indicates level
- Lightning bolt if charging

---

#### `void GFX_drawVolume(int x, int y, int level)`

Draws volume indicator.

**Parameters:**
- `x`, `y` - Position
- `level` - Volume (0-20)

**Description:**
- Draws speaker icon
- Bars indicate level

---

#### `void GFX_drawBrightness(int x, int y, int level)`

Draws brightness indicator.

**Parameters:**
- `x`, `y` - Position
- `level` - Brightness (0-10)

**Description:**
- Draws sun icon
- Brightness indicates level

---

## Sound API (SND_*)

Defined in: `/workspace/all/common/api.c`, `/workspace/all/common/api.h`

### Initialization

#### `void SND_init(void)`

Initializes audio subsystem.

**Description:**
- Opens audio device
- Sets up ring buffer
- Configures sample rate (44100 or 48000 Hz)

---

#### `void SND_quit(void)`

Shuts down audio.

**Description:**
- Closes audio device
- Releases buffers

---

### Audio Output

#### `void SND_write(int16_t* samples, int count)`

Writes audio samples.

**Parameters:**
- `samples` - Interleaved stereo samples (L, R, L, R, ...)
- `count` - Number of sample *frames* (pairs)

**Description:**
- Writes to ring buffer
- Non-blocking (drops if full)
- Samples are 16-bit signed PCM

**Example:**
```c
int16_t buffer[2048];  // 1024 frames
// ... fill buffer ...
SND_write(buffer, 1024);
```

---

## Input API (PAD_*)

Defined in: `/workspace/all/common/api.c`, `/workspace/all/common/api.h`

### Initialization

#### `void PAD_init(void)`

Initializes input subsystem.

**Description:**
- Calls `PLAT_initInput()`
- Initializes button state tracking
- Sets up auto-repeat timers

---

#### `void PAD_quit(void)`

Shuts down input.

**Description:**
- Calls `PLAT_quitInput()`
- Releases input resources

---

### Input Polling

#### `void PAD_poll(void)`

Updates button state.

**Description:**
- Calls `PLAT_pollInput()`
- Updates button state arrays
- Handles auto-repeat logic

**Must call:** Once per frame before checking buttons

---

#### `int PAD_justPressed(int button)`

Checks if button just pressed.

**Parameters:**
- `button` - Button ID (BTN_A, BTN_B, etc.)

**Returns:**
- `1` - Button pressed this frame
- `0` - Not pressed

**Description:**
- Edge detection (0 → 1 transition)
- Returns true only on first frame

**Example:**
```c
if (PAD_justPressed(BTN_A)) {
    launchGame();
}
```

---

#### `int PAD_justRepeated(int button)`

Checks if button auto-repeated.

**Parameters:**
- `button` - Button ID

**Returns:**
- `1` - Button repeated this frame
- `0` - Not repeated

**Description:**
- Initial delay: 300ms
- Repeat rate: 100ms
- Useful for menu navigation

**Example:**
```c
if (PAD_justPressed(BTN_UP) || PAD_justRepeated(BTN_UP)) {
    selection--;
}
```

---

#### `int PAD_justReleased(int button)`

Checks if button just released.

**Parameters:**
- `button` - Button ID

**Returns:**
- `1` - Button released this frame
- `0` - Not released

**Description:**
- Edge detection (1 → 0 transition)

---

#### `int PAD_isPressed(int button)`

Checks if button currently held.

**Parameters:**
- `button` - Button ID

**Returns:**
- `1` - Button currently down
- `0` - Button up

**Description:**
- Level detection (not edge)
- True every frame while held

---

### Button IDs

```c
#define BTN_UP      0
#define BTN_DOWN    1
#define BTN_LEFT    2
#define BTN_RIGHT   3
#define BTN_A       4
#define BTN_B       5
#define BTN_X       6
#define BTN_Y       7
#define BTN_L1      8
#define BTN_R1      9
#define BTN_L2      10
#define BTN_R2      11
#define BTN_SELECT  12
#define BTN_START   13
#define BTN_MENU    14
#define BTN_L3      15
#define BTN_R3      16
```

**Note:** Not all platforms have all buttons. Check `HAS_X` defines.

---

## Power API (PWR_*)

Defined in: `/workspace/all/common/api.c`, `/workspace/all/common/api.h`

### Initialization

#### `void PWR_init(void)`

Initializes power subsystem.

**Description:**
- Sets up sleep timer
- Initializes battery monitoring
- Configures CPU speed

---

#### `void PWR_quit(void)`

Shuts down power management.

---

### Update

#### `void PWR_update(void)`

Updates power state.

**Description:**
- Checks for idle timeout
- Monitors battery status
- Handles auto-sleep
- Handles auto-poweroff

**Must call:** Once per frame

---

### Battery

#### `int PWR_getBatteryLevel(void)`

Gets battery percentage.

**Returns:**
- `0-100` - Battery percentage
- `-1` - Unknown/AC powered

**Description:**
- Calls `PLAT_getBatteryStatus()`
- Cached, updated periodically

---

#### `int PWR_isCharging(void)`

Checks if charging.

**Returns:**
- `1` - Charging
- `0` - Not charging

**Description:**
- Calls `PLAT_getBatteryStatus()`

---

### Sleep/Power

#### `void PWR_sleep(void)`

Enters sleep mode.

**Description:**
- Disables backlight
- Reduces CPU speed
- Waits for wake button

---

#### `void PWR_wake(void)`

Exits sleep mode.

**Description:**
- Restores backlight
- Restores CPU speed
- Resets idle timer

---

#### `void PWR_powerOff(void)`

Shuts down device.

**Description:**
- Saves state
- Calls `PLAT_powerOff()`
- Does not return

---

### CPU Speed

#### `int PWR_getCPUSpeed(void)`

Gets current CPU frequency.

**Returns:**
- `int` - Frequency in MHz

---

#### `void PWR_setCPUSpeed(int mhz)`

Sets CPU frequency.

**Parameters:**
- `mhz` - Frequency in MHz

**Description:**
- Calls `PLAT_setCPUSpeed()`
- Platform-dependent valid values

---

## Platform Abstraction Layer (PLAT_*)

Defined in: `/workspace/[platform]/platform/platform.c`, `platform.h`

Each platform must implement these functions.

### Video

#### `void PLAT_initVideo(void)`

Initializes video.

**Description:**
- Opens framebuffer or SDL window
- Sets up display surfaces
- Configures scaling

---

#### `void PLAT_quitVideo(void)`

Shuts down video.

---

#### `void PLAT_clearVideo(void)`

Clears video memory.

---

#### `void PLAT_setVsync(int enabled)`

Enables/disables vsync.

---

#### `void PLAT_vsync(void)`

Waits for vsync.

---

#### `void PLAT_flip(void)`

Swaps buffers.

---

#### `SDL_Surface* PLAT_getScreen(void)`

Gets screen surface.

**Returns:**
- `SDL_Surface*` - Main screen surface

---

### Input

#### `void PLAT_initInput(void)`

Initializes input devices.

---

#### `void PLAT_quitInput(void)`

Shuts down input.

---

#### `void PLAT_pollInput(void)`

Polls input state.

**Description:**
- Reads all input devices
- Updates button state arrays
- Handles analog sticks

---

### Power

#### `int PLAT_getBatteryStatus(void)`

Gets battery info.

**Returns:**
- `0-100` - Battery percentage
- `101-200` - Charging (subtract 100 for %)
- `-1` - Unknown

---

#### `void PLAT_enableBacklight(int enable)`

Controls backlight.

**Parameters:**
- `enable` - 1 to enable, 0 to disable

---

#### `void PLAT_powerOff(void)`

Powers off device.

**Description:**
- Hardware shutdown
- Does not return

---

#### `void PLAT_setCPUSpeed(int speed)`

Sets CPU frequency.

**Parameters:**
- `speed` - CPU speed index (platform-specific)

---

### Scaling (Optional)

#### `void PLAT_scale(SDL_Surface* src, SDL_Rect* src_rect, SDL_Surface* dst, SDL_Rect* dst_rect, int sharp, int effect)`

Hardware-accelerated scaling.

**Parameters:**
- `src` - Source surface
- `src_rect` - Source rectangle
- `dst` - Destination surface
- `dst_rect` - Destination rectangle
- `sharp` - Sharpness mode (0-2)
- `effect` - Effect mode (0-2)

**Description:**
- Platform may provide hardware scaling
- Falls back to software if not implemented

---

## Settings Library (libmsettings)

Defined in: `/workspace/[platform]/libmsettings/msettings.c`, `msettings.h`

### Initialization

#### `int InitSettings(void)`

Initializes settings system.

**Returns:**
- `0` - Success
- `-1` - Error

---

#### `void QuitSettings(void)`

Saves and closes settings.

---

### Volume

#### `int GetVolume(void)`

Gets volume level.

**Returns:**
- `0-20` - Volume level

---

#### `void SetVolume(int value)`

Sets volume level.

**Parameters:**
- `value` - Volume (0-20)

**Description:**
- Adjusts hardware mixer
- Persists to settings file

---

### Brightness

#### `int GetBrightness(void)`

Gets brightness level.

**Returns:**
- `0-10` - Brightness level

---

#### `void SetBrightness(int value)`

Sets brightness level.

**Parameters:**
- `value` - Brightness (0-10)

**Description:**
- Adjusts backlight via sysfs
- Persists to settings file

---

### Last Play

#### `char* GetLastPlay(void)`

Gets last played ROM path.

**Returns:**
- `char*` - Full path to ROM
- `NULL` - No last play

---

#### `void SetLastPlay(char* path)`

Sets last played ROM.

**Parameters:**
- `path` - Full ROM path

**Description:**
- Saves for auto-resume
- Used by MinUI launcher

---

## Utility Functions

Defined in: `/workspace/all/common/utils.c`, `utils.h`

### File Operations

#### `int exists(const char* path)`

Checks if file exists.

**Returns:**
- `1` - Exists
- `0` - Does not exist

---

#### `int isDir(const char* path)`

Checks if path is directory.

**Returns:**
- `1` - Is directory
- `0` - Not directory or doesn't exist

---

#### `void makeDir(const char* path)`

Creates directory.

**Parameters:**
- `path` - Directory path

**Description:**
- Creates if doesn't exist
- No error if already exists

---

### String Operations

#### `int exactMatch(const char* str, const char* find)`

Case-sensitive exact match.

**Returns:**
- `1` - Strings match
- `0` - Don't match

---

#### `int prefixMatch(const char* str, const char* prefix)`

Checks if string starts with prefix.

**Returns:**
- `1` - Starts with prefix
- `0` - Doesn't start with prefix

---

#### `int suffixMatch(const char* str, const char* suffix)`

Checks if string ends with suffix.

**Returns:**
- `1` - Ends with suffix
- `0` - Doesn't end with suffix

**Example:**
```c
if (suffixMatch(filename, ".gb")) {
    // It's a Game Boy ROM
}
```

---

### Path Operations

#### `const char* getFilename(const char* path)`

Extracts filename from path.

**Parameters:**
- `path` - Full path

**Returns:**
- `const char*` - Filename portion

**Example:**
```c
getFilename("/path/to/file.txt")  // Returns "file.txt"
```

---

#### `const char* getExtension(const char* path)`

Gets file extension.

**Parameters:**
- `path` - Filename or path

**Returns:**
- `const char*` - Extension (including dot)
- `""` - No extension

**Example:**
```c
getExtension("file.txt")  // Returns ".txt"
```

---

#### `void trimExtension(char* path)`

Removes file extension.

**Parameters:**
- `path` - Path to modify (in-place)

**Example:**
```c
char name[256] = "game.gb";
trimExtension(name);
// name is now "game"
```

---

## Constants and Macros

### Screen

```c
#define FIXED_WIDTH    640   // Platform-specific
#define FIXED_HEIGHT   480   // Platform-specific
#define FIXED_BPP      16    // Always 16-bit
#define FIXED_SCALE    2     // UI scale factor
```

### Paths

```c
#define SDCARD_PATH    "/mnt/SDCARD"     // Platform-specific
#define ROMS_PATH      SDCARD_PATH "/Roms"
#define SAVES_PATH     SDCARD_PATH "/Saves"
#define BIOS_PATH      SDCARD_PATH "/Bios"
```

### Colors

```c
#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0xF800
#define COLOR_GREEN    0x07E0
#define COLOR_BLUE     0x001F
```

### Timing

```c
#define FRAME_RATE     60              // Target FPS
#define SLEEP_TIMEOUT  30000           // 30 seconds
#define POWEROFF_TIME  120000          // 2 minutes
```

---

## Usage Examples

### Simple Application

```c
#include "common/api.h"

int main(int argc, char* argv[]) {
    // Initialize
    GFX_init();
    PAD_init();
    PWR_init();

    TTF_Font* font = GFX_openFont("font.otf", 20);

    // Main loop
    int running = 1;
    while (running) {
        PAD_poll();
        PWR_update();

        if (PAD_justPressed(BTN_START)) {
            running = 0;
        }

        GFX_clear();
        GFX_drawText(10, 30, "Hello MinUI!", font, 0xFFFFFF);
        GFX_flip();
    }

    // Cleanup
    PWR_quit();
    PAD_quit();
    GFX_quit();

    return 0;
}
```

### Menu System

```c
int selection = 0;
int max_items = 10;

while (running) {
    PAD_poll();

    if (PAD_justPressed(BTN_UP) || PAD_justRepeated(BTN_UP)) {
        selection = (selection - 1 + max_items) % max_items;
    }
    if (PAD_justPressed(BTN_DOWN) || PAD_justRepeated(BTN_DOWN)) {
        selection = (selection + 1) % max_items;
    }
    if (PAD_justPressed(BTN_A)) {
        activateItem(selection);
    }

    GFX_clear();
    for (int i = 0; i < max_items; i++) {
        GFX_drawButton(10, 30 + i * 40, items[i], i == selection);
    }
    GFX_flip();
}
```
