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
project(GPUOPEN CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

if(NOT DEFINED GPUOPEN_LIB_NAME)
    set(GPUOPEN_LIB_NAME gpuopen CACHE STRING "Name of generated gpuopen lib")
endif()

### Create GpuOpen Library #############################################################################################

    amd_um_library(${GPUOPEN_LIB_NAME} STATIC)

# Install the library into "${CMAKE_INSTALL_PREFIX}/lib"
install(TARGETS ${GPUOPEN_LIB_NAME} DESTINATION lib)

### Cached Project Options #############################################################################################

option(DEVDRIVER_SHOW_DEBUG_LABELS "Show debug labels in various log outputs?" ON)

set(DEVDRIVER_LOG_LEVEL ${DEVDRIVER_LOG_LEVEL} CACHE STRING "Developer driver mode log level? [0-4]")

if(NOT DEFINED GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION)
    file(STRINGS inc/gpuopen.h GPUOPEN_MAJOR_VERSION REGEX "^#define GPUOPEN_INTERFACE_MAJOR_VERSION [0-9]+")

    if(GPUOPEN_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find GPUOPEN_INTERFACE_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "^#define GPUOPEN_INTERFACE_MAJOR_VERSION " "" GPUOPEN_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
        message(STATUS "Detected GPUOPEN_INTERFACE_MAJOR_VERSION is " ${GPUOPEN_MAJOR_VERSION})
    endif()
    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION} CACHE STRING "Interface version for gpuopen")
endif()
message(STATUS "Building gpuopen lib as ${GPUOPEN_LIB_NAME} GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION}")

# Apply the standard gpuopen warnings to the library.
apply_gpuopen_warnings(${GPUOPEN_LIB_NAME})

### Compiler Options ###################################################################################################
if(UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC Threads::Threads)
endif()

### Build Defines ######################################################################################################
target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

# Add NDEBUG flag in release builds since several code files depend on it.
target_compile_definitions(${GPUOPEN_LIB_NAME} PRIVATE $<$<CONFIG:Release>:NDEBUG>)

# Also enable these features from CMake
option(DEVDRIVER_ASSERTS_ENABLE     "Print info when asserts fail" OFF)
option(DEVDRIVER_ASSERTS_DEBUGBREAK "Trigger a debug break when asserts fail - implies DEVDRIVER_ASSERTS_ENABLE" OFF)

# Enable features with debug builds unconditionally
target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC
        $<$<CONFIG:Debug>:DEVDRIVER_ASSERTS_ENABLE>
        $<$<CONFIG:Debug>:DEVDRIVER_ASSERTS_DEBUGBREAK>)

if (DEVDRIVER_ASSERTS_DEBUGBREAK)
    set(DEVDRIVER_ASSERTS_ENABLE ON)
    target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DEVDRIVER_ASSERTS_DEBUGBREAK)
endif()

if (DEVDRIVER_ASSERTS_ENABLE)
    target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DEVDRIVER_ASSERTS_ENABLE)
endif()

if (GPUOPEN_PRINT_TO_CONSOLE)
    target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DEVDRIVER_PRINT_TO_CONSOLE)
endif()

if(DEVDRIVER_LOG_LEVEL)
    target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DEVDRIVER_LOG_LEVEL=${DEVDRIVER_LOG_LEVEL})
endif()

if(DEVDRIVER_SHOW_DEBUG_LABELS)
    target_compile_definitions(${GPUOPEN_LIB_NAME} PRIVATE $<$<CONFIG:Debug>:         DEVDRIVER_SHOW_DEBUG_LABELS>
                                                           $<$<CONFIG:RelWithDebInfo>:DEVDRIVER_SHOW_DEBUG_LABELS>)
endif()

# Indicate target architecture bits
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_ARCHITECTURE_BITS "64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TARGET_ARCHITECTURE_BITS "32")
else()
    message(FATAL_ERROR "Unsupported target architecture - pointers must be 4 or 8 bytes, not ${CMAKE_SIZEOF_VOID_P}")
endif()
target_compile_definitions(${GPUOPEN_LIB_NAME} PRIVATE "DD_CPU_BITS=${TARGET_ARCHITECTURE_BITS}")

### Include Directories ################################################################################################
target_include_directories(${GPUOPEN_LIB_NAME} PUBLIC inc)
target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE src)

### Sources ############################################################################################################

