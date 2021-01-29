/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palDeveloperHooks.h
 * @brief Common include for PAL developer callbacks. Defines common enums, typedefs, structures, etc.
 ***********************************************************************************************************************
 */

#pragma once

#include "pal.h"
#include "palCmdBuffer.h"

namespace Pal
{

// Forward declarations.
class ICmdBuffer;
class IImage;
class IPipeline;

namespace Developer
{

/// The type of the developer callback so the callback can properly perform whatever actions it needs.
///
/// @see Callback
enum class CallbackType : uint32
{
    AllocGpuMemory = 0,     ///< This callback is to inform that GPU memory has been allocated.
    FreeGpuMemory,          ///< This callback is to inform that GPU memory has been freed.
    PresentConcluded,       ///< This callback is to inform that a present has concluded.
    ImageBarrier,           ///< This callback is to inform that a barrier is being executed.
    CreateImage,            ///< This callback is to inform that an image has been created.
    BarrierBegin,           ///< This callback is to inform that a barrier is about to be executed.
    BarrierEnd,             ///< This callback is to inform that a barrier is done being executed.
    DrawDispatch,           ///< This callback is to inform that a draw or dispatch command is being recorded.
    BindPipeline,           ///< This callback is to inform that a pipeline (client or internal) has been bound.
#if PAL_BUILD_PM4_INSTRUMENTOR
    DrawDispatchValidation, ///< This callback is to describe the state validation needed by a draw or dispatch.
    OptimizedRegisters,     ///< This callback is to describe the PM4 optimizer's removal of redundant register
                            ///  sets.
#endif
    Count,                  ///< The number of info types.
};

/// Definition for developer callback.
///
/// @param [in] pPrivateData    Private data that is installed with the callback for use by the installer.
/// @param [in] deviceIndex     Unique index for the device so that the installer can properly dispatch the event.
/// @param [in] infoType        Information about the callback so the installer can make informed decisions about
///                             what actions to perform.
/// @param [in] pInfoData       Additional data related to the particular callback type.
typedef void (PAL_STDCALL *Callback)(
    void*           pPrivateData,
    const uint32    deviceIndex,
    CallbackType    type,
    void*           pCbData);

/// Enumeration describing the different ways GPU memory is allocated.
enum class GpuMemoryAllocationMethod : uint32
{
    Unassigned = 0,                         ///< Unassigned allocation method.
    Normal,                                 ///< Virtual memory allocation (not pinned/peer).
    Pinned,                                 ///< Pinned memory allocation.
    Peer,                                   ///< Peer memory allocation.
    MultiDevice,                            ///< MultiDevice memory allocation.
    Opened,                                 ///< Shared memory allocation.
    Svm,                                    ///< Shared virtual memory allocation.
};

/// Enumeration describing the different Presentation modes an application can take.
enum class PresentModeType : uint32
{
    Unknown = 0,                            ///< When the present mode is not known.
    Flip,                                   ///< when the presentation surface is used directly as the front buffer.
    Composite,                              ///< When the flipped image is drawn by a window compositor instead
                                            ///  of the application.
    Blit,                                   ///< when the presentation surface is copied to the front buffer.
};

/// Information about the presentation mode an application is in.
struct PresentationModeData
{
    PresentModeType presentationMode;       ///< Information about present mode from above enumeration.
};

/// Information for allocation/deallocation of GPU memory.
struct GpuMemoryData
{
    gpusize size;                           ///< Size, in bytes, of the allocation.
    GpuHeap heap;                           ///< The first requested heap of the allocation.

    /// Allocation description flags
    struct Flags
    {
        uint32 isClient         :  1;       ///< This allocation is requested by the client.
        uint32 isFlippable      :  1;       ///< This allocation is marked as flippable.
        uint32 isUdmaBuffer     :  1;       ///< This allocation is for a UDMA buffer.
        uint32 isVirtual        :  1;       ///< This allocation is for virtual memory.
        uint32 isCmdAllocator   :  1;       ///< This allocation is for a CmdAllocator.
        uint32 reserved         : 27;       ///< Reserved for future use.
    } flags;                                ///< Flags describing the allocation.

