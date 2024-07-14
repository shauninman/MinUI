#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <pthread.h>

// based on https://steward-fu.github.io/website/handheld/miyoo_a30_cpp_joystick.htm

#define SERIAL_GAMEDECK			"/dev/ttyS0"
#define MIYOO_AXIS_MAX_COUNT	16
#define MIYOO_PLAYER_MAGIC		0xFF
#define MIYOO_PLAYER_MAGIC_END	0xFE
#define MIYOO_PAD_FRAME_LEN		6

struct MIYOO_PAD_FRAME {
	uint8_t magic;
	uint8_t unused0;
	uint8_t unused1;
	uint8_t axis0;
	uint8_t axis1;
	uint8_t magicEnd;
};

static int s_fd = -1;
static int g_lastX = 0;
static int g_lastY = 0;

#define MIYOO_ADC_MIDDLE	128
#define MIYOO_ADC_RANGE		64

typedef struct Calibration {
	int x_max;
	int x_mid;
	int x_min;
	
	int y_max;
	int y_mid;
	int y_min;
} Calibration;

static Calibration calibration = {
	.x_max = MIYOO_ADC_MIDDLE+MIYOO_ADC_RANGE,
	.x_mid = MIYOO_ADC_MIDDLE,
	.x_min = MIYOO_ADC_MIDDLE-MIYOO_ADC_RANGE,
	
	.y_max = MIYOO_ADC_MIDDLE+MIYOO_ADC_RANGE,
	.y_mid = MIYOO_ADC_MIDDLE,
	.y_min = MIYOO_ADC_MIDDLE-MIYOO_ADC_RANGE,
};

static int MIYOO_ADC_DEAD_ZONE		= 10;
static int MIYOO_AXIS_INT8_DRIFT	= 5;

static struct MIYOO_PAD_FRAME s_frame = {0};
static int32_t s_miyoo_axis[MIYOO_AXIS_MAX_COUNT] = {0};
static int32_t s_miyoo_axis_last[MIYOO_AXIS_MAX_COUNT] = {0};

int uart_open(const char *port) {
	int fd = -1;

	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	if (-1 == fd) {
		printf("Failed to open uart\n");
		return -1;
	}

	if (fcntl(fd, F_SETFL, 0) < 0) {
		printf("Failed to call fcntl\n");
		return -1;
	}
	return fd;
}
static void uart_close(int fd) {
	close(fd);
}

static int uart_set(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity) {
	int i = 0;
	int speed_arr[] = {B115200, B19200, B9600, B4800, B2400, B1200, B300};
	int name_arr[] = {115200, 19200, 9600, 4800, 2400, 1200, 300};
	struct termios options = {0};

	if (tcgetattr(fd, &options) != 0) {
		printf("Failed to get uart attributes\n");
		return -1;
	}

	for (i = 0; i < sizeof(speed_arr) / sizeof(int); i++) {
		if (speed == name_arr[i]) {
			cfsetispeed(&options, speed_arr[i]);
			cfsetospeed(&options, speed_arr[i]);
		}
	}

	options.c_cflag |= CLOCAL;
	options.c_cflag |= CREAD;
	switch (flow_ctrl) {
	case 0:
		options.c_cflag &= ~CRTSCTS;
		break;
	case 1:
		options.c_cflag |= CRTSCTS;
		break;
	case 2:
		options.c_cflag |= IXON | IXOFF | IXANY;
		break;
	}

	options.c_cflag &= ~CSIZE;
	switch (databits) {
	case 5:
		options.c_cflag |= CS5;
		break;
	case 6:
		options.c_cflag |= CS6;
		break;
	case 7:
		options.c_cflag |= CS7;
		break;
	case 8:
		options.c_cflag |= CS8;
		break;
	default:
		return -1;
	}

	switch (parity) {
	case 'n':
	case 'N':
		options.c_cflag &= ~PARENB;
		options.c_iflag &= ~INPCK;
		break;
	case 'o':
	case 'O':
		options.c_cflag |= (PARODD | PARENB);
		options.c_iflag |= INPCK;
		break;
	case 'e':
	case 'E':
		options.c_cflag |= PARENB;
		options.c_cflag &= ~PARODD;
		options.c_iflag |= INPCK;
		break;
	case 's':
	case 'S':
		options.c_cflag &= ~PARENB;
		options.c_cflag &= ~CSTOPB;
		break;
	default:
		return -1;
	}

	switch (stopbits) {
	case 1:
		options.c_cflag &= ~CSTOPB;
		break;
	case 2:
		options.c_cflag |= CSTOPB;
		break;
	default:
		return -1;
	}

	options.c_oflag &= ~OPOST;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
	options.c_oflag &= ~(ONLCR | OCRNL);
	options.c_cc[VTIME] = 1;
	options.c_cc[VMIN] = 1;

	tcflush(fd, TCIFLUSH);
	if (tcsetattr(fd, TCSANOW, &options) != 0) {
		return -1;
	}
	return 0;
}
static int uart_init(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity) {
	if (uart_set(fd, speed, flow_ctrl, databits, stopbits, parity) == -1) {
		return -1;
	}
	return 0;
}
static int uart_read(int fd, char *rcv_buf, int data_len) {
	int f_sel;
	fd_set f_set;
	fd_set f_read;
	struct timeval time = {0};

	FD_ZERO(&f_read);
	FD_SET(fd, &f_read);

	time.tv_sec = 10;
	time.tv_usec = 0;
	f_sel = select(fd + 1, &f_read, NULL, NULL, &time);
	if (f_sel) {
		return read(fd, rcv_buf, data_len);
	}
	return 0;
}

