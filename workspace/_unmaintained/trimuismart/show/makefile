ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = show
PRODUCT= $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
CFLAGS   = -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -mcpu=cortex-a7
LDFLAGS	 = -lSDL -lSDL_image

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)