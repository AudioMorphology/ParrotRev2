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
#include "malloc.h"
#include <arm_math.h>
#include "pico/float.h"
#define SHIFT_AMOUNT 8

#define FUZZ(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))
#define RTH(x) rational_tanh(x);
uint32_t PrevTime, TimeNow;
/**
 * @brief Get free RAM using static memory defines
 *        cf. https://forums.raspberrypi.com/viewtopic.php?t=347638#p2082565
 *
 * @return size_t
 */
size_t get_free_ram()
{
    extern char __StackLimit, __bss_end__;
    size_t total_heap = &__StackLimit - &__bss_end__;
    struct mallinfo info = mallinfo();
    return total_heap - info.uordblks;
}


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
 * @brief Euclidean Delay
 * 
 * A Multi-tap delay, where the taps are either ON or OFF 
 * depending on the current Euclidean sequence, which is
 * calculated as a number of 'Hits' within a given number
 * of Steps.
 * 
 * The gain applied to each successive tap is decreased
 * compared to the inital Gain amount
 */
float Euclidean_Delay(float gain){
    union uSample ReadSample;
    float LocalSample = 0.0;
    float LocalGain = gain;
    //!!TODO should this be uint32_t LocalDelay_L = (uint32_t)(((float)glbDelay_L / glbRatio)/(float)EuclideanSteps[glbDivisor]-1.0);
    //!! or even +1??
    uint32_t LocalDelay_L = (uint32_t)(((float)glbDelay_L / glbRatio)/(float)EuclideanSteps[glbDivisor]);
//    TimeNow = time_us_32();
//    if(TimeNow > PrevTime + 500000){
//        PrevTime = TimeNow;
//        printf("glbDelay_L: %d, glbRatio: %f, Local Delay: %d, Per Step: %d\n",glbDelay_L,glbRatio,LocalDelay_L,LocalDelay_L/EuclideanSteps[glbDivisor]);
//    } 

    for (int thisStep = 0; thisStep < EuclideanSteps[glbDivisor];thisStep++){
        //check whether this step is a 'hit'
        if(EuclideanHits[thisStep]== 1){
            uint32_t LocalReadPtr_L = ((WritePointer + BUF_LEN) - (LocalDelay_L * (thisStep +1))) % BUF_LEN;
            ReadSample.iSample = psram_read32(&psram_spi, LocalReadPtr_L << 3);
            LocalSample += ReadSample.fSample * LocalGain;
            LocalGain *= 0.5f; // Each tap reduce by 6dB 
        }
      }
  return LocalSample;    
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
 * C Equivalent of the Arduino bitRead() function
 */
int bitRead(unsigned int value, unsigned int bit) {
    return (value >> bit) & 1;
}

/**
 * @brief find the binary length of a number
 */
int findlength(unsigned int bnry){
    bool lengthfound = false;
    int length=1; // no number can have a length of zero - single 0 has a length of one, but no 1s for the sytem to count
    for (int q=32;q>=0;q--){
      int r=bitRead(bnry,q);
      if(r==1 && lengthfound == false){
        length=q+1;
        lengthfound = true;
      }
    }
    return length;
  }

/**
 * @brief concatenate two binary numbers
 */
unsigned int ConcatBin(unsigned int bina, unsigned int binb){
    int binb_len=findlength(binb);
    unsigned int sum=(bina<<binb_len);
    sum = sum | binb;
    return sum;
  }


/**
 * @brief calculate Euclidean fill, given a number of steps and a fill number
 * 
 * This uses the Bjorklund algorithm to calculate a pulse pattern given a number
 * of steps and a fill value. The fill Value needs to be less than or equal to the 
 * number of steps
 * 
 * This returns a 16-Bit unsigned Int binary pattern. Our maximum number of steps
 * is 12, so 16-Bits will hold that quite adequately. The idea is that we pre-calculate
 * all possible Step/Fill combinations.
 * 
 * This is a variation of Tom Whitwell's Euclidean Sequencer Arduino code 
 * 
 * @param n Total number of steps
 * @param k Number of 'Hits' to be evenly distibuted across the steps
 */
unsigned int bjorklund(int n, int k){
    int pauses = n-k;
    int pulses = k;
    int per_pulse = pauses/k;
    int remainder = pauses%pulses;  
    unsigned int workbeat[n];
    unsigned int outbeat;
    unsigned int working;
    int workbeat_count=n;
    int a; 
    int b; 
    int trim_count;
    for (a=0;a<n;a++){ // Populate workbeat with unsorted pulses and pauses 
      if (a<pulses){
        workbeat[a] = 1;
      }
      else {
        workbeat [a] = 0;
      }
    }
  
    if (per_pulse>0 && remainder <2){ // Handle easy cases where there is no or only one remainer  
      for (a=0;a<pulses;a++){
        for (b=workbeat_count-1; b>workbeat_count-per_pulse-1;b--){
          workbeat[a]  = ConcatBin(workbeat[a], workbeat[b]);
        }
        workbeat_count = workbeat_count-per_pulse;
      }
  
      outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count 
      for (a=0;a < workbeat_count;a++){
        outbeat = ConcatBin(outbeat,workbeat[a]);
      }
      return outbeat;
    }
  
    else { 
      int groupa = pulses;
      int groupb = pauses; 
      int iteration=0;
      if (groupb<=1){
      }
      while(groupb>1){ //main recursive loop
        if (groupa>groupb){ // more Group A than Group B
          int a_remainder = groupa-groupb; // what will be left of groupa once groupB is interleaved 
          trim_count = 0;
          for (a=0; a<groupa-a_remainder;a++){ //count through the matching sets of A, ignoring remaindered
            workbeat[a]  = ConcatBin (workbeat[a], workbeat[workbeat_count-1-a]);
            trim_count++;
          }
          workbeat_count = workbeat_count-trim_count;
          groupa=groupb;
          groupb=a_remainder;
        }
        else if (groupb>groupa){ // More Group B than Group A
          int b_remainder = groupb-groupa; // what will be left of group once group A is interleaved 
          trim_count=0;
          for (a = workbeat_count-1;a>=groupa+b_remainder;a--){ //count from right back through the Bs
            workbeat[workbeat_count-a-1] = ConcatBin (workbeat[workbeat_count-a-1], workbeat[a]);
            trim_count++;
          }
          workbeat_count = workbeat_count-trim_count;
          groupb=b_remainder;
        }
        else if (groupa == groupb){ // groupa = groupb 
          trim_count=0;
          for (a=0;a<groupa;a++){
            workbeat[a] = ConcatBin (workbeat[a],workbeat[workbeat_count-1-a]);
            trim_count++;
          }
          workbeat_count = workbeat_count-trim_count;
          groupb=0;
        }
        else {
          //printf("ERROR");
        }
        iteration++;
      }
    outbeat = 0; // Concatenate workbeat into outbeat - according to workbeat_count 
      for (a=0;a < workbeat_count;a++){
        outbeat = ConcatBin(outbeat,workbeat[a]);
      }
      return outbeat;
    }
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
