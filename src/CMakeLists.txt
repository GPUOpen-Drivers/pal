##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

include(PalVersionHelper)

# Nestly PAL targets under the label following label
set(CMAKE_FOLDER "${CMAKE_FOLDER}/PAL Libs")

### Create PAL Library #################################################################################################
add_library(pal STATIC)

### PAL Subprojects ####################################################################################################

### ADDRLIB ####################################################################
add_subdirectory(${PAL_ADDR_PATH} ${PROJECT_BINARY_DIR}/addrlib)
target_link_libraries(pal PRIVATE addrlib)

### VAM ########################################################################
add_subdirectory(${PAL_VAM_PATH} ${PROJECT_BINARY_DIR}/vam)
target_link_libraries(pal PRIVATE vam)

### METROHASH ##################################################################
if(NOT TARGET metrohash)
    add_subdirectory(${PAL_METROHASH_PATH} ${PROJECT_BINARY_DIR}/metrohash)
endif()

# Metrohash needs to be a public dependency.
# See: palMetroHash.h
target_link_libraries(pal PUBLIC metrohash)

### CWPACK #####################################################################
if(NOT TARGET cwpack)
    add_subdirectory(${PAL_CWPACK_PATH} ${PROJECT_BINARY_DIR}/cwpack)
endif()

# cwpack needs to be a public dependency.
# See: palMsgPack.h
target_link_libraries(pal PUBLIC cwpack)

### GPUOPEN ####################################################################
if(PAL_BUILD_GPUOPEN)
    add_subdirectory(${PAL_GPUOPEN_PATH} ${PROJECT_BINARY_DIR}/gpuopen)

    # Gpuopen needs to be a public dependency.
    # Clients include "gpuopen.h"
    target_link_libraries(pal PUBLIC gpuopen)
endif()

### Link Libraries #####################################################################################################
include(FindThreads)
target_link_libraries(pal PRIVATE ${CMAKE_DL_LIBS})
target_link_libraries(pal PRIVATE Threads::Threads)

### Include Directories ################################################################################################

# PAL Include Directories
target_include_directories(pal
    PUBLIC
        ${PAL_SOURCE_DIR}/inc
        ${PAL_SOURCE_DIR}/inc/core
        ${PAL_SOURCE_DIR}/inc/gpuUtil
        ${PAL_SOURCE_DIR}/inc/util
        ${PAL_SOURCE_DIR}/shared/inc
    PRIVATE
        ${PAL_SOURCE_DIR}/res
        ${PAL_SOURCE_DIR}/src
)

set(PAL_XCB_REQUIRED ON)

if(UNIX AND PAL_XCB_REQUIRED)
    # Needed for dri3Loader.h
    if(NOT DEFINED DK_X11)
        if(DEFINED GLOBAL_ROOT_DK_DIR)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.19_64a)
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.19_x86)
            endif()
            target_include_directories(pal PUBLIC ${DK_X11}/include)
            target_include_directories(pal PUBLIC ${DK_X11}/include/linux)
        else()
            find_package(X11 REQUIRED)
            set(DK_X11 ${X11_INCLUDE_DIR})
            target_include_directories(pal PUBLIC ${DK_X11})
        endif()

        message_debug("DK_X11 not set.  Defaulting to ${DK_X11}")
    endif()

    if(NOT DEFINED DK_XCB)
        if(DEFINED GLOBAL_ROOT_DK_DIR)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(DK_XCB ${GLOBAL_ROOT_DK_DIR}/xcb/1.13_64a)
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(DK_XCB ${GLOBAL_ROOT_DK_DIR}/xcb/1.13_x86)
            endif()
            target_include_directories(pal PUBLIC ${DK_XCB}/include)
        else()
            find_package(XCB)
            set(DK_XCB ${XCB_INCLUDE_DIRS})
            target_include_directories(pal PUBLIC ${DK_XCB})
        endif()
        message_debug("DK_XCB not set.  Defaulting to ${DK_XCB}")
    endif()
    target_include_directories(pal PRIVATE ${DK_XCB}/include)
endif()

if(PAL_AMDGPU_BUILD)
    target_include_directories(pal PRIVATE ${PAL_SOURCE_DIR}/src/core/os/amdgpu/include/2d)
    target_include_directories(pal PRIVATE ${PAL_SOURCE_DIR}/src/core/os/amdgpu/include/drm)
endif()

# Other Includes
target_include_directories(pal PRIVATE ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared)

