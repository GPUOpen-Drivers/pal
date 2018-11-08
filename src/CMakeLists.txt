##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
add_library(pal STATIC "")
install(TARGETS pal DESTINATION ${CMAKE_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}${TARGET_ARCHITECTURE_BITS})

### Compiler Options ###################################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-pthread -fPIC -fcheck-new -fno-rtti -fno-math-errno>)
        target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers>)
        target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -std=c++0x -Wno-error=comment -Wno-comment -Wno-error=pointer-arith -Wno-pointer-arith -mpreferred-stack-boundary=6 -fno-threadsafe-statics -fmerge-all-constants -fms-extensions>)
        if(CMAKE_BUILD_TYPE_RELEASE)
            execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
            if (GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                # CMAKE-TODO: Use check_ipo_supported macro when CMake >= 3.9
                target_compile_options(pal PRIVATE $<$<CONFIG:Release>:-flto  -fuse-linker-plugin -Wno-odr>)
                message(WARNING "LTO enabled for Pal")
            endif()
        endif()

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()

	set(PAL_USE_PTHREAD TRUE)

endif()

# CMAKE-TODO: Where is this used?
#target_compile_definitions(pal PRIVATE $<$<CONFIG:Debug>:DEBUG>)

### Build Definitions ##################################################################################################
target_compile_definitions(pal PUBLIC PAL_CLIENT_INTERFACE_MAJOR_VERSION=${PAL_CLIENT_INTERFACE_MAJOR_VERSION})

target_compile_definitions(pal PRIVATE PAL_COMPILE_TYPE=${TARGET_ARCHITECTURE_BITS})

target_compile_definitions(pal PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)

if(PAL_CLOSED_SOURCE)
    # Public because it is used in the interface.
    target_compile_definitions(pal PUBLIC PAL_CLOSED_SOURCE)
endif()

if(PAL_BUILD_GPUOPEN)
    # Public because it is used in the interface.
    target_compile_definitions(pal PUBLIC PAL_BUILD_GPUOPEN)
endif()

# CMAKE-TODO: Where are PAL_ENABLE_DEBUG_PRINTS and PAL_ENABLE_ASSERTS used?
if(PAL_ENABLE_PRINTS_ASSERTS)
    # CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
    #target_compile_definitions(pal PRIVATE
    #    PAL_ENABLE_PRINTS_ASSERTS=$<OR:?$<CONFIG:Debug$<BOOL:${PAL_ENABLE_PRINTS_ASSERTS}>>
    #)

    # Public because it is used in the interface.
    target_compile_definitions(pal PUBLIC PAL_ENABLE_PRINTS_ASSERTS)
endif()

if(PAL_DEVELOPER_BUILD)
    target_compile_definitions(pal PRIVATE PAL_DEVELOPER_BUILD)
endif()

if(PAL_DBG_COMMAND_COMMENTS)
    target_compile_definitions(pal PRIVATE PAL_DBG_COMMAND_COMMENTS)
endif()

if(PAL_MEMTRACK)
    # CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
    #target_compile_definitions(pal PRIVATE
    #    PAL_MEMTRACK=$<OR:?$<CONFIG:Debug$<BOOL:${PAL_MEMTRACK}>>
    #)

    # Public because it is used in the interface.
    target_compile_definitions(pal PUBLIC PAL_MEMTRACK)
endif()

set(PAL_CLIENT_${PAL_CLIENT} 1)
if(PAL_CLIENT_VULKAN)
    target_compile_definitions(pal PUBLIC PAL_CLIENT_VULKAN)
endif()

if(PAL_BUILD_DTIF)
    target_compile_definitions(pal PRIVATE PAL_BUILD_DTIF)
endif()

target_compile_definitions(pal PRIVATE PAL_BUILD_CORE)

### Include Directories ################################################################################################
# PAL Include Directories
target_include_directories(pal
    PUBLIC
        ${PROJECT_SOURCE_DIR}/inc
        ${PROJECT_SOURCE_DIR}/inc/core
        ${PROJECT_SOURCE_DIR}/inc/gpuUtil
        ${PROJECT_SOURCE_DIR}/inc/util
    PRIVATE
        ${PROJECT_SOURCE_DIR}/res
        ${PROJECT_SOURCE_DIR}/src
)

# Include UVD IP interface file.  It's public as the file is in the PAL interface.
target_include_directories(pal PUBLIC ${GLOBAL_ROOT_SRC_DIR}/drivers/uvdfwlib/uvdfw_inc)

# CMAKE-TODO: Check this for correctness.  Why does xgl set this and not PAL?
if(UNIX)
    # Needed for dri3Loader.h
    if(NOT DEFINED DK_X11)
        if(DEFINED GLOBAL_ROOT_DK_DIR)
            if(TARGET_ARCHITECTURE_BITS EQUAL 64)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.17_64a)
            elseif(TARGET_ARCHITECTURE_BITS EQUAL 32)
                set(DK_X11 ${GLOBAL_ROOT_DK_DIR}/xfree86/1.17_x86)
            endif()
            target_include_directories(pal PUBLIC ${DK_X11}/include)
            target_include_directories(pal PUBLIC ${DK_X11}/include/linux)
        else()
            find_package(X11 REQUIRED)
            set(DK_X11 ${X11_INCLUDE_DIR})
            target_include_directories(pal PUBLIC ${DK_X11})
        endif()

        message(STATUS "DK_X11 not set.  Defaulting to ${DK_X11}")
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
        message(STATUS "DK_XCB not set.  Defaulting to ${DK_XCB}")
    endif()
    target_include_directories(pal PRIVATE ${DK_XCB}/include)

    target_include_directories(pal PRIVATE ${PROJECT_SOURCE_DIR}/src/core/os/lnx/include/2d)
    target_include_directories(pal PRIVATE ${PROJECT_SOURCE_DIR}/src/core/os/lnx/include/drm)
