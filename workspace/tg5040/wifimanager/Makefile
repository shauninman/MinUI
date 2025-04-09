ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

TARGET_ARCH = "aarch64"
TARGET_CC = $(CROSS_COMPILE)gcc
TARGET_CXX = $(CROSS_COMPILE)g++
TARGET_AR = $(CROSS_COMPILE)ar
TARGET_CFLAGS = -pthread
TARGET_LDFLAGS = 
PKG_INSTALL_DIR = 
PKG_BUILD_DIR = .

TARGET_CFLAGS = $(if $(CONFIG_USE_GLIBC),-pthread)

wifimanager:
	make -C $(PKG_BUILD_DIR)/src/core/ \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		CONFIG_IPV6="$(CONFIG_IPV6)" \
		all

all: wifimanager
