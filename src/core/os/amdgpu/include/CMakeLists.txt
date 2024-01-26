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

target_sources(pal PRIVATE CMakeLists.txt)

add_subdirectory(2d)

add_subdirectory(drm)

add_subdirectory(wsa)

set(PAL_XCB_REQUIRED ON)

# Needed for dri3Loader.h. Does not exist for standalone compiler tool build on Mac.
if (NOT APPLE AND PAL_XCB_REQUIRED)
    if(NOT DEFINED DK_X11)
        if(DEFINED GLOBAL_ROOT_DK_DIR)
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

    if(NOT DEFINED DK_XCB)
        if(DEFINED GLOBAL_ROOT_DK_DIR)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(DK_XCB ${GLOBAL_ROOT_DK_DIR}/xcb/1.13_64a)
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(DK_XCB ${GLOBAL_ROOT_DK_DIR}/xcb/1.13_x86)
            endif()
            target_include_directories(pal PUBLIC ${DK_XCB}/include)
        else()
            find_package(XCB)
            set(DK_XCB ${XCB_INCLUDE_DIRS})
            target_include_directories(pal PUBLIC ${DK_XCB})
        endif()
        message(STATUS "DK_XCB not set. Defaulting to ${DK_XCB}")
    endif()
    target_include_directories(pal PRIVATE ${DK_XCB}/include)
endif()
