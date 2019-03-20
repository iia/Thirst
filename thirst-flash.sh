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
    -v $(pwd)/src:/home/thirst/src \
    -u thirst \
    -w /home/thirst/src/ \
    iia86/env_build_thirst:v1 \
    ./thirst-flash.sh $1 $2

