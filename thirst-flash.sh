#!/usr/bin/env bash

if [ $# -ne 2 ]
then
    echo "Usage: $0 <SERIAL_INTERFACE> <FIRMWARE_ZBIN>"
    exit 1
fi

# Need to run the container with privilege to access the serial port.
docker \
    run \
    --privileged \
    -it \
    -v $(pwd)/firmware:/home/thirst/firmware \
    -u thirst \
    -w /home/thirst/firmware/ \
    iia86/env_build_thirst:v1 \
    ./thirst-flash.sh $1 $2
