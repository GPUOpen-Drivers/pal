project(whereami VERSION 1.9.1 LANGUAGES CXX)

add_library(whereami
    STATIC
        src/whereami.h
        src/whereami.cpp
)

target_include_directories(whereami PUBLIC src)
