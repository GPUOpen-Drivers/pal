/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palGpaSession.h
 * @brief PAL GPU utility GpaSession class.
 ***********************************************************************************************************************
 */

#pragma once

#include "palDeque.h"
#include "palDevice.h"
#include "palGpuUtil.h"
#include "palHashSet.h"
#include "palMutex.h"
#include "palPipeline.h"
#include "palVector.h"
#include "palPlatform.h"
#include "palSysMemory.h"
#include "palMemTrackerImpl.h"

// Forward declarations.
namespace Pal
{
    class  ICmdAllocator;
    class  ICmdBuffer;
    class  IDevice;
    class  IGpuEvent;
    class  IGpuMemory;
    class  IPerfExperiment;
    class  IQueue;
    class  IQueueSemaphore;
    struct GlobalCounterLayout;
    struct SubmitInfo;
    struct ThreadTraceLayout;
    enum   HwPipePoint : uint32;
}
struct SqttFileChunkCpuInfo;
struct SqttFileChunkAsicInfo;

namespace GpuUtil
{
// Sample id initialization value.
constexpr Pal::uint32 InvalidSampleId = 0xFFFFFFFF;

/// The available states of GpaSession
enum class GpaSessionState : Pal::uint32
{
    Reset      = 0,
    Building   = 1,
    Complete   = 2,
    Ready      = 3,
};

/// Specifies basic type of sample to perfom - either a normal set of "global" perf counters, or a trace consisting
/// of SQ thread trace and/or streaming performance counters.
enum class GpaSampleType : Pal::uint32
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 355
    Cumulative = 0x0,  ///< One 64-bit result will be returned per performance counter representing the cumulative delta
                       ///  for that counter over the sample period.  Cumulative samples must begin and end in the same
                       ///  command buffer.
    Trace      = 0x1,  ///< A GPU memory buffer will be filled with hw-specific SQ thread trace and/or streaming
                       ///  performance counter data.  Trace samples may span multiple command buffers.
    Timing     = 0x2,  ///< Two 64-bit results will be recorded in beginTs and endTs to gather timestamp data.
    None       = 0xf,  ///< No profile will be done.
#else
    None       = 0x0,  ///< No profile will be done.
    Cumulative = 0x1,  ///< One 64-bit result will be returned per performance counter representing the cumulative delta
                       ///  for that counter over the sample period.  Cumulative samples must begin and end in the same
                       ///  command buffer.
    Trace      = 0x2,  ///< A GPU memory buffer will be filled with hw-specific SQ thread trace and/or streaming
                       ///  performance counter data.  Trace samples may span multiple command buffers.
    Timing     = 0x3,  ///< Two 64-bit results will be recorded in beginTs and endTs to gather timestamp data.
    Query      = 0x4,  ///< A set of 11 pipeline stats will be collected.
    Count
#endif
};

/// Specifies a specific performance counter to be sampled with GpaSession::BeginSample() and GpaSession::EndSample().
///
/// This identifies a specific counter in a particular HW block instance, e.g., TCC instance 3 counter #19.  It is up
/// to the client to know the meaning of a particular counter, e.g., TCC #19 is TCC_PERF_SEL_MISS on Fiji.  Eventually,
/// PAL may want to support certain counters without the client needing HW-specific knowledge (i.e., select an enum
/// called L2MissRate from PAL rather than needing to know that counter is TCC #19 on Fiji), but GPA currently works in
/// this low-level mode with other drivers, and wants to keep the flexibility.
struct PerfCounterId
{
    Pal::GpuBlock block;    ///< Which GPU block to reference (e.g., CB, DB, TCC).
    Pal::uint32   instance; ///< Which instance of the specified GPU block to sample.  E.g., Tahiti has 12 TCC blocks
                            ///  (this number is returned per-block in the @ref Pal::GpuBlockPerfProperties structure).
                            ///  There is no shortcut to get results for all instances of block in the whole chip, the
                            ///  client must explicitly sample each instance and sum the results.
    Pal::uint32   eventId;  ///< Counter ID to sample.  Note that the meaning of a particular eventId for a block can
                            ///  change between chips.
};

/// Defines a set of flags for a particular gpa session.
union GpaSessionFlags
{
    struct
    {
        /// Enables timing of queue operations via Timed* functions.
        Pal::uint32 enableQueueTiming        : 1;

