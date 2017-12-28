/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
namespace Gfx6
{

class Device;

// =====================================================================================================================
// Query pool for retrieving shader execution statitus, as well as the number of invocations of some other fixed
// function parts of the geomtry pipeline.
class PipelineStatsQueryPool : public QueryPool
{
public:
    PipelineStatsQueryPool(const Device& device, const QueryPoolCreateInfo& createInfo);

    virtual void Begin(
        GfxCmdBuffer*     pCmdBuffer,
        Pal::CmdStream*   pCmdStream,
        QueryType         queryType,
        uint32            slot,
        QueryControlFlags flags) const override;

    virtual void End(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        QueryType       queryType,
        uint32          slot) const override;

    virtual void WaitForSlots(Pal::CmdStream* pCmdStream, uint32 startQuery, uint32 queryCount) const override;

protected:
    virtual ~PipelineStatsQueryPool() {}

    virtual void OptimizedReset(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        uint32          startQuery,
        uint32          queryCount) const override;

    virtual void NormalReset(
        GfxCmdBuffer*   pCmdBuffer,
        Pal::CmdStream* pCmdStream,
        uint32          startQuery,
        uint32          queryCount) const override;

    virtual size_t GetResultSizeForOneSlot(QueryResultFlags flags) const override;
    virtual bool ComputeResults(
        QueryResultFlags flags,
        QueryType        queryType,
        uint32           queryCount,
        size_t           stride,
        const void*      pGpuData,
        void*            pData) override;

private:
    const Device& m_device;
    uint32        m_numEnabledStats;

    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineStatsQueryPool);
    PAL_DISALLOW_DEFAULT_CTOR(PipelineStatsQueryPool);
};

} // Gfx6
} // Pal
