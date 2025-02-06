/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palPipelineAbi.h"
#include "palInlineFuncs.h"
#include "palMsgPack.h"

namespace Util
{
namespace HsaAbi
{

namespace PipelineMetadataKey
{
    constexpr const char Name[]    = ".name";
    constexpr const char Version[] = "amdhsa.version";
    constexpr const char Kernels[] = "amdhsa.kernels";
};

/// An enum version of the legal ".value_kind" strings. It's required so there's no "None" value.
enum class ValueKind : uint32
{
    ByValue = 0,            ///< Copy the value directly into the kernel argument buffer.
    GlobalBuffer,           ///< A global address space pointer to buffer data.
    DynamicSharedPointer,   ///< A group address space pointer to dynamically allocated LDS.
    Sampler,                ///< A global address space pointer to a sampler SRD (S#).
    Image,                  ///< A global address space pointer to an image SRD (T#).
    Pipe,                   ///< A global address space pointer to an OpenCL pipe.
    Queue,                  ///< A global address space pointer to an OpenCL device enqueue queue.
    HiddenGlobalOffsetX,    ///< The OpenCL grid dispatch global offset for the X dimension.
    HiddenGlobalOffsetY,    ///< The OpenCL grid dispatch global offset for the Y dimension.
    HiddenGlobalOffsetZ,    ///< The OpenCL grid dispatch global offset for the Z dimension.
    HiddenNone,             ///< Space must be reserved for this argument but it will not be used.
    HiddenPrintfBuffer,     ///< A global address space pointer to the runtime printf buffer.
    HiddenHostcallBuffer,   ///< A global address space pointer to the runtime hostcall buffer.
    HiddenDefaultQueue,     ///< A global address space pointer to the default OpenCL device enqueue queue.
    HiddenCompletionAction, ///< A global address space pointer to help link enqueued kernels into the ancestor tree.
    HiddenMultigridSyncArg, ///< A global address space pointer for multi-grid synchronization.
    HiddenBlockCountX,      ///< The grid dispatch complete work-group count for the X dimension
    HiddenBlockCountY,      ///< The grid dispatch complete work-group count for the Y dimension
    HiddenBlockCountZ,      ///< The grid dispatch complete work-group count for the Z dimension
    HiddenGroupSizeX,       ///< total grid size for complete workgroups for X dim (in work-items)
    HiddenGroupSizeY,       ///< total grid size for complete workgroups for Y dim (in work-items)
    HiddenGroupSizeZ,       ///< total grid size for complete workgroups for Z dim (in work-items)
    HiddenRemainderX,       ///< Dispatch workgroup size of the partial work group of the X dimension, if it exists.
    HiddenRemainderY,       ///< Dispatch workgroup size of the partial work group of the Y dimension, if it exists.
    HiddenRemainderZ,       ///< Dispatch workgroup size of the partial work group of the Z dimension, if it exists.
    HiddenGridDims,         ///< Dispatch grid dimensionality, value between 1 and 3
    HiddenHeapV1,           ///< Global address pointer to an initialized memory buffer for device side malloc/free
    HiddenDynamicLdsSize,   ///< Size of the dynamically allocated LDS memory is passed in the kernarg.
    HiddenQueuePtr          ///< A global memory address space pointer to the ROCm runtime struct amd_queue_t structure
                            ///< for the HSA queue of the associated dispatch AQL packet
};

/// An enum of the legal ".address_space" strings.
enum class AddressSpace : uint32
{
    None = 0, ///< This value was not provided.
    Private,  ///< Scratch space memory.
    Global,   ///< Global GPU memory.
    Constant, ///< Global GPU memory that is read only (permits scalar reads).
    Local,    ///< LDS memory.
    Generic,  ///< Flat access, address can map to global memory, scratch, or LDS.
    Region,   ///< GDS memory.
};

/// An enum of the legal ".access" and ".actual_access" strings.
enum class Access : uint32
{
    None = 0,  ///< This value was not provided.
    ReadOnly,  ///< Read only access.
    WriteOnly, ///< Write only access.
    ReadWrite, ///< Read and write access.
};

/// An enum of the legal ".kind" strings.
enum class Kind : uint32
{
    Normal = 0,  ///< A normal kernel (the default if not specified).
    Init,        ///< Must run when loaded and before any Normal kernels.
    Fini,        ///< Must run after all Init and Normal kernels.
};

/// A single kernel argument.
/// See: https://llvm.org/docs/AMDGPUUsage.html#amdgpu-amdhsa-code-object-kernel-argument-metadata-map-table-v5
struct KernelArgument
{
    const char*  pName;        ///< Optional: Kernel argument name.
    const char*  pTypeName;    ///< Optional: Kernel argument type name.
    uint32       size;         ///< Required: Kernel argument size in bytes.
    uint32       offset;       ///< Required: Kernel argument offset in bytes. The offset must be a multiple of the
                               ///            alignment required by the argument.
    ValueKind    valueKind;    ///< Required: Specifies how to set up the corresponding argument.
    uint32       pointeeAlign; ///< Optional: Alignment in bytes of pointee type. Must be a power of 2.  Only present
                               ///            if valueKind is DynamicSharedPointer. It will be zero if not present.
    AddressSpace addressSpace; ///< Optional: Only present if valueKind is GlobalBuffer or DynamicSharedPointer.
    Access       access;       ///< Optional: Argument access qualifier. Only present if valueKind is Image or Pipe.
    Access       actualAccess; ///< Optional: The actual memory accesses performed by the kernel on the kernel argument.
                               ///            Only present if valueKind is GlobalBuffer, Image, or Pipe.

