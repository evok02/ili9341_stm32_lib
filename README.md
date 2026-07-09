Bare-metal firmware for STM32F103C8T6 ("Blue Pill") with SPI TFT LCD and SD card FAT32 filesystem.

## Hardware
- MCU: STM32F103C8T6 (72 MHz Cortex-M3, 64K flash, 20K RAM)
- Display: ILI9341 320x240 TFT LCD
- Storage: SD/MMC card (SPI mode)

### Pinout
| Pin   | Function       |
|-------|----------------|
| PA1   | SD card CS     |
| PA2   | TFT DC         |
| PA3   | TFT RST        |
| PA4   | TFT CS         |
| PA5   | SPI1 SCK       |
| PA6   | SPI1 MISO      |
| PA7   | SPI1 MOSI      |
| PB0   | Backlight PWM  |
| PB10  | UART3 TX       |
| PB11  | UART3 RX       |
| PC13  | LED            |

## Dependencies
- `arm-none-eabi-gcc` (gcc-arm-embedded)
- `libopencm3` (included as git submodule)
- `st-flash` (for flashing) or `OpenOCD` + `arm-none-eabi-gdb`

## Build & Flash
git clone --recursive https://github.com/<user>/<repo>
cd <repo>
make -C libopencm3
make
make flash

## Project Structure
├── inc/         Headers (tft.h, fat32.h, mmc.h, spi.h, ...)
├── src/         Sources (tft.c, fat32.c, mmc.c, spi.c, ...)
├── libopencm3/  Hardware abstraction layer (submodule)
├── Makefile     Build system
├── linkerscript.ld
└── debug.sh     GDB + OpenOCD script

## Features
- SPI driver with DMA (TX/RX, interrupts, debug GPIO)
- ILI9341 LCD driver (init, fill, pixel, rectangle, 5x8 text)
- SD/MMC card driver (SPI mode, init, single/multi-block read/write)
- **FAT32 filesystem** — mount, open, close, read, write, seek, sync
- UART debug console (115200 baud) via tiny printf
- SysTick-based timing, PWM backlight control

## Current Demo
Mounts FAT32 at sector 64, opens `/lowbytes/books/text.txt`, writes 16KB of data in 4KB chunks, then reads it back.

## Credits
- [libopencm3](https://github.com/libopencm3/libopencm3) — ARM Cortex MCU library
- [printf](https://github.com/mpaland/printf) — Marco Paland's embedded printf
