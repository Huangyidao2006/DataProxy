cmake_minimum_required(VERSION 3.6.2)
project(DataProxy C)

include_directories(include)

FILE(GLOB_RECURSE SRC_FILES "src/*/*.c")

add_executable(udp_demo udp_demo.c ${SRC_FILES})
target_link_libraries(udp_demo
        pthread
        usb-1.0)

add_executable(tcp_server_demo tcp_server_demo.c ${SRC_FILES})
target_link_libraries(tcp_server_demo
        pthread
        usb-1.0)

add_executable(tcp_client_demo tcp_client_demo.c ${SRC_FILES})
target_link_libraries(tcp_client_demo
        pthread
        usb-1.0)

add_executable(usb_demo usb_demo.c ${SRC_FILES})
target_link_libraries(usb_demo
        pthread
        usb-1.0)

add_executable(queue_demo queue_demo.c ${SRC_FILES})
target_link_libraries(queue_demo
        pthread
        usb-1.0)
