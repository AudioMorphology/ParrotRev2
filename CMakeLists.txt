# The Camberwell Parrot. Audio Morphology 2024 
#
# CMakeLists File for building the .uf2
# NOTE: Set the board type according to the 
# model of RPi Pico

# Set minimum required version of CMake.
cmake_minimum_required(VERSION 3.13)

# Set the Pico board/processor type
set(PICO_BOARD pico2) 

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# Pull in CMSIS-DSP specifically, as at Jan 2025 v1.16.2
set(CMSISCORE "$ENV{PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core")
set(DISABLEFLOAT16 ON)
include(FetchContent)
FetchContent_Declare(cmsisdsp
   GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
   GIT_TAG "v1.16.2"
)
FetchContent_MakeAvailable(cmsisdsp)

# Set name of project (as PROJECT_NAME) and C/C   standards 
project(parrot C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/rp2040-psram)

# Tell CMake where to find the executable source file
add_executable(${PROJECT_NAME} 
    parrot_main.c
    parrot_core1.c
    ${CMAKE_CURRENT_LIST_DIR}/freeverb/freeverb.c
    ${CMAKE_CURRENT_LIST_DIR}/gverb/gverb.c
    ${CMAKE_CURRENT_LIST_DIR}/gverb/gverbdsp.c
    ${CMAKE_CURRENT_LIST_DIR}/pverb/pverb.c
    ${CMAKE_CURRENT_LIST_DIR}/i2s/i2s.c
    parrot_func.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # PSRAM_MUTEX=1
    PSRAM_SPINLOCK=1
    PSRAM_ASYNC=1
    PSRAM_PIN_CS=16
    PSRAM_PIN_SCK=17
    PSRAM_PIN_MOSI=18
    PSRAM_PIN_MISO=19
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/i2s/i2s.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
# set to use the fast/compact SDK/bootrom floating point implementations
pico_set_float_implementation(${PROJECT_NAME} pico)

# Dependency to ensure that libCMSISDSP.a is built
add_dependencies(${PROJECT_NAME} CMSISDSP)

# Link to pico_stdlib (gpio, time, etc. functions)
target_link_libraries(${PROJECT_NAME} 
    rp2040-psram
    pico_stdlib
    pico_multicore
    pico_float
    hardware_pio
    hardware_dma
    hardware_clocks
    hardware_irq
    hardware_adc
    hardware_sync
    ${cmsisdsp_BINARY_DIR}/Source/libCMSISDSP.a
)

# Add pico and cmsis include paths
target_include_directories(${PROJECT_NAME}  PRIVATE
  ${cmsisdsp_SOURCE_DIR}/Include
  ${PICO_SDK_PATH}/src/rp2_common/cmsis/stub/CMSIS/Core/Include
)

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)
