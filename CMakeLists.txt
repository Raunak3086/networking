cmake_minimum_required(VERSION 3.10)
project(reliable_udp_core CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

add_library(reliable_udp_core STATIC
    src/packet.cpp
    src/sha256.cpp
    src/fault_injection.cpp
    src/udp_socket.cpp
    src/stats.cpp
)

target_include_directories(reliable_udp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(WIN32)
    target_link_libraries(reliable_udp_core PUBLIC ws2_32 bcrypt)
endif()

add_executable(sender src/main_sender.cpp src/sender_base.cpp src/sender_saw.cpp src/sender_gbn.cpp src/sender_sr.cpp)
target_link_libraries(sender reliable_udp_core Threads::Threads)

add_executable(receiver src/main_receiver.cpp src/receiver.cpp)
target_link_libraries(receiver reliable_udp_core Threads::Threads)

add_executable(test_packet tests/test_packet.cpp)
target_link_libraries(test_packet reliable_udp_core)

# TCP baseline for benchmarking (Phase 7)
add_executable(tcp_sender scripts/tcp_sender.cpp)
if(WIN32)
    target_link_libraries(tcp_sender ws2_32)
endif()

add_executable(tcp_receiver scripts/tcp_receiver.cpp)
if(WIN32)
    target_link_libraries(tcp_receiver ws2_32)
endif()

# Custom test target
add_custom_target(test
    COMMAND $<TARGET_FILE:test_packet>
    DEPENDS test_packet
    COMMENT "Running unit tests"
)
