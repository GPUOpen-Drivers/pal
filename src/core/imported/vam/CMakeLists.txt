##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################
cmake_minimum_required(VERSION 3.9...3.15)

project(VAM VERSION 1.2.0 LANGUAGES CXX)

add_library(vam STATIC)
option(VAM_ENABLE_WERROR "Build with -Werror enabled" OFF)

if (NOT DEFINED VAM_CLIENT_DX12_LINUX)
    set(VAM_CLIENT_DX12_LINUX OFF)
    message(AUTHOR_WARNING "VAM_CLIENT_DX12_LINUX defaulting to ${VAM_CLIENT_DX12_LINUX}")
endif()

target_include_directories(vam
    PUBLIC
        inc
    PRIVATE
        src/core
)

target_sources(vam PRIVATE
    # inc
    inc/vambasictypes.h
    inc/vamtypes.h
    inc/vaminterface.h
    # src
    src/vaminterface.cpp
    # src/core
    src/core/vammacros.h
    src/core/vamlink.h
    src/core/vamtree.h
    src/core/vamcommon.h
    src/core/vamobject.h
    src/core/vamobject.cpp
    src/core/vamrange.h
    src/core/vamrange.cpp
    src/core/vamsuballoc.h
    src/core/vamsuballoc.cpp
    src/core/vamsectionalloc.h
    src/core/vamsectionalloc.cpp
    src/core/vamptb.h
    src/core/vamptb.cpp
    src/core/vamdevice.h
    src/core/vamdevice.cpp
)

target_compile_definitions(vam PRIVATE $<$<CONFIG:Debug>:VAM_DEBUG>)

if (VAM_CLIENT_DX12_LINUX)
    target_compile_definitions(vam PUBLIC VAM_CLIENT_DX12_LINUX)
endif()

set_target_properties(vam PROPERTIES
    CXX_STANDARD              11
    CXX_STANDARD_REQUIRED     ON
    CXX_EXTENSIONS            OFF
    POSITION_INDEPENDENT_CODE ON
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(VAM_ENABLE_WERROR)
        target_compile_options(vam PRIVATE -Werror)
    endif()

    # [GCC] Exceptions
    #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
    #
    # [GCC] Options Controlling C++ Dialect
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/C_002b_002b-Dialect-Options.html
    #
    # [GCC] Options That Control Optimization
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
    target_compile_options(vam PRIVATE
        -fno-exceptions # Disable exception handling support.
        -fno-rtti       # Disable run-time type information support.
        -fcheck-new     # Check if pointer returned by operator new is non-null.
        -fno-math-errno # Single instruction math operations do not set ERRNO.
    )

    # [GCC] Options to Request or Suppress Warnings
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Warning-Options.html
    target_compile_options(vam PRIVATE
        -Wall
        -Wextra
        -Wno-unused
        -Wno-unused-parameter
        -Wno-unused-command-line-argument
        -Wno-ignored-qualifiers
        -Wno-missing-field-initializers
    )

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    target_compile_options(vam PRIVATE
        $<$<CONFIG:Debug>:/analyze>
              # Turns on static analysis for debug build.
        /EHsc # Catches only C++ exceptions and assumes
              # functions declared as extern "C" never throw a C++ exception.
        /GR-  # Disables run-time type information.
        /GS-  # Disables detection of buffer overruns.
    )

    target_compile_options(vam PRIVATE
        /W4      # Enable warning level 4.
        /WX      # Treat warnings as errors.
        /wd4100  # unreferenced formal parameter
        /wd4127  # conditional expression is constant
        /wd4189  # local variable is initialized but not referenced
        /wd4201  # nonstandard extension used : nameless struct/union
        /wd4701  # potentially uninitialized local variable
        /wd4703  # potentially uninitialized local pointer variable
    )

else()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
endif()

# Nice ide support
get_target_property(vam_sources vam SOURCES)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
             FILES ${vam_sources}
)

