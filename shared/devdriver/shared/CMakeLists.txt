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

set(CMAKE_FOLDER "${CMAKE_FOLDER}/shared")

add_subdirectory(ddApi)
add_subdirectory(ddCommon)
add_subdirectory(ddYaml)

if (DD_BUILD_RDF)
    add_subdirectory(ddRdf)
endif()

if (DD_BP_ENABLE_DD_TEST_UTIL)
    add_subdirectory(ddTestUtil)
endif()

if (DD_BP_ENABLE_DD_MODULE_APIS)
    add_subdirectory(ddModule)
    add_subdirectory(ddModuleExtensions)
    add_subdirectory(ddModuleLoader)
    add_subdirectory(ddModuleUtil)
    add_subdirectory(protocols/info/g_client)
    add_subdirectory(ddGpuDetectiveEventStreamer)
    add_subdirectory(ddRdfEventStreamer)
    add_subdirectory(ddRmtEventTracer)
    add_subdirectory(ddAmdLogUtils)
endif()

add_subdirectory(legacy)

if (DD_BP_ENABLE_TOOL_LIBRARIES)
    add_subdirectory(ddGpuInfo)
endif()
