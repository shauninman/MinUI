ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = overclock
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS   = -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve 

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS)
clean:
	rm -f $(PRODUCT)