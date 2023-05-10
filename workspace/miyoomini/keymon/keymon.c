// miyoomini/keymon.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <msettings.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <pthread.h>

//	Button Defines
#define	BUTTON_MENU		KEY_ESC
#define	BUTTON_POWER	KEY_POWER
#define	BUTTON_SELECT	KEY_RIGHTCTRL
#define	BUTTON_START	KEY_ENTER
#define	BUTTON_L1		KEY_E
#define	BUTTON_R1		KEY_T
#define	BUTTON_L2		KEY_TAB
#define	BUTTON_R2		KEY_BACKSPACE
#define BUTTON_PLUS		KEY_VOLUMEUP
#define BUTTON_MINUS	KEY_VOLUMEDOWN

//	for keyshm
#define VOLUME		0
#define BRIGHTNESS	1
#define VOLMAX		20
#define BRIMAX		10

//	for ev.value
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

//	for button_flag
#define SELECT_BIT	0
#define START_BIT	1
#define SELECT		(1<<SELECT_BIT)
#define START		(1<<START_BIT)

//	for DEBUG
//#define	DEBUG
#ifdef	DEBUG
#define ERROR(str)	fprintf(stderr,str"\n"); quit(EXIT_FAILURE)
#else
#define ERROR(str)	quit(EXIT_FAILURE)
#endif

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

//	Global Variables
typedef struct {
    int channel_value;
    int adc_value;
} SAR_ADC_CONFIG_READ;

#define SARADC_IOC_MAGIC                     'a'
#define IOCTL_SAR_INIT                       _IO(SARADC_IOC_MAGIC, 0)
#define IOCTL_SAR_SET_CHANNEL_READ_VALUE     _IO(SARADC_IOC_MAGIC, 1)

static SAR_ADC_CONFIG_READ  adc_config = {0,0};
static int is_charging = 0;
static int is_plus = 0;
static int eased_charge = 0;
static int sar_fd = 0;
static struct input_event	ev;
static int	input_fd = 0;
static pthread_t adc_pt;

void quit(int exitcode) {
	pthread_cancel(adc_pt);
	pthread_join(adc_pt, NULL);
	QuitSettings();
	
	if (input_fd > 0) close(input_fd);
	if (sar_fd > 0) close(sar_fd);
	exit(exitcode);
}

static int getADCValue(void) {
	if (is_plus) return axp_read(0xB9) & 0x7F;
	
	ioctl(sar_fd, IOCTL_SAR_SET_CHANNEL_READ_VALUE, &adc_config);
	
	int current_charge = 0;
	if (adc_config.adc_value>=528) {
		current_charge = adc_config.adc_value - 478;
	}
	else if (adc_config.adc_value>=512){
		current_charge = adc_config.adc_value * 2.125 - 1068;
	}
	else if (adc_config.adc_value>=480){
		current_charge = adc_config.adc_value * 0.51613 - 243.742;
	}
	
	if (current_charge<0) current_charge = 0;
	else if (current_charge>100) current_charge = 100;
	
	return current_charge;
}
static int isCharging(void) {
	if (is_plus) return (axp_read(0x00) & 0x4) > 0;
		
    int i = 0;
    FILE *file = fopen("/sys/devices/gpiochip0/gpio/gpio59/value", "r");
    if (file!=NULL) {
        fscanf(file, "%i", &i);
        fclose(file);
    }
	return i;
}
static void initADC(void) {
	is_plus = access("/customer/app/axp_test", F_OK)==0;
	sar_fd = open("/dev/sar", O_WRONLY);
	ioctl(sar_fd, IOCTL_SAR_INIT, NULL);
}
static void checkADC(void) {
	int was_charging = is_charging;
	is_charging = isCharging();

	int current_charge = getADCValue();
	
	static int first_run = 1;
	if (first_run || (was_charging && !is_charging)) {
		first_run = 0;
		eased_charge = current_charge;
	}
	else if (eased_charge<current_charge) {
		eased_charge += 1;
		if (eased_charge>100) eased_charge = 100;
	}
	else if (eased_charge>current_charge) {
		eased_charge -= 1;
		if (eased_charge<0) eased_charge = 0;
	}
	
	// new implementation
	int bat_fd = open("/tmp/battery", O_CREAT | O_WRONLY | O_TRUNC);
	if (bat_fd>0) {
		char value[3];
		sprintf(value, "%d", eased_charge);
		write(bat_fd, value, strlen(value));
		close(bat_fd);
	}
}
static void* runADC(void *arg) {
	while(1) {
		sleep(5);
		checkADC();
	}
	return 0;
}

