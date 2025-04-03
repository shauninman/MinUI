ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
CFLAGS   = -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve 
LDFLAGS	 = 

all:
	mkdir -p output
	$(CC) logoread.c -o output/logoread.elf $(CFLAGS) $(LDFLAGS)
	$(CC) logomake.c -o output/logomake.elf $(CFLAGS) $(LDFLAGS)
	$(CC) logowrite.c -o output/logowrite.elf $(CFLAGS) $(LDFLAGS)
	
clean:
	rm -rf output