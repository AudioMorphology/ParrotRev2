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
 * @file parrot_core1.c
 * 
 * Contains all functions that run on the second core of the Pico (Core1)
 * These are primarily the internal and external clock routines and the 
 * less time-critical user interface routines
 * 
 * In general global variables are set by routines in parrot_core1, then read
 * and acted upon by the time-critical functions in parrot_main
 */
#include <stdio.h>
#include <math.h>
#include "parrot.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/adc.h"
#include "hardware/sync.h"

bool temp = false;

double ClockBPM;                  // BPM Value for interal clock
double ClockFreq;                 // Internal Clock Frequency
double ClockPeriod;               // Internal Clock Period
int ClockPhase;                   // Toggles between 0 & 1 
int LEDPhase;
_Atomic int32_t ExtClockPeriod;   // External Clock Period (rising edge to rising edge)
_Atomic int32_t ExtClockTick;     // Records the last tick for the External Clock
uint SyncFree;                    // Delay time is just controlled by Rotary Encoder, or sync'd to external/internal clock
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
uint32_t  ExtClock_MA[16];        // External Clock input Moving Average buffer
uint ExtClock_MA_Ptr;
uint32_t ExtClock_Average;
long ExtClock_MA_Sum;
uint8_t lrmem = 3;
int lrsum = 0;
// Rotary encoder state transition table
int8_t TRANS[] = {0, -1, 1, 14, 1, 0, 14, -1, -1, 14, 0, 1, 14, 1, -1, 0};


/***********************
 * Function Definitions
 ***********************/
/**
 * @brief Rotary Encoder finite state machine
 * 
 * This comes from a technical paper by Marko Pinteric
 * https://www.pinteric.com/rotary.html, with additions
 * by Ralph S. Bacon
 * https://github.com/RalphBacon/226-Better-Rotary-Encoder---no-switch-bounce
 */
int8_t checkRotaryEncoder()
{
    // Read BOTH pin states to deterimine validity of rotation (ie not just switch bounce)
    int8_t l = gpio_get(ENCODERA_IN);
    int8_t r = gpio_get(ENCODERB_IN);
    // Move previous value 2 bits to the left and add in our new values
    lrmem = ((lrmem & 0x03) << 2) + 2 * l + r;
    // Convert the bit pattern to a movement indicator (14 = impossible, ie switch bounce)
    lrsum += TRANS[lrmem];
    /* encoder not in the neutral (detent) state */
    if (lrsum % 4 != 0){
        return 0;
    }
    /* encoder in the neutral state - clockwise rotation*/
    if (lrsum == 4){
        lrsum = 0;
        return 1;
    }
    /* encoder in the neutral state - anti-clockwise rotation*/
    if (lrsum == -4){
        lrsum = 0;
        return -1;
    }
    // An impossible rotation has been detected - ignore the movement
    lrsum = 0;
    return 0;
}
/**
 * @brief Rotary Encoder IRQ callback
 * 
 * In Free-running mode (ie sync'd to the internal or external clock)
 * this increases or decreases the targetDelay, which is the delay that
 * the glbDelay value is aiming for, but ony moving towards by one sample
 * at a time.
 * 
 * The larger the target delay, the larger the increment and
 * therefore the lower the resolution
 */
void encoder_IRQ_handler(uint gpio,uint32_t events){
  int8_t dir = checkRotaryEncoder();
    if (dir != 0){
    // Only alter the delay if we're free-running
    if (SyncFree == 0){
      // Increment depends on the current increment value
      // TODO: probably play with these thresholds and
      // increments to arrive at something meaningful
      switch(targetDelay){
        case 0 ... 10:
          glbIncrement = 1;
        break;
        case 11 ... 100:
          glbIncrement = 10;
        break;
        case 101 ... 1000:
          glbIncrement = 100;
        break;
        case 1001 ... 10000:
          glbIncrement = 1000;
        break;
        case 10001 ... 100000:
          glbIncrement = 10000;
        break;
        case 100001 ... 1000000:
          glbIncrement = 100000;
        break;
        case 1000001 ... 10000000:
          glbIncrement = 1000000;
        break;
        default:
          glbIncrement = 48000;
      }
      if (dir == 1){
        if ((targetDelay+glbIncrement) < BUF_LEN) targetDelay += glbIncrement; else targetDelay = BUF_LEN;
      }
      else {
        if (targetDelay > glbIncrement) targetDelay -= glbIncrement; else targetDelay = 1;
      }
      // At very large increments it takes the glbDelay a long time to 
      // reach the target, so just bump it
      if (glbIncrement > 10000) glbDelay = targetDelay;
      printf("Increment: %d, Target Delay: %d\n",glbIncrement, targetDelay);
    }
  }
}
/**
 * @brief event handler for the incoming clock IRQ
 * this is normalled from the internal clock via the 
 * external clock input jack.
 * 
 * should be triggered on both the rising and falling edges
 */
void clock_callback(uint gpio, uint32_t events){
    // All interrupts end up here, so Check which gpio
    // triggered the interrupt and dispatch accordingly
    if ((gpio == ENCODERA_IN) || (gpio == ENCODERB_IN)) encoder_IRQ_handler(gpio, events);
    else{
    // Note the clock period on rising edge trigger
    // this is (currently) the Delay
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
      LEDPhase = !LEDPhase;
      //printf("Led Phase: %d\n",LEDPhase);
      gpio_put(LED_PIN,LEDPhase);
      gpio_put(ONBOARD_LED,LEDPhase);
    }
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
}
/**
 * @brief Set an alarm interrupt for the internal clock
 * 
 * This is called once during prog initialisation. Once
 * the timer fires, it repeatedly re-arms itself
 */
