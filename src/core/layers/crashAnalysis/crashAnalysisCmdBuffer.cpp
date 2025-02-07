/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palEventDefs.h"   // included for checking struct layout with static_assert
#include "palMutex.h"
#include "palSysMemory.h"
#include "palVectorImpl.h"

namespace Pal
{
namespace CrashAnalysis
{

static_assert(Pal::RgdMarkerSourceApplication == static_cast<uint8>(MarkerSource::Application));
static_assert(Pal::RgdMarkerSourceApi == static_cast<uint8>(MarkerSource::Api));
static_assert(Pal::RgdMarkerSourcePal == static_cast<uint8>(MarkerSource::Pal));
static_assert(Pal::RgdMarkerSourceHardware == static_cast<uint8>(MarkerSource::Hardware));
static_assert(Pal::RgdMarkerSourceCmdBufInfo ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerSource::CmdBufInfo));
static_assert(Pal::RgdMarkerSourceOpInfo ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerSource::OpInfo));
static_assert(Pal::RgdMarkerSourceSqttEventInfo ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerSource::SqttEvent));

static_assert(Pal::RgdMarkerInfoTypeInvalid ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::Invalid));
static_assert(Pal::RgdMarkerInfoTypeCmdBufStart ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::CmdBufStart));
static_assert(Pal::RgdMarkerInfoTypePipelineBind ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::PipelineBind));
static_assert(Pal::RgdMarkerInfoTypeDraw ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::Draw));
static_assert(Pal::RgdMarkerInfoTypeDrawUserData ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::DrawUserData));
static_assert(Pal::RgdMarkerInfoTypeDispatch ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::Dispatch));
static_assert(Pal::RgdMarkerInfoTypeBarrierBegin ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::BarrierBegin));
static_assert(Pal::RgdMarkerInfoTypeBarrierEnd ==
    static_cast<uint8>(UmdCrashAnalysisEvents::ExecutionMarkerInfoType::BarrierEnd));

static_assert(sizeof(Pal::RgdMarkerInfoHeader) ==
    sizeof(UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader));

static_assert(sizeof(Pal::RgdMarkerInfoCmdBufData) ==
    sizeof(Pal::RgdMarkerInfoHeader) + sizeof(UmdCrashAnalysisEvents::CmdBufferInfo));
static_assert(offsetof(RgdMarkerInfoCmdBufData, queue) ==
    offsetof(UmdCrashAnalysisEvents::CmdBufferInfo, queue) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoCmdBufData, deviceId) ==
    offsetof(UmdCrashAnalysisEvents::CmdBufferInfo, deviceId) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoCmdBufData, queueFlags) ==
    offsetof(UmdCrashAnalysisEvents::CmdBufferInfo, queueFlags) + sizeof(Pal::RgdMarkerInfoHeader));

static_assert(sizeof(Pal::RgdMarkerInfoBarrierBeginData) ==
    sizeof(Pal::RgdMarkerInfoHeader) + sizeof(UmdCrashAnalysisEvents::BarrierBeginInfo));
static_assert(offsetof(RgdMarkerInfoBarrierBeginData, isInternal) ==
    offsetof(UmdCrashAnalysisEvents::BarrierBeginInfo, isInternal) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoBarrierBeginData, type) ==
    offsetof(UmdCrashAnalysisEvents::BarrierBeginInfo, type) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoBarrierBeginData, reason) ==
    offsetof(UmdCrashAnalysisEvents::BarrierBeginInfo, reason) + sizeof(Pal::RgdMarkerInfoHeader));

static_assert(sizeof(Pal::RgdMarkerInfoBarrierEndData) ==
    sizeof(Pal::RgdMarkerInfoHeader) + sizeof(UmdCrashAnalysisEvents::BarrierEndInfo));
static_assert(offsetof(RgdMarkerInfoBarrierEndData, pipelineStalls) ==
    offsetof(UmdCrashAnalysisEvents::BarrierEndInfo, pipelineStalls) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoBarrierEndData, layoutTransitions) ==
    offsetof(UmdCrashAnalysisEvents::BarrierEndInfo, layoutTransitions) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoBarrierEndData, caches) ==
    offsetof(UmdCrashAnalysisEvents::BarrierEndInfo, caches) + sizeof(Pal::RgdMarkerInfoHeader));

