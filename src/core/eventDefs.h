/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPlatform.h"
#include "palDevice.h"
#include "palEventDefs.h"
#include "palJsonWriter.h"
#include "palImage.h"
#include "palPipeline.h"
#include "palGpuEvent.h"
#include "palBorderColorPalette.h"
#include "palIndirectCmdGenerator.h"
#include "palQueryPool.h"
#include "palCmdAllocator.h"

namespace Pal
{

// =====================================================================================================================
// Common definitions
#define PAL_EVENT_LOG_VERSION 1

typedef uint32 PalEventVersion;

// Header data that will be written at the top of an event log file
struct PalEventFileHeader
{
    PalEventVersion version;
    uint32          headerSize;
};

// Header data that will be written with each event when sent to the DevDriver::EventServer
struct PalEventHeader
{
    PalEvent eventId;
    int64    timestamp;
    uint64   eventDataSize;
    uint32   padding;
};

// =====================================================================================================================
// Event Data Structures
struct RmtDataVersion
{
    uint16 major;
    uint16 minor;
};

struct CreateGpuMemoryData
{
    gpusize       size;
    gpusize       alignment;
    uint32        heapCount;
    GpuHeap       heaps[GpuHeapCount];
    bool          isVirtual;
    bool          isInternal;
    bool          isExternalShared;
    gpusize       gpuVirtualAddr;
    GpuMemHandle  handle;
};

struct DestroyGpuMemoryData
{
    GpuMemHandle handle;
    gpusize      gpuVirtualAddr;
};

struct GpuMemoryResourceBindData
{
    GpuMemHandle   handle;
    gpusize        gpuVirtualAddr;
    bool           isSystemMemory;
    ResourceHandle resourceHandle;
    gpusize        requiredSize;
    gpusize        offset;
};

struct GpuMemoryCpuMapData
{
    GpuMemHandle handle;
    gpusize      gpuVirtualAddr;
};

struct GpuMemoryCpuUnmapData
{
    GpuMemHandle handle;
    gpusize      gpuVirtualAddr;
};

struct GpuMemoryAddReferenceData
{
    GpuMemHandle handle;
    gpusize      gpuVirtualAddr;
    uint32       flags;
    QueueHandle  queueHandle;
    uint32       padding;
};

struct GpuMemoryRemoveReferenceData
{
    GpuMemHandle handle;
    gpusize      gpuVirtualAddr;
    QueueHandle  queueHandle;
};

struct GpuMemoryResourceCreateData
{
    ResourceHandle   handle;
    ResourceType     type;
    uint32           descriptionSize;
    const void*      pDescription;
};

struct ResourceUpdateInfoData
{
    ResourceHandle handle;
    uint32         subresourceId;
    ResourceType   type;
    uint32         before;
    uint32         after;
};

struct GpuMemoryResourceDestroyData
{
    ResourceHandle handle;
};

struct DebugNameData
{
    ResourceHandle handle;
    uint32         nameSize;
    const char*    pDebugName;
};

struct ResourceCorrelationData
{
    ResourceHandle handle;
    ResourceHandle driverHandle;
};

struct GpuMemoryMiscData
{
    MiscEventType type;
    EngineType    engine;
};

struct GpuMemorySnapshotData
{
    const char* pSnapshotName;
};

// =====================================================================================================================
// Helper functions

// =====================================================================================================================
// Returns a human-readable string for a ResourceType enum.
static const char* ResourceTypeToStr(
    ResourceType type)
{
    const char* pRet = "Unknown";
    switch (type)
    {
        case ResourceType::Image:                pRet = "Image";                break;
        case ResourceType::Buffer:               pRet = "Buffer";               break;
        case ResourceType::GpuEvent:             pRet = "GpuEvent";             break;
        case ResourceType::BorderColorPalette:   pRet = "BorderColorPalette";   break;
        case ResourceType::IndirectCmdGenerator: pRet = "IndirectCmdGenerator"; break;
        case ResourceType::MotionEstimator:      pRet = "MotionEstimator";      break;
        case ResourceType::PerfExperiment:       pRet = "PerfExperiment";       break;
        case ResourceType::QueryPool:            pRet = "QueryPool";            break;
        case ResourceType::VideoEncoder:         pRet = "VideoEncoder";         break;
        case ResourceType::VideoDecoder:         pRet = "VideoDecoder";         break;
        case ResourceType::Timestamp:            pRet = "Timestamp";            break;
        case ResourceType::Heap:                 pRet = "Heap";                 break;
        case ResourceType::Pipeline:             pRet = "Pipeline";             break;
        case ResourceType::DescriptorHeap:       pRet = "DescriptorHeap";       break;
        case ResourceType::DescriptorPool:       pRet = "DescriptorPool";       break;
        case ResourceType::CmdAllocator:         pRet = "CmdAllocator";         break;
        case ResourceType::MiscInternal:         pRet = "MiscInternal";         break;
        case ResourceType::Count:                pRet = "Unknown";              break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
    }

    return pRet;
}

// =====================================================================================================================
// Returns a human-readable string for a ResourceOwner enum.
static const char* ResourceOwnerToStr(
    ResourceOwner type)
{
    const char* pRet = "Unknown";
    switch (type)
    {
    case ResourceOwner::ResourceOwnerApplication: pRet = "Application"; break;
    case ResourceOwner::ResourceOwnerPal:         pRet = "Pal";         break;
    case ResourceOwner::ResourceOwnerPalClient:   pRet = "PalClient";   break;
    case ResourceOwner::ResourceOwnerUnknown:     pRet = "Unknown";     break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pRet;
}

// =====================================================================================================================
// Returns a human-readable string for a ResourceCategory enum.
static const char* ResourceCategoryToStr(
    ResourceCategory category)
{
    const char* pRet = "Unknown";
    switch (category)
    {
    case ResourceCategory::ResourceCategoryApplication: pRet = "Application"; break;
    case ResourceCategory::ResourceCategoryRpm:         pRet = "RPM";         break;
    case ResourceCategory::ResourceCategoryProfiling:   pRet = "Profiling";   break;
    case ResourceCategory::ResourceCategoryDebug:       pRet = "Debug";       break;
    case ResourceCategory::ResourceCategoryRayTracing:  pRet = "RayTracing";  break;
    case ResourceCategory::ResourceCategoryVideo:       pRet = "Video";       break;
    case ResourceCategory::ResourceCategoryMisc:        pRet = "Misc";        break;
    case ResourceCategory::ResourceCategoryUnknown:     pRet = "Unknown";     break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pRet;
}

// =====================================================================================================================
// Returns a human-readable string for a PalEvent enum.
static const char* PalEventToStr(
    PalEvent eventId)
{
    static_assert(static_cast<uint32>(PalEvent::Count) == 17, "Write support for new event!");

    const char* pRet = "Unknown";
    switch (eventId)
    {
    case PalEvent::CreateGpuMemory:          pRet = "CreateGpuMemory";          break;
    case PalEvent::DestroyGpuMemory:         pRet = "DestroyGpuMemory";         break;
    case PalEvent::GpuMemoryResourceBind:    pRet = "GpuMemoryResourceBind";    break;
    case PalEvent::GpuMemoryCpuMap:          pRet = "GpuMemoryCpuMap";          break;
    case PalEvent::GpuMemoryCpuUnmap:        pRet = "GpuMemoryCpuUnmap";        break;
    case PalEvent::GpuMemoryAddReference:    pRet = "GpuMemoryAddReference";    break;
    case PalEvent::GpuMemoryRemoveReference: pRet = "GpuMemoryRemoveReference"; break;
    case PalEvent::GpuMemoryResourceCreate:  pRet = "GpuMemoryResourceCreate";  break;
    case PalEvent::GpuMemoryResourceDestroy: pRet = "GpuMemoryResourceDestroy"; break;
    case PalEvent::DebugName:                pRet = "DebugName";                break;
    case PalEvent::GpuMemorySnapshot:        pRet = "GpuMemorySnapshot";        break;
    case PalEvent::GpuMemoryMisc:            pRet = "GpuMemoryMisc";            break;
    case PalEvent::ResourceCorrelation:      pRet = "ResourceCorrelation";      break;
    case PalEvent::ResourceInfoUpdate:       pRet = "ResourceInfoUpdate";       break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pRet;
}

// =====================================================================================================================
// Returns the RMT defined MiscEventType enum value from a Pal::MiscEventType enum.
static uint32 MiscEventTypeToRmtVal(
    MiscEventType eventId)
{
    uint32 ret = 6;
    switch (eventId)
    {
    case MiscEventType::SubmitGfx:               ret = 0;  break;
    case MiscEventType::SubmitCompute:           ret = 1;  break;
    case MiscEventType::Present:                 ret = 2;  break;
    case MiscEventType::InvalidateRanges:        ret = 3;  break;
    case MiscEventType::FlushMappedMemoryRanges: ret = 4;  break;
    case MiscEventType::Trim:                    ret = 5;  break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return ret;
}

// =====================================================================================================================
// Returns a human-readable string for an EngineType enum.
static const char* EngineTypeToStr(
    EngineType engine)
{
    const char* pRet = "Unknown";
    switch (engine)
    {
    case EngineType::EngineTypeUniversal:             pRet = "Universal";             break;
    case EngineType::EngineTypeCompute:               pRet = "Compute";               break;
    case EngineType::EngineTypeDma:                   pRet = "Dma";                   break;
    case EngineType::EngineTypeTimer:                 pRet = "Timer";                 break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pRet;
}

// =====================================================================================================================
// Returns a human-readable string for a CmdAllocType enum.
static const char* CmdAllocTypeToStr(
    CmdAllocType type)
{
    const char* pRet = "Unknown";
    switch (type)
    {
    case CmdAllocType::CommandDataAlloc:        pRet = "CommandDataAlloc";        break;
    case CmdAllocType::EmbeddedDataAlloc:       pRet = "EmbeddedDataAlloc";       break;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 803
    case CmdAllocType::LargeEmbeddedDataAlloc:  pRet = "LargeEmbeddedDataAlloc";  break;
#endif
    case CmdAllocType::GpuScratchMemAlloc:      pRet = "GpuScratchMemAlloc";      break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return pRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Serialization functions that write out the event data structs using the JSON writer for the event file log
static void BeginEventLogStream(
    Util::JsonWriter* pJsonWriter)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->BeginMap(false);
}

// =====================================================================================================================
static void EndEventLogStream(
    Util::JsonWriter* pJsonWriter)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->EndList();
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeEventLogFileHeader(
    Util::JsonWriter*         pJsonWriter,
    const PalEventFileHeader& header)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("FileVersion", header.version);
    pJsonWriter->KeyAndBeginList("Events", false);
}

// =====================================================================================================================
static void SerializeEventHeader(
    Util::JsonWriter*     pJsonWriter,
    const PalEventHeader& header)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->BeginMap(false);
    pJsonWriter->KeyAndValue("EventId", PalEventToStr(header.eventId));
    pJsonWriter->KeyAndValue("Timestamp", header.timestamp);
}

// =====================================================================================================================
static void SerializeCreateGpuMemoryData(
    Util::JsonWriter*          pJsonWriter,
    const CreateGpuMemoryData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("Size", data.size);
    pJsonWriter->KeyAndValue("Alignment", data.alignment);
    pJsonWriter->KeyAndValue("HeapCount", data.heapCount);
    pJsonWriter->KeyAndBeginList("Heaps", false);
    for (uint32 i = 0; i < static_cast<uint32>(data.heapCount); i++)
    {
        pJsonWriter->Value(data.heaps[i]);
    }
    pJsonWriter->EndList();
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->KeyAndValue("IsVirtual", data.isVirtual);
    pJsonWriter->KeyAndValue("IsInternal", data.isInternal);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeDestroyGpuMemoryData(
    Util::JsonWriter*           pJsonWriter,
    const DestroyGpuMemoryData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryResourceBindData(
    Util::JsonWriter*                pJsonWriter,
    const GpuMemoryResourceBindData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->KeyAndValue("RequiredSize", data.requiredSize);
    pJsonWriter->KeyAndValue("Offset", data.offset);
    pJsonWriter->KeyAndValue("ResourceHandle", data.resourceHandle);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryCpuMapData(
    Util::JsonWriter*          pJsonWriter,
    const GpuMemoryCpuMapData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryCpuUnmapData(
    Util::JsonWriter*            pJsonWriter,
    const GpuMemoryCpuUnmapData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryAddReferenceData(
    Util::JsonWriter*                pJsonWriter,
    const GpuMemoryAddReferenceData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->KeyAndValue("QueueHandle", data.queueHandle);
    pJsonWriter->KeyAndBeginMap("Flags", false);
    pJsonWriter->KeyAndValue("CantTrim", Util::TestAnyFlagSet(data.flags, GpuMemoryRefFlags::GpuMemoryRefCantTrim));
    pJsonWriter->KeyAndValue("MustSucceed", Util::TestAnyFlagSet(data.flags, GpuMemoryRefFlags::GpuMemoryRefMustSucceed));
    pJsonWriter->EndMap();
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryRemoveReferenceData(
    Util::JsonWriter*                   pJsonWriter,
    const GpuMemoryRemoveReferenceData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuMemHandle", data.handle);
    pJsonWriter->KeyAndValue("GpuVirtualAddress", data.gpuVirtualAddr);
    pJsonWriter->KeyAndValue("QueueHandle", data.queueHandle);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeDebugName(
    Util::JsonWriter*    pJsonWriter,
    const DebugNameData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("ResourceHandle", data.handle);
    pJsonWriter->KeyAndValue("DebugName", data.pDebugName);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryMisc(
    Util::JsonWriter*        pJsonWriter,
    const GpuMemoryMiscData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("Type", MiscEventTypeToRmtVal(data.type));
    pJsonWriter->KeyAndValue("Engine", EngineTypeToStr(data.engine));
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryDebugName(
    Util::JsonWriter*    pJsonWriter,
    const DebugNameData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("ResourceHandle", data.handle);
    pJsonWriter->KeyAndValue("DebugName", data.pDebugName);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemorySnapshot(
    Util::JsonWriter*            pJsonWriter,
    const GpuMemorySnapshotData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("SnapshotName", data.pSnapshotName);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionImage(
    Util::JsonWriter*               pJsonWriter,
    const ResourceDescriptionImage& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if((data.pCreateInfo != nullptr) && (data.pMemoryLayout != nullptr))
    {
        // Image create info
        pJsonWriter->KeyAndBeginMap("CreateFlags", false);
        pJsonWriter->KeyAndValue("Invariant",                      static_cast<bool>(data.pCreateInfo->flags.invariant));
        pJsonWriter->KeyAndValue("Cloneable",                      static_cast<bool>(data.pCreateInfo->flags.cloneable));
        pJsonWriter->KeyAndValue("Shareable",                      static_cast<bool>(data.pCreateInfo->flags.shareable));
        pJsonWriter->KeyAndValue("Flippable",                      static_cast<bool>(data.pCreateInfo->flags.flippable));
        pJsonWriter->KeyAndValue("Stereo",                         static_cast<bool>(data.pCreateInfo->flags.stereo));
        pJsonWriter->KeyAndValue("Cubemap",                        static_cast<bool>(data.pCreateInfo->flags.cubemap));
        pJsonWriter->KeyAndValue("PartiallyResidentTexture",       static_cast<bool>(data.pCreateInfo->flags.prt));
        pJsonWriter->KeyAndValue("NeedsSwizzleEquations",          static_cast<bool>(data.pCreateInfo->flags.needSwizzleEqs));
        pJsonWriter->KeyAndValue("PerSubresourceInit",             static_cast<bool>(data.pCreateInfo->flags.perSubresInit));
        pJsonWriter->KeyAndValue("SeparateDepthStencilPlaneInit",  static_cast<bool>(data.pCreateInfo->flags.separateDepthPlaneInit));
        pJsonWriter->KeyAndValue("RepetitiveResolve",              static_cast<bool>(data.pCreateInfo->flags.repetitiveResolve));
        pJsonWriter->KeyAndValue("PreferSwizzleEquations",         static_cast<bool>(data.pCreateInfo->flags.preferSwizzleEqs));
        pJsonWriter->KeyAndValue("FixedTileSwizzle",               static_cast<bool>(data.pCreateInfo->flags.fixedTileSwizzle));
        pJsonWriter->KeyAndValue("VideoReferenceOnly",             static_cast<bool>(data.pCreateInfo->flags.videoReferenceOnly));
        pJsonWriter->KeyAndValue("OptimalShareable",               static_cast<bool>(data.pCreateInfo->flags.optimalShareable));
        pJsonWriter->KeyAndValue("SamplePatternAlwaysKnown",       static_cast<bool>(data.pCreateInfo->flags.sampleLocsAlwaysKnown));
        pJsonWriter->KeyAndValue("FullResolveDstOnly", static_cast<bool>(data.pCreateInfo->flags.fullResolveDstOnly));
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndBeginMap("UsageFlags", false);
        pJsonWriter->KeyAndValue("ShaderRead",              static_cast<bool>(data.pCreateInfo->usageFlags.shaderRead));
        pJsonWriter->KeyAndValue("ShaderWrite",             static_cast<bool>(data.pCreateInfo->usageFlags.shaderWrite));
        pJsonWriter->KeyAndValue("ResolveSrc",              static_cast<bool>(data.pCreateInfo->usageFlags.resolveSrc));
        pJsonWriter->KeyAndValue("ResolveDst",              static_cast<bool>(data.pCreateInfo->usageFlags.resolveDst));
        pJsonWriter->KeyAndValue("ColorTarget",             static_cast<bool>(data.pCreateInfo->usageFlags.colorTarget));
        pJsonWriter->KeyAndValue("DepthStencil",            static_cast<bool>(data.pCreateInfo->usageFlags.depthStencil));
        pJsonWriter->KeyAndValue("NoStencilShaderRead",     static_cast<bool>(data.pCreateInfo->usageFlags.noStencilShaderRead));
        pJsonWriter->KeyAndValue("HiZNeverInvalid",         static_cast<bool>(data.pCreateInfo->usageFlags.hiZNeverInvalid));
        pJsonWriter->KeyAndValue("DepthAsZ24",              static_cast<bool>(data.pCreateInfo->usageFlags.depthAsZ24));
        pJsonWriter->KeyAndValue("FirstShaderWriteableMip", static_cast<bool>(data.pCreateInfo->usageFlags.firstShaderWritableMip));
        pJsonWriter->KeyAndValue("CornerSampling", static_cast<bool>(data.pCreateInfo->usageFlags.cornerSampling));
        pJsonWriter->KeyAndValue("VrsDepth", static_cast<bool>(data.pCreateInfo->usageFlags.vrsDepth));
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndValue("ImageType", static_cast<uint32>(data.pCreateInfo->imageType));

        pJsonWriter->KeyAndBeginMap("Dimensions", true);
        pJsonWriter->KeyAndValue("Width", data.pCreateInfo->extent.width);
        pJsonWriter->KeyAndValue("Height", data.pCreateInfo->extent.height);
        pJsonWriter->KeyAndValue("Depth", data.pCreateInfo->extent.depth);
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndValue("NumFormat", static_cast<uint32>(data.pCreateInfo->swizzledFormat.format));

        pJsonWriter->KeyAndBeginMap("ChannelMapping", false);
        pJsonWriter->KeyAndValue("R", static_cast<uint32>(data.pCreateInfo->swizzledFormat.swizzle.r));
        pJsonWriter->KeyAndValue("G", static_cast<uint32>(data.pCreateInfo->swizzledFormat.swizzle.g));
        pJsonWriter->KeyAndValue("B", static_cast<uint32>(data.pCreateInfo->swizzledFormat.swizzle.b));
        pJsonWriter->KeyAndValue("A", static_cast<uint32>(data.pCreateInfo->swizzledFormat.swizzle.a));
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndValue("MipLevels", data.pCreateInfo->mipLevels);
        pJsonWriter->KeyAndValue("ArraySize", data.pCreateInfo->arraySize);
        pJsonWriter->KeyAndValue("Samples", data.pCreateInfo->samples);
        pJsonWriter->KeyAndValue("Fragments", data.pCreateInfo->fragments);
        pJsonWriter->KeyAndValue("Tiling", static_cast<uint32>(data.pCreateInfo->tiling));
        pJsonWriter->KeyAndValue("TilingOptMode", static_cast<uint32>(data.pCreateInfo->tilingOptMode));
        pJsonWriter->KeyAndValue("MetadataMode", static_cast<uint32>(data.pCreateInfo->metadataMode));
        pJsonWriter->KeyAndValue("MaxBaseAlignment", data.pCreateInfo->maxBaseAlign);
        pJsonWriter->KeyAndValue("IsPresentable", data.isPresentable);
        pJsonWriter->KeyAndValue("IsFullscreen", data.isFullscreen);

        // Image memory layout
        pJsonWriter->KeyAndValue("ImageDataSize", data.pMemoryLayout->dataSize);
        pJsonWriter->KeyAndValue("ImageDataAlignment", data.pMemoryLayout->dataAlignment);
        pJsonWriter->KeyAndValue("MetadataOffset", data.pMemoryLayout->metadataOffset);
        pJsonWriter->KeyAndValue("MetadataSize", data.pMemoryLayout->metadataSize);
        pJsonWriter->KeyAndValue("MetadataAlignment", data.pMemoryLayout->metadataAlignment);
        pJsonWriter->KeyAndValue("MetadataHeaderOffset", data.pMemoryLayout->metadataHeaderOffset);
        pJsonWriter->KeyAndValue("MetadataHeaderSize", data.pMemoryLayout->metadataHeaderSize);
        pJsonWriter->KeyAndValue("MetadataHeaderAlignment", data.pMemoryLayout->metadataHeaderAlignment);
    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionBuffer(
    Util::JsonWriter*                pJsonWriter,
    const ResourceDescriptionBuffer& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    // Create flags
    pJsonWriter->KeyAndBeginMap("CreateFlags", false);
    pJsonWriter->KeyAndValue("SparseBinding",
        Util::TestAnyFlagSet(data.createFlags, static_cast<uint32>(ResourceDescriptionBufferCreateFlags::SparseBinding)));
    pJsonWriter->KeyAndValue("SparseResidency",
        Util::TestAnyFlagSet(data.createFlags, static_cast<uint32>(ResourceDescriptionBufferCreateFlags::SparseResidency)));
    pJsonWriter->KeyAndValue("SparseAliased ",
        Util::TestAnyFlagSet(data.createFlags, static_cast<uint32>(ResourceDescriptionBufferCreateFlags::SparseAliased)));
    pJsonWriter->KeyAndValue("Protected",
        Util::TestAnyFlagSet(data.createFlags, static_cast<uint32>(ResourceDescriptionBufferCreateFlags::Protected)));
    pJsonWriter->KeyAndValue("DeviceAddressCaptureReplay",
        Util::TestAnyFlagSet(data.createFlags, static_cast<uint32>(ResourceDescriptionBufferCreateFlags::DeviceAddressCaptureReplay)));
    pJsonWriter->EndMap();

    // Usage flags
    pJsonWriter->KeyAndBeginMap("UsageFlags", false);
    pJsonWriter->KeyAndValue("TransferSrc",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::TransferSrc)));
    pJsonWriter->KeyAndValue("TransferDst",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::TransferDst)));
    pJsonWriter->KeyAndValue("UniformTexelBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::UniformTexelBuffer)));
    pJsonWriter->KeyAndValue("StorageTexelBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::StorageTexelBuffer)));
    pJsonWriter->KeyAndValue("UniformBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::UniformBuffer)));
    pJsonWriter->KeyAndValue("StorageBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::StorageBuffer)));
    pJsonWriter->KeyAndValue("IndexBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::IndexBuffer)));
    pJsonWriter->KeyAndValue("VertexBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::VertexBuffer)));
    pJsonWriter->KeyAndValue("IndirectBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::IndirectBuffer)));
    pJsonWriter->KeyAndValue("TransformFeedbackBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::TransformFeedbackBuffer)));
    pJsonWriter->KeyAndValue("TransformFeedbackCounterBuffer",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::TransformFeedbackCounterBuffer)));
    pJsonWriter->KeyAndValue("ConditionalRendering",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::ConditionalRendering)));
    pJsonWriter->KeyAndValue("RayTracing",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::RayTracing)));
    pJsonWriter->KeyAndValue("ShaderDeviceAddress",
        Util::TestAnyFlagSet(data.usageFlags, static_cast<uint32>(ResourceDescriptionBufferUsageFlags::ShaderDeviceAddress)));
    pJsonWriter->EndMap();

    pJsonWriter->KeyAndValue("Size", data.size);

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionPipeline(
    Util::JsonWriter*                  pJsonWriter,
    const ResourceDescriptionPipeline& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if (data.pPipelineInfo != nullptr)
    {
        pJsonWriter->KeyAndBeginMap("CreateFlags", false);
        pJsonWriter->KeyAndValue("ClientInternal", static_cast<bool>(data.pCreateFlags->clientInternal));
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndValue("InternalPipelineHashStable", data.pPipelineInfo->internalPipelineHash.stable);
        pJsonWriter->KeyAndValue("InternalPipelineHashUnique", data.pPipelineInfo->internalPipelineHash.unique);

        const auto& shaderHashes = data.pPipelineInfo->shader;

        pJsonWriter->KeyAndBeginMap("Stages", false);
        pJsonWriter->KeyAndValue("PS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Pixel)].hash));
        pJsonWriter->KeyAndValue("HS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Hull)].hash));
        pJsonWriter->KeyAndValue("DS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Domain)].hash));
        pJsonWriter->KeyAndValue("VS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Vertex)].hash));
        pJsonWriter->KeyAndValue("GS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Geometry)].hash));
        pJsonWriter->KeyAndValue("CS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Compute)].hash));
        pJsonWriter->KeyAndValue("TS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Task)].hash));
        pJsonWriter->KeyAndValue("MS", ShaderHashIsNonzero(shaderHashes[static_cast<uint32>(ShaderType::Mesh)].hash));
        pJsonWriter->EndMap();
    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionHeap(
    Util::JsonWriter*              pJsonWriter,
    const ResourceDescriptionHeap& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("Size", data.size);
    pJsonWriter->KeyAndValue("Alignment", data.alignment);
    pJsonWriter->KeyAndValue("PreferredHeap", data.preferredGpuHeap);

    pJsonWriter->KeyAndBeginMap("Flags", false);
    pJsonWriter->KeyAndValue("NonRenderTargetDepthStencilTextures",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::NonRenderTargetDepthStencilTextures)));
    pJsonWriter->KeyAndValue("Buffers",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::Buffers)));
    pJsonWriter->KeyAndValue("CoherentSystemWide",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::CoherentSystemWide)));
    pJsonWriter->KeyAndValue("Primary",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::Primary)));
    pJsonWriter->KeyAndValue("RenderTargetDepthStencilTextures",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::RenderTargetDepthStencilTextures)));
    pJsonWriter->KeyAndValue("DenyL0Demotion",
        Util::TestAnyFlagSet(data.flags, static_cast<uint32>(ResourceDescriptionHeapFlags::DenyL0Demotion)));
    pJsonWriter->EndMap();

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionGpuEvent(
    Util::JsonWriter*                  pJsonWriter,
    const ResourceDescriptionGpuEvent& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("GpuAccessOnly", static_cast<bool>(data.pCreateInfo->flags.gpuAccessOnly));

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionBorderColorPalette(
    Util::JsonWriter*                            pJsonWriter,
    const ResourceDescriptionBorderColorPalette& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if (data.pCreateInfo != nullptr)
    {
        pJsonWriter->KeyAndValue("PaletteSize", data.pCreateInfo->paletteSize);
    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionPerfExperiment(
    Util::JsonWriter*                        pJsonWriter,
    const ResourceDescriptionPerfExperiment& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("SpmSize", data.spmSize);
    pJsonWriter->KeyAndValue("SqttSize", data.sqttSize);
    pJsonWriter->KeyAndValue("PerfCounterSize", data.perfCounterSize);

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionQueryPool(
    Util::JsonWriter*                   pJsonWriter,
    const ResourceDescriptionQueryPool& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if (data.pCreateInfo != nullptr)
    {
        pJsonWriter->KeyAndValue("QueryPoolType", static_cast<uint32>(data.pCreateInfo->queryPoolType));
        pJsonWriter->KeyAndValue("EnableCpuAccess", static_cast<bool>(data.pCreateInfo->flags.enableCpuAccess));
    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionVideoEncoder(
    Util::JsonWriter*                      pJsonWriter,
    const ResourceDescriptionVideoEncoder& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionVideoDecoder(
    Util::JsonWriter*                      pJsonWriter,
    const ResourceDescriptionVideoDecoder& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionDescriptorHeap(
    Util::JsonWriter*                        pJsonWriter,
    const ResourceDescriptionDescriptorHeap& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("DescriptorType", static_cast<uint32>(data.type));
    pJsonWriter->KeyAndValue("IsShaderVisible", data.isShaderVisible);
    pJsonWriter->KeyAndValue("NodeMask", data.nodeMask);
    pJsonWriter->KeyAndValue("NumDescriptors", data.numDescriptors);

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionDescriptorPool(
    Util::JsonWriter*                        pJsonWriter,
    const ResourceDescriptionDescriptorPool& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if ((data.pPoolSizes != nullptr) && (data.numPoolSize > 0))
    {
        pJsonWriter->KeyAndValue("MaxSets", data.maxSets);
        pJsonWriter->KeyAndBeginList("PoolSizes", false);
        for (uint32 i = 0; i < data.numPoolSize; i++)
        {
            pJsonWriter->BeginMap(false);
            pJsonWriter->KeyAndValue("DescriptorType", static_cast<uint32>(data.pPoolSizes[i].type));
            pJsonWriter->KeyAndValue("NumDescriptors", data.pPoolSizes[i].numDescriptors);
            pJsonWriter->EndMap();
        }
        pJsonWriter->EndList();
    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionCmdAllocator(
    Util::JsonWriter*                      pJsonWriter,
    const ResourceDescriptionCmdAllocator& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    if (data.pCreateInfo != nullptr)
    {
        pJsonWriter->KeyAndBeginMap("AllocInfo", false);
        for (uint32 i = 0; i < static_cast<uint32>(CmdAllocatorTypeCount); i++)
        {
            pJsonWriter->KeyAndBeginMap(CmdAllocTypeToStr(static_cast<CmdAllocType>(i)), false);
            pJsonWriter->KeyAndValue("PreferredHeap", data.pCreateInfo->allocInfo[i].allocHeap);
            pJsonWriter->KeyAndValue("AllocSize", data.pCreateInfo->allocInfo[i].allocSize);
            pJsonWriter->KeyAndValue("SuballocSize", data.pCreateInfo->allocInfo[i].suballocSize);
            pJsonWriter->EndMap();
        }
        pJsonWriter->EndMap();

        pJsonWriter->KeyAndBeginMap("Flags", false);
        pJsonWriter->KeyAndValue("AutoMemoryReuse",          static_cast<bool>(data.pCreateInfo->flags.autoMemoryReuse));
        pJsonWriter->KeyAndValue("DisableBusyChunkTracking", static_cast<bool>(data.pCreateInfo->flags.disableBusyChunkTracking));
        pJsonWriter->KeyAndValue("ThreadSafe",               static_cast<bool>(data.pCreateInfo->flags.threadSafe));
        pJsonWriter->EndMap();

    }
    else
    {
        pJsonWriter->KeyAndNullValue("InvalidData");
    }

    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeResourceDescriptionMiscInternal(
    Util::JsonWriter*                      pJsonWriter,
    const ResourceDescriptionMiscInternal& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("Type", static_cast<uint32>(data.type));
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryResourceCreate(
    Util::JsonWriter*                  pJsonWriter,
    const GpuMemoryResourceCreateData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("ResourceHandle", data.handle);
    pJsonWriter->KeyAndValue("ResourceType", ResourceTypeToStr(data.type));
    pJsonWriter->KeyAndValue("DescriptionSize", data.descriptionSize);
    if (data.pDescription != nullptr)
    {
        pJsonWriter->KeyAndBeginMap("Description", false);
        switch (data.type)
        {
        case ResourceType::Image:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionImage));
            SerializeResourceDescriptionImage(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionImage*>(data.pDescription)));
            break;
        case ResourceType::Buffer:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionBuffer));
            SerializeResourceDescriptionBuffer(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionBuffer*>(data.pDescription)));
            break;
        case ResourceType::Pipeline:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionPipeline));
            SerializeResourceDescriptionPipeline(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionPipeline*>(data.pDescription)));
            break;
        case ResourceType::Heap:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionHeap));
            SerializeResourceDescriptionHeap(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionHeap*>(data.pDescription)));
            break;
        case ResourceType::GpuEvent:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionGpuEvent));
            SerializeResourceDescriptionGpuEvent(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionGpuEvent*>(data.pDescription)));
            break;
        case ResourceType::BorderColorPalette:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionBorderColorPalette));
            SerializeResourceDescriptionBorderColorPalette(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionBorderColorPalette*>(data.pDescription)));
            break;
        case ResourceType::PerfExperiment:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionPerfExperiment));
            SerializeResourceDescriptionPerfExperiment(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionPerfExperiment*>(data.pDescription)));
            break;
        case ResourceType::QueryPool:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionQueryPool));
            SerializeResourceDescriptionQueryPool(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionQueryPool*>(data.pDescription)));
            break;
        case ResourceType::VideoEncoder:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionVideoEncoder));
            SerializeResourceDescriptionVideoEncoder(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionVideoEncoder*>(data.pDescription)));
            break;
        case ResourceType::VideoDecoder:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionVideoDecoder));
            SerializeResourceDescriptionVideoDecoder(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionVideoDecoder*>(data.pDescription)));
            break;
        case ResourceType::DescriptorHeap:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionDescriptorHeap));
            SerializeResourceDescriptionDescriptorHeap(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionDescriptorHeap*>(data.pDescription)));
            break;
        case ResourceType::DescriptorPool:
            SerializeResourceDescriptionDescriptorPool(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionDescriptorPool*>(data.pDescription)));
            break;
        case ResourceType::CmdAllocator:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionCmdAllocator));
            SerializeResourceDescriptionCmdAllocator(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionCmdAllocator*>(data.pDescription)));
            break;
        case ResourceType::MiscInternal:
            PAL_ASSERT(data.descriptionSize == sizeof(ResourceDescriptionMiscInternal));
            SerializeResourceDescriptionMiscInternal(
                pJsonWriter,
                *(static_cast<const ResourceDescriptionMiscInternal*>(data.pDescription)));
            break;
        case ResourceType::IndirectCmdGenerator:
        case ResourceType::MotionEstimator:
        case ResourceType::Timestamp:
        case ResourceType::Count:
            // These resource types should have no description data
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    pJsonWriter->EndMap();
}

// =====================================================================================================================
static void SerializeGpuMemoryResourceDestroy(
    Util::JsonWriter*                   pJsonWriter,
    const GpuMemoryResourceDestroyData& data)
{
    PAL_ASSERT(pJsonWriter != nullptr);
    pJsonWriter->KeyAndValue("ResourceHandle", data.handle);
    pJsonWriter->EndMap();
}

} // Pal
