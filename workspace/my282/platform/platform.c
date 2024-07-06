// rgb30
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"

#include "scaler.h"

///////////////////////////////
#include <termios.h>
#define ABS_X			0x00
#define ABS_Y			0x01

// from https://steward-fu.github.io/website/handheld/miyoo_a30_cpp_joystick.htm

#define SERIAL_GAMEDECK         "/dev/ttyS0"
#define MIYOO_AXIS_MAX_COUNT    16
#define MIYOO_PLAYER_MAGIC      0xFF
#define MIYOO_PLAYER_MAGIC_END  0xFE
#define MIYOO_PAD_FRAME_LEN     6
 
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
 
static int MIYOO_ADC_MAX_X  = 200;
static int MIYOO_ADC_ZERO_X = 137;
static int MIYOO_ADC_MIN_X  = 76;
 
static int MIYOO_ADC_MAX_Y  = 200;
static int MIYOO_ADC_ZERO_Y = 135;
static int MIYOO_ADC_MIN_Y  = 72;
 
static int MIYOO_ADC_DEAD_ZONE   = 10;
static int MIYOO_AXIS_INT8_DRIFT = 5;
 
static struct MIYOO_PAD_FRAME s_frame = {0};
static int32_t s_miyoo_axis[MIYOO_AXIS_MAX_COUNT] = {0};
static int32_t s_miyoo_axis_last[MIYOO_AXIS_MAX_COUNT] = {0};
 
