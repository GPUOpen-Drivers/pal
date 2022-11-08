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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake")
include(DevDriver)

# Put ZERO_CHECK, INSTALL, etc default targets in a separate folder in VS solutions
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
    DD_BRANCH_STRING "unknown"
)

dd_bp(DD_BP_REMOTE_WIN_TRANSPORT ON
    DEPENDS_ON "WIN32"
    MSG        "Enables building remote transport on Windows. (Linux is unconditional)"
)

### Core Component #####################################################################################################
# WA: Check that this target hasn't already been added (Only until we have a global CMake context)
if(NOT TARGET ddCore)
    add_subdirectory(core)
endif()
target_link_libraries(devdriver PUBLIC ddCore)

target_link_libraries(devdriver PRIVATE mpack)
target_link_libraries(devdriver PUBLIC  metrohash)

    target_link_libraries(devdriver PUBLIC  rapidjson)

### Build Defines ######################################################################################################
target_compile_definitions(devdriver
    PUBLIC
        GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=${GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION}

    PRIVATE
        DD_BRANCH_STRING=\"${DD_BRANCH_STRING}\"
)

### Include Directories ################################################################################################
target_include_directories(devdriver PUBLIC inc)
target_include_directories(devdriver PRIVATE src)

# if devdriver is located as a submodule in amdlog then we can auto-detect
# the include paths. otherwise, DEVDRIVER_DRIVERS_ROOT must be defined
if(DEFINED ENV{DEVDRIVER_DRIVERS_ROOT})
    # verify the given DEVDRIVER_DRIVERS_ROOT looks reasonable
    if(NOT EXISTS "$ENV{DEVDRIVER_DRIVERS_ROOT}/amdlog")
        message(FATAL_ERROR "DEVDRIVER_DRIVERS_ROOT was given but does not appear to be a valid drivers root")
    endif()
    # PDC_TODO: remove this define when the ifdef guards are removed
    add_definitions(-DAMDLOG_SUPPORT_DEVDRIVER=1)
else()
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../../../../amdlog")
        set(ENV{DEVDRIVER_DRIVERS_ROOT} "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
        # PDC_TODO: remove this define when the ifdef guards are removed
        add_definitions(-DAMDLOG_SUPPORT_DEVDRIVER=1)
    endif()
endif() # defined DEVDRIVER_DRIVERS_ROOT

target_include_directories(devdriver PRIVATE "$ENV{DEVDRIVER_DRIVERS_ROOT}/amdlog/inc")
target_include_directories(devdriver PRIVATE "$ENV{DEVDRIVER_DRIVERS_ROOT}/inc")

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

    inc/protocols/ddURIClient.h
    src/protocols/ddURIClient.cpp

    src/protocols/ddURIServer.h
    src/protocols/ddURIServer.cpp

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

    add_subdirectory(gpuinfo)

    target_sources(devdriver PRIVATE
        inc/util/ddJsonWriter.h
        src/util/ddJsonWriter.cpp
    )

### Transports ###
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

# Build remote transport (Only required for Windows UM since Linux always supports remote and Windows KM does not support remote)
if(DD_BP_REMOTE_WIN_TRANSPORT)
    target_sources(devdriver PRIVATE
        src/win/ddWinSocket.cpp
        src/socketMsgTransport.cpp
    )

    # Pass the build system variable down to the code
    target_compile_definitions(devdriver PUBLIC DEVDRIVER_BUILD_REMOTE_TRANSPORT)
endif()

### Protocols & Helpers ###
target_sources(devdriver
    PRIVATE
        # Helper classes
        src/devDriverServer.cpp
        src/devDriverClient.cpp

        # Driver Control Protocol
        src/protocols/driverControlClient.cpp
        src/protocols/driverControlServer.cpp

        # RGP Protocol
        src/protocols/rgpClient.cpp
        src/protocols/rgpServer.cpp

        # ETW Client (the Driver uses the Client, this is not a typo)
        src/protocols/etwClient.cpp
)

### Services ###
    target_sources(devdriver PRIVATE
        src/protocols/ddSettingsService.cpp
        src/protocols/ddPipelineUriService.cpp
        src/protocols/ddInfoService.cpp
    )
