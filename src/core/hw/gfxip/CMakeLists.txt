##
 #######################################################################################################################
 #
 #  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

target_sources(pal PRIVATE
    borderColorPalette.cpp
    borderColorPalette.h
    CMakeLists.txt
    cmdUploadRing.cpp
    cmdUploadRing.h
    colorBlendState.h
    computeCmdBuffer.cpp
    computeCmdBuffer.h
    computePipeline.cpp
    computePipeline.h
    depthStencilState.h
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
    indirectCmdGenerator.cpp
    indirectCmdGenerator.h
    maskRam.h
    msaaState.h
    pipeline.cpp
    pipeline.h
    queryPool.cpp
    queryPool.h
    shaderLibrary.cpp
    shaderLibrary.h
    universalCmdBuffer.cpp
    universalCmdBuffer.h
)
