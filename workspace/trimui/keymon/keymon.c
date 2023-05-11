#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <linux/input.h>
#include <sys/mman.h>

#include <msettings.h>
#include <pthread.h>

//	Button Defines
#define	BUTTON_MENU	KEY_ESC
#define	BUTTON_SELECT	KEY_RIGHTCTRL
#define	BUTTON_START	KEY_ENTER
#define	BUTTON_L	KEY_TAB
#define	BUTTON_R	KEY_BACKSPACE

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

//	Global Variables
struct input_event	ev;
int	input_fd = 0;
int	memdev = 0;
uint32_t		*mem;
pthread_t		usb_pt;
//
//	Quit
//
void quit(int exitcode) {
	pthread_cancel(usb_pt);
	pthread_join(usb_pt, NULL);
	QuitSettings();
	
	if (input_fd > 0) close(input_fd);
	if (memdev > 0) close(memdev);
	exit(exitcode);
}

//
//	Init LCD
//
inline void initLCD(void) {
	memdev = open("/dev/mem", O_RDWR);
	if (memdev > 0) {
		mem = (uint32_t*)mmap(0, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED, memdev, 0x01c20000); // CCU/INTC/PIO/TIMER
		if (mem == MAP_FAILED) {
			close(memdev);
			ERROR("Failed to mmap hardware registers");
		} else {
			uint32_t pe_cfg0 = mem[0x0890>>2]; // PE_CFG0
			if (pe_cfg0 & 1) {
				mem[0x0890>>2] = pe_cfg0 & 0xF0FFFFFF | 0x03000000;
			}
		}
	} else {
		ERROR("Failed to open /dev/mem");
	}
}

//
//	Open Input Device
//
inline void openInputDevice(void) {
	FILE *fp;
	char strbuf[40];
	const char gpioname[] = "gpio_keys";
	for (uint32_t i=0; i<10; i++) {
		snprintf(strbuf,sizeof(strbuf),"/sys/class/input/event%d/device/name",i);
		fp = fopen(strbuf, "r");
		if ( fp != NULL ) {
			fgets(strbuf,sizeof(gpioname),fp);
			fclose(fp);
			if (!strcmp(strbuf,gpioname)) {
				snprintf(strbuf,sizeof(strbuf),"/dev/input/event%d",i);
				input_fd = open(strbuf, O_RDONLY);
				if (input_fd>0) return;
			}
		}
	}
	ERROR("Failed to open /dev/input/event");
}

#define HasUSBAudio() access("/dev/dsp1", F_OK)==0

void* checkUSB(void *arg) {
	uint32_t has_USB = HasUSBAudio();
	uint32_t had_USB = has_USB;
	while(1) {
		sleep(1);
		has_USB = HasUSBAudio();
		if (had_USB!=has_USB) {
			had_USB = has_USB;
			SetVolume(GetVolume());
		}
	}
	return 0;
}

//
//	Main
//
void main(void) {
	// Initialize
	signal(SIGTERM, &quit);
	signal(SIGSEGV, &quit);
	initLCD();
	openInputDevice();

	// Set Initial Volume / Brightness
	InitSettings();
	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
	
	pthread_create(&usb_pt, NULL, &checkUSB, NULL);

	// Main Loop
	register uint32_t val;
	register uint32_t pressedbuttons = 0;
	register uint32_t button_flag = 0;
	uint32_t repeat_START = 0; //	for suspend
	uint32_t repeat_LR = 0;
	while( read(input_fd, &ev, sizeof(ev)) == sizeof(ev) ) {
		val = ev.value;
		if (( ev.type != EV_KEY ) || ( val > REPEAT )) continue;
		if ( val < REPEAT ) {
			pressedbuttons += val;
			if (( val == RELEASED )&&( pressedbuttons > 0 )) pressedbuttons--;
		}
		switch (ev.code) {
		case BUTTON_SELECT:
			if ( val != REPEAT ) {
				button_flag = button_flag & (~SELECT) | (val<<SELECT_BIT);
			}
			break;
		case BUTTON_START:
			if ( val != REPEAT ) {
				button_flag = button_flag & (~START) | (val<<START_BIT);
				repeat_START = (pressedbuttons == 1 ? val : 0);
			} 
			break;
		case BUTTON_L:
			if ( val == REPEAT ) {
				// Adjust repeat speed to 1/2
				val = repeat_LR;
				repeat_LR ^= PRESSED;
			} else {
				repeat_LR = 0;
			}
			if ( val == PRESSED ) {
				switch (button_flag) {
				case SELECT:
					// SELECT + L : volume down
					val = GetVolume();
					if (val>0) SetVolume(--val);
					break;
				case START:
					// START + L : brightness down
					val = GetBrightness();
					if (val>0) SetBrightness(--val);
					break;
				default:
					break;
				}
			}
			break;
		case BUTTON_R:
			if ( val == REPEAT ) {
				// Adjust repeat speed to 1/2
				val = repeat_LR;
				repeat_LR ^= PRESSED;
			} else {
				repeat_LR = 0;
			}
			if ( val == PRESSED ) {
				switch (button_flag) {
				case SELECT:
					// SELECT + R : volume up
					val = GetVolume();
					if (val<VOLMAX) SetVolume(++val);
					break;
				case START:
					// START + R : brightness up
					val = GetBrightness();
					if (val<BRIMAX) SetBrightness(++val);
					break;
				default:
					break;
				}
			}
			break;
		default:
			break;
		}
	}
	ERROR("Failed to read input event");
}
