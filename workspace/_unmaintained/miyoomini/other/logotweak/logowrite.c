#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#define	IMGSIZE	0x20000

int main()
{
	mtd_info_t	mtdinfo;
	erase_info_t	ei;
	uint32_t	i, a, *read_ofs32;
	uint8_t		r, *read_buf, *read_ofs, *verify_buf;
	int		imgfd, mtdfd;
	struct stat	fst;
	const char	img_name[] = "logo.img";

	read_buf = malloc(IMGSIZE);
	if (!read_buf) { puts("malloc failed"); return -1; }

	printf("Opening logo.img ... ");
	imgfd = open(img_name, O_RDONLY);
	if (imgfd < 0) { puts("failed to open"); return -1; }
	fstat(imgfd, &fst);
	if (fst.st_size != IMGSIZE) { puts("invalid size"); return -1; }
	puts("OK");

	printf("Checking logo.img ... ");
	lseek(imgfd, 0, SEEK_SET);
	if (read(imgfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to read"); return -1; }
	read_ofs = read_buf;		// "SSTAR" offset
	read_ofs32 = (uint32_t*)read_ofs;
	if ((read_ofs32[0] != 0x41545353)||(read_ofs32[1] != 0x52)||(read_ofs32[2] != 4)) { puts("missing SSTAR"); return -1; }

	read_ofs = read_ofs + 12;	// "DISP" offset
	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x50534944) { puts("missing DISP"); return -1; }
	for (i=1,a=0; i<8; i++) a += read_ofs32[i];
	read_ofs += read_ofs32[8] + read_ofs32[9];	// 1st "LOGO" offset
	if ((a)||(read_ofs - read_buf) > (IMGSIZE-0x2c)) { puts("invalid DISP format"); return -1; }

	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x4F474F4C) { puts("missing LOGO1"); return -1; }
	for (i=1,a=0; i<8; i++) a += read_ofs32[i];
	read_ofs += read_ofs32[8] + read_ofs32[9];
	if ((a)||(read_ofs - read_buf) > (IMGSIZE-0x2c)) { puts("invalid LOGO1 format"); return -1; }

	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x4F474F4C) { puts("missing LOGO2"); return -1; }
	for (i=1,a=0; i<8; i++) a += read_ofs32[i];
	read_ofs += read_ofs32[8] + read_ofs32[9];
	if ((a)||(read_ofs - read_buf) > (IMGSIZE-0x2c)) { puts("invalid LOGO2 format"); return -1; }

	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x4F474F4C) { puts("missing LOGO3"); return -1; }
	for (i=1,a=0; i<8; i++) a += read_ofs32[i];
	read_ofs += read_ofs32[8] + read_ofs32[9];
	if ((a)||(read_ofs - read_buf) > IMGSIZE) { puts("invalid LOGO3 format"); return -1; }
	puts("OK");

	printf("Opening /dev/mtd3 ... ");
	mtdfd = open("/dev/mtd3", O_RDWR);
	if (mtdfd < 0) { puts("failed to open"); return -1; }

	ioctl(mtdfd, MEMGETINFO, &mtdinfo);
	if (mtdinfo.type != 3) { puts("MTD type incorrect"); return -1; }
	if (mtdinfo.size != IMGSIZE) { puts("MTD size incorrect"); return -1; }
	if (mtdinfo.erasesize != 0x10000) { puts("MTD erasesize incorrect"); return -1; }
	puts("OK");

	printf("Erasing /dev/mtd3 ... ");
	ei.length = mtdinfo.erasesize;
	for (ei.start = 0; ei.start < mtdinfo.size; ei.start += ei.length) {
		ioctl(mtdfd, MEMUNLOCK, &ei);
		ioctl(mtdfd, MEMERASE, &ei);
	}
	puts("OK");

	printf("Checking erased /dev/mtd3 ... ");
	lseek(mtdfd, 0, SEEK_SET);
	if (read(mtdfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to read"); return -1; }
	for (i=0, r=0xff; i<IMGSIZE; i++) r &= read_buf[i];
	if (r != 0xff) { puts("erase failed"); return -1; }
	puts("OK");

	printf("Reading logo.img ... ");
	lseek(imgfd, 0, SEEK_SET);
	if (read(imgfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to read"); return -1; }
	puts("OK");

	printf("Flushing ... ");
	lseek(mtdfd, 0, SEEK_SET);
	if (write(mtdfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to write"); return -1; }
	puts("OK");

	verify_buf = malloc(IMGSIZE);
	if (verify_buf) {
		printf("Verifying ... ");
		lseek(mtdfd, 0, SEEK_SET);
		if (read(mtdfd, verify_buf, IMGSIZE) != IMGSIZE) { puts("failed to read"); return -1; }
		if (memcmp(read_buf, verify_buf, IMGSIZE)) { puts("NG"); return -1; }
		free(verify_buf);
		puts("OK");
	}

	close(imgfd);
	close(mtdfd);
	puts("Flush completed");
	free(read_buf);
	return 0;
}
