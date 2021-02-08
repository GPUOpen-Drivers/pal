
##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.10..3.16)

# DevDriver.cmake adds project-specific options and warnings.
# It does this while depending on the base AMD-wide configurations defined here.
# Users of this file should only `include(DevDriver)`
include(AMD)

# A helper function to allow usage of DEBUG mode introduced in CMake 3.15.
#
# This function is intended to prevent printing out everything to STATUS.
# If using CMake older than 3.15, this function is a no-op. Developers who
# wish to see debug messages should update their CMake install.
function(devdriver_message_debug)
    # DEBUG mode was introduced in 3.15
    if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15")
        message(DEBUG ${ARGV})
    endif()
endfunction()

option(DD_OPT_FORCE_COLOR_OUPUT "Force colored diagnostic messages (Clang/gcc only)"
    ON)

string(CONCAT DD_OPT_VERBOSE_STATIC_ASSERTS_HELP_TEXT
    "C++ static_asserts cannot format strings. "
    "You can fake it with SFINAE template types, but it's rough. "
    "This enables that alternate mode for some special assert macros."
)
option(DD_OPT_VERBOSE_STATIC_ASSERTS
    ${DD_OPT_VERBOSE_STATIC_ASSERTS_HELP_TEXT}
    OFF)
unset(DD_OPT_VERBOSE_STATIC_ASSERTS_HELP_TEXT)

option(DD_OPT_WARNINGS_AS_ERRORS "Enforce a warning-clean build"
    OFF)

# Unity builds are only supported starting in 3.16, but significantly improve Windows build times
# VERSION_GREATER_EQUAL was introduced in CMake 3.7. We use NOT ${X} VERSION_LESS for compatibility with CMake 3.5.
# Based on performance tests, we only want this enabled by default on Windows.
if (NOT ${CMAKE_VERSION} VERSION_LESS "3.16.0" AND WIN32)
    set(DD_OPT_UNITY_BUILDS_DEFAULT ON)
else()
    set(DD_OPT_UNITY_BUILDS_DEFAULT OFF)
endif()
option(
    DD_OPT_UNITY_BUILDS "Optionally build all devdriver CMake targets with unity builds. Can be overwritten with CMAKE_UNITY_BUILD."
    ${DD_OPT_UNITY_BUILDS_DEFAULT})

macro(apply_devdriver_build_flags _target)

    set(DD_OPT_CPP_STD "" CACHE STRING "Passed to CMake's CXX_STANDARD to define the C++ standard")

    if(DD_OPT_CPP_STD)

        set_target_properties(${_target} PROPERTIES CXX_STANDARD ${DD_OPT_CPP_STD})

    else()

        set_target_properties(${_target} PROPERTIES CXX_STANDARD 11)

    endif()

    # Do not fallback to c++98 if the compiler does not support the requested standard
    set_target_properties(${_target} PROPERTIES CXX_STANDARD_REQUIRED TRUE)

    # Do not use flags like `-std=gnu++11`, instead use `-std=c++11`.
    set_target_properties(${_target} PROPERTIES CXX_EXTENSIONS FALSE)

    # Make a DD_SHORT_FILE macro that includes a shorter, partial file path.
    # The additional / is important to remove the last character from the path.
    # Note that it does not matter if the OS uses / or \, because we are only
    # saving the path size.
    string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
    target_compile_definitions(${_target} PUBLIC "DD_SHORT_FILE=(__FILE__+${SOURCE_PATH_SIZE})")

    if (DD_OPT_VERBOSE_STATIC_ASSERTS)
        target_compile_definitions(${_target} PUBLIC DD_OPT_VERBOSE_STATIC_ASSERTS)
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

        if (DD_OPT_FORCE_COLOR_OUPUT)

            if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)

                # For details on customizing this, see the docs:
                #   https://gcc.gnu.org/onlinedocs/gcc-5.2.0/gcc/Language-Independent-Options.html
                target_compile_options(${_target}
                    PRIVATE
                        -fdiagnostics-color
                )

            endif()

        endif()

     elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

        target_compile_options(${_target}
            PRIVATE
                # No Clang-specific options yet
        )

        if (DD_OPT_FORCE_COLOR_OUPUT)
            target_compile_options(${_target}
                PRIVATE
                    -fcolor-diagnostics
            )
        endif()

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")

        target_compile_options(${_target}
            PRIVATE
                # No AppleClang-specific options yet
        )

        if (DD_OPT_FORCE_COLOR_OUPUT)
            target_compile_options(${_target}
                PRIVATE
                    -fcolor-diagnostics
            )
        endif()

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

        target_compile_options(${_target}
            PRIVATE
                # Additional static analysis. This can be loud, so we disable some of the warnings this enables
                $<$<CONFIG:Debug>:/analyze>
        )

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