        /// Enables sample updates via the UpdateSampleTraceParams function.
        Pal::uint32 enableSampleUpdates      : 1;

        /// Reserved for future use.
        Pal::uint32 reserved                 : 30;
    };

    /// Flags packed as 32-bit uint.
    Pal::uint32 u32All;
};

/// Specifies options that direct the gpa session behavior.
struct GpaSessionBeginInfo
{
    /// Gpa Session flags used to control behavior.
    GpaSessionFlags flags;
};

/// Input structure for CmdBeginGpuProfilerSample.
///
/// Defines a set of global performance counters and/or SQ thread trace data to be sampled.
struct GpaSampleConfig
{
    /// Selects what type of data should be gathered for this sample.  This can either be _cumulative_ to gather
    /// simple deltas for the specified set of perf counters over the sample period, or it can be _trace_ to generate
    /// a blob of RGP-formatted data containing SQ thread trace and/or streaming performance monitor data.
    GpaSampleType type;

    union
    {
        struct
        {
            Pal::uint32 sampleInternalOperations      :  1;  ///< Include BLTs and internal driver operations in the
                                                             ///  results.
            Pal::uint32 cacheFlushOnCounterCollection :  1;  ///< Insert cache flush and invalidate events before and
                                                             ///  after every sample.
            Pal::uint32 sqShaderMask                  :  1;  ///< Whether or not the contents of sqShaderMask are valid.
            Pal::uint32 reserved                      : 29;  ///< Reserved for future use.
        };
        Pal::uint32 u32All;                                  ///< Bit flags packed as uint32.
    } flags;                                                 ///< Bit flags controlling sample operation for all sample
                                                             ///  types.

    Pal::PerfExperimentShaderFlags sqShaderMask;             ///< Indicates which hardware shader stages should be
                                                             ///< sampled. Only valid if flags.sqShaderMask is set to 1.

    struct
    {
        /// Number of entries in pIds.
        Pal::uint32 numCounters;

        /// List of performance counters to be gathered for a sample.  If the sample type is _cumulative_ this will
        /// result in "global" perf counters being sampled at the beginning of the sample period; if the sample type
        /// is _trace_ this will result in SPM data being added to the sample's resulting RGP blob.
        ///
        /// Note that it is up to the client to respect the hardware counter limit per block.  This can be
        /// determined by the maxGlobalOnlyCounters, maxGlobalSharedCounters, and maxSpmCounters fields of
        /// @ref Pal::GpuBlockPerfProperties.
        const PerfCounterId* pIds;

        /// Period for SPM sample collection in cycles.  Only relevant for _trace_ samples.
        Pal::uint32  spmTraceSampleInterval;

        /// Maximum amount of GPU memory in bytes this sample can allocate for SPM data.  Only relevant for _trace_
        /// samples.
        Pal::gpusize gpuMemoryLimit;
    } perfCounters;  ///< Performance counter selection (valid for both _cumulative_ and _trace_ samples).

    struct
    {
        union
        {
            struct
            {
                Pal::uint32 enable                   :  1;  ///< Include SQTT data in the trace.
                Pal::uint32 supressInstructionTokens :  1;  ///< Prevents capturing instruciton-level SQTT tokens,
                                                            ///  significantly reducing the amount of sample data.
                Pal::uint32 reserved                 : 30;  ///< Reserved for future use.
            };
            Pal::uint32 u32All;                             ///< Bit flags packed as uint32.
        } flags;                                            ///< Bit flags controlling SQTT samples.

        Pal::gpusize gpuMemoryLimit; ///< Maximum amount of GPU memory in bytes this sample can allocate for the SQTT
                                     ///  buffer.  If 0, allocate maximum size to prevent dropping tokens toward the
                                     ///  end of the sample.
    } sqtt;  ///< SQ thread trace confiruation (only valid for _trace_ samples).

    struct
    {
        Pal::HwPipePoint preSample;   ///< The point in the GPU pipeline where the begin timestamp should take place.
        Pal::HwPipePoint postSample;  ///< The point in the GPU pipeline where the end timestamp should take place.
    } timing;  ///< Timestamp configuration. (only valid for timing samples)
};

