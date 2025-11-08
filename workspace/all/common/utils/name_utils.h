#ifndef NAME_UTILS_H
#define NAME_UTILS_H

// Get display name from a file path
// Strips path, extension(s), region codes, and trailing whitespace
// Example: "/path/Game (USA).gb" -> "Game"
void getDisplayName(const char* in_name, char* out_name);

// Extract emulator name from ROM path
// Example: "/Roms/GB/game.gb" -> "GB" or "test (GB).gb" -> "GB"
void getEmuName(const char* in_name, char* out_name);

// Get the full path to an emulator's launch script
void getEmuPath(char* emu_name, char* pak_path);

#endif
