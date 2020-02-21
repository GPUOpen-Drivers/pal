/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palEventDefs.h
 * @brief Defines the Platform Abstraction Library (PAL) structures and types required for Event Logging.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palPipeline.h"
#include "palShaderLibrary.h"

#define PAL_GPU_MEMORY_LOGGING_VERSION 531

namespace Pal
{

// Forward declarations
struct BorderColorPaletteCreateInfo;
struct CmdAllocatorCreateInfo;
struct GpuEventCreateInfo;
struct ImageCreateInfo;
struct ImageMemoryLayout;
struct QueryPoolCreateInfo;

/// Enumeration of events that PAL clients can log through the @IPlatform::LogEvent() call.
enum class PalEvent : uint32
{
    CreateGpuMemory          = 0,
    DestroyGpuMemory         = 1,
    GpuMemoryResourceCreate  = 2,
    GpuMemoryResourceDestroy = 3,
    GpuMemoryMisc            = 4,
    GpuMemorySnapshot        = 5,
    DebugName                = 6,
    GpuMemoryResourceBind    = 7,
    GpuMemoryCpuMap          = 8,
    GpuMemoryCpuUnmap        = 9,
    GpuMemoryAddReference    = 10,
    GpuMemoryRemoveReference = 11,
};

typedef uint64 GpuMemHandle;
typedef uint64 ResourceHandle;
typedef uint64 QueueHandle;

/// Specifies the types of resources that can have GPU memory bound to them. Used for GPU Memory Event logging.
enum class ResourceType : uint32
{
    Image = 0,
    Buffer = 1,
    Pipeline = 2,
    Heap = 3,
    GpuEvent = 4,
    BorderColorPalette = 5,
    IndirectCmdGenerator = 6,
    MotionEstimator = 7,
    PerfExperiment = 8,
    QueryPool = 9,
    VideoEncoder = 10,
    VideoDecoder = 11,
    Timestamp = 12,
    DescriptorHeap = 13,
    DescriptorPool = 14,
    CmdAllocator = 15,
    MiscInternal = 16,