static int filterDeadzone(int newAxis, int oldAxis) {
	// TODO: this causes it to ignore small changes which might be desirable as dejitter
	// but it also ignores many small changes that might add up to a big change
	// if (abs(newAxis - oldAxis) < MIYOO_AXIS_INT8_DRIFT) {
	// 	return 1;
	// }
	return 0;
}
static int limitValue8(int value) {
	if (value > 127) value = 127;
	else if (value < -128) value = -128;
	return value;
}

static void check_axis_event(void) {
	int i = 0;
	for (i = 0; i < MIYOO_AXIS_MAX_COUNT; i++) {
		if (s_miyoo_axis[i] != s_miyoo_axis_last[i]) {
			if (!filterDeadzone(s_miyoo_axis[i], s_miyoo_axis_last[i])) {
				if (i == 0) {
					g_lastX = limitValue8(s_miyoo_axis[i]);
					// printf("X %d\n", g_lastX);
				}
				else if (i == 1) {
					g_lastY = limitValue8(s_miyoo_axis[i]);
					// printf("Y %d\n", g_lastY);
				}
			}
		}
		s_miyoo_axis_last[i] = s_miyoo_axis[i];
	}
	
	// printf("x:%i-%i (%i) y:%i-%i (%i) %i,%i (%u,%u)\n", calibration.x_min,calibration.x_max,calibration.x_mid, calibration.y_min,calibration.y_max,calibration.y_mid, g_lastX,g_lastY, s_frame.axis0,s_frame.axis1);
}
static int miyoo_frame_to_axis_x(uint8_t rawX) {
	// printf("raw x: %u\n", rawX);
	
	int maxX = rawX-8;
	int minX = rawX+8;
	if (maxX>calibration.x_max || minX<calibration.x_min) {
		if (maxX>calibration.x_max) calibration.x_max = maxX;
		if (minX<calibration.x_min) calibration.x_min = minX;
		calibration.x_mid = (calibration.x_min + calibration.x_max) / 2;
	}
	
	int value = 0;
	if (rawX > calibration.x_mid && (calibration.x_max - calibration.x_mid) != 0) {
		value = (rawX - calibration.x_mid) * 128 / (calibration.x_max - calibration.x_mid);
	}
	if (rawX < calibration.x_mid && (calibration.x_mid - calibration.x_min) != 0) {
		value = (rawX - calibration.x_mid) * 128 / (calibration.x_mid - calibration.x_min);
	}

	if (value > 0 && value < MIYOO_ADC_DEAD_ZONE) {
		return 0;
	}
	if (value < 0 && value > -(MIYOO_ADC_DEAD_ZONE)) {
		return 0;
	}
	return value;
}
static int miyoo_frame_to_axis_y(uint8_t rawY) {
	// printf("raw y: %u\n", rawY);
	
	int maxY = rawY-8;
	int minY = rawY+8;
	if (maxY>calibration.y_max || minY<calibration.y_min) {
		if (maxY>calibration.y_max) calibration.y_max = maxY;
		if (minY<calibration.y_min) calibration.y_min = minY;
		calibration.y_mid = (calibration.y_min + calibration.y_max) / 2;
	}

	int value = 0;
	if (rawY > calibration.y_mid && (calibration.y_max - calibration.y_mid) != 0) {
		value = (rawY - calibration.y_mid) * 128 / (calibration.y_max - calibration.y_mid);
	}
	if (rawY < calibration.y_mid && (calibration.y_mid - calibration.y_min) != 0) {
		value = (rawY - calibration.y_mid) * 128 / (calibration.y_mid - calibration.y_min);
	}

	if (value > 0 && value < MIYOO_ADC_DEAD_ZONE) {
		return 0;
	}
	if (value < 0 && value > -(MIYOO_ADC_DEAD_ZONE)) {
		return 0;
	}
	return value;
}

