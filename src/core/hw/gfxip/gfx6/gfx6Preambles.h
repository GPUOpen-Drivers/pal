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

#include "core/hw/gfxip/gfx6/gfx6ShadowedRegisters.h"

namespace Pal
{
namespace Gfx6
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
    PM4CMDSETDATA                             hdrThreadMgmt01;
    regCOMPUTE_STATIC_THREAD_MGMT_SE0         computeStaticThreadMgmtSe0;
    regCOMPUTE_STATIC_THREAD_MGMT_SE1         computeStaticThreadMgmtSe1;

    PM4CMDSETDATA                             hdrThreadMgmt23;
    regCOMPUTE_STATIC_THREAD_MGMT_SE2__CI__VI computeStaticThreadMgmtSe2;
    regCOMPUTE_STATIC_THREAD_MGMT_SE3__CI__VI computeStaticThreadMgmtSe3;

    size_t                                    spaceNeeded;
};

// Describes the GDS User Data register value.
struct GdsData
{
    uint32 gdsSize   : 16; // Size of GDS in bytes
    uint32 gdsOffset : 16; // Offset into GDS for this partition, in bytes
};

#if !PAL_COMPUTE_GDS_OPT
// Represents an "image" of the PM4 headers necessary to write GDS partition offset and size in graphics stage
// USER_DATA registers as required by SC.
struct GdsRangeGraphics
{
    PM4CMDSETDATA headerLs;
    GdsData       gdsDataLs;
    PM4CMDSETDATA headerHs;
    GdsData       gdsDataHs;
    PM4CMDSETDATA headerEs;
    GdsData       gdsDataEs;
    PM4CMDSETDATA headerGs;
    GdsData       gdsDataGs;
    PM4CMDSETDATA headerVs;
    GdsData       gdsDataVs;
    PM4CMDSETDATA headerPs;
    GdsData       gdsDataPs;
};
#endif

// Represents an "image" of the PM4 headers necessary to write GDS partition offset and size in the compute stage
// USER_DATA register as required by SC.
struct GdsRangeCompute
{
    PM4CMDSETDATA header;
    GdsData       gdsData;
};

// Contains a subset of commands necessary to the compute preamble command stream.
struct ComputePreamblePm4Img
{
    GdsRangeCompute       gdsRange;

    size_t                spaceNeeded;
};

// Contains the commands necessary to set-up state shadowing of GPU registers. Unless mid command buffer preemption
// is enabled, only the context control and clear state packets will be populated.
struct StateShadowPreamblePm4Img
{
    PM4CMDCONTEXTCONTROL  contextControl;
    PM4CMDCLEARSTATE      clearState;

    PM4CMDLOADDATA        loadUserCfgRegs;
    RegisterRange         userCfgRegs[NumUserConfigShadowRangesGfx7 - 1];

    PM4CMDLOADDATA        loadShRegsGfx;
    RegisterRange         gfxShRegs[NumGfxShShadowRanges - 1];

    PM4CMDLOADDATA        loadShRegsCs;
    RegisterRange         csShRegs[NumCsShShadowRanges - 1];

    PM4CMDLOADDATA        loadContextRegs;
    RegisterRange         contextRegs[MaxNumContextShadowRanges - 1];

    size_t                spaceNeeded;
};

// Contains a subset of commands necessary to the universal preamble command stream.
struct UniversalPreamblePm4Img
{
#if !PAL_COMPUTE_GDS_OPT
    GdsRangeGraphics   gdsRangeGraphics;
#endif
    GdsRangeCompute    gdsRangeCompute;

    size_t             spaceNeeded;
};

// Contains a subset of commands necessary to write a Universal Command Buffer preamble to hardware on GFX6-7 hardware.
struct Gfx6UniversalPreamblePm4Img
{
    PM4CMDSETDATA                   hdrSpiThreadMgmt;
    regSPI_STATIC_THREAD_MGMT_3__SI spiStaticThreadMgmt3;

    size_t                          spaceNeeded;
};

// Contains a subset of commands necessary to write a Universal Command Buffer preamble to hardware on GFX8 hardware.
struct Gfx8UniversalPreamblePm4Img
{
    PM4CMDSETDATA                  hdrVgtOutDeallocCntl;
    regVGT_OUT_DEALLOC_CNTL        vgtOutDeallocCntl;

    PM4CMDSETDATA                  hdrDistribution;
    regVGT_TESS_DISTRIBUTION__VI   vgtTessDistribution;

    PM4CMDSETDATA                  hdrDccControl;
    regCB_DCC_CONTROL__VI          cbDccControl;

    PM4CMDSETDATA                       hdrSmallPrimFilterCntl;
    regPA_SU_SMALL_PRIM_FILTER_CNTL__VI paSuSmallPrimFilterCntl;

    size_t                         spaceNeeded;
};

} // Gfx6
} // Pal
