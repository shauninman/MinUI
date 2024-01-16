#keymon

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = input

CC = $(CROSS_COMPILE)gcc
CFLAGS	= -Os -std=gnu99 -lSDL -lrt -ldl -Wl,--gc-sections -s
# -lmsettings -lpthread
CFLAGS  += -I. -DPLATFORM=\"$(UNION_PLATFORM)\"
# -I../../all/common -I../platform/

all:
	$(CC) $(TARGET).c -o $(TARGET).elf $(CFLAGS)
clean:
	rm -rf $(TARGET).elf