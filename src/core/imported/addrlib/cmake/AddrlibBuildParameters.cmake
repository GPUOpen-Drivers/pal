##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

# NOTE: Just like function parameters it's best for a library to reduce the amount of
# build parameters/customization they offer to clients.
# Offering more than 5+ introduces a lot of complexity for clients to deal with.
# Addrlib developers should strive to reduce the amount of build parameters.

include(Addrlib)

if (NOT DEFINED ADDRLIB_IS_TOP_LEVEL)
    message(FATAL_ERROR "ADDRLIB_IS_TOP_LEVEL not defined")
endif()

# NOTE:
# By using ADDRLIB_IS_TOP_LEVEL as the default values of the build parameters
# addrlib builds as much code as possible when building standalone.
#
# However, when being consumed by a add_subdirectory call ADDRLIB_IS_TOP_LEVEL will be OFF
# So the clients have to opt into the functionality they want/need.

if((CMAKE_CXX_COMPILER_ID MATCHES "GNU|[Cc]lang")
)
    addrlib_bp(ADDR_ENABLE_WERROR ${ADDRLIB_IS_TOP_LEVEL})
endif()

set(addrlib_is_linux OFF)
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(addrlib_is_linux ON)
endif()

addrlib_bp(ADDR_LNX_KERNEL_BUILD OFF MSG "Linux kernel build?" DEPENDS_ON ${addrlib_is_linux})
if (ADDR_LNX_KERNEL_BUILD)
    set(ADDR_KERNEL_BUILD ON)
elseif (NOT DEFINED ADDR_KERNEL_BUILD)
    set(ADDR_KERNEL_BUILD OFF)
endif()

# Build with "always" assertions by default
addrlib_bp(ADDR_SILENCE_ASSERT_ALWAYS OFF)

# Build support for fmask addressing and addr5Swizzle
addrlib_bp(ADDR_AM_BUILD ${ADDRLIB_IS_TOP_LEVEL} MSG "Build support for fmask addressing and addr5Swizzle?")

# GFX10 CARDS ######################################
addrlib_bp(ADDR_GFX10_BUILD ON)

#if ADDR_GFX11_BUILD
# GFX11 CARDS #####################################
#endif

#if ADDR_GFX11_BUILD
addrlib_bp(ADDR_GFX11_BUILD ${ADDRLIB_IS_TOP_LEVEL})
#endif

#if ADDR_NAVI31_BUILD
addrlib_bp(ADDR_NAVI31_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#endif

#if ADDR_NAVI32_BUILD
addrlib_bp(ADDR_NAVI32_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#endif

#if ADDR_NAVI33_BUILD
addrlib_bp(ADDR_NAVI33_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#endif

#if ADDR_STRIX1_BUILD
addrlib_bp(ADDR_STRIX1_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#endif

#if ADDR_PHOENIX_BUILD
addrlib_bp(ADDR_PHOENIX_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#if ADDR_PHOENIX1_BUILD
addrlib_bp(ADDR_PHOENIX1_BUILD ON DEPENDS_ON ${ADDR_PHOENIX_BUILD})
#endif
#if ADDR_PHOENIX2_BUILD
addrlib_bp(ADDR_PHOENIX2_BUILD ON DEPENDS_ON ${ADDR_PHOENIX_BUILD})
#endif

#if ADDR_STRIX_HALO_BUILD
addrlib_bp(ADDR_STRIX_HALO_BUILD ON DEPENDS_ON ${ADDR_GFX11_BUILD})
#endif

#endif

#if ADDR_GFX12_BUILD
# GFX12 CARDS #####################################
#endif

#if ADDR_GFX12_BUILD
addrlib_bp(ADDR_GFX12_BUILD ${ADDRLIB_IS_TOP_LEVEL})

#if ADDR_GFX12_SHARED_BUILD
addrlib_bp(ADDR_GFX12_SHARED_BUILD OFF DEPENDS_ON ${ADDR_GFX12_BUILD})
#endif
#endif

