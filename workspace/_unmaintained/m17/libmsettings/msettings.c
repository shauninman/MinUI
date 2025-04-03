#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>

#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 2
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int unused[3]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 2,
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

// #define BACKLIGHT_PATH "/sys/class/backlight/backlight/bl_power"
// #define BRIGHTNESS_PATH "/sys/class/backlight/backlight/brightness"
// #define JACK_STATE_PATH "/sys/bus/platform/devices/singleadc-joypad/hp"
// #define HDMI_STATE_PATH "/sys/class/extcon/hdmi/cable.0/state"

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}


void InitSettings(void) {	
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		// puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		// puts("Settings host"); // keymon
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
		// settings->hdmi = 0;
	}
	// printf("brightness: %i\nspeaker: %i \n", settings->brightness, settings->speaker);
	
	// SetJack(getInt(JACK_STATE_PATH));
	// SetHDMI(getInt(HDMI_STATE_PATH));
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
}
void QuitSettings(void) {
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
	
	int raw;
	switch (value) {
		case 0: raw=8000; break;
		case 1: raw=7500; break;
		case 2: raw=7000; break;
		case 3: raw=6500; break; 
		case 4: raw=6000; break;
		case 5: raw=5000; break;
		case 6: raw=4000; break;
		case 7: raw=3000; break;
		case 8: raw=2000; break;
		case 9: raw=1000; break;
		case 10: raw=0; break;
	}
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	// if (settings->hdmi) return;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

void SetRawBrightness(int val) { // 8000-0 (>8000 == off)
	// if (settings->hdmi) return;
	printf("SetRawBrightness(%i)\n", val); fflush(stdout);
	int fd = open("/dev/gpio-pwm", O_WRONLY);
	if (fd>=0) {
		dprintf(fd,"%d",val);
		close(fd);
	}
}
void SetRawVolume(int val) { // 0 - 100
	printf("SetRawVolume(%i)\n", val); fflush(stdout);
	char cmd[256];
	sprintf(cmd, "amixer cset name='Master Playback Volume' %i%%,0 &> /dev/null", val);
	// puts(cmd); fflush(stdout);
	system(cmd);
}

// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}
void SetJack(int value) {
	printf("SetJack(%i)\n", value); fflush(stdout);

	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {	
	// printf("GetHDMI() %i\n", settings->hdmi); fflush(stdout);
	// return settings->hdmi;
	return 0;
}
void SetHDMI(int value) {
	// printf("SetHDMI(%i)\n", value); fflush(stdout);
	
	// if (settings->hdmi!=value) system("/usr/lib/autostart/common/055-hdmi-check");
	
	// settings->hdmi = value;
	// if (value) SetRawVolume(100); // max
	// else SetVolume(GetVolume()); // restore
}

int GetMute(void) { return 0; }
void SetMute(int value) {}