### Core Components ###
target_sources(${GPUOPEN_LIB_NAME} PRIVATE
    src/ddVersion.cpp
    src/session.cpp
    src/sessionManager.cpp
    src/messageChannel.inl
    src/messageChannel.cpp
    src/baseProtocolServer.cpp
    src/baseProtocolClient.cpp
    src/ddTransferManager.cpp
    src/ddClientURIService.cpp
    src/ddURIRequestContext.cpp
    src/ddPlatform.cpp
    ## Protocols
    src/protocols/ddTransferServer.cpp
    src/protocols/ddTransferClient.cpp
    src/protocols/ddURIServer.cpp
    ## Utility functions and classes
    src/util/ddTextWriter.cpp
    src/util/ddMemory.cpp
)

# If the client doesn't specify, default to building internal metrohash library
if (NOT DEFINED GPUOPEN_BUILD_METROHASH)
    set(GPUOPEN_BUILD_METROHASH ON)
endif()

# If a MetroHash path is not specified then we fall back to compiling our internal copy
if (NOT DEFINED METROHASH_PATH)
    set(GPUOPEN_BUILD_METROHASH ON)
    set(METROHASH_PATH "src/imported/metrohash/src")
endif()

if (NOT TARGET metrohash)
    if (GPUOPEN_BUILD_METROHASH)
        target_sources(${GPUOPEN_LIB_NAME} PRIVATE
            ${METROHASH_PATH}/metrohash64.cpp
            ${METROHASH_PATH}/metrohash128.cpp)
    endif()
    target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE ${METROHASH_PATH})
else()
    target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC metrohash)
endif()


### RapidJson ###
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/util/ddJsonWriter.cpp
    )
    target_include_directories(${GPUOPEN_LIB_NAME} PUBLIC "inc/imported/rapidjson/include")

### Platform ###

    target_compile_definitions(${GPUOPEN_LIB_NAME}
        PUBLIC
            "-DDD_PLATFORM_STRING=\"${CMAKE_SYSTEM_NAME} User Mode\""
    )

if (CMAKE_SYSTEM_NAME MATCHES "Linux")

    target_compile_definitions(${GPUOPEN_LIB_NAME}
        PUBLIC
            "DD_PLATFORM_LINUX_UM=(1)"
            "DD_PLATFORM_IS_UM=(1)"
            "DD_PLATFORM_IS_GNU=(1)")
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/posix/ddPosixPlatform.cpp)

else()
    message(FATAL_ERROR "Unrecognized platform: ${CMAKE_SYSTEM_NAME} - cannot build platform abstraction")
endif()

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/posix/ddPosixSocket.cpp
        src/socketMsgTransport.cpp
    )

endif()

### Helper Classes ###
if(GPUOPEN_BUILD_SERVER_HELPERS)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/devDriverServer.cpp)
endif()
if(GPUOPEN_BUILD_CLIENT_HELPERS)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/devDriverClient.cpp)
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
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER ON)
endif()

### Listener Components ###
if(GPUOPEN_BUILD_LISTENER_CORE)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/listener.cpp
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

    target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/listener)
endif()

#################
### Protocols ###
#################

### Driver Control Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/driverControlServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/driverControlClient.cpp)
endif()

### Settings Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/settingsServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/settingsClient.cpp)
endif()

### Logging Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/loggingServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/loggingClient.cpp)
endif()

### RGP Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/rgpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/rgpClient.cpp)
endif()

### Crash Dump Protocol ###
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddGpuCrashDumpServer.cpp)
endif()
if(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddGpuCrashDumpClient.cpp)
endif()

### URI Client ###
if(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/ddURIClient.cpp)
endif()

################
### Services ###
################

    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/protocols/ddSettingsService.cpp
        src/protocols/ddPipelineUriService.cpp
        src/protocols/ddInfoService.cpp
    )

# Find Headers Helper ##############################################################################
macro(target_find_headers _target)
    get_target_property(${_target}_INCLUDES_DIRS ${_target} INCLUDE_DIRECTORIES)

    if(${_target}_INCLUDES_DIRS)
        foreach(_include_dir IN ITEMS ${${_target}_INCLUDES_DIRS})
            file(GLOB_RECURSE _include_files
                LIST_DIRECTORIES false
                "${_include_dir}/*.h"
                "${_include_dir}/*.hpp"
            )

            list(APPEND ${_target}_INCLUDES ${_include_files})
        endforeach()

        target_sources(${_target} PRIVATE ${${_target}_INCLUDES})
    endif()
endmacro()

target_find_headers(${GPUOPEN_LIB_NAME})

