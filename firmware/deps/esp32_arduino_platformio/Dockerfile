FROM ubuntu:latest

# switch to root, let the entrypoint drop back to host user
USER root
SHELL ["/bin/bash", "-c"]

ARG DEBIAN_FRONTEND=noninteractive

RUN : \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
    bison \
    ccache \
    cmake \
    curl \
    flex \
    git \
    gperf \
    jq \
    libncurses-dev \
    libssl-dev \
    libusb-1.0 \
    ninja-build \
    patch \
    python3 \
    python3-click \
    python3-cryptography \
    python3-future \
    python3-pip \
    python3-pyelftools \
    python3-pyparsing \
    python3-serial \
    python3-setuptools \
    python3-venv \
    wget \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/* \
    && :

# checkout esp32-arduino-lib-builder version 4.4 to maintain compatibility with PlatformIO, which does not support newer arduino-esp32 versions.
ARG LIBBUILDER_CLONE_URL=https://github.com/espressif/esp32-arduino-lib-builder
ARG LIBBUILDER_CLONE_BRANCH_OR_TAG=release/v4.4

ENV LIBBUILDER_PATH=/opt/esp/lib-builder
# Ccache is installed, enable it by default
ENV IDF_CCACHE_ENABLE=1

RUN echo LIBBUILDER_CLONE_BRANCH_OR_TAG=$LIBBUILDER_CLONE_BRANCH_OR_TAG && \
    git clone --recursive -b $LIBBUILDER_CLONE_BRANCH_OR_TAG \
    $LIBBUILDER_CLONE_URL $LIBBUILDER_PATH && \
    git config --system --add safe.directory $LIBBUILDER_PATH

COPY guest-build.sh $LIBBUILDER_PATH/guest-build.sh
COPY defconfig.rtc_ext_cry  $LIBBUILDER_PATH/configs/defconfig.rtc_ext_cry
COPY fix-esp-littlefs-version.patch  $LIBBUILDER_PATH/fix-esp-littlefs-version.patch

WORKDIR /opt/esp/lib-builder