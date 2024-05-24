#!/bin/bash

mkdir /mnt/a
mkdir /mnt/b
mount /dev/sda1 /mnt/a
mount /dev/sdb1 /mnt/b
mv /usr/src/linux-6.1.79-Unraid /usr/src/linux-6.1.79-Unraid2
ln -s /mnt/b/linux-6.1.79 /usr/src/linux-6.1.79-Unraid

