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
    if( (${CMAKE_VERSION} VERSION_GREATER "3.10") OR
        (${CMAKE_VERSION} VERSION_EQUAL   "3.10") )
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

function(set_ipo_compile_options)
    if ((${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "5.3") OR
        (${CMAKE_C_COMPILER_VERSION} VERSION_LESS "5.3"))
        return()
    endif()

    target_compile_options(pal PRIVATE
        $<$<CONFIG:Release>:
            -flto
            # Use linker plugin requires gcc version to be greater or equal to 5.3
            -fuse-linker-plugin
        >
    )

    message(STATUS "LTO enabled for Pal")
endfunction()

# A helper function to allow usage of DEBUG mode introduced in cmake 3.15
# This function is intended to prevent printing out everything to STATUS,
# which is undesirable for clients of PAL, since only cmake developers need to care.
function(message_debug)
    # DEBUG mode was introduced in 3.15
    if( (${CMAKE_VERSION} VERSION_GREATER "3.15") OR
        (${CMAKE_VERSION} VERSION_EQUAL   "3.15"))
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
    if( (${CMAKE_VERSION} VERSION_GREATER "3.15") OR
        (${CMAKE_VERSION} VERSION_EQUAL   "3.15"))
        message(VERBOSE ${ARGV})
    else()
        message(STATUS "VERBOSE: ${ARGV}")
    endif()
endfunction()

function(setup_ipo_new)
    # If this variable has been defined by the client then don't do anything
    # Since the client of the PAL library will clobber any IPO settings anyway
    if (DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION)
        return()
    endif()

    foreach(configType DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        if (DEFINED CMAKE_INTERPROCEDURAL_OPTIMIZATION_${configType})
            return()
        endif()
    endforeach()

    set_ipo_compile_options()
endfunction()

# Setup IPO for gcc/release builds only
function(pal_setup_gcc_ipo)
    if(NOT ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        message(FATAL_ERROR "This function is currently only intended for GCC")
    endif()

    if (${CMAKE_VERSION} VERSION_GREATER "3.9")
        # This path checks if the user is setting cmake global variables
        # that would clobber/conflict with out compile options
        setup_ipo_new()
    else()
        set_ipo_compile_options()
    endif()
endfunction()

