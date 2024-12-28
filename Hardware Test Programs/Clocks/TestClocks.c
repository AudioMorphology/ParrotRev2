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
 * Tests the Sync / Free switch, Ext Clock in, Int Clock Out, Encoder + Switch 
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "parrot.h"

double ClockBPM;                  // BPM Value for interal clock
double ClockFreq;                 // Internal Clock Frequency
double ClockPeriod;               // Internal Clock Period
int ClockPhase;                   // Toggles between 0 & 1 
int32_t PreviousClockPeriod = 0;    //
_Atomic int32_t ExtClockPeriod;   // External Clock Period (rising edge to rising edge)
_Atomic int32_t ExtClockTick;     // Records the last tick for the External Clock
static const uint ExtClock_MA_Len = 10;         // Length of the ExtClock Moving Average buffer (to filter out timing irregularities)
uint32_t  ExtClock_MA[16];        // External Clock input Moving Average buffer
uint ExtClock_MA_Ptr;
uint32_t ExtClock_Average;
long ExtClock_MA_Sum;
uint SyncFree;                    // Delay time is just controlled by Rotary Encoder, or sync'd to external/internal clock

/**
 * @brief event handler for the incoming clock IRQ
 * this is normalled from the internal clock via the 
 * external clock input jack.
 * 
 * should be triggered on both the rising and falling edges
 */
void clock_callback(uint gpio, uint32_t events){
    // Note the clock period on rising edge trigger
    if (events & GPIO_IRQ_EDGE_RISE) {
      // It took ages to find a way of persuading the GCC compiler
      // to let a Global variable be updated from within an ISR - it
      // kept optimising it out. Usual method seems to be to declare
      // the globals as Volatile, but that didn't work, but finally
      // I discovered that declaring them as Atomic did!!
      uint32_t ThisTick = time_us_32();
      ExtClockPeriod = ThisTick - ExtClockTick;
      ExtClockTick = ThisTick;
      // run it through a Moving Average buffer to reduce abrupt changes
      ExtClock_MA_Sum = ExtClock_MA_Sum - ExtClock_MA[ExtClock_MA_Ptr] + ExtClockPeriod;
      ExtClock_MA[ExtClock_MA_Ptr++] = ExtClockPeriod;
      if(ExtClock_MA_Ptr >=  ExtClock_MA_Len) ExtClock_MA_Ptr = 0;
      ExtClockPeriod = ExtClock_MA_Sum / ExtClock_MA_Len;
      if (SyncFree == 0) printf("Ext Clock In\n");
    }
}
/**
 * @brief arm the internal clock interrupt
 */
static void alarm_in_us_arm(uint32_t ClockPeriod_us) {
  uint64_t target = timer_hw->timerawl + ClockPeriod_us;
  timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
}
/**
 * @brief toggles the CLOCK_OUT gpio pin then
 * re-arms the Clock Timer according to the current
 * clock period, which is a global variable adjusted
 * via the clock ADC.
 */
static void alarm_irq(void) {
  hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
  alarm_in_us_arm(ClockPeriod);
  ClockPhase = !ClockPhase;
  gpio_put(CLOCK_OUT, ClockPhase);
  if(SyncFree == 1) ("Clock out: %d\n", ClockPhase);
}
/**
 * @brief Set an alarm interrupt for the internal clock
 * 
 * This is called once during prog initialisation. Once
 * the timer fires, it repeatedly re-arms itself
 */
static void alarm_in_us(uint32_t ClockPeriod_us) {
  printf("Alarm_in_us\n");
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  irq_set_enabled(ALARM_IRQ, true);
  alarm_in_us_arm(ClockPeriod_us);
}


/**
 * @brief Check for the Rotary Encoder button push
 * 
 * This zeroes out the entire delay buffer. Saving and disabling
 * interrupts only works for the core you are in, hence why this 
 * is run in parrot_main rather than parrot_core1
 * 
 * Pulling the XSMT (Soft Mute) pin of the PCM5102 DAC low mutes
 * the output whilst this is happening
 */
void checkReset(){
  if(!gpio_get(ENCODER_SW)){
    printf("Encoder button pressed\n");
  }
}

/**
 * @brief updates the status of the Sync/Free global variable
 * only reason this is in a function is so that it can test the 
 * current state, and output a message if/when the state changes 
 */
void updateSyncFree(){
  int ThisSync = gpio_get(SYNC_PIN);
  if (ThisSync != SyncFree){
    SyncFree = ThisSync;
    printf("Sync / Free switch = %d\n",SyncFree);
  } 
}


/**
 * @brief Main program entry point
 * 
 * Part of the Camberwell Parrot test suite
 * Depending on the state of the Sync / Free 
 * switch, this prog just sends incoming Clock
 * to the output
 */
int main(){
    // Serial port initialisation (Using USB for stdio)
    stdio_init_all();
    for(int i = 1;i <= 20; i++){
      printf("Waiting to start %d\n",20-i);
      sleep_ms(1000);
    }

    struct repeating_timer MainClock;
    // Clock out and In
    ClockPhase = 0;
    ExtClockTick = time_us_32();
    gpio_init(CLOCK_IN);
    gpio_set_dir(CLOCK_IN, GPIO_IN);
    gpio_init(CLOCK_OUT);
    gpio_set_dir(CLOCK_OUT, GPIO_OUT);
    gpio_init(SYNC_PIN);
    gpio_pull_up(SYNC_PIN);
    gpio_set_dir(SYNC_PIN, GPIO_IN);
    SyncFree = 0;                   // Not sync'd to external clock until told otherwise
    gpio_init(ENCODER_SW);
    gpio_set_dir(ENCODER_SW,GPIO_IN);
    gpio_pull_up(ENCODER_SW);

    //Initialise the internal clock
    ClockBPM = 60;
    ClockFreq = (ClockBPM/60)*2;
    ClockPeriod = 1000000/ClockFreq;
    alarm_in_us(ClockPeriod);

    // Prep the Ext Clock Moving Average buffer
    for(ExtClock_MA_Ptr=0;ExtClock_MA_Ptr <= ExtClock_MA_Len;ExtClock_MA_Ptr++){
      ExtClock_MA[ExtClock_MA_Ptr] = 0;
    }
    ExtClock_MA_Ptr = 0;
    ExtClock_Average = 0;
    ExtClock_MA_Sum = 0;
    ExtClockTick = time_us_32();
    gpio_set_irq_enabled_with_callback(CLOCK_IN,0x0C,1,clock_callback);    
    
    while(1){
      checkReset();
      updateSyncFree();
      tight_loop_contents();
    }

}




