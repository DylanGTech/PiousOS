PROJECTS=bootloader kernel
DEFAULT_ARCH=x86_64
DEFAULT_HOST=$(DEFAULT_ARCH)-elf

export PATH:=$(COMPILER_PATH)/bin:$(PATH)

MAKE=make
ARCH=x86_64
HOST=$(ARCH)-elf

AR=$(HOST)-ar
AS=$(HOST)-as
CC=$(HOST)-gcc

PREFIX=/usr
EXEC_PREFIX=$(PREFIX)
BOOTDIR=/EFI/BOOT
LIBDIR=$(EXEC_PREFIX)/lib
INCLUDEDIR=$(PREFIX)/include
SHAREDDIR=$(dir $(realpath $(firstword $(MAKEFILE_LIST))))shared

DEBUG_FLAGS=-DDEBUG_PIOUS

CFLAGS=-O3 -g
CPPFLAGS=''

# Configure the cross-compiler to use the desired system root.
SYSROOT=$(dir $(realpath $(firstword $(MAKEFILE_LIST))))sysroot
CC="$(CC) --sysroot=$(SYSROOT)"


all: build

clean:
	rm -rf $(SYSROOT)
	cd bootloader && DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) clean
	cd kernel && DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) clean

build: clean
	cd bootloader && DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) install
	cd kernel && DEFAULT_ARCH="$(DEFAULT_ARCH)" DEFAULT_HOST="$(DEFAULT_HOST)" DESTDIR="$(SYSROOT)" SHAREDDIR="$(SHAREDDIR)" DEBUG_FLAGS="$(DEBUG_FLAGS)" $(MAKE) install

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
ifeq ($(ARCH), x86_64)
	qemu-system-$(ARCH) -bios OVMF_$(ARCH).fd -drive file=Pious.img -d guest_errors
endif

ifeq ($(ARCH), aarch64)
	dd if=/dev/zero of=flash0.img bs=1M count=64 
	dd if=/usr/share/qemu-efi/QEMU_EFI.fd of=flash0.img conv=notrunc
	dd if=/dev/zero of=flash1.img bs=1M count=64

	qemu-system-$(ARCH) -d guest_errors -machine virt -d int -no-reboot -no-shutdown \
	-smp 1 \
	-device virtio-blk-device,drive=hd0 \
	-drive if=none,format=raw,file=Pious.img,id=hd0 \
	-pflash flash0.img -pflash flash1.img
endif