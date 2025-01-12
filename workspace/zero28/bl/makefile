#keymon

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
CFLAGS	= -Os -lrt -ldl -Wl,--gc-sections -s
CFLAGS  += -I.

all:
	$(CC) bl_enable.c -o bl_enable $(CFLAGS)
	$(CC) bl_disable.c -o bl_disable $(CFLAGS)
clean:
	rm -rf bl_enable bl_disable