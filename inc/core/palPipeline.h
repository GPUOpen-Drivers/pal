/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palShader.h"

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
class IShader;
class IShaderCache;
struct GpuMemSubAllocInfo;
enum class PrimitiveTopology : uint32;

/// Maximum number of supported stream-output declaration entries by any PAL device.
constexpr uint32 MaxStreamOutEntries = 512;

/// Specifies the function of a particular node in a shader's resource mapping graph.
///
/// @see ResourceMappingNode
/// @ingroup ResourceBinding
enum class ResourceMappingNodeType : uint32
{
    Resource,              ///< SRD defining a read-only resource view (t#).
    Uav,                   ///< SRD defining an unordered access view (u#).
    ConstBuffer,           ///< SRD defining a constant buffer (cb#).
    Sampler,               ///< SRD defining a sampler (s#).
    DefaultVaPtr,          ///< Pointer to a separate array of resource mapping nodes (for building descriptor table
                           ///  hierarchies).  The GPU memory allocation's address comes from the @ref VaRange::Default
                           ///  virtual address space, and therefore takes up 64-bits (2 user-data entries).
    DescriptorTableVaPtr,  ///< Pointer to a separate array of resource mapping nodes (for building descriptor table
                           ///  hierarchies).  The GPU memory allocation's address comes from the
                           ///  @ref VaRange::DescriptorTable virtual address space, giving it an assumed top 32 address
                           ///  bits.  The client should only specify the low 32 address bits in the user data entry or
                           ///  GPU memory corresponding to such a node.  This node type exists in order to make full
                           ///  use of our user-data hardware registers for clients that have enough information to put
                           ///  descriptor tables in GPU memory with a special VA range.
    IndirectUserDataVaPtr, ///< Pointer to a separate array of resource mapping nodes (for building descriptor table
                           ///  hierarchies).  This differs from @ref ResourceMappingNode::DescriptorTableVaPtr in that
                           ///  PAL is responsible for managing the video memory which contains the table data.  This
                           ///  table data is owned and managed by the @ref ICmdBuffer object which issues Dispatches or
                           ///  Draws with this pipeline.  The client should not call @ref ICmdBuffer::CmdSetUserData
                           ///  for the user-data entries associated with this node, since the GPU address is managed
                           ///  completely by PAL.
    StreamOutTableVaPtr,   ///< Pointer to a table of @ref MaxStreamOutTargets buffer SRD's which describe the target
                           ///  buffer's for a pipeline's stream-output stage.  The client should not call @ref
                           ///  ICmdBuffer::CmdSetUserData for the user-data entries associated with this node, since
                           ///  the stream-out table contents are managed by PAL.  Rather, this node type is intended
                           ///  for the purpose of giving clients flexibility in where the stream out table address is
                           ///  stored relative to other user-data entries.  This node type only has meaning for the
                           ///  shader in a pipeline with stream-output enabled.  For any other shader, it is ignored.
                           ///  This node type may not be used inside a nested descriptor table: it is only permitted
                           ///  inside the pUserDataNodes[] array in @ref PipelineShaderInfo.
    InlineConst,           ///< This node contains between 1 and 4 32-bit inline constants, to be fetched from the
                           ///  shader in a specified constant buffer slot (e.g., cb3[4]).
    InlineSrvConst,        ///< This node contains between 1 and 4 32-bit inline constants, to be fetched from the
                           ///  shader in a specified srv buffer (e.g., srv_raw_load(3) dst, 4, 0).
    Count,                 ///< Number of resource mapping node types.
};

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

