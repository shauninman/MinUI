// String utility functions
#define _GNU_SOURCE // for strcasestr
#include "string_utils.h"
#include <ctype.h>
#include <string.h>

///////////////////////////////////////
// String matching functions
///////////////////////////////////////

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

///////////////////////////////////////
// String manipulation functions
///////////////////////////////////////

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

///////////////////////////////////////
// Text parsing functions
///////////////////////////////////////

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
