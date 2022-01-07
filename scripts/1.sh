#!/bin/bash

# Install dependencies

mkdir /tmp/d/packages

function install_dep {
  printf "Dependency $1: "
  rm /tmp/d/packages/$1.txz &>/dev/null
  wget -O /tmp/d/packages/$1.txz https://mirrors.slackware.com/slackware/slackware64-current/slackware64/$2 &>/dev/null
  printf "downloaded "
  installpkg /tmp/d/packages/$1.txz &>/dev/null
  printf "installed\n"
}

install_dep bc ap/bc-1.07.1-x86_64-5.txz
install_dep binutils d/binutils-2.37-x86_64-1.txz
install_dep bison d/bison-3.8.2-x86_64-1.txz
install_dep cpio a/cpio-2.13-x86_64-3.txz
install_dep gc l/gc-8.0.6-x86_64-1.txz
install_dep gcc d/gcc-11.2.0-x86_64-2.txz
install_dep git d/git-2.34.1-x86_64-1.txz
install_dep glibc l/glibc-2.33-x86_64-4.txz
install_dep infozip a/infozip-6.0-x86_64-7.txz
#install_dep kernel-headers d/kernel-headers-5.15.13-x86-1.txz
install_dep guile d/guile-3.0.7-x86_64-1.txz
install_dep libmpc l/libmpc-1.2.1-x86_64-3.txz
install_dep make d/make-4.3-x86_64-3.txz
install_dep ncurses l/ncurses-6.3-x86_64-1.txz
install_dep perl d/perl-5.34.0-x86_64-1.txz
install_dep m4 d/m4-1.4.19-x86_64-1.txz
install_dep elfutils l/elfutils-0.186-x86_64-1.txz
install_dep openssl n/openssl-1.1.1m-x86_64-1.txz
install_dep squashfs ap/squashfs-tools-4.5-x86_64-2.txz
install_dep gettext a/gettext-0.21-x86_64-3.txz
install_dep gettext-tools d/gettext-tools-0.21-x86_64-3.txz
install_dep isl l/isl-0.24-x86_64-1.txz

#installpkg /boot/glibc.txz &>/dev/null
#printf "glibc installed"


#ln -s /usr/lib64/libunistring.so.2.1.0 /usr/lib64/libunistring.so.0
#ln -s /usr/lib64/libmpfr.so.6.0.2 /usr/lib64/libmpfr.so.4

# Build kernel

mkdir /tmp/d/kernel
cd /tmp/d/kernel
#wget https://www.kernel.org/pub/linux/kernel/v5.x/linux-5.10.28.tar.gz
#tar -C /usr/src/ -zxvf linux-5.10.28.tar.gz
#ln -sf /usr/src/linux-4.19.107 /usr/src/linux
#cp -rf /usr/src/linux-4.19.107-unRAID/* /usr/src/linux/
#cp -f /usr/src/linux-4.19.107-unRAID/.config /usr/src/linux/

#cd /usr/src/linux
#git apply *.patch
#make oldconfig

#read -p "Press Enter to continue."
#make menuconfig
#make clean
#make headers_install INSTALL_HDR_PATH=/usr
#make
#make modules_install
#make firmware_install

# Build RTL8812AU driver

#rm -R /boot/tmp/rtl8188eu &>/dev/null
#git clone https://github.com/quickreflex/rtl8188eus.git /boot/tmp/rtl8188eu
#cd /boot/tmp/rtl8188eu
#make clean
#make all
