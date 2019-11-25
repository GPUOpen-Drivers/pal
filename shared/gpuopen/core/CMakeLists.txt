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
project(DevDriverCore CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

### Create ddCore Library #############################################################################################
devdriver_library(ddCore STATIC)

### Cached Project Options #############################################################################################
option(DEVDRIVER_SHOW_DEBUG_LABELS "Show debug labels in various log outputs?" ON)

set(DEVDRIVER_LOG_LEVEL ${DEVDRIVER_LOG_LEVEL} CACHE STRING "Developer driver mode log level? [0-4]")

if(UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(ddCore PRIVATE Threads::Threads)
endif()

# Support for dynamic library loading
target_link_libraries(ddCore PRIVATE ${CMAKE_DL_LIBS})

# TODO: Should be in DevDriver.cmake
if(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # Only apply static analysis warnings to the gpuopen library itself. This can break external projects
        # so it is dangerous to add to apply_gpuopen_warnings above. We also only build it on debug, as
        # Release does not include the various static analysis hints (e.g., noreturn) necessary for it to
        # compile without errors.
        target_compile_options(ddCore PRIVATE $<$<CONFIG:Debug>:/analyze>)
    endif()
endif()

### Build Defines ######################################################################################################

# Also enable these features from CMake
option(DEVDRIVER_ASSERTS_ENABLE     "Print info when asserts fail" OFF)
option(DEVDRIVER_ASSERTS_DEBUGBREAK "Trigger a debug break when asserts fail - implies DEVDRIVER_ASSERTS_ENABLE" OFF)

# Enable features with debug builds unconditionally
target_compile_definitions(ddCore PUBLIC
        $<$<CONFIG:Debug>:DEVDRIVER_ASSERTS_ENABLE>
        $<$<CONFIG:Debug>:DEVDRIVER_ASSERTS_DEBUGBREAK>)

if (DEVDRIVER_ASSERTS_DEBUGBREAK)
    set(DEVDRIVER_ASSERTS_ENABLE ON)
    target_compile_definitions(ddCore PUBLIC DEVDRIVER_ASSERTS_DEBUGBREAK)
endif()

if (DEVDRIVER_ASSERTS_ENABLE)
    target_compile_definitions(ddCore PUBLIC DEVDRIVER_ASSERTS_ENABLE)
endif()

if(DEVDRIVER_LOG_LEVEL)
    target_compile_definitions(ddCore PUBLIC DEVDRIVER_LOG_LEVEL=${DEVDRIVER_LOG_LEVEL})
endif()

if(DEVDRIVER_SHOW_DEBUG_LABELS)
    target_compile_definitions(ddCore PRIVATE $<$<CONFIG:Debug>:         DEVDRIVER_SHOW_DEBUG_LABELS>
                                              $<$<CONFIG:RelWithDebInfo>:DEVDRIVER_SHOW_DEBUG_LABELS>)
endif()

### Include Directories ################################################################################################
target_include_directories(ddCore PUBLIC ../inc)
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
            "DD_PLATFORM_IS_GNU=1")
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
