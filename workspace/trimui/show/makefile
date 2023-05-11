TARGET=show

.PHONY: build
.PHONY: clean

CC = $(CROSS_COMPILE)gcc

SYSROOT     := $(shell $(CC) --print-sysroot)

INCLUDEDIR = $(SYSROOT)/usr/include
CFLAGS = -I$(INCLUDEDIR)
LDFLAGS = -s -lSDL -lSDL_image -lz -lm

OPTM=-O3

build: 
	$(CC) -o $(TARGET).elf show.c $(CFLAGS) $(LDFLAGS) $(OPTM)
clean:
	rm -f $(TARGET).elf