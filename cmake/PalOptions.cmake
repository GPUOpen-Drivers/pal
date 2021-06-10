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
include_guard()

include(PalVersionHelper)
include(CMakeDependentOption)

# All options/cache variables should have the prefix "PAL_" this serves two main purposes
#   Name collision issues
#   Cmake-gui allows grouping of variables based on prefixes, which then makes it clear what options PAL defined

option(PAL_DBG_COMMAND_COMMENTS "Command with comments" OFF)

option(PAL_ENABLE_PRINTS_ASSERTS "Enable print assertions?" OFF)
option(PAL_ENABLE_PRINTS_ASSERTS_DEBUG "Enable print assertions on debug builds?" ON)

option(PAL_MEMTRACK "Enable PAL memory tracker?" OFF)

option(PAL_BUILD_CORE "Build PAL Core?" ON)

option(PAL_BUILD_GPUUTIL "Build PAL GPU Util?" ON)

cmake_dependent_option(PAL_BUILD_LAYERS "Build PAL Layers?" ON "PAL_BUILD_GPUUTIL" OFF)

option(PAL_BUILD_DBG_OVERLAY "Build PAL Debug Overlay?" ON)

option(PAL_BUILD_GPU_PROFILER "Build PAL GPU Profiler?" ON)

option(PAL_DISPLAY_DCC "Enable DISPLAY DCC?" ON)

option(PAL_BUILD_DRI3 "Build PAL with DRI3 support?" ON)
option(PAL_BUILD_WAYLAND "Build PAL with WAYLAND support?" OFF)

# Paths to PAL's dependencies
set(PAL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/src/util/imported/metrohash CACHE PATH "Specify the path to the MetroHash project.")
set(   PAL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/src/util/imported/cwpack    CACHE PATH "Specify the path to the CWPack project.")
set(      PAL_VAM_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/vam       CACHE PATH "Specify the path to the VAM project.")
set(     PAL_ADDR_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/addrlib   CACHE PATH "Specify the path to the ADDRLIB project.")

set(PAL_GPUOPEN_PATH "default" CACHE PATH "Specify the path to the GPUOPEN_PATH project.")

