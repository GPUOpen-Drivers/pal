## Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. ##

cmake_minimum_required(VERSION 3.10..3.16)
project(GPUOPEN CXX)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include project-wide and AMD-wide CMake options
include(DevDriver)

devdriver_push_scope("DevDriver Libs")

### Create GpuOpen Library #############################################################################################
devdriver_library(devdriver STATIC)

# Driver clients link against `gpuopen` instead of `devdriver`, so we need to support this target.
add_library(gpuopen ALIAS devdriver)

# Warn about legacy behavior
if(DEFINED GPUOPEN_LIB_NAME)
    message(WARNING
        "The 'gpuopen' name for the DevDriver component is deprecated and will be removed in the future. "
        "Please stop setting 'GPUOPEN_LIB_NAME' and link against `devdriver` directly instead."
    )
endif()

### Project Build Parameters ###########################################################################################
dd_bp(
    GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION 42
    MSG  "Major interface version for legacy GPUOPEN library"
    # No one should be setting this to anything other than 42 at this point.
    MODE "VERBOSE"
)

dd_bp(
    DD_RMT_DATA_MAJOR_VERSION 0
    MSG "Controls usage of breaking changes in the RMT data format"
)

dd_bp(
    DD_BP_REMOTE_WIN_TRANSPORT OFF
    DEPENDS_ON "WIN32"
    MSG        "Enables building remote transport on Windows. (Linux is unconditional)"
)

### Standard Driver Protocols ###
dd_bp(
    DD_BP_STANDARD_DRIVER_PROTOCOLS ON
    MSG  "Enables standard protocol servers for drivers"
    # Our clients generally do not change this much, so we opt to log quietly if we can.
    MODE "VERBOSE"
)

### Standard Tool Protocols ###
dd_bp(
    DD_BP_STANDARD_TOOL_PROTOCOLS OFF
    MSG "Enables standard protocol clients for tools"
)

dd_bp(
    DD_BRANCH_STRING "\"unknown\""
)

### Core Component #####################################################################################################
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if(NOT TARGET ddCore)
    add_subdirectory(core)
endif()
target_link_libraries(devdriver PUBLIC ddCore)

### Third Party Components #############################################################################################
if (DEFINED DEVDRIVER_FOLDER_ROOT)
    ## WA: This does not use the devdriver_scope_* functionality. We're side-stepping it here.
    ## In the future, the driver can include the top-level repo and we can remove this work around.
    ## We do this to place Third Party at the top of the IDE Folder tree, but source-wise it's nested
    ## under ddLegacy for use in the driver.
    # Save the current folder and force third_party under "/DevDriver/"
    set(DEVDRIVER_FOLDER_BAK ${CMAKE_FOLDER})
    set(CMAKE_FOLDER ${DEVDRIVER_FOLDER_ROOT})

    add_subdirectory(third_party)

    set(CMAKE_FOLDER ${DEVDRIVER_FOLDER_BAK})
else()
    add_subdirectory(third_party)
endif()

target_link_libraries(devdriver PRIVATE mpack)
target_link_libraries(devdriver PUBLIC  metrohash)

    target_link_libraries(devdriver PUBLIC  rapidjson)

### Build Defines ######################################################################################################
target_compile_definitions(devdriver
    PUBLIC
        GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION}

    PRIVATE
        DD_BRANCH_STRING=${DD_BRANCH_STRING}
        RMT_DATA_MAJOR_VERSION=${DD_RMT_DATA_MAJOR_VERSION}
)

### Include Directories ################################################################################################
target_include_directories(devdriver PUBLIC inc)
target_include_directories(devdriver PRIVATE src)

### Sources ############################################################################################################

