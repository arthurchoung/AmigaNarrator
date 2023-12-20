#!/bin/bash

if [ "x$1" == "x" ]; then
    echo "Usage: $0 <text>"
    exit 1
fi
TEXT="$1"

./translator "$TEXT" 2>/dev/null | ./narrator - 2>/dev/null | aplay -f S8 -r 22200