    GpuMemoryAllocationMethod allocMethod;  ///< Allocation method
};

/// Information pertaining to the cache flush/invalidations and stalls performed during barrier execution.
struct BarrierOperations
{
    union
    {
        struct
        {
            uint16 eopTsBottomOfPipe              : 1;  ///< Issue an end-of-pipe event that can be waited on.
                                                        ///  When combined with waitOnTs, makes a full pipeline stall.
            uint16 vsPartialFlush                 : 1;  ///< Stall at ME, waiting for all prior VS waves to complete.
            uint16 psPartialFlush                 : 1;  ///< Stall at ME, waiting for all prior PS waves to complete.
            uint16 csPartialFlush                 : 1;  ///< Stall at ME, waiting for all prior CS waves to complete.
            uint16 pfpSyncMe                      : 1;  ///< Stall PFP until ME is at same point in command stream.
                                                        ///  flushed/invalidated are specified in the caches bitfield.
            uint16 syncCpDma                      : 1;  ///< Issue dummy cpDma command to confirm all prior cpDmas have
                                                        ///  completed.
            uint16 eosTsPsDone                    : 1;  ///< Issue an end-of-pixel-shader event that can be waited on.
            uint16 eosTsCsDone                    : 1;  ///< Issue an end-of-compute-shader event that can be waited on
            uint16 waitOnTs                       : 1;  ///< Wait on an timestamp event (EOP or EOS) at the ME.
                                                        ///  Which event is not necesarily specified here, though any
                                                        ///  that are specified here would be waited on.
            uint16 reserved                       : 7;  ///< Reserved for future use.
        };

        uint16 u16All;  ///< Unsigned integer containing all the values.

    } pipelineStalls; ///< Information about pipeline stalls performed.

    union
    {
        struct
        {
            uint16 depthStencilExpand      : 1; ///< Decompression of depth/stencil image.
            uint16 htileHiZRangeExpand     : 1; ///< Expansion of HTile's HiZ range.
            uint16 depthStencilResummarize : 1; ///< Resummarization of depth stencil.
            uint16 dccDecompress           : 1; ///< DCC decompress BLT for color images.
            uint16 fmaskDecompress         : 1; ///< Fmask decompression for shader readability.
            uint16 fastClearEliminate      : 1; ///< Expand latest specified clear color into pixel data for the fast
                                                ///  cleared color/depth resource.
            uint16 fmaskColorExpand        : 1; ///< Completely decompresses the specified color resource.
            uint16 initMaskRam             : 1; ///< Memsets uninitialized memory to prepare it for use as
                                                ///  CMask/FMask/DCC/HTile.
            uint16 updateDccStateMetadata  : 1; ///< DCC state metadata was updated.
            uint16 reserved                : 7; ///< Reserved for future use.
        };

        uint16 u16All; ///< Unsigned integer containing all the values.

    } layoutTransitions; ///< Information about layout translation performed.

    union
    {
        struct
        {
            uint16 invalTcp         : 1; ///< Invalidate vector caches.
            uint16 invalSqI$        : 1; ///< Invalidate the SQ instruction caches.
            uint16 invalSqK$        : 1; ///< Invalidate the SQ constant caches (scalar caches).
            uint16 flushTcc         : 1; ///< Flush L2 cache.
            uint16 invalTcc         : 1; ///< Invalidate L2 cache.
            uint16 flushCb          : 1; ///< Flush CB caches.
            uint16 invalCb          : 1; ///< Invalidate CB caches.
            uint16 flushDb          : 1; ///< Flush DB caches.
            uint16 invalDb          : 1; ///< Invalidate DB caches.
            uint16 invalCbMetadata  : 1; ///< Invalidate CB meta-data cache.
            uint16 flushCbMetadata  : 1; ///< Flush CB meta-data cache.
            uint16 invalDbMetadata  : 1; ///< Invalidate DB meta-data cache.
            uint16 flushDbMetadata  : 1; ///< Flush DB meta-data cache.
            uint16 invalTccMetadata : 1; ///< Invalidate TCC meta-data cache.
            uint16 invalGl1         : 1; ///< Invalidate the global L1 cache
            uint16 reserved         : 1; ///< Reserved for future use.
        };

