##
 #######################################################################################################################
 #
 #  Copyright (c) 2021-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

add_subdirectory(display)

if(PAL_BUILD_DRI3)
    add_subdirectory(dri3)
endif()

add_subdirectory(include)

if(PAL_BUILD_WAYLAND)
    add_subdirectory(wayland)
endif()

target_sources(pal PRIVATE
    amdgpuDevice.cpp
    amdgpuDevice.h
    amdgpuDmaUploadRing.cpp
    amdgpuDmaUploadRing.h
    amdgpuGpuMemory.cpp
    amdgpuGpuMemory.h
    amdgpuHeaders.h
    amdgpuImage.cpp
    amdgpuImage.h
    amdgpuPlatform.cpp
    amdgpuPlatform.h
    amdgpuPresentScheduler.cpp
    amdgpuPresentScheduler.h
    amdgpuPrivateScreen.cpp
    amdgpuQueue.cpp
    amdgpuQueue.h
    amdgpuQueueSemaphore.cpp
    amdgpuScreen.cpp
    amdgpuScreen.h
    amdgpuSwapChain.cpp
    amdgpuSwapChain.h
    amdgpuSyncobjFence.cpp
    amdgpuSyncobjFence.h
    amdgpuTimestampFence.cpp
    amdgpuTimestampFence.h
    amdgpuVamMgr.cpp
    amdgpuVamMgr.h
    amdgpuWindowSystem.cpp
    amdgpuWindowSystem.h
    CMakeLists.txt
    drmLoader.proc
    g_drmLoader.cpp
    g_drmLoader.h
)
