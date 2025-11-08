# MinUI Development Guide

This guide covers how to contribute to MinUI, develop new features, and debug issues.

## Getting Started

### Development Environment Setup

#### Required Tools

1. **Git** - Version control
2. **Docker Desktop** - Cross-compilation environment
3. **Code Editor** - VS Code, Vim, etc.
4. **Make** - Build system

#### Recommended Tools

- **VS Code Extensions:**
  - C/C++ (Microsoft)
  - Makefile Tools
  - Docker

- **Command Line Tools:**
  - `ripgrep` or `grep` - Code search
  - `tree` - Directory visualization
  - `hexdump` - Binary inspection

### Cloning the Repository

```bash
git clone https://github.com/shauninman/MinUI.git
cd MinUI
```

### Understanding the Codebase

1. **Read the documentation:**
   - [Architecture Overview](architecture.md)
   - [Project Structure](project-structure.md)
   - [Components Guide](components.md)

2. **Explore the code:**
   ```bash
   # Main applications
   workspace/all/minui/minui.c
   workspace/all/minarch/minarch.c

   # Common APIs
   workspace/all/common/api.c
   workspace/all/common/api.h

   # Example platform
   workspace/miyoomini/platform/platform.c
   ```

3. **Build the project:**
   ```bash
   make PLATFORM=miyoomini
   ```

## Development Workflow

### Typical Development Cycle

1. **Create a branch:**
   ```bash
   git checkout -b feature/my-new-feature
   ```

2. **Make changes:**
   - Edit source files
   - Test locally (if possible)

3. **Build:**
   ```bash
   make PLATFORM=miyoomini
   ```

4. **Test on device:**
   - Copy binaries to SD card
   - Test functionality

5. **Commit:**
   ```bash
   git add .
   git commit -m "Add new feature"
   ```

6. **Push and create PR:**
   ```bash
   git push origin feature/my-new-feature
   # Create pull request on GitHub
   ```

### Development Practices

#### Code Style

MinUI follows a consistent C style:

**Formatting:**
- 4-space indentation (no tabs)
- K&R brace style
- 80-character line limit (soft)

**Naming:**
- Functions: `camelCase` or `snake_case` (context-dependent)
- Structs: `CamelCase`
- Constants: `UPPER_CASE`
- Globals: `g_variableName`

**Example:**
```c
typedef struct {
    int width;
    int height;
    void* pixels;
} Surface;

static Surface* g_screen = NULL;

void GFX_init(void) {
    if (g_screen != NULL) {
        return;  // Already initialized
    }

    g_screen = SDL_SetVideoMode(
        FIXED_WIDTH,
        FIXED_HEIGHT,
        FIXED_BPP,
        SDL_SWSURFACE
    );
}
```

#### Comments

- Use comments for complex logic
- Document function purposes
- Explain platform-specific quirks
- Add TODO/FIXME for future work

```c
// TODO: Implement proper vsync timing
// FIXME: Memory leak on error path
// NOTE: Miyoo Mini requires double-buffering here
```

#### Error Handling

```c
// Check for errors
if (file == NULL) {
    fprintf(stderr, "Failed to open %s\n", path);
    return -1;
}

// Clean up resources
if (surface) {
    SDL_FreeSurface(surface);
    surface = NULL;
}
```

## Common Development Tasks

### Adding a New Feature to MinUI Launcher

**Example: Adding a favorites system**

1. **Define data structure:**
   ```c
   // In minui.c
   typedef struct {
       char path[256];
       int console;
   } Favorite;

   static Favorite favorites[100];
   static int favorite_count = 0;
   ```

2. **Implement functionality:**
   ```c
   void addFavorite(const char* rom_path, int console) {
       if (favorite_count >= 100) return;

       strncpy(favorites[favorite_count].path, rom_path, 255);
       favorites[favorite_count].console = console;
       favorite_count++;

       saveFavorites();
   }
   ```

