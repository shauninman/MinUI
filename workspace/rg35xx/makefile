#rg35xx

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

all:
	cd ./overclock && make
	cd ./boot && ./build.sh

early:
	

clean:
	cd ./overclock && make clean
	cd ./boot && rm -rf ./output
	
	