#!/bin/sh
# setup.sh (builds cross-compiler and references it in the PATH of the current shell session
sudo apt-get install make qemu gcc gnu-efi bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo


mkdir Compiler
cd Compiler
mkdir src
cd src

mkdir binutils
cd binutils
ftp -in ftp.gnu.org << SCRIPTEND
user anonymous
binary
cd gnu/binutils
mget binutils-2.33.1.tar.gz
SCRIPTEND

tar -xvzf binutils-2.33.2.tar.gz

cd ..
mkdir gcc
cd gcc
ftp -in ftp.gnu.org << SCRIPTEND
user anonymous
binary
cd gnu/gcc/gcc-9.2.0/
mget gcc-9.2.0.tar.gz
SCRIPTEND

tar -xvzf gcc-9.2.0.tar.gz

cd ../..

export PREFIX="$PWD"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

cd src

mkdir build-binutils
cd build-binutils
../binutils-2.33.2/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

cd ..
mkdir build-gcc
cd build-gcc
../gcc-9.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-gcc

export PATH="$PREFIX/bin:$PATH"
