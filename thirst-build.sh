#!/usr/bin/env bash

docker \
    run \
    -it \
    -v $(pwd)/src:/home/thirst/src \
    -u thirst \
    -w /home/thirst/src/ \
    iia86/env_build_thirst:v1 \
    ./thirst-build.sh