    union
    {
        struct
        {
            uint32 isConst    : 1;  ///< Optional: Is const qualified. Only present if valueKind is GlobalBuffer.
            uint32 isRestrict : 1;  ///< Optional: Is restrict qualified. Only present if valueKind is GlobalBuffer.
            uint32 isVolatile : 1;  ///< Optional: Is volatile qualified. Only present if valueKind is GlobalBuffer.
            uint32 isPipe     : 1;  ///< Optional: Is pipe qualified. Only present if valueKind is Pipe.
            uint32 reserved   : 28; ///< Reserved, must be 0.
        };
        uint32 uAll; ///< Flags packed as 32-bit uint.
    } flags;         ///< If the kernel argument has certain properties.
};

// =====================================================================================================================
/// The set of all HSA code object metadata we need.
/// See: https://llvm.org/docs/AMDGPUUsage.html#code-object-v5-metadata
class CodeObjectMetadata final
{
public:
    /// The CodeObjectMetadata constructor. It must be templated to construct m_allocator.
    template <typename Allocator>
    CodeObjectMetadata(Allocator*const pAllocator);

    /// The CodeObjectMetadata destructor.
    ~CodeObjectMetadata();

    /// Associate a metadata version pair with this object. Must be called before any of the deserialize functions.
    ///
    /// @param [in] metadataMajorVer The major metadata version.
    /// @param [in] metadataMinorVer The minor metadata version.
    ///
    /// @Returns Success if the version is supported, otherwise ErrorUnsupportedPipelineElfAbiVersion.
    Result SetVersion(
        uint32 metadataMajorVer,
        uint32 metadataMinorVer);

    /// Parses all HSA metadata from the note section of an HSA code object into this metadata instance.
    ///
    /// @param [in] pReader          Use this message pack reader.
    /// @param [in] pRawMetadata     The content of the metadata note section.
    /// @param [in] metadataSize     The length of the note section.
    ///
    /// @returns Success if successful, ErrorInvalidValue if the metadata is incorrect, or ErrorUnavailable if the
    ///          metadata is not supported.
    Result DeserializeNote(
        MsgPackReader*   pReader,
        const void*      pRawMetadata,
        uint32           metadataSize,
        StringView<char> kernelName);

    /// Returns a c-string pointer which names the kernel.
    const char* KernelName() const { return m_pName; }

    /// Returns a c-string pointer which names the kernel descriptor the runtimer should use.
    const char* KernelDescriptorSymbol() const { return m_pSymbol; }

    /// Returns a pointer to the kernel argument array or nullptr if there are no arguments.
    const KernelArgument* Arguments() const { return m_pArgs; }

    /// Returns the number of kernel arguments.
    uint32 NumArguments() const { return m_numArgs; }

    /// Returns the compile-time required number of threads in the X dimension. Can be zero if it's not specified.
    uint32 RequiredWorkgroupSizeX() const { return m_reqdWorkgroupSize[0]; }

    /// Returns the compile-time required number of threads in the Y dimension. Can be zero if it's not specified.
    uint32 RequiredWorkgroupSizeY() const { return m_reqdWorkgroupSize[1]; }

    /// Returns the compile-time required number of threads in the Z dimension. Can be zero if it's not specified.
    uint32 RequiredWorkgroupSizeZ() const { return m_reqdWorkgroupSize[2]; }

    /// Returns the largest supported number of threads in an entire workgroup (X * Y * Z).
    uint32 MaxFlatWorkgroupSize() const { return m_maxFlatWorkgroupSize; }

    /// Returns the size of the kernel argument buffer in bytes.
    uint32 KernargSegmentSize() const { return m_kernargSegmentSize; }

    /// Returns the max byte alignment of the arguments. Must be a power of 2.
    uint32 KernargSegmentAlign() const { return m_kernargSegmentAlign; }

    /// Returns the expected wavefront size. Must be a power of 2.
    uint32 WavefrontSize() const { return m_wavefrontSize; }

    /// Returns the amount of group segment memory (LDS) required by a workgroup in bytes.
    uint32 GroupSegmentFixedSize() const { return m_groupSegmentFixedSize; }

