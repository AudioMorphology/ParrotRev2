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
 * @file parrot_main.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 */

#include <stdio.h>
#include "parrot.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "i2s/i2s.h"
#include "psram_spi.h"
#include "hardware/sync.h"

uint32_t ReadPointer = 0;
uint32_t WritePointer = 0;
float glbFeedback = 0.1;
float glbDivisor = 1;
uint32_t encoder_tick = 0;
uint32_t glbDelay;                  // The actual current Delay (in Samples)
uint32_t targetDelay;               // The target Delay (in samples) - glbDelay will ramp towards this value
int32_t PreviousClockPeriod = 0;    //
uint32_t glbIncrement = 1;          // How many samples the delay is increased or decreased by via the rotary encoder
int spinlock_num_glbDelay;          // used to store the number of the spinlock
spin_lock_t *spinlock_glbDelay;     // Used to lock access to glbDelay
int Algorithm = 0;                  // 4 selectable Algorithms (0 to 3)
int rampcount = 0;
int ThisLeft;
int PreviousLeft;
int ThisRight;
int PreviousRight;  
float glbWet = 0;                   // wet/dry multipliers                   
float glbDry = 1;
/**
 * @brief An array of multipliers, which are applied to the master internal
 * or external clock frequency to set the delay time as a multiple of the clock period
 * 
 * 1/12 1/8 1/6 1/5 1/4 1/3 1/2 1* 2* 3* 4* 5* 6* 8* 12*
 */
float divisors[] = {1,2,3,4,5,6,8,12,1,0.5,0.33333,0.25,0.2,0.166666,0.125,0.083333};

psram_spi_inst_t* async_spi_inst;
psram_spi_inst_t psram_spi;

static __attribute__((aligned(8))) pio_i2s i2s;

// Multi-tap delay element
typedef struct tap_element{
    uint32_t delay;
    float  feedback;
}tap_element;

int get_sign(int32_t value)
{
    return (value & 0x80000000) ? -1 : (int)(value != 0);
}


/**
 * @brief process a buffer of Audio data
 * 
 * Processes Left then Right sample in turn so that,
 * if necessary, different processing can be applied 
 * to each sample.
 * 
 * @param input pointer to the Audio Innput buffer
 * @param output pointer to the Audio Output buffer
 * @param num_frames number of L-R samples
 */
static void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    // Left Sample first
    for (size_t i = 0; i < num_frames * 2; i++) {
        // If the delay changes, gradually ramping the actual delay 
        // towards the target delay helps to reduce glitches. 
        if (glbDelay < targetDelay){
            glbDelay++;
            ReadPointer = ((WritePointer  + BUF_LEN) - (glbDelay*4)) % BUF_LEN;
        }
        if (glbDelay > targetDelay){
            glbDelay--;
            ReadPointer = ((WritePointer  + BUF_LEN) - (glbDelay*4)) % BUF_LEN;
        }
        
        // We need to read from Memory first, so that an amount of that can
        // added to the incoming sample as Feedback
        union uSample ThisSample;
        union uSample DrySample;
        ThisSample.fSample = (float)input[i];
        DrySample.fSample = ThisSample.fSample;
        switch(Algorithm){
            case 0:
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback));
                break;
            case 1:
                //ThisSample.fSample = WetDry(DrySample.fSample,Ping_Pong(ThisSample, glbFeedback, true));
                //ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback));
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample));
                break;
            case 2:
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample));
                break;
            case 3:
                ThisSample.fSample = single_delay(ThisSample);
                break;
            default:
                ThisSample.fSample = single_tap(ThisSample, glbFeedback);
                break;
        }
        output[i] = (int32_t)ThisSample.fSample;
        ReadPointer+=4;
        if(ReadPointer >= BUF_LEN) ReadPointer = 0;
        WritePointer+=4;
        if(WritePointer >= BUF_LEN) WritePointer = 0;
        // Right Channel Sample
        i++;
        ThisSample.fSample = (float)input[i];
        DrySample.fSample = ThisSample.fSample;
        switch(Algorithm){
            case 0:
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback));
                break;
            case 1:
                //ThisSample.fSample = WetDry(DrySample.fSample,Ping_Pong(ThisSample, glbFeedback, true));
                //ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback));
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample));
                break;
            case 2:
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample));
                break;
            case 3:
                ThisSample.fSample = single_delay(ThisSample);
                break;
            default:
                ThisSample.fSample = single_tap(ThisSample, glbFeedback);
                break;
        }
        output[i] = (int32_t)ThisSample.fSample;
        ReadPointer+=4;
        if(ReadPointer >= BUF_LEN) ReadPointer = 0;
        WritePointer+=4;
        if(WritePointer >= BUF_LEN) WritePointer = 0;
    }
}
/**
 * @brief I2S Audio input DMA handler
 * 
 * We're double buffering using chained TCBs. By checking which buffer the
 * DMA is currently reading from, we can identify which buffer it has just
 * finished reading (the completion of which has triggered this interrupt).
 * 
 */
