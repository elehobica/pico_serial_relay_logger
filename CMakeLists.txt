cmake_minimum_required(VERSION 3.13)

# define "w" board to use cyw43_arch_xxx() function
if(PICO_BOARD)
    unset(PICO_BOARD)
endif()
if(PICO_PLATFORM STREQUAL rp2350)
    set(PICO_BOARD pico2_w)
else()
    set(PICO_BOARD pico_w)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

set(project_name "pico_serial_logger" C CXX ASM)
project(${project_name})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(lib/pico_fatfs pico_fatfs)
add_subdirectory(lib/pico_flash_param pico_flash_param)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/uart_port.cpp
    src/relay.cpp
    src/sd_logger.cpp
    src/stdio_cdc.cpp
    src/usb/usb_descriptors.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/src/usb
)

target_link_libraries(${PROJECT_NAME}
    hardware_adc
    hardware_uart
    pico_cyw43_arch_none
    pico_fatfs
    pico_flash_param
    pico_stdlib
    pico_unique_id
    tinyusb_device
)

# stdio is served by the application-owned CDC console (src/stdio_cdc.cpp);
# UART0 (GP0/GP1) is the relayed port and must not be taken by stdio
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME}) # create map/bin/hex/uf2 files
