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

add_subdirectory(chip)

target_sources(pal PRIVATE
    CMakeLists.txt
    g_gfx9MergedDataFormats.h
    g_gfx9ShadowedRegistersInit.h
    gfx9AbiToPipelineRegisters.h
    gfx9AcquireReleaseBarrier.cpp
    gfx9Barrier.cpp
    gfx9BorderColorPalette.cpp
    gfx9BorderColorPalette.h
    gfx9Chip.h
    gfx9CmdStream.cpp
    gfx9CmdStream.h
    gfx9CmdUploadRing.cpp
    gfx9CmdUploadRing.h
    gfx9CmdUtil.cpp
    gfx9CmdUtil.h
    gfx9ColorBlendState.cpp
    gfx9ColorBlendState.h
    gfx9ColorTargetView.cpp
    gfx9ColorTargetView.h
    gfx9ComputeCmdBuffer.cpp
    gfx9ComputeCmdBuffer.h
    gfx9ComputeEngine.cpp
    gfx9ComputeEngine.h
    gfx9ComputePipeline.cpp
    gfx9ComputePipeline.h
    gfx9DepthStencilState.cpp
    gfx9DepthStencilState.h
    gfx9DepthStencilView.cpp
    gfx9DepthStencilView.h
    gfx9Device.cpp
    gfx9Device.h
    gfx9FormatInfo.cpp
    gfx9FormatInfo.h
    gfx9GraphicsPipeline.cpp
    gfx9GraphicsPipeline.h
    gfx9HybridGraphicsPipeline.cpp
    gfx9HybridGraphicsPipeline.h
    gfx9Image.cpp
    gfx9Image.h
    gfx9IndirectCmdGenerator.cpp
    gfx9IndirectCmdGenerator.h
    gfx9MaskRam.cpp
    gfx9MaskRam.h
    gfx9MetaEq.cpp
    gfx9MetaEq.h
    gfx9MsaaState.cpp
    gfx9MsaaState.h
    gfx9OcclusionQueryPool.cpp
    gfx9OcclusionQueryPool.h
    gfx9PerfCtrInfo.cpp
    gfx9PerfCtrInfo.h
    gfx9PerfExperiment.cpp
    gfx9PerfExperiment.h
    gfx9PipelineChunkCs.cpp
    gfx9PipelineChunkCs.h
    gfx9PipelineChunkGs.cpp
    gfx9PipelineChunkGs.h
    gfx9PipelineChunkHs.cpp
    gfx9PipelineChunkHs.h
    gfx9PipelineChunkVsPs.cpp
    gfx9PipelineChunkVsPs.h
    gfx9PipelineStatsQueryPool.cpp
    gfx9PipelineStatsQueryPool.h
    gfx9Pm4Optimizer.cpp
    gfx9Pm4Optimizer.h
    gfx9QueueContexts.cpp
    gfx9QueueContexts.h
    gfx9SettingsLoader.cpp
    gfx9SettingsLoader.h
    gfx9ShaderLibrary.cpp
    gfx9ShaderLibrary.h
    gfx9ShaderRing.cpp
    gfx9ShaderRing.h
    gfx9ShaderRingSet.cpp
    gfx9ShaderRingSet.h
    gfx9ShadowedRegisters.h
    gfx9StreamoutStatsQueryPool.cpp
    gfx9StreamoutStatsQueryPool.h
    gfx9UniversalCmdBuffer.cpp
    gfx9UniversalCmdBuffer.h
    gfx9UniversalEngine.cpp
    gfx9UniversalEngine.h
    gfx9WorkaroundState.cpp
    gfx9WorkaroundState.h
    gfx10DmaCmdBuffer.cpp
    gfx10DmaCmdBuffer.h
    gfx10ShadowedRegisters.h
    gfx10ShadowedRegisters_gfx103.h
    gfx10ShadowedRegisters_nv10.h
    settings_gfx9.json
)
