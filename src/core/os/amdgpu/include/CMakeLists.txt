##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

target_include_directories(pal PRIVATE
    2d
    drm
    drm_exts
)
target_sources(pal PRIVATE
    CMakeLists.txt
    # Sharable metadata format
    2d/amdgpu_shared.h
    # Upstream DRM headers
    drm/amdgpu_drm.h
    drm/amdgpu.h
    drm/drm_fourcc.h
    drm/drm_mode.h
    drm/drm_sarea.h
    drm/drm.h
    drm/xf86drm.h
    drm/xf86drmMode.h
    # Additional definitions not yet upstreamed.
    drm_exts/amdgpu_drm_exts.h
    # Window System Agent
    wsa/wsa.h
)

set(PAL_XCB_REQUIRED ON)

# Needed for dri3Loader.h
if (PAL_XCB_REQUIRED)
    if(NOT DEFINED DK_X11)
        if(DEFINED GLOBAL_ROOT_DK_DIR AND NOT "${GLOBAL_ROOT_DK_DIR}" STREQUAL "")
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.19_64a)
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.19_x86)
            endif()
            target_include_directories(pal PUBLIC ${DK_X11}/include)
            target_include_directories(pal PUBLIC ${DK_X11}/include/linux)
        else()
            find_package(X11 REQUIRED)
            set(DK_X11 ${X11_INCLUDE_DIR})
            target_include_directories(pal PUBLIC ${DK_X11})
        endif()

        message(STATUS "DK_X11 not set. Defaulting to ${DK_X11}")
    endif()

    # XCB headers
    target_include_directories(pal PRIVATE ${PAL_SOURCE_DIR}/shared/amdgpu/include)
endif()
