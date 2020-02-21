/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverEventService.h"
#include "core/devDriverEventServiceConv.h"
#include "util/rmtTokens.h"
#include "util/rmtResourceDescriptions.h"
#include "core/eventDefs.h"
#include "palSysUtil.h"
#include "core/gpuMemory.h"

using namespace DevDriver;

namespace Pal
{
// =====================================================================================================================
EventService::EventService(const AllocCb& allocCb)
    : m_rmtWriter(allocCb)
    , m_isMemoryProfilingEnabled(false)
    , m_isInitialized(false)
{
}

// =====================================================================================================================
EventService::~EventService()
{
}

// =====================================================================================================================
void EventService::WriteUserdataStringToken(
    uint8       delta,
    const char* pSnapshotName,
    bool        isSnapshot)
{
    RMT_USERDATA_EVENT_TYPE type = isSnapshot ? RMT_USERDATA_EVENT_TYPE_SNAPSHOT : RMT_USERDATA_EVENT_TYPE_NAME;
    const uint32 strByteSize = DevDriver::Platform::Min(
        static_cast<uint32>(strlen(pSnapshotName) + 1),
        static_cast<uint32>(RMT_MAX_USERDATA_STRING_SIZE));
    RMT_MSG_USERDATA eventToken(delta, type, strByteSize);

    // For snapshots we fill in the size of the snapshot string in the token then write string bytes
    // immediately following the token in the stream.
    m_rmtWriter.WriteTokenData(eventToken);
    m_rmtWriter.WriteData(pSnapshotName, strByteSize);
}

// =====================================================================================================================
DevDriver::Result EventService::HandleRequest(
    IURIRequestContext* pContext)
{
    DD_ASSERT(pContext != nullptr);

    // Make sure we aren't logging while we handle a network request
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);

    DevDriver::Result result = DevDriver::Result::Unavailable;

    const char* const pArgDelim = " ";
    char* pStrtokContext = nullptr;

    // Safety note: Strtok handles nullptr by returning nullptr. We handle that below.
    char* pCmdName = Platform::Strtok(pContext->GetRequestArguments(), pArgDelim, &pStrtokContext);
    char* pCmdArg1 = Platform::Strtok(nullptr, pArgDelim, &pStrtokContext);

    if (strcmp(pCmdName, "enableMemoryProfiling") == 0)
    {
        if (m_isMemoryProfilingEnabled == false)
        {
            m_isMemoryProfilingEnabled = true;
            result = DevDriver::Result::Success;

            m_rmtWriter.Init();
            m_rmtWriter.BeginDataChunk(Util::GetIdOfCurrentProcess(), 0);
        }
    }
    else if (strcmp(pCmdName, "disableMemoryProfiling") == 0)
    {
        if (m_isMemoryProfilingEnabled)
        {
            m_isMemoryProfilingEnabled = false;
            result = DevDriver::Result::Success;

            m_rmtWriter.EndDataChunk();
            m_rmtWriter.Finalize();

            const size_t rmtDataSize = m_rmtWriter.GetRmtDataSize();
            if (rmtDataSize > 0)
            {
                IByteWriter* pWriter = nullptr;
                result = pContext->BeginByteResponse(&pWriter);
                if (result == DevDriver::Result::Success)
                {
                    pWriter->WriteBytes(m_rmtWriter.GetRmtData(), rmtDataSize);

                    result = pWriter->End();
                }
            }
        }
    }
    else if ((strcmp(pCmdName, "insertSnapshot") == 0) && (pCmdArg1 != nullptr))
    {
        // Only attempt to write a snapshot token if we have a trace active
        if (m_isMemoryProfilingEnabled)
        {
            const uint8 delta = m_rmtWriter.CalculateDelta();
            WriteUserdataStringToken(delta, pCmdArg1, true);
        }
    }

    return result;
}

