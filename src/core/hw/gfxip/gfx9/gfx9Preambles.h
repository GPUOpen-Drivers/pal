/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9ShadowedRegisters.h"

namespace Pal
{
namespace Gfx9
{

// The structures in this file represent "images" of PM4 needed either for command buffer preambles or for the queue
// context preambles. Register writes are grouped into sets based on sequential register addresses, so that the amount
// of PM4 space needed by setting several registers in each packet is minimized.
//
// In all structures, spaceNeeded is the command space needed in DWORDs. It must always be last in the structure to not
// interfere w/ the actual commands contained within.
//
// Command buffer preambles are executed at the beginning of each command buffer and cannot be skipped. They contain
// necessary state that must be reset between consecutive command buffers.
//
// Queue context preambles are executed once before a chain of command buffers are run. The queue context preamble can
// be skipped if the client (UMD) hasn't changed between submits. The queue context preamble contains necessary state
// that must be set to known values before all subsequent command buffers can be executed, but it only needs to be set
// once (in case another client [UMD] has modified it).

// Contains a subset of commands common to both Compute and Universal preamble command streams.
struct CommonPreamblePm4Img
{
    // This is the common preamble, meaning it gets executed on both compute and universal queues.  PFP is a universal
    // queue only construct in the HW, but the ability to write UCONFIG regs exists on the compute side as well.
    // Using "PFP" here to match what the "cmdUtil" class expects, but ME / MEC / PFP versions of this packet are all
    // the same.
    PM4_PFP_SET_UCONFIG_REG            hdrCoherDelay;
    regCP_COHER_START_DELAY            cpCoherStartDelay;

    // The common preamble can get executed on engines that don't support compute, so this must be last.
    PM4ME_SET_SH_REG                   hdrThreadMgmt01;
    regCOMPUTE_STATIC_THREAD_MGMT_SE0  computeStaticThreadMgmtSe0;
    regCOMPUTE_STATIC_THREAD_MGMT_SE1  computeStaticThreadMgmtSe1;

    PM4ME_SET_SH_REG                   hdrThreadMgmt23;
    regCOMPUTE_STATIC_THREAD_MGMT_SE2  computeStaticThreadMgmtSe2;
    regCOMPUTE_STATIC_THREAD_MGMT_SE3  computeStaticThreadMgmtSe3;

    size_t                             spaceNeeded;
};

// Describes the GDS User Data register value.
struct GdsData
{
    uint32 gdsSize   : 16; // Size of GDS in bytes
    uint32 gdsOffset : 16; // Offset into GDS for this partition, in bytes
};

// Represents an "image" of the PM4 headers necessary to write GDS partition offset and size in the compute stage
// USER_DATA register as required by SC.
struct GdsRangeCompute
{
    PM4MEC_SET_SH_REG  header;
    GdsData            gdsData;
};

// Contains a subset of commands necessary to the compute preamble command stream.
struct ComputePreamblePm4Img
{
    GdsRangeCompute  gdsRange;
    size_t           spaceNeeded;
};

// Gfx9-specific registers associated with the preamble
struct Gfx9UniversalPreamblePm4Img
{
    // We need to write VGT_MAX_VTX_INDX, VGT_MIN_VTX_INDX, and VGT_INDX_OFFSET. In Gfx6-8.1, these were "sticky"
    // context registers, but they have now been moved into UConfig space for GFX9. However, they are written by
    // UDX on a per-draw basis.
    PM4_PFP_SET_UCONFIG_REG  hdrVgtIndexRegs;
    regVGT_MAX_VTX_INDX      vgtMaxVtxIndx;
    regVGT_MIN_VTX_INDX      vgtMinVtxIndx;
    regVGT_INDX_OFFSET       vgtIndxOffset;
};

// Contains a subset of commands necessary to the universal preamble command stream.
struct UniversalPreamblePm4Img
{
    PM4_ME_EVENT_WRITE              pixelPipeStatControl;

    // TODO: Add support for Late Alloc VS Limit

    // TODO: The following are set on Gfx8 because the clear state doesn't set up these registers to our liking.
    //       We might be able to remove these when the clear state for Gfx9 is finalized.
    PM4_PFP_SET_CONTEXT_REG         hdrVgtOutDeallocCntl;
    regVGT_OUT_DEALLOC_CNTL         vgtOutDeallocCntl;

    PM4_PFP_SET_CONTEXT_REG         hdrVgtTessDistribution;
    regVGT_TESS_DISTRIBUTION        vgtTessDistribution;

    PM4_PFP_SET_CONTEXT_REG         hdrDccControl;
    regCB_DCC_CONTROL               cbDccControl;

    PM4_PFP_SET_CONTEXT_REG         hdrSmallPrimFilterCntl;
    regPA_SU_SMALL_PRIM_FILTER_CNTL paSuSmallPrimFilterCntl;

    PM4PFP_SET_CONTEXT_REG          hdrCoherDestBaseHi;
    regCOHER_DEST_BASE_HI_0         coherDestBaseHi;

    PM4PFP_SET_CONTEXT_REG          hdrPaScGenericScissors;
    regPA_SC_GENERIC_SCISSOR_TL     paScGenericScissorTl;
    regPA_SC_GENERIC_SCISSOR_BR     paScGenericScissorBr;

    GdsRangeCompute                 gdsRangeCompute;

    // GPU specific registers go in this union.  As the union has a variable valid size depending on the GPU
    // in use, this union must be the last PM4 data in this structure.
    union
    {
        Gfx9UniversalPreamblePm4Img     gfx9;
    };

    size_t                              spaceNeeded;
};

} // Gfx9
} // Pal
