# Set minimum required version of CMake.
cmake_minimum_required(VERSION 3.13)

# Set the Pico board/processor type
set(PICO_BOARD pico2) 

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Set name of project (as PROJECT_NAME) and C/C   standards 
project(TestAudio C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()


# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    TestAudio.c
    ${CMAKE_CURRENT_LIST_DIR}/i2s/i2s.c
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/i2s/i2s.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
#pico_set_float_implementation(${PROJECT_NAME} NAME)

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_irq
    hardware_adc
    hardware_sync
)

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
