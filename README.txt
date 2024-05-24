https://mirrors.edge.kernel.org/pub/linux/kernel/v6.x/
https://mirrors.slackware.com/slackware/slackware64-current/slackware64/
https://github.com/dwtalk/linux-unraid-6.12.10

Getting Kernel 6.1.79 compat drivers for Lenovo EMC PX4-400D for UNRAID 6.12.10

This allows for b4b.ko to be insmod as a kernel module which provides LCD and input / LED control.
Also enables LCD images to work in the misc folder.

# Steps

## Prerequisite

Unraid running on a device (either physical or virtual machine). Ensure that you have some disk that is fast to place the linux source.

1. Get the linux kernel source downloaded and extracted to /usr/src
Use the version here: https://github.com/dwtalk/linux-unraid-6.12.10

2. Run prelim.sh to install compilation requirements

## Building and Testing

1. Goto /usr/src/linux-6.1.79-Unraid --DO NOT edit the makefile to add "-Unraid" to EXTRAVERSION if using a version already prepared for unraid

2. If patches not already applied, copy patches over from /usr/src/..Unraid2
Ru with "git apply *.patch"
Copy over System.map? .oldconfig or .config or run make olddefconfig

3. Make what is needed

make modules_prepare
make modules
make headers
make headers_install
make

4. Go to b4b-unraid-master folder (from git here)

make clean
make

May need to fix errors
There are build sensitivities here with kbuild and the syntax for external kernel modules that may change at whim

10. Validate and test

modinfo b4b.ko - ensure that the version matches `uname -r` with "-Unraid" at the end

insmod b4b.ko - see if it load, use dmesg to see why if not

possible causes: version magic mismatch with kernel version, unknown symbols - check the System.map and /proc/ksyscall to see if calls allowed
