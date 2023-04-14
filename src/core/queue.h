/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/platform.h"
#include "core/queueSemaphore.h"
#include "palQueue.h"
#include "palDeque.h"
#include "palIntrusiveList.h"
#include "palMutex.h"

namespace Pal
{

class CmdBuffer;
class CmdStream;
class Device;
class Fence;
class Engine;
class Image;
class Platform;
class QueueContext;
class GpuMemory;

// On some hardware layers, particular Queue types may need to bundle several "special" command streams with each
// client submission to guarantee the state of the GPU is consistent across multiple submissions. These constants
// are the maximum number of command streams allowed for both the submission preamble and submission postamble.
constexpr uint32 MaxPreambleCmdStreams  = 4;
constexpr uint32 MaxPostambleCmdStreams = 2;

// This struct tracks per subQueue info when we do gang submission.
struct SubQueueInfo
{
    // Each subQueue needs a QueueContext to apply any hardware-specific pre- or post-processing before Submit().
    QueueContext*    pQueueContext;
    QueueCreateInfo  createInfo;
    // Each subQueue is associated with an engine.
    Engine*          pEngine;
};

union InternalSubmitFlags
{
    struct
    {
        uint32 isTmzEnabled      : 1;  // Is TMZ protected submission.
        uint32 isDummySubmission : 1;  // Is dummy submission.
        uint32 placeholder1      : 1;
        uint32 reserved          : 29; // reserved.
    };
    uint32 u32All;
};

// Contains internal information describing the submission preamble and postamble for a given QueueContext, and
// additional internal flags.
struct InternalSubmitInfo
{
    // List of command streams needed for the Queue's submission preamble.
    CmdStream*  pPreambleCmdStream[MaxPreambleCmdStreams];
    uint32      numPreambleCmdStreams;      // Number of command streams in the submission preamble.

    // List of command streams needed for the Queue's submission postamble.
    CmdStream*  pPostambleCmdStream[MaxPostambleCmdStreams];
    uint32      numPostambleCmdStreams;     // Number of command streams in the submission postamble.

    // Paging fence value associated with any internal allocations or command streams managed by the QueueContext.
    uint64  pagingFence;

    InternalSubmitFlags flags;

    uint32 stackSizeInDwords;               // Frame stack size for indirect shaders

    // Number of implicit ganged sub-queues (not including the "main" sub-queue).
    uint32 implicitGangedSubQueues;

    // The semaphore arrays are only used by Linux backend to better align with u/k interface
    uint32                  signalSemaphoreCount; // The count of semaphores that have to signal after the submission.
    uint32                  waitSemaphoreCount;   // The count of semaphores that have to wait before the submission.
    uint64*                 pSignalPoints;        // timeline semaphore signal points array.
    uint64*                 pWaitPoints;          // timeline semaphore wait points array.
    IQueueSemaphore**       ppSignalSemaphores;   // Array of semaphores that have to signal after the submission.
    IQueueSemaphore**       ppWaitSemaphores;     // Array of semaphores that have to wait after the submission.
};

// Enumerates the types of Queue commands which could be batched-up if the Queue is stalled on a Semaphore.
enum class BatchedQueueCmd : uint32
{
    Submit = 0,                   // Identifies a Submit() call
    WaitSemaphore,                // Identifies a WaitQueueSemaphore() call
    SignalSemaphore,              // Identifies a SignalQueueSemaphore() call
    PresentDirect,                // Identifies a PresentDirect() call
    Delay,                        // Identifies a Delay() call
    AssociateFenceWithLastSubmit, // Identifies a AssociateFenceWithLastSubmit() call
    RemapVirtualMemoryPages,      // Identifies a RemapVirtualMemoryPages() call
    CopyVirtualMemoryPageMappings,// Identifies a CopyVirtualMemoryPageMappings() call
};

// Defines the data payloads for each type of batched-up Queue command.
struct BatchedQueueCmdData
{
    BatchedQueueCmd command;

    union
    {
        struct
        {
            MultiSubmitInfo           submitInfo;
            const InternalSubmitInfo* pInternalSubmitInfo;
            void*                     pDynamicMem;
        } submit;

