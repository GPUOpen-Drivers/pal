/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Pm4Optimizer.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "palIterator.h"
#include "palLinearAllocator.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
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
                 CmdUtil::MinNopSizeInDwords,
                 CmdUtil::CondIndirectBufferSize,
                 isNested),
    m_cmdUtil(device.CmdUtil()),
    m_pPm4Optimizer(nullptr),
    m_pChunkPreamble(nullptr)
{
}

// =====================================================================================================================
Result CmdStream::Begin(
    CmdStreamBeginFlags     flags,
    VirtualLinearAllocator* pMemAllocator)
{
    // Note that we don't support command optimization or command prefetch for CE.
    if (m_subEngineType == SubEngineType::ConstantEngine)
    {
        flags.optimizeCommands = 0;
        flags.prefetchCommands = 0;
    }
    else
    {
        // We can't enable PM4 optimization without an allocator because we need to dynamically allocate a Pm4Optimizer.
        flags.optimizeCommands &= (pMemAllocator != nullptr);

        if (flags.prefetchCommands != 0)
        {
            // The prefetchCommands flag was already set according to the client's command buffer build info.
            // However, we really should force prefetching off if the command data is in local memory because:
            // 1. Local memory is fast enough that cold reads are no problem. Prefetching the whole command chunk
            //    ahead of time might evict things from the L2 cache that we need right now, hurting performance.
            // 2. We try to use the uncached MTYPE when allocating local memory for command data. This avoids any
            //    L2 cache pollution but also makes prefetching completely useless because it only prefetches to L2.
            if (m_pCmdAllocator->LocalCommandData())
            {
                flags.prefetchCommands = 0;
            }
            else
            {
                const Gfx9PalSettings& settings = GetGfx9Settings(*m_device.Parent());
                const PrefetchMethod   method   = (GetEngineType() == EngineTypeCompute)
                                                        ? settings.commandPrefetchMethodAce
                                                        : settings.commandPrefetchMethodGfx;

                // We also should force off prefetching if the per-engine setting says it's disabled. Note that we
                // only support CP DMA command prefetching.
                PAL_ASSERT(method != PrefetchPrimeUtcL2);

                flags.prefetchCommands = (method == PrefetchCpDma);
            }
        }
    }

    Result result = GfxCmdStream::Begin(flags, pMemAllocator);

    if ((result == Result::Success) && (m_flags.optimizeCommands == 1))
    {
        // Allocate a temporary PM4 optimizer to use during command building.
        m_pPm4Optimizer = PAL_NEW(Pm4Optimizer, m_pMemAllocator, AllocInternal)(static_cast<const Device&>(m_device));

        if (m_pPm4Optimizer == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
void CmdStream::Reset(
    CmdAllocator* pNewAllocator,
    bool          returnGpuMemory)
{
    // Reset all tracked state.
    m_pChunkPreamble = nullptr;

    GfxCmdStream::Reset(pNewAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Writes any context, SH, or uconfig register range. Register ranges may not cross register space boundaries.
// Any indexed registers must be written with a "count" parameter of 1.
// This function does not support writing perf counter regs.
// This is a high overhead function which should only be used when the callee does not know the register space
uint32* CmdStream::WriteRegisters(
    uint32        startAddr,
    uint32        count,
    const uint32* pRegData,
    uint32*       pCmdSpace)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    if (count > 1)
    {
        // Indexed registers must be written individually with no other registers.
        for (uint32 i = 0; i < count; i++)
        {
            PAL_ASSERT(CmdUtil::IsIndexedRegister(startAddr + i) == false);
        }
    }
#endif

    constexpr uint32 ComputePeristentStart = mmCOMPUTE_DISPATCH_INITIATOR;
    const uint32 endAddr = startAddr + count - 1;

    // Note that gfx11 didn't reuse the register offsets occupied by Gfx10::mmSPI_SHADER_PGM_RSRC*_VS. Thus if regAddr
    // matches one of those values we must be running on a gfx10 ASIC so there's no need to actually check.
    if ((count == 1) &&
        ((startAddr == mmSPI_SHADER_PGM_RSRC3_GS)        ||
         (startAddr == mmSPI_SHADER_PGM_RSRC4_GS)        ||
         (startAddr == mmSPI_SHADER_PGM_RSRC3_HS)        ||
         (startAddr == mmSPI_SHADER_PGM_RSRC4_HS)        ||
         (startAddr == mmSPI_SHADER_PGM_RSRC3_PS)        ||
         (startAddr == mmSPI_SHADER_PGM_RSRC4_PS)        ||
         (startAddr == Gfx10::mmSPI_SHADER_PGM_RSRC3_VS) ||
         (startAddr == Gfx10::mmSPI_SHADER_PGM_RSRC4_VS)))
    {
        // Handle indexed SH
        pCmdSpace = WriteSetOneGfxShRegIndexApplyCuMask(startAddr, *pRegData, pCmdSpace);
    }
    else if ((count == 1) && ((startAddr == mmVGT_INDEX_TYPE) || (startAddr == mmVGT_NUM_INSTANCES)))
    {
        // Handle indexed uconfig
        PFP_SET_UCONFIG_REG_INDEX_index_enum index = index__pfp_set_uconfig_reg_index__default;

        if (startAddr == mmVGT_INDEX_TYPE)
        {
            index = index__pfp_set_uconfig_reg_index__index_type;
        }
        else if (startAddr == mmVGT_NUM_INSTANCES)
        {
            index = index__pfp_set_uconfig_reg_index__num_instances;
        }

        pCmdSpace = WriteSetOneConfigReg( startAddr, *pRegData, pCmdSpace, index);
    }
    else if ((startAddr >= CONTEXT_SPACE_START) &&
             ((startAddr + count) < CntxRegUsedRangeEnd))
    {
        pCmdSpace = WriteSetSeqContextRegs(startAddr, endAddr, pRegData, pCmdSpace);
    }
    else if ((startAddr >= PERSISTENT_SPACE_START) &&
             ((startAddr + count) < ComputePeristentStart))
    {
        pCmdSpace = WriteSetSeqShRegs(startAddr, endAddr, ShaderGraphics, pRegData, pCmdSpace);
    }
    else if ((startAddr >= ComputePeristentStart) &&
             ((startAddr + count) < ShRegUsedRangeEnd))
    {
        pCmdSpace = WriteSetSeqShRegs(startAddr, endAddr, ShaderCompute, pRegData, pCmdSpace);
    }
    else if ((startAddr >= UCONFIG_SPACE_START) &&
             ((startAddr + count) < UCONFIG_SPACE_END))
    {
        pCmdSpace = WriteSetSeqConfigRegs<false>(startAddr, endAddr, pRegData, pCmdSpace);
    }
    else
    {
        // We aren't expecting inputs to this function to cross register range boundries.
        PAL_ASSERT_ALWAYS();
    }

    return pCmdSpace;
}

// =====================================================================================================================
void CmdStream::CleanupTempObjects()
{
    // Clean up the temporary PM4 optimizer object.
    if (m_pMemAllocator != nullptr)
    {
        PAL_SAFE_DELETE(m_pPm4Optimizer, m_pMemAllocator);
    }
}

// =====================================================================================================================
// Helper which updates state in the pm4optimizer and checks if the register is redundant or not. Useful for cases where
// callers want to form their own packets.
bool CmdStream::MustKeepSetShReg(
    uint32 userDataAddr,
    uint32 userDataValue)
{
    bool mustKeep = true;

    if (m_flags.optimizeCommands)
    {
        mustKeep = m_pPm4Optimizer->MustKeepSetShReg(userDataAddr, userDataValue);
    }

    return mustKeep;
}

// =====================================================================================================================
// Builds a PM4 packet to modify the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteContextRegRmw(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepContextRegRmw(regAddr, regMask, regData))
    {
        pCmdSpace += m_cmdUtil.BuildContextRegRmw(regAddr, regMask, regData, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteContextRegRmw<true>(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteContextRegRmw<false>(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace);

// =====================================================================================================================
// Wrapper for real WriteContextRegRmw() when it isn't known whether the immediate pm4 optimizer is enabled.
uint32* CmdStream::WriteContextRegRmw(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteContextRegRmw<true>(regAddr, regMask, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteContextRegRmw<false>(regAddr, regMask, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
//
// Note that we must be very careful when setting registers on gfx10. The CP's register filter CAM isn't smart enough
// to track GRBM_GFX_INDEX so it can filter out packets that set the same register value for different instances.
// The caller must set isPerfCtr = true when they could write any register to multiple instances.
template <bool isPerfCtr>
uint32* CmdStream::WriteSetOneConfigReg(
    uint32                               regAddr,
    uint32                               regData,
    uint32*                              pCmdSpace,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index
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

    const size_t totalDwords = m_cmdUtil.BuildSetOneConfigReg<isPerfCtr>(regAddr, pCmdSpace, index);
    pCmdSpace[CmdUtil::ConfigRegSizeDwords] = regData;

    return pCmdSpace + totalDwords;
}

template
uint32* CmdStream::WriteSetOneConfigReg<true>(
    uint32                               regAddr,
    uint32                               regData,
    uint32*                              pCmdSpace,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index) const;
template
uint32* CmdStream::WriteSetOneConfigReg<false>(
    uint32                               regAddr,
    uint32                               regData,
    uint32*                              pCmdSpace,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index) const;

// =====================================================================================================================
// Builds a PM4 packet to set the given set of sequential config registers.  Returns a pointer to the next unused DWORD
// in pCmdSpace.
//
// Note that we must be very careful when setting registers on gfx10. The CP's register filter CAM isn't smart enough
// to track GRBM_GFX_INDEX so it can filter out packets that set the same register value for different instances.
// The caller must set isPerfCtr = true when they could write any register to multiple instances.
template <bool isPerfCtr>
uint32* CmdStream::WriteSetSeqConfigRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    const size_t totalDwords = m_cmdUtil.BuildSetSeqConfigRegs<isPerfCtr>(startRegAddr, endRegAddr, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::ConfigRegSizeDwords],
           pData,
           (totalDwords - CmdUtil::ConfigRegSizeDwords) * sizeof(uint32));

    return (pCmdSpace + totalDwords);
}
template
uint32* CmdStream::WriteSetSeqConfigRegs<true>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);
template
uint32* CmdStream::WriteSetSeqConfigRegs<false>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);

// =====================================================================================================================
// Builds a PM4 packet to set the given set of sequential config registers to 0.  Returns a pointer to the next unused
// DWORD in pCmdSpace.
//
// Note that we must be very careful when setting registers on gfx10. The CP's register filter CAM isn't smart enough
// to track GRBM_GFX_INDEX so it can filter out packets that set the same register value for different instances.
// The caller must set isPerfCtr = true when they could write any register to multiple instances.
template <bool isPerfCtr>
uint32* CmdStream::WriteSetZeroSeqConfigRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    uint32* pCmdSpace)
{
    const size_t totalDwords = m_cmdUtil.BuildSetSeqConfigRegs<isPerfCtr>(startRegAddr, endRegAddr, pCmdSpace);

    memset(&pCmdSpace[CmdUtil::ConfigRegSizeDwords], 0, (totalDwords - CmdUtil::ConfigRegSizeDwords) * sizeof(uint32));

    return (pCmdSpace + totalDwords);
}
template
uint32* CmdStream::WriteSetZeroSeqConfigRegs<true>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteSetZeroSeqConfigRegs<false>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    uint32* pCmdSpace);

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetOneContextReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetContextReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneContextReg(regAddr, pCmdSpace);

        pCmdSpace[CmdUtil::ContextRegSizeDwords] = regData;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetOneContextReg<true>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteSetOneContextReg<false>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetOneContextReg() when it isn't known whether the immediate pm4 optimizer is enabled.
uint32* CmdStream::WriteSetOneContextReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetOneContextReg<true>(regAddr, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetOneContextReg<false>(regAddr, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register when the caller already guarantees that the write is not redundant. The
// caller should be careful not to mix this function with the regular WriteSetOneContextReg() for the same register(s).
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetOneContextRegNoOpt(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    const size_t totalDwords = m_cmdUtil.BuildSetOneContextReg(regAddr, pCmdSpace);

    pCmdSpace[CmdUtil::ContextRegSizeDwords] = regData;
    pCmdSpace += totalDwords;

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType shaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteSetOneShReg(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneShReg(regAddr, shaderType, pCmdSpace);

        pCmdSpace[CmdUtil::ShRegSizeDwords] = regData;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetOneShReg<ShaderGraphics, true>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetOneShReg<ShaderGraphics, false>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetOneShReg<ShaderCompute, true>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetOneShReg<ShaderCompute, false>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetOneShReg() for when the caller doesn't know if the immediate pm4 optimizer is enabled.
template <Pm4ShaderType shaderType>
uint32* CmdStream::WriteSetOneShReg(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetOneShReg<shaderType, true>(regAddr, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetOneShReg<shaderType, false>(regAddr, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate the template for the linker.
template
uint32* CmdStream::WriteSetOneShReg<ShaderGraphics>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetOneShReg<ShaderCompute>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);

// =====================================================================================================================
// Wrapper for the Pm4OptImmediate templated method.
uint32* CmdStream::WriteSetOneGfxShRegIndexApplyCuMask(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    if (m_flags.optimizeCommands == 0)
    {
        pCmdSpace = WriteSetOneGfxShRegIndexApplyCuMask<false>(regAddr, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetOneGfxShRegIndexApplyCuMask<true>(regAddr, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptImmediate>
uint32* CmdStream::WriteSetOneGfxShRegIndexApplyCuMask(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    if ((Pm4OptImmediate == false) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneShRegIndex(regAddr,
                                                                   ShaderGraphics,
                                                                   index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                   pCmdSpace);

        pCmdSpace[CmdUtil::ShRegSizeDwords] = regData;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetOneGfxShRegIndexApplyCuMask<true>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteSetOneGfxShRegIndexApplyCuMask<false>(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace);

// =====================================================================================================================
// Wrapper for the Pm4OptImmediate templated version.
uint32* CmdStream::WriteSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    Pm4ShaderType shaderType,
    const void*   pData,
    uint32*       pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        pCmdSpace = WriteSetSeqShRegs<true>(startRegAddr, endRegAddr, shaderType, pData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetSeqShRegs<false>(startRegAddr, endRegAddr, shaderType, pData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptImmediate>
uint32* CmdStream::WriteSetSeqShRegs(
    uint32        startRegAddrIn,
    uint32        endRegAddrIn,
    Pm4ShaderType shaderType,
    const void*   pDataIn,
    uint32*       pCmdSpace)
{
    bool          issuePacket  = true;
    uint32        startRegAddr = startRegAddrIn;
    uint32        endRegAddr   = endRegAddrIn;
    const uint32* pData        = static_cast<const uint32*>(pDataIn);

    if (Pm4OptImmediate)
    {
        issuePacket = m_pPm4Optimizer->OptimizePm4SetRegSeq<StateType::Sh>(&startRegAddr, &endRegAddr, &pData);
    }

    if (issuePacket)
    {
        const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pCmdSpace);

        memcpy(&pCmdSpace[CmdUtil::ShRegSizeDwords],
               pData,
               (totalDwords - CmdUtil::ShRegSizeDwords) * sizeof(uint32));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetSeqShRegs<true>(
    uint32        startRegAddrIn,
    uint32        endRegAddrIn,
    Pm4ShaderType shaderType,
    const void*   pDataIn,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetSeqShRegs<false>(
    uint32        startRegAddrIn,
    uint32        endRegAddrIn,
    Pm4ShaderType shaderType,
    const void*   pDataIn,
    uint32*       pCmdSpace);

// =====================================================================================================================
// Builds a PM4 packet to set the given registers to 0 unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetZeroSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    Pm4ShaderType shaderType,
    uint32*       pCmdSpace)
{
    // Only m_shadowInitCmdStream will call this function, and m_flags.optimizeCommands of 0 in m_shadowInitCmdStream.
    PAL_ASSERT(m_flags.optimizeCommands == 0);
    const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pCmdSpace);
    memset(&pCmdSpace[CmdUtil::ShRegSizeDwords], 0, (totalDwords - CmdUtil::ShRegSizeDwords) * sizeof(uint32));
    pCmdSpace += totalDwords;

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegsIndex(
    uint32                          startRegAddrIn,
    uint32                          endRegAddrIn,
    Pm4ShaderType                   shaderType,
    const void*                     pDataIn,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace)
{
    bool          issuePacket  = true;
    uint32        startRegAddr = startRegAddrIn;
    uint32        endRegAddr   = endRegAddrIn;
    const uint32* pData        = static_cast<const uint32*>(pDataIn);

    if (m_flags.optimizeCommands == 1)
    {
        issuePacket = m_pPm4Optimizer->OptimizePm4SetRegSeq<StateType::Sh>(&startRegAddr, &endRegAddr, &pData);
    }

    if (issuePacket)
    {
        const size_t totalDwords =
            m_cmdUtil.BuildSetSeqShRegsIndex(startRegAddr, endRegAddr, shaderType, index, pCmdSpace);

        memcpy(&pCmdSpace[CmdUtil::ShRegSizeDwords],
               pData,
               (totalDwords - CmdUtil::ShRegSizeDwords) * sizeof(uint32));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given packed register pairs unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType ShaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteSetShRegPairs(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetShRegPairs<ShaderType>(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace += m_cmdUtil.BuildSetShRegPairsPacked<ShaderType>(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetShRegPairs<ShaderGraphics, true>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderGraphics, false>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute, true>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute, false>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetShRegPairs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetShRegPairs(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetShRegPairs<ShaderType, true>(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetShRegPairs<ShaderType, false>(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetShRegPairs<ShaderGraphics>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);

// =====================================================================================================================
// Write an array of constant SH RegisterValuePair values to HW.
template <Pm4ShaderType ShaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteSetConstShRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);
    PAL_DEBUG_BUILD_ONLY_ASSERT((numRegs % 2) == 0);

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetShRegPairs<ShaderType>(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        constexpr RegisterRangeType RegType = (ShaderType == ShaderGraphics) ? RegRangeGfxSh : RegRangeCsSh;
        const size_t pktSize = m_cmdUtil.BuildSetRegPairsPackedHeader<RegType>(numRegs, pCmdSpace);

        memcpy(&(pCmdSpace[CmdUtil::SetRegPairsPackedHeaderSizeInDwords]),
               pRegPairs,
               sizeof(PackedRegisterPair) * (numRegs / 2));

        pCmdSpace += pktSize;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetConstShRegPairs<ShaderGraphics, true>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);
template
uint32* CmdStream::WriteSetConstShRegPairs<ShaderGraphics, false>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);
template
uint32* CmdStream::WriteSetConstShRegPairs<ShaderCompute, true>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);
template
uint32* CmdStream::WriteSetConstShRegPairs<ShaderCompute, false>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);

// =====================================================================================================================
// Builds a PM4 packet to set the given packed register pairs unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetContextRegPairs(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetContextRegPairs(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace += m_cmdUtil.BuildSetContextRegPairsPacked(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetContextRegPairs<true>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetContextRegPairs<false>(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetConstContextRegPairs() for when the caller doesn't know if the immediate mode
// pm4 optimizer is enabled.
uint32* CmdStream::WriteSetConstContextRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetConstContextRegPairs<true>(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetConstContextRegPairs<false>(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given packed register pairs unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetConstContextRegPairs(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    // The caller MUST ensure we are programming an even number of regs! For odd counts, just replicate any reg once!
    PAL_DEBUG_BUILD_ONLY_ASSERT((numRegs % 2 == 0));

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetContextRegPairs(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace += CmdUtil::BuildSetConstContextRegPairsPacked(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetConstContextRegPairs<true>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);
template
uint32* CmdStream::WriteSetConstContextRegPairs<false>(
    const PackedRegisterPair* pRegPairs,
    uint32                    numRegs,
    uint32*                   pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetContextRegPairs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteSetContextRegPairs(
    PackedRegisterPair* pRegPairs,
    uint32              numRegs,
    uint32*             pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetContextRegPairs<true>(pRegPairs, numRegs, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetContextRegPairs<false>(pRegPairs, numRegs, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given packed register pairs unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetContextRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetContextRegPairs(pRegPairs, numRegPairs, pCmdSpace);
    }
    else
    {
        pCmdSpace += CmdUtil::BuildSetRegPairs<RegRangeContext>(pRegPairs, numRegPairs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetContextRegPairs<true>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);
template
uint32* CmdStream::WriteSetContextRegPairs<false>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetContextRegPairs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteSetContextRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetContextRegPairs<true>(pRegPairs, numRegPairs, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetContextRegPairs<false>(pRegPairs, numRegPairs, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Wrapper for unpacked variant of WriteSetShRegPairs when PM4Opt isn't statically known.
template <Pm4ShaderType ShaderType>
uint32* CmdStream::WriteSetShRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetShRegPairs<ShaderType, true>(pRegPairs, numRegPairs, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetShRegPairs<ShaderType, false>(pRegPairs, numRegPairs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);

// =====================================================================================================================
// Write an array of constant SH RegisterValuePair values to HW.
template <Pm4ShaderType ShaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteSetShRegPairs(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if (Pm4OptEnabled)
    {
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetShRegPairs<ShaderType>(pRegPairs, numRegPairs, pCmdSpace);
    }
    else
    {
        constexpr RegisterRangeType RegType = (ShaderType == ShaderGraphics) ? RegRangeGfxSh : RegRangeCsSh;
        pCmdSpace += CmdUtil::BuildSetRegPairs<RegType>(pRegPairs, numRegPairs, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetShRegPairs<ShaderGraphics, true>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderGraphics, false>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute, true>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);
template
uint32* CmdStream::WriteSetShRegPairs<ShaderCompute, false>(
    const RegisterValuePair* pRegPairs,
    uint32                   numRegPairs,
    uint32*                  pCmdSpace);

// =====================================================================================================================
// Helper function for writing user-SGPRs mapped to user-data entries for a graphics or compute shader stage.
template <bool IgnoreDirtyFlags, Pm4ShaderType shaderType>
uint32* CmdStream::WriteUserDataEntriesToSgprs(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace)
{
    if (m_flags.optimizeCommands != 0)
    {
        pCmdSpace = WriteUserDataEntriesToSgprs<IgnoreDirtyFlags, shaderType, true>(entryMap, entries, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteUserDataEntriesToSgprs<IgnoreDirtyFlags, shaderType, false>(entryMap, entries, pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate template for linker.
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<false, ShaderGraphics>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<false, ShaderCompute>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<true, ShaderGraphics>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<true, ShaderCompute>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);

// =====================================================================================================================
// Helper function for writing user-SGPRs mapped to user-data entries for a graphics or compute shader stage.
template <bool IgnoreDirtyFlags, Pm4ShaderType shaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteUserDataEntriesToSgprs(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace)
{
    // Virtualized user-data entries are always remapped to a consecutive sequence of user-SGPR's. Because of this
    // mapping, we can always assume that this operation will result in a series of zero or more consecutive registers
    // being written, except in the case where we skip entries which aren't dirty (i.e., IgnoreDirtyFlags is false).
    const uint16 firstUserSgpr = entryMap.firstUserSgprRegAddr;
    const uint16 userSgprCount = entryMap.userSgprCount;

    uint32 scratchMem[NumUserDataRegisters];
    uint32* pCmdPayload = Pm4OptEnabled ? scratchMem : (pCmdSpace + CmdUtil::ShRegSizeDwords);

    if (IgnoreDirtyFlags)
    {
        if (userSgprCount != 0)
        {
            for (uint16 sgpr = 0; sgpr < userSgprCount; ++sgpr)
            {
                pCmdPayload[sgpr] = entries.entries[entryMap.mappedEntry[sgpr]];
            }

            // PM4Opt path wrote userdata to scratch space on the stack - just call normal write method.
            if (Pm4OptEnabled)
            {
                pCmdSpace = WriteSetSeqShRegs<Pm4OptEnabled>(firstUserSgpr,
                                                             (firstUserSgpr + userSgprCount - 1),
                                                             shaderType,
                                                             scratchMem,
                                                             pCmdSpace);
            }
            // The PM4Opt off path forms the packets inline in the command buffer - just need to add header/offset.
            else
            {
                const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(firstUserSgpr,
                                                                       (firstUserSgpr + userSgprCount - 1),
                                                                       shaderType,
                                                                       pCmdSpace);
                // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
                PAL_ASSERT(totalDwords == (userSgprCount + CmdUtil::ShRegSizeDwords));
                pCmdSpace += totalDwords;
            }
        }
    }
    else
    {
        // If we are honoring the dirty flags, then there may be multiple packets because skipping dirty entries
        // can break the assumption about only writing consecutive registers.
        for (uint16 sgpr = 0; sgpr < userSgprCount; ++sgpr)
        {
            const uint16 packetFirstSgpr = (firstUserSgpr + sgpr);
            uint16       packetSgprCount = 0;

            uint16 entry = entryMap.mappedEntry[sgpr];
            while ((sgpr < userSgprCount) && WideBitfieldIsSet(entries.dirty, entry))
            {
                pCmdPayload[packetSgprCount] = entries.entries[entry];
                ++packetSgprCount;
                ++sgpr;
                entry = entryMap.mappedEntry[sgpr];
            }

            if (packetSgprCount > 0)
            {
                // PM4Opt path wrote userdata to scratch space on the stack - just call normal write method.
                if (Pm4OptEnabled)
                {
                    pCmdSpace = WriteSetSeqShRegs<Pm4OptEnabled>(packetFirstSgpr,
                                                                 (packetFirstSgpr + packetSgprCount - 1),
                                                                 shaderType,
                                                                 scratchMem,
                                                                 pCmdSpace);
                }
                // The PM4Opt off path forms the packets inline in the command buffer - just need to add header/offset.
                else
                {
                    const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(packetFirstSgpr,
                                                                           (packetFirstSgpr + packetSgprCount - 1),
                                                                           shaderType,
                                                                           pCmdSpace);
                    // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
                    PAL_ASSERT(totalDwords == (packetSgprCount + CmdUtil::ShRegSizeDwords));
                    pCmdSpace   += totalDwords;
                    pCmdPayload += totalDwords;
                }
            }
        } // for each mapped user-SGPR
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteUserDataEntriesToSgprs<true, ShaderGraphics, true>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<true, ShaderGraphics, false>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<false, ShaderGraphics, true>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprs<false, ShaderGraphics, false>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries& entries,
    uint32* pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteLoadSeqContextRegs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteLoadSeqContextRegs(
    uint32      startRegAddr,
    uint32      regCount,
    gpusize     dataVirtAddr,
    uint32*     pCmdSpace)
{
    const size_t packetSizeDwords = m_cmdUtil.BuildLoadContextRegsIndex<true>(dataVirtAddr,
                                                                              startRegAddr,
                                                                              regCount,
                                                                              pCmdSpace);
    if (m_flags.optimizeCommands)
    {
        // Mark all regs updated on the GPU invalid in the PM4Optimizer.
        for (uint32 x = 0; x < regCount; x++)
        {
            m_pPm4Optimizer->SetCtxRegInvalid(startRegAddr + x);
        }
    }
    pCmdSpace += packetSizeDwords;

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetSeqContextRegs(
    uint32      startRegAddrIn,
    uint32      endRegAddrIn,
    const void* pDataIn,
    uint32*     pCmdSpace)
{
    PAL_DEBUG_BUILD_ONLY_ASSERT((m_flags.optimizeCommands == Pm4OptEnabled) ||
                                (Pm4Optimizer::IsRegisterMustWrite(startRegAddrIn) &&
                                 Pm4Optimizer::IsRegisterMustWrite(endRegAddrIn)));

    bool          issuePacket  = true;
    uint32        startRegAddr = startRegAddrIn;
    uint32        endRegAddr   = endRegAddrIn;
    const uint32* pData        = static_cast<const uint32*>(pDataIn);

    if (Pm4OptEnabled)
    {
        issuePacket = m_pPm4Optimizer->OptimizePm4SetRegSeq<StateType::Context>(&startRegAddr, &endRegAddr, &pData);
    }

    if (issuePacket)
    {
        // NOTE: We'll use other state tracking to determine whether a context roll occurred for non-immediate-mode
        //       optimizations.
        const size_t totalDwords = m_cmdUtil.BuildSetSeqContextRegs(startRegAddr, endRegAddr, pCmdSpace);

        memcpy(&pCmdSpace[CmdUtil::ContextRegSizeDwords],
               pData,
               (totalDwords - CmdUtil::ContextRegSizeDwords) * sizeof(uint32));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetSeqContextRegs<true>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);
template
uint32* CmdStream::WriteSetSeqContextRegs<false>(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetSeqContextRegs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteSetSeqContextRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetSeqContextRegs<true>(startRegAddr, endRegAddr, pData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetSeqContextRegs<false>(startRegAddr, endRegAddr, pData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given base unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetBase(
    gpusize                         address,
    PFP_SET_BASE_base_index_enum    baseIndex,
    Pm4ShaderType                   shaderType,
    uint32*                         pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetBase(address, baseIndex, shaderType))
    {
        pCmdSpace += m_cmdUtil.BuildSetBase(address, baseIndex, shaderType, pCmdSpace);
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetBase<true>(
    gpusize                         address,
    PFP_SET_BASE_base_index_enum    baseIndex,
    Pm4ShaderType                   shaderType,
    uint32*                         pCmdSpace);
template
uint32* CmdStream::WriteSetBase<false>(
    gpusize                         address,
    PFP_SET_BASE_base_index_enum    baseIndex,
    Pm4ShaderType                   shaderType,
    uint32*                         pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteSetBase() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteSetBase(
    gpusize                         address,
    PFP_SET_BASE_base_index_enum    baseIndex,
    Pm4ShaderType                   shaderType,
    uint32*                         pCmdSpace)
{
    if (m_flags.optimizeCommands)
    {
        pCmdSpace = WriteSetBase<true>(address, baseIndex, shaderType, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetBase<false>(address, baseIndex, shaderType, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Non-templated wrapper for the templated NotifyIndirectShRegWrite call.
void CmdStream::NotifyIndirectShRegWrite(
    uint32 regAddr,
    uint32 numRegsToInvalidate)
{
    if (m_flags.optimizeCommands == 1)
    {
        NotifyIndirectShRegWrite<true>(regAddr, numRegsToInvalidate);
    }
    else
    {
        NotifyIndirectShRegWrite<false>(regAddr, numRegsToInvalidate);
    }
}

// =====================================================================================================================
// If immediate mode optimizations are active, tell the optimizer to invalidate its copy of this particular SH register.
template <bool Pm4OptEnabled>
void CmdStream::NotifyIndirectShRegWrite(
    uint32 regAddr,
    uint32 numRegsToInvalidate)
{
    if (Pm4OptEnabled)
    {
        if (regAddr != UserDataNotMapped)
        {
            for (uint32 x = 0; x < numRegsToInvalidate; x++)
            {
                m_pPm4Optimizer->SetShRegInvalid(regAddr + x);
            }
        }
    }
}

template
void CmdStream::NotifyIndirectShRegWrite<true>(
    uint32 regAddr,
    uint32 numRegsToInvalidate);
template
void CmdStream::NotifyIndirectShRegWrite<false>(
    uint32 regAddr,
    uint32 numRegsToInvalidate);

// =====================================================================================================================
size_t CmdStream::BuildCondIndirectBuffer(
    CompareFunc compareFunc,
    gpusize     compareGpuAddr,
    uint64      data,
    uint64      mask,
    uint32*     pPacket
    ) const
{
    return m_cmdUtil.BuildCondIndirectBuffer(compareFunc, compareGpuAddr, data, mask, pPacket);
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
    return m_cmdUtil.BuildIndirectBuffer(GetEngineType(),
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
void CmdStream::BeginCurrentChunk()
{
    // Allocate a preamble with enough space for a DMA_DATA packet. We will patch it to DMA the stream contents into
    // the gfxL2 to improve command fetch performance.
    if (m_flags.prefetchCommands)
    {
        m_pChunkPreamble = AllocCommandSpace(CmdUtil::DmaDataSizeDwords);
        m_cmdUtil.BuildNop(CmdUtil::DmaDataSizeDwords, m_pChunkPreamble);
    }
}

// =====================================================================================================================
// Ends the final command block in the current chunk and inserts a chaining packet to chain that block to so other
// command block (perhaps in an external command stream at submit time).
void CmdStream::EndCurrentChunk(
    bool atEndOfStream)
{
    // The body of the old command block is complete so we can end it. Our block postamble is a basic chaining packet.
    uint32*const pChainPacket = EndCommandBlock(m_chainIbSpaceInDwords, true);

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
            m_cmdUtil.BuildNop(m_chainIbSpaceInDwords, pChainPacket);
            AddChainPatch(ChainPatchType::IndirectBuffer, pChainPacket);
        }
    }

    // Patch the preamble to DMA the command data into L2. We need to do this after EndCommandBlock to make sure the
    // postamble is included in the DMA range.
    if (m_pChunkPreamble != nullptr)
    {
        DmaDataInfo dmaInfo = {};
        dmaInfo.srcAddr      = m_chunkList.Back()->GpuVirtAddr();
        dmaInfo.srcAddrSpace = sas__pfp_dma_data__memory;
        dmaInfo.srcSel       = src_sel__pfp_dma_data__src_addr_using_l2;
        dmaInfo.dstSel       = dst_sel__pfp_dma_data__dst_nowhere;
        dmaInfo.numBytes     = m_chunkList.Back()->DwordsAllocated() * sizeof(uint32);
        dmaInfo.usePfp       = (GetEngineType() == EngineTypeUniversal);
        dmaInfo.disWc        = true;

        m_cmdUtil.BuildDmaData<false, false>(dmaInfo, m_pChunkPreamble);
        m_pChunkPreamble = nullptr;
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
    if (m_cmdUtil.IsUserConfigReg(regAddr) == false)
    {
        PAL_ASSERT(CmdUtil::CanUseCopyDataRegOffset(regAddr));

        pCmdSpace += m_cmdUtil.BuildCopyData(GetEngineType(),
                                             engine_sel__me_copy_data__micro_engine,
                                             dst_sel__me_copy_data__perfcounters,
                                             regAddr,
                                             src_sel__me_copy_data__immediate_data,
                                             value,
                                             count_sel__me_copy_data__32_bits_of_data,
                                             wr_confirm__me_copy_data__wait_for_confirmation,
                                             pCmdSpace);
    }
    // Use a normal SET_DATA command for normal user-config registers. The resetFilterCam bit is not valid for the
    // Compute Micro Engine. Setting this bit would cause a hang in compute-only engines.
    else if (GetEngineType() == EngineTypeUniversal)
    {
        pCmdSpace = WriteSetOneConfigReg<true>(regAddr, value, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetOneConfigReg<false>(regAddr, value, pCmdSpace);
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

    pCmdSpace += m_cmdUtil.BuildCopyData(GetEngineType(),
                                         engine_sel__me_copy_data__micro_engine,
                                         dst_sel__me_copy_data__tc_l2,
                                         dstGpuVa,
                                         src_sel__me_copy_data__perfcounters,
                                         srcReg,
                                         count_sel__me_copy_data__32_bits_of_data,
                                         wr_confirm__me_copy_data__wait_for_confirmation,
                                         pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Writes a clear state packet and updates the PM4 optimizer if the clear-state packet restored state (i.e., blew
// away everything the PM4 optimizer thought was true).
uint32* CmdStream::WriteClearState(
    PFP_CLEAR_STATE_cmd_enum  clearMode,
    uint32*                   pCmdSpace)
{
    pCmdSpace += m_cmdUtil.BuildClearState(clearMode, pCmdSpace);

    if ((clearMode == cmd__pfp_clear_state__pop_state) && (m_pPm4Optimizer != nullptr))
    {
        // We just destroyed all the state, reset the pm4 optimizer
        m_pPm4Optimizer->Reset();
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Marks current PM4 optimizer state as invalid. This is expected to be called after nested command buffer execute.
void CmdStream::NotifyNestedCmdBufferExecute()
{
    if (m_flags.optimizeCommands == 1)
    {
        // The command buffer PM4 optimizer has no knowledge of nested command buffer state.
        // Reset PM4 optimizer state so that subsequent PM4 state does not get incorrectly optimized out.
        m_pPm4Optimizer->Reset();
    }
}

#if PAL_DEVELOPER_BUILD
// =====================================================================================================================
// Calls the PAL developer callback to issue a report on how many times SET packets to each SH and context register were
// seen by the optimizer and kept after redundancy checking.
void CmdStream::IssueHotRegisterReport(
    GfxCmdBuffer* pCmdBuf
    ) const
{
    if (m_pPm4Optimizer != nullptr)
    {
        m_pPm4Optimizer->IssueHotRegisterReport(pCmdBuf);
    }
}
#endif

// =====================================================================================================================
// Writes PERF_COUNTER_WINDOW pm4 packet and tracks state to protect from accidentally missing a window configuration
uint32* CmdStream::WritePerfCounterWindow(
    bool    enableWindow,
    uint32* pCmdSpace)
{
    m_perfCounterWindowEnabled = enableWindow;

    if (IsGfx11(*m_pDevice))
    {
        pCmdSpace += m_cmdUtil.BuildPerfCounterWindow(GetEngineType(), enableWindow, pCmdSpace);
    }

    return pCmdSpace;
}

} // Gfx9
} // Pal