    /// Returns the amount of fixed private address space memory (scratch) required by a work-item in bytes.
    uint32 PrivateSegmentFixedSize() const;

    uint32 UniformWorkgroupSize() const { return m_uniformWorkgroupSize; }

    bool UsesDynamicStack() const { return m_usesDynamicStack; }

    bool WorkgroupProcessorMode() const { return m_workgroupProcessorMode; }

    /// Returns what kind of kernel this is.
    Kind KernelKind() const { return m_kind; }

private:
    Result DeserializeKernel(MsgPackReader* pReader);
    Result DeserializeKernelArgs(MsgPackReader* pReader);

    IndirectAllocator m_allocator;    ///< All dynamically allocated memory comes from this allocator.

    uint32 m_codeVersionMajor;        ///< Metadata major version.
    uint32 m_codeVersionMinor;        ///< Metadata minor version.

    // PAL only supports code objects that contain a single kernel. This is that kernel's metadata.
    const char*     m_pName;               ///< Required: Source name of the kernel.
    const char*     m_pSymbol;             ///< Required: Name of the kernel descriptor ELF symbol.
    const char*     m_pLanguage;           ///< Optional: Source language of the kernel, null if not present.
    uint32          m_languageVersion[2];  ///< Optional: The language's major and minor versions (major, then minor).
    KernelArgument* m_pArgs;               ///< Optional: The array of kernel arguments, null if not present.
    uint32          m_numArgs;             ///< Optional: The length of m_pArgs, zero if not present.

    uint32      m_reqdWorkgroupSize[3];    ///< Optional: Must be all zero or all non-zero. Specifies threads per group.
    uint32      m_workgroupSizeHint[3];    ///< Optional: The dynamic thread group size is likely to be this.
    const char* m_pVecTypeHint;            ///< Optional: The name of a scalar or vector type.
    const char* m_pDeviceEnqueueSymbol;    ///< Optional: The external symbol name associated with a kernel.
    uint32      m_kernargSegmentSize;      ///< Required: The size of the kernel argument buffer in bytes.
    uint32      m_groupSegmentFixedSize;   ///< Required: Group segment memory required by a workgroup in bytes.
    uint32      m_privateSegmentFixedSize; ///< Required: Fixed scratch space required by a work-item in bytes.
    uint32      m_kernargSegmentAlign;     ///< Required: The max byte alignment of the arguments. Must be a power of 2.
    uint32      m_wavefrontSize;           ///< Required: Wavefront size. Must be a power of 2.
    uint32      m_sgprCount;               ///< Required: Number of scalar registers required by a wavefront.
    uint32      m_vgprCount;               ///< Required: Number of vector registers required by each work-item.
    uint32      m_maxFlatWorkgroupSize;    ///< Required: Max flat workgroup size the kernel supports, in work-items.
    uint32      m_sgprSpillCount;          ///< Optional: Number of stores from a scalar register to the spill location.
    uint32      m_vgprSpillCount;          ///< Optional: Number of stores from a vector register to the spill location.
    Kind        m_kind;                    ///< Optional: What kind of kernel this is, Normal if not present.
    uint32      m_uniformWorkgroupSize;    ///< Optional: If kernel requires that grid dim is multiple of workgroup size
    bool        m_usesDynamicStack;        ///< Optional: The generated machine code is using a dynamically sized stack.
    bool        m_workgroupProcessorMode;  ///< Optional: Is this WGP mode or CU mode

    PAL_DISALLOW_COPY_AND_ASSIGN(CodeObjectMetadata);
};

// This is templated so we need to define it in the header.
template <typename Allocator>
CodeObjectMetadata::CodeObjectMetadata(Allocator* const pAllocator)
    :
    m_allocator(pAllocator),
    m_codeVersionMajor(0),
    m_codeVersionMinor(0),
    m_pName(nullptr),
    m_pSymbol(nullptr),
    m_pLanguage(nullptr),
    m_languageVersion{},
    m_pArgs(nullptr),
    m_numArgs(0),
    m_reqdWorkgroupSize{},
    m_workgroupSizeHint{},
    m_pVecTypeHint(nullptr),
    m_pDeviceEnqueueSymbol(nullptr),
    m_kernargSegmentSize(0),
    m_groupSegmentFixedSize(0),
    m_privateSegmentFixedSize(0),
    m_kernargSegmentAlign(0),
    m_wavefrontSize(0),
    m_sgprCount(0),
    m_vgprCount(0),
    m_maxFlatWorkgroupSize(0),
    m_sgprSpillCount(0),
    m_vgprSpillCount(0),
    m_kind(Kind::Normal),
    m_uniformWorkgroupSize(0),
    m_usesDynamicStack(false),
    m_workgroupProcessorMode(false)
{
}

} // HsaAbi
} // Util