        struct
        {
            IQueueSemaphore* pSemaphore;
            uint64           value;
        } semaphore;

        struct
        {
            PresentDirectInfo info;
        } presentDirect;

        struct
        {
            float time;
        } delay;

        struct
        {
            Fence* pFence;
        } associateFence;

        struct
        {
            uint32                         rangeCount;
            VirtualMemoryRemapRange*       pRanges;
            bool                           doNotWait;
            IFence*                        pFence;
        } remapVirtualMemoryPages;

        struct
        {
            uint32                                    rangeCount;
            VirtualMemoryCopyPageMappingsRange*       pRanges;
            bool                                      doNotWait;
        } copyVirtualMemoryPageMappings;

    };
};

// =====================================================================================================================
// A submission context holds queue state and logic that must persist after the queue itself has been destroyed. That
// requires all submission contexts to be internally allocated and referenced counted.
//
// Currently PAL only requires that IFence objects function after the last queue they were submitted on is destroyed
// because some clients' APIs treat all queues as having the same lifetime as devices, even internal queues. As such
// each OS-specific submission context subclass only need to manage the state necessary to query a fence's status and
// wait for that fence to be signaled (even if it never will be signaled).
class SubmissionContext
{
public:
    // Each object that holds a pointer to a context must take and later release a reference. Note that creation
    // implicitly takes a reference. When the final reference is released the context is automatically deleted.
    void TakeReference();
    void ReleaseReference();

    // Queries if a particular fence timestamp has been retired by the GPU.
    virtual bool IsTimestampRetired(uint64 timestamp) const = 0;

    // Returns a pointer to the last timestamp so that the caller can update it.
    uint64* LastTimestampPtr() { return &m_lastTimestamp; }

    uint64 LastTimestamp() const { return m_lastTimestamp; }

protected:
    SubmissionContext(Platform* pPlatform) : m_lastTimestamp(0), m_pPlatform(pPlatform), m_refCount(1) {}
    virtual ~SubmissionContext() {}

    uint64 m_lastTimestamp; // The last fence timestamp which has been submitted to the OS.

private:
    Platform*const  m_pPlatform;
    volatile uint32 m_refCount;

    PAL_DISALLOW_DEFAULT_CTOR(SubmissionContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(SubmissionContext);
};

// =====================================================================================================================
// Represents a queue of work for a particular GPU engine on a device. Work is submitted to a queue through CmdBuffer
// objects, and work can be synchronized between multiple queues using QueueSemaphore objects.
class Queue : public IQueue
{
public:
    virtual ~Queue();

    virtual Result Init(
        const QueueCreateInfo* pCreateInfo,
        void*                  pContextPlacementAddr);

    // NOTE: Part of the public IQueue interface.
    virtual Result Submit(const MultiSubmitInfo& submitInfo) override
        { return SubmitInternal(submitInfo, false); }

    // A special version of Submit with PAL-internal arguments.
    Result SubmitInternal(const MultiSubmitInfo& submitInfo, bool postBatching);

    // Config related submit flags.
    virtual void SubmitConfig(const MultiSubmitInfo& submitInfo, InternalSubmitInfo* pInternalSubmitInfos);

    // NOTE: Part of the public IQueue interface.
    virtual Result WaitIdle() override;

    // NOTE: Part of the public IQueue interface.
    virtual Result SignalQueueSemaphore(IQueueSemaphore* pQueueSemaphore, uint64 value) override
        { return SignalQueueSemaphoreInternal(pQueueSemaphore, value, false); }

    // A special version of SignalQueueSemaphore with PAL-internal arguments.
    Result SignalQueueSemaphoreInternal(IQueueSemaphore* pQueueSemaphore, uint64 value, bool postBatching);

    // NOTE: Part of the public IQueue interface.
    virtual Result WaitQueueSemaphore(IQueueSemaphore* pQueueSemaphore, uint64 value) override
        { return WaitQueueSemaphoreInternal(pQueueSemaphore, value, false); }

    // A special version of WaitQueueSemaphore with PAL-internal arguments.
    Result WaitQueueSemaphoreInternal(IQueueSemaphore* pQueueSemaphore, uint64 value, bool postBatching);

