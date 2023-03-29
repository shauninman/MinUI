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
	
clean:
	cd ./overclock && make clean
	cd ./boot && rm -rf ./output
	
	