endif()

# Other Includes
target_include_directories(pal PRIVATE ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/shared)
target_include_directories(pal PRIVATE ${GLOBAL_ROOT_SRC_DIR}/drivers/inc/asic_reg)

### PAL Subprojects ####################################################################################################
### ADDRLIB ####################################################################
add_subdirectory(${PAL_ADDR_PATH} ${PROJECT_BINARY_DIR}/addrlib)
target_link_libraries(pal PUBLIC addrlib)

### VAM ########################################################################
add_subdirectory(${PAL_VAM_PATH} ${PROJECT_BINARY_DIR}/vam)
target_link_libraries(pal PUBLIC vam)

### METROHASH ##################################################################
add_subdirectory(${PAL_METROHASH_PATH} ${PROJECT_BINARY_DIR}/metrohash)
target_link_libraries(pal PUBLIC metrohash)

### CWPACK #####################################################################
add_subdirectory(${PAL_CWPACK_PATH} ${PROJECT_BINARY_DIR}/cwpack)
target_link_libraries(pal PUBLIC cwpack)

### GPUOPEN ####################################################################
if(PAL_BUILD_GPUOPEN)
    add_subdirectory(${PAL_GPUOPEN_PATH} ${PROJECT_BINARY_DIR}/gpuopen)
    target_link_libraries(pal PUBLIC gpuopen)
endif()

### PAL Sources ########################################################################################################
### PAL core ###################################################################
if(PAL_BUILD_CORE)
    # CMAKE-TODO: check if this is still valid with the cmake targets.  I don't think it matters.
    # Also- should the generated files actually be added to be out of source (in the build directory)?
    # Pal settings must come before libInit or any of the other core files since there will be dependencies on the
    # generated files.
    target_sources(pal PRIVATE
        core/g_palSettings.cpp
        core/g_palPlatformSettings.cpp)
endif()

# CMAKE-TODO: Check if this is supposed to be added outside of core support....?
target_sources(pal PRIVATE core/libInit.cpp)

