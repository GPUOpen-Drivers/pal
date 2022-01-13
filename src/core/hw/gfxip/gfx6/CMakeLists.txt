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
    g_gfx6MergedDataFormats.h
    g_gfx6PalSettings.cpp
    g_gfx6PalSettings.h
    g_gfx6ShadowedRegistersInit.h
    gfx6Barrier.cpp
    gfx6BorderColorPalette.cpp
    gfx6BorderColorPalette.h
    gfx6Chip.h
    gfx6CmdStream.cpp
    gfx6CmdStream.h
    gfx6CmdUploadRing.cpp
    gfx6CmdUploadRing.h
    gfx6CmdUtil.cpp
    gfx6CmdUtil.h
    gfx6ColorBlendState.cpp
    gfx6ColorBlendState.h
    gfx6ColorTargetView.cpp
    gfx6ColorTargetView.h
    gfx6ComputeCmdBuffer.cpp
    gfx6ComputeCmdBuffer.h
    gfx6ComputeEngine.cpp
    gfx6ComputeEngine.h
    gfx6ComputePipeline.cpp
    gfx6ComputePipeline.h
    gfx6DepthStencilState.cpp
    gfx6DepthStencilState.h
    gfx6DepthStencilView.cpp
    gfx6DepthStencilView.h
    gfx6Device.cpp
    gfx6Device.h
    gfx6FormatInfo.cpp
    gfx6FormatInfo.h
    gfx6GraphicsPipeline.cpp
    gfx6GraphicsPipeline.h
    gfx6Image.cpp
    gfx6Image.h
    gfx6IndirectCmdGenerator.cpp
    gfx6IndirectCmdGenerator.h
    gfx6MaskRam.cpp
    gfx6MaskRam.h
    gfx6MsaaState.cpp
    gfx6MsaaState.h
    gfx6OcclusionQueryPool.cpp
    gfx6OcclusionQueryPool.h
    gfx6PerfCtrInfo.cpp
    gfx6PerfCtrInfo.h
    gfx6PerfExperiment.cpp
    gfx6PerfExperiment.h
    gfx6PipelineChunkEsGs.cpp
    gfx6PipelineChunkEsGs.h
    gfx6PipelineChunkLsHs.cpp
    gfx6PipelineChunkLsHs.h
    gfx6PipelineChunkVsPs.cpp
    gfx6PipelineChunkVsPs.h
    gfx6PipelineStatsQueryPool.cpp
    gfx6PipelineStatsQueryPool.h
    gfx6Pm4Optimizer.cpp
    gfx6Pm4Optimizer.h
    gfx6QueueContexts.cpp
    gfx6QueueContexts.h
    gfx6SettingsLoader.cpp
    gfx6SettingsLoader.h
    gfx6ShaderRing.cpp
    gfx6ShaderRing.h
    gfx6ShaderRingSet.cpp
    gfx6ShaderRingSet.h
    gfx6ShadowedRegisters.h
    gfx6StreamoutStatsQueryPool.cpp
    gfx6StreamoutStatsQueryPool.h
    gfx6UniversalCmdBuffer.cpp
    gfx6UniversalCmdBuffer.h
    gfx6UniversalEngine.cpp
    gfx6UniversalEngine.h
    gfx6WorkaroundState.cpp
    gfx6WorkaroundState.h
    settings_gfx6.json
)
