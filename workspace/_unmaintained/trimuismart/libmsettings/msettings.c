#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <string.h>

#include "sunxi_display2.h"
#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 1
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones; // available?
	int speaker;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 3,
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int disp_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

void InitSettings(void) {
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		puts("Settings host"); // keymon
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			// TODO: use settings->version for future proofing?
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}

		// these shouldn't be persisted
		// settings->jack = 0;
	}
	printf("brightness: %i\nspeaker: %i \n", settings->brightness, settings->speaker);
	
	disp_fd = open("/dev/disp", O_RDWR);
	
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
}
void QuitSettings(void) {
	close(disp_fd);
	munmap(settings, shm_size);
	if (is_host) shm_unlink(SHM_KEY);
}
static inline void SaveSettings(void) {
	int fd = open(SettingsPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int GetBrightness(void) { // 0-10
	return settings->brightness;
}
void SetBrightness(int value) {
	settings->brightness = value;

	int raw;
	switch (value) {
		case  0: raw =   8; break;
		case  1: raw =  12; break;
		case  2: raw =  16; break;
		case  3: raw =  24; break;
		case  4: raw =  32; break;
		case  5: raw =  48; break;
		case  6: raw =  64; break;
		case  7: raw =  96; break;
		case  8: raw = 128; break;
		case  9: raw = 192; break;
		case 10: raw = 256; break;
	}
	
	SetRawBrightness(raw);
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	int raw = value * 31 / 20;
	SetRawVolume(raw);
	SaveSettings();
}

void SetRawBrightness(int val) { // 0 - 255
	// char cmd[256];
	// sprintf(cmd, "iodisp 0 %i", val);
	// system(cmd);

	u32 args[4] = {0};
	args[1] = val;
	ioctl(disp_fd, DISP_LCD_SET_BRIGHTNESS, args);
}
void SetRawVolume(int val) { // 0 - 40
	char cmd[256];
	sprintf(cmd, "amixer sset 'Lineout volume' %i", val);
	system(cmd);
}

// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}
void SetJack(int value) {
	// printf("SetJack(%i)\n", value); fflush(stdout);
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {
	return 0;
}
void SetHDMI(int value) {
	// buh
}

int GetMute(void) { return 0; }
void SetMute(int value) {}
