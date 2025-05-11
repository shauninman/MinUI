CORES= c128
CORES+= c64
CORES+= fake-08
CORES+= fbneo
CORES+= fceumm
CORES+= gambatte
CORES+= gpsp
CORES+= libretro-cap32
CORES+= libretro-uae
CORES+= mednafen_pce_fast
CORES+= mednafen_supafaust
CORES+= mednafen_vb
CORES+= mgba
CORES+= pcsx_rearmed
CORES+= pet
CORES+= picodrive
CORES+= plus4
CORES+= pokemini
CORES+= race
CORES+= snes9x
CORES+= vic
CORES+= prboom

###############################

# optional core vars
# *_REPO=
# *_HASH=
# *_CORE=
# *_FLAGS=
# *_MAKEFILE=
# *_BUILD_PATH=

fbneo_REPO = https://github.com/libretro/FBNeo
fbneo_CORE = fbneo_libretro.so
fbneo_BUILD_PATH = fbneo/src/burner/libretro
fbneo_MAKE = make

mednafen_pce_fast_REPO = https://github.com/libretro/beetle-pce-fast-libretro
mednafen_vb_REPO = https://github.com/libretro/beetle-vb-libretro

fake-08_REPO = https://github.com/jtothebell/fake-08
fake-08_CORE = fake08_libretro.so
fake-08_BUILD_PATH = fake-08/platform/libretro

fceumm_REPO = https://github.com/libretro/libretro-fceumm

gambatte_REPO = https://github.com/libretro/gambatte-libretro

mednafen_supafaust_REPO = https://github.com/libretro/supafaust

pcsx_rearmed_MAKEFILE = Makefile.libretro

picodrive_REPO = https://github.com/irixxxx/picodrive
picodrive_MAKEFILE = Makefile.libretro
# No background Audio on Sega CD
# https://github.com/LoveRetro/NextUI/issues/68
picodrive_HASH = b0be121

pokemini_REPO = https://github.com/libretro/PokeMini
pokemini_MAKEFILE = Makefile.libretro
# Our patchfile doesn't work on latest Pokemon mini commits, pinning to last working hash for now
pokemini_HASH = 78656d4

prboom_REPO = https://github.com/libretro/libretro-prboom

snes9x2005_plus_REPO = https://github.com/libretro/snes9x2005
snes9x2005_plus_FLAGS = USE_BLARGG_APU=1

snes9x_REPO = https://github.com/libretro/snes9x
snes9x_CORE = snes9x_libretro.so
snes9x_BUILD_PATH = snes9x/libretro

libretro-uae_REPO = https://github.com/libretro/libretro-uae
libretro-uae_HASH = 2.6.1
libretro-uae_CORE = puae2021_libretro.so

libretro-cap32_REPO = https://github.com/libretro/libretro-cap32
libretro-cap32_CORE = cap32_libretro.so

c64_REPO  = https://github.com/libretro/vice-libretro
c64_CORE  = vice_x64_libretro.so
c64_FLAGS = EMUTYPE=x64

c128_REPO  = https://github.com/libretro/vice-libretro
c128_CORE  = vice_x128_libretro.so
c128_FLAGS = EMUTYPE=x128

vic_REPO  = https://github.com/libretro/vice-libretro
vic_CORE  = vice_xvic_libretro.so
vic_FLAGS = EMUTYPE=xvic

pet_REPO  = https://github.com/libretro/vice-libretro
pet_CORE  = vice_xpet_libretro.so
pet_FLAGS = EMUTYPE=xpet

plus4_REPO  = https://github.com/libretro/vice-libretro
plus4_CORE  = vice_xplus4_libretro.so
plus4_FLAGS = EMUTYPE=xplus4

###############################

include ../../all/cores/makefile
