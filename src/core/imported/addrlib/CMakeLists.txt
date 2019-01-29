
##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.1)

project(AddrLib VERSION 4.4.0 LANGUAGES CXX)

include(CMakeDependentOption)
include(TestBigEndian)

function(get_cpu_endianness CPU_ENDIANNESS)
    test_big_endian(isBigEndianCpu)

    if(isBigEndianCpu)
        set(${CPU_ENDIANNESS}    "BIGENDIAN_CPU" PARENT_SCOPE)
    else()
        set(${CPU_ENDIANNESS} "LITTLEENDIAN_CPU" PARENT_SCOPE)
    endif()
endfunction()

                option(ADDR_R800_BUILD       "Build support for R800?"       OFF                                               )
                option(ADDR_SI_BUILD         "Build support for SI?"         OFF                                               )
cmake_dependent_option(ADDR_CI_BUILD         "Build support for CI?"         ON  "ADDR_SI_BUILD"                            OFF)
                option(ADDR_GFX9_BUILD       "Build support for GFX9?"       OFF                                               )
cmake_dependent_option(ADDR_VEGA20_BUILD     "Build support for Vega 20?"    ON  "ADDR_GFX9_BUILD"                          OFF)
cmake_dependent_option(ADDR_RAVEN1_BUILD     "Build support for Raven 1?"    ON  "ADDR_GFX9_BUILD"                          OFF)
cmake_dependent_option(ADDR_VEGA12_BUILD     "Build support for Vega 12?"    ON  "ADDR_GFX9_BUILD"                          OFF)
cmake_dependent_option(ADDR_RAVEN2_BUILD     "Build support for Raven 2?"    ON  "ADDR_RAVEN1_BUILD"                        OFF)

                option(ADDR_AM_BUILD         "Build support for fmask addressing and addr5Swizzle?" OFF                        )
                option(ADDR_LNX_KERNEL_BUILD "Linux kernel build?"                                  OFF                        )

add_library(addrlib STATIC "")

target_include_directories(addrlib PUBLIC  inc
                                   PRIVATE src
                                           src/core)

target_sources(addrlib PRIVATE inc/addrtypes.h
                               inc/addrinterface.h)

target_sources(addrlib PRIVATE src/addrinterface.cpp)

target_sources(addrlib PRIVATE src/core/addrcommon.h
                               src/core/addrobject.h
                               src/core/addrobject.cpp
                               src/core/addrelemlib.h
                               src/core/addrelemlib.cpp
                               src/core/addrlib.h
                               src/core/addrlib.cpp
                               src/core/addrlib1.h
                               src/core/addrlib1.cpp
                               src/core/addrlib2.h
                               src/core/addrlib2.cpp
                               src/core/coord.h
                               src/core/coord.cpp)

if(ADDR_R800_BUILD)
    target_sources(addrlib PRIVATE src/chip/r800/r800_gb_reg.h
                                   src/r800/r800addrlib.h
                                   src/r800/r800addrlib.cpp)

    target_compile_definitions(addrlib PRIVATE ADDR_R800_BUILD)
endif()

if(ADDR_SI_BUILD)
    target_sources(addrlib PRIVATE src/chip/r800/si_gb_reg.h
                                   src/r800/siaddrlib.h
                                   src/r800/siaddrlib.cpp)

    target_compile_definitions(addrlib PRIVATE ADDR_SI_BUILD)
endif()

if(ADDR_CI_BUILD)
    target_sources(addrlib PRIVATE src/r800/ciaddrlib.h
                                   src/r800/ciaddrlib.cpp)

    target_compile_definitions(addrlib PRIVATE ADDR_CI_BUILD)
endif()

if(ADDR_R800_BUILD OR ADDR_SI_BUILD OR ADDR_CI_BUILD)
    target_sources(addrlib PRIVATE src/r800/egbaddrlib.h
                                   src/r800/egbaddrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/r800)
endif()

if(ADDR_GFX9_BUILD)
    target_sources(addrlib PRIVATE src/chip/gfx9/gfx9_gb_reg.h
                                   src/gfx9/gfx9addrlib.h
                                   src/gfx9/gfx9addrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/gfx9)

    target_compile_definitions(addrlib PRIVATE ADDR_GFX9_BUILD)

    if(ADDR_VEGA12_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_VEGA12_BUILD)
    endif()
    if(ADDR_RAVEN1_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_RAVEN1_BUILD)
    endif()

    if(ADDR_VEGA20_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_VEGA20_BUILD)
    endif()
    if(ADDR_RAVEN2_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_RAVEN2_BUILD)
    endif()
endif()

    target_sources(addrlib PRIVATE src/amdgpu_asic_addr.h)

if(ADDR_AM_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_AM_BUILD)
endif()

