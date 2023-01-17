##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

### ddRpcServer #######################################################################################################
devdriver_library(ddRpcServer STATIC)
target_link_libraries(ddRpcServer
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        ddRpcShared
        ddSocket
        ddNet
        ddCore
)

if (DD_BP_INSTALL)
	install(FILES   inc/ddRpcServerApi.h DESTINATION inc)
	install(TARGETS ddRpcServer          DESTINATION lib)
endif()

target_include_directories(ddRpcServer PUBLIC  inc)
target_include_directories(ddRpcServer PRIVATE src)

target_sources(ddRpcServer PRIVATE
    inc/ddRpcServerApi.h
    inc/ddRpcServer.h
    src/ddRpcServer.cpp
    src/rpcServer.h
    src/rpcServer.cpp
    src/rpcClientHandler.h
    src/rpcClientHandler.cpp
)