// =====================================================================================================================
void EventService::LogEvent(
    PalEvent    eventId,
    const void* pEventData,
    size_t      eventDataSize)
{
    // Make sure we aren't handling a network request while we log
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);
    if (m_isMemoryProfilingEnabled)
    {
        const uint8 delta = m_rmtWriter.CalculateDelta();

        switch (eventId)
        {
        case PalEvent::CreateGpuMemory:
        {
            PAL_ASSERT(sizeof(CreateGpuMemoryData) == eventDataSize);

            const CreateGpuMemoryData* pData = reinterpret_cast<const CreateGpuMemoryData*>(pEventData);

            RMT_MSG_VIRTUAL_ALLOCATE eventToken(
                delta,
                pData->size,
                pData->isInternal ? RMT_OWNER_CLIENT_DRIVER : RMT_OWNER_APP, // For now we only distinguish between driver
                                                                             // app ownership
                pData->gpuVirtualAddr,
                PalToRmtHeapType(pData->preferredHeap),
                RMT_HEAP_TYPE_LOCAL,
                RMT_HEAP_TYPE_LOCAL,
                RMT_HEAP_TYPE_LOCAL);

            m_rmtWriter.WriteTokenData(eventToken);

            break;
        }
        case PalEvent::DestroyGpuMemory:
        {
            PAL_ASSERT(sizeof(DestroyGpuMemoryData) == eventDataSize);

            const DestroyGpuMemoryData* pData = reinterpret_cast<const DestroyGpuMemoryData*>(pEventData);

            RMT_MSG_FREE_VIRTUAL eventToken(delta, pData->gpuVirtualAddr);

            m_rmtWriter.WriteTokenData(eventToken);

            break;
        }
        case PalEvent::GpuMemoryResourceCreate:
        {
            LogResourceCreateEvent(delta, pEventData, eventDataSize);
            break;
        }
        case PalEvent::GpuMemoryResourceDestroy:
        {
            PAL_ASSERT(sizeof(GpuMemoryResourceDestroyData) == eventDataSize);
            const GpuMemoryResourceDestroyData* pData = reinterpret_cast<const GpuMemoryResourceDestroyData*>(pEventData);

            RMT_MSG_RESOURCE_DESTROY eventToken(delta, static_cast<uint32>(pData->handle));

            m_rmtWriter.WriteTokenData(eventToken);

            break;
        }
        case PalEvent::GpuMemoryMisc:
        {
            PAL_ASSERT(sizeof(GpuMemoryMiscData) == eventDataSize);
            const GpuMemoryMiscData* pData = reinterpret_cast<const GpuMemoryMiscData*>(pEventData);

            RMT_MSG_MISC eventToken(delta, PalToRmtMiscEventType(pData->type));

            m_rmtWriter.WriteTokenData(eventToken);
            break;
        }
        case PalEvent::GpuMemorySnapshot:
        {
            PAL_ASSERT(sizeof(GpuMemorySnapshotData) == eventDataSize);
            const GpuMemorySnapshotData* pData = reinterpret_cast<const GpuMemorySnapshotData*>(pEventData);

            WriteUserdataStringToken(delta, pData->pSnapshotName, true);
            break;
        }
        case PalEvent::DebugName:
        {
            PAL_ASSERT(sizeof(DebugNameData) == eventDataSize);
            const DebugNameData* pData = reinterpret_cast<const DebugNameData*>(pEventData);

            WriteUserdataStringToken(delta, pData->pDebugName, false);
            break;
        }
        case PalEvent::GpuMemoryResourceBind:
        {
            PAL_ASSERT(sizeof(GpuMemoryResourceBindData) == eventDataSize);
            const GpuMemoryResourceBindData* pData =
                reinterpret_cast<const GpuMemoryResourceBindData*>(pEventData);

            RMT_MSG_RESOURCE_BIND eventToken(
                delta,
                pData->gpuVirtualAddr + pData->offset,
                pData->requiredSize,
                static_cast<uint32>(pData->resourceHandle),
                pData->isSystemMemory);

            m_rmtWriter.WriteTokenData(eventToken);

            GpuMemory* pGpuMemory = reinterpret_cast<GpuMemory*>(pData->handle);
            if (pGpuMemory != nullptr)
            {
                if (pData->requiredSize > pGpuMemory->Desc().size)
                {
                    // GPU memory smaller than resource size
                    DD_ASSERT_ALWAYS();
                }
            }
            break;
        }
        case PalEvent::GpuMemoryCpuMap:
        {
            PAL_ASSERT(sizeof(GpuMemoryCpuMapData) == eventDataSize);
            const GpuMemoryCpuMapData* pData = reinterpret_cast<const GpuMemoryCpuMapData*>(pEventData);

            RMT_MSG_CPU_MAP eventToken(delta, pData->gpuVirtualAddr, false);

            m_rmtWriter.WriteTokenData(eventToken);
            break;
        }
        case PalEvent::GpuMemoryCpuUnmap:
        {
            PAL_ASSERT(sizeof(GpuMemoryCpuUnmapData) == eventDataSize);
            const GpuMemoryCpuUnmapData* pData = reinterpret_cast<const GpuMemoryCpuUnmapData*>(pEventData);

            RMT_MSG_CPU_MAP eventToken(delta, pData->gpuVirtualAddr, true);

            m_rmtWriter.WriteTokenData(eventToken);
            break;
        }
        case PalEvent::GpuMemoryAddReference:
        {
            PAL_ASSERT(sizeof(GpuMemoryAddReferenceData) == eventDataSize);
            const GpuMemoryAddReferenceData* pData = reinterpret_cast<const GpuMemoryAddReferenceData*>(pEventData);

            RMT_MSG_RESOURCE_REFERENCE eventToken(
                delta,
                false,   // isRemove
                pData->gpuVirtualAddr,
                static_cast<uint8>(pData->queueHandle));

            m_rmtWriter.WriteTokenData(eventToken);
            break;
        }
        case PalEvent::GpuMemoryRemoveReference:
        {
            PAL_ASSERT(sizeof(GpuMemoryRemoveReferenceData) == eventDataSize);
            const GpuMemoryRemoveReferenceData* pData = reinterpret_cast<const GpuMemoryRemoveReferenceData*>(pEventData);

            RMT_MSG_RESOURCE_REFERENCE eventToken(
                delta,
                true,   // isRemove
                pData->gpuVirtualAddr,
                static_cast<uint8>(pData->queueHandle));

            m_rmtWriter.WriteTokenData(eventToken);
            break;
        }
        }
    }
}

// =====================================================================================================================
void EventService::LogResourceCreateEvent(
    uint8 delta,
    const void* pEventData,
    size_t      eventDataSize)
{
    PAL_ASSERT(eventDataSize == sizeof(GpuMemoryResourceCreateData));
    const auto* pRsrcCreateData = reinterpret_cast<const GpuMemoryResourceCreateData*>(pEventData);

    RMT_MSG_RESOURCE_CREATE rsrcCreateToken(
        delta,
        static_cast<uint32>(pRsrcCreateData->handle),
        RMT_OWNER_KMD,
        0,
        RMT_COMMIT_TYPE_COMMITTED,
        PalToRmtResourceType(pRsrcCreateData->type));
    m_rmtWriter.WriteTokenData(rsrcCreateToken);

    switch (pRsrcCreateData->type)
    {
    case ResourceType::Image:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionImage));
        const auto* pImageData = reinterpret_cast<const ResourceDescriptionImage*>(pRsrcCreateData->pDescription);
        RMT_IMAGE_DESC_CREATE_INFO imgCreateInfo = {};
        imgCreateInfo.createFlags = PalToRmtImgCreateFlags(pImageData->pCreateInfo->flags);
        imgCreateInfo.usageFlags = PalToRmtImgUsageFlags(pImageData->pCreateInfo->usageFlags);
        imgCreateInfo.imageType = PalToRmtImageType(pImageData->pCreateInfo->imageType);
        imgCreateInfo.dimensions.dimension_X = static_cast<uint16>(pImageData->pCreateInfo->extent.width);
        imgCreateInfo.dimensions.dimension_Y = static_cast<uint16>(pImageData->pCreateInfo->extent.height);
        imgCreateInfo.dimensions.dimension_Z = static_cast<uint16>(pImageData->pCreateInfo->extent.depth);
        imgCreateInfo.format = PalToRmtImageFormat(pImageData->pCreateInfo->swizzledFormat);
        imgCreateInfo.mips = static_cast<uint8>(pImageData->pCreateInfo->mipLevels);
        imgCreateInfo.slices = static_cast<uint8>(pImageData->pCreateInfo->arraySize);
        imgCreateInfo.samples = static_cast<uint8>(pImageData->pCreateInfo->samples);
        imgCreateInfo.fragments = static_cast<uint8>(pImageData->pCreateInfo->fragments);
        imgCreateInfo.tilingType = PalToRmtTilingType(pImageData->pCreateInfo->tiling);
        imgCreateInfo.tilingOptMode = PalToRmtTilingOptMode(pImageData->pCreateInfo->tilingOptMode);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 481
        imgCreateInfo.metadataMode = PalToRmtMetadataMode(pImageData->pCreateInfo->metadataMode);
