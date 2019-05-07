#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 \"<SG_API_KEY>\""
    exit 1
fi

docker \
    run \
    -it \
    -v $(pwd)/firmware:/home/thirst/firmware \
    -u thirst \
    -w /home/thirst/firmware/ \
    iia86/env_build_thirst:v1 \
    ./thirst-build.sh "$1"
