##
 #######################################################################################################################
 #
 #  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

devdriver_library(dd_common STATIC)
target_include_directories(dd_common PUBLIC inc)
target_sources(dd_common
    PRIVATE
        inc/dd_assert.h
        inc/dd_constants.h
        inc/dd_dynamic_buffer.h
        inc/dd_integer.h
        inc/dd_memory.h
        inc/dd_mutex.h
        inc/dd_optional.h
        inc/dd_result.h
        inc/dd_ring_buffer.h
        inc/dd_thread.h
        src/dd_dynamic_buffer.cpp
        src/dd_logger.cpp
        src/dd_result.cpp
        src/dd_ring_buffer.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_sources(dd_common
        PRIVATE
            src/dd_memory_windows.cpp
            src/dd_result_windows.cpp
            src/dd_thread_windows.cpp
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(dd_common
        PRIVATE
            src/dd_memory_linux.cpp
            src/dd_thread_linux.cpp
    )
endif()

target_link_libraries(dd_common
    PUBLIC
        dd_interface
    PRIVATE
        stb_sprintf
)

if (WIN32)
    target_link_libraries(dd_common
        PRIVATE
            onecore # Windows library needed for VirtualAlloc2 and MapViewOfFile3
    )
endif()

if (DD_OPT_ASSERTS_ENABLE)
    target_compile_definitions(dd_common PUBLIC DD_OPT_ASSERTS_ENABLE)
endif()
