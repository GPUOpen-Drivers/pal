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
 * @file  palDevice.h
 * @brief Defines the Platform Abstraction Library (PAL) IDevice interface and related types.
 ***********************************************************************************************************************
 */

#pragma once

#include "palDestroyable.h"
#include "palImage.h"
#include "palInlineFuncs.h"
#include "palPerfExperiment.h"
#include "palPipeline.h"
#include "palQueue.h"
#include "palFence.h"
#include "palCmdAllocator.h"

namespace Pal
{

// Forward declarations.
class  IBorderColorPalette;
class  ICmdAllocator;
class  ICmdBuffer;
class  IColorBlendState;
class  IColorTargetView;
class  IDepthStencilState;
class  IDepthStencilView;
class  IDevice;
class  IFence;
class  IGpuEvent;
class  IGpuMemory;
class  IImage;
class  IIndirectCmdGenerator;
class  IMsaaState;
class  IPerfExperiment;
class  IPipeline;
class  IPrivateScreen;
class  IQueryPool;
class  IQueue;
class  IQueueSemaphore;
class  ISwapChain;
struct BorderColorPaletteCreateInfo;
struct CmdAllocatorCreateInfo;
struct CmdBufferCreateInfo;
struct ColorBlendStateCreateInfo;
struct ColorTargetViewCreateInfo;
struct ComputePipelineCreateInfo;
struct DepthStencilStateCreateInfo;
struct DepthStencilViewCreateInfo;
struct ExternalImageOpenInfo;
struct ExternalGpuMemoryOpenInfo;
struct ExternalQueueSemaphoreOpenInfo;
struct ExternalResourceOpenInfo;
struct GpuEventCreateInfo;
struct GpuMemoryCreateInfo;
struct GpuMemoryOpenInfo;
struct GpuMemoryRef;
struct GraphicsPipelineCreateInfo;
struct ImageCreateInfo;
struct IndirectCmdGeneratorCreateInfo;
struct MsaaStateCreateInfo;
struct MsaaQuadSamplePattern;
struct PeerGpuMemoryOpenInfo;
struct PeerImageOpenInfo;
struct PerfExperimentCreateInfo;
struct PinnedGpuMemoryCreateInfo;
struct PresentableImageCreateInfo;
struct PrivateScreenCreateInfo;
struct PrivateScreenNotifyInfo;
struct QueryPoolCreateInfo;
struct QueueCreateInfo;
struct QueueSemaphoreCreateInfo;
struct QueueSemaphoreOpenInfo;
struct SwapChainCreateInfo;
struct SwapChainProperties;
struct SvmGpuMemoryCreateInfo;
struct GraphicPipelineViewInstancingInfo;
enum   WsiPlatform : uint32;
enum class PipelineBindPoint : uint32;
enum class VaRange : uint32;

/// Maximum string length for GPU names.  @see DeviceProperties.
constexpr uint32 MaxDeviceName = 256;

/// Maximum number of indirect user-data tables managed by PAL's command buffer objects.  @see DeviceFinalizeInfo.
constexpr uint32 MaxIndirectUserDataTables = 3;

/// Maximum number of supported entries in the MSAA sample pattern palette.  See IDevice::SetSamplePatternPalette().
constexpr uint32 MaxSamplePatternPaletteEntries = 16;

/// Specifies what type of GPU a particular IDevice is (i.e., discrete vs. integrated).
enum class GpuType : uint32
{
    Unknown    = 0x0,  ///< The GPU type can't be determined and is unknown.
    Integrated = 0x1,  ///< Integrated GPU (i.e., APU).
    Discrete   = 0x2,  ///< Discrete GPU.
    Virtual    = 0x3,  ///< Virtualized GPU.
    Count
};

/// Specifies which graphics IP level (GFXIP) this device has.
enum class GfxIpLevel : uint32
{
    _None    = 0x0,   ///< @internal The device does not have an GFXIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None     = _None, ///< The device does not have an GFXIP block, or its level cannot be determined
#endif

    GfxIp6    = 0x1,
    GfxIp7    = 0x2,
    GfxIp8    = 0x3,
    GfxIp8_1  = 0x4,
    GfxIp9    = 0x5,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 426
#else
#endif

    Count             ///< Count of all supported GfxIp levels.
};

/// Specifies the hardware revision.  Enumerations are in family order (Southern Islands, Sea Islands, Kaveri,
/// Carrizo, Volcanic Islands, etc.)
enum class AsicRevision : uint32
{
    Unknown    = 0x00,

    Tahiti     = 0x01,
    Pitcairn   = 0x02,
    Capeverde  = 0x03,
    Oland      = 0x04,
    Hainan     = 0x05,

    Bonaire    = 0x06,
    Hawaii     = 0x07,

    Kalindi    = 0x0A,
    Godavari   = 0x0B,
    Spectre    = 0x0C,
    Spooky     = 0x0D,

    Carrizo    = 0x0E,
    Bristol    = 0x0F,
    Stoney     = 0x10,

    Iceland    = 0x11,
    Tonga      = 0x12,
    Fiji       = 0x13,

    Polaris10  = 0x14,
    Polaris11  = 0x15,
    Polaris12  = 0x16,

#if PAL_BUILD_GFX9
    Vega10     = 0x18,
    Vega12     = 0x19,
    Raven      = 0x1B,
#endif // PAL_BUILD_GFX9

};

/// Specifies which operating-system-support IP level (OSSIP) this device has.
enum class OssIpLevel : uint32
{
    _None    = 0x0,   ///< @internal The device does not have an OSSIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None     = _None, ///< The device does not have an OSSIP block, or its level cannot be determined
#endif

    OssIp1   = 0x1,
    OssIp2   = 0x2,
    OssIp2_4 = 0x3,
    OssIp4   = 0x4,
};

/// Specifies which VCE IP level this device has.
enum class VceIpLevel : uint32
{
    _None    = 0x0,  ///< @internal The device does not have an VCEIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None     = _None, ///< The device does not have an VCEIP block, or its level cannot be determined
#endif

    VceIp1   = 0x1,
    VceIp2   = 0x2,
    VceIp3   = 0x3,
    VceIp3_1 = 0x4,
    VceIp3_4 = 0x5,
    VceIp4   = 0x6,
};

/// Specifies which UVD IP level this device has.
enum class UvdIpLevel : uint32
{
    _None    = 0x0,   ///< @internal The device does not have an UVDIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None     = _None, ///< The device does not have an UVDIP block, or its level cannot be determined
#endif

    UvdIp3_2 = 0x1,
    UvdIp4   = 0x2,
    UvdIp4_2 = 0x2,
    UvdIp5   = 0x3,
    UvdIp6   = 0x4,
    UvdIp6_2 = 0x5,
    UvdIp6_3 = 0x6,
    UvdIp7   = 0x7,
    UvdIp7_2 = 0x8,
};

/// Specifies which VCN IP level this device has.
enum class VcnIpLevel : uint32
{
    _None    = 0x0,   ///< @internal The device does not have an VCNIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None     = _None, ///< The device does not have an VCNIP block, or its level cannot be determined
#endif

    VcnIp1   = 0x1,
};

/// Specified video decode type
enum class VideoDecodeType : uint32
{
    H264                = 0x0,      ///< H264 VLD
    Vc1                 = 0x1,      ///< VC1 VLD
    Mpeg2Idct           = 0x2,      ///< Partial MPEG2 decode (IT+MP)
    Mpeg2Vld            = 0x3,      ///< Full MPEG2 decode (RE+IT+MP+DB)
    Mpeg4               = 0x4,      ///< MPEG4
    Wmv9                = 0x5,      ///< WMV9 IDCT
    Mjpeg               = 0x6,      ///< Motion JPEG
    Hevc                = 0x7,      ///< HEVC
    Vp9                 = 0x8,      ///< VP9
    Hevc10Bit           = 0x9,      ///< HEVC 10bit
    Vp910Bit            = 0xa,      ///< VP9 10bit
    Count,
};

/// Video CODEC to use for encoding
enum class VideoEncodeCodec : uint32
{
    H264               = 0x0,       ///< H.264
    H265               = 0x1,       ///< H.265
    Count
};

/// Specifies a virtual address range memory should be allocated in.
enum class VaRange : uint32
{
    Default,                ///< Default VA range.  Choose this for most allocations.
    DescriptorTable,        ///< Place the allocation in a 4GB VA range reserved by PAL for descriptor tables.  Knowing
                            ///  an allocation is allocated in this range, only one user data entry is required to
                            ///  specify a descriptor table.  @see ResourceMappingNodeType.
    ShadowDescriptorTable,  ///< Place the allocation in a 4GB VA range reserved by PAL for "shadow" descriptor tables.
                            ///  A shadow descriptor table is an additional table with the same layout as its parent
                            ///  descriptor table that can hold infrequently needed data like fmask SRDs or UAV counter
                            ///  data.  This scheme allows the client and SC to work out a known location for
                            ///  infrequently needed data without wasting a user data entry or wasting half of every
                            ///  descriptor cache line.
                            ///  Only supported if DeviceProperties::gpuMemoryProperties::flags::shadowDescVaSupport is
                            ///  set.
    Svm,                    ///< Place the allocation in a VA range reserved by PAL for shared virtual memory(SVM).
                            ///  This is a GPU VA range that is reserved also on the CPU-side.
                            ///  The size of reserved VA is set by PAL client by calling CreatePlatform.
    Count,
};

/// How to interpret a single bit in a swizzle equation.
union SwizzleEquationBit
{
    struct
    {
        uint8 valid   : 1;  ///< Indicates whether this channel setting is valid.
        uint8 channel : 2;  ///< 0 for x channel, 1 for y channel, 2 for z channel.
        uint8 index   : 5;  ///< The channel index.
    };
    uint8 u8All;            ///< The above values packed in an 8-bit uint.
};

constexpr uint32 SwizzleEquationMaxBits = 20;   ///< Swizzle equations will consider no more than this many bits.
constexpr uint8  InvalidSwizzleEqIndex  = 0xFF; ///< Indicates an invalid swizzle equation index in the equation table.
constexpr uint8  LinearSwizzleEqIndex   = 0xFE; ///< An invalid eq. index indicating a row-major, linear memory layout.

/// Supported shader cache modes
enum ShaderCacheMode : uint32
{
    ShaderCacheDisabled = 0,
    ShaderCacheRuntimeOnly = 1,
    ShaderCacheOnDisk = 2,

};

/// Texture fetch meta-data capabilities bitfield definition, used with tcCompatibleMetaData setting
enum TexFetchMetaDataCaps : uint32
{
    TexFetchMetaDataCapsNoAaColor = 0x00000001,
    TexFetchMetaDataCapsMsaaColor = 0x00000002,
    TexFetchMetaDataCapsFmask = 0x00000004,
    TexFetchMetaDataCapsNoAaDepth = 0x00000008,
    TexFetchMetaDataCapsMsaaDepth = 0x00000010,
    TexFetchMetaDataCapsAllowStencil = 0x00000020,
    TexFetchMetaDataCapsAllowZ16 = 0x00000040,

};

/// Catalyst AI setting enums
enum CatalystAiSettings : uint32
{
    CatalystAiDisable = 0,
    CatalystAiEnable = 1,
    CatalystAiMaximum = 2,

};

/// Texture Filter optimization enum values
enum TextureFilterOptimizationSettings : uint32
{
    TextureFilterOptimizationsDisabled = 0,
    TextureFilterOptimizationsEnabled = 1,
    TextureFilterOptimizationsAggressive = 2,

};

/// Distribution Tess Mode enum values
enum DistributionTessMode : uint32
{
    DistributionTessOff = 0,
    DistributionTessDefault = 1,
    DistributionTessPatch = 2,
    DistributionTessDonut = 3,
    DistributionTessTrapezoid = 4,
    DistributionTessTrapezoidOnly = 5,

};

/// Defines the context roll optimization flags
enum ContextRollOptimizationFlags : uint32
{
    OptFlagNone = 0x00000000,
    PadParamCacheSpace = 0x00000001,

};

/// Enum defining the different scopes (i.e. registry locations) where settings values are stored
enum InternalSettingScope : uint32
{
    PrivateDriverKey = 0x0,
    PublicPalKey = 0x1,
    PrivatePalKey = 0x2,
    PrivatePalGfx6Key = 0x3,
    PrivatePalGfx9Key = 0x4,
    PublicCatalystKey = 0x5,

};

static constexpr uint32 MaxPathStrLen = 512;
static constexpr uint32 MaxFileNameStrLen = 256;
static constexpr uint32 MaxMiscStrLen = 61;

/// Pal settings that are client visible and editable.
struct PalPublicSettings
{
    ///  Maximum border color palette size supported by any queue.
    uint32 borderColorPaletteSizeLimit;
    ///  When true RPM will use the graphics fast clear path for depth stencil images if possible. When false the
    ///  compute path will be preferred.
    bool useGraphicsFastDepthStencilClear;
    ///  Forces all serialized loads (LoadPipeline or LoadCompoundState) to fail.
    bool forceLoadObjectFailure;
    ///  Controls the distribution mode for tessellation, which affects how patches are processed by different VGT
    ///  units. 0: None - No distribution across VGTs (legacy mode). 1: Default - Optimal settings are chosen depending
    ///  on the gfxip. 2: Patch - Individual patches are distributed to different VGTs. 3: Donut - Patches are split
    ///  into donuts and distributed to different VGTs. 4: Trapezoid - Patches from donuts are split into trapezoids and
    ///  distributed to different VGTs. Falls back to donut mode if HW does not support this mode. 5: Trapezoid only -
    ///  Distribution turned off if HW does not support this mode.
    uint32 distributionTessMode;
    ///  Controls whether the Device shader cache should be used to try to avoid redundant shader compiles 0: Shader
    ///  Cache is disabled. 1: Shader Cache is enabled for runtime use only. 2: Shader Cache is enabled with on-disk file
    ///  backing.
    ShaderCacheMode shaderCacheMode;
    ///  Flags that control PAL optimizations to reduce context rolls. 0: Optimization disabled. 1: Pad parameter cache
    ///  space. Sets VS export count and PS interpolant number to per-command buffer maximum value. Reduces context rolls at
    ///  the expense of parameter cache space.
    uint32 contextRollOptimizationFlags;
    ///  The number of unbound descriptor debug srds to allocate. To detect reads of unbound descriptor within arrays,
    ///  multiple debug srds can be allocated.
    uint32 unboundDescriptorDebugSrdCount;
    ///  Disables compilation of internal PAL shaders. It can be enabled only if a PAL client won't use any of PAL blit
    ///  functionalities on gfx/compute engines.
    bool disableResourceProcessingManager;
    /// Controls app detect and image quality altering optimizations exposed by CCC.
    uint32 catalystAI;
    /// Controls texture filtering optimizations exposed by CCC.
    uint32 textureOptLevel;
    ///  Disables SC initialization. It can be enabled only if a PAL client won't use SC for shader compilation and
    ///  provide direct ISA binaries(usually AQL path).
    bool disableScManager;
    /// Information about the client performing the rendering. For example: Rendered By PAL (0.0.1)
    char renderedByString[MaxMiscStrLen];
    /// Debug information that the client or tester might want reported.
    char miscellaneousDebugString[MaxMiscStrLen];
    /// Allows SC to make optimizations at the expense of IEEE compliance.
    bool allowNonIeeeOperations;
    /// Controls whether or not shaders should execute one atomic instruction per wave for UAV append/consume operations.
    /// If false, one atomic will be executed per thread.
    bool appendBufPerWaveAtomic;
    ///  Bitmask of cases where texture compatible meta data will be used Single-sample color surface: 0x00000001 MSAA
    ///  color surface: 0x00000002 FMask data: 0x00000004 Single-sample depth surface: 0x00000008 MSAA depth surface:
    ///  0x00000010 Allow stencil: 0x00000020 Allow Z-16 surfs 0x00000040
    uint32 tcCompatibleMetaData;
    ///  Determines the maximum number of supported user-data entries accessible to a pipeline. If larger than the
    ///  number of user-data registers in hardware, the rest of the entries will be spilled to GPU memory. The default is the
    ///  maximum number of supported user-data entries based on client type.
    uint32 maxUserDataEntries;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 403
    ///  Controls how many instances of the user-data spill table will be contained in the ring buffer managed by
    ///  Universal Command Buffers. At most, the constant engine will be able to get that many draws or dispatches ahead of the
    ///  draw engine. This must be either zero, or divisible by four. If zero, any pipeline which requires spilling will fail
    ///  to compile because no spill table is present.
    uint32 userDataSpillTableRingSize;
    ///  Controls how many instances of the user-data stream-output table will be contained in the ring buffer managed
    ///  by Universal Command Buffers. At most, the constant engine will be able to get that many draws or dispatches ahead
    ///  of the draw engine. This must be eitehr zero, or divisible by four. If zero, any pipeline which requires stream
    ///  output will fail to compile because no SRD table is present.
    uint32 streamOutTableRingSize;
#endif
    ///  Specifies the threshold below which CmdCopyMemory() is executed via a CpDma BLT, in bytes. CPDMA copies have
    ///  lower overhead than CS/Gfx copies, but less throughput for large copies.
    uint32 cpDmaCmdCopyMemoryMaxBytes;
    ///  Forces high performance state for allocated queues. Note: currently supported in Windows only.
    bool forceHighClocks;
    ///  Controls the size of the GFX/Compute Scratch Rings. Valid values are [1-32]. Larger values allocate larger
    ///  Scratch Rings and allow more Waves to run in parallel.
    uint32 numScratchWavesPerCu;
    ///  When submitting multiple command buffers in a single grQueueSubmit call, the ICD will patch the command streams
    ///  so that the command buffers are chained together instead of submitting through KMD multiple times. This setting
    ///  limits the number of command buffers that will be chained together; reduce to prevent problems due to long running
    ///  submits.
    uint32 cmdBufBatchedSubmitChainLimit;
    ///  Flags that control PAL's command allocator residency optimizations. If a command allocation isn't optimized PAL
    ///  will wait for it to become resident at creation. 0x1 - Wait for command data to become resident at Submit-time. 0x2
    ///  - Wait for embedded data to become resident at Submit-time. 0x4 - Wait for marker data to become resident at
    ///  Submit-time.
    uint32 cmdAllocResidency;
    ///  Overrides max queued frames allowed
    uint32 maxQueuedFrames;
    ///  Maximum number of presentable images per adapter(including LDA chain) which is recommended. If app exceeds the
    ///  presentable image number threshold, awarning may be reported.
    uint32 presentableImageNumberThreshold;
    ///  Provides a hint to PAL that client knows that every individual depth stencil surfaces are always cleared with
    ///  same values.If TRUE, per-tile tracking of exp/clear will be enabled (requires HTile).
    bool hintInvariantDepthStencilClearValues;
    ///  Provides a hint to PAL that PAL should disable color compression on surfaces that are smaller than or equal to
    ///  this setting (setting * setting) in size.
    uint32 hintDisableSmallSurfColorCompressionSize;
    ///  Disables Escape call to KMD. This is a temporary setting for experimentation that is expected to break features
    ///  that currently needs Escape call.
    bool disableEscapeCall;
    /// In Win7 requests an extended TDR timeout (6 seconds).
    bool longRunningSubmissions;
    /// Disables MCBP on demand. This is a temporary setting until ATOMIC_MEM packet issue with MCBP is resolved.
    bool disableCommandBufferPreemption;
    /// Disable the fast clear eliminate skipping optimization.  This optimization will conservatively track the usage
    /// of clear values to allow the vast majority of images that never clear to a value that isn't TC-compatible to
    /// skip the CPU and front-end GPU overhead of issuing a predicated fast clear eliminate BLT.
    bool disableSkipFceOptimization;
};

/// Command Buffer Logger layer runtime settings
struct CmdBufferLoggerSettings
{
    uint32 cmdBufferLoggerFlags;
};

/// Location enum for Debug Overlay layer
enum DebugOverlayLocation : uint32
{
    DebugOverlayUpperLeft = 0,
    DebugOverlayUpperRight = 1,
    DebugOverlayLowerRight = 2,
    DebugOverlayLowerLeft = 3,
    DebugOverlayCount = 4,
};

/// Enum defining supported colors for Debug Overlay timegraph
enum TimeGraphColor : uint32
{
    BlackColor = 0,
    RedColor = 1,
    GreenColor = 2,
    BlueColor = 3,
    YellowColor = 4,
    CyanColor = 5,
    MagentaColor = 6,
    WhiteColor = 7,
};

/// Config struct that controls what information is displayed on the Debug Overlay
struct DebugOverlayConfig
{
    bool                    visualConfirmEnabled;
    bool                    timeGraphEnabled;
    DebugOverlayLocation overlayLocation;
    char                 renderedByString[61];
    char                 miscellaneousDebugString[61];
    bool                 printFrameNumber;
};

/// Time Graph display configuration
struct TimeGraphConfig
{
    uint32 gridLineColor;
    uint32 cpuLineColor;
    uint32 gpuLineColor;
};

/// Debug Overlay Benchmark configuration.
struct OverlayBenchmarkConfig
{
    uint32                  maxBenchmarkTime;
    bool   usageLogEnable;
    char   usageLogDirectory[MaxPathStrLen];
    char   usageLogFilename[MaxPathStrLen];
    bool                    logFrameStats;
    char                    frameStatsLogDirectory[MaxPathStrLen];
    uint32                  maxLoggedFrames;
};

/// Configures the memory usage display on the Debug Overlay.
struct OverlayMemoryInfoConfig
{
    bool combineNonLocal;
    bool reportCmdAllocator;
    bool reportExternal;
    bool reportInternal;
};

/// Debug overlay layer runtime settings
struct DebugOverlaySettings
{
    DebugOverlayConfig      debugOverlayConfig;
    TimeGraphConfig         timeGraphConfig;
    OverlayBenchmarkConfig  overlayBenchmarkConfig;
    OverlayMemoryInfoConfig overlayMemoryInfoConfig;
};

/// Enum describing the supported granularity for the GPU profiler layer
enum GpuProfilerGranularity : uint32
{
    GpuProfilerGranularityDraw   = 0,
    GpuProfilerGranularityCmdBuf = 1,
    GpuProfilerGranularityFrame  = 2,
};

/// Enum describing trace modes available in Gpu Profiler layer.
enum GpuProfilerTraceModeFlags : uint32
{
    GpuProfilerTraceDisabled =  0x0, ///< All tracing is disabled.
    GpuProfilerTraceSpm      =  0x1, ///< Streaming performance counter trace flag.
    GpuProfilerTraceSqtt     =  0x2  ///< SQ thread trace flag.
};

/// Defines the modes that the GPU Profiling layer can use when its buffer fills.
enum GpuProfilerStallMode : uint32
{
    GpuProfilerStallAlways     = 0, ///< Always stall to get accurate trace data
    GpuProfilerStallLoseDetail = 1, ///< Lose register-level detail if under pressure to avoid stalls
    GpuProfilerStallNever      = 2, ///< Never stall, miss trace packets
};

/// Configuration options for the PAL GPU Profiler layer.
struct GpuProfilerConfig
{
    char   logDirectory[MaxPathStrLen];
    uint32 startFrame;
    uint32 frameCount;
    bool   recordPipelineStats;
    bool   breakSubmitBatches;
    uint32 traceModeMask;
};

/// Configuration options for performance counter collection through the Profiler Layer.
struct GpuProfilerPerfCounterConfig{
    char                   globalPerfCounterConfigFile[MaxFileNameStrLen];
    bool                   cacheFlushOnCounterCollection;
    GpuProfilerGranularity granularity;
};

struct GpuProfilerSqttConfig
{
    uint32               tokenMask;
    uint32               pipelineHashHi;
    uint32               pipelineHashLo;
    uint64               pipelineHash;
    ShaderHash           vsHash;
    ShaderHash           hsHash;
    ShaderHash           dsHash;
    ShaderHash           gsHash;
    ShaderHash           psHash;
    ShaderHash           csHash;
    uint32               maxDraws;
    size_t               bufferSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 422
    GpuProfilerStallMode stallMode;
#endif
};

///Configuration options for capturing Streaming Performance Monitors through the Profiler layer.
struct GpuProfilerSpmConfig
{
    char   spmPerfCounterConfigFile[MaxFileNameStrLen];
    uint32 spmTraceInterval;
    size_t spmTraceBufferSize;
};

/// GPU profiler layer runtime settings
struct GpuProfilerSettings
{
    GpuProfilerConfig            profilerConfig;
    GpuProfilerPerfCounterConfig perfCounterConfig;
    GpuProfilerSqttConfig        sqttConfig;
    GpuProfilerSpmConfig         spmConfig;
};

/// Configuration options for the Interface Logger layer.
struct InterfaceLoggerConfig
{
    char    logDirectory[MaxPathStrLen];
    bool    multithreaded;
    uint32  basePreset;
    uint32  elevatedPreset;
};

/// Describes the equations needed to interpret the raw memory of a tiled texture.
struct SwizzleEquation
{
    SwizzleEquationBit addr[SwizzleEquationMaxBits]; ///< Address setting: each bit is the result of addr ^ xor ^ xor2.
    SwizzleEquationBit xor1[SwizzleEquationMaxBits]; ///< xor setting.
    SwizzleEquationBit xor2[SwizzleEquationMaxBits]; ///< xor2 setting.
    uint32             numBits;                      ///< The number of bits in the equation.
    bool               stackedDepthSlices;           ///< True if depth slices are treated as being stacked vertically
                                                     ///  prior to swizzling.
};

/// Specifies the hardware features supported for PRT (sparse images).
enum PrtFeatureFlags : uint32
{
    PrtFeatureBuffer                = 0x00000001,   ///< Indicates support for sparse buffers
    PrtFeatureImage2D               = 0x00000002,   ///< Indicates support for sparse 2D images
    PrtFeatureImage3D               = 0x00000004,   ///< Indicates support for sparse 3D images
    PrtFeatureImageMultisampled     = 0x00000008,   ///< Indicates support for sparse multisampled images
    PrtFeatureImageDepthStencil     = 0x00000010,   ///< Indicates support for sparse depth/stencil images
    PrtFeatureShaderStatus          = 0x00000020,   ///< Indicates support for residency status in shader instructions
    PrtFeatureShaderLodClamp        = 0x00000040,   ///< Indicates support for LOD clamping in shader instructions
    PrtFeatureUnalignedMipSize      = 0x00000080,   ///< Indicates support for non-miptail levels with dimensions that
                                                    ///  aren't integer multiples of the tile size as long as they are
                                                    ///  at least as large as a single tile
    PrtFeaturePerSliceMipTail       = 0x00000100,   ///< Indicates support for per-slice miptail (slice-major order)

