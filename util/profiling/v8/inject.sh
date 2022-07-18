#!/bin/bash

set -e

perf inject --jit --input=perf.data --output=perf.data.jitted
