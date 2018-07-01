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

#include "palCmdAllocator.h"
#include "palCmdBuffer.h"
#include "palDequeImpl.h"
#include "palFence.h"
#include "palGpuEvent.h"
#include "palGpuMemory.h"
#include "palHashSetImpl.h"
#include "palMemTrackerImpl.h"
#include "palPipeline.h"
#include "palQueue.h"
#include "palSysMemory.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include "gpaSessionPerfSample.h"
#include "gpuUtil/sqtt_file_format.h"
#include <ctime>

using namespace Pal;

namespace GpuUtil
{

// Translation table for obtaining Sqtt thread trace version given the Pal::GfxIpLevel.
static const SqttVersion GfxipToSqttVersionTranslation[static_cast<uint32>(Pal::GfxIpLevel::Count)] =
{
    SQTT_VERSION_NONE,
    SQTT_VERSION_2_0,  // Gfxip 6
    SQTT_VERSION_2_1,  // Gfxip 7
    SQTT_VERSION_2_2,  // Gfxip 8
    SQTT_VERSION_2_2,  // Gfxip 8.1
    SQTT_VERSION_2_3,  // Gfxip 9
};

// Translation table for obtaining SqttQueueType given the Pal::QueueType.
static const SqttQueueType PalQueueTypeToSqttQueueType[Pal::QueueTypeCount] =
{
    SQTT_QUEUE_TYPE_UNIVERSAL, // QueueTypeUniversal
    SQTT_QUEUE_TYPE_COMPUTE,   // QueueTypeCompute
    SQTT_QUEUE_TYPE_DMA,       // QueueTypeDma
    SQTT_QUEUE_TYPE_UNKNOWN,   // QueueTypeTimer
};

// Translation table for obtaining SqttEngineType given the Pal::QueueType.
static const SqttEngineType PalEngineTypeToSqttEngineType[Pal::EngineTypeCount] =
{
    SQTT_ENGINE_TYPE_UNIVERSAL,         // EngineTypeUniversal
    SQTT_ENGINE_TYPE_COMPUTE,           // EngineTypeCompute
    SQTT_ENGINE_TYPE_EXCLUSIVE_COMPUTE, // EngineTypeExclusiveCompute
    SQTT_ENGINE_TYPE_DMA,               // EngineTypeDma
    SQTT_ENGINE_TYPE_UNKNOWN,           // EngineTypeTimer
};

// Translation table for obtaining SqttMemoryType given a Pal::LocalMemoryType.
static constexpr SqttMemoryType SqttMemoryTypeTable[static_cast<uint32>(Pal::LocalMemoryType::Count)] =
{
    SQTT_MEMORY_TYPE_UNKNOWN, // Unknown
    SQTT_MEMORY_TYPE_DDR2,    // Ddr2
    SQTT_MEMORY_TYPE_DDR3,    // Ddr3
    SQTT_MEMORY_TYPE_DDR4,    // Ddr4
    SQTT_MEMORY_TYPE_GDDR5,   // Gddr5
    SQTT_MEMORY_TYPE_GDDR6,   // Gddr6
    SQTT_MEMORY_TYPE_HBM,     // Hbm
    SQTT_MEMORY_TYPE_HBM2,    // Hbm2
    SQTT_MEMORY_TYPE_HBM3     // Hbm3
};

struct BlockEventId
{
    Pal::GpuBlock block;    // Which GPU block to reference (e.g., CB, DB, TCC).
    Pal::uint32   eventId;  // Counter ID to sample.  Note that the meaning of a particular eventId for a block can
                            // change between chips.
};

// SQTT token mask configurations
static const uint32 SqttTokenMaskAll     = 0xFFFF; // Collect all tokens
static const uint32 SqttTokenMaskNoInst  = 0xC3FF; // Collect all tokens except for instruction related tokens
static const uint32 SqttTokenMaskMinimal = 0x81A7; // Collect a minimal set of tokens (timestamps + events)

// =====================================================================================================================
// Helper function to fill in the SqttFileChunkCpuInfo struct based on the hardware in the current system.
// Required for writing RGP files.
void FillSqttCpuInfo(
    SqttFileChunkCpuInfo* pCpuInfo)
{
    PAL_ASSERT(pCpuInfo != nullptr);

    pCpuInfo->header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_CPU_INFO;
    pCpuInfo->header.chunkIdentifier.chunkIndex = 0;
    pCpuInfo->header.version                    = 0;
    pCpuInfo->header.sizeInBytes                = sizeof(SqttFileChunkCpuInfo);

    pCpuInfo->cpuTimestampFrequency = static_cast<uint64>(Util::GetPerfFrequency());

    Util::SystemInfo systemInfo = {};
    Pal::Result result = Util::QuerySystemInfo(&systemInfo);
    if (result == Pal::Result::Success)
    {
        Util::Strncpy(reinterpret_cast<char*>(pCpuInfo->vendorId),
                      systemInfo.cpuVendorString,
                      sizeof(pCpuInfo->vendorId));
        Util::Strncpy(reinterpret_cast<char*>(pCpuInfo->processorBrand),
                      systemInfo.cpuBrandString,
                      sizeof(pCpuInfo->processorBrand));

        // @todo: Add support for querying the cpu clock speed.
        pCpuInfo->clockSpeed = 0;

        pCpuInfo->numLogicalCores = systemInfo.cpuLogicalCoreCount;
        pCpuInfo->numPhysicalCores = systemInfo.cpuPhysicalCoreCount;
        pCpuInfo->systemRamSize = systemInfo.totalSysMemSize;
    }
    else
    {
        // We were not able to successfully query system information. Fill out the structs in a way that reflects this.
        PAL_ALERT_ALWAYS();

        Util::Strncpy(reinterpret_cast<char*>(pCpuInfo->vendorId),
                      "Unknown",
                      sizeof(pCpuInfo->vendorId));
        Util::Strncpy(reinterpret_cast<char*>(pCpuInfo->processorBrand),
                      "Unknown",
                      sizeof(pCpuInfo->processorBrand));

        // @todo: Add support for querying the cpu clock speed.
        pCpuInfo->clockSpeed = 0;

        pCpuInfo->numLogicalCores = 0;
        pCpuInfo->numPhysicalCores = 0;
        pCpuInfo->systemRamSize = 0;
    }
}

// =====================================================================================================================
// Helper function to fill in the SqttFileChunkAsicInfo struct based on the DeviceProperties and
// PerfExperimentProperties provided. Required for writing RGP files.
void FillSqttAsicInfo(
    const Pal::DeviceProperties&         properties,
    const Pal::PerfExperimentProperties& perfExpProps,
    const GpuClocksSample&               gpuClocks,
    SqttFileChunkAsicInfo*               pAsicInfo)
{
    PAL_ASSERT(pAsicInfo != nullptr);

    pAsicInfo->header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_ASIC_INFO;
    pAsicInfo->header.chunkIdentifier.chunkIndex = 0;
    pAsicInfo->header.version                    = 2;
    pAsicInfo->header.sizeInBytes                = sizeof(SqttFileChunkAsicInfo);

    pAsicInfo->flags = 0;

    if (perfExpProps.features.sqttBadScPackerId)
    {
        pAsicInfo->flags |= SQTT_FILE_CHUNK_ASIC_INFO_FLAG_SC_PACKER_NUMBERING;
    }

    if (perfExpProps.features.supportPs1Events)
    {
        pAsicInfo->flags |= SQTT_FILE_CHUNK_ASIC_INFO_FLAG_PS1_EVENT_TOKENS_ENABLED;
    }

    pAsicInfo->traceShaderCoreClock       = gpuClocks.gpuEngineClockSpeed * 1000000;
    pAsicInfo->traceMemoryClock           = gpuClocks.gpuMemoryClockSpeed * 1000000;

    pAsicInfo->deviceId                   = properties.deviceId;
    pAsicInfo->deviceRevisionId           = properties.revisionId;
    pAsicInfo->vgprsPerSimd               = properties.gfxipProperties.shaderCore.vgprsPerSimd;
    pAsicInfo->sgprsPerSimd               = properties.gfxipProperties.shaderCore.sgprsPerSimd;
    pAsicInfo->shaderEngines              = properties.gfxipProperties.shaderCore.numShaderEngines;
    pAsicInfo->computeUnitPerShaderEngine = properties.gfxipProperties.shaderCore.numCusPerShaderArray *
                                            properties.gfxipProperties.shaderCore.numShaderArrays;
    pAsicInfo->simdPerComputeUnit         = properties.gfxipProperties.shaderCore.numSimdsPerCu;
    pAsicInfo->wavefrontsPerSimd          = properties.gfxipProperties.shaderCore.numWavefrontsPerSimd;
    pAsicInfo->minimumVgprAlloc           = properties.gfxipProperties.shaderCore.minVgprAlloc;
    pAsicInfo->vgprAllocGranularity       = properties.gfxipProperties.shaderCore.vgprAllocGranularity;
    pAsicInfo->minimumSgprAlloc           = properties.gfxipProperties.shaderCore.minSgprAlloc;
    pAsicInfo->sgprAllocGranularity       = properties.gfxipProperties.shaderCore.sgprAllocGranularity;
    pAsicInfo->hardwareContexts           = properties.gfxipProperties.hardwareContexts;
    pAsicInfo->gpuType                    = static_cast<SqttGpuType>(properties.gpuType);
    pAsicInfo->gfxIpLevel                 = static_cast<SqttGfxIpLevel>(properties.gfxLevel);
    pAsicInfo->gpuIndex                   = properties.gpuIndex;
    pAsicInfo->gdsSize                    = properties.gfxipProperties.gdsSize;
    pAsicInfo->gdsPerShaderEngine         = properties.gfxipProperties.gdsSize /
                                             properties.gfxipProperties.shaderCore.numShaderEngines;
    pAsicInfo->ceRamSize                  = properties.gfxipProperties.ceRamSize;

    pAsicInfo->maxNumberOfDedicatedCus    = properties.engineProperties[EngineTypeUniversal].maxNumDedicatedCu;
    pAsicInfo->ceRamSizeGraphics          = properties.engineProperties[EngineTypeUniversal].ceRamSizeAvailable;
    pAsicInfo->ceRamSizeCompute           = properties.engineProperties[EngineTypeCompute].ceRamSizeAvailable;

    pAsicInfo->vramBusWidth               = properties.gpuMemoryProperties.performance.vramBusBitWidth;
    pAsicInfo->vramSize                   = properties.gpuMemoryProperties.maxLocalMemSize;
    pAsicInfo->l2CacheSize                = properties.gfxipProperties.shaderCore.tccSizeInBytes;
    pAsicInfo->l1CacheSize                = properties.gfxipProperties.shaderCore.tcpSizeInBytes;
    pAsicInfo->ldsSize                    = properties.gfxipProperties.shaderCore.ldsSizePerCu;

    memcpy(pAsicInfo->gpuName, &properties.gpuName, SQTT_GPU_NAME_MAX_SIZE);

    pAsicInfo->aluPerClock                = properties.gfxipProperties.performance.aluPerClock;
    pAsicInfo->texturePerClock            = properties.gfxipProperties.performance.texPerClock;
    pAsicInfo->primsPerClock              = properties.gfxipProperties.performance.primsPerClock;
    pAsicInfo->pixelsPerClock             = properties.gfxipProperties.performance.pixelsPerClock;

    pAsicInfo->gpuTimestampFrequency      = properties.timestampFrequency;

    pAsicInfo->maxShaderCoreClock =
        static_cast<uint64>(properties.gfxipProperties.performance.maxGpuClock * 1000000.0f);
    pAsicInfo->maxMemoryClock =
        static_cast<uint64>(properties.gpuMemoryProperties.performance.maxMemClock * 1000000.0f);

    pAsicInfo->memoryOpsPerClock = properties.gpuMemoryProperties.performance.memOpsPerClock;

    pAsicInfo->memoryChipType =
        SqttMemoryTypeTable[static_cast<uint32>(properties.gpuMemoryProperties.localMemoryType)];

    pAsicInfo->ldsGranularity = properties.gfxipProperties.shaderCore.ldsGranularity;
}

// =====================================================================================================================
GpaSession::GpaSession(
    IPlatform*           pPlatform,
    IDevice*             pDevice,
    uint16               apiMajorVer,
    uint16               apiMinorVer,
    uint16               rgpInstrumentationSpecVer,
    uint16               rgpInstrumentationApiVer)
    :
    m_pDevice(pDevice),
    m_timestampAlignment(0),
    m_apiMajorVer(apiMajorVer),
    m_apiMinorVer(apiMinorVer),
    m_instrumentationSpecVersion(rgpInstrumentationSpecVer),
    m_instrumentationApiVersion(rgpInstrumentationApiVer),
    m_pGpuEvent(nullptr),
    m_sessionState(GpaSessionState::Reset),
    m_pSrcSession(nullptr),
    m_curGartGpuMemOffset(0),
    m_curLocalInvisGpuMemOffset(0),
    m_sampleCount(0),
    m_pPlatform(pPlatform),
    m_availableGartGpuMem(m_pPlatform),
    m_busyGartGpuMem(m_pPlatform),
    m_availableLocalInvisGpuMem(m_pPlatform),
    m_busyLocalInvisGpuMem(m_pPlatform),
    m_sampleItemArray(m_pPlatform),
    m_registeredPipelines(512, m_pPlatform),
    m_shaderRecordsCache(m_pPlatform),
    m_curShaderRecords(m_pPlatform),
    m_timedQueuesArray(m_pPlatform),
    m_queueEvents(m_pPlatform),
    m_timestampCalibrations(m_pPlatform),
    m_pCmdAllocator(nullptr)
{
    memset(&m_deviceProps,               0, sizeof(m_deviceProps));
    memset(&m_perfExperimentProps,       0, sizeof(m_perfExperimentProps));
    memset(&m_curGartGpuMem,             0, sizeof(m_curGartGpuMem));
    memset(&m_curLocalInvisGpuMem,       0, sizeof(m_curLocalInvisGpuMem));

    m_flags.u32All = 0;
}

// =====================================================================================================================
GpaSession::~GpaSession()
{
    // Destroy Gart gpu memory allocations
    RecycleGartGpuMem();
    while (m_availableGartGpuMem.NumElements() > 0)
    {
        GpuMemoryInfo info = {};
        m_availableGartGpuMem.PopFront(&info);

        PAL_ASSERT(info.pGpuMemory != nullptr);

        info.pGpuMemory->Unmap();
        info.pGpuMemory->Destroy();
        PAL_SAFE_FREE(info.pGpuMemory, m_pPlatform);
    }

    // Destroy invisible gpu memory allocation
    RecycleLocalInvisGpuMem();
    while (m_availableLocalInvisGpuMem.NumElements() > 0)
    {
        GpuMemoryInfo info = {};
        m_availableLocalInvisGpuMem.PopFront(&info);

        PAL_ASSERT(info.pGpuMemory != nullptr);

        info.pGpuMemory->Destroy();
        PAL_SAFE_FREE(info.pGpuMemory, m_pPlatform);
    }

    for (uint32 queueStateIndex = 0; queueStateIndex < m_timedQueuesArray.NumElements(); ++queueStateIndex)
    {
        TimedQueueState* pQueueState = m_timedQueuesArray.At(queueStateIndex);

        DestroyTimedQueueState(pQueueState);
    }
    m_timedQueuesArray.Clear();

    // Free each sampleItem
    FreeSampleItemArray();

    if (m_pCmdAllocator != nullptr)
    {
        m_pCmdAllocator->Destroy();
        PAL_SAFE_FREE(m_pCmdAllocator, m_pPlatform);
    }

    if (m_pGpuEvent != nullptr)
    {
        m_pGpuEvent->Destroy();
        PAL_SAFE_FREE(m_pGpuEvent, m_pPlatform);
    }

    // Clear the shader records cache.
    while (m_shaderRecordsCache.NumElements() > 0)
    {
        ShaderRecord shaderRecord = {};
        m_shaderRecordsCache.PopFront(&shaderRecord);
        PAL_ASSERT(shaderRecord.pRecord != nullptr);

        PAL_SAFE_FREE(shaderRecord.pRecord, m_pPlatform);
    }
}

// =====================================================================================================================
// Copy constructor creates an empty copy of a session.
GpaSession::GpaSession(
    const GpaSession& src)
    :
    m_pDevice(src.m_pDevice),
    m_timestampAlignment(0),
    m_apiMajorVer(src.m_apiMajorVer),
    m_apiMinorVer(src.m_apiMinorVer),
    m_instrumentationSpecVersion(src.m_instrumentationSpecVersion),
    m_instrumentationApiVersion(src.m_instrumentationApiVersion),
    m_pGpuEvent(nullptr),
    m_sessionState(GpaSessionState::Reset),
    m_pSrcSession(&src),
    m_curGartGpuMemOffset(0),
    m_curLocalInvisGpuMemOffset(0),
    m_sampleCount(0),
    m_pPlatform(src.m_pPlatform),
    m_availableGartGpuMem(m_pPlatform),
    m_busyGartGpuMem(m_pPlatform),
    m_availableLocalInvisGpuMem(m_pPlatform),
    m_busyLocalInvisGpuMem(m_pPlatform),
    m_sampleItemArray(m_pPlatform),
    m_registeredPipelines(512, m_pPlatform),
    m_shaderRecordsCache(m_pPlatform),
    m_curShaderRecords(m_pPlatform),
    m_timedQueuesArray(m_pPlatform),
    m_queueEvents(m_pPlatform),
    m_timestampCalibrations(m_pPlatform),
    m_pCmdAllocator(nullptr)
{
    memset(&m_deviceProps,               0, sizeof(m_deviceProps));
    memset(&m_perfExperimentProps,       0, sizeof(m_perfExperimentProps));
    memset(&m_curGartGpuMem,             0, sizeof(m_curGartGpuMem));
    memset(&m_curLocalInvisGpuMem,       0, sizeof(m_curLocalInvisGpuMem));

    m_flags.u32All = 0;
}

// =====================================================================================================================
// Initialize a newly created GpaSession object
Result GpaSession::Init()
{
    // Load device properties to this GpaSession
    Result result = m_pDevice->GetProperties(&m_deviceProps);

    if (result == Result::Success)
    {
        // Load PerfExperiment properties to this GpaSession
        result = m_pDevice->GetPerfExperimentProperties(&m_perfExperimentProps);
    }

    // Pre-calculate GPU memory alignment for timestamp results. Use the largest alignment across all engines to avoid
    // determining the alignment per sample granularity.
    for (uint32 i = 0; i < EngineTypeCount; i++)
    {
        m_timestampAlignment = Util::Max(m_timestampAlignment, m_deviceProps.engineProperties[i].minTimestampAlignment);
    }
    PAL_ASSERT(m_timestampAlignment != 0);

    if (result == Result::Success)
    {
        // Create gpuEvent for this gpaSession object
        const GpuEventCreateInfo createInfo = {};
        const size_t             eventSize  = m_pDevice->GetGpuEventSize(createInfo, &result);

        if (result == Result::Success)
        {
            void* pMemory = PAL_CALLOC(eventSize,
                                       m_pPlatform,
                                       Util::SystemAllocType::AllocObject);
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateGpuEvent(createInfo, pMemory, &m_pGpuEvent);
            }
        }
    }

    if (result == Result::Success)
    {
        // Create internal cmd allocator for this gpaSession object
        CmdAllocatorCreateInfo createInfo = { };

        // Reasonable constants for allocation and suballocation sizes
        constexpr size_t CmdAllocSize = 2 * 1024 * 1024;
        constexpr size_t CmdSubAllocSize = 64 * 1024;

        createInfo.allocInfo[CommandDataAlloc].allocHeap      = GpuHeapGartUswc;
        createInfo.allocInfo[CommandDataAlloc].allocSize      = CmdAllocSize;
        createInfo.allocInfo[CommandDataAlloc].suballocSize   = CmdSubAllocSize;
        createInfo.allocInfo[EmbeddedDataAlloc].allocHeap     = GpuHeapGartUswc;
        createInfo.allocInfo[EmbeddedDataAlloc].allocSize     = CmdAllocSize;
        createInfo.allocInfo[EmbeddedDataAlloc].suballocSize  = CmdSubAllocSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 395
        createInfo.allocInfo[GpuScratchMemAlloc].allocHeap    = GpuHeapInvisible;
        createInfo.allocInfo[GpuScratchMemAlloc].allocSize    = CmdAllocSize;
        createInfo.allocInfo[GpuScratchMemAlloc].suballocSize = CmdSubAllocSize;
#endif

        const size_t cmdAllocatorSize = m_pDevice->GetCmdAllocatorSize(createInfo, &result);
        if (result == Result::Success)
        {
            void* pMemory = PAL_CALLOC(cmdAllocatorSize,
                                       m_pPlatform,
                                       Util::AllocObject);
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateCmdAllocator(createInfo, pMemory, &m_pCmdAllocator);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pPlatform);
                }
            }
        }
    }

    if (result == Result::Success)
    {
        m_registerPipelineLock.Init();
    }

    if (result == Result::Success)
    {
        m_registeredPipelines.Init();
    }

    // CopySession specific work
    if ((result == Result::Success) && (m_pSrcSession != nullptr))
    {
        if ((m_pSrcSession->m_sessionState != GpaSessionState::Complete) &&
            (m_pSrcSession->m_sessionState != GpaSessionState::Ready))
        {
            result = Result::ErrorUnavailable;
        }

        if (result == Result::Success)
        {
            result = m_pGpuEvent->Reset();

            // Update session state
            m_sessionState = GpaSessionState::Complete;

            // Import total number of samples
            m_sampleCount = m_pSrcSession->m_sampleCount;
        }

        // Import SampleItem array and shader ISA database.
        if ((result == Result::Success) && (m_pSrcSession != nullptr))
        {
            // Copy shader ISA database from srcSession
            auto iter = m_pSrcSession->m_shaderRecordsCache.Begin();
            while (iter.Get())
            {
                m_shaderRecordsCache.PushBack(*iter.Get());
                iter.Next();
            }

            // Import each SampleItem
            for (uint32 i = 0; i < m_sampleCount; i++)
            {
                result = ImportSampleItem(m_pSrcSession->m_sampleItemArray.At(i));
                if (result != Result::Success)
                {
                    break;
                }
            } // End of per SampleItem importion
        } // End of SampleItem array importion

        // All GPU memory allocation is done by this time. Finalize the GpuMem pools.
        if (result == Result::Success)
        {
            // Push currently active GPU memory chunk into busy list.
            if (m_curGartGpuMem.pGpuMemory != nullptr)
            {
                m_busyGartGpuMem.PushBack(m_curGartGpuMem);
                m_curGartGpuMem       = { nullptr, nullptr };
            }
            if (m_curLocalInvisGpuMem.pGpuMemory != nullptr)
            {
                m_busyLocalInvisGpuMem.PushBack(m_curLocalInvisGpuMem);
                m_curLocalInvisGpuMem = { nullptr, nullptr };
            }
        }
        else
        {
            // Destroy any created resource if it failed to copy over all samples.
            FreeSampleItemArray();
        }
    } // End of extra work for copySession

    return result;
}

// =====================================================================================================================
// Registers a queue with the gpa session that will be used in future timing operations.
Pal::Result GpaSession::RegisterTimedQueue(
    Pal::IQueue* pQueue,
    uint64       queueId,
    Pal::uint64  queueContext)
{
    Pal::Result result = Pal::Result::Success;

    // Make sure the queue isn't already registered.
    TimedQueueState* pQueueState = nullptr;
    uint32 queueIndex = 0;
    if (FindTimedQueue(pQueue, &pQueueState, &queueIndex) == Pal::Result::Success)
    {
        result = Pal::Result::ErrorIncompatibleQueue;
    }

    size_t fenceSize = 0;
    if (result == Pal::Result::Success)
    {
        fenceSize = m_pDevice->GetFenceSize(&result);
    }

    if (result == Pal::Result::Success)
    {
        // Create a new TimedQueueState struct
        // Pack all the required data into one chunk of memory to avoid handling multiple cases of memory allocation
        // failure.
        const Pal::gpusize cmdBufferListSize = sizeof(Util::Deque<Pal::ICmdBuffer*, GpaAllocator>);
        TimedQueueState* pTimedQueueState = static_cast<TimedQueueState*>(
            PAL_CALLOC(sizeof(TimedQueueState) + (cmdBufferListSize * 2) + fenceSize,
                       m_pPlatform,
                       Util::AllocObject));

        if (pTimedQueueState != nullptr)
        {
            pTimedQueueState->pQueue = pQueue;
            pTimedQueueState->queueId = queueId;
            pTimedQueueState->queueContext = queueContext;
            pTimedQueueState->queueType = pQueue->Type();
            pTimedQueueState->engineType = pQueue->GetEngineType();
            pTimedQueueState->valid = true;
            pTimedQueueState->pAvailableCmdBuffers =
                PAL_PLACEMENT_NEW(Util::VoidPtrInc(pTimedQueueState, sizeof(TimedQueueState)))
                Util::Deque<Pal::ICmdBuffer*, GpaAllocator>(m_pPlatform);

            pTimedQueueState->pBusyCmdBuffers =
                PAL_PLACEMENT_NEW(Util::VoidPtrInc(pTimedQueueState->pAvailableCmdBuffers, cmdBufferListSize))
                Util::Deque<Pal::ICmdBuffer*, GpaAllocator>(m_pPlatform);

            Pal::FenceCreateInfo createInfo = {};
            createInfo.flags.signaled       = 1;
            result = m_pDevice->CreateFence(createInfo,
                                            Util::VoidPtrInc(pTimedQueueState->pBusyCmdBuffers, cmdBufferListSize),
                                            &pTimedQueueState->pFence);

            // Preallocate some command buffers to reduce the latency of the first trace.
            constexpr uint32 NumPreallocatedCmdBuffers = 8;

            if (result == Result::Success)
            {
                result = PreallocateTimedQueueCmdBuffers(pTimedQueueState, NumPreallocatedCmdBuffers);
            }

            if (result == Result::Success)
            {
                result = m_timedQueuesArray.PushBack(pTimedQueueState);
            }

            if (result != Result::Success)
            {
                DestroyTimedQueueState(pTimedQueueState);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Unregisters a queue prior to object destruction, and ensure that associated resources are destroyed.
Pal::Result GpaSession::UnregisterTimedQueue(
    Pal::IQueue* pQueue)
{
    TimedQueueState* pQueueState = nullptr;
    uint32 queueIndex = 0;
    Pal::Result result = FindTimedQueue(pQueue, &pQueueState, &queueIndex);

    if (result == Pal::Result::Success)
    {
        // Mark the queue as invalid. This ensures future queue lookups do not accidentally retrieve it.
        pQueueState->valid = false;

        // Reset + Destroy the fence, then invalidate the pointer.
        PAL_ASSERT(pQueueState->pFence->GetStatus() == Result::Success);
        result = m_pDevice->ResetFences(1, &pQueueState->pFence);
        PAL_ASSERT(result == Pal::Result::Success);
        pQueueState->pFence->Destroy();

        pQueueState->pFence = nullptr;
    }

    if (result == Result::Success)
    {
        // Move all cmdbuffers into the available list
        result = RecycleTimedQueueCmdBuffers(pQueueState);
        PAL_ASSERT(result == Pal::Result::Success);

        // Destroy all measurement command buffers
        while (pQueueState->pAvailableCmdBuffers->NumElements() > 0)
        {
            Pal::ICmdBuffer* pCmdBuffer = nullptr;
            result = pQueueState->pAvailableCmdBuffers->PopFront(&pCmdBuffer);
            PAL_ASSERT(result == Pal::Result::Success);

            PAL_ASSERT(pCmdBuffer != nullptr);

            pCmdBuffer->Destroy();
            PAL_SAFE_FREE(pCmdBuffer, m_pPlatform);
        }
    }
    return result;
}

// =====================================================================================================================
// Injects timing commands into a submission and submits it to pQueue.
Pal::Result GpaSession::TimedSubmit(
    Pal::IQueue*           pQueue,
    const Pal::SubmitInfo& submitInfo,
    const TimedSubmitInfo& timedSubmitInfo)
{
    Pal::Result result = m_flags.enableQueueTiming ? Pal::Result::Success : Pal::Result::ErrorUnavailable;

    TimedQueueState* pQueueState = nullptr;
    Pal::uint32 queueIndex = 0;

    if (result == Pal::Result::Success)
    {
        result = FindTimedQueue(pQueue, &pQueueState, &queueIndex);
    }

    if (result == Pal::Result::Success)
    {
        // Acquire command buffers
        const uint32 numCmdBuffersRequired = (submitInfo.cmdBufferCount + 1);

        Util::Vector<Pal::ICmdBuffer*, 8, GpaAllocator> cmdBufferList(m_pPlatform);

        for (uint32 cmdBufIndex = 0; cmdBufIndex < numCmdBuffersRequired; ++cmdBufIndex)
        {
            Pal::ICmdBuffer* pCmdBuffer = nullptr;
            result = AcquireTimedQueueCmdBuffer(pQueueState, &pCmdBuffer);

            if (result == Pal::Result::Success)
            {
                result = cmdBufferList.PushBack(pCmdBuffer);
            }

            if (result != Pal::Result::Success)
            {
                break;
            }
        }

        Util::Vector<GpuMemoryInfo, 8, GpaAllocator> timestampMemoryInfoList(m_pPlatform);
        Util::Vector<Pal::gpusize, 8, GpaAllocator> timestampMemoryOffsetList(m_pPlatform);

        if (result == Pal::Result::Success)
        {
            // Acquire timestamp memory
            const uint32 numTimestampsRequired = (2 * submitInfo.cmdBufferCount);

            for (uint32 timestampIndex = 0; timestampIndex < numTimestampsRequired; ++timestampIndex)
            {
                GpuMemoryInfo memoryInfo = {};
                Pal::gpusize memoryOffset = 0;

                result = AcquireGpuMem(sizeof(uint64),
                                       m_timestampAlignment,
                                       Pal::GpuHeapGartCacheable,
                                       &memoryInfo,
                                       &memoryOffset);

                if (result == Pal::Result::Success)
                {
                    result = timestampMemoryInfoList.PushBack(memoryInfo);
                }

                if (result == Pal::Result::Success)
                {
                    result = timestampMemoryOffsetList.PushBack(memoryOffset);
                }

                if (result != Pal::Result::Success)
                {
                    break;
                }
            }
        }

        if (result == Pal::Result::Success)
        {
            Util::Vector<Pal::ICmdBuffer*, 8, GpaAllocator> patchedCmdBufferList(m_pPlatform);
            Util::Vector<Pal::CmdBufInfo, 8, GpaAllocator> patchedCmdBufInfoList(m_pPlatform);

            for (uint32 cmdBufIndex = 0; cmdBufIndex < submitInfo.cmdBufferCount; ++cmdBufIndex)
            {
                const Pal::uint32 baseIndex                   = cmdBufIndex * 2;
                const GpuMemoryInfo* pPreTimestampMemoryInfo  = &timestampMemoryInfoList.At(baseIndex);
                const GpuMemoryInfo* pPostTimestampMemoryInfo = &timestampMemoryInfoList.At(baseIndex + 1);
                const Pal::gpusize preTimestampOffset         = timestampMemoryOffsetList.At(baseIndex);
                const Pal::gpusize postTimestampOffset        = timestampMemoryOffsetList.At(baseIndex + 1);

                Pal::ICmdBuffer* pPreCmdBuffer  = cmdBufferList.At(cmdBufIndex);
                Pal::ICmdBuffer* pCurCmdBuffer  = submitInfo.ppCmdBuffers[cmdBufIndex];
                Pal::ICmdBuffer* pPostCmdBuffer = cmdBufferList.At(cmdBufIndex + 1);

                // Sample the current cpu time before building the timing command buffers.
                const uint64 cpuTimestamp = static_cast<uint64>(Util::GetPerfCpuTime());

                // Pre Command Buffer

                Pal::CmdBufferBuildInfo buildInfo     = {};
                buildInfo.flags.optimizeOneTimeSubmit = 1;

                // Only the first cmdbuffer needs to begin the pre-cmdbuffer.
                if (result == Pal::Result::Success)
                {
                    if (cmdBufIndex == 0)
                    {
                        result = pPreCmdBuffer->Begin(buildInfo);
                    }
                }

                if (result == Pal::Result::Success)
                {
                    // The gpu memory pointer should never be null.
                    PAL_ASSERT(pPreTimestampMemoryInfo->pGpuMemory != nullptr);

                    pPreCmdBuffer->CmdWriteTimestamp(Pal::HwPipeTop,
                                                     *pPreTimestampMemoryInfo->pGpuMemory,
                                                     preTimestampOffset);

                    result = pPreCmdBuffer->End();
                }

                // Post Command Buffer

                if (result == Pal::Result::Success)
                {
                    result = pPostCmdBuffer->Begin(buildInfo);
                }

                if (result == Pal::Result::Success)
                {
                    // The gpu memory pointer should never be null.
                    PAL_ASSERT(pPostTimestampMemoryInfo->pGpuMemory != nullptr);

                    pPostCmdBuffer->CmdWriteTimestamp(Pal::HwPipeBottom,
                                                      *pPostTimestampMemoryInfo->pGpuMemory,
                                                      postTimestampOffset);

                    // Only the last cmdbuffer needs to end the post-cmdbuffer.
                    if (cmdBufIndex == (submitInfo.cmdBufferCount - 1))
                    {
                        result = pPostCmdBuffer->End();
                    }
                }

                // If this submit contains command buffer info structs, we need to insert dummy structs for each of
                // the timing command buffers.
                if (submitInfo.pCmdBufInfoList != nullptr)
                {
                    Pal::CmdBufInfo dummyCmdBufInfo = {};
                    dummyCmdBufInfo.isValid = 0;

                    // We only need to add a dummy command buffer info struct before the real one if this is the first
                    // command buffer in the list.
                    if ((result == Pal::Result::Success) && (cmdBufIndex == 0))
                    {
                        result = patchedCmdBufInfoList.PushBack(dummyCmdBufInfo);
                    }

                    if (result == Pal::Result::Success)
                    {
                        result = patchedCmdBufInfoList.PushBack(submitInfo.pCmdBufInfoList[cmdBufIndex]);
                    }

                    if (result == Pal::Result::Success)
                    {
                        result = patchedCmdBufInfoList.PushBack(dummyCmdBufInfo);
                    }
                }

                if (result == Pal::Result::Success)
                {
                    // Only the first cmdbuffer needs to add the pre-cmdbuffer.
                    if (cmdBufIndex == 0)
                    {
                        result = patchedCmdBufferList.PushBack(pPreCmdBuffer);
                    }
                }

                if (result == Pal::Result::Success)
                {
                    result = patchedCmdBufferList.PushBack(pCurCmdBuffer);
                }

                if (result == Pal::Result::Success)
                {
                    result = patchedCmdBufferList.PushBack(pPostCmdBuffer);
                }

                TimedQueueEventItem timedQueueEvent = {};
                timedQueueEvent.eventType           = TimedQueueEventType::Submit;
                timedQueueEvent.cpuTimestamp        = cpuTimestamp;

                timedQueueEvent.apiId =
                    (timedSubmitInfo.pApiCmdBufIds != nullptr) ? timedSubmitInfo.pApiCmdBufIds[cmdBufIndex] : 0;

                timedQueueEvent.sqttCmdBufId =
                    (timedSubmitInfo.pSqttCmdBufIds != nullptr) ? timedSubmitInfo.pSqttCmdBufIds[cmdBufIndex] : 0;

                timedQueueEvent.queueIndex               = queueIndex;
                timedQueueEvent.frameIndex               = timedSubmitInfo.frameIndex;
                timedQueueEvent.submitSubIndex           = cmdBufIndex;
                timedQueueEvent.gpuTimestamps.memInfo[0] = *pPreTimestampMemoryInfo;
                timedQueueEvent.gpuTimestamps.memInfo[1] = *pPostTimestampMemoryInfo;
                timedQueueEvent.gpuTimestamps.offsets[0] = preTimestampOffset;
                timedQueueEvent.gpuTimestamps.offsets[1] = postTimestampOffset;

                if (result == Pal::Result::Success)
                {
                    result = m_queueEvents.PushBack(timedQueueEvent);
                }

                if (result != Pal::Result::Success)
                {
                    break;
                }
            }

            if (result == Pal::Result::Success)
            {
                result = m_pDevice->ResetFences(1, &pQueueState->pFence);
            }

            if (result == Pal::Result::Success)
            {
                Pal::SubmitInfo patchedSubmitInfo = submitInfo;
                patchedSubmitInfo.cmdBufferCount = patchedCmdBufferList.NumElements();
                patchedSubmitInfo.ppCmdBuffers = &patchedCmdBufferList.At(0);

                if (submitInfo.pCmdBufInfoList != nullptr)
                {
                    patchedSubmitInfo.pCmdBufInfoList = &patchedCmdBufInfoList.At(0);
                }

                result = pQueue->Submit(patchedSubmitInfo);
            }

            if (result == Pal::Result::Success)
            {
                result = pQueue->AssociateFenceWithLastSubmit(pQueueState->pFence);
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Injects timing commands into a queue signal operation.
Pal::Result GpaSession::TimedSignalQueueSemaphore(
    Pal::IQueue*                   pQueue,
    IQueueSemaphore*               pQueueSemaphore,
    const TimedQueueSemaphoreInfo& timedSignalInfo)
{
    return TimedQueueSemaphoreOperation(pQueue, pQueueSemaphore, timedSignalInfo, true);
}

// =====================================================================================================================
// Injects timing commands into a queue wait operation.
Pal::Result GpaSession::TimedWaitQueueSemaphore(
    Pal::IQueue*                   pQueue,
    IQueueSemaphore*               pQueueSemaphore,
    const TimedQueueSemaphoreInfo& timedWaitInfo)
{
    return TimedQueueSemaphoreOperation(pQueue, pQueueSemaphore, timedWaitInfo, false);
}

// =====================================================================================================================
// Injects timing commands into a queue present operation.
Pal::Result GpaSession::TimedQueuePresent(
    Pal::IQueue*                   pQueue,
    const TimedQueuePresentInfo&   timedPresentInfo)
{
    Pal::Result result = m_flags.enableQueueTiming ? Pal::Result::Success : Pal::Result::ErrorUnavailable;

    if (result == Pal::Result::Success)
    {
        result = AddCpuGpuTimedQueueEvent(pQueue, TimedQueueEventType::Present, timedPresentInfo.presentID);
    }

    return result;
}

// =====================================================================================================================
// Injects an external event for a queue wait operation.
Pal::Result GpaSession::ExternalTimedWaitQueueSemaphore(
    Pal::uint64                    queueContext,
    Pal::uint64                    cpuSubmissionTimestamp,
    Pal::uint64                    cpuCompletionTimestamp,
    const TimedQueueSemaphoreInfo& timedWaitInfo)
{
    return ExternalTimedQueueSemaphoreOperation(queueContext,
                                                cpuSubmissionTimestamp,
                                                cpuCompletionTimestamp,
                                                timedWaitInfo,
                                                false);
}

// =====================================================================================================================
// Injects an external event for a queue signal operation.
Pal::Result GpaSession::ExternalTimedSignalQueueSemaphore(
    Pal::uint64                    queueContext,
    Pal::uint64                    cpuSubmissionTimestamp,
    Pal::uint64                    cpuCompletionTimestamp,
    const TimedQueueSemaphoreInfo& timedSignalInfo)
{
    return ExternalTimedQueueSemaphoreOperation(queueContext,
                                                cpuSubmissionTimestamp,
                                                cpuCompletionTimestamp,
                                                timedSignalInfo,
                                                true);
}

// =====================================================================================================================
// Samples the timing clocks if queue timing is enabled and adds a clock sample entry to the current session.
Pal::Result GpaSession::SampleTimingClocks()
{
    Pal::Result result = Pal::Result::ErrorUnavailable;

    if (m_flags.enableQueueTiming)
    {
        // Calibrate the cpu and gpu clocks

        Pal::GpuTimestampCalibration timestampCalibration = {};
        result = m_pDevice->CalibrateGpuTimestamp(&timestampCalibration);

        if (result == Result::Success)
        {
            result = m_timestampCalibrations.PushBack(timestampCalibration);
        }

        // Sample the current gpu clock speeds

        SetClockModeInput clockModeInput = {};
        clockModeInput.clockMode = DeviceClockMode::Query;

        SetClockModeOutput clockModeOutput = {};

        if (result == Result::Success)
        {
            result = m_pDevice->SetClockMode(clockModeInput, &clockModeOutput);
        }

        if (result == Result::Success)
        {
            const float maxEngineClock = m_deviceProps.gfxipProperties.performance.maxGpuClock;
            const float maxMemoryClock = m_deviceProps.gpuMemoryProperties.performance.maxMemClock;
            const uint32 engineClock = static_cast<uint32>(maxEngineClock * clockModeOutput.engineClockRatioToPeak);
            const uint32 memoryClock = static_cast<uint32>(maxMemoryClock * clockModeOutput.memoryClockRatioToPeak);

            m_lastGpuClocksSample.gpuEngineClockSpeed = engineClock;
            m_lastGpuClocksSample.gpuMemoryClockSpeed = memoryClock;
        }
    }

    return result;
}

// =====================================================================================================================
// Moves the session from the _reset_ state to the _building_ state.

Result GpaSession::Begin(
    const GpaSessionBeginInfo& info)
{
    Result result = Result::Success;

    if (m_sessionState != GpaSessionState::Reset)
    {
        result = Result::ErrorUnavailable;
    }
    else
    {
        result = m_pGpuEvent->Reset();
    }

    if (result == Result::Success)
    {
        m_flags = info.flags;
    }

    if (result == Result::Success)
    {
        // Update session state if successfully bind gpu memory to gpuEvent
        m_sessionState = GpaSessionState::Building;
    }

    return result;
}

// =====================================================================================================================
// Moves the session from the _building_ state to the _complete_ state.
Result GpaSession::End(
    ICmdBuffer* pCmdBuf)
{
    Result result = Result::Success;

    if (m_sessionState != GpaSessionState::Building)
    {
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        // Copy all SQTT results to CPU accessible memory
        const uint32 numEntries = m_sampleItemArray.NumElements();
        bool needsPostTraceIdle = true;
        for (uint32 i = 0; i < numEntries; i++)
        {
            SampleItem* pSampleItem = m_sampleItemArray.At(i);
            PAL_ASSERT(pSampleItem != nullptr);

            if (pSampleItem->sampleConfig.type == GpaSampleType::Trace)
            {
                if (needsPostTraceIdle)
                {
                    needsPostTraceIdle = false;

                    // Issue a barrier to make sure work being measured is complete before copy
                    BarrierTransition barrierTransition;
                    constexpr HwPipePoint HwPipeBottomConst = HwPipeBottom;

                    barrierTransition.srcCacheMask     = CoherMemory;
                    barrierTransition.dstCacheMask     = CoherCopy;
                    barrierTransition.imageInfo.pImage = nullptr;

                    BarrierInfo barrierInfo = {};

                    barrierInfo.waitPoint          = HwPipePreBlt;
                    barrierInfo.pipePointWaitCount = 1;
                    barrierInfo.pPipePoints        = &HwPipeBottomConst;
                    barrierInfo.transitionCount    = 1;
                    barrierInfo.pTransitions       = &barrierTransition;

                    barrierInfo.reason             = Developer::BarrierReasonPostSqttTrace;

                    pCmdBuf->CmdBarrier(barrierInfo);
                }

                // Add cmd to copy from gpu local invisible memory to Gart heap memory for CPU access.
                static_cast<TraceSample*>(pSampleItem->pPerfSample)->WriteCopyTraceData(pCmdBuf);
            }
        }

        // Mark completion after heap copy cmd finishes
        pCmdBuf->CmdSetEvent(*m_pGpuEvent, HwPipeBottom);
        m_sessionState = GpaSessionState::Complete;

        // Push currently active GPU memory chunk into busy list.
        if (m_curGartGpuMem.pGpuMemory != nullptr)
        {
            m_busyGartGpuMem.PushBack(m_curGartGpuMem);
            m_curGartGpuMem = { nullptr, nullptr };
        }
        if (m_curLocalInvisGpuMem.pGpuMemory != nullptr)
        {
            m_busyLocalInvisGpuMem.PushBack(m_curLocalInvisGpuMem);
            m_curLocalInvisGpuMem = { nullptr, nullptr };
        }

        // Copy all entries in the shader record cache into the current shader records list.
        // Make sure to acquire the pipeline registration lock while we perform this operation to prevent new pipelines
        // from being added to the cache.
        m_registerPipelineLock.LockForWrite();
        auto iter = m_shaderRecordsCache.Begin();
        while (iter.Get())
        {
            m_curShaderRecords.PushBack(*iter.Get());
            iter.Next();
        }
        m_registerPipelineLock.UnlockForWrite();
    }

    return result;
}

// =====================================================================================================================
// Marks the beginning of a range of GPU operations to be measured and specifies what data should be recorded.
uint32 GpaSession::BeginSample(
    ICmdBuffer*            pCmdBuf,
    const GpaSampleConfig& sampleConfig)
{
    PAL_ASSERT(m_sessionState == GpaSessionState::Building);

    Result result = Result::Success;

    uint32      sampleId    = m_sampleCount; // sampleId starts from 0 as resizable array index
    SampleItem* pSampleItem = nullptr;

    // Validate sample type
    if ((sampleConfig.type != GpaSampleType::Cumulative) &&
        (sampleConfig.type != GpaSampleType::Trace)      &&
        (sampleConfig.type != GpaSampleType::Timing)     &&
        (sampleConfig.type != GpaSampleType::Query))
    {
        // Undefined sample type
        result = Result::Unsupported;
    }

    if (result == Result::Success)
    {
        // Create instance for map entry
        pSampleItem = static_cast<SampleItem*>(PAL_CALLOC(sizeof(SampleItem),
                                                          m_pPlatform,
                                                          Util::SystemAllocType::AllocObject));
        if (pSampleItem != nullptr)
        {
            // Save a copy of the sample config struct
            pSampleItem->sampleConfig = sampleConfig;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        // Cumulative/Trace mode branch
        if ((pSampleItem->sampleConfig.type == GpaSampleType::Cumulative) ||
            (pSampleItem->sampleConfig.type == GpaSampleType::Trace))
        {
            GpuMemoryInfo primaryGpuMemInfo   = {};
            GpuMemoryInfo secondaryGpuMemInfo = {};
            gpusize       primaryOffset       = 0;
            gpusize       secondaryOffset     = 0;
            gpusize       heapSize            = 0;

            // Get an idle performance experiment from the queue's pool.
            IPerfExperiment* pPerfExperiment = nullptr;
            result = AcquirePerfExperiment(sampleConfig,
                                           &primaryGpuMemInfo,
                                           &primaryOffset,
                                           &secondaryGpuMemInfo,
                                           &secondaryOffset,
                                           &heapSize,
                                           &pPerfExperiment);

            if (result == Result::Success)
            {
                PAL_ASSERT(pPerfExperiment != nullptr);

                pSampleItem->pPerfExperiment = pPerfExperiment;

                if (pSampleItem->sampleConfig.type == GpaSampleType::Cumulative)
                {
                    // CounterSample initialization.
                    CounterSample* pCtrSample = PAL_NEW(CounterSample, m_pPlatform, Util::SystemAllocType::AllocObject)
                                                       (m_pDevice, pPerfExperiment, m_pPlatform);
                    if (pCtrSample != nullptr)
                    {
                        pSampleItem->pPerfSample = pCtrSample;
                        pCtrSample->SetSampleMemoryProperties(secondaryGpuMemInfo, secondaryOffset, heapSize);

                        result = pCtrSample->Init(pSampleItem->sampleConfig.perfCounters.numCounters);
                    }
                    else
                    {
                        result = Result::ErrorOutOfMemory;
                    }
                }
                else if (pSampleItem->sampleConfig.type == GpaSampleType::Trace)
                {
                    // TraceSample initialization
                    TraceSample* pTraceSample = PAL_NEW(TraceSample, m_pPlatform, Util::SystemAllocType::AllocObject)
                                                        (m_pDevice, pPerfExperiment, m_pPlatform);
                    if (pTraceSample != nullptr)
                    {
                        pSampleItem->pPerfSample = pTraceSample;
                        pTraceSample->SetSampleMemoryProperties(secondaryGpuMemInfo, secondaryOffset, heapSize);
                        pTraceSample->SetTraceMemory(primaryGpuMemInfo, primaryOffset, heapSize);

                        // Initialize the thread trace portion of the TraceSample.
                        if (pSampleItem->sampleConfig.sqtt.flags.enable)
                        {
                            result = pTraceSample->InitThreadTrace(
                                        m_deviceProps.gfxipProperties.shaderCore.numShaderEngines);
                        }

                        // Spm trace is enabled, so init the Spm trace portion of the TraceSample.
                        if (pSampleItem->sampleConfig.perfCounters.numCounters > 0)
                        {
                            result = pTraceSample->InitSpmTrace(pSampleItem->sampleConfig.perfCounters.numCounters);
                        }
                    }
                    else
                    {
                        result = Result::ErrorOutOfMemory;
                    }
                }
            }

            if (result == Result::Success)
            {
                // Begin the perf experiment once the samples have been successfully initialized.
                // The perf experiment has been configured with perf counters/trace at this point.
                pCmdBuf->CmdBeginPerfExperiment(pPerfExperiment);
            }
        }
        // TimingSample initialization. This sample does not uses PerfExperiment.
        else if (pSampleItem->sampleConfig.type == GpaSampleType::Timing)
        {
            // NOTE: client is responsible for checking if the engine supports timestamp.
            // Create a cumulative perf sample.
            TimingSample* pTimingSample = PAL_NEW (TimingSample, m_pPlatform, Util::SystemAllocType::AllocObject)
                                                  (m_pDevice, nullptr, m_pPlatform);
            if (pTimingSample != nullptr)
            {
                pSampleItem->pPerfSample = pTimingSample;

                GpuMemoryInfo gpuMemInfo;
                gpusize       offset;

                // Acquire GPU memory for both pre-call/post-call timestamp in one chunk, so later we just need to
                // copy the results once.
                result = AcquireGpuMem(m_timestampAlignment + sizeof(uint64),
                                       m_timestampAlignment,
                                       GpuHeapGartCacheable,
                                       &gpuMemInfo,
                                       &offset);

                pTimingSample->SetTimestampMemoryInfo(gpuMemInfo, offset, m_timestampAlignment);
                pTimingSample->Init(sampleConfig.timing.preSample, sampleConfig.timing.postSample);
                pCmdBuf->CmdWriteTimestamp(sampleConfig.timing.preSample, *gpuMemInfo.pGpuMemory, offset);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
        // QuerySample initialization. This sample does not uses PerfExperiment.
        else if (pSampleItem->sampleConfig.type == GpaSampleType::Query)
        {
            GpuMemoryInfo gpuMemInfo   = {};
            gpusize       offset       = 0;
            gpusize       heapSize     = 0;

            // Get an idle query.
            IQueryPool* pPipeStatsQuery = nullptr;
            result = AcquirePipeStatsQuery(&gpuMemInfo,
                                           &offset,
                                           &heapSize,
                                           &pPipeStatsQuery);

            if (result == Result::Success)
            {
                PAL_ASSERT(pPipeStatsQuery != nullptr);

                QuerySample* pQuerySample = PAL_NEW(QuerySample, m_pPlatform, Util::SystemAllocType::AllocObject)
                    (m_pDevice, nullptr, m_pPlatform);

                if (pQuerySample != nullptr)
                {
                    pSampleItem->pPerfSample = pQuerySample;
                    pQuerySample->SetPipeStatsQuery(pPipeStatsQuery);
                    pQuerySample->SetSampleMemoryProperties(gpuMemInfo, offset, heapSize);

                    // Reset and begin the query.
                    const QueryControlFlags flags = {};
                    pCmdBuf->CmdResetQueryPool(*pQuerySample->GetPipeStatsQuery(), 0, 1);
                    pCmdBuf->CmdBeginQuery(*pQuerySample->GetPipeStatsQuery(), QueryType::PipelineStats, 0, flags);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
        }
    }

    if (result == Result::Success)
    {
        // Finally add <sampleId,SampleItem> pair to the Map
        m_sampleItemArray.PushBack(pSampleItem);

        m_sampleCount++;
        PAL_ASSERT(m_sampleCount == m_sampleItemArray.NumElements());
    }
    else
    {
        sampleId = InvalidSampleId;
    }

    return sampleId;
}

// =====================================================================================================================
// Updates the trace parameters for a specific sample.
Pal::Result GpaSession::UpdateSampleTraceParams(
    Pal::ICmdBuffer* pCmdBuf,
    Pal::uint32      sampleId)
{
    PAL_ASSERT(m_sessionState == GpaSessionState::Building);
    PAL_ASSERT(m_flags.enableSampleUpdates);

    Pal::Result result = Pal::Result::ErrorInvalidPointer;

    if (pCmdBuf != nullptr)
    {
        SampleItem* pSampleItem = m_sampleItemArray.At(sampleId);
        PAL_ASSERT(pSampleItem != nullptr);

        if (pSampleItem->sampleConfig.type == GpaSampleType::Trace)
        {
            const bool skipInstTokens = pSampleItem->sampleConfig.sqtt.flags.supressInstructionTokens;
            const uint32 tokenMask    = skipInstTokens ? SqttTokenMaskNoInst : SqttTokenMaskAll;
            pCmdBuf->CmdUpdatePerfExperimentSqttTokenMask(pSampleItem->pPerfExperiment,
                                                          tokenMask);

            result = Result::Success;
        }
        else
        {
            result = Pal::Result::ErrorInvalidObjectType;
        }
    }

    return result;
}

// =====================================================================================================================
// Marks the end of a range of command buffer operations to be measured.
void GpaSession::EndSample(
    ICmdBuffer* pCmdBuf,
    uint32      sampleId)
{
    PAL_ASSERT(m_sessionState == GpaSessionState::Building);

    SampleItem* pSampleItem = m_sampleItemArray.At(sampleId);
    PAL_ASSERT(pSampleItem != nullptr);

    if ((pSampleItem->sampleConfig.type == GpaSampleType::Cumulative) ||
        (pSampleItem->sampleConfig.type == GpaSampleType::Trace))
    {
        IPerfExperiment* pPerfExperiment = pSampleItem->pPerfExperiment;
        PAL_ASSERT(pPerfExperiment != nullptr);
        pCmdBuf->CmdEndPerfExperiment(pPerfExperiment);
    }
    else if (pSampleItem->sampleConfig.type == GpaSampleType::Timing)
    {
        TimingSample* pSample = static_cast<TimingSample*>(pSampleItem->pPerfSample);

        pCmdBuf->CmdWriteTimestamp(pSample->GetPostSamplePoint(),
                                   *(pSample->GetEndTsGpuMem()),
                                   pSample->GetEndTsGpuMemOffset());
    }
    else if (pSampleItem->sampleConfig.type == GpaSampleType::Query)
    {
        QuerySample* pSample = static_cast<QuerySample*>(pSampleItem->pPerfSample);
        IQueryPool* pQuery   = pSample->GetPipeStatsQuery();
        PAL_ASSERT(pQuery != nullptr);
        pCmdBuf->CmdEndQuery(*pQuery, QueryType::PipelineStats, 0);
    }
    else
    {
        PAL_NEVER_CALLED(); // beginSample prevents undefined-mode sample to be added to the list.
        // TODO: Record error code in SampleItem, and return error result at End().
    }
}

// =====================================================================================================================
// Reports if GPU execution of this session has completed and results are _ready_ for querying from the CPU via
// GetResults().
bool GpaSession::IsReady() const
{
    bool isReady = true;

    Result result = m_pGpuEvent->GetStatus();
    PAL_ASSERT((result == Result::EventSet) || (result == Result::EventReset));
    if (result != Result::EventSet)
    {
        isReady = false;
    }
    else if (m_flags.enableQueueTiming)
    {
        // Make sure all of the queue fences have retired.
        for (uint32 queueIndex = 0; queueIndex < m_timedQueuesArray.NumElements(); ++queueIndex)
        {
            const TimedQueueState* pQueueState = m_timedQueuesArray.At(queueIndex);
            if (pQueueState->pFence != nullptr)
            {
                if (pQueueState->pFence->GetStatus() == Pal::Result::NotReady)
                {
                    isReady = false;
                    break;
                }
            }
        }
    }

    return isReady;
}

// =====================================================================================================================
// Reports results of a particular sample.  Only valid for sessions in the _ready_ state.
Result GpaSession::GetResults(
    uint32  sampleId,
    size_t* pSizeInBytes,
    void*   pData
    ) const
{
    PAL_ASSERT(m_sessionState == GpaSessionState::Complete);

    Result result = Result::Success;

    SampleItem* pSampleItem = m_sampleItemArray.At(sampleId);

    if (pSampleItem->sampleConfig.type == GpaSampleType::Cumulative)
    {
        CounterSample* pSample = static_cast<CounterSample*>(pSampleItem->pPerfSample);

        result = pSample->GetCounterResults(pData, pSizeInBytes);
    }
    else if (pSampleItem->sampleConfig.type == GpaSampleType::Trace)
    {
        // Thread trace results.
        TraceSample* pTraceSample = static_cast<TraceSample*>(pSampleItem->pPerfSample);

        if (pTraceSample->GetTraceBufferSize() > 0)
        {
            if (pSizeInBytes == nullptr)
            {
                result = Result::ErrorInvalidPointer;
            }

            if ((result == Result::Success) &&
                (pTraceSample->IsThreadTraceEnabled() || pTraceSample->IsSpmTraceEnabled()))
            {
                // The client is expected to query size or provide size of data already in the buffer.
                PAL_ASSERT(pSizeInBytes != nullptr);

                // Dump both thread trace and spm trace results in the RGP file.
                result = DumpRgpData(pTraceSample, pData, pSizeInBytes);
            }
        }
    }
    else if (pSampleItem->sampleConfig.type == GpaSampleType::Timing)
    {
        TimingSample* pSample = static_cast<TimingSample*>(pSampleItem->pPerfSample);

        result = pSample->GetTimingSampleResults(pData, pSizeInBytes);
    }
    else if (pSampleItem->sampleConfig.type == GpaSampleType::Query)
    {
        QuerySample* pSample = static_cast<QuerySample*>(pSampleItem->pPerfSample);

        result = pSample->GetQueryResults(pData, pSizeInBytes);
    }
    else
    {
        result = Result::Unsupported;
    }

    return result;
}

// =====================================================================================================================
// Moves the session to the _reset_ state, marking all sessions resources as unused and available for reuse when
// the session is re-built.
Result GpaSession::Reset()
{
    Result result = Result::Success;
    if (m_sessionState == GpaSessionState::Building)
    {
        result = Result::NotReady;
    }
    else if (m_pSrcSession != nullptr)
    {
        // A copy session cannot be reset
        result = Result::Unsupported;
    }

    if (result == Pal::Result::Success)
    {
        // Reset all TimedQueueState objects
        for (uint32 queueIndex = 0; queueIndex < m_timedQueuesArray.NumElements(); ++queueIndex)
        {
            result = ResetTimedQueueState(m_timedQueuesArray.At(queueIndex));
            if (result != Pal::Result::Success)
            {
                break;
            }
        }
    }

    if (result == Pal::Result::Success)
    {
        m_queueEvents.Clear();
        m_timestampCalibrations.Clear();
        result = m_pCmdAllocator->Reset();
    }

    if (result == Pal::Result::Success)
    {
        // Clear the current shader records.
        while (m_curShaderRecords.NumElements() > 0)
        {
            ShaderRecord shaderRecord = {};
            m_curShaderRecords.PopFront(&shaderRecord);
        }

        // Recycle Gart gpu memory allocations, gpu rafts are reserved
        RecycleGartGpuMem();
        m_curGartGpuMem.pGpuMemory = nullptr;
        m_curGartGpuMem.pCpuAddr   = nullptr;
        m_curGartGpuMemOffset      = 0;

        // Recycle invisible gpu memory allocation, gpu rafts are reserved
        RecycleLocalInvisGpuMem();
        m_curLocalInvisGpuMem.pGpuMemory = nullptr;
        m_curLocalInvisGpuMem.pCpuAddr   = nullptr;
        m_curLocalInvisGpuMemOffset      = 0;

        // Free each sampleItem
        FreeSampleItemArray();

        // Reset counter of session-owned samples
        m_sampleCount = 0;

        // Reset flags
        m_flags.u32All = 0;

        // Reset session state
        m_sessionState = GpaSessionState::Reset;
    }

    return result;
}

// =====================================================================================================================
// Uses the GPU to copy results from a nested command buffer's session into a root-level command buffer's per-
// invocation session data.
void GpaSession::CopyResults(
    ICmdBuffer* pCmdBuf)
{
    Result result = Result::Success;

    if (m_sessionState != GpaSessionState::Complete) // Implies the source session is at least at complete stage
    {
        result = Result::ErrorUnavailable;
    }

    if (result == Result::Success)
    {
        // Issue a barrier to make sure work being measured is complete before copy
        BarrierTransition barrierTransition;

        barrierTransition.srcCacheMask = CoherCopy | CoherMemory; // Counter | SQTT
        barrierTransition.dstCacheMask = CoherCopy;
        barrierTransition.imageInfo.pImage = nullptr;

        BarrierInfo barrierInfo = {};
        const IGpuEvent* pGpuEventsConst = m_pSrcSession->m_pGpuEvent;

        barrierInfo.waitPoint         = HwPipePreBlt;
        barrierInfo.gpuEventWaitCount = 1;
        barrierInfo.ppGpuEvents       = &pGpuEventsConst;
        barrierInfo.transitionCount   = 1;
        barrierInfo.pTransitions      = &barrierTransition;

        barrierInfo.reason            = Developer::BarrierReasonPrePerfDataCopy;

        pCmdBuf->CmdBarrier(barrierInfo);

        // copy each perfExperiment result from source session to this copy session
        for (uint32 i = 0; i < m_sampleItemArray.NumElements(); i++)
        {
            SampleItem* pSampleItem = m_sampleItemArray.At(i);

            // Add cmd to copy from gpu source session's heap to copy session's heap
            PerfSample* pPerfSample = pSampleItem->pPerfSample;

            if (pPerfSample != nullptr)
            {
                // Ask each sample to write commands to copy from the src sample data to its sample data buffer.
                pPerfSample->WriteCopySampleData(pCmdBuf);
            }

        }

        // Mark completion after heap copy cmd finishes
        pCmdBuf->CmdSetEvent(*m_pGpuEvent, HwPipeBottom);
        m_sessionState = GpaSessionState::Complete;
    }

    PAL_ALERT(result != Result::Success);
}

// =====================================================================================================================
// Finds the TimedQueueState associated with pQueue.
Pal::Result GpaSession::FindTimedQueue(
    Pal::IQueue*      pQueue,
    TimedQueueState** ppQueueState,
    Pal::uint32*      pQueueIndex)
{
    Pal::Result result = Pal::Result::ErrorInvalidPointer;

    if ((ppQueueState != nullptr) & (pQueueIndex != nullptr))
    {
        for (Pal::uint32 queueIndex = 0; queueIndex < m_timedQueuesArray.NumElements(); ++queueIndex)
        {
            TimedQueueState* pQueueState = m_timedQueuesArray.At(queueIndex);
            if ((pQueueState->valid) & (pQueueState->pQueue == pQueue))
            {
                *ppQueueState = pQueueState;
                *pQueueIndex = queueIndex;

                result = Pal::Result::Success;

                break;
            }
        }

        if (result != Pal::Result::Success)
        {
            result = Pal::Result::ErrorIncompatibleQueue;
        }
    }

    return result;
}

// =====================================================================================================================
// Finds the TimedQueueState associated with queueContext.
Pal::Result GpaSession::FindTimedQueueByContext(
    Pal::uint64       queueContext,
    TimedQueueState** ppQueueState,
    Pal::uint32*      pQueueIndex)
{
    Pal::Result result = Pal::Result::ErrorInvalidPointer;

    if ((ppQueueState != nullptr) & (pQueueIndex != nullptr))
    {
        for (Pal::uint32 queueIndex = 0; queueIndex < m_timedQueuesArray.NumElements(); ++queueIndex)
        {
            TimedQueueState* pQueueState = m_timedQueuesArray.At(queueIndex);
            if ((pQueueState->valid) & (pQueueState->queueContext == queueContext))
            {
                *ppQueueState = pQueueState;
                *pQueueIndex = queueIndex;

                result = Pal::Result::Success;

                break;
            }
        }

        if (result != Pal::Result::Success)
        {
            result = Pal::Result::ErrorIncompatibleQueue;
        }
    }

    return result;
}

// =====================================================================================================================
// Executes a timed queue semaphore operation.
Pal::Result GpaSession::TimedQueueSemaphoreOperation(
    Pal::IQueue*                   pQueue,
    Pal::IQueueSemaphore*          pQueueSemaphore,
    const TimedQueueSemaphoreInfo& timedSemaphoreInfo,
    bool                           isSignalOperation)
{
    Pal::Result result = m_flags.enableQueueTiming ? Pal::Result::Success : Pal::Result::ErrorUnavailable;

    if (result == Pal::Result::Success)
    {
        result = isSignalOperation ? pQueue->SignalQueueSemaphore(pQueueSemaphore)
                                   : pQueue->WaitQueueSemaphore(pQueueSemaphore);
    }

    if (result == Pal::Result::Success)
    {
        const TimedQueueEventType eventType = isSignalOperation ? TimedQueueEventType::Signal :
                                                                  TimedQueueEventType::Wait;
        const uint64 apiId = timedSemaphoreInfo.semaphoreID;
        result             = AddCpuGpuTimedQueueEvent(pQueue, eventType, apiId);
    }

    return result;
}

// =====================================================================================================================
// Injects an external timed queue semaphore operation event
Pal::Result GpaSession::ExternalTimedQueueSemaphoreOperation(
    Pal::uint64                    queueContext,
    Pal::uint64                    cpuSubmissionTimestamp,
    Pal::uint64                    cpuCompletionTimestamp,
    const TimedQueueSemaphoreInfo& timedSemaphoreInfo,
    bool                           isSignalOperation)
{
    Pal::Result result = m_flags.enableQueueTiming ? Pal::Result::Success : Pal::Result::ErrorUnavailable;

    TimedQueueState* pQueueState = nullptr;
    Pal::uint32 queueIndex = 0;

    if (result == Pal::Result::Success)
    {
        result = FindTimedQueueByContext(queueContext, &pQueueState, &queueIndex);
    }

    if (result == Pal::Result::Success)
    {
        // Build the timed queue event struct and add it to our queue events list.
        TimedQueueEventItem timedQueueEvent    = {};

        timedQueueEvent.eventType              = isSignalOperation ? TimedQueueEventType::ExternalSignal
                                                                   : TimedQueueEventType::ExternalWait;
        timedQueueEvent.cpuTimestamp           = cpuSubmissionTimestamp;
        timedQueueEvent.cpuCompletionTimestamp = cpuCompletionTimestamp;
        timedQueueEvent.apiId                  = timedSemaphoreInfo.semaphoreID;
        timedQueueEvent.queueIndex             = queueIndex;

        result = m_queueEvents.PushBack(timedQueueEvent);
    }

    return result;
}

// =====================================================================================================================
// Helper function to sample CPU & GPU timestamp, and insert a timed queue operation event.
Pal::Result GpaSession::AddCpuGpuTimedQueueEvent(
    Pal::IQueue*        pQueue,
    TimedQueueEventType eventType,
    uint64              apiId)
{
    TimedQueueState* pQueueState = nullptr;
    Pal::uint32 queueIndex       = 0;

    Pal::Result result = FindTimedQueue(pQueue, &pQueueState, &queueIndex);

    // Sample the current cpu time before building the command buffer.
    const uint64 cpuTimestamp = static_cast<uint64>(Util::GetPerfCpuTime());

    // Acquire a measurement command buffer
    Pal::ICmdBuffer* pCmdBuffer = nullptr;
    if (result == Pal::Result::Success)
    {
        result = AcquireTimedQueueCmdBuffer(pQueueState, &pCmdBuffer);
    }

    // Acquire memory for the timestamp and a fence to track when the queue semaphore operation completes.
    GpuMemoryInfo timestampMemoryInfo  = {};
    Pal::gpusize timestampMemoryOffset = 0;
    if (result == Pal::Result::Success)
    {
        result = AcquireGpuMem(sizeof(uint64),
                               m_timestampAlignment,
                               Pal::GpuHeapGartCacheable,
                               &timestampMemoryInfo,
                               &timestampMemoryOffset);
    }

    // Begin the measurement command buffer
    if (result == Pal::Result::Success)
    {
        Pal::CmdBufferBuildInfo buildInfo     = {};
        buildInfo.flags.optimizeOneTimeSubmit = 1;

        result = pCmdBuffer->Begin(buildInfo);
    }

    // Record the commands for the measurement buffer and close it
    if (result == Pal::Result::Success)
    {
        // The gpu memory pointer should never be null.
        PAL_ASSERT(timestampMemoryInfo.pGpuMemory != nullptr);

        pCmdBuffer->CmdWriteTimestamp(Pal::HwPipeTop, *timestampMemoryInfo.pGpuMemory, timestampMemoryOffset);

        result = pCmdBuffer->End();
    }

    if (result == Pal::Result::Success)
    {
        result = m_pDevice->ResetFences(1, &pQueueState->pFence);
    }

    if (result == Pal::Result::Success)
    {
        // Submit the measurement command buffer
        Pal::SubmitInfo submitInfo = {};
        submitInfo.cmdBufferCount  = 1;
        submitInfo.ppCmdBuffers    = &pCmdBuffer;
        submitInfo.pFence          = pQueueState->pFence;

        result = pQueue->Submit(submitInfo);
    }

    if (result == Pal::Result::Success)
    {
        // Build the timed queue event struct and add it to our queue events list.
        TimedQueueEventItem timedQueueEvent      = {};
        timedQueueEvent.eventType                = eventType;
        timedQueueEvent.cpuTimestamp             = cpuTimestamp;
        timedQueueEvent.apiId                    = apiId;
        timedQueueEvent.queueIndex               = queueIndex;
        timedQueueEvent.gpuTimestamps.memInfo[0] = timestampMemoryInfo;
        timedQueueEvent.gpuTimestamps.offsets[0] = timestampMemoryOffset;

        result = m_queueEvents.PushBack(timedQueueEvent);
    }

    return result;
}

// =====================================================================================================================
// Converts a CPU timestamp to a GPU timestamp using a GpuTimestampCalibration struct
Pal::uint64 GpaSession::ConvertCpuTimestampToGpuTimestamp(
    Pal::uint64                         cpuTimestamp,
    const Pal::GpuTimestampCalibration& calibration
    ) const
{
    const Pal::uint64 cpuTimestampFrequency = static_cast<Pal::uint64>(Util::GetPerfFrequency());
    const Pal::uint64 gpuTimestampFrequency = m_deviceProps.timestampFrequency;

    // Convert from host time into wall time.
    const Pal::int64 signedHostClock = static_cast<Pal::int64>(cpuTimestamp);
    const Pal::int64 rebasedHostClock = signedHostClock - calibration.cpuWinPerfCounter;
    const double deltaInMicro =
        static_cast<double>(rebasedHostClock) / static_cast<double>((cpuTimestampFrequency / 1000));

    // Take the wall time delta and scale that into global clock.
    const double deltaInGlobalClock = deltaInMicro * static_cast<double>(gpuTimestampFrequency / 1000);
    const Pal::int64 globalClockTimestamp = static_cast<Pal::int64>(deltaInGlobalClock) + calibration.gpuTimestamp;
    return static_cast<Pal::uint64>(globalClockTimestamp);

}

// =====================================================================================================================
// Extracts a GPU timestamp from a queue event
Pal::uint64 GpaSession::ExtractGpuTimestampFromQueueEvent(
    const TimedQueueEventItem& queueEvent
    ) const
{
    // There should always be at least one timestamp calibration chunk if we're writing external
    // signal/wait event events into the file.
    PAL_ASSERT(m_timestampCalibrations.NumElements() > 0);

    // Always use the last calibration value since that's how RGP currently does this.
    const Pal::GpuTimestampCalibration& calibration =
        m_timestampCalibrations.At(m_timestampCalibrations.NumElements() - 1);

    const Pal::uint64 gpuTimestamp =
        ConvertCpuTimestampToGpuTimestamp(queueEvent.cpuCompletionTimestamp, calibration);

    return gpuTimestamp;
}

// =====================================================================================================================
// Creates a new command buffer for use on pQueue
Pal::Result GpaSession::CreateCmdBufferForQueue(
    Pal::IQueue*      pQueue,
    Pal::ICmdBuffer** ppCmdBuffer)
{
    Pal::Result result = Pal::Result::ErrorInvalidPointer;

    if (ppCmdBuffer != nullptr)
    {
        Pal::CmdBufferCreateInfo createInfo = {};
        createInfo.pCmdAllocator = m_pCmdAllocator;
        createInfo.queueType     = pQueue->Type();
        createInfo.engineType    = pQueue->GetEngineType();

        const size_t cmdBufferSize = m_pDevice->GetCmdBufferSize(createInfo, &result);
        if (result == Pal::Result::Success)
        {
            void* pMemory = PAL_CALLOC(cmdBufferSize,
                                       m_pPlatform,
                                       Util::AllocObject);
            if (pMemory == nullptr)
            {
                result = Pal::Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateCmdBuffer(createInfo, pMemory, ppCmdBuffer);

                if (result != Pal::Result::Success)
                {
                    PAL_SAFE_FREE(pMemory, m_pPlatform);
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Acquires a command buffer from the TimedQueueState's command buffer pool
Pal::Result GpaSession::AcquireTimedQueueCmdBuffer(
    TimedQueueState*  pQueueState,
    Pal::ICmdBuffer** ppCmdBuffer)
{
    Pal::Result result = Pal::Result::ErrorInvalidPointer;

    if ((pQueueState != nullptr) && (ppCmdBuffer != nullptr))
    {
        if (pQueueState->pAvailableCmdBuffers->NumElements() > 0)
        {
            // Use an idle cmdbuffer from the pool if available.
            result = pQueueState->pAvailableCmdBuffers->PopFront(ppCmdBuffer);
        }
        else
        {
            // No cmdbuffers are currently idle (or possibly none exist at all) - allocate a new cmdbuffer.
            result = CreateCmdBufferForQueue(pQueueState->pQueue, ppCmdBuffer);
        }

        if (result == Pal::Result::Success)
        {
            result = pQueueState->pBusyCmdBuffers->PushBack(*ppCmdBuffer);
            PAL_ASSERT(result == Pal::Result::Success);
        }
    }

    return result;
}

// =====================================================================================================================
// Recycles busy command buffers in pQueueState
// It is the caller's responsibility to ensure that the command buffers are completed before calling this function
Pal::Result GpaSession::RecycleTimedQueueCmdBuffers(
    TimedQueueState* pQueueState)
{
    Pal::Result result = Pal::Result::Success;
    while (pQueueState->pBusyCmdBuffers->NumElements() > 0)
    {
        Pal::ICmdBuffer* pCmdBuffer = nullptr;

        result = pQueueState->pBusyCmdBuffers->PopFront(&pCmdBuffer);
        PAL_ASSERT(result == Pal::Result::Success);

        result = pQueueState->pAvailableCmdBuffers->PushBack(pCmdBuffer);
        PAL_ASSERT(result == Pal::Result::Success);
    }

    return result;
}

// =====================================================================================================================
// Preallocates a fixed number of command buffers for pQueueState and adds them to the command buffer pool
Pal::Result GpaSession::PreallocateTimedQueueCmdBuffers(
    TimedQueueState * pQueueState,
    uint32            numCmdBuffers)
{
    Pal::Result result = Pal::Result::Success;

    for (uint32 cmdBufIndex = 0; cmdBufIndex < numCmdBuffers; ++cmdBufIndex)
    {
        Pal::ICmdBuffer* pCmdBuffer = nullptr;
        result = CreateCmdBufferForQueue(pQueueState->pQueue, &pCmdBuffer);

        if (result == Pal::Result::Success)
        {
            result = pQueueState->pAvailableCmdBuffers->PushBack(pCmdBuffer);
            if (result != Pal::Result::Success)
            {
                pCmdBuffer->Destroy();
                PAL_SAFE_FREE(pCmdBuffer, m_pPlatform);
                break;
            }
        }
        else
        {
            break;
        }
    }

    return result;
}

// =====================================================================================================================
// Resets all per session state in pQueueState
Pal::Result GpaSession::ResetTimedQueueState(
    TimedQueueState* pQueueState)
{
    Pal::Result result = RecycleTimedQueueCmdBuffers(pQueueState);
    PAL_ASSERT(result == Pal::Result::Success);

    for (auto iter = pQueueState->pAvailableCmdBuffers->Begin(); iter.Get() != nullptr; iter.Next())
    {
        result = (*iter.Get())->Reset(m_pCmdAllocator, true);
        PAL_ASSERT(result == Pal::Result::Success);
    }

    return result;
}

// =====================================================================================================================
// Destroys the memory and resources for pQueueState
void GpaSession::DestroyTimedQueueState(
    TimedQueueState* pQueueState)
{
    // Move all cmdbuffers into the available list
    Pal::Result result = RecycleTimedQueueCmdBuffers(pQueueState);
    PAL_ASSERT(result == Pal::Result::Success);

    // Destroy all measurement command buffers
    while (pQueueState->pAvailableCmdBuffers->NumElements() > 0)
    {
        Pal::ICmdBuffer* pCmdBuffer = nullptr;
        result = pQueueState->pAvailableCmdBuffers->PopFront(&pCmdBuffer);
        PAL_ASSERT(result == Pal::Result::Success);

        PAL_ASSERT(pCmdBuffer != nullptr);

        pCmdBuffer->Destroy();
        PAL_SAFE_FREE(pCmdBuffer, m_pPlatform);
    }

    // Destroy the command buffer arrays
    pQueueState->pAvailableCmdBuffers->~Deque();
    pQueueState->pBusyCmdBuffers->~Deque();

    // Destroy the fence
    if (pQueueState->pFence != nullptr)
    {
        pQueueState->pFence->Destroy();
    }

    // Destroy the queue state memory
    PAL_SAFE_FREE(pQueueState, m_pPlatform);
}

// =====================================================================================================================
// Registers a pipeline with the GpaSession. Returns ErrorInvalidPointer on duplicate pipeline.
Result GpaSession::RegisterPipeline(
    const IPipeline* pPipeline)
{
    Result result = Result::Success;

    // Save IPipeline* to the data-structure.
    PAL_ASSERT(pPipeline != nullptr);

    PipelineInfo pipeInfo = pPipeline->GetInfo();

    m_registerPipelineLock.LockForWrite();

    if (m_registeredPipelines.Contains(pipeInfo.pipelineHash) == false)
    {
        m_registeredPipelines.Insert(pipeInfo.pipelineHash);
    }
    else
    {
        result = Result::AlreadyExists;
    }
    m_registerPipelineLock.UnlockForWrite();

    if (result == Result::Success)
    {
        // Local copy of shader data that will later be copied into the shaderRecords list under lock.
        ShaderRecord pShaderRecords[NumShaderTypes];
        uint32 numShaders = 0;

        for (uint32 i = 0; ((i < NumShaderTypes) && (result == Result::Success)); ++i)
        {
            // Extract shader data from the pipeline.
            // The upper 64-bits of the shader hash can be 0 when 64-bit CRCs are used
            if (ShaderHashIsNonzero(pipeInfo.shader[i].hash))
            {
                result = CreateShaderRecord(static_cast<ShaderType>(i), pPipeline, &pShaderRecords[numShaders]);
                PAL_ASSERT(result == Result::Success);

                ++numShaders;
            }
        }

        if (result == Result::Success)
        {
            m_registerPipelineLock.LockForWrite();

            for (uint32 i = 0; i < numShaders; ++i)
            {
                result = m_shaderRecordsCache.PushBack(pShaderRecords[i]);
            }

            m_registerPipelineLock.UnlockForWrite();
        }
    }

    return result;
}

// =====================================================================================================================
// Helper function to import one sample item from a source session to copy session.
Result GpaSession::ImportSampleItem(
    const SampleItem* pSrcSampleItem)
{
    Result result = Result::Success;

    SampleItem* pSampleItem = nullptr;

    if (result == Result::Success)
    {
        // Create instance for map entry
        pSampleItem = static_cast<SampleItem*>(PAL_CALLOC(sizeof(SampleItem),
                                                            m_pPlatform,
                                                            Util::SystemAllocType::AllocObject));
        if (pSampleItem == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        // Import sampleConfig
        pSampleItem->sampleConfig = pSrcSampleItem->sampleConfig;

        if ((pSampleItem->sampleConfig.type == GpaSampleType::Cumulative) ||
            (pSampleItem->sampleConfig.type == GpaSampleType::Trace))
        {
            // Allocate gpu memory for the copy session
            GpuMemoryRequirements gpuMemReqs = {};
            pSrcSampleItem->pPerfExperiment->GetGpuMemoryRequirements(&gpuMemReqs);

            GpuMemoryInfo gpuMemInfo = {};
            gpusize       offset     = 0;

            result = AcquireGpuMem(gpuMemReqs.size,
                                    gpuMemReqs.alignment,
                                    GpuHeapGartCacheable,
                                    &gpuMemInfo,
                                    &offset);

            if (result == Result::Success)
            {
                PAL_ASSERT(gpuMemInfo.pGpuMemory != nullptr);

                // Create and initialize a new PerfSample based on properties of the src sample of the
                // same type. Once the source session's sample data GPU memory location is copied, later
                // on during the lifetime of the GpaSession, a GPU copy is issued which essentially
                // copies the sample data from the src session to the copy session.
                if (pSampleItem->sampleConfig.type == GpaSampleType::Cumulative)
                {
                    CounterSample* pCounterSample = PAL_NEW (CounterSample,
                                                                m_pPlatform,
                                                                Util::SystemAllocType::AllocObject)
                                                                (m_pDevice, nullptr, m_pPlatform);
                    if (pCounterSample != nullptr)
                    {
                        pSampleItem->pPerfSample      = pCounterSample;
                        PerfSample* pSrcCounterSample = pSrcSampleItem->pPerfSample;

                        pCounterSample->SetCopySampleMemInfo(
                            pSrcCounterSample->GetSampleDataGpuMem().pGpuMemory,
                            pSrcCounterSample->GetSampleDataOffset());

                        pCounterSample->SetSampleMemoryProperties(gpuMemInfo, offset, gpuMemReqs.size);

                        // Import global perf counter layout to this session's perf item.
                        result = pCounterSample->SetCounterLayout(
                            pSampleItem->sampleConfig.perfCounters.numCounters,
                            static_cast<CounterSample*>(pSrcCounterSample)->GetCounterLayout());
                    }
                    else
                    {
                        result = Result::ErrorOutOfMemory;
                    }
                }
                else if (pSampleItem->sampleConfig.type == GpaSampleType::Trace)
                {
                    TraceSample* pSample = PAL_NEW (TraceSample,
                                                    m_pPlatform,
                                                    Util::SystemAllocType::AllocObject)
                                                    (m_pDevice, nullptr, m_pPlatform);
                    if (pSample != nullptr)
                    {
                        pSampleItem->pPerfSample    = pSample;
                        PerfSample* pSrcTraceSample = pSrcSampleItem->pPerfSample;

                        pSample->SetCopySampleMemInfo(pSrcTraceSample->GetSampleDataGpuMem().pGpuMemory,
                                                        pSrcTraceSample->GetSampleDataOffset());

                        pSample->SetSampleMemoryProperties(gpuMemInfo, offset, gpuMemReqs.size);

                        // Import thread trace layout to this session's perf item.
                        result = pSample->SetThreadTraceLayout(
                            pSampleItem->sampleConfig.perfCounters.numCounters,
                            static_cast<TraceSample*>(pSrcTraceSample)->GetThreadTraceLayout());
                    }
                    else
                    {
                        result = Result::ErrorOutOfMemory;
                    }
                }
            }
            else
            {
                // AcquireGpuMem failed.
                result = Result::ErrorOutOfGpuMemory;
            }
        }
        else if (pSampleItem->sampleConfig.type == GpaSampleType::Timing)
        {
            GpuMemoryInfo gpuMemInfo = {};
            gpusize offset           = 0;

            // Acquire GPU memory for both pre-call/post-call timestamp in one chunk, so later we just
            // need to copy the results once. Both pre-call post-call timestamps need to be aligned, so we
            // cannot only allocate 2*sizeof(uint64).
            result = AcquireGpuMem(sizeof(uint64) + m_timestampAlignment,
                                    m_timestampAlignment,
                                    GpuHeapGartCacheable,
                                    &gpuMemInfo,
                                    &offset);

            if (result == Result::Success)
            {
                PAL_ASSERT(gpuMemInfo.pGpuMemory != nullptr);

                TimingSample* pTimingSample = PAL_NEW (TimingSample,
                                                        m_pPlatform,
                                                        Util::SystemAllocType::AllocObject)
                                                        (m_pDevice, nullptr, m_pPlatform);
                if (pTimingSample != nullptr)
                {
                    pSampleItem->pPerfSample = pTimingSample;

                    PerfSample* pSrcTimingSample = pSrcSampleItem->pPerfSample;

                    pTimingSample->SetCopySampleMemInfo(
                        static_cast<TimingSample*>(pSrcTimingSample)->GetBeginTsGpuMem(),
                        static_cast<TimingSample*>(pSrcTimingSample)->GetBeginTsGpuMemOffset());

                    pTimingSample->SetTimestampMemoryInfo(gpuMemInfo, offset, m_timestampAlignment);

                    pTimingSample->SetSampleMemoryProperties(gpuMemInfo,
                                                                offset,
                                                                sizeof(uint64) + m_timestampAlignment);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
            else
            {
                result = Result::ErrorOutOfGpuMemory;
            }
        }
        else if (pSampleItem->sampleConfig.type == GpaSampleType::Query)
        {
            GpuMemoryInfo gpuMemInfo   = {};
            gpusize       offset       = 0;
            gpusize       heapSize     = 0;

            // Allocate a query for the copy session. This query only acts as a placeholder to store
            // the copied data.
            IQueryPool* pPipeStatsQuery = nullptr;
            result = AcquirePipeStatsQuery(&gpuMemInfo,
                                            &offset,
                                            &heapSize,
                                            &pPipeStatsQuery);

            if (result == Result::Success)
            {
                PAL_ASSERT(pPipeStatsQuery != nullptr);

                QuerySample* pQuerySample = PAL_NEW(QuerySample, m_pPlatform, Util::SystemAllocType::AllocObject)
                    (m_pDevice, nullptr, m_pPlatform);

                if (pQuerySample != nullptr)
                {
                    pSampleItem->pPerfSample = pQuerySample;

                    QuerySample* pSrcQuerySample = static_cast<QuerySample*>(pSrcSampleItem->pPerfSample);
                    pQuerySample->SetPipeStatsQuery(pPipeStatsQuery);
                    pQuerySample->SetCopySampleMemInfo(
                        pSrcQuerySample->GetSampleDataGpuMem().pGpuMemory,
                        pSrcQuerySample->GetSampleDataOffset());
                    pQuerySample->SetSampleMemoryProperties(gpuMemInfo, offset, heapSize);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
        }
    } // End init different sample types.

    if (result == Result::Success)
    {
        // Add sample to list if it's successfully created.
        result = m_sampleItemArray.PushBack(pSampleItem);
    }

    return result;
}

// =====================================================================================================================
// Acquires a range of queue-owned GPU memory for use by the next command buffer submission.
Result GpaSession::AcquireGpuMem(
    gpusize        size,
    gpusize        alignment,
    GpuHeap        heapType,
    GpuMemoryInfo* pGpuMem,
    gpusize*       pOffset)
{
    Util::Deque<GpuMemoryInfo, GpaAllocator>* pAvailableList = &m_availableGartGpuMem;
    Util::Deque<GpuMemoryInfo, GpaAllocator>* pBusyList      = &m_busyGartGpuMem;
    GpuMemoryInfo* pCurGpuMem = &m_curGartGpuMem;
    gpusize* pCurGpuMemOffset = &m_curGartGpuMemOffset;

    if (heapType == GpuHeapInvisible)
    {
        pBusyList        = &m_busyLocalInvisGpuMem;
        pAvailableList   = &m_availableLocalInvisGpuMem;
        pCurGpuMem       = &m_curLocalInvisGpuMem;
        pCurGpuMemOffset = &m_curLocalInvisGpuMemOffset;
    }

    *pCurGpuMemOffset = Util::Pow2Align(*pCurGpuMemOffset, alignment);

    constexpr gpusize MinRaftSize = 4 * 1024 * 1024; // 4MB
    const gpusize pageSize = m_deviceProps.gpuMemoryProperties.fragmentSize;
    const gpusize gpuMemoryRaftSize = Util::Max<gpusize>(MinRaftSize, Util::Pow2Align(size, pageSize));

    Result result = Result::Success;

    // If there isn't enough space left in the current allocation to fulfill this request, get a new allocation.  This
    // is done in a loop to handle the low GPU memory case where we may need to wait for prior work to finish then
    // try again.
    while ((pCurGpuMem->pGpuMemory == nullptr) || ((*pCurGpuMemOffset) + size > pCurGpuMem->pGpuMemory->Desc().size))
    {
        // Mark the current allocation as busy and associated with the upcoming submit.
        if (pCurGpuMem->pGpuMemory != nullptr)
        {
            result = pBusyList->PushBack(*pCurGpuMem);
        }
        PAL_ASSERT(result == Result::Success);

        if (pAvailableList->NumElements() > 0)
        {
            // We already have an idle GPU memory allocation in the pool, return that to the caller.
            result = pAvailableList->PopFront(pCurGpuMem);
        }
        else
        {
            GpuMemoryCreateInfo createInfo = {};
            createInfo.size      = gpuMemoryRaftSize;
            createInfo.alignment = pageSize;
            createInfo.vaRange   = VaRange::Default;
            createInfo.heapCount = 1;
            createInfo.heaps[0]  = heapType;
            createInfo.priority  = (heapType == GpuHeapInvisible) ? GpuMemPriority::High : GpuMemPriority::Normal;

            void* pMemory = PAL_CALLOC(m_pDevice->GetGpuMemorySize(createInfo, nullptr),
                                       m_pPlatform,
                                       Util::SystemAllocType::AllocObject);
            if (pMemory == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                result = m_pDevice->CreateGpuMemory(createInfo, pMemory, &pCurGpuMem->pGpuMemory);
            }

            if (result == Result::Success)
            {
                // GpaSession's Gpu memory is perma-resident.
                GpuMemoryRef memRef = {};
                memRef.pGpuMemory   = pCurGpuMem->pGpuMemory;

                result = m_pDevice->AddGpuMemoryReferences(1, &memRef, nullptr, GpuMemoryRefCantTrim);
            }

            if ((result == Result::Success) && (heapType != GpuHeapInvisible))
            {
                // GpaSession's Gpu memory is perma-mapped.
                result = pCurGpuMem->pGpuMemory->Map(&pCurGpuMem->pCpuAddr);
            }

            if (result != Result::Success)
            {
                if (pCurGpuMem->pGpuMemory != nullptr)
                {
                    pCurGpuMem->pGpuMemory->Destroy();
                    pCurGpuMem->pGpuMemory = nullptr;
                    pCurGpuMem->pCpuAddr   = nullptr;
                }

                PAL_SAFE_FREE(pMemory, m_pPlatform);

                // Hitting this assert means that we are out of GPU memory. Consider reducing the amount
                // of data collected (e.g., reduce sqtt.gpuMemoryLimit or reduce number of global perf
                // counters listed in the client specified config data, or enable supressInstructionTokens
                // to only gather specific types of data
                PAL_ASSERT(pAvailableList->NumElements() > 0);
            }
        }

        *pCurGpuMemOffset = 0;
    }

    *pGpuMem = *pCurGpuMem;
    *pOffset = *pCurGpuMemOffset;

    *pCurGpuMemOffset += size;

    return result;
}

// =====================================================================================================================
// Acquires a GpaSession-owned performance experiment based on the device's active perf counter requests.
Result GpaSession::AcquirePerfExperiment(
    const GpaSampleConfig& sampleConfig,
    GpuMemoryInfo*         pGpuMem,
    gpusize*               pOffset,
    GpuMemoryInfo*         pSecondaryGpuMem,
    gpusize*               pSecondaryOffset,
    gpusize*               pHeapSize,
    IPerfExperiment**      ppExperiment
    )
{
    // No experiments are currently idle (or possibly none exist at all) - allocate a new one.
    PerfExperimentCreateInfo createInfo                   = {};

    createInfo.optionFlags.sampleInternalOperations       = 1;
    createInfo.optionFlags.cacheFlushOnCounterCollection  = 1;

    createInfo.optionValues.sampleInternalOperations      = sampleConfig.flags.sampleInternalOperations;
    createInfo.optionValues.cacheFlushOnCounterCollection = sampleConfig.flags.cacheFlushOnCounterCollection;
    createInfo.optionFlags.sqShaderMask                   = sampleConfig.flags.sqShaderMask;
    createInfo.optionValues.sqShaderMask                  = sampleConfig.sqShaderMask;

    void* pMemory = PAL_CALLOC(m_pDevice->GetPerfExperimentSize(createInfo, nullptr),
                               m_pPlatform,
                               Util::SystemAllocType::AllocObject);

    Result result = Result::ErrorOutOfMemory;

    if (pMemory != nullptr)
    {
        result = m_pDevice->CreatePerfExperiment(createInfo, pMemory, ppExperiment);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, m_pPlatform);
        }
    }

    if (result == Result::Success)
    {
        if (sampleConfig.type == GpaSampleType::Cumulative)
        {
            const uint32         numCounters = sampleConfig.perfCounters.numCounters; // blocks*instances*counters
            const PerfCounterId* pCounters   = sampleConfig.perfCounters.pIds;

            // Counts how many counters are enabled per hardware block.
            uint32 count[static_cast<size_t>(GpuBlock::Count)] = {};

            Util::HashSet<BlockEventId, GpaAllocator> counterSet(16, m_pPlatform);
            result = counterSet.Init();

            if (result == Result::Success)
            {
                // Add each perfCounter instance to perfExperiment.
                for (uint32 i = 0; i < numCounters; i++)
                {
                    // Validate the requested counters
                    // NOTE: client should be responsible for this check. However it's better GpaSession checks it too
                    //       for the sake of client's debugging time when issues happen.
                    const uint32 blockIdx = static_cast<uint32>(pCounters[i].block);

                    PAL_ASSERT(blockIdx < static_cast<uint32>(GpuBlock::Count));

                    BlockEventId key = { pCounters[i].block, pCounters[i].eventId };
                    if (counterSet.Contains(key) == false)
                    {
                        count[blockIdx]++;

                        if (count[blockIdx] > m_perfExperimentProps.blocks[blockIdx].maxGlobalSharedCounters)
                        {
                            // Too many counters enabled for this block.
                            result = Result::ErrorInitializationFailed;
                        }
                        else if (pCounters[i].eventId > m_perfExperimentProps.blocks[blockIdx].maxEventId)
                        {
                            // Invalid event ID.
                            result = Result::ErrorInitializationFailed;
                        }
                        else
                        {
                            result = counterSet.Insert(key);
                        }
                    }

                    // Add each requested global counter to the experiment.
                    if (result == Result::Success)
                    {
                        PerfCounterInfo counterInfo = {};

                        counterInfo.counterType = PerfCounterType::Global;
                        counterInfo.block       = pCounters[i].block;
                        counterInfo.eventId     = pCounters[i].eventId;
                        counterInfo.instance    = pCounters[i].instance;

                        result = (*ppExperiment)->AddCounter(counterInfo);
                    }
                    PAL_ALERT(result != Result::Success);
                }
            }
        }
        else if (sampleConfig.type == GpaSampleType::Trace)
        {
            // Add SQ thread trace to the experiment.
            if (sampleConfig.sqtt.flags.enable)
            {
                // Use default SQTT size if client doesn't request specific size.
                const size_t sqttSeBufferSize = static_cast<size_t>((sampleConfig.sqtt.gpuMemoryLimit == 0) ?
                    m_perfExperimentProps.maxSqttSeBufferSize :
                    sampleConfig.sqtt.gpuMemoryLimit / m_perfExperimentProps.shaderEngineCount);

                const size_t alignedBufferSize = Util::Pow2AlignDown(sqttSeBufferSize,
                                                                     m_perfExperimentProps.sqttSeBufferAlignment);

                const bool skipInstTokens = sampleConfig.sqtt.flags.supressInstructionTokens;
                ThreadTraceInfo sqttInfo = { };
                sqttInfo.traceType               = PerfTraceType::ThreadTrace;
                sqttInfo.optionFlags.bufferSize  = 1;
                sqttInfo.optionValues.bufferSize = alignedBufferSize;

                // Set up the thread trace token mask. Use the minimal mask if queue timing is enabled. The mask will be
                // updated to a different value at a later time when sample updates are enabled.
                const uint32 standardTokenMask             = skipInstTokens ? SqttTokenMaskNoInst : SqttTokenMaskAll;
                sqttInfo.optionFlags.threadTraceTokenMask  = 1;
                sqttInfo.optionValues.threadTraceTokenMask = m_flags.enableSampleUpdates ? SqttTokenMaskMinimal
                                                                                         : standardTokenMask;

                for (uint32 i = 0; (i < m_perfExperimentProps.shaderEngineCount) && (result == Result::Success); i++)
                {
                    sqttInfo.instance = i;
                    result = (*ppExperiment)->AddThreadTrace(sqttInfo);
                }
            }

            // Configure and add an Spm trace to the perf experiment if the GpaSampleType is a Trace while perf counters
            // are also requested.
            if ((result == Result::Success) && (sampleConfig.perfCounters.numCounters > 0))
            {
                const uint32 numStreamingCounters = sampleConfig.perfCounters.numCounters;
                const PerfCounterId* pCounters    = sampleConfig.perfCounters.pIds;

                SpmTraceCreateInfo spmCreateInfo = {};
                spmCreateInfo.numPerfCounters    = numStreamingCounters;
                spmCreateInfo.spmInterval        = sampleConfig.perfCounters.spmTraceSampleInterval;
                spmCreateInfo.ringSize           = sampleConfig.perfCounters.gpuMemoryLimit;

                void* pMem = PAL_CALLOC(numStreamingCounters * sizeof(PerfCounterInfo),
                                        m_pPlatform,
                                        Util::SystemAllocType::AllocInternal);

                if (pMem != nullptr)
                {
                    spmCreateInfo.pPerfCounterInfos = static_cast<PerfCounterInfo*>(pMem);
                    PerfCounterInfo* pCounterInfo   = nullptr;

                    // Add each perfCounter instance to perfExperiment.
                    for (uint32 i = 0; i < numStreamingCounters; i++)
                    {
                        pCounterInfo = &(static_cast<PerfCounterInfo*>(pMem)[i]);
                        pCounterInfo->block    = pCounters[i].block;
                        pCounterInfo->eventId  = pCounters[i].eventId;
                        pCounterInfo->instance = pCounters[i].instance;
                    }

                    result = (*ppExperiment)->AddSpmTrace(spmCreateInfo);

                    // Free the memory allocated for the PerfCounterInfo(s) once AddSpmTrace returns.
                    PAL_SAFE_FREE(pMem, m_pPlatform);
                }
                else
                {
                    result = Result::ErrorOutOfMemory;
                }
            }
        }
        else
        {
            // undefined case
            result = Result::Unsupported;
        }
    }

    if (result == Result::Success)
    {
        result = (*ppExperiment)->Finalize();
    }

    if (result == Result::Success)
    {
        // Acquire GPU memory for the query from the pool and bind it.
        GpuMemoryRequirements gpuMemReqs = {};
        (*ppExperiment)->GetGpuMemoryRequirements(&gpuMemReqs);

        result = AcquireGpuMem(gpuMemReqs.size,
                               gpuMemReqs.alignment,
                               GpuHeapGartCacheable,
                               pGpuMem,
                               pOffset);

        if (result == Result::Success)
        {
            *pHeapSize = gpuMemReqs.size;

            // For full frame traces, the Gart heap becomes the secondary heap from which perf experiment
            // results are read.
            *pSecondaryGpuMem = *pGpuMem;
            *pSecondaryOffset = *pOffset;

            // Acquire new local invisible gpu memory for use as the trace buffer into which the  trace data is written
            // by the GPU. Trace data will later be copied to the secondary memory which is CPU-visible.
            if (sampleConfig.type == GpaSampleType::Trace)
            {
                result = AcquireGpuMem(gpuMemReqs.size,
                                       gpuMemReqs.alignment,
                                       GpuHeapInvisible,
                                       pGpuMem,
                                       pOffset);
            }
        }

        if ((result == Result::Success) && (pGpuMem->pGpuMemory != nullptr))
        {
            (*ppExperiment)->BindGpuMemory(pGpuMem->pGpuMemory, *pOffset);
        }
        else
        {
            // We weren't able to get memory for this perf experiment. Let's not accidentally bind a perf
            // experiment with no backing memory. Clean up this perf experiment.
            (*ppExperiment)->Destroy();
            PAL_SAFE_FREE((*ppExperiment), m_pPlatform);
        }
    }

    return result;
}

// =====================================================================================================================
// Acquires a queue-owned pipeline stats query.
Result GpaSession::AcquirePipeStatsQuery(
    GpuMemoryInfo* pGpuMem,
    gpusize*       pOffset,
    gpusize*       pHeapSize,
    IQueryPool**   ppQuery)
{
    // No queries are currently idle (or possibly none exist at all) - allocate a new one.
    QueryPoolCreateInfo createInfo = {};
    createInfo.queryPoolType       = QueryPoolType::PipelineStats;
    createInfo.numSlots            = 1;
    createInfo.enabledStats        = QueryPipelineStatsAll;

    void* pMemory = PAL_CALLOC(m_pDevice->GetQueryPoolSize(createInfo, nullptr),
                               m_pPlatform,
                               Util::SystemAllocType::AllocObject);

    Result result = Result::ErrorOutOfMemory;
    if (pMemory != nullptr)
    {
        result = m_pDevice->CreateQueryPool(createInfo, pMemory, ppQuery);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pMemory, m_pPlatform);
        }
    }

    if (result == Result::Success)
    {
        PAL_ASSERT(*ppQuery != nullptr);

        // Acquire GPU memory from pool and bind it.
        GpuMemoryRequirements gpuMemReqs = {};
        (*ppQuery)->GetGpuMemoryRequirements(&gpuMemReqs);

        result = AcquireGpuMem(gpuMemReqs.size,
                               gpuMemReqs.alignment,
                               GpuHeapGartCacheable,
                               pGpuMem,
                               pOffset);

        if (result == Result::Success)
        {
            *pHeapSize = gpuMemReqs.size;

            result = (*ppQuery)->BindGpuMemory(pGpuMem->pGpuMemory, *pOffset);
        }
    }

    return result;
}

// =====================================================================================================================
// Dump SQ thread trace data and spm trace data, if available, in rgp format.
Result GpaSession::DumpRgpData(
    TraceSample* pTraceSample,
    void*        pRgpOutput,
    size_t*      pTraceSize   // [in|out] Size of the thread trace data and/or spm trace data.
    ) const
{
    ThreadTraceLayout* pThreadTraceLayout = nullptr;
    void* pResults = pTraceSample->GetPerfExpResults();

    // Some of the calculations performed below depend on the assumed position of some fields in the chunk headers
    // defined in sqtt_file_format.h. TODO: Remove after some form of versioning is in place.
    static_assert((sizeof(SqttFileChunkHeader) == 16U) && (sizeof(SqttFileChunkIsaDatabase) == 28U),
        "The sizes of the chunk parameters in sqtt_file_format has been changed. Update GpaSession::DumRgpData.");

    Result result = Result::Success;

    gpusize curFileOffset = 0;

    SqttFileHeader fileHeader   = {};
    SqttFileHeader* pFileHeader = &fileHeader;
    fileHeader.magicNumber      = SQTT_FILE_MAGIC_NUMBER;
    fileHeader.versionMajor     = 1;
    fileHeader.versionMinor     = 0;
    fileHeader.flags            = 0;
    fileHeader.chunkOffset      = sizeof(fileHeader);

    // Get time info for rgp dump
    time_t rawTime;
    time(&rawTime);
    tm* pTimeInfo  = localtime(&rawTime);
    const tm& time = *pTimeInfo;

    fileHeader.second            = time.tm_sec;
    fileHeader.minute            = time.tm_min;
    fileHeader.hour              = time.tm_hour;
    fileHeader.dayInMonth        = time.tm_mday;
    fileHeader.month             = time.tm_mon;
    fileHeader.year              = time.tm_year;
    fileHeader.dayInWeek         = time.tm_wday;
    fileHeader.dayInYear         = time.tm_yday;
    fileHeader.isDaylightSavings = time.tm_isdst;

    if (pRgpOutput != nullptr)
    {
        if (static_cast<size_t>(curFileOffset + sizeof(fileHeader)) > *pTraceSize)
        {
            result = Result::ErrorInvalidMemorySize;
        }
        else
        {
            memcpy(pRgpOutput, pFileHeader, sizeof(fileHeader));
        }
    }
    curFileOffset += sizeof(fileHeader);

    // Get cpu info for rgp dump
    SqttFileChunkCpuInfo cpuInfo = {};
    FillSqttCpuInfo(&cpuInfo);

    if ((result == Result::Success) && (pRgpOutput != nullptr))
    {
        if (static_cast<size_t>(curFileOffset + sizeof(cpuInfo)) > *pTraceSize)
        {
            result = Result::ErrorInvalidMemorySize;
        }
        else
        {
            memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)), &cpuInfo, sizeof(cpuInfo));
        }
    }
    curFileOffset += sizeof(cpuInfo);

    // Get gpu info for rgp dump
    SqttFileChunkAsicInfo gpuInfo = {};
    FillSqttAsicInfo(m_deviceProps, m_perfExperimentProps, m_lastGpuClocksSample, &gpuInfo);

    if ((result == Result::Success) && (pRgpOutput != nullptr))
    {
        if (static_cast<size_t>(curFileOffset + sizeof(gpuInfo)) > *pTraceSize)
        {
            result = Result::ErrorInvalidMemorySize;
        }
        else
        {
            memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)), &gpuInfo, sizeof(gpuInfo));
        }
    }
    curFileOffset += sizeof(gpuInfo);

    // Get api info for rgp dump
    SqttFileChunkApiInfo apiInfo = {};
    apiInfo.header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_API_INFO;
    apiInfo.header.chunkIdentifier.chunkIndex = 0;
    apiInfo.header.version = 0;
    apiInfo.header.sizeInBytes = sizeof(apiInfo);
    apiInfo.apiType = SQTT_API_TYPE_VULKAN;
    apiInfo.versionMajor = m_apiMajorVer;
    apiInfo.versionMinor = m_apiMinorVer;

    if ((result == Result::Success) && (pRgpOutput != nullptr))
    {
        if (static_cast<size_t>(curFileOffset + sizeof(apiInfo)) > *pTraceSize)
        {
            result = Result::ErrorInvalidMemorySize;
        }
        else
        {
            memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)), &apiInfo, sizeof(apiInfo));
        }
    }
    curFileOffset += sizeof(apiInfo);

    if (pTraceSample->IsThreadTraceEnabled())
    {
        pThreadTraceLayout = pTraceSample->GetThreadTraceLayout();

        // Get each shader engine's data for rgp dump
        uint32 shaderEngineCount = m_deviceProps.gfxipProperties.shaderCore.numShaderEngines;
        for (uint32 i = 0; i < shaderEngineCount; i++)
        {
            const ThreadTraceSeLayout& seLayout     = pThreadTraceLayout->traces[i];

            // Get desc info for rgp dump
            SqttFileChunkSqttDesc desc             = {};
            desc.header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_SQTT_DESC;
            desc.header.chunkIdentifier.chunkIndex = i;
            desc.header.version                    = 1;
            desc.header.sizeInBytes                = sizeof(desc);
            desc.shaderEngineIndex                 = seLayout.shaderEngine;
            desc.v1.instrumentationSpecVersion     = m_instrumentationSpecVersion;
            desc.v1.instrumentationApiVersion      = m_instrumentationApiVersion;
            desc.v1.computeUnitIndex               = seLayout.computeUnit;

            desc.sqttVersion = GfxipToSqttVersionTranslation[static_cast<uint32>(m_deviceProps.gfxLevel)];

            if ((result == Result::Success) && (pRgpOutput != nullptr))
            {
                if (static_cast<size_t>(curFileOffset + sizeof(desc)) > *pTraceSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)), &desc, sizeof(desc));
                }
            }
            curFileOffset += sizeof(desc);

            // Get data info and data for rgp dump
            const auto& info  = *static_cast<const ThreadTraceInfoData*>(
                Util::VoidPtrInc(pResults, static_cast<size_t>(seLayout.infoOffset)));
            const void* pData = static_cast<const void*>(
                Util::VoidPtrInc(pResults, static_cast<size_t>(seLayout.dataOffset)));

            // curOffset reports the amount of SQTT data written by the hardware in units of 32 bytes.
            const uint32 sqttBytesWritten = info.curOffset * 32;

            SqttFileChunkSqttData data             = {};
            data.header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_SQTT_DATA;
            data.header.chunkIdentifier.chunkIndex = i;
            data.header.version                    = 0;
            data.header.sizeInBytes                = sizeof(data) + sqttBytesWritten;
            data.offset                            = static_cast<int32>(curFileOffset + sizeof(data));
            data.size                              = sqttBytesWritten;

            if ((result == Result::Success) && (pRgpOutput != nullptr))
            {
                if (static_cast<size_t>(curFileOffset + sizeof(data)) > *pTraceSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)), &data, sizeof(data));
                }
            }
            curFileOffset += sizeof(data);

            if ((result == Result::Success) && (pRgpOutput != nullptr))
            {
                if (static_cast<size_t>(curFileOffset + sqttBytesWritten) > *pTraceSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)),
                           pData,
                           sqttBytesWritten);
                }
            }
            curFileOffset += sqttBytesWritten;
        }

        // Write Shader ISA Database to the RGP file.
        if (result == Result::Success)
        {
            // Shader ISA database header.
            if (pRgpOutput != nullptr)
            {
                if (static_cast<size_t>(curFileOffset + sizeof(SqttFileChunkIsaDatabase)) > *pTraceSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    SqttFileChunkIsaDatabase shaderIsaDb          = {};
                    shaderIsaDb.header.chunkIdentifier.chunkType  = SQTT_FILE_CHUNK_TYPE_ISA_DATABASE;
                    shaderIsaDb.header.chunkIdentifier.chunkIndex = 0;
                    shaderIsaDb.header.version                    = 0;
                    shaderIsaDb.recordCount = static_cast<uint32>(m_curShaderRecords.NumElements());

                    int32 shaderDatabaseSize = sizeof(SqttFileChunkIsaDatabase);
                    auto iter = m_curShaderRecords.Begin();
                    while (iter.Get())
                    {
                        shaderDatabaseSize += (*iter.Get()).recordSize;
                        iter.Next();
                    }

                    // The sizes must be updated by adding the size of the rest of the chunk later.
                    shaderIsaDb.header.sizeInBytes                = shaderDatabaseSize;
                    // TODO: Duplicate - will have to remove later once RGP spec is updated.
                    shaderIsaDb.size                              = shaderDatabaseSize;

                    // The ISA database starts from the beginning of the chunk.
                    shaderIsaDb.offset                            = static_cast<int32>(curFileOffset);

                    memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)),
                           &shaderIsaDb,
                           sizeof(SqttFileChunkIsaDatabase));
                }
            }

            curFileOffset += sizeof(SqttFileChunkIsaDatabase);

            auto iter = m_curShaderRecords.Begin();

            while (iter.Get())
            {
                ShaderRecord* pShaderRecord = iter.Get();

                if (pRgpOutput != nullptr)
                {
                    if (static_cast<size_t>(curFileOffset + pShaderRecord->recordSize) > *pTraceSize)
                    {
                        result = Result::ErrorInvalidMemorySize;
                    }
                    else
                    {
                        // Copy one record to the buffer provided.
                        memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)),
                               pShaderRecord->pRecord,
                               pShaderRecord->recordSize);
                    }
                }

                curFileOffset += pShaderRecord->recordSize;
                iter.Next();
            }
        }
    }

    // Only write queue timing and calibration chunks if queue timing was enabled during the session.
    if (m_flags.enableQueueTiming)
    {
        // SqttQueueEventTimings chunk
        SqttFileChunkQueueEventTimings eventTimings = {};
        eventTimings.header.chunkIdentifier.chunkType = SQTT_FILE_CHUNK_TYPE_QUEUE_EVENT_TIMINGS;
        eventTimings.header.chunkIdentifier.chunkIndex = 0;
        eventTimings.header.version = 0;

        const uint32 numQueueInfoRecords = m_timedQueuesArray.NumElements();
        const uint32 numQueueEventRecords = m_queueEvents.NumElements();

        const uint32 queueInfoTableSize = numQueueInfoRecords * sizeof(SqttQueueInfoRecord);
        const uint32 queueEventTableSize = numQueueEventRecords * sizeof(SqttQueueEventRecord);

        eventTimings.header.sizeInBytes = sizeof(eventTimings) + queueInfoTableSize + queueEventTableSize;

        eventTimings.queueInfoTableRecordCount = numQueueInfoRecords;
        eventTimings.queueInfoTableSize = queueInfoTableSize;

        eventTimings.queueEventTableRecordCount = numQueueEventRecords;
        eventTimings.queueEventTableSize = queueEventTableSize;

        // Write the chunk header into the buffer
        if ((result == Result::Success) && (pRgpOutput != nullptr))
        {
            if (static_cast<size_t>(curFileOffset + sizeof(eventTimings)) > *pTraceSize)
            {
                result = Result::ErrorInvalidMemorySize;
            }
            else
            {
                memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)),
                    &eventTimings,
                    sizeof(eventTimings));
            }
        }
        curFileOffset += sizeof(eventTimings);

        // Write the queue info table
        if ((result == Result::Success) && (pRgpOutput != nullptr))
        {
            if (static_cast<size_t>(curFileOffset + queueInfoTableSize) > *pTraceSize)
            {
                result = Result::ErrorInvalidMemorySize;
            }
            else
            {
                void* pQueueInfoTableFileOffset = Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset));

                for (uint32 queueIndex = 0; queueIndex < numQueueInfoRecords; ++queueIndex)
                {
                    TimedQueueState* pQueueState = m_timedQueuesArray.At(queueIndex);

                    SqttQueueInfoRecord queueInfoRecord     = {};
                    queueInfoRecord.queueID                 = pQueueState->queueId;
                    queueInfoRecord.queueContext            = pQueueState->queueContext;
                    queueInfoRecord.hardwareInfo.queueType  = PalQueueTypeToSqttQueueType[pQueueState->queueType];
                    queueInfoRecord.hardwareInfo.engineType = PalEngineTypeToSqttEngineType[pQueueState->engineType];

                    memcpy(pQueueInfoTableFileOffset, &queueInfoRecord, sizeof(queueInfoRecord));

                    pQueueInfoTableFileOffset = Util::VoidPtrInc(pQueueInfoTableFileOffset, sizeof(queueInfoRecord));
                }
            }
        }
        curFileOffset += queueInfoTableSize;

        // Write the queue event table
        if ((result == Result::Success) && (pRgpOutput != nullptr))
        {
            if (static_cast<size_t>(curFileOffset + queueEventTableSize) > *pTraceSize)
            {
                result = Result::ErrorInvalidMemorySize;
            }
            else
            {
                void* pQueueEventTableFileOffset = Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset));

                for (uint32 eventIndex = 0; eventIndex < numQueueEventRecords; ++eventIndex)
                {
                    const TimedQueueEventItem* pQueueEvent = &m_queueEvents.At(eventIndex);

                    SqttQueueEventRecord queueEventRecord = {};
                    queueEventRecord.frameIndex           = pQueueEvent->frameIndex;
                    queueEventRecord.queueInfoIndex       = pQueueEvent->queueIndex;
                    queueEventRecord.cpuTimestamp         = pQueueEvent->cpuTimestamp;

                    switch (pQueueEvent->eventType)
                    {
                    case TimedQueueEventType::Submit:
                    {
                        const uint64* pPreTimestamp = reinterpret_cast<const uint64*>(Util::VoidPtrInc(
                            pQueueEvent->gpuTimestamps.memInfo[0].pCpuAddr,
                            static_cast<size_t>(pQueueEvent->gpuTimestamps.offsets[0])));

                        const uint64* pPostTimestamp = reinterpret_cast<const uint64*>(Util::VoidPtrInc(
                            pQueueEvent->gpuTimestamps.memInfo[1].pCpuAddr,
                            static_cast<size_t>(pQueueEvent->gpuTimestamps.offsets[1])));

                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_CMDBUF_SUBMIT;
                        queueEventRecord.gpuTimestamps[0] = *pPreTimestamp;
                        queueEventRecord.gpuTimestamps[1] = *pPostTimestamp;
                        queueEventRecord.apiId            = pQueueEvent->apiId;
                        queueEventRecord.sqttCbId         = pQueueEvent->sqttCmdBufId;
                        queueEventRecord.submitSubIndex   = pQueueEvent->submitSubIndex;

                        break;
                    }

                    case TimedQueueEventType::Signal:
                    {
                        const uint64* pTimestamp = reinterpret_cast<const uint64*>(Util::VoidPtrInc(
                            pQueueEvent->gpuTimestamps.memInfo[0].pCpuAddr,
                            static_cast<size_t>(pQueueEvent->gpuTimestamps.offsets[0])));

                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_SIGNAL_SEMAPHORE;
                        queueEventRecord.gpuTimestamps[0] = *pTimestamp;
                        queueEventRecord.apiId            = pQueueEvent->apiId;

                        break;
                    }

                    case TimedQueueEventType::Wait:
                    {
                        const uint64* pTimestamp = reinterpret_cast<const uint64*>(Util::VoidPtrInc(
                            pQueueEvent->gpuTimestamps.memInfo[0].pCpuAddr,
                            static_cast<size_t>(pQueueEvent->gpuTimestamps.offsets[0])));

                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_WAIT_SEMAPHORE;
                        queueEventRecord.gpuTimestamps[0] = *pTimestamp;
                        queueEventRecord.apiId            = pQueueEvent->apiId;

                        break;
                    }

                    case TimedQueueEventType::Present:
                    {
                        const uint64* pTimestamp = reinterpret_cast<const uint64*>(Util::VoidPtrInc(
                            pQueueEvent->gpuTimestamps.memInfo[0].pCpuAddr,
                            static_cast<size_t>(pQueueEvent->gpuTimestamps.offsets[0])));

                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_PRESENT;
                        queueEventRecord.gpuTimestamps[0] = *pTimestamp;
                        queueEventRecord.apiId            = pQueueEvent->apiId;

                        break;
                    }

                    case TimedQueueEventType::ExternalSignal:
                    {
                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_SIGNAL_SEMAPHORE;
                        queueEventRecord.gpuTimestamps[0] = ExtractGpuTimestampFromQueueEvent(*pQueueEvent);
                        queueEventRecord.apiId            = pQueueEvent->apiId;

                        break;
                    }

                    case TimedQueueEventType::ExternalWait:
                    {
                        queueEventRecord.eventType        = SQTT_QUEUE_TIMING_EVENT_WAIT_SEMAPHORE;
                        queueEventRecord.gpuTimestamps[0] = ExtractGpuTimestampFromQueueEvent(*pQueueEvent);
                        queueEventRecord.apiId            = pQueueEvent->apiId;

                        break;
                    }

                    default:
                    {
                        // Invalid event type
                        PAL_ASSERT_ALWAYS();
                        break;
                    }
                    }

                    memcpy(pQueueEventTableFileOffset, &queueEventRecord, sizeof(queueEventRecord));

                    pQueueEventTableFileOffset = Util::VoidPtrInc(pQueueEventTableFileOffset, sizeof(queueEventRecord));
                }
            }
        }
        curFileOffset += queueEventTableSize;

        // SqttClockCalibration chunk
        SqttFileChunkClockCalibration clockCalibration = {};
        clockCalibration.header.chunkIdentifier.chunkType = SQTT_FILE_CHUNK_TYPE_CLOCK_CALIBRATION;
        clockCalibration.header.version = 0;
        clockCalibration.header.sizeInBytes = sizeof(clockCalibration);

        const uint32 numClockCalibrationSamples = m_timestampCalibrations.NumElements();

        for (uint32 sampleIndex = 0; sampleIndex < numClockCalibrationSamples; ++sampleIndex)
        {
            const Pal::GpuTimestampCalibration& timestampCalibration = m_timestampCalibrations.At(sampleIndex);

            clockCalibration.header.chunkIdentifier.chunkIndex = sampleIndex;
            clockCalibration.cpuTimestamp = timestampCalibration.cpuWinPerfCounter;
            clockCalibration.gpuTimestamp = timestampCalibration.gpuTimestamp;

            // Write the chunk header into the buffer
            if ((result == Result::Success) && (pRgpOutput != nullptr))
            {
                if (static_cast<size_t>(curFileOffset + sizeof(clockCalibration)) > *pTraceSize)
                {
                    result = Result::ErrorInvalidMemorySize;
                }
                else
                {
                    memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(curFileOffset)),
                        &clockCalibration,
                        sizeof(clockCalibration));
                }
            }
            curFileOffset += sizeof(clockCalibration);
        }
    }

    if ((result == Result::Success) && (pTraceSample->IsSpmTraceEnabled()))
    {
        // Add Spm chunk to RGP file.
        result = AppendSpmTraceData(pTraceSample, (*pTraceSize), pRgpOutput, &curFileOffset);
    }

    *pTraceSize = static_cast<size_t>(curFileOffset);

    return result;
}

// =====================================================================================================================
// Appends the spm trace data in the buffer provided. If nullptr buffer is provided, it returns the size required for
// the spm data.
Result GpaSession::AppendSpmTraceData(
    TraceSample* pTraceSample,  // [in] The PerfSample from which to get the spm trace data.
    size_t       bufferSize,    // [in] Size of the buffer.
    void*        pRgpOutput,    // [out] The buffer into which spm trace data is written. May contain thread trace data.
    gpusize*     pCurFileOffset // [in|out] Current wptr position in buffer.
    ) const
{
    Result result = Result::Success;

    // Initialize the Sqtt chunk, get the spm trace results and add to the file.
    gpusize spmDataSize   = 0;
    gpusize numSpmSamples = 0;
    pTraceSample->GetSpmResultsSize(&spmDataSize, &numSpmSamples);

    if (pRgpOutput != nullptr)
    {
        // Header for spm chunk.
        if (static_cast<gpusize>(*pCurFileOffset + sizeof(SqttFileChunkSpmDb) + spmDataSize) > bufferSize)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            // Write the chunk header first.
            SqttFileChunkSpmDb spmDbChunk               = { };
            spmDbChunk.header.chunkIdentifier.chunkType = SQTT_FILE_CHUNK_TYPE_SPM_DB;
            spmDbChunk.header.sizeInBytes               = static_cast<int32>(sizeof(SqttFileChunkSpmDb) + spmDataSize);
            spmDbChunk.numTimestamps                    = static_cast<uint32>(numSpmSamples);
            spmDbChunk.numSpmCounterInfo                = pTraceSample->GetNumSpmCounters();

            memcpy(Util::VoidPtrInc(pRgpOutput, static_cast<size_t>(*pCurFileOffset)), &spmDbChunk, sizeof(spmDbChunk));

            size_t curWriteOffset = static_cast<size_t>(*pCurFileOffset + sizeof(SqttFileChunkSpmDb));

            result = pTraceSample->GetSpmTraceResults(Util::VoidPtrInc(pRgpOutput, curWriteOffset),
                                                      (bufferSize - curWriteOffset));
        }
    }

    (*pCurFileOffset) += (sizeof(SqttFileChunkSpmDb) + spmDataSize);

    return result;
}