#else
        imgCreateInfo.metadataMode = RMT_IMAGE_METADATA_MODE_DEFAULT;
#endif
        imgCreateInfo.maxBaseAlignment = pImageData->pCreateInfo->maxBaseAlign;
        imgCreateInfo.isPresentable = pImageData->isPresentable;
        imgCreateInfo.imageSize = static_cast<uint32>(pImageData->pMemoryLayout->dataSize);
        imgCreateInfo.metadataOffset = static_cast<uint32>(pImageData->pMemoryLayout->metadataOffset);
        imgCreateInfo.metadataSize = static_cast<uint32>(pImageData->pMemoryLayout->metadataSize);
        imgCreateInfo.metadataHeaderOffset = static_cast<uint32>(pImageData->pMemoryLayout->metadataHeaderOffset);
        imgCreateInfo.metadataHeaderSize = static_cast<uint32>(pImageData->pMemoryLayout->metadataHeaderSize);
        imgCreateInfo.imageAlignment = pImageData->pMemoryLayout->dataAlignment;
        imgCreateInfo.metadataAlignment = pImageData->pMemoryLayout->metadataAlignment;
        imgCreateInfo.metadataHeaderAlignment = pImageData->pMemoryLayout->metadataHeaderAlignment;
        imgCreateInfo.isFullscreen = pImageData->isFullscreen;

        RMT_RESOURCE_TYPE_IMAGE_TOKEN imgDesc(imgCreateInfo);

        m_rmtWriter.WriteTokenData(imgDesc);
        break;
    }

    case ResourceType::Buffer:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionBuffer));
        const auto* pBufferData = reinterpret_cast<const ResourceDescriptionBuffer*>(pRsrcCreateData->pDescription);

        // @TODO - add static asserts to make sure the bit positions in RMT_BUFFER_CREATE/USAGE_FLAGS match Pal values
        RMT_RESOURCE_TYPE_BUFFER_TOKEN bufferDesc(
            static_cast<uint8>(pBufferData->createFlags),
            static_cast<uint16>(pBufferData->usageFlags),
            pBufferData->size);

        m_rmtWriter.WriteTokenData(bufferDesc);
        break;
    }

    case ResourceType::Pipeline:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionPipeline));
        const auto* pPipelineData = reinterpret_cast<const ResourceDescriptionPipeline*>(pRsrcCreateData->pDescription);

        RMT_PIPELINE_CREATE_FLAGS flags;
        flags.CLIENT_INTERNAL   = pPipelineData->pCreateFlags->clientInternal;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 488
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 488) && (PAL_CLIENT_INTERFACE_MAJOR_VERSION < 502)
        flags.OVERRIDE_GPU_HEAP = 0;
#else
        flags.OVERRIDE_GPU_HEAP = pPipelineData->pCreateFlags->overrideGpuHeap;
#endif
#else
        flags.OVERRIDE_GPU_HEAP = 0;
#endif

        RMT_PIPELINE_HASH hash;
        hash.hashUpper = pPipelineData->pPipelineInfo->internalPipelineHash.unique;
        hash.hashLower = pPipelineData->pPipelineInfo->internalPipelineHash.stable;

        const auto& shaderHashes = pPipelineData->pPipelineInfo->shader;
        RMT_PIPELINE_STAGES stages;
        stages.PS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Pixel)].hash);
        stages.HS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Hull)].hash);
        stages.DS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Domain)].hash);
        stages.VS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Vertex)].hash);
        stages.GS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Geometry)].hash);
        stages.CS_STAGE = ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Compute)].hash);

        RMT_RESOURCE_TYPE_PIPELINE_TOKEN pipelineDesc(flags, hash, stages, false);

        m_rmtWriter.WriteTokenData(pipelineDesc);
        break;
    }

    case ResourceType::Heap:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionHeap));
        const auto* pHeapData = reinterpret_cast<const ResourceDescriptionHeap*>(pRsrcCreateData->pDescription);

        RMT_HEAP_FLAGS rmtFlags = {};
        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::NonRenderTargetDepthStencilTextures)))
        {
            rmtFlags.NON_RT_DS_TEXTURES = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::Buffers)))
        {
            rmtFlags.BUFFERS = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::CoherentSystemWide)))
        {
            rmtFlags.COHERENT_SYSTEM_WIDE = 1;
        }
        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::Primary)))
        {
            rmtFlags.PRIMARY = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::RenderTargetDepthStencilTextures)))
        {
            rmtFlags.RT_DS_TEXTURES = 1;
        }

        if (Util::TestAnyFlagSet(pHeapData->flags,
            static_cast<uint32>(ResourceDescriptionHeapFlags::DenyL0Demotion)))
        {
            rmtFlags.DENY_L0_PROMOTION = 1;
        }

        RMT_RESOURCE_TYPE_HEAP_TOKEN heapDesc(
            rmtFlags,
            pHeapData->size,
            RMT_PAGE_SIZE_4KB,  //< @TODO - we don't currently have this info, so just set to 4KB
            static_cast<uint8>(pHeapData->preferredGpuHeap));

        m_rmtWriter.WriteTokenData(heapDesc);
        break;
    }

    case ResourceType::GpuEvent:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionGpuEvent));
        const auto* pGpuEventData = reinterpret_cast<const ResourceDescriptionGpuEvent*>(pRsrcCreateData->pDescription);

        const bool isGpuOnly = (pGpuEventData->pCreateInfo->flags.gpuAccessOnly == 1);
        RMT_RESOURCE_TYPE_GPU_EVENT_TOKEN gpuEventDesc(isGpuOnly);

        m_rmtWriter.WriteTokenData(gpuEventDesc);
        break;
    }

    case ResourceType::BorderColorPalette:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionBorderColorPalette));
        const auto* pBcpData = reinterpret_cast<const ResourceDescriptionBorderColorPalette*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_BORDER_COLOR_PALETTE_TOKEN bcpDesc(static_cast<uint8>(pBcpData->pCreateInfo->paletteSize));

        m_rmtWriter.WriteTokenData(bcpDesc);
        break;
    }

    case ResourceType::PerfExperiment:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionPerfExperiment));
        const auto* pPerfExperimentData =
            reinterpret_cast<const ResourceDescriptionPerfExperiment*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_PERF_EXPERIMENT_TOKEN perfExperimentDesc(
            static_cast<uint32>(pPerfExperimentData->spmSize),
            static_cast<uint32>(pPerfExperimentData->sqttSize),
            static_cast<uint32>(pPerfExperimentData->perfCounterSize));

        m_rmtWriter.WriteTokenData(perfExperimentDesc);
        break;
    }

    case ResourceType::QueryPool:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionQueryPool));
        const auto* pQueryPoolData = reinterpret_cast<const ResourceDescriptionQueryPool*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_QUERY_HEAP_TOKEN queryHeapDesc(
            PalToRmtQueryHeapType(pQueryPoolData->pCreateInfo->queryPoolType),
            (pQueryPoolData->pCreateInfo->flags.enableCpuAccess == 1));

        m_rmtWriter.WriteTokenData(queryHeapDesc);
        break;
    }

    case ResourceType::VideoEncoder:
    {
        break;
    }

    case ResourceType::VideoDecoder:
    {
        break;
    }

    case ResourceType::DescriptorHeap:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionDescriptorHeap));
        const auto* pDescriptorHeapData =
            reinterpret_cast<const ResourceDescriptionDescriptorHeap*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_DESCRIPTOR_HEAP_TOKEN descriptorHeapDesc(
            PalToRmtDescriptorType(pDescriptorHeapData->type),
            pDescriptorHeapData->isShaderVisible,
            static_cast<uint8>(pDescriptorHeapData->nodeMask),
            static_cast<uint16>(pDescriptorHeapData->numDescriptors));

        m_rmtWriter.WriteTokenData(descriptorHeapDesc);
        break;
    }

    case ResourceType::DescriptorPool:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionDescriptorPool));
        const auto* pDescriptorPoolData =
            reinterpret_cast<const ResourceDescriptionDescriptorPool*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_POOL_SIZE_TOKEN poolSizeDesc(
            static_cast<uint16>(pDescriptorPoolData->maxSets),
            static_cast<uint8>(pDescriptorPoolData->numPoolSize));

        m_rmtWriter.WriteTokenData(poolSizeDesc);

        // Then loop through writing RMT_POOL_SIZE_DESCs
        for (uint32 i = 0; i < pDescriptorPoolData->numPoolSize; ++i)
        {
            RMT_POOL_SIZE_DESC poolSize(
                PalToRmtDescriptorType(pDescriptorPoolData->pPoolSizes[i].type),
                static_cast<uint16>(pDescriptorPoolData->pPoolSizes[i].numDescriptors));

            m_rmtWriter.WriteTokenData(poolSize);
        }
        break;
    }

    case ResourceType::CmdAllocator:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionCmdAllocator));
        const auto* pCmdAllocatorData =
            reinterpret_cast<const ResourceDescriptionCmdAllocator*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_CMD_ALLOCATOR_TOKEN cmdAllocatorDesc(
            PalToRmtCmdAllocatorCreateFlags(pCmdAllocatorData->pCreateInfo->flags),
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::CommandDataAlloc].suballocSize,
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::EmbeddedDataAlloc].suballocSize,
            PalToRmtHeapType(pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].allocHeap),
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].allocSize,
            pCmdAllocatorData->pCreateInfo->allocInfo[CmdAllocType::GpuScratchMemAlloc].suballocSize);

        m_rmtWriter.WriteTokenData(cmdAllocatorDesc);
        break;
    }

    case ResourceType::MiscInternal:
    {
        PAL_ASSERT(pRsrcCreateData->descriptionSize == sizeof(ResourceDescriptionMiscInternal));
        const auto* pMiscInternalData =
            reinterpret_cast<const ResourceDescriptionMiscInternal*>(pRsrcCreateData->pDescription);

        RMT_RESOURCE_TYPE_MISC_INTERNAL_TOKEN miscInternalDesc(PalToRmtMiscInternalType(pMiscInternalData->type));

        m_rmtWriter.WriteTokenData(miscInternalDesc);
        break;
    }

    case ResourceType::IndirectCmdGenerator:
        // IndirectCmdGenerator has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    case ResourceType::MotionEstimator:
        // MotionEstimator has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    case ResourceType::Timestamp:
        // Timestamp has no description data
        PAL_ASSERT(pRsrcCreateData->descriptionSize == 0);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

} // Pal
