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
if (DEFINED PalVersionHelper_pal_include_guard)
    return()
endif()
set(PalVersionHelper_pal_include_guard ON)

# A helper macro to have include guards with pre 3.10 compatibility
# See the documentation on include guards for more info (IE the "#pragma once" of cmake)
# https://cmake.org/cmake/help/latest/command/include_guard.html
#
# This needs to be a macro to allow include_guard() to work. Include guard cannot be called
# from inside a function.
macro(pal_include_guard client_var)
    # If you can use 3.10 functionality then use the standard include guard
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.10")
        include_guard()
    else()
        # Return if the variable already exists to not waste time
        if (DEFINED ${client_var}_pal_include_guard)
            return()
        endif()

        # Otherwise create the variable
        set(${client_var}_pal_include_guard ON)
    endif()
endmacro()

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

# Use this when you don't to create a cache variable, but still want to inform
# the client about build options. As cache variables have downsides.
#
# This function is especially helpful for build options that are expected to change
# over time. Something cache variables aren't well suited to.
# The best example is GPU build options, this is something PAL expects it's clients to override.
#
# This is also really helpful when creating new GPU support, since clients are alerted about the suppport
# programatically!
function(pal_warn_about_default_gpu VARIABLE MESSAGE DEFAULT_VALUE)
    # Ex:
    # VARIABLE = FOOBAR
    # MESSAGE = "FOORBAR is a cool idea"
    # DEFAULT_VALUE = OFF

    # If PAL is being built standalone, no need to display as warnings. Since the warnings are intended
    # for PAL clients.
    if (PAL_IS_STANDALONE)
        set(mode "STATUS")
    else()
        set(mode "AUTHOR_WARNING")
    endif()

    if (NOT DEFINED ${VARIABLE})
        set(${VARIABLE} ${DEFAULT_VALUE} PARENT_SCOPE)

        message(${mode} "${VARIABLE} not specified. Defaulting to ${DEFAULT_VALUE}\n"
                        "Message: ${MESSAGE}")
    endif()

    # To assist in potential debugging
    message_debug("PAL GPU SUPPORT: ${VARIABLE} set to ${${VARIABLE}}")
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
