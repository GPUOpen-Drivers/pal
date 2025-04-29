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

#include "core/cmdAllocator.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
CmdStream::CmdStream(
    const Device&  device,
    ICmdAllocator* pCmdAllocator,
    EngineType     engineType,
    SubEngineType  subEngineType,
    CmdStreamUsage cmdStreamUsage,
    bool           isNested)
    :
    GfxCmdStream(device,
                 pCmdAllocator,
                 engineType,
                 subEngineType,
                 cmdStreamUsage,
                 CmdUtil::ChainSizeInDwords(engineType),
                 1,
                 CmdUtil::CondIndirectBufferSize,
                 isNested),
    m_cmdUtil(device.CmdUtil()),
    m_perfCounterWindowEnabled(false),
    m_pPerfCounterWindowLastPacket(nullptr),
    m_usePerfCounterWindow(device.Settings().gfx12EnablePerfCounterWindow)
{
}

// =====================================================================================================================
void CmdStream::BeginCurrentChunk()
{
    const PalSettings& settings = m_pDevice->Settings();

}

// =====================================================================================================================
void CmdStream::EndCurrentChunk(
    bool atEndOfStream)
{
    // The body of the old command block is complete so we can end it. Our block postamble is a basic chaining packet.
    uint32* const pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, true);

    if (m_chainIbSpaceInDwords > 0)
    {
        if (atEndOfStream)
        {
            // Let the GfxCmdStream handle the special chain at the end of each command stream.
            UpdateTailChainLocation(pChainPacket);
        }
        else
        {
            // Fill the chain packet with a NOP and ask for it to be replaced with a real chain to the new chunk.
            CmdUtil::BuildNop(m_chainIbSpaceInDwords, pChainPacket);

            AddChainPatch(ChainPatchType::IndirectBuffer, pChainPacket);
        }
    }
}

