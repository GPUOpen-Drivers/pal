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

target_sources(pal PRIVATE CMakeLists.txt)

### LIBUUID ####################################################################
add_subdirectory(libuuid)
target_link_libraries(pal PRIVATE pal_uuid)

### METROHASH ##################################################################

# Metrohash needs to be a public dependency.
# See: palMetroHash.h

if (NOT TARGET metrohash)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pal_metrohash")
        add_subdirectory(pal_metrohash)
    elseif(EXISTS "${GLOBAL_ROOT_SRC_DIR}/drivers/third_party/metrohash")
        add_subdirectory("${GLOBAL_ROOT_SRC_DIR}/drivers/third_party/metrohash" "${PROJECT_BINARY_DIR}/metrohash")
    else()
        message(FATAL_ERROR "MetroHash not found (either vendored or in drivers/third_party)")
    endif()
endif()

target_link_libraries(pal PUBLIC metrohash)

### CWPACK #####################################################################

# cwpack needs to be a public dependency.
# See: palMsgPack.h

if (NOT TARGET cwpack)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pal_cwpack")
        add_subdirectory(pal_cwpack)
    elseif(EXISTS "${GLOBAL_ROOT_SRC_DIR}/drivers/third_party/cwpack")
        add_subdirectory("${GLOBAL_ROOT_SRC_DIR}/drivers/third_party/cwpack" "${PROJECT_BINARY_DIR}/cwpack")
    else()
        message(FATAL_ERROR "CWPack not found (either vendored or in drivers/third_party)")
    endif()
endif()

target_link_libraries(pal PUBLIC cwpack)

### LZ4 ########################################################################
add_subdirectory(pal_lz4)
target_link_libraries(pal PRIVATE pal_lz4)

### DevDriver ####################################################################

set(PAL_DEVDRIVER_PATH "default" CACHE PATH "Specify the path to the devdriver project.")

if (${PAL_DEVDRIVER_PATH} STREQUAL "default")
    add_subdirectory(${PAL_SOURCE_DIR}/shared/devdriver ${PROJECT_BINARY_DIR}/devdriver)
else()
    add_subdirectory(${PAL_DEVDRIVER_PATH} ${PROJECT_BINARY_DIR}/devdriver_overridden)
endif()

# Currently driver will fail if we don't add this call...
enable_language(C)

# DevDriver needs to be a public dependency.
# Clients include "gpuopen.h"
target_link_libraries(pal
    PUBLIC
        ddSettings
        devdriver
)
