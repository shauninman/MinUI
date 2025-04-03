#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <mtd/mtd-user.h>

#define	IMGSIZE	0x20000

int main()
{
	mtd_info_t	mtdinfo;
	uint32_t	i, a, size, imgcount, *read_ofs32;
	uint8_t		*read_buf, *read_ofs;
	const char	img_name[3][12] = { "image1.jpg", "image2.jpg", "image3.jpg" };
	int		mtdfd, imgfd;

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

	for (imgcount=0; imgcount<3; imgcount++) {
		printf("Writing %s ... ",img_name[imgcount]);

		if (read_ofs32[0] != 0x4F474F4C) { puts("missing LOGO"); return -1; }
		for (i=1,a=0; i<8; i++) a += read_ofs32[i];
		if ((a)||(read_ofs32[9] != 0x2c)||(read_ofs32[10] != 0x1)) { puts("invalid LOGO format"); return -1; }
		size = read_ofs32[8];
		read_ofs += 0x2c;

		imgfd = creat(img_name[imgcount], 777);
		if (imgfd < 0) { puts("failed to create"); return -1; }
		if (size) if (write(imgfd, read_ofs, size) != size) { puts("failed to read"); return -1; }
		close(imgfd);
		puts("OK");

		read_ofs += size;
		read_ofs32 = (uint32_t*)read_ofs;
	}
	free(read_buf);

	return 0;
}
