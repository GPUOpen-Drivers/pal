cmake_minimum_required(VERSION 3.10..3.16)
project(DevDriverThirdParty CXX)

set(CMAKE_FOLDER "${CMAKE_FOLDER}/Third Party")

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

        #if DD_ACCESS_INTERNAL
        # [MSVC] Warning Level
        #   https://docs.microsoft.com/en-us/cpp/build/reference/compiler-option-warning-level
        #endif
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

    # Only libraries need PIC enabled
    get_target_property(target_type ${target} TYPE)
    if (target_type STREQUAL "STATIC_LIBRARY")

        set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)

    endif()

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
#       dd_third_party_add_lib(TARGET mpack DIRECTORY git-submodules/mpack)
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
#if DD_CLOSED_SOURCE
# WA: We will move this logic into a toolchain
AND NOT DD_TOOLCHAIN_WA_BUILD_KERNELMODE
#endif
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

### Fast (De)Compression ###
# WA: Check that these targets haven't already been added (Only until we have a global CMake context)
if (NOT TARGET lz4)

    dd_third_party_add_lib(
        TARGETS   lz4 xxhash
        DIRECTORY lz4
    )

endif()

### Locate the current executable and module in the filesystem ###
# WA: Check that these targets haven't already been added (Only until we have a global CMake context)
if (NOT TARGET whereami)

    dd_third_party_add_lib(
        TARGETS   whereami
        DIRECTORY whereami
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
if (DD_ENABLE_TESTS)
    # This is only used when building test (as it's a testing framework), so we disable it otherwise.
    # Building this is nontrivial, so we want to be conservative about when we do.
    if (NOT TARGET gtest)

        dd_third_party_add_lib(
            TARGETS   gtest
            DIRECTORY gtest
        )

    endif()
endif()

### Cross platform printf with bonus features ###
# WA: Check that these targets haven't already been added (Only until we have a global CMake context)
if (NOT TARGET tiny_printf)

    dd_third_party_add_lib(
        TARGETS   tiny_printf
        DIRECTORY tiny_printf
    )

endif()
