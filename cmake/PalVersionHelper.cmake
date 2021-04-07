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

# Store the file's location for relative paths used in the functions.
set(PAL_VERSION_HELPER_DIR ${CMAKE_CURRENT_LIST_DIR})

# A helper function to allow usage of DEBUG mode introduced in cmake 3.15
# This function is intended to prevent printing out everything to STATUS,
# which is undesirable for clients of PAL, since only cmake developers need to care.
function(message_debug)
    # DEBUG mode was introduced in 3.15
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
        message(DEBUG ${ARGV})
    else()
        message(STATUS "DEBUG: ${ARGV}")
    endif()
endfunction()

# A helper function to allow usage of VERBOSE mode introduced in cmake 3.15
# This function is intended to prevent printing out everything to STATUS,
# which is undesirable for clients of PAL, since only cmake developers need to care.
function(message_verbose)
    # VERBOSE mode was introduced in 3.15
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
        message(VERBOSE ${ARGV})
    else()
        message(STATUS "VERBOSE: ${ARGV}")
    endif()
endfunction()

# Cache variables aren't ideal for customizing pal build's
# They have serious problems. Particularly for dirty builds.
# It's very important clients are explicitly opting in for support they desire.
# Otherwise you get silent bugs, that no one understands.
function(pal_build_parameter VARIABLE MESSAGE DEFAULT_VALUE MODE)
    # Ex:
    # VARIABLE = FOOBAR
    # MESSAGE = "FOORBAR is a cool idea"
    # DEFAULT_VALUE = OFF
    # MODE = AUTHOR_WARNING

    if (NOT DEFINED ${VARIABLE})
        set(${VARIABLE} ${DEFAULT_VALUE} PARENT_SCOPE)

        set(msg "${VARIABLE} not specified. Defaulting to ${DEFAULT_VALUE}. ${MESSAGE}")

        # Support debug/verbose modes (cmake 3.15+ users)
        if (${MODE} STREQUAL "DEBUG")
            message_debug(${msg})
        elseif (${MODE} STREQUAL "VERBOSE")
            message_verbose(${msg})
        else()
            message(${MODE} ${msg})
        endif()
    endif()

    # To assist in potential debugging
    message_debug("PAL BUILD PARAMETER: ${VARIABLE} set to ${${VARIABLE}}")
endfunction()

# PAL uses specific asics, SC uses generations, Addrlib does both...
# That's why I made the helper function "pal_set_or", to help set variables that specify graphics generations.
# Basically the idea being if any are enabled it'll activate.
# Cmake doesn't have a 'set_or' function (ie |= )
function(pal_set_or VARIABLE VALUE)
    # Ex:
    # VARIABLE = FOOBAR
    # VALUE = OFF

    # If the variable hasn't been defined before then set it.
    if (NOT DEFINED ${VARIABLE})
        set(${VARIABLE} ${VALUE} PARENT_SCOPE)
        return()
    endif()

    set(cur_value ${${VARIABLE}})

    if (NOT ${cur_value})
        set(${VARIABLE} ${VALUE} PARENT_SCOPE)
    endif()
endfunction()

# Helper function to override cache variables, that pal's clients have
function(pal_override VARIABLE VALUE)
    # Always use string as the 'type'
    # 'type' is only used for gui presentation. Everything in cmake is really a string.
    set(${VARIABLE} ${VALUE} CACHE STRING "PAL OVERRIDE" FORCE)
    mark_as_advanced(${VARIABLE})
endfunction()

# Helper function that sets the value of the variable passed in to the current value of
# PAL_INTERFACE_MAJOR_VERSION defined in palLib.h.
function(pal_get_current_pal_interface_major_version VARIABLE)
    # Relative path to palLib.h from this file.
    set(LIB_H_PATH ${PAL_VERSION_HELPER_DIR}/../inc/core/palLib.h)
    if(NOT EXISTS ${LIB_H_PATH})
        message(FATAL_ERROR "Could not find palLib.h at \"${LIB_H_PATH}\"")
    endif()
    # Read the line from the file where the version is defined.
    file(STRINGS
        ${LIB_H_PATH}
        PAL_INTERFACE_MAJOR_VERSION
        REGEX "^#define PAL_INTERFACE_MAJOR_VERSION [0-9]+$"
    )
    # Validate the version was found.
    if(NOT PAL_INTERFACE_MAJOR_VERSION)
        message(FATAL_ERROR "Could not find PAL_INTERFACE_MAJOR_VERSION in \"${LIB_H_PATH}\"")
    endif()
    # Read the version from the line.
    string(REGEX REPLACE
        "#define PAL_INTERFACE_MAJOR_VERSION " ""
        PAL_INTERFACE_MAJOR_VERSIONX ${PAL_INTERFACE_MAJOR_VERSION}
    )
    # Set the value of VARIABLE to the version.
    set(${VARIABLE} ${PAL_INTERFACE_MAJOR_VERSIONX} PARENT_SCOPE)
endfunction()

# Source Groups Helper #############################################################################
# This helper creates source groups for generators that support them. This is primarily for MSVC,
# but there are other generators that support IDE project files.
#
# Note: this only adds files that have been added to the target's SOURCES property. To add headers
# to this list, be sure that you call target_find_headers before you call target_source_groups.
function(pal_target_source_groups _target)
    get_target_property(${_target}_SOURCES ${_target} SOURCES)
    foreach(_source IN ITEMS ${${_target}_SOURCES})
        set(_source ${_source})
        get_filename_component(_source_path "${_source}" ABSOLUTE)
        file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
        get_filename_component(_source_path_rel "${_source_path_rel}" DIRECTORY)
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endfunction()
