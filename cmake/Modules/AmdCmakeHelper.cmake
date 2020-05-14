##
 #######################################################################################################################
 #
 #  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
include(CMakeDependentOption)
include(PalVersionHelper)

pal_include_guard(AmdCmakeHelper)

# Build Type Helper ################################################################################
if (CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE_DEBUG $<CONFIG:Debug>)
    set(CMAKE_BUILD_TYPE_RELEASE $<CONFIG:Release>)
    set(CMAKE_BUILD_TYPE_RELWITHDEBINFO $<CONFIG:RelWithDebInfo>)
else()
    string(TOUPPER "${CMAKE_BUILD_TYPE}" capital_CMAKE_BUILD_TYPE)

    if (CMAKE_BUILD_TYPE AND
        NOT capital_CMAKE_BUILD_TYPE MATCHES "^(DEBUG|RELEASE|RELWITHDEBINFO|MINSIZEREL)$")
        message(FATAL_ERROR "Invalid value for CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
    endif()

    if(capital_CMAKE_BUILD_TYPE STREQUAL "DEBUG")
        set(CMAKE_BUILD_TYPE_DEBUG ON)
        set(CMAKE_BUILD_TYPE_RELEASE OFF)
    else()
        set(CMAKE_BUILD_TYPE_DEBUG OFF)
        set(CMAKE_BUILD_TYPE_RELEASE ON)
    endif()
endif()

# Options Helpers ##################################################################################
macro(dropdown_option _option _options)
    set_property(CACHE ${_option} PROPERTY STRINGS ${${_options}})

    list(FIND ${_options} ${${_option}} ${_option}_INDEX)
    if(${${_option}_INDEX} EQUAL -1)
        message(FATAL_ERROR "Option ${${_option}} not supported, valid entries are ${${_options}}")
    endif()
endmacro()

macro(mark_grouped_as_advanced _group)
    get_cmake_property(_groupVariables CACHE_VARIABLES)
    foreach(_groupVariable ${_groupVariables})
        if(_groupVariable MATCHES "^${_group}_.*")
            mark_as_advanced(FORCE ${_groupVariable})
        endif()
    endforeach()
endmacro()

# System Architecture Helpers ######################################################################
include(TestBigEndian)

function(get_system_architecture_endianess endianess)
    test_big_endian(architectureIsBigEndian)
    if (architectureIsBigEndian)
        set(${endianess} "BIG" PARENT_SCOPE)
    else()
        set(${endianess} "LITTLE" PARENT_SCOPE)
    endif()
endfunction()

function(get_system_architecture_bits bits)
    math(EXPR ${bits} "8*${CMAKE_SIZEOF_VOID_P}")
    set(${bits} ${${bits}} PARENT_SCOPE)
endfunction()

# Architecture Endianness ##########################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_ENDIANESS)
    get_system_architecture_endianess(TARGET_ARCHITECTURE_ENDIANESS)
    set(TARGET_ARCHITECTURE_ENDIANESS ${TARGET_ARCHITECTURE_ENDIANESS} CACHE STRING "Specify the target architecture endianess.")
    set(TARGET_ARCHITECTURE_ENDIANESS_OPTIONS "BIG" "LITTLE")
    dropdown_option(TARGET_ARCHITECTURE_ENDIANESS TARGET_ARCHITECTURE_ENDIANESS_OPTIONS)
endif()

# Architecture Bits ################################################################################
if(NOT DEFINED TARGET_ARCHITECTURE_BITS)
    get_system_architecture_bits(TARGET_ARCHITECTURE_BITS)
    set(TARGET_ARCHITECTURE_BITS ${TARGET_ARCHITECTURE_BITS} CACHE STRING "Specify the target architecture bits.")
    set(TARGET_ARCHITECTURE_BITS_OPTIONS "32" "64")
    dropdown_option(TARGET_ARCHITECTURE_BITS TARGET_ARCHITECTURE_BITS_OPTIONS)
endif()

# Find Headers Helper ##############################################################################
macro(target_find_headers _target)
    get_target_property(${_target}_INCLUDES_DIRS ${_target} INCLUDE_DIRECTORIES)

    if(${_target}_INCLUDES_DIRS)
        foreach(_include_dir IN ITEMS ${${_target}_INCLUDES_DIRS})
            file(GLOB_RECURSE _include_files
                LIST_DIRECTORIES false
                "${_include_dir}/*.h"
                "${_include_dir}/*.hpp"
            )

            list(APPEND ${_target}_INCLUDES ${_include_files})
        endforeach()

        target_sources(${_target} PRIVATE ${${_target}_INCLUDES})
    endif()
endmacro()

# Source Groups Helper #############################################################################
# This helper creates source groups for generators that support them. This is primarily MSVC and
# XCode, but there are other generators that support IDE project files.
#
# Note: this only adds files that have been added to the target's SOURCES property. To add headers
# to this list, be sure that you call target_find_headers before you call target_source_groups.
macro(target_source_groups _target)
    get_target_property(${_target}_SOURCES ${_target} SOURCES)
    foreach(_source IN ITEMS ${${_target}_SOURCES})
        set(_source ${_source})
        get_filename_component(_source_path "${_source}" ABSOLUTE)
        file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
        get_filename_component(_source_path_rel "${_source_path_rel}" DIRECTORY)
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endmacro()

# Deprecated Visual Studio Filter Helper ###########################################################
macro(target_vs_filters _target)
    message(DEPRECATION "target_vs_filters is deprecated. use 'target_find_headers' and 'target_source_groups' instead.")
    target_find_headers(${_target})
    if(MSVC)
        target_source_groups(${_target})
    endif()
endmacro()

# Visual Studio Specific Options ###################################################################
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()