static_assert(sizeof(Pal::RgdMarkerInfoDrawUserData) ==
    sizeof(Pal::RgdMarkerInfoHeader) + sizeof(UmdCrashAnalysisEvents::DrawUserData));
static_assert(offsetof(RgdMarkerInfoDrawUserData, vertexOffset) ==
    offsetof(UmdCrashAnalysisEvents::DrawUserData, vertexOffset) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoDrawUserData, instanceOffset) ==
    offsetof(UmdCrashAnalysisEvents::DrawUserData, instanceOffset) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoDrawUserData, drawId) ==
    offsetof(UmdCrashAnalysisEvents::DrawUserData, drawId) + sizeof(Pal::RgdMarkerInfoHeader));

static_assert(sizeof(Pal::RgdMarkerInfoDispatchData) ==
    sizeof(Pal::RgdMarkerInfoHeader) + sizeof(UmdCrashAnalysisEvents::DispatchInfo));
static_assert(offsetof(RgdMarkerInfoDispatchData, type) ==
    offsetof(UmdCrashAnalysisEvents::DispatchInfo, dispatchType) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoDispatchData, threadX) ==
    offsetof(UmdCrashAnalysisEvents::DispatchInfo, threadX) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoDispatchData, threadY) ==
    offsetof(UmdCrashAnalysisEvents::DispatchInfo, threadY) + sizeof(Pal::RgdMarkerInfoHeader));
static_assert(offsetof(RgdMarkerInfoDispatchData, threadZ) ==
    offsetof(UmdCrashAnalysisEvents::DispatchInfo, threadZ) + sizeof(Pal::RgdMarkerInfoHeader));

// =====================================================================================================================
// Generates a Crash Analysis marker from an origination source and a marker ID.
constexpr uint32 GenerateMarker(
    MarkerSource source,
    uint32       value)
{
    PAL_ASSERT_MSG((0x0fffffff | value) == 0x0fffffff,
                   "Malformed value (0x%X): unexpected top bits",
                    value);
    return ((static_cast<uint32>(source) << 28) | (value & 0x0fffffff));
}