int main (int argc, char *argv[]) {
	initADC();
	checkADC();
	pthread_create(&adc_pt, NULL, &runADC, NULL);
	
	// Set Initial Volume / Brightness
	InitSettings();
	
	input_fd = open("/dev/input/event0", O_RDONLY);

	// Main Loop
	register uint32_t val;
	register uint32_t code;
	register uint32_t button_flag = 0;
	register uint32_t menu_pressed = 0;
	register uint32_t power_pressed = 0;
	uint32_t repeat_LR = 0;
	while( read(input_fd, &ev, sizeof(ev)) == sizeof(ev) ) {
		val = ev.value;
		if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
		code = ev.code;
		switch (code) {
		case BUTTON_MENU:
			if ( val != REPEAT ) menu_pressed = val;
			break;
		case BUTTON_POWER:
			if ( val != REPEAT ) power_pressed = val;
			break;
		case BUTTON_SELECT:
			if ( val != REPEAT ) button_flag = button_flag & (~SELECT) | (val<<SELECT_BIT);
			// if (val) {
			// 	static int tick = 0;
			// 	char cmd[256];
			//
			// 	sprintf(cmd, "ps ax -o pid,nice,comm,args &> /mnt/SDCARD/%04i-nice.txt", tick);
			// 	system(cmd);
			//
			// 	sprintf(cmd, "top -b -n 1 > /mnt/SDCARD/%04i-top.txt", tick++);
			// 	system(cmd);
			// }
			break;
		case BUTTON_START:
			if ( val != REPEAT ) button_flag = button_flag & (~START) | (val<<START_BIT);
			break;
		case BUTTON_L1:
		case BUTTON_L2:
		case BUTTON_MINUS:
			if (code==BUTTON_MINUS || !is_plus) {
				if ( val == REPEAT ) {
					// Adjust repeat speed to 1/2
					val = repeat_LR;
					repeat_LR ^= PRESSED;
				} else {
					repeat_LR = 0;
				}
			
				if ( val == PRESSED ) {
					if ((is_plus && !menu_pressed) || button_flag==SELECT) {
						// VOLUMEDOWN or SELECT + L : volume down
						val = GetVolume();
						if (val>0) SetVolume(--val);
					}
					else if ((is_plus && menu_pressed) || button_flag==START) {
						// VOLUMEDOWN or START + L : brightness down
						val = GetBrightness();
						if (val>0) SetBrightness(--val);
					}
				}
			}
			break;
		case BUTTON_R1:
		case BUTTON_R2:
		case BUTTON_PLUS:
			if (code==BUTTON_PLUS || !is_plus) {
				if ( val == REPEAT ) {
					// Adjust repeat speed to 1/2
					val = repeat_LR;
					repeat_LR ^= PRESSED;
				} else {
					repeat_LR = 0;
				}
			
				if ( val == PRESSED ) {
					if ((is_plus && !menu_pressed) || button_flag==SELECT) {
						// VOLUMEUP or SELECT + R : volume up
						val = GetVolume();
						if (val<VOLMAX) SetVolume(++val);
					}
					else if ((is_plus && menu_pressed) || button_flag==START) {
						// VOLUMEUP or START + R : brightness up
						val = GetBrightness();
						if (val<BRIMAX) SetBrightness(++val);
					}
				}
			}
			break;
		default:
			break;
		}
		
		if (menu_pressed && power_pressed) {
			menu_pressed = power_pressed = 0;
			system("shutdown");
			while (1) pause();
		}
	}
	ERROR("Failed to read input event");
}
