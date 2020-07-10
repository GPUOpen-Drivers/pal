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

pal_include_guard(PalOverridesGpu)

# These are options that override PAL subproject options related to GPUs/Asics.  As these overrides are managed and force
# set by PAL, mark_as_advanced is used to hide them from the CMake GUI. These are placed in a different file from the rest of
# the override logic for several reasons:
#
# 1.) Better version control history over gpu overrides
# 2.) Simplifies the logic
# 3.) Makes it clear where GPU cmake changes go.
# 4.) GPU overrides are a particular AMD exclusive problem, it isn't a regular problem most projects have to deal with.
#     And having a file focused on the issue helps.
macro(pal_overrides_gpu)

    if(PAL_BUILD_GFX9)
        # Generic support for GFX9 cards
        set(ADDR_GFX9_BUILD    ON CACHE BOOL "PAL override to build ADDRLIB with GFX9 support." FORCE)
        mark_as_advanced(ADDR_GFX9_BUILD)
        set(ADDR_GFX9_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9/chip CACHE PATH "PAL override for ADDRLIB GFX9 register chip headers." FORCE)
        mark_as_advanced(ADDR_GFX9_CHIP_DIR)

        # Vega12
        set(ADDR_VEGA12_BUILD  ON CACHE BOOL "PAL override to build ADDRLIB with Vega12 support." FORCE)
        mark_as_advanced(ADDR_VEGA12_BUILD)

        # Vega20
        set(ADDR_VEGA20_BUILD ${PAL_BUILD_VEGA20} CACHE BOOL "PAL override to build ADDRLIB with Vega20 support." FORCE)
        mark_as_advanced(ADDR_VEGA20_BUILD)
        set(CHIP_HDR_VEGA20   ${PAL_BUILD_VEGA20} CACHE BOOL "PAL override to build chip register header with Vega20 support." FORCE)
        mark_as_advanced(CHIP_HDR_VEGA20)

        # Raven2
        set(ADDR_RAVEN2_BUILD ${PAL_BUILD_RAVEN2} CACHE BOOL "PAL override to build ADDRLIB with Raven2 support." FORCE)
        mark_as_advanced(ADDR_RAVEN2_BUILD)
        set(CHIP_HDR_RAVEN2   ${PAL_BUILD_RAVEN2} CACHE BOOL "PAL override to build chip register header with Raven2 support." FORCE)
        mark_as_advanced(CHIP_HDR_RAVEN2)

        if(PAL_BUILD_RENOIR)
            set(ADDR_RENOIR_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with RENOIR support." FORCE)
        endif()

        set(ADDR_GFX10_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with GFX10 support." FORCE)

        if(PAL_BUILD_NAVI14)
            set(ADDR_NAVI14_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with NAVI14 support." FORCE)
        endif()

    endif() # PAL_BUILD_GFX9

endmacro()
