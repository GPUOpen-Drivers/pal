/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/

#include "core/layers/shaderDbg/shaderDbgCmdBuffer.h"
#include "core/layers/shaderDbg/shaderDbgDevice.h"
#include "core/layers/shaderDbg/shaderDbgPipeline.h"
#include "core/layers/shaderDbg/shaderDbgPlatform.h"
#include "core/layers/shaderDbg/shaderDbgQueue.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include "palFile.h"
#include "palSysUtil.h"
#include "shaderDbgData.h"

using namespace Util;

namespace Pal
{
namespace ShaderDbg
{

// =====================================================================================================================
Queue::Queue(
    IQueue*    pNextQueue,
    Device*    pDevice)
    :
    QueueDecorator(pNextQueue, pDevice),
    m_pDevice(pDevice),
    m_submitCount(0),
    m_pFence(nullptr)
{
}

// =====================================================================================================================
Result Queue::Init()
{
    Result result = Result::ErrorOutOfMemory;

    m_pFence = static_cast<IFence*>(PAL_MALLOC(m_pDevice->GetFenceSize(&result),
                                               m_pDevice->GetPlatform(),
                                               AllocInternal));

    if (m_pFence != nullptr)
    {
        FenceCreateInfo createInfo = {};
        result = m_pDevice->CreateFence(createInfo, m_pFence, &m_pFence);
    }

    return result;
}

// =====================================================================================================================
void Queue::Destroy()
{
    m_pFence->Destroy();
    PAL_SAFE_FREE(m_pFence, m_pDevice->GetPlatform());

    IQueue* pNextLayer = m_pNextLayer;
    this->~Queue();
    pNextLayer->Destroy();
}

// =====================================================================================================================
Result Queue::Submit(
    const SubmitInfo& submitInfo)
{
    Result result = Result::Success;

    PlatformDecorator* pPlatform = m_pDevice->GetPlatform();

    AutoBuffer<ICmdBuffer*,  32, PlatformDecorator> nextCmdBuffers(submitInfo.cmdBufferCount,     pPlatform);
    AutoBuffer<CmdBufInfo,   32, PlatformDecorator> nextCmdBufInfoList(submitInfo.cmdBufferCount, pPlatform);
    AutoBuffer<GpuMemoryRef, 32, PlatformDecorator> nextGpuMemoryRefs(submitInfo.gpuMemRefCount,  pPlatform);
    AutoBuffer<DoppRef,      32, PlatformDecorator> nextDoppRefs(submitInfo.doppRefCount,         pPlatform);

    const bool hasCmdBufInfo = (submitInfo.pCmdBufInfoList != nullptr);

    if ((nextCmdBuffers.Capacity()     < submitInfo.cmdBufferCount) ||
        (nextCmdBufInfoList.Capacity() < submitInfo.cmdBufferCount) ||
        (nextDoppRefs.Capacity()       < submitInfo.doppRefCount)   ||
        (nextGpuMemoryRefs.Capacity()  < submitInfo.gpuMemRefCount))
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < submitInfo.gpuMemRefCount; i++)
        {
            nextGpuMemoryRefs[i].pGpuMemory   = NextGpuMemory(submitInfo.pGpuMemoryRefs[i].pGpuMemory);
            nextGpuMemoryRefs[i].flags.u32All = submitInfo.pGpuMemoryRefs[i].flags.u32All;
        }

        for (uint32 i = 0; i < submitInfo.doppRefCount; i++)
        {
            nextDoppRefs[i].pGpuMemory   = NextGpuMemory(submitInfo.pDoppRefs[i].pGpuMemory);
            nextDoppRefs[i].flags.u32All = submitInfo.pDoppRefs[i].flags.u32All;
        }

        const IGpuMemory* pNextBlockIfFlipping[MaxBlockIfFlippingCount] = {};
        PAL_ASSERT(submitInfo.blockIfFlippingCount <= MaxBlockIfFlippingCount);

        for (uint32 i = 0; i < submitInfo.blockIfFlippingCount; i++)
        {
            pNextBlockIfFlipping[i] = NextGpuMemory(submitInfo.ppBlockIfFlipping[i]);
        }

        size_t numShaderDbgInstances = 0;

        for (uint32 i = 0; i < submitInfo.cmdBufferCount; i++)
        {
            numShaderDbgInstances += static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[i])->GetTraceData().NumElements();
            nextCmdBuffers[i]      = NextCmdBuffer(submitInfo.ppCmdBuffers[i]);

            if (hasCmdBufInfo)
            {
                // We need to copy the caller's CmdBufInfo.
                const CmdBufInfo& cmdBufInfo = submitInfo.pCmdBufInfoList[i];

                nextCmdBufInfoList[i].u32All = cmdBufInfo.u32All;

                if (cmdBufInfo.isValid)
                {
                    nextCmdBufInfoList[i].pPrimaryMemory = NextGpuMemory(cmdBufInfo.pPrimaryMemory);
                }
            }
        }

