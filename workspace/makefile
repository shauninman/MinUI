# workspace (all)

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
	cd ./$(PLATFORM)/libmsettings && make
	cd ./$(PLATFORM) && make early # eg. other libs
	cd ./$(PLATFORM)/keymon && make
	cd ./all/minui/ && make
	cd ./all/minarch/ && make
	cd ./all/battery/ && make
	cd ./all/clock/ && make
	cd ./all/libbatmondb/ && make
	cd ./all/batmon/ && make
	cd ./all/syncsettings/ && make
	cd ./$(PLATFORM)/cores && make
	cd ./$(PLATFORM) && make

clean:
	cd ./$(PLATFORM)/libmsettings && make clean
	cd ./$(PLATFORM)/keymon && make clean
	cd ./all/minui/ && make clean
	cd ./all/minarch/ && make clean
	cd ./all/battery/ && make clean
	cd ./all/clock/ && make clean
	cd ./all/libbatmondb/ && make clean
	cd ./all/batmon/ && make clean
	cd ./all/minput/ && make clean
	cd ./all/syncsettings/ && make clean
	cd ./$(PLATFORM) && make clean