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

add_subdirectory(imported)

if (UNIX)
    add_subdirectory(lnx)
endif()

#if ANDROID
if(ANDROID)
    # Needed to pick up __android_log_write
    target_link_libraries(pal PUBLIC log)
endif()
#endif

target_sources(pal PRIVATE
    arFile.cpp
    assert.cpp
    CMakeLists.txt
    dbgLogger.cpp
    dbgLogMgr.cpp
    dbgPrint.cpp
    elfReader.cpp
    file.cpp
    jsonWriter.cpp
    lz4Compressor.cpp
    lz4Compressor.h
    math.cpp
    memMapFile.cpp
    platformKey.cpp
    platformKey.h
    stringUtil.cpp
    sysMemory.cpp
    sysUtil.cpp
    time.cpp
    uuid.cpp
)
