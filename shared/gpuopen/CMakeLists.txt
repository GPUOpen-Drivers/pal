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
project(GPUOPEN)

### CMake Includes #####################################################################################################
include(AmdCmakeHelper)

### Create GpuOpen Library #############################################################################################
set(CMAKE_CXX_STANDARD          14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
add_library(gpuopen STATIC "")

### Cached Project Options #############################################################################################

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

# Warning Options ######################################################################################################
macro(apply_gpuopen_warnings _target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|(Apple)?Clang)$")
        target_compile_options(${_target} PRIVATE
            -Wall -Wextra
            # This warning triggers when you memcpy into or out of a "non trivial" type.
            # The requirements for "trivial type" are hard - e.g. some user supplied constructors are enough to make
            #   it not count.
            #   Properly fixing this would require embracing more C++14 than we currently do. (e.g. `= default` ctors)
            #   Read more here: https://msdn.microsoft.com/en-us/library/mt767760.aspx
            #   This warning is new in gcc 8.x
            -Wno-class-memaccess
        )
    else()
        message(FATAL_ERROR "Using unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endmacro()

# Apply the standard gpuopen warnings to the library.
apply_gpuopen_warnings(gpuopen)

### Compiler Options ###################################################################################################

# CMAKE-TODO: Include all used compiler features.
target_compile_features(gpuopen PUBLIC cxx_strong_enums)

set_target_properties(gpuopen PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(gpuopen Threads::Threads)

    if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|(Apple)?Clang)$")
        target_compile_options(gpuopen PRIVATE
            -fno-exceptions
            -fno-rtti
        )
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

### Build Defines ######################################################################################################
target_compile_definitions(gpuopen PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

# Add NDEBUG flag in release builds since several code files depend on it.
target_compile_definitions(gpuopen PRIVATE $<$<CONFIG:Release>:NDEBUG>)

# Enable features with debug builds
target_compile_definitions(gpuopen PRIVATE
        $<$<CONFIG:Debug>:DEVDRIVER_FORCE_ASSERT>      # Force asserts on
        $<$<CONFIG:Debug>:DEVDRIVER_HARD_ASSERT>)      # Always throw a hard assert

if (GPUOPEN_PRINT_TO_CONSOLE)
    target_compile_definitions(gpuopen PRIVATE DEVDRIVER_PRINT_TO_CONSOLE)
endif()

if(DEVDRIVER_LOG_LEVEL)
    target_compile_definitions(gpuopen PRIVATE DEVDRIVER_LOG_LEVEL=${DEVDRIVER_LOG_LEVEL})
endif()

### Include Directories ################################################################################################
target_include_directories(gpuopen PUBLIC inc)
target_include_directories(gpuopen PRIVATE src)

### Sources ############################################################################################################

### Core Components ###
target_sources(gpuopen PRIVATE
    src/session.cpp
    src/sessionManager.cpp
    src/messageChannel.inl
    src/baseProtocolServer.cpp
    src/baseProtocolClient.cpp
    src/ddTransferManager.cpp
    src/ddURIRequestContext.cpp
    src/protocols/ddTransferServer.cpp
    src/protocols/ddTransferClient.cpp
    src/ddClientURIService.cpp
    src/protocols/ddURIServer.cpp
    src/protocols/ddSettingsService.cpp
    src/protocols/ddPipelineUriService.cpp
    src/util/ddTextWriter.cpp
    src/util/ddJsonWriter.cpp
)

# If the client doesn't specify, default to building internal metrohash library
if (NOT DEFINED GPUOPEN_BUILD_METROHASH)
    set(GPUOPEN_BUILD_METROHASH ON)
endif()

# If a MetroHash path is not specified then we fall back to compiling our internal copy
if (NOT DEFINED METROHASH_PATH)
    set(GPUOPEN_BUILD_METROHASH ON)
endif()

if (GPUOPEN_BUILD_METROHASH)
    set(METROHASH_PATH "src/imported/metrohash/src")
    target_sources(gpuopen PRIVATE
        ${METROHASH_PATH}/metrohash64.cpp
        ${METROHASH_PATH}/metrohash128.cpp)
endif()

target_include_directories(gpuopen PRIVATE ${METROHASH_PATH})

### RapidJson ###
target_include_directories(gpuopen PUBLIC "inc/imported/rapidjson/include")

### Platform ###
if(UNIX)
    target_sources(gpuopen PRIVATE src/posix/ddPosixPlatform.cpp)
endif()

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(gpuopen PRIVATE
        src/posix/ddPosixSocket.cpp
        src/socketMsgTransport.cpp
    )
endif()

### Helper Classes ###
if(GPUOPEN_BUILD_SERVER_HELPERS)
    target_sources(gpuopen PRIVATE src/devDriverServer.cpp)
endif()
if(GPUOPEN_BUILD_CLIENT_HELPERS)
    target_sources(gpuopen PRIVATE src/devDriverClient.cpp)
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
        listener/listenerCore.cpp
        listener/routerCore.cpp
        listener/transportThread.cpp
        listener/hostMsgTransport.cpp
        listener/listenerServer.cpp
        listener/ddListenerURIService.cpp
        listener/transports/hostTransport.cpp
        listener/transports/socketTransport.cpp
        listener/clientmanagers/listenerClientManager.cpp
    )

    target_include_directories(gpuopen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/listener)
endif()

#################
### Protocols ###
#################

### Driver Control Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER)
    target_sources(gpuopen PRIVATE src/protocols/driverControlServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/driverControlClient.cpp)
endif()

### Settings Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER)
    target_sources(gpuopen PRIVATE src/protocols/settingsServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/settingsClient.cpp)
endif()

### Logging Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER)
    target_sources(gpuopen PRIVATE src/protocols/loggingServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/loggingClient.cpp)
endif()

### RGP Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER)
    target_sources(gpuopen PRIVATE src/protocols/rgpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/rgpClient.cpp)
endif()

### Crash Dump Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER)
    target_sources(gpuopen PRIVATE src/protocols/ddGpuCrashDumpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/ddGpuCrashDumpClient.cpp)
endif()

### URI Client ###
if(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/ddURIClient.cpp)
endif()

### ETW Protocol ###

if(GPUOPEN_BUILD_PROTOCOL_ETW_CLIENT)
    target_sources(gpuopen PRIVATE src/protocols/etwClient.cpp)
endif()

### Visual Studio Filters ##############################################################################################
target_find_headers(gpuopen)
target_source_groups(gpuopen)
