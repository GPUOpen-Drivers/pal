##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.13..3.21)
project(rdf LANGUAGES CXX C)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(RDF_IS_TOP_LEVEL ON)
else()
    set(RDF_IS_TOP_LEVEL OFF)
endif()

option(RDF_BUILD_TESTS         "Build the tests"  ${RDF_IS_TOP_LEVEL})
option(RDF_BUILD_TOOLS         "Build RDF tools"  ${RDF_IS_TOP_LEVEL})
option(RDF_BUILD_INSTALL       "Install RDF libs" ${RDF_IS_TOP_LEVEL})
option(RDF_ENABLE_CXX_BINDINGS "Enable the C++ interface" ON)
option(RDF_STATIC              "Build RDF as a static library" OFF)

if(RDF_BUILD_TESTS)
    enable_testing()
endif()

# Set the C++ standard for all libraries/executables in RDF
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Don't modify these variables for projects that consume this libary using add_subdirectory
if (RDF_IS_TOP_LEVEL)
    set(CMAKE_SKIP_BUILD_RPATH FALSE)
    set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    if("${isSystemDir}" STREQUAL "-1")
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    endif()

    set(CMAKE_EXECUTABLE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

################################################################################
# Import third party libraries
# For each library, three steps are checked to ensure it is present:
#   1. Check if target is already provided
#   2. Try to find lib via vcpkg
#   3. Add target locally
# We have to do this here for VCPKG to work, as we can't alias targets from
# VCPKG in a subdirectory, and VCPKG uses different target names which we
# don't want to force on everyone
# We use QUIET on all finds because failure is an option (we just fall back
# to our internal copy.)

if(NOT TARGET zstd)
    find_package(zstd QUIET)
	if(TARGET zstd::libzstd_shared)
		add_library(zstd ALIAS zstd::libzstd_shared)
	elseif(TARGET zstd::libzstd_static)
		add_library(zstd ALIAS zstd::libzstd_static)
    else()
	    add_subdirectory(imported/zstd)
	endif()
endif()

if(RDF_BUILD_TESTS AND (NOT TARGET catch2))
    find_package(Catch2 QUIET)
    if(TARGET Catch2::Catch2)
		add_library(catch2 ALIAS Catch2::Catch2WithMain)
    else()
		add_subdirectory(imported/catch2)
    endif()
endif()

if(RDF_BUILD_TOOLS)
    if(NOT TARGET cli11)
        find_package(CLI11 QUIET)
        if(TARGET CLI11::CLI11)
            add_library(cli11 ALIAS CLI11::CLI11)
        else()
            add_subdirectory(imported/cli11)
        endif()
    endif()

    if(NOT TARGET json)
        find_package(nlohmann_json QUIET)
        if(TARGET nlohmann_json::nlohmann_json)
            add_library(json ALIAS nlohmann_json::nlohmann_json)
        else()
            add_subdirectory(imported/json)
        endif()
    endif()
endif()

################################################################################
# Project build
add_subdirectory(rdf)
if(RDF_BUILD_TOOLS)
    add_subdirectory(rdfi)
    add_subdirectory(rdfg)
    add_subdirectory(rdfm)
endif()
