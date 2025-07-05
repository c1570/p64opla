cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

project(p64opla C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

#add_subdirectory(main)

#add_definitions(-D...)

add_executable(p64opla
        p64opla.c
)

target_compile_options(p64opla PRIVATE -Wall -save-temps)

#pico_set_binary_type(p64opla no_flash)

target_link_libraries(p64opla
        hardware_pio
        pico_stdlib
        pico_multicore
        pico_util
)

pico_enable_stdio_usb(p64opla 1)
pico_enable_stdio_uart(p64opla 0)

#pico_generate_pio_header(p64opla ${CMAKE_CURRENT_LIST_DIR}/p64opla.pio)

pico_add_extra_outputs(p64opla)
