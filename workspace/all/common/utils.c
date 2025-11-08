#define _GNU_SOURCE // for strcasestr
#include "utils.h"
#include "defines.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

///////////////////////////////
// Timing utilities
///////////////////////////////

uint64_t getMicroseconds(void) {
	uint64_t ret;
	struct timeval tv;

	gettimeofday(&tv, NULL);

	ret = (uint64_t)tv.tv_sec * 1000000;
	ret += (uint64_t)tv.tv_usec;

	return ret;
}

///////////////////////////////
// String utilities
///////////////////////////////

int prefixMatch(char* pre, char* str) {
	return (strncasecmp(pre, str, strlen(pre)) == 0);
}

int suffixMatch(char* suf, char* str) {
	int len = strlen(suf);
	int offset = strlen(str) - len;
	return (offset >= 0 && strncasecmp(suf, str + offset, len) == 0);
}

int exactMatch(const char* str1, const char* str2) {
	if (!str1 || !str2)
		return 0; // NULL isn't safe here
	size_t len1 = strlen(str1);
	if (len1 != strlen(str2))
		return 0;
	return (strncmp(str1, str2, len1) == 0);
}

int containsString(char* haystack, char* needle) {
	return strcasestr(haystack, needle) != NULL;
}

int hide(char* file_name) {
	return file_name[0] == '.' || suffixMatch(".disabled", file_name) ||
	       exactMatch("map.txt", file_name);
}

void normalizeNewline(char* line) {
	int len = strlen(line);
	if (len > 1 && line[len - 1] == '\n' && line[len - 2] == '\r') { // windows!
		line[len - 2] = '\n';
		line[len - 1] = '\0';
	}
}

void trimTrailingNewlines(char* line) {
	int len = strlen(line);
	while (len > 0 && line[len - 1] == '\n') {
		line[len - 1] = '\0'; // trim newline
		len -= 1;
	}
}

void trimSortingMeta(char** str) { // eg. `001) `
	// TODO: this code is suss
	char* safe = *str;
	while (isdigit(**str))
		*str += 1; // ignore leading numbers

	if (*str[0] == ')') { // then match a closing parenthesis
		*str += 1;
	} else { //  or bail, restoring the string to its original value
		*str = safe;
		return;
	}

	while (isblank(**str))
		*str += 1; // ignore leading space
}

int splitTextLines(char* str, char** lines, int max_lines) {
	if (!str || max_lines < 1)
		return 0;

	int count = 0;
	char* tmp;
	lines[count++] = str;
	while ((tmp = strchr(lines[count - 1], '\n')) != NULL) {
		if (count + 1 > max_lines)
			break;
		lines[count++] = tmp + 1;
	}
	return count;
}

///////////////////////////////
// File I/O utilities
///////////////////////////////

int exists(char* path) {
	return access(path, F_OK) == 0;
}

void touch(char* path) {
	close(open(path, O_RDWR | O_CREAT, 0777));
}

void putFile(const char* path, const char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}

void getFile(const char* path, char* buffer, size_t buffer_size) {
	FILE* file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size > buffer_size - 1)
			size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}

char* allocFile(const char* path) { // caller must free!
	char* contents = NULL;
	FILE* file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		contents = calloc(size + 1, sizeof(char));
		if (contents) {
			fseek(file, 0L, SEEK_SET);
			fread(contents, sizeof(char), size, file);
			contents[size] = '\0';
		}
		fclose(file);
	}
	return contents;
}

