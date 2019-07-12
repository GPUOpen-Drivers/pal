/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdBuffer.h"
#include "core/fence.h"
#include "core/cmdStream.h"
#include "core/device.h"
#include "core/engine.h"
#include "core/fence.h"
#include "core/gpuMemory.h"
#include "core/platform.h"
#include "core/queue.h"
#include "core/queueContext.h"
#include "core/queueSemaphore.h"
#include "core/swapChain.h"
#include "core/hw/ossip/ossDevice.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/rpm/rsrcProcMgr.h"
#include "palDequeImpl.h"
#include "palSysUtil.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
void SubmissionContext::TakeReference()
{
    AtomicIncrement(&m_refCount);
}

// =====================================================================================================================
void SubmissionContext::ReleaseReference()
{
    PAL_ASSERT(m_refCount > 0);

    if (AtomicDecrement(&m_refCount) == 0)
    {
        Platform*const pPlatform = m_pPlatform;
        PAL_DELETE_THIS(SubmissionContext, pPlatform);
    }
}

// =====================================================================================================================
Queue::Queue(
    Device*                pDevice,
    const QueueCreateInfo& createInfo)
    :
    m_pDevice(pDevice),
    m_type(createInfo.queueType),
    m_engineType(createInfo.engineType),
    m_engineId(createInfo.engineIndex),
    m_submitOptMode((pDevice->Settings().submitOptModeOverride == 0)
        ? createInfo.submitOptMode : static_cast<SubmitOptMode>(pDevice->Settings().submitOptModeOverride - 1)),
    m_pEngine(pDevice->GetEngine(createInfo.engineType, createInfo.engineIndex)),
    m_pSubmissionContext(nullptr),
    m_pDummyCmdBuffer(nullptr),
    m_ifhMode(IfhModeDisabled),
    m_numReservedCu(0),
    m_pQueueContext(nullptr),
    m_stalled(false),
    m_pWaitingSemaphore(nullptr),
    m_batchedSubmissionCount(0),
    m_batchedCmds(pDevice->GetPlatform()),
    m_deviceMembershipNode(this),
    m_engineMembershipNode(this),
    m_lastFrameCnt(0),
    m_submitIdPerFrame(0),
    m_queuePriority(QueuePriority::Low),
    m_persistentCeRamOffset(0),
    m_persistentCeRamSize(0),
    m_pTrackedCmdBufferDeque(nullptr)
{
    if (m_pDevice->Settings().ifhGpuMask & (0x1 << m_pDevice->ChipProperties().gpuIndex))
    {
        m_ifhMode = m_pDevice->Settings().ifh;
    }

    m_flags.u32All = 0;

    // override the priority here.
    // the RtCuHighCompute and Exclusive Compute are only supported on Windows now.
    m_queuePriority = (m_engineType == EngineTypeExclusiveCompute) ?
                                       QueuePriority::High :
                                       createInfo.priority;

    const auto engineSubType = m_pDevice->EngineProperties().perEngine[m_engineType].engineSubType[m_engineId];
    if (engineSubType == EngineSubType::RtCuHighCompute)
    {
        m_numReservedCu = createInfo.numReservedCu;
    }

    m_queuePriority = (engineSubType == EngineSubType::RtCuMedCompute) ? QueuePriority::Medium : m_queuePriority;

    if (createInfo.windowedPriorBlit != false)
    {
        m_flags.windowedPriorBlit = 1;
    }

    if (pDevice->EngineProperties().perEngine[m_engineType].flags.physicalAddressingMode != 0)
    {
        m_flags.physicalModeSubmission = 1;
    }

    if (pDevice->IsPreemptionSupported(m_engineType))
    {
        m_flags.midCmdBufPreemption = 1;
    }

    if (pDevice->EngineProperties().perEngine[m_engineType].flags.supportPersistentCeRam == 0)
    {
        PAL_ASSERT((createInfo.persistentCeRamOffset == 0) && (createInfo.persistentCeRamSize == 0));

        m_persistentCeRamOffset = 0;
        m_persistentCeRamSize   = 0;
    }
    else
    {
        constexpr uint32 CeRamAlignBytes = 32;

        // Align the offset and size of persistent CE RAM to 32 bytes (8 DWORDs).
        m_persistentCeRamOffset = Pow2AlignDown(createInfo.persistentCeRamOffset, CeRamAlignBytes);
        const uint32 difference = (createInfo.persistentCeRamOffset - m_persistentCeRamOffset);
        m_persistentCeRamSize   =
            static_cast<uint32>(Pow2Align(((sizeof(uint32) * createInfo.persistentCeRamSize) + difference),
                                          CeRamAlignBytes) / sizeof(uint32));

        PAL_ASSERT((m_persistentCeRamOffset == createInfo.persistentCeRamOffset) &&
                   (m_persistentCeRamSize   == createInfo.persistentCeRamSize));

        // The client can request some part of the CE ram to be persistent through consecutive submissions, and the
        // whole CE ram used must be at least as big as that.
        PAL_ASSERT(pDevice->CeRamDwordsUsed(EngineTypeUniversal) >= m_persistentCeRamOffset + m_persistentCeRamSize);
    }
}

// =====================================================================================================================
// Queues must be careful to clean up their member classes before destructing because some of them may call one of the
// queues' virtual functions.
void Queue::Destroy()
{
    // NOTE: If there are still outstanding batched commands for this Queue, something has gone very wrong!
    PAL_ASSERT(m_batchedCmds.NumElements() == 0);

    // There are some CmdStreams which are created with UntrackedCmdAllocator, then the CmdStreamChunks in those
    // CmdStreams will have race condition when CmdStreams are destructed. Only CPU side reference count is used to
    // track chunks. Multi-queues share the same UntrackedCmdAllocator will have chance to overwrite command chunk
    // which is still not executed (but is marked as free by RemoveCommandStreamReference in the ~CmdStream() )
    // and can cause ASIC hang. This issue could be easily re-produced under SRIOV platform because virtual GPU is
    // slow and have chance to be preempted. Solution is call WaitIdle before doing anything else.
    WaitIdle();

    if (m_pTrackedCmdBufferDeque != nullptr)
    {
        while (m_pTrackedCmdBufferDeque->NumElements() > 0)
        {
            TrackedCmdBuffer* pTrackedCmdBuffer = nullptr;
            m_pTrackedCmdBufferDeque->PopFront(&pTrackedCmdBuffer);
            DestroyTrackedCmdBuffer(pTrackedCmdBuffer);
        }
        PAL_SAFE_DELETE(m_pTrackedCmdBufferDeque, m_pDevice->GetPlatform());
    }

    if (m_pDummyCmdBuffer != nullptr)
    {
        m_pDummyCmdBuffer->DestroyInternal();
        m_pDummyCmdBuffer = nullptr;
    }

    if (m_pQueueContext != nullptr)
    {
        m_pQueueContext->Destroy();
        m_pQueueContext = nullptr;
    }

    if (m_engineMembershipNode.InList())
    {
        m_pEngine->RemoveQueue(&m_engineMembershipNode);
    }

    if (m_deviceMembershipNode.InList())
    {
        m_pDevice->RemoveQueue(this);
    }

    if (m_pSubmissionContext != nullptr)
    {
        m_pSubmissionContext->ReleaseReference();
        m_pSubmissionContext = nullptr;
    }

    this->~Queue();
}

// =====================================================================================================================
// Initializes this Queue object's QueueContext and batched-command Mutex objects.
Result Queue::Init(
    void* pContextPlacementAddr)
{
    Result      result     = m_batchedCmdsLock.Init();
    GfxDevice*  pGfxDevice = m_pDevice->GetGfxDevice();

    if (result == Result::Success)
    {
        // NOTE: OSSIP hardware is used for DMA Queues, GFXIP hardware is used for Compute & Universal Queues, and
        // no hardware block is used for Timer Queues since those are software-only.
        switch (m_type)
        {
        case QueueTypeCompute:
        case QueueTypeUniversal:
            if (pGfxDevice != nullptr)
            {
                result = pGfxDevice->CreateQueueContext(this, m_pEngine, pContextPlacementAddr, &m_pQueueContext);
            }
            else
            {
                result = Result::ErrorIncompatibleDevice;
            }
            break;
        case QueueTypeDma:
            {
                OssDevice*  pOssDevice = m_pDevice->GetOssDevice();

                if (pOssDevice != nullptr)
                {
                    result = pOssDevice->CreateQueueContext(this, pContextPlacementAddr, &m_pQueueContext);
                }
                else if ((pGfxDevice != nullptr) && IsGfx10(*m_pDevice))
                {
                    result = pGfxDevice->CreateQueueContext(this, m_pEngine, pContextPlacementAddr, &m_pQueueContext);
                }
                else
                {
                    result = Result::ErrorIncompatibleDevice;
                }
            }
            break;
        case QueueTypeTimer:
            m_pQueueContext = PAL_PLACEMENT_NEW(pContextPlacementAddr) QueueContext(m_pDevice);
            break;

        default:
            // We shouldn't get here. It means someone tried to create a queue type we don't support.
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorUnknown;
            break;
        }
    }

    // Skip the dummy command buffer on timer engines because there is no timer engine command buffer.
    if ((result == Result::Success) && (m_engineType != EngineTypeTimer))
    {
        CmdBufferCreateInfo createInfo = {};
        createInfo.pCmdAllocator = m_pDevice->InternalCmdAllocator(m_engineType);
        createInfo.queueType     = m_type;
        createInfo.engineType    = m_engineType;

        CmdBufferInternalCreateInfo internalInfo = {};
        internalInfo.flags.isInternal = 1;

        result = m_pDevice->CreateInternalCmdBuffer(createInfo, internalInfo, &m_pDummyCmdBuffer);

        if (result == Result::Success)
        {
            CmdBufferBuildInfo buildInfo = {};
            buildInfo.flags.optimizeExclusiveSubmit = 1;
            result = m_pDummyCmdBuffer->Begin(buildInfo);

            if (result == Result::Success)
            {
                result = m_pDummyCmdBuffer->End();
            }
        }
    }

    const bool shouldAllocTrackedCmdBuffers =
        m_pDevice->GetPlatform()->IsDeveloperModeEnabled();

    // Initialize internal tracked command buffers if they are required.
    if ((result == Result::Success) && shouldAllocTrackedCmdBuffers)
    {
        m_pTrackedCmdBufferDeque =
            PAL_NEW(TrackedCmdBufferDeque, m_pDevice->GetPlatform(), AllocInternal)
            (m_pDevice->GetPlatform());

        result = (m_pTrackedCmdBufferDeque != nullptr) ? Result::Success : Result::ErrorOutOfMemory;
    }

    return result;
}

// =====================================================================================================================
// Submits a set of command buffers for execution on this Queue.
Result Queue::SubmitInternal(
    const SubmitInfo& submitInfo,
    bool              postBatching)
{
    Result result = Result::Success;

    InternalSubmitInfo internalSubmitInfo = {};

    for (uint32 idx = 0; (idx < submitInfo.cmdBufferCount) && (result == Result::Success); ++idx)
    {
        // Pre-process the command buffers before submission.
        // Command buffers that require building the commands at submission time should build them here.
        auto*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[idx]);
        result = pCmdBuffer->PreSubmit();
    }

    if (result == Result::Success)
    {
        result = ValidateSubmit(submitInfo);
    }

    if (result == Result::Success)
    {
        result = m_pQueueContext->PreProcessSubmit(&internalSubmitInfo, submitInfo);
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    if (result == Result::Success)
    {
        // Dump command buffer
        DumpCmdToFile(submitInfo, internalSubmitInfo);
    }
#endif

    if (result == Result::Success)
    {
        if (m_ifhMode == IfhModeDisabled)
        {
            for (uint32 idx = 0; idx < submitInfo.cmdBufferCount; ++idx)
            {
                // Each command buffer being submitted needs to be notified about it, so the command stream(s) can
                // manage their GPU-completion tracking.
                auto*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[idx]);
                pCmdBuffer->IncrementSubmitCount();
            }
        }

        if (submitInfo.pFence != nullptr)
        {
            static_cast<Fence*>(submitInfo.pFence)->AssociateWithContext(m_pSubmissionContext);
        }

        // Either execute the submission immediately, or enqueue it for later, depending on whether or not we are
        // stalled and/or the caller is a function after the batching logic and thus must execute immediately.
        if (postBatching || (m_stalled == false))
        {
            result = OsSubmit(submitInfo, internalSubmitInfo);
        }
        else
        {
            result = EnqueueSubmit(submitInfo, internalSubmitInfo);
        }
    }

    if (result == Result::Success)
    {
        m_pQueueContext->PostProcessSubmit();
    }

    return result;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Dumps a set of command buffers submitted on this Queue.
