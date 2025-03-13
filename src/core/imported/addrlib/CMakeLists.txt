##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.13...3.28)
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

add_subdirectory(cmake)

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
                               src/core/addrlib2.h
                               src/core/addrlib2.cpp
                               src/core/addrswizzler.h
                               src/core/addrswizzler.cpp
                               src/core/coord.h
                               src/core/coord.cpp)

if(ADDR_SILENCE_ASSERT_ALWAYS)
    target_compile_definitions(addrlib PRIVATE ADDR_SILENCE_ASSERT_ALWAYS)
endif()

if(ADDR_GFX10_BUILD)
    target_sources(addrlib PRIVATE src/chip/gfx10/gfx10_gb_reg.h
                                   src/gfx10/gfx10SwizzlePattern.h
                                   src/gfx10/gfx10addrlib.h
                                   src/gfx10/gfx10addrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/gfx10)
    target_compile_definitions(addrlib PRIVATE ADDR_GFX10_BUILD)

endif()

#if ADDR_GFX11_BUILD
if(ADDR_GFX11_BUILD)
    target_sources(addrlib PRIVATE src/chip/gfx11/gfx11_gb_reg.h
                                   src/gfx11/gfx11SwizzlePattern.h
                                   src/gfx11/gfx11addrlib.h
                                   src/gfx11/gfx11addrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/gfx11)

    target_compile_definitions(addrlib PUBLIC ADDR_GFX11_BUILD)
    #if ADDR_NAVI31_BUILD
    if(ADDR_NAVI31_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI31_BUILD)
    endif()
    #endif
    #if ADDR_NAVI32_BUILD
    if(ADDR_NAVI32_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI32_BUILD)
    endif()
    #endif
    #if ADDR_NAVI33_BUILD
    if(ADDR_NAVI33_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_NAVI33_BUILD)
    endif()
    #endif
    #if ADDR_STRIX1_BUILD
    if(ADDR_STRIX1_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_STRIX1_BUILD)
    endif()
    #endif
    #if ADDR_STRIX_HALO_BUILD
    if(ADDR_STRIX_HALO_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_STRIX_HALO_BUILD)
    endif()
    #endif
    #if ADDR_PHOENIX_BUILD
    if(ADDR_PHOENIX_BUILD)
        target_compile_definitions(addrlib PRIVATE ADDR_PHOENIX_BUILD)
        #if ADDR_PHOENIX1_BUILD
        if(ADDR_PHOENIX1_BUILD)
            target_compile_definitions(addrlib PRIVATE ADDR_PHOENIX1_BUILD)
        endif()
        #endif
        #if ADDR_PHOENIX2_BUILD
        if(ADDR_PHOENIX2_BUILD)
            target_compile_definitions(addrlib PRIVATE ADDR_PHOENIX2_BUILD)
        endif()
        #endif
    endif()
    #endif
endif()
#endif

#if ADDR_GFX12_BUILD
if(ADDR_GFX12_BUILD)
    target_sources(addrlib PRIVATE src/core/addrlib3.h
                                   src/core/addrlib3.cpp
                                   src/gfx12/gfx12SwizzlePattern.h
                                   src/gfx12/gfx12addrlib.h
                                   src/gfx12/gfx12addrlib.cpp)

    target_include_directories(addrlib PRIVATE src/chip/gfx12)
    target_compile_definitions(addrlib PUBLIC ADDR_GFX12_BUILD)
#if ADDR3_SUPPORT
    target_compile_definitions(addrlib PUBLIC ADDR3_SUPPORT)
#endif

    #if ADDR_GFX12_SHARED_BUILD
    if (ADDR_GFX12_SHARED_BUILD)
        target_sources(addrlib PRIVATE src/gfx12/shared/addr_shared.cpp
                                       src/gfx12/shared/addr_shared.h)

        target_compile_definitions(addrlib PUBLIC ADDR_SHARED)
        target_compile_definitions(addrlib PUBLIC ADDR_GFX12_SHARED_BUILD)
    endif()
    #endif
endif()
#endif

# Everybody requires addrlib's internal asic definitions header
target_sources(addrlib PRIVATE src/amdgpu_asic_addr.h)

if(ADDR_AM_BUILD)
    target_compile_definitions(addrlib PUBLIC ADDR_AM_BUILD)
endif()

if(ADDR_LNX_KERNEL_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_LNX_KERNEL_BUILD)
endif()

if(NOT ADDR_KERNEL_BUILD)
    target_compile_definitions(addrlib PRIVATE ADDR_ALLOW_TLS ADDR_ALLOW_STDIO)
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

