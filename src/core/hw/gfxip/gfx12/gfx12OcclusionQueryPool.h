/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "core/hw/gfxip/queryPool.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// Occlusion query data has to be 16 bytes aligned for CP access
static constexpr gpusize OcclusionQueryMemoryAlignment = 16;

static constexpr uint32 ResetOcclusionQueryPoolSrcSlots = 256;

// Defines the structure of the 64-bit data reported by each RB for z-pass data
union OcclusionQueryResult
{
    uint64 data;

    struct
    {
        uint64 zPassData : 63;
        uint64 valid     : 1;
    } bits;
};

static_assert(sizeof(OcclusionQueryResult) == sizeof(uint64), "OcclusionQueryResult is the wrong size.");

// Defines the structure of a begin / end pair of data.
struct OcclusionQueryResultPair
{
    OcclusionQueryResult begin;
    OcclusionQueryResult end;
};

// =====================================================================================================================
// Query Pool for counting the number of samples that pass the depth and stencil tests.
class OcclusionQueryPool final : public Pal::QueryPool
{
public:
    OcclusionQueryPool(const Device& device, const QueryPoolCreateInfo& createInfo);

    virtual Result Reset(
        uint32 startQuery,
        uint32 queryCount,
        void*  pMappedCpuAddr) override;

    virtual void Begin(
        GfxCmdBuffer*     pCmdBuffer,
        Pal::CmdStream*   pCmdStream,
        Pal::CmdStream*   pHybridCmdStream,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) const override;

    virtual void End(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        Pal::CmdStream* pHybridCmdStream,
        QueryType       queryType,
        uint32          slot) const override;

    // Stalls the ME until the results of the query range are in memory. This function should never be called for GFX12
    // occlusion queries, as waiting is implemented in the shader.
    virtual void WaitForSlots(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        uint32          startQuery,
        uint32          queryCount) const override
        { PAL_NEVER_CALLED(); }

protected:
    virtual ~OcclusionQueryPool() {}

    virtual void GpuReset(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        uint32          startQuery,
        uint32          queryCount) const override;

    virtual size_t GetResultSizeForOneSlot(Pal::QueryResultFlags flags) const override;
    virtual bool ComputeResults(
        Pal::QueryResultFlags flags,
        QueryType             queryType,
        uint32                queryCount,
        size_t                stride,
        const void*           pGpuData,
        void*                 pData) override;

private:
    const Device& m_device;

    PAL_DISALLOW_COPY_AND_ASSIGN(OcclusionQueryPool);
    PAL_DISALLOW_DEFAULT_CTOR(OcclusionQueryPool);
};

} // Gfx12
} // Pal