3. **Add UI elements:**
   ```c
   void drawFavoritesMenu(void) {
       GFX_clear();

       for (int i = 0; i < favorite_count; i++) {
           GFX_drawText(10, 30 + i * 20, favorites[i].path);
       }

       GFX_flip();
   }
   ```

4. **Handle input:**
   ```c
   if (PAD_justPressed(BTN_X)) {
       addFavorite(current_rom, current_console);
   }
   ```

### Adding a New Option to Minarch

**Example: Adding a frame skip option**

1. **Add configuration field:**
   ```c
   // In minarch.c
   static int g_frame_skip = 0;  // 0 = off, 1 = skip 1, etc.
   ```

2. **Implement setting:**
   ```c
   void loadSettings(void) {
       // ... existing code ...
       g_frame_skip = ini_getInt("frameskip", 0);
   }

   void saveSettings(void) {
       // ... existing code ...
       ini_setInt("frameskip", g_frame_skip);
   }
   ```

3. **Add to menu:**
   ```c
   // In menu rendering code
   sprintf(buffer, "Frame Skip: %d", g_frame_skip);
   drawMenuItem(y_pos, buffer);
   ```

4. **Implement logic:**
   ```c
   static int frame_counter = 0;

   void runFrame(void) {
       core_run();

       frame_counter++;
       if (frame_counter > g_frame_skip) {
           frame_counter = 0;
           renderFrame();
       }
   }
   ```

### Modifying the Common API

**Example: Adding a new graphics primitive**

1. **Add declaration to api.h:**
   ```c
   void GFX_drawCircle(int x, int y, int radius, uint32_t color);
   ```

2. **Implement in api.c:**
   ```c
   void GFX_drawCircle(int x, int y, int radius, uint32_t color) {
       // Midpoint circle algorithm
       int x1 = 0;
       int y1 = radius;
       int d = 3 - 2 * radius;

       while (x1 <= y1) {
           putPixel(x + x1, y + y1, color);
           putPixel(x - x1, y + y1, color);
           // ... etc

           if (d < 0) {
               d = d + 4 * x1 + 6;
           } else {
               d = d + 4 * (x1 - y1) + 10;
               y1--;
           }
           x1++;
       }
   }
   ```

3. **Update all applications that use it:**
   - Rebuild minui
   - Rebuild minarch
   - Test on target platform

### Adding Platform-Specific Code

**Example: Adding HDMI detection**

1. **Check if platform supports it:**
   ```c
   // In workspace/miyoomini/platform/platform.h
   #define HAS_HDMI 1
   ```

2. **Implement detection:**
   ```c
   // In workspace/miyoomini/platform/platform.c
   int PLAT_isHDMIConnected(void) {
       FILE* f = fopen("/sys/class/hdmi/status", "r");
       if (!f) return 0;

       char status[16];
       fgets(status, 16, f);
       fclose(f);

       return strncmp(status, "connected", 9) == 0;
   }
   ```

3. **Add to platform abstraction:**
   ```c
   // In workspace/all/common/api.h
   #ifdef HAS_HDMI
   int PLAT_isHDMIConnected(void);
   #endif
   ```

4. **Use in applications:**
   ```c
   // In minui.c
   #ifdef HAS_HDMI
   if (PLAT_isHDMIConnected()) {
       // Show HDMI-specific UI
   }
   #endif
   ```

## Debugging

### Logging

MinUI applications can log to stdout/stderr:

```c
#define LOG(fmt, ...) fprintf(stderr, "[MinUI] " fmt "\n", ##__VA_ARGS__)

LOG("Loading ROM: %s", rom_path);
LOG("Battery: %d%%", battery_level);
```

**Viewing logs on device:**
```bash
# Via SSH or serial console
tail -f /var/log/messages
# or
dmesg | grep MinUI
```

### Using GDB (if available on platform)

