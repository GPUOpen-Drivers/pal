/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfxBlendOptimizer.h"
#include "palDeque.h"

namespace Pal
{

class      PerfExperiment;
enum class ShaderType : uint32;

// Set of flags indicating which graphics states have been modified in a command buffer.
union GraphicsStateFlags
{
    struct
    {
        union
        {
            // These bits are tested in ValidateDraw() (in Gfx6 or Gfx9)
            struct
            {
                uint16 colorBlendState        : 1; // Gfx6 & Gfx9
                uint16 depthStencilState      : 1; // Gfx6 & Gfx9
                uint16 msaaState              : 1; // Gfx6 & Gfx9
                uint16 quadSamplePatternState : 1; // Gfx6 & Gfx9
                uint16 viewports              : 1; // Gfx6 & Gfx9
                uint16 scissorRects           : 1; // Gfx6 & Gfx9
                uint16 inputAssemblyState     : 1; // Gfx6 & Gfx9
                uint16 triangleRasterState    : 1; // Gfx6 & Gfx9
                uint16 queryState             : 1; // Gfx6 & Gfx9
                uint16 colorTargetView        : 1; // Gfx9 only
                uint16 depthStencilView       : 1; // Gfx9 only
                uint16 reservedValidationBits : 5;
            };

            uint16 u16All;

        } validationBits;

        union
        {
            // These bits are not tested in ValidateDraw()
            struct
            {
                uint16 streamOutTargets          : 1;
                uint16 iaState                   : 1;
                uint16 blendConstState           : 1;
                uint16 depthBiasState            : 1;
                uint16 depthBoundsState          : 1;
                uint16 pointLineRasterState      : 1;
                uint16 stencilRefMaskState       : 1;
                uint16 globalScissorState        : 1;
                uint16 reservedNonValidationBits : 8;
            };

            uint16 u16All;
        } nonValidationBits;
    };

    uint32 u32All;
};

static_assert(sizeof(GraphicsStateFlags) == sizeof(uint32), "Bad bitfield size.");

// Represents the graphics state which is currently active within the command buffer.
struct GraphicsState
{
    PipelineState               pipelineState;

    DynamicGraphicsShaderInfos  dynamicGraphicsInfo; // Info used during pipeline bind.

    BindTargetParams            bindTargets;
    // Lower MaxColorTargets bits are used. Each indicate how this slot is bound.
    // 0 indicates that it's bound to NULL, 1 means it's bound to a color target.
    uint32                      boundColorTargetMask;

    BindStreamOutTargetParams   bindStreamOutTargets;

    const IColorBlendState*     pColorBlendState;
    const IDepthStencilState*   pDepthStencilState;
    const IMsaaState*           pMsaaState;

    UserDataEntries             gfxUserDataEntries;

    struct
    {
        gpusize                 indexAddr;            // GPU virtual address of the index buffer data
        uint32                  indexCount;           // Number of indices in the index buffer
        IndexType               indexType;            // Data type of the indices
    } iaState; // Input Assembly State

    InputAssemblyStateParams    inputAssemblyState;     // Current input assembler state
    BlendConstParams            blendConstState;        // (CmdSetBlendConst)
    DepthBiasParams             depthBiasState;         // (CmdSetDepthBiasState)
    DepthBoundsParams           depthBoundsState;       // (CmdSetDepthBounds)
    PointLineRasterStateParams  pointLineRasterState;   // (CmdSetPointLineRasterState)
    StencilRefMaskParams        stencilRefMaskState;    // (CmdSetStencilRefMasks)
    TriangleRasterStateParams   triangleRasterState;    // (CmdSetTriangleRasterState)
    ViewportParams              viewportState;          // (CmdSetViewports)
    ScissorRectParams           scissorRectState;       // (CmdSetScissorRects)
    GlobalScissorParams         globalScissorState;     // (CmdSetGlobalScissor)
    MsaaQuadSamplePattern       quadSamplePatternState; // (CmdSetQuadSamplePattern)