if(PAL_BUILD_CORE)
    # Add rest of core files here, only if the client wants core support.  Util files are always required.
    target_sources(pal PRIVATE
        core/g_heapPerf.cpp
        core/cmdAllocator.cpp
        core/cmdBuffer.cpp
        core/cmdStream.cpp
        core/cmdStreamAllocation.cpp
        core/device.cpp
        core/engine.cpp
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
        core/perfCounter.cpp
        core/perfExperiment.cpp
        core/perfTrace.cpp
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

    if(PAL_BUILD_GPUOPEN)
        target_sources(pal PRIVATE core/devDriverUtil.cpp)
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
    # We don't know if the null device is needed or not until run-time, so always compile it in.
    target_sources(pal PRIVATE
        core/os/nullDevice/ndDevice.cpp
        core/os/nullDevice/ndGpuMemory.cpp
        core/os/nullDevice/ndPlatform.cpp
        core/os/nullDevice/ndQueue.cpp
		core/os/nullDevice/ndFence.cpp
    )

    if(UNIX)
        target_sources(pal PRIVATE
            core/os/lnx/lnxDevice.cpp
            core/os/lnx/lnxTimestampFence.cpp
            core/os/lnx/lnxSyncobjFence.cpp
            core/os/lnx/lnxGpuMemory.cpp
            core/os/lnx/lnxImage.cpp
            core/os/lnx/lnxPlatform.cpp
            core/os/lnx/lnxPresentScheduler.cpp
            core/os/lnx/lnxPrivateScreen.cpp
            core/os/lnx/lnxQueue.cpp
            core/os/lnx/lnxQueueSemaphore.cpp
            core/os/lnx/lnxSwapChain.cpp
            core/os/lnx/drmLoader.cpp
            core/os/lnx/lnxVamMgr.cpp
            core/os/lnx/dri3/dri3WindowSystem.cpp
            core/os/lnx/dri3/dri3Loader.cpp
            core/os/lnx/display/displayWindowSystem.cpp
            core/os/lnx/lnxWindowSystem.cpp
            core/os/lnx/lnxScreen.cpp
        )

        # Check if enable wayland
        find_package(Wayland)

        if(WAYLAND_CLIENT_FOUND)
            message(STATUS "PAL build with Wayland enabled")
            target_include_directories(pal PRIVATE ${PROJECT_SOURCE_DIR}/src/core/os/lnx/wayland)
            target_sources(pal PRIVATE
            core/os/lnx/wayland/waylandWindowSystem.cpp
            core/os/lnx/wayland/g_waylandLoader.cpp
            core/os/lnx/wayland/mesa/wayland-drm-protocol.c
            )
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Dwl_buffer_interface=pWlBufferInterface")
            target_compile_definitions(pal PRIVATE PAL_HAVE_WAYLAND_PLATFORM)
            target_include_directories(pal PRIVATE WAYLAND_CLIENT_INCLUDE_DIR)
        endif()

    endif()

### PAL core/hw ################################################################
    if(PAL_BUILD_GFX)
        target_compile_definitions(pal PRIVATE PAL_BUILD_GFX)

        target_sources(pal PRIVATE
            core/hw/gfxip/borderColorPalette.cpp
            core/hw/gfxip/cmdUploadRing.cpp
            core/hw/gfxip/colorBlendState.cpp
            core/hw/gfxip/computeCmdBuffer.cpp
            core/hw/gfxip/computePipeline.cpp
            core/hw/gfxip/depthStencilState.cpp
            core/hw/gfxip/gfxBlendOptimizer.cpp
            core/hw/gfxip/gfxCmdBuffer.cpp
            core/hw/gfxip/gfxCmdStream.cpp
            core/hw/gfxip/gfxDevice.cpp
            core/hw/gfxip/gfxImage.cpp
            core/hw/gfxip/graphicsPipeline.cpp
            core/hw/gfxip/indirectCmdGenerator.cpp
            core/hw/gfxip/msaaState.cpp
            core/hw/gfxip/pipeline.cpp
            core/hw/gfxip/prefetchMgr.cpp
            core/hw/gfxip/queryPool.cpp
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
            target_compile_definitions(pal PRIVATE PAL_BUILD_GFX6)
            target_sources(pal PRIVATE
                core/hw/gfxip/gfx6/g_gfx6PalSettings.cpp
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
                core/hw/gfxip/gfx6/gfx6Gds.cpp
                core/hw/gfxip/gfx6/gfx6GraphicsPipeline.cpp
                core/hw/gfxip/gfx6/gfx6Image.cpp
                core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.cpp
                core/hw/gfxip/gfx6/gfx6MaskRam.cpp
                core/hw/gfxip/gfx6/gfx6MsaaState.cpp
                core/hw/gfxip/gfx6/gfx6OcclusionQueryPool.cpp
                core/hw/gfxip/gfx6/gfx6PerfCounter.cpp
                core/hw/gfxip/gfx6/gfx6PerfCtrInfo.cpp
                core/hw/gfxip/gfx6/gfx6PerfExperiment.cpp
                core/hw/gfxip/gfx6/gfx6PerfTrace.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkEsGs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkLsHs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineChunkVsPs.cpp
                core/hw/gfxip/gfx6/gfx6PipelineStatsQueryPool.cpp
                core/hw/gfxip/gfx6/gfx6Pm4Optimizer.cpp
                core/hw/gfxip/gfx6/gfx6PrefetchMgr.cpp
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

        endif()

        if(PAL_BUILD_GFX9)
            # Add the GFX9 files here, only if the client wants GFXIP 9 support.
            target_compile_definitions(pal PRIVATE PAL_BUILD_GFX9)
            target_sources(pal PRIVATE
                core/hw/gfxip/gfx9/g_gfx9PalSettings.cpp
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
                core/hw/gfxip/gfx9/gfx9Gds.cpp
                core/hw/gfxip/gfx9/gfx9GraphicsPipeline.cpp
                core/hw/gfxip/gfx9/gfx9Image.cpp
                core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.cpp
                core/hw/gfxip/gfx9/gfx9MaskRam.cpp
                core/hw/gfxip/gfx9/gfx9MetaEq.cpp
                core/hw/gfxip/gfx9/gfx9MsaaState.cpp
                core/hw/gfxip/gfx9/gfx9OcclusionQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9PerfCounter.cpp
                core/hw/gfxip/gfx9/gfx9PerfCtrInfo.cpp
                core/hw/gfxip/gfx9/gfx9PerfExperiment.cpp
                core/hw/gfxip/gfx9/gfx9PerfTrace.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkGs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkHs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineChunkVsPs.cpp
                core/hw/gfxip/gfx9/gfx9PipelineStatsQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9Pm4Optimizer.cpp
                core/hw/gfxip/gfx9/gfx9PrefetchMgr.cpp
                core/hw/gfxip/gfx9/gfx9QueueContexts.cpp
                core/hw/gfxip/gfx9/gfx9SettingsLoader.cpp
                core/hw/gfxip/gfx9/gfx9ShaderRing.cpp
                core/hw/gfxip/gfx9/gfx9ShaderRingSet.cpp
                core/hw/gfxip/gfx9/gfx9StreamoutStatsQueryPool.cpp
                core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.cpp
                core/hw/gfxip/gfx9/gfx9UniversalEngine.cpp
                core/hw/gfxip/gfx9/gfx9WorkaroundState.cpp
            )

            # Resource Processing Manager(RPM) GFXIP 9 files.
            target_sources(pal PRIVATE core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.cpp)

        endif()
    endif()

    if(PAL_BUILD_OSS)
        target_compile_definitions(pal PRIVATE PAL_BUILD_OSS)

        if(PAL_BUILD_OSS1)
            # Add the OSS1 files here, only if the client wants OSSIP 1 support.
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS1)
            target_sources(pal PRIVATE
                core/hw/ossip/oss1/oss1Device.cpp
                core/hw/ossip/oss1/oss1DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS2)
            # Add the OSS2 files here, only if the client wants OSSIP 2 support.
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS2)
            target_sources(pal PRIVATE
                core/hw/ossip/oss2/oss2Device.cpp
                core/hw/ossip/oss2/oss2DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS2_4)
            # Add the OSS2.4 files here, only if the client wants OSSIP 2.4 support.
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS2_4)
            target_sources(pal PRIVATE
                core/hw/ossip/oss2_4/oss2_4Device.cpp
                core/hw/ossip/oss2_4/oss2_4DmaCmdBuffer.cpp
            )
        endif()

        if(PAL_BUILD_OSS4)
            # Add the OSS4 files here, only if the client wants OSSIP 4 support.
            target_compile_definitions(pal PRIVATE PAL_BUILD_OSS4)
            target_sources(pal PRIVATE
                core/hw/ossip/oss4/oss4Device.cpp
                core/hw/ossip/oss4/oss4DmaCmdBuffer.cpp
            )
        endif()
    endif()

