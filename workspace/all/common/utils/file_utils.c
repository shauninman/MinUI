// File I/O utility functions
#include "file_utils.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