        uint16 u16All; ///< Unsigned integer containing all the values.

    } caches; ///< Information about cache operations performed for the barrier.
};

/// Enumeration for PAL barrier reasons
enum BarrierReason : uint32
{
    BarrierReasonInvalid = 0,                               ///< Invalid barrier reason

    BarrierReasonFirst   = 0x80000000,                      ///< The first valid barrier reason value
                                                            ///  The only value that can smaller than this is the
                                                            ///  invalid value.
    BarrierReasonLast    = 0xbfffffff,                      ///< The last valid barrier reason value
                                                            ///  The only value that can larger than this is the
                                                            ///  unknown value.

    BarrierReasonPreComputeColorClear = BarrierReasonFirst, ///< Barrier issued before a color clear
    BarrierReasonPostComputeColorClear,                     ///< Barrier issued after a color clear
    BarrierReasonPreComputeDepthStencilClear,               ///< Barrier issued before a depth/stencil clear
    BarrierReasonPostComputeDepthStencilClear,              ///< Barrier issued after a depth/stencil clear
    BarrierReasonMlaaResolveEdgeSync,                       ///< Barrier issued to sync mlaa edge calculations
    BarrierReasonAqlWaitForParentKernel,                    ///< Barrier issued to wait for the parent kernel to
                                                            ///  complete in an AQL submission
    BarrierReasonAqlWaitForChildrenKernels,                 ///< Barrier issued to wait for the children kernels to
                                                            ///  complete in an AQL submission
    BarrierReasonP2PBlitSync,                               ///< Barrier issued to synchronize peer-to-peer blits
    BarrierReasonTimeGraphGrid,                             ///< Barrier issued to wait for the time graph grid
    BarrierReasonTimeGraphGpuLine,                          ///< Barrier issued to wait for the time graph gpu line
    BarrierReasonDebugOverlayText,                          ///< Barrier issued to wait for the debug overlay text
    BarrierReasonDebugOverlayGraph,                         ///< Barrier issued to wait for the debug overlay graph
    BarrierReasonDevDriverOverlay,                          ///< Barrier issued to wait for developer driver overlay
    BarrierReasonDmaImgScanlineCopySync,                    ///< Barrier issued to synchronize between image scanline
                                                            ///  copies on the dma hardware
    BarrierReasonPostSqttTrace,                             ///< Barrier issued to wait for work from an sqtt trace
    BarrierReasonPrePerfDataCopy,                           ///< Barrier issued to wait for perf data to become
                                                            ///  available for copy
    BarrierReasonFlushL2CachedData,                         ///< Barrier issued to flush L2 cached data to main memory
    BarrierReasonInternalLastDefined,                       ///< Only used for asserts.
    BarrierReasonUnknown = 0xFFFFFFFF,                      ///< Unknown barrier reason

    /// Backwards compatibility reasons
    BarrierReasonPreSyncClear  = BarrierReasonPreComputeColorClear,
    BarrierReasonPostSyncClear = BarrierReasonPostComputeColorClear
};

/// Style of barrier
enum class BarrierType : uint32
{
    Full,    ///< A traditional blocking barrier.
    Release, ///< A pipelined barrier that flushes caches and starts transitions.
    Acquire, ///< A barrier that waits on previous 'Release' barriers.
};

/// Information for barrier executions.
struct BarrierData
{
    ICmdBuffer*       pCmdBuffer;    ///< The command buffer that is executing the barrier.
    BarrierTransition transition;    ///< The particular transition that is currently executing.
    bool              hasTransition; ///< Whether or not the transition structure is populated.
    BarrierOperations operations;    ///< Detailed cache and pipeline operations performed during this barrier execution
    uint32            reason;        ///< Reason that the barrier was invoked. Only filled at BarrierStart.
    BarrierType       type;          ///< What style of barrier this is. Only filled at BarrierStart.
};

/// Enumeration describing the different types of tile mode dimensions
enum class Gfx6ImageTileModeDimension : uint32
{
    Linear = 0, ///< Linear tile mode.
    Dim1d,      ///< 1D tile mode.
    Dim2d,      ///< 2D tile mode.
    Dim3d,      ///< 3D tile mode.
};

/// Tile mode information
struct Gfx6ImageTileMode
{
    Gfx6ImageTileModeDimension dimension;   ///< Dimensionality of tile mode.