/// If next available quad falls outside tile aligned region of size defined by this enumeration the SC will
/// force end of vector in the SC to shader wavefront
enum class WaveBreakSize : uint32
{
    None   = 0x0,
    _8x8   = 0x1,
    _16x16 = 0x2,
    _32x32 = 0x3,
};

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
        uint32 disableOptimization  :  1;  ///< Disables pipeline link-time optimizations.  Should only be used for
                                           ///  debugging.
        uint32 disableUserDataRemap :  1;  ///< Disables user-data entry remapping and compaction.  Should only be used
                                           ///  for debugging.
        uint32 sm5_1ResourceBinding :  1;  ///< If set, this pipeline will be compiled to interpret the IL decl and
                                           ///  load/sample instructions assuming a shader model 5.1 binding mode.  This
                                           ///  means that the register t5 will be interpreted as a logical range of
                                           ///  textures whose bounds are explicitly declared by the shader and resource
                                           ///  mapping nodes. Each texture in that range would be accessible as t5[13]
                                           ///  or t5[r3.x], etc.
                                           ///  If clear, this pipeline will use the pre SM5.1 binding mode, where each
                                           ///  resource register (e.g., t5) is treated as a single, unique resource.
                                           ///  In addition, the 'stride' and 'startIndex' fields of
                                           ///  ResourceMappindNode's 'srdRange' structure are ignored.

        uint32 disableOptimizationC0  :  1;  ///< Disable SC optimization option SCOption_C0
        uint32 disableOptimizationC1  :  1;  ///< Disable SC optimization option SCOption_C1
        uint32 disableOptimizationC2  :  1;  ///< Disable SC optimization option SCOption_C2
        uint32 disableOptimizationC3  :  1;  ///< Disable SC optimization option SCOption_C3
        uint32 disableOptimizationC4  :  1;  ///< Disable SC optimization option SCOption_C4
#if (PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 309)
        uint32 clientInternal         :  1;  ///< Internal pipeline not created by the application.
        uint32 reserved               : 23;  ///< Reserved for future use.
#else
        uint32 reserved               : 24;  ///< Reserved for future use.
#endif
    };
    uint32 u32All;                         ///< Flags packed as 32-bit uint.
};

/// Constant defining an unbounded range of indexable SRD's in a descriptor table.  Unbounded ranges of SRD's must be
/// the last item in whatever table they appear in.
constexpr uint32 UnboundedSrdRange = 0xFFFFFFFF;

/// Defines one node in a graph defining how the user data bound in a command buffer at draw/dispatch time maps to
/// resources referenced by a shader (t#, u#, etc.).
///
/// @ingroup ResourceBinding
struct ResourceMappingNode
{
    /// Specifies what kind of node this is.  There are four basic categories:
    ///
    /// 1. _Shader resource descriptor (SRD)_: This node contains a hardware SRD (or indexable range of SRD's)
    ///    describing a t#, u#, cb#, or s# referenced by this shader.  The resource ID is specified in the _srdRange_
    ///    structure in the union below.
    /// 2. _Descriptor table pointer_: This node contains a GPU address pointing to another table of nodes.  Details on
    ///    the layout of this table should be specified in the _tablePtr_ structure in the union below.
    /// 3. _Inline constants_: This node contains 1 to 4 32-bit values used directly as a cb#[#] value in the shader
    ///    without a separate constant buffer SRD fetch.
    ResourceMappingNodeType type;

