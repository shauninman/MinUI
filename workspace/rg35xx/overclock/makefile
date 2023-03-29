#overclock

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = overclock

CC = $(CROSS_COMPILE)gcc
CFLAGS	= -Os -lrt -ldl -Wl,--gc-sections -s
CFLAGS  += -I. -DPLATFORM=\"$(UNION_PLATFORM)\"

all:
	$(CC) $(TARGET).c -o $(TARGET).elf $(CFLAGS)
clean:
	rm -rf $(TARGET).elf