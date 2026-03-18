cmake_minimum_required(VERSION 3.20)
project(FreezeCamVirtualCamera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(FreezeCamVirtualCamera SHARED
    dllmain.cpp
    class_factory.cpp
    class_factory.h
    virtual_camera_filter.cpp
    virtual_camera_filter.h
    virtual_camera_pin.cpp
    virtual_camera_pin.h
    guids.h
    ../shared/shared_memory.h
    exports.def
)

target_include_directories(FreezeCamVirtualCamera PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared
)

target_link_libraries(FreezeCamVirtualCamera PRIVATE
    strmiids
    ole32
    oleaut32
    uuid
    advapi32
    winmm
)

target_compile_definitions(FreezeCamVirtualCamera PRIVATE
    WIN32
    _WINDOWS
    _USRDLL
    UNICODE
    _UNICODE
)

set_target_properties(FreezeCamVirtualCamera PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)
