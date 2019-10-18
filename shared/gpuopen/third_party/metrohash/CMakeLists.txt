##
 #######################################################################################################################
 #
 #  Modifications Copyright© 2019 Advanced Micro Devices, Inc. All rights reserved.
 #
 #######################################################################################################################

cmake_minimum_required(VERSION 3.5)
project(MetroHash VERSION 1.0.0 LANGUAGES CXX)

add_library(metrohash STATIC)

target_include_directories(metrohash PUBLIC SYSTEM src)

target_sources(metrohash PRIVATE src/metrohash.h
                                 src/metrohash64.h
                                 src/metrohash64.cpp
                                 src/platform.h)

set_target_properties(metrohash PROPERTIES CXX_STANDARD          98
                                           CXX_STANDARD_REQUIRED ON
                                           CXX_EXTENSIONS        OFF)
if (WIN32)

    # MSVC 2017's headers expect C++14. Earlier versions of the standard (e.g. with Clang) will not compile.
    set_target_properties(metrohash PROPERTIES CXX_STANDARD 14)

endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    # [GCC] x86 Options
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/x86-Options.html
    target_compile_options(metrohash PRIVATE
        -msse4.2) # Enable the use of SSE4.2 instructions.

endif()
