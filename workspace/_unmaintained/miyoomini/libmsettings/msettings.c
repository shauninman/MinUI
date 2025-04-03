#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>

#include <mi_ao.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "msettings.h"

///////////////////////////////////////

//	mmplus axp223 (via eggs)
#define	AXPDEV	"/dev/i2c-1"
#define	AXPID	(0x34)

//
//	AXP223 write (plus)
//		32 .. bit7: Shutdown Control
//
int axp_write(unsigned char address, unsigned char val) {
	struct i2c_msg msg[1];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char buf[2];
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	buf[0] = address;
	buf[1] = val;
	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = buf;

	packets.nmsgs = 1;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if (ret < 0) return -1;
	return 0;
}

//
//	AXP223 read (plus)
//		00 .. C4/C5(USBDC connected) 00(discharging)
//			bit7: ACIN presence indication 0:ACIN not exist, 1:ACIN exists
//			bit6: Indicating whether ACIN is usable (used by axp_test)
//			bit4: Indicating whether VBUS is usable (used by axp_test)
//			bit2: Indicating the Battery current direction 0: discharging, 1: charging
//			bit0: Indicating whether the boot source is ACIN or VBUS
//		01 .. 70(charging) 30(non-charging)
//			bit6: Charge indication 0:not charge or charge finished, 1: in charging
//		B9 .. (& 0x7F) battery percentage
//
int axp_read(unsigned char address) {
	struct i2c_msg msg[2];
	struct i2c_rdwr_ioctl_data packets;
	unsigned char val;
	int ret;
	int fd = open(AXPDEV, O_RDWR);
	ioctl(fd, I2C_TIMEOUT, 5);
	ioctl(fd, I2C_RETRIES, 1);

	msg[0].addr = AXPID;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &address;
	msg[1].addr = AXPID;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &val;

	packets.nmsgs = 2;
	packets.msgs = &msg[0];
	ret = ioctl(fd, I2C_RDWR, &packets);

	close(fd);
	if(ret < 0) return -1;
	return val;
}

///////////////////////////////////////

typedef struct Settings {
	int version; // future proofing
	int brightness;
	int unused1;
	int speaker;
	int unused[4]; // for future use
} Settings;
static Settings DefaultSettings = {
	.version = 1,
	.brightness = 3,
	.speaker = 20,
};
static Settings* settings;

#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);
static int is_plus = 0;

void InitSettings(void) {
	is_plus = access("/customer/app/axp_test", F_OK)==0;
	
	sprintf(SettingsPath, "%s/msettings.bin", getenv("USERDATA_PATH"));
	
	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644); // see if it exists
	if (shm_fd==-1 && errno==EEXIST) { // already exists
		puts("Settings client");
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	}
	else { // host
		puts("Settings host");
		is_host = 1;
		// we created it so set initial size and populate
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		
		int fd = open(SettingsPath, O_RDONLY);
		if (fd>=0) {
			read(fd, settings, shm_size);
			// TODO: use settings->version for future proofing
			close(fd);
		}
		else {
			// load defaults
			memcpy(settings, &DefaultSettings, shm_size);
		}
	}
	printf("brightness: %i\nspeaker: %i\n", settings->brightness, settings->speaker);

	MI_AO_Enable(0);
	MI_AO_EnableChn(0,0);
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
	SetRawBrightness(value==0?6:value*10);
	settings->brightness = value;
	SaveSettings();
}

int GetVolume(void) { // 0-20
	return settings->speaker;
}
void SetVolume(int value) {
	int raw = -60 + value * 3;
	SetRawVolume(raw);
	settings->speaker = value;
	SaveSettings();
}

void SetRawBrightness(int val) {
	int fd = open("/sys/class/pwm/pwmchip0/pwm0/duty_cycle", O_WRONLY);
	if (fd>=0) {
		dprintf(fd,"%d",val);
		close(fd);
	}
}

#define MI_AO_SETMUTE	0x4008690d
static void setMute(int flag) {
	int fd = open("/dev/mi_ao", O_RDWR); // TODO: can this be kept open?
	if (fd >= 0) {
		int buf2[] = {0, flag};
		uint64_t buf1[] = {sizeof(buf2), (uintptr_t)buf2};
		ioctl(fd, MI_AO_SETMUTE, buf1);
		close(fd);
	}
}

void SetRawVolume(int val) {
	int old; MI_AO_GetVolume(0, &old);
	// printf("SetRawVolume(%i) // %i\n", val,old); fflush(stdout);
	if (old!=val) {
			 if (val==-60) setMute(1);
		else if (old==-60) setMute(0);
	}
	MI_AO_SetVolume(0,val);
}

int GetJack(void) {
	return 0;
}
void SetJack(int value) {
	// buh
}

int GetHDMI(void) {
	return 0;
}
void SetHDMI(int value) {
	// buh
}

int GetMute(void) { return 0; }
void SetMute(int value) {}
