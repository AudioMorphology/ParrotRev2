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
#include "psram_spi.h"

//#define ALARM_NUM 1
//ALARM_IRQ TIMER_IRQ_1
#define ALARM_NUM 0
#define ALARM_IRQ timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM)



/**
 * @brief Various fixed point conversion, multiplication and division macros
 * 
 * These came from here:
 * https://people.ece.cornell.edu/land/courses/ece4760/RP2040/C_SDK_fixed_pt/speed_test/Fixed_point_test.c
 */
typedef signed int s15x16;
#define muls15x16(a,b) ((s15x16)(((( signed long long )(a))*(( signed long long )(b)))>>16)) //multiply two fixed 16:16
#define float_to_s15x16(a) ((s15x16)((a)*65536.0)) // 2^16
#define s15x16_to_float(a) ((float)(a)/65536.0)
#define s15x16_to_int(a)    ((int)((a)>>16))
#define int_to_s15x16(a)    ((s15x16)((a)<<16))
#define divs15x16(a,b) ((s15x16)((((signed long long)(a)<<16)/(b)))) 
#define abss15x16(a) abs(a)
// and for 1.14
typedef signed short s1x14 ;
#define muls1x14(a,b) ((s1x14)((((int)(a))*((int)(b)))>>14)) 
#define float_to_s1x14(a) ((s1x14)((a)*16384.0)) // 2^14
#define s1x14_to_float(a) ((float)(a)/16384.0)
#define abss1x14(a) abs(a) 
#define divs1x14(a,b) ((s1x14)((((signed int)(a)<<14)/(b)))) 
// Int to float
#define float_to_int(x) (((int) ((x * 8388607) + 8388608.5f)) - 8388608)
#define float_to_int32(x) (((int) ((x * 2147483647) + 2147483648.5f)) - 2147483648)
#define int_to_float(x) ((((float) (x + 8388608)) - 8388608.5f) / 8388607.f)
#define int32_to_float(x) ((((float) (x + 2147483648)) - 2147483648.5f) / 2147483648.f)
/**
 * @brief union of a float and a 32-Bit integer
 * 
 * The elements of the Union can be accessed separately even 
 * though they are the _same_ 4 x 8-bit bytes. This enables a
 * Float to be passed about for processing, but stored and 
 * read from RSRAM as if it were a 32-Bit word
 */
union uSample {
    float fSample;
    int32_t iSample;
};

