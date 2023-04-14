/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/pm4CmdStream.h"
#include "core/hw/gfxip/gfxBlendOptimizer.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "palDeque.h"

namespace Pal
{

namespace Pm4
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
                uint32 colorBlendState        :  1; // Gfx6 & Gfx9
                uint32 depthStencilState      :  1; // Gfx6 & Gfx9
                uint32 msaaState              :  1; // Gfx6 & Gfx9
                uint32 quadSamplePatternState :  1; // Gfx6 & Gfx9
                uint32 viewports              :  1; // Gfx6 & Gfx9
                uint32 scissorRects           :  1; // Gfx6 & Gfx9
                uint32 inputAssemblyState     :  1; // Gfx6 & Gfx9
                uint32 triangleRasterState    :  1; // Gfx6 & Gfx9
                uint32 occlusionQueryActive   :  1; // Gfx6 & Gfx9
                uint32 lineStippleState       :  1; // Gfx6 & Gfx9
                uint32 colorTargetView        :  1; // Gfx9 only
                uint32 depthStencilView       :  1; // Gfx9 only
                uint32 vrsRateParams          :  1; // 10.3+ only
                uint32 vrsCenterState         :  1; // 10.3+ only
                uint32 vrsImage               :  1; // 10.3+ only
                uint32 reserved               : 17;
            };

            uint32 u32All;

        } validationBits;

        union
        {
            // These bits are not tested in ValidateDraw()
            struct
            {
                uint32 streamOutTargets          : 1;
                uint32 iaState                   : 1;
                uint32 blendConstState           : 1;
                uint32 depthBiasState            : 1;
                uint32 depthBoundsState          : 1;
                uint32 pointLineRasterState      : 1;
                uint32 stencilRefMaskState       : 1;
                uint32 globalScissorState        : 1;
                uint32 clipRectsState            : 1;
                uint32 pipelineStatsQuery        : 1;
                uint32 reservedNonValidationBits : 22;
            };

            uint32 u32All;
        } nonValidationBits;
    };

    uint64 u64All;
};

static_assert(sizeof(GraphicsStateFlags) == sizeof(uint64), "Bad bitfield size.");

union TargetExtent2d
{
    struct
    {
        uint32 width  : 16; ///< Width of region (max width is 16k).
        uint32 height : 16; ///< Height of region (max height is 16k).
    };
    uint32 value;
};

constexpr uint32 MaxScissorExtent = 16384;

// The Max rectangle number that is allowed for clip rects.
constexpr uint32 MaxClipRects = 4;

// The default value of clip rule which means no clip rectangles.
constexpr uint16 DefaultClipRectsRule = 0xFFFF;

// Represents the graphics state which is currently active within the command buffer.
struct GraphicsState
{
    PipelineState               pipelineState;

    DynamicGraphicsShaderInfos  dynamicGraphicsInfo; // Info used during pipeline bind.

    BindTargetParams            bindTargets;
    // Lower MaxColorTargets bits are used. Each indicate how this slot is bound.
    // 0 indicates that it's bound to NULL, 1 means it's bound to a color target.
    uint32                      boundColorTargetMask;
    TargetExtent2d              targetExtent;

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
    LineStippleStateParams      lineStippleState;       // (CmdSetLineStippleState)
    StencilRefMaskParams        stencilRefMaskState;    // (CmdSetStencilRefMasks)
    TriangleRasterStateParams   triangleRasterState;    // (CmdSetTriangleRasterState)
    ViewportParams              viewportState;          // (CmdSetViewports)
    ScissorRectParams           scissorRectState;       // (CmdSetScissorRects)
    GlobalScissorParams         globalScissorState;     // (CmdSetGlobalScissor)
    MsaaQuadSamplePattern       quadSamplePatternState; // (CmdSetQuadSamplePattern)

    VrsRateParams               vrsRateState;           // (CmdSetPerDrawVrsRate)
    VrsCenterState              vrsCenterState;         // (CmdSetVrsCenterState)
    const Image*                pVrsImage;              // (CmdBindSampleRateImage)

    uint32                      numSamplesPerPixel;     // (CmdSetQuadSamplePattern)

    uint32                      viewInstanceMask;       // (CmdSetViewInstanceMask)

    struct
    {
        uint32  enableMultiViewport    : 1;  // Is the current pipeline using viewport-array-index?
        uint32  depthClampMode         : 2;  // The current pipeline's depth clamp mode. The is of type DepthClampMode
        uint32  useCustomSamplePattern : 1;  // If use custom sample pattern instead of default sample pattern
    };

    InheritedStateParams inheritedState; // States provided to nested command buffer from primary command buffer.

    struct
    {
        uint16 clipRule;
        uint32 rectCount;
        Rect   rectList[MaxClipRects];
    } clipRectsState; // (CmdSetClipRects)

    GraphicsStateFlags   dirtyFlags;
    GraphicsStateFlags   leakFlags;      // Graphics state which a nested command buffer "leaks" back to its caller.
};

struct ValidateDrawInfo
{
    uint32 vtxIdxCount;       // Vertex or index count for the draw (depending on if the it is indexed).
    uint32 instanceCount;     // Instance count for the draw. A count of zero indicates draw-indirect.
    uint32 firstVertex;       // First vertex
    uint32 firstInstance;     // First instance
    uint32 firstIndex;        // First index
    uint32 drawIndex;         // draw index
    bool   useOpaque;         // If draw opaque
    bool   multiIndirectDraw; // Is multi indirect draw?
};

