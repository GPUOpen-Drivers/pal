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

target_include_directories(pal PUBLIC .)
target_include_directories(palCompilerDeps PUBLIC .)

target_sources(pal PRIVATE
    CMakeLists.txt
    g_palPipelineAbiMetadata.h
    g_palPipelineAbiMetadataImpl.h
    pal.h
    palBorderColorPalette.h
    palCacheLayer.h
    palCmdAllocator.h
    palCmdBuffer.h
    palColorBlendState.h
    palColorTargetView.h
    palDbgLoggerDevDriver.h
    palDepthStencilState.h
    palDepthStencilView.h
    palDestroyable.h
    palDeveloperHooks.h
    palDevice.h
    palEventDefs.h
    palFence.h
    palFormat.h
    palFormatInfo.h
    palGpuEvent.h
    palGpuMemory.h
    palGpuMemoryBindable.h
    palHasher.h
    palHsaAbiMetadata.h
    palImage.h
    palIndirectCmdGenerator.h
    palLib.h
    palMotionEstimator.h
    palMsaaState.h
    palOglPresent.h
    palPerfExperiment.h
    palPipeline.h
    palPipelineAbi.h
    palPipelineAbiProcessor.h
    palPipelineAbiProcessorImpl.h
    palPipelineAbiReader.h
    palPipelineAbiUtils.h
    palPipelineArFile.h
    palPlatform.h
    palPrivateScreen.h
    palQueryPool.h
    palQueue.h
    palQueueSemaphore.h
    palScreen.h
    palSettingsLoader.h
    palShaderLibrary.h
    palSwapChain.h
)
