HOST?=DEFAULT_HOST

CFLAGS?=-O2 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX=
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/EFI/BOOT
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -fno-stack-protector -fpic -I$(SHAREDDIR)/include -fshort-wchar -O3 -ffreestanding \
				-I/usr/include/efi -I/usr/include/efi/$(DEFAULT_ARCH) -I/usr/include/efi/protocol 
CPPFLAGS:=$(CPPFLAGS) -Iinclude -D$(DEFAULT_ARCH) -I$(SHAREDDIR)/include
LDFLAGS:=$(LDFLAGS) -nostdlib -znocombreloc -shared --warn-common --no-undefined -znocombreloc \
		-Bsymbolic -L /usr/lib -Iinclude -I$(SHAREDDIR)/include
LIBS:=$(LIBS) -nostdlib -lefi -lgnuefi

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
/usr/lib/crt0-efi-$(DEFAULT_ARCH).o


.PHONY: all clean install install-bootloader
.SUFFIXES: .o .c .S

all: 

BOOTX64.EFI: $(OBJS)
	$(DEFAULT_ARCH)-elf-ld -T /usr/lib/elf_$(DEFAULT_ARCH)_efi.lds -o boot.so $(CFLAGS) $(LINK_LIST) -lgnuefi -lefi

	objcopy -j .text -j .sdata -j .data -j .dynamic \
	-j .dynsym  -j .rel -j .rela -j .reloc \
	--target=efi-app-$(DEFAULT_ARCH) boot.so BOOTX64.EFI

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS) $(DEBUG_FLAGS) -T $(ARCHDIR)/linker.ld

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f BOOTX64.EFI
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-bootloader

install-bootloader: BOOTX64.EFI
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp BOOTX64.EFI $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
