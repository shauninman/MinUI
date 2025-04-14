ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

# include $(TOPDIR)/rules.mk
# include $(BUILD_DIR)/package.mk

CC = $(CROSS_COMPILE)gcc

target = libwifimg.so

INCLUDES        += -I. -I./include
LIBS            += -lpthread -ldl -lrt

SRCS =wifimanager.c wifi_event.c scan.c \
      wifi.c wpa_supplicant_conf.c wifi_udhcpc.c tool.c wmg_debug.c status_info.c
install:
	-@mkdir -p $(PREFIX)/lib
	-@mkdir -p $(PREFIX)/include
	@cp ./include/*.h $(PREFIX)/include
	@cp $(target) $(PREFIX)/lib

#ifeq ($(CONFIG_IPV6), y)
#EXT_LDFLAGS=-DCONFIG_IPV6
#endif

#libwifimg.so
#ifeq ($(CONFIG_ARCH), "aarch64")
    SRCS += libwpa_client_64.a
#else
#    ifeq ($(CONFIG_SOFT_FLOAT), y)
#         SRCS += libwpa_client_softft.a
#    else
#         ifeq ($(TARGET_ARCH), riscv)
#              SRCS += libwpa_client_riscv.a
#         else
#              SRCS += libwpa_client.a
#         endif
#    endif
#endif

#SRCS += wpa_ctrl.o \
#	os_unix.o \
#	common.o \
#	wpa_debug.o

$(target): $(SRCS)
	$(CC) -fPIC -shared $(INCLUDES) $(LDFLAGS) $(EXT_LDFLAGS) $^ -o $@ $(LIBS)
	@cp $@ $(PREFIX)/lib

####################################################################
all: $(target) install 
clean:
	rm -rf $(target)
