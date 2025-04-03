#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
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
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
	int hdmi; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 2,
	.headphones = 4,
	.speaker = 8,
	.jack = 0,
	.hdmi = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

int getInt(char* path) {
	int i = 0;
	FILE *file = fopen(path, "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
void getFile(char* path, char* buffer, size_t buffer_size) {
	FILE *file = fopen(path, "r");
	if (file) {
		fseek(file, 0L, SEEK_END);
		size_t size = ftell(file);
		if (size>buffer_size-1) size = buffer_size - 1;
		rewind(file);
		fread(buffer, sizeof(char), size, file);
		fclose(file);
		buffer[size] = '\0';
	}
}
void putFile(char* path, char* contents) {
	FILE* file = fopen(path, "w");
	if (file) {
		fputs(contents, file);
		fclose(file);
	}
}
void putInt(char* path, int value) {
	char buffer[8];
	sprintf(buffer, "%d", value);
	putFile(path, buffer);
}

int exactMatch(char* str1, char* str2) {
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}


#define JACK_STATE_PATH "/sys/class/gpio/gpio150/value"
#define HDMI_STATE_PATH "/sys/class/drm/card0-HDMI-A-1/status"

static int JACK_enabled(void) {
	return !getInt(JACK_STATE_PATH);
}
static int HDMI_enabled(void) {
	char value[64];
	getFile(HDMI_STATE_PATH, value, 64);
	return exactMatch(value, "connected\n");
}

void InitSettings(void) {	
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		puts("Settings host"); // should always be keymon
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
	
	int jack = JACK_enabled();
	int hdmi = HDMI_enabled();
	printf("brightness: %i (hdmi: %i)\nspeaker: %i (jack: %i)\n", settings->brightness, hdmi, settings->speaker, jack); fflush(stdout);
	
	// both of these set volume
	SetJack(jack);
	SetHDMI(hdmi);
	
	char cmd[256];
	sprintf(cmd, "amixer sset 'Playback Path' '%s' > /dev/null 2>&1", GetJack() ? "HP" : "SPK");
	system(cmd);

	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	// system("echo $(< " BRIGHTNESS_PATH ")");
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
	if (settings->hdmi) return;
	
	int raw;
	switch (value) {
		// TODO :revisit
		case  0: raw =   1; break;	// 
		case  1: raw =   6; break;	// 
		case  2: raw =  10; break;	// 
		case  3: raw =  16; break;	// 
		case  4: raw =  32; break;	// 
		case  5: raw =  48; break;	// 
		case  6: raw =  64; break;	// 
		case  7: raw =  96; break;	// 
		case  8: raw = 128; break;	// 
		case  9: raw = 192; break;	// 
		case 10: raw = 255; break;	// 
	}
	
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (settings->hdmi) return;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;
	
	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

#define DISP_LCD_SET_BRIGHTNESS  0x102
void SetRawBrightness(int val) { // 0 - 255
	if (settings->hdmi) return;
	
	printf("SetRawBrightness(%i)\n", val); fflush(stdout);
	putInt("/sys/class/backlight/backlight/brightness", val);
}
void SetRawVolume(int val) { // 0 - 100
	printf("SetRawVolume(%i)\n", val); fflush(stdout);
	
	system("amixer sset 'SPK' 1% > /dev/null 2>&1"); // ensure there is always a change
	if (settings->jack) {
		system("amixer sset 'Playback Path' 'HP' > /dev/null 2>&1");
		puts("headphones"); fflush(stdout);
	}
	else if (val==0) {
		system("amixer sset 'Playback Path' 'OFF' > /dev/null 2>&1"); // mute speaker (not headphone as that produces white noise)
		puts("mute"); fflush(stdout);
	}
	else {
		system("amixer sset 'Playback Path' 'SPK' > /dev/null 2>&1");
		puts("speaker"); fflush(stdout);
	}
	
	char cmd[256];
	sprintf(cmd, "amixer sset 'SPK' %i%% > /dev/null 2>&1", val);
	puts(cmd); fflush(stdout);
	system(cmd);
}

// monitored and set by thread in keymon
int GetJack(void) {
	// printf("GetJack() %i\n", settings->jack); fflush(stdout);
	return settings->jack;
}
void SetJack(int value) {
	// printf("SetJack(%i)\n", value); fflush(stdout);
	
	// char cmd[256];
	// sprintf(cmd, "amixer cset name='Playback Path' '%s' &> /dev/null", value?"HP":"SPK");
	// system(cmd);
	
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetHDMI(void) {	
	// printf("GetHDMI() %i\n", settings->hdmi); fflush(stdout);
	return settings->hdmi;
}
void SetHDMI(int value) {
	// printf("SetHDMI(%i)\n", value); fflush(stdout);
	
	settings->hdmi = value;
	if (value) SetRawVolume(100); // max
	else SetVolume(GetVolume()); // restore
}

int GetMute(void) { return 0; }
void SetMute(int value) {}
