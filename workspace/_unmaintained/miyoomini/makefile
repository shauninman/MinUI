###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL=other/sdl
REQUIRES_COMMANDER=other/DinguxCommander

all: readmes
	cd blank && make
	cd batmon && make
	cd lumon && make
	cd show && make
	cd overclock && make

early: $(REQUIRES_SDL) $(REQUIRES_COMMANDER)
	cd other/sdl && ./make.sh
	cd other/logotweak && make
	cd other/latency_reduction && make
	cd other/DinguxCommander && make -j

clean:
	cd blank && make clean
	cd batmon && make clean
	cd lumon && make clean
	cd show && make clean
	cd other/sdl && make distclean
	cd other/latency_reduction && make clean
	cd other/DinguxCommander && make clean

###########################################################

$(REQUIRES_SDL):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/SDL-1.2.git $(REQUIRES_SDL)
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile
