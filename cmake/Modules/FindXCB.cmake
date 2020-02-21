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

set(_XCB_SEARCHES)

# Search XCB_ROOT first if it is set.
if(XCB_ROOT)
  set(_XCB_SEARCH_ROOT PATHS ${XCB_ROOT} NO_DEFAULT_PATH)
  list(APPEND _XCB_SEARCHES _XCB_SEARCH_ROOT)
endif()

list(APPEND _XCB_SEARCHES _XCB_SEARCH_NORMAL)

find_path(XCB_INCLUDE_PATH xcb/xcb.h ${_XCB_SEARCHES})
find_path(XCB_DRI3_INCLUDE_PATH xcb/dri3.h ${_XCB_SEARCHES})
find_path(XCB_DRI2_INCLUDE_PATH xcb/dri2.h ${_XCB_SEARCHES})
find_path(XCB_PRESENT_INCLUDE_PATH xcb/present.h ${_XCB_SEARCHES})
find_path(XCB_RANDR_INCLUDE_PATH xcb/randr.h ${_XCB_SEARCHES})

if(XCB_INCLUDE_PATH AND XCB_DRI3_INCLUDE_PATH AND XCB_DRI2_INCLUDE_PATH AND XCB_PRESENT_INCLUDE_PATH AND XCB_RANDR_INCLUDE_PATH)
    set(XCB_FOUND 1)
    set(XCB_INCLUDE_DIRS ${XCB_INCLUDE_PATH} ${XCB_DRI3_INCLUDE_PATH} ${XCB_DRI2_INCLUDE_PATH} ${XCB_PRESENT_INCLUDE_PATH} ${XCB_RANDR_INCLUDE_PATH})

    if(XCB_RANDR_INCLUDE_PATH AND EXISTS "${XCB_RANDR_INCLUDE_PATH}/xcb/randr.h")
        file(STRINGS "${XCB_RANDR_INCLUDE_PATH}/xcb/randr.h" XCB_RANDR_MAJOR_VERSION REGEX "^#define XCB_RANDR_MAJOR_VERSION [0-9]+")
        string(REGEX REPLACE "#define XCB_RANDR_MAJOR_VERSION " "" XCB_RANDR_MAJOR_VERSION ${XCB_RANDR_MAJOR_VERSION})
        file(STRINGS "${XCB_RANDR_INCLUDE_PATH}/xcb/randr.h" XCB_RANDR_MINOR_VERSION REGEX "^#define XCB_RANDR_MINOR_VERSION [0-9]+")
        string(REGEX REPLACE "#define XCB_RANDR_MINOR_VERSION " "" XCB_RANDR_MINOR_VERSION ${XCB_RANDR_MINOR_VERSION})
    endif()

    if((${XCB_RANDR_MAJOR_VERSION} GREATER 1) OR (${XCB_RANDR_MAJOR_VERSION} EQUAL 1 AND ${XCB_RANDR_MINOR_VERSION} GREATER 5))
        set(XCB_RANDR_LEASE 1)
    else()
        set(XCB_RANDR_LEASE 0)
    endif()

    message(STATUS "Found XCB: ${XCB_INCLUDE_DIRS}")
endif ()
