##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
include_guard()
include(PalVersionHelper)

# This file is meant to encapsulate all the variables that PAL's clients are intended to
# manipulate when customizing PAL for their purposes. Making it the first place cmake developers
# for DXCP, XGL, etc. will look to when they have build problems/ideas/confusion.

# It's very convenient to have the PAL_CLIENT_INTERFACE_MAJOR_VERSION be defined before anything else
# And it needs to be defined hence the FATAL_ERROR.
pal_build_parameter(PAL_CLIENT_INTERFACE_MAJOR_VERSION "Pal library interface value" "-1" FATAL_ERROR)

if (DEFINED PAL_CLIENT_INTERFACE_MINOR_VERSION)
    message(AUTHOR_WARNING "Unneccessary to specify PAL_CLIENT_INTERFACE_MINOR_VERSION")
endif()

pal_build_parameter(PAL_CLIENT "Client pal should build for" "mandatory" FATAL_ERROR)

#if PAL_DEVELOPER_BUILD
pal_build_parameter(PAL_DEVELOPER_BUILD "Enable developer build" OFF AUTHOR_WARNING)

# Enable these developer layers based on the PAL_DEVELOPER_BUILD value. This is a nice quality of life for developer builds.
pal_build_parameter(PAL_BUILD_CMD_BUFFER_LOGGER "Enable cmd buffer layer"       ${PAL_DEVELOPER_BUILD} VERBOSE)
pal_build_parameter(PAL_BUILD_GPU_DEBUG         "Enable GPU debug layer"        ${PAL_DEVELOPER_BUILD} VERBOSE)
pal_build_parameter(PAL_BUILD_INTERFACE_LOGGER  "Enable interface logger layer" ${PAL_DEVELOPER_BUILD} VERBOSE)
pal_build_parameter(PAL_BUILD_PM4_INSTRUMENTOR  "Enable PM4 instrumentor layer" ${PAL_DEVELOPER_BUILD} VERBOSE)
#endif

pal_build_parameter(PAL_BUILD_OSS  "Build PAL with Operating System support?" ON AUTHOR_WARNING)
pal_build_parameter(PAL_BUILD_OSS1   "Build PAL with OSS1?"   ${PAL_BUILD_OSS} DEBUG)
pal_build_parameter(PAL_BUILD_OSS2   "Build PAL with OSS2?"   ${PAL_BUILD_OSS} DEBUG)
pal_build_parameter(PAL_BUILD_OSS2_4 "Build PAL with OSS2_4?" ${PAL_BUILD_OSS} DEBUG)
pal_build_parameter(PAL_BUILD_OSS4   "Build PAL with OSS4?"   ${PAL_BUILD_OSS} DEBUG)

# Create a more convenient variable to avoid string comparisons.
set(PAL_CLIENT_${PAL_CLIENT} ON)

# This variable controls wether PAL is built with an amdgpu back-end
set(PAL_AMDGPU_BUILD ${UNIX})

pal_build_parameter(PAL_BUILD_NULL_DEVICE "Build null device backend for offline compilation" ON AUTHOR_WARNING)

pal_build_parameter(PAL_BUILD_GFX "Build PAL with Graphics support?" ON AUTHOR_WARNING)

# If PAL is being built standalone, no need to display as warnings. Since the warnings are intended
# for PAL clients.
if (PAL_IS_STANDALONE)
    set(pal_gpu_mode "STATUS")
else()
    set(pal_gpu_mode "AUTHOR_WARNING")
endif()

### Specify GPU build options ##########################################################################################
if (PAL_BUILD_GFX)
    pal_build_parameter(PAL_BUILD_GFX6 "Build PAL with GFX6 support?" ${PAL_BUILD_GFX} ${pal_gpu_mode})
    pal_build_parameter(PAL_BUILD_GFX9 "Build PAL with GFX9 support?" ${PAL_BUILD_GFX} ${pal_gpu_mode})
endif() # PAL_BUILD_GFX

# If the client wants Gfx9 support, them give them all the neccessary build parameters they need to fill out.
if (PAL_BUILD_GFX9)
    pal_build_parameter(PAL_BUILD_NAVI12 "Build PAL with Navi12 support?" ON ${pal_gpu_mode})

    pal_build_parameter(PAL_BUILD_NAVI14 "Build PAL with Navi14 support?" ON ${pal_gpu_mode})

    pal_build_parameter(PAL_BUILD_NAVI21 "Build PAL with Navi21 support?" ON ${pal_gpu_mode})
    pal_set_or(PAL_BUILD_GFX10_3 ${PAL_BUILD_NAVI21})

    pal_build_parameter(PAL_BUILD_NAVI22 "Build PAL with Navi22 support?" ON ${pal_gpu_mode})
    pal_set_or(PAL_BUILD_GFX10_3 ${PAL_BUILD_NAVI22})

endif() # PAL_BUILD_GFX9
