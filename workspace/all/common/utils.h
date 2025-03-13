#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdbool.h>

int prefixMatch(char* pre, char* str);
int suffixMatch(char* suf, char* str);
int exactMatch(char* str1, char* str2);
int containsString(char* haystack, char* needle);
int hide(char* file_name);

char *splitString(char *str, const char *delim);
char *replaceString2(char *orig, char *rep, char *with);
size_t trimString(char *out, size_t len, const char *str, bool first);
void removeParentheses(char *str_out, const char *str_in);
void serializeTime(char *dest_str, int nTime);
int countChar(const char *str, char ch);
char *removeExtension(const char *myStr);
const char *baseName(const char *filename);
void folderPath(const char *filePath, char *folder_path);
void cleanName(char *name_out, const char *file_name);
bool pathRelativeTo(char *path_out, const char *dir_from, const char *file_to);

void getDisplayName(const char* in_name, char* out_name);
void getEmuName(const char* in_name, char* out_name);
void getEmuPath(char* emu_name, char* pak_path);

void normalizeNewline(char* line);
void trimTrailingNewlines(char* line);
void trimSortingMeta(char** str);

int exists(char* path);
void touch(char* path);
void putFile(char* path, char* contents);
char* allocFile(char* path); // caller must free
void getFile(char* path, char* buffer, size_t buffer_size);
void putInt(char* path, int value);
int getInt(char* path);

uint64_t getMicroseconds(void);

#endif
