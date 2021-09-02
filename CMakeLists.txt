cmake_minimum_required(VERSION 3.6.2)
project(DataProxy C)

include_directories(include)

FILE(GLOB_RECURSE SRC_FILES "src/*/*.c")

add_executable(data_proxy main.c ${SRC_FILES})
target_link_libraries(data_proxy
        pthread
        usb-1.0)
