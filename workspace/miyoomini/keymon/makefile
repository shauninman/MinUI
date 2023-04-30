ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc
FLAGS	= -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s

all: $(PREFIX)/include/msettings.h
	$(CC) $(TARGET).c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make