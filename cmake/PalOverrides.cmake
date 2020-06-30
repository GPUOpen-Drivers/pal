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
include(PalOverridesGpu)

pal_include_guard(PalOverrides)

# These are options that override PAL subproject options.  As these overrides are managed and force
# set by PAL, mark_as_advanced is used to hide them from the CMake GUI.
macro(pal_overrides)

    # ADDRLIB
    set(ADDR_ENABLE_LTO ${PAL_ENABLE_LTO} CACHE BOOL "PAL override to build ADDRLIB without lto support." FORCE)
    mark_as_advanced(ADDR_ENABLE_LTO)

    set(ADDR_SI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with SI support." FORCE)
    mark_as_advanced(ADDR_SI_BUILD)
    set(ADDR_CI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with CI support." FORCE)
    mark_as_advanced(ADDR_CI_BUILD)
    set(ADDR_VI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with VI support." FORCE)
    mark_as_advanced(ADDR_VI_BUILD)

    set(ADDR_SI_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6/chip CACHE PATH "PAL override for ADDRLIB SI/CI/VI register chip headers." FORCE)
    mark_as_advanced(ADDR_SI_CHIP_DIR)

    # VAM
    set(VAM_ENABLE_LTO ${PAL_ENABLE_LTO} CACHE BOOL "PAL override to build ADDRLIB without lto support." FORCE)
    mark_as_advanced(VAM_ENABLE_LTO)

    # GPUOPEN
    if(PAL_BUILD_GPUOPEN)
        set(GPUOPEN_BUILD_METROHASH OFF CACHE BOOL "PAL override to build GPUOpen without the Metrohash library since PAL has its own." FORCE)
        mark_as_advanced(GPUOPEN_BUILD_METROHASH)

        set(METROHASH_PATH ${PAL_METROHASH_PATH}/src CACHE PATH "PAL override to specify the path to the MetroHash module." FORCE)
        mark_as_advanced(METROHASH_PATH)

        set(GPUOPEN_BUILD_SERVER_HELPERS ON CACHE BOOL "PAL override to build GPUOpen with server helper classes." FORCE)
        mark_as_advanced(GPUOPEN_BUILD_SERVER_HELPERS)

        set(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS ON CACHE BOOL "PAL override to build GPUOpen with support for the standard driver protocols." FORCE)
        mark_as_advanced(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS)
    endif()

    # ADDRLIB/SC GPU Overrides
    pal_overrides_gpu()
endmacro()
