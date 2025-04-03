#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define DISP_LCD_BACKLIGHT_DISABLE	0x105
int main (int argc, char *argv[]) {
    int fd = open("/dev/disp", O_RDWR);
	if (fd) {
		int _;
		ioctl(fd, DISP_LCD_BACKLIGHT_DISABLE, &_);
		close(fd);
	}
	return 0;
}