/// Extra metadata about a command buffer submission
struct TimedSubmitInfo
{
    const Pal::uint64* pApiCmdBufIds;  ///< Array of api specific command buffer ids
    const Pal::uint32* pSqttCmdBufIds; ///< Array of sqtt command buffer ids
    Pal::uint64 frameIndex;            ///< The global frame index for the application.
};

/// Extra metadata about a queue semaphore operation
struct TimedQueueSemaphoreInfo
{
    Pal::uint64 semaphoreID; ///< Api specific id associated with a semaphore.
};

/// Extra metadata about a queue present operation
struct TimedQueuePresentInfo
{
    Pal::uint64 presentID; ///< Api specific id associated with a present.
};

/// Struct for storing information about gpu clock speeds.
struct GpuClocksSample
{
    Pal::uint32 gpuEngineClockSpeed; // Current speed of the gpu engine clock in MHz
    Pal::uint32 gpuMemoryClockSpeed; // Current speed of the gpu memory clock in MHz
};

/**
***********************************************************************************************************************
* @class GpaSession
* @brief Helper class providing common driver functionality required by all PAL clients that support the GPUPerfAPI
*        (GPA).  Abstracts IPerfExperiment creation, memory management, completion confirmation, and results reporting
*        at a level convenient for GPA.  Each PAL client driver will need to publish an API extension exposing this
*        support for use by GPA.
*
* A GpaSession is a container for a set of _samples_ of performance counter and/or SQ thread trace data.  Its main
* purpose is to manage resources (IPerfExperiments and their backing system/GPU memory) in an efficient manner that is
* consistent with command buffer management in modern APIs.  Consider GpaSession as a peer of DX12's command
* allocator or Vulkan's command pool objects.
*
* Basic flow of usage:
*     - Newly create sessions are in the _reset_ state.
*     - A session is moved from the _reset_ state to the _building_ state by calling Begin().
*     - Samples are added to a session by specifying desired data for each query and marking a begin and end location
*       in ICmdBuffers as they are built.  Internally required resources, like GPU memory where counters will be
*       written, are allocated from internal pools managed by the session.
*     - A session is moved from the _building_ state to the _complete_ state by calling End().
*     - The application will submit all command buffers referenced by the session.
*     - The session is confirmed as _ready_, either using standard PAL fences to confirm all assocated submission have
*       completed, or by polling IsReady() on the session.
*     - Results for all samples in the session can be queried via GetResults().
*     - Reset() should be called once results have been gathered and before building a new session.  Resources are
*       retained by the session object for use in the newly built session.  The session object must be destroyed in
*       order to fully release all resource back to the system.
*
* Cumulative-type samples may not span multiple command buffers, as other apps could interfere with the counts and
* there the final data doesn't have time-based visibility to detect that happened.
*
* @warning GpaSession is not thread safe.  Performing samples in command buffers being built simultaneously by multiple
*          threads should use multiple GpaSession objects.
***********************************************************************************************************************
*/
class GpaSession
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 318
    typedef Pal::IPlatform             GpaAllocator;
#else
    typedef Util::GenericAllocatorAuto GpaAllocator;
#endif

