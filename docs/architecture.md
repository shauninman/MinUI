# MinUI Architecture Overview

## Design Philosophy

MinUI follows a minimalist, hardware-first approach to retro gaming on handheld devices:

1. **Minimal Dependencies** - Direct hardware access, no heavy frameworks
2. **Fast Performance** - Optimized C code with NEON acceleration where applicable
3. **Clean Abstraction** - Platform-specific code isolated from core logic
4. **Extensible Design** - Pak system allows customization without core changes
5. **User-Focused** - Simple UI, instant resume, automatic state management

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        User Layer                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  MinUI   │  │ Minarch  │  │  Clock   │  │  Tools   │   │
│  │ Launcher │  │ Frontend │  │  Utils   │  │  (Paks)  │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                     Common API Layer                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │   GFX    │  │   SND    │  │   PAD    │  │   PWR    │   │
│  │ Graphics │  │  Audio   │  │  Input   │  │  Power   │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
│  ┌──────────┐  ┌──────────┐                                │
│  │  Scaler  │  │  Utils   │                                │
│  │ Renderer │  │ Helpers  │                                │
│  └──────────┘  └──────────┘                                │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                  Platform Abstraction Layer                 │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              platform.c / platform.h                 │   │
│  │  - Video Init    - Input Polling  - Power Mgmt      │   │
│  │  - Display       - Battery        - Sleep/Wake      │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │           libmsettings (Settings Library)            │   │
│  │  - Volume        - Brightness     - Persistence     │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                      Hardware Layer                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │Framebuffer│  │  Audio   │  │  Input   │  │  Power   │   │
│  │   /dev   │  │   ALSA   │  │  evdev   │  │  /sys    │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. MinUI (Launcher)
**Location**: `/workspace/all/minui/minui.c`

The main launcher application responsible for:
- ROM browsing and organization
- Collections management
- Recent games tracking
- Settings display
- Launching emulator paks

**Key Features**:
- Smart ROM name display (hides tags, extensions)
- Automatic resume from last state
- Fast navigation with pill-style UI
- Hardware info display (volume, brightness, battery)

### 2. Minarch (Libretro Frontend)
**Location**: `/workspace/all/minarch/minarch.c`

The emulator frontend that runs libretro cores:
- Loads and executes emulator cores
- In-game menu system
- Save state management (10 slots + auto-resume)
- Video scaling and effects
- Audio processing
- Input remapping

**Key Features**:
- Quicksave on power-off
- Auto-resume from slot 9
- Per-console and per-core configuration
- Threaded video rendering option
- Disc changing for multi-disc games
- Screenshot support

### 3. Common API
**Location**: `/workspace/all/common/api.c`

Provides unified interfaces for:

**Graphics (GFX_*)**:
- Asset loading and sprite rendering
- Text rendering with TTF fonts
- UI element drawing
- Frame timing (60 FPS)

**Sound (SND_*)**:
- Audio initialization
- Sample buffering
- Ring buffer management

**Input (PAD_*)**:
- Button state tracking
- Auto-repeat logic
- Analog stick handling

**Power (PWR_*)**:
- Sleep timer management
- Battery monitoring
- Brightness/volume control
- CPU speed scaling

### 4. Platform Abstraction Layer
**Location**: `/workspace/[platform]/platform/`

Each platform implements:
- Video initialization (framebuffer or SDL)
- Input polling (keyboard, joystick, raw devices)
- Power management (battery, charging, sleep/wake)
- Display rendering and scaling
- Platform-specific features (HDMI, analog sticks, etc.)

### 5. Pak System
**Location**: `/skeleton/SYSTEM/[platform]/paks/`

Plugin architecture for emulators and tools:
- Self-contained directories with launch scripts
- Can bundle custom cores or reuse system cores
- Configuration system for core options
- Support for native applications

### 6. Background Services

**keymon** - Input monitor daemon:
- Monitors hardware buttons
- Handles global shortcuts
- Manages sleep/wake events

**syncsettings** - Settings sync daemon:
- Restores MinUI settings after third-party apps
- Prevents interference from stock applications

## Data Flow

