![Thirst Banner](.readme-resources/thirst.png)

Thirst
======

Thirst is a system that can monitor soil moisture and send notifications via
email in case the moisture is above or below a pre-configured threshold. The
main purpose of this system is to monitor plants and when they might need water.
Particularly useful for people who forget to water their plants. This system will
check the soil moisture once every day and notify the user is needed.

Hardware
--------

The hardware is based on,

 * [Seeed Studio Wio Node](https://www.seeedstudio.com/Wio-Node-p-2637.html)
 * [Grove Moisture Sensor](https://www.seeedstudio.com/Grove---Moisture-Sensor-p-955.html)
 * LiPo battery with JST connector

The moisture sensor must be connected on PORT1 of the Wio Node.

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware.jpg" width="350" height="450" />
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

| Reset     | Function                                        | State              |
| --------- | ----------------------------------------------- | ------------------ |
| Click     | -                                               | Reset              |
| Click     | Pressed                                         | Flashing Mode      |
| Click x 2 | Delayed Press and hold until blue LED lights up | Configuration Mode |

To enter configuration mode reset the device by clicking the reset button and
just after resetting press and hold the function button until the blue LED turns
on.

LED Indicators
--------------

* Red
    - Turned on, reset button is pressed and in reset held state.

* Green
    - Blinking, there is no battery present but the system is powered by the
      USB port.
    - Turned on, battery is being charged through the power provided
      by the USB port. In this state the system uses power from the USB port.
    - Turned off, battery is fully charged or operating from battery power
      and no USB power present.

* Blue
    - Blinking, in error state.
    - Turned on, in configuration mode.

Compiling and Flashing the Firmware
-----------------------------------
Firmware compilation has package dependencies which can be satisfied on a
Debian or Debian based system with the following command,

    sudo apt install yui-compressor python-serial python-pip python-setuptools -y

Before compiling the firmware the build environment must be prepared.

First download the latest released Espressif Non-OS SDK for ESP8266
from [here](https://github.com/espressif/ESP8266_NONOS_SDK/releases) and extract
the archive.

On the root directory of the extracted SDK archive clone this repository and
(the libesphttpd repository)[https://github.com/Spritetm/libesphttpd].

Patch libesphttpd. From the root directory of the extracted SDK,

    cd thirst/
    patch -b -N -d ../libesphttpd/ -p1 < patch/libesphttpd-Makefile.patch

Compile the firmware. From the root directory of the extracted SDK,

    make -C thirst/ clean
    make -C thirst/ COMPILE=gcc BOOT=new APP=1 SPI_SPEED=80 SPI_MODE=QIO SPI_SIZE_MAP=6

Before flashing the firmware make sure **esptool** is installed,

    sudo pip install esptool

After esptool is installed make sure the device has it's serial port connected
and is in flashing mode. Then the device can be flashed. From the root directory
of the extracted SDK,

    esptool.py \
    --port <SERIAL_USB_PORT> \
    --baud 2000000 \
    write_flash \
    --flash_mode qio \
    --flash_size 4MB \
    --flash_freq 80m \
    0x000000 bin/boot_v1.7.bin \
    0x001000 bin/upgrade/user1.4096.new.6.bin \
    0x3FC000 bin/esp_init_data_default.bin \
    0x3FE000 bin/blank.bin

Where <SERIAL_USB_PORT> is the serial port which connects to the device.

Notifier Service
----------------

For sending the email notifications this project uses a microservice.
For details check [here](https://github.com/iia/notifier).

Configuring the System
----------------------

To switch the device into configuration mode follow the instructions described
in the [Button Functions](https://github.com/iia/thirst/blob/master/README.md#button-functions)
section. In configuration mode the system presents a WiFi access point. Initially
the name of the access point would be of form, **Thirst-XXX** where each **X** is
a 1-byte hexadecimal number and the password will be **1234567890**.

After connecting to this access point the configuration interface will be
available on IP address: **192.168.7.1** which can be used with a web browser.

<p align="center">
    <img align="center" src="https://github.com/iia/Thirst/blob/master/.readme-resources/configuration_interface.png" width="" height="" />
</p>
