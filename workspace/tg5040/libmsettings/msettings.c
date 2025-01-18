// tg5040
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
// #include <tinyalsa/mixer.h>

#include "msettings.h"

///////////////////////////////////////

#define SETTINGS_VERSION 3
typedef struct Settings {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int mute;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = 2,
	.headphones = 4,
	.speaker = 8,
	.mute = 0,
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
int exactMatch(char* str1, char* str2) {
	if (!str1 || !str2) return 0; // NULL isn't safe here
	int len1 = strlen(str1);
	if (len1!=strlen(str2)) return 0;
	return (strncmp(str1,str2,len1)==0);
}

static int is_brick = 0;

void InitSettings(void) {	
	char* device = getenv("DEVICE");
	is_brick = exactMatch("brick", device);
	
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
		settings->mute = 0;
	}
	// printf("brightness: %i\nspeaker: %i \n", settings->brightness, settings->speaker);
	 
	system("amixer sset 'Headphone' 0"); // 100%
	system("amixer sset 'digital volume' 0"); // 100%
	system("amixer sset 'DAC Swap' Off"); // Fix L/R channels
	// volume is set with 'digital volume'
	
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
	if (is_brick) {
		switch (value) {
			case 0: raw=1; break; 		// 0
			case 1: raw=8; break; 		// 8
			case 2: raw=16; break; 		// 8
			case 3: raw=32; break; 		// 16
			case 4: raw=48; break;		// 16
			case 5: raw=72; break;		// 24
			case 6: raw=96; break;		// 24
			case 7: raw=128; break;		// 32
			case 8: raw=160; break;		// 32
			case 9: raw=192; break;		// 32
			case 10: raw=255; break;	// 64
		}
	}
	else {
		switch (value) {
			case 0: raw=4; break; 		//  0
			case 1: raw=6; break; 		//  2
			case 2: raw=10; break; 		//  4
			case 3: raw=16; break; 		//  6
			case 4: raw=32; break;		// 16
			case 5: raw=48; break;		// 16
			case 6: raw=64; break;		// 16
			case 7: raw=96; break;		// 32
			case 8: raw=128; break;		// 32
			case 9: raw=192; break;		// 64
			case 10: raw=255; break;	// 64
		}
	}
	SetRawBrightness(raw);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	if (settings->mute) return 0;
	return settings->jack ? settings->headphones : settings->speaker;
}
void SetVolume(int value) { // 0-20
	if (settings->mute) return SetRawVolume(0);
	// if (settings->hdmi) return;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	int raw = value * 5;
	SetRawVolume(raw);
	SaveSettings();
}

#define DISP_LCD_SET_BRIGHTNESS  0x102
void SetRawBrightness(int val) { // 0 - 255
	// if (settings->hdmi) return;
	
	printf("SetRawBrightness(%i)\n", val); fflush(stdout);

    int fd = open("/dev/disp", O_RDWR);
	if (fd) {
	    unsigned long param[4]={0,val,0,0};
		ioctl(fd, DISP_LCD_SET_BRIGHTNESS, &param);
		close(fd);
	}
}
void SetRawVolume(int val) { // 0-100
	printf("SetRawVolume(%i)\n", val); fflush(stdout);
	if (settings->mute) val = 0;
	
	// Note: 'digital volume' mapping is reversed
	char cmd[256];
	sprintf(cmd, "amixer sset 'digital volume' -M %i%% &> /dev/null", 100-val);
	system(cmd);
	
	// Setting just 'digital volume' to 0 still plays audio quietly. Also set DAC volume to 0
	if (val == 0) system("amixer sset 'DAC volume' 0 &> /dev/null");
	else system("amixer sset 'DAC volume' 160 &> /dev/null"); // 160=0dB=max for 'DAC volume'

	// TODO: unfortunately doing it this way creating a linker nightmare
	// struct mixer *mixer = mixer_open(0);
	// struct mixer_ctl *ctl;
	//
	// // digital volume (one-time?)
	// ctl = mixer_get_ctl(mixer, 3);
	// mixer_ctl_set_value(ctl,0,0);
	//
	// // Soft Volume Master (one-time?)
	// ctl = mixer_get_ctl(mixer, 16);
	// mixer_ctl_set_value(ctl,0,255);
	// mixer_ctl_set_value(ctl,1,255);
	//
	// // DAC volume
	// ctl = mixer_get_ctl(mixer, 7);
	// mixer_ctl_set_value(ctl,0,val);
	// mixer_ctl_set_value(ctl,1,val);
	// mixer_close(mixer);
	
	// char cmd[256];
	// sprintf(cmd, "amixer sset 'digital volume' %i%% &> /dev/null", 100-val);
	// // puts(cmd); fflush(stdout);
	// system(cmd);
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
int GetMute(void) {
	return settings->mute;
}
void SetMute(int value) {
	settings->mute = value;
	if (settings->mute) SetRawVolume(0);
	else SetVolume(GetVolume());
}