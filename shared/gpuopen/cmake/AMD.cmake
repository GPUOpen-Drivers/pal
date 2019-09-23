
##
 #######################################################################################################################
 #
 #  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

function(amd_target name)

        # [GCC] Exceptions
        #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
        #
        # [GCC] Options Controlling C++ Dialect
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/C_002b_002b-Dialect-Options.html
        #
        # [GCC] Options That Control Optimization
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Optimize-Options.html
        target_compile_options(${name} PRIVATE
            -fno-exceptions  # Disable exception handling support.
            -fno-rtti        # Disable run-time type information support.
            -fno-math-errno) # Single instruction math operations do not set ERRNO.

        # [GCC] Options to Request or Suppress Warnings
        #   https://gcc.gnu.org/onlinedocs/gcc-8.1.0/gcc/Warning-Options.html
        target_compile_options(${name} PRIVATE
            -Wall    # Enable warnings about questionable language constructs.
            -Wextra  # Enable extra warnings that are not enabled by -Wall.
            -Werror  # Turn warnings into errors.
        )

endfunction()

function(amd_executable name)

    add_executable(${name} ${ARGN} "")
    amd_target    (${name})

endfunction()

function(amd_um_library name type)

    add_library(${name} ${type} ${ARGN} "")
    amd_target (${name})

    set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)

endfunction()

function(amd_library name type)

    amd_um_library(${name} ${type} ${ARGN} "")

endfunction()

# Indicate target architecture bits
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_ARCHITECTURE_BITS "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TARGET_ARCHITECTURE_BITS "32")
else()
    message(FATAL_ERROR "Unsupported target architecture - pointers must be 4 or 8 bytes, not ${CMAKE_SIZEOF_VOID_P}")
endif()
