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
 * @file parrot_func.c
 * 
 * Various signal processing functions
 */
#include <stdio.h>
#include "parrot.h"
#include "psram_spi.h"
#include <arm_math.h>

#include "pico/float.h"
#define SHIFT_AMOUNT 8

#define FUZZ(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))
#define RTH(x) rational_tanh(x);

/**
 * @brief wavefolder
 * 
 * Basic mathematical wavefolder - takes a sample
 * and a gain (from 0 to 1) and applies a simple 
 * mathematical fold to the output
 *  */
float WaveFolder(float input, float gain){
    float output = input * (1.0f + (gain * 3.0f));
    if (output > 1.0f) output = 2.0f - output;
    else if (output < -1.0f) output = -2.0f - output;
    return output;
}

/**
 * @brief WaveWrapper
 * 
 * Basic mathematical wrap function takes an input
 * sample and a Gain in the range 0 .. 1
 */
float WaveWrapper(float input, float gain){
    float output = input * (1.0f + (gain * 2.0f));
    if (output > 1.0f) output -= 2.0f;
    else if (output < -1.0f) output += 2.0f;
    return output;
}

/**
 * @brief
 * 
 * Non-linear amplifier with soft distortion curve.
 */
float CubicAmplifier( float input )
{
    float output, temp;
    if( input < 0.0 ) {
        temp = input + 1.0f;
        output = (temp * temp * temp) - 1.0f;
    }
    else {
        temp = input - 1.0f;
        output = (temp * temp * temp) + 1.0f;
    }
    return output;
}

/**
 * @brief Single Delay
 * 
 * Just writes the Input Sample to the current write
 * point, and returns the delayed sample pointed to 
 * by the Read Pointer
 * 
 * @param InSample Input Sample from Audio buffer
 * @param IsLeft indicates left vs right channel
 */
float single_delay(union uSample InSample, bool IsLeft){
    union uSample ReadSample;
    if (IsLeft == true) {
        ReadSample.iSample = psram_read32(&psram_spi, ReadPointer_L << 3);
        psram_write32(&psram_spi, WritePointer << 3,InSample.iSample);
    } else {
        ReadSample.iSample = psram_read32(&psram_spi, ReadPointer_R << 3);
        psram_write32(&psram_spi, (WritePointer << 3) + 4,InSample.iSample);
    }
    return ReadSample.fSample;
}

/**
 * @brief allpass_filter 
 * 
 * Implements a simple allpass filter
 * 
 * @param InSample 
 * @param delay in a whole number of samples
 * @param gain in the range 0 .. 1
 * @param IsLeft indicates whether we are processing Left or Right sample
 */
float allpass_filter(float InSample, uint32_t delay, float gain, bool IsLeft){
    float fRetVal = AllPassState + gain *InSample;
    AllPassState = InSample - gain * fRetVal;
    return fRetVal;
}

void initAllPassFilter(AllPassFilter *filter, float coefficient) {
    filter->a1 = coefficient;
    filter->z1 = 0.0;
}

float processAllPassFilter(AllPassFilter *filter, float input) {
    float output = filter->a1 * (input - filter->z1) + filter->z1;
    filter->z1 = output + input * filter->a1;
    return output;
}




/**
 * @brief Single-tap delay
 * 
 * Single-tap delay - writes a mix of the delayed signal with
 * the current input sample, and writes it to the current write 
 * position in the buffer.
 * 
 * @param InSample Current Sample from the Audio Input buffer
 * @param gain feedback gain
 * @param IsLeft indicates Left vs Right channel
 *  
 */
float single_tap(union uSample InSample, float gain, bool IsLeft){
    union uSample ReadSample;
    if (IsLeft == true){
        ReadSample.iSample = psram_read32(&psram_spi, ReadPointer_L << 3);
        InSample.fSample += ReadSample.fSample * gain;
        psram_write32(&psram_spi, WritePointer<<3,InSample.iSample);
    } else {
        ReadSample.iSample = psram_read32(&psram_spi, (ReadPointer_R << 3)+4);
        InSample.fSample += ReadSample.fSample * gain;
        psram_write32(&psram_spi, (WritePointer<<3)+4,InSample.iSample);
    }
    return ReadSample.fSample;
}

/**
 * @brief Ping Pong delay
 * 
 * @param InSample current input sample
 * @param gain feedback gain
 * @param IsLeft whether this is the Left or Right channel
 */
float Ping_Pong(union uSample InSample, float gain, bool IsLeft){
    union uSample ReadSample;

    if(IsLeft == true){
        // Left channel is delayed by half the glbDelay period again
        uint32_t localReadPtr = ReadPointer_L;
        localReadPtr = (ReadPointer_L + (glbDelay_L >> 1)) & BUF_LEN;
        ReadSample.iSample = psram_read32(&psram_spi, localReadPtr << 3);
        InSample.fSample += ReadSample.fSample * gain;
        psram_write32(&psram_spi, WritePointer << 3,InSample.iSample);
    } else {
        // Right Channel
        ReadSample.iSample = psram_read32(&psram_spi, (ReadPointer_R << 3)+4);
        InSample.fSample += ReadSample.fSample * gain;
        psram_write32(&psram_spi, (WritePointer << 3)+4,InSample.iSample);
    }
    return ReadSample.fSample;
}

// To avoid the multiplication or division, this just uses a simple bit-shift
// for the feedback level, gain is the number of bits to shift right, so more
// is less volume
int32_t single_tap_shift(int32_t InSample, uint32_t delay, uint8_t gain){
    //int32_t ReadSample = Peek32(spi,ReadPointer);
    //InSample += ReadSample >> gain;
    //Poke32(spi,(ReadPointer + delay) & BUF_LEN,InSample);
    //return ReadSample;     
    return 0;
}


/**
 * @brief rational tanh
 *
 * This is a rational function to approximate a tanh-like soft clipper. 
 * It is based on the pade-approximation of the tanh function with tweaked coefficients.
 * The function is in the range x=-3..3 and outputs the range y=-1..1. 
 * Beyond this range the output must be clamped to -1..1.
 * The first two derivatives of the function vanish at -3 and 3, 
 * so the transition to the hard clipped region is C2-continuous.
 */
float rational_tanh(float x) {
    if( x < -3.0f ) return -1.0;
    else if( x > 3.0f ) return 1.0;
    else return x * ( 27.0f + x * x ) / ( 27.0f + 9.0f * x * x );
}

/**
 * @brief Soft Clip function
 * 
 * implements a 1.5x - 0.5x^3 waveshaper
 */
float soft_clip(float x) {
    if (x > 1) x = 1;
    if (x < -1) x = -1;
    return 1.5 * x - 0.5 * x * x * x; // Simple f(x) = 1.5x - 0.5x^3 waveshaper
}
