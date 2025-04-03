#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <png.h>

//
//	Rumble ON(1) / OFF(0)
//
void rumble(uint32_t val) {
	int fd;
	const char str_export[] = "48";
	const char str_direction[] = "out";
	char value[1];
	value[0] = ((val&1)^1) + 0x30;

	fd = open("/sys/class/gpio/export", O_WRONLY);
		if (fd > 0) {
			write(fd, str_export, 2);
			close(fd);
		}
	fd = open("/sys/class/gpio/gpio48/direction", O_WRONLY);
		if (fd > 0) {
			write(fd, str_direction, 3);
			close(fd);
		}
	fd = open("/sys/class/gpio/gpio48/value", O_WRONLY);
		if (fd > 0) {
			write(fd, value, 1);
			close(fd);
		}
}

//
//	Search pid of running executable
//
pid_t searchpid(const char *commname) {
	DIR *procdp;
	struct dirent *dir;
	char fname[32];
	char comm[128];
	pid_t pid;
	pid_t ret = 0;

	procdp = opendir("/proc");
	while ((dir = readdir(procdp))) {
		if (dir->d_type == DT_DIR) {
			pid = atoi(dir->d_name);
			if ( pid > 2 ) {
				sprintf(fname, "/proc/%d/comm", pid);
				FILE *fp = fopen(fname, "r");
				if (fp) {
					fscanf(fp, "%127s", comm);
					fclose(fp);
					if (!strcmp(comm, commname)) { ret = pid; break; }
				}
			}
		}
	}
	closedir(procdp);
	return ret;
}

//
//	Get Most recent file name from Roms/recentlist.json
//
char* getrecent(char *filename) {
	FILE		*fp;
	char		*fnptr;
	uint32_t	i;

	strcpy(filename, "/mnt/SDCARD/Screenshots/");
	if (access(filename, F_OK)) mkdir(filename, 777);

	fnptr = filename + strlen(filename);

	if (!access("/tmp/cmd_to_run.sh", F_OK)) {
		// for stock
		if ((fp = fopen("/mnt/SDCARD/Roms/recentlist.json", "r"))) {
			fscanf(fp, "%*255[^:]%*[:]%*[\"]%255[^\"]", fnptr);
			fclose(fp);
		}
	} else if ((fp = fopen("/tmp/next", "r"))) {
		// for MiniUI
		char ename[256];
		char fname[256];
		char *strptr;
		ename[0] = 0; fname[0] = 0;
		fscanf(fp, "%*[\"]%255[^\"]%*[\" ]%255[^\"]", ename, fname);
		fclose(fp);
		if (fname[0]) {
			if ((strptr = strrchr(fname, '.'))) *strptr = 0;
			if ((strptr = strrchr(fname, '/'))) strptr++; else strptr = fname;
			strcpy(fnptr, strptr);
		} else if (ename[0]) {
			if ((strptr = strrchr(ename, '/'))) *strptr = 0;
			if ((strptr = strrchr(ename, '.'))) *strptr = 0;
			if ((strptr = strrchr(ename, '/'))) strptr++; else strptr = ename;
			strcpy(fnptr, strptr);
		}
	}

	if (!(*fnptr)) {
		if (searchpid("MiniUI")) strcat(filename, "MiniUI");
		else strcat(filename, "MainUI");
	}

	fnptr = filename + strlen(filename);
	for (i=0; i<1000; i++) {
		sprintf(fnptr, "_%03d.png", i);
		if ( access(filename, F_OK) != 0 ) break;
	} if (i > 999) return NULL;
	return filename;
}

//
//	Screenshot (640x480, rotate180, png)
//
void screenshot(void) {
	char		screenshotname[512];
	uint32_t	*buffer, *src;
	uint32_t	linebuffer[640], x, y, pix;
	FILE		*fp;
	int		fd_fb;
	struct		fb_var_screeninfo vinfo;
	png_structp	png_ptr;
	png_infop	info_ptr;

	if (getrecent(screenshotname) == NULL) return;
	if ((buffer = (uint32_t*)malloc(640*480*4)) != NULL) {
		fd_fb = open("/dev/fb0", O_RDWR);
		ioctl(fd_fb, FBIOGET_VSCREENINFO, &vinfo);
		lseek(fd_fb, 640*vinfo.yoffset*4, SEEK_SET);
		read(fd_fb, buffer, 640*480*4);
		close(fd_fb);

		fp = fopen(screenshotname, "wb");
		if (fp != NULL) {
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
			info_ptr = png_create_info_struct(png_ptr);
			png_init_io(png_ptr, fp);
			png_set_IHDR(png_ptr, info_ptr, 640, 480, 8, PNG_COLOR_TYPE_RGBA,
				PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
			png_write_info(png_ptr, info_ptr);
			src = buffer + 640*480;
			for (y=0; y<480; y++) {
				for (x=0; x<640; x++){
					pix = *--src;
					linebuffer[x] = 0xFF000000 | (pix & 0x0000FF00) | (pix & 0x00FF0000)>>16 | (pix & 0x000000FF)<<16;
				}
				png_write_row(png_ptr, (png_bytep)linebuffer);
			}
			png_write_end(png_ptr, info_ptr);
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			sync();
		}
		free(buffer);
	}
}

//
//	Screenshot Sample Main
//
#define	BUTTON_MENU	KEY_ESC
#define	BUTTON_POWER	KEY_POWER
#define	BUTTON_L2	KEY_TAB
#define	BUTTON_R2	KEY_BACKSPACE
int main() {
	int			input_fd;
	struct input_event	ev;
	uint32_t		val;
	uint32_t		l2_pressed = 0;
	uint32_t		r2_pressed = 0;

	// Prepare for Poll button input
	input_fd = open("/dev/input/event0", O_RDONLY);

	while( read(input_fd, &ev, sizeof(ev)) == sizeof(ev) ) {
		val = ev.value;
		if (( ev.type != EV_KEY ) || ( val > 1 )) continue;
		if (( ev.code == BUTTON_L2 )||( ev.code == BUTTON_R2 )) {
			if ( ev.code == BUTTON_L2 ) {
				l2_pressed = val;
			} else if ( ev.code == BUTTON_R2 ) {
				r2_pressed = val;
			}
			if (l2_pressed & r2_pressed) {
				rumble(1);
				screenshot();
				usleep(100000);	//0.1s
				rumble(0);
				l2_pressed = r2_pressed = 0;
			}
		}
	}

	// Quit
	close(input_fd);

	return 0;
}
