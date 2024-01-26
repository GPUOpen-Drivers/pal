##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

devdriver_library(ddGpuInfo STATIC)

target_link_libraries(ddGpuInfo
    PUBLIC
        # Link against ddCore to get standard devdriver defs
        ddCore
        devdriver_gpuinfo
)

# This library contains exactly one cpp file, chosen based on platform:
if (CMAKE_SYSTEM_NAME MATCHES "Linux|Android")
    if (ANDROID)
        # These flags allow CMake to search beyond the scope of the Android SDK. Libdrm will
        # not be picked up without this change.
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
    endif()

    # If this call fails install both libdrm-dev:amd64 and libdrm-dev:i386
    find_package(Libdrm REQUIRED)

    target_sources(ddGpuInfo PRIVATE src/ddLinuxAmdGpuInfo.cpp)
    target_include_directories(ddGpuInfo PRIVATE ${Libdrm_INCLUDE_DIRS})
elseif (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    target_sources(ddGpuInfo PRIVATE src/ddMacAmdGpuInfo.mm)
    target_link_libraries(ddGpuInfo PUBLIC "-framework Cocoa -framework Metal")
else()
    message(WARNING "No QueryGpuInfo() implementation available on ${CMAKE_SYSTEM_NAME} with current configuration")
    target_sources(ddGpuInfo PRIVATE src/ddEmptyAmdGpuInfo.cpp)
endif()