    PrtFeatureTileAliasing          = 0x00000200,   ///< Indicates support for aliasing tiles (without metadata)
    PrtFeatureStrictNull            = 0x00000400,   ///< Indicates whether reads of unmapped tiles always return zero
    PrtFeatureNonStandardImage3D    = 0x00000800,   ///< Indicates support for sparse 3D images restricted to
                                                    ///  non-standard tile shapes that match the tile mode block depth
};

/// Describe the settings' scope accessible by clients.
enum class SettingScope
{
    Driver,   ///< For settings specific to a UMD
    Global,   ///< For global settings controlled by CCC
};

/// The engine subtype is used to indicate engine's purpose.
enum class EngineSubType : uint32
{
    _None = 0x0,        ///< @internal The device does not have an OSSIP block, or its level cannot be determined

    // Unfortunately for Linux clients, X.h includes a "#define None 0" macro.  Clients have their choice of either
    // undefing None before including this header or using _None when dealing with PAL.
#ifndef None
    None = _None,       ///< The device does not have an OSSIP block, or its level cannot be determined
#endif
    RtCuHighCompute,    ///< Engine is for real time audio with high priority and
                        ///  compute unit reservation.
    RtCuMedCompute,     ///< Engine is for real time audio with medium priority.
    LowLatency,         ///< Engine is for low latency usage.
    VrHighPriority,     ///< Engine is for VR with high priority.
    Count,
};

/// Big Software (BigSW) Release information structure
/// Software release management uses this version # to control a rollout of big SW features together.
struct BigSoftwareReleaseInfo
{
    uint32 majorVersion;         ///< BigSW Release Major version
    uint32 minorVersion;         ///< BigSW Release Minor version.
    uint32 miscControl;          ///< BigSW Release miscellaneous control.
};

/// Virtual display capabilities as determined by the OS. The reported values bound the valid ranges of values supported
/// by the @ref VirtualDisplayInfo structure passed in to @ref IDevice::CreateVirtualDisplay.
struct VirtualDisplayCapabilities
{
    uint32   maxVirtualDisplays; ///< The maximum number of virtual display supported
    Rational minRefreshRate;     ///< The minimum refresh rate
    Rational maxRefreshRate;     ///< The maximum refresh rate
};

/// The properties of a specific virtual display
struct VirtualDisplayProperties
{
    bool isVirtualDisplay; ///< True, if it's a virtual display
};

/// Enumerates all of the types of local video memory which could be associated with a GPU.
enum class LocalMemoryType : uint32
{
    Unknown = 0,
    Ddr2,
    Ddr3,
    Ddr4,
    Gddr5,
    Gddr6,
    Hbm,
    Hbm2,
    Hbm3,
    Count
};

/// Reports various properties of a particular IDevice to the client.  @see IDevice::GetProperties.
struct DeviceProperties
{
    uint32     vendorId;                     ///< Vendor ID (should always be 0x1002 for AMD).
    uint32     deviceId;                     ///< GPU device ID (e.g., Hawaii XT = 0x67B0).
    uint32     revisionId;                   ///< GPU revision.  HW-specific value differentiating between different
                                             ///  SKUs or revisions.  Corresponds to one of the PRID_* revision IDs.
    AsicRevision revision;                   ///< ASIC revision.
    GpuType    gpuType;                      ///< Type of GPU (discrete vs. integrated).
    GfxIpLevel gfxLevel;                     ///< IP level of this GPU's GFX block
    OssIpLevel ossLevel;                     ///< IP level of this GPU's OSS block
    VceIpLevel vceLevel;                     ///< IP level of this GPU's VCE block
    UvdIpLevel uvdLevel;                     ///< IP level of this GPU's UVD block
    VcnIpLevel vcnLevel;                     ///< IP level of this GPU's VCN block
    uint32     gfxStepping;                  ///< Stepping level of this GPU's GFX block
    char       gpuName[MaxDeviceName];       ///< Null terminated string identifying the GPU.
    uint32     gpuIndex;                     ///< Device's index in a linked adapter chain.
    uint32     maxGpuMemoryRefsResident;     ///< Maximum number of GPU memory references that can be resident
                                             ///  at any time.  Memory references set both via IQueue and IDevice
                                             ///  (via AddGpuMemoryReferences() or Submit()) count against this limit.
    uint64     timestampFrequency;           ///< Frequency of the device's timestamp counter in Hz.
                                             ///  @see ICmdBuffer::CmdWriteTimestamp.
    uint32     attachedScreenCount;          ///< Number of screen attached to the device.
    uint32     maxSemaphoreCount;            ///< Queue semaphores cannot have a signal count higher than this value.
                                             ///  For example, one indicates that queue semaphores are binary.
    PalPublicSettings settings;              ///< Public settings that the client has the option of overriding

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 415
    struct
    {
        ///< A mask of SwapChainModeSupport flags for each present mode.
        ///  This indicates which kinds of swap chains can be
        ///  created depending on the client's intended present mode.
        uint32 supportedSwapChainModes[static_cast<uint32>(Pal::PresentMode::Count)];

    } swapChainProperties;
#endif

    struct
    {
        union
        {
            struct
            {
                /// This engine supports timestamps (ICmdBuffer::CmdWriteTimestamp()).
                uint32 supportsTimestamps              :  1;

                /// This engine supports ICmdBuffer::CmdSetPredication() based on Streamout/Occlusion query
                uint32 supportsQueryPredication        :  1;

                /// This engine supports ICmdBuffer::CmdSetPredication() based on GPU memory allocation
                uint32 supportsMemoryPredication       :  1;

                /// This engine supports ICmdBuffer::If(), Else() and EndIf() calls.
                uint32 supportsConditionalExecution    :  1;

                /// This engine supports ICmdBuffer::While() and EndWhile() calls.
                uint32 supportsLoopExecution           :  1;

                /// This engine supports ICmdBuffer::CmdWaitRegisterValue(), WaitMemoryValue() and
                /// CopyRegisterToMemory() calls.
                uint32 supportsRegMemAccess            :  1;

                /// This engine supports ICmdBuffer::CmdCopyImage() between optimally tiled images with
                /// mismatched tiling tokens.
                uint32 supportsMismatchedTileTokenCopy :  1;

                /// This engine supports ICmdBuffer::Barrier() calls that transition out of the @ref
                /// LayoutUninitializedTarget layout.
                uint32 supportsImageInitBarrier        :  1;

                /// This engine supports ICmdBuffer::Barrier() calls that transition out of the @ref
                /// LayoutUninitializedTarget layout for individual subresources. If this is not set and
                /// supportsImageInitBarrier is set, the subresource range must span the entire image.
                uint32 supportsImageInitPerSubresource :  1;

                /// This engine does not support any virtual memory features. IQueue::RemapVirtualMemoryPages and
                /// IQueue::CopyVirtualPageMappings are not supported on Queues using this engine.
                uint32 runsInPhysicalMode              :  1;

                /// Indicates whether this engine can do virtual memory remap or not.
                uint32 supportVirtualMemoryRemap       :  1;

                /// Indicates whether this Queues using this engine can maintain the contents of CE RAM across
                /// consecutive submissions.  If this is not set, the client must not specify a nonzero value for
                /// either @ref QueueCreateInfo::persistentCeRamSize or @ref QueueCreateInfo::persistentCeRamOffset.
                uint32 supportPersistentCeRam          :  1;

                /// If true, this engine does not support peer-to-peer copies that target memory in the invisible heap
                /// on another GPU due to a hardware bug.
                uint32 p2pCopyToInvisibleHeapIllegal  :  1;

                /// Reserved for future use.
                uint32 reserved                        : 19;
            };
            uint32 u32All;                  ///< Flags packed as 32-bit uint.
        } flags;                            ///< Engines property flags.
        EngineSubType engineSubType[MaxAvailableEngines]; ///< Engines subtype.

        uint32   engineCount;                   ///< Number available engines of this type.
        uint32   queueSupport;                  ///< Mask of QueueTypeSupport flags indicating which queues are
                                                ///  supported by this engine.
        uint32   maxBorderColorPaletteSize;     ///< Maximum size of a border color palette on this engine.
        uint32   controlFlowNestingLimit;       ///< Maximum depth of command-buffer control flow nesting on this
                                                ///  engine.
        uint32   ceRamSizeAvailable;            ///< Size, in bytes, of constant engine RAM available on this engine.
        Extent3d minTiledImageCopyAlignment;    ///< Minimum alignments (pixels) for X/Y/Z/Width/Height/Depth for
                                                ///  ICmdBuffer::CmdCopyImage() between optimally tiled images.
        Extent3d minTiledImageMemCopyAlignment; ///< Minimum alignments (bytes) for X/Y/Z/Width/Height/Depth for
                                                ///  ICmdBuffer::CmdCopyImage() with an optimally tiled image and a
                                                ///  linearly tiled image. Also applies to
                                                ///  ICmdBuffer::CmdCopyImageToMemory() or
                                                ///  ICmdBuffer::CmdCopyMemoryToImage() with an optimally tiled image.
        Extent3d minLinearMemCopyAlignment;     ///< Minimum alignments (bytes) for X/Y/Z/Width/Height/Depth for
                                                ///  ICmdBuffer::CmdCopyTypedBuffer().
        uint32   minTimestampAlignment;         ///< If supportsTimestamps is set, this is the minimum address alignment
                                                ///  in bytes of the dstOffset in ICmdBuffer::CmdWriteTimestamp().
        uint32   availableGdsSize;              ///< Total GDS size in bytes available for all engines of a particular
                                                ///  engine type.
        uint32   gdsSizePerEngine;              ///< Maximum GDS size in bytes available for a single engine.
        uint32   maxNumDedicatedCu;             ///< The maximum number of dedicated CUs for the real time audio queue

        /// Specifies the suggested heap preference clients should use when creating an @ref ICmdAllocator that will
        /// allocate command space for this engine type.  These heap preferences should be specified in the allocHeap
        /// parameter of @ref CmdAllocatorCreateInfo.  Clients are free to ignore these defaults and use their own
        /// heap preferences, but may suffer a performance penalty.
        GpuHeap preferredCmdAllocHeaps[CmdAllocatorTypeCount];
    } engineProperties[EngineTypeCount];    ///< Lists available engines on this device and their properties.

    struct
    {
        union
        {
            struct
            {
                /// This queue supports IQueue::PresentSwapChain() calls.  Note that a queue may support swap chain
                /// presents even if the supportedDirectPresentModes flags below indicate no support for direct
                /// presents; instead swap chain PresentMode support is queried via GetSwapChainInfo.
                uint32 supportsSwapChainPresents :  1;

                /// Reserved for future use.
                uint32 reserved                  : 31;
            };
            uint32 u32All;                    ///< Flags packed as 32-bit uint.
        } flags;                              ///< Queue property flags.

        uint32   supportedDirectPresentModes; ///< A mask of PresentModeSupport flags indicating support for various
                                              ///  PresentModes when calling IQueue::PresentDirect().
    } queueProperties[QueueTypeCount];        ///< Lists the properties of all queues supported by PAL.

    struct
    {
        union
        {
            struct
            {
                /// Indicates support for virtual GPU memory allocations.  @see IQueue::RemapVirtualMemoryPages.
                uint32 virtualRemappingSupport          :  1;

                /// Indicates support for pinning system memory for access as GPU memory.
                /// @see IDevice::PinSystemMemory.
                uint32 pinningSupport                   :  1;

                /// Indicates support pinned memory which is host-mapped from foreign device.
                uint32 supportHostMappedForeignMemory   :  1;

                /// Indicates whether specifying memory references at Submit time is supported. If not supported
                /// all memory references must be manged via IDevice or IQueue AddGpuMemoryReferences()
                uint32 supportPerSubmitMemRefs          :  1;

                /// Indicates support for GPU virtual addresses that are visible to all devices.
                uint32 globalGpuVaSupport               :  1;

                /// Indicates support for Shared Virtual Memory VA range.
                uint32 svmSupport                       :  1;

                /// Indicates support for shadow desc VA range.
                uint32 shadowDescVaSupport              :  1;

                /// Indicates support for IOMMUv2. Fine grain SVM is not supported without IOMMU.
                /// PAL client needs to check this flag before using fine grain SVM.
                /// IOMMU is a memory management unit (MMU) that connects a direct-memory-access-capable
                /// (DMA-capable) I/O bus to the main memory.
                uint32 iommuv2Support                   :  1;

                /// Indiciates that the platform supports automatic GPU memory priority management.
                uint32 autoPrioritySupport              :  1;

                /// Indicates KMD has enabled HBCC(High Bandwidth Cache Controller) page migration support.
                /// This means shaders must be compiled such that all memory clauses can be replayed in response to an XNACK.
                uint32 pageMigrationEnabled             :  1;
                /// Placeholder.
                uint32 placeholder0                     :  1;
                /// Reserved for future use.
                uint32 reserved                         : 21;
            };
            uint32 u32All;           ///< Flags packed as 32-bit uint.
        } flags;                     ///< GPU memory property flags.

        gpusize realMemAllocGranularity;    ///< The addresses and sizes of "real" GPU memory objects must be aligned
                                            ///  to at least this many bytes.
        gpusize virtualMemAllocGranularity; ///< The addresses and sizes of virtual GPU memory objects must be aligned
                                            ///  to at least this many bytes.
        gpusize virtualMemPageSize;         ///< Size in bytes of a virtual GPU memory page.
                                            ///  @see IQueue::RemapVirtualMemoryPages.
        gpusize fragmentSize;               ///< Size in bytes of a video memory fragment.  If GPU memory object
                                            ///  addresses and sizes are aligned to at least this value, VA translation
                                            ///  will be a bit faster.  It is aligned to the allocation granularities.
        gpusize maxVirtualMemSize;          ///< Total virtual GPU memory available (total VA space size).
        gpusize maxPhysicalMemSize;         ///< Total VRAM available (Local + Invisible + non-Local heap sizes).
        gpusize vaStart;                    ///< Starting address of the GPU's virtual address space.
        gpusize vaEnd;                      ///< Ending address of the GPU's virtual address space.
        gpusize descTableVaStart;           ///< Starting address of the descriptor table's virtual address space
        gpusize shadowDescTableVaStart;     ///< Starting address of the shadow descriptor table's virtual address space
        gpusize privateApertureBase;        ///< Private memory base address for generic address space (Windows only).
        gpusize sharedApertureBase;         ///< Shared memory base address for generic address space (Windows only).

        gpusize busAddressableMemSize;      ///< SDI/DirectGMA GPU aperture size set in CCC
        gpusize maxLocalMemSize;            ///< Total VRAM available on the GPU (Local + Invisible heap sizes).
        LocalMemoryType localMemoryType;    ///< Type of local memory used by the GPU.

        struct
        {
            float  maxMemClock;      ///< Maximum GPU memory clock in MHz. For DX builds this value is valid only after
                                     ///  the device has been finalized.
            uint32 memPerfRating;    ///< Precomputed performance rating of memory operations.
            uint32 vramBusBitWidth;  ///< Memory bus width.
            uint32 memOpsPerClock;   ///< Memory operations per clock.
        } performance;               ///< Performance-related memory properties.

    } gpuMemoryProperties;           ///< Memory properties for this device.

    struct
    {
        union
        {
            struct
            {
                /// Single-sample images created on this device support texture quilting
                uint32 supportsSingleSampleQuilting :  1;

                /// Images created on this device supports AQBS stereo mode, this AQBS stereo mode doesn't apply to the
                /// array-based stereo feature supported by Presentable images.
                uint32 supportsAqbsStereoMode       :  1;

                uint32 reservedForFutureHw          :  1;

                /// Reserved for future use.
                uint32 reserved                     : 29;
            };
            uint32 u32All;              ///< Flags packed as 32-bit uint.
        } flags;                        ///< GPU memory property flags.

        Extent3d        maxDimensions;  ///< Maximum supported width/height/depth for an image.
        uint32          maxArraySlices; ///< Maximum supported number of array slices for a 1D or 2D image.
        PrtFeatureFlags prtFeatures;    ///< PRT features supported by the hardware.
        gpusize         prtTileSize;    ///< Size, in bytes, of a PRT tile.
        uint8           numSwizzleEqs;  ///< How many swizzle equations are in pSwizzleEqs.

        const SwizzleEquation* pSwizzleEqs; ///< These describe how to interpret device-dependent tiling modes.

        bool     tilingSupported[static_cast<size_t>(ImageTiling::Count)]; ///< If each image tiling is supported.
    } imageProperties;                  ///< Image properties for this device.

    struct
    {
        /// Maximum number of available shader-accessible user data entries. @see PipelineShaderInfo.
        uint32 maxUserDataEntries;
        /// Maximum number of fast user data entries (typically corresponds to the number of user data hardware
        /// registers that aren't reserved for internal use by PAL).
        /// @warning Some shaders have optional special input values which are passed to the shader through fast user
        /// data entries (such as Vulkan's gl_DrawId for vertex shaders).  If a shader uses one such input, those
        /// inputs are mapped to fast user-data *before* the shader's resource mapping nodes.  Additionally, pipelines
        /// which use stream-output may lose some fast user data entries as well, depending on which shader stages are
        /// present in that pipeline.
        /// @see ResourceMappingNode.
        uint32 fastUserDataEntries[NumShaderTypes];
        uint32 maxThreadGroupSize;  ///< Per-device limit on threads per threadgroup for compute shaders.
        /// Some hardware supported by PAL has a bug which can cause a GPU hang if async compute enginesare used while
        /// compute shaders with > maxAsyncComputeThreadGroupSize are in flight on any queue. This reports the
        /// maximum "safe" limit on threads per threadgroup for compute shaders for this device if the client wishes to
        /// use async compute engines. Note that the bug can occur if the following conditions are met:
        ///  (a) Async compute workloads are running *somewhere* on the GPU, in any process;
        ///  (b) Some compute workloads on either the async compute engine or on the universal engine have a threads per
        ///      threadgroup amount which exceeds maxAsyncComputeThreadGroupSize.
        ///
        /// It is up to the client to choose how to work around this bug. They are free to either limit applications to
        /// only creating compute shaders with <= maxAsyncComputeThreadGroupSize threads per group, or to avoid using
        /// the async compute engines at all.
        ///
        /// If this value equals maxThreadGroupSize, then the device does not have this bug and the client can use
        /// any compute shader on any queue.
        uint32 maxAsyncComputeThreadGroupSize;
        uint32 maxBufferViewStride; ///< Maximum stride, in bytes, that can be specified in a buffer view.
        uint32 gdsSize;             ///< Size of the GDS of the GPU in bytes.

        uint32 hardwareContexts;    ///< Number of distinct state contexts available for graphics workloads.  Mostly
                                    ///  irrelevant to clients, but may be useful to tools.
        uint32 ceRamSize;           ///< Maximum on-chip CE RAM size in bytes.

        union
        {
            struct
            {
                uint32 support8bitIndices                       : 1; ///< Hardware natively supports 8bit indices
                uint32 support16BitInstructions                 : 1; ///< Hardware supports FP16 and INT16 instructions
                uint32 supportDoubleRate16BitInstructions       : 1; ///< Hardware supports double rate packed math
                uint32 supportFp16Fetch                         : 1; ///< Hardware supports FP16 texture fetches
                uint32 supportConservativeRasterization         : 1; ///< Hardware supports conservative rasterization
                uint32 supportImplicitPrimitiveShader           : 1; ///< Device supports implicit compiling of the
                                                                     ///  hardware vertex shader as a primitive
                                                                     ///  shader to perform culling and compaction
                                                                     ///  optimizations in the shader.
                uint32 supportPrtBlendZeroMode                  : 1; ///< Blend zero mode support.
                uint32 supports2BitSignedValues                 : 1; ///< Hardware natively supports 2-bit signed
                                                                     ///  values.
                uint32 supportPrimitiveOrderedPs                : 1; ///< Hardware supports primitive ordered UAV
                                                                     ///  accesses in the PS.
                uint32 supportPatchTessDistribution             : 1; ///< Hardware supports patch level tessellation
                                                                     ///  distribution among VGTs.
                uint32 supportDonutTessDistribution             : 1; ///< Hardware supports donut granularity of
                                                                     ///  tessellation distribution among VGTs.
                uint32 supportTrapezoidTessDistribution         : 1; ///< Hardware supports trapezoid granularity of
                                                                     ///  tessellation distribution among VGTs.
                uint32 supportPerChannelMinMaxFilter            : 1; ///< Hardware returns min/max value on a
                                                                     ///  per-channel basis. If not set, min/max
                                                                     ///  filtering operates on only one channel at
                                                                     ///  a time.
                uint32 supportRgpTraces                         : 1; ///< Hardware supports RGP traces.
                uint32 placeholder0                             : 1; ///< Reserved for future hardware.

                uint32 placeholder1                             : 1; ///< Reserved for future hardware.

                uint32 placeholder2                             : 1; ///< Reserved for future hardware.

                uint32 supportSpp                               : 1; ///< Hardware supports Shader Profiling for Power.
                uint32 timestampResetOnIdle                     : 1; ///< GFX timestamp resets after idle between
                                                                     ///  submissions. The client cannot assume that
                                                                     ///  timestamps will increase monotonically across
                                                                     ///  command buffer submissions.
                uint32 placeholder3                             : 1; ///< Reserved for future hardware.
                uint32 support1xMsaaSampleLocations             : 1; ///< HW supports 1xMSAA custom quad sample patterns
                uint32 placeholder4                             : 1; ///< Placeholder, do not use
                uint32 reserved                                 : 10; ///< Reserved for future use.
            };
            uint32 u32All;           ///< Flags packed as 32-bit uint.
        } flags;                     ///< Device IP property flags.

        struct
        {
            uint32 bufferView;  ///< Size in bytes (and required alignment) of a buffer view SRD.
                                ///  @see IDevice::CreateTypedBufferViewSrds() and CreateUntypedBufferViewSrds().
            uint32 imageView;   ///< Size in bytes (and required alignment) of an image view SRD.
                                ///  @see IDevice::CreateImageViewSrds().
            uint32 fmaskView;   ///< Size in bytes (and required alignment) of an fmask view SRD.
                                ///  @see IDevice::CreateFmaskViewSrds().
            uint32 sampler;     ///< Size in bytes (and required alignment) of a sampler SRD.
                                ///  @see IDevice::CreateSamplerSrds().
        } srdSizes;             ///< Sizes for various types of _shader resource descriptor_ (SRD).

        struct
        {
            const void* pNullBufferView;  ///< Pointer to null buffer view srd
            const void* pNullImageView;   ///< Pointer to null image view srd
            const void* pNullFmaskView;   ///< Pointer to null fmask view srd
            const void* pNullSampler;     ///< Pointer to null sampler srd
        } nullSrds;                       ///< Null SRDs are used to drop shader writes or read 0

        struct
        {
            float  maxGpuClock;      ///< Maximum GPU engine clock in MHz. For DX builds this value is valid only after
                                     ///  the device has been finalized.
            float  aluPerClock;      ///< Maximum shader ALU operations per clock.
            float  texPerClock;      ///< Maximum texture fetches per clock.
            float  primsPerClock;    ///< Maximum primitives processed per clock.
            float  pixelsPerClock;   ///< Maximum pixels processed per clock.
            uint32 gfxipPerfRating;  ///< Precomputed performance rating of the GfxIp block.
        } performance;               ///< Performance-related device properties.

        struct
        {
            uint32  numShaderEngines;       ///< Number of shader engines.
            uint32  numShaderArrays;        ///< Number of shader arrays.
            uint32  numCusPerShaderArray;   ///< Number of CUs per shader array that are actually usable.
            uint32  maxCusPerShaderArray;   ///< Maximum number of CUs per shader array. Count of physical CUs prior to
                                            ///< harvesting CUs for yield in certain variants of ASICs (ex: Fiji PRO).
            uint32  numSimdsPerCu;          ///< Number of SIMDs per compute unit.
            uint32  numWavefrontsPerSimd;   ///< Number of wavefront slots in each SIMD.
            uint32  wavefrontSize;          ///< Wavefront size.
            uint32  numAvailableSgprs;      ///< Number of available SGPRs.
            uint32  sgprsPerSimd;           ///< Number of physical SGPRs per SIMD.
            uint32  minSgprAlloc;           ///< Minimum number of SGPRs that can be allocated by a wave.
            uint32  sgprAllocGranularity;   ///< SGPRs are allocated in groups of this size.  Meaning, if your shader
                                            ///  only uses 1 SGPR, you will still end up reserving this number of
                                            ///  SGPRs.
            uint32  numAvailableVgprs;      ///< Number of available VGPRs.
            uint32  vgprsPerSimd;           ///< Number of physical VGPRs per SIMD.
            uint32  minVgprAlloc;           ///< Minimum number of VGPRs that can be allocated by a wave.
            uint32  vgprAllocGranularity;   ///< VGPRs are allocated in groups of this size.  Meaning, if your shader
                                            ///  only uses 1 VGPR, you will still end up reserving this number of
                                            ///  VGPRs.
            uint32 ldsSizePerCu;            ///< Local Data Store size available in bytes per CU.
            uint32 ldsSizePerThreadGroup;   ///< Local Data Store size available in bytes per thread-group.
            uint32 ldsGranularity;          ///< Local Data Store allocation granularity expressed in bytes.
            uint32 gsPrimBufferDepth;       ///< Hardware configuration for the GS prim buffer depth.
            uint32 gsVgtTableDepth;         ///< Hardware configuration for the GS VGT table depth.
            uint32 offchipTessBufferSize;   ///< Size of each buffer used for passing data between shader stages when
                                            ///  tessellation passes data using off-chip memory.
            uint32 tessFactorBufSizePerSe;  ///< Size of GPU's the tessellatio-factor buffer, per shader engine.
            uint32 tccSizeInBytes;          ///< Size of total L2 TCC cache in bytes.
            uint32 tcpSizeInBytes;          ///< Size of one L1 TCP cache in bytes. There is one TCP per CU.
            uint32 maxLateAllocVsLimit;     ///< Maximum number of VS waves that can be in flight without
                                            ///  having param cache and position buffer space.

