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
 * @file parrot_main.c
 * 
 * Main program entry point
 * 
 * Project homepage: https://github.com/audiomorphology/ParrotRev2
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
#include "arm_math.h"

// There are separate Read pointers for 
// Left and Riht channels, though only one 
// write pointer, as everythign is written to 
// a single here-and-now position, but read
// accoring to differing delays Left and Right
uint32_t ReadPointer_L = 0;         
uint32_t ReadPointer_R = 0;
uint32_t WritePointer = 0;
const float div = (1.0f/16777215.0f);
const float mul = 16777215.0f;
float glbWet = 0;                   // wet/dry multipliers                   
float glbDry = 1;                   // Between 0 and 1
float glbFeedback = 0.1;            // Global feedback level (between 0 and 1)
float glbRatio = 1.00;              // Global Clock Multiplication / Division ratio
int glbDivisor;                     // Global value for the mul/div switch (0 to 15)
int glbAlgorithm;                   // Global value for Algorithm switch used by other functions
int glbEncoderSw;                   // Global value for Encoder Switch used by other functions
int LatestEncoderSw = 0;            // latest value for the Encoder Switch
uint64_t EncoderSwChangedTime;      // Time at which the Encoder Switch last changed

uint64_t DeBounceTime = 200000;     // Switch Debounce time in uS

uint32_t encoder_tick = 0;

uint32_t glbDelay_L;                // The actual current Delay (in Samples) Left Channel
uint32_t glbDelay_R;                // The actual current Delay (in Samples) Right Channel
uint32_t targetDelay_L;             // The target Delay (in samples) - glbDelay will ramp towards this value
uint32_t targetDelay_R;             // The target Delay (in samples) - glbDelay will ramp towards this value
int32_t PreviousClockPeriod = 0;    //
uint32_t glbIncrement = 1;          // How many samples the delay is increased or decreased by via the rotary encoder
int spinlock_num_glbDelay;          // used to store the number of the spinlock
spin_lock_t *spinlock_glbDelay;     // Used to lock access to glbDelay

/**
 * @brief An array of multipliers, which are applied to the master internal
 * or external clock frequency to set the delay time as a multiple of the clock period
 * 
 * With just an 8-Way switch plus a multiply/divide switch, there are only 15 possible 
 * values, so I've chosen the most common musically-relevant values
 * 
 * 1/12 1/9 1/8 1/6 1/4 1/3 1/2 1* 2* 3* 4* 6* 8* 9* 12*
 */
float divisors[] = {1,2,3,4,6,8,9,12,1,0.5,0.333333,0.25,0.166666,0.125,0.111111,0.083333};

psram_spi_inst_t* async_spi_inst;
psram_spi_inst_t psram_spi;

static __attribute__((aligned(8))) pio_i2s i2s;
static float input_buffer[STEREO_BUFFER_SIZE * 2];
static float output_buffer[STEREO_BUFFER_SIZE * 2];
float *inbuffptr = &input_buffer[0];
float *outbuffptr = &output_buffer[0];

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
 * @param input pointer to the Audio Input buffer
 * @param output pointer to the Audio Output buffer
 * @param num_frames number of L-R samples
 */
