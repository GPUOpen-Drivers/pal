project(DD_RAPIDYAML VERSION 0.4.1 LANGUAGES CXX)

add_library(dd_rapidyaml
    STATIC
        src/rapidyaml.cpp
        rapidyaml.natvis
)

target_include_directories(dd_rapidyaml
    PUBLIC
        include
    PRIVATE
        .
)
