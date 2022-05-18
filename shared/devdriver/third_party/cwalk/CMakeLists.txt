project(cwalk VERSION 1.22 LANGUAGES CXX)

add_library(cwalk
    STATIC
        include/cwalk.h
        src/cwalk.cpp
)

target_include_directories(cwalk PUBLIC include)
