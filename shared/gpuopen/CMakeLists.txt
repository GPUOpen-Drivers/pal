##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

project(GPUOPEN CXX)

### CMake Includes #####################################################################################################
include(AmdCmakeHelper)

### Create GPUOPEN Library ############################################################################################
add_library(gpuopen STATIC "")
install(TARGETS gpuopen DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE})

### Cached Project Options #############################################################################################
option(DEVDRIVER_DEBUG_BUILD "Build ${PROJECT_NAME} in debug mode?" ${CMAKE_BUILD_TYPE_DEBUG})

if(CMAKE_BUILD_TYPE_DEBUG)
    set(DEVDRIVER_LOG_LEVEL 0)
elseif(CMAKE_BUILD_TYPE_RELEASE)
    set(DEVDRIVER_LOG_LEVEL 4)
endif()

set(DEVDRIVER_LOG_LEVEL ${DEVDRIVER_LOG_LEVEL} CACHE STRING "Developer driver mode log level?")
set(DEVDRIVER_LOG_LEVEL_OPTIONS 0 1 2 3 4)
dropdown_option(DEVDRIVER_LOG_LEVEL DEVDRIVER_LOG_LEVEL_OPTIONS)

if(NOT DEFINED GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION)
    file(STRINGS inc/gpuopen.h GPUOPEN_MAJOR_VERSION REGEX "^#define GPUOPEN_INTERFACE_MAJOR_VERSION [0-9]+")

    if(GPUOPEN_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find GPUOPEN_INTERFACE_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "^#define GPUOPEN_INTERFACE_MAJOR_VERSION " "" GPUOPEN_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
        message(STATUS "Detected GPUOPEN_INTERFACE_MAJOR_VERSION is " ${GPUOPEN_MAJOR_VERSION})
    endif()
    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
endif()

# CMAKE-TODO: As there is some difficulty specifying multiple libs to link to with the AMD make system
# embedded builds will be used instead.
# ${PROJECT_SOURCE_DIR}/ also has to be added in front of the sources.
set(DEVDRIVER_SCOPE PRIVATE)
if(AMD_MAKE_BUILD)
    option(DEVDRIVER_EMBEDDED "Build ${PROJECT_NAME} embedded into parent project?" ON)
    if(DEVDRIVER_EMBEDDED)
        set(DEVDRIVER_SCOPE PUBLIC)
    endif()
endif()

### Compiler Options ###################################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(gpuopen PRIVATE -pthread -fPIC)
        target_compile_options(gpuopen PRIVATE -Wno-unused -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers)
        target_compile_options(gpuopen PRIVATE -fno-exceptions -std=c++0x -fno-rtti)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Build Defines ######################################################################################################
target_compile_definitions(gpuopen PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

if(DEVDRIVER_DEBUG_BUILD)
    target_compile_definitions(gpuopen ${DEVDRIVER_SCOPE}
        DEVDRIVER_FORCE_ASSERT
        DEVDRIVER_HARD_ASSERT
        DEVDRIVER_FAKEIOCTL
    )
endif()

if(DEVDRIVER_LOG_LEVEL)
    target_compile_definitions(gpuopen ${DEVDRIVER_SCOPE} DEVDRIVER_LOG_LEVEL=${DEVDRIVER_LOG_LEVEL})
endif()

### Include Directories ################################################################################################
target_include_directories(gpuopen ${DEVDRIVER_SCOPE} ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared)

target_include_directories(gpuopen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

### Sources ############################################################################################################
# Build core developer driver server support
target_sources(gpuopen ${DEVDRIVER_SCOPE}
    ${PROJECT_SOURCE_DIR}/src/baseProtocolServer.cpp
    ${PROJECT_SOURCE_DIR}/src/devDriverServer.cpp
    ${PROJECT_SOURCE_DIR}/src/session.cpp
    ${PROJECT_SOURCE_DIR}/src/sessionManager.cpp
    ${PROJECT_SOURCE_DIR}/src/baseProtocolClient.cpp
    ${PROJECT_SOURCE_DIR}/src/ddTransferManager.cpp
    ${PROJECT_SOURCE_DIR}/src/ddClientURIService.cpp
)

# Build support for protocols
target_sources(gpuopen ${DEVDRIVER_SCOPE}
    ${PROJECT_SOURCE_DIR}/src/protocols/driverControlServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/loggingServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/rgpServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/settingsServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddTransferServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddTransferClient.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddURIServer.cpp
)

if(UNIX)
    target_sources(gpuopen ${DEVDRIVER_SCOPE}
        ${PROJECT_SOURCE_DIR}/src/posix/ddPosixPlatform.cpp
        ${PROJECT_SOURCE_DIR}/src/posix/ddPosixSocket.cpp
        ${PROJECT_SOURCE_DIR}/src/socketMsgTransport.cpp
    )
endif()

### Visual Studio Filters ##############################################################################################
target_vs_filters(gpuopen)
