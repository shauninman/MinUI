ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

CC = $(CROSS_COMPILE)gcc
FLAGS	= -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s

all:
	$(CC) keymon.c -o keymon $(FLAGS)
clean:
	rm -rf keymon