##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# This file is meant to encapsulate all the variables that PAL's clients are intended to
# manipulate when customizing PAL for their purposes. Making it the first place cmake developers
# for DXCP, XGL, etc. will look to when they have build problems/ideas/confusion.

# It's very convenient to have the PAL_CLIENT_INTERFACE_MAJOR_VERSION be defined before anything else
# And it needs to be defined hence the FATAL_ERROR.
pal_bp(PAL_CLIENT_INTERFACE_MAJOR_VERSION "-1" MODE "FATAL_ERROR")

if (DEFINED PAL_CLIENT_INTERFACE_MINOR_VERSION)
    message(AUTHOR_WARNING "Unneccessary to specify PAL_CLIENT_INTERFACE_MINOR_VERSION")
endif()

# See README.md for explanation of CORE/GPUUTIL functionality.
# NOTE: There is no PAL_BUILD_UTIL, that functionality isn't optional.
pal_bp(PAL_BUILD_CORE ON)
pal_bp(PAL_BUILD_GPUUTIL ON)

#if PAL_DEVELOPER_BUILD
pal_bp(PAL_DEVELOPER_BUILD OFF)
#endif

# Allows RPC in settings. The default is to use the URI path
pal_bp(PAL_ENABLE_RPC_SETTINGS OFF)

# Build PAL with Operating System support
pal_bp(PAL_BUILD_OSS    ON)
#if PAL_BUILD_OSS2_4
pal_bp(PAL_BUILD_OSS2_4 ON DEPENDS_ON PAL_BUILD_OSS)
#endif
pal_bp(PAL_BUILD_OSS4   ON DEPENDS_ON PAL_BUILD_OSS)

# Clients must define this variable so pal know what API it's facilitating
pal_bp(PAL_CLIENT "-1" MODE "FATAL_ERROR")

# Create a more convenient variable to avoid string comparisons.
set(PAL_CLIENT_${PAL_CLIENT} ON)

# This variable controls whether PAL is built with an amdgpu back-end
if (UNIX)
    set(PAL_AMDGPU_BUILD ON)
else()
    set(PAL_AMDGPU_BUILD OFF)
endif()

pal_bp(PAL_BUILD_DRI3 ON DEPENDS_ON PAL_AMDGPU_BUILD)
pal_bp(PAL_BUILD_WAYLAND OFF DEPENDS_ON PAL_AMDGPU_BUILD)

pal_bp(PAL_DISPLAY_DCC ON DEPENDS_ON PAL_AMDGPU_BUILD)

# Build null device backend for offline compilation
pal_bp(PAL_BUILD_NULL_DEVICE ON)

# Build PAL with Graphics support?
pal_bp(PAL_BUILD_GFX ON)

### Specify GPU build options ##########################################################################################

if (PAL_BUILD_GFX)
    pal_bp(PAL_BUILD_GFX6 ${PAL_BUILD_GFX} MODE "AUTHOR_WARNING")
    pal_bp(PAL_BUILD_GFX9 ${PAL_BUILD_GFX} MODE "AUTHOR_WARNING")
endif() # PAL_BUILD_GFX

# If the client wants Gfx9 support, them give them all the neccessary build parameters they need to fill out.
if (PAL_BUILD_GFX9)
    pal_bp( PAL_BUILD_NAVI12 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                CHIP_HDR_NAVI12
          )

    pal_bp( PAL_BUILD_NAVI14 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                CHIP_HDR_NAVI14
          )

    pal_bp( PAL_BUILD_NAVI21 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                PAL_BUILD_NAVI2X
                CHIP_HDR_NAVI21
          )

    pal_bp( PAL_BUILD_NAVI22 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                PAL_BUILD_NAVI2X
                CHIP_HDR_NAVI22
          )

    pal_bp( PAL_BUILD_NAVI23 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                PAL_BUILD_NAVI2X
                CHIP_HDR_NAVI23
          )

    pal_bp( PAL_BUILD_NAVI24 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                PAL_BUILD_NAVI2X
                CHIP_HDR_NAVI24
          )

    pal_bp( PAL_BUILD_REMBRANDT ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                CHIP_HDR_REMBRANDT
          )

    pal_bp( PAL_BUILD_MENDOCINO ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                PAL_BUILD_RAPHAEL
                CHIP_HDR_RAPHAEL
          )

    pal_bp( PAL_BUILD_RAPHAEL ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX103
                PAL_BUILD_GFX10_3
                CHIP_HDR_RAPHAEL
          )

#if PAL_BUILD_NAVI31
    pal_bp( PAL_BUILD_NAVI31 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX11
                PAL_BUILD_NAVI3X
                CHIP_HDR_NAVI31
          )
#endif

#if PAL_BUILD_NAVI33
    pal_bp( PAL_BUILD_NAVI33 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX11
                PAL_BUILD_NAVI3X
                CHIP_HDR_NAVI33
          )
#endif

#if PAL_BUILD_PHOENIX1
    pal_bp( PAL_BUILD_PHOENIX1 ON MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX11
                PAL_BUILD_PHOENIX
                PAL_BUILD_NPI
                CHIP_HDR_PHOENIX1
          )
#endif

endif() # PAL_BUILD_GFX9