// =====================================================================================================================
// Writes a perfcounter config register even if it's not in user-config space.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetOnePerfCtrReg(
    uint32  regAddr,
    uint32  value,
    uint32* pCmdSpace)
{
    if (CmdUtil::IsUserConfigReg(regAddr) == false)
    {
        CopyDataInfo copyData = {};
        copyData.engineType   = GetEngineType();
        copyData.engineSel    = engine_sel__me_copy_data__micro_engine;
        copyData.dstSel       = dst_sel__me_copy_data__perfcounters;
        copyData.dstAddr      = regAddr;
        copyData.srcSel       = src_sel__me_copy_data__immediate_data;
        copyData.srcAddr      = value;
        copyData.countSel     = count_sel__me_copy_data__32_bits_of_data;
        copyData.wrConfirm    = wr_confirm__me_copy_data__wait_for_confirmation;

        pCmdSpace += CmdUtil::BuildCopyData(copyData, pCmdSpace);
    }
    else
    {
        // Use a normal SET_UCONFIG_REG command for normal user-config registers.
        // The resetFilterCam bit is not supported on the MEC, hence set it to 0 as recommended in the PM4 packet spec.
        const EngineType engineType = GetEngineType();
        if (engineType == EngineTypeUniversal)
        {
            pCmdSpace = WriteSetOneUConfigReg<true>(regAddr, value, pCmdSpace);
        }
        else
        {
            pCmdSpace = WriteSetOneUConfigReg<false>(regAddr, value, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes a command which reads a 32-bit perfcounter register and writes it into 4-byte aligned GPU memory.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteCopyPerfCtrRegToMemory(
    uint32  srcReg,
    gpusize dstGpuVa,
    uint32* pCmdSpace)
{
    PAL_ASSERT(srcReg != 0);

    CopyDataInfo copyData = {};
    copyData.engineType   = GetEngineType();
    copyData.engineSel    = engine_sel__me_copy_data__micro_engine;
    copyData.dstSel       = dst_sel__me_copy_data__tc_l2;
    copyData.dstAddr      = dstGpuVa;
    copyData.srcSel       = src_sel__me_copy_data__perfcounters;
    copyData.srcAddr      = srcReg;
    copyData.countSel     = count_sel__me_copy_data__32_bits_of_data;
    copyData.wrConfirm    = wr_confirm__me_copy_data__wait_for_confirmation;

    pCmdSpace += CmdUtil::BuildCopyData(copyData, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
size_t CmdStream::BuildIndirectBuffer(
    gpusize  ibAddr,
    uint32   ibSize,
    bool     preemptionEnabled,
    bool     chain,
    uint32*  pPacket
    ) const
{
    return CmdUtil::BuildIndirectBuffer(GetEngineType(),
                                        ibAddr,
                                        ibSize,
                                        chain,
                                        preemptionEnabled,
                                        pPacket);
}

// =====================================================================================================================
// Update the address contained within indirect buffer packets associated with the current command block
void CmdStream::PatchCondIndirectBuffer(
    ChainPatch*  pPatch,
    gpusize      address,
    uint32       ibSizeDwords
    ) const
{
    PM4_PFP_COND_INDIRECT_BUFFER* pPacket = static_cast<PM4_PFP_COND_INDIRECT_BUFFER*>(pPatch->pPacket);

    switch (pPatch->type)
    {
    case ChainPatchType::CondIndirectBufferPass:
        // The PM4 spec says that the first IB base/size are used if the conditional passes.
        pPacket->ordinal9.u32All       = LowPart(address);
        pPacket->ordinal10.ib_base1_hi = HighPart(address);
        PAL_ASSERT(pPacket->ordinal9.bitfields.reserved1 == 0);

        pPacket->ordinal11.bitfields.ib_size1 = ibSizeDwords;
        break;

    case ChainPatchType::CondIndirectBufferFail:
        // The PM4 spec says that the second IB base/size are used if the conditional fails.
        pPacket->ordinal12.u32All      = LowPart(address);
        pPacket->ordinal13.ib_base2_hi = HighPart(address);
        PAL_ASSERT(pPacket->ordinal12.bitfields.reserved1 == 0);

        pPacket->ordinal14.bitfields.ib_size2 = ibSizeDwords;
        break;

    default:
        // Other patch types should be handled by the base class
        PAL_ASSERT_ALWAYS();
        break;
    } // end switch
}

// =====================================================================================================================
void CmdStream::Call(
    const Pal::CmdStream & targetStream,
    bool                   exclusiveSubmit,
    bool                   allowIb2Launch)
{
    if (targetStream.IsEmpty() == false)
    {
        const auto& gfxStream = static_cast<const GfxCmdStream&>(targetStream);

        // The following are some sanity checks to make sure that the caller and callee are compatible.
        PAL_ASSERT((gfxStream.m_chainIbSpaceInDwords == m_chainIbSpaceInDwords) ||
                   (gfxStream.m_chainIbSpaceInDwords == 0));
        PAL_ASSERT(m_pCmdAllocator->ChunkSize(CommandDataAlloc) >= targetStream.GetFirstChunk()->Size());

        // If this command stream is preemptible, PAL assumes that the target command stream to also be preemptible.
        PAL_ASSERT(IsPreemptionEnabled() == targetStream.IsPreemptionEnabled());

        if (allowIb2Launch)
        {
            // The simplest way of "calling" a nested command stream is to use an IB2 packet, which tells the CP to
            // go execute the indirect buffer and automatically return to the call site. However, compute queues do
            // not support IB2 packets.
            PAL_ASSERT(GetEngineType() != EngineTypeCompute);

            if (gfxStream.m_chainIbSpaceInDwords == 0)
            {
                for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
                {
                    // Note: For nested command buffer which don't support chaining, we need to issue a separate IB2
                    // packet for each chunk.
                    const auto*const pChunk     = chunkIter.Get();
                    uint32*const     pIb2Packet = AllocCommandSpace(m_chainIbSpaceInDwords);
                    BuildIndirectBuffer(pChunk->GpuVirtAddr(),
                                        pChunk->CmdDwordsToExecute(),
                                        targetStream.IsPreemptionEnabled(),
                                        false,
                                        pIb2Packet);
                }
            }
            else
            {
                const auto*const pJumpChunk = targetStream.GetFirstChunk();
                uint32*const     pIb2Packet = AllocCommandSpace(m_chainIbSpaceInDwords);
                BuildIndirectBuffer(pJumpChunk->GpuVirtAddr(),
                                    pJumpChunk->CmdDwordsToExecute(),
                                    targetStream.IsPreemptionEnabled(),
                                    false,
                                    pIb2Packet);
            }
        }
        else if (exclusiveSubmit && (m_chainIbSpaceInDwords != 0) && (gfxStream.m_chainIbSpaceInDwords != 0))
        {
            // NOTE: To call a command stream which supports chaining and has the exclusive submit optmization enabled,
            // we only need to jump to the callee's first chunk, and then jump back here when the callee finishes.

            if (IsEmpty())
            {
                // The call to EndCommandBlock() below will not succeed if this command stream is currently empty. Add
                // the smallest-possible NOP packet to prevent the stream from being empty.
                uint32*const pNopPacket = AllocCommandSpace(m_minNopSizeInDwords);
                BuildNop(m_minNopSizeInDwords, pNopPacket);
            }

            // End our current command block, using the jump to the callee's first chunk as our block postamble.
            const auto*const pJumpChunk   = targetStream.GetFirstChunk();
            uint32*const     pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, false);
            BuildIndirectBuffer(pJumpChunk->GpuVirtAddr(),
                                pJumpChunk->CmdDwordsToExecute(),
                                targetStream.IsPreemptionEnabled(),
                                true,
                                pChainPacket);

            // Returning to the call site requires patching the callee's tail-chain with a packet which brings us
            // back here. However, we need to know the size of the current command block in order to fully construct
            // a chaining packet. So, the solution is to add a chain patch at the callee's tail-chain location which
            // will correspond to the current block.

            // NOTE: The callee's End() method was called after it was done being recorded. That call already built
            // us a dummy NOP packet at the tail-chain location, so we don't need to build a new one at this time!
            AddChainPatch(ChainPatchType::IndirectBuffer, gfxStream.m_pTailChainLocation);
        }
        else
        {
            // NOTE: The target command stream either doesn't have the exclusive submit optimization turned on, or
            // does not support chaining. In either case, we just simply walk over the target's command chunks, and
            // copy their contents into this stream (effectively making this an "inline" call).
            for (auto chunkIter = targetStream.GetFwdIterator(); chunkIter.IsValid(); chunkIter.Next())
            {
                const auto*const pChunk = chunkIter.Get();
                const uint32 sizeInDwords = (pChunk->CmdDwordsToExecute() - gfxStream.m_chainIbSpaceInDwords);

                uint32*const pCmdSpace = AllocCommandSpace(sizeInDwords);
                memcpy(pCmdSpace, pChunk->CpuAddr(), (sizeof(uint32) * sizeInDwords));
            }
        }
    }
}

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetSeqShRegsIndex(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    const void*                     pData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace)
{
    const size_t totalDwords = CmdUtil::BuildSetSeqShRegsIndex<ShaderType>(startRegAddr, endRegAddr, index, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::ShRegIndexSizeDwords],
           pData,
           (totalDwords - CmdUtil::ShRegIndexSizeDwords) * sizeof(uint32));
    pCmdSpace += totalDwords;

    return pCmdSpace;
}

template uint32* CmdStream::WriteSetSeqShRegsIndex<ShaderGraphics>(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    const void*                     pData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace);
template uint32* CmdStream::WriteSetSeqShRegsIndex<ShaderCompute >(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    const void*                     pData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace);

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetOneShRegIndex(
    uint32                          regAddr,
    uint32                          regData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace)
{
    return WriteSetSeqShRegsIndex<ShaderType>(regAddr, regAddr, &regData, index, pCmdSpace);
}

template uint32* CmdStream::WriteSetOneShRegIndex<ShaderGraphics>(
    uint32                          regAddr,
    uint32                          regData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace);
template uint32* CmdStream::WriteSetOneShRegIndex<ShaderCompute >(
    uint32                          regAddr,
    uint32                          regData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace);

// =====================================================================================================================
uint32* CmdStream::WriteSetSeqContextRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    // NOTE: We'll use other state tracking to determine whether a context roll occurred for non-immediate-mode
    //       optimizations.
    const size_t totalDwords = CmdUtil::BuildSetSeqContextRegs(startRegAddr, endRegAddr, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::SetContextRegHeaderSizeDwords],
           pData,
           (totalDwords - CmdUtil::SetContextRegHeaderSizeDwords) * sizeof(uint32));
    pCmdSpace += totalDwords;

    return pCmdSpace;
}

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetOneShReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    return WriteSetSeqShRegs<ShaderType>(regAddr, regAddr, &regData, pCmdSpace);
}

template uint32* CmdStream::WriteSetOneShReg<ShaderGraphics>(uint32, uint32, uint32*);
template uint32* CmdStream::WriteSetOneShReg<ShaderCompute >(uint32, uint32, uint32*);

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetSeqShRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    const size_t totalDwords = CmdUtil::BuildSetSeqShRegs<ShaderType>(startRegAddr, endRegAddr, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::SetShRegHeaderSizeDwords],
           pData,
           (totalDwords - CmdUtil::SetShRegHeaderSizeDwords) * sizeof(uint32));

    pCmdSpace += totalDwords;

    return pCmdSpace;
}