public:
    /// Constructor.
    GpaSession(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 318
        Pal::IPlatform*      pPlatform,
#endif
        Pal::IDevice*        pDevice,
        Pal::uint16          apiMajorVer,
        Pal::uint16          apiMinorVer,
        Pal::uint16          rgpInstrumentationSpecVer = 0,
        Pal::uint16          rgpInstrumentationApiVer  = 0);

    ~GpaSession();

    /// Copy constructor creates an empty copy of a session.
    ///
    /// Newly constructed session copies the GPU memory allocations and their layout from the source session, making
    /// this a valid destination for a CopyResults command.  This new object is effectively in the _complete_ state.
    ///
    /// The purpose of such objects is to handle sampling data from bundles or nested command buffers where the same
    /// set of commands might be executed multiple times from a single root-level command buffer.  The client should
    /// note such cases, and create a copy of the bundle's session for each invocation, then call CopyResults() from
    /// the original session into the copy after the invocation.
    ///
    /// @param [in] src Session to be copied.  Must either be in the _complete_ or _ready_ state.
    explicit GpaSession(const GpaSession& src);

    /// Initialize the newly constructed GPA session.
    Pal::Result Init();

    /// Registers a queue with the GpaSession that will be submitted to using TimedSubmit. This must be called on any
    /// queues that are submitted to via the Timed* functions.
    Pal::Result RegisterTimedQueue(Pal::IQueue* pQueue,
                                   Pal::uint64 queueId,
                                   Pal::uint64 queueContext);

    /// Unregisters a queue prior to object destruction, and ensure that associated resources are destroyed. Work can
    /// no longer be submitted on the queue after this has been called.
    Pal::Result UnregisterTimedQueue(Pal::IQueue* pQueue);

    /// Used to trigger a timed submit of one or more command buffers through the specified queue.  Each command buffer
    /// is timed individually and will be tagged with an atomically increasing event index on the given queue since the
    /// last reset.
    Pal::Result TimedSubmit(Pal::IQueue* pQueue,
                            const Pal::SubmitInfo& submitInfo,
                            const TimedSubmitInfo& timedSubmitInfo);

    /// Executes a timed queue semaphore signal through the given queue.  The HW time is measured when the queue semaphore
    /// is signaled.
    Pal::Result TimedSignalQueueSemaphore(Pal::IQueue* pQueue,
                                          Pal::IQueueSemaphore* pQueueSemaphore,
                                          const TimedQueueSemaphoreInfo& timedSignalInfo);

    /// Executes a timed queue semaphore wait through the given queue.  The HW time is measured when the queue semaphore
    /// wait finishes.
    Pal::Result TimedWaitQueueSemaphore(Pal::IQueue* pQueue,
                                        Pal::IQueueSemaphore* pQueueSemaphore,
                                        const TimedQueueSemaphoreInfo& timedWaitInfo);

    /// Injects a timed queue present event.
    Pal::Result TimedQueuePresent(Pal::IQueue* pQueue,
                                  const TimedQueuePresentInfo& timedPresentInfo);

    /// Injects a timed wait queue semaphore event using information supplied by an external source.
    Pal::Result ExternalTimedWaitQueueSemaphore(Pal::uint64 queueContext,
                                                Pal::uint64 cpuSubmissionTimestamp,
                                                Pal::uint64 cpuCompletionTimestamp,
                                                const TimedQueueSemaphoreInfo& timedWaitInfo);

    /// Injects a timed signal queue semaphore event using information supplied by an external source.
    Pal::Result ExternalTimedSignalQueueSemaphore(Pal::uint64 queueContext,
                                                  Pal::uint64 cpuSubmissionTimestamp,
                                                  Pal::uint64 cpuCompletionTimestamp,
                                                  const TimedQueueSemaphoreInfo& timedSignalInfo);

    /// Samples the timing clocks if queue timing is enabled and adds a clock sample entry to the current session.
    Pal::Result SampleTimingClocks();

    /// Moves the session from the _reset_ state to the _building_ state.
    ///
    /// Invalid to call Begin() on a session that isn't in the _reset_ state.
    ///
    /// @param [in] info Information about the gpa sessions desired behavior.
    ///
    /// @returns Success if the session was successfully moved to the _building_ state.  Otherwise, possible errors
    ///          include:
    ///          + ErrorUnavailable if the sessions isn't current in the _reset_ state.
    Pal::Result Begin(const GpaSessionBeginInfo& info);

    /// Moves the session from the _building_ state to the _complete_ state.
    ///
    /// Invalid to call End() on a session that isn't in the _building_ state.  The implementation _may_ insert GPU
    /// commands into the specified pCmdBuf - in the case of a session that spans multiple command buffers, the
    /// command buffer specified to End() _must_ be the last command buffer of the session that is submitted.
    ///
    /// @param [in] pCmdBuf Last (normally _only_) command buffer of the session.  Can be used by implementation
    ///                     to insert GPU commands required after all samples are inserted (e.g., to confirm session
    ///                     completion).
    ///
    /// @returns Success if the session was successfully moved to the _complete_ state.  Otherwise, possible errors
    ///          include:
    ///          + ErrorUnavailable if the sessions isn't current in the _building_ state.
    Pal::Result End(Pal::ICmdBuffer* pCmdBuf);

    /// Marks the beginning of a range of GPU operations to be measured and specifies what data should be recorded.
    ///
    /// It is possible the sample will not succeed due to internal memory allocation failure, etc.  In those cases,
    /// the session will be marked invalid and no sample commands will be inserted.  Reporting of this error is
    /// delayed until GetResults().
    ///
    /// @param [in] pCmdBuf      Command buffer to issue the begin sample commands.  All operations performed
    ///                          between executing the BeginSample() and EndSample() GPU commands will contribute to
    ///                          the sample results.
    /// @param [in] sampleConfig Describes what data should be sampled.
    ///
    /// @returns An ID corresponding to this sample.  This ID should be recorded and passed back to EndSample() when
    ///          the sampled command buffer range is complete.  This ID should also be passed to GetResults() when
    ///          the session is in the _ready_ state in order to get the results of this sample.
    Pal::uint32 BeginSample(
        Pal::ICmdBuffer*        pCmdBuf,
        const GpaSampleConfig&  sampleConfig);

    /// Updates the trace parameters for a specific sample.
    ///
    /// @param [in] pCmdBuf   Command buffer to issue the update commands.
    /// @param [in] sampleId  Identifies the sample to be updated.  This should be a value returned by BeginSample().
    ///                       This value must also correspond to a thread trace sample specifically.
    ///
    /// @returns Success if the update was successful.  Otherwise, possible errors
    ///          include:
    ///          + ErrorInvalidPointer if pCmdBuf is nullptr.
    ///          + ErrorInvalidObjectType if the sample associated with sampleId is not a trace sample.
    ///
    /// @note UpdateSampleTraceParams() must be called after BeginSample() and before EndSample() and queue timing must
    ///       also be enabled on the gpa session when this function is called.
    Pal::Result UpdateSampleTraceParams(
        Pal::ICmdBuffer*          pCmdBuf,
        Pal::uint32               sampleId);

    /// Marks the end of a range of command buffer operations to be measured.
    ///
    /// @param [in] pCmdBuf  Command buffer to issue the end sample commands.  All operations performed between
    ///                      executing the BeginSample() and EndSample() GPU commands will contribute to the sample
    ///                      results.  _Cumulative_ samples (i.e., global performance counter samples) must never span
    ///                      multiple command buffers (EndSample() should be called in the same command buffer as
    ///                      BeginSample()).
    /// @param [in] sampleId Identifies the sample to be ended.  This should be the value returned by BeginSample()
    ///                      for the sample that is being ended.
    ///
    /// @note BeginSample() must be called before EndSample() _and_ the GPU commands inserted by BeginSample() must be
    ///       executed before the command inserted by EndSample().  Since a session is a single-threaded object, this
    ///       will normally happen naturally.
    void EndSample(
        Pal::ICmdBuffer* pCmdBuf,
        Pal::uint32      sampleId);

    /// Reports if GPU execution of this session has completed and results are _ready_ for querying from the CPU via
    /// GetResults().
    ///
    /// @returns true if all samples in the session have completed GPU execution.
    bool IsReady() const;

    /// Reports results of a particular sample.  Only valid for sessions in the _ready_ state.
    ///
    /// Results will be formatted depending on the sample type:
    /// + Cumulative: Results will be an array of uint64 values in the order of perf counter IDs specified by
    ///               BeginSample().
    /// + SqThreadTrace: Results will be a binary blob in the RGP file format.
    ///
    /// @param [in]     sampleId     Sample to be reported.  Corresponds to value returned by BeginSample().
    /// @param [in,out] pSizeInBytes If pData is non-null, the input value of *pSizeInBytes is the amount of space
    ///                              available in pData, and *pSizeInBytes will be set to the amount of space written
    ///                              to pData.  If pData is null, *pSizeInBytes will be set to the amount of space
    ///                              required.
    /// @param [out]    pData        Can be null to query how much size is required (should only be necessary when
    ///                              getting RGP data).  If non-null, the sample results will be written to this
    ///                              location.
    ///
    /// @returns Success if the sample results are successfully written to pData (or, if pData is null, the required
    ///          size is successfully written to pSizeInBytes).  Otherwise, possible errors include:
    ///          + ErrorUnavailable if the session is not in the _ready_ state.
    ///          + ErrorOutOfGpuMemory if the session wasn't properly built due to running out of GPU memory resources.
    ///          + ErrorInvalidMemorySize if *pSizeInBytes isn't big enough to hold the results.
    Pal::Result GetResults(
        Pal::uint32 sampleId,
        size_t*     pSizeInBytes,
        void*       pData) const;

    /// Moves the session to the _reset_ state, marking all sessions resources as unused and available for reuse when
    /// the session is re-built.
    ///
    /// @warning This function cannot be called when the session is queued for execution on the GPU.  The client must
    ///          confirm this is not the case using IsReady(), fences, etc.
    ///
    /// @returns Success if the session was successfully moved to the _reset_ state.  Otherwise, possible errors
    ///          include:
    ///          + ErrorUnknown if an internal PAL error occurs.
    Pal::Result Reset();

    /// Uses the GPU to copy results from a nested command buffer's session into a root-level command buffer's per-
    /// invocation session data.
    ///
    /// This command will implicitly wait for the source session (as specified in the copy constructor) to be complete
    /// then use the GPU to update this session's data.  This allows the client to get accurate sample data in the
    /// case where a nested command buffer is launched multiple times from the same root-level command buffer.
    ///
    /// The session remains in the _complete_ state after calling this, and the client should submit the commands
    /// and verify their completion to move to the _ready_ state.
    ///
    /// @param pCmdBuf Command buffer where the session copy should be performed.
    void CopyResults(Pal::ICmdBuffer* pCmdBuf);

    /// Register pipeline with GpaSession for obtaining shader dumps in the RGP file.
    ///
    /// @param [in] pPipeLine The PAL pipeline to be tracked.
    ///
    /// @returns Success if the pipeline has been registered with GpaSession successfully.
    ///          + AlreadyExists if a duplicate pipeline is provided.
    Pal::Result RegisterPipeline(const Pal::IPipeline* pPipeline);

