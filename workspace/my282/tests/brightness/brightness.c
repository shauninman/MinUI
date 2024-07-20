#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

// stock = 20,50,70,140,200
// my panel gets really gnarly below 20

#define DISP_LCD_SET_BRIGHTNESS 0x102
#define DISP_LCD_GET_BRIGHTNESS	0x103

int main (int argc, char *argv[]) {
    int fd = open("/dev/disp", O_RDWR);
	if (fd) {
		int val = argc>1?atoi(argv[1]):-1;
	    unsigned long param[4]={0,0,0,0};
		int bl = ioctl(fd, DISP_LCD_GET_BRIGHTNESS, &param);
		printf("%i\n", bl); fflush(stdout);
		if (val>=0) {
			param[1] = val;
			ioctl(fd, DISP_LCD_SET_BRIGHTNESS, &param);
		}
		close(fd);
	}
	return 0;
}
