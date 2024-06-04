##
 #######################################################################################################################
 #
 #  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

function(pal_init_precompiled_headers)
    # target_precompile_headers was introduced in 3.16
    if(${CMAKE_VERSION} VERSION_LESS "3.16")
        message(STATUS "PCH requires CMake version 3.16")
        set(PAL_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)
    endif()
endfunction()

function(target_set_precompiled_headers TARGET)
    list(TRANSFORM ARGN PREPEND "$<$<COMPILE_LANGUAGE:CXX>:" OUTPUT_VARIABLE CXX_HEADERS)
    list(TRANSFORM CXX_HEADERS APPEND ">" OUTPUT_VARIABLE CXX_HEADERS)
    target_precompile_headers(${TARGET} PRIVATE ${CXX_HEADERS})
endfunction()

function(pal_setup_precompiled_headers)
    pal_init_precompiled_headers()
    if (PAL_PRECOMPILED_HEADERS)
        target_set_precompiled_headers(pal "${PAL_SOURCE_DIR}/src/core/precompiledHeaders.h")

        set(PAL_UTIL_HEADER "${PAL_SOURCE_DIR}/src/util/precompiledHeaders.h")
        target_set_precompiled_headers(palUtil ${PAL_UTIL_HEADER})
    endif()
endfunction()
