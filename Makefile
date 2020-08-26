DEFAULT_ARCH=x86_64
DEFAULT_HOST=$(DEFAULT_ARCH)-elf

export PATH:=$(COMPILER_PATH)/bin:$(PATH)

MAKE=make
HOST=$(ARCH)-elf

# Configure the cross-compiler to use the desired system root.
SYSROOT=sysroot

COMMON_DEBUG_FLAGS=-DDEBUG_PIOUS
COMMON_CFLAGS=-O3 -g
COMMON_CPPFLAGS=


KERNEL_CFLAGS:=$(COMMON_CFLAGS) -fno-stack-protector \
		-fshort-wchar -O3 -ffreestanding -nostdlib $(DEBUG_FLAGS) -Iinc \
		-Ignu-efi/inc -Ignu-efi/inc/$(DEFAULT_ARCH) -Ignu-efi/inc/protocol
KERNEL_CPPFLAGS:=$(COMMON_CPPFLAGS) -D$(DEFAULT_ARCH) $(DEBUG_FLAGS) $(COMMON_DEBUG_FLAGS)
KERNEL_LDFLAGS:=-nostdlib -znocombreloc --warn-common --no-undefined -znocombreloc \
		-Bsymbolic -Iinc
KERNEL_LIBS:=-nostdlib -L gnu-efi/$(DEFAULT_ARCH)/gnuefi -L gnu-efi/$(DEFAULT_ARCH)/lib

KERNEL_SRC_C:=$(shell find src/kernel arch/kernel/$(DEFAULT_ARCH) -name *.c)
KERNEL_SRC_ASM:=$(shell find arch/kernel/$(DEFAULT_ARCH) -name *.S)

KERNEL_OBJ_C:=$(patsubst %.c,%.o,$(KERNEL_SRC_C))
KERNEL_OBJ_ASM:=$(patsubst %.S,%.o,$(KERNEL_SRC_ASM))



BOOTLOADER_CFLAGS:=$(COMMON_CFLAGS) -fno-stack-protector -fpic -Iinc -fshort-wchar -ffreestanding \
		-Ignu-efi/inc -Ignu-efi/inc/$(DEFAULT_ARCH) -Ignu-efi/inc/protocol $(DEBUG_FLAGS)
BOOTLOADER_CPPFLAGS:=$(COMMON_CPPFLAGS) -D$(DEFAULT_ARCH) $(DEBUG_FLAGS) $(COMMON_DEBUG_FLAGS)
BOOTLOADER_LDFLAGS:=-nostdlib -znocombreloc -shared --warn-common --no-undefined -znocombreloc \
		-Bsymbolic -Iinclude -Iinc gnu-efi/$(DEFAULT_ARCH)/gnuefi/crt0-efi-$(DEFAULT_ARCH).o
BOOTLOADER_LIBS:=-nostdlib -Lgnu-efi/$(DEFAULT_ARCH)/gnuefi -Lgnu-efi/$(DEFAULT_ARCH)/lib -L/usr/lib -lefi -lgnuefi


BOOTLOADER_SRC_C:=$(shell find src/bootloader arch/bootloader/$(DEFAULT_ARCH) -name *.c)
BOOTLOADER_OBJ_C:=$(patsubst %.c,%.o,$(BOOTLOADER_SRC_C))

.PHONY: all config clean build image qemu
all: build

clean:
	rm -rf $(SYSROOT)
	rm -f Kernel.exe boot.so
	rm -f $(KERNEL_OBJ_ASM) $(KERNEL_OBJ_C) $(patsubst %.o,%.d,$(KERNEL_OBJ_C) $(KERNEL_OBJ_ASM))
	rm -f $(BOOTLOADER_OBJ_C) $(patsubst %.o,%.d,$(BOOTLOADER_OBJ_C))


config:
ifeq ($(DEFAULT_ARCH),x86_64)
KERNEL_CFLAGS+= -DEFI_FUNCTION_WRAPPER
BOOTLOADER_EXEC=BOOTX64.EFI
endif

ifeq ($(DEFAULT_ARCH),aarch64)
KERNEL_CFLAGS+= -DEFI_FUNCTION_WRAPPER -mstrict-align
BOOTLOADER_EXEC=BOOTAA64.EFI
endif

