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

# This CMake file installs pre-built binaries of libtraceevent and libtracefs
# which are used for capturing kernel events on Linux. The binaries are
# downloaded automatically when built internally, or the existing ones on disk
# and pointed to by the corresponding CMake variables,

include_guard()

set(LIBTRACE_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/linux/third_party")

### libtraceevent ##################################################################################################

set(LIBTRACEEVENT_DIR "" CACHE PATH "The absolute path to libtraceevent pre-built binary and include headers.")

if(LIBTRACEEVENT_DIR STREQUAL "")
    message(FATAL_ERROR "libtraceevent binary directory is not set. Please set LIBTRACEEVENT_DIR to the absolute path of libtraceevent binary directory.")
else()
    message(STATUS "LIBTRACEEVENT_DIR=${LIBTRACEEVENT_DIR}")
endif()

add_library(libtraceevent SHARED IMPORTED)

set_target_properties(libtraceevent PROPERTIES
    IMPORTED_LOCATION ${LIBTRACEEVENT_DIR}/lib/libtraceevent.so.1
    INTERFACE_INCLUDE_DIRECTORIES ${LIBTRACEEVENT_DIR}/include
    IMPORTED_SONAME libtraceevent.so.1
)

if (DD_BP_INSTALL)
    install(FILES ${LIBTRACEEVENT_DIR}/lib/libtraceevent.so.1 DESTINATION bin)
endif()

### libtracefs #####################################################################################################

set(LIBTRACEFS_DIR "" CACHE PATH "The absolute path to libtracefs pre-built binary and include headers.")

if(LIBTRACEFS_DIR STREQUAL "")
    message(FATAL_ERROR "libtracefs binary directory is not set. Please set LIBTRACEFS_DIR to point to the absolute path of libtracefs binary directory.")
else()
    message(STATUS "LIBTRACEFS_DIR=${LIBTRACEFS_DIR}")
endif()

add_library(libtracefs SHARED IMPORTED)

set_target_properties(libtracefs PROPERTIES
    IMPORTED_LOCATION ${LIBTRACEFS_DIR}/lib/libtracefs.so.1
    INTERFACE_INCLUDE_DIRECTORIES ${LIBTRACEFS_DIR}/include
    IMPORTED_SONAME libtracefs.so.1
)

if (DD_BP_INSTALL)
    install(FILES ${LIBTRACEFS_DIR}/lib/libtracefs.so.1 DESTINATION bin)
endif()
