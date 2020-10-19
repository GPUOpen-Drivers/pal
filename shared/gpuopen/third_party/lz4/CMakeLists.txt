cmake_minimum_required(VERSION 3.10..3.16)
project(lz4 VERSION 1.9.1 LANGUAGES CXX)

add_library(xxhash
    STATIC
        xxhash.h
        xxhash.c
)
# Compile as C++
set_target_properties(xxhash PROPERTIES LINKER_LANGUAGE CXX)

add_library(lz4
    STATIC
        lz4.h
        lz4hc.h
        lz4frame.h
        lz4_all.c
)
# Compile as C++
set_target_properties(lz4 PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(lz4 PUBLIC xxhash)

target_compile_definitions(lz4 PUBLIC LZ4_DISABLE_DEPRECATE_WARNINGS)
