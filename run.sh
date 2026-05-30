#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: ./run.sh <file.spp>"
    exit 1
fi

INPUT="$1"
BASE="${INPUT%.spp}"

./spp "$INPUT" && \
nasm -f elf64 "$BASE.asm" -o "out.o" && \
gcc "out.o" -o "out" -no-pie && \
./out

rm -f "$BASE.asm" out.o out