/**
 * keymon.c - Miyoo Mini / Miyoo Mini Plus hardware button monitoring daemon
 *
 * Background daemon that monitors physical button presses and handles system-level
 * shortcuts on the Miyoo Mini and Miyoo Mini Plus handheld devices. This is the
 * most complex keymon implementation, featuring:
 *
 * - Dual hardware variant support (standard Miyoo Mini and Plus model)
 * - Battery monitoring via SAR ADC (Mini) or AXP223 PMIC (Plus)
 * - Charging state detection
 * - Volume and brightness control through button combinations
 * - MENU+POWER shutdown combination
 * - Hardware volume button support on Plus model
 *
 * Button combinations:
 * - Standard Miyoo Mini:
 *   - SELECT+L1/R1: Adjust volume
 *   - START+L1/R1: Adjust brightness
 * - Miyoo Mini Plus:
 *   - VOLUMEUP/DOWN alone: Adjust volume
 *   - MENU+VOLUMEUP/DOWN: Adjust brightness
 *   - L1/R1 also work as alternatives
 * - Both models:
 *   - MENU+POWER: System shutdown
 *
 * The daemon auto-detects the hardware variant by checking for /customer/app/axp_test
 * and uses the appropriate battery monitoring interface.
 *
 * Runs continuously polling input events and battery status.
 */

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

// Button code definitions (from Linux input.h)
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

// System limits
#define VOLUME		0
#define BRIGHTNESS	1
#define VOLMAX		20
#define BRIMAX		10

// Input event values from linux/input.h
#define RELEASED	0
#define PRESSED		1
#define REPEAT		2

// Button state bit flags for tracking SELECT/START combinations
#define SELECT_BIT	0
#define START_BIT	1
#define SELECT		(1<<SELECT_BIT)
#define START		(1<<START_BIT)

// Debug error handling
//#define	DEBUG
#ifdef	DEBUG
#define ERROR(str)	fprintf(stderr,str"\n"); quit(EXIT_FAILURE)
#else
#define ERROR(str)	quit(EXIT_FAILURE)
#endif

// Miyoo Mini Plus AXP223 PMIC I2C configuration (via eggs)
#define	AXPDEV	"/dev/i2c-1"
#define	AXPID	(0x34)

/**
 * Writes a register value to the AXP223 PMIC via I2C.
 *
 * Used on Miyoo Mini Plus to control power management features.
 * Register 0x32 bit7 controls system shutdown.
 *
 * @param address Register address to write
 * @param val Value to write to register
 * @return 0 on success, -1 on I2C communication failure
 *
 * @note Only available on Miyoo Mini Plus (has AXP223 PMIC)
 */
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

/**
 * Reads a register value from the AXP223 PMIC via I2C.
 *
 * Used on Miyoo Mini Plus for battery and charging status:
 * - Register 0x00: Power status
 *   - bit7: ACIN presence (0=not present, 1=present)
 *   - bit6: ACIN usable
 *   - bit4: VBUS usable
 *   - bit2: Battery current direction (0=discharging, 1=charging)
 *   - bit0: Boot source (ACIN or VBUS)
 * - Register 0x01: Charging status
 *   - bit6: Charge indication (0=not charging/finished, 1=charging)
 * - Register 0xB9: Battery percentage (masked with 0x7F)
 *
 * @param address Register address to read
 * @return Register value (0-255), or -1 on I2C communication failure
 *
 * @note Only available on Miyoo Mini Plus (has AXP223 PMIC)
 */
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

// SAR ADC configuration structure for standard Miyoo Mini battery monitoring
typedef struct {
    int channel_value;
    int adc_value;
} SAR_ADC_CONFIG_READ;

#define SARADC_IOC_MAGIC                     'a'
#define IOCTL_SAR_INIT                       _IO(SARADC_IOC_MAGIC, 0)
#define IOCTL_SAR_SET_CHANNEL_READ_VALUE     _IO(SARADC_IOC_MAGIC, 1)

static SAR_ADC_CONFIG_READ  adc_config = {0,0};
static int is_charging = 0;           // Current charging state
static int is_plus = 0;               // Hardware variant flag (1=Plus, 0=standard)
static int eased_charge = 0;          // Smoothed battery percentage
static int sar_fd = 0;                // SAR ADC file descriptor
static struct input_event	ev;
static int	input_fd = 0;
static pthread_t adc_pt;              // Battery monitoring thread

