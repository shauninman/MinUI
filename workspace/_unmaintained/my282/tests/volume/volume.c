#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
   
int main(int argc, char* argv[])
{
    int fd = -1;
    uint8_t *mem = NULL;
    volatile uint32_t *p = NULL;
      
    fd = open("/dev/mem", O_RDWR);
    mem = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x1c22000);
 	
	int val = argc>1?atoi(argv[1]):-1;
    uint32_t l = val;
    uint32_t r = val;
 
    p = (uint32_t *)(&mem[0xc00 + 0x258]);
    printf("%d\n", (*p & 0xff)); fflush(stdout);
	if (val>-1) *p = (l << 8) | r;
  
    munmap(mem, 4096);
    close(fd);
    return 0;
}