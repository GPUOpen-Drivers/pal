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

# This file is dedicated to overriding PAL subproject options.
# When overriding a cache variable use pal_override to simplify the code

# ADDRLIB
pal_override(ADDR_SI_BUILD ON)
pal_override(ADDR_CI_BUILD ON)
pal_override(ADDR_VI_BUILD ON)

# PAL override for ADDRLIB SI/CI/VI register chip headers
pal_override(ADDR_SI_CHIP_DIR "${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6/chip")

# VAM

# GPUOPEN

# PAL override to build GPUOpen without the Metrohash library since PAL has its own.
pal_override(GPUOPEN_BUILD_METROHASH OFF)

# PAL override to specify the path to the MetroHash module.
pal_override(METROHASH_PATH "${PAL_METROHASH_PATH}/src")

# PAL override to build GPUOpen with server helper classes
pal_override(GPUOPEN_BUILD_SERVER_HELPERS ON)

# PAL override to build GPUOpen with support for the standard driver protocols
pal_override(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS ON)

# GPU Overrides

if(PAL_BUILD_GFX9)
    # Generic support for GFX9 cards
    pal_override(ADDR_GFX9_BUILD ON)
    pal_override(ADDR_VEGA12_BUILD ON)
    pal_override(ADDR_VEGA20_BUILD ON)
    pal_override(ADDR_RAVEN1_BUILD ON)
    pal_override(ADDR_RAVEN2_BUILD ON)
    pal_override(ADDR_RENOIR_BUILD ON)

    pal_override(ADDR_GFX9_CHIP_DIR "${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9/chip")

    pal_override(ADDR_GFX10_BUILD ON)

    pal_override(ADDR_NAVI12_BUILD ${PAL_BUILD_NAVI12})

    pal_override(ADDR_NAVI14_BUILD ${PAL_BUILD_NAVI14})

    pal_override(ADDR_NAVI21_BUILD ${PAL_BUILD_NAVI21})

    pal_override(ADDR_NAVI22_BUILD ${PAL_BUILD_NAVI22})

endif() # PAL_BUILD_GFX9

