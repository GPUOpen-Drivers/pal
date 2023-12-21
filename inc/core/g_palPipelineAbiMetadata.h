/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
namespace PalAbi
{

using MsgPackOffset  = uint32;
using StringViewType = StringView<char>;

struct BinaryData
{
    const void*  pBuffer;
    uint32       sizeInBytes;
};

/// Per-API shader metadata.
struct ShaderMetadata
{
    /// Input shader hash, typically passed in from the client.
    uint64                apiShaderHash[2];
    /// Flags indicating the HW stages this API shader maps to.
    uint32                hardwareMapping;
    /// Shader subtype
    Abi::ApiShaderSubType shaderSubtype;

    union
    {
        struct
        {
            uint8 apiShaderHash   : 1;
            uint8 hardwareMapping : 1;
            uint8 shaderSubtype   : 1;
            uint8 reserved        : 5;
        };
        uint8 uAll;
    } hasEntry;
};

/// Instance of a constant buffer read from an instruction.
struct CbConstUsageMetadata
{
    /// constant buffer id
    uint32                bufferId;
    /// constant buffer index in the range
    uint32                bufferIndex;
    /// slot
    uint32                elem;
    /// channel select
    uint8                 chan;
    /// constant usage
    Abi::CbConstUsageType usage;

    union
    {
        struct
        {
            uint8 bufferId    : 1;
            uint8 bufferIndex : 1;
            uint8 elem        : 1;
            uint8 chan        : 1;
            uint8 usage       : 1;
            uint8 reserved    : 3;
        };
        uint8 uAll;
    } hasEntry;
};

/// Per-hardware stage metadata.
struct HardwareStageMetadata
{
    /// The symbol pointing to this pipeline's stage entrypoint.
    Abi::PipelineSymbolType entryPoint;
    /// Scratch memory size in bytes.
    uint32                  scratchMemorySize;
    /// size in bytes of the stack managed by the compiler backend.
    uint32                  backendStackSize;
    /// size in bytes of the stack managed by the frontend.
    uint32                  frontendStackSize;
    /// Local Data Share size in bytes.
    uint32                  ldsSize;
    /// Performance data buffer size in bytes.
    uint32                  perfDataBufferSize;
    /// Number of VGPRs used.
    uint32                  vgprCount;
    /// Number of SGPRs used.
    uint32                  sgprCount;
    /// If non-zero, indicates the shader was compiled with a directive to instruct the compiler to limit the VGPR usage
    /// to be less than or equal to the specified value (only set if different from HW default).
    uint32                  vgprLimit;
    /// SGPR count upper limit (only set if different from HW default).
    uint32                  sgprLimit;

    /// Thread-group X/Y/Z dimensions (Compute only).
    uint32                  threadgroupDimensions[3];
    /// Original thread-group X/Y/Z dimensions (Compute only).
    uint32                  origThreadgroupDimensions[3];
    /// Instance of a constant buffer read from an instruction.
    CbConstUsageMetadata    cbConstUsage[16];
    /// Size of cbConstUsage array (max 16 entries)
    uint8                   numCbConstUsages;
    /// Wavefront size (only set if different from HW default).
    uint32                  wavefrontSize;
    /// User data register mapping to user data entries. See :ref:`amdgpu-amdpal-code-object-user-data-section` for more
    /// details.
    uint32                  userDataRegMap[32];
    /// Value used for shader profiling for power feature.
    uint32                  checksumValue;
    /// Float mode for waves of this shader.
    uint8                   floatMode;
    /// Number of USER_DATA SGPRs.
    uint8                   userSgprs;
    /// Which exceptions to trap on.
    uint16                  excpEn;
    /// Number of shared VGPRs for Wave64. Must be 0 for Wave32.
    uint8                   sharedVgprCnt;
    /// Wave limit per shader engine.
    uint32                  wavesPerSe;

    union
    {
        struct
        {
            /// FP16 overflow mode.
            uint16 fp16Overflow      : 1;
            /// IEEE mode.
            uint16 ieeeMode          : 1;
            /// Whether waves of this shader will be launched in CU-centric or WGP-centric mode.
            uint16 wgpMode           : 1;
            /// Indiciates if using mem ordred mode. If false, indiciates that all loads, stores, and samples are
            /// unordered with respect to each other. If true, indicates that loads and samples are kept in order with
            /// each other, but stores are not ordered with loads.
            uint16 memOrdered        : 1;
            /// Indicates if using forward progress.
            uint16 forwardProgress   : 1;
            /// Indicates the debug mode.
            uint16 debugMode         : 1;
            /// Whether this wave uses scratch space for register spilling.
            uint16 scratchEn         : 1;
            /// Whether a trap handler has been enabled for this wave.
            uint16 trapPresent       : 1;
            /// Whether offchip LDS information needs to be loaded.
            uint16 offchipLdsEn      : 1;
            /// The shader reads or writes UAVs.
            uint16 usesUavs          : 1;
            /// The shader reads or writes ROVs.
            uint16 usesRovs          : 1;
            /// The shader writes to one or more UAVs.
            uint16 writesUavs        : 1;
            /// The shader writes out a depth value.
            uint16 writesDepth       : 1;
            /// The shader uses append and/or consume operations, either memory or GDS.
            uint16 usesAppendConsume : 1;
            /// The shader uses PrimID.
            uint16 usesPrimId        : 1;
            uint16 placeholder0      : 1;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint64 entryPoint                : 1;
            uint64 scratchMemorySize         : 1;
            uint64 backendStackSize          : 1;
            uint64 frontendStackSize         : 1;
            uint64 ldsSize                   : 1;
            uint64 perfDataBufferSize        : 1;
            uint64 vgprCount                 : 1;
            uint64 sgprCount                 : 1;
            uint64 vgprLimit                 : 1;
            uint64 sgprLimit                 : 1;
            uint64 placeholder0              : 1;
            uint64 threadgroupDimensions     : 1;
            uint64 origThreadgroupDimensions : 1;
            uint64 cbConstUsage              : 1;
            uint64 numCbConstUsages          : 1;
            uint64 wavefrontSize             : 1;
            uint64 userDataRegMap            : 1;
            uint64 checksumValue             : 1;
            uint64 floatMode                 : 1;
            uint64 fp16Overflow              : 1;
            uint64 ieeeMode                  : 1;
            uint64 wgpMode                   : 1;
            uint64 memOrdered                : 1;
            uint64 forwardProgress           : 1;
            uint64 debugMode                 : 1;
            uint64 scratchEn                 : 1;
            uint64 trapPresent               : 1;
            uint64 userSgprs                 : 1;
            uint64 excpEn                    : 1;
            uint64 offchipLdsEn              : 1;
            uint64 sharedVgprCnt             : 1;
            uint64 wavesPerSe                : 1;
            uint64 usesUavs                  : 1;
            uint64 usesRovs                  : 1;
            uint64 writesUavs                : 1;
            uint64 writesDepth               : 1;
            uint64 usesAppendConsume         : 1;
            uint64 usesPrimId                : 1;
            uint64 placeholder1              : 1;
            uint64 reserved                  : 25;
        };
        uint64 uAll;
    } hasEntry;
};

/// Pixel shader input semantic info.
struct PsInputSemanticMetadata
{
    /// Key for input and output interface match between Ps and pre-raster stage.
    uint16 semantic;

    union
    {
        struct
        {
            uint8 semantic : 1;
            uint8 reserved : 7;
        };
        uint8 uAll;
    } hasEntry;
};

/// Output semantic info in pre-raster stage which is before pixel shader.
struct PrerasterOutputSemanticMetadata
{
    /// Key for input and output interface match between Ps and pre-raster stage.
    uint16 semantic;
    /// Parameter index in pre-raster stage export.
    uint8  index;

    union
    {
        struct
        {
            uint8 semantic : 1;
            uint8 index    : 1;
            uint8 reserved : 6;
        };
        uint8 uAll;
    } hasEntry;
};

struct PaClClipCntlMetadata
{

    union
    {
        struct
        {
            /// Whether User Clip Plane 0 is enabled.
            uint16 userClipPlane0Ena   : 1;
            /// Whether User Clip Plane 1 is enabled.
            uint16 userClipPlane1Ena   : 1;
            /// Whether User Clip Plane 2 is enabled.
            uint16 userClipPlane2Ena   : 1;
            /// Whether User Clip Plane 3 is enabled.
            uint16 userClipPlane3Ena   : 1;
            /// Whether User Clip Plane 4 is enabled.
            uint16 userClipPlane4Ena   : 1;
            /// Whether User Clip Plane 5 is enabled.
            uint16 userClipPlane5Ena   : 1;
            /// Whether the clipper performs special t_factor adjustment from DX10 to calculate the attribute
            /// barycentric coordinates to allow for linear gradient appearance across clipped triangle fan. If reset,
            /// vertices will use perspective correct barycentric coordinates.
            uint16 dxLinearAttrClipEna : 1;
            /// Whether depth near clipping is disabled.
            uint16 zclipNearDisable    : 1;
            /// Whether depth far clipping is disabled.
            uint16 zclipFarDisable     : 1;
            /// Whether rasterization kill is enabled.
            uint16 rasterizationKill   : 1;
            /// Whether clipping is disabled. Must be set if the VS ouputs window coordinates.
            uint16 clipDisable         : 1;
            uint16 reserved            : 5;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 userClipPlane0Ena   : 1;
            uint16 userClipPlane1Ena   : 1;
            uint16 userClipPlane2Ena   : 1;
            uint16 userClipPlane3Ena   : 1;
            uint16 userClipPlane4Ena   : 1;
            uint16 userClipPlane5Ena   : 1;
            uint16 dxLinearAttrClipEna : 1;
            uint16 zclipNearDisable    : 1;
            uint16 zclipFarDisable     : 1;
            uint16 rasterizationKill   : 1;
            uint16 clipDisable         : 1;
            uint16 reserved            : 5;
        };
        uint16 uAll;
    } hasEntry;
};

struct PaClVteCntlMetadata
{

    union
    {
        struct
        {
            /// Indicates that the incoming X, Y have already been multiplied by 1/W0. Must be set if the vertex shader
            /// outputs window coordinates.
            uint16 vtxXyFmt   : 1;
            /// Indicates that the incoming Z has already been multiplied by 1/W0. Must be set if the vertex shader
            /// outputs window coordinates.
            uint16 vtxZFmt    : 1;
            /// Whether the Viewport Transform performs scaling on the X component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 xScaleEna  : 1;
            /// Whether the Viewport Transform adds the offset on the X component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 xOffsetEna : 1;
            /// Whether the Viewport Transform performs scaling on the Y component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 yScaleEna  : 1;
            /// Whether the Viewport Transform adds the offset on the Y component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 yOffsetEna : 1;
            /// Whether the Viewport Transform performs scaling on the Z component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 zScaleEna  : 1;
            /// Whether the Viewport Transform adds the offset on the Z component. Must be false if the vertex shader
            /// outputs window coordinates.
            uint16 zOffsetEna : 1;
            /// Indicates that the incoming W0 is not 1/W0. Must be false if the vertex shader outputs window
            /// coordinates.
            uint16 vtxW0Fmt   : 1;
            uint16 reserved   : 7;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 vtxXyFmt   : 1;
            uint16 vtxZFmt    : 1;
            uint16 xScaleEna  : 1;
            uint16 xOffsetEna : 1;
            uint16 yScaleEna  : 1;
            uint16 yOffsetEna : 1;
            uint16 zScaleEna  : 1;
            uint16 zOffsetEna : 1;
            uint16 vtxW0Fmt   : 1;
            uint16 reserved   : 7;
        };
        uint16 uAll;
    } hasEntry;
};

struct PaSuVtxCntlMetadata
{
    /// Controls conversion of X,Y coordinates from IEEE to fixed-point - 0 = Truncate - 1 = Round - 2 = Round to Even -
    /// 3 = Round to Odd
    uint8 roundMode;
    /// Controls conversion of X,Y coordinates from IEEE to fixed-point.
    /// Determines fixed point format and how many fractional bits are actually utilized.
    /// Modes 0-4 are not supported when conservative rasterization is enabled.
    /// - 0 = 16.8 fixed point. 1/16th ( 4 fractional bits used)
    /// - 1 = 16.8 fixed point. 1/8th ( 3 fractional bits used)
    /// - 2 = 16.8 fixed point. 1/4th ( 2 fractional bits used)
    /// - 3 = 16.8 fixed point. 1/2 ( 1 fractional bit used)
    /// - 4 = 16.8 fixed point. 1 ( 0 fractional bits used)
    /// - 5 = 16.8 fixed point. 1/256th ( 8 fractional bits)
    /// - 6 = 14.10 fixed point. 1/1024th (10 fractional bits)
    /// - 7 = 12.12 fixed point. 1/4096th (12 fractional bits)
    uint8 quantMode;

