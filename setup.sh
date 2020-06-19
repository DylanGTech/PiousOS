#!/bin/bash
# setup.sh (builds cross-compiler and references it in the PATH)
sudo apt-get install make gcc bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo qemu-system

mkdir Compiler-$1
cd Compiler-$1
mkdir src
cd src

ftp -inp ftp.gnu.org << SCRIPTEND
user anonymous
binary
cd gnu/binutils
mget binutils-2.33.1.tar.gz
SCRIPTEND

tar -xzf binutils-2.33.1.tar.gz


ftp -inp ftp.gnu.org << SCRIPTEND
user anonymous
binary
cd gnu/gcc/gcc-9.2.0/
mget gcc-9.2.0.tar.gz
SCRIPTEND

tar -xzf gcc-9.2.0.tar.gz

cd ..

export PREFIX="$PWD"
export TARGET=$1-elf
export PATH="$PREFIX/bin:$PATH"

cd src

mkdir build-binutils
cd build-binutils
../binutils-2.33.1/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

cd ..
mkdir build-gcc
cd build-gcc
../gcc-9.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc
make all-target-libgcc
make install-gcc
make install-target-libgcc


echo export PATH=\"$PREFIX/bin:\$PATH\" >> ~/.profile



cd ../..
wget -q http://downloads.sourceforge.net/project/gnu-efi/gnu-efi-3.0.11.tar.bz2
tar -xjf gnu-efi-3.0.11.tar.bz2
mv gnu-efi-3.0.11 gnu-efi

cd gnu-efi
make
sudo make install

cd ..

if [ $1 = "aarch64" ]
    mkdir Compiler-$1-efi
    cd Compiler-$1-efi
    sudo apt-get install binutils-mingw-w64 gcc-mingw-w64
    wget http://www.tysos.org/files/efi/mkgpt-latest.tar.bz2
    tar -xjf mkgpt-latest.tar.bz2
    cd mkgpt && ./configure && make && sudo make install && cd ..
fi