    // NOTE: Part of the public IQueue interface.
    virtual Result PresentDirect(const PresentDirectInfo& presentInfo) override
        { return PresentDirectInternal(presentInfo, true); }

    // A special version of PresentDirect with PAL-internal arguments.
    Result PresentDirectInternal(const PresentDirectInfo& presentInfo, bool isClientPresent);

    // NOTE: Part of the public IQueue interface.
    virtual Result PresentSwapChain(const PresentSwapChainInfo& presentInfo) override;

    // NOTE: Part of the public IQueue interface.
    virtual Result Delay(float delay) override;

    // NOTE: Part of the public IQueue interface.
    virtual Result DelayAfterVsync(float delayInUs, const IPrivateScreen* pScreen) override;

    // NOTE: Part of the public IQueue interface.
    virtual Result RemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRanges,
        bool                           doNotWait,
        IFence*                        pFence) override;

    // NOTE: Part of the public IQueue interface.
    virtual Result CopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) override;

    // NOTE: Part of the public IQueue interface.
    virtual Result AssociateFenceWithLastSubmit(IFence* pFence) override;

    // NOTE: Part of the public IQueue interface.
    virtual void SetExecutionPriority(QueuePriority priority) override
        { m_pQueueInfos[0].createInfo.priority = priority; }

    // NOTE: Part of the public IQueue interface.
    virtual Result QueryAllocationInfo(size_t* pNumEntries, GpuMemSubAllocInfo* const pAllocInfoList) override;

    // NOTE: Part of the public IQueue interface.
    virtual QueueType Type() const override { return m_pQueueInfos[0].createInfo.queueType; }

    // NOTE: Part of the public IQueue interface.
    virtual EngineType GetEngineType() const override { return m_pQueueInfos[0].createInfo.engineType; }

    // NOTE: Part of the public IQueue interface.
    virtual Result QueryKernelContextInfo(KernelContextInfo* pKernelContextInfo) const override
        { return Result::ErrorUnavailable; }

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    // This must be called right after initialization to allow the queue to perform any initialization work which
    // requires a fully initialized queue.
    virtual Result LateInit();

    Result ReleaseFromStalledState(
        QueueSemaphore* pWaitingSemaphore,
        uint64          value);

    virtual uint32 EngineId() const { return m_pQueueInfos[0].createInfo.engineIndex; }
    virtual QueuePriority Priority() const { return m_pQueueInfos[0].createInfo.priority; }
    CmdBuffer*    DummyCmdBuffer() const { return m_pDummyCmdBuffer; }
    Result        DummySubmit(bool postBatching);

    virtual bool UsesDispatchTunneling() const { return (m_pQueueInfos[0].createInfo.dispatchTunneling != 0); }
    virtual bool IsWindowedPriorBlit()   const { return (m_pQueueInfos[0].createInfo.windowedPriorBlit != 0); }
    bool UsesPhysicalModeSubmission() const;
    bool IsPreemptionSupported()      const;

    virtual uint32 PersistentCeRamOffset() const { return m_pQueueInfos[0].createInfo.persistentCeRamOffset; }
    virtual uint32 PersistentCeRamSize()   const { return m_pQueueInfos[0].createInfo.persistentCeRamSize; }

    Util::IntrusiveListNode<Queue>* DeviceMembershipNode() { return &m_deviceMembershipNode; }

    Device* GetDevice() const { return m_pDevice; }

    bool IsStalled() const { return m_stalled; }

    void IncFrameCount();

    static bool SupportsComputeShader(QueueType queueType)
        { return (queueType == QueueTypeUniversal) || (queueType == QueueTypeCompute); }

    virtual bool IsPresentModeSupported(PresentMode presentMode) const;

    SubmissionContext* GetSubmissionContext() const { return m_pSubmissionContext; }

    // Performs OS-specific Queue submission behavior.
    virtual Result OsSubmit(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo* pInternalSubmitInfos) = 0;

protected:
    Queue(uint32 queueCount, Device* pDevice, const QueueCreateInfo* pCreateInfo);