### ROM Launch Flow
```
1. User selects ROM in MinUI
2. MinUI identifies console type from directory tag
3. MinUI locates appropriate .pak directory
4. MinUI executes launch.sh with ROM path
5. launch.sh sets up environment and calls minarch
6. minarch loads libretro core
7. minarch loads ROM and any save states
8. Core runs game loop
9. On exit, minarch saves state and settings
10. Control returns to MinUI
```

### Video Rendering Flow
```
1. Libretro core renders frame to buffer
2. minarch receives callback with frame data
3. Software scaler converts and scales (if needed)
4. GFX API blits to framebuffer or SDL surface
5. Platform layer presents to display
6. Frame timing ensures 60 FPS
```

### Input Processing Flow
```
1. keymon monitors raw input devices
2. Global shortcuts handled by keymon
3. Platform layer polls input devices
4. PAD API tracks button states
5. Applications read button state via PAD_*
6. minarch maps to libretro inputs
7. Core receives input callbacks
```

## Build Architecture

### Docker Toolchain System
```
Host Machine (macOS/Linux)
    ↓
Master Makefile
    ↓
Docker Container (per-platform toolchain)
    ↓
Platform Builds (cross-compilation)
    ↓
Skeleton Population (file copying)
    ↓
Release Packaging (.zip files)
```

### Compilation Model
- Host-driven orchestration
- Docker containers for reproducible builds
- Cross-compilation for ARM targets
- Shared code compiled per-platform
- Platform code linked into each binary

## File System Layout

### SD Card Structure
```
/
├── BOOT/              # Bootstrap files
├── SYSTEM/            # MinUI system files
│   ├── res/           # Shared resources (fonts, assets)
│   └── [platform]/    # Platform-specific files
│       ├── bin/       # Executables (.elf)
│       ├── lib/       # Libraries (.so)
│       ├── cores/     # Libretro cores
│       └── paks/      # System paks
│           └── Emus/  # Emulator paks
└── BASE/              # User-facing content
    ├── Roms/          # ROM directories
    ├── Saves/         # Save states/RAM
    └── Bios/          # BIOS files
```

## Key Design Patterns

### 1. Hardware Abstraction Layer (HAL)
Platform-specific code isolated in platform.c, common code uses standard API.

### 2. Plugin Architecture
Paks allow extensibility without modifying core code.

### 3. Libretro Integration
Standard interface for emulator cores enables easy addition of new consoles.

### 4. Double Buffering
Page flipping for smooth graphics without tearing.

### 5. Ring Buffers
Lock-free audio buffers for low-latency sound.

### 6. State Management
Automatic saving and resuming of game states.

### 7. Daemon Pattern
Background services (keymon, syncsettings) for system-wide functionality.

## Performance Optimizations

1. **NEON Intrinsics** - ARM SIMD for video scaling
2. **Direct Hardware Access** - Bypass abstraction layers
3. **Minimal Allocations** - Stack allocation where possible
4. **Software Rendering** - Full control over display pipeline
5. **Threaded Video** - Optional async rendering
6. **Asset Caching** - Load graphics once
7. **Ring Buffers** - Lock-free audio

## Portability Strategy

### Platform Independence
- Core logic in `/workspace/all/`
- No platform-specific code in common files
- SDL abstraction where appropriate

### Platform Specifics
- Each platform in `/workspace/[platform]/`
- Platform HAL implements standard interface
- Build system selects platform at compile time

### Extensibility
- Pak system for user additions
- Configuration files for customization
- Launch scripts for flexibility

## Security Considerations

MinUI runs with full system privileges on most platforms:
- Direct hardware access required for performance
- No sandboxing or privilege separation
- Trust model: user controls all content
- Paks can execute arbitrary code

This is acceptable for single-user gaming devices but would require hardening for multi-user or networked environments.

## Limitations and Trade-offs

1. **RGB565 Only** - No 24-bit color support (performance)
2. **No OpenGL** - Software rendering only (portability)
3. **SDL 1.2/2.0** - Older libraries for broader support
4. **Single-threaded** - Mostly single-threaded (simplicity)
5. **No Audio Resampling** - Core must match device rate
6. **Platform Coupling** - Some platform quirks leak into common code

These trade-offs prioritize performance, portability, and simplicity over flexibility and modern best practices.