    uint32                      numSamplesPerPixel;     // (CmdSetQuadSamplePattern)

    uint32                      viewInstanceMask;       // (CmdSetViewInstanceMask)

    struct
    {
        uint32  enableMultiViewport   : 1;  // Is the current pipeline using viewport-array-index?
        uint32  everUsedMultiViewport : 1;  // Did this command buffer ever draw with a pipeline which used
                                            // viewport-array-index?
    };

    InheritedStateParams inheritedState; // States provided to nested command buffer from primary command buffer.

    GraphicsStateFlags   dirtyFlags;
    GraphicsStateFlags   leakFlags;      // Graphics state which a nested command buffer "leaks" back to its caller.
};

struct ValidateDrawInfo
{
    uint32 vtxIdxCount;   // Vertex or index count for the draw (depending on if the it is indexed).
    uint32 instanceCount; // Instance count for the draw. A count of zero indicates draw-indirect.
    uint32 firstVertex;   // First vertex
    uint32 firstInstance; // First instance
    uint32 firstIndex;    // First index
};

// Tracks the state of a user-data table stored in GPU memory.  The tables contents are managed using CE RAM.
struct CeRamUserDataTableState
{
    gpusize  gpuVirtAddr;   // GPU virtual address where the current copy of the table data is stored.
    // Offset into CE RAM (in bytes!) where the staging area is located. This can be zero if the table is being
    // managed using CPU updates instead of the constant engine.
    uint32   ceRamOffset;
    struct
    {
        uint32  sizeInDwords : 31; // Size of one full instance of the user-data table, in DWORD's.
        uint32  dirty        :  1; // Indicates that the CPU copy of the user-data table is more up to date than the
                                   // copy currently in GPU memory and should be updated before the next dispatch.
    };
};

// Tracks the state of a ring buffer used for receiving CE RAM dumps.
struct CeRamUserDataRingBuffer
{
    gpusize  baseGpuVirtAddr;   // Base GPU virtual address of the ring buffer memory
    uint32   instanceBytes;     // Size of each table instance contained in the ring buffer, in bytes
    uint32   numInstances;      // Number of table instances in the entire ring
    uint32   currRingPos;       // Currently active instance within the ring buffer
};

// =====================================================================================================================
// Class for executing basic hardware-specific functionality common to all universal command buffers.
class UniversalCmdBuffer : public GfxCmdBuffer
{
public:
    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnDataChunks) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdBindIndexData(
        gpusize   gpuAddr,
        uint32    indexCount,
        IndexType indexType) override;

    virtual void CmdSetViewInstanceMask(uint32 mask) override;

    virtual void CmdSetIndirectUserDataWatermark(
        uint16 tableId,
        uint32 dwordLimit) override;

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;
#endif

    // Universal command buffers have two command streams: Draw Engine and Constant Engine.
    static constexpr uint32 NumCmdStreamsVal = 2;

    // Returns the number of command streams associated with this command buffer.
    virtual uint32 NumCmdStreams() const override { return NumCmdStreamsVal; }
    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const override;

    // Universal command buffers support every type of query
    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const override { return true; }

    virtual void PushGraphicsState() override;
    virtual void PopGraphicsState() override;

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() override
    {
        m_pDeCmdStream->IncrementSubmitCount();
        m_pCeCmdStream->IncrementSubmitCount();
    }

    // Current graphics state
    const GraphicsState& GetGraphicsState() const { return m_graphicsState; }

    static void SetStencilRefMasksState(
        const StencilRefMaskParams& updatedRefMaskState,
        StencilRefMaskParams*       pStencilRefMaskState);

    bool UseRingBufferForCeRamDumps() const
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 395
        { return (m_buildFlags.useLinearBufferForCeRamDumps == 0); }
#else
        { return (m_buildFlags.useEmbeddedDataForCeRamDumps == 0); }
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    // Returns true if the graphics state is currently pushed.
    bool IsGraphicsStatePushed() const { return m_graphicsStateIsPushed; }
#endif

