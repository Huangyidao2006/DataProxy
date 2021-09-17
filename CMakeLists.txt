cmake_minimum_required(VERSION 3.6.2)
project(DataProxy)

set(CMAKE_CXX_STANDARD 11)

include_directories(include
        3rdparty/protobuf-3.14.x/src)

FILE(GLOB_RECURSE SRC_FILES "src/*/*.c" "src/*/*.cpp" "src/*/*.cc")
LIST(FILTER SRC_FILES EXCLUDE REGEX "proxy.pb.cc")

aux_source_directory(src/proto PROTO_SRC_FILES)

add_subdirectory(3rdparty/protobuf-3.14.x/cmake)

add_custom_command(OUTPUT abc
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libusb/autogen.sh
        COMMAND make -j8)

add_custom_target(build_libusb
        DEPENDS abc)

add_library(prebuilt-usb-1.0 STATIC IMPORTED)
set_target_properties(prebuilt-usb-1.0 PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libusb/.libs/libusb-1.0.a)
target_include_directories(prebuilt-usb-1.0 INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libusb)
target_link_libraries(prebuilt-usb-1.0 INTERFACE
        udev)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libusb/.libs/libusb-1.0.a")
    add_dependencies(prebuilt-usb-1.0 build_libusb)
endif()

add_executable(udp_demo udp_demo.c ${SRC_FILES})
target_link_libraries(udp_demo
        pthread
        prebuilt-usb-1.0)

add_executable(tcp_server_demo tcp_server_demo.c ${SRC_FILES})
target_link_libraries(tcp_server_demo
        pthread
        prebuilt-usb-1.0)

add_executable(tcp_client_demo tcp_client_demo.c ${SRC_FILES})
target_link_libraries(tcp_client_demo
        pthread
        prebuilt-usb-1.0)

add_executable(usb_demo usb_demo.cpp ${SRC_FILES} ${PROTO_SRC_FILES})
target_link_libraries(usb_demo
        libprotobuf-lite
        pthread
        prebuilt-usb-1.0)

add_executable(queue_demo queue_demo.c ${SRC_FILES})
target_link_libraries(queue_demo
        pthread
        prebuilt-usb-1.0)

add_executable(handler_demo handler_demo.cpp ${SRC_FILES})
target_link_libraries(handler_demo
        pthread
        prebuilt-usb-1.0)