    /// Specifies the size of this node in DWORD's.
    ///
    /// If the client packs different node types into a heterogeneous, indexable table, the size should be set to the
    /// table's stride.
    ///
    /// If this node represents an indexable range of SRD's with an unbounded size, this should be programmed to
    /// 'UnboundedSrdRange'.  It must also be the last element in its descriptor table.
    ///
    /// Minimum sizes of each node type:
    ///
    /// ResourceMappingNodeType | Minimum Size (could be padded to a uniform stride in a heterogeneous table)
    /// ----------------------- | ------------------------------------------------------------------------------------
    /// Resource                | Could be an image, buffer, or fmask view SRD.  See srdSizes in DeviceProperties.
    /// Uav                     | Same as Resource.
    /// ConstBuffer             | Must be a buffer SRD: GetBufferSrdSize() / sizeof(uint32).
    /// Sampler                 | Sampler SRD: GetSamplerSrdSize() / sizeof(uint32).
    /// DefaultVaPtr            | 64-bit virtual address: 2 dwords.
    /// DescriptorTableVaPtr    | 32-bit virtual address low portion: 1 dword.
    /// IndirectUserDataVaPtr   | 32-bit virtual address low portion: 1 dword.
    /// StreamOutTableVaPtr     | 32-bit virtual address low portion: 1 dword.
    /// InlineConst             | From 1 to 4 dwords (controls whether x, xy, xyz, or xyzw channels are available).
    ///
    /// NOTE: if the client enables the sm5_1ResourceBinding flag in PipelineCreateFlags, for Resource/Uav/ConstBuffer
    /// where 4-DWORD buffer SRD is used, 2-DWORD GPU virtual address could be used instead to save user-data entries.
    /// In that case, the client doesn't need to call CreateBufferViewSrds to build SRD but just pass down 2-DWORD GPU
    /// virtual adddress (top 16-bit are assumed to be 0 and dropped) to root level user-data entries through CmdSetU-
    /// serData (including spill table), SC is then responsible for expanding to full 4-DWORD SRD looking like below:
    ///     STRIDE      = Constant buffer: 16 / Raw buffer: 0 / Sturctured buffer: use value declared in shader
    ///     NUM_RECORDS = 16384 DWORDS
    ///     DST_SEL_X   = SQ_SEl_X (4)
    ///     DST_SEL_Y   = SQ_SEl_Y (5)
    ///     DST_SEL_Z   = SQ_SEl_Z (6)
    ///     DST_SEL_W   = SQ_SEl_W (7)
    ///     NUM_FORMAT  = BUF_NUM_FORMAT_UINT (4)
    ///     DATA_FORMAT = BUF_DATA_FORMAT_32 (4)
    ///     TYPE        = SQ_RSRC_BUF (0)
    uint32 sizeInDwords;
    uint32 offsetInDwords;     ///< Offset of this node (from the beginning of the resource mapping table) in DWORD's.

    union
    {
        struct
        {
            uint32 id;         ///< Logcial id for this indexable range of SRD's.  (E.g., for an indexable range of
                               ///  resources t5[7:10], this would be programmed to 5.)  Note, if this node doesn't
                               ///  represent an indexable range, this would be the normal id of the texture (e.g.,
                               ///  t5 in Mantle).
            uint32 startIndex; ///< Starting index for this indexable range of SRD's.  (E.g., for an indexable range
                               ///  of resources t5[7:10], this would be programmed to 7.)
            uint32 stride;     ///< Size of each SRD in the indexable range, in DWORDs.
        } srdRange;            ///< Information for SRD nodes (Resource, Uav, ConstBuffer, Sampler).

        struct
        {
            uint32                     nodeCount;  ///< Number of entries in the pNext array.
            const ResourceMappingNode* pNext;      ///< Array of node structures describing the next hierarchical level
                                                   ///  of mapping.  When read by the shader, this node will contain a
                                                   ///  GPU virtual address.
            uint16                     indirectId; ///< For IndirectUserDataVaPtr, indicates which indirect user-data
                                                   ///  table will be read from.  Ignored for all other node types.
        } tablePtr;                                ///< Information for hierarchical nodes (DefaultVaPtr,
                                                   ///  DescriptorTableVaPtr, IndirectUserDataVaPtr).  The type of node
                                                   ///  determines whether this node takes up one or two DWORDs.

        struct
        {
            uint32 id;           ///< Logical id for this indexable range of constant buffer.  (E.g., for an indexable
                                 ///  range of constant buffers cb5[0:2], this would be programmed to 5.)  Note, if
                                 ///  this node doesn't represent an indexable range, this would be the normal id of
                                 ///  the constant buffer (e.g., cb5 in DX11).
            uint32 slot;         ///< This field has different meanings, depending on whether or not the
                                 ///  sm5_1ResourceBinding flag is set:
                                 ///  If the flag is set, then this indicates which index in the indexable range of
                                 ///  constant buffers this inline constant maps to. The entire contents of the buffer
                                 ///  must be present in the mapping node and sizeInDwords must be a multiple of 4.
                                 ///  Otherwise, this is the first slot (i.e., vec4) in the constant buffer which this
                                 ///  maps to.  Note that sizeInDwords can be any number and determines how many vec4's
                                 ///  and which channels can be read in the shader: 1 = x, 2 = xy, 3 = xyz, 4 = xyzw.
        } inlineConst; ///< Information for inline constant nodes (InlineConst).
    };
};

/// Specifies data for link-time constant buffers.
struct LinkConstBuffer
{
    uint32      bufferId;     ///< Which cb# this data is for.
    size_t      bufferSize;   ///< Size of constant buffer.
    const void* pBufferData;  ///< Constant buffer data.
};

