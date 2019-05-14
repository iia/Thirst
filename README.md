<p align="center">
    <img
        width="225px"
        style="text-align: center;"
        src="https://github.com/iia/thirst/blob/master/.readme-resources/thirst.png" />
</p>

# Thirst

Thirst is a system that can monitor soil moisture and send notifications via
E-mail in case the moisture is above or below a pre-configured threshold. The
main purpose of this system is to monitor plants and when they might need water.
Particularly useful for people who forget to water their plants. This system will
check the soil moisture once every day and notify the user if needed.

## Hardware

The hardware is based on,

 * [Seeed Studio Wio Node](https://www.seeedstudio.com/Wio-Node-p-2637.html).
 * [Grove Moisture Sensor](https://www.seeedstudio.com/Grove---Moisture-Sensor-p-955.html).
 * LiPo battery with JST connector/Powerbank etc.

The moisture sensor must be connected to PORT1 of the Wio Node.

<p align="center">
    <img
        width="350px"
        height="450px"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware_front.png" />
</p>
<p align="center">
    <img
        width="350px"
        height="450px"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware_back.png" />
</p>

## LEDs and Buttons

There are three LEDs, red, green and blue on the system and two buttons, reset
and function.

The red LED turns on when reset button is pressed to indicate reset held state.
The green LED indicate charging states. The blue LED indicate operating modes.

## Button Functions

The following table describes the functionalities of the buttons.

| Reset     | Function                                            | State              |
| --------- | --------------------------------------------------- | ------------------ |
| Click     | -                                                   | Reset              |
| Click     | Pressed                                             | Flashing Mode      |
| Click x 2 | Delayed press and hold until the blue LED lights up | Configuration Mode |

:information_source: **_NOTE: The RESET and the FUNCTION buttons are marked GREEN and RED respectively in the image._**

<p align="center">
    <img
        width="428px"
        height="434px"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware_wio_node_buttons.png" />
</p>

## LED Indicators

* Red
    - Turned on, reset button is pressed and is in reset held state.

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

:information_source: **_NOTE: The RED, GREEN and BLUE LEDs are marked RED, GREEN and BLUE respectively in the image._**


<p align="center">
    <img
        width="428px"
        height="434px"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware_wio_node_leds.png" />
</p>

## The Firmware

The firmware has the following dependencies:

 * [ESP8266 Non-OS SDK v2.1.0](https://github.com/espressif/ESP8266_NONOS_SDK/).
 * [libesphttpd](https://github.com/Spritetm/libesphttpd) (commit: b7bb4a625bf4a2c7e7eb699caa244ca7989fd079).
 * [heatshrink v0.4.1](https://github.com/atomicobject/heatshrink).
 * [SendGrid API v3](https://sendgrid.com/docs/API_Reference/Web_API_v3/index.html).

### SendGrid API v3

The firmware uses [SendGrid](https://sendgrid.com/) API for sending the notification E-mails. Hence if you are
compiling the firmware you have to provide the SendGrid API v3 key.

:information_source: **_NOTE: The released firmware binaries are already compiled with the key._**

### Building and Flashing

:information_source: **_NOTE: Currently building and flashing is only supported for Linux based systems._**

Firmware building and flashing are managed by a Docker image. The image is pulled and used automatically by
the build and flash scripts. These scripts require a working Docker installation.

Check [Docker documentation](https://docs.docker.com/) for how to install and setup Docker.

There are four scripts for managing building and flashing:

 * thirst-build.sh
 * thirst-build-flash.sh
 * thirst-flash.sh
 * thirst-erase.sh

:information_source: **_NOTE: Make sure that the Wio Node is in flashing mode before executing any script that communicates over the serial port. Refer to [this table](#button-functions) for switching the Wio Node to flashing mode._**

### thirst-build.sh

This script is used to simply build the firmware. After this script has finished executing successfully the
firmware binaries will be generated in the *src/bin* directory. The script takes the SendGrid API v3 key as
the only argument.

```console
foo@bar:~/thirst$ ./thirst-build.sh "<GATEWAY_KEY>"
```

### thirst-build-flash.sh

This script is pretty much the same as the *thirst-build.sh* script except that this script will also take care
of flashing the built firmware. This script takes two arguments:
 * SendGrid API v3 key.
 * The serial interface to be used to flash the Wio Node.

```console
foo@bar:~/thirst$ ./thirst-build-flash.sh "<GATEWAY_KEY>" /dev/ttyUSB0
```

Considering */dev/ttyUSB0* is the serial interface to be used for flashing the Wio Node.

### thirst-flash.sh

This one can be used to simply flash the firmware. Particularly useful for flashing released firmware binaries
where no compilation or building is involved. This script must be invoked with two arguments which will specify
the serial interface to be used for flashing and the firmware .zbin file containing the firmware binaries.

:information_source: **_NOTE: The firmware .zbin file must be placed in the *src/* directory for this script to work._**

```console
foo@bar:~/thirst$ ./thirst-flash.sh /dev/ttyUSB0 ./thirst_v1.zbin
```

Considering */dev/ttyUSB0* is the serial interface to be used for flashing the Wio Node and the firmware file *thirst_v1.zbin* is present in *src/thirst_v1.zbin*.

### thirst-erase.sh

This script can be used to erase the entire flash memory of the Wio Node. Can be useful to use before flashing the device.

```console
foo@bar:~/thirst$ ./thirst-erase.sh /dev/ttyUSB0
```

Considering */dev/ttyUSB0* is the serial interface to be used for erasing the Wio Node flash.

### Hardware Connection for Flashing

To flash the Wio Node the serial USART device must be connected as shown below.

:information_source: **_NOTE: The RX, TX and GND of the USART interface must be connected to GPIO1/TxD, GPIO3/RxD and GND pins of PORT0 respectively as marked in the image._**

<p align="center">
    <img
        width="400px"
        height="355px"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/hardware_wio_node_pinout.png" />
</p>

## Configuring the System

To switch the device into configuration mode follow the instructions described
in the [Button Functions](https://github.com/iia/thirst/blob/master/README.md#button-functions)
section. In configuration mode the system presents a WiFi access point. Initially
the name of the access point would be of form, **Thirst-XXX** where each **X** is
a 1-byte hexadecimal number and the password will be **1234567890**.

After connecting to this access point the configuration interface will be
available on IP address: **192.168.7.1** which can be used with a web browser.

<p align="center">
    <img
        width="350px"
        height="450px"
        align="center"
        src="https://github.com/iia/Thirst/blob/master/.readme-resources/configuration_interface.png" />
</p>
