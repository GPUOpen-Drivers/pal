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

#include "core/g_palPlatformSettings.h"
#include "core/layers/shaderDbg/shaderDbgCmdBuffer.h"
#include "core/layers/shaderDbg/shaderDbgDevice.h"
#include "core/layers/shaderDbg/shaderDbgPipeline.h"
#include "core/layers/shaderDbg/shaderDbgPlatform.h"
#include "palAutoBuffer.h"
#include "palDequeImpl.h"
#include <cinttypes>
#include "shaderDbg.h"
#include "shaderDbgData.h"

using namespace Util;

namespace Pal
{
namespace ShaderDbg
{

constexpr Sdl_HwShaderStage AbiToSdlHwStage[] =
{
    Sdl_HwShaderStage_Count,    // Abi::HardwareStage::Ls
    Sdl_Hs,                     // Abi::HardwareStage::Hs
    Sdl_HwShaderStage_Count,    // Abi::HardwareStage::Es
    Sdl_Gs,                     // Abi::HardwareStage::Gs
    Sdl_Vs,                     // Abi::HardwareStage::Vs
    Sdl_Ps,                     // Abi::HardwareStage::Ps
    Sdl_Cs,                     // Abi::HardwareStage::Cs
};

static_assert(ArrayLen(AbiToSdlHwStage) == static_cast<uint32>(Abi::HardwareStage::Count),
              "HardwareStageStrings is not the same size as HardwareStage enum!");

// =====================================================================================================================
static Sdl_GfxIpLevel PalToSdlGfxIpLevel(
    GfxIpLevel gfxLevel)
{
    Sdl_GfxIpLevel sdlGfxLevel = Sdl_None;

    switch (gfxLevel)
    {
#if PAL_BUILD_GFX9
    case GfxIpLevel::GfxIp9:
        sdlGfxLevel = Sdl_GfxIp9;
        break;
#endif
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    PAL_ASSERT(sdlGfxLevel != Sdl_None);
    return sdlGfxLevel;
}

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, static_cast<DeviceDecorator*>(pDevice->GetNextLayer())),
    m_pDevice(pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_maxNumTracedDraws(m_pDevice->GetPlatform()->PlatformSettings().shaderDbgConfig.shaderDbgNumDrawsPerCmdBuffer),
    m_pCurrentPipeline(nullptr),
    m_currentDraw(0),
    m_currentDispatch(0),
    m_numTracedDraws(0),
    m_traceData(static_cast<Platform*>(pDevice->GetPlatform()))
{
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]  = &CmdBuffer::CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] = &CmdBuffer::CmdSetUserDataGfx;

    m_funcTable.pfnCmdDraw                     = CmdDraw;
    m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque;
    m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed;
    m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti;
    m_funcTable.pfnCmdDispatch                 = CmdDispatch;
    m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirect;
    m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffset;
}

// =====================================================================================================================
CmdBuffer::~CmdBuffer()
{
    ResetState();
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    Result result = ResetState();

    if (result == Result::Success)
    {
        result = GetNextLayer()->Begin(NextCmdBufferBuildInfo(info));
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    return GetNextLayer()->End();
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    Result result = ResetState();

    if (result == Result::Success)
    {
        result = GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::ResetState()
{
    Result result = Result::Success;

    while ((m_traceData.NumElements() > 0) && (result == Result::Success))
    {
        TraceData traceData = {};
        result = m_traceData.PopBack(&traceData);
        PAL_ASSERT(result == Result::Success);

        result = m_pDevice->ReleaseMemoryChunk(traceData.pTraceMemory);
    }

    m_currentDraw     = 0;
    m_currentDispatch = 0;
    m_numTracedDraws  = 0;

    return result;
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    m_pCurrentPipeline = static_cast<const Pipeline*>(params.pPipeline);
    GetNextLayer()->CmdBindPipeline(NextPipelineBindParams(params));
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);
    pCmdBuf->GetNextLayer()->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);
    pCmdBuf->GetNextLayer()->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDraw - ID #%d", pThis->m_currentDraw);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(true, pThis->m_currentDraw++);
    pThis->GetNextLayer()->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize streamOutFilledSizeVa,
    uint32  streamOutOffset,
    uint32  stride,
    uint32  firstInstance,
    uint32  instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDrawOpaque - ID #%d", pThis->m_currentDraw);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(true, pThis->m_currentDraw++);
    pThis->GetNextLayer()->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDrawIndexed - ID #%d", pThis->m_currentDraw);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(true, pThis->m_currentDraw++);
    pThis->GetNextLayer()->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDrawIndirectMulti - ID #%d", pThis->m_currentDraw);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(true, pThis->m_currentDraw++);
    pThis->GetNextLayer()->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDrawIndexedIndirectMulti - ID #%d", pThis->m_currentDraw);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(true, pThis->m_currentDraw++);
    pThis->GetNextLayer()->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                       offset,
                                                       stride,
                                                       maximumCount,
                                                       countGpuAddr);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDispatch - ID #%d", pThis->m_currentDispatch);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(false, pThis->m_currentDispatch++);
    pThis->GetNextLayer()->CmdDispatch(x, y, z);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDispatchIndirect - ID #%d", pThis->m_currentDispatch);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(false, pThis->m_currentDispatch++);
    pThis->GetNextLayer()->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);
#if PAL_ENABLE_PRINTS_ASSERTS
    char stringBuf[256] = {};
    Snprintf(&stringBuf[0], 256, "CmdDispatchOffset - ID #%d", pThis->m_currentDispatch);
    pThis->CmdCommentString(&stringBuf[0]);
#endif

