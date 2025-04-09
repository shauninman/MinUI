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

TARGET = settings
INCDIR = -I. -I../common/ -I../../$(PLATFORM)/platform/
SOURCE = -c ../common/utils.c ../common/api.c ../common/config.c ../common/scaler.c ../../$(PLATFORM)/platform/platform.c
CXXSOURCE = $(TARGET).cpp menu.cpp wifimenu.cpp keyboardprompt.cpp build/$(PLATFORM)/utils.o build/$(PLATFORM)/api.o build/$(PLATFORM)/config.o build/$(PLATFORM)/scaler.o build/$(PLATFORM)/platform.o 

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CFLAGS += $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -DUSE_$(SDL)  -Ofast 
CXXFLAGS += $(CFLAGS) -std=c++17
CFLAGS += -fpermissive
LDFLAGS	+= -ldl $(LIBS) -l$(SDL) -l$(SDL)_image -l$(SDL)_ttf -lpthread -lm -lz
LDFLAGS +=  -lmsettings

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: $(PREFIX)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) $(CFLAGS) $(LDFLAGS)
	mv utils.o api.o config.o scaler.o platform.o build/$(PLATFORM)
	$(CXX) $(CXXSOURCE) -o $(PRODUCT) $(CXXFLAGS) $(LDFLAGS) -lstdc++
clean:
	rm -f $(PRODUCT)

$(PREFIX)/include/msettings.h:
	cd /root/workspace/$(PLATFORM)/libmsettings && make