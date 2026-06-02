cmake_minimum_required(VERSION 3.15)
project(URLScanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where you extracted the Npcap SDK
set(NPCAP_SDK_DIR "C:/npcap-sdk")

add_executable(scanner
    src/main.cpp
    src/heuristic.hpp
)

# Include Npcap headers and libraries
target_include_directories(scanner PRIVATE src "${NPCAP_SDK_DIR}/Include")
target_link_directories(scanner PRIVATE "${NPCAP_SDK_DIR}/Lib/x64")
target_link_libraries(scanner PRIVATE wpcap Packet)

if(MSVC)
    target_compile_options(scanner PRIVATE /W4)
else()
    target_compile_options(scanner PRIVATE -Wall -Wextra)
endif()