# NextUI

# NOTE: this runs on the host system (eg. macOS) not in a docker image
# it has to, otherwise we'd be running a docker in a docker and oof

# prevent accidentally triggering a full build with invalid calls
ifneq (,$(PLATFORM))
ifeq (,$(MAKECMDGOALS))
$(error found PLATFORM arg but no target, did you mean "make PLATFORM=$(PLATFORM) shell"?)
endif
endif

ifeq (,$(PLATFORMS))
#PLATFORMS = miyoomini trimuismart rg35xx rg35xxplus my355 tg5040 zero28 rgb30 m17 gkdpixel my282 magicmini
PLATFORMS = tg5040
endif

###########################################################

BUILD_HASH:=$(shell git rev-parse --short HEAD)
BUILD_BRANCH:=$(shell git rev-parse --abbrev-ref HEAD)
RELEASE_TIME:=$(shell TZ=GMT date +%Y%m%d)
ifeq ($(BUILD_BRANCH),main)
  RELEASE_BETA :=
else
  RELEASE_BETA := -$(BUILD_BRANCH)
endif
RELEASE_BASE=NextUI-$(RELEASE_TIME)$(RELEASE_BETA)
RELEASE_DOT:=$(shell find ./releases/. -regex ".*/${RELEASE_BASE}-[0-9]+-base\.zip" | wc -l | sed 's/ //g')
RELEASE_NAME ?= $(RELEASE_BASE)-$(RELEASE_DOT)

###########################################################

.PHONY: build

export MAKEFLAGS=--no-print-directory

all: setup $(PLATFORMS) special package done
	
shell:
	make -f makefile.toolchain PLATFORM=$(PLATFORM)

name: 
	@echo $(RELEASE_NAME)

build:
	# ----------------------------------------------------
	make build -f makefile.toolchain PLATFORM=$(PLATFORM) COMPILE_CORES=$(COMPILE_CORES)
	# ----------------------------------------------------

build-cores:
	make build-cores -f makefile.toolchain PLATFORM=$(PLATFORM) COMPILE_CORES=true
	# ----------------------------------------------------

cores-json:
	@cat workspace/tg5040/cores/makefile | grep ^CORES | cut -d' ' -f2 | jq  --raw-input .  | jq --slurp -cM .

build-core:
ifndef CORE
	$(error CORE is not set)
endif
	make build-core -f makefile.toolchain PLATFORM=$(PLATFORM) COMPILE_CORES=true CORE=$(CORE)

system:
	make -f ./workspace/$(PLATFORM)/platform/makefile.copy PLATFORM=$(PLATFORM)
	
	# populate system
	cp ./workspace/$(PLATFORM)/keymon/keymon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/$(PLATFORM)/libmsettings/libmsettings.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/nextui/build/$(PLATFORM)/nextui.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/minarch/build/$(PLATFORM)/minarch.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/syncsettings/build/$(PLATFORM)/syncsettings.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/nextval/build/$(PLATFORM)/nextval.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/clock/build/$(PLATFORM)/clock.elf ./build/EXTRAS/Tools/$(PLATFORM)/Clock.pak/
	cp ./workspace/all/minput/build/$(PLATFORM)/minput.elf ./build/EXTRAS/Tools/$(PLATFORM)/Input.pak/

	# battery tracking
	cp ./workspace/all/libbatmondb/build/$(PLATFORM)/libbatmondb.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/batmon/build/$(PLATFORM)/batmon.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/battery/build/$(PLATFORM)/battery.elf ./build/EXTRAS/Tools/$(PLATFORM)/Battery.pak/

	# game time tracking
	cp ./workspace/all/libgametimedb/build/$(PLATFORM)/libgametimedb.so ./build/SYSTEM/$(PLATFORM)/lib
	cp ./workspace/all/gametimectl/build/$(PLATFORM)/gametimectl.elf ./build/SYSTEM/$(PLATFORM)/bin/
	cp ./workspace/all/gametime/build/$(PLATFORM)/gametime.elf ./build/EXTRAS/Tools/$(PLATFORM)/Game\ Tracker.pak/
  
	cp ./workspace/all/settings/build/$(PLATFORM)/settings.elf ./build/EXTRAS/Tools/$(PLATFORM)/Settings.pak/
