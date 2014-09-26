To set permissions on /dev/hidrawX device, use the following udev rule :
KERNEL=="hidraw*", SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0fde", ATTRS{idProduct}=="ca01", MODE="0664", GROUP="agocontrol"
