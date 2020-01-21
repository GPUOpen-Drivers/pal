/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
constexpr uint32 MaxPreambleCmdStreams  = 3;
constexpr uint32 MaxPostambleCmdStreams = 2;

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

    MgpuSlsInfo mgpuSlsInfo;

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
};

// Defines the data payloads for each type of batched-up Queue command.
struct BatchedQueueCmdData
{
    BatchedQueueCmd command;

    union
    {
        struct
        {
            SubmitInfo         submitInfo;
            InternalSubmitInfo internalSubmitInfo;
            void*              pDynamicMem;
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
    virtual ~Queue() {}

    virtual Result Init(void* pContextPlacementAddr);

    // NOTE: Part of the public IQueue interface.
    virtual Result Submit(const SubmitInfo& submitInfo) override
        { return SubmitInternal(submitInfo, false); }

    // A special version of Submit with PAL-internal arguments.
    Result SubmitInternal(const SubmitInfo& submitInfo, bool postBatching);

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
    virtual Result AssociateFenceWithLastSubmit(IFence* pFence) override;

    // NOTE: Part of the public IQueue interface.
    virtual void SetExecutionPriority(QueuePriority priority) override
        { m_queuePriority = priority; }

    // NOTE: Part of the public IQueue interface.
    virtual Result QueryAllocationInfo(size_t* pNumEntries, GpuMemSubAllocInfo* const pAllocInfoList) override;

    // NOTE: Part of the public IQueue interface.
    virtual QueueType Type() const override { return m_type; }

    // NOTE: Part of the public IQueue interface.
    virtual EngineType GetEngineType() const override { return m_engineType; }

    // NOTE: Part of the public IQueue interface.
    virtual Result QueryKernelContextInfo(KernelContextInfo* pKernelContextInfo) const override
        { return Result::ErrorUnavailable; }

    // NOTE: Part of the public IDestroyable interface.
    virtual void Destroy() override;

    // This must be called right after initialization to allow the queue to perform any initialization work which
    // requires a fully initialized queue.
    Result LateInit();

    Result ReleaseFromStalledState();

    uint32        EngineId() const { return m_engineId; }
    QueuePriority Priority() const { return m_queuePriority; }
    CmdBuffer*    DummyCmdBuffer() const { return m_pDummyCmdBuffer; }
    Result        DummySubmit(bool postBatching);

    bool UsesDispatchTunneling()      const { return (m_flags.dispatchTunneling      != 0); }
    bool IsWindowedPriorBlit()        const { return (m_flags.windowedPriorBlit      != 0); }
    bool UsesPhysicalModeSubmission() const { return (m_flags.physicalModeSubmission != 0); }
    bool IsPreemptionSupported()      const { return (m_flags.midCmdBufPreemption    != 0); }

    uint32 PersistentCeRamOffset() const { return m_persistentCeRamOffset; }
    uint32 PersistentCeRamSize()   const { return m_persistentCeRamSize; }

    IQueueSemaphore* WaitingSemaphore() const { return m_pWaitingSemaphore; }
    void SetWaitingSemaphore(IQueueSemaphore* pQueueSemaphore) { m_pWaitingSemaphore = pQueueSemaphore; }

    Util::IntrusiveListNode<Queue>* DeviceMembershipNode() { return &m_deviceMembershipNode; }

    Device*const GetDevice() { return m_pDevice; }

    bool IsStalled() const { return m_stalled; }

    void IncFrameCount();

    static bool SupportsComputeShader(QueueType queueType)
        { return (queueType == QueueTypeUniversal) || (queueType == QueueTypeCompute); }

    bool IsPresentModeSupported(PresentMode presentMode) const;

    // Performs OS-specific Queue submission behavior.
    virtual Result OsSubmit(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo) = 0;

protected:
    Queue(Device* pDevice, const QueueCreateInfo& createInfo);

    // Performs OS-specific Queue wait-idle behavior.
    virtual Result OsWaitIdle() = 0;

    // Performs OS-specific Queue delay behavior. Only supported on Timer Queues.
    virtual Result OsDelay(float delay, const IPrivateScreen* pPrivateScreen) = 0;

    // Performs OS-specific Queue direct presentation behavior.
    virtual Result OsPresentDirect(const PresentDirectInfo& presentInfo) = 0;

    virtual Result UpdateAppPowerProfile(const wchar_t* pFileName, const wchar_t* pPathName) override
        { return Result::Unsupported; }

    Result SubmitFence(IFence* pFence);

