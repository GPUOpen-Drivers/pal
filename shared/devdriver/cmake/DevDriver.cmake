##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

include_guard()

# https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DEVDRIVER_IS_TOP_LEVEL ON)
else()
    set(DEVDRIVER_IS_TOP_LEVEL OFF)
endif()

# https://cmake.org/cmake/help/v3.21/policy/CMP0092.html?highlight=w3
# Our CI needs this outside the top level check, since we build devdriver via add_subdirectory.
# There isn't any harm in having this check outside the top level check anyway.
string(REPLACE " /W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

if (DEVDRIVER_IS_TOP_LEVEL)
    set(DD_BRANCH_STRING "dev")

    if (MSVC_IDE)
        # Build with Multiple Processes, this is a nice quality of life for VS users
        # This is the preferred way of speeding up vs builds.
        add_compile_options(/MP)

        # Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
        set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    endif()
endif()

option(DD_ENABLE_WARNINGS "Enforce a warning-clean build" ${DEVDRIVER_IS_TOP_LEVEL})

# DevDriver.cmake adds project-specific options and warnings.
# It does this while depending on the base AMD-wide configurations defined here.
# Users of this file should only `include(DevDriver)`
include(AMD)
include(DevDriverBP)

macro(apply_devdriver_build_flags _target)
    set(DD_OPT_CPP_STD "" CACHE STRING "Passed to CMake's CXX_STANDARD to define the C++ standard")
    if(DD_OPT_CPP_STD)
        set_target_properties(${_target} PROPERTIES CXX_STANDARD ${DD_OPT_CPP_STD})
    else()
        set_target_properties(${_target} PROPERTIES CXX_STANDARD 11)
    endif()

    set_target_properties(${_target} PROPERTIES
        # Ensure the standard is supported by the compiler
        CXX_STANDARD_REQUIRED TRUE
        # Use -std=c++11 rather than -std=gnu++11
        CXX_EXTENSIONS FALSE
    )

    # Make a DD_SHORT_FILE macro that includes a shorter, partial file path.
    # The additional / is important to remove the last character from the path.
    # Note that it does not matter if the OS uses / or \, because we are only
    # saving the path size.
    string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
    target_compile_definitions(${_target} PUBLIC "DD_SHORT_FILE=(__FILE__+${SOURCE_PATH_SIZE})")

    if (WIN32)
        # Allow usage of unsafe C run time functionality
        target_compile_definitions(${_target} PUBLIC
            "_CRT_INSECURE_NO_DEPRECATE"
            "_CRT_SECURE_NO_WARNINGS"
        )
    endif()
endmacro()

function(apply_devdriver_warnings ${name})
    if (NOT DD_ENABLE_WARNINGS)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${name} PRIVATE
            # Enable warnings about questionable language constructs.
            -Wall
            # Enable extra warnings that are not enabled by -Wall.
            -Wextra
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE
            # https://github.com/MicrosoftDocs/cpp-docs/blob/master/docs/build/reference/compiler-option-warning-level.md
            /W4
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE /WX)
    else()
        target_compile_options(${name} PRIVATE -Werror)
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
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE
            # Additional static analysis. This can be loud, so we disable some of the warnings this enables
            $<$<CONFIG:Debug>:/analyze>
        )

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
    endif()
endfunction()

# Apply build config options unique to DevDriver targets
function(apply_devdriver_build_configs name)
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

        amd_library(${name} ${type} ${ARGN})
        devdriver_target(${name})

endfunction()