    pThis->AllocateHwShaderDbg(false, pThis->m_currentDispatch++);
    pThis->GetNextLayer()->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);
    pThis->PostDrawDispatch();
}

// =====================================================================================================================
void CmdBuffer::AllocateHwShaderDbg(
    bool   isDraw,
    uint32 uniqueId)
{
    gpusize traceAddrs[static_cast<uint32>(Abi::HardwareStage::Count)] = {};

    if ((m_maxNumTracedDraws == 0) || (m_numTracedDraws < m_maxNumTracedDraws))
    {
        PAL_ASSERT(m_pCurrentPipeline != nullptr);
        uint64 compilerHash = m_pCurrentPipeline->GetInfo().compilerHash;
        uint32 hwShaderDbgMask = m_pCurrentPipeline->HwShaderDbgMask();

        const uint32 numShaders = Util::CountSetBits(hwShaderDbgMask);
        AutoBuffer<IGpuMemory*,
                   static_cast<uint32>(Abi::HardwareStage::Count),
                   Platform> allocations(numShaders, m_pPlatform);

        Result result = (allocations.Capacity() >= static_cast<uint32>(Abi::HardwareStage::Count)) ?
                        Result::Success :
                        Result::ErrorOutOfMemory;

        for (uint32 i = 0; ((i < numShaders) && (result == Result::Success)); i++)
        {
            result = m_pDevice->GetMemoryChunk(&allocations[i]);
        }

        if (result == Result::Success)
        {
            const Sdl_GfxIpLevel gfxIpLevel = PalToSdlGfxIpLevel(m_pDevice->DeviceProps().gfxLevel);

            uint32 currentIdx = 0;
            uint32 lowSetBit  = 0;
            while ((BitMaskScanForward(&lowSetBit, hwShaderDbgMask)) && (result == Result::Success))
            {
                const Sdl_HwShaderStage hwStage = AbiToSdlHwStage[lowSetBit];
                PAL_ASSERT(hwStage != Sdl_HwShaderStage_Count);

                IGpuMemory* pGpuMemory = allocations[currentIdx];
                void*       pData      = nullptr;

                result = pGpuMemory->Map(&pData);

                if (result == Result::Success)
                {
                    memset(pData, 0, static_cast<size_t>(pGpuMemory->Desc().size));

                    Sdl_DumpHeader header   = {};
                    header.dumpType         = Sdl_DumpTypeHeader;
                    header.majorVersion     = SHADERDBG_MAJOR_VERSION;
                    header.minorVersion     = SHADERDBG_MINOR_VERSION;
                    header.uniqueId         = uniqueId;
                    header.gfxIpLevel       = gfxIpLevel;
                    header.pipelineHash     = compilerHash;
                    header.hwShaderStage    = hwStage;
                    header.bufferSize       = static_cast<uint32>(pGpuMemory->Desc().size);

                    memcpy(pData, &header, sizeof(Sdl_DumpHeader));

                    result = pGpuMemory->Unmap();
                }

                if (result == Result::Success)
                {
                    hwShaderDbgMask &= ~(1 << lowSetBit);
                    traceAddrs[lowSetBit] = allocations[currentIdx]->Desc().gpuVirtAddr;

                    TraceData traceData = {};
                    traceData.pPipeline    = m_pCurrentPipeline;
                    traceData.pTraceMemory = allocations[currentIdx];
                    traceData.hwStage      = hwStage;
                    traceData.isDraw       = isDraw;
                    traceData.uniqueId     = uniqueId;
                    result = m_traceData.PushBack(traceData);

                    currentIdx++;
                }
            }

            m_numTracedDraws++;
        }
    }
    m_pNextLayer->CmdSetShaderDbgData(traceAddrs);
}

// =====================================================================================================================
void CmdBuffer::PostDrawDispatch()
{
    if (m_pCurrentPipeline->HwShaderDbgMask() != 0)
    {
        // If there is an instrumented shader, we need to do a barrier to ensure that the memory written by the shader
        // is flushed out to be accesible by the CPU.
        BarrierTransition transition = {};
        transition.srcCacheMask = CoherShader | CoherMemory;
        transition.dstCacheMask = CoherCpu;

        const HwPipePoint waitPoint = HwPipePoint::HwPipeBottom;

        BarrierInfo barrier         = {};
        barrier.waitPoint           = HwPipePoint::HwPipeTop;
        barrier.pipePointWaitCount  = 1;
        barrier.pPipePoints         = &waitPoint;
        barrier.transitionCount     = 1;
        barrier.pTransitions        = &transition;

        m_pNextLayer->CmdBarrier(barrier);
    }
}

} // ShaderDbg
} // Pal
