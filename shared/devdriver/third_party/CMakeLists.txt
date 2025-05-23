##
 #######################################################################################################################
 #
 #  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

# We cannot fix warnings in third-party code, so we silence them instead.
# This should never be used on code you can easily modify.
function(dd_third_party_disable_warnings name)

    get_target_property(target_type ${name} TYPE)
    if (${target_type} STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

        # [GCC] Options to Request or Suppress Warnings
        #   https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
        target_compile_options(${name}
            PRIVATE
                -w
        )

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

        target_compile_options(${name}
            PRIVATE
                /W0
        )

    else()

        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")

    endif()

endfunction()

function(dd_mark_third_party_target target)

    amd_target_options(${target})
    dd_third_party_disable_warnings(${target})

endfunction()

# dd_third_party_add_lib: Add a third party library.
#   This function is a no-op if the target in question already exists,
#   otherwise it creates it by calling `add_subdirectory()` on the directory specified with DIRECTORY, or by TARGET.
#
#   Named Arguments:
#       [TARGET target]       Name of target that should be created. Causes a fatal error when the target isn't created
#       [DIRECTORY directory] Directory to pass to `add_subdirectory()`. If omitted, defaults to value of TARGET
#
#   Basic Usage:
#       dd_third_party_add_lib(TARGET mpack)
#
#   Advanced Usage:
#       dd_third_party_add_lib(TARGET mpack DIRECTORY submodules/mpack)
#
function(dd_third_party_add_lib)

    ## This parses the arguments into the format we expect.
    ## For comprehensive documentation, see the CMake Docs:
    ## https://cmake.org/cmake/help/v3.10/command/cmake_parse_arguments.html#command:cmake_parse_arguments
    # 'options' is a list of things that are used or not - like `--flags` in CLI apps.
    set(options         "")
    # 'multiValueArgs' is a list of things that have one or more arguments.
    set(multiValueArgs  TARGETS)
    # 'oneValueArgs' is a list of things that expect exactly one argument, like `-j12`, `-j=12`, or `-j 12`.
    set(oneValueArgs    DIRECTORY)

    cmake_parse_arguments(third_party "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (third_party_UNPARSED_ARGUMENTS)

        message(FATAL_ERROR "Unexpected arguments: ${third_party_UNPARSED_ARGUMENTS}")

    endif()

    if (third_party_KEYWORDS_MISSING_VALUES)

        message(FATAL_ERROR "The following arguments are missing their values: ${third_party_KEYWORDS_MISSING_VALUES}")

    endif()

    if (NOT third_party_DIRECTORY)

        set(third_party_DIRECTORY ${third_party_TARGET})

    endif()

    message(VERBOSE "Adding third party targets \"${third_party_TARGETS}\" from ${CMAKE_CURRENT_SOURCE_DIR}/${third_party_DIRECTORY}")
    add_subdirectory("${third_party_DIRECTORY}" EXCLUDE_FROM_ALL)

    set(BAD_TARGETS "")

    foreach(target ${third_party_TARGETS})
        if (TARGET ${target})

            dd_mark_third_party_target(${target})

        else()

            list(APPEND BAD_TARGETS "${target}")
            message(WARNING "Failed to add third party target ${target}")

        endif()
    endforeach()

    if (BAD_TARGETS)
        message(FATAL_ERROR "Failed to add some third party targets")
    endif()

endfunction()

set(CMAKE_FOLDER ${CMAKE_FOLDER}/third_party)

### MessagePack Reader/Writer ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET mpack)

    dd_third_party_add_lib(
        TARGETS   mpack
        DIRECTORY mpack
    )

endif()

### JSON Reader/Writer ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET rapidjson
)

    dd_third_party_add_lib(
        TARGETS   rapidjson
        DIRECTORY rapidjson
    )

endif()

### Fast Hasher ###
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if (NOT TARGET metrohash)

    dd_third_party_add_lib(
        TARGETS   metrohash
        DIRECTORY metrohash
    )

endif()

### Path Utility Library ###
# WA: Check that these targets haven't already been added (Only until we have a global CMake context)
if (NOT TARGET cwalk)

    dd_third_party_add_lib(
        TARGETS   cwalk
        DIRECTORY cwalk
    )

endif()

### C++ Unit Tests ###
# WA: Check that these targets haven't already been added (Only until we have a global CMake context)
if (DEVDRIVER_IS_TOP_LEVEL)
    # This is only used when building test (as it's a testing framework), so we disable it otherwise.
    # Building this is nontrivial, so we want to be conservative about when we do.
    if (NOT TARGET gtest)

        dd_third_party_add_lib(
            TARGETS   gtest
            DIRECTORY gtest
        )

    endif()
endif()

dd_third_party_add_lib(
    TARGETS dd_libyaml
    DIRECTORY libyaml
)

dd_third_party_add_lib(
    TARGETS   stb_sprintf
    DIRECTORY stb_sprintf
)
