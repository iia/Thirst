Thirst
======

Thirst is one of my personal projects for fun. In this project a device is developed
which can monitor soil moisture and report via email in case the moisture is above or
below a pre-configured threshold. Main purpose of this device is to monitor plants and
when they might need water. Particularly useful for people who forget to water their
plants.

Hardware
--------
The hardware is based on Adafruit Feather HUZZAH ESP8266, SparkFun soil moisture
sensor and a 2000mAh LiPo battery. The soil moisture sensor is read through a voltage
divider network which scales 3.3v to 1v. The output is connected to the ADC pin of the
ESP8266 module. There are two click buttons available on the system. One on the main
ESP8266 module and the other on the board. The button on the module is used to reset the
module and the one on the board is used to switch the system into configuration mode.

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/img/hardware.jpg" width="350" height="450" />
</p>

LED Status
----------

There are two LEDs, one blue and one red on the system. If any error situation occurs during the operation then the red LED wil turn on. The blue LED is turned on when the device is in configuration mode. In normal operation mode the blue LED blinks every minute which indicates that the device is operating properly.

Compiling and Flashing the Firmware
-----------------------------------
Before compiling the firmware the build environment must be prepared,

    cd firmware/
    make prepare

Then patches must be applied,

    cd firmware/
    patch -b -N -d libesphttpd/ -p1 < patch/libesphttpd-httpd.c.patch
    patch -b -N -d libesphttpd/ -p1 < patch/libesphttpd-Makefile.patch

Then to compile the firmware,

    make clean
    make all

The compiled firmware can be flashed using,

    make ESPPORT=/dev/<SERIAL_USB_PORT> flash

Note that for this the Python package **esptool** must be installed on the system.

Configuring the System
----------------------

To switch the device into configuration mode,

* Whe nthe system is powered on press both of the click buttons
* Release the reset button which is on the module
* Wait a second after releasing the reset button and then release the button on the board

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/img/configuration_interface.png" width="" height="" />
</p>
