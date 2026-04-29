#! /usr/bin/sh

arm-none-eabi-gdb -ex 'target remote :3333' \
    -ex 'monitor reset halt' \
    -ex 'load' \
    -ex 'break main' \
    -ex 'c' \
    ./tft_spi.elf
