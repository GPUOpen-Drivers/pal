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

#include "core/hw/gfxip/queryPool.h"
#include "palCmdBuffer.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// =====================================================================================================================
// Query pool for retrieving the number of primitives written and primitives needed from the geometry pipeline.
class StreamoutStatsQueryPool final : public QueryPool
{
public:
    StreamoutStatsQueryPool(const Device& device, const QueryPoolCreateInfo& createInfo);

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

    // Stalls the ME until the results of the query range are in memory.
    virtual void WaitForSlots(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        uint32          startQuery,
        uint32          queryCount) const override;

    virtual Result Reset(
        uint32 startQuery,
        uint32 queryCount,
        void*  pMappedCpuAddr) override;

protected:
    virtual ~StreamoutStatsQueryPool() {}

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
    bool IsQueryDataValid(volatile const uint64* pData) const;

    const Device& m_device;

    PAL_DISALLOW_COPY_AND_ASSIGN(StreamoutStatsQueryPool);
    PAL_DISALLOW_DEFAULT_CTOR(StreamoutStatsQueryPool);
};

} // Gfx12
} // Pal
