# PiousOS
PiousOS is a simple system for modern hardware. It boots and runs via UEFI and is henceforth being designed with modularity in mind. x86_64 is currently the only supported platform, and currently the internals simply boots to "Hello!" with interrupts and paging enabled. Aarch64 support is being added and works on compliant hardware, but it currently cannot be tested through Qemu/OVMF due to lack of firmware driver support for GPU.

## Setup Envornment
Building is designed for a Linux host. Currently tested with Ubuntu. Install the required packages and build the necessary cross-compiler with the setup script (only needs to be run once)
```
./setup.sh x86_64
```
The compiler will be used to create the executables used by PiousOS
## Running
Because PiousOS is dependant on UEFI, firmware must support it.

Running PiousOS in qemu can be done through the makefile
```
make qemu
```
**NOTE**: root is required for the appropriate image file to be created

Running PiousOS on real hardware can be achieved through bulding with ``make build``, then copying the contents of ``sysroot``to the root directory of a FAT32-formatted bootable media, such as a flash drive

## Debugging
Debugging can be enabled by changing ``DEBUG_FLAGS=`` to ``DEBUG_FLAGS=-DDEBUG_PIOUS`` in the main makefile, and building it as normal

## Contributing
Contributions of any kind are welcome. Cleaning up the code and making the system more efficient are by far the best way to help as of now. If any major changes are made, please create an Issue describing such changes in advance to avoid any confusion.

## Licensing
Much of the original code can be attributed to KNNSpeed's "Simple Bootloader" and "Simple Kernel" system under a "give credit" license, although they are different, and hence incompatable with one another. The original author's profile is no longer on GitHub.
Dylan Green and other authors' code in this project is licensed under the Apache 2.0 License