    union
    {
        struct
        {
            /// Specifies where the pixel center of the incoming vertex is. The drawing engine itself has pixel centers
            /// @ 0.5, so if this bit is `0`, 0.5 will be added to the X,Y coordinates to move the incoming vertex onto
            /// our internal grid.
            uint8 pixCenter : 1;
            uint8 reserved  : 7;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 pixCenter : 1;
            uint8 roundMode : 1;
            uint8 quantMode : 1;
            uint8 reserved  : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtShaderStagesEnMetadata
{
    /// Whether the ES stage is enabled.
    /// - 0 - ES stage off.
    /// - 1 - ES stage on, and the ES is a Domain shader.
    /// - 2 - ES stage on, and the ES is a Vertex shader.
    uint8 esStageEn;
    /// Whether the VS stage is enabled.
    /// - 0 - VS stage is on, and is an API Vertex Shader.
    /// - 1 - VS stage is on, and is an API Domain Shader.
    /// - 2 - VS stage is on, and is a copy shader.
    uint8 vsStageEn;
    /// Maximum number of primgroups that can be combined into a single ES or VS wave.
    uint8 maxPrimgroupInWave;
    /// Whether NGG subgroups should be launched in a different mode, possibly at a faster rate.
    uint8 gsFastLaunch;

    union
    {
        struct
        {
            /// Whether the LS stage is enabled.
            uint16 lsStageEn            : 1;
            /// Whether the HS stage is enabled.
            uint16 hsStageEn            : 1;
            /// Whether the GS stage is enabled.
            uint16 gsStageEn            : 1;
            /// Whether the output of the HS stage stays on chip or whether it is dynamically decided to use offchip.
            uint16 dynamicHs            : 1;
            /// Whether or not Next Generation Geometry (Prim Shader) is enabled.
            uint16 primgenEn            : 1;
            /// Whether the ordered wave id for the primitive shader is created per sub-group or per wave.
            /// - false - WaveId per sub-group
            /// - true  - WaveId per wave
            uint16 orderedIdMode        : 1;
            /// Whether the NGG wave ID will be incremented.
            uint16 nggWaveIdEn          : 1;
            /// Whether the NGG pipeline is run in passthrough mode.
            uint16 primgenPassthruEn    : 1;
#if PAL_BUILD_GFX11
            /// When the NGG pipeline is in passthrough mode, whether or not the shader must send the allocation
            /// message.
            uint16 primgenPassthruNoMsg : 1;
#else
            uint16 placeholder0         : 1;
#endif
            uint16 reserved             : 7;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 lsStageEn            : 1;
            uint16 hsStageEn            : 1;
            uint16 esStageEn            : 1;
            uint16 gsStageEn            : 1;
            uint16 vsStageEn            : 1;
            uint16 dynamicHs            : 1;
            uint16 maxPrimgroupInWave   : 1;
            uint16 primgenEn            : 1;
            uint16 orderedIdMode        : 1;
            uint16 nggWaveIdEn          : 1;
            uint16 gsFastLaunch         : 1;
            uint16 primgenPassthruEn    : 1;
#if PAL_BUILD_GFX11
            uint16 primgenPassthruNoMsg : 1;
#else
            uint16 placeholder0         : 1;
#endif
            uint16 reserved             : 3;
        };
        uint16 uAll;
    } hasEntry;
};

struct VgtGsModeMetadata
{
    /// Which GS scenario to enable.
    uint8 mode;
    /// On-chip mode for ESGS and GSVS communication.
    /// - 0 - EsGs and GsVs data is offchip.
    /// - 1 - GsVs data is offchip.
    /// - 3 - EsGs and GsVs data in onchip.
    uint8 onchip;
    /// Cut length, dependent on how many vertices the GS emits.
    /// - 0 - More than 512 GS emit vertices.
    /// - 1 - More than 256 GS emit vertices and less than equal to 512.
    /// - 2 - More than 128 GS emit vertices and less than equal to 256.
    /// - 3 - Less than equal to 128 GS emit vertices.
    uint8 cutMode;

