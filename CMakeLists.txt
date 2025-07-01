# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(websockets_example C CXX ASM)

pico_sdk_init()

add_executable(websockets_example 
               main.c
               src/ap.c
               src/http.c
               dhcpserver/dhcpserver.c 
               dnsserver/dnsserver.c
              )

pico_set_program_name(websockets_example "websockets_example")
pico_set_program_version(websockets_example "0.1")

pico_enable_stdio_uart(websockets_example 0)
pico_enable_stdio_usb(websockets_example 1)

target_link_libraries(websockets_example
        pico_stdlib
        pico_cyw43_driver
        pico_cyw43_arch_lwip_threadsafe_background
        )

target_include_directories(websockets_example PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/includes
        ${CMAKE_CURRENT_LIST_DIR}/routes
        ${CMAKE_CURRENT_LIST_DIR}/configs
)

add_subdirectory(picow_websockets)

target_link_libraries(websockets_example 
        hardware_i2c
        hardware_dma
        hardware_timer
        pico_multicore
        picow_websockets
        hardware_irq
        hardware_watchdog
        hardware_pio
        )

pico_add_extra_outputs(websockets_example)