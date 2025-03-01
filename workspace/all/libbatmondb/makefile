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

ifeq (,$(PREFIX))
	$(error missing PREFIX for this toolchain)
endif

###########################################################

include ../../$(PLATFORM)/platform/makefile.env
SDL?=SDL

###########################################################

TARGET=batmondb

.PHONY: build
.PHONY: clean

CC = $(CROSS_COMPILE)gcc

SYSROOT     := $(shell $(CC) --print-sysroot)

INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = $(TARGET).c ../common/utils.c ../common/api.c ../../$(PLATFORM)/platform/platform.c
# INCLUDEDIR = $(SYSROOT)/usr/include

CFLAGS   = $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL) -Ofast 
LDFLAGS = -ldl -lrt $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lsqlite3 -s

PRODUCT= build/$(PLATFORM)/lib$(TARGET).so

build: 
	mkdir -p build/$(PLATFORM)
	$(CC) -c -Werror -fpic $(SOURCE) -Wl,--no-as-needed $(LDFLAGS) $(CFLAGS)
	$(CC) -shared -o $(PRODUCT) "$(TARGET).o" $(LDFLAGS)
	cp "$(TARGET).h" "$(PREFIX)/include"
	cp $(PRODUCT) "$(PREFIX)/lib"
clean:
	rm -f *.o
	rm -f $(PRODUCT)
	rm -f $(PREFIX)/include/$(TARGET).h
	rm -f $(PREFIX)/lib/lib$(TARGET).so