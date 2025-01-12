# zero28

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER=other/DinguxCommander-sdl2

all: readmes
	cd bl && make
	cd show && make

early: $(REQUIRES_COMMANDER)
	mkdir -p other
	cd $(REQUIRES_COMMANDER) && make -j
		
clean:
	cd bl && make clean
	cd show && make clean
	cd $(REQUIRES_COMMANDER) && make clean
	
$(REQUIRES_COMMANDER):
	git clone --depth 1 https://github.com/shauninman/DinguxCommander-sdl2.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile
