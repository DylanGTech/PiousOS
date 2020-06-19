PROJECTS=bootloader kernel
DEFAULT_ARCH=aarch64
DEFAULT_HOST=$(DEFAULT_ARCH)-elf

export PATH:=$(COMPILER_PATH)/bin:$(PATH)

MAKE=make
HOST=$(ARCH)-elf

# Configure the cross-compiler to use the desired system root.
SYSROOT=$(dir $(realpath $(firstword $(MAKEFILE_LIST))))sysroot

AR=$(DEFAULT_HOST)-ar
AS=$(DEFAULT_HOST)-as
CC=$(DEFAULT_HOST)-gcc --sysroot=$(SYSROOT)

PREFIX=/usr
EXEC_PREFIX=$(PREFIX)
BOOTDIR=/EFI/BOOT
LIBDIR=$(EXEC_PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include
SHAREDDIR=$(dir $(realpath $(firstword $(MAKEFILE_LIST))))shared

DEBUG_FLAGS=-DDEBUG_PIOUS

CFLAGS=-O3 -g
CPPFLAGS=''


all: build

clean:
	rm -rf $(SYSROOT)
	cd bootloader && CC="$(CC)" AS="$(AS)" AR="$(AR)" DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) clean
	cd kernel && CC="$(CC)" AS="$(AS)" AR="$(AR)" DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) clean

build: clean
	cd bootloader && CC="$(CC)" AS="$(AS)" AR="$(AR)" DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) install
	cd kernel && CC="$(CC)" AS="$(AS)" AR="$(AR)" DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) install

image: LOOPDEV=$(shell losetup -f)

image: build

	./createBlankUEFIImage.sh
	cp BlankUEFI.img Pious.img

	sudo losetup --offset 1048576 --sizelimit 66060288 $(LOOPDEV) Pious.img


	sudo mkdosfs -F 32 $(LOOPDEV)
	sudo mount $(LOOPDEV) /mnt
	sudo cp -R sysroot/* /mnt/

	sudo umount /mnt
	sudo losetup -d $(LOOPDEV)

qemu: image
ifeq ($(DEFAULT_ARCH), x86_64)
	qemu-system-$(DEFAULT_ARCH) -bios OVMF_$(DEFAULT_ARCH).fd -drive file=Pious.img -d guest_errors -m 2G -debugcon file:uefi_debug.log -global isa-debugcon.iobase=0x402
endif

ifeq ($(DEFAULT_ARCH), aarch64)
	dd if=/dev/zero of=flash0.img bs=1M count=64 
	dd if=OVMF_$(DEFAULT_ARCH).fd of=flash0.img conv=notrunc
	dd if=/dev/zero of=flash1.img bs=1M count=64

	#qemu-system-$(DEFAULT_ARCH) -d guest_errors -machine virt -d int -no-reboot -no-shutdown -m 1G \
	#-smp 1 \
	#-device virtio-blk-device,drive=hd0 \
	#-drive if=none,format=raw,file=Pious.img,id=hd0 \
	#-pflash flash0.img -pflash flash1.img

	qemu-system-$(DEFAULT_ARCH) -m 2G -cpu cortex-a72 -M virt -drive format=raw,file=flash0.img,if=pflash -drive format=raw,file=flash1.img,if=pflash -drive if=none,file=Pious.img,id=hd0,format=raw -device virtio-blk-device,drive=hd0 -d guest_errors -device virtio-gpu-pci -device qemu-xhci -device usb-mouse -device usb-kbd -serial stdio -net none > debug.txt
endif