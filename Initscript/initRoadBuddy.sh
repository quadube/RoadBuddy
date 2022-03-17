#!/bin/sh

#camera module
modprobe bcm2835-v4l2

#module for the bluetooth dongle
modprobe btusb

#i2c modules
modprobe i2c-dev
modprobe i2c-bcm2835

#rtc module
modprobe rtc-ds1307
#0x68 is the address of the rtc
echo ds1307 0x68 >> /sys/class/i2c-adapter/i2c-1/new_device

#assign the rtc clock as system clock
hwclock -s

#usb pen drive daemon
/etc/roadbuddy/daemons/dPendriveManager

#get car status daemon
/etc/roadbuddy/daemons/dGetCarStatus

#bluetooth daemon
/usr/libexec/bluetooth/bluetoothd &

#insert buttons driver
insmod /etc/roadbuddy/drivers/butt.ko

#insert buzzer driver
insmod /etc/roadbuddy/drivers/buzz.ko

#run the main process
#/etc/roadbuddy/roadbuddy
