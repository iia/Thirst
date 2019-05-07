#!/usr/bin/env bash

if [ $# -ne 2 ]
then
    echo "Usage: $0 \"<GATEWAY_KEY>\" <SERIAL_INTERFACE>"
    exit 1
fi

# Build and flash.
./thirst-build.sh "$1" && \
esptool.py \
    --port $2 \
    --baud 2000000 \
    write_flash \
    --flash_mode qio \
    --flash_size 32m-c1 \
    --flash_freq 80m \
    0x000000 bin/boot_v1.7.bin \
    0x001000 bin/upgrade/user1.4096.new.6.bin \
    0x3FC000 bin/esp_init_data_default_v08.bin \
    0x3FE000 bin/blank.bin
