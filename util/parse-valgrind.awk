#!/bin/awk -f

BEGIN {
    sep
    while (getline) {
        if ($0 ~ /^==[[:digit:]]+==\s*$/) {
            sep = "\n\n\n"
        } else if ($0 ~ /^==[[:digit:]]+==/) {
            printf "%s%s\n", sep, $0
            sep = ""
        }
    }
}