/**
 * Clean shutdown handler.
 *
 * Cancels battery monitoring thread, closes file descriptors, and exits.
 *
 * @param exitcode Exit status code
 */
void quit(int exitcode) {
	pthread_cancel(adc_pt);
	pthread_join(adc_pt, NULL);
	QuitSettings();

	if (input_fd > 0) close(input_fd);
	if (sar_fd > 0) close(sar_fd);
	exit(exitcode);
}

/**
 * Reads current battery percentage.
 *
 * On Miyoo Mini Plus: Reads directly from AXP223 register 0xB9 (0-100%).
 * On standard Miyoo Mini: Reads SAR ADC and applies piecewise linear calibration
 * curve to convert raw ADC values to percentage.
 *
 * @return Battery percentage (0-100)
 *
 * @note ADC calibration curve is hardware-specific to Miyoo Mini
 */
static int getADCValue(void) {
	if (is_plus) return axp_read(0xB9) & 0x7F;

	ioctl(sar_fd, IOCTL_SAR_SET_CHANNEL_READ_VALUE, &adc_config);

	// Piecewise linear calibration for standard Miyoo Mini
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

	// Clamp to valid range
	if (current_charge<0) current_charge = 0;
	else if (current_charge>100) current_charge = 100;

	return current_charge;
}

/**
 * Checks if device is currently charging.
 *
 * On Miyoo Mini Plus: Reads AXP223 register 0x00 bit2 (charging status).
 * On standard Miyoo Mini: Reads GPIO59 value.
 *
 * @return 1 if charging, 0 if not charging
 */
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

/**
 * Initializes battery monitoring hardware.
 *
 * Auto-detects hardware variant by checking for /customer/app/axp_test file
 * (present on Miyoo Mini Plus only). Opens appropriate battery monitoring
 * interface (SAR ADC for standard, AXP223 I2C for Plus).
 */
static void initADC(void) {
	is_plus = access("/customer/app/axp_test", F_OK)==0;
	sar_fd = open("/dev/sar", O_WRONLY);
	ioctl(sar_fd, IOCTL_SAR_INIT, NULL);
}
/**
 * Updates battery status and writes to /tmp/battery file.
 *
 * Reads current battery percentage and charging state, applies easing
 * to smooth out fluctuations (1% per call), and writes the result to
 * /tmp/battery for use by MinUI.
 *
 * Easing is reset on first run or when transitioning from charging to
 * not charging (to quickly reflect fully charged state).
 */
static void checkADC(void) {
	int was_charging = is_charging;
	is_charging = isCharging();

	int current_charge = getADCValue();

	static int first_run = 1;
	if (first_run || (was_charging && !is_charging)) {
		// Immediately update on first run or when unplugging fully charged
		first_run = 0;
		eased_charge = current_charge;
	}
	else if (eased_charge<current_charge) {
		// Gradually increase (1% per update)
		eased_charge += 1;
		if (eased_charge>100) eased_charge = 100;
	}
	else if (eased_charge>current_charge) {
		// Gradually decrease (1% per update)
		eased_charge -= 1;
		if (eased_charge<0) eased_charge = 0;
	}

	// Write battery percentage to file for MinUI to read
	int bat_fd = open("/tmp/battery", O_CREAT | O_WRONLY | O_TRUNC);
	if (bat_fd>0) {
		char value[3];
		sprintf(value, "%d", eased_charge);
		write(bat_fd, value, strlen(value));
		close(bat_fd);
	}
}

/**
 * Background thread for battery monitoring.
 *
 * Updates battery status every 5 seconds.
 *
 * @param arg Thread argument (unused)
 * @return Never returns (runs infinite loop)
 */
static void* runADC(void *arg) {
	while(1) {
		sleep(5);
		checkADC();
	}
	return 0;
}

