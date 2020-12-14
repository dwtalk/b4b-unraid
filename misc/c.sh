mkdir /tmp/c
sudo mv /usr/src/linux-4.19.107-Unraid/  /usr/src/linux-4.19.107-Unraid2/
mount /dev/sdc /tmp/c
sudo ln -sf /tmp/c/linux-4.19.107/ /usr/src/linux-4.19.107-Unraid
