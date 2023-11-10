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
	# all
	
early: $(REQUIRES_SDL) $(REQUIRES_COMMANDER)
	cd other/sdl && ./make.sh
	cd other/DinguxCommander && make -j

clean:
	cd other/sdl && make distclean
	cd other/DinguxCommander && make clean

###########################################################

$(REQUIRES_SDL):
	git clone --depth 1 --branch union-trimui-model-s https://github.com/shauninman/SDL-1.2.git $(REQUIRES_SDL)
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch union-trimui-model-s https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile
