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

cmake_minimum_required(VERSION 3.5)

# In cmake 3.10 you can use an actual include_guard.
if (DEFINED PalHelperFunctions)
    return()
endif()
set(PalHelperFunctions 1)

# https://cmake.org/cmake/help/latest/policy/CMP0069.html
# Don't use the old limited definition that only applies to the Intel Compiler for Linux
# Use the new policy
if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()

# CMake 3.13+: option() honors normal variables.
# Using the new policy forces option() to honor normal variables.
# For example, a project that embeds another project as a subdirectory
# may want to hard-code options of the subproject to build the way it needs.
# Sadly this doesn't cover cache variables created by the set() call
if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

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

macro(pal_find_python)
    # If we have a newer cmake, the new FindPython scripts are much more reliable
    if( (${CMAKE_VERSION} VERSION_GREATER "3.12") OR
        (${CMAKE_VERSION} VERSION_EQUAL "3.12"))

        find_package(Python3
            REQUIRED
            COMPONENTS Interpreter
        )

        set(PYTHON_CMD ${Python3_EXECUTABLE})
    else()
        find_package(PythonInterp 3)
        if(NOT PYTHONINTERP_FOUND)
            if(UNIX)
                message(FATAL_ERROR "Python 3 is needed to generate some source files.")
            endif()
        endif()
        set(PYTHON_CMD ${PYTHON_EXECUTABLE})
    endif()
endmacro()