// =====================================================================================================================
// Class for executing basic hardware-specific functionality common to all PM4 universal command buffers.
class UniversalCmdBuffer : public Pm4CmdBuffer
{
public:
    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnDataChunks) override;

    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    virtual void CmdDuplicateUserData(
        PipelineBindPoint source,
        PipelineBindPoint dest) override;

    virtual void CmdBindIndexData(
        gpusize   gpuAddr,
        uint32    indexCount,
        IndexType indexType) override;

    virtual void CmdSetViewInstanceMask(uint32 mask) override;

    virtual void CmdSetLineStippleState(
        const LineStippleStateParams& params) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    virtual void CmdSetColorWriteMask(const ColorWriteMaskParams& params) override;

    virtual void CmdSetRasterizerDiscardEnable(bool rasterizerDiscardEnable) override;
#endif

    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;
    virtual void EndCmdBufferDump(const Pal::CmdStream** ppCmdStreams, uint32 cmdStreamsNum) override;

    // Universal command buffers have three command streams: Draw Engine, Constant Engine and a hidden ACE cmd stream.
    static constexpr uint32 NumCmdStreamsVal = 3;
    static constexpr uint32 AceStreamCount   = 1;

    // Returns the number of command streams associated with this command buffer.
    virtual uint32 NumCmdStreams() const override { return NumCmdStreamsVal; }

    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const override;

    virtual uint32 NumCmdStreamsInSubQueue(int32 subQueueIndex) const override;
    virtual const CmdStream* GetCmdStreamInSubQueue(int32 subQueueIndex, uint32 cmdStreamIndex) const override;

    // Universal command buffers support every type of query
    virtual bool IsQueryAllowed(QueryPoolType queryPoolType) const override { return true; }

    virtual void CmdSaveGraphicsState() override;
    virtual void CmdRestoreGraphicsState() override;

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() override
    {
        m_pDeCmdStream->IncrementSubmitCount();

        if (m_pCeCmdStream != nullptr)
        {
            m_pCeCmdStream->IncrementSubmitCount();
        }

        if (m_pAceCmdStream != nullptr)
        {
            m_pAceCmdStream->IncrementSubmitCount();
        }
    }

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

    // Current graphics state
    const GraphicsState& GetGraphicsState() const { return m_graphicsState; }

    static void SetStencilRefMasksState(
        const StencilRefMaskParams& updatedRefMaskState,
        StencilRefMaskParams*       pStencilRefMaskState);

    // Used to initialize boundColorTargetMask. Null color target is bound only when the slot was not NULL and being
    // bound to NULL. Set all 1s so NULL color targets will be bound when BuildNullColorTargets() is called first time.
    static constexpr uint32 NoNullColorTargetMask = ((1 << MaxColorTargets) - 1);

    virtual void CmdBindPipelineWithOverrides(
        const PipelineBindParams& params,
        SwizzledFormat            swizzledFormat,
        uint32                    targetIndex) override;

protected:
    UniversalCmdBuffer(
        const GfxDevice&           device,
        const CmdBufferCreateInfo& createInfo,
        Pm4::CmdStream*            pDeCmdStream,
        Pm4::CmdStream*            pCeCmdStream,
        Pm4::CmdStream*            pAceCmdStream,
        bool                       blendOptEnable);

    virtual ~UniversalCmdBuffer() {}

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

    bool IsAnyGfxUserDataDirty() const;

    virtual void SetGraphicsState(const GraphicsState& newGraphicsState);

    GraphicsState  m_graphicsState;        // Currently bound graphics command buffer state.
    GraphicsState  m_graphicsRestoreState; // State pushed by the previous call to CmdSaveGraphicsState.

    GfxBlendOptimizer::BlendOpts  m_blendOpts[MaxColorTargets]; // Current blend optimization state

    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override
        { CmdBuffer::P2pBltWaCopyNextRegion(m_pDeCmdStream, chunkAddr); }
    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override
        { return pCmdSpace + m_pDeCmdStream->BuildNop(numDwords, pCmdSpace); }

    virtual void CmdSetPerDrawVrsRate(const VrsRateParams&  rateParams) override;

    virtual void CmdSetVrsCenterState(const VrsCenterState&  centerState) override;

    virtual void CmdBindSampleRateImage(const IImage*  pImage) override;

    // Late-initialized ACE command buffer stream.
    // Ace command stream is used for ganged submit of compute workloads (task shader workloads)
    // after which graphics workloads will be submitted on the DE command stream.
    Pm4::CmdStream* m_pAceCmdStream;

    TessDistributionFactors m_tessDistributionFactors;

    uint8 m_contextStatesPerBin;
    uint8 m_persistentStatesPerBin;

private:
    const GfxDevice&     m_device;
    Pm4::CmdStream*const m_pDeCmdStream; // Draw engine command buffer stream.
    Pm4::CmdStream*const m_pCeCmdStream; // Constant engine command buffer stream.
    const bool           m_blendOptEnable;

    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(UniversalCmdBuffer);
};

} // Pm4
} // Pal