private:
    // Tracking structure for a single IGpuMemory allocation owned by a GpaSession::GpaSession. In particular, it
    // tracks the associated CPU pointer since these allocations remain mapped for CPU access for their lifetime.
    struct GpuMemoryInfo
    {
        Pal::IGpuMemory* pGpuMemory;
        void*            pCpuAddr;
    };

    // Represents all information to be contained in one SqttIsaDbRecord
    struct ShaderRecord
    {
        Pal::uint32 recordSize;
        void*       pRecord;
    };

    Pal::IDevice*const            m_pDevice;                    // Device associated with this GpaSession.
    Pal::DeviceProperties         m_deviceProps;
    Pal::PerfExperimentProperties m_perfExperimentProps;
    Pal::uint32                   m_timestampAlignment;         // Pre-calculated timestamp data alignment.
    Pal::uint16                   m_apiMajorVer;                // API major version, used in RGP dumps.
    Pal::uint16                   m_apiMinorVer;                // API minor version, used in RGP dumps.
    Pal::uint16                   m_instrumentationSpecVersion; // Spec version of RGP instrumetation.
    Pal::uint16                   m_instrumentationApiVersion;  // Api version of RGP instrumetation.

    Pal::IGpuEvent*               m_pGpuEvent;
    GpaSessionState               m_sessionState;

    const GpaSession* const       m_pSrcSession;                // source session for session created via copy c'tor

    // Tracks the current GPU memory object and offset being sub-allocated for AcquireGpuMem().
    GpuMemoryInfo                 m_curGartGpuMem;
    Pal::gpusize                  m_curGartGpuMemOffset;
    GpuMemoryInfo                 m_curLocalInvisGpuMem;
    Pal::gpusize                  m_curLocalInvisGpuMemOffset;

    // counts number of samples that has been spawned since begin of this GpaSession for sampleId creation ONLY
    Pal::uint32                   m_sampleCount;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 318
    Pal::IPlatform*const          m_pPlatform;                  // Platform associated with this GpaSesion.