// Global Variables - Constants
static const double ClockScale = 0.04884;       // (= (240-40)/4095) scales the internal clock BPM from 40 to 240BPM
static const uint Clock_MA_Len = 3;             // Length of the Clock Pot Moving Average buffer
static const uint WetDry_MA_Len = 5;            // Length of the Wet/Dry pot MA buffer
static const uint ExtClock_MA_Len = 10;         // Length of the ExtClock Moving Average buffer (to filter out timing irregularities)
static const uint32_t ClockHysteresis = 0xFF;   // +/- Amount the clock has to vary before we note a new clock period (to overcome jitter)
static const float Feedback_scale = 0.0002442;  // (= 1/4095 )fixed scale factor to scale the Feedback ADC value to give a Percentage feedback from 0 to 1
static const float WetDry_Scale = 0.02442;      // (= 100/4095)fixed scale factor to scale the Divisor ADC value to give a value from 0 to 100
static const float SampleLength = 1000000.0f/96000.0f; // Length of 1 stereo sample in uS (10.4166ms).  
static const uint Feedback_MA_Len = 3;          // Length of the Feedback Moving Average ring buffer
static const uint Tick_MA_Len = 1;              // Length of the Rotary Encoder tick speed Moving Average ring buffer
static const uint32_t BUF_LEN = 0x7FFFFC;       // Actual Audio Buffer length in Mb = 8Mb. 
// GPIO Pin definitions
static const uint ALGORITHM_0 = 0;              // LSB of Algorithm 4-Way BCD Switch Physical Pin 1
static const uint ALGORITHM_1 = 1;              // MSB of Algorithm 4-Way BCD Switch Physical Pin 2
static const uint DIVISOR_A = 2;                // 16-Way BCD switch-A    Physical Pin 4
static const uint DIVISOR_B = 3;                // 16-Way BCD switch-B    Physical Pin 5
static const uint DIVISOR_C = 4;                // 16-Way BCD switch-C    Physical Pin 6
static const uint DIVISOR_D = 5;                // 16-Way BCD switch-D    Physical Pin 7
static const uint I2S_DOUT = 6;                 // I2S Data Out
static const uint I2S_DIN = 7;                  // I2S Data In
static const uint I2S_BCK = 8;                  // I2S Bit Clock
static const uint I2S_LRCK = 9;                 // I2S Left-Right Clock
static const uint I2S_SCK = 10;                 // I2S System Clock
static const uint LED_PIN = 11;                 // LED                   physical pin 15
static const uint XSMT_PIN = 12;                // PCM5102 Soft-mute     Physical Pin 16 
static const uint ENCODER_SW = 13;              // Rotary Encoder switch Physical pin 17
static const uint ENCODERA_IN = 14;             // Rotary encoder A-leg  physical pin 20 
static const uint ENCODERB_IN = 15;             // Rotary encoder B-leg  physical pin 21
static const uint SPI_CS = 16;                  // SPI Chip Select
static const uint SPI_SCK = 17;                 // SPI Serial Clock
static const uint SPI_MOSI = 19;                // SPI Master Out Slave In
static const uint SPI_MISO = 20;                // SPI Master In Slave Out
static const uint SYNC_PIN = 20;                // Delay sync'd to clock physical pin 26
static const uint CLOCK_OUT = 21;               // Internal clock out    physical pin 27
static const uint CLOCK_IN = 22;                // External clock in     physical pin 29
static const uint POWERSAVE_PIN = 23;           // Set high turns on PWM mode the internal 3v3 regulator, which reduces noise 
static const uint VBUS_SENSE = 24;              // High if VBUS is present, low otherwise
static const uint ONBOARD_LED = 25;             // On-Board LED
static const uint FEEDBACK_PIN = 26;            // feedback amount       physical pin 31 (ADC0 Input)
static const uint CLOCKSPEED_PIN = 27;          // Clock Speed           physical pin 32 (ADC1 Input)
static const uint WETDRY_PIN = 28;              // Wet/Dry control       physical pin 34 (ADC2 Input)
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
extern float glbDivisor;
extern float divisors[];
extern int Algorithm;

//  Global variables defined in parrot_core1.c
extern _Atomic int32_t ExtClockPeriod;     // External Clock Period (rising edge to rising edge)
extern _Atomic int32_t ExtClockTick;       // Records the last tick for the External Clock
extern uint32_t glbDelay;
extern uint32_t targetDelay;
extern int32_t PreviousClockPeriod;
extern uint32_t glbIncrement;
extern int32_t DelayDiff;
extern float glbWet;
extern float glbDry;
extern int spinlock_num_glbDelay;
extern spin_lock_t *spinlock_glbDelay;
extern int ClockPhase;             // Toggles between 0 & 1
extern int LEDPhase;
extern float FeedbackPercent;

// function prototypes - parrot_core1.c
void core1_entry(void);

// function prototypes - parrot_func.c
extern psram_spi_inst_t psram_spi;
float single_delay(union uSample);
float single_tap(union uSample, float);
float Ping_Pong(union uSample, float, bool);
int32_t single_tap_shift(int32_t, uint32_t, uint8_t);
/**
 * @brief Wet/Dry mix
 * 
 * Uses the global Wet & Dry levels to retun a mix of the 
 * passed wet and dry samples
 * 
 * @param DrySample
 * @param WetSample
 */
__force_inline static float WetDry(float DrySample, float WetSample){
    return (DrySample * glbDry) + (WetSample * glbWet); 
}

float rational_tanh(float);
float soft_clip(float);

#endif