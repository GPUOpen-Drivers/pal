##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")

# https://cmake.org/cmake/help/latest/variable/PROJECT_IS_TOP_LEVEL.html
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DEVDRIVER_IS_TOP_LEVEL ON)
else()
    set(DEVDRIVER_IS_TOP_LEVEL OFF)
endif()

# If we are the top level project avoid unneeded nesting
if (NOT DEVDRIVER_IS_TOP_LEVEL)
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/Developer Driver")
    list(APPEND CMAKE_MESSAGE_CONTEXT "${PROJECT_NAME}")
endif()

if (DEVDRIVER_IS_TOP_LEVEL)
    set(DD_BRANCH_STRING "dev")

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)

    if (MSVC_IDE)
        # Build with Multiple Processes, this is a nice quality of life for VS users
        # This is the preferred way of speeding up VS builds.
        add_compile_options(/MP)

        # Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
        set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    endif()
endif()

if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    message(AUTHOR_WARNING "CMAKE_POSITION_INDEPENDENT_CODE not set!")
endif()

option(
    DD_ENABLE_WARNINGS
    "Enforce devdriver specific warning configuration (some warings are suppressed)."
    ${DEVDRIVER_IS_TOP_LEVEL})

# DevDriver.cmake adds project-specific options and warnings.
# It does this while depending on the base AMD-wide configurations defined here.
# Users of this file should only `include(DevDriver)`
include(AMD)
include(DevDriverBP)

macro(apply_devdriver_build_flags _target)
    set_target_properties(${_target} PROPERTIES
        CXX_STANDARD 17
        # Ensure the standard is supported by the compiler.
        CXX_STANDARD_REQUIRED TRUE
        # Use -std=c++17 rather than -std=gnu++17.
        CXX_EXTENSIONS FALSE
    )

    if (WIN32)
        # Allow usage of unsafe C run time functionality
        target_compile_definitions(${_target} PUBLIC
            "_CRT_INSECURE_NO_DEPRECATE"
            "_CRT_SECURE_NO_WARNINGS"
        )
    endif()
endmacro()

function(apply_devdriver_warnings name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${name} PRIVATE
            -Wall   # Enable all warnings.
            -Wextra # Enable extra warnings that are not enabled by -Wall.
            -Werror # Treat warnings as errors
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${name} PRIVATE
            /W4 # highest level of warnings
            /WX # warning as error
        )
    endif()

    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        if (DD_MSVC_CODE_ANALYZE)
            target_compile_options(${name} PRIVATE
                # Additional static analysis. This can be loud, so we disable
                # some of the warnings this enables
                $<$<CONFIG:Debug>:/analyze>
            )
        endif()

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

function(apply_devdriver_compile_definitions name)
    if (WIN32)
        target_compile_definitions(${name} PRIVATE DD_TARGET_PLATFORM_WINDOWS)
    elseif (LINUX)
        target_compile_definitions(${name} PRIVATE DD_TARGET_PLATFORM_LINUX)
    elseif (ANDROID)
        target_compile_definitions(${name} PRIVATE DD_TARGET_PLATFORM_ANDROID)
    endif()
endfunction()

function(devdriver_target name)
    amd_target(${name} ${ARGN})

    # Interface libraries cannot have many of their properties set
    get_target_property(target_type ${name} TYPE)
    if (NOT ${target_type} STREQUAL "INTERFACE_LIBRARY")

        apply_devdriver_build_flags(${name})
        apply_devdriver_build_configs(${name})
        apply_devdriver_compile_definitions(${name})

        # Apply this last, since it may override previous options
        if (DD_ENABLE_WARNINGS)
            apply_devdriver_warnings(${name})
        endif()
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

function(add_subdirectory_if_exist subdir)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${subdir})
        add_subdirectory(${subdir})
    endif()
endfunction()