// =====================================================================================================================
constexpr MarkerSource ExtractSourceFromMarker(
    uint32 markerValue)
{
    return static_cast<MarkerSource>(markerValue >> 28);
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
    m_cmdBufferId(0),
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

    // Re-generate the command buffer ID each time we begin command buffer recording.
    // This allows us to distinguish between command buffers that are re-recorded.
    m_cmdBufferId = m_pPlatform->GenerateResourceId();

    if (m_pMemoryChunk != nullptr)
    {
        m_pMemoryChunk->ReleaseReference();
    }

    Result result = m_pDevice->GetMemoryChunk(&m_pMemoryChunk);

    if (result == Result::Success)
    {
        // Initialize the memory chunk CPU-side, in the event that
        // we crash before the TOP writes in `AddPreamble` are hit.
        if (m_pMemoryChunk->pCpuAddr != nullptr)
        {
            m_pMemoryChunk->pCpuAddr->cmdBufferId = m_cmdBufferId;
            m_pMemoryChunk->pCpuAddr->markerBegin = 0x0;
            m_pMemoryChunk->pCpuAddr->markerEnd   = 0x0;
        }

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
    PAL_ASSERT(m_pMemoryChunk->pCpuAddr->cmdBufferId == m_cmdBufferId);

    CmdWriteImmediate(
        PipelineStageTopOfPipe,
        CrashAnalysis::InitialMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        GetGpuVa(offsetof(MarkerState, markerBegin)));
    CmdWriteImmediate(
        PipelineStageTopOfPipe,
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
        PipelineStageBottomOfPipe,
        CrashAnalysis::FinalMarkerValue,
        ImmediateDataWidth::ImmediateData32Bit,
        gpuVaBegin);

    CmdWriteImmediate(
        PipelineStageBottomOfPipe,
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
    const MarkerSource source = static_cast<MarkerSource>(sourceId);
    uint32 marker = 0;

    if (source == MarkerSource::OpInfo)
    {
        UmdCrashAnalysisEvents::EventId lastEventId;
        uint32      markerValue = 0;
        const char* pDontCare   = nullptr;

        // Get last eventId and markerValue
        m_pEventCache->GetEventAt(m_pEventCache->Count() - 1,
            &lastEventId,
            nullptr,
            &markerValue,
            &pDontCare,
            nullptr
        );

        InsertInfoMarker(markerValue, pMarkerName, markerNameSize);
        marker = markerValue;
    }
    else if (source == MarkerSource::CmdBufInfo)
    {
        InsertInfoMarker(CrashAnalysis::InitialMarkerValue, pMarkerName, markerNameSize);
        marker = CrashAnalysis::InitialMarkerValue;
    }
    else if (source == MarkerSource::SqttEvent)
    {
        PAL_ASSERT(markerNameSize == sizeof(uint32));
        PAL_ASSERT(pMarkerName != nullptr);
        m_stgSqttEvent = *reinterpret_cast<const uint32*>(pMarkerName);
    }
    else
    {
        if (isBegin)
        {
            marker = InsertBeginMarker(source, pMarkerName, markerNameSize);
        }
        else
        {
            marker = InsertEndMarker(source);
        }
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
void CmdBuffer::InsertInfoMarker(
    uint32      marker,
    const char* pMarkerInfo,
    uint32      markerInfoSize)
{
    auto pHeader = reinterpret_cast<const UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader*>(pMarkerInfo);

    if (pHeader->infoType == UmdCrashAnalysisEvents::ExecutionMarkerInfoType::DrawUserData)
    {
        PAL_ASSERT(markerInfoSize == sizeof(*pHeader) + sizeof(UmdCrashAnalysisEvents::DrawUserData));
        const char* pData = pMarkerInfo + sizeof(UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader);

#pragma pack(push, 1)
        struct
        {
            UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader header;
            UmdCrashAnalysisEvents::DrawInfo drawInfo;
        } info{};
#pragma pack(pop)

        info.header.infoType        = UmdCrashAnalysisEvents::ExecutionMarkerInfoType::Draw;
        info.drawInfo.drawType      = m_stgSqttEvent;
        info.drawInfo.instanceCount = m_stgDrawInfo.instanceCount;
        info.drawInfo.startIndex    = m_stgDrawInfo.startIndex;
        info.drawInfo.vtxIdxCount   = m_stgDrawInfo.vtxIdxCount;
        info.drawInfo.userData      = *reinterpret_cast<const UmdCrashAnalysisEvents::DrawUserData*>(pData);

        m_pEventCache->CacheExecutionMarkerInfo(
            m_cmdBufferId,
            marker,
            reinterpret_cast<const char*>(&info),
            sizeof(info));
    }
    else
    {
        m_pEventCache->CacheExecutionMarkerInfo(
            m_cmdBufferId,
            marker,
            pMarkerInfo,
            markerInfoSize);
    }
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
        const uint32  stage  = isBegin ? PipelineStageTopOfPipe
                                       : PipelineStageBottomOfPipe;
        const gpusize offset = isBegin ? offsetof(MarkerState, markerBegin)
                                       : offsetof(MarkerState, markerEnd);
        CmdWriteImmediate(stage,
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
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdExecuteNestedCmdBuffers(cmdBufferCount, ppCmdBuffers);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    const char       markerName[]   = "Barrier";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdBarrier(barrierInfo);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
uint32 CmdBuffer::CmdRelease(
#else
ReleaseToken CmdBuffer::CmdRelease(
#endif
    const AcquireReleaseInfo& releaseInfo)
{
    const char       markerName[]   = "Release";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    const uint32 syncToken = CmdBufferFwdDecorator::CmdRelease(releaseInfo);
#else
    const ReleaseToken syncToken = CmdBufferFwdDecorator::CmdRelease(releaseInfo);
#endif
    InsertEndMarker(MarkerSource::Pal);

    return syncToken;
}

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    const uint32*             pSyncTokens)
#else
    const ReleaseToken*       pSyncTokens)
#endif
{
    const char       markerName[]   = "Acquire";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    const char       markerName[]   = "ReleaseEvent";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdReleaseEvent(releaseInfo, pGpuEvent);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void CmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    const char       markerName[]   = "AcquireEvent";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    const char       markerName[]   = "ReleaseThenAcquire";
    constexpr uint32 MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);

    InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    CmdBufferFwdDecorator::CmdReleaseThenAcquire(barrierInfo);
    InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
#pragma pack(push, 1)
    struct
    {
        UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader header;
        UmdCrashAnalysisEvents::PipelineInfo pipelineInfo;
    } info{};
#pragma pack(pop)

    info.header.infoType         = UmdCrashAnalysisEvents::ExecutionMarkerInfoType::PipelineBind;
    info.pipelineInfo.bindPoint  = static_cast<uint32>(params.pipelineBindPoint);
    info.pipelineInfo.apiPsoHash = params.apiPsoHash;

    // Generate a new markerValue without inserting timestamp because no GPU work for BindPipeline
    const uint32 markerValue = GenerateMarker(MarkerSource::Pal, (++m_markerCounter));
    InsertInfoMarker(markerValue, reinterpret_cast<const char*>(&info), sizeof(info));

    CmdBufferFwdDecorator::CmdBindPipeline(params);
}

// =====================================================================================================================

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
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer* const    pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    // Don't have complete DrawInfo yet so store the info and wait
    pThis->m_stgDrawInfo.vtxIdxCount    = vertexCount;
    pThis->m_stgDrawInfo.instanceCount  = instanceCount;
    pThis->m_stgDrawInfo.startIndex     = firstVertex;

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
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
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
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
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->m_stgDrawInfo.vtxIdxCount   = indexCount;
    pThis->m_stgDrawInfo.instanceCount = instanceCount;
    pThis->m_stgDrawInfo.startIndex    = firstIndex;

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount, drawId);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMultiDecorator(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    const char          markerName[]   = "DrawIndirectMulti";
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDrawIndirectMulti(gpuVirtAddrAndStride,
                                                maximumCount,
                                                countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMultiDecorator(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    const char          markerName[]   = "DrawIndexedIndirectMulti";
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDrawIndexedIndirectMulti(gpuVirtAddrAndStride,
                                                maximumCount,
                                                countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchDecorator(
    ICmdBuffer*       pCmdBuffer,
    DispatchDims      size,
    DispatchInfoFlags infoFlags)
{
    const char          markerName[]   = "Dispatch";
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDispatch(size, infoFlags);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirectDecorator(
    ICmdBuffer* pCmdBuffer,
    gpusize     gpuVirtAddr)
{
    const char          markerName[]   = "DispatchIndirect";
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDispatchIndirect(gpuVirtAddr);
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
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDispatchOffset(offset, launchSize, logicalSize);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchMeshDecorator(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    const char          markerName[]   = "DispatchMesh";
    constexpr uint32    MarkerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    uint32 markerValue = pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], MarkerNameSize);
    pThis->GetNextLayer()->CmdDispatchMesh(size);

#pragma pack(push, 1)
    struct
    {
        UmdCrashAnalysisEvents::ExecutionMarkerInfoHeader header;
        UmdCrashAnalysisEvents::DispatchInfo dispatch;
    } info{};
#pragma pack(pop)

    // Matching RgpSqttMarkerEventType::CmdUnknown/RgpSqttMarkerApiType::RGP_SQTT_MARKER_API_UNKNOWN
    constexpr uint32 UnknownEvent = 0x7fff;

    // DisptachMesh is a Draw rather than a Dispatch. Dimension information is lost in client callback and has to be
    // collected here.
    info.header.infoType       = UmdCrashAnalysisEvents::ExecutionMarkerInfoType::Dispatch;
    info.dispatch.dispatchType = UnknownEvent;  // Client specific type is not available here
    info.dispatch.threadX      = size.x;
    info.dispatch.threadY      = size.y;
    info.dispatch.threadZ      = size.z;
    pThis->InsertInfoMarker(markerValue, reinterpret_cast<const char*>(&info), sizeof(info));

    pThis->InsertEndMarker(MarkerSource::Pal);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchMeshIndirectMultiDecorator(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    const char          markerName[]   = "DispatchMeshIndirectMulti";
    constexpr uint32    markerNameSize = static_cast<uint32>(sizeof(markerName) - 1);
    CmdBuffer*const     pThis          = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->InsertBeginMarker(MarkerSource::Pal, &markerName[0], markerNameSize);
    pThis->GetNextLayer()->CmdDispatchMeshIndirectMulti(gpuVirtAddrAndStride,
                                                        maximumCount,
                                                        countGpuAddr);
    pThis->InsertEndMarker(MarkerSource::Pal);
}

} // namespace CrashAnalysis
} // namespace Pal
