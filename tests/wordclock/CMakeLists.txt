cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(wordclock_tests)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../src
)

target_sources(app PRIVATE
    src/main.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/timeprocess/time_frame.c
    ${CMAKE_CURRENT_LIST_DIR}/../../src/word_map/word_map.c
)
