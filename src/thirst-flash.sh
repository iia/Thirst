#!/usr/bin/env bash

if [ $# -ne 2 ]
then
    echo "Usage: $0 <SERIAL_INTERFACE> <FIRMWARE_ZBIN>"
    exit 1
fi

# Extract firmware archive.
unzip $2 -d /tmp/thirst/

# Flash.
esptool.py \
    --port $1 \
    --baud 2000000 \
    write_flash \
    --flash_mode qio \
    --flash_size 32m-c1 \
    --flash_freq 80m \
    0x000000 /tmp/thirst/boot_v1.7.bin \
    0x001000 /tmp/thirst/user1.4096.new.6.bin \
    0x3FC000 /tmp/thirst/esp_init_data_default.bin \
    0x3FE000 /tmp/thirst/blank.bin