```bash
# On device
gdbserver :1234 /mnt/SDCARD/SYSTEM/miyoomini/bin/minui.elf

# On host
arm-linux-gnueabihf-gdb minui.elf
(gdb) target remote [device-ip]:1234
(gdb) break main
(gdb) continue
```

### Printf Debugging

```c
void debugFunction(void) {
    FILE* log = fopen("/mnt/SDCARD/debug.txt", "a");
    fprintf(log, "Entered debugFunction\n");
    fprintf(log, "Variable x = %d\n", x);
    fclose(log);
}
```

### Common Debugging Scenarios

**1. Crash on startup:**
- Check for null pointer dereferences
- Verify file paths exist
- Check permissions

**2. Graphics not displaying:**
- Verify framebuffer initialization
- Check surface formats match
- Ensure flip/vsync called

**3. Input not working:**
- Check button mappings in platform.h
- Verify input devices exist
- Test with minput.elf utility

**4. Audio issues:**
- Check sample rate matches
- Verify audio device initialization
- Test buffer sizes

### Testing on Device

**Quick test cycle:**

1. **Build binary:**
   ```bash
   make -C workspace/all/minui PLATFORM=miyoomini
   ```

2. **Copy to device:**
   ```bash
   # Via network
   scp workspace/all/minui/minui.elf root@[device-ip]:/mnt/SDCARD/SYSTEM/miyoomini/bin/

   # Or copy to SD card via card reader
   cp workspace/all/minui/minui.elf /Volumes/SDCARD/SYSTEM/miyoomini/bin/
   ```

3. **Restart MinUI:**
   - Power off device
   - Power on, test changes

**Faster iteration (if SSH available):**
```bash
# On device
pkill minui
/mnt/SDCARD/SYSTEM/miyoomini/bin/minui.elf &
```

## Performance Optimization

### Profiling

**Manual timing:**
```c
#include <time.h>

clock_t start = clock();
// ... code to measure ...
clock_t end = clock();

double seconds = (double)(end - start) / CLOCKS_PER_SEC;
LOG("Operation took %.3f seconds", seconds);
```

**Frame timing:**
```c
static uint32_t last_frame_time = 0;

void measureFrameRate(void) {
    uint32_t current = SDL_GetTicks();
    uint32_t delta = current - last_frame_time;
    last_frame_time = current;

    LOG("Frame time: %d ms (%.1f FPS)", delta, 1000.0 / delta);
}
```

### Common Optimizations

**1. Reduce allocations:**
```c
// Bad
for (int i = 0; i < 1000; i++) {
    char* buffer = malloc(256);
    // ... use buffer ...
    free(buffer);
}

// Good
char buffer[256];
for (int i = 0; i < 1000; i++) {
    // ... use buffer ...
}
```

**2. Cache frequently used values:**
```c
// Bad
for (int i = 0; i < array_count; i++) {
    process(array[i], getConfigValue("setting"));
}

// Good
int setting = getConfigValue("setting");
for (int i = 0; i < array_count; i++) {
    process(array[i], setting);
}
```

**3. Use platform-specific optimizations:**
```c
#ifdef __ARM_NEON__
// Use NEON SIMD instructions
#else
// Fallback to scalar code
#endif
```

**4. Minimize file I/O:**
```c
// Cache ROM list instead of rescanning every frame
static RomList* cached_roms = NULL;

if (!cached_roms) {
    cached_roms = scanRoms();
}
```

## Platform Porting

See [Adding Platform Support](adding-platform.md) for detailed guide.

**Quick overview:**

1. Create platform directory structure
2. Implement platform.c/platform.h
3. Implement libmsettings
4. Create keymon daemon
5. Configure cores to build
6. Test and iterate

## Creating Custom Paks

See [PAKS.md](../PAKS.md) for complete guide.

**Quick example:**