/// Specifies data for static descriptor values
struct DescriptorRangeValue
{
     ResourceMappingNodeType type;        /// Specifies what kind of node this is, It can be one of Resource, Uav,
                                          /// ConstBuffer and Sampler. But currently SC only support Sampler
     uint32                  srdRangeId;  /// Logcial id for this indexable range of SRD's
     uint32                  arraySize;   /// Size of this indexable SRD range
     const uint32*           pValue;      /// Static SRDs
};

/// Specifies a shader and how its resources should be mapped to user data entries.
struct PipelineShaderInfo
{
    IShader*               pShader;              /// Shader object.

    uint32                 linkConstBufferCount; ///< Number of link-time constant buffers.
    const LinkConstBuffer* pLinkConstBufferInfo; ///< Data for each link-time constant buffer.

    uint32                 numDescriptorRangeValues; ///< Number of valid entries in the pDescriptorRangeValues array.
    DescriptorRangeValue*  pDescriptorRangeValues;   ///< An array of static descriptors

    uint32                 numUserDataNodes;     ///< Number of valid entries in the pUserDataNodes array.

    /// Provides the root-level mapping of descriptors in user-data entries (physical registers or GPU memory) to
    /// resources referenced in this shader.
    ///
    /// Entries in this array describe how user data set with ICmdBuffer::SetUserData should be interpreted by this
    /// shader.  The user data may contain SRD's directly, pointer to tables of SRD's in GPU memory, or even inline
    /// constants.  This forms the base of a graph of ResourceMappingNodes which allows arbitrarily deep hierarchies
    /// of descriptor tables.
    ///
    /// Normally, this user data will correspond to the GPU's user data registers.  However, PAL needs some user data
    /// registers for internal use, so some user data may spill to internal GPU memory managed by PAL.  The fastUserData
    /// field in DeviceProperties gives an indication of how many user-data registers are available for client use.
    /// Early entries in this array will be assigned to hardware registers first.
    ///
    /// @note The index into this array is not equivalent to the index that should be passed to ICmdBuffer::SetUserData
    ///       when setting the corresponding data values.  This array is packed by mapping _node_, while setting user
    ///       data is packed by user data _entry_ (i.e., 32-bit value).  For example, if nodex 0 and 1 are 4-DWORD
    ///       buffer SRD's, then node 1 corresponds to entry 4.
    const ResourceMappingNode* pUserDataNodes;

    union
    {
        struct
        {
            uint32   trapPresent   :  1;  ///< Indicates a trap handler will be present when this pipeline is executed,
                                          ///  and any trap conditions encountered in this shader should call the trap
                                          ///  handler. This could include an arithmetic exception, an explicit trap
                                          ///  request from the host, or a trap after every instruction when in debug
                                          ///  mode.
            uint32   debugMode     :  1;  ///< When set, this shader should cause the trap handler to be executed after
                                          ///  every instruction.  Only valid if trapPresent is set.
            uint32   innerCoverage :  1;  ///< Related to conservative rasterization.  Must be zero if conservative
                                          ///  rasterization is disabled.
            uint32   vFaceIsFloat  :  1;  ///< Indicates whether vFace register is float point or unsigned int. vFace
                                          ///  register should be float point for SM3.0 and unsigned int for SM4.0+.
            uint32   shadowFmask   :  1;  ///< All FMask descriptors will be loaded out of shadow descriptor tables.
                                          ///  @see VaRange::ShadowDescriptorTable
            uint32   initUndefZero :  1;  ///< If set, undefined IL registers will be initialized to zero.
            uint32   switchWinding :  1;  ///< If true, reverse the HS declared output primitive vertex order.
            uint32   reserved      : 25;  ///< Reserved for future use.
        };
        uint32 u32All;                ///< Flags packed as 32-bit uint.
    } flags;                          ///< Various boolean settings controlling compilation of individual shaders.

