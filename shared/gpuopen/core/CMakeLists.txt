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
project(DevDriverCore CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

### Create ddCore Library #############################################################################################
devdriver_library(ddCore STATIC)

### Cached Project Options #############################################################################################
option(DD_OPT_SHOW_DEBUG_LABELS "Show debug labels in various log outputs?" ON)

set(DD_OPT_LOG_LEVEL ${DD_OPT_LOG_LEVEL} CACHE STRING "Developer driver mode log level? [0-4]")

if(UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(ddCore PRIVATE Threads::Threads)
endif()

# Support for dynamic library loading
target_link_libraries(ddCore PRIVATE ${CMAKE_DL_LIBS})

# Portable printf
target_link_libraries(ddCore PRIVATE tiny_printf)

# Common Hashing Utility
# This is used in headers and needs needs to be public
target_link_libraries(ddCore PUBLIC metrohash)

### Build Defines ######################################################################################################

# Also enable these features from CMake
option(DD_OPT_ASSERTS_ENABLE     "Print info when asserts fail" OFF)
option(DD_OPT_ASSERTS_DEBUGBREAK "Trigger a debug break when asserts fail - implies DD_OPT_ASSERTS_ENABLE" OFF)

# Enable features with debug builds unconditionally
target_compile_definitions(ddCore PUBLIC
        $<$<CONFIG:Debug>:DD_OPT_ASSERTS_ENABLE>
        $<$<CONFIG:Debug>:DD_OPT_ASSERTS_DEBUGBREAK>)

if (DD_OPT_ASSERTS_DEBUGBREAK)
    set(DD_OPT_ASSERTS_ENABLE ON)
    target_compile_definitions(ddCore PUBLIC DD_OPT_ASSERTS_DEBUGBREAK)
endif()

if (DD_OPT_ASSERTS_ENABLE)
    target_compile_definitions(ddCore PUBLIC DD_OPT_ASSERTS_ENABLE)
endif()

if(DD_OPT_LOG_LEVEL)
    target_compile_definitions(ddCore PUBLIC DD_OPT_LOG_LEVEL=${DD_OPT_LOG_LEVEL})
endif()

if(DD_OPT_SHOW_DEBUG_LABELS)
    target_compile_definitions(ddCore PRIVATE $<$<CONFIG:Debug>:         DD_OPT_SHOW_DEBUG_LABELS>
                                              $<$<CONFIG:RelWithDebInfo>:DD_OPT_SHOW_DEBUG_LABELS>)
endif()

### Include Directories ################################################################################################
target_include_directories(ddCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../inc)
target_include_directories(ddCore PRIVATE src)

### Sources ############################################################################################################
target_sources(ddCore
    PRIVATE
        src/ddcPlatform.cpp
        src/ddcMemory.cpp
)
target_sources(ddCore
    PRIVATE
        inc/ddcDefs.h
        inc/ddcPlatform.h
        inc/ddcTemplate.h
)

### Platform ###

    target_compile_definitions(ddCore
        PUBLIC
            "-DDD_PLATFORM_STRING=\"${CMAKE_SYSTEM_NAME} User Mode\""
    )

if (CMAKE_SYSTEM_NAME MATCHES "Linux")

    target_compile_definitions(ddCore
        PUBLIC
            "DD_PLATFORM_LINUX_UM=1"
            "DD_PLATFORM_IS_UM=1"
            "DD_PLATFORM_IS_GNU=1"
            "DD_PLATFORM_IS_POSIX=1")
    target_sources(ddCore
        PRIVATE
            src/platforms/ddcPosixPlatform.cpp
            inc/platforms/ddcPosixPlatform.h)

elseif(CMAKE_SYSTEM_NAME MATCHES "Android")

    # Android is Linux, so we reuse the Linux platform.
    # Note however, that it is not GNU, so some GNU-things are not available on Android.
    target_compile_definitions(ddCore
        PUBLIC
            "DD_PLATFORM_LINUX_UM=1"
            "DD_PLATFORM_IS_UM=1"
            "DD_PLATFORM_IS_POSIX=1")
    target_sources(ddCore
        PRIVATE
            src/platforms/ddcPosixPlatform.cpp
            inc/platforms/ddcPosixPlatform.h)

elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")

    target_compile_definitions(ddCore
        PUBLIC
            "DD_PLATFORM_DARWIN_UM=1"
            "DD_PLATFORM_IS_UM=1"
            "DD_PLATFORM_IS_POSIX=1")
    target_sources(ddCore
        PRIVATE
            src/platforms/ddcPosixPlatform.cpp
            inc/platforms/ddcPosixPlatform.h)
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")

        target_compile_definitions(ddCore
            PUBLIC
                "DD_PLATFORM_WINDOWS_UM=1"
                "DD_PLATFORM_IS_UM=1")
        target_sources(ddCore
            PRIVATE
                src/platforms/ddcWinPlatform.cpp
                inc/platforms/ddcWinPlatform.h)

else()
    message(FATAL_ERROR "Unrecognized platform: ${CMAKE_SYSTEM_NAME} - cannot build platform abstraction")
endif()
