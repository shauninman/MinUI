# MinUI/workspace

###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

.PHONY: all

all:
	echo "workspace all"
	# common libs
	cd ./$(PLATFORM) && make
	cd ./all/minui/ && make
	# cd ./all/minarch/ && make

clean:
	echo "workspace clean"