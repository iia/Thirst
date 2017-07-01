Thirst
======

Thirst is a system that can monitor soil moisture and send notifications via
email in case the moisture is above or below a pre-configured threshold. The
main purpose of this system is to monitor plants and when they might need water.
Particularly useful for people who forget to water their plants. This system will
check the soil moisture once every day and notify the user is needed.

Hardware
--------

The hardware is based on `Seeed Studio Wio Node
<https://www.seeedstudio.com/Wio-Node-p-2637.html>`_, `Grove Moisture Sensor
<https://www.seeedstudio.com/Grove---Moisture-Sensor-p-955.html>`_ and a LiPo
battery with JST connector.

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/img/hardware.jpg" width="350" height="450" />
</p>

LEDs and Buttons
----------------

There are three LEDs, red, green and blue on the system and two buttons, reset
and function.

The red LED turns on when reset button is pressed to indicate reset held state.
The green LED indicate charging states. The blue LED indicate operating modes.

Button Functions
----------------

The following table describes the functionalities of the buttons.

| Reset | Function      | State              |
| ----- | ------------- | ------------------ |
| Click | -             | Reset              |
| Click | Pressed       | Flashing Mode      |
| Click | Delayed Press | Configuration Mode |

To enter configuration mode reset the device by clicking the reset button and
just after resetting press and hold the function button until the blue LED turns
on.

LED Indicators
--------------

Red:
    * Turned on, reset button is pressed and in reset held state.

Green:
    * Blinking, there is no battery present but the system is powered by the
      USB port.
    * Turned on, battery is being charged through the power provided
      by the USB port. In this state the system uses power from the USB port.
    * Turned off, battery is fully charged or operating from battery power
      and no USB power present.

Blue:
    * Blinking, in error state.
    * Turned on, in configuration mode.

Compiling and Flashing the Firmware
-----------------------------------
Before compiling the firmware the build environment must be prepared,

    make -C firmware/ prepare

Then patches must be applied,

    cd firmware/
    patch -b -N -d libesphttpd/ -p1 < patch/libesphttpd-httpd.c.patch
    patch -b -N -d libesphttpd/ -p1 < patch/libesphttpd-Makefile.patch

Then to compile the firmware,

    make -C firmware/ clean
    make -C firmware/ all

The compiled firmware can be flashed using,

    make -C firmware/ ESPPORT=/dev/<SERIAL_USB_PORT> flash

Note that for this the Python package **esptool** must be installed on the system.

Configuring the System
----------------------

To switch the device into configuration mode follow the instructions described
in the Button Functions section.

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/img/configuration_interface.png" width="" height="" />
</p>
