cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(wordclock)

target_sources(app PRIVATE
    src/main.c
    src/time_process/time_process.c
    src/time_process/time_frame.c
    src/ambiente/ambiente.c
    src/display/display.c
    src/word_map/word_map.c
)