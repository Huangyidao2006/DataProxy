cmake_minimum_required(VERSION 3.6.2)
project(DataProxy)

set(CMAKE_CXX_STANDARD 11)

include_directories(include
        3rdparty/protobuf-3.14.x/src)

FILE(GLOB_RECURSE SRC_FILES "src/*/*.c" "src/*/*.cpp" "src/*/*.cc")
LIST(FILTER SRC_FILES EXCLUDE REGEX "proxy.pb.cc")

aux_source_directory(src/proto PROTO_SRC_FILES)

add_subdirectory(3rdparty/protobuf-3.14.x/cmake)

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

add_executable(usb_demo usb_demo.cpp ${SRC_FILES} ${PROTO_SRC_FILES})
target_link_libraries(usb_demo
        libprotobuf-lite
        pthread
        usb-1.0)

add_executable(queue_demo queue_demo.c ${SRC_FILES})
target_link_libraries(queue_demo
        pthread
        usb-1.0)
