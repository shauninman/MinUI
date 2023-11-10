#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER=other/DinguxCommander

all: readmes
	cd ./overclock && make
	cd ./boot && ./build.sh

early: $(REQUIRES_COMMANDER)
	cd other/DinguxCommander && make -j

clean:
	cd ./overclock && make clean
	cd ./boot && rm -rf ./output
	cd other/DinguxCommander && make clean
	
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch minui-rg35xx https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)

include ../all/readmes/makefile
