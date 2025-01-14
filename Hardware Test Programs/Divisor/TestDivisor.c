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
 * @file TestDivisor.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 * 
 * Tests the * / - switch plus 8-way rotary switch on Rev 1 hardware
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "parrot.h"
#include "hardware/clocks.h"

float glbRatio = 1.00;
int glbDivisor;
int LatestDivisor;
uint64_t DivisorChangedTime;

int glbAlgorithm;               // global value for divisor used by other functions
int LatestAlgorithm = 0;        // latest value for the Divisor that may or may not be the same as glbDivisor
uint64_t AlgorithmChangedTime;  // Time at which the divisor changed
uint64_t DeBounceTime = 200000; // Debounce time in uS

/**
 * @brief An array of multipliers, which are applied to the master internal
 * or external clock frequency to set the delay time as a multiple of the clock period
 * 
 * 1/12 1/8 1/6 1/5 1/4 1/3 1/2 1* 2* 3* 4* 5* 6* 8* 12*
 */
float divisors[] = {1,2,3,4,5,6,8,12,1,0.5,0.33333,0.25,0.2,0.166666,0.125,0.083333};

/**
 * @brief read the 4-Bit BCD value from the Divisor switch, read
 * the appropriate Divisor (multiple of the clock period) and set
 * the global divisor value accordingly
 * 
 * This implements a software de-bounce, which looks for any
 * change to be stable for at least 200ms before deciding that
 * it can be trusted
 */
void updateDivisor(){
  uint32_t thisDivisor = gpio_get_all();
  thisDivisor = thisDivisor >> 3;             // shift so that gpio3 is LSB
  thisDivisor &= 0xf;
  if (thisDivisor > 15) thisDivisor = 15;
  if ((int)thisDivisor != LatestDivisor) {
    LatestDivisor = thisDivisor;
    // note the time of the most recent change
    DivisorChangedTime = time_us_64();
  } else {
    if (time_us_64() >= DivisorChangedTime + DeBounceTime){
      // we can trust this value, so set the global if different
      if (glbDivisor != (int)thisDivisor) {
        //todo poss protect with spinlocks?
        glbDivisor = (int)thisDivisor;
        glbRatio = divisors[thisDivisor];
        printf("Divisor = %d, Ratio = %f\n",glbDivisor, glbRatio); 
      }
    }
  }
}

/**
 * @brief read the 3-Bit BCD value from the Algorithm switch
 * 
 * This implements a software de-bounce, which looks for any
 * change to be stable for at least 200ms before deciding that
 * it can be trusted
 */
void updateAlgorithm(){
  uint32_t thisAlgorithm = gpio_get_all();
  thisAlgorithm &= 0x7;
  if (thisAlgorithm > 7) thisAlgorithm = 7;
  if ((int)thisAlgorithm != LatestAlgorithm) {
    LatestAlgorithm = thisAlgorithm;
    AlgorithmChangedTime = time_us_64();
  } else {
    if (time_us_64() >= AlgorithmChangedTime + DeBounceTime){
      // we can trust this value, so set the global if different
      if (glbAlgorithm != (int)thisAlgorithm) {
        //todo poss protect with spinlocks?
        glbAlgorithm = (int)thisAlgorithm;
        printf("Algorithm = %d\n",glbAlgorithm); 
      }
    }
  }
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
    for(int i = 1;i <= 40; i++){
      printf("\n");
    }
    // 8-Way switch + mul/div toggle are
    // BCD-encoded into a 4-Bit number
    gpio_init(DIVISOR_0);
    gpio_set_dir(DIVISOR_0,GPIO_IN);
    gpio_init(DIVISOR_1);
    gpio_set_dir(DIVISOR_1,GPIO_IN);
    gpio_init(DIVISOR_2);
    gpio_set_dir(DIVISOR_2,GPIO_IN);
    gpio_init(DIVISOR_3);
    gpio_set_dir(DIVISOR_3,GPIO_IN);
    gpio_pull_up(DIVISOR_3);    // Switched to ground, so use Pull UP
    // 8-Way switch BCD-encoded into 
    // a 3-Bit number
    gpio_init(ALGORITHM_0);
    gpio_set_dir(ALGORITHM_0,GPIO_IN);
    gpio_init(ALGORITHM_1);
    gpio_set_dir(ALGORITHM_1,GPIO_IN);
    gpio_init(ALGORITHM_2);
    gpio_set_dir(ALGORITHM_2,GPIO_IN);

    LatestDivisor = 0;
    glbDivisor = 0;
    glbRatio = 1.00;
    DivisorChangedTime = time_us_64();
    LatestAlgorithm = 0;
    glbAlgorithm = 0;
    AlgorithmChangedTime = time_us_64();

    while(1){
        updateDivisor();
        updateAlgorithm();
        tight_loop_contents();
    }

}