### PAL core/layers ############################################################
    if(PAL_BUILD_LAYERS)
        target_compile_definitions(pal PRIVATE PAL_BUILD_LAYERS)
        target_sources(pal PRIVATE core/layers/decorators.cpp)

        if(PAL_BUILD_DBG_OVERLAY)
            target_compile_definitions(pal PRIVATE PAL_BUILD_DBG_OVERLAY)

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
            target_compile_definitions(pal PRIVATE PAL_BUILD_GPU_PROFILER)

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

        if(PAL_BUILD_CMD_BUFFER_LOGGER)
            # CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
            #target_compile_definitions(pal PRIVATE
            #    PAL_BUILD_CMD_BUFFER_LOGGER=$<OR:?$<CONFIG:Debug$<BOOL:${PAL_BUILD_CMD_BUFFER_LOGGER}>>
            #)
            target_compile_definitions(pal PRIVATE PAL_BUILD_CMD_BUFFER_LOGGER)

            # Add the CmdBuffer Logger files here, only if the client wants CmdBuffer Logger support.
            target_sources(pal PRIVATE
                core/layers/cmdBufferLogger/cmdBufferLoggerCmdBuffer.cpp
                core/layers/cmdBufferLogger/cmdBufferLoggerDevice.cpp
                core/layers/cmdBufferLogger/cmdBufferLoggerImage.cpp
                core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.cpp
            )
        endif()

        if(PAL_BUILD_INTERFACE_LOGGER)
            # CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
            #target_compile_definitions(pal PRIVATE
            #    PAL_BUILD_INTERFACE_LOGGER=$<OR:?$<CONFIG:Debug$<BOOL:${PAL_BUILD_INTERFACE_LOGGER}>>
            #)
            target_compile_definitions(pal PRIVATE PAL_BUILD_INTERFACE_LOGGER)

            # Add the log layer files here, only if the client wants interface logging support.
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
                core/layers/interfaceLogger/interfaceLoggerSwapChain.cpp
            )

        endif()
    endif()
