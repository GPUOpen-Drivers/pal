##
 #######################################################################################################################
 #
 #  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
    gfx12Barrier.cpp
    gfx12Barrier.h
    gfx12BorderColorPalette.cpp
    gfx12BorderColorPalette.h
    gfx12Chip.h
    gfx12CmdStream.cpp
    gfx12CmdStream.h
    gfx12CmdUploadRing.cpp
    gfx12CmdUploadRing.h
    gfx12CmdUtil.cpp
    gfx12CmdUtil.h
    gfx12ColorBlendState.cpp
    gfx12ColorBlendState.h
    gfx12ColorTargetView.cpp
    gfx12ColorTargetView.h
    gfx12ComputeCmdBuffer.cpp
    gfx12ComputeCmdBuffer.h
    gfx12ComputePipeline.cpp
    gfx12ComputePipeline.h
    gfx12ComputeShaderLibrary.cpp
    gfx12ComputeShaderLibrary.h
    gfx12DepthStencilState.cpp
    gfx12DepthStencilState.h
    gfx12DepthStencilView.cpp
    gfx12DepthStencilView.h
    gfx12Device.cpp
    gfx12Device.h
    gfx12ExecuteIndirectCmdUtil.cpp
    gfx12ExecuteIndirectCmdUtil.h
    gfx12FormatInfo.cpp
    gfx12FormatInfo.h
    gfx12GraphicsPipeline.cpp
    gfx12GraphicsPipeline.h
    gfx12GraphicsShaderLibrary.cpp
    gfx12GraphicsShaderLibrary.h
    gfx12HybridGraphicsPipeline.cpp
    gfx12HybridGraphicsPipeline.h
    gfx12Image.cpp
    gfx12Image.h
    gfx12IndirectCmdGenerator.cpp
    gfx12IndirectCmdGenerator.h
    gfx12Metadata.cpp
    gfx12Metadata.h
    gfx12MsaaState.cpp
    gfx12MsaaState.h
    gfx12OcclusionQueryPool.cpp
    gfx12OcclusionQueryPool.h
    gfx12PerfCtrInfo.cpp
    gfx12PerfCtrInfo.h
    gfx12PerfExperiment.cpp
    gfx12PerfExperiment.h
    gfx12PipelineChunkCs.cpp
    gfx12PipelineChunkCs.h
    gfx12PipelineStatsQueryPool.cpp
    gfx12PipelineStatsQueryPool.h
    gfx12QueueContexts.cpp
    gfx12QueueContexts.h
    gfx12QueueRingBuffer.cpp
    gfx12QueueRingBuffer.h
    gfx12RegPairHandler.h
    gfx12SettingsLoader.cpp
    gfx12SettingsLoader.h
    gfx12ShaderRing.cpp
    gfx12ShaderRing.h
    gfx12ShaderRingSet.cpp
    gfx12ShaderRingSet.h
    gfx12StreamoutStatsQueryPool.cpp
    gfx12StreamoutStatsQueryPool.h
    gfx12UniversalCmdBuffer.cpp
    gfx12UniversalCmdBuffer.h
    gfx12UserDataLayout.cpp
    gfx12UserDataLayout.h
    gfx12Srd.cpp
    settings_gfx12.json
)
