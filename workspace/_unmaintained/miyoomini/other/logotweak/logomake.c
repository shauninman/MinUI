#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#define	IMGSIZE	0x20000

int main()
{
	mtd_info_t	mtdinfo;
	uint32_t	i, a, size, size32, imgcount, *read_ofs32;
	uint8_t		*read_buf, *read_ofs;
	const char	img_name[4][12] = { "image1.jpg", "image2.jpg", "image3.jpg", "logo.img" };
	int		mtdfd, imgfd;
	struct stat	fst;

	read_buf = malloc(IMGSIZE);
	if (!read_buf) { puts("malloc failed"); return -1; }

	printf("Reading /dev/mtd3 ... ");
	mtdfd = open("/dev/mtd3", O_RDONLY);
	if (mtdfd < 0) { puts("failed to open"); return -1; }

	ioctl(mtdfd, MEMGETINFO, &mtdinfo);
	if (mtdinfo.type != 3) { puts("MTD type incorrect"); return -1; }
	if (mtdinfo.size != IMGSIZE) { puts("MTD size incorrect"); return -1; }
	if (mtdinfo.erasesize != 0x10000) { puts("MTD erasesize incorrect"); return -1; }

	lseek(mtdfd, 0, SEEK_SET);
	if (read(mtdfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to read"); return -1; }
	close(mtdfd);
	puts("OK");

	printf("Checking partition ... ");
	read_ofs = read_buf;		// "SSTAR" offset
	read_ofs32 = (uint32_t*)read_ofs;
	if ((read_ofs32[0] != 0x41545353)||(read_ofs32[1] != 0x52)||(read_ofs32[2] != 4)) { puts("missing SSTAR"); return -1; }

	read_ofs = read_ofs + 12;	// "DISP" offset
	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x50534944) { puts("missing DISP"); return -1; }
	for (i=1,a=0; i<8; i++) a += read_ofs32[i];
	read_ofs = read_ofs + read_ofs32[8] + read_ofs32[9];	// 1st "LOGO" offset
	if ((a)||(read_ofs - read_buf) > (IMGSIZE-0x2c)) { puts("invalid DISP format"); return -1; }

	read_ofs32 = (uint32_t*)read_ofs;
	if (read_ofs32[0] != 0x4F474F4C) { puts("missing LOGO"); return -1; }
	puts("OK");

	// read 3 jpg images
	for (imgcount=0; imgcount<3; imgcount++) {
		printf("Reading %s ... ",img_name[imgcount]);

		read_ofs32[0] = 0x4F474F4C;
		for (i=1; i<8; i++) read_ofs32[i] = 0;
		read_ofs32[9] = 0x2c; read_ofs32[10] = 1;

		imgfd = open(img_name[imgcount], O_RDONLY);
		if (imgfd < 0) { puts("failed to open"); return -1; }

		fstat(imgfd, &fst);
		size = fst.st_size;
		size32 = (size+3)&~3;
		read_ofs32[8] = size32;

		read_ofs += 0x2c;
		if (read_ofs + size32 > read_buf + IMGSIZE) { puts("img size overflow"); return -1; }
		if (size) {
			memset(read_ofs, 0, size32);
			if (read(imgfd, read_ofs, size) != size) { puts("failed to read"); return -1; }
		}
		close(imgfd);
		if (size != size32) read_ofs[size] = 0xff;
		puts("OK");

		read_ofs += size32;
		read_ofs32 = (uint32_t*)read_ofs;
	}

	// fill rest bytes to 0xFF
	if (IMGSIZE-(read_ofs-read_buf) > 0) memset(read_ofs, 0xFF, IMGSIZE-(read_ofs-read_buf));

	// write logo.img
	printf("Writing %s ... ",img_name[3]);
	imgfd = creat(img_name[3], 777);
	if (imgfd < 0) { puts("failed to create"); return -1; }
	if (write(imgfd, read_buf, IMGSIZE) != IMGSIZE) { puts("failed to write"); return -1; }
	close(imgfd);
	puts("OK");

	free(read_buf);

	return 0;
}
