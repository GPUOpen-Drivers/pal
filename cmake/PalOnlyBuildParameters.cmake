##
 #######################################################################################################################
 #
 #  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

# This file is meant to encapsulate all the variables that PAL uses internally.
# Client-facing variables *must not* be defined in this file.

########################################################################################################################
# Start with any feature enablements.

pal_internal_bp(PAL_CLIENT_WINDOWS_SUBSYSTEM OFF
    DEPENDS_ON
        "NOT WIN32"
    FOR_ANY
)

#if PAL_BUILD_GFX12
#endif

# End feature enablement section.
########################################################################################################################

########################################################################################################################
# Now, handle any per-ASIC variable propagation that needs to occur.
# For example, a PAL_BUILD_* CMake variable for an ASIC may have a CHIP_HDR_* counterpart that also must be set.
# NOTE: Do not use this to propagate values up the hierarchy (ASIC -> Major IP -> Hardware Layer -> Top-Level IP).
#       Only propagate downwards (Chip Header <- ASIC)!

#if PAL_BUILD_STRIX_HALO
pal_propagate_internal_or(PAL_BUILD_STRIX_HALO CHIP_HDR_STRIX_HALO)
#endif

#if PAL_BUILD_NAVI48
pal_propagate_internal_or(PAL_BUILD_NAVI48 CHIP_HDR_NAVI48)
#endif

# End of per-ASIC variable propagation section.
########################################################################################################################
