#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 <SERIAL_INTERFACE>"
    exit 1
fi

# Build.
./thirst-build.sh

# Flash.
esptool.py \
    --port $1 \
    --baud 2000000 \
    write_flash \
    --flash_mode qio \
    --flash_size 32m-c1 \
    --flash_freq 80m \
    0x000000 bin/boot_v1.7.bin \
    0x001000 bin/upgrade/user1.4096.new.6.bin \
    0x3FC000 bin/esp_init_data_default.bin \
    0x3FE000 bin/blank.bin

# Clean.
make -C libesphttpd/ clean
make -C thirst/ clean
make clean