    union
    {
        struct
        {
            /// Whether the ESGS ring is optimized for write combining.
            uint8 esWriteOptimize : 1;
            /// Whether the GSVS ring is optimized for write combining.
            uint8 gsWriteOptimize : 1;
            uint8 reserved        : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 mode            : 1;
            uint8 onchip          : 1;
            uint8 esWriteOptimize : 1;
            uint8 gsWriteOptimize : 1;
            uint8 cutMode         : 1;
            uint8 reserved        : 3;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtTfParamMetadata
{
    /// Tessellation type.
    /// - 0 - Isoline
    /// - 1 - Triangle
    /// - 2 - Quad
    uint8 type;
    /// Partition type.
    /// - 0 - Integer
    /// - 1 - Pow2
    /// - 2 - Fractional Odd
    /// - 3 - Fractional Even
    uint8 partitioning;
    /// Output primitive topology.
    /// - 0 - Point
    /// - 1 - Line
    /// - 2 - Triangle Clockwise
    /// - 3 - Triangle Counter-clockwise
    uint8 topology;
    /// How many DS waves (ES/VS) are sent to the same SIMD before spilling to other SIMDs to use the offchip LDS data
    uint8 numDsWavesPerSimd;
    /// Mode used for distributed tessellation.
    /// Requires offchip tessellation to be enabled for PATCHES and DONUT modes of distribution.
    /// - 0 - No distribution.
    /// - 1 - Patches
    /// - 2 - Donuts
    /// - 3 - Trapezoids
    uint8 distributionMode;

    union
    {
        struct
        {
            /// Whether to disable donut walking pattern is used by the tessellator.
            uint8 disableDonuts : 1;
            uint8 reserved      : 7;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 type              : 1;
            uint8 partitioning      : 1;
            uint8 topology          : 1;
            uint8 disableDonuts     : 1;
            uint8 numDsWavesPerSimd : 1;
            uint8 distributionMode  : 1;
            uint8 reserved          : 2;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtLsHsConfigMetadata
{
    /// Number of patches in a threadgroup. Max verts/threadgroup is 256.
    uint8 numPatches;
    /// Number of control points in HS input patch. Valid range is 1-32.
    uint8 hsNumInputCp;
    /// Number of control points in HS output patch. Valid range is 1-32.
    uint8 hsNumOutputCp;

    union
    {
        struct
        {
            uint8 numPatches    : 1;
            uint8 hsNumInputCp  : 1;
            uint8 hsNumOutputCp : 1;
            uint8 reserved      : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct IaMultiVgtParamMetadata
{
    /// Number of primitives sent to one of the frontends before switching to the next frontend. Implied +1.
    uint16 primgroupSize;

    union
    {
        struct
        {
            /// Whether the frontend will issue a VS wave as soon as a primgroup is finished, or if it will continue a
            /// VS wave from one primgroup into the next within a draw call.
            uint8 partialVsWaveOn : 1;
            /// Whether the frontend will issue an ES wave as soon as a primgroup is finished, or if it will continue a
            /// ES wave from one primgroup into the next within a draw call.
            uint8 partialEsWaveOn : 1;
            /// Whether the overall frontend will switch between frontends at packet boundaries, otherwise will switch
            /// based on size of primgroups.
            uint8 switchOnEop     : 1;
            /// Whether the overall frontend will switch between frontends at instance boundaries, otherwise will switch
            /// based on size of primgroups.
            uint8 switchOnEoi     : 1;
            uint8 reserved        : 4;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 primgroupSize   : 1;
            uint8 partialVsWaveOn : 1;
            uint8 partialEsWaveOn : 1;
            uint8 switchOnEop     : 1;
            uint8 switchOnEoi     : 1;
            uint8 reserved        : 3;
        };
        uint8 uAll;
    } hasEntry;
};

struct SpiInterpControlMetadata
{
    Abi::PointSpriteSelect pointSpriteOverrideX;
    Abi::PointSpriteSelect pointSpriteOverrideY;
    Abi::PointSpriteSelect pointSpriteOverrideZ;
    Abi::PointSpriteSelect pointSpriteOverrideW;

    union
    {
        struct
        {
            /// Enable point sprite override for point primitives.
            uint8 pointSpriteEna : 1;
            uint8 reserved       : 7;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 pointSpriteEna       : 1;
            uint8 pointSpriteOverrideX : 1;
            uint8 pointSpriteOverrideY : 1;
            uint8 pointSpriteOverrideZ : 1;
            uint8 pointSpriteOverrideW : 1;
            uint8 reserved             : 3;
        };
        uint8 uAll;
    } hasEntry;
};

struct SpiPsInputCntlMetadata
{
    /// PS input offset - specifies which parameter cache outputs are for this input.
    uint8 offset;
    /// Selects default value if no semantic match is found.
    uint8 defaultVal;
    /// Cylindrical wrap control.
    uint8 cylWrap;

    union
    {
        struct
        {
            /// Flat shade select. Set if interpolation mode is constant.
            uint8 flatShade      : 1;
            /// Whether this parameter should be overridden with texture coordinates if global point sprite enable is
            /// set.
            uint8 ptSpriteTex    : 1;
            /// Specifies that up to two parameters are interpolated in FP16 mode and loaded as an FP16 pair in the PS
            /// input GPR.
            uint8 fp16InterpMode : 1;
            /// Whether the first FP16 parameter is valid. Only valid if fp16_interp_mode is set.
            uint8 attr0Valid     : 1;
            /// Whether the second FP16 parameter is valid. Only valid if fp16_interp_mode is set.
            uint8 attr1Valid     : 1;
            /// Whether the hardware will provide provoking vertex ID and rotate the raw attribute parameter cache
            /// pointers accordingly.
            uint8 rotatePcPtr    : 1;
#if PAL_BUILD_GFX11
            /// Whether this parameter is a primitive attribute.
            uint8 primAttr       : 1;
#else
            uint8 placeholder0   : 1;
#endif
            uint8 reserved       : 1;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 offset         : 1;
            uint16 defaultVal     : 1;
            uint16 flatShade      : 1;
            uint16 cylWrap        : 1;
            uint16 ptSpriteTex    : 1;
            uint16 fp16InterpMode : 1;
            uint16 attr0Valid     : 1;
            uint16 attr1Valid     : 1;
            uint16 rotatePcPtr    : 1;
#if PAL_BUILD_GFX11
            uint16 primAttr       : 1;
#else
            uint16 placeholder0   : 1;
#endif
            uint16 reserved       : 6;
        };
        uint16 uAll;
    } hasEntry;
};

#if PAL_BUILD_GFX11
struct SpiShaderGsMeshletDimMetadata
{
    /// Threadgroup size in the X dimension.
    uint16 numThreadX;
    /// Threadgroup size in the Y dimension.
    uint16 numThreadY;
    /// Threadgroup size in the Z dimension.
    uint16 numThreadZ;
    /// Threadgroup size (X * Y * Z).
    uint32 threadgroupSize;

    union
    {
        struct
        {
            uint8 numThreadX      : 1;
            uint8 numThreadY      : 1;
            uint8 numThreadZ      : 1;
            uint8 threadgroupSize : 1;
            uint8 reserved        : 4;
        };
        uint8 uAll;
    } hasEntry;
};
#endif

#if PAL_BUILD_GFX11
struct SpiShaderGsMeshletExpAllocMetadata
{
    /// Maximum position export space per meshlet subgroup.
    uint16 maxExpVerts;
    /// Maximum primitive export space per meshlet subgroup.
    uint16 maxExpPrims;

    union
    {
        struct
        {
            uint8 maxExpVerts : 1;
            uint8 maxExpPrims : 1;
            uint8 reserved    : 6;
        };
        uint8 uAll;
    } hasEntry;
};
#endif

struct VgtGsInstanceCntMetadata
{
    /// Number of GS primitive instances. If set to 0, GS instancing is treated as disabled.
    uint8 count;

    union
    {
        struct
        {
            /// Whether or not GS instancing is enabled.
            uint8 enable                    : 1;
            /// Allows each GS instance to emit max_vert_out.
            uint8 enMaxVertOutPerGsInstance : 1;
            uint8 reserved                  : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 enable                    : 1;
            uint8 count                     : 1;
            uint8 enMaxVertOutPerGsInstance : 1;
            uint8 reserved                  : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtGsOutPrimTypeMetadata
{
    /// Output primitive type from the geometry shader for stream 0.
    Abi::GsOutPrimType outprimType;
    /// Output primitive type from the geometry shader for stream 1.
    Abi::GsOutPrimType outprimType_1;
    /// Output primitive type from the geometry shader for stream 2.
    Abi::GsOutPrimType outprimType_2;
    /// Output primitive type from the geometry shader for stream 3.
    Abi::GsOutPrimType outprimType_3;

    union
    {
        struct
        {
            /// If set, outprim_type[0] field only represents stream 0. Otherwise, outprim_type[0] field represents all
            /// streams.
            uint8 uniqueTypePerStream : 1;
            uint8 reserved            : 7;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 outprimType         : 1;
            uint8 outprimType_1       : 1;
            uint8 outprimType_2       : 1;
            uint8 outprimType_3       : 1;
            uint8 uniqueTypePerStream : 1;
            uint8 reserved            : 3;
        };
        uint8 uAll;
    } hasEntry;
};

struct GeNggSubgrpCntlMetadata
{
    /// Controls the maximum amplification factor applied to each primitive in a subgroup.
    uint16 primAmpFactor;
    /// Controls the number of threads launched per subgroup in NGG fast launch mode.
    uint16 threadsPerSubgroup;

    union
    {
        struct
        {
            uint8 primAmpFactor      : 1;
            uint8 threadsPerSubgroup : 1;
            uint8 reserved           : 6;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtGsOnchipCntlMetadata
{
    /// Worst case number of ES vertices needed to create the GS prims specified in gs_prims_per_subgroup.
    uint16 esVertsPerSubgroup;
    /// Number of GS primitives that can fit into LDS.
    uint16 gsPrimsPerSubgroup;
    /// Total number of GS primitives taking into account GS instancing.
    uint16 gsInstPrimsPerSubgrp;

    union
    {
        struct
        {
            uint8 esVertsPerSubgroup   : 1;
            uint8 gsPrimsPerSubgroup   : 1;
            uint8 gsInstPrimsPerSubgrp : 1;
            uint8 reserved             : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct PaClVsOutCntlMetadata
{

    union
    {
        struct
        {
            /// Enable ClipDistance 0 to be used for user-defined clipping.
            uint32 clipDistEna_0          : 1;
            /// Enable ClipDistance 1 to be used for user-defined clipping.
            uint32 clipDistEna_1          : 1;
            /// Enable ClipDistance 2 to be used for user-defined clipping.
            uint32 clipDistEna_2          : 1;
            /// Enable ClipDistance 3 to be used for user-defined clipping.
            uint32 clipDistEna_3          : 1;
            /// Enable ClipDistance 4 to be used for user-defined clipping.
            uint32 clipDistEna_4          : 1;
            /// Enable ClipDistance 5 to be used for user-defined clipping.
            uint32 clipDistEna_5          : 1;
            /// Enable ClipDistance 6 to be used for user-defined clipping.
            uint32 clipDistEna_6          : 1;
            /// Enable ClipDistance 7 to be used for user-defined clipping.
            uint32 clipDistEna_7          : 1;
            /// Enable CullDistance 0 to be used for user-defined clip discard.
            uint32 cullDistEna_0          : 1;
            /// Enable CullDistance 1 to be used for user-defined clip discard.
            uint32 cullDistEna_1          : 1;
            /// Enable CullDistance 2 to be used for user-defined clip discard.
            uint32 cullDistEna_2          : 1;
            /// Enable CullDistance 3 to be used for user-defined clip discard.
            uint32 cullDistEna_3          : 1;
            /// Enable CullDistance 4 to be used for user-defined clip discard.
            uint32 cullDistEna_4          : 1;
            /// Enable CullDistance 5 to be used for user-defined clip discard.
            uint32 cullDistEna_5          : 1;
            /// Enable CullDistance 6 to be used for user-defined clip discard.
            uint32 cullDistEna_6          : 1;
            /// Enable CullDistance 7 to be used for user-defined clip discard.
            uint32 cullDistEna_7          : 1;
            /// Use the PointSize output from the VS.
            uint32 useVtxPointSize        : 1;
            /// Use the EdgeFlag output from the VS.
            uint32 useVtxEdgeFlag         : 1;
            /// Use the RenderTargetArrayIndex output from the VS.
            uint32 useVtxRenderTargetIndx : 1;
            /// Use the ViewportArrayIndex output from the VS.
            uint32 useVtxViewportIndx     : 1;
            /// Use the KillFlag output from the VS.
            uint32 useVtxKillFlag         : 1;
            /// Output the VS output misc vector from the VS.
            uint32 vsOutMiscVecEna        : 1;
            /// Output the VS output ccdist0 vector from the VS.
            uint32 vsOutCcDist0VecEna     : 1;
            /// Output the VS output ccdist1 vector from the VS.
            uint32 vsOutCcDist1VecEna     : 1;
            /// Enable performance optimization where SX outputs vs_out_misc_vec data on extra side bus.
            uint32 vsOutMiscSideBusEna    : 1;
            /// Use the LineWidth output from the VS.
            uint32 useVtxLineWidth        : 1;
            /// Use the VRS rates output from the VS.
            uint32 useVtxVrsRate          : 1;
            /// Force the vertex rate combiner into bypass mode.
            uint32 bypassVtxRateCombiner  : 1;
            /// Force the primitive rate combiner into bypass mode.
            uint32 bypassPrimRateCombiner : 1;
            /// Use the GsCutFlag output from the VS.
            uint32 useVtxGsCutFlag        : 1;
#if PAL_BUILD_GFX11
            /// Use the FSR select output from the VS.
            uint32 useVtxFsrSelect        : 1;
#else
            uint32 placeholder0           : 1;
#endif
            uint32 reserved               : 1;
        };
        uint32 uAll;
    } flags;

    union
    {
        struct
        {
            uint32 clipDistEna_0          : 1;
            uint32 clipDistEna_1          : 1;
            uint32 clipDistEna_2          : 1;
            uint32 clipDistEna_3          : 1;
            uint32 clipDistEna_4          : 1;
            uint32 clipDistEna_5          : 1;
            uint32 clipDistEna_6          : 1;
            uint32 clipDistEna_7          : 1;
            uint32 cullDistEna_0          : 1;
            uint32 cullDistEna_1          : 1;
            uint32 cullDistEna_2          : 1;
            uint32 cullDistEna_3          : 1;
            uint32 cullDistEna_4          : 1;
            uint32 cullDistEna_5          : 1;
            uint32 cullDistEna_6          : 1;
            uint32 cullDistEna_7          : 1;
            uint32 useVtxPointSize        : 1;
            uint32 useVtxEdgeFlag         : 1;
            uint32 useVtxRenderTargetIndx : 1;
            uint32 useVtxViewportIndx     : 1;
            uint32 useVtxKillFlag         : 1;
            uint32 vsOutMiscVecEna        : 1;
            uint32 vsOutCcDist0VecEna     : 1;
            uint32 vsOutCcDist1VecEna     : 1;
            uint32 vsOutMiscSideBusEna    : 1;
            uint32 useVtxLineWidth        : 1;
            uint32 useVtxVrsRate          : 1;
            uint32 bypassVtxRateCombiner  : 1;
            uint32 bypassPrimRateCombiner : 1;
            uint32 useVtxGsCutFlag        : 1;
#if PAL_BUILD_GFX11
            uint32 useVtxFsrSelect        : 1;
#else
            uint32 placeholder0           : 1;
#endif
            uint32 reserved               : 1;
        };
        uint32 uAll;
    } hasEntry;
};

struct SpiVsOutConfigMetadata
{
    /// Number of vectors exported by the VS.
    uint8 vsExportCount;
    /// Number of vectors exported by the primitive shader as a primitive attribute.
    uint8 primExportCount;

    union
    {
        struct
        {
            /// Whether the associated draw's waves or groups will allocate zero parameter cache space.
            uint8 noPcExport : 1;
            uint8 reserved   : 7;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 noPcExport      : 1;
            uint8 vsExportCount   : 1;
            uint8 primExportCount : 1;
            uint8 reserved        : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtStrmoutConfigMetadata
{
    /// Stream for which rasterization is enabled.
    uint8 rastStream;
    /// Mask indicating which stream is enabled.
    uint8 rastStreamMask;

    union
    {
        struct
        {
            /// Whether stream output to stream 0 is enabled.
            uint8 streamout_0En     : 1;
            /// Whether stream output to stream 1 is enabled.
            uint8 streamout_1En     : 1;
            /// Whether stream output to stream 2 is enabled.
            uint8 streamout_2En     : 1;
            /// Whether stream output to stream 3 is enabled.
            uint8 streamout_3En     : 1;
            /// Whether the hardware will count output prims seen irrespective of streamout enabled.
            uint8 primsNeededCntEn  : 1;
            /// Whether rast_stream_mask is valid and should be used, otherwise use rast_stream.
            uint8 useRastStreamMask : 1;
            uint8 reserved          : 2;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 streamout_0En     : 1;
            uint8 streamout_1En     : 1;
            uint8 streamout_2En     : 1;
            uint8 streamout_3En     : 1;
            uint8 rastStream        : 1;
            uint8 primsNeededCntEn  : 1;
            uint8 rastStreamMask    : 1;
            uint8 useRastStreamMask : 1;
        };
        uint8 uAll;
    } hasEntry;
};

struct VgtStrmoutBufferConfigMetadata
{
    /// Mask of which buffers are bound for stream 0.
    uint8 stream_0BufferEn;
    /// Mask of which buffers are bound for stream 1.
    uint8 stream_1BufferEn;
    /// Mask of which buffers are bound for stream 2.
    uint8 stream_2BufferEn;
    /// Mask of which buffers are bound for stream 3.
    uint8 stream_3BufferEn;

    union
    {
        struct
        {
            uint8 stream_0BufferEn : 1;
            uint8 stream_1BufferEn : 1;
            uint8 stream_2BufferEn : 1;
            uint8 stream_3BufferEn : 1;
            uint8 reserved         : 4;
        };
        uint8 uAll;
    } hasEntry;
};

struct CbShaderMaskMetadata
{
    /// 4-bit mask of which color RT0's components are enabled.
    uint8 output0Enable;
    /// 4-bit mask of which color RT1's components are enabled.
    uint8 output1Enable;
    /// 4-bit mask of which color RT2's components are enabled.
    uint8 output2Enable;
    /// 4-bit mask of which color RT3's components are enabled.
    uint8 output3Enable;
    /// 4-bit mask of which color RT4's components are enabled.
    uint8 output4Enable;
    /// 4-bit mask of which color RT5's components are enabled.
    uint8 output5Enable;
    /// 4-bit mask of which color RT6's components are enabled.
    uint8 output6Enable;
    /// 4-bit mask of which color RT7's components are enabled.
    uint8 output7Enable;

    union
    {
        struct
        {
            uint8 output0Enable : 1;
            uint8 output1Enable : 1;
            uint8 output2Enable : 1;
            uint8 output3Enable : 1;
            uint8 output4Enable : 1;
            uint8 output5Enable : 1;
            uint8 output6Enable : 1;
            uint8 output7Enable : 1;
        };
        uint8 uAll;
    } hasEntry;
};

struct DbShaderControlMetadata
{
    /// Indicates shader's preference for which type of Z testing.
    uint8 zOrder;
    /// Forces Z exports to be either less than or greater than the source Z value.
    uint8 conservativeZExport;

    union
    {
        struct
        {
            /// Whether to use DB shader export's red channel as Z instead of the interpolated Z value.
            uint16 zExportEnable                : 1;
            /// Whether to use DB shader export's green[7:0] as the stencil test value.
            uint16 stencilTestValExportEnable   : 1;
            /// Whether to use DB shader export's green [15:8] as the stencil operation value.
            uint16 stencilOpValExportEnable     : 1;
            /// Whether the shader can kill pixels through texkill.
            uint16 killEnable                   : 1;
            /// Whether to use DB shader export's alpha channel as an independent alpha to mask operation.
            uint16 coverageToMaskEn             : 1;
            /// Whether to use DB shader export's blue channel as sample mask for pixel.
            uint16 maskExportEnable             : 1;
            /// Will execute the shader even if hierarchical Z or Stencil would kill the quad.
            uint16 execOnHierFail               : 1;
            /// Will execute the shader even if nothing uses the shader's color or depth exports.
            uint16 execOnNoop                   : 1;
            /// Whether to disable alpha to mask.
            uint16 alphaToMaskDisable           : 1;
            /// Whether the shader is declared after to run after depth by definition.
            uint16 depthBeforeShader            : 1;
            /// Enables primitive ordered pixel shader.
            uint16 primitiveOrderedPixelShader  : 1;
            /// If sample_coverage_ena is set, override the pre-culling sample coverage mask.
            uint16 preShaderDepthCoverageEnable : 1;
            uint16 reserved                     : 4;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 zExportEnable                : 1;
            uint16 stencilTestValExportEnable   : 1;
            uint16 stencilOpValExportEnable     : 1;
            uint16 zOrder                       : 1;
            uint16 killEnable                   : 1;
            uint16 coverageToMaskEn             : 1;
            uint16 maskExportEnable             : 1;
            uint16 execOnHierFail               : 1;
            uint16 execOnNoop                   : 1;
            uint16 alphaToMaskDisable           : 1;
            uint16 depthBeforeShader            : 1;
            uint16 conservativeZExport          : 1;
            uint16 primitiveOrderedPixelShader  : 1;
            uint16 preShaderDepthCoverageEnable : 1;
            uint16 reserved                     : 2;
        };
        uint16 uAll;
    } hasEntry;
};

struct SpiPsInControlMetadata
{
    /// Number of vertex parameters to interpolate.
    uint8 numInterps;
    /// Number of primitive parameters to interpolate.
    uint8 numPrimInterp;

    union
    {
        struct
        {
            /// Whether to generate gradients for ST coordinates.
            uint8 paramGen          : 1;
            /// Indicates that attribute data was written offchip.
            uint8 offchipParamEn    : 1;
            /// Indicates PS wave controller should wait until after a wave has completed before acting on a dealloc.
            uint8 latePcDealloc     : 1;
            /// Disable barycentric optimization which only transfers one set of I, J values when center equals
            /// centroid.
            uint8 bcOptimizeDisable : 1;
            uint8 reserved          : 4;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 numInterps        : 1;
            uint8 paramGen          : 1;
            uint8 offchipParamEn    : 1;
            uint8 latePcDealloc     : 1;
            uint8 numPrimInterp     : 1;
            uint8 bcOptimizeDisable : 1;
            uint8 reserved          : 2;
        };
        uint8 uAll;
    } hasEntry;
};

struct PaScShaderControlMetadata
{
    /// If next available quad falls outside tile aligned region of size specified here, the scan converter will force
    /// end of vector.
    uint8 waveBreakRegionSize;

    union
    {
        struct
        {
            /// Enables loading of POPS overlay term into an SGPR.
            uint8 loadCollisionWaveid    : 1;
            /// Enables loading of POPS intrawave collision term into an SGPR.
            uint8 loadIntrawaveCollision : 1;
            uint8 reserved               : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 loadCollisionWaveid    : 1;
            uint8 loadIntrawaveCollision : 1;
            uint8 waveBreakRegionSize    : 1;
            uint8 reserved               : 5;
        };
        uint8 uAll;
    } hasEntry;
};

struct SpiBarycCntlMetadata
{
    /// Per-pixel floating point position (at center, centroid, or iterated sample).
    uint8 posFloatLocation;

    union
    {
        struct
        {
            /// Whether to use the entire 32b value to determine front-facing.
            uint8 frontFaceAllBits : 1;
            uint8 placeholder0     : 1;
            uint8 reserved         : 6;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 posFloatLocation : 1;
            uint8 frontFaceAllBits : 1;
            uint8 placeholder0     : 1;
            uint8 placeholder1     : 1;
            uint8 placeholder2     : 1;
            uint8 reserved         : 3;
        };
        uint8 uAll;
    } hasEntry;
};

struct SpiPsInputEnaMetadata
{

    union
    {
        struct
        {
            /// Whether perspective gradients at sample are enabled.
            uint16 perspSampleEna    : 1;
            /// Whether perspective gradients at center are enabled.
            uint16 perspCenterEna    : 1;
            /// Whether perspective gradients at centroid are enabled.
            uint16 perspCentroidEna  : 1;
            /// Whether to provide I, J, 1/W to VGPR for pull model interpolation.
            uint16 perspPullModelEna : 1;
            /// Whether linear gradients at sample are enabled.
            uint16 linearSampleEna   : 1;
            /// Whether linear gradients at center are enabled.
            uint16 linearCenterEna   : 1;
            /// Whether linear gradients at centroid are enabled.
            uint16 linearCentroidEna : 1;
            /// Whether line stipple texture generation, per pixel calculation, and VGPR are loaded.
            uint16 lineStippleTexEna : 1;
            /// Whether per-pixel floating point X position is enabled.
            uint16 posXFloatEna      : 1;
            /// Whether per-pixel floating point Y position is enabled.
            uint16 posYFloatEna      : 1;
            /// Whether per-pixel floating point Z position is enabled.
            uint16 posZFloatEna      : 1;
            /// Whether per-pixel floating point W position is enabled.
            uint16 posWFloatEna      : 1;
            /// Whether front face is enabled.
            uint16 frontFaceEna      : 1;
            /// Whether ancillary data, including render target array index, iterated sample number, and primitive type
            /// are enabled.
            uint16 ancillaryEna      : 1;
            /// Whether sample coverage is enabled.
            uint16 sampleCoverageEna : 1;
            /// Whether per-pixel fixed point position is enabled.
            uint16 posFixedPtEna     : 1;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 perspSampleEna    : 1;
            uint16 perspCenterEna    : 1;
            uint16 perspCentroidEna  : 1;
            uint16 perspPullModelEna : 1;
            uint16 linearSampleEna   : 1;
            uint16 linearCenterEna   : 1;
            uint16 linearCentroidEna : 1;
            uint16 lineStippleTexEna : 1;
            uint16 posXFloatEna      : 1;
            uint16 posYFloatEna      : 1;
            uint16 posZFloatEna      : 1;
            uint16 posWFloatEna      : 1;
            uint16 frontFaceEna      : 1;
            uint16 ancillaryEna      : 1;
            uint16 sampleCoverageEna : 1;
            uint16 posFixedPtEna     : 1;
        };
        uint16 uAll;
    } hasEntry;
};

struct SpiPsInputAddrMetadata
{

    union
    {
        struct
        {
            /// Whether perspective gradients at sample are enabled.
            uint16 perspSampleEna    : 1;
            /// Whether perspective gradients at center are enabled.
            uint16 perspCenterEna    : 1;
            /// Whether perspective gradients at centroid are enabled.
            uint16 perspCentroidEna  : 1;
            /// Whether to provide I, J, 1/W to VGPR for pull model interpolation.
            uint16 perspPullModelEna : 1;
            /// Whether linear gradients at sample are enabled.
            uint16 linearSampleEna   : 1;
            /// Whether linear gradients at center are enabled.
            uint16 linearCenterEna   : 1;
            /// Whether linear gradients at centroid are enabled.
            uint16 linearCentroidEna : 1;
            /// Whether line stipple texture generation, per pixel calculation, and VGPR are loaded.
            uint16 lineStippleTexEna : 1;
            /// Whether per-pixel floating point X position is enabled.
            uint16 posXFloatEna      : 1;
            /// Whether per-pixel floating point Y position is enabled.
            uint16 posYFloatEna      : 1;
            /// Whether per-pixel floating point Z position is enabled.
            uint16 posZFloatEna      : 1;
            /// Whether per-pixel floating point W position is enabled.
            uint16 posWFloatEna      : 1;
            /// Whether front face is enabled.
            uint16 frontFaceEna      : 1;
            /// Whether ancillary data, including render target array index, iterated sample number, and primitive type
            /// are enabled.
            uint16 ancillaryEna      : 1;
            /// Whether sample coverage is enabled.
            uint16 sampleCoverageEna : 1;
            /// Whether per-pixel fixed point position is enabled.
            uint16 posFixedPtEna     : 1;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint16 perspSampleEna    : 1;
            uint16 perspCenterEna    : 1;
            uint16 perspCentroidEna  : 1;
            uint16 perspPullModelEna : 1;
            uint16 linearSampleEna   : 1;
            uint16 linearCenterEna   : 1;
            uint16 linearCentroidEna : 1;
            uint16 lineStippleTexEna : 1;
            uint16 posXFloatEna      : 1;
            uint16 posYFloatEna      : 1;
            uint16 posZFloatEna      : 1;
            uint16 posWFloatEna      : 1;
            uint16 frontFaceEna      : 1;
            uint16 ancillaryEna      : 1;
            uint16 sampleCoverageEna : 1;
            uint16 posFixedPtEna     : 1;
        };
        uint16 uAll;
    } hasEntry;
};

struct SpiShaderColFormatMetadata
{
    /// Specifies the format of color export 0.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_0ExportFormat;
    /// Specifies the format of color export 1.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_1ExportFormat;
    /// Specifies the format of color export 2.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_2ExportFormat;
    /// Specifies the format of color export 3.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_3ExportFormat;
    /// Specifies the format of color export 4.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_4ExportFormat;
    /// Specifies the format of color export 5.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_5ExportFormat;
    /// Specifies the format of color export 6.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_6ExportFormat;
    /// Specifies the format of color export 7.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8 col_7ExportFormat;

    union
    {
        struct
        {
            uint8 col_0ExportFormat : 1;
            uint8 col_1ExportFormat : 1;
            uint8 col_2ExportFormat : 1;
            uint8 col_3ExportFormat : 1;
            uint8 col_4ExportFormat : 1;
            uint8 col_5ExportFormat : 1;
            uint8 col_6ExportFormat : 1;
            uint8 col_7ExportFormat : 1;
        };
        uint8 uAll;
    } hasEntry;
};

/// Abstracted graphics-only register values.
struct GraphicsRegisterMetadata
{
    /// If the NGG culling data buffer is not already specified by a hardware stage's user_data_reg_map, then this field
    /// specified the register offset that is expected to point to the low 32-bits of address to the buffer.
    uint16                             nggCullingDataReg;
    /// How many LS VGPR components to load.
    uint8                              lsVgprCompCnt;
    /// How many ES VGPR components to load.
    uint8                              esVgprCompCnt;
    /// How many GS VGPR components to load.
    uint8                              gsVgprCompCnt;
    /// How many VS VGPR components to load.
    uint8                              vsVgprCompCnt;
    /// Extra LDS size to allocate, in bytes.
    uint32                             psExtraLdsSize;
    PaClClipCntlMetadata               paClClipCntl;
    PaClVteCntlMetadata                paClVteCntl;
    PaSuVtxCntlMetadata                paSuVtxCntl;
    VgtShaderStagesEnMetadata          vgtShaderStagesEn;
    VgtGsModeMetadata                  vgtGsMode;
    VgtTfParamMetadata                 vgtTfParam;
    VgtLsHsConfigMetadata              vgtLsHsConfig;
    IaMultiVgtParamMetadata            iaMultiVgtParam;
    SpiInterpControlMetadata           spiInterpControl;
    SpiPsInputCntlMetadata             spiPsInputCntl[32];
    /// Specifies a minimum tessellation level clamp that is applied to fetched tessellation factors. Values in the
    /// range (0.0, 64.0) are legal. If the incoming factor is a Nan, a negative number or Zero, it is not clamped
    /// against this value.
    float                              vgtHosMinTessLevel;
    /// Specifies a maximum tessellation level clamp that is applied to fetched tessellation factors. Values in the
    /// range (0.0, 64.0) are legal. If the incoming factor is a Nan, a negative number or Zero, it is not clamped
    /// against this value.
    float                              vgtHosMaxTessLevel;
#if PAL_BUILD_GFX11
    SpiShaderGsMeshletDimMetadata      spiShaderGsMeshletDim;
#endif
#if PAL_BUILD_GFX11
    SpiShaderGsMeshletExpAllocMetadata spiShaderGsMeshletExpAlloc;
#endif
    /// Maximum number of verts that can be emitted from a geometry shader.
    uint16                             vgtGsMaxVertOut;
    VgtGsInstanceCntMetadata           vgtGsInstanceCnt;
    /// EsGs ring item size in dwords.
    uint16                             vgtEsgsRingItemsize;
    VgtGsOutPrimTypeMetadata           vgtGsOutPrimType;
    /// Size of each vertex, in dwords, for the specified stream.
    uint16                             vgtGsVertItemsize[4];
    /// Offset of each stream (starting at index 1) from the base.
    uint16                             vgtGsvsRingOffset[3];
    /// Size of each primitive exported by the GS, in dwords.
    uint16                             vgtGsvsRingItemsize;
    /// Maximum number of ES vertices per GS thread.
    uint16                             vgtEsPerGs;
    /// Maximum number of GS prims per ES thread.
    uint16                             vgtGsPerEs;
    /// Maximum number of GS threads per VS thread.
    uint16                             vgtGsPerVs;
    /// Maximum number of prims exported per subgroup. Expected to be programmed to gs_inst_prims_per_subgrp *
    /// max_vert_out.
    uint16                             maxVertsPerSubgroup;
    /// Specifies the format of the primitive export.
    /// - 0 - None
    /// - 1 - 1 Component
    /// - 2 - 2 Components
    /// - 3 - 4 Components, Compressed
    /// - 4 - 4 Components
    uint8                              spiShaderIdxFormat;
    GeNggSubgrpCntlMetadata            geNggSubgrpCntl;
    VgtGsOnchipCntlMetadata            vgtGsOnchipCntl;
    PaClVsOutCntlMetadata              paClVsOutCntl;
    /// Specifies the format of the position exports coming out of the shader.
    /// - 0 - None
    /// - 1 - 1 Component
    /// - 2 - 2 Components
    /// - 3 - 4 Components, Compressed
    /// - 4 - 4 Components
    uint8                              spiShaderPosFormat[5];
    SpiVsOutConfigMetadata             spiVsOutConfig;
    VgtStrmoutConfigMetadata           vgtStrmoutConfig;
    VgtStrmoutBufferConfigMetadata     vgtStrmoutBufferConfig;
    CbShaderMaskMetadata               cbShaderMask;
    DbShaderControlMetadata            dbShaderControl;
    SpiPsInControlMetadata             spiPsInControl;
    /// Specifies how to populate the sample mask provided to the pixel shader.
    Abi::CoverageToShaderSel           aaCoverageToShaderSelect;
    PaScShaderControlMetadata          paScShaderControl;
    SpiBarycCntlMetadata               spiBarycCntl;
    SpiPsInputEnaMetadata              spiPsInputEna;
    SpiPsInputAddrMetadata             spiPsInputAddr;
    SpiShaderColFormatMetadata         spiShaderColFormat;
    /// Specifies the format of the depth export.
    /// - 0 - No exports done
    /// - 1 - Can be FP32 or SINT32/UINT32 R Component
    /// - 2 - Can be FP32 or SINT32/UINT32 GR components
    /// - 3 - Can be FP32 or SINT32/UINT32 AR Components
    /// - 4 - FP16 ABGR Components
    /// - 5 - UNORM16 ABGR Components
    /// - 6 - SNORM16 ABGR Components
    /// - 7 - UINT16 ABGR Components
    /// - 8 - SINT16 ABGR Components
    /// - 9 - Can be FP32 or SINT32/UINT32 ABGR Components
    uint8                              spiShaderZFormat;

    union
    {
        struct
        {
            /// Enables loading of threadgroup related info into SGPR.
            uint16 hsTgSizeEn                 : 1;
            /// Whether to enable loading of streamout base0 into SGPR.
            uint16 vsSoBase0En                : 1;
            /// Whether to enable loading of streamout base1 into SGPR.
            uint16 vsSoBase1En                : 1;
            /// Whether to enable loading of streamout base2 into SGPR.
            uint16 vsSoBase2En                : 1;
            /// Whether to enable loading of streamout base3 into SGPR.
            uint16 vsSoBase3En                : 1;
            /// Whether to enable loading of streamout buffer config into SGPR.
            uint16 vsStreamoutEn              : 1;
            /// Whether to enable loading of offchip parameter cache base into SGPR.
            uint16 vsPcBaseEn                 : 1;
            /// Whether to enable loading of the PS provoking vertex information into the SGPR.
            uint16 psLoadProvokingVtx         : 1;
            /// Whether the HW increments a per-wave count for PS and load the value into SGPR.
            uint16 psWaveCntEn                : 1;
            /// Enables per-sample (i.e. unique shader-computed value per sample) pixel shader execution
            uint16 psIterSample               : 1;
            /// Whether vertex reuse in the frontend is disabled.
            uint16 vgtReuseOff                : 1;
            /// Mesh shader uses linear dispatch from task shader thread group dimensions.
            uint16 meshLinearDispatchFromTask : 1;
            /// Whether the primitive export contains additional payload.
            uint16 vgtDrawPrimPayloadEn       : 1;
            /// Whether primitive ID generation is enabled.
            uint16 vgtPrimitiveIdEn           : 1;
            /// Whether to disable reuse on provoking vertex in NGG.
            uint16 nggDisableProvokReuse      : 1;
            uint16 reserved                   : 1;
        };
        uint16 uAll;
    } flags;

    union
    {
        struct
        {
            uint64 nggCullingDataReg          : 1;
            uint64 lsVgprCompCnt              : 1;
            uint64 hsTgSizeEn                 : 1;
            uint64 esVgprCompCnt              : 1;
            uint64 gsVgprCompCnt              : 1;
            uint64 vsVgprCompCnt              : 1;
            uint64 vsSoBase0En                : 1;
            uint64 vsSoBase1En                : 1;
            uint64 vsSoBase2En                : 1;
            uint64 vsSoBase3En                : 1;
            uint64 vsStreamoutEn              : 1;
            uint64 vsPcBaseEn                 : 1;
            uint64 psLoadProvokingVtx         : 1;
            uint64 psWaveCntEn                : 1;
            uint64 psExtraLdsSize             : 1;
            uint64 paClClipCntl               : 1;
            uint64 paClVteCntl                : 1;
            uint64 paSuVtxCntl                : 1;
            uint64 psIterSample               : 1;
            uint64 vgtShaderStagesEn          : 1;
            uint64 vgtReuseOff                : 1;
            uint64 vgtGsMode                  : 1;
            uint64 vgtTfParam                 : 1;
            uint64 vgtLsHsConfig              : 1;
            uint64 iaMultiVgtParam            : 1;
            uint64 spiInterpControl           : 1;
            uint64 spiPsInputCntl             : 1;
            uint64 vgtHosMinTessLevel         : 1;
            uint64 vgtHosMaxTessLevel         : 1;
#if PAL_BUILD_GFX11
            uint64 spiShaderGsMeshletDim      : 1;
            uint64 spiShaderGsMeshletExpAlloc : 1;
#else
            uint64 placeholder0               : 1;
            uint64 placeholder1               : 1;
#endif
            uint64 meshLinearDispatchFromTask : 1;
            uint64 vgtGsMaxVertOut            : 1;
            uint64 vgtGsInstanceCnt           : 1;
            uint64 vgtEsgsRingItemsize        : 1;
            uint64 vgtDrawPrimPayloadEn       : 1;
            uint64 vgtGsOutPrimType           : 1;
            uint64 vgtGsVertItemsize          : 1;
            uint64 vgtGsvsRingOffset          : 1;
            uint64 vgtGsvsRingItemsize        : 1;
            uint64 vgtEsPerGs                 : 1;
            uint64 vgtGsPerEs                 : 1;
            uint64 vgtGsPerVs                 : 1;
            uint64 maxVertsPerSubgroup        : 1;
            uint64 spiShaderIdxFormat         : 1;
            uint64 geNggSubgrpCntl            : 1;
            uint64 vgtGsOnchipCntl            : 1;
            uint64 paClVsOutCntl              : 1;
            uint64 spiShaderPosFormat         : 1;
            uint64 spiVsOutConfig             : 1;
            uint64 vgtPrimitiveIdEn           : 1;
            uint64 nggDisableProvokReuse      : 1;
            uint64 vgtStrmoutConfig           : 1;
            uint64 vgtStrmoutBufferConfig     : 1;
            uint64 cbShaderMask               : 1;
            uint64 dbShaderControl            : 1;
            uint64 spiPsInControl             : 1;
            uint64 aaCoverageToShaderSelect   : 1;
            uint64 paScShaderControl          : 1;
            uint64 spiBarycCntl               : 1;
            uint64 spiPsInputEna              : 1;
            uint64 spiPsInputAddr             : 1;
            uint64 spiShaderColFormat         : 1;
            uint64 spiShaderZFormat           : 1;
            uint64 placeholder2               : 1;
            uint64 reserved                   : 63;
        };
        uint64 uAll[2];
    } hasEntry;
};

/// Abstracted compute-only register values.
struct ComputeRegisterMetadata
{

    /// Specifies how many thread_id_in_group terms to write into VGPR.
    /// 0 = X, 1 = XY, 2 = XYZ
    uint8  tidigCompCnt;

    union
    {
        struct
        {
            /// Enables loading of TGID.X into SGPR.
            uint8 tgidXEn       : 1;
            /// Enables loading of TGID.Y into SGPR.
            uint8 tgidYEn       : 1;
            /// Enables loading of TGID.Z into SGPR.
            uint8 tgidZEn       : 1;
            /// Enables loading of threadgroup related info into SGPR.
            uint8 tgSizeEn      : 1;
            uint8 placeholder0  : 1;
            uint8 reserved      : 3;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint8 tgidXEn       : 1;
            uint8 tgidYEn       : 1;
            uint8 tgidZEn       : 1;
            uint8 tgSizeEn      : 1;
            uint8 placeholder0  : 1;
            uint8 placeholder1  : 1;
            uint8 placeholder2  : 1;
            uint8 tidigCompCnt  : 1;
        };
        uint8 uAll;
    } hasEntry;
};

/// Per-pipeline metadata.
struct PipelineMetadata
{
    /// Source name of the pipeline.
    StringViewType                  name;
    /// Pipeline type, e.g. VsPs.
    Abi::PipelineType               type;
    /// Internal compiler hash for this pipeline.
    /// Lower 64 bits is the "stable" portion of the hash, used for e.g. shader replacement lookup.
    /// Upper 64 bits is the "unique" portion of the hash, used for e.g. pipeline cache lookup.
    uint64                          internalPipelineHash[2];
    /// 64-bit hash of the resource mapping used when compiling this pipeline.
    uint64                          resourceHash;
    /// Per-API shader metadata.
    ShaderMetadata                  shader[static_cast<uint32>(Abi::ApiShaderType::Count)];
    /// Per-hardware stage metadata.
    HardwareStageMetadata           hardwareStage[static_cast<uint32>(Abi::HardwareStage::Count)];
    /// Per-shader function metadata (offset in bytes into the msgpack blob to map of map). See :ref:`amdgpu-amdpal-
    /// code-object-shader-function-map-table`
    MsgPackOffset                   shaderFunctions;
    /// <Deprecated> Hardware register configuration (offset in bytes into the msgpack blob to map).
    MsgPackOffset                   registers;

    /// Number of user data entries accessed by this pipeline.
    uint32                          userDataLimit;
    /// The user data spill threshold.  0xFFFF for NoUserDataSpilling.
    uint32                          spillThreshold;
    /// Size in bytes of LDS space used internally for handling data-passing between the ES and GS shader stages. This
    /// can be zero if the data is passed using off-chip buffers. This value should be used to program all user-SGPRs
    /// which have been marked with "UserDataMapping::EsGsLdsSize" (typically only the GS and VS HW stages will ever
    /// have a user-SGPR so marked).
    uint32                          esGsLdsSize;
    /// Explicit maximum subgroup size for NGG shaders (maximum number of threads in a subgroup).
    uint32                          nggSubgroupSize;
    /// Graphics only. Number of PS interpolants.
    uint32                          numInterpolants;
    /// Max mesh shader scratch memory used.
    uint32                          meshScratchMemorySize;

    /// Pixel shader input semantic info.
    PsInputSemanticMetadata         psInputSemantic[32];
    /// Output semantic info in pre-raster stage which is before pixel shader.
    PrerasterOutputSemanticMetadata prerasterOutputSemantic[32];
    /// Name of the client graphics API.
    char                            api[16];
    /// Graphics API shader create info binary blob. Can be defined by the driver using the compiler if they want to be
    /// able to correlate API-specific information used during creation at a later time.
    BinaryData                      apiCreateInfo;
    /// Dword stride between vertices in given stream-out-buffer.
    uint16                          streamoutVertexStrides[4];
    /// Abstracted graphics-only register values.
    GraphicsRegisterMetadata        graphicsRegister;
    /// Abstracted compute-only register values.
    ComputeRegisterMetadata         computeRegister;

    union
    {
        struct
        {
            /// Indicates whether or not the pipeline uses the viewport array index feature. Pipelines which use this
            /// feature can render into all 16 viewports, whereas pipelines which do not use it are restricted to
            /// viewport #0.
            uint8 usesViewportArrayIndex : 1;
            /// Whether the GS outputs lines (needed by client for MSAA dispatch)
            uint8 gsOutputsLines         : 1;
            /// Set if there may be a PS dummy export that actually writes to an MRT, including the case of the compiler
            /// adding a null PS. The client driver or PAL may need to disable binding of MRTs for a pipeline where this
            /// is set.
            uint8 psDummyExport          : 1;
            /// Set if a PS is using sample mask.
            uint8 psSampleMask           : 1;
            uint8 reserved               : 4;
        };
        uint8 uAll;
    } flags;

    union
    {
        struct
        {
            uint32 name                    : 1;
            uint32 type                    : 1;
            uint32 internalPipelineHash    : 1;
            uint32 resourceHash            : 1;
            uint32 shader                  : 1;
            uint32 hardwareStage           : 1;
            uint32 shaderFunctions         : 1;
            uint32 registers               : 1;
            uint32 placeholder0            : 1;
            uint32 userDataLimit           : 1;
            uint32 spillThreshold          : 1;
            uint32 usesViewportArrayIndex  : 1;
            uint32 esGsLdsSize             : 1;
            uint32 nggSubgroupSize         : 1;
            uint32 numInterpolants         : 1;
            uint32 meshScratchMemorySize   : 1;
            uint32 placeholder1            : 1;
            uint32 psInputSemantic         : 1;
            uint32 prerasterOutputSemantic : 1;
            uint32 api                     : 1;
            uint32 apiCreateInfo           : 1;
            uint32 gsOutputsLines          : 1;
            uint32 psDummyExport           : 1;
            uint32 psSampleMask            : 1;
            uint32 streamoutVertexStrides  : 1;
            uint32 graphicsRegister        : 1;
            uint32 computeRegister         : 1;
            uint32 reserved                : 5;
        };
        uint32 uAll;
    } hasEntry;
};

/// PAL code object metadata.
struct CodeObjectMetadata
{
    /// PAL code object metadata (major, minor) version.
    uint32                version[2];
    /// Per-pipeline metadata.
    PipelineMetadata      pipeline;

    union
    {
        struct
        {
            uint8 version       : 1;
            uint8 pipeline      : 1;
            uint8 placeholder0  : 1;
            uint8 reserved      : 5;
        };
        uint8 uAll;
    } hasEntry;
};

namespace CodeObjectMetadataKey
{
    static constexpr char Version[]        = "amdpal.version";
    static constexpr char Pipelines[]      = "amdpal.pipelines";

};

namespace PipelineMetadataKey
{
    static constexpr char Name[]                    = ".name";
    static constexpr char Type[]                    = ".type";
    static constexpr char InternalPipelineHash[]    = ".internal_pipeline_hash";
    static constexpr char ResourceHash[]            = ".resource_hash";
    static constexpr char Shaders[]                 = ".shaders";
    static constexpr char HardwareStages[]          = ".hardware_stages";
    static constexpr char ShaderFunctions[]         = ".shader_functions";
    static constexpr char Registers[]               = ".registers";

    static constexpr char UserDataLimit[]           = ".user_data_limit";
    static constexpr char SpillThreshold[]          = ".spill_threshold";
    static constexpr char UsesViewportArrayIndex[]  = ".uses_viewport_array_index";
    static constexpr char EsGsLdsSize[]             = ".es_gs_lds_size";
    static constexpr char NggSubgroupSize[]         = ".nggSubgroupSize";
    static constexpr char NumInterpolants[]         = ".num_interpolants";
    static constexpr char MeshScratchMemorySize[]   = ".mesh_scratch_memory_size";

    static constexpr char PsInputSemantic[]         = ".ps_input_semantic";
    static constexpr char PrerasterOutputSemantic[] = ".preraster_output_semantic";
    static constexpr char Api[]                     = ".api";
    static constexpr char ApiCreateInfo[]           = ".api_create_info";
    static constexpr char GsOutputsLines[]          = ".gs_outputs_lines";
    static constexpr char PsDummyExport[]           = ".ps_dummy_export";
    static constexpr char PsSampleMask[]            = ".ps_sample_mask";
    static constexpr char StreamoutVertexStrides[]  = ".streamout_vertex_strides";
    static constexpr char GraphicsRegisters[]       = ".graphics_registers";
    static constexpr char ComputeRegisters[]        = ".compute_registers";
};

namespace ComputeRegisterMetadataKey
{
    static constexpr char TgidXEn[]       = ".tgid_x_en";
    static constexpr char TgidYEn[]       = ".tgid_y_en";
    static constexpr char TgidZEn[]       = ".tgid_z_en";
    static constexpr char TgSizeEn[]      = ".tg_size_en";

    static constexpr char TidigCompCnt[]  = ".tidig_comp_cnt";
};

namespace GraphicsRegisterMetadataKey
{
    static constexpr char NggCullingDataReg[]          = ".ngg_culling_data_reg";
    static constexpr char LsVgprCompCnt[]              = ".ls_vgpr_comp_cnt";
    static constexpr char HsTgSizeEn[]                 = ".hs_tg_size_en";
    static constexpr char EsVgprCompCnt[]              = ".es_vgpr_comp_cnt";
    static constexpr char GsVgprCompCnt[]              = ".gs_vgpr_comp_cnt";
    static constexpr char VsVgprCompCnt[]              = ".vs_vgpr_comp_cnt";
    static constexpr char VsSoBase0En[]                = ".vs_so_base0_en";
    static constexpr char VsSoBase1En[]                = ".vs_so_base1_en";
    static constexpr char VsSoBase2En[]                = ".vs_so_base2_en";
    static constexpr char VsSoBase3En[]                = ".vs_so_base3_en";
    static constexpr char VsStreamoutEn[]              = ".vs_streamout_en";
    static constexpr char VsPcBaseEn[]                 = ".vs_pc_base_en";
    static constexpr char PsLoadProvokingVtx[]         = ".ps_load_provoking_vtx";
    static constexpr char PsWaveCntEn[]                = ".ps_wave_cnt_en";
    static constexpr char PsExtraLdsSize[]             = ".ps_extra_lds_size";
    static constexpr char PaClClipCntl[]               = ".pa_cl_clip_cntl";
    static constexpr char PaClVteCntl[]                = ".pa_cl_vte_cntl";
    static constexpr char PaSuVtxCntl[]                = ".pa_su_vtx_cntl";
    static constexpr char PsIterSample[]               = ".ps_iter_sample";
    static constexpr char VgtShaderStagesEn[]          = ".vgt_shader_stages_en";
    static constexpr char VgtReuseOff[]                = ".vgt_reuse_off";
    static constexpr char VgtGsMode[]                  = ".vgt_gs_mode";
    static constexpr char VgtTfParam[]                 = ".vgt_tf_param";
    static constexpr char VgtLsHsConfig[]              = ".vgt_ls_hs_config";
    static constexpr char IaMultiVgtParam[]            = ".ia_multi_vgt_param";
    static constexpr char SpiInterpControl[]           = ".spi_interp_control";
    static constexpr char SpiPsInputCntl[]             = ".spi_ps_input_cntl";
    static constexpr char VgtHosMinTessLevel[]         = ".vgt_hos_min_tess_level";
    static constexpr char VgtHosMaxTessLevel[]         = ".vgt_hos_max_tess_level";
#if PAL_BUILD_GFX11
    static constexpr char SpiShaderGsMeshletDim[]      = ".spi_shader_gs_meshlet_dim";
#endif

#if PAL_BUILD_GFX11
    static constexpr char SpiShaderGsMeshletExpAlloc[] = ".spi_shader_gs_meshlet_exp_alloc";
#endif

    static constexpr char MeshLinearDispatchFromTask[] = ".mesh_linear_dispatch_from_task";
    static constexpr char VgtGsMaxVertOut[]            = ".vgt_gs_max_vert_out";
    static constexpr char VgtGsInstanceCnt[]           = ".vgt_gs_instance_cnt";
    static constexpr char VgtEsgsRingItemsize[]        = ".vgt_esgs_ring_itemsize";
    static constexpr char VgtDrawPrimPayloadEn[]       = ".vgt_draw_prim_payload_en";
    static constexpr char VgtGsOutPrimType[]           = ".vgt_gs_out_prim_type";
    static constexpr char VgtGsVertItemsize[]          = ".vgt_gs_vert_itemsize";
    static constexpr char VgtGsvsRingOffset[]          = ".vgt_gsvs_ring_offset";
    static constexpr char VgtGsvsRingItemsize[]        = ".vgt_gsvs_ring_itemsize";
    static constexpr char VgtEsPerGs[]                 = ".vgt_es_per_gs";
    static constexpr char VgtGsPerEs[]                 = ".vgt_gs_per_es";
    static constexpr char VgtGsPerVs[]                 = ".vgt_gs_per_vs";
    static constexpr char MaxVertsPerSubgroup[]        = ".max_verts_per_subgroup";
    static constexpr char SpiShaderIdxFormat[]         = ".spi_shader_idx_format";
    static constexpr char GeNggSubgrpCntl[]            = ".ge_ngg_subgrp_cntl";
    static constexpr char VgtGsOnchipCntl[]            = ".vgt_gs_onchip_cntl";
    static constexpr char PaClVsOutCntl[]              = ".pa_cl_vs_out_cntl";
    static constexpr char SpiShaderPosFormat[]         = ".spi_shader_pos_format";
    static constexpr char SpiVsOutConfig[]             = ".spi_vs_out_config";
    static constexpr char VgtPrimitiveIdEn[]           = ".vgt_primitive_id_en";
    static constexpr char NggDisableProvokReuse[]      = ".ngg_disable_provok_reuse";
    static constexpr char VgtStrmoutConfig[]           = ".vgt_strmout_config";
    static constexpr char VgtStrmoutBufferConfig[]     = ".vgt_strmout_buffer_config";
    static constexpr char CbShaderMask[]               = ".cb_shader_mask";
    static constexpr char DbShaderControl[]            = ".db_shader_control";
    static constexpr char SpiPsInControl[]             = ".spi_ps_in_control";
    static constexpr char AaCoverageToShaderSelect[]   = ".aa_coverage_to_shader_select";
    static constexpr char PaScShaderControl[]          = ".pa_sc_shader_control";
    static constexpr char SpiBarycCntl[]               = ".spi_baryc_cntl";
    static constexpr char SpiPsInputEna[]              = ".spi_ps_input_ena";
    static constexpr char SpiPsInputAddr[]             = ".spi_ps_input_addr";
    static constexpr char SpiShaderColFormat[]         = ".spi_shader_col_format";
    static constexpr char SpiShaderZFormat[]           = ".spi_shader_z_format";

};

namespace SpiShaderColFormatMetadataKey
{
    static constexpr char Col_0ExportFormat[] = ".col_0_export_format";
    static constexpr char Col_1ExportFormat[] = ".col_1_export_format";
    static constexpr char Col_2ExportFormat[] = ".col_2_export_format";
    static constexpr char Col_3ExportFormat[] = ".col_3_export_format";
    static constexpr char Col_4ExportFormat[] = ".col_4_export_format";
    static constexpr char Col_5ExportFormat[] = ".col_5_export_format";
    static constexpr char Col_6ExportFormat[] = ".col_6_export_format";
    static constexpr char Col_7ExportFormat[] = ".col_7_export_format";
};

namespace SpiPsInputAddrMetadataKey
{
    static constexpr char PerspSampleEna[]    = ".persp_sample_ena";
    static constexpr char PerspCenterEna[]    = ".persp_center_ena";
    static constexpr char PerspCentroidEna[]  = ".persp_centroid_ena";
    static constexpr char PerspPullModelEna[] = ".persp_pull_model_ena";
    static constexpr char LinearSampleEna[]   = ".linear_sample_ena";
    static constexpr char LinearCenterEna[]   = ".linear_center_ena";
    static constexpr char LinearCentroidEna[] = ".linear_centroid_ena";
    static constexpr char LineStippleTexEna[] = ".line_stipple_tex_ena";
    static constexpr char PosXFloatEna[]      = ".pos_x_float_ena";
    static constexpr char PosYFloatEna[]      = ".pos_y_float_ena";
    static constexpr char PosZFloatEna[]      = ".pos_z_float_ena";
    static constexpr char PosWFloatEna[]      = ".pos_w_float_ena";
    static constexpr char FrontFaceEna[]      = ".front_face_ena";
    static constexpr char AncillaryEna[]      = ".ancillary_ena";
    static constexpr char SampleCoverageEna[] = ".sample_coverage_ena";
    static constexpr char PosFixedPtEna[]     = ".pos_fixed_pt_ena";
};

namespace SpiPsInputEnaMetadataKey
{
    static constexpr char PerspSampleEna[]    = ".persp_sample_ena";
    static constexpr char PerspCenterEna[]    = ".persp_center_ena";
    static constexpr char PerspCentroidEna[]  = ".persp_centroid_ena";
    static constexpr char PerspPullModelEna[] = ".persp_pull_model_ena";
    static constexpr char LinearSampleEna[]   = ".linear_sample_ena";
    static constexpr char LinearCenterEna[]   = ".linear_center_ena";
    static constexpr char LinearCentroidEna[] = ".linear_centroid_ena";
    static constexpr char LineStippleTexEna[] = ".line_stipple_tex_ena";
    static constexpr char PosXFloatEna[]      = ".pos_x_float_ena";
    static constexpr char PosYFloatEna[]      = ".pos_y_float_ena";
    static constexpr char PosZFloatEna[]      = ".pos_z_float_ena";
    static constexpr char PosWFloatEna[]      = ".pos_w_float_ena";
    static constexpr char FrontFaceEna[]      = ".front_face_ena";
    static constexpr char AncillaryEna[]      = ".ancillary_ena";
    static constexpr char SampleCoverageEna[] = ".sample_coverage_ena";
    static constexpr char PosFixedPtEna[]     = ".pos_fixed_pt_ena";
};

namespace SpiBarycCntlMetadataKey
{
    static constexpr char PosFloatLocation[] = ".pos_float_location";
    static constexpr char FrontFaceAllBits[] = ".front_face_all_bits";

};

namespace PaScShaderControlMetadataKey
{
    static constexpr char LoadCollisionWaveid[]    = ".load_collision_waveid";
    static constexpr char LoadIntrawaveCollision[] = ".load_intrawave_collision";
    static constexpr char WaveBreakRegionSize[]    = ".wave_break_region_size";
};

namespace SpiPsInControlMetadataKey
{
    static constexpr char NumInterps[]        = ".num_interps";
    static constexpr char ParamGen[]          = ".param_gen";
    static constexpr char OffchipParamEn[]    = ".offchip_param_en";
    static constexpr char LatePcDealloc[]     = ".late_pc_dealloc";
    static constexpr char NumPrimInterp[]     = ".num_prim_interp";
    static constexpr char BcOptimizeDisable[] = ".bc_optimize_disable";
};

namespace DbShaderControlMetadataKey
{
    static constexpr char ZExportEnable[]                = ".z_export_enable";
    static constexpr char StencilTestValExportEnable[]   = ".stencil_test_val_export_enable";
    static constexpr char StencilOpValExportEnable[]     = ".stencil_op_val_export_enable";
    static constexpr char ZOrder[]                       = ".z_order";
    static constexpr char KillEnable[]                   = ".kill_enable";
    static constexpr char CoverageToMaskEn[]             = ".coverage_to_mask_en";
    static constexpr char MaskExportEnable[]             = ".mask_export_enable";
    static constexpr char ExecOnHierFail[]               = ".exec_on_hier_fail";
    static constexpr char ExecOnNoop[]                   = ".exec_on_noop";
    static constexpr char AlphaToMaskDisable[]           = ".alpha_to_mask_disable";
    static constexpr char DepthBeforeShader[]            = ".depth_before_shader";
    static constexpr char ConservativeZExport[]          = ".conservative_z_export";
    static constexpr char PrimitiveOrderedPixelShader[]  = ".primitive_ordered_pixel_shader";
    static constexpr char PreShaderDepthCoverageEnable[] = ".pre_shader_depth_coverage_enable";
};

namespace CbShaderMaskMetadataKey
{
    static constexpr char Output0Enable[] = ".output0_enable";
    static constexpr char Output1Enable[] = ".output1_enable";
    static constexpr char Output2Enable[] = ".output2_enable";
    static constexpr char Output3Enable[] = ".output3_enable";
    static constexpr char Output4Enable[] = ".output4_enable";
    static constexpr char Output5Enable[] = ".output5_enable";
    static constexpr char Output6Enable[] = ".output6_enable";
    static constexpr char Output7Enable[] = ".output7_enable";
};

namespace VgtStrmoutBufferConfigMetadataKey
{
    static constexpr char Stream_0BufferEn[] = ".stream_0_buffer_en";
    static constexpr char Stream_1BufferEn[] = ".stream_1_buffer_en";
    static constexpr char Stream_2BufferEn[] = ".stream_2_buffer_en";
    static constexpr char Stream_3BufferEn[] = ".stream_3_buffer_en";
};

namespace VgtStrmoutConfigMetadataKey
{
    static constexpr char Streamout_0En[]     = ".streamout_0_en";
    static constexpr char Streamout_1En[]     = ".streamout_1_en";
    static constexpr char Streamout_2En[]     = ".streamout_2_en";
    static constexpr char Streamout_3En[]     = ".streamout_3_en";
    static constexpr char RastStream[]        = ".rast_stream";
    static constexpr char PrimsNeededCntEn[]  = ".prims_needed_cnt_en";
    static constexpr char RastStreamMask[]    = ".rast_stream_mask";
    static constexpr char UseRastStreamMask[] = ".use_rast_stream_mask";
};

namespace SpiVsOutConfigMetadataKey
{
    static constexpr char NoPcExport[]      = ".no_pc_export";
    static constexpr char VsExportCount[]   = ".vs_export_count";
    static constexpr char PrimExportCount[] = ".prim_export_count";
};

namespace PaClVsOutCntlMetadataKey
{
    static constexpr char ClipDistEna_0[]          = ".clip_dist_ena_0";
    static constexpr char ClipDistEna_1[]          = ".clip_dist_ena_1";
    static constexpr char ClipDistEna_2[]          = ".clip_dist_ena_2";
    static constexpr char ClipDistEna_3[]          = ".clip_dist_ena_3";
    static constexpr char ClipDistEna_4[]          = ".clip_dist_ena_4";
    static constexpr char ClipDistEna_5[]          = ".clip_dist_ena_5";
    static constexpr char ClipDistEna_6[]          = ".clip_dist_ena_6";
    static constexpr char ClipDistEna_7[]          = ".clip_dist_ena_7";
    static constexpr char CullDistEna_0[]          = ".cull_dist_ena_0";
    static constexpr char CullDistEna_1[]          = ".cull_dist_ena_1";
    static constexpr char CullDistEna_2[]          = ".cull_dist_ena_2";
    static constexpr char CullDistEna_3[]          = ".cull_dist_ena_3";
    static constexpr char CullDistEna_4[]          = ".cull_dist_ena_4";
    static constexpr char CullDistEna_5[]          = ".cull_dist_ena_5";
    static constexpr char CullDistEna_6[]          = ".cull_dist_ena_6";
    static constexpr char CullDistEna_7[]          = ".cull_dist_ena_7";
    static constexpr char UseVtxPointSize[]        = ".use_vtx_point_size";
    static constexpr char UseVtxEdgeFlag[]         = ".use_vtx_edge_flag";
    static constexpr char UseVtxRenderTargetIndx[] = ".use_vtx_render_target_indx";
    static constexpr char UseVtxViewportIndx[]     = ".use_vtx_viewport_indx";
    static constexpr char UseVtxKillFlag[]         = ".use_vtx_kill_flag";
    static constexpr char VsOutMiscVecEna[]        = ".vs_out_misc_vec_ena";
    static constexpr char VsOutCcDist0VecEna[]     = ".vs_out_cc_dist0_vec_ena";
    static constexpr char VsOutCcDist1VecEna[]     = ".vs_out_cc_dist1_vec_ena";
    static constexpr char VsOutMiscSideBusEna[]    = ".vs_out_misc_side_bus_ena";
    static constexpr char UseVtxLineWidth[]        = ".use_vtx_line_width";
    static constexpr char UseVtxVrsRate[]          = ".use_vtx_vrs_rate";
    static constexpr char BypassVtxRateCombiner[]  = ".bypass_vtx_rate_combiner";
    static constexpr char BypassPrimRateCombiner[] = ".bypass_prim_rate_combiner";
    static constexpr char UseVtxGsCutFlag[]        = ".use_vtx_gs_cut_flag";
#if PAL_BUILD_GFX11
    static constexpr char UseVtxFsrSelect[]        = ".use_vtx_fsr_select";
#endif

};

namespace VgtGsOnchipCntlMetadataKey
{
    static constexpr char EsVertsPerSubgroup[]   = ".es_verts_per_subgroup";
    static constexpr char GsPrimsPerSubgroup[]   = ".gs_prims_per_subgroup";
    static constexpr char GsInstPrimsPerSubgrp[] = ".gs_inst_prims_per_subgrp";
};

namespace GeNggSubgrpCntlMetadataKey
{
    static constexpr char PrimAmpFactor[]      = ".prim_amp_factor";
    static constexpr char ThreadsPerSubgroup[] = ".threads_per_subgroup";
};

namespace VgtGsOutPrimTypeMetadataKey
{
    static constexpr char OutprimType[]         = ".outprim_type";
    static constexpr char OutprimType_1[]       = ".outprim_type_1";
    static constexpr char OutprimType_2[]       = ".outprim_type_2";
    static constexpr char OutprimType_3[]       = ".outprim_type_3";
    static constexpr char UniqueTypePerStream[] = ".unique_type_per_stream";
};

namespace VgtGsInstanceCntMetadataKey
{
    static constexpr char Enable[]                    = ".enable";
    static constexpr char Count[]                     = ".count";
    static constexpr char EnMaxVertOutPerGsInstance[] = ".en_max_vert_out_per_gs_instance";
};

#if PAL_BUILD_GFX11
namespace SpiShaderGsMeshletExpAllocMetadataKey
{
    static constexpr char MaxExpVerts[] = ".max_exp_verts";
    static constexpr char MaxExpPrims[] = ".max_exp_prims";
};
#endif

#if PAL_BUILD_GFX11
namespace SpiShaderGsMeshletDimMetadataKey
{
    static constexpr char NumThreadX[]      = ".num_thread_x";
    static constexpr char NumThreadY[]      = ".num_thread_y";
    static constexpr char NumThreadZ[]      = ".num_thread_z";
    static constexpr char ThreadgroupSize[] = ".threadgroup_size";
};
#endif

namespace SpiPsInputCntlMetadataKey
{
    static constexpr char Offset[]         = ".offset";
    static constexpr char DefaultVal[]     = ".default_val";
    static constexpr char FlatShade[]      = ".flat_shade";
    static constexpr char CylWrap[]        = ".cyl_wrap";
    static constexpr char PtSpriteTex[]    = ".pt_sprite_tex";
    static constexpr char Fp16InterpMode[] = ".fp16_interp_mode";
    static constexpr char Attr0Valid[]     = ".attr0_valid";
    static constexpr char Attr1Valid[]     = ".attr1_valid";
    static constexpr char RotatePcPtr[]    = ".rotate_pc_ptr";
#if PAL_BUILD_GFX11
    static constexpr char PrimAttr[]       = ".prim_attr";
#endif

};

namespace SpiInterpControlMetadataKey
{
    static constexpr char PointSpriteEna[]       = ".point_sprite_ena";
    static constexpr char PointSpriteOverrideX[] = ".point_sprite_override_x";
    static constexpr char PointSpriteOverrideY[] = ".point_sprite_override_y";
    static constexpr char PointSpriteOverrideZ[] = ".point_sprite_override_z";
    static constexpr char PointSpriteOverrideW[] = ".point_sprite_override_w";
};

namespace IaMultiVgtParamMetadataKey
{
    static constexpr char PrimgroupSize[]   = ".primgroup_size";
    static constexpr char PartialVsWaveOn[] = ".partial_vs_wave_on";
    static constexpr char PartialEsWaveOn[] = ".partial_es_wave_on";
    static constexpr char SwitchOnEop[]     = ".switch_on_eop";
    static constexpr char SwitchOnEoi[]     = ".switch_on_eoi";
};

namespace VgtLsHsConfigMetadataKey
{
    static constexpr char NumPatches[]    = ".num_patches";
    static constexpr char HsNumInputCp[]  = ".hs_num_input_cp";
    static constexpr char HsNumOutputCp[] = ".hs_num_output_cp";
};

namespace VgtTfParamMetadataKey
{
    static constexpr char Type[]              = ".type";
    static constexpr char Partitioning[]      = ".partitioning";
    static constexpr char Topology[]          = ".topology";
    static constexpr char DisableDonuts[]     = ".disable_donuts";
    static constexpr char NumDsWavesPerSimd[] = ".num_ds_waves_per_simd";
    static constexpr char DistributionMode[]  = ".distribution_mode";
};

namespace VgtGsModeMetadataKey
{
    static constexpr char Mode[]            = ".mode";
    static constexpr char Onchip[]          = ".onchip";
    static constexpr char EsWriteOptimize[] = ".es_write_optimize";
    static constexpr char GsWriteOptimize[] = ".gs_write_optimize";
    static constexpr char CutMode[]         = ".cut_mode";
};

namespace VgtShaderStagesEnMetadataKey
{
    static constexpr char LsStageEn[]            = ".ls_stage_en";
    static constexpr char HsStageEn[]            = ".hs_stage_en";
    static constexpr char EsStageEn[]            = ".es_stage_en";
    static constexpr char GsStageEn[]            = ".gs_stage_en";
    static constexpr char VsStageEn[]            = ".vs_stage_en";
    static constexpr char DynamicHs[]            = ".dynamic_hs";
    static constexpr char MaxPrimgroupInWave[]   = ".max_primgroup_in_wave";
    static constexpr char PrimgenEn[]            = ".primgen_en";
    static constexpr char OrderedIdMode[]        = ".ordered_id_mode";
    static constexpr char NggWaveIdEn[]          = ".ngg_wave_id_en";
    static constexpr char GsFastLaunch[]         = ".gs_fast_launch";
    static constexpr char PrimgenPassthruEn[]    = ".primgen_passthru_en";
#if PAL_BUILD_GFX11
    static constexpr char PrimgenPassthruNoMsg[] = ".primgen_passthru_no_msg";
#endif

};

namespace PaSuVtxCntlMetadataKey
{
    static constexpr char PixCenter[] = ".pix_center";
    static constexpr char RoundMode[] = ".round_mode";
    static constexpr char QuantMode[] = ".quant_mode";
};

namespace PaClVteCntlMetadataKey
{
    static constexpr char VtxXyFmt[]   = ".vtx_xy_fmt";
    static constexpr char VtxZFmt[]    = ".vtx_z_fmt";
    static constexpr char XScaleEna[]  = ".x_scale_ena";
    static constexpr char XOffsetEna[] = ".x_offset_ena";
    static constexpr char YScaleEna[]  = ".y_scale_ena";
    static constexpr char YOffsetEna[] = ".y_offset_ena";
    static constexpr char ZScaleEna[]  = ".z_scale_ena";
    static constexpr char ZOffsetEna[] = ".z_offset_ena";
    static constexpr char VtxW0Fmt[]   = ".vtx_w0_fmt";
};

namespace PaClClipCntlMetadataKey
{
    static constexpr char UserClipPlane0Ena[]   = ".user_clip_plane0_ena";
    static constexpr char UserClipPlane1Ena[]   = ".user_clip_plane1_ena";
    static constexpr char UserClipPlane2Ena[]   = ".user_clip_plane2_ena";
    static constexpr char UserClipPlane3Ena[]   = ".user_clip_plane3_ena";
    static constexpr char UserClipPlane4Ena[]   = ".user_clip_plane4_ena";
    static constexpr char UserClipPlane5Ena[]   = ".user_clip_plane5_ena";
    static constexpr char DxLinearAttrClipEna[] = ".dx_linear_attr_clip_ena";
    static constexpr char ZclipNearDisable[]    = ".zclip_near_disable";
    static constexpr char ZclipFarDisable[]     = ".zclip_far_disable";
    static constexpr char RasterizationKill[]   = ".rasterization_kill";
    static constexpr char ClipDisable[]         = ".clip_disable";
};

namespace PrerasterOutputSemanticMetadataKey
{
    static constexpr char Semantic[] = ".semantic";
    static constexpr char Index[]    = ".index";
};

namespace PsInputSemanticMetadataKey
{
    static constexpr char Semantic[] = ".semantic";
};

namespace HardwareStageMetadataKey
{
    static constexpr char EntryPoint[]                = ".entry_point";
    static constexpr char ScratchMemorySize[]         = ".scratch_memory_size";
    static constexpr char BackendStackSize[]          = ".backend_stack_size";
    static constexpr char FrontendStackSize[]         = ".frontend_stack_size";
    static constexpr char LdsSize[]                   = ".lds_size";
    static constexpr char PerfDataBufferSize[]        = ".perf_data_buffer_size";
    static constexpr char VgprCount[]                 = ".vgpr_count";
    static constexpr char SgprCount[]                 = ".sgpr_count";
    static constexpr char VgprLimit[]                 = ".vgpr_limit";
    static constexpr char SgprLimit[]                 = ".sgpr_limit";

    static constexpr char ThreadgroupDimensions[]     = ".threadgroup_dimensions";
    static constexpr char OrigThreadgroupDimensions[] = ".orig_threadgroup_dimensions";
    static constexpr char CbConstUsages[]             = ".cb_const_usages";
    static constexpr char NumCbConstUsages[]          = ".num_cb_const_usages";
    static constexpr char WavefrontSize[]             = ".wavefront_size";
    static constexpr char UserDataRegMap[]            = ".user_data_reg_map";
    static constexpr char ChecksumValue[]             = ".checksum_value";
    static constexpr char FloatMode[]                 = ".float_mode";
    static constexpr char Fp16Overflow[]              = ".fp16_overflow";
    static constexpr char IeeeMode[]                  = ".ieee_mode";
    static constexpr char WgpMode[]                   = ".wgp_mode";
    static constexpr char MemOrdered[]                = ".mem_ordered";
    static constexpr char ForwardProgress[]           = ".forward_progress";
    static constexpr char DebugMode[]                 = ".debug_mode";
    static constexpr char ScratchEn[]                 = ".scratch_en";
    static constexpr char TrapPresent[]               = ".trap_present";
    static constexpr char UserSgprs[]                 = ".user_sgprs";
    static constexpr char ExcpEn[]                    = ".excp_en";
    static constexpr char OffchipLdsEn[]              = ".offchip_lds_en";
    static constexpr char SharedVgprCnt[]             = ".shared_vgpr_cnt";
    static constexpr char WavesPerSe[]                = ".waves_per_se";
    static constexpr char UsesUavs[]                  = ".uses_uavs";
    static constexpr char UsesRovs[]                  = ".uses_rovs";
    static constexpr char WritesUavs[]                = ".writes_uavs";
    static constexpr char WritesDepth[]               = ".writes_depth";
    static constexpr char UsesAppendConsume[]         = ".uses_append_consume";
    static constexpr char UsesPrimId[]                = ".uses_prim_id";

};

namespace CbConstUsageMetadataKey
{
    static constexpr char BufferId[]    = ".buffer_id";
    static constexpr char BufferIndex[] = ".buffer_index";
    static constexpr char Elem[]        = ".elem";
    static constexpr char Chan[]        = ".chan";
    static constexpr char Usage[]       = ".usage";
};

namespace ShaderMetadataKey
{
    static constexpr char ApiShaderHash[]   = ".api_shader_hash";
    static constexpr char HardwareMapping[] = ".hardware_mapping";
    static constexpr char ShaderSubtype[]   = ".shader_subtype";
};

namespace Metadata
{

Result DeserializeCodeObjectMetadata(
    MsgPackReader*  pReader,
    CodeObjectMetadata*  pMetadata);

Result SerializeEnum(MsgPackWriter* pWriter, Abi::PipelineType value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::ApiShaderType value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::ApiShaderSubType value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::HardwareStage value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::PipelineSymbolType value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::PointSpriteSelect value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::GsOutPrimType value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::CoverageToShaderSel value);
Result SerializeEnum(MsgPackWriter* pWriter, Abi::CbConstUsageType value);

template <typename EnumType>
Result SerializeEnumBitflags(MsgPackWriter* pWriter, uint32 bitflags);

} // Metadata

} // PalAbi
} // Util