if(ADDR_LNX_KERNEL_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_LNX_KERNEL_BUILD)
endif()

get_cpu_endianness(CPU_ENDIANNESS)
target_compile_definitions(addrlib PRIVATE ${CPU_ENDIANNESS})

target_compile_definitions(addrlib PRIVATE $<$<CONFIG:Debug>:DEBUG>)

set_target_properties(addrlib PROPERTIES CXX_STANDARD              11
                                         CXX_STANDARD_REQUIRED     ON
                                         CXX_EXTENSIONS            OFF
                                         POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

    # [GCC] Options That Control Optimization
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
    if(CMAKE_BUILD_TYPE_RELEASE)
        execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
            target_compile_options(addrlib PRIVATE $<$<CONFIG:Release>:-flto  -fuse-linker-plugin -Wno-odr>)
            message(WARNING "LTO enabled for address lib")
        endif()
    endif()

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

    # [Clang] Command Line Argument Reference
    #    https://clang.llvm.org/docs/ClangCommandLineReference.html
    #
    # [Clang] Thin LTO
    #    https://clang.llvm.org/docs/ThinLTO.html
    target_compile_options(addrlib PRIVATE -flto=thin)

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    # [MSVC] Whole Program Optimization
    #    https://msdn.microsoft.com/en-us/library/0zza0de8.aspx
    target_compile_options(addrlib PRIVATE /GL)

    # [MSVC] Link-time Code Generation
    #    https://msdn.microsoft.com/en-us/library/xbf3tbeh.aspx
    set_target_properties(addrlib PROPERTIES STATIC_LIBRARY_FLAGS /LTCG)

else()
    message(FATAL_ERROR "IPO is not supported!")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")

    # [GCC] Exceptions
    #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
    #
    # [GCC] Options Controlling C++ Dialect
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/C_002b_002b-Dialect-Options.html
    #
    # [GCC] Options That Control Optimization
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
    target_compile_options(addrlib PRIVATE
        -fno-exceptions  # Disable exception handling support.
        -fno-rtti        # Disable run-time type information support.
        -fcheck-new      # Check if pointer returned by operator new is non-null.
        -fno-math-errno) # Single instruction math operations do not set ERRNO.

    # [GCC] Options to Request or Suppress Warnings
    #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Warning-Options.html
    target_compile_options(addrlib PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wno-unused
        -Wno-unused-parameter
        -Wno-ignored-qualifiers
        -Wno-missing-field-initializers
        -Wno-self-assign
        -Wno-implicit-fallthrough)

elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

    # [MSVC] Code Analysis
    #    https://msdn.microsoft.com/en-us/library/ms173498.aspx
    #
    # [MSVC] Exception Handling Model
    #    https://msdn.microsoft.com/en-us/library/1deeycx5.aspx
    #
    # [MSVC] Enable Run-Time Type Information
    #    https://msdn.microsoft.com/en-us/library/we6hfdy0.aspx
    #
    # [MSVC] Buffer Security Check
    #    https://msdn.microsoft.com/en-us/library/8dbf701c.aspx
    #
    # [MSVC] Whole Program Optimization
    #    https://msdn.microsoft.com/en-us/library/0zza0de8.aspx
    target_compile_options(addrlib PRIVATE
        $<$<CONFIG:Debug>:/analyze>
              # Turns on static analysis for debug build.
        /EHsc # Catches only C++ exceptions and assumes
              # functions declared as extern "C" never throw a C++ exception.
        /GR-  # Disables run-time type information.
        /GS-) # Disables detection of buffer overruns.

    # [MSVC] Warning Level
    #    https://msdn.microsoft.com/en-us/library/thxezb7y.aspx
    #
    # [MSVC] Compiler Warnings
    #    https://msdn.microsoft.com/en-us/library/ysb0wexw.aspx
    #    https://msdn.microsoft.com/en-us/library/t460hcc3.aspx
    #    https://msdn.microsoft.com/en-us/library/cfahxw6k.aspx
    #
    # [MSVC] Code Analysis for C/C++ Warnings
    #    https://msdn.microsoft.com/en-us/library/9t02bbsx.aspx
    target_compile_options(addrlib PRIVATE
        /W4      # Enable warning level 4.
        /WX      # Treat warnings as errors.
        /wd4018  # signed/unsigned mismatch
        /wd4065  # switch statement contains 'default' but no 'case' labels
        /wd4100  # unreferenced formal parameter
        /wd4127  # conditional expression is constant
        /wd4189  # local variable is initialized but not referenced
        /wd4201  # nonstandard extension used : nameless struct/union
        /wd4244  # conversion from 'type1' to 'type2', possible loss of data
        /wd4701  # potentially uninitialized local variable
        /wd6297) # arithmetic overflow: 32-bit value is shifted, then cast to 64-bit value

else()
    message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
endif()
