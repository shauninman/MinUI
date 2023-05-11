#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <tinyalsa/asoundlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>

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
static int is_host = 0;
static int shm_size = sizeof(Settings);

#define HasUSBAudio() access("/dev/dsp1", F_OK)==0

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
	// TODO: this feels weird...
	SetRawBrightness(70 + (value * 5)); // 0..10 -> 70..120
	settings->brightness = value;
	SaveSettings();
	// settings->brightness = value;
	//
	// int raw;
	// switch (value) {
	// 	case  0: raw =   8; break;
	// 	case  1: raw =  12; break;
	// 	case  2: raw =  16; break;
	// 	case  3: raw =  24; break;
	// 	case  4: raw =  32; break;
	// 	case  5: raw =  48; break;
	// 	case  6: raw =  64; break;
	// 	case  7: raw =  96; break;
	// 	case  8: raw = 128; break;
	// 	case  9: raw = 192; break;
	// 	case 10: raw = 256; break;
	// }
	//
	// SetRawBrightness(raw);
	// SaveSettings();
}

int GetVolume(void) { // 0-20
	return HasUSBAudio() ? settings->headphones : settings->speaker;
}
void SetVolume(int value) {
	if (HasUSBAudio()) settings->headphones = value;
	else settings->speaker = value;

	SetRawVolume(value * 5);
	SaveSettings();
}

void SetRawBrightness(int val) { // 0 - 255
	// char cmd[256];
	// sprintf(cmd, "iodisp 0 %i", val);
	// system(cmd);
	int fd = open("/sys/class/disp/disp/attr/lcdbl", O_WRONLY);
	if (fd>=0) {
		dprintf(fd,"%d",val);
		close(fd);
	}
}
void SetRawVolume(int val) { // 0-100
	struct mixer *mixer;
	struct mixer_ctl *ctl;
#define	MAX_VOL_CTL_NUM	8
	static int mixer_vol_ctl_numbers = 0;
	static int mixer_vol_ctl_num[MAX_VOL_CTL_NUM];
	static int mixer_vol_ctl_val[MAX_VOL_CTL_NUM];
	const char *ctl_name;
	int i,j;

	if (HasUSBAudio()) {
		// for USB Headphones/Headset
		mixer = mixer_open(1);
		if (mixer != NULL) {
			// get ctl numbers/values of Volume if needed
			if (mixer_vol_ctl_numbers == 0) {
				j = mixer_get_num_ctls(mixer);
				for (i=0; i<j; i++) {
					ctl = mixer_get_ctl(mixer,i);
					if (ctl) {
						ctl_name = mixer_ctl_get_name(ctl);
						if (strstr(ctl_name,"Playback Volume") && !strstr(ctl_name,"Mic")) {
							mixer_vol_ctl_num[mixer_vol_ctl_numbers] = i;
							mixer_vol_ctl_val[mixer_vol_ctl_numbers] = mixer_ctl_get_num_values(ctl);
							if (++mixer_vol_ctl_numbers >= MAX_VOL_CTL_NUM) break;
						}
					}
				}
			}
			// set Volume to USB Headphones/Headset
			for (i=0; i < mixer_vol_ctl_numbers; i++) {
				ctl = mixer_get_ctl(mixer,mixer_vol_ctl_num[i]);
				if (ctl) {
					for (j=0; j<mixer_vol_ctl_val[i]; j++) mixer_ctl_set_percent(ctl,j,val);
				}
			}
			mixer_close(mixer);
			return;
		}
	}
	// for internal speaker
	mixer_vol_ctl_numbers = 0;
	mixer = mixer_open(0);
	if (mixer != NULL) {
		ctl = mixer_get_ctl(mixer,22);
		if (ctl) {
			mixer_ctl_set_percent(ctl,0,val);
		}
		mixer_close(mixer);
	}
}

// monitored and set by thread in keymon
int GetJack(void) {
	return HasUSBAudio();
}
void SetJack(int value) {
	// printf("SetJack(%i)\n", value); fflush(stdout);
	settings->jack = value;
	SetVolume(GetVolume());
}
