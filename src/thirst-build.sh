#!/usr/bin/env bash

if [ $# -ne 1 ]
then
    echo "Usage: $0 \"<SG_API_KEY>\""
    exit 1
fi

# Clean binary output directory.
rm -vrf ./bin/upgrade/

# Clean and build libesphttpd.
make -C libesphttpd/ clean
make \
    -C libesphttpd/ \
    TOOLPREFIX=xtensa-lx106-elf- \
    USE_OPENSDK=yes \
    SDK_BASE=../ \
    HTTPD_MAX_CONNECTIONS=8 \
    GZIP_COMPRESSION=no \
    COMPRESS_W_YUI=yes \
    YUI-COMPRESSOR=/usr/bin/yui-compressor \
    USE_HEATSHRINK=yes \
    HTTPD_WEBSOCKETS=no \
    HTMLDIR=../thirst/user/html

# Clean and build Thirst.
make -C thirst/ clean
make \
    -C thirst/ \
    SG_API_KEY="$1" \
    COMPILE=gcc \
    BOOT=new \
    APP=1 \
    SPI_SPEED=80 \
    SPI_MODE=QIO \
    SPI_SIZE_MAP=6

RET_MAKE=$?

# Clean.
make -C libesphttpd/ clean
make -C thirst/ clean
make clean

if [ $RET_MAKE -ne 0 ]
then
    exit $RET_MAKE
else
    exit 0
fi

