#!/bin/sh

# taken verbatim from https://github.com/octathorp/m17_tools/blob/master/ADB/usb_enabler.sh

CONFIG_STRING=adb
PID=0x0006

mount -t configfs none /sys/kernel/config
mkdir /sys/kernel/config/usb_gadget/rockchip  -m 0770
echo 0x2207 > /sys/kernel/config/usb_gadget/rockchip/idVendor
echo $PID > /sys/kernel/config/usb_gadget/rockchip/idProduct
mkdir /sys/kernel/config/usb_gadget/rockchip/strings/0x409   -m 0770
SERIAL=`cat /proc/cpuinfo | grep Serial | awk '{print $3}'`
if [ -z $SERIAL ];then
	SERIAL=0123456789ABCDEF
fi
echo $SERIAL > /sys/kernel/config/usb_gadget/rockchip/strings/0x409/serialnumber
echo "rockchip"  > /sys/kernel/config/usb_gadget/rockchip/strings/0x409/manufacturer
echo "rk3xxx"  > /sys/kernel/config/usb_gadget/rockchip/strings/0x409/product
mkdir /sys/kernel/config/usb_gadget/rockchip/configs/b.1  -m 0770
mkdir /sys/kernel/config/usb_gadget/rockchip/configs/b.1/strings/0x409  -m 0770
echo 500 > /sys/kernel/config/usb_gadget/rockchip/configs/b.1/MaxPower
echo \"$CONFIG_STRING\" > /sys/kernel/config/usb_gadget/rockchip/configs/b.1/strings/0x409/configuration

mkdir /sys/kernel/config/usb_gadget/rockchip/functions/ffs.adb
ln -s /sys/kernel/config/usb_gadget/rockchip/functions/ffs.adb /sys/kernel/config/usb_gadget/rockchip/configs/b.1/ffs.adb

mkdir -p /dev/usb-ffs/adb
mount -o uid=2000,gid=2000 -t functionfs adb /dev/usb-ffs/adb
export service_adb_tcp_port=5555
adbd&
sleep 1

UDC=`ls /sys/class/udc/| awk '{print $1}'`
echo $UDC > /sys/kernel/config/usb_gadget/rockchip/UDC

