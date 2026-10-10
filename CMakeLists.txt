cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(wordclock)

target_sources(app PRIVATE
    src/main.c
    src/timeprocess/timeprocess.c
    src/ambiente/ambiente.c
    src/display/display.c
)