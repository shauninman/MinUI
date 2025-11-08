#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

// String matching functions
int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(char* str1, char* str2);
int containsString(char* haystack, char* needle);
int hide(char* file_name);

// String manipulation functions
void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);

// Timing functions
uint64_t getMicroseconds(void);

#endif