#else
    GpaAllocator                  m_allocator;                  // Internal allocator for this session object.
    GpaAllocator*                 m_pPlatform;                  // Set to &m_allocator for backwards compatibility.
#endif

    // GartHeap and InvisHeap GPU chunk pools.
    Util::Deque<GpuMemoryInfo, GpaAllocator> m_availableGartGpuMem;
    Util::Deque<GpuMemoryInfo, GpaAllocator> m_busyGartGpuMem;
    Util::Deque<GpuMemoryInfo, GpaAllocator> m_availableLocalInvisGpuMem;
    Util::Deque<GpuMemoryInfo, GpaAllocator> m_busyLocalInvisGpuMem;

    struct SampleItem;
    class PerfSample;
    class CounterSample;
    class TraceSample;
    class TimingSample;
    class QuerySample;

    Util::Vector<SampleItem*, 16, GpaAllocator> m_sampleItemArray;

    // Unique pipelines registered with this GpaSession.
    Util::HashSet<Pal::uint64, GpaAllocator> m_registeredPipelines;

    // List of cached shader isa records that will be copied to the final shader records database at the end of a trace
    Util::Deque<ShaderRecord, GpaAllocator>  m_shaderRecordsCache;

    // List of shader isa records that were registered during a trace
    Util::Deque<ShaderRecord, GpaAllocator>  m_curShaderRecords;

    Util::RWLock m_registerPipelineLock;

    // Event type for timed queue events
    enum class TimedQueueEventType : Pal::uint32
    {
        Submit,
        Signal,
        Wait,
        Present,
        ExternalSignal,
        ExternalWait
    };

    // Struct that contains information about a specific timed queue event.
    struct TimedQueueEventItem
    {
        TimedQueueEventType eventType;      // Type of event
        Pal::uint64         cpuTimestamp;   // Time when the event was processed on the cpu
        Pal::uint64         apiId;          // The api specific id for the queue event
        Pal::uint32         sqttCmdBufId;   // The sqtt command buffer id value associated with a submit event
        Pal::uint32         submitSubIndex; // The sub index of an event within a submission event.
        Pal::uint32         queueIndex;     // The index of the associated queue in the m_timedQueuesArray
        Pal::uint64         frameIndex;     // The index of the current frame being rendered
        union
        {
            struct
            {
                GpuMemoryInfo memInfo[2];   // The gpu memory for the timestamps associated with the event
                Pal::gpusize  offsets[2];   // Memory offsets for the associated timestamp gpu memory
            } gpuTimestamps;

            Pal::uint64       cpuCompletionTimestamp; // The time when the event completed on the cpu
        };
    };

    // Struct for keeping track of timed operation on a specific queue
    struct TimedQueueState
    {
        Pal::IQueue*                                 pQueue;               // Pal Queue
        Pal::uint64                                  queueId;              // Api specific queue id
        Pal::uint64                                  queueContext;         // Api specific queue context
        Pal::QueueType                               queueType;            // Queue type
        Pal::EngineType                              engineType;           // Engine type
        bool                                         valid;                // Used to track if the queue is valid
        Util::Deque<Pal::ICmdBuffer*, GpaAllocator>* pAvailableCmdBuffers; // List of available cmdbuffers
        Util::Deque<Pal::ICmdBuffer*, GpaAllocator>* pBusyCmdBuffers;      // List of busy cmdbuffers
        Pal::IFence*                                 pFence;               // Used to track queue
                                                                           // operations
    };

    // Flags for the current session.
    GpaSessionFlags m_flags;

    // Array containing all of the queues registered for timing operations
    Util::Vector<TimedQueueState*, 8, GpaAllocator> m_timedQueuesArray;

    // List of timed queue events for the current session
    Util::Vector<TimedQueueEventItem, 16, GpaAllocator> m_queueEvents;

    // List of timestamp calibration samples
    Util::Vector<Pal::GpuTimestampCalibration, 4, GpaAllocator> m_timestampCalibrations;

    // The most recent gpu clocks sample
    GpuClocksSample m_lastGpuClocksSample;

    // Internal command allocator used for timing command buffers
    Pal::ICmdAllocator* m_pCmdAllocator;

    // Finds the TimedQueueState associated with pQueue.
    Pal::Result FindTimedQueue(Pal::IQueue* pQueue,
                               TimedQueueState** ppQueueState,
                               Pal::uint32* pQueueIndex);

    // Finds the TimedQueueState associated with queueContext.
    Pal::Result FindTimedQueueByContext(Pal::uint64 queueContext,
                                        TimedQueueState** ppQueueState,
                                        Pal::uint32* pQueueIndex);

    /// Executes a timed queue semaphore operation through the given queue
    Pal::Result TimedQueueSemaphoreOperation(Pal::IQueue* pQueue,
                                             Pal::IQueueSemaphore* pQueueSemaphore,
                                             const TimedQueueSemaphoreInfo& timedSemaphoreInfo,
                                             bool isSignalOperation);

    /// Injects an external timed queue semaphore operation event
    Pal::Result ExternalTimedQueueSemaphoreOperation(Pal::uint64 queueContext,
                                                     Pal::uint64 cpuSubmissionTimestamp,
                                                     Pal::uint64 cpuCompletionTimestamp,
                                                     const TimedQueueSemaphoreInfo& timedSemaphoreInfo,
                                                     bool isSignalOperation);

    /// Helper function to sample CPU & GPU timestamp, and insert a timed queue operation event.
    Pal::Result AddCpuGpuTimedQueueEvent(Pal::IQueue* pQueue,
                                         TimedQueueEventType eventType,
                                         Pal::uint64 apiId);

    /// Converts a CPU timestamp to a GPU timestamp using a GpuTimestampCalibration struct
    Pal::uint64 ConvertCpuTimestampToGpuTimestamp(Pal::uint64                         cpuTimestamp,
                                                  const Pal::GpuTimestampCalibration& calibration) const;

    /// Extracts a GPU timestamp from a queue event
    Pal::uint64 ExtractGpuTimestampFromQueueEvent(const TimedQueueEventItem& queueEvent) const;

    // Creates a new command buffer for use on pQueue
    Pal::Result CreateCmdBufferForQueue(Pal::IQueue* pQueue,
                                        Pal::ICmdBuffer** ppCmdBuffer);

    // Acquires a command buffer from the TimedQueueState's command buffer pool
    Pal::Result AcquireTimedQueueCmdBuffer(TimedQueueState* pQueueState,
                                           Pal::ICmdBuffer** ppCmdBuffer);

    // Recycles busy command buffers in pQueueState
    Pal::Result RecycleTimedQueueCmdBuffers(TimedQueueState* pQueueState);

    // Preallocates a fixed number of command buffers for pQueueState and adds them to the command buffer pool
    Pal::Result PreallocateTimedQueueCmdBuffers(TimedQueueState* pQueueState,
                                                Pal::uint32 numCmdBuffers);

    // Resets all per session state in pQueueState
    Pal::Result ResetTimedQueueState(TimedQueueState* pQueueState);

    // Destroys the memory and resources for pQueueState
    void DestroyTimedQueueState(TimedQueueState* pQueueState);

    // Helper function to import one sample item from a source session to copy session.
    Pal::Result ImportSampleItem(const SampleItem* pSrcSampleItem);

    // Acquires a range of queue-owned GPU memory for use by the next command buffer submission.
    Pal::Result AcquireGpuMem(
        Pal::gpusize            size,
        Pal::gpusize            alignment,
        Pal::GpuHeap            heapType,
        GpuMemoryInfo*          pGpuMem,
        Pal::gpusize*           pOffset);

    // Acquires a GpaSession-owned performance experiment based on the device's active perf counter requests.
    Pal::IPerfExperiment* AcquirePerfExperiment(
        const GpaSampleConfig&  sampleConfig,
        GpuMemoryInfo*          pGpuMem,
        Pal::gpusize*           pOffset,
        GpuMemoryInfo*          pSecondaryGpuMem,
        Pal::gpusize*           pSecondaryOffset,
        Pal::gpusize*           pHeapSize);

    // Acquires a session-owned pipeline stats query.
    Pal::Result AcquirePipeStatsQuery(
        GpuMemoryInfo*          pGpuMem,
        Pal::gpusize*           pOffset,
        Pal::gpusize*           pHeapSize,
        Pal::IQueryPool**       ppQuery);

    // Dump SQ thread trace data in rgp format
    Pal::Result DumpRgpData(TraceSample* pTraceSample, void* pRgpOutput, size_t* pTraceSize) const;

    // Dumps the spm trace data in the buffer provided.
    Pal::Result AppendSpmTraceData(TraceSample*  pTraceSample,
                                   size_t        bufferSize,
                                   void*         pData,
                                   Pal::gpusize* pSizeInBytes) const;

    // recycle used Gart rafts and put back to available pool
    void RecycleGartGpuMem();

    // recycle used Local Invisible rafts and put back to available pool
    void RecycleLocalInvisGpuMem();

    // Destroy and free the m_sampleItemArray and associated memory allocation
    void FreeSampleItemArray();

    Pal::Result CreateShaderRecord(
        Pal::ShaderType       shaderType,
        const Pal::IPipeline* pPipeline,
        ShaderRecord*         pShaderRecord);

    PAL_DISALLOW_DEFAULT_CTOR(GpaSession);
    GpaSession& operator =(const GpaSession&);
};
} // GpuUtil
