# fl2k-psk
built on initial work in https://github.com/peterbmarks/fl2k and https://osmocom.org/projects/osmo-fl2k/wiki

###### install libusb
sudo apt-get install libusb-1.0-0-dev

###### increase usb buffer size
On Ubuntu: $ sudo sh -c 'echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb'

###### install udev rule to allow non root users to access the device
$ sudo cp osmo-fl2k.rules /etc/udev/rules.d/
$ sudo udevadm control -R
$ sudo udevadm trigger

###### compile
$ make