int getInt(const char* path) {
	int i = 0;
	FILE* file = fopen(path, "r");
	if (file != NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}

void putInt(const char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

///////////////////////////////
// Name processing utilities
///////////////////////////////

void getDisplayName(const char* in_name, char* out_name) {
	char* tmp;
	char work_name[256];
	strcpy(work_name, in_name);
	strcpy(out_name, in_name);

	if (suffixMatch("/" PLATFORM, work_name)) { // hide platform from Tools path...
		tmp = strrchr(work_name, '/');
		tmp[0] = '\0';
	}

	// extract just the filename if necessary
	tmp = strrchr(work_name, '/');
	if (tmp)
		strcpy(out_name, tmp + 1);

	// remove extension(s), eg. .p8.png
	while ((tmp = strrchr(out_name, '.')) != NULL) {
		int len = strlen(tmp);
		if (len > 2 && len <= 5)
			tmp[0] = '\0'; // 1-4 letter extension plus dot (was 1-3, extended for .doom
		// files)
		else
			break;
	}

	// remove trailing parens (round and square)
	strcpy(work_name, out_name);
	while ((tmp = strrchr(out_name, '(')) != NULL || (tmp = strrchr(out_name, '[')) != NULL) {
		if (tmp == out_name)
			break;
		tmp[0] = '\0';
		tmp = out_name;
	}

	// make sure we haven't nuked the entire name
	if (out_name[0] == '\0')
		strcpy(out_name, work_name);

	// remove trailing whitespace
	tmp = out_name + strlen(out_name) - 1;
	while (tmp > out_name && isspace((unsigned char)*tmp))
		tmp--;
	tmp[1] = '\0';
}

void getEmuName(const char* in_name,
                char* out_name) { // NOTE: both char arrays need to be MAX_PATH length!
	char* tmp;
	strcpy(out_name, in_name);
	tmp = out_name;

	// printf("--------\n  in_name: %s\n",in_name); fflush(stdout);

	// extract just the Roms folder name if necessary
	if (prefixMatch(ROMS_PATH, tmp)) {
		tmp += strlen(ROMS_PATH) + 1;
		char* tmp2 = strchr(tmp, '/');
		if (tmp2)
			tmp2[0] = '\0';
		// printf("    tmp1: %s\n", tmp);
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = out_name;
	}

	// finally extract pak name from parenths if present
	tmp = strrchr(tmp, '(');
	if (tmp) {
		tmp += 1;
		// printf("    tmp2: %s\n", tmp);
		memmove(out_name, tmp, strlen(tmp) + 1);
		tmp = strchr(out_name, ')');
		tmp[0] = '\0';
	}

	// printf(" out_name: %s\n", out_name); fflush(stdout);
}

void getEmuPath(char* emu_name, char* pak_path) {
	sprintf(pak_path, "%s/Emus/%s/%s.pak/launch.sh", SDCARD_PATH, PLATFORM, emu_name);
	if (exists(pak_path))
		return;
	sprintf(pak_path, "%s/Emus/%s.pak/launch.sh", PAKS_PATH, emu_name);
}

///////////////////////////////
// Date/time utilities
///////////////////////////////

int isLeapYear(uint32_t year) {
	return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
}

int getDaysInMonth(int month, uint32_t year) {
	switch (month) {
	case 2:
		return isLeapYear(year) ? 29 : 28;
	case 4:
	case 6:
	case 9:
	case 11:
		return 30;
	default:
		return 31;
	}
}

void validateDateTime(int32_t* year, int32_t* month, int32_t* day, int32_t* hour, int32_t* minute,
                      int32_t* second) {
	// Month wrapping
	if (*month > 12)
		*month -= 12;
	else if (*month < 1)
		*month += 12;

	// Year clamping
	if (*year > 2100)
		*year = 2100;
	else if (*year < 1970)
		*year = 1970;

	// Day validation (depends on month and leap year)
	int max_days = getDaysInMonth(*month, *year);
	if (*day > max_days)
		*day -= max_days;
	else if (*day < 1)
		*day += max_days;

	// Time wrapping
	if (*hour > 23)
		*hour -= 24;
	else if (*hour < 0)
		*hour += 24;

	if (*minute > 59)
		*minute -= 60;
	else if (*minute < 0)
		*minute += 60;

	if (*second > 59)
		*second -= 60;
	else if (*second < 0)
		*second += 60;
}

int convertTo12Hour(int hour24) {
	if (hour24 == 0)
		return 12;
	else if (hour24 > 12)
		return hour24 - 12;
	else
		return hour24;
}

///////////////////////////////
// Math utilities
///////////////////////////////

int gcd(int a, int b) {
	return b ? gcd(b, a % b) : a;
}

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
// ARM assembly optimized version (32-bit ARM only)
uint32_t average16(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x0821;
	__asm__("eor %0, %2, %3\r\n"
	        "and %0, %0, %1\r\n"
	        "add %0, %3, %0\r\n"
	        "add %0, %0, %2\r\n"
	        "lsr %0, %0, #1\r\n"
	        : "=&r"(ret)
	        : "r"(lowbits), "r"(c1), "r"(c2)
	        :);
	return ret;
}
#else
// C fallback version
uint32_t average16(uint32_t c1, uint32_t c2) {
	return (c1 + c2 + ((c1 ^ c2) & 0x0821)) >> 1;
}
#endif

#if defined(__ARM_ARCH) && __ARM_ARCH >= 5 && !defined(__aarch64__)
// ARM assembly optimized version (32-bit ARM only)
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t ret, lowbits = 0x08210821;

	__asm__("eor %0, %3, %1\r\n"
	        "and %0, %0, %2\r\n"
	        "adds %0, %1, %0\r\n"
	        "and %1, %1, #0\r\n"
	        "movcs %1, #0x80000000\r\n"
	        "adds %0, %0, %3\r\n"
	        "rrx %0, %0\r\n"
	        "orr %0, %0, %1\r\n"
	        : "=&r"(ret), "+r"(c2)
	        : "r"(lowbits), "r"(c1)
	        : "cc");

	return ret;
}
#else
// C fallback version
uint32_t average32(uint32_t c1, uint32_t c2) {
	uint32_t sum = c1 + c2;
	uint32_t ret = sum + ((c1 ^ c2) & 0x08210821);
	uint32_t of = ((sum < c1) | (ret < sum)) ? 0x80000000 : 0;

	return (ret >> 1) | of;
}
#endif
