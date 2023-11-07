###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_UNZIP60=other/unzip60

all: readmes
	cd show && make

readmes:
	cd ../readmes && fmt -w 40 -s ./BASE-in.txt > ./BASE-out.txt
	cd ../readmes && fmt -w 40 -s ./EXTRAS-in.txt > ./EXTRAS-out.txt

early: $(REQUIRES_UNZIP60)
	mkdir -p other
	cd other/unzip60 && make -f unix/Makefile.trimuismart unzip

clean:
	cd show && make clean
	cd other/unzip60 && make -f unix/Makefile.trimuismart clean

###########################################################

$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)
