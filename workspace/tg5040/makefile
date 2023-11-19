###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_EVTEST=other/evtest
REQUIRES_UNZIP60=other/unzip60

all: readmes
	cd show && make

early: $(REQUIRES_UNZIP60) $(REQUIRES_EVTEST)
	mkdir -p other
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c

clean:
	cd show && make clean
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean

###########################################################

$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

include ../all/readmes/makefile
