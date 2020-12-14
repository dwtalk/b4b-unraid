#!/bin/bash

# Install dependencies

#mkdir /boot/packages
#mkdir /boot/tmp

function install_dep {
  printf "Dependency $1: "
  rm /boot/packages/$1.txz &>/dev/null
  wget -O /boot/packages/$1.txz https://mirrors.slackware.com/slackware/slackware64-14.2/slackware64/$2 &>/dev/null
  printf "downloaded "
  installpkg /boot/packages/$1.txz &>/dev/null
  printf "installed\n"
}

install_dep bc ap/bc-1.06.95-x86_64-3.txz
install_dep binutils d/binutils-2.26-x86_64-3.txz
install_dep bison d/bison-3.0.4-x86_64-1.txz
install_dep cpio a/cpio-2.12-x86_64-1.txz
install_dep gc l/gc-7.4.2-x86_64-3.txz
install_dep gcc d/gcc-5.3.0-x86_64-3.txz
install_dep git d/git-2.9.0-x86_64-1.txz
#install_dep glibc l/glibc-2.23-x86_64-1.txz
install_dep infozip a/infozip-6.0-x86_64-3.txz
install_dep kernel-headers d/kernel-headers-4.4.14-x86-1.txz
install_dep guile d/guile-2.0.11-x86_64-2.txz
install_dep libmpc l/libmpc-1.0.3-x86_64-1.txz
install_dep make d/make-4.1-x86_64-2.txz
install_dep ncurses l/ncurses-5.9-x86_64-4.txz
install_dep perl d/perl-5.22.2-x86_64-1.txz
install_dep m4 d/m4-1.4.17-x86_64-1.txz
install_dep elfutils l/elfutils-0.163-x86_64-1.txz
install_dep squashfs ap/squashfs-tools-4.3-x86_64-1.txz

#installpkg /boot/glibc.txz &>/dev/null
#printf "glibc installed"

#cd /boot/ix4-300d-master/wixlcm
#make

ln -s /usr/lib64/libunistring.so.2.1.0 /usr/lib64/libunistring.so.0
ln -s /usr/lib64/libmpfr.so.6.0.2 /usr/lib64/libmpfr.so.4

# Build kernel

#cd /boot/tmp
#wget https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.19.107.tar.gz
#tar -C /usr/src/ -zxvf linux-4.19.107.tar.gz
#ln -sf /usr/src/linux-4.19.107 /usr/src/linux
#cp -rf /usr/src/linux-4.19.107-unRAID/* /usr/src/linux/
#cp -f /usr/src/linux-4.19.107-unRAID/.config /usr/src/linux/

#cd /usr/src/linux
#git apply *.patch
#make oldconfig

echo "
In the next step you will be presented with menuconfig.
You will need to choose cfg80211, cfg80211 wireless extensions, mac80211 and RF switch (rfkill):
[*] Networking support
-*-   Wireless
<M>     cfg80211 - wireless configuration API
[ ]       nl80211 testmode command (NEW)
[ ]       enable developer warnings (NEW)
[*]       enable powersave by default (NEW)
[*]       cfg80211 wireless extensions compatibility
<M>     Generic IEEE 802.11 Networking Stack (mac80211)
        Default rate control algorithm (Minstrel)
[ ]     Enable mac80211 mesh networking (pre-802.11s) support (NEW)
[ ]     Trace all mac80211 debug messages (NEW)
[ ]     Select mac80211 debugging features (NEW)
<M>   RF switch subsystem support
"
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
