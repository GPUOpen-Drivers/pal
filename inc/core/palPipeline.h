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
/**
 ***********************************************************************************************************************
 * @file  palPipeline.h
 * @brief Defines the Platform Abstraction Library (PAL) IPipeline interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palGpuMemoryBindable.h"
#include "palImage.h"

namespace Util
{
namespace Abi
{
union ApiHwShaderMapping;
enum class HardwareStage : uint32;
}
}

namespace Pal
{

// Forward declarations.
struct GpuMemSubAllocInfo;
enum class PrimitiveTopology : uint32;

/// ShaderHash represents a 128-bit shader hash.
struct ShaderHash
{
    uint64 lower;   ///< Lower 64-bits of hash
    uint64 upper;   ///< Upper 64-bits of hash
};

/// PipelineHash represents a concatenated pair of 64-bit hashes.
struct PipelineHash
{
    uint64 stable;   ///< Lower 64-bits of hash.  "Stable" portion, suitable for e.g. shader replacement use cases.
    uint64 unique;   ///< Upper 64-bits of hash.  "Unique" portion, suitable for e.g. pipeline cache use cases.
};

///@{
/// Determines whether two ShaderHashes or PipelineHashes are equal.
///
/// @param  [in]    hash1    The first 128-bit shader hash or pipeline hash
/// @param  [in]    hash2    The second 128-bit shader hash or pipeline hash
///
/// @returns True if the hashes are equal.
PAL_INLINE bool ShaderHashesEqual(const ShaderHash hash1, const ShaderHash hash2)
    { return ((hash1.lower  == hash2.lower)  && (hash1.upper  == hash2.upper)); }
PAL_INLINE bool PipelineHashesEqual(const PipelineHash hash1, const PipelineHash hash2)
    { return ((hash1.stable == hash2.stable) && (hash1.unique == hash2.unique)); }
///@}

///@{
/// Determines whether the given ShaderHash or PipelineHash is non-zero.
///
/// @param  [in]    hash    A 128-bit shader hash or pipeline hash
///
/// @returns True if the hash is non-zero.
PAL_INLINE bool ShaderHashIsNonzero(const ShaderHash hash)     { return ((hash.upper  | hash.lower)  != 0); }
PAL_INLINE bool PipelineHashIsNonzero(const PipelineHash hash) { return ((hash.stable | hash.unique) != 0); }
///@}

/// Specifies a shader type (i.e., what stage of the pipeline this shader was written for).
enum class ShaderType : uint32
{
    Compute = 0,
    Vertex,
    Hull,
    Domain,
    Geometry,
    Pixel,
};

/// Number of shader program types supported by PAL.
constexpr uint32 NumShaderTypes =
    (1u + static_cast<uint32>(ShaderType::Pixel) - static_cast<uint32>(ShaderType::Compute));

/// Maximum number of viewports.
constexpr uint32 MaxViewports = 16;

/// Maximum number of supported stream-output declaration entries by any PAL device.
constexpr uint32 MaxStreamOutEntries = 512;

/// Specifies a general primitive category without differentiating between a strip or list and without specifying
/// whether a the primitive will include adjacency info or not.
enum class PrimitiveType : uint32
{
    Point    = 0x0,
    Line     = 0x1,
    Triangle = 0x2,
    Rect     = 0x3,
    Quad     = 0x4,
    Patch    = 0x5
};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 409
/// If next available quad falls outside tile aligned region of size defined by this enumeration the SC will
/// force end of vector in the SC to shader wavefront
enum class WaveBreakSize : uint32
{
    None   = 0x0,
    _8x8   = 0x1,
    _16x16 = 0x2,
    _32x32 = 0x3,
};
#endif

/// Specifies the target range of Z values after viewport transform.
enum class DepthRange : uint32
{
    ZeroToOne        = 0x0,
    NegativeOneToOne = 0x1
};

/// Specifies whether the v/t texture coordinates of a point sprite map 0 to 1 from top to bottom or bottom to top.
enum class PointOrigin : uint32
{
    UpperLeft = 0x0,
    LowerLeft = 0x1
};

/// Specifies primitive's shade mode.
enum class ShadeMode : uint32
{
    Gouraud = 0x0,      ///< Gouraud shading mode, pixel shader input is interpolation of vertex
    Flat    = 0x1       ///< Flat shading mode, pixel shader input from provoking vertex
};

/// Defines a logical operation applied between the color coming from the pixel shader and the current value in the
/// target image.
enum class LogicOp : uint32
{
    Copy         = 0x0,
    Clear        = 0x1,
    And          = 0x2,
    AndReverse   = 0x3,
    AndInverted  = 0x4,
    Noop         = 0x5,
    Xor          = 0x6,
    Or           = 0x7,
    Nor          = 0x8,
    Equiv        = 0x9,
    Invert       = 0xA,
    OrReverse    = 0xB,
    CopyInverted = 0xC,
    OrInverted   = 0xD,
    Nand         = 0xE,
    Set          = 0xF
};

/// Specifies whether to override binning setting for pipeline. Enum value of Default follows the PBB global setting.
/// Enable or Disable value overrides PBB global setting for the pipeline and sets binning accordingly.
enum class BinningOverride : uint32
{
    Default = 0x0,
    Disable = 0x1,
    Enable  = 0x2,
    Count
};

/// Common flags controlling creation of both compute and graphics pipeline.
union PipelineCreateFlags
{
    struct
    {
        uint32 clientInternal :  1; ///< Internal pipeline not created by the application.
        uint32 reserved       : 31; ///< Reserved for future use.
    };
    uint32 u32All;                  ///< Flags packed as 32-bit uint.
};

/// Constant definining the max number of view instance count that is supported.
constexpr uint32 MaxViewInstanceCount = 6;

/// Specifies graphic pipeline view instancing state.
struct ViewInstancingDescriptor
{
    uint32         viewInstanceCount;                           ///< The view instance count of the graphic pipeline
    uint32         viewId[MaxViewInstanceCount];                ///< The view instance ids.
    uint32         renderTargetArrayIdx[MaxViewInstanceCount];  ///< The instance render target array index, can be
                                                                ///  used in hardware accelerated stereo rendering.
    uint16         viewportArrayIdx[MaxViewInstanceCount];      ///< The instance viewport array index, can be
                                                                ///  used in hardware accelerated stereo rendering.
    bool           enableMasking;                               ///< Indicate whether instance masking is enabled.
};

/// Specifies properties about an indirect function belonging to a compute @ref IPipelne object.  Part of the input
/// structure to IDevice::CreateComputePipeline().
struct ComputePipelineIndirectFuncInfo
{
    const char*  pSymbolName; ///< ELF Symbol name for the associated function.  Must not be null.
    gpusize      gpuVirtAddr; ///< [out] GPU virtual address of the function.  This is compute by PAL during
                              ///  pipeline creation.
};

/// Specifies properties for creation of a compute @ref IPipeline object.  Input structure to
/// IDevice::CreateComputePipeline().
struct ComputePipelineCreateInfo
{
    PipelineCreateFlags flags;                   ///< Flags controlling pipeline creation.

    const void*         pPipelineBinary;         ///< Pointer to Pipeline ELF binary implementing the Pipeline ABI
                                                 ///  interface. The Pipeline ELF contains pre-compiled shaders,
                                                 ///  register values, and additional metadata.
    size_t              pipelineBinarySize;      ///< Size of Pipeline ELF binary in bytes.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 440
    /// Optional.  Specifies a set of indirect functions for PAL to compute virtual addresses for during pipeline
    /// creation.  These GPU addresses can then be passed as shader arguments for a later dispatch operation to
    /// allow the pipeline's shaders to jump to that function.  Similar to a function pointer on the GPU.
    ComputePipelineIndirectFuncInfo*  pIndirectFuncList;
    uint32                            indirectFuncCount; ///< Number of entries in the pIndirectFuncList array.  Must
                                                         ///  be zero if pIndirectFuncList is null.
#endif
};

/// Specifies properties for creation of a graphics @ref IPipeline object.  Input structure to
/// IDevice::CreateGraphicsPipeline().
struct GraphicsPipelineCreateInfo
{
    PipelineCreateFlags flags;                 ///< Flags controlling pipeline creation.

    const void*         pPipelineBinary;         ///< Pointer to Pipeline ELF binary implementing the Pipeline ABI
                                                 ///  interface. The Pipeline ELF contains pre-compiled shaders,
                                                 ///  register values, and additional metadata.
    size_t              pipelineBinarySize;      ///< Size of Pipeline ELF binary in bytes.
    bool                useLateAllocVsLimit;   ///< If set, use the specified lateAllocVsLimit instead of PAL internally
                                               ///  determining the limit.
    uint32              lateAllocVsLimit;      ///< The number of VS waves that can be in flight without having param
                                               ///  cache and position buffer space. If useLateAllocVsLimit flag is set,
                                               ///  PAL will use this limit instead of the PAL-specified limit.
    struct
    {
        struct
        {
            PrimitiveType primitiveType;       ///< Basic primitive category: points, line, triangles, patches.
            uint32        patchControlPoints;  ///< Number of control points per patch.  Only required if primitiveType
                                               ///  is PrimitiveType::Patch.
            bool          adjacency;           ///< Primitive includes adjacency info.
        } topologyInfo;                        ///< Various information about the primitive topology that will be used
                                               ///  with this pipeline.  All of this info must be consistent with the
                                               ///  full topology specified by ICmdBuffer::SetPrimitiveTopology() when
                                               ///  drawing with this pipeline bound.
    } iaState;                                 ///< Input assembler state.

    struct
    {
        PointOrigin     pointCoordOrigin;          ///< Controls texture coordinate orientation for point sprites.
        bool            expandLineWidth;           ///< If true, line primitives will have their width expanded by 1/cos(a)
                                                   ///  where a is the minimum angle from horizontal or vertical.
                                                   ///  This can be used in conjunction with PS patching for a client to
                                                   ///  implement line antialiasing.
        ShadeMode       shadeMode;                 ///< Specifies shading mode, Gouraud or Flat
        bool            rasterizeLastLinePixel;    ///< Specifies whether to draw last pixel in a line.
        bool            outOfOrderPrimsEnable;     ///< Enables out-of-order primitive rasterization.  PAL silently
                                                   ///  ignores this if it is unsupported in hardware.
        bool            perpLineEndCapsEnable;     ///< Forces the use of perpendicular line end caps as opposed to
                                                   ///  axis-aligned line end caps during line rasterization.
        BinningOverride binningOverride;           ///< Binning setting for this pipeline.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 381
        bool            depthClampDisable;         ///< Disable depth clamping to viewport min/max depth
#else
        bool            depthClampEnable;          ///< Disable depth clamping to viewport min/max depth
#endif
    } rsState;             ///< Rasterizer state.

    struct
    {
        bool    alphaToCoverageEnable;          ///< Enable alpha to coverage.
        bool    dualSourceBlendEnable;          ///< Blend state bound at draw time will use a dual source blend mode.
        LogicOp logicOp;                        ///< Logic operation to perform.

        struct
        {
            SwizzledFormat swizzledFormat;      ///< Color target format and channel swizzle. Set the format to invalid
                                                ///  if no color target will be bound at this slot.
            uint8          channelWriteMask;    ///< Color target write mask.  Bit 0 controls the red channel, bit 1 is
                                                ///  green, bit 2 is blue, and bit 3 is alpha.
        } target[MaxColorTargets];              ///< Per-MRT color target info.
    } cbState;                                  ///< Color target state.

    ViewInstancingDescriptor viewInstancingDesc;    ///< Descriptor describes view instancing state
                                                    ///  of the graphics pipeline

};

/// The graphic pipeline view instancing information. This is used to determine if hardware accelerated stereo rendering
/// can be enabled for a graphic pipeline.
struct GraphicPipelineViewInstancingInfo
{
    union
    {
        struct
        {
            uint32 shaderUseViewId                  : 1;   ///< If any shader in pipeline uses view id.
            uint32 gsExportRendertargetArrayIndex   : 1;   ///< If gs exports render target array index,
                                                           ///  must be 0 if there is no gs.
            uint32 gsExportViewportArrayIndex       : 1;   ///< If gs exports viewport array index,
                                                           ///  must be 0 if there is no gs.
            uint32 reserved                         : 29;  ///< Reserved for future use.
        };
        uint32 apiShaderFlags;
    };

    const ViewInstancingDescriptor* pViewInstancingDesc;    ///< View Instancing descriptor
};

/// Reports properties of a compiled pipeline.  This includes hashes for the pipeline and shaders that the client can
/// use to correlate PAL pipeline/shader dumps with corresponding API-level pipelines/shaders.
struct PipelineInfo
{
    PipelineHash internalPipelineHash;  ///< 128-bit identifier extracted from this pipeline's ELF binary, composed of
                                        ///  the state the compiler decided was appropriate to identify the compiled
                                        ///  shaders.  The lower 64 bits are "stable"; the upper 64 bits are "unique".
    uint64       palRuntimeHash;        ///< Unique 64-bit identifier for the PAL pipeline, composed of compiler
                                        ///  information and PAL-specific runtime-adjacent information. Mapping of
                                        ///  PAL runtime hash to internal pipeline hash is many-to-one.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 460
    uint64 pipelineHash;      ///< Unique 64-bit identifier for the PAL pipeline, composed of compiler information and
                              ///  PAL-specific runtime-adjacent information.
    uint64 compilerHash;      ///< 64-bit identifier extracted from this pipeline's ELF binary, composed of the state
                              ///  the compiler decided was appropriate to identify the compiled shaders.  Pipelines
                              ///  can have identical compiler hashes but different pipeline hashes.  Note that this
                              ///  is not computed by taking a hash of the binary blob data.
#endif

    struct
    {
        ShaderHash hash;      ///< Unique 128-bit identifier for this shader.  0 indicates there is no shader bound for
                              ///  the corresponding shader stage.
    } shader[NumShaderTypes]; ///< Array of per-shader pipeline properties.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 387
    struct
    {
        union
        {
            struct
            {
                uint32 perSampleShading : 1;    ///< Shader instructions want per-sample execution.
                uint32 reserved         : 31;   ///< Reserved for future use.
            };
            uint32 u32All;                      ///< All flags combined as a single uint32.
        } flags;
    } ps;                                       ///< Pixel shader properties.
#endif
};

/// Used to represent API level shader stage.
enum ShaderStageFlagBits : uint32
{
    ApiShaderStageCompute  = 0x00000001,
    ApiShaderStageVertex   = 0x00000002,
    ApiShaderStageHull     = 0x00000004,
    ApiShaderStageDomain   = 0x00000008,
    ApiShaderStageGeometry = 0x00000010,
    ApiShaderStagePixel    = 0x00000020
};

/// Common shader pre and post compilation stats.
struct CommonShaderStats
{
    uint32  numUsedVgprs;              ///< Number of VGPRs used by this shader
    uint32  numUsedSgprs;              ///< Number of SGPRs used by this shader
    uint32  ldsSizePerThreadGroup;     ///< LDS size per thread group in bytes.
    size_t  ldsUsageSizeInBytes;       ///< LDS usage by this shader.
    size_t  scratchMemUsageInBytes;    ///< Amount of scratch mem used by this shader.
    gpusize gpuVirtAddress;            ///< Gpu mem address of shader ISA code.
};

/// Reports shader stats. Multiple bits set in the shader stage mask indicates that multiple shaders have been combined
/// due to HW support. The same information will be repeated for both the constituent shaders in this case.
struct ShaderStats
{
    uint32             shaderStageMask;        ///< Indicates the stages of the pipeline this shader is
                                               /// used for. If multiple bits are set, it implies
                                               /// shaders were merged.
    CommonShaderStats  common;                 ///< The shader compilation parameters for this shader.
    /// Maximum number of VGPRs the compiler was allowed to use for this shader.  This limit will be the minimum
    /// of any architectural restriction and any client-requested limit intended to increase the number of waves in
    /// flight.
    uint32             numAvailableVgprs;
    /// Maximum number of SGPRs the compiler was allowed to use for this shader.  This limit will be the minimum
    /// of any architectural restriction and any client-requested limit intended to increase the number of waves in
    /// flight.
    uint32             numAvailableSgprs;
    size_t             isaSizeInBytes;         ///< Size of the shader ISA disassembly for this shader.
    ShaderHash         palShaderHash;          ///< Internal hash of the shader compilation data used by PAL.

    union
    {
        struct
        {
            uint32 writesUAV   : 1;     ///< This shader performs writes to UAVs.
            uint32 writesDepth : 1;     ///< Indicates explicit depth writes performed by the shader stage.
            uint32 streamOut   : 1;     ///< The shader performs stream out of shader generated data.
            uint32 reserved    : 29;    ///< Reserved for future use.
        };
        uint32 u32All;                  ///< All flags combined as a single uint32.

    } shaderOperations;                       ///< Flags depicting shader operations.

    struct
    {
        uint32 numThreadsPerGroupX;           ///< Number of compute threads per thread group in X dimension.
        uint32 numThreadsPerGroupY;           ///< Number of compute threads per thread group in Y dimension.
        uint32 numThreadsPerGroupZ;           ///< Number of compute threads per thread group in Z dimension.
    } cs;                                     ///< Parameters specific to compute shader only.

    union
    {
        struct
        {
            uint8 copyShaderPresent : 1;      ///< Indicates that the copy shader data is valid.
            uint8 reserved          : 7;      ///< Reserved for future use.
        };
        uint8 u8All;                          ///< All the flags as a single value.
    } flags;                                  ///< Flags related to this shader data.

    CommonShaderStats  copyShader;            ///< This data is valid only when the copyShaderPresent flag above is set.
};

/**
 ***********************************************************************************************************************
 * @interface IPipeline
 * @brief     Monolithic object containing all shaders and a large amount of "shader adjacent" state.  Separate concrete
 *            implementations will support compute or graphics pipelines.
 *
 * @see IDevice::CreateComputePipeline()
 * @see IDevice::CreateGraphicsPipeline()
 * @see IDevice::LoadPipeline()
 ***********************************************************************************************************************
 */

