FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        build-essential \
        clang \
        clang-tools \
        gcc-9 \
        gcc-8 \
        libpcre3-dev \
        libssl-dev \
        libgd-dev \
        valgrind \
        gcovr \
        nodejs \
        sudo \
        curl \
        git
