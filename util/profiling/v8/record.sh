#!/bin/bash

set -e

# https://v8.dev/docs/linux-perf
perf record --clockid=mono \
    --output=perf.data \
    ./work/buildroot/nginx
