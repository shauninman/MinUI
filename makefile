# MinUI

PLATFORMS=rg35xx miyoomini # trimuismart

###########################################################

all: setup $(PLATFORMS) package

common:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM)
	# ----------------------------------------------------
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarc/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	# cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

clean:
	rm -rf ./build

setup:
	# ----------------------------------------------------
	tty -s # make sure we're running in an input device
	rm -rf ./build
	mkdir -p ./releases
	cp -R ./skeleton ./build

package:
	# ----------------------------------------------------
	# zip up build

###########################################################

rg35xx:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------
	# cp ./workspace/$@/clock/clock.elf ./build/EXTRAS/Tools/$@/Clock.pak/

miyoomini:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------
	# cp ./workspace/$@/lumon/lumon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	# cp ./workspace/$@/batmon/batmon.elf ./build/SYSTEM/$(PLATFORM)/bin/
