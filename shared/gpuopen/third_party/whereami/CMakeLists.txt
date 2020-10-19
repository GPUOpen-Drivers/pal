cmake_minimum_required(VERSION 3.10..3.16)
project(whereami VERSION 1.9.1 LANGUAGES CXX)

add_library(whereami
    STATIC
        src/whereami.h
        src/whereami.cpp
)

target_include_directories(whereami PUBLIC src)
