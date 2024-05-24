http://mirror.centos.org/altarch/7/experimental/x86_64/Packages/
https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/
https://mirrors.slackware.com/slackware/slackware64-current/slackware64/a/

Getting Kernel 5.10.28 compat drivers for Lenovo EMC PX4-400D for UNRAID 6.9.2

This allows for b4b.ko to be insmod as a kernel module which provides LCD and input / LED control.
Also enables LCD images to work in the misc folder.

# Steps

## Prerequisite

Unraid running on a USB stick in the PX4
External Hard Drive plugged into USB /dev/sdc1

1. Get the linux kernel source downloaded and extracted
/linux-5.10.28-Unraid (add the -Unraid)

2. Ge the appropriate devel headers that match the kernel version/
Might not find exact, but slackware or centos will have something.
I used centOS RPM and extracted with "bsdtar"

## Building and Testing

1. mkdir /tmp/d

2. mount /dev/sdc1 /tmp/d

3. Run scripts to download and install the latest build packages for kernel module compilation
These may need updated to match slackware-current

1.sh
2.sh

4. Goto /usr/src/linux-5.10.28-Unraid and edit the makefile to add "-Unraid" to EXTRAVERSION

5. Extract the devel kernel headers and copy to /usr/include

6. Copy patches over from /usr/src/..Unraid2
Ru with "git apply *.patch"
Copy over System.map? Modules.symvers?

7. Run "make menuconfig" from the linux src folder
Disable trace and kernel debug and wireless drivers

8. Make what is needed

make modules_prepare
make modules
make headers
make headers_install

9. Go to b4b-unraid-master folder (from git here)

make clean
make

May need to fix errors
There are build sensitivities here with kbuild and the syntax for external kernel modules that may change at whim

10. Validate and test

modinfo b4b.ko - ensure that the version matches `uname -r` with "-Unraid" at the end

insmod b4b.ko - see if it load, use dmesg to see why if not

possible causes: version magic mismatch with kernel version, unknown symbols - chekc the System.map and /proc/ksyscall to see if calls allowed