            gpusize primitiveBufferSize;    ///< Size of the offchip primitive buffer.  Zero indicates that no offchip
                                            ///  buffer is present.
            gpusize positionBufferSize;     ///< Size of offchip position buffer.  Zero indicates that no offchip buffer
                                            ///  is present.
            gpusize controlSidebandSize;    ///< Size of offchip control sideband buffer.  Zero indicates that no
                                            ///  offchip control sideband is present.
            gpusize parameterCacheSize;     ///< Size of offchip parameter cache buffer.  Zero indicates that no offchip
                                            ///  parameter cache is present.

        } shaderCore;                       ///< Properties of computational power of the shader engine.

    } gfxipProperties;

    struct
    {
        union
        {
            struct
            {
                uint32 supportTurboSync  :  1;    ///< Whether TurboSync is supported by KMD
                uint32 enableUmdFpsCap   :  1;    ///< Whether UMD FPS CAP enabled
                uint32 isCwgSupported    :  1;    ///< KMD supports Creator Who Game (CWG) feature
                uint32 isGamingDriver    :  1;    ///< KMD works in gaming mode

                uint32 reserved          : 28;    ///< Reserved for future use.
            };
            uint32 u32All;                        ///< Flags packed as 32-bit uint.
        } flags;                                  ///< OS-specific property flags.

        bool   supportOpaqueFdSemaphore; ///< Support export/import semaphore as opaque fd in linux KMD.
        bool   supportSyncFileSemaphore; ///< Support export/import semaphore as sync file in linux KMD.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 398
        bool   supportSyncFileFence;     ///< Support export/import fence as sync file in linux KMD.
#endif

        bool   supportQueuePriority;        ///< Support create queue with priority
        bool   supportDynamicQueuePriority; ///< Support set the queue priority through IQueue::SetExecutionPriority

        uint32                     umdFpsCapFrameRate;   ///< The frame rate of the UMD FPS CAP
        VirtualDisplayCapabilities virtualDisplayCaps;   ///< Capabilities of virtual display, it's provided by KMD
    } osProperties;                 ///< OS-specific properties of this device.

    struct
    {
        uint32 busNumber;                   ///< PCI bus number.
        uint32 deviceNumber;                ///< PCI device number.
        uint32 functionNumber;              ///< PCI function number.

        union
        {
            struct
            {
                uint32 gpuConnectedViaThunderbolt :  1; ///< Device is an externally housed GPU connected to the system
                                                        ///  via Thunderbolt. This will drastically impact CPU read and
                                                        ///  write performance of memory in the @ref GpuHeapLocal heap.
                uint32 gpuEmulatedInSoftware      :  1; ///< Device is really a software package which emulates the
                                                        ///  GPU.  This is meant for pre-silicon development.
                uint32 reserved                   : 30; ///< Reserved for future use.
            };
            uint32 u32All;                  ///< Flags packed as 32-bit uint.
        } flags;                            ///< PCI bus property flags.
    } pciProperties;                        ///< PCI bus properties of this device.

    BigSoftwareReleaseInfo bigSoftwareReleaseInfo;   ///< Big Software (BigSW) Release Version information
};

/// Defines callback function to notify client of private screen changes.
typedef void (PAL_STDCALL *TopologyChangeNotificationFunc)(void* pClient);

/// Defines callback function to notify client of the private screen removal.
typedef void (PAL_STDCALL *DestroyNotificationFunc)(void* pOwner);

/// Specifies the private screen topology change notification data.
struct PrivateScreenNotifyInfo
{
    void*                          pClient;       ///< Pointer to client, PAL use this pointer as parameter when PAL
                                                  ///  calls callback pfnOnTopology.
    TopologyChangeNotificationFunc pfnOnTopology; ///< Pointer to client provided function. PAL should call this when
                                                  ///  the topology change happens and let the client handle the change.
    DestroyNotificationFunc        pfnOnDestroy;  ///< Pointer to client provdided function. PAL should call this when
                                                  ///  a private screen object is to be destroyed. The pOwner data is
                                                  ///  passed at @ref IPrivateScreen::BindOwner() time.
};

/// Specifies properties for GDS partition allocation.  Input and output structure for IDevice::AllocateGds().
struct DeviceGdsAllocInfo
{
    uint32 gdsSizes[EngineTypeCount][MaxAvailableEngines]; ///< Specifies requested GDS size for individual engines
    bool   perPipelineBindPointGds;                        ///< If set, each engine will split up their GDS equally
                                                           ///  across all supported pipeline bind points. In practice
                                                           ///  this means that for universal queues there will be two
                                                           ///  equally sized partitions with the compute partition
                                                           ///  starting at offset zero and the graphics partition
                                                           ///  starting at the middle of the requested GDS partition's
                                                           ///  size for that universal engine.
};

/// Specifies fullscreen frame metadata control flags. Used for the KMD to notify clients about which types of frame
/// metadata it needs to send to KMD. The meaning depends on the context:
/// - During device finalization, client can set the flags indicating the specified metadata 'is supported' by client.
/// - During present, client can query these flags that indicate which metadata 'is enabled' currently so that the
///   client should send them to the KMD.
union FullScreenFrameMetadataControlFlags
{
    struct
    {
        uint32 timerNodeSubmission       :  1; ///< Timer node submission, used for cases such as FRTC/FP/PFPA.
        uint32 frameBeginFlag            :  1; ///< FrameBegin flag on CmdBufInfo, see CmdBufInfo for details.
        uint32 frameEndFlag              :  1; ///< FrameEnd flag on CmdBufInfo, see CmdBufInfo for details.
        uint32 primaryHandle             :  1; ///< Pending primary handle for pre-flip primary access (PFPA)
        uint32 p2pCmdFlag                :  1; ///< P2P copy command. See CmdBufInfo comments for details.
        uint32 forceSwCfMode             :  1; ///< Force software crossfire mode.
        uint32 postFrameTimerSubmission  :  1; ///< It indicates whether the timer node submission at frame N is to
                                               ///  synchronize the flip of frame N (postFrameTimerSubmission == TRUE)
                                               ///  or N+1 (postFrameTimerSubmission == FALSE).
                                               ///  It's only valid when timerNodeSubmission is also set.
        uint32 reserved                  : 25; ///< Reserved for future use.
    };
    uint32 u32All;    ///< Flags packed as 32-bit uint.
};

/// Indicates the desired UMD behavior with timer node submission.
/// This is used to distinguish FP Vsync On + FreeSync Off case from HSync or FreeSync cases, the former case doesn't
/// hold flip while the later cases do.
enum class TimerNodeMode : uint32
{
    Unspecified,       ///< Unspecified, client can decide what to do with the timer submission.
    ForceFlipHold,     ///< Client must hold flip with the timer submission
};

/// Specifies fullscreen frame metadata control data. Including FullScreenFrameMetadataControlFlags plus extended data.
/// According to KMD's design, the difference is that 'flags' can be used to indicate 'client caps' during device
/// initialization, while the 'data' is only passed from KMD to UMD.
struct PerSourceFrameMetadataControl
{
    FullScreenFrameMetadataControlFlags flags;                   ///< The frame metadata control flags
    TimerNodeMode                       timerNodeSubmissionMode; ///< Desired UMD behavior with timer node submission
};

/// Specifies the texture optimization level to use for an image.
///
/// @ingroup ResourceBinding
enum class ImageTexOptLevel : uint32
{
    Default = 0,        ///< Use device default setting
    Disabled,           ///< Disable texture filter optimization
    Enabled,            ///< Enable texture filter optimization
    Maximum,            ///< Maximum texture filter optimization
    Count
};

/// Specifies properties for @ref IDevice finalization.  Input structure to IDevice::Finalize().
struct DeviceFinalizeInfo
{
    union
    {
        struct
        {
            uint32 supportPrivateScreens        :  1; ///< Initializes private screen support.
            uint32 requireFlipStatus            :  1; ///< Requires to initialize flip status shared memory
            uint32 requireFrameMetadata         :  1; ///< Requires to initialize frame metadata flags shared memory.
                                                      ///  Clients should only set this flag on the master device in an
                                                      ///  LDA chain.
            uint32 internalGpuMemAutoPriority   :  1; ///< Forces internal GPU memory allocation priorities to be
                                                      ///  determined automatically. It is an error to set this flag
                                                      ///  if the device does not report that it supports this feature.
            uint32 reserved                     : 28; ///< Reserved for future use.
        };
        uint32 u32All;                    ///< Flags packed as 32-bit uint.
    } flags;                              ///< Device finalization flags.

    struct
    {
        uint32 engines; ///< A mask of which engines are requested.
    } requestedEngineCounts[EngineTypeCount]; /// Specifies which engines of each type should be created for the device.

    size_t ceRamSizeUsed[EngineTypeCount]; ///< Bytes of CE RAM to be used by the client for each engine type. This value
                                           ///  must be <= ceRamSizeAvailable reported for that engine type. In the case
                                           ///  where more than one engine of a given type is requested it is assumed
                                           ///  each engine of that type will use this amount of CE RAM so the total size
                                           ///  of (ceRamSizeUsed * queueCounts) must be <= ceRamSizeAvailable for that
                                           ///  engine type.  Each entry must be either zero or a multiple of 32 bytes.

    /// PAL provides several "indirect" user-data tables to the client for use within a command buffer.  Each of these
    /// tables resides in GPU memory and is fully managed by PAL, with the client able to update the tables' contents
    /// just like the normal user-data entries.  Typically, the contents of these tables are updated using the Constant
    /// Engine (on a universal queue), so the combined size of all three user-data tables _must_ be small enough to fit
    /// inside the space declared by the universal queue's @ref DeviceFinalizeInfo::ceRamSizeUsed byte amount.
    ///
    /// @see ResourceMappingNodeType::IndirectUserDataTableVaPtr
    /// @see ICmdBuffer::CmdSetIndirectUserData
    struct
    {
        size_t  sizeInDwords;       ///< Size of this indirect user-data table.  If this is zero, then this table will
                                    ///  be totally unavailable for use by any pipeline or command buffer.
        size_t  offsetInDwords;     ///< CE RAM offset of this indirect user-data table.  PAL may or may not always use
                                    ///  CE RAM to support these tables, but to be safe, these offsets should be chosen
                                    ///  such that multiple tables won't overlap in CE RAM.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 403
        uint32  ringSize;           ///< PAL manages the GPU memory for the indirect user-data tables with a ring
                                    ///  buffer.  This tells PAL the preferred number of 'instances' of the table which
                                    ///  the GPU ring buffer will have space for.  Typically, larger numbers will yield
                                    ///  improved performance at the expense of a larger GPU memory footprint.
#endif
    } indirectUserDataTable[MaxIndirectUserDataTables];

    /// @see PrivateScreenNotifyInfo
    /// Private screen notify info, must be filled when supportPrivateScreens=1. The client pointer and callback are to
    /// be saved in device. PAL should call the callback when there is any topology (hotplug) change with the client
    /// pointer as parameter.
    PrivateScreenNotifyInfo  privateScreenNotifyInfo;

    /// Fullscreen frame metadata control flags indicating the types of metadata that the client supports.
    /// During adapter initialization, capable KMD notifies clients that it supports frame metadata,
    /// clients should then set these flags on device finalization info, indicating which types of metadata the client
    /// supports.
    FullScreenFrameMetadataControlFlags supportedFullScreenFrameMetadata;

    /// Specify the texture optimization level which only applies to internally-created views by PAL (e.g., for BLTs),
    /// client-created views must use the texOptLevel parameter in ImageViewInfo.
    ImageTexOptLevel internalTexOptLevel;
};

/// Reports the compatibility and available features when using two particular devices in a multi-GPU system.  Output
/// structure from IDevice::GetMultiGpuCompatibility().
struct GpuCompatibilityInfo
{
    union
    {
        struct
        {
            uint32 gpuFeatures         :  1;  ///< The devices have an exact feature match: same internal tiling, same
                                              ///  pipeline binary data, etc.
            uint32 iqMatch             :  1;  ///< Devices produce images with same precision.
            uint32 peerTransfer        :  1;  ///< Peer-to-peer transfers are supported.  See IDevice::OpenPeerMemory()
                                              ///  and IDevice::OpenPeerImage().
            uint32 sharedMemory        :  1;  ///< Devices can share memory objects with.  IDevice::OpenSharedMemory().
            uint32 sharedSync          :  1;  ///< Devices can share queue semaphores with
                                              ///  IDevice::OpenSharedQueueSemaphore().
            uint32 shareThisGpuScreen  :  1;  ///< Either device can present to this device.  Means that the device
                                              ///  indicated by the otherDevice param in
                                              ///  IDevice::GetMultiGpuCompatibility() can present to the device the
                                              ///  method was called on.
            uint32 shareOtherGpuScreen :  1;  ///< Either device can present to the other device.  Means that the
                                              ///  device IDevice::GetMultiGpuCompatibility() was called on can present
                                              ///  to the GPU indicated by the otherGpu param.
            uint32 reserved            : 25;  ///< Reserved for future use.
        };
        uint32 u32All;                        ///< Flags packed as 32-bit uint.
    } flags;                                  ///< GPU compatibility flags.
};

/// Reports properties of a GPU memory heap.
///
/// @note The performance ratings represent an approximate memory throughput for a particular access scenario, but
///       should not be taken as an absolute performance metric.
struct GpuMemoryHeapProperties
{
    union
    {
        struct
        {
            uint32 cpuVisible       :  1;  ///< Accessible with IGpuMemory::Map()
            uint32 cpuGpuCoherent   :  1;  ///< Cache coherent between the CPU and GPU.
            uint32 cpuUncached      :  1;  ///< Not cached by CPU, but could still be GPU cached.
            uint32 cpuWriteCombined :  1;  ///< CPU write-combined memory.
            uint32 holdsPinned      :  1;  ///< GPU memory objects created by IDevice::CreatePinnedGpuMemory() are in
                                           ///  this heap.
            uint32 shareable        :  1;  ///< GPU memory objects in this heap can be shared between multiple devices.
            uint32 placeholder0     :  1;  ///< Placeholder.
            uint32 reserved         : 25;  ///< Reserved for future use.
        };
        uint32 u32All;                     ///< Flags packed as 32-bit uint.
    } flags;                               ///< GPU memory heap property flags.

    gpusize  heapSize;                     ///< Size of the heap in bytes. If HBCC is enabled, certain heaps may be
                                           ///< virtualized and the logical size will exceed the physical size.
    gpusize  physicalHeapSize;             ///< Physical size of the heap in bytes

    float    gpuReadPerfRating;            ///< Relative GPU read performance rating for this heap.
    float    gpuWritePerfRating;           ///< Relative GPU write performance rating for this heap.
    float    cpuReadPerfRating;            ///< Relative CPU read performance rating for this heap.
    float    cpuWritePerfRating;           ///< Relative GPU write performance rating for this heap.
};

/// Flags structure reporting available capabilities of a particular format.
enum FormatFeatureFlags : uint32
{
    FormatFeatureCopy                = 0x00001,  ///< Images of this format can be used as a copy source or destination.
    FormatFeatureFormatConversion    = 0x00002,  ///< Images of this format support format conversion in copy
                                                 ///  operations.
    FormatFeatureImageShaderRead     = 0x00004,  ///< Images of this format can be read from a shader.
    FormatFeatureImageShaderWrite    = 0x00008,  ///< Images of this format can be written from a shader.
    FormatFeatureImageShaderAtomics  = 0x00010,  ///< Images of this format can be written atomically from a shader.
    FormatFeatureMemoryShaderRead    = 0x00020,  ///< Memory views of this format can be read from a shader.
    FormatFeatureMemoryShaderWrite   = 0x00040,  ///< Memory views of this format can be written from a shader.
    FormatFeatureMemoryShaderAtomics = 0x00080,  ///< Memory views of this format can be written atomically from a
                                                 ///  shader.
    FormatFeatureColorTargetWrite    = 0x00100,  ///< Images of this format can be bound as a color target.
    FormatFeatureColorTargetBlend    = 0x00200,  ///< Images of this format can be bound as a color target for blending.
    FormatFeatureDepthTarget         = 0x00400,  ///< Images of this format can be bound as a depth target.
    FormatFeatureStencilTarget       = 0x00800,  ///< Images of this format can be bound as a stencil target.
    FormatFeatureMsaaTarget          = 0x01000,  ///< Images of this format can support multisampling.
    FormatFeatureWindowedPresent     = 0x02000,  ///< Images of this format can support windowed-mode presents.
                                                 ///  Fullscreen present capability is queried using the @ref
                                                 ///  IScreen::GetScreenModeList method.
    FormatFeatureImageFilterLinear   = 0x04000,  ///< Images of this format can be linearly filtered.
    FormatFeatureImageFilterMinMax   = 0x08000,  ///< Images of this format can be min/max filtered.
    FormatFeatureFormatConversionSrc = 0x10000,  ///< Images of this format support format conversion in copy
                                                 ///  operations as the source image.
                                                 ///  @note This is aliased to FormatFeatureFormatConversionDst for
                                                 ///  backwards compatibility.
    FormatFeatureFormatConversionDst = 0x20000,  ///< Images of this format support format conversion in copy
                                                 ///  operations as the destination image.
                                                 ///  @note This is aliased to FormatFeatureFormatConversionSrc for
                                                 ///  backwards compatibility.
};

/// Enumeration for indexing into the format properties table based on tiling.
enum FormatPropertiesTiling : uint32
{
    IsLinear  = 0,  ///< Format properties requested is for linearly-tiled surfaces.
    IsNonLinear,    ///< Format properties requested is for non-linearly tiled surfaces.
    Count,          ///< Number of format property tile types.
};

/// The format properties lookup table.  Contains information about which device access features are available for all
/// formats and tiling modes.  The tiling features for non-linear tiling modes are identical so we only store linear
/// and non-linear tiling features.  From left to right, it is indexed by format and "is-non-linear".
/// Returned by IDevice::GetFormatProperties().
struct MergedFormatPropertiesTable
{
    FormatFeatureFlags features[static_cast<size_t>(ChNumFormat::Count)][FormatPropertiesTiling::Count];
};

/// Reports properties of a specific GPU block required for interpretting performance experiment data from that block.
/// See @ref PerfExperimentProperties.
struct GpuBlockPerfProperties
{
    bool     available;                ///< If performance data is available for this block.
    uint32   instanceCount;            ///< How many instances of this block are in the device.
    uint32   maxEventId;               ///< Maximum event ID for this block.
    uint32   maxGlobalOnlyCounters;    ///< Number of counters available only for global counts.
    uint32   maxGlobalSharedCounters;  ///< Total counters available per instance of the block.
    uint32   maxSpmCounters;           ///< Counters available for streaming only.
};

/// Reports performance experiment capabilities of a device.  Returned by IDevice::GetPerfExperimentProperties().
struct PerfExperimentProperties
{
    PerfExperimentDeviceFeatureFlags features;                              ///< Performance experiment device features.
    size_t                 maxSqttSeBufferSize;                             ///< SQTT buffer size per shader engine.
    size_t                 sqttSeBufferAlignment;                           ///< SQTT buffer size and base address
                                                                            ///  alignment.
    uint32                 shaderEngineCount;                               ///< Number of shader engines.
    GpuBlockPerfProperties blocks[static_cast<size_t>(GpuBlock::Count)];    ///< Reports availability and properties of
                                                                            ///  each device block.
};

/// Reports maximum alignments for images created with a @ref ImageTiling::Linear tiling mode assuming the images'
/// elements are no larger than maxElementSize.
struct LinearImageAlignments
{
    uint16 maxElementSize; ///< Maximum element size in bytes.
    uint16 baseAddress;    ///< Minimum required base address alignment in bytes.
    uint16 rowPitch;       ///< Minimum required row pitch alignment in bytes.
    uint16 depthPitch;     ///< Minimum required depth pitch alignment in bytes.
};

/// Specifies image view type (i.e., 1D, 2D, 3D, or cubemap).
///
/// @ingroup ResourceBinding
enum class ImageViewType : uint32
{
    Tex1d    = 0x0,
    Tex2d    = 0x1,
    Tex3d    = 0x2,
    TexCube  = 0x3,
    TexQuilt = 0x4,

    Count
};

/// Enumeration which defines the mode for magnification and minification sampling
///
/// @ingroup ResourceBinding
enum XyFilter : uint32
{
    XyFilterPoint = 0,          ///< Use single point sampling
    XyFilterLinear,             ///< Use linear sampling
    XyFilterAnisotropicPoint,   ///< Use anisotropic with single point sampling
    XyFilterAnisotropicLinear,  ///< Use anisotropic with linear sampling
    XyFilterCount
};

/// Enumeration which defines the mode for volume texture sampling
///
/// @ingroup ResourceBinding
enum ZFilter : uint32
{
    ZFilterNone = 0,            ///< Disable Z filtering
    ZFilterPoint,               ///< Use single point sampling
    ZFilterLinear,              ///< Use linear sampling
    ZFilterCount
};

/// Enumeration which defines the mode for mip-map texture sampling
///
/// @ingroup ResourceBinding
enum MipFilter : uint32
{
    MipFilterNone = 0,          ///< Disable Mip filtering
    MipFilterPoint,             ///< Use single point sampling
    MipFilterLinear,            ///< Use linear sampling
    MipFilterCount
};

/// Specifies parameters for an image view descriptor controlling how a given texture is sampled
///
/// @ingroup ResourceBinding
struct TexFilter
{
    union
    {
        struct
        {
            uint32 magnification : 2;  ///< Used with enum XyFilter for Plane magnification filtering
            uint32 minification  : 2;  ///< Used with enum XyFilter for Plane minification filtering
            uint32 zFilter       : 2;  ///< Used with enum ZFilter for volume texture filtering
            uint32 mipFilter     : 2;  ///< Used with enum MipFilter for mip-map filtering
            uint32 reserved      : 24; ///< Reserved for future use
        };
        uint32 u32All;                 ///< Value of flags bitfield
    };
};

/// Determines if "TexFilter" should be ignored or not.
enum class TexFilterMode : uint32
{
    Blend = 0x0, ///< Use the filter method specified by the TexFilter enumeration
    Min   = 0x1, ///< Use the minimum value returned by the sampler, no blending op occurs
    Max   = 0x2, ///< Use the maximum value returned by the sampler, no blending op occurs
};

/// Specifies how texture coordinates outside of texture boundaries are interpreted.
///
/// @ingroup ResourceBinding
enum class TexAddressMode : uint32
{
    Wrap        = 0x0,  ///< Repeat the texture.
    Mirror      = 0x1,  ///< Mirror the texture by flipping it at every other coordinate interval.
    Clamp       = 0x2,  ///< Clamp the texture to the texture's edge pixel.
    MirrorOnce  = 0x3,  ///< Mirror the texture once then clamp.
    ClampBorder = 0x4,  ///< Clamp the texture to the border color specified in the sampler.
    Count
};

/// Specifies how a border color should be chosen when the TexAddressClampBorder texture addressing is used by a
/// sampler.
///
/// @ingroup ResourceBinding
enum class BorderColorType : uint32
{
    White            = 0x0,  ///< White border color (1.0, 1.0, 1.0, 1.0).
    TransparentBlack = 0x1,  ///< Transparent black border color (0.0, 0.0, 0,0, 0.0).
    OpaqueBlack      = 0x2,  ///< Opaque black border color (0.0, 0.0, 0.0, 1.0).
    PaletteIndex     = 0x3,  ///< Fetch border color from the border color palette.
    Count
};

/// Specifies parameters for a buffer view descriptor that control how a range of GPU memory is viewed by a shader.
///
/// Input to either CreateTypedBufferViewSrds() or CreateUntypedBufferViewSrds().  Used for any buffer descriptor,
/// including read-only shader resources, UAVs, vertex buffers, etc.  The usage of stride and format depends on the
/// expected shader instruction access:
///
/// + _Typed buffer_ access must set a valid format and channel mapping, and the value of stride must be equal
///   to the format's element size.
/// + _Raw buffer_ access is indicated by setting an invalid format and setting stride to 1.
/// + _Structured buffer_ access is indicated by setting an invalid format and setting stride to any value except 1.  A
///   stride of 0 maps all view accesses to the first structure stored in memory.
///
/// _Typed buffer_ SRD's must be created using @ref IDevice::CreateTypedBufferViewSrds().
/// _Raw buffer_ and _structured buffer_ SRD's must be created using @ref IDevice::CreateUntypedBufferViewSrds().
///
/// If necessary, PAL will adjust the out of bounds read/write behavior to match the client's API requirements based on
/// the client defines - PAL_CLIENT_VULKAN, etc.
///
/// @ingroup ResourceBinding
struct BufferViewInfo
{
    gpusize         gpuAddr;        ///< GPU memory virtual address where the buffer view starts, in bytes.
    gpusize         range;          ///< Restrict the buffer view to this many bytes.  Will be rounded down to a
                                    ///< multiple of the stride.
    gpusize         stride;         ///< Stride in bytes. Must match the bytes-per-element of the view format for typed
                                    ///  access.
    SwizzledFormat  swizzledFormat; ///< Format and channel swizzle for typed access. Must be Undefined for structured
                                    ///  or raw access.
};

/// Specifies parameters for an image view descriptor controlling how a shader will view the specified image.
///
/// Input to CreateImageViewSrd().  Used for any image view descriptor, including read-only shader resources and UAVs.
///
/// @ingroup ResourceBinding
struct ImageViewInfo
{
    const IImage*  pImage;         ///< Image associated with the view.
    ImageViewType  viewType;       ///< 1D, 2D, 3D, or Cubemap.  Typically this should match the image type, but a
                                   ///  Cubemap view can be imposed on a 2D array image.  TexQuilt allowed only if the
                                   ///  imageProperties.flags.supportsQuilting flag is set.
    SwizzledFormat swizzledFormat; ///< Specifies the image view format and channel swizzle. Must be compatible (same
                                   ///  bit-widths per channel) with the image's base format.
                                   ///  @note: YUV formats are invalid for an ImageView. A format should be chosen to be
                                   ///  compatible with either the luma or chroma plane(s) of the YUV format.
    SubresRange    subresRange;    ///< Specifies a subset of subresources to include in the view.  If the base Image
                                   ///  has a YUV planar format, the number of array slices in the range must be 1.
    float          minLod;         ///< Minimum mip level of detail to use for this view.

