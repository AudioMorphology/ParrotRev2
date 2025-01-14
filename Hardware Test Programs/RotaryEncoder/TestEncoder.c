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
 * @file TestEncoder.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 * 
 * Tests the Rotary Encoder & panic switch on Rev 1 hardware
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "parrot.h"

uint16_t  Encoder_MA[16];           // Clock input Moving Average buffer
uint Encoder_MA_Ptr;
uint16_t Encoder_Average;
long Encoder_MA_Sum;
int Encoder_MA_Len = 2; 

uint SyncFree = 0;   
uint32_t glbDelay;                  // The actual current Delay (in Samples)
uint32_t targetDelay;               // The target Delay (in samples) - glbDelay will ramp towards this value
uint32_t glbIncrement = 1;          // How many samples the delay is increased or decreased by via the rotary encoder
uint8_t lrmem = 3;
int lrsum = 0;
uint32_t PrevClick;
uint32_t ThisClick;
uint32_t ClickDiff;

int glbEncoderSw;                 // global value for divisor used by other functions
int LatestEncoderSw = 0;          // latest value for the Divisor that may or may not be the same as glbDivisor
uint64_t EncoderSwChangedTime;    // Time at which the divisor changed
uint64_t DeBounceTime = 200000;   // Debounce time in uS


// Rotary encoder state transition table
int8_t TRANS[] = {0, -1, 1, 14, 1, 0, 14, -1, -1, 14, 0, 1, 14, 1, -1, 0};

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

void poll_encoder(){
  int8_t dir = checkRotaryEncoder();
    if (dir != 0){
    // Only alter the delay if we're free-running
    printf("Direction: %d\n",dir);
  }
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

    ThisClick = time_us_32();
    ClickDiff = ThisClick - PrevClick;
    PrevClick = ThisClick;
    printf("Speed = %d\n",ClickDiff);

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
      //printf("Dir: %d, Increment: %d, Target Delay: %d\n",dir, glbIncrement, targetDelay);
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

  }
}

void checkReset(){
  int thisEncoderSw = gpio_get(ENCODER_SW);
  if ((int)thisEncoderSw != LatestEncoderSw) {
    LatestEncoderSw = thisEncoderSw;
    EncoderSwChangedTime = time_us_64();
  } else {
    if (time_us_64() >= EncoderSwChangedTime + DeBounceTime){
      // we can trust this value, so set the global if different
      if (glbEncoderSw != (int)thisEncoderSw) {
        //todo poss protect with spinlocks?
        glbEncoderSw = (int)thisEncoderSw;
        printf("Encoder Switch = %d\n",glbEncoderSw); 
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
    // Attach IRQ to external clock input
    // Triggered on Rising AND Falling edges
    // Note: this function should only be called once
    // any subsequent IRQs need to be set with gpio_set_irq_enabled
    // Then this primary IRQ calls other functions depending on the pin.
    //gpio_set_irq_enabled_with_callback(CLOCK_IN,0x0C,1,clock_callback);
    // This attaches an IRQ to the encoder pins. Unfortunately
    // there is only one IRQ Handler, which has to then dispatch
    // calls based upon which GPIO triggered the interrupt 
    //gpio_set_irq_enabled(ENCODERA_IN,GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,1);
    //gpio_set_irq_enabled(ENCODERB_IN,GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,1);
    for(Encoder_MA_Ptr=0;Encoder_MA_Ptr <= Encoder_MA_Len;Encoder_MA_Ptr++){
      Encoder_MA[Encoder_MA_Ptr] = 0;
    }
    Encoder_MA_Ptr = 0;
    Encoder_Average = 0;
    Encoder_MA_Sum = 0;
    PrevClick = time_us_32();
    glbEncoderSw = 0;
    LatestEncoderSw = 0;
    EncoderSwChangedTime = time_us_64();

    while(1){
        checkReset();
        poll_encoder();
        tight_loop_contents();
    }

}




