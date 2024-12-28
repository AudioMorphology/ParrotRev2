/******************************************************************************

parrot

Copyright © 2024 Richard R. Goodwin / Audio Morphology Ltd.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the “Software”), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

******************************************************************************/
/**
 * @file parrot.h
 * 
 * Header file - constant definitions, function prototypes etc
 */
#ifndef PARROT_H
#define PARROT_H

/* REV 1 pinouts
static const uint I2S_DOUT = 6;                 // I2S Data Out
static const uint I2S_DIN = 7;                  // I2S Data In
static const uint I2S_BCK = 8;                  // I2S Bit Clock
static const uint I2S_LRCK = 9;                 // I2S Left-Right Clock
static const uint I2S_SCK = 10;                 // I2S System Clock
*/
static const uint XSMT_PIN = 12;                // PCM5102 Soft-mute     Physical Pin 16 
static const uint POWERSAVE_PIN = 23;           // Set high turns on PWM mode the internal 3v3 regulator, which reduces noise 

/* REV 2 I2S Pinouts */
static const uint I2S_DOUT = 7;                 // I2S Data Out
static const uint I2S_DIN = 8;                  // I2S Data In
static const uint I2S_BCK = 9;                  // I2S Bit Clock
static const uint I2S_LRCK = 10;                 // I2S Left-Right Clock
static const uint I2S_SCK = 11;                 // I2S System Clock

#endif