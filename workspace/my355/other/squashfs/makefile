#my355

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

all:
	mkdir -p output
	mkdir -p output/bin
	mkdir -p output/lib
	cp /usr/bin/*squashfs output/bin/
	cp /usr/lib/aarch64-linux-gnu/liblzma.so.5.4.1 output/lib/liblzma.so.5
	cp /usr/lib/aarch64-linux-gnu/liblzo2.so.2.0.0 output/lib/liblzo2.so.2
	cp /usr/lib/aarch64-linux-gnu/liblz4.so.1.9.4 output/lib/liblz4.so.1
	cp /usr/lib/aarch64-linux-gnu/libzstd.so.1.5.4 output/lib/libzstd.so.1

clean:
	rm -rf output