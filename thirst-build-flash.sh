#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 <SERIAL_INTERFACE>"
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
    ./thirst-build-flash.sh $1
