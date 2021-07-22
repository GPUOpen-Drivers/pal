##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
cmake_minimum_required(VERSION 3.13...3.16)

### Define Project #####################################################################################################
project(PAL
    # While PAL is primarily a C++ library, it also has some C files.
    # TestBigEndian called from AmdCmakeHelper requires C or CXX language to be enabled.
    LANGUAGES CXX C
)

### CMake Includes #####################################################################################################
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/)
include(PalVersionHelper)
include(PalCompileDefinitions)
include(PalCompilerOptions)

### Standlone Build ####################################################################################################
set(PAL_IS_STANDALONE OFF)
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_SOURCE_DIR})
    message(AUTHOR_WARNING "Pal is being built as a standalone project. This isn't properly supported yet.")
    set(PAL_IS_STANDALONE ON)
endif()

### Build Parameters ###################################################################################################
include(PalBuildParameters)

### Project Options ####################################################################################################
include(PalOptions)

### Option Overrides ###################################################################################################
include(PalOverrides)

### Create PAL Library #################################################################################################
set(CMAKE_FOLDER "${CMAKE_FOLDER}/PAL Libs")
add_library(pal STATIC)
add_library(AMD::PAL ALIAS pal)

### Add Subdirectories #################################################################################################
add_subdirectory(cmake)
add_subdirectory(inc)
add_subdirectory(res)
add_subdirectory(src)
add_subdirectory(shared)

### Build Definitions ##################################################################################################
pal_compile_definitions(pal)

### Compiler Options ###################################################################################################
pal_compiler_options(pal)

### Custom Commands ####################################################################################################

### IDE support ########################################################################################################
get_target_property(pal_sources pal SOURCES)
source_group(TREE ${PAL_SOURCE_DIR}/
             FILES ${pal_sources}
)