void Queue::DumpCmdToFile(
    const SubmitInfo&         submitInfo,
    const InternalSubmitInfo& internalSubmitInfo)
{
    // To dump the command buffer upon submission for the specified frames
    const auto&            settings   = m_pDevice->Settings();
    const CmdBufDumpFormat dumpFormat = settings.cmdBufDumpFormat;

    static const char* const pSuffix[] =
    {
        ".txt",     // CmdBufDumpFormat::CmdBufDumpFormatText
        ".bin",     // CmdBufDumpFormat::CmdBufDumpFormatBinary
        ".pm4"      // CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders
    };

    const uint32 frameCnt = m_pDevice->GetFrameCount();
    const bool cmdBufDumpEnabled = (m_pDevice->IsCmdBufDumpEnabled() ||
                                    ((frameCnt >= settings.submitTimeCmdBufDumpStartFrame) &&
                                     (frameCnt <= settings.submitTimeCmdBufDumpEndFrame)));

    if ((settings.cmdBufDumpMode == CmdBufDumpModeSubmitTime) &&
        (cmdBufDumpEnabled))
    {
        const char* pLogDir = &settings.cmdBufDumpDirectory[0];
        // Create the directory. We don't care if it fails (existing is fine, failure is caught when opening the file).
        MkDir(pLogDir);

        // Maximum length of a filename allowed for command buffer dumps, seems more reasonable than 32
        constexpr uint32 MaxFilenameLength = 512;

        char filename[MaxFilenameLength] = {};
        File logFile;

        // Multiple submissions of one frame
        if (m_lastFrameCnt == frameCnt)
        {
            m_submitIdPerFrame++;
        }
        else
        {
            // First submission of one frame
            m_submitIdPerFrame = 0;
        }

        // Add queue type and this pointer to file name to make name unique since there could be multiple queues/engines
        // and/or multiple vitual queues (on the same engine on) which command buffers are submitted
        Snprintf(filename, MaxFilenameLength, "%s/Frame_%u_%p_%u_%04u%s",
                 pLogDir,
                 m_type,
                 this,
                 frameCnt,
                 m_submitIdPerFrame,
                 pSuffix[dumpFormat]);

        m_lastFrameCnt = frameCnt;

        if (dumpFormat == CmdBufDumpFormat::CmdBufDumpFormatText)
        {
            PAL_ALERT_MSG(logFile.Open(&filename[0], FileAccessMode::FileAccessWrite) != Result::Success,
                          "Failed to open CmdBuf dump file '%s'", filename);
        }
        else if ((dumpFormat == CmdBufDumpFormat::CmdBufDumpFormatBinary) ||
                 (dumpFormat == CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders))
        {
            const uint32 fileMode = FileAccessMode::FileAccessWrite | FileAccessMode::FileAccessBinary;
            PAL_ALERT_MSG(logFile.Open(&filename[0], fileMode) != Result::Success,
                          "Failed to open CmdBuf dump file '%s'", filename);

            if (dumpFormat == CmdBufDumpFormat::CmdBufDumpFormatBinaryHeaders)
            {
                const CmdBufferDumpFileHeader fileHeader =
                {
                    static_cast<uint32>(sizeof(CmdBufferDumpFileHeader)), // Structure size
                    1,                                                    // Header version
                    m_pDevice->ChipProperties().familyId,                 // ASIC family
                    m_pDevice->ChipProperties().deviceId,                 // Reserved, but use for PCI device ID
                    0                                                     // Reserved
                };
                logFile.Write(&fileHeader, sizeof(fileHeader));
            }

            CmdBufferListHeader listHeader =
            {
                static_cast<uint32>(sizeof(CmdBufferListHeader)),   // Structure size
                EngineId(),                                         // Engine index
                0                                                   // Number of command buffer chunks
            };

            for (uint32 idxCmdBuf = 0; idxCmdBuf < submitInfo.cmdBufferCount; ++idxCmdBuf)
            {
                const auto*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[idxCmdBuf]);

                for (uint32 idxStream = 0; idxStream < pCmdBuffer->NumCmdStreams(); ++idxStream)
                {
                    listHeader.count += pCmdBuffer->GetCmdStream(idxStream)->GetNumChunks();
                }
            }

            for (uint32 idx = 0; idx < internalSubmitInfo.numPreambleCmdStreams; ++idx)
            {
                PAL_ASSERT(internalSubmitInfo.pPreambleCmdStream[idx] != nullptr);
                listHeader.count += internalSubmitInfo.pPreambleCmdStream[idx]->GetNumChunks();
            }

            for (uint32 idx = 0; idx < internalSubmitInfo.numPostambleCmdStreams; ++idx)
            {
                PAL_ASSERT(internalSubmitInfo.pPostambleCmdStream[idx] != nullptr);
                listHeader.count += internalSubmitInfo.pPostambleCmdStream[idx]->GetNumChunks();
            }

            logFile.Write(&listHeader, sizeof(listHeader));
        }
        else
        {
            // If we get here, dumping is enabled, but it's not one of the modes listed above.
            // Perhaps someone added a new mode?
            PAL_ASSERT_ALWAYS();
        }

        const char* QueueTypeStrings[] =
        {
            "# Universal Queue - QueueContext Command length = ",
            "# Compute Queue - QueueContext Command length = ",
            "# DMA Queue - QueueContext Command length = ",
            "",
        };

        static_assert(ArrayLen(QueueTypeStrings) == static_cast<size_t>(QueueTypeCount),
                      "Mismatch between QueueTypeStrings array size and QueueTypeCount");

        for (uint32 idx = 0; idx < internalSubmitInfo.numPreambleCmdStreams; ++idx)
        {
            PAL_ASSERT(internalSubmitInfo.pPreambleCmdStream[idx] != nullptr);
            internalSubmitInfo.pPreambleCmdStream[idx]->DumpCommands(&logFile,
                                                                     QueueTypeStrings[m_type],
                                                                     dumpFormat);
        }

        for (uint32 idxCmdBuf = 0; idxCmdBuf < submitInfo.cmdBufferCount; ++idxCmdBuf)
        {
            const auto*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[idxCmdBuf]);
            pCmdBuffer->DumpCmdStreamsToFile(&logFile, dumpFormat);
        }

        for (uint32 idx = 0; idx < internalSubmitInfo.numPostambleCmdStreams; ++idx)
        {
            PAL_ASSERT(internalSubmitInfo.pPostambleCmdStream[idx] != nullptr);
            internalSubmitInfo.pPostambleCmdStream[idx]->DumpCommands(&logFile,
                                                                      QueueTypeStrings[m_type],
                                                                      dumpFormat);
        }
    }
}
#endif