## Introduce a new scope for various CMake project settings
#
# This introduces a new "scope" for CMake projects. Scopes function like a stack, so when this
# scope is "popped", the previous scope is restored.
#
# Currently, scopes include the following:
#   - CMAKE_FOLDER management of new targets. (targets can still override their property to opt-out)
#   - CMAKE_MESSAGE_CONTEXT for message() calls in this scope. This functionality is enabled with
#       CMAKE_MESSAGE_CONTEXT_SHOW=ON, or --log-context starting in CMake 3.17.
#       Before CMake 3.17, this functionality is ignored.
#
# This scope is "popped" automatically at the end of a CMake scope. Most commonly, this occurs
# at the end of a file.
# e.g. You can include `devdriver_push_scope("My Cool Folder") at the top of a CMakeLists.txt,
# and it will automatically pop the scope before "returning" to the previous CMakeLists.txt.
function(devdriver_push_scope name)
    list(LENGTH devdriver_cmake_folder_stack length_folder)
    list(LENGTH CMAKE_MESSAGE_CONTEXT length_ctx)

    ## Save the current CMAKE_FOLDER onto our own stack, and overwrite the current value
    # CMake does not provide a folder stack, so we maintain one ourselves.
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/${name}")
    list(APPEND devdriver_cmake_folder_stack "${CMAKE_FOLDER}")

    ## Append the next CMAKE_MESSAGE_CONTEXT to our stack
    # CMake already treats CMAKE_MESSAGE_CONTEXT like a stack, so we just use it directly.
    # Contexts must be valid CMake identifiers, which are roughly valid C identifiers
    string(MAKE_C_IDENTIFIER ${name} name_as_ident)
    # Use our scope name for the next context
    list(APPEND CMAKE_MESSAGE_CONTEXT ${name_as_ident})

    ## Note: We have to use PARENT_SCOPE to propagate anything to the PARENT_SCOPE

    # Save the CMAKE_FOLDER variable and its stack back into the parent scope
    set(CMAKE_FOLDER                 ${CMAKE_FOLDER}                 PARENT_SCOPE)
    set(devdriver_cmake_folder_stack ${devdriver_cmake_folder_stack} PARENT_SCOPE)
    # Save the CMAKE_MESSAGE_CONTEXT list back into the parent scope
    set(CMAKE_MESSAGE_CONTEXT        ${CMAKE_MESSAGE_CONTEXT}        PARENT_SCOPE)
endfunction()

## Remove a previously popped scope
#
# This removes the most recently pushed scope from `devdriver_push_scope`. This is not necessary for
# a correct program, but may be useful for direct control over the scope stack.
#
# Typical usage should probably let the CMake file scoping rules clear the scope on file exit.
function(devdriver_pop_scope)
    list(LENGTH devdriver_cmake_folder_stack length_folder)
    list(LENGTH CMAKE_MESSAGE_CONTEXT length_ctx)

    # CMake 3.15 introduced list(POP_BACK), but we need to handle previous versions.
    # In the future, when we require CMake >= 3.15, we can delete this check.
    if (CMAKE_VERSION LESS "3.15")
        ## Remove the last CMAKE_FOLDER that we saved
        if (devdriver_cmake_folder_stack)
            list(LENGTH devdriver_cmake_folder_stack folder_stack_length)
            math(EXPR CMAKE_FOLDER "${folder_stack_length} - 1")
            list(REMOVE_AT devdriver_cmake_folder_stack folder_stack_length)
        endif()

        ## Remove the last CMAKE_MESSAGE_CONTEXT that we saved
        if (CMAKE_MESSAGE_CONTEXT)
            list(LENGTH CMAKE_MESSAGE_CONTEXT context_stack_length)
            math(EXPR last_context "${context_stack_length} - 1")
            list(REMOVE_AT CMAKE_MESSAGE_CONTEXT context_stack_length)
        endif()
    else()
        ## Remove the last CMAKE_FOLDER that we saved
        list(POP_BACK devdriver_cmake_folder_stack CMAKE_FOLDER)

        ## Remove the last CMAKE_MESSAGE_CONTEXT that we saved
        list(POP_BACK CMAKE_MESSAGE_CONTEXT)
    endif()

    ## Note: We have to use PARENT_SCOPE to propagate anything to the PARENT_SCOPE

    # Save the CMAKE_FOLDER variable and its stack back into the parent scope
    set(CMAKE_FOLDER                 ${CMAKE_FOLDER}                 PARENT_SCOPE)
    set(devdriver_cmake_folder_stack ${devdriver_cmake_folder_stack} PARENT_SCOPE)
    # Save the CMAKE_MESSAGE_CONTEXT list back into the parent scope
    set(CMAKE_MESSAGE_CONTEXT        ${CMAKE_MESSAGE_CONTEXT}        PARENT_SCOPE)
endfunction()
