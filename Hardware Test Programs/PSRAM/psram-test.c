#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>
#include "hardware/clocks.h"
#include "psram_spi.h"

psram_spi_inst_t* async_spi_inst;

int main()
{
    // Overclock!
    set_sys_clock_khz(280000, true);
    // This from I2S...
    //set_sys_clock_khz(132000, true);
    stdio_init_all();
    for(int i=0;i<20;i++){
        printf("waiting to start : %d\n",i-20);
        sleep_ms(1000);
    }
    
    printf("**************************************************\n");
    printf("* This tests the PSRAM memory, by writing then   *\n");
    printf("* reading back sequences of 16-Bit, 32-Bit and   *\n");
    printf("* 128-Bit Numbers. This was originally written   *\n");
    printf("* Ian Scott a.k.a. Polpo, who wrote this header- *\n");
    printf("* only PPSRAM Library for the rp2040, which uses *\n");
    printf("* the PIO in order to achieve superior read and  *\n");
    printf("* write speeds compared to the Pico default SPI) *\n");
    printf("* implementation )                               *\n");
    printf("**************************************************\n");
    sleep_ms(1000);    
    for(int i=0;i<20;i++){
        printf("\n");
    }
    printf("PSRAM test - rp2040-psram v1.0.0\n");
    printf("Initing PSRAM...\n");
    psram_spi_inst_t psram_spi = psram_spi_init(pio0, -1);

    uint32_t psram_begin, psram_elapsed;
    float psram_speed;

    printf("Testing PSRAM...\n");

    // **************** 32 bits testing ****************
    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 4) {
        psram_write32(
            &psram_spi, addr,
            (uint32_t)(
                (((addr + 3) & 0xFF) << 24) |
                (((addr + 2) & 0xFF) << 16) |
                (((addr + 1) & 0xFF) << 8)  |
                (addr & 0XFF))
        );
    }
    psram_elapsed = time_us_32() - psram_begin;
    psram_speed = 1000000.0 * 8 * 1024.0 * 1024 / psram_elapsed;
    printf("32 bit: PSRAM write 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

    psram_begin = time_us_32();
    for (uint32_t addr = 0; addr < (8 * 1024 * 1024); addr += 4) {
        uint32_t result = psram_read32(&psram_spi, addr);
        if ((uint32_t)(
            (((addr + 3) & 0xFF) << 24) |
            (((addr + 2) & 0xFF) << 16) |
            (((addr + 1) & 0xFF) << 8)  |
            (addr & 0XFF)) != result
        ) {
            printf("PSRAM failure at address %x (%x != %x) ", addr, (
                (((addr + 3) & 0xFF) << 24) |
                (((addr + 2) & 0xFF) << 16) |
                (((addr + 1) & 0xFF) << 8)  |
                (addr & 0XFF)), result
            );
            return 1;
        }
    }
    psram_elapsed = (time_us_32() - psram_begin);
    psram_speed = 1000000.0 * 8 * 1024 * 1024 / psram_elapsed;
    printf("32 bit: PSRAM read 8MB in %d us, %d B/s\n", psram_elapsed, (uint32_t)psram_speed);

    sleep_ms(1000);
    printf("All tests completed\n");
    sleep_ms(1000);
}
