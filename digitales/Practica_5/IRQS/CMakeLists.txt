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

set(PICO_BOARD pico)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Copiar pico_sdk_import.cmake desde pico-sdk/external/ a esta carpeta
include(pico_sdk_import.cmake)

project(lab05_irq C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(lab05_irq
    main.c
    hw.c
    meas.c
    fmt.c
    uart_tx.c
    report.c
    app_irq.c
    sig_gen.c)

target_include_directories(lab05_irq PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(lab05_irq PRIVATE -Wall -Wextra)
target_link_libraries(lab05_irq PRIVATE
    pico_stdlib
    hardware_gpio
    hardware_uart
    hardware_timer
    hardware_irq
    hardware_sync
    hardware_pwm)

pico_enable_stdio_usb(lab05_irq 0)
pico_enable_stdio_uart(lab05_irq 0)

pico_add_extra_outputs(lab05_irq)