### PAL Sources ########################################################################################################

### Add cmake files  ###################################################################################################
target_sources(pal PRIVATE
    ${PAL_SOURCE_DIR}/CMakeLists.txt
    # PAL/XGL modules
    ${PAL_SOURCE_DIR}/cmake/Modules/SetupAmdGlobalRoots.cmake
    ${PAL_SOURCE_DIR}/cmake/Modules/AmdCmakeHelper.cmake
    ${PAL_SOURCE_DIR}/cmake/Modules/FindDRM.cmake
    ${PAL_SOURCE_DIR}/cmake/Modules/FindWayland.cmake
    ${PAL_SOURCE_DIR}/cmake/Modules/FindXCB.cmake
    # PAL modules
    ${PAL_SOURCE_DIR}/cmake/PalCompileDefinitions.cmake
    ${PAL_SOURCE_DIR}/cmake/PalCompilerOptions.cmake
    ${PAL_SOURCE_DIR}/cmake/PalCompilerWarnings.cmake
    ${PAL_SOURCE_DIR}/cmake/PalOptions.cmake
    ${PAL_SOURCE_DIR}/cmake/PalOverrides.cmake
    ${PAL_SOURCE_DIR}/cmake/PalVersionHelper.cmake
)

### PAL core ###################################################################
if(PAL_BUILD_CORE)
    # CMAKE-TODO: check if this is still valid with the cmake targets.  I don't think it matters.
    # Also- should the generated files actually be added to be out of source (in the build directory)?
    # Pal settings must come before libInit or any of the other core files since there will be dependencies on the
    # generated files.
    target_sources(pal PRIVATE
        core/g_palSettings.cpp
        core/settings_core.json
        core/g_palPlatformSettings.cpp
        core/settings_platform.json
    )
endif()

# CMAKE-TODO: Check if this is supposed to be added outside of core support....?
target_sources(pal PRIVATE core/libInit.cpp)

if(PAL_BUILD_CORE)
    # Add rest of core files here, only if the client wants core support.  Util files are always required.
    target_sources(pal PRIVATE
        core/cmdAllocator.cpp
        core/cmdBuffer.cpp
        core/cmdStream.cpp
        core/cmdStreamAllocation.cpp
        core/device.cpp
        core/dmaUploadRing.cpp
        core/engine.cpp
        core/eventProvider.cpp
        core/fence.cpp
        core/formatInfo.cpp
        core/gpuEvent.cpp
        core/gpuMemPatchList.cpp
        core/gpuMemory.cpp
        core/image.cpp
        core/internalMemMgr.cpp
        core/masterQueueSemaphore.cpp
        core/openedQueueSemaphore.cpp
        core/palSettingsLoader.cpp
        core/perfExperiment.cpp
        core/platform.cpp
        core/platformSettingsLoader.cpp
        core/presentScheduler.cpp
        core/privateScreen.cpp
        core/queue.cpp
        core/queueContext.cpp
        core/queueSemaphore.cpp
        core/settingsLoader.cpp
        core/svmMgr.cpp
        core/swapChain.cpp
        core/vamMgr.cpp
        core/dmaCmdBuffer.cpp
    )

    target_include_directories(pal PRIVATE core/)

    if(PAL_BUILD_GPUOPEN)
        target_sources(pal PRIVATE
            core/devDriverUtil.cpp
            core/devDriverEventService.cpp
        )
    endif()

### PAL core/addrMgr ###########################################################
    # Address library support is required for core support
    target_sources(pal PRIVATE core/addrMgr/addrMgr.cpp)

    if(PAL_BUILD_GFX6)
        # Address manager support specific to GFX6-8
        target_sources(pal PRIVATE core/addrMgr/addrMgr1/addrMgr1.cpp)
    endif()

    if(PAL_BUILD_GFX9)
        # Address manager support specific to GFX9
        target_sources(pal PRIVATE core/addrMgr/addrMgr2/addrMgr2.cpp)
    endif()