    uint32         quiltWidthInSlices; ///< Width of a quilted surface.  Only used if viewType == TexQuilt.
                                       ///  Must be a power of 2.

    uint32         samplePatternIdx;  ///< Index into the currently bound MSAA sample pattern palette to be
                                      ///  read/evaluated when samplepos shader instructions are executed on this
                                      ///  view.  Can be ignored if the samplepos shadinstruction will not be used.
                                      ///  Must be less than MaxSamplePatternPaletteEntries.  See
                                      ///  IDevice::SetSamplePatternPalette().
    Range          zRange;            ///< Specifies the z offset and z range.

    ImageTexOptLevel texOptLevel;     ///< Specific the texture optimization level.

    union
    {
        struct
        {
            uint32 shaderWritable : 1;  ///< True if used with an image that has been transitioned to a shader-
                                        ///  writable image state (e.g. [Graphics|Compute][WriteOnly|ReadWrite])
            uint32 zRangeValid    : 1;  ///< whether z offset/ range value is valid.
            uint32 includePadding : 1;  ///< Whether internal padding should be included in the view range.
            uint32 reserved       : 29; ///< Reserved for future use
        };
        uint32 u32All;                  ///< Value of flags bitfield
    } flags;                            ///< Image view flags.
};

/// Specifies parameters controlling execution of sample instructions in a shader.  Input to CreateSamplerSrd().
///
/// @ingroup ResourceBinding
struct SamplerInfo
{
    TexFilterMode   filterMode;               ///< Min/max filtering modes
    TexFilter       filter;                   ///< Filtering to apply to texture fetches.
    TexAddressMode  addressU;                 ///< Addressing mode for U texture coords outside of the [0..1] range.
    TexAddressMode  addressV;                 ///< Addressing mode for V texture coords outside of the [0..1] range.
    TexAddressMode  addressW;                 ///< Addressing mode for W texture coords outside of the [0..1] range.
    float           mipLodBias;               ///< Bias for mipmap level of detail selection.
    uint32          maxAnisotropy;            ///< Anisotropy value clamp when the filter mode is TexFilterAnisotropic.
    CompareFunc     compareFunc;              ///< Comparison function to apply to fetched data.
    float           minLod;                   ///< High-resolution mipmap LOD clamp.
    float           maxLod;                   ///< Low-resolution mipmap LOD clamp.
    BorderColorType borderColorType;          ///< Selects border color when an address mode is TexAddressClampBorder.
    uint32          borderColorPaletteIndex;  ///< Choose color from the border color palette when borderColorType is
                                              ///  BorderColorPalette.
    float anisoThreshold;                     ///< Opt-in, flags.useAnisoThreshold == 1 and flags.preciseAniso == 0.
                                              ///  The value should be computed taking account the maxAnisotropy
                                              ///  setting. This is a high resolution value which is quantized and
                                              ///  clamped down to 3 bits to the domain [0.0, 0.875] for current Hw.
                                              ///  We can interpret the functioning of the threshold value as follows.
                                              ///  maxAnisotropy per-pixel can be 1, 2, 4, 8 or 16 (N).
                                              ///  During sampling, the initial count (or S) is computed in Hw for each
                                              ///  quad and the domain for current Hw is [0-16].
                                              ///  Final sample count = min(pow(2, ceil(log2(S - anisoThreshold))), N)
                                              ///  Note: when flags.useAnisoThreshold == 0, Pal will ignore this value
                                              ///  and instead use a maximum of 0.25 at the highest anisotropic setting.
                                              ///  It is important to be aware that this feature tunes quality vs
                                              ///  performance, so care should be taken to not degrade image quality
                                              ///  'noticeably' when enabling using this feature
    uint32 perfMip;                           ///< Controls the value of the PERF_MIP field in Sampler SRD's.
                                              ///  This field basically controls the Fractional part of the LOD
                                              ///  calculation. if LOD is fractional so let us say 1.23, in this case
                                              ///  you must avg.out your samples from both MIP 1 and 2.But if PERF_MIP
                                              ///  is set to nonzero the HW will perform an optimization and may fetch
                                              ///  from only 1 MIP.
    union
    {
        struct
        {
            uint32 mgpuIqMatch         : 1;  ///< Enables image compatibility for MGPU scenarios where paired devices
                                             ///  come from different hardware families.
            uint32 preciseAniso        : 1;  ///< Anisotropic filtering should prefer precision over speed.
            uint32 unnormalizedCoords  : 1;  ///< If set then always use unnormalized texture coordinates instead of
                                             ///  zero to one.  Only works under certain conditions (no mip filtering,
                                             ///  no computed LOD, no offsets, only edge or border clamp address modes)
            uint32 truncateCoords      : 1;  ///< If set then hardware will truncate mantissa instead of
                                             ///  rounding to nearest even in float point to fixed point
                                             ///  texture coordinate conversion
            uint32 seamlessCubeMapFiltering : 1;  ///< If set then there's filtering across the edges of the cube map.
            uint32 prtBlendZeroMode    : 1;  ///< Allow unmapped PRT texels to be treated as zero and blended with
                                             ///  mapped texels. If set to 0, the destination of the sample instruction
                                             ///  is written with all 0s when TFE == 0; if set to 1, Treat unmapped
                                             ///  texels as zeros and blend them with other mapped texels, write the
                                             ///  result of this sample instruction to the destination GPRs.
            uint32 dx9Mipclamping      : 1;  ///< Specifies whether to follow dx9 spec to clamp mip id in hardware.
                                             ///  DX10+ requires a 0.5 offset before clamping, while DX9 does not.
            uint32 useAnisoThreshold   : 1;  ///< If set, Hw will use the value assigned in anisoThreshold, but
                                             ///  only if preciseAniso is set to 0, also.
            uint32 reserved            : 24; ///< Reserved for future use
        };
        uint32 u32All;                ///< Value of flags bitfield
    } flags;
};

/// Specifies parameters for an fmask view descriptor.
///
/// Input to CreateFmaskViewSrd().  Allows the client to access fmask from a shader using the load_fptr IL instruction.
///
/// @ingroup ResourceBinding
struct FmaskViewInfo
{
    const IImage* pImage;          ///< Image associated with the fmask view.
    uint32        baseArraySlice;  ///< First slice in the view.
    uint32        arraySize;       ///< Number of slices in the view.

    union
    {
        struct
        {
            uint32 shaderWritable : 1;  ///< True if used with an image that has been transitioned to a shader-
                                        ///  writable image state (e.g. [Graphics|Compute][WriteOnly|ReadWrite])
            uint32 reserved       : 31; ///< Reserved for future use
        };
        uint32     u32All;              ///< Value of flags bitfield
    } flags;                            ///< Fmask view flags
};

/// Element of the multisample pattern representing a sample position (X, Y), type of SamplePatternPalette, which
/// matches the layout defined by SC.
struct SamplePos
{
    float x;                  ///< x coordinate of sample position.
    float y;                  ///< y coordinate of sample position.
    uint32 reserved1;         ///< reserved for future use
    uint32 reserved2;         ///< reserved for future use
};

/// Specifies a palette of MSAA sample patterns used by the client.  Input to SetSamplePatternPalette, which is used
/// to implement samplepos shader instruction support.
typedef SamplePos SamplePatternPalette[MaxSamplePatternPaletteEntries][MaxMsaaRasterizerSamples];

/// Reports a current GPU timestamp along with a current CPU clock value, for use in calibrating CPU and GPU timelines.
struct GpuTimestampCalibration
{
    uint64       gpuTimestamp;       ///< Current GPU timestamp value compatible with ICmdBuffer::CmdWriteTimestamp().
    union
    {
        uint64   cpuWinPerfCounter;  ///< Current CPU performance counter value at the time of the corresponding GPU
                                     ///  timestamp.  This is a Windows-specific value as returned by
                                     ///  [QueryPerformanceCounter](http://tinyurl.com/9a45puz).
    };
};

/// Specifies connector types
enum class DisplayConnectorType : uint32
{
    Unknown = 0,    ///< Unknown connector type
    Vga,            ///< VGA
    DviD,           ///< DVI_D
    DviI,           ///< DVI_I
    Hdmi,           ///< HDMI
    Dp,             ///< DP
    Edp,            ///< EDP
    Minidp,         ///< MINI_DP
    Count
};

/// Specifies properties for display connectors connected to GPU
struct DisplayConnectorProperties
{
    DisplayConnectorType type;        ///< Connector type - VGA, DVI, HDMI, DP etc
};

/// Specifies pre-defined power profile which is used to communicate with KMD/PPLib and set correspond power states.
enum class PowerProfile : uint32
{
    Default   = 0,      ///< Default power profile.
    VrCustom  = 1,      ///< Power profile used by custom VR scenario.
    VrDefault = 2,      ///< Power profile used by default VR scenario.
    Idle      = 3,      ///< Power profile used for forced DPM0, in case HMD is taken off but the game is still running.
};

/// Fine-grain power switch info.
struct PowerSwitchInfo
{
    uint32 time;        ///< Time in microseconds, relative to the frame start at V-sync. Clients should consider the
                        ///  powerSwitchLatency value reported in @ref PrivateScreenProperties when specifying
                        ///  switch times.

    uint32 performance; ///< Performance to be set (between 0-100), which is mapped to a certain DPM level by KMD.
};

/// Maximum number of power switch info allowed in one custom power profile.
static constexpr uint32 MaxNumPowerSwitchInfo = 5;

/// Fine-grain power management for dynamic power mode. This structure specifies multiple DPM states to be cycled
/// through each frame.
struct CustomPowerProfile
{
    IPrivateScreen*  pScreen;       ///< Dynamic power mode needs V-sync so a private screen object is needed.
    uint32           numSwitchInfo; ///< Number of discrete DPM states to cycle through per frame.  Number entries
                                    ///  in switchInfo[] and actualSwitchInfo[].

    PowerSwitchInfo  switchInfo[MaxNumPowerSwitchInfo];  ///< Specifies the set of power states to cycle through each
                                                         ///  frame. Each entry specifies an offset into the frame where
                                                         ///  the DPM state should be switched, and a rough performance
                                                         ///  requirement value which will be translated into an
                                                         ///  appropriate DPM state by KMD.
    PowerSwitchInfo  actualSwitchInfo[MaxNumPowerSwitchInfo]; ///< The actual set of power states that KMD/PPLib sets.
};

/// Flags for IDevice::AddGpuMemoryReferences().  Depending on their residency model, a client may set these flags as
/// directed by the application or hard-code them to a single value.  Driver-internal memory references should be marked
/// as CantTrim unless the client explicitly handles trim support.
///
/// Note that the CantTrim and MustSucceed flags are based on the same WDDM2 flags; it is expected that PAL will ignore
/// them on non-WDDM2 platforms.
enum GpuMemoryRefFlags : uint32
{
    GpuMemoryRefCantTrim    = 0x1, ///< The caller can't or won't free this allocation on OS request.
    GpuMemoryRefMustSucceed = 0x2, ///< Hint to the OS that we can't process a failure here, this may result in a TDR.
};

/// Specifies input arguments for IDevice::GetPrimaryInfo(). Client must specify a display ID and properties of the
/// primary surface that will drive that display in order to query capabilities.
struct GetPrimaryInfoInput
{
    uint32          vidPnSrcId;                 ///< Video present source id.
    uint32          width;                      ///< Primary surface width.
    uint32          height;                     ///< Primary surface height.
    SwizzledFormat  swizzledFormat;             ///< Format and swizzle of the primary surface.
    Rational        refreshRate;                ///< Video refresh rate, this is only valid if refreshRateValid is set.
    union
    {
        struct
        {
            uint32 qbStereoRequest              :  1;   ///< Going to set a stereo mode.
            uint32 refreshRateValid             :  1;   ///< Refresh rate is valid.
            uint32 freeSyncInCrossFireSupport   :  1;   ///< True if client supports FreeSync in CrossFire.
            uint32 useKmdCalcFramePacing        :  1;   ///< True if client uses KMD frame pacing. If so, the client
                                                        ///  creates a timer queue to delay the present, and the delay
                                                        ///  value is calculated by KMD.
            uint32 reserved                     : 28;   ///< Reserved for future use.
        };
        uint32 u32All;                      ///< Flags packed as 32-bit uint.
    } flags;                                ///< get primary surface info input flags.
};

/// Specifies output arguments for IDevice::GetStereoDisplayModes(), returning supported stereo mode
struct StereoDisplayModeOutput
{
    Extent2d          extent;           ///< Dimensions in pixels WxH.
    Rational          refreshRate;      ///< Refresh rate.
    SwizzledFormat    format;           ///< Format and swizzle of the primary surface.
};

/// Specifies output arguments for IDevice::GetActive10BitPackedPixelMode(), returning which, if any, 10-bit
/// display mode is active.
struct Active10BitPackedPixelModeOutput
{
    bool             isInWs10BitMode;           ///< Whether the workstation 10-bit feature is enabled.
    bool             notifyKmd10bitsPresent;    ///< When in 10-bit mode and at present time, if the
                                                ///  client driver sees a 10-bit to 8-bit surface blt,
                                                ///  it needs to call RequestKmdReinterpretAs10Bit() to
                                                ///  inform the KMD that the dst surface must be reinterpreted
                                                ///  as 10-bits per channel for all KMD-initiated BLTs.
    PackedPixelType  packedPixelType;           ///< Format of the packed pixels.
    uint32           pixelPackRatio;            ///< The number of 10-bit pixels that are packed into one 8-8-8-8
                                                ///  format pixel.
};

/// Specifies primary surface stereo mode.
enum StereoMode : uint32
{
    StereoModeHwAlignedViews = 0,  ///< The stereo views are HW aligned on the display.
    StereoModeSwPackedViews  = 1,  ///< The layout of the stereo views on the display are determined by the client.
    StereoModeNotSupported   = 2,  ///< Not support stereo mode
    StereoModeSideBySide     = 3,  ///< The two stereo views are put side by side on the display.
    StereoModeTopBottom      = 4   ///< One stereo view is on the top of the display, and the other is on the bottom.
};

/// Specifies output arguments for IDevice::GetPrimaryInfo(), returning capabilitiy information for a display in
/// a particular mode.
struct GetPrimaryInfoOutput
{
    uint32          tilingCaps;                ///< Tiling caps supported by this primary surface.
    StereoMode      stereoMode;                ///< Stereo mode supported by this primary surface.
    union
    {
        struct
        {
            /// MGPU flag: this primary surface supports DVO HW compositing mode.
            uint32 dvoHwMode                    :  1;
            /// MGPU flag: this primary surface supports XDMA HW compositing mode.
            uint32 xdmaHwMode                   :  1;
            /// MGPU flag: this primary surface supports client doing SW compositing mode.
            uint32 swMode                       :  1;
            /// MGPU flag: this primary surface supports freesync.
            uint32 isFreeSyncEnabled            :  1;
            /// SGPU flag: gives hint to the client that they should use rotated tiling mode.
            uint32 hwRotationPortraitMode       :  1;
            /// SGPU flag: this primary surface supports non local heap.
            uint32 displaySupportsNonLocalHeap  :  1;
            /// Reserved for future use.
            uint32  reserved                    : 26;
        };
        uint32 u32All;  ///< Flags packed as 32-bit uint.
    } flags;            ///< get primary surface support info output flags.
};

/// Specifies different clock modes that the device can be set to.
enum class DeviceClockMode : uint32
{
    Default        = 0,   ///< Device clocks and other power settings are restored to default.
    Query          = 1,   ///< Queries the current device clock ratios. Leaves the clock mode of the device unchanged.
    Profiling      = 2,   ///< Scale down from peak ratio. Clocks are set to a constant amount which is
                          ///  known to be power and thermal sustainable. The engine/memory clock ratio
                          ///  will be kept the same as much as possible.
    MinimumMemory  = 3,   ///< Memory clock is set to the lowest available level. Engine clock is set to
                          ///  thermal and power sustainable level.
    MinimumEngine  = 4,   ///< Engine clock is set to the lowest available level. Memory clock is set to
                          ///  thermal and power sustainable level.
    Peak           = 5,   ///< Clocks set to maximum when possible. Fan set to maximum. Note: Under power
                          ///  and thermal constraints device will clock down.
    QueryProfiling = 6,   ///< Queries the profiling device clock ratios. Leaves the clock mode of the device unchanged.
    QueryPeak      = 7,   ///< Queries the peak device clock ratios. Leaves the clock mode of the device unchanged.
    Count
};

/// Specifies input argument to IDeive::SetClockMode. The caller can read the clock ratios the device is currently
/// running by querying using the mode DeviceClockMode::DeviceClockModeQuery.
struct SetClockModeOutput
{
    float memoryClockRatioToPeak;  ///< Ratio of current mem clock to peak clock as obtained from
                                   ///  DeviceProperties::maxMemClock.
    float engineClockRatioToPeak;  ///< Ratio of current gpu core clock to peak clock as obtained from
                                   ///  DeviceProperties::maxGpuClock.
};

/// Specifies input argument to IDeive::SetClockMode. The caller must specify the mode in which to set the device.
struct SetClockModeInput
{
    DeviceClockMode clockMode; ///< Used to specify the clock mode for the device.
};

/// Specifies primary surface MGPU compositing mode.
enum MgpuMode : uint32
{
    MgpuModeOff  = 0,  ///< MGPU compositing mode off, the client does not do SW compositing at all, e.g. AFR disabled.
    MgpuModeSw   = 1,  ///< MGPU SW compositing mode, the client handle the SW compositing.
    MgpuModeDvo  = 2,  ///< MGPU DVO HW compositing mode
    MgpuModeXdma = 3   ///< MGPU XDMA HW compositing mode
};

/// Specifies input arguments for IDevice::SetMgpuMode(). A client set a particular MGPU compositing mode and whether
/// frame pacing is enabled for a display.
struct SetMgpuModeInput
{
    uint32      vidPnSrcId;             ///< Video present source id.
    MgpuMode    mgpuMode;               ///< Primary surface MGPU compositing mode.
    bool        isFramePacingEnabled;   ///< True if frame pacing enabled. If so, the client creates a timer queue
                                        ///  to delay the present, and the delay value is calculated by KMD.
};

constexpr uint32 XdmaMaxDevices = 8;    ///< Maximum number of Devices for XDMA compositing.

/// Specifies XDMA cache buffer info for each gpu.
struct XdmaBufferInfo
{
    uint32 bufferSize;      ///< XDMA cache buffer size of each device
    uint32 startAlignment;  ///< XDMA cache buffer start alignment of each device
};

/// Specifies output arguments for IDevice::GetXdmaInfo(), returning the XDMA cache buffer information of each GPU for
/// a display.
struct GetXdmaInfoOutput
{
    XdmaBufferInfo  xdmaBufferInfo[XdmaMaxDevices]; ///< Output XDMA cache buffer info
};

/// Specifies flipping status flags on a specific VidPnSource. It's Windows specific.
union FlipStatusFlags
{
    struct
    {
        uint32 immediate : 1;  ///< Is immediate flip
        uint32 dwmFlip   : 1;  ///< Is DWM conducted flip
        uint32 iFlip     : 1;  ///< Is independent exclusive flip
        uint32 reserved  : 29; ///< Reserved for future use.
    };
    uint32 u32All;             ///< Flags packed as 32-bit uint.
};

/// Specifies the VSync mode of virtual display.
enum class VirtualDisplayVSyncMode : uint32
{
    Default   = 0,  ///< Using the default VSync mode based on refresh rate
    Immediate = 1,  ///< The presentation should be executed immediately without waiting for vsync to display
    HMD       = 2,  ///< Using HMD VSync, the HMD is specified by pPrivateScreen
    Count
};

/// The VirtualDisplayInfo is provided by application and KMD uses it to create a virtual display.
/// @see IDevice::CreateVirtualDisplay.
struct VirtualDisplayInfo
{
    uint32                  width;              ///< Horizontal dimension in pixels
    uint32                  height;             ///< Vertical dimension in pixels
    Rational                refreshRate;        ///< Refresh rate of virtual display
    VirtualDisplayVSyncMode vsyncMode;          ///< VSync mode
    uint32                  vsyncOffset;        ///< VSync front porch location in pixels or lines.
                                                ///  It's needed when VSyncMode is HMD
    Pal::IPrivateScreen*    pPrivateScreen;     ///< A pointer to IPrivateScreen.
                                                ///  It's needed when VSyncMode is HMD
};

/// Function pointer type definition for creating a buffer view SRD.
///
/// @see IDevice::CreateTypedBufferViewSrds()/CreateUntypedBufferViewSrds().
///
/// @param [in]  pDevice         Pointer to the device this function is called on.
/// @param [in]  count           Number of buffer view SRDs to create; size of the pBufferViewInfo array.
/// @param [in]  pBufferViewInfo Array of buffer view descriptions directing SRD construction.
/// @param [out] pOut            Client-provided space where opaque, hardware-specific SRD data is written.
///
/// @ingroup ResourceBinding
typedef void (PAL_STDCALL *CreateBufferViewSrdsFunc)(
    const IDevice*        pDevice,
    uint32                count,
    const BufferViewInfo* pBufferViewInfo,
    void*                 pOut);

/// Function pointer type definition for creating an image view SRD.
///
/// @see IDevice::CreateImageViewSrds().
///
/// @param [in]  pDevice      Pointer to the device this function is called on.
/// @param [in]  count        Number of buffer view SRDs to create; size of the pImageViewInfo array.
/// @param [in]  pImgViewInfo Array of image view descriptions directing SRD construction.
/// @param [out] pOut         Client-provided space where opaque, hardware-specific SRD data is written.
///
/// @ingroup ResourceBinding
typedef void (PAL_STDCALL *CreateImageViewSrdsFunc)(
    const IDevice*       pDevice,
    uint32               count,
    const ImageViewInfo* pImgViewInfo,
    void*                pOut);

/// Function pointer type definition for creating a fmask view SRD.
///
/// @see IDevice::CreateFmaskViewSrds().
///
/// @param [in]  pDevice        Pointer to the device this function is called on.
/// @param [in]  count          Number of fmask view SRDs to create; size of the pFmaskViewInfo array.
/// @param [in]  pFmaskViewInfo Array of fmask view descriptions directing SRD construction.
/// @param [out] pOut           Client-provided space where opaque, hardware-specific SRD data is written.
///
/// @ingroup ResourceBinding
typedef void (PAL_STDCALL *CreateFmaskViewSrdsFunc)(
    const IDevice*       pDevice,
    uint32               count,
    const FmaskViewInfo* pFmaskViewInfo,
    void*                pOut);

/// Function pointer type definition for creating a sampler SRD.
///
/// @see IDevice::CreateSamplerSrds().
///
/// @param [in]  pDevice      Pointer to the device this function is called on.
/// @param [in]  count        Number of sampler SRDs to create; size of the pSamplerInfo array.
/// @param [in]  pSamplerInfo Array of sampler descriptions directing SRD construction.
/// @param [out] pOut         Client-provided space where opaque, hardware-specific SRD data is written.
///
/// @ingroup ResourceBinding
typedef void (PAL_STDCALL *CreateSamplerSrdsFunc)(
    const IDevice*      pDevice,
    uint32              count,
    const SamplerInfo*  pSamplerInfo,
    void*               pOut);

/// Specifies output arguments for IDevice::QueryWorkstationCaps(), returning worksation feature information
/// on this device workstation board.
union WorkStationCaps
{
    struct
    {
        uint32 workStationBoard             : 1;  ///< Running a workstation driver on a workstation board.
                                                  ///  On workstation boards that support CWG (Creator Who Game),
                                                  ///  the user can switch to a Gaming/consumer driver on the
                                                  ///  workstation board, and then this will be false.
        uint32 supportWorkstationAppPerfOpt : 1;  ///< Workstation boards have optimizations for kinds of workstation
                                                  ///  applications. These optimization is enabled if it is set.
        uint32 supportWorkstationEdgeFlag   : 1;  ///< Workstation boards have a DX9 feature that edge flag can be
                                                  ///  exported via point size output in VS. The feature is enabled
                                                  ///  if it is set.
        uint32 reserved                     : 29; ///< Reserved for future use.
    };
    uint32 u32All;         ///< Flags packed as 32-bit uint.
};

/// FrameLock/GenLock support state enum
enum class FlglSupport : uint32
{
    NotAvailable  = 0,   ///< FL/GL not supported by the GPU
    NotConnected  = 1,   ///< FL/GL support available in the GPU, but is not connected to a GLSync board
    Available     = 2,   ///< FL/GL support available and connected
    Count
};

/// Container structure for FrameLock/GenLock state.
struct FlglState
{
    union
    {
        struct
        {
            uint32 genLockEnabled    : 1;   ///< True if genlock is currently enabled. Genlock is a system-wide setting
                                            ///< in CCC. Genlock provides a singal source (which is used in framelock)
            uint32 frameLockEnabled  : 1;   ///< True if framelock is currently enabled. Framelock is the mechanism to
                                            ///< sync all presents in multiple adapters.
            uint32 isTimingMaster    : 1;   ///< True if the display being driven by the current adapter is the timing
                                            ///< master in a genlock configuration
            uint32 reserved          : 29;  ///< Reserved for future use.
        };
        uint32 u32All;  ///< Packed 32-bit uint value.
    };
    FlglSupport support;         ///< The state of the FLGL support in current adapter
    uint32      firmwareVersion; ///< Firmware version number of the GLSync hardware (S400 board), if available
};

/**
 ***********************************************************************************************************************
 * @interface IDevice
 * @brief     Interface representing a client-configurable context for a particular GPU.
 *
 * This object becomes the root of all client/PAL interaction to get work done on that GPU.  The main functionality
 * provided by the device object:
 *
 * + Creation of all other PAL objects.
 * + GPU memory management.
 ***********************************************************************************************************************
 */
class IDevice
{
public:
    /// Get the maximum alignments for images created with a @ref ImageTiling::Linear tiling mode assuming the images'
    /// elements are no larger than pAlignments->maxElementSize.
    ///
    /// @param [out] pAlignments  Its maxElementSize will be used to fill the rest of its members with valid alignments.
    ///
    /// @returns Success if pAlignments was filled with data.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pAlignments is null.
    ///          + ErrorInvalidValue if pAlignments->maxElementSize is zero.
    virtual Result GetLinearImageAlignments(
        LinearImageAlignments* pAlignments) const = 0;

    /// Fills out a structure with details on the properties of this device.  This includes capability flags,
    /// supported engines/queues, performance characteristics, etc.  This should only be called after a client has
    /// called @ref CommitSettingsAndInit().
    ///
    /// @see DeviceProperties
    ///
    /// @param [out] pInfo Properties structure to be filled out by PAL based on properties of this device.
    ///
    /// @returns Success if the device properties were successfully returned in pInfo.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pInfo is null.
    virtual Result GetProperties(
        DeviceProperties* pInfo) const = 0;

    /// Checks and returns execution state of the device. Currently unsupported for DX and Linux clients and
    /// will return Unavailable if called by those clients.
    ///
    /// @returns Success if device is operational and running.  Otherwise, one of the following errors may be
    ///          + ErrorDeviceLost if device is lost, reset or not responding,
    ///          + ErrorOutOfGpuMemory if ran out of GPU memory,
    ///          + ErrorUnknown if device is in unknown state.
    virtual Result CheckExecutionState() const = 0;

    /// Returns this devices client-visible settings structure initialized with appropriate defaults.  Clients can
    /// modify parameters in this structure as they wish in order to modify PAL's behavior for this device.  After
    /// modifying settings, the client must call CommitSettingsAndInit() before creating finalizing the device.
    ///
    /// @warning The returned value points to an internal PAL structure.  Modifying data using this pointer after
    ///          calling FinalizeSettings() will result in undefined behavior.
    ///
    /// @returns Pointer to this devices public settings for examination and/or modification by the client.
    virtual PalPublicSettings* GetPublicSettings() = 0;

    /// Returns this devices Command Buffer Logger layer settings structure initialized with appropriate defaults.
    ///
    /// @returns Pointer to this devices Command Buffer Logger layer settings.
    virtual const CmdBufferLoggerSettings& GetCmdBufferLoggerSettings() const = 0;

    /// Returns this devices Debug Overlay layer settings structure initialized with appropriate defaults.
    ///
    /// @returns Pointer to this devices Debug Overlay layer settings.
    virtual const DebugOverlaySettings& GetDbgOverlaySettings() const = 0;

    /// Returns this devices GPU Profiler layer settings structure initialized with appropriate defaults.
    ///
    /// @returns Pointer to this devices GPU Profiler layer settings.
    virtual const GpuProfilerSettings& GetGpuProfilerSettings() const = 0;

    /// Returns this devices Interface Logger layer settings structure initialized with appropriate defaults.
    ///
    /// @returns Pointer to this devices Interface Logger layer settings.
        virtual const InterfaceLoggerConfig& GetInterfaceLoggerSettings() const = 0;

    /// Reads a specific setting from the operating system specific source (e.g. registry or config file).
    ///
    /// @param [in]  pSettingName Name of the setting. Must be null-terminated.
    /// @param [in]  settingScope The scope of settings accessible.
    /// @param [in]  valueType    The type of the setting to return (e.g. bool or int).
    /// @param [out] pValue       Buffer to write data that was read. Must be non-null.
    /// @param [out] bufferSz     Size of string buffer (pValue). Only necessary for ValueType::Str.
    ///
    /// @returns True if the read of specified setting is successful. False indicates failure.
    virtual bool ReadSetting(
        const char*     pSettingName,
        SettingScope    settingScope,
        Util::ValueType valueType,
        void*           pValue,
        size_t          bufferSz = 0) const = 0;

    /// Indicates that the client has finished overriding public settings so the settings struct can be finalized and
    /// any late-stage initialization can be done. This method must be called before @ref IDevice::Finalize() can be
    /// called.
    ///
    /// @note The only functions in IDevice that are able to be called before CommitSettingsAndInit():
    ///       + GetLinearImageAlignments()
    ///       + GetPublicSettings()
    ///       + ReadSetting()
    ///
    /// @note Finalizing the settings may override values set by the client.  This can occur if:
    ///       + Invalid settings, either because they are not supported by hardware or are somehow self-conflicting,
    ///         will be overridden.
    ///       + Settings specified in the private settings will override client-specified settings.
    ///
    /// @returns Success if settings have been committed successfully and any late-stage initialization is completed
    ///          successfully as well.
    virtual Result CommitSettingsAndInit() = 0;

    /// Allocates GDS for individual engines. Returns in a separate output parameter the actually allocated GDS
    /// ranges. Must be called after @ref CommitSettingsAndInit() and before @ref Finalize(). Should only be used if
    /// atomic counters weren't requested at platform creation time. When atomic counter support is requested GDS
    /// allocation happens automatically and the paritions are split equally across the requested engines.
    /// The GDS partitions allocated with this function can also be used as atomic counters, however, the clients
    /// allocating GDS for atomic counters this way should only call GetMaxAtomicCounters() after calling this
    /// function.
    ///
    /// @param [in] requested     Requested GDS allocation scheme.
    /// @param [in] pAllocated    Allocated GDS partitioning. Can be null.
    ///
    /// @returns Success if allocating GDS for all engines has been successful.
    ///          + ErrorOutOfGpuMemory if any subset of the GDS allocations have failed. In this case pAllocated gives
    ///            information about which allocations succeeded and which failed. The caller can retry the allocation
    ///            with a different setting which may succeed.
    ///            NOTE: GDS allocations should only be requested when needed as it is not guaranteed that those
    ///            resources can be freed until Device destruction.
    virtual Result AllocateGds(
        const DeviceGdsAllocInfo&   requested,
        DeviceGdsAllocInfo*         pAllocated = nullptr) = 0;

    /// Computes the maximum number of atomic counters avaliable for an engine ID with the specified engine type.
    /// Will be the calculated available maximum per engine ID, up to maxNumEngines.
    ///
    /// @param [in] engineType    Selects which type of engine to query for atomic counters (e.g., universal or
    ///                           compute).
    /// @param [in] maxNumEngines Maximum number of engines that could be created during lifetime.
    ///
    /// @returns The maximum number of atomic counters avaliable per engine ID.
    virtual uint32 GetMaxAtomicCounters(
        EngineType engineType,
        uint32     maxNumEngines) const = 0;

    /// Returns the largest possible GPU memory alignment requirement for any IGpuMemoryBindable object created on this
    /// device.
    ///
    /// This is useful for clients that may want to allocate generic GPU memory rafts up front to support many objects
    /// without creating all of those objects to query their alignment requirements ahead of time.  In practice, most
    /// objects have a much smaller alignment requirement than the allocation granularity, but images may require more
    /// than the allocation granularity on some devices.
    ///
    /// @returns Largest possible GPU memory byte alignment for an IGpuMemoryBindable object on this device.
    virtual gpusize GetMaxGpuMemoryAlignment() const = 0;

    /// Indicates that the client is able to finalize the initialization of this device with the requisite information.
    /// This method must be called before any of the factory creation methods may be called.
    ///
    /// @note The only functions in IDevice that are able to be called before Finalize():
    ///         + The functions listed in IDevice::CommitSettingsAndInit().
    ///         + AllocateGds()
    ///         + GetMaxAtomicCounters()
    ///         + GetMaxGpuMemoryAlignment()
    ///         + GetProperties()
    ///
    /// @param [in] finalizeInfo Device finalization properties.
    ///
    /// @returns Success if final initilization is successful.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidOrdinal if the indirect user-data tables combined sizes/offsets run beyond the amount of
    ///            client-used CE RAM space for the Universal queue.
    virtual Result Finalize(
        const DeviceFinalizeInfo& finalizeInfo) = 0;

    /// Cleans up all internal state, undoing any work done by CommitSettingsAndInit() and Finalize(). Following a call
    /// to this function, the device will be in its initial state as if it was re-enumerated; the client may requery
    /// settings and build up the device for further use. If the client doesn't call this function, it will be called
    /// automatically when IPlatform::Destroy() is called or when devices are re-enumerated.
    ///
    /// This function provides clients with a way to return devices to a trival state, one in which they have no
    /// lingering OS or kernel driver dependencies. If a client pairs external state (e.g., an OS handle) with their
    /// devices they may be required to call this function when they destroy their API device objects.
    ///
    /// It is expected that all PAL objects created by the device have already been destroyed (e.g. GPU memory, queues),
    /// if not, the device may fall into an illegal state and the client will experience undefined behavior.
    ///
    /// @returns Success if no errors occurred.
    virtual Result Cleanup() = 0;

    /// Specifies how many frames can be placed in the presentation queue.  This limits how many frames the CPU can get
    /// in front of the device.
    ///
    /// @param [in] maxFrames Maximum number of frames that can be batched.  Specifying a value of 0 resets the limit to
    ///                       a default system value (3 frames on Windows).
    ///
    /// @returns Success if the limit was successfully adjusted.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if this function is not available on this OS.
    virtual Result SetMaxQueuedFrames(
        uint32 maxFrames) = 0;

    /// Compares this device against another device object to determine how compatible they are for multi-GPU
    /// operations.
    ///
    /// @param [in]  otherDevice Device to determine MGPU compatibility with.
    /// @param [out] pInfo       Result compatibility info.
    ///
    /// @returns Success if the compatibility info was successfully returned in pInfo.  Otherwise, one of the following
    ///          errors may be returned:
    ///          + ErrorInvalidPointer if pInfo is null.
    virtual Result GetMultiGpuCompatibility(
        const IDevice&        otherDevice,
        GpuCompatibilityInfo* pInfo) const = 0;

    /// Reports properties of all GPU memory heaps available to this device (e.g., size, whether it is CPU visible or
    /// not, performance characteristics, etc.).
    ///
    /// @param [out] info Properties of each GPU heap available to this device, indexed by the GPU ID defined in
    ///                   @ref GpuHeap.  If a particular heap is unavailable, its entry will report a size of 0.
    ///
    /// @returns Success if the heap properties were successfully queried and returned in info[].  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorUnknown if an unexpected internal error occured.
    virtual Result GetGpuMemoryHeapProperties(
        GpuMemoryHeapProperties info[GpuHeapCount]) const = 0;

    /// Reports all format and tiling mode related properties for this device.
    ///
    /// @param [out] pInfo  Output properties.
    ///
    /// @returns Success if the properties were successfully queried and returned in pProperties.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pInfo is null.
    virtual Result GetFormatProperties(
        MergedFormatPropertiesTable* pInfo) const = 0;

    /// Reports performance experiment related properties for this device.
    ///
    /// Enumerates the GPU family, blocks, capabilities, etc..
    ///
    /// @param [out] pProperties Output properties.
    ///
    /// @returns Success if the properties were successfully queried and returned in pProperties.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pProperties is null.
    virtual Result GetPerfExperimentProperties(
        PerfExperimentProperties* pProperties) const = 0;

    /// Adds a list of per-device memory object references that persist across command buffer submissions. It is the
    /// responsibility of the client to make sure that all required memory references have been added before submitting
    /// the command buffer that uses on them. References can be added at the device, queue or specified at submit time.
    /// gpuMemRefCount and ppGpuMemory cannot be 0/null. PAL will assert and crash if these values are invalid.   If
    /// multiple references are provided for the same memory, PAL will retain the safest set of GpuMemoryRef flags.
    ///
    /// see @ref IQueue::Submit()
    ///
    /// @param [in] gpuMemRefCount Number of memory references in the memory reference list, must be non-zero.
    /// @param [in] pGpuMemoryRefs Array of gpuMemRefCount GPU memory references.
    /// @param [in] pQueue         Optional IQueue that the memory references will be used on, used to optimize
    ///                            residency operations, can be null. Note, if a queue is specified here the same queue
    ///                            should be specified in RemoveGpuMemoryReferences.
    /// @param [in] flags          Flags from GpuMemoryRefFlags that will apply to all memory object references.
    ///
    /// @returns Success if the memory references were successfully added. Can also return NotReady if the client
    ///          passes in a valid pPagingFence pointer and the operation doesn't complete before the function returns.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorTooManyMemoryReferences if gpuMemRefCount will cause the total reference count to exceed the
    ///            limit of this device.
    ///          + ErrorOutOfMemory if GPU memory objects will not fit in available GPU memory space (i.e. GPU Memory
    ///            is overcommitted).
    virtual Result AddGpuMemoryReferences(
        uint32              gpuMemRefCount,
        const GpuMemoryRef* pGpuMemoryRefs,
        IQueue*             pQueue,
        uint32              flags
        ) = 0;

    /// Removes a list of per-device memory object references that have previously been added via
    /// IDevice::AddGpuMemoryReferences(). PAL is responsible for ensuring that timestamps have been retired prior to
    /// actually performing any residency operations related to removal of a memory reference, so clients are free to
    /// call this function without regard for command buffer use. Memory references are reference counted, so an
    /// individual memory reference will only be removed when the total internal reference count reaches zero.
    /// gpuMemoryCount and ppGpuMemory cannot be 0/null, PAL will assert and crash if these values are invalid.
    ///
    /// @param [in] gpuMemoryCount Number of memory objects in the memory reference list (size of ppGpuMemory array).
    ///                            This count must be greater than zero.
    /// @param [in] ppGpuMemory    Array of GPU memory references.
    /// @param [in] pQueue         Optional IQueue that the memory references were used on, used to optimize residency
    ///                            operations, can be null. Note, if a queue was specified in AddGpuMemoryReferences,
    ///                            pQueue must match.
    ///
    /// @returns Success if the memory references were successfully updated.
    virtual Result RemoveGpuMemoryReferences(
        uint32            gpuMemoryCount,
        IGpuMemory*const* ppGpuMemory,
        IQueue*           pQueue
        ) = 0;

    /// Get primary surface MGPU support information based upon primary surface create info and input flags provided
    /// by client.
    ///
    /// This function should not be called by clients that rely on PAL for compositor management.  Basically, if your
    /// client uses the IScreen's interface to take full screen exclusive mode, then don't call this.
    ///
    /// @param [in]     primaryInfoInput        Primary surface info input arguments.
    /// @param [in,out] pPrimaryInfoOutput      Primary surface info output arguments.
    ///
    /// @returns Success if the primary surface MGPU support information were successfully queried.
    virtual Result GetPrimaryInfo(
        const GetPrimaryInfoInput&  primaryInfoInput,
        GetPrimaryInfoOutput*       pPrimaryInfoOutput) const = 0;

    /// Returns the supported stereo modes list.
    ///
    /// @param [in,out] pStereoModeCount Input value specifies the maximum number of stereo modes to enumerate, and the
    ///                                  output value specifies the total number of stereo modes that were enumerated
    ///                                  in pStereoModeList.  The input value is ignored if pStereoModeList is null.
    ///                                  This pointer must not be null.
    /// @param [out]    pStereoModeList  Output list of stereo modes.  Can be null, in which case the total number of
    ///                                  available modes will be written to pStereoModeCount.
    ///
    /// @returns Success if the display modes were successfully queried and the results were reported in
    ///          pStereoModeCount/pStereoModeList.  Otherwise, one of the following errors may be returned:
    ///          + Unsupported if stereo mode is not supported, or the stereo modes can't be queried.
    ///          + ErrorOutOfMemory if temp memeory allocation failed.
    virtual Result GetStereoDisplayModes(
        uint32*                   pStereoModeCount,
        StereoDisplayModeOutput*  pStereoModeList) const = 0;

    /// Return information about active workstation support for 10-bit (potentially packed pixel) displays.
    ///
    /// @param [out]    pMode  Output reports if the workstation 10-bit display feature is enabled, and if so,
    ///                        details on any required pixel packing.
    ///
    /// @returns Success if the 10-bits and packed-pixel format were successfully queried and the result were
    ///          reported in pMode.
    virtual Result GetActive10BitPackedPixelMode(
        Active10BitPackedPixelModeOutput*  pMode) const = 0;

    /// Inform the KMD that this allocation must be reinterpreted as 10-bits per channel for the all
    /// KMD-initiated BLTs.
    ///
    /// When in 10-bit mode and at present time, if the dx9p driver sees a 10-bit to 8-bit surface blt,
    /// it will use this interface to inform the KMD that the blt dst surface must be reinterpreted as
    /// 10-bits per channel for the all KMD-initiated BLTs.
    ///
    /// @param [in] pGpuMemory    The dst GPU memory reference which will be marked as 10 bits format.
    ///
    /// @returns Success if the KMD has been sucessfully notified.
    virtual Result RequestKmdReinterpretAs10Bit(
        const IGpuMemory* pGpuMemory) const = 0;

    /// Set or query device clock mode.
    ///
    /// This function can be called by clients to set the device engine and memory clocks to certain pre-defined ratios.
    /// If a call to restore the device clocks to default does not occur, the device stays in the previously set mode.
    ///
    /// @param [in]  setClockModeInput        Specify the clock mode to set the device to.
    /// @param [out] pSetClockModeOutput      @b Optional - Output device clock mode. If not nullptr, it is used
    ///                                       to query the current clock mode the device is running in.
    ///
    /// @returns Success if the device clock mode query/set request was successful.
    virtual Result SetClockMode(
        const SetClockModeInput& setClockModeInput,
        SetClockModeOutput*      pSetClockModeOutput) = 0;

    /// Set up MGPU compositing mode of a display provided by client.
    ///
    /// This function should not be called by clients that rely on PAL for compositor management.  Basically, if your
    /// client uses the IScreen's interface to take full screen exclusive mode, then don't call this.
    ///
    /// @param [in] setMgpuModeInput        Set MGPU compositing mode input arguments.
    ///
    /// @returns Success if the MGPU compositing mode were successfully set.
    virtual Result SetMgpuMode(
        const SetMgpuModeInput& setMgpuModeInput) const = 0;

    /// Get XDMA cache buffer information of each GPU based upon video present source ID provided by client.
    ///
    /// This function should not be called by clients that rely on PAL for compositor management.  Basically, if your
    /// client uses the IScreen's interface to take full screen exclusive mode, then don't call this.
    ///
    /// @param [in]     vidPnSrcId              Video present source id.
    /// @param [in]     gpuMemory               Primary surface GPU memory.
    /// @param [in,out] pGetXdmaInfoOutput      Set XDMA cache buffer info output arguments.
    ///
    /// @returns Success if the XDMA cache buffer information were successfully queried.
    virtual Result GetXdmaInfo(
        uint32              vidPnSrcId,
        const IGpuMemory&   gpuMemory,
        GetXdmaInfoOutput*  pGetXdmaInfoOutput) const = 0;

    /// Polls current fullscreen frame metadata controls on given vidPnSourceId, including extended data.
    ///
    /// The function is used by clients that support frame metadata through KMD-UMD shared memory.
    /// It polls the frame metadata shared memory for the given VidPnSource. Indicating which types of metadata
    /// the UMD should send to KMD.
    /// Clients should only call this function on the master device in an LDA chain.
    ///
    /// @param [in]  vidPnSrcId             Video present source id
    /// @param [out] pFrameMetadataControl  @b Optional - Output frame metadata controls. Clients can pass null to
    ///                                     check if the buffer is initialized successfully and if the
    ///                                     vidPnSrcId is valid.
    ///
    /// @returns Success if the metadata controls on the given vidPnSrcId was successfully polled.
    ///          Otherwise, one of the following erros may be returned:
    ///          + ErrorInvalidValue if vidPnSrcId is invalid (out of range)
    ///          + ErrorUnavailable if no implementation on current platform or if metadata shared buffer is null.
    virtual Result PollFullScreenFrameMetadataControl(
        uint32                         vidPnSrcId,
        PerSourceFrameMetadataControl* pFrameMetadataControl) const = 0;

    /// Get flip status flags and a flag indicating if current device owns the flags. (DX only)
    ///
    /// The function is used by clients that need flip status polling through KMD-UMD shared memory.
    /// It provides caller the FlipStatusFlags on the given VidPnSource. Note that the flag returned is only a hint,
    /// and can have a one frame delay during flip status transition.
    ///
    /// @param [in]  vidPnSrcId    Video present source id.
    /// @param [out] pFlipFlags    Output flip flags on given vidPnSrcId. Must not be null.
    /// @param [out] pIsFlipOwner  Output indicating if the current device owns the flip flags.
    ///
    /// @returns Success if flipping flags on given vidPnSrcId was successfully polled.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if vidPnSrcId is invalid.
    ///          + ErrorUnavailable if no implementation on current platform.
    ///          + ErrorInitializationFailed if flip status shared buffer was failed to initialize.
    virtual Result GetFlipStatus(
        uint32           vidPnSrcId,
        FlipStatusFlags* pFlipFlags,
        bool*            pIsFlipOwner) const = 0;

    /// Resets the specified set of fences.
    ///
    /// All fences must be reset before passing them to a submission command.
    ///
    /// @param [in] fenceCount Number of fences to reset.
    /// @param [in] ppFences   Array of fences to reset.
    ///
    /// @returns Success if the specified fences have been successfully reset.
    ///
    /// @note The function assumes that neither ppFences is null nor that any of the elements of the array pointed by
    /// ppFences are null.
    virtual Result ResetFences(
        uint32              fenceCount,
        IFence*const*       ppFences) const = 0;

    /// Stalls the current thread until one or all of the specified fences have been reached by the device.
    ///
    /// All fences must have been submitted at least once before this is called.  Using a zero timeout value returns
    /// immediately and can be used to determine the status of a set of fences without stalling.
    ///
    /// @param [in] fenceCount Number of fences to wait for (i.e., size of the ppFences array).
    /// @param [in] ppFences   Array of fences to be waited on.
    /// @param [in] waitAll    If true, wait for completion of all fences in the array before returning; if false,
    ///                        return after any single fence in the array has completed.
    /// @param [in] timeout    This method will return after this many nanoseconds even if the fences do not complete.
    ///
    /// @returns Success if the specified fences have been reached, or Timeout if the fences have not been reached but
    ///          the specified timeout time has elapsed.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if:
    ///              - ppFences is null.
    ///              - Any member of the ppFences array is null.
    ///          + ErrorInvalidValue if:
    ///              - fenceCount is zero.
    ///          + ErrorFenceNeverSubmitted if:
    ///              - Any of the specified fences haven't been submitted.
    virtual Result WaitForFences(
        uint32              fenceCount,
        const IFence*const* ppFences,
        bool                waitAll,
        uint64              timeout) const = 0;

    /// Correlates a current GPU timestamp with the CPU clock, allowing tighter CPU/GPU synchronization using
    /// timestamps.
    ///
    /// @param [out] pCalibrationData  Reports a current GPU timestamp along with the CPU clock value at the time that
    ///                                timestamp was written.  The CPU clock data is OS-specific.
    ///
    /// @returns Success if the calibration was successful.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pCalibrationData is null.
    virtual Result CalibrateGpuTimestamp(
        GpuTimestampCalibration* pCalibrationData) const = 0;

    /// Binds the specified GPU memory as a trap handler for the specified pipeline type.  This GPU memory must hold
    /// shader machine code (i.e., the client must generate HW-specific shader binaries through some external means,
    /// probably the SP3 assembler).
    ///
    /// The same trap handler will be installed for all shader stages that are part of the pipeline.  A trap handler
    /// will only ever be executed for shaders that set the trapPresent bit in @ref PipelineShaderInfo.
    ///
    /// @param [in] pipelineType Select compute or graphics pipeline.  If graphics, this trap handler will be installed
    ///                          for _all_ hardware shader stages.
    /// @param [in] pGpuMemory   GPU memory allocation holding the trap handler.
    /// @param [in] offset       Offset in bytes into pGpuMemory where the trap handler shader code begins.  Must be
    ///                          256 byte aligned.
    virtual void BindTrapHandler(
        PipelineBindPoint pipelineType,
        IGpuMemory*       pGpuMemory,
        gpusize           offset) = 0;

    /// Binds the specified GPU memory location as a trap buffer for the specified pipeline type.  This GPU memory will
    /// be available to the trap handler as scratch memory to use as it chooses.  The same trap buffer will be installed
    /// for all shader stages that are part of the pipeline.
    ///
    /// There is no size parameter for the trap buffer.  The client is responsible for ensuring that the trap handler
    /// only reads/writes data within the bounds designated for trap buffer usage.
    ///
    /// @param [in] pipelineType Select compute or graphics pipeline.  If graphics, this trap buffer will be installed
    ///                          for _all_ hardware shader stages.
    /// @param [in] pGpuMemory   GPU memory allocation holding the trap buffer range.
    /// @param [in] offset       Offset in bytes into pGpuMemory where the trap buffer range starts.  Must be 256 byte
    ///                          aligned.
    virtual void BindTrapBuffer(
        PipelineBindPoint pipelineType,
        IGpuMemory*       pGpuMemory,
        gpusize           offset) = 0;

    /// Get the swap chain information for creating a swap chain and presenting an image.
    ///
    /// @param [in]  hDisplay                Display handle of the local window system.
    /// @param [in]  hWindow                 Window handle of the local window system.
    /// @param [in]  wsiPlatform             WSI Platform the swapchain supposed to work on
    /// @param [out] pSwapChainProperties    Contains swap chain information.
    ///
    /// @returns Success if get swap chain information successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result GetSwapChainInfo(
        OsDisplayHandle      hDisplay,
        OsWindowHandle       hWindow,
        WsiPlatform          wsiPlatform,
        SwapChainProperties* pSwapChainProperties) = 0;

    /// Determines if the given window system requirement is supported by the underlying wsiPlatform.
    ///
    /// @param [in] hDisplay                  Display handle of the local window system.
    /// @param [in] wsiPlatform               WSI Platform the request supposed to send to
    /// @param [in] visualId                  Requested visual information which may not needed for some wsiPlatforms
    ///
    /// @returns Success if the request is supported. Otherwise, one of the following erros may be returned:
    ///         + Unsupported
    virtual Result DeterminePresentationSupported(
        OsDisplayHandle      hDisplay,
        WsiPlatform          wsiPlatform,
        int64                visualId) = 0;

    /// Returns a mask of SwapChainModeSupport flags for each present mode. The swapchain modes are different for each
    /// WsiPlatform.
    ///
    /// @param [in]  wsiPlatform             WSI Platform the swapchain is supposed to work on.
    /// @param [in]  mode                    The swap chain will use this present mode.
    ///
    /// @returns Returns a mask of SwapChainModeSupport.
    virtual uint32 GetSupportedSwapChainModes(
        WsiPlatform wsiPlatform,
        PresentMode mode) const = 0;

