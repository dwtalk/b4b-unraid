#!/bin/bash

# Build kernel

#cd /boot/tmp
#wget https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.19.107.tar.gz
#tar -C /usr/src/ -zxvf linux-4.19.107.tar.gz
#ln -sf /usr/src/linux-4.19.107 /usr/src/linux
#cp -rf /usr/src/linux-4.19.107-unRAID/* /usr/src/linux/
#cp -f /usr/src/linux-4.19.107-unRAID/.config /usr/src/linux/

cd /usr/src/linux-5.10.28-Unraid
make oldconfig

#echo "
#In the next step you will be presented with menuconfig.
#You will need to choose cfg80211, cfg80211 wireless extensions, mac80211 and RF switch (rfkill):
#[*] Networking support
#-*-   Wireless
#<M>     cfg80211 - wireless configuration API
#[ ]       nl80211 testmode command (NEW)
#[ ]       enable developer warnings (NEW)
#[*]       enable powersave by default (NEW)
#[*]       cfg80211 wireless extensions compatibility
#<M>     Generic IEEE 802.11 Networking Stack (mac80211)
#        Default rate control algorithm (Minstrel)
#[ ]     Enable mac80211 mesh networking (pre-802.11s) support (NEW)
#[ ]     Trace all mac80211 debug messages (NEW)
#[ ]     Select mac80211 debugging features (NEW)
#<M>   RF switch subsystem support
#"

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