### PAL core/os ################################################################
    target_sources(pal PRIVATE
        core/os/nullDevice/ndDevice.cpp
        core/os/nullDevice/ndDmaUploadRing.cpp
        core/os/nullDevice/ndGpuMemory.cpp
        core/os/nullDevice/ndPlatform.cpp
        core/os/nullDevice/ndQueue.cpp
        core/os/nullDevice/ndFence.cpp
    )

    if(PAL_AMDGPU_BUILD)
        target_sources(pal PRIVATE
            core/os/amdgpu/amdgpuDevice.cpp
            core/os/amdgpu/amdgpuDmaUploadRing.cpp
            core/os/amdgpu/amdgpuGpuMemory.cpp
            core/os/amdgpu/amdgpuImage.cpp
            core/os/amdgpu/amdgpuPlatform.cpp
            core/os/amdgpu/amdgpuPresentScheduler.cpp
            core/os/amdgpu/amdgpuPrivateScreen.cpp
            core/os/amdgpu/amdgpuQueue.cpp
            core/os/amdgpu/amdgpuQueueSemaphore.cpp
            core/os/amdgpu/amdgpuScreen.cpp
            core/os/amdgpu/amdgpuSyncobjFence.cpp
            core/os/amdgpu/amdgpuSwapChain.cpp
            core/os/amdgpu/amdgpuTimestampFence.cpp
            core/os/amdgpu/amdgpuWindowSystem.cpp
            core/os/amdgpu/amdgpuVamMgr.cpp
            core/os/amdgpu/display/displayWindowSystem.cpp
            core/os/amdgpu/g_drmLoader.cpp
        )

        if(PAL_BUILD_DRI3)
            target_include_directories(pal PRIVATE ${PAL_SOURCE_DIR}/src/core/os/amdgpu/dri3)
            target_sources(pal PRIVATE
               core/os/amdgpu/dri3/dri3WindowSystem.cpp
               core/os/amdgpu/dri3/g_dri3Loader.cpp
            )
        endif()

        if(PAL_BUILD_WAYLAND)
            find_package(Wayland)
            target_include_directories(pal PRIVATE ${PAL_SOURCE_DIR}/src/core/os/amdgpu/wayland)
            target_sources(pal PRIVATE
                core/os/amdgpu/wayland/g_waylandLoader.cpp
                core/os/amdgpu/wayland/waylandWindowSystem.cpp
                core/os/amdgpu/wayland/mesa/wayland-drm-protocol.c
            )
            set_property(SOURCE core/os/amdgpu/wayland/mesa/wayland-drm-protocol.c
                         APPEND PROPERTY COMPILE_DEFINITIONS
                         wl_buffer_interface=wlBufferInterface)
            target_include_directories(pal PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIR})
        endif()

    endif()

