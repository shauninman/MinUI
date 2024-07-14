#my282

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
	cd show && make
	cd overclock && make
	cd other/squashfs && make

early: $(REQUIRES_UNZIP60)
	mkdir -p other
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd libmstick && make
	
clean:
	cd show && make clean
	cd overclock && make clean
	cd other/squashfs && make clean
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean
	
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)
	
include ../all/readmes/makefile
