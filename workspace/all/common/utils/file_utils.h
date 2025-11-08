#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stddef.h>

// Check if a file exists
int exists(char* path);

// Create an empty file (touch)
void touch(char* path);

// Write string contents to a file
void putFile(char* path, char* contents);

// Read file contents into a buffer
void getFile(char* path, char* buffer, size_t buffer_size);

// Allocate and read file contents (caller must free)
char* allocFile(char* path);

// Write an integer to a file
void putInt(char* path, int value);

// Read an integer from a file
int getInt(char* path);

#endif