# Apply/disable warnings unique to DevDriver targets.
# Warnings are treated as Errors when DD_OPT_WARNINGS_AS_ERRORS=ON
function(apply_devdriver_warnings ${name})

    if (DD_OPT_WARNINGS_AS_ERRORS)

        if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

            target_compile_options(${name}
                PRIVATE
                    /WX
            )

        else()

            target_compile_options(${name}
                PRIVATE
                    -Werror
            )

        endif()

    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

        # Apply special options for GCC 8.x+
        if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)

            target_compile_options(${name}
                PRIVATE
                    # This warning triggers when you memcpy into or out of a "non trivial" type.
                    # The requirements for "trivial type" are hard - e.g. some user supplied constructors are enough to make
                    #   it not count.
                    #   Properly fixing this would require embracing more C++14 than we currently do. (e.g. `= default` ctors)
                    #   This warning is new in gcc 8.x
                    -Wno-class-memaccess
            )

        endif()

        # Apply special options for versions earlier than GCC 5.x
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)

            target_compile_options(${name}
                PRIVATE
                    # This warning triggers when we default initialize structures with the "StructType x = {};" syntax.
                    # It only triggers on GCC 4.8
                    -Wno-missing-field-initializers
            )

        endif()

     elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

        target_compile_options(${name}
            PRIVATE
                # No Clang-specific options yet
        )

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")

        target_compile_options(${name}
            PRIVATE
                # No AppleClang-specific options yet
        )

    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")

        target_compile_options(${name} PRIVATE
            # Disable specific warnings
            /wd4127 # Conditional expression is constant
            /wd4201 # Nonstandard extension used : nameless struct/union
            /wd4512 # Assignment operator could not be generated
            /wd6326 # Potential comparison of a constant with another constant

            # Enable extra warnings
            /we4296 # Unsigned integer comparison is constant
            /we5038 # Initialization order
            /we4746 # Warn about potential issues with MSVC's two modes of volatile
        )

    else()

        message(FATAL_ERROR "Using unknown compiler: ${CMAKE_CXX_COMPILER_ID}")

    endif()

endfunction()

# Apply build config options unique to DevDriver targets
function(apply_devdriver_build_configs name)

    # Do not overwrite the CMake system-wide define.
    if (NOT DEFINED CMAKE_UNITY_BUILD)
        set_target_properties(${name}
            PROPERTIES
                UNITY_BUILD ${DD_OPT_UNITY_BUILDS}
        )
    endif()

    # Set some standard output directories
    set_target_properties(${name}
        PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

endfunction()

function(devdriver_target name)

    amd_target(${name} ${ARGN})

    # Interface libraries cannot have many of their properties set
    get_target_property(target_type ${name} TYPE)
    if (NOT ${target_type} STREQUAL "INTERFACE_LIBRARY")

        apply_devdriver_build_flags(${name})
        apply_devdriver_build_configs(${name})

        # Apply this last, since it may override previous options
        apply_devdriver_warnings(${name})

    endif()

endfunction()

function(devdriver_executable name)

    amd_executable(${name} ${ARGN})
    devdriver_target(${name})

endfunction()

function(devdriver_library name type)

    #if DD_CLOSED_SOURCE
    # WA: For now, we need to use special functions to make KM libraries
    if(DD_TOOLCHAIN_WA_BUILD_KERNELMODE)

        wdk_add_library(${name} ${type})
        amd_target(${name})
        devdriver_target(${name} ${type})

    else()
    #endif

        amd_library(${name} ${type} ${ARGN})
        devdriver_target(${name})

    #if DD_CLOSED_SOURCE
    endif()
    #endif

endfunction()