    uint32 psOnlyPointCoordEnable;    ///< Pixelshader only - 32-bit mask enabling point texture coordinate generation
                                      ///  per interpolator.  Bit0 controls v0 in IL, bit1 controls v1 in IL, etc.
    const uint8* pPsTexWrapping;      ///< Pixelshader only - texture wrapping array with each entry
                                      ///  corresponds to one pixel shader input usage index. Texture wrapping causes
                                      ///  the rasterizer to take the shortest route between texture coordinate sets.
    uint32 numPsTexWrappingEntries;   ///< Number of valid entries in the pPsTexWrapping array.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 345
    /// Limits the number of waves in flight per compute unit.  This can be used to selectively throttle certain
    /// workloads that bottleneck multiqueue applications.  For ease of use, a value of zero means no limit is set.
    /// The remaining valid values are in the range [1, 40] and specify the maximum number of waves per compute unit.
    ///
    /// If the hardware has one wave limit control for multiple shader stages PAL will select the most strict limit.
    uint32 maxWavesPerCu;
#endif
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

/// Specifies the properties of a vertex element written to a stream output buffer.
///
/// PAL-capable hardware is able to perform stream-output on any pipeline, regardless of which shader stages are active.
/// The shader in the pipeline which actually performs the stream output is determined thusly:
///  - If the geometry shader is present, it does the stream-out;
///  - If the tessellation shaders are present, the domain shader does it;
///  - Otherwise, the vertex shader does it.
///
/// If stream-output is enabled for a pipeline, the client must specify exactly one @ref ResourceMappingNode with a type
/// of @ref ResourceMappingNodeType::StreamOutTableVaPtr for that shader, or behavior is undefined.
struct StreamOutEntry
{
    uint8    stream;         ///< Stream number which this entry writes to.  The valid range is zero to three.  However,
                             ///  streams other than stream zero are only valid when the geometry shader is present.
    uint8    buffer;         ///< Stream output buffer which this entry writes to.  The valid range is zero to three.
    uint32   registerIndex;  ///< "Register" in the shader using stream output which gets written to this stream output
                             ///  entry.
    uint8    registerMask;   ///< Mask of the components of the "register" which are written to stream output.  The
                             /// least significant four bits of this field are used to indicate the mask.
    gpusize  memOffset;      ///< Memory offset into the stream buffer where this entry is written (in DWORD's).
};

/// Specifies properties for creation of a compute @ref IPipeline object.  Input structure to
/// IDevice::CreateComputePipeline().
struct ComputePipelineCreateInfo
{
    PipelineCreateFlags flags;                   ///< Flags controlling pipeline creation.
    /// Shader cache that should be used to search for the compiled shader data. If shader data is not found it will be
    /// added to this shader cache object if possible. Can be nullptr to use the Device internal shader cache.
    IShaderCache*       pShaderCache;
    const void*         pShaderCacheClientData;  ///< Private client data, used to support external shader cache.

    const void*         pPipelineBinary;         ///< Pointer to Pipeline ELF binary implementing the Pipeline ABI
                                                 ///  interface. The Pipeline ELF contains pre-compiled shaders,
                                                 ///  register values, and additional metadata.
    size_t              pipelineBinarySize;      ///< Size of Pipeline ELF binary in bytes.

    PipelineShaderInfo  cs;                      ///< Compute shader information.
};

/// Specifies information about the stream-out behavior for a graphics @ref IPipeline object.  Part of the input
/// structure to @ref IDevice::CreateGraphicsPipeline().
struct PipelineStreamOutInfo
{
    /// Number of stream-output element entries needed for this pipeline.  If this is zero, then stream-output is
    /// not active for this shader and all other parameters in 'soState' are ignored.  It is an error for this to
    /// exceed the maximum supported number of stream-output entries.
    uint32  numStreamOutEntries;
    /// Set of stream-output element entries describing how vertex streams are written to GPU memory. The size of
    /// this array is determined by the value of 'numStreamOutEntries'. Can be nullptr only if numStreamOutEntries
    /// is zero.
    const StreamOutEntry*  pSoEntries;
    /// Mask of which stream(s) will be rasterized.  Since there are only four streams available, the upper 4 bits
    /// are ignored.
    uint8   rasterizedStreams;
    uint32  bufferStrides[MaxStreamOutTargets]; ///< The stride of each stream-output buffer (in bytes).
};

/// Specifies properties for creation of a graphics @ref IPipeline object.  Input structure to
/// IDevice::CreateGraphicsPipeline().
struct GraphicsPipelineCreateInfo
{
    PipelineCreateFlags flags;                 ///< Flags controlling pipeline creation.
    IShaderCache*       pShaderCache;          ///< Shader cache that should be used to search for the compiled shader
                                               ///  data. If shader data is not found it will be added to this shader
                                               ///  cache object if possible. Can be nullptr to use the Device internal
                                               ///  shader cache.
    /// Private client data, used to support external shader cache.
    const void*         pShaderCacheClientData;
    PipelineShaderInfo  vs;                    ///< Vertex shader information.
    PipelineShaderInfo  hs;                    ///< Hull shader information.
    PipelineShaderInfo  ds;                    ///< Domain shader information.
    PipelineShaderInfo  gs;                    ///< Geometry shader information.
    PipelineShaderInfo  ps;                    ///< Pixel shader information.

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
        bool        disableVertexReuse;        ///< Disable reusing vertex shader output for indexed draws.
    } iaState;                                 ///< Input assembler state.

