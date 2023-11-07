# MinUI

# NOTE: this runs on the host system (eg. macOS) not in a docker image
# it has to, otherwise we'd be running a docker in a docker and oof

ifeq (,$(PLATFORMS))
PLATFORMS = tg5040 rgb30 trimuismart miyoomini rg35xx
endif

###########################################################

BUILD_HASH:=$(shell git rev-parse --short HEAD)
RELEASE_TIME:=$(shell date +%Y%m%d)
RELEASE_BETA=b
RELEASE_BASE=MinUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find -E ./releases/. -regex ".*/${RELEASE_BASE}-[0-9]+-base\.zip" | wc -l | sed 's/ //g')
RELEASE_NAME=$(RELEASE_BASE)-$(RELEASE_DOT)

###########################################################
.PHONY: build

export MAKEFLAGS=--no-print-directory

all: setup $(PLATFORMS) special package done
	
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

name: 
	echo $(RELEASE_NAME)

build:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM)
	# ----------------------------------------------------

system:
	make -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)
	
	# populate system
	cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/minui/build/$(PLATFORM)/minui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/clock/build/$(PLATFORM)/clock.elf ./build/EXTRAS/Tools/$(PLATFORM)/Clock.pak/

# TODO: this is a brittle mess...can't build rg353 as a result

cores: # TODO: can't assume every platform will have the same stock cores (platform should be responsible for copy too)
	# stock cores
	cp ./workspace/$(PLATFORM)/cores/output/fceumm_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gambatte_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gpsp_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/picodrive_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
ifneq ($(PLATFORM),trimui)
	cp ./workspace/$(PLATFORM)/cores/output/snes9x2005_plus_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/pcsx_rearmed_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	
	# extras
ifeq ($(PLATFORM), trimuismart) # TODO tmp?
	cp ./workspace/miyoomini/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/trimuismart/P8.pak
else
	cp ./workspace/$(PLATFORM)/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
endif
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/MGBA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SGB.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_pce_fast_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PCE.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_supafaust_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SUPA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_vb_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/VB.pak
	cp ./workspace/$(PLATFORM)/cores/output/pokemini_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PKM.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGP.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGPC.pak
endif

common: build system cores
	
clean:
	rm -rf ./build

setup:
	# ----------------------------------------------------
	# make sure we're running in an input device
	tty -s 
	
	# ready fresh build
	rm -rf ./build
	mkdir -p ./releases
	cp -R ./skeleton ./build
	
	# remove authoring detritus
	cd ./build && find . -type f -name '.keep' -delete
	cd ./build && find . -type f -name '*.meta' -delete
	echo $(BUILD_HASH) > ./workspace/hash.txt
	
	# copy readmes to workspace so we can use Linux fmt instead of host's
	mkdir -p ./workspace/readmes
	cp ./skeleton/BASE/README.txt ./workspace/readmes/BASE-in.txt
	cp ./skeleton/EXTRAS/README.txt ./workspace/readmes/EXTRAS-in.txt
	
done:
	say "done"

special:
	# ----------------------------------------------------
	# setup miyoomini/trimui family .tmp_update in BOOT
	mv ./build/BOOT/common ./build/BOOT/.tmp_update
	mv ./build/BOOT/miyoo ./build/BASE/
	mv ./build/BOOT/trimui ./build/BASE/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/miyoo/app/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/trimui/app/
	cp -R ./build/BASE/miyoo ./build/BASE/miyoo354

tidy:
	# ----------------------------------------------------
	# remove systems we're not ready to support yet
	
	# TODO: tmp, figure out a cleaner way to do this
	rm -rf ./build/SYSTEM/rg353
	rm -rf ./build/SYSTEM/trimui
	rm -rf ./build/EXTRAS/Tools/rg353
	rm -rf ./build/EXTRAS/Tools/trimui

package: tidy
	# ----------------------------------------------------
	# zip up build
		
	# move formatted readmes from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README.txt
	rm -rf ./workspace/readmes
	
	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	mkdir -p ./build/PAYLOAD
	mv ./build/SYSTEM ./build/PAYLOAD/.system
	cp -R ./build/BOOT/.tmp_update ./build/PAYLOAD/
	cd ./build/PAYLOAD && zip -r ../BASE/trimui.zip .tmp_update
	
	cd ./build/PAYLOAD && zip -r MinUI.zip .system .tmp_update
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE
	
	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui dmenu.bin MinUI.zip README.txt
	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Tools README.txt
	echo "$(RELEASE_NAME)" > ./build/latest.txt
	

###########################################################

# TODO: make this a template like the cores makefile?

rg35xx:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

miyoomini:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

trimuismart:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

rg353:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

trimui:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

rgb30:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

nano:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

tg5040:
	# ----------------------------------------------------
	make common PLATFORM=$@
	# ----------------------------------------------------

