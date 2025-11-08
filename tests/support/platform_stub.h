// Stub platform header for testing
// Provides minimal platform definitions needed to compile utils.c

#ifndef PLATFORM_STUB_H
#define PLATFORM_STUB_H

// Platform identification
#define PLATFORM "test"

// Path definitions
#define SDCARD_PATH "/tmp/test"
#define ROMS_PATH SDCARD_PATH "/Roms"
#define ROOT_SYSTEM_PATH SDCARD_PATH "/.system/"
#define SYSTEM_PATH SDCARD_PATH "/.system/" PLATFORM
#define PAKS_PATH SYSTEM_PATH "/paks"

// Display configuration (minimal for compilation)
#define FIXED_SCALE 1
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define FIXED_PITCH 640
#define FIXED_SIZE 307200

// Button codes (not used in utils.c but needed for defines.h)
#define BUTTON_NA -1
#define CODE_NA -1
#define JOY_NA -1

#define BUTTON_POWER BUTTON_NA
#define CODE_POWER CODE_NA
#define JOY_POWER JOY_NA
#define BUTTON_MENU BUTTON_NA
#define CODE_MENU CODE_NA
#define JOY_MENU JOY_NA

#endif
