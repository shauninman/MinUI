###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER=other/DinguxCommander
REQUIRES_UNZIP60=other/unzip60

all: readmes
	cd show && make

early: $(REQUIRES_COMMANDER) $(REQUIRES_UNZIP60)
	mkdir -p other
	cd other/DinguxCommander && make -j
	cd other/unzip60 && make -f unix/Makefile.trimuismart unzip

clean:
	cd show && make clean
	cd other/DinguxCommander && make clean
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean
	
$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch trimui-smart https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)
$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

include ../all/readmes/makefile