// Name processing utilities for MinUI
#include "name_utils.h"
#include "../defines.h"
#include "file_utils.h"
#include "string_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

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
