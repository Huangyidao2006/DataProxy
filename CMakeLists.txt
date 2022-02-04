cmake_minimum_required(VERSION 3.6.2)
project(DataProxy)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DHAVE_STPNCPY)

include_directories(include
        3rdparty/protobuf-3.14.x/src
        3rdparty/libusbmuxd/include)

file(GLOB_RECURSE SRC_FILES "src/*/*.c" "src/*/*.cpp" "src/*/*.cc")
list(FILTER SRC_FILES EXCLUDE REGEX "proxy.pb.cc")

aux_source_directory(src/proto PROTO_SRC_FILES)

add_subdirectory(3rdparty/protobuf-3.14.x/cmake)

add_custom_command(OUTPUT aaa
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libusb/autogen.sh
        COMMAND make -j8)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/libimobiledevice)
add_custom_command(OUTPUT bbb
        WORKING_DIRECTORY libimobiledevice
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libimobiledevice/autogen.sh
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libimobiledevice/configure --without-cython
        COMMAND make -j8)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/libimobiledevice-glue)
add_custom_command(OUTPUT glue
        WORKING_DIRECTORY libimobiledevice-glue
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libimobiledevice-glue/autogen.sh
        COMMAND make -j8)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/libusbmuxd)
add_custom_command(OUTPUT ccc
        WORKING_DIRECTORY libusbmuxd
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libusbmuxd/autogen.sh
        COMMAND make -j8)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/libplist)
add_custom_command(OUTPUT ddd
        WORKING_DIRECTORY libplist
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libplist/autogen.sh
        COMMAND bash ${CMAKE_SOURCE_DIR}/3rdparty/libplist/configure --without-cython
        COMMAND make -j8)

add_custom_target(build_libusb
        DEPENDS aaa)

add_custom_target(build_libimobiledevice
        DEPENDS bbb)

add_custom_target(build_libimobiledevice-glue
        DEPENDS glue)

add_custom_target(build_libusbmuxd
        DEPENDS ccc)

add_custom_target(build_libplist
        DEPENDS ddd)

add_library(prebuilt-usb-1.0 STATIC IMPORTED)
set_target_properties(prebuilt-usb-1.0 PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libusb/.libs/libusb-1.0.a)
target_include_directories(prebuilt-usb-1.0 INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libusb)
target_link_libraries(prebuilt-usb-1.0 INTERFACE
        udev)

add_library(prebuilt-imobiledevice SHARED IMPORTED)
set_target_properties(prebuilt-imobiledevice PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libimobiledevice/src/.libs/libimobiledevice-1.0.so)
target_include_directories(prebuilt-imobiledevice INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libimobiledevice)

add_library(prebuilt-imobiledevice-glue SHARED IMPORTED)
set_target_properties(prebuilt-imobiledevice-glue PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libimobiledevice-glue/src/.libs/libimobiledevice-glue-1.0.so)
target_include_directories(prebuilt-imobiledevice-glue INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libimobiledevice-glue)

add_library(prebuilt-libusbmuxd STATIC IMPORTED)
set_target_properties(prebuilt-libusbmuxd PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libusbmuxd/src/.libs/libusbmuxd-2.0.a)
target_include_directories(prebuilt-libusbmuxd INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libusbmuxd)

add_library(prebuilt-libplist STATIC IMPORTED)
set_target_properties(prebuilt-libplist PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/libplist/src/.libs/libplist-2.0.a)
target_include_directories(prebuilt-libplist INTERFACE
        ${CMAKE_SOURCE_DIR}/3rdparty/libplist)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libusb/.libs/libusb-1.0.a")
    add_dependencies(prebuilt-usb-1.0 build_libusb)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libimobiledevice/src/.libs/libimobiledevice-1.0.so")
    add_dependencies(prebuilt-imobiledevice build_libimobiledevice)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libimobiledevice-glue/src/.libs/libimobiledevice-glue-1.0.so")
    add_dependencies(prebuilt-imobiledevice-glue build_libimobiledevice-glue)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libusbmuxd/src/.libs/libusbmuxd-2.0.a")
    add_dependencies(prebuilt-libusbmuxd build_libusbmuxd)
endif()

if(NOT EXISTS "${CMAKE_BINARY_DIR}/libplist/src/.libs/libplist-2.0.a")
    add_dependencies(prebuilt-libplist build_libplist)
endif()

set(UDP_SRC_FILES "${SRC_FILES}")
list(FILTER UDP_SRC_FILES EXCLUDE REGEX "usb/")
list(FILTER UDP_SRC_FILES EXCLUDE REGEX "usbmuxd/")

add_executable(udp_demo udp_demo.c ${UDP_SRC_FILES})
target_link_libraries(udp_demo
        pthread
        prebuilt-usb-1.0)

set(TCP_SERVER_SRC_FILES "${SRC_FILES}")
list(FILTER TCP_SERVER_SRC_FILES EXCLUDE REGEX "usb/")
list(FILTER TCP_SERVER_SRC_FILES EXCLUDE REGEX "usbmuxd/")
add_executable(tcp_server_demo tcp_server_demo.c ${TCP_SERVER_SRC_FILES})
target_link_libraries(tcp_server_demo
        pthread
        prebuilt-usb-1.0)

set(TCP_CLIENT_SRC_FILES "${SRC_FILES}")
list(FILTER TCP_CLIENT_SRC_FILES EXCLUDE REGEX "usb/")
list(FILTER TCP_CLIENT_SRC_FILES EXCLUDE REGEX "usbmuxd/")
add_executable(tcp_client_demo tcp_client_demo.c ${TCP_CLIENT_SRC_FILES})
target_link_libraries(tcp_client_demo
        pthread
        prebuilt-usb-1.0)

add_executable(usb_demo usb_demo.cpp ${SRC_FILES} ${PROTO_SRC_FILES})
target_link_libraries(usb_demo
        libprotobuf-lite
        pthread
        prebuilt-usb-1.0
        prebuilt-libusbmuxd
        prebuilt-imobiledevice-glue
        prebuilt-imobiledevice
        prebuilt-libplist)

set(QUEUE_DEMO_SRC_FILES "${SRC_FILES}")
list(FILTER QUEUE_DEMO_SRC_FILES EXCLUDE REGEX "usb/")
list(FILTER QUEUE_DEMO_SRC_FILES EXCLUDE REGEX "usbmuxd/")
add_executable(queue_demo queue_demo.c ${QUEUE_DEMO_SRC_FILES})
target_link_libraries(queue_demo
        pthread
        prebuilt-usb-1.0)

set(HANDLER_DEMO_SRC_FILES "${SRC_FILES}")
list(FILTER HANDLER_DEMO_SRC_FILES EXCLUDE REGEX "usb/")
list(FILTER HANDLER_DEMO_SRC_FILES EXCLUDE REGEX "usbmuxd/")
add_executable(handler_demo handler_demo.cpp ${HANDLER_DEMO_SRC_FILES})
target_link_libraries(handler_demo
        pthread
        prebuilt-usb-1.0)
