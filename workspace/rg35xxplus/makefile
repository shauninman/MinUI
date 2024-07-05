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

all: readmes
	cd boot && ./build.sh
	cd init && make
	cd show && make

early: $(REQUIRES_UNZIP60)
	mkdir -p other
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	# copy libs from toolchain (later moved to build)
	cp $(PREFIX)/lib/libSDL2_image-2.0.so.0.0.1 ../libSDL2_image-2.0.so.0
	cp $(PREFIX)/lib/libSDL2_ttf-2.0.so.0.14.0 ../libSDL2_ttf-2.0.so.0
	
clean:
	cd boot && rm -rf output
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean
	
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)
	
include ../all/readmes/makefile