    /// Determines if the given information corresponds to an external shared image.
    ///
    /// Some clients may not know if a given external shared resource is a simple GPU memory allocation or an image; it
    /// is expected they will call this function to determine which set of open functions they must call.
    ///
    /// @param [in]  openInfo  The open info describing the external shared resource.
    /// @param [out] pIsImage  Its contents will be set to true if the external shared resource is an image.
    ///
    /// @returns Success if PAL was able to determine whether or not the resource is an image.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pIsImage is null.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result DetermineExternalSharedResourceType(
        const ExternalResourceOpenInfo& openInfo,
        bool*                           pIsImage) const = 0;

    /// @name FactoryMethods Device Factory Methods
    ///
    /// The following set of IDevice methods is the interface through which almost all PAL objects are created.
    ///
    /// PAL does not allocate its own system memory for these objects.  Instead, the client must query the amount of
    /// system memory required for the object then provide a pointer where PAL will construct the object.
    ///
    /// This approach allows the client to roll the PAL object into its own allocations without unnecessary heap
    /// allocations and cache misses.  It can also allocate many objects in a single memory space without PAL
    /// involvement.
    ///
    /// @{

    /// Determines the amount of system memory required for a queue object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateQueue().
    ///
    /// @param [in]  createInfo  Properties of the new queue such as engine type and engine index.
    /// @param [out] pResult     The validation result if pResult is non-null. This argument can be null to avoid the
    ///                          additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IQueue object with the specified properties.
    ///          A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const = 0;

    /// Creates a queue object.
    ///
    /// @param [in]  createInfo     Properties of the new queue such as engine type and engine index.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetQueueSize() with the same
    ///                             create info.
    /// @param [out] ppQueue        Constructed queue object.  When successful, the returned address will be the same
    ///                             as specified in pPlacementAddr.
    ///
    /// @returns Success if the queue was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppQueue is null.
    ///          + ErrorInvalidValue if the create info's engineType is invalid or if the engineIndex is invalid.
    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) = 0;

    /// Determines the amount of system memory required for a GPU memory object.
    ///
    /// An allocation of this amount of memory must be provided in the pPlacementAddr parameter of CreateGpuMemory().
    ///
    /// @param [in]  createInfo Data controlling the GPU memory properties, such as size, alignment, and allowed heaps.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IGpuMemory object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetGpuMemorySize(
        const GpuMemoryCreateInfo& createInfo,
        Result*                    pResult) const = 0;

    /// Creates an @ref IGpuMemory object with the requested properties.
    ///
    /// This method can create either _real_ or _virtual_ GPU memory allocations.
    ///
    /// @param [in]  createInfo     Data controlling the GPU memory properties, such as size, alignment, and allowed
    ///                             heaps.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetGpuMemorySize() with the same
    ///                             createInfo param.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the GPU memory was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + TooManyFlippableAllocations if the GPU memory was successfully created, but the client has reached
    ///            the limit of flippable allocations for this Device.  This is a warning that future flippable GPU
    ///            memory may fail to be created due to internal OS limitations.
    ///          + ErrorInvalidPointer if pPlacementAddr or ppGpuMemory is null.
    ///          + ErrorInvalidMemorySize if createInfo.size is invalid.
    ///          + ErrorInvalidAlignment if createInfo.alignment is invalid.
    ///          + ErrorInvalidValue if createInfo.heapCount is 0 for real allocations or non-0 for virtual allocations.
    ///          + ErrorOutOfGpuMemory if the allocation failed due to a lack of GPU memory.
    virtual Result CreateGpuMemory(
        const GpuMemoryCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IGpuMemory**               ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a pinned GPU memory object.
    ///
    /// An allocation of this amount of memory must be provided in the pPlacementAddr parameter of
    /// CreatePinnedGpuMemory().
    ///
    /// @param [in]  createInfo Data controlling the GPU memory properties, such as size and the allocation to pin.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IGpuMemory object pinned with the specified
    ///          properties.
    virtual size_t GetPinnedGpuMemorySize(
        const PinnedGpuMemoryCreateInfo& createInfo,
        Result*                          pResult) const = 0;

    /// Pins a segment of system memory in place and create an @ref IGpuMemory object allowing access by the GPU.
    ///
    /// @param [in]  createInfo     Data controlling the GPU memory properties, such as size and the allocation to pin.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetPinnedGpuMemorySize() with the
    ///                             same params.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the system memory was successfully pinned and a corresponding GPU memory object was created.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if createInfo.pSysMem, pPlacementAddr, or ppGpuMemory is null, or if
    ///            createInfo.pSysMem is not allocation granularity aligned.
    ///          + ErrorInvalidMemorySize if createInfo.memSize is not allocation granularity aligned.
    ///          + ErrorOutOfMemory if the creation failed because the system memory could not be pinned.
    virtual Result CreatePinnedGpuMemory(
        const PinnedGpuMemoryCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IGpuMemory**                     ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a SVM memory object
    ///
    /// An allocation of this amount of memory must be provided in the pPlacementAddr parameter of
    /// CreateSvmGpuMemory().
    ///
    /// @param [in]  createInfo Data controlling the SVM memory properties, such as size and type of SVM buffer.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IGpuMemory object with the specified properties.
    virtual size_t GetSvmGpuMemorySize(
        const SvmGpuMemoryCreateInfo& createInfo,
        Result*                       pResult) const = 0;

    /// Creates an SVM (Shared Virtual Memory) IGpuMemory object.
    /// The basic idea of SVM is to create system memory that has the same CPU and GPU virtual address
    /// (i.e., "pointer is a pointer").  This can work in two modes: fine-grain, or coarse-grain.
    ///
    /// Fine-grain (SGPU): The client should just call this function with pReservedGpuVaOwner set to null.
    /// PAL will allocate GPU-accessible system memory that will have the same CPU virtual address
    /// (as returned by IGpuMemory::Map()) as GPU virtual address
    /// (as returned in the gpuVirtAddr value returned by IGpuMemory::Desc()).
    ///
    /// Fine-grain (MGPU): The client can call this function with pReservedGpuVaOwner set to IGpuMemory object
    /// allocated on the first device and receive mapping to the same GPU VA location on another device.
    ///
    /// Coarse-grain: In this mode, there are actually two separate IGpuMemory objects.
    /// The client should first create the "local" GPU memory object by calling IDevice::CreateGpuMemory() to create
    /// a standard GPU memory object in the VaRange::Svm VA space.  Next, the client should create the "staging" GPU
    /// memory by calling this function (CreateSvmGpuMemory) with pReserveGpuVaOwner pointing to the "local" GPU
    /// memory object.  PAL will create system memory for the "staging" GPU memory with a CPU virtual address matching
    /// the "local" GPU memory's GPU virtual address.  The GPU virtual address of the "staging" GPU memory is
    /// arbitrarily assigned.  The client is responsible for managing the contents of the two related allocations
    /// per their API rules.
    ///
    /// @param [in]  createInfo     Data controlling the SVM memory properties, such as size and location of SVM buffer.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetSvmGpuMemorySize() with the
    ///                             same params.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the SVM buffer was successfully created and a corresponding GPU memory object was created.
    ///          Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr, or ppGpuMemory is null.
    ///          + ErrorInvalidMemorySize if createInfo.memSize is not allocation granularity aligned.
    ///          + ErrorOutOfMemory if the creation failed because there is not enough GPU memory
    ///            or the system memory could not be pinned.
    virtual Result CreateSvmGpuMemory(
        const SvmGpuMemoryCreateInfo& createInfo,
        void*                         pPlacementAddr,
        IGpuMemory**                  ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a GPU memory object created by opening an allocation from a
    /// different GPU.
    ///
    /// An allocation of this amount of memory must be provided in the pPlacementAddr parameter of
    /// OpenSharedGpuMemory().
    ///
    /// @param [in]  openInfo Specifies a handle to a shared GPU memory object to open.
    /// @param [out] pResult  The validation result if pResult is non-null. This argument can be null to avoid the
    ///                       additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for opening a shared IGpuMemory object with the specified
    ///          properties.  A return value of 0 indicates the openInfo was invalid.
    virtual size_t GetSharedGpuMemorySize(
        const GpuMemoryOpenInfo& openInfo,
        Result*                  pResult) const = 0;

    /// Opens a shareable GPU memory object created on another device for use on this device.
    ///
    /// @param [in]  openInfo       Specifies a handle to a shared GPU memory object to open.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetSharedGpuMemorySize() with the
    ///                             same params.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the shared memory was successfully opened for access on this device.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppGpuMemory is null.
    ///          + ErrorNotShareable if the specified memory object was not marked as shareable on creation.
    virtual Result OpenSharedGpuMemory(
        const GpuMemoryOpenInfo& openInfo,
        void*                    pPlacementAddr,
        IGpuMemory**             ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a external GPU memory object created by opening
    ///  an allocation from a compatible device, such as D3D device.
    ///
    /// @param [out] pResult  The validation result if pResult is non-null. This argument can be null to avoid the
    ///                       additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for opening a shared IGpuMemory object with the specified
    ///          properties.  A return value of 0 indicates the openInfo was invalid.
    virtual size_t GetExternalSharedGpuMemorySize(
        Result* pResult) const = 0;

    /// Opens an external shared memory object which is created by a compatible device, such as D3D device.
    /// There could be more than one underlying allocations in the shared memory object, only one allocation
    /// created on the device's GPU will be opened, other allocations will be ignored.
    ///
    /// @param [in]  openInfo       Open info.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetExternalSharedGpuMemorySize()
    ///                              with the same params.
    /// @param [out] pMemCreateInfo Return CreateInfo of the external shared GPU memory.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the shared memory was successfully opened for access on this device.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr, ppGpuMemory or ppGpuMemory is null.
    ///          + ErrorNotShareable if none of allocations in the shared memory object is created on the device's GPU.
    virtual Result OpenExternalSharedGpuMemory(
        const ExternalGpuMemoryOpenInfo& openInfo,
        void*                            pPlacementAddr,
        GpuMemoryCreateInfo*             pMemCreateInfo,
        IGpuMemory**                     ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a proxy GPU memory object to a GPU memory object on a
    /// different GPU.  An allocation of this amount of memory must be provided in the pPlacementAddr parameter of
    /// OpenPeerGpuMemory().
    ///
    /// @param [in]  openInfo Specifies a handle to a GPU memory object to open for peer-to-peer transfer access.
    /// @param [out] pResult  The validation result if pResult is non-null. This argument can be null to avoid the
    ///                       additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for opening a peer IGpuMemory object with the specified
    ///          properties.  A return value of 0 indicates the openInfo was invalid.
    virtual size_t GetPeerGpuMemorySize(
        const PeerGpuMemoryOpenInfo& openInfo,
        Result*                      pResult) const = 0;

    /// Opens previously created GPU memory object for peer access on another device.
    ///
    /// @param [in]  openInfo       Specifies a handle to a shared GPU memory object to open.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetPeerGpuMemorySize() with the same
    ///                             params.
    /// @param [out] ppGpuMemory    Constructed GPU memory object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the memory was successfully opened for peer access on this device.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr, ppGpuMemory, or openInfo.pOriginalMem is null.
    virtual Result OpenPeerGpuMemory(
        const PeerGpuMemoryOpenInfo& openInfo,
        void*                        pPlacementAddr,
        IGpuMemory**                 ppGpuMemory) = 0;

    /// Determines the amount of system memory required for an image object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateImage().
    ///
    /// @param [in]  createInfo Properties of the new image such as pixel format and dimensions.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an @ref IImage object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const = 0;

    /// Creates an @ref IImage object with the requested properties.
    ///
    /// @param [in]  createInfo     Properties of the new image such as pixel format and dimensions.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetImageSize() with the same
    ///                             createInfo param.
    /// @param [out] ppImage        Constructed image object.  When successful, the returned address will be the same as
    ///                             specified in pPlacementAddr.
    ///
    /// @returns Success if the image was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if:
    ///              - The image dimensions are invalid based on the image type.
    ///              - The image dimensions are not properly aligned for compressed formats.
    ///              - The number of samples is invalid for the image type and format.
    ///              - MSAA is enabled for an image that doesn't support color or depth usage.
    ///              - MSAA images have more than one mip level.
    ///              - The array size is zero, non-1 for 3D images, or beyond the max number of slices for 1D or 2D
    ///                images.
    ///              - The number of mipmaps is invalid for the image dimensions.
    ///          + ErrorInvalidPointer if pPlacementAddr or ppImage is null.
    ///          + ErrorInvalidFormat if:
    ///              - The format doesn't support the usage flags.
    ///              - A 1D image specifies a compressed format.
    ///          + ErrorInvalidFlags if:
    ///              - The color target and depth/stencil usages are specified simultaneously.
    ///              - The color target flag is set for a 1D image.
    ///              - The depth/stencil flag is set for a non-2D image.
    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) = 0;

    /// Determines the amount of system memory required for a presentable image object (and an associated memory
    /// object).  Allocations of these amounts of memory must be provided in the pImagePlacementAddr and
    /// pGpuMemoryPlacementAddr parameters of CreatePresentableImage().
    ///
    /// Only images created through this interface are valid sources for IQueue::Present().
    ///
    /// @param [in]  createInfo     Properties of the image to create such as width/height and pixel format.
    /// @param [out] pImageSize     Size, in bytes, of system memory required for the IImage.
    ///                             Should be specified to the pImagePlacementAddr argument of CreatePresentableImage().
    /// @param [out] pGpuMemorySize Size, in bytes, of system memory required for a IGpuMemory object attached to the
    ///                             presentable IImage.  Should be specified to the pGpuMemoryPlacementAddr argument
    ///                             of CreatePresentableImage().
    /// @param [out] pResult        The validation result if pResult is non-null. This argument can be null to avoid the
    ///                             additional validation.
    virtual void GetPresentableImageSizes(
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult) const = 0;

    /// Creates a presentable image. Presentable image must have internally bound GPU memory allocated as OS needs the
    /// information of image/memory via OS callbacks.
    ///
    /// @param [in]  createInfo              Properties of the image to create such as width/height and pixel format.
    /// @param [in]  pImagePlacementAddr     Pointer to the location where PAL should construct this object.  There must
    ///                                      be as much size available here as reported by calling
    ///                                      GetPresentableImageSizes().
    /// @param [in]  pGpuMemoryPlacementAddr Pointer to the location where PAL should construct a IGpuMemory associated
    ///                                      with this presentable image.  There must be as much size available here as
    ///                                      reported by calling GetPresentableImageSizes().
    /// @param [out] ppImage                 Constructed image object.
    /// @param [out] ppGpuMemory             Constructed memory object.  This object is only valid for specifying in a
    ///                                      memory reference list.  It must be destroyed when the image is destroyed.
    ///
    /// @returns Success if the image was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + TooManyFlippableAllocations if the image was successfully created, but the client has reached the
    ///            limit of flippable allocations for this Device.  This is a warning that future presentable Images
    ///            may fail to be created due to internal OS limitations.
    ///          + ErrorTooManyPresentableImages if the swap chain cannot be associated with more presentable images.
    ///          + ErrorInvalidPointer if pImagePlacementAddr, pGpuMemoryPlacementAddr, ppImage, or ppGpuMemory is null.
    ///          + ErrorInvalidValue if:
    ///              - The image dimensions are invalid.
    ///              - The refresh rate is invalid for a fullscreen image.
    ///          + ErrorInvalidFormat if the format doesn't support presentation.
    virtual Result CreatePresentableImage(
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory) = 0;

    /// Determines the amount of system memory required for an image object (and an associated memory object) opened for
    /// peer access to an image created on another GPU.  Allocations of these amounts of memory must be provided in the
    /// pImagePlacementAddr and pGpuMemoryPlacementAddr parameters of OpenPeerImage().
    ///
    /// @param [in]  openInfo           Specifies the image to be opened for peer access from another GPU.
    /// @param [out] pPeerImageSize     Size, in bytes, of system memory required for a peer IImage.  Should be
    ///                                 specified to the pImagePlacementAddr argument to OpenPeerImage().
    /// @param [out] pPeerGpuMemorySize Size, in bytes, of system memory required for a dummy IGpuMemory object attached
    ///                                 to a peer IImage.  Should be specified to the pGpuMemoryPlacementAddr argument
    ///                                 to OpenPeerImage().
    /// @param [out] pResult            The validation result if pResult is non-null. This argument can be null to avoid
    ///                                 the additional validation.
    virtual void GetPeerImageSizes(
        const PeerImageOpenInfo& openInfo,
        size_t*                  pPeerImageSize,
        size_t*                  pPeerGpuMemorySize,
        Result*                  pResult) const = 0;

    /// Creates an @ref IImage object as a proxy to an IImage on another GPU to be used for peer-to-peer transfers.
    ///
    /// @note The @ref IImage object provided in the @ref PeerImageOpenInfo must be bound to an existing
    ///       @ref IGpuMemory object prior to opening a peer version of it.
    ///       If the new image needs to bind to an existing peer memory allocation, as when images are suballocated,
    ///       pGpuMemoryPlacementAddr must equal nullptr and ppGpuMemory must point to an existing IGpuMemory object
    ///       that was previously opened to reference the same memory from the current device.
    /// @param [in]  openInfo                Specifies the image to be opened for peer access from another GPU.
    /// @param [in]  pImagePlacementAddr     Pointer to the location where PAL should construct this object.  There must
    ///                                      be as much size available here as reported by calling GetPeerImageSizes().
    /// @param [in]  pGpuMemoryPlacementAddr If nonzero, this is a pointer to the location where PAL should construct a
    ///                                      IGpuMemory to be associated with this peer image. There must be as much
    ///                                      size available here as reported by calling GetPeerImageSizes().
    /// @param [out] ppImage                 Constructed image object.
    /// @param [in]  ppGpuMemory             Constructed dummy memory object.  This object is only valid for specifying
    ///                                      in a memory reference list.
    ///
    /// @returns Success if the image was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pImagePlacementAddr, pGpuMemoryPlacementAddr, ppImage, ppGpuMemory, or
    ///            openInfo.pOriginalImage is null.
    virtual Result OpenPeerImage(
        const PeerImageOpenInfo& openInfo,
        void*                    pImagePlacementAddr,
        void*                    pGpuMemoryPlacementAddr,
        IImage**                 ppImage,
        IGpuMemory**             ppGpuMemory) = 0;

    /// Determines the amount of system memory required for an external shared image object (and an associated memory
    /// object).  Allocations of these amounts of memory must be provided in the pImagePlacementAddr and
    /// pGpuMemoryPlacementAddr parameters of OpenExternalSharedImage().
    ///
    /// @param [in]  openInfo       Specifies the external image to be opened.
    /// @param [out] pImageSize     Size, in bytes, of system memory required for pImagePlacementAddr.
    /// @param [out] pGpuMemorySize Size, in bytes, of system memory required for pGpuMemoryPlacementAddr.
    /// @param [out] pImgCreateInfo If non-null, it will be filled out with information describing the shared image.
    /// @returns Success if the shared image was successfully opened for access on this device.  Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pImageSize or pGpuMemorySize is null.
    ///          + ErrorNotShareable if none of allocations in the shared image is created on the device's GPU.
    virtual Result GetExternalSharedImageSizes(
        const ExternalImageOpenInfo& openInfo,
        size_t*                      pImageSize,
        size_t*                      pGpuMemorySize,
        ImageCreateInfo*             pImgCreateInfo) const = 0;

    /// Opens an external shared image object which was created by a compatible device, such as D3D device.
    /// There could be more than one underlying allocations in the shared image object, only one allocation
    /// created on the device's GPU will be opened, other allocations will be ignored.
    ///
    /// @param [in]  openInfo                Specifies the external image to be opened.
    /// @param [in]  pImagePlacementAddr     Pointer to the location where PAL should construct the image object.
    ///                                      There must be as much space available here as reported by calling
    ///                                      GetExternalSharedImageSizes() with the same params.
    /// @param [in]  pGpuMemoryPlacementAddr Pointer to the location where PAL should construct the GPU memory object.
    ///                                      There must be as much space available here as reported by calling
    ///                                      GetExternalSharedImageSizes() with the same params.
    /// @param [out] pMemCreateInfo          If non-null, it is filled with information describing the external GPU
    ///                                      memory that backs this external image.
    /// @param [out] ppImage                 Constructed image object.  When successful, the returned address will
    ///                                      be the same as specified in pImagePlacementAddr.
    /// @param [out] ppGpuMemory             Constructed GPU memory object.  When successful, the returned address will
    ///                                      be the same as specified in pGpuMemoryPlacementAddr.
    /// @returns Success if the shared image was successfully opened for access on this device. Otherwise, one of the
    ///          following errors may be returned:
    ///          + ErrorInvalidPointer if pImagePlacementAddr, pGpuMemoryPlacementAddr, ppImage or ppGpuMemory is null.
    ///          + ErrorNotShareable if none of allocations in the shared image object is created on the device's GPU.
    virtual Result OpenExternalSharedImage(
        const ExternalImageOpenInfo& openInfo,
        void*                        pImagePlacementAddr,
        void*                        pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*         pMemCreateInfo,
        IImage**                     ppImage,
        IGpuMemory**                 ppGpuMemory) = 0;

    /// Determines the amount of system memory required for a color target view object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateColorTargetView().
    ///
    /// Unlike most creation methods in this class, GetColorTargetViewSize() does not require a ColorTargetCreateInfo
    /// parameter.  PAL must guarantee that all color target view objects are the same size in order to support DX12,
    /// where these views are treated similarly to SRDs.
    ///
    /// @param [out] pResult            The validation result if pResult is non-null. This argument can be null to avoid
    ///                                 the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IColorTargetView object.
    virtual size_t GetColorTargetViewSize(
        Result* pResult) const = 0;

    /// Creates an @ref IColorTargetView object with the requested properties.
    ///
    /// @param [in]  createInfo        Properties of the color target view to create.
    /// @param [in]  pPlacementAddr    Pointer to the location where PAL should construct this object.  There must be as
    ///                                much size available here as reported by calling GetColorTargetViewSize().
    /// @param [out] ppColorTargetView Constructed color target view object.  When successful, the returned address will
    ///                                be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the color target view was successfully created.  Otherwise, one of the following
    ///          errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr, ppColorTargetView, or createInfo.pImage is null.
    ///          + ErrorInvalidValue if:
    ///              - The base slice is invalid for the given image object and view type.
    ///              - The number of array slices is zero or the range of slices is too large for the specified image.
    ///              - The mip level is invalid for the given image object.
    ///          + ErrorInvalidImage if the image object doesn't have the color target access flag set.
    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorTargetView**               ppColorTargetView) const = 0;

    /// Determines the amount of system memory required for a depth/stencil view object.  An allocation of this amount
    /// of memory must be provided in the pPlacementAddr parameter of CreateDepthStencilView().
    ///
    /// Unlike most creation methods in this class, GetDepthStencilViewSize() does not require a
    /// DepthStencilViewCreateInfo parameter.  PAL must guarantee that all color target view objects are the same size
    /// in order to support DX12, where these views are treated similarly to SRDs.
    ///
    /// @param [out] pResult            The validation result if pResult is non-null. This argument can be null to avoid
    ///                                 the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IDepthStencilView object.
    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const = 0;

    /// Creates an @ref IDepthStencilView object with the requested properties.
    ///
    /// @param [in]  createInfo         Properties of the depth/stencil view to create.
    /// @param [in]  pPlacementAddr     Pointer to the location where PAL should construct this object.  There must be
    ///                                 as much size available here as reported by calling GetDepthStencilViewSize().
    /// @param [out] ppDepthStencilView Constructed depth/stencil view object.  When successful, the returned address
    ///                                 will be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the depth/stencil view was successfully created.  Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr, ppDepthStencilView, or createInfo.pImage is null.
    ///          + ErrorInvalidValue if:
    ///              - The base slice is invalid for the given image object and view type.
    ///              - The number of array slices is zero or the range of slices is too large for the specified image.
    ///              - The mip level is invalid for the given image object.
    ///          + ErrorInvalidImage if the image object doesn't have the depth/stencil target access flag set.
    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IDepthStencilView**               ppDepthStencilView) const = 0;

    /// Creates one or more typed buffer view _shader resource descriptors (SRDs)_ in memory provided by the client.
    ///
    /// The client is responsible for providing _count_ times the amount of memory reported by srdSizes.bufferView in
    /// DeviceProperties, and must also ensure the provided memory is aligned to the size of one SRD.
    ///
    /// The SRD can be created in either system memory or pre-mapped GPU memory.  If updating GPU memory, the client
    /// must ensure there are no GPU accesses of this memory in flight before calling this method.
    ///
    /// The generated buffer view SRD allows a range of a GPU memory allocation to be accessed by a shader, and should
    /// be setup based on shader usage as described in @ref BufferViewInfo.  The client should put the resulting SRD
    /// in an appropriate location based on the shader resource mapping specified by the bound pipeline, either directly
    /// in user data (ICmdBuffer::CmdSetUserData()) or a table in GPU memory indirectly referenced by user data.
    ///
    /// For performance reasons, this method returns void and does minimal error-checking. However, in debug builds,
    /// to assist clients' debug efforts, the following conditions will be checked with runtime assertions:
    ///     + If pBufferViewInfo or pOut, is null.
    ///     + If count is 0.
    ///     + If pBufferViewInfo[].format is Undefined.
    ///     + If pBufferViewInfo[].stride does not match the size of an element of that format.
    ///     + If pBufferViewInfo[].gpuAddr is 0.
    ///     + If pBufferViewInfo[].gpuAddr is not properly aligned to Min(4, pBufferViewInfo[].stride).
    ///
    /// @param [in]  count           Number of buffer view SRDs to create; size of the pBufferViewInfo array.
    /// @param [in]  pBufferViewInfo Array of buffer view descriptions directing SRD construction.
    /// @param [out] pOut            Client-provided space where opaque, hardware-specific SRD data is written.
    ///
    /// @ingroup ResourceBinding
    PAL_INLINE void CreateTypedBufferViewSrds(
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut) const
        { m_pfnTable.pfnCreateTypedBufViewSrds(this, count, pBufferViewInfo, pOut); }

    /// Creates one or more untyped buffer view _shader resource descriptors (SRDs)_ in memory provided by the client.
    /// These SRDs can be accessed in a shader as either _raw_ or _structured_ views.
    ///
    /// The client is responsible for providing _count_ times the amount of memory reported by srdSizes.bufferView in
    /// DeviceProperties, and must also ensure the provided memory is aligned to the size of one SRD.
    ///
    /// The SRD can be created in either system memory or pre-mapped GPU memory.  If updating GPU memory, the client
    /// must ensure there are no GPU accesses of this memory in flight before calling this method.
    ///
    /// The generated buffer view SRD allows a range of a GPU memory allocation to be accessed by a shader, and should
    /// be setup based on shader usage as described in @ref BufferViewInfo.  The client should put the resulting SRD
    /// in an appropriate location based on the shader resource mapping specified by the bound pipeline, either directly
    /// in user data (ICmdBuffer::CmdSetUserData()) or a table in GPU memory indirectly referenced by user data.
    ///
    /// For performance reasons, this method returns void and does minimal error-checking. However, in debug builds,
    /// to assist clients' debug efforts, the following conditions will be checked with runtime assertions:
    ///     + If pBufferViewInfo or pOut, is null.
    ///     + If count is 0.
    ///     + If pBufferViewInfo[].format is not Undefined.
    ///     + If pBufferViewInfo[].gpuAddr is 0.
    ///     + If pBufferViewInfo[].gpuAddr is not properly aligned to Min(4, pBufferViewInfo[].stride).
    ///
    /// @param [in]  count           Number of buffer view SRDs to create; size of the pBufferViewInfo array.
    /// @param [in]  pBufferViewInfo Array of buffer view descriptions directing SRD construction.
    /// @param [out] pOut            Client-provided space where opaque, hardware-specific SRD data is written.
    ///
    /// @ingroup ResourceBinding
    PAL_INLINE void CreateUntypedBufferViewSrds(
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut) const
        { m_pfnTable.pfnCreateUntypedBufViewSrds(this, count, pBufferViewInfo, pOut); }

    /// Validates image view SRD input parameters.  Error checking for image view SRDs is handled by a separate
    /// function for performance reasons and to avoid rechecking parameters that the client knows are correct when
    /// rebuilding SRDs.
    ///
    /// @param [in] viewInfo        Input image view SRD parameter info.
    ///
    /// @returns Success if the parameters pass validation.  Otherwise, one of the following errors may be returned:
    ///     + ErrorImageAspectUnavailable if the requested image aspect specified in the view is not available on
    ///       the image.
    ///     + ErrorImageNotShaderAccessible if the image does not have a shader-readable or shader-writable usage.
    ///     + ErrorInvalidFormatSwizzle if the view's channel swizzle specifies components not available in the view
    ///       format.
    ///     + ErrorInvalidBaseMipLevel if the view's start subresource has a mip level larger than the number of
    ///       of available mip levels.
    ///     + ErrorFormatIncompatibleWithImageFormat if the view's format is not compatible with the image's format.
    ///       This can happen if:
    ///         - For color aspect views, the bit-depths of the two formats are not equal.
    ///     + ErrorFormatIncompatibleWithImageAspect if the view's format is not compatible with the image's aspect.
    ///       This can happen if:
    ///         - For depth aspect views, the bit-depths of the view format and the depth component of the image
    ///           are not equal.
    ///         - For stencil aspect views, the bit-depths of the view format and the stencil component of the image
    ///           are not equal.
    ///     + ErrorInvalidViewArraySize if:
    ///         - The view array size is 0.
    ///         - The image type is 3D and the view array size is not 1.
    ///     + ErrorViewTypeIncompatibleWithImageType if:
    ///         - The image type is 1D and the view type is not 1D
    ///         - The image type is 2D and the view type is not 2D or cubemap
    ///         - The image type is 3D and the view type is not 3D
    ///     + ErrorInsufficientImageArraySize if the number of viewed array slices is more than available on
    ///       the image.
    ///     + ErrorCubemapIncompatibleWithMsaa if the view type is a cubemap view and the image has
    ///       multiple samples.
    ///     + ErrorCubemapNonSquareFaceSize if the view type is a cubemap view and the image 2D extents are not
    ///       square.
    ///     + ErrorInvalidViewBaseSlice
    ///         - If the image type is 3D and the view base slice is not 0.
    ///
    /// @ingroup ResourceBinding
    virtual Result ValidateImageViewInfo(const ImageViewInfo& viewInfo) const = 0;

    /// Creates one or more image view _shader resource descriptors (SRDs)_ in memory provided by the client.
    ///
    /// The client is responsible for providing _count_ times the amount of memory reported by srdSizes.imageView
    /// in DeviceProperties, and must also ensure the provided memory is aligned to the size of one SRD.
    ///
    /// The SRD can be created in either system memory or pre-mapped GPU memory.  If updating GPU memory, the client
    /// must ensure there are no GPU accesses of this memory in flight before calling this method.
    ///
    /// The generated image view SRD allows a set of subresources in an image to be accessed by a shader, and should
    /// be setup as described in @ref ImageViewInfo.  The client should put the resulting SRD in an appropriate
    /// location based on the shader resource mapping specified by the bound pipeline, either directly in user data
    /// (ICmdBuffer::CmdSetUserData()) or a table in GPU memory indirectly referenced by user data.
    ///
    /// @param [in]  count        Number of buffer view SRDs to create; size of the pImageViewInfo array.
    /// @param [in]  pImgViewInfo Array of image view descriptions directing SRD construction.
    /// @param [out] pOut         Client-provided space where opaque, hardware-specific SRD data is written.
    ///
    /// @ingroup ResourceBinding
    PAL_INLINE void CreateImageViewSrds(
        uint32               count,
        const ImageViewInfo* pImgViewInfo,
        void*                pOut) const
        { m_pfnTable.pfnCreateImageViewSrds(this, count, pImgViewInfo, pOut); }

    /// Validates an fmask view SRD input parameters.  Error checking for fmask view SRDs is handled by a separate
    /// function for performance reasons and to avoid rechecking parameters that the client knows are correct when
    /// rebuilding SRDs.
    ///
    /// @param [in] viewInfo        Input image view SRD parameter info.
    ///
    /// @returns Success if the parameters pass validation.  Otherwise, one of the following errors may be returned:
    ///     + ErrorImageFmaskUnavailable if the image does not have an FMask.
    ///     + ErrorInvalidViewArraySize if the view array size is 0.
    ///     + ErrorViewTypeIncompatibleWithImageType if the image type is not 2D.
    ///     + ErrorInsufficientImageArraySize if the view base array slice and size define an out of bounds array range.
    ///
    /// @ingroup ResourceBinding
    virtual Result ValidateFmaskViewInfo(const FmaskViewInfo& viewInfo) const = 0;

    /// Creates one or more fmask view _shader resource descriptors (SRDs)_ in memory provided by the client.
    ///
    /// The client is responsible for providing _count_ times the amount of memory reported by srdSizes.fmaskView
    /// in DeviceProperties, and must also ensure the provided memory is aligned to the size of one SRD.
    ///
    /// The SRD can be created in either system memory or pre-mapped GPU memory.  If updating GPU memory, the client
    /// must ensure there are no GPU accesses of this memory in flight before calling this method.
    ///
    /// The generated fmask view SRD allows a range of image slices to be accessed bo the load_fptr IL instruction,
    /// which allows a shader to read compressed MSAA data at the expense of a texture indirection.  This SRD should be
    /// setup as described in @ref FmaskViewInfo.  The client should put the resulting SRD in an appropriate
    /// location based on the shader resource mapping specified by the bound pipeline, either directly in user data
    /// (ICmdBuffer::CmdSetUserData()) or a table in GPU memory indirectly referenced by user data.
    ///
    /// @param [in]  count          Number of fmask view SRDs to create; size of the pFmaskViewInfo array.
    /// @param [in]  pFmaskViewInfo Array of fmask view descriptions directing SRD construction.
    /// @param [out] pOut           Client-provided space where opaque, hardware-specific SRD data is written.
    ///
    /// @ingroup ResourceBinding
    PAL_INLINE void CreateFmaskViewSrds(
        uint32               count,
        const FmaskViewInfo* pFmaskViewInfo,
        void*                pOut) const
        { m_pfnTable.pfnCreateFmaskViewSrds(this, count, pFmaskViewInfo, pOut); }

    /// Validates a sampler SRD input parameters.  Error checking for sampler SRDs is handled by a separate function for
    /// performance reasons and to avoid rechecking parameters that the client knows are correct when rebuilding SRDs.
    ///
    /// @param [in] samplerInfo Input sampler SRD parameter info.
    ///
    /// @returns Success if the parameters pass validation.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidValue if:
    ///              - The max anisotropy or LOD bias value is outside of the legal range.
    ///              - The min/max LOD values are outside the legal range or if the max LOD is smaller than the min LOD.
    ///              - The border color palette index is out of the legal range.
    ///
    /// @ingroup ResourceBinding
    virtual Result ValidateSamplerInfo(const SamplerInfo& samplerInfo) const = 0;

    /// Creates one or more sampler _shader resource descriptors (SRDs)_ in memory provided by the client.
    ///
    /// The client is responsible for providing _count_ times the amount of memory reported by srdSizes.sampler in
    /// DeviceProperties, and must also ensure the provided memory is aligned to the size of one SRD.
    ///
    /// The SRD can be created in either system memory or pre-mapped GPU memory.  If updating GPU memory, the client
    /// must ensure there are no GPU accesses of this memory in flight before calling this method.
    ///
    /// The generated sampler SRD controlls execution of sample instructions in a shader, and should be setup as
    /// described in @ref SamplerInfo.  The client should put the resulting SRD in an appropriate location based on the
    /// shader resource mapping specified by the bound pipeline, either directly in user data
    /// (ICmdBuffer::CmdSetUserData()) or a table in GPU memory indirectly referenced by user data.
    ///
    /// @param [in]  count        Number of sampler SRDs to create; size of the pSamplerInfo array.
    /// @param [in]  pSamplerInfo Array of sampler descriptions directing SRD construction.
    /// @param [out] pOut         Client-provided space where opaque, hardware-specific SRD data is written.
    ///
    /// @returns Success if the sampler SRD data was successfully written to pOut.  Otherwise, one of the following
    ///          errors may be returned:
    ///          + ErrorInvalidPointer if pSamplerInfo or pOut is null.
    ///          + ErrorInvalidValue if:
    ///              - The max anisotropy or LOD bias value is outside of the legal range.
    ///              - The min/max LOD values are outside the legal range or if the max LOD is smaller than the min LOD.
    ///              - The border color palette index is out of the legal range.
    ///
    /// @ingroup ResourceBinding
    PAL_INLINE void CreateSamplerSrds(
        uint32             count,
        const SamplerInfo* pSamplerInfo,
        void*              pOut) const
        { m_pfnTable.pfnCreateSamplerSrds(this, count, pSamplerInfo, pOut); }

    /// The MSAA sample pattern palette is a client-managed table of sample patterns that might be in use by the app.
    ///
    /// The only purpose of this palette is to implement the samplepos shader instruction.  This instruction returns the
    /// position of a particular sample based on the sample pattern of the current rasterizer state or a particular
    /// specified resource.  When this instruction is executed, the shader will determine the correct palette index
    /// as specified in the pipeline (see samplePatternIdx in the rsState structure inside GraphicsPipelineCreateInfo)
    /// or in the image view SRD (see samplePatternIdx in ImageViewInfo).  The shader will then return the position
    /// for the specified sample in the specified entry of the currently bound sample pattern palette as set with this
    /// function.
    ///
    /// The initial bound sample pattern palette values are undefined.  A palette entry must be specified before it is
    /// referenced by a samplepos instruction.
    ///
    /// @warning The samplepos instruction and sample pattern palette both assume every pixel has the same pattern.
    ///          This may not be accurate if the application uses custom sample patterns that aren't identical for
    ///          every pixel in the quad.
    ///
    /// @param [in] palette Small set of sample patterns.  Each sample pattern consists of an x,y tuple per sample
    ///                     describing where it is located in a pixel.  The coordinate system is described in
    ///                     MsaaStateCreateInfo.
    ///
    /// @returns Success if the palette was successfully updated.  An error is only possible due to an internal error,
    ///          such as a failure to allocate GPU memory for a new table.
    virtual Result SetSamplePatternPalette(
        const SamplePatternPalette& palette) = 0;

    /// Determines the amount of system memory required for a border color palette object.  An allocation of this amount
    /// of memory must be provided in the pPlacementAddr parameter of CreateBorderColorPalette().
    ///
    /// @param [in]  createInfo Border color palette creation info (specifies number of entries).
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IBorderColorPalette object with the specified
    ///          properties.  A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetBorderColorPaletteSize(
        const BorderColorPaletteCreateInfo& createInfo,
        Result*                             pResult) const = 0;

    /// Creates a border color palette object.
    ///
    /// @param [in]  createInfo     Border color palette creation info (number of entries).
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetBorderColorPaletteSize() with the
    ///                             same createInfo param.
    /// @param [out] ppPalette      Constructed border color palette object.  When successful, the returned address will
    ///                             be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the border color palette was successfully created.  Otherwise, one of the following errors
    ///          may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppPalette is null.
    ///          + ErrorInvalidValue if the palette size is too large to be used on any queue on this device.
    virtual Result CreateBorderColorPalette(
        const BorderColorPaletteCreateInfo& createInfo,
        void*                               pPlacementAddr,
        IBorderColorPalette**               ppPalette) const = 0;

    /// Determines the amount of system memory required for a compute pipeline object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateComputePipeline().
    ///
    /// @param [in]  createInfo Pipeline properties including shaders and descriptor set mappings.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IPipeline object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const = 0;

    /// Creates a compute @ref IPipeline object with the requested properties.
    ///
    /// @param [in]     createInfo     Pipeline properties including shaders and descriptor set mappings.
    /// @param [in]     pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                                much size available here as reported by calling GetComputePipelineSize() with the
    ///                                same createInfo param.
    /// @param [out]    ppPipeline     Constructed pipeline object.  When successful, the returned address will be the
    ///                                same as specified in pPlacementAddr.
    ///
    /// @returns Success if the pipeline was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if:
    ///              - pPlacementAddr or ppPipeline is null.
    ///              - A required shader pointer is null.
    ///              - The link time constant data pointer is null.
    ///          + ErrorInvalidValue if:
    ///              - The link constant buffer info pointer isn't consistent with the link constant buffer count value.
    ///              - The dynamic memory view mapping slot object type is not unused, resource, of UAV.
    ///          + ErrorUnsupportedShaderIlVersion if an incorrect shader type is used in any shader stage.
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IPipeline**                      ppPipeline) = 0;

    /// Determines the amount of system memory required for a graphics pipeline object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateGraphicsPipeline().
    ///
    /// @param [in]  createInfo Pipeline properties including shaders and descriptor set mappings.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IPipeline object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result*                           pResult) const = 0;

    /// Creates a graphics @ref IPipeline object with the requested properties.
    ///
    /// @param [in]  createInfo     Pipeline properties including shaders and descriptor set mappings.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetGraphicsPipelineSize() with the
    ///                             same createInfo param.
    /// @param [out] ppPipeline     Constructed pipeline object.  When successful, the returned address will be the same
    ///                             as specified in pPlacementAddr.
    ///
    /// @returns Success if the pipeline was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if:
    ///              - pPlacementAddr or ppPipeline is null.
    ///              - A required shader pointer is null.
    ///              - The link time constant data pointer is null.
    ///          + ErrorInvalidValue if:
    ///              - The number of control points is invalid for a tessellation pipeline.
    ///              - Logic operations are enabled while some of the color targets enable blending.
    ///              - The dual source blend enable doesn't match expectations for color target and blend enable setup.
    ///              - The link constant buffer info pointer isn't consistent with the link constant buffer count value.
    ///              - The dynamic memory view mapping slot object type is not unused, resource, of UAV.
    ///          + ErrorInvalidFormat if:
    ///              - Blending is enabled by the color target format doesn't support blending.
    ///              - Logic operations are enabled by an incompatible format is used.
    ///          + ErrorUnsupportedShaderIlVersion if an incorrect shader type is used in any shader stage.
    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IPipeline**                       ppPipeline) = 0;

    /// Determines the amount of system memory required for a MSAA state object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateMsaaState().
    ///
    /// @param [in]  createInfo MSAA state creation properties.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an @ref IMsaaState object with the specified properties.
    ///          A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetMsaaStateSize(
        const MsaaStateCreateInfo& createInfo,
        Result*                    pResult) const = 0;

    /// Creates an @ref IMsaaState object with the requested properties.
    ///
    /// @param [in]  createInfo      Properties of the MSAA state object to create.
    /// @param [in]  pPlacementAddr  Pointer to the location where PAL should construct this object.  There must be as
    ///                              much size available here as reported by calling GetMsaaStateSize() with the same
    ///                              createInfo param.
    /// @param [out] ppMsaaState     Constructed MSAA state object.  When successful, the returned address will be the
    ///                              same as specified in pPlacementAddr.
    ///
    /// @returns Success if the MSAA state was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppMsaaState is null.
    ///          + ErrorInvalidValue if:
    ///              - The number of samples is unsupported.
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const = 0;
    /// Determines the amount of system memory required for a color blend state object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateColorBlendState().
    ///
    /// @param [in]  createInfo Color blend state creation properties.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an @ref IColorBlendState object with the specified
    ///          properties.  A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetColorBlendStateSize(
        const ColorBlendStateCreateInfo& createInfo,
        Result*                          pResult) const = 0;

    /// Creates an @ref IColorBlendState object with the requested properties.
    ///
    /// @param [in]  createInfo        Properties of the color blend state object to create.
    /// @param [in]  pPlacementAddr    Pointer to the location where PAL should construct this object.  There must be as
    ///                                much size available here as reported by calling GetColorBlendStateSize() with the
    ///                                same createInfo param.
    /// @param [out] ppColorBlendState Constructed color blend state object.  When successful, the returned address will
    ///                                be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the color blend state was successfully created.  Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppColorBlendState is null.
    ///          + ErrorInvalidValue if:
    ///              - An unsupported blend function is used with dual source blending.
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const = 0;

    /// Determines the amount of system memory required for a depth/stencil state object.  An allocation of this amount
    /// of memory must be provided in the pPlacementAddr parameter of CreateDepthStencilState().
    ///
    /// @param [in]  createInfo Depth/stencil state creation properties.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an @ref IDepthStencilState object with the specified
    ///          properties.  A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetDepthStencilStateSize(
        const DepthStencilStateCreateInfo& createInfo,
        Result*                            pResult) const = 0;

    /// Creates an @ref IDepthStencilState object with the requested properties.
    ///
    /// @param [in]  createInfo          Properties of the depth/stencil state object to create.
    /// @param [in]  pPlacementAddr      Pointer to the location where PAL should construct this object.  There must be
    ///                                  as much size available here as reported by calling GetDepthStencilStateSize()
    ///                                  with the same createInfo param.
    /// @param [out] ppDepthStencilState Constructed depth/stencil state object.  When successful, the returned address
    ///                                  will be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the depth/stencil state was successfully created.  Otherwise, one of the following errors
    ///          may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppDepthStencilState is null.
    ///          + ErrorInvalidValue if:
    ///              - Depth bounds is enabled and the depth range is invalid.
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const = 0;

    /// Determines the amount of system memory required for a queue semaphore object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateQueueSemaphore().
    ///
    /// @param [in]  createInfo Data controlling the queue semaphore properties, such as an initial semaphore count.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IQueueSemaphore object with the specified properties.
    ///          A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetQueueSemaphoreSize(
        const QueueSemaphoreCreateInfo& createInfo,
        Result*                         pResult) const = 0;

    /// Creates an @ref IQueueSemaphore object with the requested properties.
    ///
    /// @param [in]  createInfo       Data controlling the queue semaphore properties, such as an initial semaphore
    ///                               count.
    /// @param [in]  pPlacementAddr   Pointer to the location where PAL should construct this object.  There must be as
    ///                               much size available here as reported by calling GetQueueSemaphoreSize() with the
    ///                               same createInfo param.
    /// @param [out] ppQueueSemaphore Constructed queue semaphore object.  When successful, the returned address will be
    ///                               the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the queue semaphore was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppGpuMemory is null.
    ///          + ErrorInvalidValue if createInfo.initialCount is outside of the [0..31] range.
    virtual Result CreateQueueSemaphore(
        const QueueSemaphoreCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IQueueSemaphore**               ppQueueSemaphore) = 0;

    /// Determines the amount of system memory required for a queue semaphore object created by opening a semaphore
    /// from a different device.  An allocation of this amount of memory must be provided in the pPlacementAddr
    /// parameter of OpenSharedQueueSemaphore().
    ///
    /// @param [in]  openInfo Specifies a handle to a shared queue semaphore object to open.
    /// @param [out] pResult  The validation result if pResult is non-null. This argument can be null to avoid the
    ///                       additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for opening a shared IQueueSemaphore object with the
    ///          specified properties.  A return value of 0 indicates the openInfo was invalid.
    virtual size_t GetSharedQueueSemaphoreSize(
        const QueueSemaphoreOpenInfo& openInfo,
        Result*                       pResult) const = 0;

    /// Opens a shareable queue semaphore object created on another device for use on this device.
    ///
    /// @param [in]  openInfo         Specifies a handle to a queue semaphore memory object to open.
    /// @param [in]  pPlacementAddr   Pointer to the location where PAL should construct this object.  There must be as
    ///                               much size available here as reported by calling GetSharedQueueSemaphoreSize() with
    ///                               the same params.
    /// @param [out] ppQueueSemaphore Constructed queue semaphore object.  When successful, the returned address will be
    ///                               the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the shared semaphore was successfully opened for access on this device.  Otherwise, one of
    ///          the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppQueueSemaphore is null.
    ///          + ErrorNotShareable if the specified queue semaphore object was not marked as shareable on creation.
    virtual Result OpenSharedQueueSemaphore(
        const QueueSemaphoreOpenInfo& openInfo,
        void*                         pPlacementAddr,
        IQueueSemaphore**             ppQueueSemaphore) = 0;

    /// Determines the amount of system memory required for a queue semaphore object created by opening a semaphore from
    /// a different API which isn't a PAL client.  An allocation of this amount of memory must be provided in the
    /// pPlacementAddr parameter of OpenExternalSharedQueueSemaphore().
    ///
    /// @param [in]  openInfo Specifies a handle to a shared queue semaphore object to open.
    /// @param [out] pResult  The validation result if pResult is non-null. This argument can be null to avoid
    ///                       the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for opening a shared IQueueSemaphore object with the
    ///          specified properties.  A return value of 0 indicates the openInfo was invalid.
    virtual size_t GetExternalSharedQueueSemaphoreSize(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        Result*                               pResult) const = 0;

    /// Opens a shareable queue semaphore object created on another API which isn't a PAL client for use on this device.
    ///
    /// @param [in]  openInfo         Specifies a handle to a queue semaphore memory object to open and flags.
    /// @param [in]  pPlacementAddr   Pointer to the location where PAL should construct this object.  There must be as
    ///                               much size available here as reported by calling GetSharedQueueSemaphoreSize() with
    ///                               the same params.
    /// @param [out] ppQueueSemaphore Constructed queue semaphore object.  When successful, the returned address will be
    ///                               the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the shared semaphore was successfully opened for access on this device.  Otherwise, one of
    ///          the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppQueueSemaphore is null.
    virtual Result OpenExternalSharedQueueSemaphore(
        const ExternalQueueSemaphoreOpenInfo& openInfo,
        void*                                 pPlacementAddr,
        IQueueSemaphore**                     ppQueueSemaphore) = 0;

    /// Determines the amount of system memory required for an IFence object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateFence().
    ///
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IFence object.
    virtual size_t GetFenceSize(
        Result* pResult) const = 0;

    /// Creates a GPU fence object.
    ///
    /// @param [in]  createInfo         Data controlling the fence properties
    /// @param [in]  pPlacementAddr     Pointer to the location where PAL should construct this object.  There must be as
    ///                                 much size available here as reported by calling GetFenceSize().
    /// @param [out] ppFence            Constructed fence object.  When successful, the returned address will be
    ///                                 the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the fence was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppFence is null.
    virtual Result CreateFence(
        const FenceCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IFence**               ppFence) const = 0;

    /// Opens a fence wihich was shared by another Device.
    ///
    /// @param  [in] openInfo         A reference to FenceOpenInfo, the handle is used if it's not null, or the
    ///                               event is opened via name.
    /// @param [in]  pPlacementAddr   Pointer to the location where PAL should construct this object.  There must be as
    ///                               much size available here as reported by calling GetFenceSize().
    /// @param [out] ppFence          Constructed fence object.  When successful, the returned address will be
    ///                               the same as specified in pPlacementAddr.
    /// @returns Success if the event was successfully reconstructed, otherwise an appropriate error code.
    virtual Result OpenFence(
        const FenceOpenInfo& openInfo,
        void*                pPlacementAddr,
        IFence**             ppFence) const = 0;

    /// Determines the amount of system memory required for an IGpuEvent object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateGpuEvent().
    ///
    /// @param [in]  createInfo Properties of the GPU event object to create.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IGpuEvent object.
    virtual size_t GetGpuEventSize(
        const GpuEventCreateInfo& createInfo,
        Result*                   pResult) const = 0;

    /// Creates a GPU event object.
    ///
    /// @param [in]  createInfo     Properties of the GPU event object to create.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetEventSize().
    /// @param [out] ppGpuEvent     Constructed event object.  When successful, the returned address will be the same as
    ///                             specified in pPlacementAddr.
    ///
    /// @returns Success if the event was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppEvent is null.
    virtual Result CreateGpuEvent(
        const GpuEventCreateInfo& createInfo,
        void*                     pPlacementAddr,
        IGpuEvent**               ppGpuEvent) = 0;

    /// Determines the amount of system memory required for a query pool object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateQueryPool().
    ///
    /// @param [in]  createInfo Data controlling the query pool, such as what type of queries and how many slots are in
    ///                         the pool.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IQueryPool object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetQueryPoolSize(
        const QueryPoolCreateInfo& createInfo,
        Result*                    pResult) const = 0;

    /// Creates an @ref IQueryPool object with the requested properties.
    ///
    /// @param [in]  createInfo     Data controlling the query pool, such as what type of queries and how many slots are
    ///                             in the pool.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetQueryPoolSize() with the same
    ///                             createInfo param.
    /// @param [out] ppQueryPool    Constructed query pool object.  When successful, the returned address will be the
    ///                             same as specified in pPlacementAddr.
    ///
    /// @returns Success if the query pool was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppQueryPool is null.
    ///          + ErrorInvalidValue if createInfo.numSlots is zero.
    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const = 0;

    /// Determines the amount of system memory required for a command allocator object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateCmdAllocator().
    ///
    /// @param [in]  createInfo Command allocator properties including GPU memory allocation sizes.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid the
    ///                         additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an ICmdAllocator object with the specified properties.
    ///          A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetCmdAllocatorSize(
        const CmdAllocatorCreateInfo& createInfo,
        Result*                       pResult) const = 0;

    /// Creates a command allocator object that can allocate GPU memory with the specified properties for use by command
    /// buffer objects.
    ///
    /// @param [in]  createInfo     Command allocator properties including GPU memory allocation sizes.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetCmdAllocatorSize() with the
    ///                             same createInfo param.
    /// @param [out] ppCmdAllocator Constructed command allocator object.  When successful, the returned address will be
    ///                             the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the command allocator was successfully created.  Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppCmdAllocator is null.
    virtual Result CreateCmdAllocator(
        const CmdAllocatorCreateInfo& createInfo,
        void*                         pPlacementAddr,
        ICmdAllocator**               ppCmdAllocator) = 0;

    /// Determines the amount of system memory required for a command buffer object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreateCmdBuffer().
    ///
    /// @param [in]  createInfo Command buffer properties including the target queue type.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an ICmdBuffer object with the specified properties.  A
    ///          return value of 0 indicates the createInfo was invalid.
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const = 0;

    /// Creates a command buffer object that can build work intended for a particular queue type.
    ///
    /// @param [in]  createInfo     Command buffer properties including the target queue type.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetCmdBufferSize() with the same
    ///                             createInfo param.
    /// @param [out] ppCmdBuffer    Constructed command buffer object.  When successful, the returned address will be
    ///                             the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the command buffer was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppCmdBuffer is null.
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) = 0;

    /// Determines the amount of system memory required for an indirect command generator object.  An allocation of this
    /// amount must be provided in the pPlacementAddr parameter of CreateIndirectCmdGenerator().
    ///
    /// @param [in]  createInfo Indirect command generator properties.
    /// @param [out] pResult    The validation result if pResult is non-null.  This argument can be null to avoid the
    ///                         additional validation steps.
    ///
    /// @returns Size, in bytes, of system memory required for an IIndirectCmdGenerator object with the specified
    ///          properties.  A return value of zero indicates the createInfo was invalid.
    virtual size_t GetIndirectCmdGeneratorSize(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        Result*                               pResult) const = 0;

    /// Creates an indirect command generator object which can translate an application-specified command buffer into a
    /// format understandable by the GPU.
    ///
    /// @param [in]  createInfo
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetIndirectCmdGeneratorSize() with
    ///                             the same createInfo param.
    /// @param [out] ppGenerator    Constructed indirect command generator object.  When successful, the returned
    ///                             address will be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the command generator was successfully created.  Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppGenerator is null.
    virtual Result CreateIndirectCmdGenerator(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        void*                                 pPlacementAddr,
        IIndirectCmdGenerator**               ppGenerator) const = 0;

    /// Determines the amount of system memory required for a perf experiment object.  An allocation of this amount of
    /// memory must be provided in the pPlacementAddr parameter of CreatePerfExperiment().
    ///
    /// @param [in]  createInfo Properties of the performance experiment to be created.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an IPerfExperiment object with the specified properties.
    ///          A return value of 0 indicates the createInfo was invalid.
    virtual size_t GetPerfExperimentSize(
        const PerfExperimentCreateInfo& createInfo,
        Result*                         pResult) const = 0;

    /// Creates a performance experiment object that can gather performance counter and trace data for a specific span
    /// of a command buffer.
    ///
    /// @param [in]  createInfo       Properties of the performance experiment to be created.
    /// @param [in]  pPlacementAddr   Pointer to the location where PAL should construct this object.  There must be as
    ///                               much size available here as reported by calling GetPerfExperimentSize() with the
    ///                               same createInfo param.
    /// @param [out] ppPerfExperiment Constructed performance experiment object.  When successful, the returned address
    ///                               will be the same as specified in pPlacementAddr.
    ///
    /// @returns Success if the perf experiment was successfully created.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorInvalidPointer if pPlacementAddr or ppCmdBuffer is null.
    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const = 0;

    /// Gets @ref IPrivateScreen objects owned by this device. Private screens are screens not exposed through standard
    /// OS mechanisms. This function should be called again when any of the private screens are plugged or unplugged.
    /// The first call to this function enumerates all private screens and stores in device object as well. The next
    /// call triggered by hot-plug event enumerates private screens again but only destroys removed ones and creates
    /// new private screen objects for newly-added ones. The hash code generated at enumeration time is used as id of
    /// private screens. If the id of an enumerated private screen already exists, it is treated as unchanged. The EDID
    /// array and display index are used to generate MD5 hash code.
    ///
    /// @param [out]  pNumScreens  Pointer to the number of private sceens, note that this number does not mean first
    ///                            *pNumScreens elements in ppScreens are valid but just a hint that total *pNumScreens
    ///                            out of MaxPrivateScreens are valid.
    /// @param [out]  ppScreens    Pointer to the array of private screens. The client must pass in the pointer to an
    ///                            array of at least MaxPrivateScreens pointers to IPrivateScreen.
    ///
    /// @returns Success if the private screens are correctly retrieved. Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if the device does not support private screen functionalities.
    virtual Result GetPrivateScreens(
        uint32*          pNumScreens,
        IPrivateScreen** ppScreens) = 0;

    /// Registers an emulated @ref IPrivateScreen objects owned by this device. An emulated private screen doesn't have
    /// a physical display hardware connected to the GPU. This could be useful for debugging unusual configurations or
    /// using in automation systems when no real HMDs are available.
    ///
    /// @param [in]   createInfo   Properties of to create an emulated private screen object.
    /// @param [out]  pTargetId    Pointer to returned emulated private screen target id.
    ///
    /// @returns Success if the emulated private screen is correctly created. Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorTooManyPrivateScreens if the device cannot create an emulated private screen.
    virtual Result AddEmulatedPrivateScreen(
        const PrivateScreenCreateInfo& createInfo,
        uint32*                        pTargetId) = 0;

    /// Removes an emulated @ref IPrivateScreen objects owned by this device.
    ///
    /// @param [in]  targetId       Target id of emulated private screen to be removed.
    ///
    /// @returns Success if the emulated private screen is correctly removed. Otherwise, one of the following errors may
    ///          be returned:
    ///          + ErrorUnknown if any unknown error occurs.
    virtual Result RemoveEmulatedPrivateScreen(
        uint32 targetId) = 0;

    /// Determines the amount of system memory required for a private screen image object (and an associated memory
    /// object).  Allocations of these amounts of memory must be provided in the pImagePlacementAddr and
    /// pGpuMemoryPlacementAddr parameters of CreatePrivateScreenImage().
    ///
    /// Only images created through this interface are valid sources for IPrivateScreen::Present().
    ///
    /// @param [in]  createInfo     Properties of the image to create such as width/height and pixel format.
    /// @param [out] pImageSize     Size, in bytes, of system memory required for the IImage.
    ///                             Should be specified to the pImagePlacementAddr argument of CreatePresentableImage().
    /// @param [out] pGpuMemorySize Size, in bytes, of system memory required for a dummy IGpuMemory object attached to
    ///                             the private screen IImage. Should be specified to the pGpuMemoryPlacementAddr
    ///                             argument of CreatePrivateScreenImage().
    /// @param [out] pResult        The validation result if pResult is non-null. This argument can be null to avoid
    ///                             the additional validation.
    virtual void GetPrivateScreenImageSizes(
        const PrivateScreenImageCreateInfo& createInfo,
        size_t*                             pImageSize,
        size_t*                             pGpuMemorySize,
        Result*                             pResult) const = 0;

    /// Creates private screen presentable image. A private screen presentable image is similar to a regular presentable
    /// image but can only be presented on the private screens. It has some implicit properties relative to standard
    /// images, such as mipLevels=1, arraySize=1, numSamples=1 and etc. It also requires its bound GPU memory to be
    /// pinned before presenting.
    ///
    /// @param [in]  createInfo              Create info.
    /// @param [in]  pImagePlacementAddr     Pointer to the location where PAL should construct this object. There must
    ///                                      be as much size available here as reported by calling
    ///                                      GetPrivateScreenImageSizes().
    /// @param [in]  pGpuMemoryPlacementAddr Pointer to the location where PAL should construct a IGpuMemory associated
    ///                                      with this peer image.  There must be as much size available here as
    ///                                      reported by calling GetPrivateScreenImageSizes().
    /// @param [out] ppImage                 Constructed image object.
    /// @param [out] ppGpuMemory             Constructed dummy memory object.  This object is only valid for specifying
    ///                                      in a memory reference list.
    ///
    /// @returns Success if the image was successfully created.  Otherwise, one of the following errors may be returned:
    ///          + ErrorPrivateScreenInvalidFormat if the format isn't supported on the private screen.
    ///          + ErrorPrivateScreenRemoved if the private screen was removed.
    virtual Result CreatePrivateScreenImage(
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory) = 0;

    /// Determines the amount of system memory required for an ISwapChain object.  An allocation of this amount of memory
    /// must be provided in the pPlacementAddr parameter of CreateSwapChain().
    ///
    /// @param [in]  createInfo All the information related with this swap chain.
    /// @param [out] pResult    The validation result if pResult is non-null. This argument can be null to avoid
    ///                         the additional validation.
    ///
    /// @returns Size, in bytes, of system memory required for an ISwapChain object.
    virtual size_t GetSwapChainSize(
        const SwapChainCreateInfo& createInfo,
        Result*                    pResult) const = 0;

    /// Create swap chain object based on the local window system. It doesn't include creation of presentable images.
    /// The presentable images should be associated with SwapChain object when presentable image is created.
    ///
    /// @param [in]  createInfo     All the information related with this swap chain.
    /// @param [in]  pPlacementAddr Pointer to the location where PAL should construct this object.  There must be as
    ///                             much size available here as reported by calling GetSwapChainSize().
    /// @param [out] ppSwapChain    Constructed swapchain object.  When successful, the returned address will be the same as
    ///                             specified in pPlacementAddr.
    ///
    /// @returns Success if create swap chain instance successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result CreateSwapChain(
        const SwapChainCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ISwapChain**               ppSwapChain) = 0;

    /// Sets a power profile for this device.
    ///
    /// @param [in]      profile A profile is a pre-defined configuration indicates how KMD/PPLib is notified to work,
    ///                  e.g. raise or lower the GPU clock etc.
    /// @param [in,out]  pInfo Custom power profile info needed for VrCustom mode, can be null for other modes. Note the
    ///                  actualSwitchInfo[] field is output part of @ref CustomPowerProfile.
    ///
    /// @returns Success if the profile is set successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if this function is not available on this OS.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result SetPowerProfile(
        PowerProfile        profile,
        CustomPowerProfile* pInfo) = 0;

    /// Queries workstation caps on this device.
    ///
    /// @param [out]  pCaps Pointer to location where pal should write back workstation caps.
    ///
    /// @returns Success if wokstation caps is got from KMD successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorOutOfMemory if out of system memory.
    virtual Result QueryWorkStationCaps(
        WorkStationCaps* pCaps) const = 0;

    /// Queries display connectors installed on the GPU
    ///
    /// @param [in,out]  pConnectorCount Input value specifies the maximum number of connectors to enumerate, and the
    ///                                  output value specifies the total number of display modes that were enumerated
    ///                                  in pConnectors.  The input value is ignored if pConnectors is null.
    ///                                  This pointer must not be null.
    /// @param [out]     pConnectors     Output list of connectors.  Can be null, in which case the total number of
    ///                                  available connectors will be written to pConnectorCount.
    ///
    ///
    /// @returns Success if the profile is set successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if this function is not available on this OS.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result QueryDisplayConnectors(
        uint32*                     pConnectorCount,
        DisplayConnectorProperties* pConnectors) = 0;

    /// Get the valid FormatFeatureFlags for the provided ChNumFormat, ImageAspect, and ImageTiling.
    /// Formats report all supported operations.  For certain aspects some of those operations might be unsupported.
    /// GetValidFormatFeatureFlags is a helper to return only the valid FormatFeatureFlags for a particular aspect.
    ///
    /// @param [in]  format ChNumFormat to get the FormatFeatureFlags from.
    /// @param [in]  aspect ImageAspect to query with.
    /// @param [in]  tiling ImageTiling to query with.
    ///
    /// @returns The valid FormatFeatureFlags.
    virtual uint32 GetValidFormatFeatureFlags(
        const ChNumFormat format,
        const ImageAspect aspect,
        const ImageTiling tiling) const = 0;

    /// @}

    /// Query the Flgl state from the device. Device will query the connectivity of GLSync card and return
    /// the state. Pal internal state of Flgl will be updated.
    ///
    /// @param [out]    pState  Pointer to the location that PAL should write the internal flgl states back.
    ///
    /// @returns Success if query returns with success. Otherwise, one of the following errors may returned:
    ///          + ErrorOutOfMemory if out of system memory.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result FlglQueryState(
       FlglState* pState) = 0;

    /// Set the Framelock to disable or enable. Client should call this interface first to enable/disable Flgl,
    /// then submit CmdFlglEnable()/CmdFlglDisable() to corresponding queue
    ///
    /// @param [in]    enable           If true enables the framelock, otherwise disables framelock.
    ///
    /// @returns Success if framelock enable/disable successfully. Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if this function is not supported on this Asic.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result FlglSetFrameLock(
       bool enable) = 0;

    /// Reset the framelock HW counter. The following counter operations are directly submit to hardware via I2C
    /// interface Pal doesn't store the counter internally. Client should manage the counter
    ///
    /// @returns Success if the HW counter is reset successfully.  Otherwise, one of the following errors may be
    ///          returned:
    ///          + ErrorUnavailable if this function is not available on this Asic.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result FlglResetFrameCounter() const = 0;

    /// Check if a reset/discontinuity of HW framecounter occurs. If returns false, there is no need to query HW frame
    /// counter, client should update its software counter instead. If returns true, client is required to query HW
    /// counter and adjusts its software counter accordingly.
    ///
    /// @param [out]    pReset   Pointer to the location that PAL should write the reset status back.
    ///
    /// @returns Success if the reset status is returned.  Otherwise, one of the following errors may be returned:
    ///          + ErrorUnavailable if this function is not available on this Asic.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result FlglGetFrameCounterResetStatus(
        bool* pReset) const = 0;

    /// Get the framelock HW counter.
    ///
    /// @param [out]    pValue   Pointer to the location that PAL should write the frame counter value back.
    ///
    /// @returns Success if the frame counter is returned.  Otherwise, one of the following errors may be returned:
    ///          + ErrorUnavailable if this function is not available on this Asic.
    ///          + ErrorUnknown if an unexpected internal error occurs.
    virtual Result FlglGetFrameCounter(
        uint64* pValue) const = 0;

    /// Checks if Chill settings have changed since the last time the function was called.
    /// This is intended to be a lightweight function that can be called once a frame. If the function returns
    /// Result::Success and *pChangeDetected = true, then the user changed some Chill related settings in the UI.
    /// The Pal client should re-read the Chill application profile settings by calling
    /// IPlatform::QueryRawApplicationProfile() with client = ApplicationProfileClient::Chill for the system
    /// app profiles and then with client = ApplicationProfileClient::User3D for any per user
    /// chill overrides.
    ///
    /// @param [out] pChangeDetected    Pointer to the location that PAL should write the change status back
    ///
    /// @returns Success if the call succeeded.
    virtual Result DidChillSettingsChange(
        bool* pChangeDetected) = 0;

    /// Gets the value of a KMD-managed flag for globally enabling Chill. This function will typically be called
    /// at the same time as the DidChillSettingsChange(), and is also very lightweight.
    ///
    /// @param [out]    pGlobalEnable    Pointer to the location where PAL should write the flag value
    ///
    /// @returns Success if the call succeeded.
    virtual Result GetChillGlobalEnable(
        bool* pGlobalEnable) = 0;

    /// Update Chill Status (last active time stamp). After every frame, UMD needs to generate a time stamp and inform
    /// KMD through the shared memory, if the time stamp changes between 2 frames, it means Chill is active and KMD
    /// needs to adjust power through PSM.
    ///
    /// @param [in]  lastChillActiveTimeStampUs     the last Chill active time stamp in microseconds to set
    ///
    /// @returns Success if the call succeeded.
    virtual Result UpdateChillStatus(
        uint64 lastChillActiveTimeStampUs) = 0;

    /// Make the Bus Addressable allocations available to be accessed by remote device.
    /// Exposes the surface and marker bus addresses for each allocation. These bus addresses can be accessed by
    /// calling @ref IGpuMemory::Desc() on the appropriate object.
    /// Client drivers must call @ref AddGpuMemoryReferences() for all relevant allocations before calling this.
    ///
    /// @param [in]  pQueue         Queue used by PAL for performing this operation.
    /// @param [in]  gpuMemCount    Number of GPU memory allocations to expose to remote devices.
    /// @param [in]  ppGpuMemList   Array of gpuMemCount IGpuMemory objects.
    ///
    /// @returns Success if bus addresses are available by calling @ref IGpuMemory::Desc() on all IGpuMemory objects
    virtual Result InitBusAddressableGpuMemory(
        IQueue*           pQueue,
        uint32            gpuMemCount,
        IGpuMemory*const* ppGpuMemList) = 0;

    /// Create virtual display. Virtual display is similar to the regular display (IScreen), the difference is the
    /// virtual display doesn't have a physical monitor connected. When CreateVirtualDisplay is called, KMD will
    /// generate a hot-plug-in event to notify application a new display is added. Then the app/client will call PAL to
    /// re-querythe attached screens and they will find a new one in the list that is pretend, but they can use it just
    /// like a normal display.
    ///
    /// @param [in]  virtualDisplayInfo   Virtual display creation infomation.
    /// @param [out] pScreenTargetId      The screen target ID returned by KMD
    ///
    /// @returns Success if the call succeeded.
    virtual Result CreateVirtualDisplay(
        const VirtualDisplayInfo& virtualDisplayInfo,
        uint32*                   pScreenTargetId) = 0;

    /// Destroy virtual display. When DestroyVirtualDisplay is called KMD will generate a hot-plug-out event to notify
    /// application a virtual display is removed, it also will be removed from the display list, and app/client can't
    /// uses it anymore.
    ///
    /// @param [in]  screenTargetId  Screen target ID.
    ///
    /// @returns Success if the call succeeded.
    virtual Result DestroyVirtualDisplay(
        uint32     screenTargetId) = 0;

    /// Query virtual display Properties from screen target Id.
    ///
    /// @param [in]  screenTargetId             Screen target ID.
    /// @param [out] pVirtualDisplayProperties  A pointer to VirtualDisplayProperties
    ///
    /// @returns Success if the call succeeded.
    virtual Result GetVirtualDisplayProperties(
        uint32                    screenTargetId,
        VirtualDisplayProperties* pProperties) = 0;

    /// Determines if hardware accelerated stereo rendering can be enabled for given graphic pipeline.
    /// If hardware accelerate stereo rendering can be enabled, client doesn't need to do shader patching
    /// which includes translating view id intrinsic to user data slot, outputing render target
    /// array index and viewport array index in shader closest to scan converter.
    ///
    /// @param [in]  viewInstancingInfo  Graphic pipeline view instancing information.
    ///
    /// @returns True if hardware accelerated stereo rendering can be enabled, False otherwise.
    virtual bool DetermineHwStereoRenderingSupported(
        const GraphicPipelineViewInstancingInfo& viewInstancingInfo) const = 0;

    /// Get connector ID from RandR output object.
    ///
    /// @param [in]   hDisplay        Display handle of the window system.
    /// @param [in]   randrOutput     RandR output object which is going to be leased. The output represents the
    ///                               underlying display hardware which include encoder and connector.
    /// @param [in]   wsiPlatform     WSI platform.
    /// @param [out]  pConnectorId    Connector ID. Connector represents a display connector (HDMI, DP, VGA, DVI...).
    ///
    /// @returns Success if the call succeeded.
    virtual Result GetConnectorIdFromOutput(
        OsDisplayHandle hDisplay,
        uint32          randrOutput,
        WsiPlatform     wsiPlatform,
        uint32*         pConnectorId) = 0;

    /// Get file path used to put all files for cache purpose
    ///
    /// @returns Pointer to cache file path.
    virtual const char* GetCacheFilePath() const = 0;

    /// Get file path used to put all files for debug purpose (such as logs, dumps, replace shader)
    ///
    /// @returns Pointer to debug file path.
    virtual const char* GetDebugFilePath() const = 0;

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
    IDevice() : m_pClientData(nullptr) {}

    /// @internal Destructor.  Prevent use of delete operator on this interface.  Device objects will be destroyed when
    ///           @ref IPlatform::Destroy() is called.
    virtual ~IDevice() { }

    /// Function pointer table for SRD creation methods.
    struct DevicePfnTable
    {
        CreateBufferViewSrdsFunc pfnCreateTypedBufViewSrds;     ///< Typed Buffer view SRD creation function pointer.
        CreateBufferViewSrdsFunc pfnCreateUntypedBufViewSrds;   ///< Untyped Buffer view SRD creation function ptr.
        CreateImageViewSrdsFunc  pfnCreateImageViewSrds;        ///< Image view SRD creation function pointer.
        CreateFmaskViewSrdsFunc  pfnCreateFmaskViewSrds;        ///< Fmask View SRD creation function pointer.
        CreateSamplerSrdsFunc    pfnCreateSamplerSrds;          ///< Sampler SRD creation function pointer.
    } m_pfnTable;                                               ///< SRD creation function pointer table.

