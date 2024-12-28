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

float glbDivisor = 1;
uint32_t PrevDivisor = 0;
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
 */
void updateDivisor(){
  uint32_t Divisor = gpio_get_all();
  Divisor = Divisor >> 2;             // shift so that gpio2 is LSB
  Divisor &= 0xf;
  if (Divisor > 15) Divisor = 15;
  glbDivisor = divisors[Divisor];
  if (PrevDivisor != Divisor) {
    // Only print if it changes
    PrevDivisor = Divisor;
    printf("Divisor = %d, glbDivisor = %f\n",Divisor, glbDivisor); 
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
    // 8-Way switch + mul/div toggle are
    // BCD-encoded into a 4-Bit number
    // Internal Pull-down resistors serve no purpose
    gpio_init(DIVISOR_A);
    gpio_set_dir(DIVISOR_A,GPIO_IN);
    //gpio_pull_down(DIVISOR_A);
    gpio_init(DIVISOR_B);
    gpio_set_dir(DIVISOR_B,GPIO_IN);
    //gpio_pull_down(DIVISOR_B);
    gpio_init(DIVISOR_C);
    gpio_set_dir(DIVISOR_C,GPIO_IN);
    //gpio_pull_down(DIVISOR_C);
    gpio_init(DIVISOR_D);
    gpio_set_dir(DIVISOR_D,GPIO_IN);
    gpio_pull_up(DIVISOR_D);    // Switched to ground, so use Pull UP

    while(1){
        updateDivisor();
        tight_loop_contents();
    }

}




