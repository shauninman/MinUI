###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

###########################################################

REQUIRES_COMMANDER=other/DinguxCommander

all:
	# $(PLATFORM) all

early:
	cd other/DinguxCommander && make -j

clean:
	cd other/DinguxCommander && make clean
	

$(REQUIRES_COMMANDER):
	git clone --depth 1 --branch trimui-smart https://github.com/shauninman/DinguxCommander.git $(REQUIRES_COMMANDER)
