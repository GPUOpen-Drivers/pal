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

cmake_minimum_required(VERSION 3.5)

project(GpuOpen)

### CMake Includes #####################################################################################################
include(AmdCmakeHelper)

### Create GpuOpen Library #############################################################################################
add_library(gpuopen STATIC "")

### Cached Project Options #############################################################################################
option(DEVDRIVER_DEBUG_BUILD "Build ${PROJECT_NAME} in debug mode?" ${CMAKE_BUILD_TYPE_DEBUG})

if(CMAKE_BUILD_TYPE_DEBUG)
    set(DEVDRIVER_LOG_LEVEL 0)
else()
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

# Warnings Helper #####################################################################################################
macro(apply_gpuopen_warnings _target)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(${_target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endmacro()

# Apply the standard gpuopen warnings to the library.
apply_gpuopen_warnings(gpuopen)

### Compiler Options ###################################################################################################
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
    # for a list of all options and documentation.
    target_compile_options(gpuopen PRIVATE -pthread -std=c++0x)
    target_compile_options(gpuopen PRIVATE -fPIC -fno-exceptions -fno-rtti)
    target_link_libraries(gpuopen PRIVATE pthread)

    message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    #message(STATUS "Configured compiler options for Clang.")
    message(WARNING "Clang is untested.")
else()
    message(FATAL_ERROR "Using unknown compiler.")
endif()

### Build Defines ######################################################################################################
target_compile_definitions(gpuopen PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

if(DEVDRIVER_DEBUG_BUILD)
    target_compile_definitions(gpuopen PRIVATE
        DEVDRIVER_FORCE_ASSERT
        DEVDRIVER_HARD_ASSERT
        DEVDRIVER_FAKEIOCTL
    )
endif()

if(DEVDRIVER_LOG_LEVEL)
    target_compile_definitions(gpuopen PRIVATE DEVDRIVER_LOG_LEVEL=${DEVDRIVER_LOG_LEVEL})
endif()

### Include Directories ################################################################################################
target_include_directories(gpuopen PRIVATE ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared)

target_include_directories(gpuopen PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/inc)

### Sources ############################################################################################################

### Core Components ###
target_sources(gpuopen PRIVATE
    ${PROJECT_SOURCE_DIR}/src/session.cpp
    ${PROJECT_SOURCE_DIR}/src/sessionManager.cpp
    ${PROJECT_SOURCE_DIR}/src/messageChannel.inl
    ${PROJECT_SOURCE_DIR}/src/baseProtocolServer.cpp
    ${PROJECT_SOURCE_DIR}/src/baseProtocolClient.cpp
    ${PROJECT_SOURCE_DIR}/src/ddTransferManager.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddTransferServer.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddTransferClient.cpp
    ${PROJECT_SOURCE_DIR}/src/ddClientURIService.cpp
    ${PROJECT_SOURCE_DIR}/src/protocols/ddURIServer.cpp
)

### Platform ###
if(UNIX)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/posix/ddPosixPlatform.cpp
    )
endif()

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/posix/ddPosixSocket.cpp
        ${PROJECT_SOURCE_DIR}/src/socketMsgTransport.cpp
    )
endif()

### Helper Classes ###
if(GPUOPEN_BUILD_SERVER_HELPERS)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/devDriverServer.cpp
    )
endif()
if(GPUOPEN_BUILD_CLIENT_HELPERS)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/devDriverClient.cpp
    )
endif()

### Standard Driver Protocols ###
if(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER ON)
endif()

### Standard Tool Protocols ###
if(GPUOPEN_BUILD_STANDARD_TOOL_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER ON)
endif()

### Standard Listener Protocols ###
if(GPUOPEN_BUILD_STANDARD_LISTENER_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_ETW_SERVER ON)
endif()

### Listener Components ###
if(GPUOPEN_BUILD_LISTENER_CORE)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/listener/listenerCore.cpp
        ${PROJECT_SOURCE_DIR}/listener/routerCore.cpp
        ${PROJECT_SOURCE_DIR}/listener/transportThread.cpp
        ${PROJECT_SOURCE_DIR}/listener/hostMsgTransport.cpp
        ${PROJECT_SOURCE_DIR}/listener/listenerServer.cpp
        ${PROJECT_SOURCE_DIR}/listener/ddListenerURIService.cpp
        ${PROJECT_SOURCE_DIR}/listener/transports/hostTransport.cpp
        ${PROJECT_SOURCE_DIR}/listener/transports/socketTransport.cpp
        ${PROJECT_SOURCE_DIR}/listener/clientmanagers/listenerClientManager.cpp
    )

    target_include_directories(gpuopen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/listener)
endif()

#################
### Protocols ###
#################

### Driver Control Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/driverControlServer.cpp
    )
endif()
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/driverControlClient.cpp
    )
endif()

### Settings Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/settingsServer.cpp
    )
endif()
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/settingsClient.cpp
    )
endif()

### Logging Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/loggingServer.cpp
    )
endif()
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/loggingClient.cpp
    )
endif()

### RGP Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/rgpServer.cpp
    )
endif()
if(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/rgpClient.cpp
    )
endif()

### Crash Dump Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/ddGpuCrashDumpServer.cpp
    )
endif()
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/ddGpuCrashDumpClient.cpp
    )
endif()

### URI Client ###
if(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/ddURIClient.cpp
    )
endif()

### ETW Protocol ###

if(GPUOPEN_BUILD_PROTOCOL_ETW_CLIENT)
    target_sources(gpuopen PRIVATE
        ${PROJECT_SOURCE_DIR}/src/protocols/etwClient.cpp
    )
endif()

### Visual Studio Filters ##############################################################################################
target_vs_filters(gpuopen)
