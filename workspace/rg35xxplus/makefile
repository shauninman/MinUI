#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_UNZIP60=other/unzip60
REQUIRES_SDL2=other/sdl2

all: readmes
	cd boot && ./build.sh
	cd init && make
	cd show && make

early: $(REQUIRES_UNZIP60) $(REQUIRES_SDL2)
	mkdir -p other
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd $(REQUIRES_SDL2) && CFLAGS="-O2 -march=armv8-a+crc -mtune=cortex-a53 -mcpu=cortex-a53 -ffast-math -ftree-vectorize" \
		./configure CC=${CROSS_COMPILE}gcc --host=arm-buildroot-linux-gnueabihf \
		  --enable-arm-neon \
		  --disable-oss \
		  --disable-rpath \
		  --disable-arts \
		  --disable-esd \
		  --disable-dbus \
		  --disable-diskaudio \
		  --disable-dummyaudio \
		  --disable-pulseaudio \
		  --disable-video-kmsdrm \
		  --disable-video-vivante \
		  --disable-video-cocoa \
		  --disable-video-metal \
		  --disable-video-dummy \
		  --disable-video-offscreen \
		  --disable-video-x11 \
		  --disable-video-vulkan \
		  --disable-ime \
		  --disable-ibus \
		  --disable-fcitx \
		  --disable-joystick-mfi \
		  --disable-directx \
		  --disable-xinput \
		  --disable-wasapi \
		  --disable-hidapi-joystick \
		  --disable-hidapi-libusb \
		  --disable-joystick-virtual \
		  --enable-video-mali \
		  --disable-libsamplerate && make
	# copy libs from toolchain (later moved to build)
	cp $(PREFIX)/lib/libSDL2_image-2.0.so.0.0.1 ../libSDL2_image-2.0.so.0
	cp $(PREFIX)/lib/libSDL2_ttf-2.0.so.0.14.0 ../libSDL2_ttf-2.0.so.0
	
clean:
	cd boot && rm -rf output
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean
	
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

$(REQUIRES_SDL2):
	git clone https://github.com/JohnnyonFlame/SDL-malifbdev-rot $(REQUIRES_SDL2)

include ../all/readmes/makefile
