#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <pthread.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

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

static int is_plus = 0;
static int is_charging = 1;
static int screen_on = 0;
static unsigned long screen_start;
void screenOn(void) {
	screen_start = SDL_GetTicks();
	if (!screen_on) system("echo 50 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle");
	screen_on = 1;
}
void screenOff(void) {
	system("echo 0 > /sys/class/pwm/pwmchip0/pwm0/duty_cycle");
	screen_on = 0;
}
int isCharging(void) {
	if (is_plus) return (axp_read(0x00) & 0x4) > 0;
	
	int i = 0;
	FILE *file = fopen("/sys/devices/gpiochip0/gpio/gpio59/value", "r");
	if (file!=NULL) {
		fscanf(file, "%i", &i);
		fclose(file);
	}
	return i;
}
static pthread_t charging_pt;
void* chargingThread(void* arg) {
	while (1) {
		sleep(1);
		is_charging = isCharging();
	}
}

static int launch = 0;
static int input_fd;
static pthread_t input_pt;
void* inputThread(void* arg) {
	struct input_event	event;
	while (read(input_fd, &event, sizeof(event))==sizeof(event)) {
		if (event.type!=EV_KEY || event.value>1) continue;
		if (event.type==EV_KEY) screenOn();
		if (event.code==KEY_POWER) launch = 1;
	}	
}

int main(void) {
	is_plus = access("/customer/app/axp_test", F_OK)==0;
	int fb0_fd = open("/dev/fb0", O_RDWR);
	struct fb_var_screeninfo vinfo;
	ioctl(fb0_fd, FBIOGET_VSCREENINFO, &vinfo);
	int map_size = vinfo.xres * vinfo.yres * (vinfo.bits_per_pixel / 8); // 640x480x4
	char* fb0_map = (char*)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb0_fd, 0);
	
	memset(fb0_map, 0, map_size); // clear screen
	
	SDL_Surface* img = IMG_Load("/mnt/SDCARD/.system/res/charging-640-480.png"); // 24-bit opaque png
	
	uint8_t* dst = (uint8_t*)fb0_map; // rgba
	uint8_t* src = (uint8_t*)img->pixels; // bgr
	src += ((img->h * img->w) - 1) * 3;
	for (int y=0; y<img->h; y++) {
		for (int x=0; x<img->w; x++) {
			*(dst+0) = *(src+2); // r
			*(dst+1) = *(src+1); // g
			*(dst+2) = *(src+0); // b
			*(dst+3) = 0xf; // alpha
			dst += 4;
			src -= 3;
		}
	}
	SDL_FreeSurface(img);
	
	screenOn();
	
	input_fd = open("/dev/input/event0", O_RDONLY);
	pthread_create(&input_pt, NULL, &inputThread, NULL);
	
	pthread_create(&charging_pt, NULL, &chargingThread, NULL);
	
	while (!launch && is_charging) {
		if (screen_on && SDL_GetTicks()-screen_start>=3000) screenOff();
	}
	
	close(input_fd);
	
	pthread_cancel(input_pt);
	pthread_join(input_pt, NULL);
	
	pthread_cancel(charging_pt);
	pthread_join(charging_pt, NULL);

	munmap(fb0_map, map_size);
	close(fb0_fd);
	
	if (!launch) {
		system("shutdown");
		while (1) pause();
	}
	
    return EXIT_SUCCESS;
}
