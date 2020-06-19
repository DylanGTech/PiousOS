HOST?=DEFAULT_HOST

CFLAGS?=-O3 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX=
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/EFI/BOOT
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -fno-stack-protector -fpic -I$(SHAREDDIR)/include -fshort-wchar -O3 -ffreestanding \
		-I../gnu-efi/inc -I../gnu-efi/inc/$(DEFAULT_ARCH) -I../gnu-efi/inc/protocol
CPPFLAGS:=$(CPPFLAGS) -Iinclude -D$(DEFAULT_ARCH) -I$(SHAREDDIR)/include
LDFLAGS:=$(LDFLAGS) -nostdlib -znocombreloc -shared --warn-common --no-undefined -znocombreloc \
		-Bsymbolic -Iinclude -I$(SHAREDDIR)/include
LIBS:=$(LIBS) -nostdlib -L../gnu-efi/$(DEFAULT_ARCH)/gnuefi -L../gnu-efi/$(DEFAULT_ARCH)/lib -lefi -lgnuefi

ARCHDIR=arch/$(DEFAULT_ARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(BOOTLOADER_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(BOOTLOADER_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(BOOTLOADER_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(BOOTLOADER_ARCH_LIBS)

BOOTLOADER_OBJS=\
$(BOOTLOADER_ARCH_OBJS) \
bootloader/boot.o

OBJS=\
$(BOOTLOADER_OBJS) \

LINK_LIST=\
$(LDFLAGS) \
$(BOOTLOADER_OBJS) \
$(LIBS) \
../gnu-efi/$(DEFAULT_ARCH)/gnuefi/crt0-efi-$(DEFAULT_ARCH).o


.PHONY: all clean install install-bootloader
.SUFFIXES: .o .c .S

all: 

BOOTX64.EFI: $(OBJS)
	$(DEFAULT_ARCH)-linux-gnu-ld -T ../gnu-efi/gnuefi/elf_$(DEFAULT_ARCH)_efi.lds -o boot.so $(LINK_LIST) -L ../gnu-efi/$(DEFAULT_ARCH)/gnuefi -L ../gnu-efi/$(DEFAULT_ARCH)/lib -lefi -lgnuefi

	$(DEFAULT_ARCH)-linux-gnu-objcopy -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .reloc \
	--target=efi-app-$(DEFAULT_ARCH) boot.so BOOTX64.EFI

BOOTAA64.EFI: $(OBJS)
	$(DEFAULT_ARCH)-linux-gnu-ld -T ../gnu-efi/gnuefi/elf_$(DEFAULT_ARCH)_efi.lds -o boot.so $(LINK_LIST) -L ../gnu-efi/$(DEFAULT_ARCH)/gnuefi -L ../gnu-efi/$(DEFAULT_ARCH)/lib -lefi -lgnuefi --defsym=EFI_SUBSYSTEM=10

	$(DEFAULT_ARCH)-linux-gnu-objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* -j .reloc -O binary boot.so BOOTAA64.EFI
	#--target=efi-app-little$(DEFAULT_ARCH) boot.so BOOTX64.EFI

	#x86_64-w64-mingw32-gcc -nostdlib -Wl,-dll -shared -Wl,--subsystem,10 -e efi_main -o BOOTX64.EFI hello.o data.o -lgcc



.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS) $(DEBUG_FLAGS) -T $(ARCHDIR)/linker.ld

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f BOOTX64.EFI
	rm -f BOOTAA64.EFI
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d
	rm -f boot.so

install: install-bootloader-$(DEFAULT_ARCH)

install-bootloader-x86_64: BOOTX64.EFI
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp BOOTX64.EFI $(DESTDIR)$(BOOTDIR)

install-bootloader-aarch64: BOOTAA64.EFI
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp BOOTAA64.EFI $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