static void alarm_in_us(uint32_t ClockPeriod_us) {
  hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
  irq_set_exclusive_handler(ALARM_IRQ, alarm_irq);
  irq_set_enabled(ALARM_IRQ, true);
  alarm_in_us_arm(ClockPeriod_us);
}
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
      //printf("Raw: %d, Average = %d, Feedback: %f\n",Feedback_raw, Feedback_Average, glbFeedback);
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
      ClockBPM = round((40 + (Clock_Average * ClockScale)) * 2)/2;
      ClockFreq = (ClockBPM/60)*2;
      ClockPeriod = 1000000/ClockFreq;  
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
      //printf("Raw: %d, Average = %d, Wet: %f, Dry: %f\n",WetDry_raw, WetDry_Average, glbWet, glbDry);
}
/**
 * @brief Check the state of the Algorithm pins - two pins generate
 * a 2-bit value, which is used to switch between the 4 different 
 * delay / reverb algorithms 
 */
void updateAlgorithm(){
  Algorithm = gpio_get(ALGORITHM_0);
  Algorithm += gpio_get(ALGORITHM_1) * 2;
  //printf("Algorithm: %d\n",Algorithm);
}
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
  //printf("Divisor = %d, glbDivisor = %f\n",Divisor, glbDivisor); 
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
 * @brief core1 entry point called from parrot_main.c
  */
void core1_entry(){
    // All of the stuff to do with the UI, clocks and
    // timers runs on Core 1 leaving Core 0 to run the
    // time-critical Audio I/O
    struct repeating_timer MainClock;
    // Clock out and In
    ClockPhase = 0;
    LEDPhase = 0;
    ExtClockTick = time_us_32();
    glbDelay = 0;
    gpio_init(CLOCK_IN);
    gpio_set_dir(CLOCK_IN, GPIO_IN);
    gpio_init(CLOCK_OUT);
    gpio_set_dir(CLOCK_OUT, GPIO_OUT);
    gpio_init(SYNC_PIN);
    gpio_pull_up(SYNC_PIN);
    gpio_set_dir(SYNC_PIN, GPIO_IN);
    SyncFree = 0;                   // Not sync'd to external clock until told otherwise
    //printf("Multicore Launch\n");

    // Rotary Encoder
    gpio_init(ENCODERA_IN);
    gpio_set_dir(ENCODERA_IN,GPIO_IN);
    gpio_pull_up(ENCODERA_IN);
    gpio_init(ENCODERB_IN);
    gpio_set_dir(ENCODERB_IN,GPIO_IN);
    gpio_pull_up(ENCODERB_IN);
    gpio_init(ENCODER_SW);
    gpio_set_dir(ENCODER_SW,GPIO_IN);
    gpio_pull_up(ENCODER_SW);
    // 8-Way switch + mul/div toggle are
    // BCD-encoded into a 4-Bit number
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
    //gpio_pull_down(DIVISOR_D);

    ExtClockTick = time_us_32();

    //Initialise the internal clock
    ClockBPM = 60;
    ClockFreq = (ClockBPM/60)*2;
    ClockPeriod = 1000000/ClockFreq;
    alarm_in_us(ClockPeriod);

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

    // Prep the Ext Clock Moving Average buffer
    for(ExtClock_MA_Ptr=0;ExtClock_MA_Ptr <= ExtClock_MA_Len;ExtClock_MA_Ptr++){
      ExtClock_MA[ExtClock_MA_Ptr] = 0;
    }
    ExtClock_MA_Ptr = 0;
    ExtClock_Average = 0;
    ExtClock_MA_Sum = 0;

    // Attach IRQ to external clock input
    // Triggered on Rising AND Falling edges
    // Note: this function should only be called once
    // any subsequent IRQs need to be set with gpio_set_irq_enabled
    // Then this primary IRQ calls other functions depending on the pin.
    gpio_set_irq_enabled_with_callback(CLOCK_IN,0x0C,1,clock_callback);
    // This attaches an IRQ to the encoder pins. Unfortunately
    // there is only one IRQ Handler, which has to then dispatch
    // calls based upon which GPIO triggered the interrupt 
    gpio_set_irq_enabled(ENCODERA_IN,GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,1);
    gpio_set_irq_enabled(ENCODERB_IN,GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,1);

    // Main Loop
    while(1){
      // Update the feedback amount
      updateFeedback();
      // check and adjust internal Clock speed
      updateClock();
      // check and adjust the Wet/Dry
      updateWetDry();
      // Check the Status of the Sync / Free switch
      updateSyncFree();
      // check and update the Algoritm
      updateAlgorithm();
      // check and update the clock divisor
      updateDivisor();
      // If we are sync'd to External Clock then update the delay 
      // based on the ExtClockPeriod
      if(SyncFree == 1){
          // Target Delay will actually be some multiple or sub-multiple of the
          // External Clock period (1*, 2* /2, /4 etc), according to the multiple selected  
          PreviousClockPeriod = ExtClockPeriod;
          targetDelay = (uint32_t)((float)ExtClockPeriod / SampleLength) * glbDivisor;
          //printf("Ext Clock Period %d, SampleLength %f, TargetDelay: %d, Divisor: %f\n", ExtClockPeriod, SampleLength,targetDelay, glbDivisor);
          if (targetDelay > BUF_LEN) targetDelay = BUF_LEN;
      }
    }
}


