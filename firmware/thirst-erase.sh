#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 <SERIAL_INTERFACE>"
    exit 1
fi

# Erase flash.
esptool.py --port $1 erase_flash

