/******************************************************************************

The Camberwell Parrot

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
#include "freeverb/freeverb.h"
#include "pverb/pverb.h"
#include "gverb/include/gverb.h"

// External Clock input interrupt
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

// AllPass filter structure
typedef struct {
    float a1; // Coefficient for the filter
    float z1; // Previous input value
} AllPassFilter;

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
//static const uint32_t BUF_LEN = 0x7FFFFC;       // Actual Audio Buffer length in Mb = 8Mb. 
// GPIO Pin definitions
static const uint32_t BUF_LEN = 0x7FFFF;        // PSRAM buffer length in L-R Sample pairs 
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

// Global Variables defined in parrot_main.c
extern float AllPassState;
extern double ClockBPM;            // BPM Value for interal clock
extern double ClockFreq;           // Internal Clock Frequency
extern double ClockPeriod;         // Internal Clock Period
extern uint32_t ReadPointer_L; 
extern uint32_t ReadPointer_R; 
extern uint32_t WritePointer;
extern float glbFeedback;
extern float glbRatio;
extern int glbDivisor;
extern int glbEuclideanFill; 
extern float divisors[];
extern int EuclideanSteps[];
extern int EuclideanHits[];
extern int glbAlgorithm;
extern uint64_t DeBounceTime;
extern ty_gverb * parrot_gverb;
extern fv_Context parrot_freeverb;
extern pv_Context parrot_pverb;

//  Global variables defined in parrot_core1.c
extern _Atomic int32_t ExtClockPeriod;     // External Clock Period (rising edge to rising edge)
extern _Atomic int32_t ExtClockTick;       // Records the last tick for the External Clock
extern uint32_t glbDelay_L;
extern uint32_t glbDelay_R;
extern uint32_t targetDelay_L;
extern uint32_t targetDelay_R;
extern int32_t PreviousClockPeriod;
extern uint32_t glbIncrement;
extern int32_t DelayDiff;
extern float glbWet;
extern float glbDry;
extern int spinlock_num_glbDelay;
extern spin_lock_t *spinlock_glbDelay;
extern int ClockPhase;                      // Toggles between 0 & 1
extern int LEDPhase;                        // On-board LED phase
extern float FeedbackPercent;

// function prototypes - parrot_core1.c
void core1_entry(void);

// function prototypes - parrot_func.c
unsigned int bjorklund(int,int);
unsigned int euclid_bit_pattern(int,int);
int bitRead(unsigned int, unsigned int);
size_t get_free_ram(void);
float WaveFolder(float, float);
float WaveWrapper(float, float);
extern psram_spi_inst_t psram_spi;
float Euclidean_Delay(float);
float single_delay(union uSample, bool);
float single_tap(union uSample, float, bool);
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

#define WORD16_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define WORD16_TO_BINARY(byte)  \
  ((byte) & 0x0008000 ? '1' : '0'), \
  ((byte) & 0x0004000 ? '1' : '0'), \
  ((byte) & 0x0002000 ? '1' : '0'), \
  ((byte) & 0x0001000 ? '1' : '0'), \
  ((byte) & 0x0000800 ? '1' : '0'), \
  ((byte) & 0x0000400 ? '1' : '0'), \
  ((byte) & 0x0000200 ? '1' : '0'), \
  ((byte) & 0x0000100 ? '1' : '0'), \
  ((byte) & 0x0000080 ? '1' : '0'), \
  ((byte) & 0x0000040 ? '1' : '0'), \
  ((byte) & 0x0000020 ? '1' : '0'), \
  ((byte) & 0x0000010 ? '1' : '0'), \
  ((byte) & 0x0000008 ? '1' : '0'), \
  ((byte) & 0x0000004 ? '1' : '0'), \
  ((byte) & 0x0000002 ? '1' : '0'), \
  ((byte) & 0x0000001 ? '1' : '0') 


#define WORD32_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define WORD32_TO_BINARY(byte)  \
  ((byte) & 0x8000000 ? '1' : '0'), \
  ((byte) & 0x4000000 ? '1' : '0'), \
  ((byte) & 0x2000000 ? '1' : '0'), \
  ((byte) & 0x1000000 ? '1' : '0'), \
  ((byte) & 0x0800000 ? '1' : '0'), \
  ((byte) & 0x0400000 ? '1' : '0'), \
  ((byte) & 0x0200000 ? '1' : '0'), \
  ((byte) & 0x0100000 ? '1' : '0'), \
  ((byte) & 0x0800000 ? '1' : '0'), \
  ((byte) & 0x0400000 ? '1' : '0'), \
  ((byte) & 0x0200000 ? '1' : '0'), \
  ((byte) & 0x0100000 ? '1' : '0'), \
  ((byte) & 0x0080000 ? '1' : '0'), \
  ((byte) & 0x0040000 ? '1' : '0'), \
  ((byte) & 0x0020000 ? '1' : '0'), \
  ((byte) & 0x0010000 ? '1' : '0'), \
  ((byte) & 0x0008000 ? '1' : '0'), \
  ((byte) & 0x0004000 ? '1' : '0'), \
  ((byte) & 0x0002000 ? '1' : '0'), \
  ((byte) & 0x0001000 ? '1' : '0'), \
  ((byte) & 0x0000800 ? '1' : '0'), \
  ((byte) & 0x0000400 ? '1' : '0'), \
  ((byte) & 0x0000200 ? '1' : '0'), \
  ((byte) & 0x0000100 ? '1' : '0'), \
  ((byte) & 0x0000080 ? '1' : '0'), \
  ((byte) & 0x0000040 ? '1' : '0'), \
  ((byte) & 0x0000020 ? '1' : '0'), \
  ((byte) & 0x0000010 ? '1' : '0'), \
  ((byte) & 0x0000008 ? '1' : '0'), \
  ((byte) & 0x0000004 ? '1' : '0'), \
  ((byte) & 0x0000002 ? '1' : '0'), \
  ((byte) & 0x0000001 ? '1' : '0') 
#endif