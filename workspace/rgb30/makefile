###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif

###########################################################

REQUIRES_COMMANDER=other/DinguxCommander

all: 
	# all
	
early: $(REQUIRES_COMMANDER)
	cd other/DinguxCommander && make -j

clean:
	cd other/DinguxCommander && make clean

###########################################################

$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch minui-rgb30 https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)
