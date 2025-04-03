#!/bin/bash
DIR="$(dirname "$0")"
cd "$DIR"

{

# must be connected to wifi
if [ `cat /sys/class/net/wlan0/operstate` != "up" ]; then
	show.elf "$DIR/wifi.png" 8
	exit 0
fi

show.elf "$DIR/ssh.png" 200 &

# switch language from mandarin to english since we require a reboot anyway
locale-gen "en_US.UTF-8"
echo "LANG=en_US.UTF-8" > /etc/default/locale

# install or update ssh server
apt -y update && apt -y install --reinstall openssh-server
echo "d /run/sshd 0755 root root" > /etc/tmpfiles.d/sshd.conf

# enable login root:root
echo "PermitRootLogin yes" >> /etc/ssh/sshd_config
printf "root\nroot" | passwd root

killall -9 show.elf

} &> ./log.txt

mv "$DIR" "$DIR.disabled"
reboot # required for root:root to be recognized and new locale to take effect
