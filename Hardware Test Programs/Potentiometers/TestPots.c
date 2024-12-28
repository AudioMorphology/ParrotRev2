/******************************************************************************

TestDivisor

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
 * @file TestPots.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 * 
 * Tests the 3 analogue potentiometers on Rev 1 Hardware
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "parrot.h"
#include "hardware/adc.h"
#include <math.h>

float ClockBPM;                  // BPM Value for interal clock
double ClockFreq;                 // Internal Clock Frequency
double ClockPeriod;               // Internal Clock Period
float glbFeedback = 0.1;


uint16_t Feedback_MA[16];         // 16-byte moving average buffer for feedback pot. NOTE: statically defined length. We'll not use all 16 elements.
uint Feedback_MA_Ptr;             // Pointer to Moving Average buffer
uint16_t Feedback_Average;        // Moving average result
long Feedback_MA_Sum;             // Running total of the values in the buffer
uint16_t  Clock_MA[16];           // Clock input Moving Average buffer
uint Clock_MA_Ptr;
uint16_t Clock_Average;
long Clock_MA_Sum;
uint16_t  WetDry_MA[16];          // We/Dry input Moving Average buffer
uint WetDry_MA_Ptr;
uint16_t WetDry_Average;
long WetDry_MA_Sum;

float glbWet = 0;                   // wet/dry multipliers                   
float glbDry = 1;

/**
 * @brief check the Feedback Pot ADC input and update
 * the global Feedback amount parameters. This updates
 * a moving average figure to try and eliminate or at
 * least reduce the noise on the ADC input
 */
void updateFeedback(){
      adc_select_input(ADC_feedback);
      sleep_ms(1);  //  settling time
      uint16_t  Feedback_raw = adc_read();
      // Run this through a moving average buffer
      Feedback_MA_Sum = Feedback_MA_Sum - Feedback_MA[Feedback_MA_Ptr] + Feedback_raw;
      Feedback_MA[Feedback_MA_Ptr++] = Feedback_raw;
      if(Feedback_MA_Ptr >=  Feedback_MA_Len) Feedback_MA_Ptr = 0;
      Feedback_Average = Feedback_MA_Sum / Feedback_MA_Len;
      glbFeedback = Feedback_Average * Feedback_scale;
      // ensure we have a good solid 1.0 and 0.0
      // TODO: Check these thresholds!!
      if (glbFeedback > 0.97) glbFeedback = 1.0;
      if (glbFeedback < 0.01) glbFeedback = 0.0;
      printf("Raw: %d, Average = %d, Feedback: %f\n",Feedback_raw, Feedback_Average, glbFeedback);
}
/**
 * @brief Checks the Clock Pot ADC input and updates
 * the global Internal Clock Speed. This updates
 * a moving average figure to try and eliminate or at
 * least reduce the noise on the ADC input
 */
void updateClock(){
      adc_select_input(ADC_Clock);
      sleep_ms(1);  //  settling time
      uint16_t  Clock_raw = adc_read();
      //printf("Clock Raw: %d\n",Clock_raw);
      // Run this through a moving average buffer
      Clock_MA_Sum = Clock_MA_Sum - Clock_MA[Clock_MA_Ptr] + Clock_raw;
      Clock_MA[Clock_MA_Ptr++] = Clock_raw;
      if(Clock_MA_Ptr >=  Clock_MA_Len) Clock_MA_Ptr = 0;
      Clock_Average = Clock_MA_Sum / Clock_MA_Len;
      // Round the BPM values to 0.5 BPM accuracy to prevent clock drift
      // Do this by doubling, rounding then halving
      //ClockBPM = round((40 + ((float)Clock_Average * ClockScale)) * 2.0)/2;
      ClockBPM = 40.0 + ((float)Clock_Average * ClockScale);
      ClockBPM = round(ClockBPM * 2.0)/2;
      ClockFreq = (ClockBPM/60.0) * 2.0;
      ClockPeriod = 1000000/ClockFreq;
      printf("Clock Raw: %d, Clock Average: %d,Int Clock Speed: %f, Int Clock Freq: %f, \n",Clock_raw,Clock_Average,ClockBPM, ClockFreq);  
}
/**
 * @brief check the value of the wet/dry pot and 
 * update the global Wet and Dry values. Raw values
 * are run through a moving average buffer in order 
 * to reduce noise
 */
void updateWetDry(){
      adc_select_input(ADC_WetDry);
      sleep_ms(1);  //  settling time
      uint16_t  WetDry_raw = adc_read();
      // Run this through a moving average buffer
      WetDry_MA_Sum = WetDry_MA_Sum - WetDry_MA[WetDry_MA_Ptr] + WetDry_raw;
      WetDry_MA[WetDry_MA_Ptr++] = WetDry_raw;
      if(WetDry_MA_Ptr >=  WetDry_MA_Len) WetDry_MA_Ptr = 0;
      WetDry_Average = WetDry_MA_Sum / WetDry_MA_Len;
      // Convert this to a floating point scale of 0...1
      glbWet = (WetDry_Average * WetDry_Scale)/100;
      // ensure we have a good solid 1.0 and 0.0
      // TODO: Check these thresholds!!
      if (glbWet > 0.97) glbWet = 1.0;
      if (glbWet < 0.05) glbWet = 0.0;
      glbDry = 1 - glbWet;
      printf("Raw: %d, Average = %d, Wet: %f, Dry: %f\n",WetDry_raw, WetDry_Average, glbWet, glbDry);
}


/**
 * @brief Main program entry point
 * 
 * Part of the Camberwell Parrot test suite
 * This prog just displays the value of the 
 * Div / Mult switches each time they change
 */
int main(){
    // Serial port initialisation (Using USB for stdio)
    stdio_init_all();
    for(int i = 1;i <= 20; i++){
      printf("Waiting to start %d\n",20-i);
      sleep_ms(1000);
    }
    //Initialise ADC Inputs
    adc_init();
    adc_gpio_init(FEEDBACK_PIN);    //ADC 0
    adc_gpio_init(CLOCKSPEED_PIN);  //ADC 1
    adc_gpio_init(WETDRY_PIN);      //ADC 2

    // Prep the Feedback Moving Average buffer
    for(Feedback_MA_Ptr=0;Feedback_MA_Ptr <= Feedback_MA_Len;Feedback_MA_Ptr++){
      Feedback_MA[Feedback_MA_Ptr] = 0;
    }
    Feedback_MA_Ptr = 0;
    Feedback_Average = 0;
    Feedback_MA_Sum = 0;
    // Prep the Clock Moving Average buffer
    for(Clock_MA_Ptr=0;Clock_MA_Ptr <= Clock_MA_Len;Clock_MA_Ptr++){
      Clock_MA[Clock_MA_Ptr] = 0;
    }
    Clock_MA_Ptr = 0;
    Clock_Average = 0;
    Clock_MA_Sum = 0;
    // Prep the Wet/Dry Moving Average buffer
    for(WetDry_MA_Ptr=0;WetDry_MA_Ptr <= WetDry_MA_Len;WetDry_MA_Ptr++){
      WetDry_MA[WetDry_MA_Ptr] = 0;
    }
    WetDry_MA_Ptr = 0;
    WetDry_Average = 0;
    WetDry_MA_Sum = 0;

    while(1){
      updateClock();
      updateFeedback();
      updateWetDry();
      printf("-\n");
      sleep_ms(1000);
      tight_loop_contents();
    }

}