// =====================================================================================================================
Result Queue::CreateTrackedCmdBuffer(
    TrackedCmdBuffer** ppTrackedCmdBuffer)
{
    PAL_ASSERT(m_pTrackedCmdBufferDeque != nullptr);

    Result result = Result::Success;

    TrackedCmdBuffer* pTrackedCmdBuffer = nullptr;

    if ((m_pTrackedCmdBufferDeque->NumElements() == 0) ||
        (m_pTrackedCmdBufferDeque->Front()->pFence->GetStatus() == Result::NotReady))
    {
        pTrackedCmdBuffer = PAL_NEW(TrackedCmdBuffer, m_pDevice->GetPlatform(), AllocInternal);

        if (pTrackedCmdBuffer == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            Pal::FenceCreateInfo createInfo = {};
            result = m_pDevice->CreateInternalFence(createInfo, &pTrackedCmdBuffer->pFence);
        }

        if (result == Result::Success)
        {
            CmdBufferCreateInfo createInfo = {};
            createInfo.pCmdAllocator       = m_pDevice->InternalCmdAllocator(m_engineType);
            createInfo.queueType           = m_type;
            createInfo.engineType          = m_engineType;

    #if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 449)
            createInfo.engineSubType = m_pDevice->EngineProperties().perEngine[m_engineType].engineSubType[m_engineId];
    #endif

            CmdBufferInternalCreateInfo internalInfo = {};
            internalInfo.flags.isInternal            = 1;

            result = m_pDevice->CreateInternalCmdBuffer(createInfo, internalInfo, &pTrackedCmdBuffer->pCmdBuffer);
        }
    }
    else
    {
        result = m_pTrackedCmdBufferDeque->PopFront(&pTrackedCmdBuffer);
    }

    // Immediately push this command buffer onto the back of the deque to avoid leaking memory.
    if (result == Result::Success)
    {
        result = m_pTrackedCmdBufferDeque->PushBack(pTrackedCmdBuffer);
    }

    if ((result != Result::Success) && (pTrackedCmdBuffer != nullptr))
    {
        DestroyTrackedCmdBuffer(pTrackedCmdBuffer);
        pTrackedCmdBuffer = nullptr;
    }

    // Rebuild the command buffer with the assumption it will be used for a single submission.
    if (result == Result::Success)
    {
        CmdBufferBuildInfo buildInfo = {};
        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pTrackedCmdBuffer->pCmdBuffer->Begin(buildInfo);
    }

    if (result == Result::Success)
    {
        *ppTrackedCmdBuffer = pTrackedCmdBuffer;
    }

    return result;
}

// =====================================================================================================================
Result Queue::SubmitTrackedCmdBuffer(
    TrackedCmdBuffer* pTrackedCmdBuffer,
    const GpuMemory*  pWrittenPrimary)
{
    Result result = pTrackedCmdBuffer->pCmdBuffer->End();

    if (result == Result::Success)
    {
        result = m_pDevice->ResetFences(1, reinterpret_cast<IFence**>(&pTrackedCmdBuffer->pFence));
    }

    if (result == Result::Success)
    {
        SubmitInfo submitInfo     = {};
        submitInfo.cmdBufferCount = 1;
        submitInfo.ppCmdBuffers   = reinterpret_cast<ICmdBuffer**>(&pTrackedCmdBuffer->pCmdBuffer);
        submitInfo.pFence         = pTrackedCmdBuffer->pFence;

        if ((m_pDevice->GetPlatform()->GetProperties().supportBlockIfFlipping == 1) &&
            (pWrittenPrimary != nullptr) && (pWrittenPrimary->IsFlippable()))
        {
            submitInfo.ppBlockIfFlipping    = reinterpret_cast<const IGpuMemory**>(&pWrittenPrimary);
            submitInfo.blockIfFlippingCount = 1;
        }

        result = Submit(submitInfo);
    }

    return result;
}

