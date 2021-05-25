cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

set(projname "pico_estlcam_handrad")

project(${projname} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${projname}
    main.c
    rotaryencoder.c
    buttondebounce.c
)


pico_enable_stdio_uart(${projname} 1)
pico_add_extra_outputs(${projname})

target_link_libraries(${projname} pico_stdlib hardware_i2c hardware_irq hardware_adc hardware_dma)