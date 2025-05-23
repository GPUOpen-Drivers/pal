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

add_subdirectory(mlaa)
add_subdirectory(textWriter)
add_subdirectory(timeGraph)
add_subdirectory(msaaImageCopy)

target_include_directories(pal PUBLIC .)

target_sources(pal PRIVATE
    CMakeLists.txt
    palAppProfileIterator.h
    palCodeObjectTraceSource.h
    palGpaSession.h
    palGpuEventPool.h
    palGpuEventPoolImpl.h
    palGpuUtil.h
    palMlaaUtil.h
    palMlaaUtilImpl.h
    palMsaaImageCopyUtil.h
    palMsaaImageCopyUtilImpl.h
    palQueueTimingsTraceSource.h
    palStringTableTraceSource.h
    palRenderOpTraceController.h
    palTextWriter.h
    palTextWriterFont.h
    palTextWriterImpl.h
    palTimeGraph.h
    palTimeGraphImpl.h
    palTraceSession.h
    palUserMarkerHistoryTraceSource.h
)