    struct
    {
        float fixedTessFactor;                 ///< If non-0, indicates the hull shader doesn't compute variable
                                               ///  tessFactor parameters, and always outputs the specified value.
    } tessState;                               ///< Tessellation state.

    struct
    {
        bool       depthClipEnable;            ///< Enable clipping based on Z coordinate.
        DepthRange depthRange;                 ///< Specifies Z dimensions of screen space (i.e., post viewport
                                               ///  transform: 0 to 1 or -1 to 1).
    } vpState;                                 ///< Viewport state.

    struct
    {
        PointOrigin     pointCoordOrigin;          ///< Controls texture coordinate orientation for point sprites.
        bool            rasterizerDiscardEnable;   ///< Kill all rasterized pixels. This is implicitly true if stream out
                                                   ///  is enabled and no streams are rasterized.
        bool            expandLineWidth;           ///< If true, line primitives will have their width expanded by 1/cos(a)
                                                   ///  where a is the minimum angle from horizontal or vertical.
                                                   ///  This can be used in conjunction with PS patching for a client to
                                                   ///  implement line antialiasing.
        uint32          numSamples;                ///< Number of coverage samples used when rendering with this pipeline.
                                                   ///  Should match the coverageSamples value set in MsaaStateCreateInfo in
                                                   ///  MSAA state objects bound while rendering with this pipeline.  This
                                                   ///  field is currently only required to support the sampleinfo shader
                                                   ///  shader instruction, and can be set to 0 for clients that don't need
                                                   ///  to support that instruction.
        uint32          samplePatternIdx;          ///< Index into the currently bound MSAA sample pattern table that
                                                   ///  matches the sample pattern used by the rasterizer when rendering
                                                   ///  with this pipeline.  This field is only required to support the
                                                   ///  samplepos shader instruction, and will be ignored if no shader in
                                                   ///  the pipeline issues that instruction.
        ShadeMode       shadeMode;                 ///< Specifies shading mode, Gouraud or Flat
        bool            dx9PixCenter;              ///< Specifies whether to follow dx9 pix center spec.
                                                   ///  Pixel centers for DX9 are exactly in integer locations,
                                                   ///  while in DX10+ and OpenGL there is a (0.5, 0.5) offset.
        uint8           usrClipPlaneMask;          ///< Mask to indicate the enabled user defined clip planes
        bool            rasterizeLastLinePixel;    ///< Specifies whether to draw last pixel in a line.
        bool            outOfOrderPrimsEnable;     ///< Enables out-of-order primitive rasterization.  PAL silently
                                                   ///  ignores this if it is unsupported in hardware.
        bool            perpLineEndCapsEnable;     ///< Forces the use of perpendicular line end caps as opposed to
                                                   ///  axis-aligned line end caps during line rasterization.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 310
        BinningOverride binningOverride;           ///< Binning setting for this pipeline.
#endif
    } rsState;             ///< Rasterizer state.

    PipelineStreamOutInfo  soState;            ///< Stream output state.

    struct
    {
        bool    alphaToCoverageEnable;          ///< Enable alpha to coverage.
        bool    dualSourceBlendEnable;          ///< Blend state bound at draw time will use a dual source blend mode.
        LogicOp logicOp;                        ///< Logic operation to perform.

