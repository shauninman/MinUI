###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

TARGET = minarch
INCDIR = -I. -I./libretro-common/include/ -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/scaler.c ../common/utils.c ../common/config.c ../common/api.c ../../$(PLATFORM)/platform/platform.c

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -O3 -Ofast -std=gnu99
LDFLAGS	 += -lmsettings -lzip -lsamplerate
# CFLAGS  += -Wall -Wno-unused-variable -Wno-unused-function -Wno-format-overflow
# CFLAGS  += -fsanitize=address -fno-common
# LDFLAGS += -lasan

BUILD_DATE!=date +%Y.%m.%d
BUILD_HASH!=cat ../../hash.txt
CFLAGS += -DBUILD_DATE=\"${BUILD_DATE}\" -DBUILD_HASH=\"${BUILD_HASH}\"

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: libretro-common $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	cp /usr/lib/aarch64-linux-gnu/libsamplerate.so.0 build/$(PLATFORM)
	# This is a bandaid fix, needs to be cleaned up if/when we expand to other platforms.
	cp /usr/local/lib/libzip.so.5 build/$(PLATFORM)
	cp /lib/aarch64-linux-gnu/libbz2.so.1.0 build/$(PLATFORM)
	cp /lib/aarch64-linux-gnu/liblzma.so.5 build/$(PLATFORM)
	cp /usr/lib/aarch64-linux-gnu/libzstd.so.1 build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)

libretro-common:
	git clone https://github.com/libretro/libretro-common
	
$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make