```bash
mkdir -p MyTool.pak
cat > MyTool.pak/launch.sh << 'EOF'
#!/bin/sh
/mnt/SDCARD/SYSTEM/miyoomini/bin/mytool.elf
EOF
chmod +x MyTool.pak/launch.sh
```

## Contributing Guidelines

### Before Submitting a PR

1. **Test thoroughly:**
   - Test on at least one physical device
   - Test on multiple platforms if possible
   - Verify no regressions

2. **Follow code style:**
   - Match existing style
   - Add comments for complex logic
   - Update documentation if needed

3. **Keep commits clean:**
   - One logical change per commit
   - Clear commit messages
   - No merge commits (rebase)

4. **Update documentation:**
   - Update relevant .md files
   - Update README if user-visible
   - Add to PAKS.md if pak-related

### PR Description Template

```markdown
## Description
Brief description of changes

## Motivation
Why is this change needed?

## Testing
- [ ] Tested on Miyoo Mini
- [ ] Tested on RG35XX Plus
- [ ] No regressions found

## Screenshots (if UI change)
[Add screenshots]

## Checklist
- [ ] Code follows project style
- [ ] Documentation updated
- [ ] No new warnings
```

## Common Pitfalls

### 1. Platform Assumptions

**Bad:**
```c
// Assumes 640x480 screen
int x = 320;  // Center of screen
```

**Good:**
```c
// Use platform constants
int x = FIXED_WIDTH / 2;
```

### 2. Hardcoded Paths

**Bad:**
```c
FILE* f = fopen("/mnt/SDCARD/roms/game.gb", "r");
```

**Good:**
```c
char path[256];
snprintf(path, 256, "%s/Roms/Game Boy (GB)/game.gb", SDCARD_PATH);
FILE* f = fopen(path, "r");
```

### 3. Memory Leaks

**Bad:**
```c
Surface* loadImage(const char* path) {
    Surface* s = SDL_LoadBMP(path);
    return s;
    // Caller must remember to free!
}
```

**Good:**
```c
// Document ownership
Surface* loadImage(const char* path) {
    // Returns surface - caller must free with SDL_FreeSurface
    return SDL_LoadBMP(path);
}
```

### 4. Buffer Overflows

**Bad:**
```c
char buffer[256];
strcpy(buffer, user_input);  // Unsafe!
```

**Good:**
```c
char buffer[256];
strncpy(buffer, user_input, 255);
buffer[255] = '\0';
```

### 5. Race Conditions

**Bad:**
```c
// Multiple threads accessing shared state
static int counter = 0;
counter++;  // Not atomic!
```

**Good:**
```c
// Use mutexes
static SDL_mutex* counter_lock;
static int counter = 0;

SDL_LockMutex(counter_lock);
counter++;
SDL_UnlockMutex(counter_lock);
```

## Resources

### Documentation

- [Architecture Overview](architecture.md)
- [Project Structure](project-structure.md)
- [Components Guide](components.md)
- [Build System](build-system.md)
- [Platform Abstraction](platform-abstraction.md)
- [API Reference](api-reference.md)

### External Resources

- [Libretro API Documentation](https://docs.libretro.com/)
- [SDL 1.2 Documentation](https://www.libsdl.org/release/SDL-1.2.15/docs/)
- [SDL 2.0 Documentation](https://wiki.libsdl.org/)

### Community

- [MinUI GitHub Issues](https://github.com/shauninman/MinUI/issues)
- [MinUI Discussions](https://github.com/shauninman/MinUI/discussions)

## Getting Help

If you're stuck:

1. **Check existing documentation**
2. **Search GitHub issues**
3. **Ask in discussions**
4. **Create a detailed issue with:**
   - What you're trying to do
   - What you've tried
   - Error messages
   - Platform and version

## Next Steps

- [Adding Platform Support](adding-platform.md) - Port to new devices
- [API Reference](api-reference.md) - Detailed API documentation
- [Troubleshooting](troubleshooting.md) - Common issues and solutions
