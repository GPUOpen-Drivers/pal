cmake_minimum_required(VERSION 3.10..3.16)
project(cwalk VERSION 1.22 LANGUAGES CXX)

add_library(cwalk
    STATIC
        include/cwalk.h
        src/cwalk.cpp
)

target_include_directories(cwalk PUBLIC include)
