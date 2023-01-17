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

### ddEventServer #####################################################################################################
devdriver_library(ddEventServer STATIC)
target_link_libraries(ddEventServer
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
    install(FILES   inc/ddEventServerApi.h DESTINATION inc)
    install(TARGETS ddEventServer          DESTINATION lib)
endif()

target_include_directories(ddEventServer PUBLIC  inc)
target_include_directories(ddEventServer PRIVATE src)

target_sources(ddEventServer PRIVATE
    inc/ddEventServerApi.h
    inc/ddEventServer.h
    src/ddEventServer.cpp

    src/eventServer.h
    src/eventServer.cpp
    src/eventProvider.h
    src/eventProvider.cpp
    src/eventShared.h
)
