HOST?=DEFAULT_HOST

CFLAGS?=-O3 -g
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX=
EXEC_PREFIX?=$(PREFIX)
#KERNELDIR?=$(EXEC_PREFIX)/Pious
KERNELDIR?=/Pious
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=-fno-stack-protector -fpic \
		-fshort-wchar -O3 -ffreestanding -nostdlib $(DEBUG_FLAGS) -I$(SHAREDDIR)/include \
		-I/usr/include/efi -I/usr/include/efi/$(DEFAULT_ARCH) -I/usr/include/efi/protocol
CPPFLAGS:=$(CPPFLAGS) -Iinclude -D$(DEFAULT_ARCH) $(DEBUG_FLAGS) -I$(SHAREDDIR)/indlude
LDFLAGS:=$(LDFLAGS) -nostdlib -znocombreloc -shared --warn-common --no-undefined -znocombreloc \
		-Bsymbolic -L /usr/lib -Iinclude -I$(SHAREDDIR)/include
LIBS:=$(LIBS) -nostdlib -lefi -lgnuefi


ARCHDIR=arch/$(DEFAULT_ARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
kernel/kernel.o \
kernel/graphics.o \
kernel/memory.o \


OBJS=\
$(KERNEL_OBJS) \

LINK_LIST=\
$(LDFLAGS) \
$(KERNEL_OBJS) \
$(LIBS) \

.PHONY: all clean install install-kernel
.SUFFIXES: .o .c .S

all: 

Kernel.exe: $(OBJS)
	$(DEFAULT_ARCH)-elf-gcc $(KERNEL_OBJS) -o $@ $(CFLAGS) -T $(ARCHDIR)/linker.ld -Bdynamic -pie
.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS) -T $(ARCHDIR)/linker.ld

.S.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	rm -f Kernel.exe
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-kernel

install-kernel: Kernel.exe
	mkdir -p $(DESTDIR)$(KERNELDIR)
	cp Kernel.exe $(DESTDIR)$(KERNELDIR)

-include $(OBJS:.o=.d)