static int parser_miyoo_input(const char *cmd, int len) {
	int i = 0;
	int p = 0;

	if ((!cmd) || (len < MIYOO_PAD_FRAME_LEN)) {
		return - 1;
	}

	for (i = 0; i < len - MIYOO_PAD_FRAME_LEN + 1; i += MIYOO_PAD_FRAME_LEN) {
		for (p = 0; p < MIYOO_PAD_FRAME_LEN - 1; p++) {
			if ((cmd[i] == MIYOO_PLAYER_MAGIC) && (cmd[i + MIYOO_PAD_FRAME_LEN - 1] == MIYOO_PLAYER_MAGIC_END)) {
				memcpy(&s_frame, cmd + i, sizeof(s_frame));
				break;
			}
			else {
				i++;
			}
		}
	}
	s_miyoo_axis[ABS_X] = miyoo_frame_to_axis_x(s_frame.axis0);
	s_miyoo_axis[ABS_Y] = miyoo_frame_to_axis_y(s_frame.axis1);
	check_axis_event();
	return 0;
}

static int miyoo_init_serial_input(void) {
	int err = 0;

	memset(&s_frame, 0, sizeof(s_frame));
	memset(s_miyoo_axis, 0, sizeof(s_miyoo_axis));
	memset(s_miyoo_axis_last, 0, sizeof(s_miyoo_axis_last));
	s_fd = uart_open(SERIAL_GAMEDECK);
	err = uart_init(s_fd, 9600, 0, 8, 1, 'N');
	if (s_fd <= 0) {
		return -1;
	}
	return 0;
}
static void miyoo_close_serial_input(void) {
	uart_close(s_fd);
	s_fd = -1;
}

///////////////////////////////

static char StickPath[256];
static pthread_t stick_pt;

static void printCalibration(void) {
	printf("calibration\n");
	
	printf("\tx_max: %i\n",calibration.x_max);
	printf("\tx_mid: %i\n",calibration.x_mid);
	printf("\tx_min: %i\n",calibration.x_min);
	printf("\ty_max: %i\n",calibration.y_max);
	printf("\ty_mid: %i\n",calibration.y_mid);
	printf("\ty_min: %i\n",calibration.y_min);
	fflush(stdout);
}

static void* readStick(void *arg) {
	while (1) {
		char rcv_buf[256] = {0};
		int len = uart_read(s_fd, rcv_buf, 256);
		if (len > 0) {
			rcv_buf[len] = '\0';
			parser_miyoo_input(rcv_buf, len);
		}
	}
	return NULL;
}

void Stick_init(void) {
	miyoo_init_serial_input();
	
	// load calibration data if present
	sprintf(StickPath, "%s/mstick.bin", getenv("USERDATA_PATH"));
	int fd = open(StickPath, O_RDONLY);
	if (fd>=0) {
		read(fd, &calibration, sizeof(Calibration));
		close(fd);
		
		printf("loaded stick calibration\n");
		printCalibration();
	}
	
	pthread_create(&stick_pt, NULL, &readStick, NULL);
}
void Stick_quit(void) {
	miyoo_close_serial_input();
	
	// save calibration data
	int fd = open(StickPath, O_CREAT|O_WRONLY, 0644);
	if (fd>=0) {
		write(fd, &calibration, sizeof(Calibration));
		close(fd);
		sync();

		printf("saved stick calibration\n");
		printCalibration();
	}
	
	pthread_cancel(stick_pt);
	pthread_join(stick_pt, NULL);
}
void Stick_get(int* x, int* y) { // -32768 thru 32767
	*x = g_lastX * 256;
	*y = g_lastY * 256;
}