        struct
        {
            bool          blendEnable;          ///< Blend will be enabled for this target at draw time.

            bool          blendSrcAlphaToColor; ///< Whether source alpha is blended to color channels for this target
                                                ///  at draw time.

            SwizzledFormat swizzledFormat;      ///< Color target format and channel swizzle. Set the format to invalid
                                                ///  if no color target will be bound at this slot.
            uint8          channelWriteMask;    ///< Color target write mask.  Bit 0 controls the red channel, bit 1 is
                                                ///  green, bit 2 is blue, and bit 3 is alpha.
        } target[MaxColorTargets];              ///< Per-MRT color target info.
    } cbState;                                  ///< Color target state.

    struct
    {
        SwizzledFormat swizzledFormat;         ///< Depth/stencil target format and channel swizzle. Set the format to
                                               ///  invalid if no depth/stencil target will be bound.
    } dbState;                                 ///< Depth/stencil state.

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 332
    struct
    {
        union
        {
            struct
            {
                uint32 enableImplicitPrimShader     :  1; ///< If possible, requests that this pipeline enable implicit
                                                          ///  primitive shader compiling.
                uint32 disableBackfaceCulling       :  1; ///< Disables culling of primitives that don't meet "facing"
                                                          ///  criteria (back and/or front).
                uint32 enableFrustumCulling         :  1; ///< Enables discarding of primitives outside of the view
                                                          ///  frustum.
                uint32 enableBoxFilterCulling       :  1; ///< Enable simpler frustum culler that is less accurate.
                uint32 enableSphereCulling          :  1; ///< Enable frustum culling based on a sphere.
                uint32 enableSmallPrimFilter        :  1; ///< Enables trivial sub-sample primitive culling.
                uint32 enableFasterLaunchRate       :  1; ///< Enables the hardware to launch subgroups of work at a
                                                          ///  faster launch rate. Additional shader instructions are
                                                          ///  inserted for hardware functionality that is now done by
                                                          ///  the shader.
                uint32 enableVertexReuse            :  1; ///< Enables optimization to cull duplicate vertices.
                                                          ///  @note: Requires enableFasterLaunchRate to be enabled.
                uint32 knownPrimitiveTopology       :  1; ///< The full primitive topology is known at compile time and
                                                          ///  can be given to the compiler. If set, @ref primTopolgy
                                                          ///  must be specified.
                uint32 positionBufferWritesIgnoreL2 :  1; ///< Buffer writes to the offchip position buffer bypass L2
                                                          ///  and go straight to memory.
                uint32 reserved                     : 23; ///< Reserved for future use.
            };
            uint32 flags;                    ///< Flags packed as a 32-bit uint.
        };

