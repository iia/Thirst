#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 \"<SG_API_KEY>\""
    exit 1
fi

docker \
    run \
    -it \
    -v $(pwd)/src:/home/thirst/src \
    -u thirst \
    -w /home/thirst/src/ \
    iia86/env_build_thirst:v1 \
    ./thirst-build.sh "$1"

