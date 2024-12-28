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
 * @file TestAlgorithm.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 * 
 * Tests the Sync / Run switch plus 4-way rotary switch on Rev 1 hardware
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "parrot.h"

uint SyncFree;                    
uint PrevSyncFree;
int Algorithm = 0;
int PrevAlgorithm; 

/**
 * @brief Check the state of the Algorithm pins - two pins generate
 * a 2-bit value, which is used to switch between the 4 different 
 * delay / reverb algorithms 
 */
void updateAlgorithm(){
  Algorithm = gpio_get(ALGORITHM_0);
  Algorithm += gpio_get(ALGORITHM_1) * 2;
  SyncFree = gpio_get(SYNC_PIN);
  if (PrevAlgorithm != Algorithm){
    PrevAlgorithm = Algorithm;
    printf("Algorithm: %d\n",Algorithm);
  }
  if (PrevSyncFree != SyncFree) {
    PrevSyncFree = SyncFree;
    printf("Sync / Free: %d\n",SyncFree);
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
    // Algorithm Pins
    gpio_init(ALGORITHM_0);
    gpio_set_dir(ALGORITHM_0,GPIO_IN);
    //gpio_pull_down(ALGORITHM_0);
    gpio_init(ALGORITHM_1);
    gpio_set_dir(ALGORITHM_1,GPIO_IN);
    //gpio_pull_down(ALGORITHM_1);
    // Sync / Free switch
    gpio_init(SYNC_PIN);
    gpio_pull_up(SYNC_PIN);
    gpio_set_dir(SYNC_PIN, GPIO_IN);
    SyncFree = 0;                   // Not sync'd to external clock until told otherwise

    while(1){
        updateAlgorithm();
        tight_loop_contents();
    }

}