ifeq ($(PLATFORM), tg5040)
	cp ./workspace/all/ledcontrol/build/$(PLATFORM)/ledcontrol.elf ./build/EXTRAS/Tools/$(PLATFORM)/LedControl.pak/
	cp ./workspace/all/bootlogo/build/$(PLATFORM)/bootlogo.elf ./build/EXTRAS/Tools/$(PLATFORM)/Bootlogo.pak/
	
	# lib dependencies
	cp ./workspace/all/minarch/build/$(PLATFORM)/libsamplerate.* ./build/SYSTEM/$(PLATFORM)/lib/
	# This is a bandaid fix, needs to be cleaned up if/when we expand to other platforms.
	cp ./workspace/all/minarch/build/$(PLATFORM)/libzip.* ./build/SYSTEM/$(PLATFORM)/lib/
	cp ./workspace/all/minarch/build/$(PLATFORM)/libbz2.* ./build/SYSTEM/$(PLATFORM)/lib/
	cp ./workspace/all/minarch/build/$(PLATFORM)/liblzma.* ./build/SYSTEM/$(PLATFORM)/lib/
	cp ./workspace/all/minarch/build/$(PLATFORM)/libzstd.* ./build/SYSTEM/$(PLATFORM)/lib/
endif


cores: # TODO: can't assume every platform will have the same stock cores (platform should be responsible for copy too)
	# stock cores
	cp ./workspace/$(PLATFORM)/cores/output/fceumm_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gambatte_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/gpsp_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/picodrive_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/snes9x_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	cp ./workspace/$(PLATFORM)/cores/output/pcsx_rearmed_libretro.so ./build/SYSTEM/$(PLATFORM)/cores
	
	# extras
ifeq ($(PLATFORM), trimuismart)
	cp ./workspace/miyoomini/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
else ifeq ($(PLATFORM), m17)
	cp ./workspace/miyoomini/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
else ifneq ($(PLATFORM),gkdpixel)
	cp ./workspace/$(PLATFORM)/cores/output/fake08_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/P8.pak
endif
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/MGBA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mgba_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SGB.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_pce_fast_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PCE.pak
	cp ./workspace/$(PLATFORM)/cores/output/pokemini_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PKM.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGP.pak
	cp ./workspace/$(PLATFORM)/cores/output/race_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/NGPC.pak
	cp ./workspace/$(PLATFORM)/cores/output/fbneo_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/FBN.pak
ifneq ($(PLATFORM),gkdpixel)
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_supafaust_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/SUPA.pak
	cp ./workspace/$(PLATFORM)/cores/output/mednafen_vb_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/VB.pak
endif
	cp ./workspace/$(PLATFORM)/cores/output/cap32_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/CPC.pak
	cp ./workspace/$(PLATFORM)/cores/output/puae2021_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PUAE.pak
	cp ./workspace/$(PLATFORM)/cores/output/prboom_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PRBOOM.pak
	cp ./workspace/$(PLATFORM)/cores/output/vice_x64_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/C64.pak
	cp ./workspace/$(PLATFORM)/cores/output/vice_x128_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/C128.pak
	cp ./workspace/$(PLATFORM)/cores/output/vice_xplus4_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PLUS4.pak
	cp ./workspace/$(PLATFORM)/cores/output/vice_xpet_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/PET.pak
	cp ./workspace/$(PLATFORM)/cores/output/vice_xvic_libretro.so ./build/EXTRAS/Emus/$(PLATFORM)/VIC.pak

common: build system cores
	
clean:
	rm -rf ./build

