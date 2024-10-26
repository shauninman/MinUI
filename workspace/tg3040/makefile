###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

all: readmes
	cd show && make

early: 
	# nothing

clean:
	cd show && make clean

###########################################################

include ../all/readmes/makefile
