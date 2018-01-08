/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUploadRing.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
Result CmdUploadRing::CreateInternal(
    const CmdUploadRingCreateInfo& createInfo,
    Device*                        pDevice,
    Pal::CmdUploadRing**           ppCmdUploadRing)
{
    Result       result  = Result::ErrorOutOfMemory;
    const size_t size    = sizeof(CmdUploadRing) + Pal::CmdUploadRing::GetPlacementSize(*pDevice->Parent());
    void*const   pMemory = PAL_MALLOC(size, pDevice->GetPlatform(), AllocInternal);

    if (pMemory != nullptr)
    {
        // The postamble must contain enough space for a chain packet and also the smallest NOP packet if we don't
        // support single-DWORD NOPs because we might otherwise end up with one extra DWORD of space we can't fill.
        uint32 minPostambleBytes = CmdUtil::GetChainSizeInDwords() * sizeof(uint32);

        if (pDevice->CmdUtil().GetMinNopSizeInDwords() > 1)
        {
            minPostambleBytes += pDevice->CmdUtil().GetMinNopSizeInDwords() * sizeof(uint32);
        }

        auto*const pCmdUploadRing = PAL_PLACEMENT_NEW(pMemory) CmdUploadRing(createInfo, pDevice, minPostambleBytes);

        result = pCmdUploadRing->Init(pCmdUploadRing + 1);

        if (result == Result::Success)
        {
            *ppCmdUploadRing = pCmdUploadRing;
        }
        else
        {
            pCmdUploadRing->DestroyInternal();
        }
    }

    return result;
}

// =====================================================================================================================
CmdUploadRing::CmdUploadRing(
    const CmdUploadRingCreateInfo& createInfo,
    Device*                        pDevice,
    uint32                         minPostambleBytes)
    :
    Pal::CmdUploadRing(createInfo,
                       pDevice->Parent(),
                       minPostambleBytes,
                       CmdUtil::GetMaxIndirectBufferSize() * sizeof(uint32)),
    m_cmdUtil(pDevice->CmdUtil())
{
}

// =====================================================================================================================
// Updates the copy command buffer to write commands into the raft memory at the postamble offset such that the
// postamble is completely filled by NOPs followed by one chain packet which points at the chain destination.
// If the chain address is zero the postamble is completely filled with NOPs.
void CmdUploadRing::UploadChainPostamble(
    const IGpuMemory& raftMemory,
    ICmdBuffer*       pCopyCmdBuffer,
    gpusize           postambleOffset,
    gpusize           postambleBytes,
    gpusize           chainDestAddr,
    gpusize           chainDestBytes,
    bool              isConstantEngine,
    bool              isPreemptionEnabled)
{
    constexpr uint32 ChainDwords = CmdUtil::GetChainSizeInDwords();
    constexpr uint32 ChainBytes  = ChainDwords * sizeof(uint32);
    PAL_ASSERT(postambleBytes >= ChainBytes);

    // First upload a NOP header that fills all of the space before the chain (or all space if there's no chain).
    const gpusize nopBytes = postambleBytes - ((chainDestAddr > 0) ? ChainBytes : 0);

    if (nopBytes > 0)
    {
        PAL_ASSERT(IsPow2Aligned(nopBytes, sizeof(uint32)));

        uint32 nopHeader = 0;
        m_cmdUtil.BuildNop(static_cast<size_t>(nopBytes / sizeof(uint32)), &nopHeader);

        pCopyCmdBuffer->CmdUpdateMemory(raftMemory,
                                        postambleOffset,
                                        sizeof(nopHeader),
                                        &nopHeader);
    }

    if (chainDestAddr > 0)
    {
        // Then upload the chain packet at the end of the postamble.
        PAL_ASSERT(IsPow2Aligned(chainDestBytes, sizeof(uint32)));

        uint32 chainPacket[ChainDwords] = {};
        m_cmdUtil.BuildIndirectBuffer(chainDestAddr,
                                      static_cast<size_t>(chainDestBytes / sizeof(uint32)),
                                      true,
                                      isConstantEngine,
                                      isPreemptionEnabled,
                                      chainPacket);

        pCopyCmdBuffer->CmdUpdateMemory(raftMemory,
                                        postambleOffset + nopBytes,
                                        sizeof(chainPacket),
                                        chainPacket);
    }
}

} // Gfx6
} // Pal
