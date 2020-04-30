
##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

# Goal: Define a global variable so that all AMD components can query the
#       bit count in the processor they're compiling for.
# Problem:
#       Scoping rules in CMake are complex and nuanced. It's not obvious when a
#       set command is going to be visible to different components
# Solution:
#       Define a global property for this information. Anyone anywhere can
#       query it, as shown below and in `amd_target_definitions`.
#
# Define target CPU architecture bits.
define_property(
    GLOBAL
    PROPERTY    AMD_TARGET_ARCH_BITS
    BRIEF_DOCS  "The \"bitness\" of the target processor"
    FULL_DOCS   "Processors have a \"bitness\" to them that is commonly used to
                 define their pointer size. In practice this is always
                32 or 64bit"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_property(GLOBAL PROPERTY AMD_TARGET_ARCH_BITS 64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_property(GLOBAL PROPERTY AMD_TARGET_ARCH_BITS 32)
else()
    message(FATAL_ERROR
        "Target CPU architecture ${CMAKE_SYSTEM_PROCESSOR} is not supported!
         Adresses must be 4-byte or 8-byte wide, not ${CMAKE_SIZEOF_VOID_P}-byte wide."
    )
endif()

# Set the variable here for convenience. Only CMake "code" that directly
# `include()`s this file can access this variable. `add_subdirectory` introduces
# a new scope, and won't propagate this variable otherwise.
get_property(AMD_TARGET_ARCH_BITS GLOBAL PROPERTY AMD_TARGET_ARCH_BITS)

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
                # Specify the use of C++ Standard volatile, not the MS extension
                # We enable warning C4746 too.
                # See: https://docs.microsoft.com/en-us/cpp/build/reference/volatile-volatile-keyword-interpretation?view=vs-2019
                /volatile:iso

                # MSVC does not report the correct value for __cplusplus for legacy reasons
                # This does not change the langauge standard, just makes the behavior standard
                /Zc:__cplusplus
        )

    else()

        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")

    endif()

endfunction()

function(amd_target_warnings name)

    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

        # [GCC] Options to Request or Suppress Warnings
        #   https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
        target_compile_options(${name}
            PRIVATE
                -Wall   # Enable warnings about questionable language constructs.
                -Wextra # Enable extra warnings that are not enabled by -Wall.
        )

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name}
            PRIVATE
                /W4 # Enable warning level 4.
        )

    else()

        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")

    endif()

endfunction()

function(amd_target_definitions name)

    get_property(AMD_TARGET_ARCH_BITS GLOBAL PROPERTY AMD_TARGET_ARCH_BITS)

    # Interface targets can only have INTERFACE defines/etc, so we must care for that.
    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        set(VISIBILITY INTERFACE)
    else()
        set(VISIBILITY PUBLIC)
    endif()

    target_compile_definitions(${name}
        ${VISIBILITY}
            AMD_TARGET_ARCH_BITS=${AMD_TARGET_ARCH_BITS}
    )

endfunction()

function(amd_target name)

    amd_target_options(${name})
    amd_target_definitions(${name})
    amd_target_warnings(${name})

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
        set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    amd_target (${name})

endfunction()
