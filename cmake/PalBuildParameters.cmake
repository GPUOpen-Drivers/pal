##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

if (PAL_CLIENT_INTERFACE_MAJOR_VERSION LESS 888)
    # Build PAL with Operating System support
    pal_bp(PAL_BUILD_OSS    ON)
#if PAL_BUILD_OSS2_4
    pal_bp(PAL_BUILD_OSS2_4 ON DEPENDS_ON PAL_BUILD_OSS)
#endif
endif()

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

pal_bp(PAL_BUILD_BRANCH "2420")

### Specify GPU build options ##########################################################################################

if (PAL_BUILD_GFX)
    pal_bp(PAL_BUILD_GFX9 ${PAL_BUILD_GFX} MODE "AUTHOR_WARNING")
    # PAL's GFX11 support is part of its GFX9 HWL so you need to enable both to get GFX11.
    pal_bp(PAL_BUILD_GFX11 ${PAL_BUILD_GFX} DEPENDS_ON PAL_BUILD_GFX9 MODE "AUTHOR_WARNING")
#if PAL_BUILD_GFX12
    pal_bp(PAL_BUILD_GFX12 ${PAL_BUILD_GFX} MODE "AUTHOR_WARNING")
#endif
endif() # PAL_BUILD_GFX

# If the client wants Gfx9 support, them give them all the neccessary build parameters they need to fill out.
if (PAL_BUILD_GFX9)

    # Clients should directly set PAL_BUILD_GFX9 and PAL_BUILD_GFX11 in their cmakes to request support for these GPUs.
    # These used to be pal_bp calls but were simplified to pal_set_or calls to avoid breaking clients that depend on
    # the ASIC-specific variables automatically setting PAL_BUILD_GFX11 via ASIC_CONFIG.
    if ((PAL_CLIENT_INTERFACE_MAJOR_VERSION LESS 888) AND DEFINED PAL_BUILD_PHOENIX2)
        pal_set_or(PAL_BUILD_GFX11 ${PAL_BUILD_PHOENIX2})
    endif()
    if ((PAL_CLIENT_INTERFACE_MAJOR_VERSION LESS 917) AND DEFINED PAL_BUILD_STRIX1)
        pal_set_or(PAL_BUILD_GFX11 ${PAL_BUILD_STRIX1})
    endif()

#if PAL_BUILD_STRIX_HALO
    pal_bp( PAL_BUILD_STRIX_HALO OFF MODE "AUTHOR_WARNING"
            ASIC_CONFIG
                PAL_BUILD_GFX11
                CHIP_HDR_STRIX_HALO
          )
#endif

endif() # PAL_BUILD_GFX9

#if PAL_BUILD_GFX12
if (PAL_BUILD_GFX12)

#if PAL_BUILD_NAVI48
    pal_bp(PAL_BUILD_NAVI48 ON MODE "AUTHOR_WARNING"
           ASIC_CONFIG
               PAL_BUILD_GFX11
               PAL_BUILD_GFX12
               PAL_BUILD_NAVI4X
               CHIP_HDR_NAVI48
          )
#endif
endif() # PAL_BUILD_GFX12
#endif

#if PAL_BUILD_RDF
pal_bp(PAL_BUILD_RDF ON)
#endif

# "This must always be enabled unless the client guarantees they do not use GFX/3D queues"
pal_bp(PAL_BUILD_RPM_GFX_SHADERS ON)

