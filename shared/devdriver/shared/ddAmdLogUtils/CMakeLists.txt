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

####################################################################################
# This file is auto generated, see the source for details on updating this
####################################################################################

add_library(AmdLogUtilsClient STATIC)

set_target_properties(AmdLogUtilsClient
    PROPERTIES
        CXX_STANDARD 11
)

target_sources(AmdLogUtilsClient PRIVATE
    inc/g_AmdLogUtilsClient.h
    src/g_AmdLogUtilsClient.cpp
)

target_include_directories(AmdLogUtilsClient
    PUBLIC
        inc
    PRIVATE
        src
)

target_link_libraries(AmdLogUtilsClient
    PRIVATE
        ddCommon
    PUBLIC
        ddRpcClient
)

# Need to apply devdriver specific warning configuration, because
# this target indirectly includes ddPlatform.h which causes some
# compiler warnings.
apply_devdriver_warnings(AmdLogUtilsClient)
