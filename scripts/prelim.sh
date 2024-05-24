#!/bin/bash

if [ ! -d /mnt/a/build/packages ]; then
	mkdir /mnt/a/build/packages
fi

function install_dep {
	printf "Dependency $1: \n"
	if [ -f /mnt/a/build/packages/$1 ]; then
		printf "File exists \n"
	else
		wget -O /mnt/a/build/packages/$1.txz https://mirrors.slackware.com/slackware/slackware64-current/slackware64/$2 &>/dev/null
	fi
	
	printf "File in packages \n"
	installpkg /mnt/a/build/packages/$1.txz &>/dev/null
	printf "Package installed \n"
}

install_dep bc ap/bc-1.07.1-x86_64-6.txz
install_dep binutils d/binutils-2.42-x86_64-1.txz
install_dep bison d/bison-3.8.2-x86_64-1.txz
install_dep cpio a/cpio-2.15-x86_64-1.txz
install_dep gc l/gc-8.2.6-x86_64-1.txz
#install_dep gcc d/gcc-14.1.0-x86_64-1.txz
installpkg /mnt/a/build/packages/gcc.txz
install_dep git d/git-2.45.1-x86_64-1.txz
install_dep glibc l/glibc-2.39-x86_64-2.txz
install_dep infozip a/infozip-6.0-x86_64-7.txz
install_dep guile d/guile-3.0.9-x86_64-2.txz
install_dep libmpc l/libmpc-1.3.1-x86_64-1.txz
install_dep make d/make-4.4.1-x86_64-1.txz
install_dep ncurses l/ncurses-6.5-x86_64-1.txz
install_dep perl d/perl-5.38.2-x86_64-2.txz
install_dep m4 d/m4-1.4.19-x86_64-1.txz
install_dep elfutils l/elfutils-0.191-x86_64-1.txz
install_dep openssl n/openssl-3.3.0-x86_64-1.txz
#install_dep squashfs ap/squashfs
install_dep gettext a/gettext-0.22.5-x86_64-2.txz
install_dep gettext-tools d/gettext-tools-0.22.5-x86_64-2.txz
install_dep isl l/isl-0.26-x86_64-1.txz
#install_dep kernel-headers d/kernel-headers-6.9.1-x86-1.txz
installpkg /mnt/a/build/packages/kernel-headers.txz
#install_dep libc 
