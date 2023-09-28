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

if(PAL_BUILD_GFX6)
    add_subdirectory(gfx6)
endif()

if(PAL_BUILD_GFX9)
    add_subdirectory(gfx9)
endif()

add_subdirectory(rpm)
add_subdirectory(sdma)

target_sources(pal PRIVATE
    borderColorPalette.cpp
    borderColorPalette.h
    CMakeLists.txt
    cmdUploadRing.cpp
    cmdUploadRing.h
    colorBlendState.h
    computePipeline.cpp
    computePipeline.h
    computeShaderLibrary.cpp
    computeShaderLibrary.h
    depthStencilState.h
    gfxBarrier.cpp
    gfxBarrier.h
    gfxBlendOptimizer.cpp
    gfxBlendOptimizer.h
    gfxCmdBuffer.cpp
    gfxCmdBuffer.h
    gfxCmdStream.cpp
    gfxCmdStream.h
    gfxDevice.cpp
    gfxDevice.h
    gfxImage.cpp
    gfxImage.h
    graphicsPipeline.cpp
    graphicsPipeline.h
    graphicsShaderLibrary.cpp
    graphicsShaderLibrary.h
    msaaState.h
    pipeline.cpp
    pipeline.h
    pm4CmdBuffer.cpp
    pm4CmdBuffer.h
    pm4CmdStream.cpp
    pm4CmdStream.h
    pm4ComputeCmdBuffer.cpp
    pm4ComputeCmdBuffer.h
    pm4Image.cpp
    pm4Image.h
    pm4IndirectCmdGenerator.cpp
    pm4IndirectCmdGenerator.h
    pm4MaskRam.h
    pm4UniversalCmdBuffer.cpp
    pm4UniversalCmdBuffer.h
    queryPool.cpp
    queryPool.h
    shaderLibrary.cpp
    shaderLibrary.h
)

# PAL core code that the internal compiler depends on goes in target palCompilerDeps
target_sources(palCompilerDeps PRIVATE
    hsaAbiMetadata.cpp
    pipelineAbiReader.cpp
)