### PAL core/hw ################################################################
    if(PAL_BUILD_GFX)
        target_sources(pal PRIVATE
            core/hw/gfxip/borderColorPalette.cpp
            core/hw/gfxip/cmdUploadRing.cpp
            core/hw/gfxip/computeCmdBuffer.cpp
            core/hw/gfxip/computePipeline.cpp
            core/hw/gfxip/gfxBlendOptimizer.cpp
            core/hw/gfxip/gfxCmdBuffer.cpp
            core/hw/gfxip/gfxCmdStream.cpp
            core/hw/gfxip/gfxDevice.cpp
            core/hw/gfxip/gfxImage.cpp
            core/hw/gfxip/graphicsPipeline.cpp
            core/hw/gfxip/indirectCmdGenerator.cpp
            core/hw/gfxip/pipeline.cpp
            core/hw/gfxip/queryPool.cpp
            core/hw/gfxip/shaderLibrary.cpp
            core/hw/gfxip/universalCmdBuffer.cpp
        )

        # Resource Processing Manager (RPM) files.
        target_sources(pal PRIVATE
            core/hw/gfxip/rpm/g_rpmComputePipelineInit.cpp
            core/hw/gfxip/rpm/g_rpmGfxPipelineInit.cpp
            core/hw/gfxip/rpm/rpmUtil.cpp
            core/hw/gfxip/rpm/rsrcProcMgr.cpp
        )

        if(PAL_BUILD_GFX6)
            # Add the GFX6 files here, only if the client wants GFXIP 6 support.
            target_sources(pal PRIVATE
                core/hw/gfxip/gfx6/g_gfx6PalSettings.cpp
                core/hw/gfxip/gfx6/settings_gfx6.json
                core/hw/gfxip/gfx6/gfx6Barrier.cpp
                core/hw/gfxip/gfx6/gfx6BorderColorPalette.cpp
                core/hw/gfxip/gfx6/gfx6CmdStream.cpp
                core/hw/gfxip/gfx6/gfx6CmdUploadRing.cpp
                core/hw/gfxip/gfx6/gfx6CmdUtil.cpp
                core/hw/gfxip/gfx6/gfx6ColorBlendState.cpp
                core/hw/gfxip/gfx6/gfx6ColorTargetView.cpp
                core/hw/gfxip/gfx6/gfx6ComputeCmdBuffer.cpp
                core/hw/gfxip/gfx6/gfx6ComputeEngine.cpp
                core/hw/gfxip/gfx6/gfx6ComputePipeline.cpp
                core/hw/gfxip/gfx6/gfx6DepthStencilState.cpp
                core/hw/gfxip/gfx6/gfx6DepthStencilView.cpp
                core/hw/gfxip/gfx6/gfx6Device.cpp
                core/hw/gfxip/gfx6/gfx6FormatInfo.cpp
                core/hw/gfxip/gfx6/gfx6GraphicsPipeline.cpp
                core/hw/gfxip/gfx6/gfx6Image.cpp
                core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.cpp
                core/hw/gfxip/gfx6/gfx6MaskRam.cpp
                core/hw/gfxip/gfx6/gfx6MsaaState.cpp
                core/hw/gfxip/gfx6/gfx6OcclusionQueryPool.cpp
                core/hw/gfxip/gfx6/gfx6PerfCtrInfo.cpp
                core/hw/gfxip/gfx6/gfx6PerfExperiment.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineStatsQueryPool.cpp
                core/hw/gfxip/gfx6/gfx6Pm4Optimizer.cpp
                core/hw/gfxip/gfx6/gfx6QueueContexts.cpp
                core/hw/gfxip/gfx6/gfx6SettingsLoader.cpp
                core/hw/gfxip/gfx6/gfx6ShaderRing.cpp
                core/hw/gfxip/gfx6/gfx6ShaderRingSet.cpp
                core/hw/gfxip/gfx6/gfx6StreamoutStatsQueryPool.cpp
                core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.cpp
                core/hw/gfxip/gfx6/gfx6UniversalEngine.cpp
                core/hw/gfxip/gfx6/gfx6WorkaroundState.cpp
            )

            # Resource Processing Manager(RPM) GFXIP 6 files.
            target_sources(pal PRIVATE core/hw/gfxip/rpm/gfx6/gfx6RsrcProcMgr.cpp)

        endif() # PAL_BUILD_GFX6

        if(PAL_BUILD_GFX9)
            # Add the GFX9 files here, only if the client wants GFXIP 9 support.
            target_sources(pal PRIVATE
                core/hw/gfxip/gfx9/g_gfx9PalSettings.cpp
                core/hw/gfxip/gfx9/settings_gfx9.json
                core/hw/gfxip/gfx9/gfx9Barrier.cpp
                core/hw/gfxip/gfx9/gfx9BorderColorPalette.cpp
                core/hw/gfxip/gfx9/gfx9CmdStream.cpp
                core/hw/gfxip/gfx9/gfx9CmdUploadRing.cpp
                core/hw/gfxip/gfx9/gfx9CmdUtil.cpp
                core/hw/gfxip/gfx9/gfx9ColorBlendState.cpp
                core/hw/gfxip/gfx9/gfx9ColorTargetView.cpp
                core/hw/gfxip/gfx9/gfx9ComputeCmdBuffer.cpp
                core/hw/gfxip/gfx9/gfx9ComputeEngine.cpp
                core/hw/gfxip/gfx9/gfx9ComputePipeline.cpp
                core/hw/gfxip/gfx9/gfx9DepthStencilState.cpp
                core/hw/gfxip/gfx9/gfx9DepthStencilView.cpp
                core/hw/gfxip/gfx9/gfx9Device.cpp
                core/hw/gfxip/gfx9/gfx9FormatInfo.cpp
                core/hw/gfxip/gfx9/gfx9GraphicsPipeline.cpp
                core/hw/gfxip/gfx9/gfx9Image.cpp
                core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.cpp
                core/hw/gfxip/gfx9/gfx9MaskRam.cpp
                core/hw/gfxip/gfx9/gfx9MetaEq.cpp
                core/hw/gfxip/gfx9/gfx9MsaaState.cpp
                core/hw/gfxip/gfx9/gfx9OcclusionQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9PerfCtrInfo.cpp
                core/hw/gfxip/gfx9/gfx9PerfExperiment.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkCs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkGs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkHs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9Pm4Optimizer.cpp
                core/hw/gfxip/gfx9/gfx9QueueContexts.cpp
                core/hw/gfxip/gfx9/gfx9SettingsLoader.cpp
                core/hw/gfxip/gfx9/gfx9ShaderLibrary.cpp
                core/hw/gfxip/gfx9/gfx9ShaderRing.cpp
                core/hw/gfxip/gfx9/gfx9ShaderRingSet.cpp
                core/hw/gfxip/gfx9/gfx9StreamoutStatsQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.cpp
                core/hw/gfxip/gfx9/gfx9UniversalEngine.cpp
                core/hw/gfxip/gfx9/gfx9WorkaroundState.cpp
                core/hw/gfxip/gfx9/gfx9AcquireReleaseBarrier.cpp
            )

            # Resource Processing Manager(RPM) GFXIP 9 files.
            target_sources(pal PRIVATE core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.cpp)

            target_sources(pal PRIVATE core/hw/gfxip/gfx9/gfx10DmaCmdBuffer.cpp)

        endif() # PAL_BUILD_GFX9
    endif() # PAL_BUILD_GFX

    if(PAL_BUILD_OSS)
        if(PAL_BUILD_OSS1)
            # Add the OSS1 files here, only if the client wants OSSIP 1 support.
            target_sources(pal PRIVATE
                core/hw/ossip/oss1/oss1Device.cpp
                core/hw/ossip/oss1/oss1DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS2)
            # Add the OSS2 files here, only if the client wants OSSIP 2 support.
            target_sources(pal PRIVATE
                core/hw/ossip/oss2/oss2Device.cpp
                core/hw/ossip/oss2/oss2DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS2_4)
            # Add the OSS2.4 files here, only if the client wants OSSIP 2.4 support.
            target_sources(pal PRIVATE
                core/hw/ossip/oss2_4/oss2_4Device.cpp
                core/hw/ossip/oss2_4/oss2_4DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS4)
            # Add the OSS4 files here, only if the client wants OSSIP 4 support.
            target_sources(pal PRIVATE
                core/hw/ossip/oss4/oss4Device.cpp
                core/hw/ossip/oss4/oss4DmaCmdBuffer.cpp
            )
        endif()
    endif()