class IPipeline : public IDestroyable
{
public:
    /// Returns PAL-computed properties of this pipeline and its corresponding shaders.
    ///
    /// @returns Property structure describing this pipeline.
    virtual const PipelineInfo& GetInfo() const = 0;

    /// Returns a list of GPU memory allocations used by this pipeline.
    ///
    /// @param [in,out] pNumEntries    Input value specifies the available size in pAllocInfoList; output value
    ///                                reports the number of GPU memory allocations.
    /// @param [out]    pAllocInfoList If pAllocInfoList=nullptr, then pNumEntries is ignored on input.  On output it
    ///                                will reflect the number of allocations that make up this pipeline.  If
    ///                                pAllocInfoList!=nullptr, then on input pNumEntries is assumed to be the number
    ///                                of entries in the pAllocInfoList array.  On output, pNumEntries reflects the
    ///                                number of entries in pAllocInfoList that are valid.
    /// @returns Success if the allocation info was successfully written to the buffer.
    ///          + ErrorInvalidValue if the caller provides a buffer size that is different from the size needed.
    ///          + ErrorInvalidPointer if pNumEntries is nullptr.
    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const = 0;

    /// Obtains the binary code object for this pipeline.
    ///
    /// @param [in, out] pSize  Represents the size of the shader ISA code.
    ///
    /// @param [out] pBuffer    If non-null, the pipeline ELF is written in the buffer. If null, the size required
    ///                         for the pipeline ELF is given out in the location pSize.
    ///
    /// @returns Success if the pipeline binary was fetched successfully.
    ///          +ErrorUnavailable if the pipeline binary was not fetched successfully.
    virtual Result GetPipelineElf(
        uint32*  pSize,
        void*    pBuffer) const = 0;

    /// Obtains the shader pre and post compilation stats/params for the specified shader stage.
    ///
    /// @param [in]  shaderType The shader stage for which the stats are requested.
    ///
    /// @param [out] pShaderStats Pointer to the ShaderStats structure which will be filled with the shader stats for
    ///                           the shader stage mentioned in shaderType. This cannot be nullptr.
    /// @param [in]  getDisassemblySize If set to true performs disassembly on the shader binary code and reports the
    ///                                 size of the disassembly string in ShaderStats::isaSizeInBytes. Else reports 0.
    /// @returns Success if the stats were successfully obtained for this shader, including the shader disassembly size.
    ///          +ErrorUnavailable if a wrong shader stage for this pipeline was specified, or if some internal error
    ///                           occured.
    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const = 0;

    /// Obtains the compiled shader ISA code for the shader stage specified.
    ///
    /// @param [in]  shaderType The shader stage for which the shader cache entry is requested.
    ///
    /// @param [in, out] pSize  Represents the size of the shader ISA code.
    ///
    /// @param [out] pBuffer    If non-null, the shader ISA code is written in the buffer. If null, the size required
    ///                         for the shader ISA is given out in the location pSize.
    ///
    /// @returns Success if the shader ISA code was fetched successfully.
    ///          +ErrorUnavailable if the shader ISA code was not fetched successfully.
    virtual Result GetShaderCode(
        ShaderType shaderType,
        size_t*    pSize,
        void*      pBuffer) const = 0;

