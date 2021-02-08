##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

cmake_minimum_required(VERSION 3.10..3.16)
project(GPUOPEN CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# This folder may be used stand-alone, or as part of our larger CMake package
# We'll pick a name for the folder based on the use-case/
if ("${CMAKE_FOLDER}" MATCHES "DevDriver")
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/Libs")
else()
    set(CMAKE_FOLDER "${CMAKE_FOLDER}/DevDriver Libs")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

if(NOT DEFINED GPUOPEN_LIB_NAME)
    set(GPUOPEN_LIB_NAME gpuopen CACHE STRING "Name of generated gpuopen lib")
endif()

### Create GpuOpen Library #############################################################################################
devdriver_library(${GPUOPEN_LIB_NAME} STATIC)

### Cached Project Options #############################################################################################
if(NOT DEFINED GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION)
    file(STRINGS inc/gpuopen.h GPUOPEN_MAJOR_VERSION REGEX "^#define GPUOPEN_INTERFACE_MAJOR_VERSION [0-9]+")

    if(GPUOPEN_MAJOR_VERSION STREQUAL "")
        message(FATAL_ERROR "Failed to automatically extract GPUOPEN_INTERFACE_MAJOR_VERSION and GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION was not manually specified. Configuration cannot continue.")
    else()
        string(REGEX REPLACE "^#define GPUOPEN_INTERFACE_MAJOR_VERSION " "" GPUOPEN_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION})
        devdriver_message_debug("Extracted GPUOPEN_INTERFACE_MAJOR_VERSION from header as " ${GPUOPEN_MAJOR_VERSION})
    endif()
    set(GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION ${GPUOPEN_MAJOR_VERSION} CACHE STRING "Interface version for gpuopen")
endif()
devdriver_message_debug("Building gpuopen lib as ${GPUOPEN_LIB_NAME} GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION}")

### Core Component #####################################################################################################
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if(NOT TARGET ddCore)
    add_subdirectory(core)
endif()
target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC ddCore)

### Third Party Components #############################################################################################
add_subdirectory(third_party)

target_link_libraries(${GPUOPEN_LIB_NAME} PRIVATE mpack)
target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC  metrohash)

    target_link_libraries(${GPUOPEN_LIB_NAME} PUBLIC  rapidjson)

### Build Defines ######################################################################################################
target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION})

if (NOT DEFINED DD_BRANCH_STRING)
    set(DD_BRANCH_STRING \"unknown\")
endif()
target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DD_BRANCH_STRING=${DD_BRANCH_STRING})

# Add NDEBUG flag in release builds since several code files depend on it.
target_compile_definitions(${GPUOPEN_LIB_NAME} PRIVATE $<$<CONFIG:Release>:NDEBUG>)

### Include Directories ################################################################################################
target_include_directories(${GPUOPEN_LIB_NAME} PUBLIC inc)
target_include_directories(${GPUOPEN_LIB_NAME} PRIVATE src)

### Sources ############################################################################################################

### Core Components ###
target_sources(${GPUOPEN_LIB_NAME} PRIVATE
    ## Core files
    src/ddVersion.cpp
    src/session.cpp
    src/sessionManager.cpp
    src/messageChannel.inl
    src/messageChannel.cpp
    src/baseProtocolServer.cpp
    src/legacyProtocolClient.cpp
    src/ddTransferManager.cpp
    src/ddClientURIService.cpp
    src/ddURIRequestContext.cpp

    ## Protocols
    src/protocols/ddTransferServer.cpp
    src/protocols/ddTransferClient.cpp
    src/protocols/ddURIServer.cpp
    src/protocols/ddEventClient.cpp
    src/protocols/ddEventParser.cpp
    src/protocols/ddEventServer.cpp
    src/protocols/ddEventProvider.cpp
    src/protocols/ddEventServerSession.cpp
    src/protocols/ddInternalService.cpp

    ## Utility functions and classes
    src/util/ddTextWriter.cpp
    src/util/ddStructuredReader.cpp
    src/util/rmtWriter.cpp
    src/util/ddEventTimer.cpp
)

    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/util/ddJsonWriter.cpp
    )

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/posix/ddPosixSocket.cpp
        src/socketMsgTransport.cpp
    )
elseif(WIN32
)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/win/ddWinPipeMsgTransport.cpp
    )