### PAL core/layers ############################################################
    if(PAL_BUILD_LAYERS)
        target_sources(pal PRIVATE core/layers/decorators.cpp)

        if(PAL_BUILD_DBG_OVERLAY)
            # Add the debug overlay files here, only if the client wants debug overlay support.
            target_sources(pal PRIVATE
                core/layers/dbgOverlay/dbgOverlayCmdBuffer.cpp
                core/layers/dbgOverlay/dbgOverlayDevice.cpp
                core/layers/dbgOverlay/dbgOverlayFpsMgr.cpp
                core/layers/dbgOverlay/dbgOverlayImage.cpp
                core/layers/dbgOverlay/dbgOverlayPlatform.cpp
                core/layers/dbgOverlay/dbgOverlayQueue.cpp
                core/layers/dbgOverlay/dbgOverlayTextWriter.cpp
                core/layers/dbgOverlay/dbgOverlayTimeGraph.cpp
            )
        endif()

        if(PAL_BUILD_GPU_PROFILER)
            # Add the GPU profiler files here, only if the client wants GPU profiler support.
            target_sources(pal PRIVATE
                core/layers/gpuProfiler/gpuProfilerCmdBuffer.cpp
                core/layers/gpuProfiler/gpuProfilerDevice.cpp
                core/layers/gpuProfiler/gpuProfilerPlatform.cpp
                core/layers/gpuProfiler/gpuProfilerQueue.cpp
                core/layers/gpuProfiler/gpuProfilerQueueFileLogger.cpp
                core/layers/gpuProfiler/gpuProfilerPipeline.cpp
            )
        endif()

        target_sources(pal PRIVATE
            core/layers/cmdBufferLogger/cmdBufferLoggerCmdBuffer.cpp
            core/layers/cmdBufferLogger/cmdBufferLoggerDevice.cpp
            core/layers/cmdBufferLogger/cmdBufferLoggerImage.cpp
            core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.cpp
        )

        target_sources(pal PRIVATE
            core/layers/gpuDebug/gpuDebugCmdBuffer.cpp
            core/layers/gpuDebug/gpuDebugColorBlendState.cpp
            core/layers/gpuDebug/gpuDebugColorTargetView.cpp
            core/layers/gpuDebug/gpuDebugDevice.cpp
            core/layers/gpuDebug/gpuDebugImage.cpp
            core/layers/gpuDebug/gpuDebugPipeline.cpp
            core/layers/gpuDebug/gpuDebugPlatform.cpp
            core/layers/gpuDebug/gpuDebugQueue.cpp
        )

        target_sources(pal PRIVATE
            core/layers/interfaceLogger/interfaceLoggerBorderColorPalette.cpp
            core/layers/interfaceLogger/interfaceLoggerCmdAllocator.cpp
            core/layers/interfaceLogger/interfaceLoggerCmdBuffer.cpp
            core/layers/interfaceLogger/interfaceLoggerColorBlendState.cpp
            core/layers/interfaceLogger/interfaceLoggerDepthStencilState.cpp
            core/layers/interfaceLogger/interfaceLoggerDevice.cpp
            core/layers/interfaceLogger/interfaceLoggerFence.cpp
            core/layers/interfaceLogger/interfaceLoggerGpuEvent.cpp
            core/layers/interfaceLogger/interfaceLoggerGpuMemory.cpp
            core/layers/interfaceLogger/interfaceLoggerImage.cpp
            core/layers/interfaceLogger/interfaceLoggerIndirectCmdGenerator.cpp
            core/layers/interfaceLogger/interfaceLoggerLogContext.cpp
            core/layers/interfaceLogger/interfaceLoggerLogContextEnum.cpp
            core/layers/interfaceLogger/interfaceLoggerLogContextStruct.cpp
            core/layers/interfaceLogger/interfaceLoggerMsaaState.cpp
            core/layers/interfaceLogger/interfaceLoggerPipeline.cpp
            core/layers/interfaceLogger/interfaceLoggerPlatform.cpp
            core/layers/interfaceLogger/interfaceLoggerPrivateScreen.cpp
            core/layers/interfaceLogger/interfaceLoggerQueryPool.cpp
            core/layers/interfaceLogger/interfaceLoggerQueue.cpp
            core/layers/interfaceLogger/interfaceLoggerQueueSemaphore.cpp
            core/layers/interfaceLogger/interfaceLoggerScreen.cpp
            core/layers/interfaceLogger/interfaceLoggerShaderLibrary.cpp
            core/layers/interfaceLogger/interfaceLoggerSwapChain.cpp

        )

        target_sources(pal PRIVATE
            core/layers/pm4Instrumentor/pm4InstrumentorCmdBuffer.cpp
            core/layers/pm4Instrumentor/pm4InstrumentorDevice.cpp
            core/layers/pm4Instrumentor/pm4InstrumentorPlatform.cpp
            core/layers/pm4Instrumentor/pm4InstrumentorQueue.cpp
        )

    endif() # PAL_BUILD_LAYERS

