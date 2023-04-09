# SPDX-License-Identifier: GPL-2.0
# Core module
#obj-m             += fbtft.o
#fbtft-y                          += fbtft-core.o fbtft-sysfs.o fbtft-bus.o fbtft-io.o

# drivers
obj-m += fb_ili9488.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
install:
	sudo cp fb_ili9488.ko /lib/modules/$(shell uname -r)/kernel/drivers/staging/fbtft/ && sudo depmod && sudo cp ./sun8i-h3-ili9488.dts /boot/dtb/overlay/ && sudo dtc -O dtb -o /boot/dtb/overlay/sun8i-h3-ili9488.dtbo /boot/dtb/overlay/sun8i-h3-ili9488.dts && sudo armbian-add-overlay sun8i-h3-ili9488.dts

