cmake_minimum_required(VERSION 3.20)
project(Calculator LANGUAGES CXX)

# Require a 64-bit build
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This project must be built for x64 (64-bit).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Win32 GUI application (no console window)
add_executable(calculator WIN32
    src/calculator.cpp
)

target_compile_definitions(calculator PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(calculator PRIVATE
    comctl32
)

# Place the binary in a predictable location
set_target_properties(calculator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS calculator DESTINATION bin)