endif()

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
    util/dbgPrint.cpp
    util/file.cpp
    util/jsonWriter.cpp
    util/math.cpp
    util/assert.cpp
    util/md5.cpp
    util/memMapFile.cpp
    util/sysMemory.cpp
    util/sysUtil.cpp
)

if(UNIX)
### PAL util/lnx ###############################################################
    target_sources(pal PRIVATE
        util/lnx/lnxConditionVariable.cpp
        util/lnx/lnxEvent.cpp
        util/lnx/lnxFileMap.cpp
        util/lnx/lnxMutex.cpp
        util/lnx/lnxSemaphore.cpp
        util/lnx/lnxSysMemory.cpp
        util/lnx/lnxSysUtil.cpp
        util/lnx/lnxThread.cpp
        util/lnx/lnxTimeout.cpp
    )
endif()

### PAL Auto-generated Files ###########################################################################################
set(GEN_TOOLS_DIR ${PROJECT_SOURCE_DIR}/tools/generate)
set(GEN_SETTINGS  ${GEN_TOOLS_DIR}/genSettingsCode.py)

set(GEN_SETTINGS_FILES
    ${GEN_SETTINGS}
    ${GEN_TOOLS_DIR}/settingsCodeTemplates.py
    ${GEN_TOOLS_DIR}/platformSettingsCodeTemplates.py
    ${GEN_TOOLS_DIR}/configParser.py
)

