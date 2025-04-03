ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = keymon
PRODUCT = $(TARGET).elf

CC = $(CROSS_COMPILE)gcc -I../../all/common/ -I../platform/
FLAGS	= -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s

all:
	$(CC) $(TARGET).c -o $(PRODUCT) $(FLAGS)
clean:
	rm -rf $(PRODUCT)