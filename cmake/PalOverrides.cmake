##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

# This file is dedicated to overriding PAL subproject options

# ADDRLIB
set(ADDR_SI_BUILD ON)
set(ADDR_CI_BUILD ON)
set(ADDR_VI_BUILD ON)

# PAL override for ADDRLIB SI/CI/VI register chip headers
set(ADDR_SI_CHIP_DIR "${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6/chip")

# GPU Overrides

if(PAL_BUILD_GFX9)
    # Generic support for GFX9 cards
    set(ADDR_GFX9_BUILD ON)
    set(ADDR_VEGA12_BUILD ON)
    set(ADDR_VEGA20_BUILD ON)
    set(ADDR_RAVEN1_BUILD ON)
    set(ADDR_RAVEN2_BUILD ON)
    set(ADDR_RENOIR_BUILD ON)

    set(ADDR_GFX9_CHIP_DIR "${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9/chip")

    set(ADDR_GFX10_BUILD ON)

    set(ADDR_NAVI12_BUILD ${PAL_BUILD_NAVI12})

    set(ADDR_NAVI14_BUILD ${PAL_BUILD_NAVI14})

    set(ADDR_NAVI21_BUILD ${PAL_BUILD_NAVI21})

    set(ADDR_NAVI22_BUILD ${PAL_BUILD_NAVI22})

    set(ADDR_NAVI23_BUILD ${PAL_BUILD_NAVI23})

    set(ADDR_NAVI24_BUILD ${PAL_BUILD_NAVI24})

endif() # PAL_BUILD_GFX9
