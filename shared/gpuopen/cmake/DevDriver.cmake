
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

# DevDriver.cmake adds project-specific options and warnings.
# It does this while depending on the base AMD-wide configurations defined here.
# Users of this file should only `include(DevDriver)`
include(AMD)

option(
    DEVDRIVER_USE_CPP17 "Optionally build using the C++17 standard. If disabled, build as C++11"
    OFF)

option(
    DEVDRIVER_FORCE_COLOR_OUPUT "Force colored diagnostic messages (Clang/gcc only)"
    ON)

string(CONCAT DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS_HELP_TEXT
    "C++ static_asserts cannot format strings. "
    "You can fake it with SFINAE template types, but it's rough. "
    "This enables that alternate mode for some special assert macros."
)
option(DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS
    ${DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS_HELP_TEXT}
    OFF)
unset(DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS_HELP_TEXT)

# Unity builds are only supported starting in 3.16, but significantly improve Windows build times
# VERSION_GREATER_EQUAL was introduced in CMake 3.7. We use NOT ${X} VERSION_LESS for compatibility with CMake 3.5.
if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0")
    set(DEVDRIVER_UNITY_BUILDS_DEFAULT ON)
else()
    set(DEVDRIVER_UNITY_BUILDS_DEFAULT OFF)
endif()
option(
    DEVDRIVER_UNITY_BUILDS "Optionally build all devdriver CMake targets with unity builds. Can be overwritten with CMAKE_UNITY_BUILD."
    ${DEVDRIVER_UNITY_BUILDS_DEFAULT})

# Configure compilation options depending on available CPU cores
include(ProcessorCount)

macro(apply_gpuopen_warnings _target)

    if(DEVDRIVER_USE_CPP17)
        message(VERBOSE "Using C++17 for ${_target}")

        # WA: CMake supports C++17 since version 3.8
        if (CMAKE_VERSION VERSION_LESS "3.8")

            if (CMAKE_CXX_COMPILER_ID MATCHES "GCC|Clang")

                # [GCC] C++ Standards Support in GCC
                #   https://gcc.gnu.org/projects/cxx-status.html#cxx17
                target_compile_options(${_target} PRIVATE -std=c++17) # Enable C++17 support.

            elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

                target_compile_options(${_target} PRIVATE /std:c++17) # Enable C++17 features.

            else()
                message(FATAL_ERROR "Using unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
            endif()

        else()
            set_target_properties(${_target} PROPERTIES CXX_STANDARD 17)
        endif()

    else()

        if (WIN32)
            set_target_properties(${_target} PROPERTIES CXX_STANDARD 14)
        else()
            set_target_properties(${_target} PROPERTIES CXX_STANDARD 11)
        endif()

    endif()

    # Do not fallback to c++98 if the compiler does not support 11/17.
    set_target_properties(${_target} PROPERTIES CXX_STANDARD_REQUIRED TRUE)
    # Do not use flags like `-std=gnu++11`, instead use `-std=c++11`.
    set_target_properties(${_target} PROPERTIES CXX_EXTENSIONS FALSE)

    # Make a DD_SHORT_FILE macro that includes a shorter, partial file path.
    # The additional / is important to remove the last character from the path.
    # Note that it does not matter if the OS uses / or \, because we are only
    # saving the path size.
    string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
    target_compile_definitions(${_target} PUBLIC "DD_SHORT_FILE=(__FILE__+${SOURCE_PATH_SIZE})")

    if (DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS)
        target_compile_definitions(${_target} PUBLIC DEVDRIVER_ENABLE_VERBOSE_STATIC_ASSERTS)
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

        # Apply special options for GCC 8.x+
        if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)

            target_compile_options(${_target} PRIVATE
                # This warning triggers when you memcpy into or out of a "non trivial" type.
                # The requirements for "trivial type" are hard - e.g. some user supplied constructors are enough to make
                #   it not count.
                #   Properly fixing this would require embracing more C++14 than we currently do. (e.g. `= default` ctors)
                #   This warning is new in gcc 8.x
                -Wno-class-memaccess
            )

        # Apply special options for versions earlier than GCC 5.x
        elseif (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)

            target_compile_options(${_target} PRIVATE
                # This warning triggers when we default initialize structures with the "StructType x = {};" syntax.
                # It only triggers on GCC 4.8
                -Wno-missing-field-initializers
            )

        endif()

        if (DEVDRIVER_FORCE_COLOR_OUPUT)

            if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)

                # For details on customizing this, see the docs:
                #   https://gcc.gnu.org/onlinedocs/gcc-5.2.0/gcc/Language-Independent-Options.html
                target_compile_options(${_target} PRIVATE -fdiagnostics-color)

            endif()

        endif()

     elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

        target_compile_options(${_target} PRIVATE
            # No Clang-specific options yet
        )
        if (DEVDRIVER_FORCE_COLOR_OUPUT)
            target_compile_options(${_target} PRIVATE -fcolor-diagnostics)
        endif()

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")

        target_compile_options(${_target} PRIVATE
            # No AppleClang-specific options yet
        )
        if (DEVDRIVER_FORCE_COLOR_OUPUT)
            target_compile_options(${_target} PRIVATE -fcolor-diagnostics)
        endif()

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

        target_compile_options(${_target} PRIVATE
            /wd4127            # conditional expression is constant
            /wd4201            # nonstandard extension used : nameless struct/union
            /wd4512            # assignment operator could not be generated
            /we4296            # unsigned integer comparison is constant
            /we5038            # initialization order
            /diagnostics:caret # Format error messages with a "^"
            /permissive-       # Enable stricter standards conformance
        )

        # Compile files in parallel
        ProcessorCount(CoreCount)
        target_compile_options(${_target} PRIVATE /MP${CoreCount})

    else()
        message(FATAL_ERROR "Using unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()

    if (WIN32)

        # "ThIs FUNCtiOn oR VariABle MAy Be uNSafE."
        target_compile_definitions(${_target}
            PUBLIC
                "_CRT_INSECURE_NO_DEPRECATE"
                "_CRT_SECURE_NO_WARNINGS"
        )

    endif()

endmacro()

function(apply_devdriver_build_configs name)
    # Do not overwrite the CMake system-wide define.
    if (NOT DEFINED CMAKE_UNITY_BUILD)
        set_target_properties(
            ${name}
            PROPERTIES
                UNITY_BUILD ${DEVDRIVER_UNITY_BUILDS}
        )
    endif()
endfunction()

function(devdriver_target name)

    amd_target(${name} ${ARGN})
    apply_gpuopen_warnings(${name})
    apply_devdriver_build_configs(${name})

endfunction()

function(devdriver_executable name)

    amd_executable(${name} ${ARGN})
    apply_gpuopen_warnings(${name})
    apply_devdriver_build_configs(${name})

endfunction()

function(devdriver_library name type)

    amd_library(${name} ${type} ${ARGN})
    if (NOT ${type} STREQUAL "INTERFACE")
        apply_gpuopen_warnings(${name})
        apply_devdriver_build_configs(${name})
    endif()

endfunction()