setup: name
	# ----------------------------------------------------
	# make sure we're running in an input device
	tty -s || echo "No tty detected"
	
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
	say "done" 2>/dev/null || true

special:
	# setup miyoomini/trimui/magicx family .tmp_update in BOOT
	mv ./build/BOOT/common ./build/BOOT/.tmp_update
#	mv ./build/BOOT/miyoo ./build/BASE/
	mv ./build/BOOT/trimui ./build/BASE/
#	mv ./build/BOOT/magicx ./build/BASE/
#	cp -R ./build/BOOT/.tmp_update ./build/BASE/miyoo/app/
	cp -R ./build/BOOT/.tmp_update ./build/BASE/trimui/app/
#	cp -R ./build/BOOT/.tmp_update ./build/BASE/magicx/
#	cp -R ./build/BASE/miyoo ./build/BASE/miyoo354
#	cp -R ./build/BASE/miyoo ./build/BASE/miyoo355
#ifneq (,$(findstring my355, $(PLATFORMS)))
#	cp -R ./workspace/my355/init ./build/BASE/miyoo355/app/my355
#	cp -r ./workspace/my355/other/squashfs/output/* ./build/BASE/miyoo355/app/my355/payload/
#endif

tidy:
	rm -f releases/$(RELEASE_NAME)-base.zip 
	rm -f releases/$(RELEASE_NAME)-extras.zip
	rm -f releases/$(RELEASE_NAME)-all.zip
	# ----------------------------------------------------
	# copy update from merged platform to old pre-merge platform bin so old cards update properly
#ifneq (,$(findstring rg35xxplus, $(PLATFORMS)))
#	mkdir -p ./build/SYSTEM/rg40xxcube/bin/
#	cp ./build/SYSTEM/rg35xxplus/bin/install.sh ./build/SYSTEM/rg40xxcube/bin/
#endif

package: tidy
	# ----------------------------------------------------
	# zip up build
		
	# move formatted readmes from workspace to build
	cp ./workspace/readmes/BASE-out.txt ./build/BASE/README.txt
	cp ./workspace/readmes/EXTRAS-out.txt ./build/EXTRAS/README.txt
	rm -rf ./workspace/readmes
	
	cd ./build/SYSTEM && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	# ./commits.sh > ./build/SYSTEM/commits.txt
	cd ./build && find . -type f -name '.DS_Store' -delete
	mkdir -p ./build/PAYLOAD
	mv ./build/SYSTEM ./build/PAYLOAD/.system
	cp -R ./build/BOOT/.tmp_update ./build/PAYLOAD/
	cp -R ./build/EXTRAS/Tools ./build/PAYLOAD/
	
	cd ./build/PAYLOAD && zip -r MinUI.zip .system .tmp_update Tools
	mv ./build/PAYLOAD/MinUI.zip ./build/BASE
	
	# TODO: can I just add everything in BASE to zip?
	# cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves miyoo miyoo354 trimui rg35xx rg35xxplus gkdpixel miyoo355 magicx em_ui.sh MinUI.zip README.txt
	cd ./build/BASE && zip -r ../../releases/$(RELEASE_NAME)-base.zip Bios Roms Saves Shaders trimui em_ui.sh MinUI.zip README.txt
	cd ./build/EXTRAS && zip -r ../../releases/$(RELEASE_NAME)-extras.zip Bios Emus Roms Saves Shaders Tools README.txt
	echo "$(RELEASE_VERSION)" > ./build/latest.txt

	# compound zip (brew install libzip needed) 
	cd ./releases && zipmerge $(RELEASE_NAME)-all.zip $(RELEASE_NAME)-base.zip  && zipmerge $(RELEASE_NAME)-all.zip $(RELEASE_NAME)-extras.zip
	
###########################################################

.DEFAULT:
	# ----------------------------------------------------
	# $@
	@echo "$(PLATFORMS)" | grep -q "\b$@\b" && (make common PLATFORM=$@) || (exit 1)
	
