# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico CACHE STRING "Board type")

# Copia pico_sdk_import.cmake desde <pico-sdk>/external/ a esta carpeta
# (la extensión de VS Code "Raspberry Pi Pico" ya lo genera).
include(pico_sdk_import.cmake)

project(Version_C_del_patron C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(Version_C_del_patron Version_C_del_patron.c)

target_link_libraries(Version_C_del_patron
    pico_stdlib
    pico_multicore
    hardware_gpio
)

# Serial por USB (equivalente al Serial de Arduino).
# La UART se desactiva porque GPIO0 y GPIO1 (UART0 por defecto) son botones.
pico_enable_stdio_usb(Version_C_del_patron 1)
pico_enable_stdio_uart(Version_C_del_patron 0)

pico_add_extra_outputs(Version_C_del_patron)
