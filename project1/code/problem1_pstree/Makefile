obj-m := pstreecall.o
KID := /usr/local/android-kernel/kernel/goldfish
CROSS_COMPILE=/usr/local/ndk/android-ndk-r11-linux-x86_64.tar.bz2/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
all:
	make -C $(KID) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules
	
clean:
	rm -rf *.ko *.o *.mod.c *.order *.symvers
