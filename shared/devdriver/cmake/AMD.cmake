##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Apply options to an AMD target.
# These options are hard requirements to build. If they cannot be applied, we
# will need to modify the CMakeLists.txt of the project until they can be applied.
function(amd_target_options name)
    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        # [GCC] Exceptions
        #   https://gcc.gnu.org/onlinedocs/libstdc++/manual/using_exceptions.html
        #
        # [GCC] Options for Code Generation Conventions
        #   https://gcc.gnu.org/onlinedocs/gcc/Code-Gen-Options.html
        #
        # [GCC] Options Controlling C++ Dialect
        #   https://gcc.gnu.org/onlinedocs/gcc/C_002b_002b-Dialect-Options.html
        #
        # [GCC] Options That Control Optimization
        #   https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html
        #
        # [GCC] Options That Control Visibility
        #   https://gcc.gnu.org/wiki/Visibility
        target_compile_options(${name}
            PRIVATE
                # Disable exception handling support.
                -fno-exceptions

                # Disable run-time type information support.
                -fno-rtti

                # Single instruction math operations do not set ERRNO.
                -fno-math-errno

                # Disable aggressive type aliasing rules
                -fno-strict-aliasing

                # Hide export symbols by default
                -fvisibility=hidden

                # Also hide inline export symbols
                -fvisibility-inlines-hidden
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name}
            PRIVATE
                /volatile:iso

                /Zc:__cplusplus

                # Disable permissive C++ semantics. Help code portability.
                /permissive-
        )
    else()
        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
    endif()
endfunction()

function(amd_target_definitions name)
    # Interface targets can only have INTERFACE defines/etc, so we must care for that.
    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        set(VISIBILITY INTERFACE)
    else()
        set(VISIBILITY PUBLIC)
    endif()

    math(EXPR bits "8 * ${CMAKE_SIZEOF_VOID_P}")
    target_compile_definitions(${name} ${VISIBILITY} DD_ARCH_BITS=${bits})
endfunction()

function(amd_target name)
    amd_target_options(${name})
    amd_target_definitions(${name})
endfunction()

function(amd_executable name)
    add_executable(${name} ${ARGN} "")
    amd_target    (${name})
endfunction()

function(amd_library name type)
    if (${type} STREQUAL "INTERFACE")
        add_library(${name} ${type} ${ARGN})
    else()
        add_library(${name} ${type} ${ARGN} "")
    endif()

    amd_target (${name})
endfunction()