    // Performs OS-specific Queue wait-idle behavior.
    virtual Result OsWaitIdle() = 0;

    // Performs OS-specific Queue delay behavior. Only supported on Timer Queues.
    virtual Result OsDelay(float delay, const IPrivateScreen* pPrivateScreen) = 0;

    // Performs OS-specific Queue direct presentation behavior.
    virtual Result OsPresentDirect(const PresentDirectInfo& presentInfo) = 0;

    // Performs OS-specific Queue RemapVirtualMemoryPages behavior.
    virtual Result OsRemapVirtualMemoryPages(
        uint32                         rangeCount,
        const VirtualMemoryRemapRange* pRanges,
        bool                           doNotWait,
        IFence*                        pFence) = 0;;

    // Performs OS-specific Queue CopyVirtualMemoryPageMappings behavior.
    virtual Result OsCopyVirtualMemoryPageMappings(
        uint32                                    rangeCount,
        const VirtualMemoryCopyPageMappingsRange* pRanges,
        bool                                      doNotWait) = 0;

    virtual Result UpdateAppPowerProfile(const wchar_t* pFileName, const wchar_t* pPathName) override
        { return Result::Unsupported; }

    Result SubmitFence(IFence* pFence);

    bool IsMinClockRequired() const { return false; }

    virtual Result DoAssociateFenceWithLastSubmit(Fence* pFence) = 0;

    Result GfxIpWaitPipelineUploading(const MultiSubmitInfo& submitInfo);

    Device*const        m_pDevice;

    // Each Queue is associated with an engine and a submission context. Note that the submission context is created
    // by a subclass but owned by this class.
    SubmissionContext*  m_pSubmissionContext;

    // A dummy command buffer for any situation where we need to submit something without doing anything. Each specific
    // OS backend has different situations when this command buffer is needed. For example, most OS backends need to
    // submit something to signal fences so the dummy command buffer is used when the client provides none.
    //
    // In theory we could save some space by creating one dummy command buffer of each type in the device but for now
    // we create one in each queue for simplicity.
    CmdBuffer*          m_pDummyCmdBuffer;

    IfhMode m_ifhMode;       // Cache IFH mode for this Queue to avoid looking up the IFH mode and IFH GPU mask
                             // settings on every submit.

    SubQueueInfo* m_pQueueInfos; // m_pQueueInfos struct tracks per subQueue info when we do gang submission.
    const uint32  m_queueCount;

private:

    virtual Result ValidateSubmit(const MultiSubmitInfo& submitInfo) const;

    Result EnqueueSubmit(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo* pInternalSubmitInfo);

    Result WaitQueueSemaphoreNoChecks(
        IQueueSemaphore* pQueueSemaphore,
        volatile bool*   pIsStalled);

    bool IsCmdDumpEnabled() const;
    Result OpenCommandDumpFile(
        const MultiSubmitInfo&      submitInfo,
        const InternalSubmitInfo&   internalSubmitInfo,
        Util::File*                 logFile);

    void DumpCmdBuffers(
        const MultiSubmitInfo&    submitInfo,
        const InternalSubmitInfo& internalSubmitInfo) const;
    void DumpCmdStream(
        const CmdBufferDumpDesc& cmdBufferDesc,
        const CmdStream*         pCmdStream,
        CmdDumpCallback          pCmdDumpCallback,
        void*                    pUserData) const;

    // Tracks whether or not this Queue is stalled by a Queue Semaphore, and if so, the Semaphore which is blocking
    // this Queue.
    volatile bool     m_stalled;

    volatile uint32   m_batchedSubmissionCount; // How many batched submissions will be sent to OS layer later on.

    Util::Deque<BatchedQueueCmdData, Platform>  m_batchedCmds;
    Util::Mutex                                 m_batchedCmdsLock;

    // Each queue must register itself with its device and engine so that they can manage their internal lists.
    Util::IntrusiveListNode<Queue>              m_deviceMembershipNode;

    uint32           m_lastFrameCnt;       // Most recent frame in which the queue submission occurs
    uint32           m_submitIdPerFrame;   // The Nth queue submission of the frame

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // Pal