    union
    {
        struct
        {
            uint32 prt       : 1;   ///< Image is a PRT.
            uint32 thin      : 1;   ///< Thin tiled.
            uint32 thick     : 1;   ///< Thick tiled.
            uint32 reserved  : 29;  ///< Reserved for future use.
        };
        uint32 u32All;              ///< Flags packed as 32-bit uint.
    } properties;                   ///< Bitfield of properties
};

/// Enumeration describing the different tile types
enum class Gfx6ImageTileType : uint32
{
    Displayable = 0,    ///< Displayable tiling.
    NonDisplayable,     ///< Non-displayable tiling.
    DepthSampleOrder,   ///< Same as non-displayable plus depth-sample-order.
    Rotated,            ///< Rotated displayable tiling.
    Thick,              ///< Thick micro-tiling.
};

/// Meta-data-related properties
struct ImageMetaDataInfo
{
    union
    {
        struct
        {
            uint32 color                 : 1;   ///< Flag indicates this is a color buffer.
            uint32 depth                 : 1;   ///< Flag indicates this is a depth/stencil buffer.
            uint32 stencil               : 1;   ///< Flag indicates this is a stencil buffer.
            uint32 texture               : 1;   ///< Flag indicates this is a texture.
            uint32 cube                  : 1;   ///< Flag indicates this is a cubemap.
            uint32 volume                : 1;   ///< Flag indicates this is a volume texture.
            uint32 fmask                 : 1;   ///< Flag indicates this is an fmask.
            uint32 compressZ             : 1;   ///< Flag indicates z buffer is compressed.
            uint32 overlay               : 1;   ///< Flag indicates this is an overlay surface.
            uint32 noStencil             : 1;   ///< Flag indicates this depth has no separate stencil.
            uint32 display               : 1;   ///< Flag indicates this should match display controller req.
            uint32 opt4Space             : 1;   ///< Flag indicates this surface should be optimized for space
                                                ///  i.e. save some memory but may lose performance.
            uint32 prt                   : 1;   ///< Flag for partially resident texture.
            uint32 tcCompatible          : 1;   ///< Image's metadata is TC-compatible.  This reduces the maximum
                                                ///  compression levels, but allows the shader to read the data without
                                                ///  an expensive decompress operation.
            uint32 dccCompatible         : 1;   ///< GFX 8: whether to make MSAA surface support dcc fast clear.
            uint32 dccPipeWorkaround     : 1;   ///< GFX 8: whether to workaround the HW limit that
                                                ///  dcc can't be enabled if pipe config of tile mode
                                                ///  is different from that of ASIC.
            uint32 disableLinearOpt      : 1;   ///< Disable tile mode optimization to linear.
            uint32 reserved              : 15;  ///< Reserved for future use.
        };
        uint32 u32All;              ///< Flags packed as 32-bit uint.
    } properties;                   ///< Bitfield of properties
};

/// Information for allocation of a PAL Image - AddrLib surface info.
struct ImageDataAddrMgrSurfInfo
{
    union
    {
        struct
        {
            Gfx6ImageTileMode mode; ///< Tile mode.
            Gfx6ImageTileType type; ///< Micro tiling type.
        } gfx6;
        struct
        {
            uint32 swizzle;         ///< Swizzle mode.
        } gfx9;
    } tiling;

    ImageMetaDataInfo flags;    ///< Metadata info.
    uint64 size;                ///< Surface size, in bytes.
    uint32 bpp;                 ///< Bits per pixel.
    uint32 width;               ///< Width.
    uint32 height;              ///< Height.
    uint32 depth;               ///< Depth.
};

/// Type of draw or dispatch operation for a DrawDispatch callback
enum class DrawDispatchType : uint32
{
    CmdDraw = 0,                  ///< Auto-indexed draw
    CmdDrawOpaque,                ///< Auto draw
    CmdDrawIndexed,               ///< Indexed draw
    CmdDrawIndirectMulti,         ///< (Multi) indirect draw
    CmdDrawIndexedIndirectMulti,  ///< (Multi) indirect indexed draw
    CmdDispatchMesh,              ///< Task/Mesh shader dispatch.
    CmdDispatchMeshIndirectMulti, ///< Indirect Task/Mesh shader dispatch.
    CmdDispatch,                  ///< Direct compute dispatch
    CmdDispatchIndirect,          ///< Indirect compute dispatch
    CmdDispatchOffset,            ///< Direct compute dispatch (offsetted start)

