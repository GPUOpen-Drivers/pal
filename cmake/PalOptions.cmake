##
 #######################################################################################################################
 #
 #  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

include(PalVersionHelper)
include(CMakeDependentOption)

pal_include_guard(PalOptions)

# This macro is meant to encapsulate all the variables that PAL's clients are intended to
# manipulate when customizing PAL for their purposes. Making it the first place cmake developers
# for DXCP, XGL, etc. will look to when they have build problems/ideas/confusion.
#
# All variables should have the prefix "PAL_" this serves two main purposes
#   Name collision issues
#   Cmake-gui allows grouping of variables based on prefixes, which then makes it clear what options PAL defined
macro(pal_options)

    option(PAL_BUILD_NULL_DEVICE "Build null device backend for offline compilation?" ON)

    option(PAL_BUILD_GPUOPEN "Build GPUOpen developer driver support?" OFF)

    option(PAL_ENABLE_LTO "Enable LTO" ON)

    option(PAL_ENABLE_DEVDRIVER_USAGE "Enables developer driver suppport." ON)

    option(PAL_DBG_COMMAND_COMMENTS "Command with comments" OFF)

    option(PAL_ENABLE_PRINTS_ASSERTS "Enable print assertions?" OFF)
    option(PAL_ENABLE_PRINTS_ASSERTS_DEBUG "Enable print assertions on debug builds?" ON)

    option(PAL_MEMTRACK "Enable PAL memory tracker?" OFF)

    option(PAL_BUILD_CORE "Build PAL Core?" ON)

    # If present, it specifies that the build is on a release branch (not stg) and en/disables features
    # specific to that branch. On stg, all branch-related features are enabled.
    if (DEFINED PAL_BUILD_BRANCH)
        message_verbose("Client specified PAL_BUILD_BRANCH: ${PAL_BUILD_BRANCH}")
    endif()

    option(PAL_BUILD_GPUUTIL "Build PAL GPU Util?" ON)

    cmake_dependent_option(PAL_BUILD_LAYERS "Build PAL Layers?" ON "PAL_BUILD_GPUUTIL" OFF)

    option(PAL_BUILD_DBG_OVERLAY "Build PAL Debug Overlay?" ON)

    option(PAL_BUILD_GPU_PROFILER "Build PAL GPU Profiler?" ON)

    option(PAL_DISPLAY_DCC "Enable DISPLAY DCC?" OFF)

#if PAL_DEVELOPER_BUILD
    option(PAL_DEVELOPER_BUILD "Enable developer build" OFF)

    # If the client turns on PAL developer build they expect these ALL these features to be turned on
    if (${PAL_DEVELOPER_BUILD})
        # Notice how these aren't cache variables.
        # Because if they were cache variables either they/we would have to use the FORCE keyword
        # Either way it would be a bad interface for the client
        set(PAL_BUILD_CMD_BUFFER_LOGGER ON)
        set(PAL_BUILD_INTERFACE_LOGGER  ON)
        set(PAL_BUILD_PM4_INSTRUMENTOR  ON)
    # Otherwise give them the ability to turn them on individually
    else()
        option(PAL_BUILD_CMD_BUFFER_LOGGER "Build PAL Command Buffer Logger?" OFF)
        option(PAL_BUILD_INTERFACE_LOGGER  "Build PAL Interface Logger?"      OFF)
        option(PAL_BUILD_PM4_INSTRUMENTOR  "Build PAL PM4 Instrumentor?"      OFF)
    endif()
#endif

    option(PAL_BUILD_GFX  "Build PAL with Graphics support?" ON)
    cmake_dependent_option(PAL_BUILD_GFX6 "Build PAL with GFX6 support?" ON "PAL_BUILD_GFX" OFF)
    cmake_dependent_option(PAL_BUILD_GFX9 "Build PAL with GFX9 support?" ON "PAL_BUILD_GFX" OFF)

    option(PAL_BUILD_OSS  "Build PAL with Operating System support?" ON)
    cmake_dependent_option(PAL_BUILD_OSS1   "Build PAL with OSS1?"   ON "PAL_BUILD_OSS" OFF)
    cmake_dependent_option(PAL_BUILD_OSS2   "Build PAL with OSS2?"   ON "PAL_BUILD_OSS" OFF)
    cmake_dependent_option(PAL_BUILD_OSS2_4 "Build PAL with OSS2_4?" ON "PAL_BUILD_OSS" OFF)
    cmake_dependent_option(PAL_BUILD_OSS4   "Build PAL with OSS4?"   ON "PAL_BUILD_OSS" OFF)

    set(PAL_AMDGPU_BUILD ${UNIX})
    option(PAL_BUILD_DRI3 "Build PAL with DRI3 support?" ON)
    option(PAL_BUILD_WAYLAND "Build PAL with WAYLAND support?" OFF)

    # PAL Client Options ###############################################################################
    if (NOT DEFINED PAL_CLIENT)
        message(FATAL_ERROR "User didn't specify PAL_CLIENT")
    endif()

    # Create a more convenient variable to avoid string comparisons.
    set(PAL_CLIENT_${PAL_CLIENT} ON)

    if(DEFINED PAL_CLIENT_INTERFACE_MAJOR_VERSION)
        message_verbose("Client configured PAL_INTERFACE_MAJOR_VERSION as " ${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
    else()
        message(FATAL_ERROR "Client must specify PAL_CLIENT_INTERFACE_MAJOR_VERSION")
    endif()

    if(DEFINED PAL_CLIENT_INTERFACE_MINOR_VERSION)
        message(AUTHOR_WARNING "Unneccessary to specify PAL_CLIENT_INTERFACE_MINOR_VERSION")
    endif()

    # Paths to PAL's dependencies
    set(PAL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/src/util/imported/metrohash CACHE PATH "Specify the path to the MetroHash project.")
    set(   PAL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/src/util/imported/cwpack    CACHE PATH "Specify the path to the CWPack project.")
    set(      PAL_VAM_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/vam       CACHE PATH "Specify the path to the VAM project.")
    set(     PAL_ADDR_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/addrlib   CACHE PATH "Specify the path to the ADDRLIB project.")

    if (PAL_BUILD_GPUOPEN)
        set(PAL_GPUOPEN_PATH ${PROJECT_SOURCE_DIR}/shared/gpuopen CACHE PATH "Specify the path to the GPUOPEN_PATH project.")
    endif()

endmacro()