    Count = 17
};

/// Enumeration of miscellaneous events, used for GPU memory event logging
enum class MiscEventType : uint32
{
    SubmitGfx = 0,
    SubmitCompute = 1,
    Present = 2,
    InvalidateRanges = 3,
    FlushMappedMemoryRanges = 4,
    Trim = 5,
};

/// Describes the binding of a GPU Memory object to a resource
struct GpuMemoryResourceBindEventData
{
    const void*         pObj;               ///< Opaque pointer to the resource having memory bound to it.
    gpusize             requiredGpuMemSize; ///< GPU memory size required by pObj.
    const IGpuMemory*   pGpuMemory;         ///< IGpuMemory object being bound to the resource.
    gpusize             offset;             ///< Offset within pGpuMemory where the resource is being bound.
    bool                isSystemMemory;     ///< If true then system memory is being bound to the object. In this case,
                                            ///  pGpuMemory and offset should be set to zero.
};

/// Describes the creation of an object relevant to GpuMemory event logging
struct ResourceCreateEventData
{
    const void*         pObj;              ///< Opaque pointer to the object that was created
    ResourceType        type;              ///< Type of resource being described
    const void*         pResourceDescData; ///< Pointer to memory containing the resource type-specific description data
    uint32              resourceDescSize;  ///< Size of the memory pointed to by pEventData
};

// Event data related to the destruction of an object relevant to GpuMemory event logging
struct ResourceDestroyEventData
{
    const void* pObj;  ///< Opaque pointer to the object being destroyed
};

/// Event data related for a DebugName event
struct DebugNameEventData
{
    const void* pObj;       ///< Opaque pointer to the object being named.
    const char* pDebugName; ///< String name being given to the object.
};

/// Event data for a GpuMemoryMisc event
struct MiscEventData
{
    MiscEventType eventType;  ///< Type of miscellaneous event being logged
    EngineType    engine;     ///< Engine associated with the event, can be EngineTypeCount if not applicable.
};

/// Event data for a GPU Memory Snapshot event, this adds a named marker to the GPU Memory event stream.
struct GpuMemorySnapshotEventData
{
    const char* pSnapshotName;  ///< Name of the snapshot being created
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Resource Type-Specific Description Structures

/// Describes an Image Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter for
/// Image ResourceTypes.
struct ResourceDescriptionImage
{
    const ImageCreateInfo*   pCreateInfo;    //< Pointer to the image create info.
    const ImageMemoryLayout* pMemoryLayout;  //< Pointer to the image memory layout.
    bool                     isPresentable;  //< Flag indicating if the image is presentable.
    bool                     isFullscreen;   //< Flag indicating if the image can be used for fullscreen present.
};

/// Bitmask flags used to describe Buffer creation info for GPU memory event logging. This list mirrors the VK list.
enum class ResourceDescriptionBufferCreateFlags : uint32
{
    SparseBinding              = 0x1,
    SparseResidency            = 0x2,
    SparseAliased              = 0x4,
    Protected                  = 0x8,
    DeviceAddressCaptureReplay = 0x10,
};

/// Bitmask flags used to describe Buffer usage info for GPU memory event logging. This list mirrors the VK list.
enum class ResourceDescriptionBufferUsageFlags : uint32
{
    TransferSrc                     = 0x1,
    TransferDst                     = 0x2,
    UniformTexelBuffer              = 0x4,
    StorageTexelBuffer              = 0x8,
    UniformBuffer                   = 0x10,
    StorageBuffer                   = 0x20,
    IndexBuffer                     = 0x40,
    VertexBuffer                    = 0x80,
    IndirectBuffer                  = 0x100,
    ConditionalRendering            = 0x200,
    RayTracing                      = 0x400,
    TransformFeedbackBuffer         = 0x800,
    TransformFeedbackCounterBuffer  = 0x1000,
    ShaderDeviceAddress             = 0x20000,
};

/// Describes a Buffer Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter for
/// Buffer ResourceTypes.
struct ResourceDescriptionBuffer
{
    uint64 size;        //< Size of the buffer, in bytes.
    uint32 createFlags; //< Buffer create flags, see @ResourceDescriptionBufferCreateFlags
    uint32 usageFlags;  //< Buffer usage flags, see @ResourceDescriptionBufferUsageFlags
};

/// Describes a Pipeline Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter for
/// Pipeline ResourceTypes.
struct ResourceDescriptionPipeline
{
    const PipelineInfo*        pPipelineInfo;  //< Pointer to the PipelineInfo.
    const PipelineCreateFlags* pCreateFlags;   //< Pipeline create flags.
};

/// Describes a Shader Library Resource,
struct ResourceDescriptionShaderLibrary
{
    const LibraryInfo*        pLibrarynfo;  //< Pointer to the LibraryInfo.
    const LibraryCreateFlags* pCreateFlags; //< LibraryInfo create flags.
};

/// Bitmask flags used to describe a Heap resource for GPU memory event logging.
enum class ResourceDescriptionHeapFlags : uint32
{
    NonRenderTargetDepthStencilTextures = 0x2,
    Buffers                             = 0x4,
    CoherentSystemWide                  = 0x8,
    Primary                             = 0x10,
    RenderTargetDepthStencilTextures    = 0x20,
    DenyL0Demotion                      = 0x40,
};

/// Describes a Heap Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter for
/// Heap ResourceTypes.
struct ResourceDescriptionHeap
{
    uint64  size;              //< Size of the heap, in bytes.
    uint64  alignment;         //< Alignment of the heap.
    GpuHeap preferredGpuHeap;  //< The GPU heap that the heap was requested to be placed in.
    uint32  flags;             //< Flags associated with the heap. see @ResourceDescriptionHeapFlags.
};

/// Describes a GpuEvent Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter
/// for GpuEvent ResourceTypes.
struct ResourceDescriptionGpuEvent
{
    const GpuEventCreateInfo* pCreateInfo;   //< Pointer to GpuEvent create info.
};

/// Describes a BorderColorPalette Resource passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription
/// parameter for BorderColorPalette ResourceTypes.
struct ResourceDescriptionBorderColorPalette
{
    const BorderColorPaletteCreateInfo* pCreateInfo;  //< Pointer to BorderColorPalette create info.

};

/// Describes a PerfExperiment Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription
/// parameter for PerfExperiment ResourceTypes.
struct ResourceDescriptionPerfExperiment
{
    gpusize spmSize;            //< Bytes of GPU memory required by this perf experiment for SPM data.
    gpusize sqttSize;           //< Bytes of GPU memory required by this perf experiment for SQTT data.
    gpusize perfCounterSize;    //< Bytes of GPU memory required by this perf experiment for Perf Counter data.
};

/// Describes a QueryPool Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription parameter
/// for QueryPool ResourceTypes.
struct ResourceDescriptionQueryPool
{
    const QueryPoolCreateInfo* pCreateInfo;  //< Pointer to the QueryPool create info.
};

/// Describes a VideoEncoder Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription
/// parameter for VideoEncoder ResourceTypes.
struct ResourceDescriptionVideoEncoder
{
};

/// Describes a VideoDecoder Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription.
/// parameter for VideoDecoder ResourceTypes.
struct ResourceDescriptionVideoDecoder
{
};

/// Enumeration of Descriptor types for GPU memory event logging.
enum class ResourceDescriptionDescriptorType : uint32
{
    ConstantBufferShaderResourceUAV = 1,
    Sampler                         = 2,
    RenderTargetView                = 3,
    DepthStencilView                = 4,
    CombinedImageSampler            = 5,
    SampledImage                    = 6,
    StorageImage                    = 7,
    UniformTexelBuffer              = 8,
    StorageTexelBuffer              = 9,
    UniformBuffer                   = 10,
    StorageBuffer                   = 11,
    UniformBufferDynamic            = 12,
    StorageBufferDynamic            = 13,
    InputAttachment                 = 14,
    InlineUniformBlock              = 15,
    AccelerationStructure           = 16,
    Count                           = 17,
};

/// Describes a Descriptor Heap, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription.
/// parameter for DescriptorHeap ResourceTypes.
struct ResourceDescriptionDescriptorHeap
{
    ResourceDescriptionDescriptorType type;            //< Type of descriptors this heap contains.
    bool                              isShaderVisible; //< Flag indicating whether the heap is shader-visible.
    uint32                            nodeMask;        //< For single adapter this is set to zero, for multiple adapter
                                                       //< mode this is a bitmask to identify which adapters the heap applies to.
    uint32                            numDescriptors;  //< The number of descriptors in the heap.
};

/// Describes the type and size for a particular decriptor type in a Descriptor Pool
struct ResourceDescriptionPoolSize
{
    ResourceDescriptionDescriptorType type;           //< Type of descriptors this pool contains.
    uint32                            numDescriptors; //< Number of descriptors to be allocated by this pool.
};

/// Describes a Descriptor Pool, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription.
/// parameter for DescriptorPool ResourceTypes.
struct ResourceDescriptionDescriptorPool
{
    uint32                             maxSets;      //< Maximum number of descriptor sets that can be allocated from the pool.
    uint32                             numPoolSize;  //< The number of pool size structs in pPoolSizes;
    const ResourceDescriptionPoolSize* pPoolSizes;   //< Array of PoolSize structs.
};

/// Describes a CmdAllocator Resource, passed to @IPlatform::LogResourceDescEvent() as the pResourceDescription
/// parameter for CmdAllocator ResourceTypes
struct ResourceDescriptionCmdAllocator
{
    const CmdAllocatorCreateInfo* pCreateInfo;  //< Pointer to the CmdAllocator create info.
};

/// Enumeration of the miscellaneous types of internal GPU memory allocation
enum class MiscInternalAllocType : uint32
{
    OcclusionQueryResetData   = 0,
    Cpdmapatch                = 1,
    OcclusionQueryResultPair  = 2,
    ShaderMemory              = 3,
    ShaderRing                = 4,
    SrdTable                  = 5,
    DebugStallMemory          = 6,
    FrameCountMemory          = 7,
    PipelinePerfData          = 8,
    PageFaultSRD              = 9,
    DummyChunk                = 10,
    DelagDevice               = 11,
    TileGridMemory            = 12,
    Fmaskmemory               = 13,
    VideoDecoderHeap          = 14,
    Unknown                   = 15,
};

/// Describes a miscellaneous internal GPU memory allocation
struct ResourceDescriptionMiscInternal
{
    MiscInternalAllocType type; //< The type of the miscellaneous internal allocation
};
} // Pal
