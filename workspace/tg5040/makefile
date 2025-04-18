###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_EVTEST=other/evtest
REQUIRES_JSTEST=other/jstest
REQUIRES_UNZIP60=other/unzip60
REQUIRES_FILEMGR=other/NextCommander

all: readmes
	cd show && make

early: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST) $(REQUIRES_UNZIP60) $(REQUIRES_FILEMGR)
	mkdir -p other
	cd $(REQUIRES_FILEMGR) && make -j
	cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	cd $(REQUIRES_JSTEST) && $(CROSS_COMPILE)gcc -o jstest jstest.c
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip

clean:
	cd show && make clean
	cd $(REQUIRES_FILEMGR) && make clean
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean

###########################################################

$(REQUIRES_FILEMGR):
	git clone --depth 1 https://github.com/LoveRetro/NextCommander.git $(REQUIRES_FILEMGR)
	
$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

$(REQUIRES_JSTEST):
	git clone --depth 1 https://github.com/datrh/joyutils.git $(REQUIRES_JSTEST)

$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

include ../all/readmes/makefile
