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


static __attribute__((aligned(8))) pio_i2s i2s;

/**
 * @brief process a buffer of Audio data
 * 
 * For testing just copy the input buffer to the output
 */
static void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
    // Just copy the input to the output
    for (size_t i = 0; i < num_frames * 2; i++) {
        output[i] = input[i];
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

    // Do nothing for now, but we need to utilise this core
    // to do something useful
    while(1){
        // check the panic button (encoder switch)
        tight_loop_contents();
    }

}