endif()

### Remote Transport ###
# Build remote transport (Only required for Windows since Linux always supports remote)
if(WIN32 AND GPUOPEN_BUILD_REMOTE_TRANSPORT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/win/ddWinSocket.cpp
        src/socketMsgTransport.cpp
    )

    # Pass the build system variable down to the code
    target_compile_definitions(${GPUOPEN_LIB_NAME} PUBLIC DEVDRIVER_BUILD_REMOTE_TRANSPORT)

    # Libraries required for remote support
    target_link_libraries(${GPUOPEN_LIB_NAME} INTERFACE ${GPUOPEN_LIB_NAME} ws2_32.lib)
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
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_SERVER      ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER       ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_SERVER           ON)

endif()

### Standard Tool Protocols ###
if(GPUOPEN_BUILD_STANDARD_TOOL_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_DRIVERCONTROL_CLIENT ON)
    set(GPUOPEN_BUILD_PROTOCOL_SETTINGS_CLIENT      ON)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_CLIENT       ON)
    set(GPUOPEN_BUILD_PROTOCOL_RGP_CLIENT           ON)
    set(GPUOPEN_BUILD_PROTOCOL_URI_CLIENT           ON)
    set(GPUOPEN_BUILD_PROTOCOL_CRASHDUMP_SERVER     ON)
endif()

### Standard Listener Protocols ###
if(GPUOPEN_BUILD_STANDARD_LISTENER_PROTOCOLS)
    set(GPUOPEN_BUILD_PROTOCOL_LOGGING_SERVER ON)
    if(WIN32)
        # The ETW protocol is only available on Windows
        set(GPUOPEN_BUILD_PROTOCOL_ETW_SERVER ON)
    endif()
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

    # The listener core requires a special pipe transport implementation on Windows
    if(WIN32)
        target_sources(${GPUOPEN_LIB_NAME} PRIVATE
            listener/transports/winPipeTransport.cpp

        )
    endif()

    # We have special code to query information about Amd gpus. This changes per platform and is only built in the Listener.
    add_subdirectory(../gpuinfo gpuinfo)
    target_link_libraries(${GPUOPEN_LIB_NAME}
        PUBLIC
            devdriver_gpuinfo
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

### ETW Protocol ###
if(WIN32 AND GPUOPEN_BUILD_PROTOCOL_ETW_SERVER)
    # ETW Server is only available on Windows

    target_sources(${GPUOPEN_LIB_NAME} PRIVATE
        src/protocols/etwServer.cpp
        src/win/traceSession.cpp
    )

    # Libraries required for ETW support
    target_link_libraries(${GPUOPEN_LIB_NAME} INTERFACE ${GPUOPEN_LIB_NAME} advapi32.lib)
    target_link_libraries(${GPUOPEN_LIB_NAME} INTERFACE ${GPUOPEN_LIB_NAME} Tdh.lib)
    target_link_libraries(${GPUOPEN_LIB_NAME} INTERFACE ${GPUOPEN_LIB_NAME} Ole32.lib)

endif()

if(GPUOPEN_BUILD_PROTOCOL_ETW_CLIENT)
    target_sources(${GPUOPEN_LIB_NAME} PRIVATE src/protocols/etwClient.cpp)
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

# Source Groups Helper #############################################################################
# This helper creates source groups for generators that support them. This is primarily MSVC and
# XCode, but there are other generators that support IDE project files.
#
# Note: this only adds files that have been added to the target's SOURCES property. To add headers
# to this list, be sure that you call target_find_headers before you call target_source_groups.
macro(target_source_groups _target)
    get_target_property(${_target}_SOURCES ${_target} SOURCES)
    foreach(_source IN ITEMS ${${_target}_SOURCES})
        set(_source ${_source})
        get_filename_component(_source_path "${_source}" ABSOLUTE)
        file(RELATIVE_PATH _source_path_rel "${PROJECT_SOURCE_DIR}" "${_source_path}")
        get_filename_component(_source_path_rel "${_source_path_rel}" DIRECTORY)
        string(REPLACE "/" "\\" _group_path "${_source_path_rel}")
        source_group("${_group_path}" FILES "${_source}")
    endforeach()
endmacro()

target_source_groups(${GPUOPEN_LIB_NAME})
