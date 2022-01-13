##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

if(NOT DEFINED GLOBAL_ROOT_SRC_DIR)
    execute_process(
        COMMAND find_depth
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GLOBAL_ROOT_SRC_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GLOBAL_ROOT_SRC_DIR_RESULT
    )
    if(GLOBAL_ROOT_SRC_DIR_RESULT EQUAL 0)
        set(GLOBAL_ROOT_SRC_DIR ${CMAKE_SOURCE_DIR}/${GLOBAL_ROOT_SRC_DIR} CACHE PATH "Global root source directory.")
    else()
        message(FATAL_ERROR "The GLOBAL_ROOT_SRC_DIR could not be defined, because calling find_depth failed!")
    endif()
endif()
