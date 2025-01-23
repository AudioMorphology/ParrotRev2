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
 * @file TestAudio.c
 * 
 * Test harness prog to just link Input and Output audio buffers together
 * 
 * Project homepage: https://github.com/audiomorphology/Parrot
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "parrot.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "i2s/i2s.h"

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80000000 ? '1' : '0'), \
  ((byte) & 0x40000000 ? '1' : '0'), \
  ((byte) & 0x20000000 ? '1' : '0'), \
  ((byte) & 0x10000000 ? '1' : '0'), \
  ((byte) & 0x8000000 ? '1' : '0'), \
  ((byte) & 0x4000000 ? '1' : '0'), \
  ((byte) & 0x2000000 ? '1' : '0'), \
  ((byte) & 0x1000000 ? '1' : '0'), \
  ((byte) & 0x800000 ? '1' : '0'), \
  ((byte) & 0x400000 ? '1' : '0'), \
  ((byte) & 0x200000 ? '1' : '0'), \
  ((byte) & 0x100000 ? '1' : '0'), \
  ((byte) & 0x80000 ? '1' : '0'), \
  ((byte) & 0x40000 ? '1' : '0'), \
  ((byte) & 0x20000 ? '1' : '0'), \
  ((byte) & 0x10000 ? '1' : '0'), \
  ((byte) & 0x8000 ? '1' : '0'), \
  ((byte) & 0x4000 ? '1' : '0'), \
  ((byte) & 0x2000 ? '1' : '0'), \
  ((byte) & 0x1000 ? '1' : '0'), \
  ((byte) & 0x800 ? '1' : '0'), \
  ((byte) & 0x400 ? '1' : '0'), \
  ((byte) & 0x200 ? '1' : '0'), \
  ((byte) & 0x100 ? '1' : '0'), \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0') 

static float input_buffer[STEREO_BUFFER_SIZE * 2];
static float output_buffer[STEREO_BUFFER_SIZE * 2];
float *inbuffptr = &input_buffer[0];
float *outbuffptr = &output_buffer[0];
uint32_t PrevTime;

static __attribute__((aligned(8))) pio_i2s i2s;

/**
 * @brief process a buffer of Audio data
 * 
 * For testing just copy the input buffer to the output, though
 * this does go through the float conversion and back again
 */
static void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    /*
    * Convert to Floats and normalise to -1.0 - +1.0f
    */
    for (size_t i = 0; i < num_frames * 2; i++){
        input_buffer[i] = (float)input[i] / (float)(0x7FFFFFFF);
        }
    
    /*
    *   Just Copy input to output
    */
    for (size_t i = 0; i < num_frames * 2; i++) {
        output_buffer[i] = input_buffer[i];
    }
    /*
    * Randomly sample and output something from the buffer
    * every 500ms
    */
    uint32_t ThisTime = time_us_32();
    if (ThisTime - PrevTime > 500000) {
        PrevTime = ThisTime;
        int32_t tmp = (input[8] >> 8);
        //float ftmp = (float)tmp / (float)0x07FFFFF;
        float ftmp = (float)(input[8] >> 8) / (float)0x07FFFFF;
        int32_t outtmp = (int32_t)(ftmp * 0x07FFFFF) << 8;
        printf(BYTE_TO_BINARY_PATTERN", %f, "BYTE_TO_BINARY_PATTERN"\n" ,BYTE_TO_BINARY(tmp), ftmp, BYTE_TO_BINARY(outtmp));
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
 * @brief Main program entry point
 */
int main(){
    // Main program entry point
    // Set a 132.000 MHz system clock to more evenly divide the audio frequencies
    set_sys_clock_khz(132000, true);
    //set_sys_clock_khz(280000, true);
    // Serial port initialisation (Using USB for stdio)
    stdio_init_all();
    for(int i = 1;i <= 20; i++){
      printf("Waiting to start %d\n",20-i);
      sleep_ms(1000);
    }
    printf("**************************************************\n");
    printf("* This tests the Audio I/O Functions by simply   *\n");
    printf("* setting up the PIO functions to receive and    *\n");
    printf("* data to/from I2S input and output CODECS and   *\n");
    printf("* copying the input DMA buffer to the output DMA *\n");
    printf("* if succcessful, all that will happen is that   *\n");
    printf("* audio presented to the input ADC will be copied*\n");
    printf("* verbatim to the putput DAC                     *\n");
    printf("* for testing purposes this samples the input    *\n");
    printf("* signal every 500ms and prints out a binary     *\n");
    printf("* representation of what's arrived, whiich is a  *\n");
    printf("* handy way of checking signed float conversions!*\n");
    printf("**************************************************\n");
    sleep_ms(1000);    
    for(int i=0;i<20;i++){
        printf("\n");
    }

    // Setting gpio 23 high turns off power save mode
    // on the internal SMPS, and keeps it in PWM mode, 
    // which improves noise on the 3v3 rail. 
    gpio_init(POWERSAVE_PIN);
    gpio_set_dir(POWERSAVE_PIN,GPIO_OUT);
    gpio_put(POWERSAVE_PIN,1);
    gpio_init(XSMT_PIN);
    gpio_set_dir(XSMT_PIN,GPIO_OUT);
    gpio_put(XSMT_PIN,0);
    /**
     * @brief Start the Audio I2S interface, which uses pio1
     */
    i2s_program_start_synched(pio1, &i2s_config_default, dma_i2s_in_handler, &i2s);
    // Un-mute the output
    gpio_put(XSMT_PIN,1);
    PrevTime = time_us_32();
    // Do nothing for now, but we need to utilise this core
    // to do something useful
    while(1){
        // check the panic button (encoder switch)
        tight_loop_contents();
    }

}




