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

### Utility Functionality For Tests ###################################################################################
devdriver_library(ddTestUtil STATIC)

target_link_libraries(ddTestUtil
    PUBLIC
        ddNet
        ddRouter
        ddCommon
        gtest
        ddRpcClient
        ddRpcServer
)

target_compile_definitions(ddTestUtil
    PUBLIC
        # gpuopen.h requires this been defined.
        # This is public so any test taking a dependency on this will have this
        # definition.
        GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION=42
)

target_include_directories(ddTestUtil
    PUBLIC
        inc
    PRIVATE
        src
)

target_sources(ddTestUtil PRIVATE
    inc/ddTestUtil.h
    src/ddTestUtil.cpp
)