template uint32* CmdStream::WriteSetSeqShRegs<ShaderGraphics>(uint32, uint32, const void*, uint32*);
template uint32* CmdStream::WriteSetSeqShRegs<ShaderCompute >(uint32, uint32, const void*, uint32*);

// =====================================================================================================================
uint32* CmdStream::WriteSetContextPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    uint32*                  pCmdSpace)
{
    return pCmdSpace + CmdUtil::BuildSetContextPairs(pPairs, numPairs, pCmdSpace);
}

// =====================================================================================================================
uint32* CmdStream::WriteSetOneContextReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{

    RegisterValuePair regPair;
    regPair.offset = regAddr - CONTEXT_SPACE_START;
    regPair.value  = regData;

    return pCmdSpace + CmdUtil::BuildSetContextPairs(&regPair, 1, pCmdSpace);
}

// =====================================================================================================================
// Builds a PM4 packet to modify the given register.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteContextRegRmw(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace)
{
    return pCmdSpace + CmdUtil::BuildContextRegRmw(regAddr, regMask, regData, pCmdSpace);
}

// =====================================================================================================================
// Builds a PM4 packet to set the given config register. Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool IsPerfCtr>
uint32* CmdStream::WriteSetOneUConfigReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace
    ) const
{
    // If we write GRBM_GFX_INDEX to non-broadcast mode, the firmware needs to be configured with PERF_COUNTER_WINDOW
    // enabled to prevent hardware signals invoking behavior that may break while GRBM is not broadcasting
    PAL_ASSERT_MSG(((regAddr != mmGRBM_GFX_INDEX) ||
                    (m_perfCounterWindowEnabled == TRUE) ||
                    ((reinterpret_cast<GRBM_GFX_INDEX*>(&regData)->bitfields.INSTANCE_BROADCAST_WRITES == 1) &&
                     (reinterpret_cast<GRBM_GFX_INDEX*>(&regData)->bitfields.SA_BROADCAST_WRITES == 1) &&
                     (reinterpret_cast<GRBM_GFX_INDEX*>(&regData)->bitfields.SE_BROADCAST_WRITES == 1))),
        "PERF_COUNTER_WINDOW not set for non-broadcast GRBM read/writes");

    return pCmdSpace + CmdUtil::BuildSetOneUConfigReg<IsPerfCtr>(regAddr, regData, pCmdSpace);
}