private:
    /// @internal Client data pointer. This can have an arbitrary value and can be returned by calling GetClientData()
    /// and set via SetClientData().
    /// For non-top-layer objects, this will point to the layer above the current object.
    void* m_pClientData;
};

/**
 ***********************************************************************************************************************
 * @defgroup ResourceBinding Resource Binding Model
 *
 * _Resource binding_ refers to the process of binding resources (textures, UAVs, samplers, etc.) for access by shaders
 * in a pipeline.  This is an area where 3D APIs diverge significantly.  PAL's model is designed to minimally abstract
 * the underlying hardware in a way that allows performant implementations by each client driver.
 *
 * ### Hardware User Data
 * GCN hardware has 16 _user data_ registers that act as a generic interface for passing values from a command buffer to
 * a shader.  User data registers are set to their desired value via packets in a command buffer, then the specified
 * values are loaded from the user data registers into shader GPRs when a wave is launched.  Since the user data is just
 * arbitrary generic data, this method can be used to pass any type of data a client may want to specify directly from
 * a command buffer, for example:
 *
 * - __Constant value__ - a 32-bit floating point or integer constant could be written into user data then be used
 *   directly by the shader.
 * - __Shader resource descriptor (SRD)__ - 4 or 8 dwords of consecutive data could be an SRD which will be used as a
 *   t#, s#, etc. by the shader.
 * - __Pointer__ - The user data could be an arbitrary GPU virtual address where a table of constants, SRDs, etc. are
 *   stored.
 *
 * The 3D driver and shader compiler are responsible for working together to define how resources referenced in a shader
 * should be mapped to user data bound in a command buffer.
 *
 * ### PAL User Data
 * PAL only lightly abstracts the hardware user data concept.  DeviceProperties reports the number of user data entries
 * supported on the device: maxUserDataEntries and fastUserDataEntries.  Note that some clients may require more
 * user data entries than there are physical user data registers - fastUserDataEntries gives a hint as to how many
 * user data entries will fit in hardware registers.  PAL will manage "spilling" of user data entries to GPU memory if
 * necessary.
 *
 * User data entries are set in a command buffer by calling ICmdBuffer::CmdSetUserData().
 *
 * ### Shader User Data Mapping
 * When creating a pipeline, the client must specify how the user data entries set in a command buffer map to resources
 * referenced by each shader in the pipeline.  This is done in the pUserDataNodes array of PipelineShaderInfo.
 *
 * The resource mapping is built as a graph of _resource mapping nodes_ where the root nodes in the graph correspond
 * to the user data entries.  Each node fits in one of the following categories:
 *
 * - __SRD__: A 4 or 8 dword descriptor describing a shader resource.  The mapping specifies the type and slot the SRD
 *   corresponds to (e.g., UAV 3 or sampler 7).
 * - __Descriptor table pointer__: A GPU virtual address pointing at an array of other nodes.  Typically this will be a
 *   pointer to GPU memory containing just SRDs, but tables are free to be built hierarchically such that tables have
 *   pointers to other tables in them.
 * - __Inline constants__: 32-bit constants loaded directly byu the shader.  The mapping specified the CB slot that
 *   should load the constant (e.g., cb3[1]).
 * - __Unused__: A particular shader may not use all entries in a user data layout, and those should be marked unused.
 *
 * The following image illustrates a simple user data mapping:
 *
 * @image html userDataMapping.png
 *
 * ### Building Descriptor Tables
 * The client is responsible for building specifying SRDs and pointers to GPU memory in order to execute the shader
 * resource mapping specified during pipeline creation.  SRDs can be created with several methods provided by IDevice:
 *
 * - CreateTypedBufferViewSrds()
 * - CreateUntypedBufferViewSrds()
 * - CreateImageViewSrds()
 * - CreateFmaskViewSrds()
 * - CreateSamplerSrds()
 *
 * The size required for each of these SRD types is returned in the srdSizes structure in DeviceProperties.
 *
 * When building descriptor tables in GPU memory, the client will need to retrieve a virtual address of the GPU memory
 * where the tables exist in order to reference them from user data or from other descriptor tables.  IGpuMemory
 * provides the GetVirtAddr() method for this purpose.
 ***********************************************************************************************************************
 */

} // Pal