    // Used to initialize boundColorTargetMask. Null color target is bound only when the slot was not NULL and being
    // bound to NULL. Set all 1s so NULL color targets will be bound when BuildNullColorTargets() is called first time.
    static constexpr uint32 NoNullColorTargetMask = ((1 << MaxColorTargets) - 1);

protected:
    UniversalCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo,
        PrefetchMgr*               pPrefetchMgr,
        GfxCmdStream*              pDeCmdStream,
        GfxCmdStream*              pCeCmdStream,
        bool                       blendOptEnable);

    virtual ~UniversalCmdBuffer() {}

    void InitReservedCeRamPartitions(
        uint32*  pIndirectUserDataTableMem,
        uint32   reservedCeRamBytes,
        gpusize* pGpuVirtAddr,
        uint32*  pCeRamOffset);

    virtual Pal::PipelineState* PipelineState(PipelineBindPoint bindPoint) override;

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual void ResetState() override;

    void LeakNestedCmdBufferState(
        const UniversalCmdBuffer& cmdBuffer);

    template <bool filterRedundantUserData>
    static void PAL_STDCALL CmdSetUserDataGfx(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);

    bool FilterSetUserDataGfx(UserDataArgs* pUserDataArgs);

    virtual void SetGraphicsState(const GraphicsState& newGraphicsState);

    GraphicsState  m_graphicsState;        // Currently bound graphics command buffer state.
    GraphicsState  m_graphicsRestoreState; // State pushed by the previous call to PushGraphicsState.

    GfxBlendOptimizer::BlendOpts  m_blendOpts[MaxColorTargets]; // Current blend optimization state

    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override
        { CmdBuffer::P2pBltWaCopyNextRegion(m_pDeCmdStream, chunkAddr); }
    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override
        { return pCmdSpace + m_pDeCmdStream->BuildNop(numDwords, pCmdSpace); }

    struct
    {
        // Client-specified high-watermark for each indirect user-data table. This indicates how much of each table
        // is dumped from CE RAM to memory before a draw or dispatch.
        uint32   watermark : 31;
        // Tracks whether or not this indirect user-data table was modified somewhere in the command buffer.
        uint32   modified  :  1;
        uint32*  pData;  // Tracks the contents of each indirect user-data table.

        CeRamUserDataTableState  state;  // Tracks the state for the indirect user-data table
        CeRamUserDataRingBuffer  ring;   // Tracks the state for the indirect user-data table's GPU memory ring buffer

    }  m_indirectUserDataInfo[MaxIndirectUserDataTables];

    struct
    {
        CeRamUserDataTableState  stateCs;  // Tracks the state of the compute spill table
        CeRamUserDataTableState  stateGfx; // Tracks the state of the graphics spill table
        CeRamUserDataRingBuffer  ring;     // Tracks the state of the spill tables' shared GPU memory ring buffer
    }  m_spillTable;

private:
    const GfxDevice&   m_device;
    GfxCmdStream*const m_pDeCmdStream; // Draw engine command buffer stream.
    GfxCmdStream*const m_pCeCmdStream; // Constant engine command buffer stream.
    const bool         m_blendOptEnable;

#if PAL_ENABLE_PRINTS_ASSERTS
    bool  m_graphicsStateIsPushed; // If PushGraphicsState was called without a matching PopGraphicsState.
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
};

// =====================================================================================================================
// Helper function for resetting a user-data table which is managed using CE RAM at the beginning of a command buffer.
void PAL_INLINE ResetUserDataTable(
    CeRamUserDataTableState* pTable)
{
    pTable->gpuVirtAddr = 0;
    pTable->dirty       = 0;
}

// =====================================================================================================================
// Helper function for resetting a user-data ring buffer at the beginning of a command buffer.
void PAL_INLINE ResetUserDataRingBuffer(
    CeRamUserDataRingBuffer* pRing)
{
    pRing->currRingPos = 0;
}

} // Pal
