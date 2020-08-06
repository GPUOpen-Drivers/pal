cmake_minimum_required(VERSION 3.5)
project(TinyPrintf VERSION 1.9.1 LANGUAGES CXX)

add_library(tiny_printf
    STATIC
        include/tiny_printf/tiny_printf.h
        tiny_printf.cpp
)

target_include_directories(tiny_printf PUBLIC include)
