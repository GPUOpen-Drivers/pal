/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/crashAnalysis/crashAnalysisCmdBuffer.h"
#include "core/layers/crashAnalysis/crashAnalysisDevice.h"
#include "core/layers/crashAnalysis/crashAnalysisPlatform.h"

#include "palSysMemory.h"
#include "palVectorImpl.h"
#include "palMutex.h"

namespace Pal
{
namespace CrashAnalysis
{

// =====================================================================================================================
// Generates a Crash Analysis marker from an origination source and a marker ID.
constexpr uint32 GenerateMarker(const MarkerSource source, const uint32 value)
{
    PAL_ASSERT_MSG((0x0fffffff | value) == 0x0fffffff,
                   "Malformed value (0x%X): unexpected top bits",
                    value);
    return ((static_cast<uint32>(source) << 28) | (value & 0x0fffffff));
}

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pDevice),
    m_pDevice(pDevice),
    m_pPlatform(static_cast<Platform*>(m_pDevice->GetPlatform())),
    m_cmdBufferId(m_pPlatform->GenerateResourceId()),
    m_markerCounter(0),
    m_pMemoryChunk(nullptr),
    m_pEventCache(nullptr),
    m_markerStack(m_pPlatform)
{
    // Create the 'marker stack' for each of the 16 possible marker sources.
    // Note: this does not allocate. The default capacity of 'm_markerStack'
    //  is exactly 'MarkerStackCount': we are simply initializing the vector
    //  elements here.
    for (int i = 0; i < MarkerStackCount; i++)
    {
        m_markerStack.PushBack(MarkerStack(m_pPlatform));
    }
    PAL_ASSERT(m_markerStack.NumElements() == MarkerStackCount);

    // Implement function table overrides
    m_funcTable.pfnCmdDraw                      = CmdDrawDecorator;
    m_funcTable.pfnCmdDrawOpaque                = CmdDrawOpaqueDecorator;
    m_funcTable.pfnCmdDrawIndexed               = CmdDrawIndexedDecorator;
    m_funcTable.pfnCmdDrawIndirectMulti         = CmdDrawIndirectMultiDecorator;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti  = CmdDrawIndexedIndirectMultiDecorator;
    m_funcTable.pfnCmdDispatch                  = CmdDispatchDecorator;
    m_funcTable.pfnCmdDispatchIndirect          = CmdDispatchIndirectDecorator;
    m_funcTable.pfnCmdDispatchOffset            = CmdDispatchOffsetDecorator;
    m_funcTable.pfnCmdDispatchDynamic           = CmdDispatchDynamicDecorator;
    m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMeshDecorator;
    m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMultiDecorator;
}

// =====================================================================================================================
CmdBuffer::~CmdBuffer()
{
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    ResetState();

    if (m_pMemoryChunk != nullptr)
    {
        m_pMemoryChunk->ReleaseReference();
    }

    Result result = m_pDevice->GetMemoryChunk(&m_pMemoryChunk);

    if (result == Result::Success)
    {
        // Release the old event cache
        if (m_pEventCache != nullptr)
        {
            m_pEventCache->ReleaseReference();
        }

        // Create a new event cache
        m_pEventCache = PAL_NEW(EventCache,
                                m_pPlatform,
                                Util::SystemAllocType::AllocInternal)(m_pPlatform);
    }

    if (result == Result::Success)
    {
        result = GetNextLayer()->Begin(NextCmdBufferBuildInfo(info));
    }

    if (result == Result::Success)
    {
        AddPreamble();
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    AddPostamble();
    return GetNextLayer()->End();
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    ResetState();

    if (m_pEventCache != nullptr)
    {
        Result result = m_pEventCache->CacheCmdBufferReset(m_cmdBufferId);
        PAL_ASSERT(result == Result::Success);
    }

    return GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
void CmdBuffer::Destroy()
{
    if (m_pMemoryChunk != nullptr)
    {
        m_pMemoryChunk->ReleaseReference();
        m_pMemoryChunk = nullptr;
    }

    if (m_pEventCache != nullptr)
    {
        m_pEventCache->ReleaseReference();
        m_pEventCache = nullptr;
    }

    ICmdBuffer* pNextLayer = m_pNextLayer;
    this->~CmdBuffer();
    pNextLayer->Destroy();
}

// =====================================================================================================================
void CmdBuffer::ResetState()
{
    for (uint32 i = 0; i < MarkerStackCount; i++)
    {
        m_markerStack[i].Clear();
    }
    m_markerCounter = 0;
}

// =====================================================================================================================
void CmdBuffer::AddPreamble()
{
    CmdWriteImmediate(
        HwPipePoint::HwPipeTop,
        m_cmdBufferId,
        ImmediateDataWidth::ImmediateData32Bit,
        GetGpuVa(offsetof(MarkerState, cmdBufferId)));
    CmdWriteImmediate(
        HwPipePoint::HwPipeTop,
        CrashAnalysis::InitialMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        GetGpuVa(offsetof(MarkerState, markerBegin)));
    CmdWriteImmediate(
        HwPipePoint::HwPipeTop,
        CrashAnalysis::InitialMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        GetGpuVa(offsetof(MarkerState, markerEnd)));
}

// =====================================================================================================================
// Write the final marker value BOP to indicate to the Crash Analysis tool that the command buffer has finished executing.
void CmdBuffer::AddPostamble()
{
    const gpusize gpuVaBegin = GetGpuVa(offsetof(MarkerState, markerBegin));
    const gpusize gpuVaEnd   = GetGpuVa(offsetof(MarkerState, markerEnd));

    CmdWriteImmediate(
        HwPipePoint::HwPipeBottom,
        CrashAnalysis::FinalMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        gpuVaBegin);

    CmdWriteImmediate(
        HwPipePoint::HwPipeBottom,
        CrashAnalysis::FinalMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        gpuVaEnd);
}

// =====================================================================================================================
MemoryChunk* CmdBuffer::GetMemoryChunk()
{
    if (m_pMemoryChunk != nullptr)
    {
        m_pMemoryChunk->TakeReference();
    }
    return m_pMemoryChunk;
}

// =====================================================================================================================
EventCache* CmdBuffer::GetEventCache()
{
    if (m_pEventCache != nullptr)
    {
        m_pEventCache->TakeReference();
    }
    return m_pEventCache;
}

// =====================================================================================================================
// Public entry-point for marker insertion.
uint32 CmdBuffer::CmdInsertExecutionMarker(
    bool         isBegin,
    uint8        sourceId,
    const char*  pMarkerName,
    uint32       markerNameSize)
{
    PAL_ASSERT_MSG((sourceId & 0xF0) == 0, "Source ID must be 4 bits");

    const MarkerSource source = static_cast<MarkerSource>(sourceId);
    uint32 marker;

    if (isBegin)
    {
        marker = InsertBeginMarker(source, pMarkerName, markerNameSize);
    }
    else
    {
        marker = InsertEndMarker(source);
    }

    return marker;
}

// =====================================================================================================================
// Insert top-of-pipe marker and emits an event annotated with the marker name.
uint32 CmdBuffer::InsertBeginMarker(
    MarkerSource source,
    const char*  pMarkerName,
    uint32       markerNameSize)
{
    const uint32 marker = GenerateMarker(source, (++m_markerCounter));

    if (PushMarker(source, marker) == Result::Success)
    {
        WriteMarkerImmediate(true, marker);

        m_pEventCache->CacheExecutionMarkerBegin(
            m_cmdBufferId,
            marker,
            pMarkerName,
            markerNameSize
        );
    }

    return marker;
}

// =====================================================================================================================
// Insert bottom-of-pipe marker and emits confirmation event.
uint32 CmdBuffer::InsertEndMarker(
    MarkerSource source)
{
    uint32 marker;

    if (PopMarker(source, &marker) == Result::Success)
    {
        WriteMarkerImmediate(false, marker);
        m_pEventCache->CacheExecutionMarkerEnd(m_cmdBufferId, marker);
    }

    return marker;
}

// =====================================================================================================================
Result CmdBuffer::PushMarker(
    MarkerSource source,
    uint32       marker)
{
    const uint32 index = static_cast<uint32>(source);
    return m_markerStack[index].PushBack(marker);
}

// =====================================================================================================================
Result CmdBuffer::PopMarker(
    MarkerSource source,
    uint32*      pMarker)
{
    Result       result = Result::ErrorUnknown;
    const uint32 index  = static_cast<uint32>(source);

    if ((pMarker != nullptr) && (m_markerStack[index].NumElements() > 0))
    {
        m_markerStack[index].PopBack(pMarker);
        result = (pMarker != nullptr) ? Result::Success : Result::ErrorUnknown;
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::WriteMarkerImmediate(
    bool   isBegin,
    uint32 marker)
{
    // There should never be a circumstance where m_pMemoryChunk is null here
    PAL_ALERT(m_pMemoryChunk == nullptr);

    if (m_pMemoryChunk != nullptr)
    {
        const HwPipePoint pipePoint = (isBegin) ? HwPipePoint::HwPipeTop
                                                : HwPipePoint::HwPipeBottom;
        const gpusize     offset    = (isBegin) ? offsetof(MarkerState, markerBegin)
                                                : offsetof(MarkerState, markerEnd);
        CmdWriteImmediate(pipePoint,
                          marker,
                          ImmediateDataWidth::ImmediateData32Bit,
                          GetGpuVa(offset));
    }
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32             cmdBufferCount,
    ICmdBuffer* const* ppCmdBuffers)
{
    const char          markerName[]   = "ExecuteNestedCmdBuffers";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    CmdBufferFwdDecorator::CmdExecuteNestedCmdBuffers(cmdBufferCount, ppCmdBuffers);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    const char          markerName[]   = "Draw";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount, drawId);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaqueDecorator(
    ICmdBuffer* pCmdBuffer,
    gpusize streamOutFilledSizeVa,
    uint32  streamOutOffset,
    uint32  stride,
    uint32  firstInstance,
    uint32  instanceCount)
{
    const char          markerName[]   = "DrawOpaque";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedDecorator(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    const char          markerName[]   = "DrawIndexed";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount, drawId);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMultiDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    const char          markerName[]   = "DrawIndirectMulti";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMultiDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    const char          markerName[]   = "DrawIndexedIndirectMulti";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                       offset,
                                                       stride,
                                                       maximumCount,
                                                       countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchDecorator(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    const char          markerName[]   = "Dispatch";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatch(size);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirectDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    const char          markerName[]   = "DispatchIndirect";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffsetDecorator(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    const char          markerName[]   = "CmdDispatchOffset";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchOffset(offset, launchSize, logicalSize);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchDynamicDecorator(
    ICmdBuffer*  pCmdBuffer,
    gpusize      gpuVa,
    DispatchDims size)
{
    const char          markerName[]   = "DispatchDynamic";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchDynamic(gpuVa, size);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchMeshDecorator(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    const char          markerName[]   = "DispatchMesh";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchMesh(size);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchMeshIndirectMultiDecorator(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    const char          markerName[]   = "DispatchMeshIndirectMultiDecorator";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchMeshIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                        offset,
                                                        stride,
                                                        maximumCount,
                                                        countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

} // namespace CrashAnalysis
} // namespace Pal
