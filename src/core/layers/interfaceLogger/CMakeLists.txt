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

target_sources(pal PRIVATE
    CMakeLists.txt
    interfaceLoggerBorderColorPalette.cpp
    interfaceLoggerBorderColorPalette.h
    interfaceLoggerCmdAllocator.cpp
    interfaceLoggerCmdAllocator.h
    interfaceLoggerCmdBuffer.cpp
    interfaceLoggerCmdBuffer.h
    interfaceLoggerColorBlendState.cpp
    interfaceLoggerColorBlendState.h
    interfaceLoggerColorTargetView.h
    interfaceLoggerDepthStencilState.cpp
    interfaceLoggerDepthStencilState.h
    interfaceLoggerDepthStencilView.h
    interfaceLoggerDevice.cpp
    interfaceLoggerDevice.h
    interfaceLoggerFence.cpp
    interfaceLoggerFence.h
    interfaceLoggerGpuEvent.cpp
    interfaceLoggerGpuEvent.h
    interfaceLoggerGpuMemory.cpp
    interfaceLoggerGpuMemory.h
    interfaceLoggerImage.cpp
    interfaceLoggerImage.h
    interfaceLoggerIndirectCmdGenerator.cpp
    interfaceLoggerIndirectCmdGenerator.h
    interfaceLoggerLogContext.cpp
    interfaceLoggerLogContext.h
    interfaceLoggerLogContextEnum.cpp
    interfaceLoggerLogContextStruct.cpp
    interfaceLoggerMsaaState.cpp
    interfaceLoggerMsaaState.h
    interfaceLoggerPipeline.cpp
    interfaceLoggerPipeline.h
    interfaceLoggerPlatform.cpp
    interfaceLoggerPlatform.h
    interfaceLoggerPrivateScreen.cpp
    interfaceLoggerPrivateScreen.h
    interfaceLoggerQueryPool.cpp
    interfaceLoggerQueryPool.h
    interfaceLoggerQueue.cpp
    interfaceLoggerQueue.h
    interfaceLoggerQueueSemaphore.cpp
    interfaceLoggerQueueSemaphore.h
    interfaceLoggerScreen.cpp
    interfaceLoggerScreen.h
    interfaceLoggerShaderLibrary.cpp
    interfaceLoggerShaderLibrary.h
    interfaceLoggerSwapChain.cpp
    interfaceLoggerSwapChain.h
)