int uart_open(const char *port)
{
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
 
static void uart_close(int fd)
{
    close(fd);
}
 
static int uart_set(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{
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
 
static int uart_init(int fd, int speed, int flow_ctrl, int databits, int stopbits, int parity)
{
    if (uart_set(fd, speed, flow_ctrl, databits, stopbits, parity) == -1) {
        return -1;
    }
    return 0;
}
 
static int uart_read(int fd, char *rcv_buf, int data_len)
{
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
 
static void defaule_cal_config(void)
{
    MIYOO_ADC_MAX_X = 200;
    MIYOO_ADC_ZERO_X = 137;
    MIYOO_ADC_MIN_X = 76;
    MIYOO_ADC_MAX_Y = 200;
    MIYOO_ADC_ZERO_Y = 135;
    MIYOO_ADC_MIN_Y = 72;
}
 
static int filterDeadzone(int newAxis, int oldAxis)
{
    if (abs(newAxis - oldAxis) < 5) {
        return 1;
    }
    return 0;
}
 
static int limitValue8(int value)
{
    if (value > 127) {
        value = 127;
    }
    else if (value < -128) {
        value = -128;
    }
    return value;
}
 
static void check_axis_event(void)
{
	int i = 0;
    for (i = 0; i < MIYOO_AXIS_MAX_COUNT; i++) {
        if (s_miyoo_axis[i] != s_miyoo_axis_last[i]) {
            if (!filterDeadzone(s_miyoo_axis[i], s_miyoo_axis_last[i])) {
                if (i == 0) {
                   g_lastX = limitValue8(s_miyoo_axis[i]);
                    // printf("X %d\n", g_lastX);
				    pad.laxis.x = g_lastX * 256;
                }
                else if (i == 1) {
                    g_lastY = limitValue8(s_miyoo_axis[i]);
                    // printf("Y %d\n", g_lastY);
					pad.laxis.y = g_lastY * 256;
                }
            }
        }
        s_miyoo_axis_last[i] = s_miyoo_axis[i];
    }
}
 
static int miyoo_frame_to_axis_x(uint8_t rawX)
{
    int value = 0;
 
    if (rawX > MIYOO_ADC_ZERO_X) {
        value = (rawX - MIYOO_ADC_ZERO_X) * 126 / (MIYOO_ADC_MAX_X - MIYOO_ADC_ZERO_X);
    }
 
    if (rawX < MIYOO_ADC_ZERO_X) {
        value = (rawX - MIYOO_ADC_ZERO_X) * 126 / (MIYOO_ADC_ZERO_X - MIYOO_ADC_MIN_X);
    }
 
    if (value > 0 && value < MIYOO_ADC_DEAD_ZONE) {
        return 0;
    }
 
    if (value < 0 && value > -(MIYOO_ADC_DEAD_ZONE)) {
        return 0;
    }
    return value;
}
 
static int miyoo_frame_to_axis_y(uint8_t rawY)
{
    int value = 0;
 
    if (rawY > MIYOO_ADC_ZERO_Y) {
        value = (rawY - MIYOO_ADC_ZERO_Y) * 126 / (MIYOO_ADC_MAX_Y - MIYOO_ADC_ZERO_Y);
    }
 
    if (rawY < MIYOO_ADC_ZERO_Y) {
        value = (rawY - MIYOO_ADC_ZERO_Y) * 126 / (MIYOO_ADC_ZERO_Y - MIYOO_ADC_MIN_Y);
    }
 
    if (value > 0 && value < MIYOO_ADC_DEAD_ZONE) {
        return 0;
    }
 
    if (value < 0 && value > -(MIYOO_ADC_DEAD_ZONE)) {
        return 0;
    }
    return value;
}
 
static int parser_miyoo_input(const char *cmd, int len)
{
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
 
static int miyoo_init_serial_input(void)
{
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
 
static void miyoo_close_serial_input(void)
{
    uart_close(s_fd);
    s_fd = -1;
}

///////////////////////////////

#define RAW_UP		103
#define RAW_DOWN	108
#define RAW_LEFT	105
#define RAW_RIGHT	106
#define RAW_A		 57
#define RAW_B		 29
#define RAW_X		 42
#define RAW_Y		 56
#define RAW_START	 28
#define RAW_SELECT	 97
#define RAW_MENU	  1
#define RAW_L1		 18
#define RAW_L2		 15
#define RAW_R1		 20
#define RAW_R2		 14
#define RAW_PLUS	115
#define RAW_MINUS	114
#define RAW_POWER	116

#define INPUT_COUNT 2
static int inputs[INPUT_COUNT];

void PLAT_initInput(void) {
	inputs[0] = open("/dev/input/event0", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // power
	inputs[1] = open("/dev/input/event3", O_RDONLY | O_NONBLOCK | O_CLOEXEC); // controller
	miyoo_init_serial_input(); // analog
}
void PLAT_quitInput(void) {
	miyoo_close_serial_input();
	for (int i=0; i<INPUT_COUNT; i++) {
		close(inputs[i]);
	}
}

// from <linux/input.h> which has BTN_ constants that conflict with platform.h
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EV_KEY			0x01
#define EV_ABS			0x03

void PLAT_pollInput(void) {
	// reset transient state
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	for (int i=0; i<BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick>=pad.repeat_at[i])) {
			pad.just_repeated |= btn; // set
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}
	
	// the actual poll
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type!=EV_KEY && event.type!=EV_ABS) continue;

			int btn = BTN_NONE;
			int pressed = 0; // 0=up,1=down
			int id = -1;
			int type = event.type;
			int code = event.code;
			int value = event.value;
			
			// TODO: tmp, hardcoded, missing some buttons
			if (type==EV_KEY) {
				if (value>1) continue; // ignore repeats
			
				pressed = value;
				// LOG_info("key event: %i (%i)\n", code,pressed);
					 if (code==RAW_UP) 		{ btn = BTN_UP; 		id = BTN_ID_UP; }
	 			else if (code==RAW_DOWN)	{ btn = BTN_DOWN; 		id = BTN_ID_DOWN; }
				else if (code==RAW_LEFT)	{ btn = BTN_LEFT; 		id = BTN_ID_LEFT; }
				else if (code==RAW_RIGHT)	{ btn = BTN_RIGHT; 		id = BTN_ID_RIGHT; }
				else if (code==RAW_A)		{ btn = BTN_A; 			id = BTN_ID_A; }
				else if (code==RAW_B)		{ btn = BTN_B; 			id = BTN_ID_B; }
				else if (code==RAW_X)		{ btn = BTN_X; 			id = BTN_ID_X; }
				else if (code==RAW_Y)		{ btn = BTN_Y; 			id = BTN_ID_Y; }
				else if (code==RAW_START)	{ btn = BTN_START; 		id = BTN_ID_START; }
				else if (code==RAW_SELECT)	{ btn = BTN_SELECT; 	id = BTN_ID_SELECT; }
				else if (code==RAW_MENU)	{ btn = BTN_MENU; 		id = BTN_ID_MENU; }
				else if (code==RAW_L1)		{ btn = BTN_L1; 		id = BTN_ID_L1; }
				else if (code==RAW_L2)		{ btn = BTN_L2; 		id = BTN_ID_L2; }
				else if (code==RAW_R1)		{ btn = BTN_R1; 		id = BTN_ID_R1; }
				else if (code==RAW_R2)		{ btn = BTN_R2; 		id = BTN_ID_R2; }
				else if (code==RAW_PLUS)	{ btn = BTN_PLUS; 		id = BTN_ID_PLUS; }
				else if (code==RAW_MINUS)	{ btn = BTN_MINUS; 		id = BTN_ID_MINUS; }
				else if (code==RAW_POWER)	{ btn = BTN_POWER; 		id = BTN_ID_POWER; }
			}
			
			if (btn==BTN_NONE) continue;
		
			if (!pressed) {
				pad.is_pressed		&= ~btn; // unset
				pad.just_repeated	&= ~btn; // unset
				pad.just_released	|= btn; // set
			}
			else if ((pad.is_pressed & btn)==BTN_NONE) {
				pad.just_pressed	|= btn; // set
				pad.just_repeated	|= btn; // set
				pad.is_pressed		|= btn; // set
				pad.repeat_at[id]	= tick + PAD_REPEAT_DELAY;
			}
		}
	}
	
	// analog
	char rcv_buf[256] = {0};
	int len = uart_read(s_fd, rcv_buf, 256);
	if (len > 0) {
		rcv_buf[len] = '\0';
		parser_miyoo_input(rcv_buf, len);
	}
}

int PLAT_shouldWake(void) {
	int input;
	static struct input_event event;
	for (int i=0; i<INPUT_COUNT; i++) {
		input = inputs[i];
		while (read(input, &event, sizeof(event))==sizeof(event)) {
			if (event.type==EV_KEY && event.code==RAW_POWER && event.value==0) {
				return 1;
			}
		}
	}
	return 0;
}

///////////////////////////////

// based on rg35xxplus

static struct VID_Context {
	SDL_Window* window;
	SDL_Renderer* renderer;
	SDL_Texture* texture;
	SDL_Texture* target;
	SDL_Surface* buffer;
	SDL_Surface* screen;
	
	GFX_Renderer* blit; // yeesh
	
	int width;
	int height;
	int pitch;
	int sharpness;
} vid;


static int device_width;
static int device_height;
static int device_pitch;
static int rotate = 0;
SDL_Surface* PLAT_initVideo(void) {
	// LOG_info("PLAT_initVideo\n");
	
	SDL_InitSubSystem(SDL_INIT_VIDEO);
	SDL_ShowCursor(0);
	
	// SDL_version compiled;
	// SDL_version linked;
	// SDL_VERSION(&compiled);
	// SDL_GetVersion(&linked);
	// LOG_info("Compiled SDL version %d.%d.%d ...\n", compiled.major, compiled.minor, compiled.patch);
	// LOG_info("Linked SDL version %d.%d.%d.\n", linked.major, linked.minor, linked.patch);
	//
	// int num_displays = SDL_GetNumVideoDisplays();
	// LOG_info("SDL_GetNumVideoDisplays(): %i\n", num_displays);
	//
	// LOG_info("Available video drivers:\n");
	// for (int i=0; i<SDL_GetNumVideoDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetVideoDriver(i));
	// }
	// LOG_info("Current video driver: %s\n", SDL_GetCurrentVideoDriver());
	//
	// LOG_info("Available render drivers:\n");
	// for (int i=0; i<SDL_GetNumRenderDrivers(); i++) {
	// 	SDL_RendererInfo info;
	// 	SDL_GetRenderDriverInfo(i,&info);
	// 	LOG_info("- %s\n", info.name);
	// }
	//
	// LOG_info("Available display modes:\n");
	// SDL_DisplayMode mode;
	// for (int i=0; i<SDL_GetNumDisplayModes(0); i++) {
	// 	SDL_GetDisplayMode(0, i, &mode);
	// 	LOG_info("- %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	// }
	// SDL_GetCurrentDisplayMode(0, &mode);
	// LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));

	// LOG_info("Available audio drivers:\n");
	// for (int i=0; i<SDL_GetNumAudioDrivers(); i++) {
	// 	LOG_info("- %s\n", SDL_GetAudioDriver(i));
	// }
	// LOG_info("Current audio driver: %s\n", SDL_GetCurrentAudioDriver()); // NOTE: hadn't been selected yet so will always be NULL!

	// SDL_SetHint(SDL_HINT_RENDER_VSYNC,"0"); // ignored

	int w = FIXED_WIDTH;
	int h = FIXED_HEIGHT;
	int p = FIXED_PITCH;
	vid.window   = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w,h, SDL_WINDOW_SHOWN);
	LOG_info("window size: %ix%i\n", w,h);
	
	SDL_DisplayMode mode;
	SDL_GetCurrentDisplayMode(0, &mode);
	LOG_info("Current display mode: %ix%i (%s)\n", mode.w,mode.h, SDL_GetPixelFormatName(mode.format));
	if (mode.h>mode.w) rotate = 3;
	vid.renderer = SDL_CreateRenderer(vid.window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
	// SDL_RenderSetLogicalSize(vid.renderer, w,h); // TODO: wrong, but without and with the below it's even wrong-er
	
	// int renderer_width,renderer_height;
	// SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// LOG_info("output size: %ix%i\n", renderer_width, renderer_height);
	// if (renderer_width!=w) { // I think this can only be hdmi
	// 	float x_scale = (float)renderer_width / w;
	// 	float y_scale = (float)renderer_height / h;
	// 	SDL_SetWindowSize(vid.window, w / x_scale, h / y_scale);
	//
	// 	SDL_GetRendererOutputSize(vid.renderer, &renderer_width, &renderer_height);
	// 	LOG_info("adjusted size: %ix%i\n", renderer_width, renderer_height);
	// 	x_scale = (float)renderer_width / w;
	// 	y_scale = (float)renderer_height / h;
	// 	SDL_RenderSetScale(vid.renderer, x_scale,y_scale);
	//
	// 	// for some reason we need to clear and present
	// 	// after setting the window size or we'll miss
	// 	// the first frame
	// 	SDL_RenderClear(vid.renderer);
	// 	SDL_RenderPresent(vid.renderer);
	// }
	
	// SDL_RendererInfo info;
	// SDL_GetRendererInfo(vid.renderer, &info);
	// LOG_info("Current render driver: %s\n", info.name);
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"1"); // linear
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	vid.target	= NULL; // only needed for non-native sizes
	
	// TODO: doesn't work here
	// SDL_SetTextureScaleMode(vid.texture, SDL_ScaleModeLinear); // we always start at device size so use linear for better upscaling over hdmi
	
	// SDL_ScaleMode scale_mode;
	// SDL_GetTextureScaleMode(vid.texture, &scale_mode);
	// LOG_info("texture scale mode: %i\n", scale_mode);
	
	// int format;
	// int access_;
	// SDL_QueryTexture(vid.texture, &format, &access_, NULL,NULL);
	// LOG_info("texture format: %s (streaming: %i)\n", SDL_GetPixelFormatName(format), access_==SDL_TEXTUREACCESS_STREAMING);
	
	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);
	vid.screen	= SDL_CreateRGBSurface(SDL_SWSURFACE, w,h, FIXED_DEPTH, RGBA_MASK_565);
	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
	
	device_width	= w;
	device_height	= h;
	device_pitch	= p;
	
	vid.sharpness = SHARPNESS_SOFT;
	
	return vid.screen;
}

static void clearVideo(void) {
	SDL_FillRect(vid.screen, NULL, 0);
	for (int i=0; i<3; i++) {
		SDL_RenderClear(vid.renderer);
		SDL_RenderPresent(vid.renderer);
	}
}

void PLAT_quitVideo(void) {
	// clearVideo();

	SDL_FreeSurface(vid.screen);
	SDL_FreeSurface(vid.buffer);
	if (vid.target) SDL_DestroyTexture(vid.target);
	SDL_DestroyTexture(vid.texture);
	SDL_DestroyRenderer(vid.renderer);
	SDL_DestroyWindow(vid.window);

	// system("cat /dev/zero > /dev/fb0");
	SDL_Quit();
}

void PLAT_clearVideo(SDL_Surface* screen) {
	SDL_FillRect(screen, NULL, 0); // TODO: revisit
}
void PLAT_clearAll(void) {
	PLAT_clearVideo(vid.screen); // TODO: revist
	SDL_RenderClear(vid.renderer);
}

void PLAT_setVsync(int vsync) {
	// buh
}

static int hard_scale = 4; // TODO: base src size, eg. 160x144 can be 4

static void resizeVideo(int w, int h, int p) {
	if (w==vid.width && h==vid.height && p==vid.pitch) return;
	
	// TODO: minarch disables crisp (and nn upscale before linear downscale) when native
	
	if (w>=device_width && h>=device_height) hard_scale = 1;
	else if (h>=160) hard_scale = 2; // limits gba and up to 2x (seems sufficient)
	else hard_scale = 4;

	LOG_info("resizeVideo(%i,%i,%i) hard_scale: %i crisp: %i\n",w,h,p, hard_scale,vid.sharpness==SHARPNESS_CRISP);

	SDL_FreeSurface(vid.buffer);
	SDL_DestroyTexture(vid.texture);
	if (vid.target) SDL_DestroyTexture(vid.target);
	
	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, vid.sharpness==SHARPNESS_SOFT?"1":"0", SDL_HINT_OVERRIDE);
	vid.texture = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w,h);
	
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "1", SDL_HINT_OVERRIDE);
		vid.target = SDL_CreateTexture(vid.renderer,SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, w * hard_scale,h * hard_scale);
	}
	else {
		vid.target = NULL;
	}

	vid.buffer	= SDL_CreateRGBSurfaceFrom(NULL, w,h, FIXED_DEPTH, p, RGBA_MASK_565);

	vid.width	= w;
	vid.height	= h;
	vid.pitch	= p;
}

