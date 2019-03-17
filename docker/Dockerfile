FROM ubuntu:18.04

# Me!
MAINTAINER Ishraq Ibne Ashraf 'ishraq.i.ashraf@gmail.com'

# Prepare.
USER root
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies.
RUN apt update -y
RUN apt upgrade -y
RUN apt install \
    git \
    wget \
    gcc \
    g++ \
    sed \
    tar \
    make \
    flex \
    gawk \
    gperf \
    bison \
    unzip \
    bzip2 \
    python \
    libtool \
    texinfo \
    help2man \
    automake \
    autoconf \
    unrar-free \
    python-dev \
    python-pip \
    ncurses-dev \
    libtool-bin \
    libexpat-dev \
    python-serial \
    yui-compressor \
    build-essential \
    python-setuptools -y
RUN pip install esptool

# Add normal user.
RUN adduser --disabled-password --gecos '' thirst

# So the user can access serial ports.
RUN usermod -aG dialout thirst

# Build ESP8266 toolchain.
USER thirst

WORKDIR /home/thirst/
RUN git clone \
    --recursive https://github.com/pfalcon/esp-open-sdk.git
WORKDIR /home/thirst/esp-open-sdk/
RUN make STANDALONE=n

ENV PATH "/home/thirst/esp-open-sdk/xtensa-lx106-elf/bin/:${PATH}"

# Set default working directory of the image.
WORKDIR /home/thirst/
