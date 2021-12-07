project(gtest VERSION 1.0.0 LANGUAGES CXX)

add_library(gtest
    STATIC
        src/gtest-all.cpp
)

target_include_directories(gtest PUBLIC SYSTEM include)
target_include_directories(gtest PRIVATE       .)
