##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

# GPU Overrides

if(PAL_BUILD_GFX9)
    set(ADDR_GFX10_BUILD ON)

    # These ASICs don't have build macros, they're always present if this HWL is enabled.
    set(ADDR_NAVI12_BUILD ON)
    set(ADDR_NAVI14_BUILD ON)
    set(ADDR_NAVI21_BUILD ON)
    set(ADDR_NAVI22_BUILD ON)
    set(ADDR_NAVI23_BUILD ON)
    set(ADDR_NAVI24_BUILD ON)
    set(ADDR_REMBRANDT_BUILD ON)
    set(ADDR_RAPHAEL_BUILD ON)
    set(ADDR_MENDOCINO_BUILD ON)

#if PAL_BUILD_GFX11
    set(ADDR_GFX11_BUILD        ${PAL_BUILD_GFX11})
    set(PAL_SWD_BUILD_GFX11     ${PAL_BUILD_GFX11})
#endif

#if PAL_BUILD_NAVI31
    set(ADDR_NAVI31_BUILD        ${PAL_BUILD_NAVI31})
    set(PAL_SWD_BUILD_NAVI31     ${PAL_BUILD_NAVI31})
    set(PAL_SWD_BUILD_NAVI3X     ${PAL_BUILD_NAVI3X})
#endif

#if PAL_BUILD_NAVI32
    set(ADDR_NAVI32_BUILD        ${PAL_BUILD_NAVI32})
    set(PAL_SWD_BUILD_NAVI32     ${PAL_BUILD_NAVI32})
    set(PAL_SWD_BUILD_NAVI3X     ${PAL_BUILD_NAVI3X})
#endif

#if PAL_BUILD_NAVI33
    set(ADDR_NAVI33_BUILD        ${PAL_BUILD_NAVI33})
    set(PAL_SWD_BUILD_NAVI33     ${PAL_BUILD_NAVI33})
    set(PAL_SWD_BUILD_NAVI3X     ${PAL_BUILD_NAVI3X})
#endif

#if PAL_BUILD_PHOENIX1
    set(PAL_SWD_BUILD_PHX1         ${PAL_BUILD_PHOENIX1})
    pal_set_or(PAL_SWD_BUILD_PHX   ${PAL_BUILD_PHOENIX1})
    pal_set_or(ADDR_PHOENIX_BUILD  ${PAL_BUILD_PHOENIX1})
    pal_set_or(ADDR_PHOENIX1_BUILD ${PAL_BUILD_PHOENIX1})
#endif

endif() # PAL_BUILD_GFX9