set(GEN_HEAP_PERF ${GEN_TOOLS_DIR}/genHeapPerfMain.pl)
set(GEN_HEAP_PERF_FILES
    ${GEN_HEAP_PERF}
    ${GEN_TOOLS_DIR}/genHeapPerfCode.pm
    ${GEN_TOOLS_DIR}/heapPerfParser.pm
)

if(PAL_BUILD_CORE)
    set(SETTINGS_DIR ${PROJECT_SOURCE_DIR}/src/core)
    set(SETTINGS_CFG_DIR ${PROJECT_SOURCE_DIR}/src/core)

    add_custom_command(
        OUTPUT ${SETTINGS_DIR}/g_palPlatformSettings.cpp ${SETTINGS_DIR}/g_palPlatformSettings.h
        COMMAND ${PYTHON_CMD} ${GEN_SETTINGS} --settingsFile ${SETTINGS_CFG_DIR}/settings_platform.json --codeTemplateFile ${GEN_TOOLS_DIR}/platformSettingsCodeTemplates.py --outFilename g_palPlatformSettings --classNameOverride PlatformSettingsLoader --genRegistryCode
        DEPENDS ${GEN_SETTINGS_FILES} ${SETTINGS_CFG_DIR}/settings_platform.json
        COMMENT "Generating settings code from settings_platform.json"
    )

    add_custom_command(
        OUTPUT ${SETTINGS_DIR}/g_palSettings.cpp ${SETTINGS_DIR}/g_palSettings.h
        COMMAND ${PYTHON_CMD} ${GEN_SETTINGS} --settingsFile ${SETTINGS_CFG_DIR}/settings_core.json --codeTemplateFile ${GEN_TOOLS_DIR}/settingsCodeTemplates.py --outFilename g_palSettings --genRegistryCode
        DEPENDS ${GEN_SETTINGS_FILES} ${SETTINGS_CFG_DIR}/settings_core.json
        COMMENT "Generating settings code from settings_core.json"
    )

    add_custom_target(
        RunSettingsGenerator
        DEPENDS ${GEN_SETTINGS_FILES} ${SETTINGS_CFG_DIR}/settings_core.json ${SETTINGS_CFG_DIR}/settings_platform.json
        COMMENT "Checking if re-generation is required for settings"
    )

    set(HEAP_PERF_OUTPUT_DIR ${PROJECT_SOURCE_DIR}/src/core)
    set(HEAP_PERF_INPUT_DIR ${PROJECT_SOURCE_DIR}/src/core)

    file(GLOB HEAP_PERF_FILES ${HEAP_PERF_DIR}/hw/heapPerf/asic*.txt)

    add_custom_command(
        OUTPUT ${HEAP_PERF_OUTPUT_DIR}/g_heapPerf.cpp
        COMMAND ${PERL_CMD} ${GEN_HEAP_PERF} ${HEAP_PERF_INPUT_DIR}/hw/heapPerf g_heapPerf ${HEAP_PERF_OUTPUT_DIR}
        DEPENDS ${GEN_HEAP_PERF_FILES} ${HEAP_PERF_FILES}
        COMMENT "Generating heap perf code from core/hw/heapPerf/asic*.txt"
    )

    add_custom_target(
        RunHeapPerfGenerator
        DEPENDS ${GEN_HEAP_PERF_FILES} ${HEAP_PERF_FILES}
        COMMENT "Checking if re-generation is required for heapPerf"
    )

    if(PAL_BUILD_GFX)
        if(PAL_BUILD_GFX6)
            set(GFX6_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6)
            set(GFX6_CFG_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6)
            file(MAKE_DIRECTORY ${GFX6_DIR})

            add_custom_command(
                OUTPUT ${GFX6_DIR}/g_gfx6PalSettings.cpp ${GFX6_DIR}/g_gfx6PalSettings.h
                COMMAND ${PYTHON_CMD} ${GEN_SETTINGS} --settingsFile ${GFX6_CFG_DIR}/settings_gfx6.json --codeTemplateFile ${GEN_TOOLS_DIR}/settingsCodeTemplates.py --outFilename g_gfx6PalSettings --genRegistryCode
                DEPENDS ${GEN_SETTINGS_FILES} ${GFX6_CFG_DIR}/settings_gfx6.json
                COMMENT "Generating gfx6Settings code from settings_gfx6.json"
            )

            add_custom_target(
                RunGfx6SettingsGenerator
                DEPENDS ${GEN_SETTINGS_FILES} ${GFX6_CFG_DIR}/settings_gfx6.json
                COMMENT "Checking if re-generation is required for gfx6Settings"
            )
        endif()

        if(PAL_BUILD_GFX9)
            set(GFX9_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9)
            set(GFX9_CFG_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9)
            file(MAKE_DIRECTORY ${GFX9_DIR})

            add_custom_command(
                OUTPUT ${GFX9_DIR}/g_gfx9PalSettings.cpp ${GFX9_DIR}/g_gfx9PalSettings.h
                COMMAND ${PYTHON_CMD} ${GEN_SETTINGS} --settingsFile ${GFX9_CFG_DIR}/settings_gfx9.json --codeTemplateFile ${GEN_TOOLS_DIR}/settingsCodeTemplates.py --outFilename g_gfx9PalSettings --genRegistryCode
                DEPENDS ${GEN_SETTINGS_FILES} ${GFX9_CFG_DIR}/settings_gfx9.json
                COMMENT "Generating gfx9Settings code from settings_gfx9.json"
            )

            add_custom_target(
                RunGfx9SettingsGenerator
                DEPENDS ${GEN_SETTINGS_FILES} ${GFX9_CFG_DIR}/settings_gfx9.json
                COMMENT "Checking if re-generation is required for gfx9Settings"
            )
        endif()
    endif()