// =====================================================================================================================
// Destroys a tracked command buffer object. It is the caller's responsibility to make sure that the command buffer has
// finished executing before calling this function and that it has been removed from the deque.
void Queue::DestroyTrackedCmdBuffer(
    TrackedCmdBuffer* pTrackedCmdBuffer)
{
    if (pTrackedCmdBuffer->pCmdBuffer != nullptr)
    {
        pTrackedCmdBuffer->pCmdBuffer->DestroyInternal();
    }

    if (pTrackedCmdBuffer->pFence != nullptr)
    {
        pTrackedCmdBuffer->pFence->DestroyInternal(m_pDevice->GetPlatform());
    }

    PAL_SAFE_DELETE(pTrackedCmdBuffer, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Waits for all requested submissions on this Queue to finish, including any batched-up submissions. This call never
// fails, but may wait awhile if the command buffers are long-running, or forever if the GPU is hung.) We do not wait
// for pending semaphore waits or delay operations.
// NOTE: Part of the public IQueue interface.
Result Queue::WaitIdle()
{
    Result result = Result::Success;

    // If this queue is blocked by a semaphore, this will spin loop until all batched submissions have been processed.
    while (m_batchedSubmissionCount > 0)
    {
        // Yield this CPU to give other threads a chance to run and so we don't burn too much power.
        YieldThread();
    }

    // When we get here, all batched operations (if there were any) have been processed, so wait for the OS-specific
    // Queue to become idle.
    return OsWaitIdle();
}

// =====================================================================================================================
// Signals the specified Semaphore using this Queue. The 'signal' operation will be executed by the GPU or OS scheduler
// when all previously-submitted work on this Queue has completed.
// NOTE: Part of the public IQueue interface.
Result Queue::SignalQueueSemaphoreInternal(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value,
    bool             postBatching)
{
    QueueSemaphore*const pSemaphore = static_cast<QueueSemaphore*>(pQueueSemaphore);

    Result result = Result::Success;

    // Either signal the semaphore immediately, or enqueue it for later, depending on whether or not we are stalled
    // and/or the caller is a function after the batching logic and thus must execute immediately.
    if (postBatching || (m_stalled == false))
    {
        // The Semaphore object is responsible for notifying any stalled Queues which may get released by this signal
        // operation.
        result = pSemaphore->Signal(this, value);
    }
    else
    {
        // After taking the lock, check again to see if we're stalled. The original check which brought us down
        // this path didn't take the lock beforehand, so its possible that another thread released this Queue
        // from the stalled state before we were able to get into this method.
        MutexAuto lock(&m_batchedCmdsLock);
        if (m_stalled)
        {
            BatchedQueueCmdData cmdData  = { };
            cmdData.command              = BatchedQueueCmd::SignalSemaphore;
            cmdData.semaphore.pSemaphore = pQueueSemaphore;
            cmdData.semaphore.value      = value;

            result = m_batchedCmds.PushBack(cmdData);
        }
        else
        {
            result = pSemaphore->Signal(this, value);
        }
    }

    return result;
}

// =====================================================================================================================
// Waits on the specified Semaphore using this Queue. The 'wait' operation may be batched-up if the corresponding
// Signal has not been sent by the client yet. After this wait, all future GPU work submitted to this Queue will not
// execute until the Semaphore has been signaled by the GPU on another Queue.
// NOTE: Part of the public IQueue interface.
Result Queue::WaitQueueSemaphoreInternal(
    IQueueSemaphore* pQueueSemaphore,
    uint64           value,
    bool             postBatching)
{
    QueueSemaphore*const pSemaphore = static_cast<QueueSemaphore*>(pQueueSemaphore);

    Result result = Result::Success;

    // Either wait on the semaphore immediately, or enqueue it for later, depending on whether or not we are stalled
    // and/or the caller is a function after the batching logic and thus must execute immediately.
    if (postBatching || (m_stalled == false))
    {
        // If this Queue isn't stalled yet, we can execute the wait immediately (which, of course, could stall
        // this Queue).
        result = pSemaphore->Wait(this, value, &m_stalled);
    }
    else
    {
        // After taking the lock, check again to see if we're stalled. The original check which brought us down
        // this path didn't take the lock beforehand, so its possible that another thread released this Queue
        // from the stalled state before we were able to get into this method.
        MutexAuto lock(&m_batchedCmdsLock);
        if (m_stalled)
        {
            BatchedQueueCmdData cmdData  = { };
            cmdData.command              = BatchedQueueCmd::WaitSemaphore;
            cmdData.semaphore.pSemaphore = pQueueSemaphore;
            cmdData.semaphore.value      = value;

            result = m_batchedCmds.PushBack(cmdData);
        }
        else
        {
            result = pSemaphore->Wait(this, value, &m_stalled);
        }
    }

    return result;
}

// =====================================================================================================================
// Queues the specified image for presentation on the screen. All previous work done on this queue will complete before
// the image is displayed. If isClientPresent is true, this call came directly from the client and can be used to
// denote a frame boundary. In some cases PAL may make internal calls which do not denote frame boundaries.
Result Queue::PresentDirectInternal(
    const PresentDirectInfo& presentInfo,
    bool                     isClientPresent)
{

    Result result = Result::Success;

    // Check if our queue supports the given present mode.
    if (IsPresentModeSupported(presentInfo.presentMode) == false)
    {
        if ((presentInfo.presentMode == PresentMode::Windowed) && (m_pDevice->IsMasterGpu() == false))
        {
            result = Result::ErrorWindowedPresentUnavailable;
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }
    else
    {
        // We only want to add postprocessing when this is a non-internal present. Internal presents are expected to
        // have done so themselves.
        if (((result == Result::Success) && isClientPresent) && (presentInfo.pSrcImage != nullptr))
        {
            result = SubmitPostprocessCmdBuffer(static_cast<Image&>(*presentInfo.pSrcImage));
        }

        if (result == Result::Success)
        {
            // Either execute the present immediately, or enqueue it for later, depending on whether or not we are
            // stalled.
            if (m_stalled == false)
            {
                result = OsPresentDirect(presentInfo);
            }
            else
            {
                // After taking the lock, check again to see if we're stalled. The original check which brought us down
                // this path didn't take the lock beforehand, so its possible that another thread released this Queue
                // from the stalled state before we were able to get into this method.
                MutexAuto lock(&m_batchedCmdsLock);
                if (m_stalled)
                {
                    BatchedQueueCmdData cmdData = {};
                    cmdData.command             = BatchedQueueCmd::PresentDirect;
                    cmdData.presentDirect.info  = presentInfo;

                    result = m_batchedCmds.PushBack(cmdData);
                }
                else
                {
                    result = OsPresentDirect(presentInfo);
                }
            }
        }
    }

    // Increment our frame counter if this present denotes a frame boundary.
    if (isClientPresent)
    {
        m_pDevice->IncFrameCount();
    }

    return result;
}

// =====================================================================================================================
// Queues the specified image for presentation on the screen. All previous work done on this queue will complete before
// the image is displayed.
// NOTE: Part of the public IQueue interface.
Result Queue::PresentSwapChain(
    const PresentSwapChainInfo& presentInfo)
{
    Result result = Result::Success;

    const auto*const  pSrcImage       = static_cast<const Image*>(presentInfo.pSrcImage);
    const Image*const pPresentedImage =
        pSrcImage;

    auto*const pSwapChain = static_cast<SwapChain*>(presentInfo.pSwapChain);

    // Validate the present info. If this succeeds we must always call into the swap chain to release ownership of the
    // image index. Otherwise, the application will deadlock on AcquireNextImage at some point in the future.
    if ((pSrcImage == nullptr) || (pSwapChain == nullptr))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if ((pPresentedImage->IsPresentable() == false) ||
             ((presentInfo.presentMode == PresentMode::Fullscreen) && (pPresentedImage->IsFlippable() == false)) ||
             (presentInfo.imageIndex >= pSwapChain->CreateInfo().imageCount))
    {
        result = Result::ErrorInvalidValue;
    }

    if (result == Result::Success)
    {
        result = SubmitPostprocessCmdBuffer(static_cast<Image&>(*presentInfo.pSrcImage));
    }

    if (presentInfo.flags.notifyOnly == 0)
    {
        if (result == Result::Success)
        {
            // Always execute the Present immediately, even if we are stalled. We should (and can) do this because:
            // - The swap chain and present scheduler Present code was designed to only issue batchable queue
            //   operations.
            // - We must release the given swap chain image index before this function returns.
            result = pSwapChain->Present(presentInfo, this);
        }

        IncFrameCount();
    }

    return result;
}

// =====================================================================================================================
// Inserts a delay of a specified amount of time before processing more commands on this queue. Only supported on Timer
// Queues.
// NOTE: Part of the public IQueue interface.
Result Queue::Delay(
    float delay)
{
    Result result = Result::ErrorUnavailable;

    if (m_type == QueueTypeTimer)
    {
        // Either execute the delay immediately, or enqueue it for later, depending on whether or not we are stalled.
        if (m_stalled == false)
        {
            result = OsDelay(delay, nullptr);
        }
        else
        {
            // After taking the lock, check again to see if we're stalled. The original check which brought us down
            // this path didn't take the lock beforehand, so its possible that another thread released this Queue
            // from the stalled state before we were able to get into this method.
            MutexAuto lock(&m_batchedCmdsLock);
            if (m_stalled)
            {
                BatchedQueueCmdData cmdData = { };
                cmdData.command    = BatchedQueueCmd::Delay;
                cmdData.delay.time = delay;

                result = m_batchedCmds.PushBack(cmdData);
            }
            else
            {
                result = OsDelay(delay, nullptr);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Inserts a delay of a specified amount of time after a vsync on a private screen. Only supported on Timer Queues.
// NOTE: Part of the public IQueue interface.
Result Queue::DelayAfterVsync(
    float                 delayInUs,
    const IPrivateScreen* pScreen)
{
    Result result = Result::ErrorUnavailable;

    if (m_type == QueueTypeTimer)
    {
        // Either execute the delay immediately, or enqueue it for later, depending on whether or not we are stalled.
        if (m_stalled == false)
        {
            result = OsDelay(delayInUs, pScreen);
        }
        else
        {
            // After taking the lock, check again to see if we're stalled. The original check which brought us down
            // this path didn't take the lock beforehand, so its possible that another thread released this Queue
            // from the stalled state before we were able to get into this method.
            MutexAuto lock(&m_batchedCmdsLock);
            if (m_stalled == false)
            {
                result = OsDelay(delayInUs, pScreen);
            }
            else
            {
                // NOTE: Currently there shouldn't be a use case that queue is blocked as external semaphore is used to
                // synchronize submissions in DX and timer queue delays in Mantle, thus application is responsible for
                // correct pairing. Even in case the queue is stalled (in future), we don't want to queue a delay-after-
                // vsync but simply returns an error code to the application.
                PAL_ALERT_ALWAYS();
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Associates the given fence with the last submit before processing more commands on this queue.
// NOTE: Part of the public IQueue interface.
Result Queue::AssociateFenceWithLastSubmit(
    IFence* pFence)
{
    Result result = Result::ErrorInvalidPointer;

    if (pFence != nullptr)
    {
        auto*const pCoreFence = static_cast<Fence*>(pFence);

        // Associate fence with this queue's submission context.
        pCoreFence->AssociateWithContext(m_pSubmissionContext);

        // Either associate the fence timestamp immediately or later, depending on whether or not we are stalled.
        if (m_stalled == false)
        {
            result = DoAssociateFenceWithLastSubmit(pCoreFence);
        }
        else
        {
            // After taking the lock, check again to see if we're stalled. The original check which brought us down
            // this path didn't take the lock beforehand, so its possible that another thread released this Queue
            // from the stalled state before we were able to get into this method.
            MutexAuto lock(&m_batchedCmdsLock);

            if (m_stalled)
            {
                BatchedQueueCmdData cmdData = { };
                cmdData.command               = BatchedQueueCmd::AssociateFenceWithLastSubmit;
                cmdData.associateFence.pFence = pCoreFence;

                result = m_batchedCmds.PushBack(cmdData);
            }
            else
            {
                result = DoAssociateFenceWithLastSubmit(pCoreFence);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// This must be called right after initialization to allow the queue to perform any initialization work which
// requires a fully initialized queue.
Result Queue::LateInit()
{
    Result result = Result::Success;

    // We won't have a dummy command buffer available if we're on a timer queue so we need to check first.
    if (m_pDummyCmdBuffer != nullptr)
    {
        // If ProcessInitialSubmit returns Success, we need to perform a dummy submit with special preambles
        // to initialize the queue. Otherwise, it's not required for this queue.
        InternalSubmitInfo internalSubmitInfo = {};
        if (m_pQueueContext->ProcessInitialSubmit(&internalSubmitInfo) == Result::Success)
        {
            ICmdBuffer*const pCmdBuffer = m_pDummyCmdBuffer;
            SubmitInfo submitInfo       = {};
            submitInfo.cmdBufferCount   = 1;
            submitInfo.ppCmdBuffers     = &pCmdBuffer;

            if (m_ifhMode == IfhModeDisabled)
            {
                m_pDummyCmdBuffer->IncrementSubmitCount();
            }

            if (result == Result::Success)
            {
                result = OsSubmit(submitInfo, internalSubmitInfo);
            }
        }
    }

    if (result == Result::Success)
    {
        result = m_pDevice->AddQueue(this);
    }

    if ((result == Result::Success) && (m_pEngine != nullptr))
    {
        m_pEngine->AddQueue(&m_engineMembershipNode);
    }

    return result;
}

// =====================================================================================================================
// Used to notify this Queue that it has been released by one of the Semaphores that it has been stalled by. If this
// Queue is no longer stalled by any Semaphores, then this will start executing any commands batched on this Queue.
//
// NOTE: This method is invoked whenever a QueueSemaphore which was blocking this Queue becomes signaled, and needs
// "wake up" the blocked Queue. Since the blocking Semaphore can be signaled on a separate thread from threads which
// are batching-up more Queue commands, a race condition can exist while accessing the list of batched-up commands.
// (which is why we need m_batchedCmdsLock).
Result Queue::ReleaseFromStalledState()
{
    Result result = Result::Success;

    bool stalledAgain = false; // It is possible for one of the batched-up commands to be a Semaphore wait which
                               // may cause this Queue to become stalled once more.

    MutexAuto lock(&m_batchedCmdsLock);

    // Execute all of the batched-up commands as long as we don't become stalled again and don't encounter an error.
    while ((m_batchedCmds.NumElements() > 0) && (stalledAgain == false) && (result == Result::Success))
    {
        BatchedQueueCmdData cmdData = { };

        result = m_batchedCmds.PopFront(&cmdData);
        PAL_ASSERT(result == Result::Success);

        switch (cmdData.command)
        {
        case BatchedQueueCmd::Submit:
            result = OsSubmit(cmdData.submit.submitInfo, cmdData.submit.internalSubmitInfo);

            // Once we've executed the submission, we need to free the submission's dynamic arrays. They are all stored
            // in the same memory allocation which was saved in pDynamicMem for convenience.
            PAL_SAFE_FREE(cmdData.submit.pDynamicMem, m_pDevice->GetPlatform());

            // Decrement this count to permit WaitIdle to query the status of the queue's submissions.
            PAL_ASSERT(m_batchedSubmissionCount > 0);
            AtomicDecrement(&m_batchedSubmissionCount);
            break;

        case BatchedQueueCmd::SignalSemaphore:
            result = static_cast<QueueSemaphore*>(cmdData.semaphore.pSemaphore)->Signal(this, cmdData.semaphore.value);
            break;

        case BatchedQueueCmd::WaitSemaphore:
            result = static_cast<QueueSemaphore*>(cmdData.semaphore.pSemaphore)->Wait(this, cmdData.semaphore.value,
                                                                                      &stalledAgain);
            break;

        case BatchedQueueCmd::PresentDirect:
            result = OsPresentDirect(cmdData.presentDirect.info);
            break;

        case BatchedQueueCmd::Delay:
            PAL_ASSERT(m_type == QueueTypeTimer);
            result = OsDelay(cmdData.delay.time, nullptr);
            break;

        case BatchedQueueCmd::AssociateFenceWithLastSubmit:
            result = DoAssociateFenceWithLastSubmit(cmdData.associateFence.pFence);
            break;

        }
    }

    // Update our stalled status: either we've completely drained all batched-up commands and are not stalled, or
    // one of the batched-up commands caused this Queue to become stalled again.
    m_stalled = stalledAgain;

    return result;
}

// =====================================================================================================================
// Validates that the inputs to a Submit() call are legal according to the conditions defined in palQueue.h.
Result Queue::ValidateSubmit(
    const SubmitInfo& submitInfo
    ) const
{
    Result result = Result::Success;

    if (m_type == QueueTypeTimer)
    {
        result = Result::ErrorUnavailable;
    }
    else if (((submitInfo.cmdBufferCount > 0)       && (submitInfo.ppCmdBuffers      == nullptr)) ||
             ((submitInfo.gpuMemRefCount > 0)       && (submitInfo.pGpuMemoryRefs    == nullptr)) ||
             ((submitInfo.doppRefCount > 0)         && (submitInfo.pDoppRefs         == nullptr)) ||
             ((submitInfo.blockIfFlippingCount > 0) && (submitInfo.ppBlockIfFlipping == nullptr)))
    {
        result = Result::ErrorInvalidPointer;
    }
    else if ((submitInfo.blockIfFlippingCount > MaxBlockIfFlippingCount) ||
             ((submitInfo.blockIfFlippingCount > 0) &&
              (m_pDevice->GetPlatform()->GetProperties().supportBlockIfFlipping == 0)))
    {
        result = Result::ErrorInvalidValue;
    }
    else
    {
        for (uint32 idx = 0; idx < submitInfo.cmdBufferCount; ++idx)
        {
            const auto*const pCmdBuffer = static_cast<CmdBuffer*>(submitInfo.ppCmdBuffers[idx]);

            if (pCmdBuffer == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
            else if (pCmdBuffer->RecordState() != CmdBufferRecordState::Executable)
            {
                result = Result::ErrorIncompleteCommandBuffer;
                break;
            }
            else if (pCmdBuffer->GetQueueType() != m_type)
            {
                result = Result::ErrorIncompatibleQueue;
                break;
            }

            PAL_ASSERT(pCmdBuffer->IsNested() == false);
        }
    }

    if (result == Result::Success)
    {
        for (uint32 idx = 0; idx < submitInfo.gpuMemRefCount; ++idx)
        {
            if (submitInfo.pGpuMemoryRefs[idx].pGpuMemory == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        for (uint32 idx = 0; idx < submitInfo.doppRefCount; ++idx)
        {
            if (submitInfo.pDoppRefs[idx].pGpuMemory == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
        }
    }

    if (result == Result::Success)
    {
        for (uint32 idx = 0; idx < submitInfo.blockIfFlippingCount; ++idx)
        {
            if (submitInfo.ppBlockIfFlipping[idx] == nullptr)
            {
                result = Result::ErrorInvalidPointer;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Enqueues a command buffer submission for later execution, once this Queue is no longer blocked by any Semaphores.
Result Queue::EnqueueSubmit(
    const SubmitInfo&         submitInfo,
    const InternalSubmitInfo& internalSubmitInfo)
{
    Result result = Result::Success;

    // After taking the lock, check again to see if we're stalled. The original check which brought us down this path
    // didn't take the lock beforehand, so its possible that another thread released this Queue from the stalled state
    // before we were able to get into this method.
    MutexAuto lock(&m_batchedCmdsLock);
    if (m_stalled)
    {
        BatchedQueueCmdData cmdData;
        cmdData.command                   = BatchedQueueCmd::Submit;
        cmdData.submit.submitInfo         = submitInfo;
        cmdData.submit.internalSubmitInfo = internalSubmitInfo;
        cmdData.submit.pDynamicMem        = nullptr;

        // The submitInfo structure we are batching-up needs to have its own copies of the command buffer and memory
        // reference lists, because there's no guarantee those user arrays will remain valid once we become unstalled.
        const bool   hasCmdBufInfo       = ((submitInfo.pCmdBufInfoList != nullptr) && (submitInfo.cmdBufferCount > 0));
        const size_t cmdBufListBytes     = (sizeof(ICmdBuffer*)  * submitInfo.cmdBufferCount);
        const size_t memRefListBytes     = (sizeof(GpuMemoryRef) * submitInfo.gpuMemRefCount);
        const size_t blkIfFlipBytes      = (sizeof(IGpuMemory*)  * submitInfo.blockIfFlippingCount);
        const size_t cmdBufInfoListBytes = hasCmdBufInfo ? (sizeof(CmdBufInfo) * submitInfo.cmdBufferCount) : 0;
        const size_t doppRefListBytes    = (sizeof(DoppRef) * submitInfo.doppRefCount);
        const size_t totalBytes          = cmdBufListBytes + memRefListBytes + doppRefListBytes +
                                            blkIfFlipBytes + cmdBufInfoListBytes;

        if (totalBytes > 0)
        {
            cmdData.submit.pDynamicMem = PAL_MALLOC(totalBytes, m_pDevice->GetPlatform(), AllocInternal);

            if (cmdData.submit.pDynamicMem == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                void* pNextBuffer = cmdData.submit.pDynamicMem;

                if (submitInfo.cmdBufferCount > 0)
                {
                    auto**const ppBatchedCmdBuffers = reinterpret_cast<ICmdBuffer**>(pNextBuffer);
                    memcpy(ppBatchedCmdBuffers, submitInfo.ppCmdBuffers, cmdBufListBytes);

                    cmdData.submit.submitInfo.ppCmdBuffers = ppBatchedCmdBuffers;
                    pNextBuffer                            = VoidPtrInc(pNextBuffer, cmdBufListBytes);
                }

                if (submitInfo.gpuMemRefCount > 0)
                {
                    auto*const pBatchedGpuMemoryRefs = static_cast<GpuMemoryRef*>(pNextBuffer);
                    memcpy(pBatchedGpuMemoryRefs, submitInfo.pGpuMemoryRefs, memRefListBytes);

                    cmdData.submit.submitInfo.pGpuMemoryRefs = pBatchedGpuMemoryRefs;
                    pNextBuffer                              = VoidPtrInc(pNextBuffer, memRefListBytes);
                }

                if (submitInfo.doppRefCount > 0)
                {
                    auto*const pBatchedDoppRefs = static_cast<DoppRef*>(pNextBuffer);
                    memcpy(pBatchedDoppRefs, submitInfo.pDoppRefs, doppRefListBytes);

                    cmdData.submit.submitInfo.pDoppRefs = pBatchedDoppRefs;
                    pNextBuffer                         = VoidPtrInc(pNextBuffer, doppRefListBytes);
                }

                if (submitInfo.blockIfFlippingCount > 0)
                {
                    auto**const ppBatchedBlockIfFlipping = static_cast<IGpuMemory**>(pNextBuffer);
                    memcpy(ppBatchedBlockIfFlipping, submitInfo.ppBlockIfFlipping, blkIfFlipBytes);

                    cmdData.submit.submitInfo.ppBlockIfFlipping = ppBatchedBlockIfFlipping;
                    pNextBuffer                                 = VoidPtrInc(pNextBuffer, blkIfFlipBytes);
                }

                if (hasCmdBufInfo)
                {
                    auto*const pBatchedCmdBufInfoList = static_cast<CmdBufInfo*>(pNextBuffer);
                    memcpy(pBatchedCmdBufInfoList, submitInfo.pCmdBufInfoList, cmdBufInfoListBytes);

                    cmdData.submit.submitInfo.pCmdBufInfoList = pBatchedCmdBufInfoList;
                }
            }
        }

        if (result == Result::Success)
        {
            result = m_batchedCmds.PushBack(cmdData);

            if (result == Result::Success)
            {
                // We must track the number of batched submissions to make WaitIdle spin until all submissions have been
                // submitted to the OS layer.
                AtomicIncrement(&m_batchedSubmissionCount);
            }
            else
            {
                PAL_SAFE_FREE(cmdData.submit.pDynamicMem, m_pDevice->GetPlatform());
            }
        }
    }
    else
    {
        // We had a false-positive and aren't really stalled. Submit immediately.
        result = OsSubmit(submitInfo, internalSubmitInfo);
    }

    return result;
}

// =====================================================================================================================
Result Queue::QueryAllocationInfo(
    size_t*                    pNumEntries,
    GpuMemSubAllocInfo* const  pAllocInfoList)
{
    Result result = Result::Success;

    if (pNumEntries != nullptr)
    {
        *pNumEntries = 0;
    }
    else
    {
        result = Result::ErrorInvalidPointer;
    }

    return result;
}

// =====================================================================================================================
// Performs a queue submit with zero command buffer count and the fence provided.
Result Queue::SubmitFence(
    IFence* pFence)
{
    SubmitInfo submitInfo = {};
    submitInfo.pFence = pFence;

    return SubmitInternal(submitInfo, false);
}

// =====================================================================================================================
// Increment frame count and move to next frame
void Queue::IncFrameCount()
{
    m_pDevice->IncFrameCount();
}

// =====================================================================================================================
// Applies developer overlay and other postprocessing to be done prior to presenting an image.
Result Queue::SubmitPostprocessCmdBuffer(
    const Image& image)
{
    Result result = Result::Success;

    const bool shouldPostprocess =
        (Queue::SupportsComputeShader(m_type) &&
         (
          m_pDevice->GetPlatform()->ShowDevDriverOverlay()));

    if (shouldPostprocess)
    {
        const auto& presentedImage =
            image;

        TrackedCmdBuffer* pTrackedCmdBuffer = nullptr;
        result = CreateTrackedCmdBuffer(&pTrackedCmdBuffer);

        if (result == Result::Success)
        {

            // If developer mode is enabled, we need to apply the developer overlay.
            if (m_pDevice->GetPlatform()->ShowDevDriverOverlay())
            {
                m_pDevice->ApplyDevOverlay(presentedImage, pTrackedCmdBuffer->pCmdBuffer);
            }

            result = SubmitTrackedCmdBuffer(pTrackedCmdBuffer, presentedImage.GetBoundGpuMemory().Memory());
        }
    }

    return result;
}

// =====================================================================================================================
// Check whether the present mode is supported by the queue.
bool Queue::IsPresentModeSupported(
    PresentMode presentMode
    ) const
{
    const uint32 supportedPresentModes = m_pDevice->QueueProperties().perQueue[m_type].supportedDirectPresentModes;
    const uint32 presentModeFlag       = (presentMode == PresentMode::Fullscreen) ? SupportFullscreenPresent :
                                         (m_flags.windowedPriorBlit == 1)         ? SupportWindowedPriorBlitPresent :
                                                                                    SupportWindowedPresent;
    return TestAnyFlagSet(supportedPresentModes, presentModeFlag);
}

// =====================================================================================================================
// Perform a dummy submission on this queue.
Result Queue::DummySubmit()
{
    ICmdBuffer*const pCmdBuffer = DummyCmdBuffer();
    SubmitInfo submitInfo       = {};
    submitInfo.cmdBufferCount   = 1;
    submitInfo.ppCmdBuffers     = &pCmdBuffer;
    return SubmitInternal(submitInfo, false);
}
} // Pal
