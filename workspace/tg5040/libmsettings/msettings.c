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

// Legacy MinUI settings
typedef struct SettingsV3 {
	int version; // future proofing
	int brightness;
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack;
} SettingsV3;

// First NextUI settings format
typedef struct SettingsV4 {
	int version; // future proofing
	int brightness;
	int colortemperature; // 0-20
	int headphones;
	int speaker;
	int mute;
	int unused[2];
	int jack; 
} SettingsV4;

// Second NextUI settings format
typedef struct SettingsV5 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV5;

// Third NextUI settings format
typedef struct SettingsV6 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV6;

typedef struct SettingsV7 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int mutedbrightness;
	int mutedcolortemperature;
	int mutedcontrast;
	int mutedsaturation;
	int mutedexposure;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV7;

typedef struct SettingsV8 {
	int version; // future proofing
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int mute;
	int contrast;
	int saturation;
	int exposure;
	int toggled_brightness;
	int toggled_colortemperature;
	int toggled_contrast;
	int toggled_saturation;
	int toggled_exposure;
	int toggled_volume;
	int unused[2]; // for future use
	// NOTE: doesn't really need to be persisted but still needs to be shared
	int jack; 
} SettingsV8;

// When incrementing SETTINGS_VERSION, update the Settings typedef and add
// backwards compatibility to InitSettings!
#define SETTINGS_VERSION 8
typedef SettingsV8 Settings;
static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = SETTINGS_DEFAULT_BRIGHTNESS,
	.colortemperature = SETTINGS_DEFAULT_COLORTEMP,
	.headphones = SETTINGS_DEFAULT_HEADPHONE_VOLUME,
	.speaker = SETTINGS_DEFAULT_VOLUME,
	.mute = 0,
	.contrast = SETTINGS_DEFAULT_CONTRAST,
	.saturation = SETTINGS_DEFAULT_SATURATION,
	.exposure = SETTINGS_DEFAULT_EXPOSURE,
	.toggled_brightness = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_colortemperature = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_contrast = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_saturation = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_exposure = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.toggled_volume = 0, // mute is default
	.jack = 0,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

int scaleBrightness(int);
int scaleColortemp(int);
int scaleContrast(int);
int scaleSaturation(int);
int scaleExposure(int);
int scaleVolume(int);

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

