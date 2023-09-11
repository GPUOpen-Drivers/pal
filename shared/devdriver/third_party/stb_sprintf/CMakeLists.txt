### Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved. ###

add_library(stb_sprintf STATIC)

target_sources(stb_sprintf
    PRIVATE
        inc/stb_sprintf.h
        src/stb_sprintf.cpp)

target_include_directories(stb_sprintf PUBLIC inc)