static void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    /*
    * Convert to Floats and normalise to -1.0 - +1.0f
    */
    for (size_t i = 0; i < num_frames * 2; i++){
        // Data is 24-Bit left-justified in 32-Bit word
        // so right-shift to get 24-Bits
        //uint32_t tmp = input[i] >> 8;
        // Mask everything except lower 24-Bits
        //tmp &= 0xFFFFFF;
        // check the sign bit
        //if (tmp & 0x800000){tmp |= ~0xFFFFFFF;}
        // normalise to float (-1.0, +1.0)
        //input_buffer[i] = (float)tmp / (float)(0x7FFFFF);
        input_buffer[i] = (float)input[i] / (float)(0x7FFFFFFF);
        }
    
    /*
    *   Left Channel Sample
    */
    for (size_t i = 0; i < num_frames * 2; i++) {
        // If the delay changes, gradually ramping the actual delay 
        // towards the target delay helps to reduce glitches. 
        if (glbDelay_L < targetDelay_L){
            glbDelay_L++;
        }
        if (glbDelay_L > targetDelay_L){
            glbDelay_L--;
        }
        ReadPointer_L = ((WritePointer + BUF_LEN) - glbDelay_L) % BUF_LEN;
        // We need to read from Memory first, so that an amount of that can
        // added to the incoming sample as Feedback
        union uSample ThisSample;
        union uSample DrySample;
        ThisSample.fSample = input_buffer[i];
        DrySample.fSample = ThisSample.fSample;
        switch(glbAlgorithm){
            case 0:
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, true));
                break;
            case 1:
                ThisSample.fSample = WetDry(DrySample.fSample,Ping_Pong(ThisSample, glbFeedback, true));
                break;
            case 2:
                // BUT delay change only affects Left Channel
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, true));
                break;
            case 3:
                // BUT delay change only affects Right Channel
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, true));
                break;
            case 4:
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample, true));
                break;
            case 5:
                ThisSample.fSample = single_delay(ThisSample, true);
                break;
            case 6:
                ThisSample.fSample = single_delay(ThisSample, true);
                break;
            case 7:
                //float tmp = input_buffer[i];
                //if (tmp > 0.99f) tmp = 0.99f;
                //if (tmp < -0.3f) tmp = -0.3f;
                //output_buffer[i] = tmp; 
                output_buffer[i] = input_buffer[i];
                //output_buffer[i] = WaveFolder(input_buffer[i],glbWet);
                break;
            default:
                ThisSample.fSample = single_tap(ThisSample, glbFeedback, true);
                break;
        }
        //output_buffer[i] = ThisSample.fSample;
        //ReadPointer_L = ReadPointer_L++ & BUF_LEN;
        /*
        *   Right Channel Sample
        */
        i++;
        if (glbDelay_R < targetDelay_R){
            glbDelay_R++;
        }
        if (glbDelay_R > targetDelay_R){
            glbDelay_R--;
        }
        ReadPointer_R = ((WritePointer + BUF_LEN) - glbDelay_R) % BUF_LEN;
        ThisSample.fSample = input_buffer[i];
        DrySample.fSample = ThisSample.fSample;
        switch(glbAlgorithm){
            case 0:
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, false));
                break;
            case 1:
                ThisSample.fSample = WetDry(DrySample.fSample,Ping_Pong(ThisSample, glbFeedback, false));
                break;
            case 2:
                // BUT delay change only affects Left Channel
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, false));
                break;
            case 3:
                // BUT delay change only affects Right Channel
                ThisSample.fSample = WetDry(DrySample.fSample,single_tap(ThisSample, glbFeedback, false));
                break;
            case 4:
                ThisSample.fSample = WetDry(DrySample.fSample,single_delay(ThisSample, false));
                break;
            case 5:
                ThisSample.fSample = single_delay(ThisSample, false);
                break;
            case 6:
                ThisSample.fSample = single_delay(ThisSample, false);
                break;
            case 7:
                //float tmp = input_buffer[i];
                //if (tmp > 0.99f) tmp = 0.99f;
                //if (tmp < -0.3f) tmp = -0.3f;
                //output_buffer[i] = tmp; 
                output_buffer[i] = input_buffer[i];
                //output_buffer[i] = WaveFolder(input_buffer[i],glbWet);
                break;
            default:
                ThisSample.fSample = single_tap(ThisSample, glbFeedback, false);
                break;
        }
        //output_buffer[i] = ThisSample.fSample;
        //ReadPointer_R = ReadPointer_R++ & BUF_LEN;
        WritePointer++;
        WritePointer &= BUF_LEN;
    }
    /*
    * Convert back from floats
    */
    for (size_t i = 0; i < num_frames * 2; i++){
        //int32_t tmp = (int32_t)(output_buffer[i] * 0x7FFFFF);
        //output[i] = (int32_t)tmp << 8;
        //output[i] = (int32_t)(output_buffer[i] * (0x7FFFFF));
        output[i] = (int32_t)(output_buffer[i] * 0x7FFFFFFF);
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
  int thisEncoderSw = !gpio_get(ENCODER_SW);
  if ((int)thisEncoderSw != LatestEncoderSw) {
    LatestEncoderSw = thisEncoderSw;
    EncoderSwChangedTime = time_us_64();
  } else {
    if (time_us_64() >= EncoderSwChangedTime + (DeBounceTime/2)){
      // we can trust this value, so set the global if different
      if (glbEncoderSw != (int)thisEncoderSw) {
        //todo poss protect with spinlocks?
        glbEncoderSw = (int)thisEncoderSw;
        if (glbEncoderSw == 1){
            //printf("Encoder Switch Pressed!\n");
            uint32_t ResetCounter;
            uint32_t InterruptStatus;
            // Mute the output
            gpio_put(XSMT_PIN,0);
            // Save and disable interrupts
            InterruptStatus = save_and_disable_interrupts();
            for(ResetCounter = 0;ResetCounter < BUF_LEN; ResetCounter++){
                psram_write32(&psram_spi, ResetCounter << 3,0x0);
                psram_write32(&psram_spi, (ResetCounter << 3) + 4,0x0);
            }
            glbDelay_L = targetDelay_L; //be done with it!
            glbDelay_R = targetDelay_R; //be done with it!
            restore_interrupts(InterruptStatus);
            // Un-Mute the output
            gpio_put(XSMT_PIN,1);
        } 
      }
    }
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
    
    // Onboard LED (just in case we use it)
    gpio_init(ONBOARD_LED);
    gpio_set_dir(ONBOARD_LED, GPIO_OUT);
    // Setting gpio 23 high turns off power save mode
    // on the internal SMPS, and keeps it in PWM mode, 
    // which improves noise on the 3v3 rail. 
    gpio_init(POWERSAVE_PIN);
    gpio_set_dir(POWERSAVE_PIN,GPIO_OUT);
    gpio_put(POWERSAVE_PIN,1);

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
    for(WritePointer = 0;WritePointer < BUF_LEN; WritePointer++){
        psram_write32(&psram_spi, WritePointer << 3,0x0);
        psram_write32(&psram_spi, (WritePointer << 3)+4,0x0);
    }
    // Give the Write Pointer a head start & set the 
    // target delays
    ReadPointer_L = 0;
    ReadPointer_R = 0;
    WritePointer = 0;
    targetDelay_L = 0;
    targetDelay_R = 0;
    glbDelay_L = 0;
    glbDelay_R = 0;
    ExtClockPeriod = 48000;

    glbEncoderSw = 0;
    LatestEncoderSw = 0;
    EncoderSwChangedTime = time_us_64();

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




