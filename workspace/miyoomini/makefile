###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_SDL=other/sdl

all: 
	cd blank && make
	cd batmon && make
	cd lumon && make

early: $(REQUIRES_SDL)
	cd other/sdl && ./make.sh
	cd other/latency_reduction && make

clean:
	cd blank && make clean
	cd batmon && make clean
	cd lumon && make clean
	cd other/sdl && make distclean
	cd other/latency_reduction && make clean

###########################################################

$(REQUIRES_SDL):
	git clone --depth 1 --branch miniui-miyoomini https://github.com/shauninman/SDL-1.2.git other/sdl