    bool IsMinClockRequired() const { return false; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 518
    // Applies developer overlay and other postprocessing to be done prior to presenting an image.
    Result SubmitPostprocessCmdBuffer(const Image& image);
#endif

    virtual Result DoAssociateFenceWithLastSubmit(Fence* pFence) = 0;

    Device*const        m_pDevice;
    const QueueType     m_type;
    const EngineType    m_engineType;
    const uint32        m_engineId;
    const SubmitOptMode m_submitOptMode;

    // Each Queue is associated with an engine and a submission context. Note that the submission context is created
    // by a subclass but owned by this class.
    Engine*            m_pEngine;
    SubmissionContext* m_pSubmissionContext;

    // A dummy command buffer for any situation where we need to submit something without doing anything. Each specific
    // OS backend has different situations when this command buffer is needed. For example, most OS backends need to
    // submit something to signal fences so the dummy command buffer is used when the client provides none.
    //
    // In theory we could save some space by creating one dummy command buffer of each type in the device but for now
    // we create one in each queue for simplicity.
    CmdBuffer*         m_pDummyCmdBuffer;

    union
    {
        struct
        {
            uint32  physicalModeSubmission :  1;
            uint32  midCmdBufPreemption    :  1;
            uint32  windowedPriorBlit      :  1;
            uint32  placeholder0           :  1;
            uint32  placeholder1           :  1;
            uint32  dispatchTunneling      :  1;
            uint32  reserved               : 26;
        };
        uint32  u32All;
    }  m_flags; // Flags describing properties of this Queue.

    IfhMode m_ifhMode;       // Cache IFH mode for this Queue to avoid looking up the IFH mode and IFH GPU mask
                             // settings on every submit.

    uint32  m_numReservedCu; // The number of reserved CUs for RT queue

private:
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 518
    // A command buffer and a fence to track its submission state wrapped into one object.
    struct TrackedCmdBuffer
    {
        CmdBuffer* pCmdBuffer;
        Fence*     pFence;
    };
#endif

    Result ValidateSubmit(const SubmitInfo& submitInfo) const;
    Result EnqueueSubmit(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo);

    Result WaitQueueSemaphoreNoChecks(
        IQueueSemaphore* pQueueSemaphore,
        volatile bool*   pIsStalled);

#if PAL_ENABLE_PRINTS_ASSERTS
    void DumpCmdToFile(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo);
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 555
#if PAL_ENABLE_PRINTS_ASSERTS
    bool IsCmdDumpEnabled() const;
    Result OpenCommandDumpFile(
        const SubmitInfo&           submitInfo,
        const InternalSubmitInfo&   internalSubmitInfo,
        Util::File*                 logFile);
#endif // PAL_ENABLE_PRINTS_ASSERTS

    void DumpCmdBuffers(
        const SubmitInfo&         submitInfo,
        const InternalSubmitInfo& internalSubmitInfo) const;
    void DumpCmdStream(
        const CmdBufferDumpDesc& cmdBufferDesc,
        const CmdStream*         pCmdStream,
        CmdDumpCallback          pCmdDumpCallback,
        void*                    pUserData) const;
#endif // PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 555

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 518
    Result CreateTrackedCmdBuffer(TrackedCmdBuffer** ppTrackedCmdBuffer);
    void   DestroyTrackedCmdBuffer(TrackedCmdBuffer* pTrackedCmdBuffer);
    Result SubmitTrackedCmdBuffer(TrackedCmdBuffer* pTrackedCmdBuffer, const GpuMemory* pWrittenPrimary);
#endif

    // Each Queue needs a QueueContext to apply any hardware-specific pre- or post-processing before Submit().
    QueueContext*     m_pQueueContext;

    // Tracks whether or not this Queue is stalled by a Queue Semaphore, and if so, the Semaphore which is blocking
    // this Queue.
    volatile bool     m_stalled;
    IQueueSemaphore*  m_pWaitingSemaphore;

    volatile uint32   m_batchedSubmissionCount; // How many batched submissions will be sent to OS layer later on.

    Util::Deque<BatchedQueueCmdData, Platform>  m_batchedCmds;
    Util::Mutex                                 m_batchedCmdsLock;

    // Each queue must register itself with its device and engine so that they can manage their internal lists.
    Util::IntrusiveListNode<Queue>              m_deviceMembershipNode;
    Util::IntrusiveListNode<Queue>              m_engineMembershipNode;
    uint32           m_lastFrameCnt;       // Most recent frame in which the queue submission occurs
    uint32           m_submitIdPerFrame;   // The Nth queue submission of the frame
    QueuePriority    m_queuePriority;      // The queue priority could be adjusted by calling SetExecutionPriority

    uint32  m_persistentCeRamOffset;       // Byte offset to the beginning of the region of CE RAM whose contents will
                                           // be made persistent across multiple submissions.
    uint32  m_persistentCeRamSize;         // Amount of CE RAM space (in DWORDs) which this Queue will keep persistent
                                           // across multiple submissions.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 518
    // Internal command buffers used by this queue for various postprocess tasks such as render the developer overlay.
    // The least recently used item is at the front.
    typedef Util::Deque<TrackedCmdBuffer*, Platform> TrackedCmdBufferDeque;
    TrackedCmdBufferDeque* m_pTrackedCmdBufferDeque;
#endif

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // Pal
