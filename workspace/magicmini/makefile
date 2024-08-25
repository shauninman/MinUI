#magicmini

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_351FILES=other/351files

all: readmes
	
early: $(REQUIRES_351FILES)
	mkdir -p other
	cd $(REQUIRES_351FILES) && START_PATH=/storage/TF2 CC=${CROSS_COMPILE}gcc make
	
clean:
	cd $(REQUIRES_351FILES) && make clean
	
$(REQUIRES_351FILES):
	git clone --depth 1 https://github.com/shauninman/351Files.git $(REQUIRES_351FILES)
	
include ../all/readmes/makefile