// =====================================================================================================================
// recycle used Gart rafts and put back to available pool
void GpaSession::RecycleGartGpuMem()
{
    while (m_busyGartGpuMem.NumElements() > 0)
    {
        GpuMemoryInfo info = {};
        m_busyGartGpuMem.PopFront(&info);
        m_availableGartGpuMem.PushBack(info);
    }
    PAL_ASSERT(m_curGartGpuMem.pGpuMemory == nullptr);
}

// =====================================================================================================================
// recycle used Local Invisible rafts and put back to available pool
void GpaSession::RecycleLocalInvisGpuMem()
{
    while (m_busyLocalInvisGpuMem.NumElements() > 0)
    {
        GpuMemoryInfo info = {};
        m_busyLocalInvisGpuMem.PopFront(&info);
        m_availableLocalInvisGpuMem.PushBack(info);
    }
    PAL_ASSERT(m_curGartGpuMem.pGpuMemory == nullptr);
}

// =====================================================================================================================
// Destroy and free the m_sampleItemArray and associated memory allocation
void GpaSession::FreeSampleItemArray()
{
    const uint32 numEntries = m_sampleItemArray.NumElements();
    for (uint32 i = 0; i < numEntries; i++)
    {
        SampleItem* pSampleItem = m_sampleItemArray.At(i);
        PAL_ASSERT(pSampleItem != nullptr);

        if (pSampleItem->pPerfExperiment != nullptr)
        {
            pSampleItem->pPerfExperiment->Destroy();
            PAL_SAFE_FREE(pSampleItem->pPerfExperiment, m_pPlatform);
        }

        PerfSample* pSample = pSampleItem->pPerfSample;

        if (pSample != nullptr)
        {
            PAL_SAFE_DELETE(pSample, m_pPlatform);
        }

        PAL_SAFE_FREE(pSampleItem, m_pPlatform);
    }
    m_sampleItemArray.Clear();
}

