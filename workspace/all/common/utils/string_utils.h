#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <stddef.h>

// String matching functions
int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(const char* str1, const char* str2);
int containsString(char* haystack, char* needle);
int hide(char* file_name);

// String manipulation functions
void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);

// Text line splitting
// Splits a string by '\n' into an array of line pointers
// Returns the number of lines found (max MAX_TEXT_LINES)
#define MAX_TEXT_LINES 16
int splitTextLines(char* str, char** lines, int max_lines);

#endif