static void dma_i2s_in_handler(void) {
    if (*(int32_t**)dma_hw->ch[i2s.dma_ch_in_ctrl].read_addr == i2s.input_buffer) {
        // It is inputting to the second buffer so we can overwrite the first
        process_audio(i2s.input_buffer, i2s.output_buffer, AUDIO_BUFFER_FRAMES);
    } else {
        // It is currently inputting the first buffer, so we write to the second
        process_audio(&i2s.input_buffer[STEREO_BUFFER_SIZE], &i2s.output_buffer[STEREO_BUFFER_SIZE], AUDIO_BUFFER_FRAMES);
    }
    dma_hw->ints0 = 1u << i2s.dma_ch_in_data;  // clear the IRQ
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
    uint32_t ResetCounter;
    uint32_t InterruptStatus;
    // Mute the output
    gpio_put(XSMT_PIN,0);
    // Save and disable interrupts
    InterruptStatus = save_and_disable_interrupts();
    for(ResetCounter = 0;ResetCounter <= BUF_LEN; ResetCounter +=4){
        psram_write32(&psram_spi, ResetCounter,0x0);
    }
    glbDelay = targetDelay; //be done with it!
    restore_interrupts(InterruptStatus);
    // Un-Mute the output
    gpio_put(XSMT_PIN,1);
  }
}
/**
 * @brief Main program entry point
 */
int main(){
    // Main program entry point
    // Set a 132.000 MHz system clock to more evenly divide the audio frequencies
    //set_sys_clock_khz(132000, true);
    set_sys_clock_khz(280000, true);
    // Serial port initialisation (Using USB for stdio)
    stdio_init_all();
    // Pre-start delay for testing
    //for(int i = 1;i <= 20; i++){
    //  printf("Waiting to start %d\n",20-i);
    //  sleep_ms(1000);
    //}
    
    // Onboard LED
    gpio_init(ONBOARD_LED);
    gpio_set_dir(ONBOARD_LED, GPIO_OUT);
    // External LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    // Setting gpio 23 high turns off power save mode
    // on the internal SMPS, and keeps it in PWM mode, 
    // which improves noise on the 3v3 rail. 
    gpio_init(POWERSAVE_PIN);
    gpio_set_dir(POWERSAVE_PIN,GPIO_OUT);
    gpio_put(POWERSAVE_PIN,1);
    // Algorithm Pins
    gpio_init(ALGORITHM_0);
    gpio_set_dir(ALGORITHM_0,GPIO_IN);
    //gpio_pull_down(ALGORITHM_0);
    gpio_init(ALGORITHM_1);
    gpio_set_dir(ALGORITHM_1,GPIO_IN);
    //gpio_pull_down(ALGORITHM_1);
    // PCM 5102 soft mute
    gpio_init(XSMT_PIN);
    gpio_set_dir(XSMT_PIN,GPIO_OUT);
    gpio_put(XSMT_PIN,0);

    /**
     * @brief Initialise the PSRAM SPI interface, which uses pio0
     */
    psram_spi = psram_spi_init(pio0, -1);
    // Claim and initialize a spinlock
    spinlock_num_glbDelay = spin_lock_claim_unused(true) ;
    spinlock_glbDelay = spin_lock_init(spinlock_num_glbDelay) ;
    // Zero out the Audio Buffer
    for(WritePointer = 0;WritePointer <= BUF_LEN; WritePointer +=4){
        psram_write32(&psram_spi, WritePointer,0x0);
    }
    // Give the Write Pointer a head start & set the 
    // target delays
    ReadPointer = 0;
    WritePointer = 48000;
    targetDelay = 48000;
    glbDelay = 0;
    ExtClockPeriod = 48000;


    /**
     * @brief Start the Audio I2S interface, which uses pio1
     */
    i2s_program_start_synched(pio1, &i2s_config_default, dma_i2s_in_handler, &i2s);
    // Un-mute the output
    gpio_put(XSMT_PIN,1);

    // Launch core 1
    multicore_launch_core1(core1_entry);

    // Do nothing for now, but we need to utilise this core
    // to do something useful
    while(1){
        // check the panic button (encoder switch)
        checkReset();
        tight_loop_contents();
    }

}




