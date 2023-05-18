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

add_subdirectory(addrMgr)
add_subdirectory(hw)
add_subdirectory(imported)
add_subdirectory(layers)
add_subdirectory(os)
add_subdirectory(g_service)
add_subdirectory(rpcDriverUtilsService)

# Add required rpc modules from DevDriver
target_link_libraries(pal PRIVATE
    UberTraceService
    DriverUtilsService
    ddNet
)

target_sources(pal PRIVATE
    CMakeLists.txt
    cmdAllocator.cpp
    cmdAllocator.h
    cmdBuffer.cpp
    cmdBuffer.h
    cmdStream.cpp
    cmdStream.h
    cmdStreamAllocation.cpp
    cmdStreamAllocation.h
    dbgLoggerDevDriver.cpp
    devDriverEventServiceConv.h
    devDriverUtil.cpp
    devDriverUtil.h
    device.cpp
    device.h
    dmaCmdBuffer.cpp
    dmaCmdBuffer.h
    dmaUploadRing.cpp
    dmaUploadRing.h
    engine.cpp
    engine.h
    eventDefs.h
    fence.cpp
    fence.h
    formatInfo.cpp
    g_mergedFormatInfo.h
    gpuEvent.cpp
    gpuEvent.h
    gpuMemory.cpp
    gpuMemory.h
    gpuMemoryEventProvider.cpp
    gpuMemoryEventProvider.h
    gpuMemPatchList.cpp
    gpuMemPatchList.h
    image.cpp
    image.h
    internalMemMgr.cpp
    internalMemMgr.h
    libInit.cpp
    masterQueueSemaphore.cpp
    masterQueueSemaphore.h
    openedQueueSemaphore.cpp
    openedQueueSemaphore.h
    palSettingsLoader.cpp
    perfExperiment.cpp
    perfExperiment.h
    platform.cpp
    platform.h
    platformSettingsLoader.cpp
    platformSettingsLoader.h
    presentScheduler.cpp
    presentScheduler.h
    privateScreen.cpp
    privateScreen.h
    queue.cpp
    queue.h
    queueContext.cpp
    queueContext.h
    queueSemaphore.cpp
    queueSemaphore.h
    settings_core.json
    settings_platform.json
    settingsLoader.cpp
    settingsLoader.h
    svmMgr.cpp
    svmMgr.h
    swapChain.cpp
    swapChain.h
    vamMgr.cpp
    vamMgr.h
    driverUtilsService.h
    driverUtilsService.cpp
)