    Count,
    FirstDispatch = CmdDispatch   ///< All callbacks with an enum value greater or equal than this are dispatches
};

/// Draw-specific information for DrawDispatch callbacks
struct DrawDispatchDrawArgs
{
    /// Contains information about user data register indices for certain draw parameter state.
    /// Some of these values may not be available for all draws on all clients, and in such
    /// cases the value will be UINT_MAX.
    struct
    {
        uint32 firstVertex;    ///< Vertex offset (first vertex) user data register index
        uint32 instanceOffset; ///< Instance offset (start instance) user data register index
        uint32 drawIndex;      ///< Draw ID SPI user data register index
    } userDataRegs;
};

/// Dispatch-specific information for DrawDispatch callbacks
struct DrawDispatchDispatchArgs
{
    uint32 groupStart[3]; ///< Thread/workgroup start offsets in X/Y/Z dimensions.  Only valid for CmdDispatchOffset.
    uint32 groupDims[3];  ///< Thread/workgroup counts in X/Y/Z dimensions.  Only valid for CmdDispatch[Offset].
};

/// Information for DrawDispatch callbacks
struct DrawDispatchData
{
    ICmdBuffer*      pCmdBuffer; ///< The command buffer that is recording this command
    DrawDispatchType cmdType;    ///< Draw/dispatch command type.  This influences which sub-structure below is valid.

    union
    {
        /// Draw-specific parameters.  Valid when cmdType is CmdDraw*.
        DrawDispatchDrawArgs draw;

        /// Dispatch-specific parameters.  Valid when cmdType is CmdDispatch*
        DrawDispatchDispatchArgs dispatch;
    };
};

/// Information for BindPipeline callbacks
struct BindPipelineData
{
    const IPipeline*  pPipeline;  ///< The currently-bound pipeline
    ICmdBuffer*       pCmdBuffer; ///< The command buffer that is recording this command
    uint64            apiPsoHash; ///< The hash to correlate APIs and corresponding PSOs.
    PipelineBindPoint bindPoint;  ///< The bind point of the pipeline within a queue.
};

#if PAL_BUILD_PM4_INSTRUMENTOR
/// Information for DrawDispatchValidation callbacks
struct DrawDispatchValidationData
{
    ICmdBuffer* pCmdBuffer;         ///< The command buffer which is recording the triggering draw or dispatch.
    uint32      pipelineCmdSize;    ///< Size of PM4 commands used to validate the current pipeline state (bytes).
    uint32      userDataCmdSize;    ///< Size of PM4 commands used to validate the current user-data entries (bytes).
    uint32      miscCmdSize;        ///< Size of PM4 commands for all other draw- or dispatch-time validation (bytes).
};

/// Information for OptimizedRegisters callbacks
struct OptimizedRegistersData
{
    ICmdBuffer*   pCmdBuffer;       ///< The command buffer which is recording the triggering PM4 stream.
    /// Array containing the number of times the PM4 optimizer saw a SET packet which modified each register
    const uint32* pShRegSeenSets;
    ///< Array containing the number of times the PM4 optimizer kept a SET packet which modified each register
    const uint32* pShRegKeptSets;
    uint32        shRegCount;       ///< Number of SH registers
    uint16        shRegBase;        ///< Base address of SH registers
    /// Array containing the number of times the PM4 optimizer saw a SET or RMW packet which modified each register
    const uint32* pCtxRegSeenSets;
    ///< Array containing the number of times the PM4 optimizer kept a SET or RMW packet which modified each register
    const uint32* pCtxRegKeptSets;
    uint32        ctxRegCount;      ///< Number of context registers
    uint16        ctxRegBase;       ///< Base address of context registers
};
#endif

} // Developer
} // Pal