template
uint32* CmdStream::WriteSetOneUConfigReg<true>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace) const;
template
uint32* CmdStream::WriteSetOneUConfigReg<false>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace) const;

// =====================================================================================================================
// Builds a PM4 packet to set the given set of sequential config registers.  Returns a pointer to the next unused DWORD
// in pCmdSpace.
template <bool IsPerfCtr>
uint32* CmdStream::WriteSetSeqUConfigRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    const size_t totalDwords = CmdUtil::BuildSetSeqUConfigRegs<IsPerfCtr>(startRegAddr, endRegAddr, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::SetUConfigRegHeaderSizeDwords],
           pData,
           (totalDwords - CmdUtil::SetUConfigRegHeaderSizeDwords) * sizeof(uint32));

    pCmdSpace += totalDwords;

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetSeqUConfigRegs<true>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);
template
uint32* CmdStream::WriteSetSeqUConfigRegs<false>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);

// =====================================================================================================================
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetShPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    uint32*                  pCmdSpace)
{
    return pCmdSpace + CmdUtil::BuildSetShPairs<ShaderType, false>(pPairs, numPairs, pCmdSpace);
}

template uint32* CmdStream::WriteSetShPairs<ShaderGraphics>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    uint32*                  pCmdSpace);
template uint32* CmdStream::WriteSetShPairs<ShaderCompute>(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    uint32*                  pCmdSpace);

// =====================================================================================================================
uint32* CmdStream::WriteSetUConfigPairs(
    const RegisterValuePair* pPairs,
    uint32                   numPairs,
    uint32*                  pCmdSpace)
{
    return pCmdSpace + CmdUtil::BuildSetUConfigPairs(pPairs, numPairs, pCmdSpace);
}

// =====================================================================================================================
size_t CmdStream::BuildCondIndirectBuffer(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      data,
    uint64      mask,
    uint32*     pPacket
    ) const
{
    return CmdUtil::BuildCondIndirectBuffer(compareFunc, compareGpuAddr, data, mask, pPacket);
}

// =====================================================================================================================
// Writes PERF_COUNTER_WINDOW pm4 packet and tracks state to protect from accidentally missing a window configuration
uint32* CmdStream::WritePerfCounterWindow(
    bool    enableWindow,
    uint32* pCmdSpace)
{
    if (enableWindow != m_perfCounterWindowEnabled)
    {
        m_perfCounterWindowEnabled = enableWindow;

        if (m_usePerfCounterWindow)
        {
            // If the perf counter window was changed back to back, update to only latest state by overwriting the previous
            // packet. Basically a low pass filter
            if ((m_pPerfCounterWindowLastPacket + CmdUtil::PerfCounterWindowSizeDwords) == pCmdSpace)
            {
                m_cmdUtil.BuildPerfCounterWindow(GetEngineType(), enableWindow, m_pPerfCounterWindowLastPacket);
            }
            else
            {
                m_pPerfCounterWindowLastPacket = pCmdSpace;
                pCmdSpace += m_cmdUtil.BuildPerfCounterWindow(GetEngineType(), enableWindow, pCmdSpace);
            }
        }
    }

    return pCmdSpace;
}

} // namespace Gfx12
} // namespace Pal