SDL_Surface* PLAT_resizeVideo(int w, int h, int p) {
	resizeVideo(w,h,p);
	return vid.screen;
}

void PLAT_setVideoScaleClip(int x, int y, int width, int height) {
	// buh
}
void PLAT_setNearestNeighbor(int enabled) {
	// buh
}
void PLAT_setSharpness(int sharpness) {
	if (vid.sharpness==sharpness) return;
	int p = vid.pitch;
	vid.pitch = 0;
	vid.sharpness = sharpness;
	resizeVideo(vid.width,vid.height,p);
}
void PLAT_vsync(int remaining) {
	if (remaining>0) SDL_Delay(remaining);
}

scaler_t PLAT_getScaler(GFX_Renderer* renderer) {
	return scale1x1_c16;
}

void PLAT_blitRenderer(GFX_Renderer* renderer) {
	vid.blit = renderer;
	SDL_RenderClear(vid.renderer);
	resizeVideo(vid.blit->true_w,vid.blit->true_h,vid.blit->src_p);
}

// TODO: should we be using true_* instead of src_* below?
void PLAT_flip(SDL_Surface* IGNORED, int ignored) {
	if (!vid.blit) {
		resizeVideo(device_width,device_height,FIXED_PITCH); // !!!???
		SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
		SDL_BlitSurface(vid.screen, NULL, vid.buffer, NULL);
		SDL_UnlockTexture(vid.texture);
		if (rotate) SDL_RenderCopyEx(vid.renderer,vid.texture,NULL,&(SDL_Rect){0,device_width,device_width,device_height},rotate*90,&(SDL_Point){0,0},SDL_FLIP_NONE);
		else SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_RenderPresent(vid.renderer);
		return;
	}
	
	SDL_LockTexture(vid.texture,NULL,&vid.buffer->pixels,&vid.buffer->pitch);
	((scaler_t)vid.blit->blit)(
		vid.blit->src,vid.buffer->pixels,
		// vid.blit->src_w,vid.blit->src_h,vid.blit->src_p,
		vid.blit->true_w,vid.blit->true_h,vid.blit->src_p, // TODO: fix to be confirmed, issue may not present on this platform
		vid.buffer->w,vid.buffer->h,vid.buffer->pitch
	);
	SDL_UnlockTexture(vid.texture);
	
	SDL_Texture* target = vid.texture;
	int w = vid.blit->src_w;
	int h = vid.blit->src_h;
	if (vid.sharpness==SHARPNESS_CRISP) {
		SDL_SetRenderTarget(vid.renderer,vid.target);
		SDL_RenderCopy(vid.renderer, vid.texture, NULL,NULL);
		SDL_SetRenderTarget(vid.renderer,NULL);
		w *= hard_scale;
		h *= hard_scale;
		target = vid.target;
	}
	
	SDL_Rect* src_rect = &(SDL_Rect){0,0,w,h};
	SDL_Rect* dst_rect = &(SDL_Rect){0,0,device_width,device_height};
	if (vid.blit->aspect==0) { // native or cropped
		int w = vid.blit->src_w * vid.blit->scale;
		int h = vid.blit->src_h * vid.blit->scale;
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	else if (vid.blit->aspect>0) { // aspect
		int h = device_height;
		int w = h * vid.blit->aspect;
		if (w>device_width) {
			double ratio = 1 / vid.blit->aspect;
			w = device_width;
			h = w * ratio;
		}
		int x = (device_width - w) / 2;
		int y = (device_height - h) / 2;
		// dst_rect = &(SDL_Rect){x,y,w,h};
		dst_rect->x = x;
		dst_rect->y = y;
		dst_rect->w = w;
		dst_rect->h = h;
	}
	
	int ox,oy;
	oy = (device_width-device_height)/2;
	ox = -oy;
	if (rotate) SDL_RenderCopyEx(vid.renderer,target,src_rect,&(SDL_Rect){ox+dst_rect->x,oy+dst_rect->y,dst_rect->w,dst_rect->h},rotate*90,NULL,SDL_FLIP_NONE);
	else SDL_RenderCopy(vid.renderer, target, src_rect, dst_rect);
	// uint32_t then = SDL_GetTicks();
	SDL_RenderPresent(vid.renderer);
	// LOG_info("SDL_RenderPresent blocked for %ims\n", SDL_GetTicks()-then);
	vid.blit = NULL;
}

///////////////////////////////

// TODO: 
#define OVERLAY_WIDTH PILL_SIZE // unscaled
#define OVERLAY_HEIGHT PILL_SIZE // unscaled
#define OVERLAY_BPP 4
#define OVERLAY_DEPTH 16
#define OVERLAY_PITCH (OVERLAY_WIDTH * OVERLAY_BPP) // unscaled
#define OVERLAY_RGBA_MASK 0x00ff0000,0x0000ff00,0x000000ff,0xff000000 // ARGB
static struct OVL_Context {
	SDL_Surface* overlay;
} ovl;

SDL_Surface* PLAT_initOverlay(void) {
	ovl.overlay = SDL_CreateRGBSurface(SDL_SWSURFACE, SCALE2(OVERLAY_WIDTH,OVERLAY_HEIGHT),OVERLAY_DEPTH,OVERLAY_RGBA_MASK);
	return ovl.overlay;
}
void PLAT_quitOverlay(void) {
	if (ovl.overlay) SDL_FreeSurface(ovl.overlay);
}
void PLAT_enableOverlay(int enable) {

}

///////////////////////////////

static int online = 0;
void PLAT_getBatteryStatus(int* is_charging, int* charge) {
	// *is_charging = 0;
	// *charge = PWR_LOW_CHARGE;
	// return;
	
	*is_charging = getInt("/sys/class/power_supply/usb/online");

	int i = getInt("/sys/class/power_supply/battery/capacity");
	// worry less about battery and more about the game you're playing
	     if (i>80) *charge = 100;
	else if (i>60) *charge =  80;
	else if (i>40) *charge =  60;
	else if (i>20) *charge =  40;
	else if (i>10) *charge =  20;
	else           *charge =  10;

	// wifi status, just hooking into the regular PWR polling
	// char status[16];
	// getFile("/sys/class/net/wlan0/operstate", status,16);
	// online = prefixMatch("up", status);
}

#define LED_PATH "/sys/class/leds/led1/brightness"
void PLAT_enableBacklight(int enable) {
	if (enable) {
		SetBrightness(GetBrightness());
		system("echo 0 > " LED_PATH);
	}
	else {
		SetRawBrightness(0);
		system("echo 255 > " LED_PATH);
	}
}

void PLAT_powerOff(void) {
	system("rm -f /tmp/minui_exec && sync");
	sleep(2);

	SetRawVolume(MUTE_VOLUME_RAW);
	PLAT_enableBacklight(0);
	system("echo 255 > " LED_PATH);
	SND_quit();
	VIB_quit();
	PWR_quit();
	GFX_quit();
	exit(0);
}

///////////////////////////////

void PLAT_setCPUSpeed(int speed) {
	int freq = 0;
	switch (speed) {
		case CPU_SPEED_MENU: 		freq =  576; break;
		case CPU_SPEED_POWERSAVE:	freq = 1152; break;
		case CPU_SPEED_NORMAL: 		freq = 1344; break;
		case CPU_SPEED_PERFORMANCE: freq = 1512; break;
	}

	char cmd[128];
	sprintf(cmd,"overclock.elf userspace 1 %d 384 1080 0", freq);
	system(cmd);
}

#define RUMBLE_PATH "/sys/devices/virtual/timed_output/vibrator/enable"

void PLAT_setRumble(int strength) {
	// TODO: this is a bad comparison
	static int last = 0;
	strength = strength?10000:0;
	if (strength!=last) {
		char cmd[256];
		sprintf(cmd,"echo %i > %s", strength, RUMBLE_PATH); // ms
		system(cmd);
		last = strength;
	}
}

int PLAT_pickSampleRate(int requested, int max) {
	return MIN(requested, max);
}

char* PLAT_getModel(void) {
	return "Miyoo A30";
}

int PLAT_isOnline(void) {
	return online;
}