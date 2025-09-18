cmake_minimum_required(VERSION 3.15)
project(heapster C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(heapster STATIC
    src/arena.c
    src/block.c
    src/heapster.c
    src/policy.c
    src/stats.c
)

target_include_directories(heapster
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

add_executable(heapster_tests
    tests/test_main.c
)

target_link_libraries(heapster_tests PRIVATE heapster)
