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

#pragma once

#include "core/hw/gfxip/pipeline.h"

namespace Pal
{

// =====================================================================================================================
// Hardware independent graphics pipeline class.  Implements all details of a graphics pipeline that are common across
// all hardware types (and combination of shader stages) but distinct from a compute pipeline.
class GraphicsPipeline : public Pipeline
{
public:
    virtual ~GraphicsPipeline() { }

    virtual Result Init(
        const GraphicsPipelineCreateInfo&         createInfo,
        const GraphicsPipelineInternalCreateInfo& internalInfo);

    bool IsGsEnabled() const { return m_flags.gsEnabled; }
    bool IsGsOnChip() const { return m_flags.isGsOnchip; }
    bool IsTessEnabled() const { return m_flags.tessEnabled; }
    bool UsesStreamOut() const { return m_flags.streamOut; }
    bool PsUsesUavs() const { return m_flags.psUsesUavs; }
    bool PsWritesUavs() const { return m_flags.psWritesUavs; }
    bool PsUsesRovs() const { return m_flags.psUsesRovs; }
    bool PsUsesAppendConsume() const { return m_flags.psUsesAppendConsume; }
    bool UsesViewportArrayIndex() const { return m_flags.vportArrayIdx; }
    bool IsPerpEndCapsEnabled() const { return m_flags.perpLineEndCapsEnable; }

    BinningOverride GetBinningOverride() const { return m_binningOverride; }

    uint32 VertsPerPrimitive() const { return m_vertsPerPrim; }

    const ViewInstancingDescriptor& GetViewInstancingDesc() const { return m_viewInstancingDesc; };

    // Accessors for arrays of per-target info stored from pipeline creation.
    const SwizzledFormat* TargetFormats() const { return &m_targetSwizzledFormats[0]; }
    const uint8* TargetWriteMasks() const { return &m_targetWriteMasks[0]; }

protected:
    GraphicsPipeline(Device* pDevice, bool isInternal);

    virtual Result HwlInit(
        const GraphicsPipelineCreateInfo& createInfo,
        const AbiProcessor&               abiProcessor,
        const CodeObjectMetadata&         metadata,
        Util::MsgPackReader*              pMetadataReader) = 0;

    bool UsesAdjacency()        const { return m_flags.adjacencyPrim; }
    bool IsDccDecompress()      const { return m_flags.dccDecompress; }
    bool IsResolveFixedFunc()   const { return m_flags.resolveFixedFunc; }
    bool IsFastClearEliminate() const { return m_flags.fastClearElim; }
    bool IsFmaskDecompress()    const { return m_flags.fmaskDecompress; }
    bool IsLateAllocVsLimit()   const { return m_flags.lateAllocVsLimit; }
    bool WritesDepth()          const { return m_flags.psWritesDepth; }

    void SetIsGsOnChip(bool onChip) { m_flags.isGsOnchip = onChip; }

    uint32 GetLateAllocVsLimit() const { return m_lateAllocVsLimit; }

private:
    Result InitFromPipelineBinary(
        const GraphicsPipelineCreateInfo&         createInfo,
        const GraphicsPipelineInternalCreateInfo& internalInfo);

    union
    {
        struct
        {
            uint32 gsEnabled             :  1; // Geometry shader is active.
            uint32 tessEnabled           :  1; // Tessellation shaders (HS/DS) are active.
            uint32 streamOut             :  1; // Stream-output is active.
            uint32 adjacencyPrim         :  1; // Primitive topology contains adjacency info.
            uint32 vportArrayIdx         :  1; // GS outputs a viewport array index parameter.
            uint32 psUsesUavs            :  1; // PS reads/writes at least one UAV.
            uint32 psUsesRovs            :  1; // PS reads/writes at least one ROV.
            uint32 fastClearElim         :  1; // Internal pipeline for RPM fast-clear eliminate BLTs.
            uint32 fmaskDecompress       :  1; // Internal pipeline for RPM fmask decompression BLTs.
            uint32 dccDecompress         :  1; // Internal pipeline for RPM DCC decompression BLTs.
            uint32 resolveFixedFunc      :  1; // Internal pipeline for fixed function resolve.
            uint32 isGsOnchip            :  1; // Whether or not the Geometry shader (GS) is on-chip.
            uint32 lateAllocVsLimit      :  1; // Whether to use the client specified lateAllocVsLimit.
            uint32 psWritesDepth         :  1; // This pipeline explicitly outputs depth data.
            uint32 psUsesAppendConsume   :  1; // PS uses atomic append/consume instructions.
            uint32 perpLineEndCapsEnable :  1; // use perpendicular line end caps instead of axis-aligned end caps
            uint32 placeholder0          :  1;
            uint32 psWritesUavs          :  1; // PS writes at least one UAV.
            uint32 reserved              : 14;
        };
        uint32 u32All;
    } m_flags;

    BinningOverride          m_binningOverride; // Override global batched binning. Gfx9 only.

    uint32                   m_vertsPerPrim; // Number of vertices per primitive (based on topology).

    // Store any info from the pipeline creation info that might be needed later, such as for draw-time blend
    // optimization programming.
    SwizzledFormat           m_targetSwizzledFormats[MaxColorTargets];
    uint8                    m_targetWriteMasks[MaxColorTargets];

    // Use this late_alloc_vs limit if lateAllocVsLimit flag is set.
    uint32                   m_lateAllocVsLimit;

    ViewInstancingDescriptor m_viewInstancingDesc;  // View instancing descriptor.

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsPipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsPipeline);
};

} // Pal