endif() # PAL_BUILD_CORE

### PAL gpuUtil ################################################################
if(PAL_BUILD_GPUUTIL)
    target_sources(pal PRIVATE
        gpuUtil/appProfileIterator.cpp
        gpuUtil/gpaSession.cpp
        gpuUtil/gpuUtil.cpp
        gpuUtil/gpaSessionPerfSample.cpp
    )
endif()

### PAL util ###################################################################
target_sources(pal PRIVATE
    util/assert.cpp
    util/dbgPrint.cpp
    util/cacheLayerBase.cpp
    util/elfReader.cpp
    util/file.cpp
    util/fileArchiveCacheLayer.cpp
    util/jsonWriter.cpp
    util/math.cpp
    util/md5.cpp
    util/memMapFile.cpp
    util/memoryCacheLayer.cpp
    util/pipelineAbiReader.cpp
    util/stringUtil.cpp
    util/sysMemory.cpp
    util/sysUtil.cpp
    util/trackingCacheLayer.cpp
    util/platformKey.cpp
)

if(UNIX)
### PAL util/lnx ###############################################################
    target_sources(pal PRIVATE
        util/lnx/lnxArchiveFile.cpp
        util/lnx/lnxConditionVariable.cpp
        util/lnx/lnxEvent.cpp
        util/lnx/lnxFileMap.cpp
        util/lnx/lnxHashProvider.cpp
        util/lnx/lnxLibrary.cpp
        util/lnx/lnxMutex.cpp
        util/lnx/lnxOpenssl.cpp
        util/lnx/lnxSemaphore.cpp
        util/lnx/lnxSysMemory.cpp
        util/lnx/lnxSysUtil.cpp
        util/lnx/lnxThread.cpp
        util/lnx/lnxTimeout.cpp
    )
endif()

### Find headers and build source groups ###############################################################################
target_find_headers(pal)
target_source_groups(pal)

