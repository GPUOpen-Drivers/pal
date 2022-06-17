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

### ddEventClient #####################################################################################################
devdriver_library(ddEventClient STATIC)
target_link_libraries(ddEventClient
    PUBLIC
        ddApi
    PRIVATE
        ddCommon
        devdriver
)

if (DD_BP_INSTALL)
    install(FILES   inc/ddEventClientApi.h DESTINATION inc)
    install(TARGETS ddEventClient          DESTINATION lib)
endif()

target_include_directories(ddEventClient PUBLIC  inc)
target_include_directories(ddEventClient PRIVATE src)

target_sources(ddEventClient PRIVATE
    inc/ddEventClientApi.h
    inc/ddEventClient.h
    src/ddEventClient.cpp

    src/eventClient.h
    src/eventClient.cpp

    src/legacy/legacyEventClient.h
    src/legacy/legacyEventClient.cpp
)