build: config clean $(BOOTLOADER_EXEC) Kernel.exe

	mkdir $(SYSROOT)
	mkdir $(SYSROOT)/Pious
	cp Kernel.exe $(SYSROOT)/Pious/Kernel.exe
	mkdir $(SYSROOT)/EFI
	mkdir $(SYSROOT)/EFI/BOOT
	cp $(BOOTLOADER_EXEC) $(SYSROOT)/EFI/BOOT/$(BOOTLOADER_EXEC)


BOOTX64.EFI: $(BOOTLOADER_OBJ_C)
	$(DEFAULT_ARCH)-linux-gnu-ld $(BOOTLOADER_OBJ_C) -T gnu-efi/gnuefi/elf_$(DEFAULT_ARCH)_efi.lds -o boot.so $(BOOTLOADER_LDFLAGS) -L gnu-efi/$(DEFAULT_ARCH)/gnuefi -L gnu-efi/$(DEFAULT_ARCH)/lib $(BOOTLOADER_LIBS)

	$(DEFAULT_ARCH)-linux-gnu-objcopy -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .reloc \
	--target=efi-app-$(DEFAULT_ARCH) boot.so BOOTX64.EFI

BOOTAA64.EFI: $(BOOTLOADER_OBJ_C)
	$(DEFAULT_ARCH)-linux-gnu-ld $(BOOTLOADER_OBJ_C) -T gnu-efi/gnuefi/elf_$(DEFAULT_ARCH)_efi.lds -o boot.so $(BOOTLOADER_LDFLAGS) -L gnu-efi/$(DEFAULT_ARCH)/gnuefi -L gnu-efi/$(DEFAULT_ARCH)/lib --defsym=EFI_SUBSYSTEM=10 $(BOOTLOADER_LIBS)

	$(DEFAULT_ARCH)-linux-gnu-objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* -j .reloc -O binary boot.so BOOTAA64.EFI

src/bootloader/%.o: src/bootloader/%.c
	$(DEFAULT_ARCH)-linux-gnu-gcc -MD -c $< -o $@ -std=gnu11 $(BOOTLOADER_CFLAGS) $(BOOTLOADER_CPPFLAGS) -T arch/bootloader/$(DEFAULT_ARCH)/linker.ld

src/kernel/%.o: src/kernel/%.c
	$(DEFAULT_HOST)-gcc -MD -c $< -o $@ -std=gnu11 $(KERNEL_CFLAGS) $(KERNEL_CPPFLAGS) -T arch/kernel/$(DEFAULT_ARCH)/linker.ld $(KERNEL_LIBS)

arch/kernel/$(DEFAULT_ARCH)/%.o: arch/kernel/$(DEFAULT_ARCH)/%.c
	$(DEFAULT_HOST)-gcc -MD -c $< -o $@ -std=gnu11 $(KERNEL_CFLAGS) $(KERNEL_CPPFLAGS) -T arch/kernel/$(DEFAULT_ARCH)/linker.ld $(KERNEL_LIBS)

arch/kernel/$(DEFAULT_ARCH)/%.o: arch/kernel/$(DEFAULT_ARCH)/%.S
	$(DEFAULT_HOST)-gcc -MD -c $< -o $@ $(KERNEL_CFLAGS) $(KERNEL_CPPFLAGS) $(KERNEL_LIBS)


Kernel.exe: $(KERNEL_OBJ_C) $(KERNEL_OBJ_ASM)
	$(DEFAULT_HOST)-gcc $(KERNEL_OBJ_C) $(KERNEL_OBJ_ASM) -o $@ $(KERNEL_CFLAGS) -T arch/kernel/$(DEFAULT_ARCH)/linker.ld -Bdynamic



image: LOOPDEV=$(shell losetup -f)

image: build

	./createBlankUEFIImage.sh
	cp BlankUEFI.img Pious.img

	sudo losetup --offset 1048576 --sizelimit 66060288 $(LOOPDEV) Pious.img


	sudo mkdosfs -F 32 $(LOOPDEV)
	sudo mount $(LOOPDEV) /mnt
	sudo cp -R $(SYSROOT)/* /mnt

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

	qemu-system-$(DEFAULT_ARCH) -s -S -m 2G -cpu cortex-a72 -M virt -drive format=raw,file=flash0.img,if=pflash -drive format=raw,file=flash1.img,if=pflash -drive if=none,file=Pious.img,id=hd0,format=raw -device virtio-blk-device,drive=hd0 -d guest_errors -device virtio-gpu-pci -device qemu-xhci -device usb-mouse -device usb-kbd -serial stdio -net none
endif