        PrimitiveTopology primTopology; ///< Fully specified primitive topology type. Only valid if
                                        ///  @ref knownPrimitiveTopology is set.
    } implicitPrimitiveShaderControl;   ///< Requests that this pipeline have its hardware vertex shader compiled into a
                                        ///  primitive shader that performs various culling and compaction within the
                                        ///  shader, rather than by fixed-function hardware.
#else
    union
    {
        struct
        {
            uint8 enableImplicitPrimShader     : 1; ///< If possible, requests that this pipeline enable implicit
                                                    ///  primitive shader compiling.
            uint8 disableBackfaceCulling       : 1; ///< Disables culling of primitives that don't meet "facing"
                                                    ///  criteria (back and/or front).
            uint8 enableFrustumCulling         : 1; ///< Enables discarding of primitives outside of the view frustum.
            uint8 enableSmallPrimFilter        : 1; ///< Enables trivial sub-sample primitive culling.
            uint8 enableFasterLaunchRate       : 1; ///< Enables the hardware to launch subgroups of work at a faster
                                                    ///  launch rate. Additional shader instructions are inserted for
                                                    ///  hardware functionality that is now done by the shader.
            uint8 enableVertexReuse            : 1; ///< Enables optimization to cull duplicate vertices.
                                                    ///  @note: Requires enableFasterLaunchRate to be enabled.
            uint8 reserved                     : 2; ///< Reserved for future use.
        };
        uint8 flags;                    ///< Flags packed as a 8-bit uint.
    } implicitPrimitiveShaderControl;   ///< Requests that this pipeline have its hardware vertex shader compiled into a
                                        ///  primitive shader that performs various culling and compaction within the
                                        ///  shader, rather than by fixed-function hardware.
                                        ///  @note: Support for these flags can be found in @ref DeviceProperties.
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 338
    ViewInstancingDescriptor viewInstancingDesc;    ///< Descriptor describes view instancing state
                                                    ///  of the graphics pipeline
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 344
    uint64 pipelineHash;      ///< Unique 64-bit identifier for the PAL pipeline, composed of compiler information and
                              ///  PAL-specific information.
    uint64 compilerHash;      ///< 64-bit identifier extracted from this pipeline's ELF binary, composed of the state
                              ///  the compiler decided was appropriate to identify the compiled shaders.  Pipelines
                              ///  can have identical compiler hashes but different pipeline hashes.  Note that this
                              ///  is not computed by taking a hash of the binary blob data.
#else
    ///< Unique 64-bit identifier for the PAL pipeline, composed of compiler information and PAL-specific information.
    ///  The union allows us to use the renamed pipelineHash while preserving backwards compatibility.
    union
    {
        uint64 hash;
        uint64 pipelineHash;
    };

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
    /// Stores a serialized representation of this pipeline to a region of client-provided system memory.
    ///
    /// @param [in,out] pDataSize Input value specifies the available size in pData in bytes; output value reports the
    ///                           number of bytes written to pData.
    /// @param [out]    pData     Binary pipeline representation.  Can be nullin order to only query the required size.
    ///
    /// @returns Success if the serialized pipeline was successfully returned.  Otherwise, one of the following error
    ///          codes may be returned.
    ///          + ErrorInvalidPointer if pDataSize is null.
    ///          + ErrorInvalidMemorySize if pData is non-null and the input value of pDataSize is too small.
    virtual Result Store(
        size_t* pDataSize,
        void*   pData) = 0;

    /// Returns the shader disassembly for the specified shader stage associated with this pipeline.
    ///
    /// @param [in]     shaderType Shader stage for which disassembly is requested.
    /// @param [out]    pBuffer    Location of the buffer to which shader disassembly is written. Can be nullptr to only
    ///                            query the length of the shader disassembly string.
    /// @param [in,out] pSize      Input value  specifies the available size in pBuffer; output value reports the length
    ///                            of the shader disassembly string of shaderType.
    /// @returns Success if the shader disassembly was successfully written to the buffer; if the shader stage
    ///          specified was not associated with this pipeline the size returned is zero.
    ///          + ErrorInvalidMemorySize if the caller provides a buffer that is not large enough.
    ///          + ErrorUnavailable if the shader specified was not able to be retrieved.
    ///          + ErrorInvalidPointer if the input pSize pointer is invalid.
    ///          + ErrorUnknown if an internal PAL error occurs.
    virtual Result GetShaderDisassembly(
        ShaderType shaderType,
        void*      pBuffer,
        size_t*    pSize) const = 0;

    /// Returns PAL-computed properties of this pipeline and its corresponding shaders.
    ///
    /// @returns Property structure describing this pipeline.
    virtual const PipelineInfo& GetInfo() const = 0;

    /// Add the shaders associated with this pipeline to the provided shader cache.
    ///
    /// @param [in,out]  pShaderCache Shader cache where the shaders should be added.
    ///
    /// @returns Success if shaders were successfully added, or Unavailable if the shader cache is unintialized/invalid.
    virtual Result AddShadersToCache(
        IShaderCache* pShaderCache) = 0;

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
        GpuMemSubAllocInfo* const  pAllocInfoList) = 0;

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
    /// @note This is temporary until the SCPC separation is finalized.
    ///
    /// @returns The appropriate mapping for this pipeline.
    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const = 0;

    /// Returns the value of the associated arbitrary client data pointer.
    /// Can be used to associate arbitrary data with a particular PAL object.
    ///
    /// @returns Pointer to client data.
    PAL_INLINE void* GetClientData() const
    {
        return m_pClientData;
    }

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
