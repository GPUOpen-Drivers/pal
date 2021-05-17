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
cmake_minimum_required(VERSION 3.13...3.15)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(ADDRLIB_IS_TOP_LEVEL OFF)
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    set(ADDRLIB_IS_TOP_LEVEL ON)
endif()

project(AddrLib
    VERSION 4.4.0
    LANGUAGES CXX
)

add_library(addrlib STATIC)
include(AddrlibCompilerOptions)
include(AddrlibBuildParameters)
include(Addrlib)

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
    if(ADDR_RENOIR_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_RENOIR_BUILD)
    endif()
endif()

if(ADDR_GFX10_BUILD)
    target_sources(addrlib PRIVATE src/chip/gfx10/gfx10_gb_reg.h
                                   src/gfx10/gfx10SwizzlePattern.h
                                   src/gfx10/gfx10addrlib.h
                                   src/gfx10/gfx10addrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/gfx10)

    target_compile_definitions(addrlib PRIVATE ADDR_GFX10_BUILD)
    if(ADDR_NAVI12_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI12_BUILD)
    endif()
    if(ADDR_NAVI14_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI14_BUILD)
    endif()
    if(ADDR_NAVI21_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI21_BUILD)
    endif()
    if(ADDR_NAVI22_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI22_BUILD)
    endif()
endif()

    target_sources(addrlib PRIVATE src/amdgpu_asic_addr.h)

if(ADDR_AM_BUILD)
    target_compile_definitions(addrlib PUBLIC ADDR_AM_BUILD)
endif()

if(ADDR_LNX_KERNEL_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_LNX_KERNEL_BUILD)
endif()

addr_get_cpu_endianness(CPU_ENDIANNESS)
target_compile_definitions(addrlib PRIVATE ${CPU_ENDIANNESS})

target_compile_definitions(addrlib PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if (MSVC_IDE)
    # Get all of the source files
    get_target_property(addrlib_sources addrlib SOURCES)

    # Display the folders directory structure in Visual Studio
    # If this isn't done then you just get generic Header/Source Files filters
    source_group(TREE  ${CMAKE_CURRENT_SOURCE_DIR}/
                 FILES ${addrlib_sources}
    )
endif()