endif()

if(PAL_BUILD_CORE)
    add_dependencies(pal RunSettingsGenerator)
    add_dependencies(pal RunHeapPerfGenerator)

    if(PAL_BUILD_GFX)
        if(PAL_BUILD_GFX6)
            add_dependencies(pal RunGfx6SettingsGenerator)
        endif()

        if(PAL_BUILD_GFX9)
            add_dependencies(pal RunGfx9SettingsGenerator)
        endif()
    endif()
endif()

### Link Libraries #####################################################################################################
if(PAL_USE_PTHREAD)
    target_link_libraries(pal INTERFACE pthread)
endif()

### Find headers and build source groups ###############################################################################
target_find_headers(pal)
target_source_groups(pal)

### Other ##############################################################################################################
# CMAKE-TODO: The AMD Make builds have sub projects inherit compiler options / definitions.
# The desired behavior is not fully known at this time but this works to keep the same behavior.
if(AMD_MAKE_BUILD)
    # if(AMD_ADDITIONAL_C_OPTIONS)
    #     separate_arguments(AMD_ADDITIONAL_C_OPTIONS UNIX_COMMAND ${AMD_ADDITIONAL_C_OPTIONS})
    #     target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:C>:${AMD_ADDITIONAL_C_OPTIONS}>)
    # endif()

    if(AMD_ADDITIONAL_C_DEFINITIONS)
        separate_arguments(AMD_ADDITIONAL_C_DEFINITIONS UNIX_COMMAND ${AMD_ADDITIONAL_C_DEFINITIONS})
        target_compile_definitions(pal PRIVATE $<$<COMPILE_LANGUAGE:C>:${AMD_ADDITIONAL_C_DEFINITIONS}>)
    endif()

    # if(AMD_ADDITIONAL_CXX_OPTIONS)
    #     separate_arguments(AMD_ADDITIONAL_CXX_OPTIONS UNIX_COMMAND ${AMD_ADDITIONAL_CXX_OPTIONS})
    #     target_compile_options(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AMD_ADDITIONAL_CXX_OPTIONS}>)
    # endif()

    if(AMD_ADDITIONAL_CXX_DEFINITIONS)
        separate_arguments(AMD_ADDITIONAL_CXX_DEFINITIONS UNIX_COMMAND ${AMD_ADDITIONAL_CXX_DEFINITIONS})
        target_compile_definitions(pal PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${AMD_ADDITIONAL_CXX_DEFINITIONS}>)
    endif()
endif()
