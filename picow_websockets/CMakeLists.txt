project(picow_websockets C)

add_library(picow_websockets STATIC
    b64.h
    encrypt.h
    tenysha1.h
    websocket.h
    packet_ops.c
    websocket.c
)

target_include_directories(picow_websockets
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../configs
)

target_link_libraries(picow_websockets
    PUBLIC
        pico_stdlib
        pico_cyw43_arch_lwip_threadsafe_background
)