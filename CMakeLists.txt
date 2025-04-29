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

cmake_minimum_required(VERSION 3.21)

include(cmake/PalVersionHelper.cmake)

project(PAL LANGUAGES CXX)

message(STATUS "PAL_SOURCE_DIR = ${PAL_SOURCE_DIR}")

# Create PAL targets:
# - pal : The overall PAL target that a client links to
# - palUtil : Just the PAL util library
set(CMAKE_FOLDER "${CMAKE_FOLDER}/PAL Libs")
add_library(pal STATIC)
add_library(palUtil STATIC)
target_link_libraries(pal PUBLIC palUtil)

# - palCompilerDeps : Selected parts of PAL core that internal compiler depends on
add_library(palCompilerDeps STATIC)
target_link_libraries(palCompilerDeps PRIVATE palUtil)
target_link_libraries(pal PRIVATE palCompilerDeps)

# PAL's build parameters must be defined *after* the pal target is defined.
# To aid in the sharing of PAL build parameters, set_target_property() is used on the pal
# target to provide a method for clients to retrieve the final, proper value of the build parameter.
# set_target_property() will not work if the target is not yet defined.
include(PalBuildParameters)
include(PalOnlyBuildParameters)
include(PalCodegen)
include(PalOverrides)
include(PalPrecompiledHeaders)

add_subdirectory(cmake)
add_subdirectory(inc)
add_subdirectory(res)
add_subdirectory(shared)
add_subdirectory(src)
add_subdirectory(tools)

pal_compile_definitions(pal)
pal_compiler_options(pal)
pal_compile_definitions(palCompilerDeps)
pal_compiler_options(palCompilerDeps)
pal_compile_definitions(palUtil)
pal_compiler_options(palUtil)
pal_setup_generated_code()
pal_setup_precompiled_headers()

nongen_source_groups(${PAL_SOURCE_DIR}
    TARGETS
        pal
        palCompilerDeps
        palUtil
)