/**
 * Main event loop for hardware button monitoring.
 *
 * Continuously polls input device for button events and handles:
 * - Standard Miyoo Mini:
 *   - SELECT+L1/R1/L2/R2: Adjust volume
 *   - START+L1/R1/L2/R2: Adjust brightness
 * - Miyoo Mini Plus:
 *   - VOLUMEUP/DOWN alone: Adjust volume
 *   - MENU+VOLUMEUP/DOWN: Adjust brightness
 *   - L1/R1/L2/R2 also work as alternatives
 * - Both models:
 *   - MENU+POWER: System shutdown
 *
 * Also starts battery monitoring thread to update /tmp/battery every 5 seconds.
 *
 * Implements custom repeat handling (halved repeat rate: every other REPEAT event).
 *
 * @param argc Argument count (unused)
 * @param argv Argument values (unused)
 * @return Never returns (runs infinite loop)
 */
int main (int argc, char *argv[]) {
	// Initialize battery monitoring
	initADC();
	checkADC();
	pthread_create(&adc_pt, NULL, &runADC, NULL);

	// Initialize settings (volume/brightness)
	InitSettings();

	input_fd = open("/dev/input/event0", O_RDONLY);

	// Main button processing loop
	register uint32_t val;
	register uint32_t code;
	register uint32_t button_flag = 0;  // Bit flags for SELECT/START state
	register uint32_t menu_pressed = 0;
	register uint32_t power_pressed = 0;
	uint32_t repeat_LR = 0;  // Toggle for halving repeat rate
	while( read(input_fd, &ev, sizeof(ev)) == sizeof(ev) ) {
		val = ev.value;
		// Only process key events (PRESSED, RELEASED, or REPEAT)
		if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
		code = ev.code;

		// Process hardware button events
		switch (code) {
		case BUTTON_MENU:
			// Ignore REPEAT events for MENU (modifier key)
			if ( val != REPEAT ) menu_pressed = val;
			break;
		case BUTTON_POWER:
			// Ignore REPEAT events for POWER
			if ( val != REPEAT ) power_pressed = val;
			break;
		case BUTTON_SELECT:
			// Update SELECT bit in button_flag (ignore REPEAT)
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
			// Update START bit in button_flag (ignore REPEAT)
			if ( val != REPEAT ) button_flag = button_flag & (~START) | (val<<START_BIT);
			break;
		case BUTTON_L1:
		case BUTTON_L2:
		case BUTTON_MINUS:
			// Volume/brightness down (MINUS on Plus, L1/L2 on both)
			if (code==BUTTON_MINUS || !is_plus) {
				if ( val == REPEAT ) {
					// Halve repeat rate (process every other REPEAT event)
					val = repeat_LR;
					repeat_LR ^= PRESSED;
				} else {
					repeat_LR = 0;
				}

				if ( val == PRESSED ) {
					if ((is_plus && !menu_pressed) || button_flag==SELECT) {
						// Plus: VOLUMEDOWN alone, or Mini: SELECT+L
						val = GetVolume();
						if (val>0) SetVolume(--val);
					}
					else if ((is_plus && menu_pressed) || button_flag==START) {
						// Plus: MENU+VOLUMEDOWN, or Mini: START+L
						val = GetBrightness();
						if (val>0) SetBrightness(--val);
					}
				}
			}
			break;
		case BUTTON_R1:
		case BUTTON_R2:
		case BUTTON_PLUS:
			// Volume/brightness up (PLUS on Plus, R1/R2 on both)
			if (code==BUTTON_PLUS || !is_plus) {
				if ( val == REPEAT ) {
					// Halve repeat rate (process every other REPEAT event)
					val = repeat_LR;
					repeat_LR ^= PRESSED;
				} else {
					repeat_LR = 0;
				}

				if ( val == PRESSED ) {
					if ((is_plus && !menu_pressed) || button_flag==SELECT) {
						// Plus: VOLUMEUP alone, or Mini: SELECT+R
						val = GetVolume();
						if (val<VOLMAX) SetVolume(++val);
					}
					else if ((is_plus && menu_pressed) || button_flag==START) {
						// Plus: MENU+VOLUMEUP, or Mini: START+R
						val = GetBrightness();
						if (val<BRIMAX) SetBrightness(++val);
					}
				}
			}
			break;
		default:
			break;
		}
		
		// MENU+POWER: Initiate system shutdown
		if (menu_pressed && power_pressed) {
			menu_pressed = power_pressed = 0;
			system("shutdown");
			while (1) pause();  // Wait for shutdown to complete
		}
	}
	ERROR("Failed to read input event");
}
