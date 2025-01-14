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

// GPIO Pin definitions
static const uint ALGORITHM_0 = 0;              // LSB of Algorithm 8-Way BCD Switch    Physical Pin 1
static const uint ALGORITHM_1 = 1;              // MSB of Algorithm 8-Way BCD Switch    Physical Pin 2
static const uint ALGORITHM_2 = 2;              // MSB of Algorithm 8-Way BCD Switch    Physical Pin 4
static const uint DIVISOR_0 = 3;                // 8-Way BCD switch-A                   Physical Pin 5
static const uint DIVISOR_1 = 4;                // 8-Way BCD switch-B                   Physical Pin 6
static const uint DIVISOR_2 = 5;                // 8-Way BCD switch-C                   Physical Pin 7
static const uint DIVISOR_3 = 6;                // Mul/Div switch provides 4th Bit      Physical Pin 9
static const uint I2S_DOUT = 7;                 // I2S Data Out                         Physical Pin 10
static const uint I2S_DIN = 8;                  // I2S Data In                          Physical Pin 11
static const uint I2S_BCK = 9;                  // I2S Bit Clock                        Physical Pin 12
static const uint I2S_LRCK = 10;                // I2S Left-Right Clock                 Physical Pin 14
static const uint I2S_SCK = 11;                 // I2S System Clock                     Physical Pin 15
static const uint XSMT_PIN = 12;                // PCM5102 Soft-mute                    Physical Pin 16 
static const uint ENCODER_SW = 13;              // Rotary Encoder switch                Physical pin 17
static const uint ENCODERA_IN = 14;             // Rotary encoder A-leg                 physical pin 19 
static const uint ENCODERB_IN = 15;             // Rotary encoder B-leg                 physical pin 20
static const uint SPI_CS=16;                    // Set in CMakeLists as PSRAM_PIN_CS    Physical Pin 21 
static const uint SPI_SCK=17;                   // Set in CMakeLists as PSRAM_PIN_SCK   Physical Pin 22
static const uint SPI_MOSI=18;                  // Set in CMakeLists as PSRAM_PIN_MOSI  Physical Pin 24
static const uint SPI_MISO=19;                  // Set in CMakeLists as PSRAM_PIN_MISO  Physical Pin 25
static const uint SYNC_FREE = 20;               // Delay sync'd to clock                Physical pin 26
static const uint CLOCK_OUT = 21;               // Internal clock out                   Physical pin 27
static const uint CLOCK_IN = 22;                // External clock in                    Physical pin 29
static const uint POWERSAVE_PIN = 23;           // Set high turns on PWM mode the internal 3v3 regulator, which reduces noise 
static const uint VBUS_SENSE = 24;              // High if VBUS is present, low otherwise
static const uint ONBOARD_LED = 25;             // On-Board LED
static const uint FEEDBACK_PIN = 26;            // feedback amount                      Physical pin 31 (ADC0 Input)
static const uint CLOCKSPEED_PIN = 27;          // Clock Speed                          Physical pin 32 (ADC1 Input)
static const uint WETDRY_PIN = 28;              // Wet/Dry control                      Physical pin 34 (ADC2 Input)
// ADC Channels
static const uint ADC_feedback = 0;
static const uint ADC_Clock = 1;
static const uint ADC_WetDry = 2; 

// AP 6404L Constants
static const uint8_t AP6404_READ = 0x03;        // Read
static const uint8_t AP6404_FREAD = 0x0B;       // Fast Read
static const uint8_t AP6404_WRITE = 0x02;       // Write
static const uint8_t AP6404_RST_EN = 0x66;      // Reset Enable
static const uint8_t AP6404_RESET = 0x99;       // Reset
static const uint8_t AP6404_WBT = 0xC0;         // Wrap Boundary Toggle  
static const uint8_t AP6404_READID = 0x9F;      // Read ID

// Global Variables defined in parrot_main.c
extern double ClockBPM;            // BPM Value for interal clock
extern double ClockFreq;           // Internal Clock Frequency
extern double ClockPeriod;         // Internal Clock Period
extern uint32_t ReadPointer; 
extern uint32_t WritePointer;
extern float glbFeedback;

// function prototypes - parrot_func.c

#endif