int peekVersion(const char *filename) {
	int version = 0;
	FILE *file = fopen(filename, "r");
	if (file) {
		fread(&version, sizeof(int), 1, file);
		fclose(file);
	}
	return version;
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

		// peek the first int from fd, it's the version
		int version = peekVersion(SettingsPath);
		if(version > 0) {
			int fd = open(SettingsPath, O_RDONLY);
			if (fd>=0) {
				if (version == SETTINGS_VERSION) {
					read(fd, settings, shm_size);
				}
				else if(version==7) {
					SettingsV7 old;
					read(fd, &old, sizeof(SettingsV7));
					// default muted
					settings->toggled_volume = 0;
					// muted* -> toggled*
					settings->toggled_brightness = old.mutedbrightness;
					settings->toggled_colortemperature = old.mutedcolortemperature;
					settings->toggled_contrast = old.mutedcontrast;
					settings->toggled_exposure = old.mutedexposure;
					settings->toggled_saturation = old.mutedsaturation;
					// copy the rest
					settings->saturation = old.saturation;
					settings->contrast = old.contrast;
					settings->exposure = old.exposure;
					settings->colortemperature = old.colortemperature;
					settings->brightness = old.brightness;
					settings->headphones = old.headphones;
					settings->speaker = old.speaker;
					settings->mute = old.mute;
					settings->jack = old.jack;
				}
				else if(version==6) {
					SettingsV6 old;
					read(fd, &old, sizeof(SettingsV6));
					// no muted* settings yet, default values used.
					settings->toggled_brightness = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
					settings->toggled_colortemperature = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
					settings->toggled_contrast = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
					settings->toggled_exposure = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
					settings->toggled_saturation = SETTINGS_DEFAULT_MUTE_NO_CHANGE;
					// copy the rest
					settings->saturation = old.saturation;
					settings->contrast = old.contrast;
					settings->exposure = old.exposure;
					settings->colortemperature = old.colortemperature;
					settings->brightness = old.brightness;
					settings->headphones = old.headphones;
					settings->speaker = old.speaker;
					settings->mute = old.mute;
					settings->jack = old.jack;
				}
				else if(version==5) {
					SettingsV5 old;
					read(fd, &old, sizeof(SettingsV5));
					// no display settings yet, default values used. 
					settings->saturation = 0;
					settings->contrast = 0;
					settings->exposure = 0;
					// copy the rest
					settings->colortemperature = old.colortemperature;
					settings->brightness = old.brightness;
					settings->headphones = old.headphones;
					settings->speaker = old.speaker;
					settings->mute = old.mute;
					settings->jack = old.jack;
				}
				else if(version==4) {
					SettingsV4 old;
					read(fd, &old, sizeof(SettingsV4));
					// colortemp was 0-20 here
					settings->colortemperature = old.colortemperature * 2;
				}
				else if(version==3) {
					SettingsV3 old;
					read(fd, &old, sizeof(SettingsV3));
					// no colortemp setting yet, default value used. 
					// copy the rest
					settings->brightness = old.brightness;
					settings->headphones = old.headphones;
					settings->speaker = old.speaker;
					settings->mute = old.mute;
					settings->jack = old.jack;
					settings->colortemperature = 20;
				}
				
				close(fd);
			}
			else {
				// load defaults
				memcpy(settings, &DefaultSettings, shm_size);
			}
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

	// This will implicitly update all other settings based on FN switch state
	SetMute(settings->mute);
}
int InitializedSettings(void) {
	return (settings != NULL);
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

///////// Getters exposed in public API

int GetBrightness(void) { // 0-10
	return settings->brightness;
}
int GetColortemp(void) { // 0-10
	return settings->colortemperature;
}
int GetVolume(void) { // 0-20
	if (settings->mute && GetMutedVolume() != SETTINGS_DEFAULT_MUTE_NO_CHANGE)
		return GetMutedVolume();
	return settings->jack ? settings->headphones : settings->speaker;
}
// monitored and set by thread in keymon
int GetJack(void) {
	return settings->jack;
}
int GetHDMI(void) {	
	// printf("GetHDMI() %i\n", settings->hdmi); fflush(stdout);
	// return settings->hdmi;
	return 0;
}
int GetMute(void) {
	return settings->mute;
}
int GetContrast(void)
{
	return settings->contrast;
}
int GetSaturation(void)
{
	return settings->saturation;
}
int GetExposure(void)
{
	return settings->exposure;
}
int GetMutedBrightness(void)
{
	return settings->toggled_brightness;
}
int GetMutedColortemp(void)
{
	return settings->toggled_colortemperature;
}
int GetMutedContrast(void)
{
	return settings->toggled_contrast;
}
int GetMutedSaturation(void)
{
	return settings->toggled_saturation;
}
int GetMutedExposure(void)
{
	return settings->toggled_exposure;
}
int GetMutedVolume(void)
{
	return settings->toggled_volume;
}

///////// Setters exposed in public API

void SetBrightness(int value) {
	SetRawBrightness(scaleBrightness(value));
	settings->brightness = value;
	SaveSettings();
}
void SetColortemp(int value) {
	SetRawColortemp(scaleColortemp(value));
	settings->colortemperature = value;
	SaveSettings();
}
void SetVolume(int value) { // 0-20
	if (settings->mute) 
		return SetRawVolume(scaleVolume(GetMutedVolume()));
	// if (settings->hdmi) return;
	
	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	SetRawVolume(scaleVolume(value));
	SaveSettings();
}
// monitored and set by thread in keymon
void SetJack(int value) {
	printf("SetJack(%i)\n", value); fflush(stdout);
	
	settings->jack = value;
	SetVolume(GetVolume());
}
void SetHDMI(int value) {
	// printf("SetHDMI(%i)\n", value); fflush(stdout);
	
	// if (settings->hdmi!=value) system("/usr/lib/autostart/common/055-hdmi-check");
	
	// settings->hdmi = value;
	// if (value) SetRawVolume(100); // max
	// else SetVolume(GetVolume()); // restore
}
void SetMute(int value) {
	settings->mute = value;
	if (settings->mute) {
		if (GetMutedVolume() != SETTINGS_DEFAULT_MUTE_NO_CHANGE)
			SetRawVolume(scaleVolume(GetMutedVolume()));
		// custom mute mode display settings
		if (GetMutedBrightness() != SETTINGS_DEFAULT_MUTE_NO_CHANGE)
			SetRawBrightness(scaleBrightness(GetMutedBrightness()));
		if(GetMutedColortemp() != SETTINGS_DEFAULT_MUTE_NO_CHANGE) 
			SetRawColortemp(scaleColortemp(GetMutedColortemp()));
		if(GetMutedContrast() != SETTINGS_DEFAULT_MUTE_NO_CHANGE) 
			SetRawContrast(scaleContrast(GetMutedContrast()));
		if(GetMutedSaturation() != SETTINGS_DEFAULT_MUTE_NO_CHANGE) 
			SetRawSaturation(scaleSaturation(GetMutedSaturation()));
		if(GetMutedExposure() != SETTINGS_DEFAULT_MUTE_NO_CHANGE) 
			SetRawExposure(scaleExposure(GetMutedExposure()));
	} 
	else {
		SetVolume(GetVolume());
		SetBrightness(GetBrightness());
		SetColortemp(GetColortemp());
		SetContrast(GetContrast());
		SetSaturation(GetSaturation());
		SetExposure(GetExposure());
	}
}
void SetContrast(int value)
{
	SetRawContrast(scaleContrast(value));
	settings->contrast = value;
	SaveSettings();
}
void SetSaturation(int value)
{
	SetRawSaturation(scaleSaturation(value));
	settings->saturation = value;
	SaveSettings();
}
void SetExposure(int value)
{
	SetRawExposure(scaleExposure(value));
	settings->exposure = value;
	SaveSettings();
}

void SetMutedBrightness(int value)
{
	settings->toggled_brightness = value;
	SaveSettings();
}

void SetMutedColortemp(int value)
{
	settings->toggled_colortemperature = value;
	SaveSettings();
}

void SetMutedContrast(int value)
{
	settings->toggled_contrast = value;
	SaveSettings();
}

void SetMutedSaturation(int value)
{
	settings->toggled_saturation = value;
	SaveSettings();
}

void SetMutedExposure(int value)
{
	settings->toggled_exposure = value;
	SaveSettings();
}

void SetMutedVolume(int value)
{
	settings->toggled_volume = value;
	SaveSettings();
}

///////// Platform specific scaling

int scaleVolume(int value) {
	return value * 5;
}

int scaleBrightness(int value) {
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
	return raw;
}
int scaleColortemp(int value) {
	int raw;
	
	switch (value) {
		case 0: raw=-200; break; 		// 8
		case 1: raw=-190; break; 		// 8
		case 2: raw=-180; break; 		// 16
		case 3: raw=-170; break;		// 16
		case 4: raw=-160; break;		// 24
		case 5: raw=-150; break;		// 24
		case 6: raw=-140; break;		// 32
		case 7: raw=-130; break;		// 32
		case 8: raw=-120; break;		// 32
		case 9: raw=-110; break;	// 64
		case 10: raw=-100; break; 		// 0
		case 11: raw=-90; break; 		// 8
		case 12: raw=-80; break; 		// 8
		case 13: raw=-70; break; 		// 16
		case 14: raw=-60; break;		// 16
		case 15: raw=-50; break;		// 24
		case 16: raw=-40; break;		// 24
		case 17: raw=-30; break;		// 32
		case 18: raw=-20; break;		// 32
		case 19: raw=-10; break;		// 32
		case 20: raw=0; break;	// 64
		case 21: raw=10; break; 		// 0
		case 22: raw=20; break; 		// 8
		case 23: raw=30; break; 		// 8
		case 24: raw=40; break; 		// 16
		case 25: raw=50; break;		// 16
		case 26: raw=60; break;		// 24
		case 27: raw=70; break;		// 24
		case 28: raw=80; break;		// 32
		case 29: raw=90; break;		// 32
		case 30: raw=100; break;		// 32
		case 31: raw=110; break;	// 64
		case 32: raw=120; break; 		// 0
		case 33: raw=130; break; 		// 8
		case 34: raw=140; break; 		// 8
		case 35: raw=150; break; 		// 16
		case 36: raw=160; break;		// 16
		case 37: raw=170; break;		// 24
		case 38: raw=180; break;		// 24
		case 39: raw=190; break;		// 32
		case 40: raw=200; break;		// 32
	}
	return raw;
}
int scaleContrast(int value) {
	int raw;
	
	switch (value) {
		// dont offer -5/ raw 0, looks like it might turn off the display completely?
		case -4: raw=10; break;
		case -3: raw=20; break;
		case -2: raw=30; break;
		case -1: raw=40; break;
		case 0: raw=50; break;
		case 1: raw=60; break;
		case 2: raw=70; break;
		case 3: raw=80; break;
		case 4: raw=90; break;
		case 5: raw=100; break;
	}
	return raw;
}
int scaleSaturation(int value) {
	int raw;
	
	switch (value) {
		case -5: raw=0; break;
		case -4: raw=10; break;
		case -3: raw=20; break;
		case -2: raw=30; break;
		case -1: raw=40; break;
		case 0: raw=50; break;
		case 1: raw=60; break;
		case 2: raw=70; break;
		case 3: raw=80; break;
		case 4: raw=90; break;
		case 5: raw=100; break;
	}
	return raw;
}
int scaleExposure(int value) {
	int raw;
	
	switch (value) {
		// stock OS also avoids setting anything lower, so we do the same here.
		case -4: raw=10; break;
		case -3: raw=20; break;
		case -2: raw=30; break;
		case -1: raw=40; break;
		case 0: raw=50; break;
		case 1: raw=60; break;
		case 2: raw=70; break;
		case 3: raw=80; break;
		case 4: raw=90; break;
		case 5: raw=100; break;
	}
	return raw;
}

///////// Platform specific, unscaled accessors

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
void SetRawColortemp(int val) { // 0 - 255
	// if (settings->hdmi) return;
	
	printf("SetRawColortemp(%i)\n", val); fflush(stdout);

	FILE *fd = fopen("/sys/class/disp/disp/attr/color_temperature", "w");
	if (fd) {
		fprintf(fd, "%i", val);
		fclose(fd);
	}
}
void SetRawVolume(int val) { // 0-100
	printf("SetRawVolume(%i)\n", val); fflush(stdout);
	if (settings->mute) val = scaleVolume(GetMutedVolume());
	
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

void SetRawContrast(int val){
	// if (settings->hdmi) return;
	
	printf("SetRawContrast(%i)\n", val); fflush(stdout);

	FILE *fd = fopen("/sys/class/disp/disp/attr/enhance_contrast", "w");
	if (fd) {
		fprintf(fd, "%i", val);
		fclose(fd);
	}
}
void SetRawSaturation(int val){
	// if (settings->hdmi) return;

	printf("SetRawSaturation(%i)\n", val); fflush(stdout);

	FILE *fd = fopen("/sys/class/disp/disp/attr/enhance_saturation", "w");
	if (fd) {
		fprintf(fd, "%i", val);
		fclose(fd);
	}
}
void SetRawExposure(int val){
	// if (settings->hdmi) return;

	printf("SetRawExposure(%i)\n", val); fflush(stdout);

	FILE *fd = fopen("/sys/class/disp/disp/attr/enhance_bright", "w");
	if (fd) {
		fprintf(fd, "%i", val);
		fclose(fd);
	}
}