### Core Components ###
target_sources(devdriver PRIVATE
    ## Core files
    src/ddVersion.cpp

    src/session.h
    src/session.cpp

    src/sessionManager.h
    src/sessionManager.cpp

    src/messageChannel.h
    src/messageChannel.inl
    src/messageChannel.cpp

    inc/baseProtocolServer.h
    src/baseProtocolServer.cpp

    inc/legacyProtocolClient.h
    src/legacyProtocolClient.cpp

    inc/ddTransferManager.h
    src/ddTransferManager.cpp

    src/ddClientURIService.h
    src/ddClientURIService.cpp

    inc/ddURIRequestContext.h
    src/ddURIRequestContext.cpp

    ## Protocols
    inc/protocols/ddTransferServer.h
    src/protocols/ddTransferServer.cpp

    inc/protocols/ddTransferClient.h
    src/protocols/ddTransferClient.cpp

    src/protocols/ddURIServer.h
    src/protocols/ddURIServer.cpp

    inc/protocols/ddEventClient.h
    src/protocols/ddEventClient.cpp

    inc/protocols/ddEventParser.h
    src/protocols/ddEventParser.cpp

    inc/protocols/ddEventServer.h
    src/protocols/ddEventServer.cpp

    inc/protocols/ddEventProvider.h
    src/protocols/ddEventProvider.cpp

    src/protocols/ddEventServerSession.h
    src/protocols/ddEventServerSession.cpp

    src/protocols/ddInternalService.h
    src/protocols/ddInternalService.cpp

    ## Utility functions and classes
    inc/util/ddBitSet.h

    inc/util/ddByteReader.h

    inc/util/ddByteWriter.h

    inc/util/ddByteSwap.h

    inc/util/ddEventTimer.h
    src/util/ddEventTimer.cpp

    inc/util/ddMetroHash.h

    inc/util/ddStructuredReader.h
    src/util/ddStructuredReader.cpp

    inc/util/ddTextWriter.h
    src/util/ddTextWriter.cpp

    inc/util/hashBase.h
    inc/util/hashFunc.h
    inc/util/hashMap.h
    inc/util/hashSet.h

    inc/util/rmtCommon.h
    inc/util/rmtFileFormat.h
    inc/util/rmtResourceDescriptions.h
    inc/util/rmtTokens.h
    inc/util/rmtWriter.h
    src/util/rmtWriter.cpp

    inc/util/queue.h
    inc/util/sharedptr.h
    inc/util/string.h
    inc/util/vector.h
)

    target_sources(devdriver PRIVATE
        inc/util/ddJsonWriter.h
        src/util/ddJsonWriter.cpp
    )

### Local Transport ###
# Local transports are always supported
if(UNIX)
    target_sources(devdriver PRIVATE
        src/posix/ddPosixSocket.cpp
        src/socketMsgTransport.cpp
    )
elseif(WIN32
)
    target_sources(devdriver PRIVATE
        src/win/ddWinPipeMsgTransport.cpp
    )
endif()

### Remote Transport ###
# Build remote transport (Only required for Windows since Linux always supports remote)
if(DD_BP_REMOTE_WIN_TRANSPORT)
    target_sources(devdriver PRIVATE
        src/win/ddWinSocket.cpp
        src/socketMsgTransport.cpp
    )

    # Pass the build system variable down to the code
    target_compile_definitions(devdriver PUBLIC DEVDRIVER_BUILD_REMOTE_TRANSPORT)

    # Libraries required for remote support
    target_link_libraries(devdriver INTERFACE devdriver ws2_32.lib)
endif()

### Helper Classes ###
if(DD_BP_STANDARD_DRIVER_PROTOCOLS)
    target_sources(devdriver PRIVATE src/devDriverServer.cpp)
endif()

if(DD_BP_STANDARD_TOOL_PROTOCOLS)
    target_sources(devdriver PRIVATE src/devDriverClient.cpp)
endif()

#################
### Protocols ###
#################

### Driver Side
if(DD_BP_STANDARD_DRIVER_PROTOCOLS)
    target_sources(devdriver
        PRIVATE
            # Driver Control Protocol
            src/protocols/driverControlServer.cpp

            # RGP Protocol
            src/protocols/rgpServer.cpp

            # ETW Client (the Driver uses the Client, this is not a typo)
            src/protocols/etwClient.cpp
    )
endif()

### Tool Side
if(DD_BP_STANDARD_TOOL_PROTOCOLS)
    target_sources(devdriver
        PRIVATE
            # Driver Control Protocol
            src/protocols/driverControlClient.cpp

            # RGP Protocol
            src/protocols/rgpClient.cpp

            # URI Client
            src/protocols/ddURIClient.cpp
    )
endif()

################
### Services ###
################

    target_sources(devdriver PRIVATE
        src/protocols/ddSettingsService.cpp
        src/protocols/ddPipelineUriService.cpp
        src/protocols/ddInfoService.cpp
    )

# If tests are being built, we need to enable everything. Tests are not configurable at this time.
if (DD_OPT_ENABLE_TESTS)
    if (NOT DD_BP_STANDARD_DRIVER_PROTOCOLS)
        message(SEND_ERROR "Tests are not configurable and require DD_BP_STANDARD_DRIVER_PROTOCOLS=ON")
    endif()

    if (NOT DD_BP_STANDARD_TOOL_PROTOCOLS)
        message(SEND_ERROR "Tests are not configurable and require DD_BP_STANDARD_TOOL_PROTOCOLS=ON")
    endif()

endif()