    /// Obtains the generated performance data for the shader stage specified.
    ///
    /// @param [in]      hardwareStage  The hardware stage of the shader which the performance data is requested.
    /// @param [in, out] pSize          Represents the size of the performance data.
    /// @param [out]     pBuffer        If non-null, the performance data is written in the buffer. If null, the size
    ///                                 required for the performance data is given out in the location pSize.
    ///
    /// @returns Success if the performance data was fetched successfully.
    ///          +ErrorUnavailable if the performance data was not fetched successfully.
    virtual Result GetPerformanceData(
        Util::Abi::HardwareStage hardwareStage,
        size_t*                  pSize,
        void*                    pBuffer) = 0;

    /// Returns the API shader type to hardware stage mapping for the pipeline.
    ///
    /// @returns The appropriate mapping for this pipeline.
    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const { return m_pClientData; }

    /// Sets the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @param  [in]    pClientData     A pointer to arbitrary client data.
    PAL_INLINE void SetClientData(
        void* pClientData)
    {
        m_pClientData = pClientData;
    }

protected:
    /// @internal Constructor. Prevent use of new operator on this interface. Client must create objects by explicitly
    /// called the proper create method.
    IPipeline() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Client must destroy objects by
    /// explicitly calling IDestroyable::Destroy() and is responsible for freeing the system memory allocated for the
    /// object on their own.
    virtual ~IPipeline() { }

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

} // Pal
