#!/bin/bash

set -e

perf report --input=perf.data.jitted