        SubmitInfo nextSubmitInfo        = submitInfo;
        nextSubmitInfo.pGpuMemoryRefs    = &nextGpuMemoryRefs[0];
        nextSubmitInfo.pDoppRefs         = &nextDoppRefs[0];
        nextSubmitInfo.ppBlockIfFlipping = &pNextBlockIfFlipping[0];

        // If our list of command buffers contains any cases where we've instrumented a shader, we need to
        // split apart the list. Only allow a single command buffer to submit at a time, wait for it to complete,
        // and then dump the data from it to disk.
        if (numShaderDbgInstances > 0)
        {
            nextSubmitInfo.cmdBufferCount    = 1;
            nextSubmitInfo.pFence            = nullptr;

            for (uint32 i = 0; ((i < submitInfo.cmdBufferCount) && (result == Result::Success)); i++)
            {
                const CmdBuffer* pCmdBuffer    = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[i]);
                const bool       hasShaderDbg  = (pCmdBuffer->GetTraceData().NumElements() > 0);
                nextSubmitInfo.ppCmdBuffers    = &nextCmdBuffers[i];
                nextSubmitInfo.pCmdBufInfoList = (hasCmdBufInfo) ? &nextCmdBufInfoList[i] : nullptr;

                if (hasShaderDbg)
                {
                    if (((i + 1) == submitInfo.cmdBufferCount) && (submitInfo.pFence != nullptr))
                    {
                        nextSubmitInfo.pFence = NextFence(submitInfo.pFence);
                    }
                    else
                    {
                        result = m_pDevice->ResetFences(1, &m_pFence);
                        nextSubmitInfo.pFence = NextFence(m_pFence);
                    }
                }
                else if ((i + 1) == submitInfo.cmdBufferCount)
                {
                    nextSubmitInfo.pFence = NextFence(submitInfo.pFence);
                }
                else
                {
                    nextSubmitInfo.pFence = nullptr;
                }

                if (result == Result::Success)
                {
                    result = m_pNextLayer->Submit(nextSubmitInfo);
                }

                if ((result == Result::Success) && (nextSubmitInfo.pFence != nullptr))
                {
                    // Wait for a maximum of 5 seconds.
                    constexpr uint64 Timeout = 5000000000ull;
                    result = m_pDevice->GetNextLayer()->WaitForFences(1, &nextSubmitInfo.pFence, true, Timeout);
                }

                if ((result == Result::Success) && hasShaderDbg)
                {
                    result = DumpShaderDbgData(static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[i]), m_submitCount);
                }
            }
        }
        else
        {
            nextSubmitInfo.ppCmdBuffers    = &nextCmdBuffers[0];
            nextSubmitInfo.pCmdBufInfoList = (hasCmdBufInfo) ? &nextCmdBufInfoList[0] : nullptr;
            nextSubmitInfo.pFence          = NextFence(nextSubmitInfo.pFence);
            result = m_pNextLayer->Submit(nextSubmitInfo);
        }
    }

    m_submitCount++;
    return result;
}

// =====================================================================================================================
Result Queue::DumpShaderDbgData(
    CmdBuffer* pCmdBuffer,
    uint32     submitId
    ) const
{
    Result result = Result::Success;
    auto   it     = pCmdBuffer->GetTraceData().Begin();

    while ((it.Get() != nullptr) && (result == Result::Success))
    {
        TraceData* pTraceData = it.Get();

        const auto& pipelineInfo = pTraceData->pPipeline->GetInfo();

        File dumpFile;

        ShaderDumpInfo dumpInfo = {};
        dumpInfo.pCmdBuffer     = pCmdBuffer;
        dumpInfo.hwStage        = pTraceData->hwStage;
        dumpInfo.pipelineHash   = pipelineInfo.pipelineHash;
        dumpInfo.compilerHash   = pipelineInfo.compilerHash;
        dumpInfo.isDraw         = pTraceData->isDraw;
        dumpInfo.uniqueId       = pTraceData->uniqueId;
        dumpInfo.submitId       = submitId;
        dumpInfo.pFile          = &dumpFile;

        const bool fileOpened = pTraceData->pPipeline->OpenUniqueDumpFile(dumpInfo);

        if (fileOpened)
        {
            const size_t traceSize = static_cast<size_t>(pTraceData->pTraceMemory->Desc().size);

            void* pData = nullptr;
            result      = pTraceData->pTraceMemory->Map(&pData);

            if (result == Result::Success)
            {
                result = dumpFile.Write(pData, traceSize);
            }

            if (result == Result::Success)
            {
                result = dumpFile.Flush();
            }

            if (result == Result::Success)
            {
                // Command buffers can be submitted multiple times.
                // As a result, we need to reset the memory for future submits.
                const size_t sizeToMemset = traceSize - offsetof(Sdl_DumpHeader, bytesWritten);
                memset(VoidPtrInc(pData, offsetof(Sdl_DumpHeader, bytesWritten)), 0, sizeToMemset);

                result = pTraceData->pTraceMemory->Unmap();
            }
        }

        it.Next();
    }

    return result;
}

} // ShaderDbg
} // Pal