// =====================================================================================================================
// Extracts all shader data for the shader type specified from this pipeline and fills the ShaderRecord. Allocates
// memory to cache the shader ISA and shader stats as RGP chunks.
Result GpaSession::CreateShaderRecord(
    ShaderType       shaderType,
    const IPipeline* pPipeline,
    ShaderRecord*    pShaderRecord)
{
    Result result                = Result::Success;
    SqttIsaDbRecord record       = {};
    ShaderStats shaderStats      = {};
    SqttShaderIsaBlobHeader blob = {};
    size_t shaderCodeSize        = 0;
    PipelineInfo pipeInfo        = pPipeline->GetInfo();

    result = pPipeline->GetShaderStats(shaderType, &shaderStats, false);

    // Get the shader ISA from the pipeline.
    if (result == Result::Success)
    {
        result = pPipeline->GetShaderCode(shaderType, &shaderCodeSize, nullptr);
        PAL_ASSERT(result == Result::Success);
    }

    // Cache SqttIsaDatabaseRecord, SqttShaderIsaBlobHeader and shader ISA code in GpaSession-owned memory.
    if (result == Result::Success)
    {
        const size_t curBlobSize = sizeof(SqttShaderIsaBlobHeader) + shaderCodeSize;

        // Update this record.
        record.shaderStage = shaderStats.shaderStageMask;
        record.recordSize  = static_cast<uint32>(sizeof(SqttIsaDatabaseRecord) + curBlobSize);

        // Update the fields of this blob header.
        blob.sizeInBytes     = static_cast<uint32>(curBlobSize);
        blob.actualVgprCount = shaderStats.common.numUsedVgprs;
        blob.actualSgprCount = shaderStats.common.numUsedSgprs;
        blob.apiShaderHash.lower = pipeInfo.shader[static_cast<uint32>(shaderType)].hash.lower;
        blob.apiShaderHash.upper = pipeInfo.shader[static_cast<uint32>(shaderType)].hash.upper;
        blob.palShaderHash.lower = shaderStats.palShaderHash.lower;
        blob.palShaderHash.upper = shaderStats.palShaderHash.upper;
        blob.actualLdsCount  = static_cast<uint16>(shaderStats.common.ldsUsageSizeInBytes);
        blob.baseAddress     = shaderStats.common.gpuVirtAddress;
        blob.scratchSize     =
            static_cast<uint16>(shaderStats.common.scratchMemUsageInBytes);

        // Update shader flags.
        if (shaderStats.shaderOperations.streamOut)
        {
            blob.flags |= SqttShaderFlags::SQTT_SHADER_STREAM_OUT_ENABLED;
        }
        if (shaderStats.shaderOperations.writesDepth)
        {
            blob.flags |= SqttShaderFlags::SQTT_SHADER_WRITES_DEPTH;
        }
        if (shaderStats.shaderOperations.writesUAV)
        {
            blob.flags |= SqttShaderFlags::SQTT_SHADER_WRITES_UAV;
        }

        // Allocate space to store all the information for one record.
        void* pBuffer = PAL_MALLOC(record.recordSize, m_pPlatform, Util::SystemAllocType::AllocInternal);

        if (pBuffer != nullptr)
        {
            pShaderRecord->recordSize = record.recordSize;
            pShaderRecord->pRecord    = pBuffer;

            // Write the record header.
            memcpy(pBuffer, &record, sizeof(SqttIsaDatabaseRecord));

            // Write the blob header.
            pBuffer = Util::VoidPtrInc(pBuffer, sizeof(SqttIsaDatabaseRecord));
            memcpy(pBuffer, &blob, sizeof(SqttShaderIsaBlobHeader));

            // Write the shader ISA.
            pBuffer = Util::VoidPtrInc(pBuffer, sizeof(SqttShaderIsaBlobHeader));
            result  = pPipeline->GetShaderCode(shaderType, &shaderCodeSize, pBuffer);

            if (result != Result::Success)
            {
                // Deallocate if some error occured.
                PAL_SAFE_FREE(pBuffer, m_pPlatform);
            }
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}
} //GpuUtil
