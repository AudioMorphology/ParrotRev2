# Parrot

##  Overview

Parrot is a 12hp Eurorack digital delay powered by a Raspberry Pi Pico microcontroller.

The objective is to provide a CD-Quality audio delay  / reverb effect, with a reasonably 
low BOM count

It utilises an 8Mb x 8-Bit (64mBit) PSRAM Memory chip (the AP6404L), and the whole of
this is treated as a 2Mb x 32-Bit audio buffer. This means that we can wrap the read and 
write pointers  with a simple logical &. 

2Mb x 32-Bit is 1Mb at 32-Bit Stereo. At a 48kHz sample rate, this gives a maximum delay 
time of 1,048,576 / 48,000 = 21.84 seconds 

The ADC and DAC ICs communicate over the i2S protocol, which is handled by custom PIO
functions.

Reading and writing to the PSRAM buffer is double-buffered using DMA calls from the I2S
interface.

It uses two PIO State machines:

PIO0 for SPI
PIO1 for I2S

The ADC and DAC utilises the low-cost and readily-available I2S modules, which have the 
advantages of already including various ancilliary components, which further reduces the
component count and complexity.

## NOTICE

This code incorporates and acknowledges the following:

### rp2040-psram Copyright © 2023 Ian Scott

rp2040-psram is a header-only library, which implements the SPI protocol using the rp2040 PIO
rather than the in-built SPI interface. This gives far better performance for PSRAM SPI peripherals

### i2s Copyright (c) 2022 Daniel Collins

These functions implement the I2S protocol using the rp2040 PIO. and utilise interrupt-driven
DMA double-buffering for Audio I/O.

### Elements of hardware design Copyright 2012 Emilie Gillet (emilie.o.gillet@gmail.com)

The input and output buffer circuitry was taken directly from Clouds:
https://github.com/pichenettes/eurorack/tree/master/clouds/hardware_design/pcb 



