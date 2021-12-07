/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
                 GetChainSizeInDwords(device, engineType, isNested),
                 CmdUtil::MinNopSizeInDwords,
                 CmdUtil::CondIndirectBufferSize,
                 isNested),
    m_cmdUtil(device.CmdUtil()),
    m_pPm4Optimizer(nullptr),
    m_pChunkPreamble(nullptr),
    m_contextRollDetected(false)
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
        flags.optimizeCommands = false;
        flags.prefetchCommands = false;
    }
    else
    {
        // We can't enable PM4 optimization without an allocator because we need to dynamically allocate a Pm4Optimizer.
        flags.optimizeCommands &= (pMemAllocator != nullptr);

        // We may want to modify prefetchCommands based on this setting.
        switch (static_cast<const Gfx9::Device&>(m_device).Settings().prefetchCommandBuffers)
        {
        case Gfx9PrefetchCommandsDisabled:
            flags.prefetchCommands = false;
            break;

        case Gfx9PrefetchCommandsBuildInfo:
            // The prefetchCommands flag was set according to the client's command buffer build info.
            // However, we really should force prefetching off if the command data is in local memory because:
            // 1. Local memory is fast enough that cold reads are no problem. Prefetching the whole command chunk
            //    ahead of time might evict things from the L2 cache that we need right now, hurting performance.
            // 2. We try to use the uncached MTYPE when allocating local memory for command data. This avoids any
            //    L2 cache pollution but also makes prefetching completely useless because it only prefetches to L2.
            if (m_pCmdAllocator->LocalCommandData())
            {
                flags.prefetchCommands = false;
            }
            break;

        case Gfx9PrefetchCommandsForceAllDe:
            flags.prefetchCommands = (GetEngineType() == EngineTypeUniversal);
            break;

        case Gfx9PrefetchCommandsForceAllDeAce:
            flags.prefetchCommands = true;
            break;

        default:
            PAL_ASSERT_ALWAYS();
            break;
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
    m_pChunkPreamble      = nullptr;
    m_contextRollDetected = false;

    GfxCmdStream::Reset(pNewAllocator, returnGpuMemory);
}

// =====================================================================================================================
// Helper function for determining the command buffer chain size (in DWORD's). Early versions of the CP microcode did
// not properly support IB2 chaining, so we need to check the ucode version before enabling chaining for IB2's.
uint32 CmdStream::GetChainSizeInDwords(
    const Device& device,
    EngineType    engineType,
    bool          isNested) const
{
    const Pal::Device* pPalDevice = device.Parent();
    uint32              chainSize = CmdUtil::ChainSizeInDwords(engineType);

    constexpr uint32 UcodeVersionWithIb2ChainingFix = 31;
    if (isNested &&
        (pPalDevice->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9) &&
        (pPalDevice->EngineProperties().cpUcodeVersion < UcodeVersionWithIb2ChainingFix))
    {
        // Disable chaining for nested command buffers if the microcode version does not support the IB2 chaining fix.
        chainSize = 0;
    }

    return chainSize;
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
        m_contextRollDetected = true;
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
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetVgtLsHsConfig(
    regVGT_LS_HS_CONFIG vgtLsHsConfig,
    uint32*             pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetContextReg(mmVGT_LS_HS_CONFIG, vgtLsHsConfig.u32All))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneContextReg(
                                        mmVGT_LS_HS_CONFIG,
                                        pCmdSpace,
                                        index__pfp_set_context_reg_index__vgt_ls_hs_config__GFX09);
        m_contextRollDetected = true;
        pCmdSpace[CmdUtil::ContextRegSizeDwords] = vgtLsHsConfig.u32All;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetVgtLsHsConfig<true>(
    regVGT_LS_HS_CONFIG vgtLsHsConfig,
    uint32*             pCmdSpace);
template
uint32* CmdStream::WriteSetVgtLsHsConfig<false>(
    regVGT_LS_HS_CONFIG vgtLsHsConfig,
    uint32*             pCmdSpace);

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
    PFP_SET_UCONFIG_REG_INDEX_index_enum index)
{
    const size_t totalDwords = m_cmdUtil.BuildSetOneConfigReg<isPerfCtr>(regAddr, pCmdSpace, index);
    pCmdSpace[CmdUtil::ConfigRegSizeDwords] = regData;

    return pCmdSpace + totalDwords;
}

template
uint32* CmdStream::WriteSetOneConfigReg<true>(
    uint32                               regAddr,
    uint32                               regData,
    uint32*                              pCmdSpace,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index);
template
uint32* CmdStream::WriteSetOneConfigReg<false>(
    uint32                               regAddr,
    uint32                               regData,
    uint32*                              pCmdSpace,
    PFP_SET_UCONFIG_REG_INDEX_index_enum index);

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
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetOneContextReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetContextReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneContextReg(regAddr, pCmdSpace);

        pCmdSpace[CmdUtil::ContextRegSizeDwords] = regData;
        pCmdSpace += totalDwords;
        m_contextRollDetected = true;

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
    m_contextRollDetected = true;

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
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

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
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetOneShRegIndex(
    uint32                          regAddr,
    uint32                          regData,
    Pm4ShaderType                   shaderType,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace)
{
    if ((m_flags.optimizeCommands == 0) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneShRegIndex(regAddr, shaderType, index, pCmdSpace);

        pCmdSpace[CmdUtil::ShRegSizeDwords] = regData;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    Pm4ShaderType shaderType,
    const void*   pData,
    uint32*       pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        PM4_ME_SET_SH_REG setData;
        m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setData,
                                                                static_cast<const uint32*>(pData),
                                                                pCmdSpace);
    }
    else
    {
        const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pCmdSpace);

        memcpy(&pCmdSpace[CmdUtil::ShRegSizeDwords],
               pData,
               (totalDwords - CmdUtil::ShRegSizeDwords) * sizeof(uint32));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegsIndex(
    uint32                          startRegAddr,
    uint32                          endRegAddr,
    Pm4ShaderType                   shaderType,
    const void*                     pData,
    PFP_SET_SH_REG_INDEX_index_enum index,
    uint32*                         pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        PM4_ME_SET_SH_REG setData;
        m_cmdUtil.BuildSetSeqShRegsIndex(startRegAddr, endRegAddr, shaderType, index, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setData,
                                                                static_cast<const uint32*>(pData),
                                                                pCmdSpace);
    }
    else
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
// Helper function for writing the user-SGPR's mapped to user-data entries for a graphics or compute shader stage.
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
// Helper function for writing the user-SGPR's mapped to user-data entries for a graphics or compute shader stage.
template <bool IgnoreDirtyFlags, Pm4ShaderType shaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteUserDataEntriesToSgprs(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace)
{
    // Virtualized user-data entries are always remapped to a consecutive sequence of user-SGPR's.  Because of this
    // mapping, we can always assume that this operation will result in a series of zero or more consecutive registers
    // being written, except in the case where we skip entries which aren't dirty (i.e., IgnoreDirtyFlags is false).
    const uint16 firstUserSgpr = entryMap.firstUserSgprRegAddr;
    const uint16 userSgprCount = entryMap.userSgprCount;

    uint32 scratchMem[NumUserDataRegisters - FastUserDataStartReg];
    uint32* pCmdPayload = Pm4OptEnabled ? scratchMem : (pCmdSpace + CmdUtil::ShRegSizeDwords);

    if (IgnoreDirtyFlags)
    {
        if (userSgprCount != 0)
        {
            for (uint16 sgpr = 0; sgpr < userSgprCount; ++sgpr)
            {
                pCmdPayload[sgpr] = entries.entries[entryMap.mappedEntry[sgpr]];
            }

            if (Pm4OptEnabled)
            {
                PM4_ME_SET_SH_REG setShReg;
                m_cmdUtil.BuildSetSeqShRegs(firstUserSgpr,
                                            (firstUserSgpr + userSgprCount - 1),
                                            shaderType,
                                            &setShReg);

                pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg, pCmdPayload, pCmdSpace);
            }
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
                if (Pm4OptEnabled)
                {
                    PM4_ME_SET_SH_REG setShReg;
                    m_cmdUtil.BuildSetSeqShRegs(packetFirstSgpr,
                                                (packetFirstSgpr + packetSgprCount - 1),
                                                shaderType,
                                                &setShReg);

                    pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg, pCmdPayload, pCmdSpace);
                }
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

// =====================================================================================================================
// Builds a PM4 packet to load a single group of consecutive context registers from an indirect video memory offset.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteLoadSeqContextRegs(
    uint32      startRegAddr,
    uint32      regCount,
    gpusize     dataVirtAddr,
    uint32*     pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    // In gfx9+, PM4_PFP_LOAD_CONTEXT_REG_INDEX is always supported.
    const size_t packetSizeDwords = m_cmdUtil.BuildLoadContextRegsIndex<true>(
                                    dataVirtAddr,
                                    startRegAddr,
                                    regCount,
                                    pCmdSpace);
    if (Pm4OptEnabled)
    {
        m_pPm4Optimizer->HandleLoadContextRegsIndex(
                reinterpret_cast<const PM4_PFP_LOAD_CONTEXT_REG_INDEX&>(*pCmdSpace));
    }
    pCmdSpace += packetSizeDwords;

    return pCmdSpace;
}

template
uint32* CmdStream::WriteLoadSeqContextRegs<true>(
    uint32      startRegAddr,
    uint32      regCount,
    gpusize     dataVirtAddr,
    uint32*     pCmdSpace);
template
uint32* CmdStream::WriteLoadSeqContextRegs<false>(
    uint32      startRegAddr,
    uint32      regCount,
    gpusize     dataVirtAddr,
    uint32*     pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteLoadSeqContextRegs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteLoadSeqContextRegs(
    uint32      startRegAddr,
    uint32      regCount,
    gpusize     dataVirtAddr,
    uint32*     pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        pCmdSpace = WriteLoadSeqContextRegs<true>(startRegAddr, regCount, dataVirtAddr, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteLoadSeqContextRegs<false>(startRegAddr, regCount, dataVirtAddr, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetSeqContextRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if (Pm4OptEnabled)
    {
        PM4_PFP_SET_CONTEXT_REG setData;
        m_cmdUtil.BuildSetSeqContextRegs(startRegAddr, endRegAddr, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqContextRegs(setData,
                                                                     &m_contextRollDetected,
                                                                     static_cast<const uint32*>(pData),
                                                                     pCmdSpace);
    }
    else
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
// If immediate mode optimizations are active, tell the optimizer to invalidate its copy of this particular SH register.
void CmdStream::NotifyIndirectShRegWrite(
    uint32 regAddr)
{
    if (m_flags.optimizeCommands == 1)
    {
        m_pPm4Optimizer->SetShRegInvalid(regAddr);
    }
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
    return m_cmdUtil.BuildCondIndirectBuffer(
        compareFunc, compareGpuAddr, data, mask, (m_subEngineType == SubEngineType::ConstantEngine), pPacket);
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
                                         (m_subEngineType == SubEngineType::ConstantEngine),
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
        dmaInfo.usePfp       = true;

        m_cmdUtil.BuildDmaData(dmaInfo, m_pChunkPreamble);
        m_pChunkPreamble = nullptr;
    }
}

// =====================================================================================================================
// Writes a register for performance counters. (Some performance counter reg's are protected and others aren't). Returns
// the size of the PM4 command written, in DWORDs.
uint32* CmdStream::WriteSetOnePerfCtrReg(
    uint32  regAddr,
    uint32  value,
    uint32* pCmdSpace) // [out] Build the PM4 packet in this buffer.
{
    uint32* pReturnVal = nullptr;

    if (m_cmdUtil.IsUserConfigReg(regAddr) == false)
    {
        // Protected register: use our COPY_DATA backdoor to write the register.
        pReturnVal = WriteSetOnePrivilegedConfigReg(regAddr, value, pCmdSpace);
    }
    else
    {
        // Non-protected register: use a normal SET_DATA command.
        if (m_device.Parent()->ChipProperties().gfxLevel != GfxIpLevel::GfxIp9)
        {
            const auto engineType = GetEngineType();

            // The resetFilterCam bit is not valid for the Compute Micro Engine. Setting this bit would cause a hang in
            // compute-only engines.
            if (engineType == EngineTypeUniversal)
            {
                pReturnVal = WriteSetOneConfigReg<true>(regAddr, value, pCmdSpace);
            }
            else
            {
                pReturnVal = WriteSetOneConfigReg<false>(regAddr, value, pCmdSpace);
            }
        }
        else
        {
            pReturnVal = WriteSetOneConfigReg<false>(regAddr, value, pCmdSpace);
        }
    }

    return pReturnVal;
}

// =====================================================================================================================
// Writes a config register using a COPY_DATA packet. This is a back-door we have to write privileged registers which
// cannot be set using a SET_DATA packet. Returns the size of the PM4 command written, in DWORDs.
uint32* CmdStream::WriteSetOnePrivilegedConfigReg(
    uint32     regAddr,
    uint32     value,
    uint32*    pCmdSpace) // [out] Build the PM4 packet in this buffer.
{
    // We must use the perfcounters select if the target isn't a user config register.
    const ME_COPY_DATA_dst_sel_enum dstSelect = (m_cmdUtil.IsUserConfigReg(regAddr)
                                                 ? dst_sel__me_copy_data__mem_mapped_register
                                                 : dst_sel__me_copy_data__perfcounters);

    // Assert that our register address will fit in the COPY_DATA packet.
    PAL_ASSERT(CmdUtil::CanUseCopyDataRegOffset(regAddr));

    return pCmdSpace + m_cmdUtil.BuildCopyData(GetEngineType(),
                                               engine_sel__me_copy_data__micro_engine,
                                               dstSelect,
                                               regAddr,
                                               src_sel__me_copy_data__immediate_data,
                                               value,
                                               count_sel__me_copy_data__32_bits_of_data,
                                               wr_confirm__me_copy_data__do_not_wait_for_confirmation,
                                               pCmdSpace);
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

// =====================================================================================================================
// Resets any draw time state in the CmdStream or the Pm4Optimizer.
void CmdStream::ResetDrawTimeState()
{
    m_contextRollDetected = false;

    if (m_flags.optimizeCommands == 1)
    {
        m_pPm4Optimizer->ResetContextRollState();
    }
}

// =====================================================================================================================
// Sets context roll detected state to true if a context roll occurred
template <bool canBeOptimized>
void CmdStream::SetContextRollDetected()
{
    // If the context roll is due to a context register write, the PM4 optimizer may eliminate it. If the context roll
    // is due to an ACQUIRE_MEM, it should not be affected by the PM4 optimizer.
    if ((m_flags.optimizeCommands == 1) && canBeOptimized)
    {
        m_contextRollDetected |= m_pPm4Optimizer->GetContextRollState();
    }
    else
    {
        m_contextRollDetected = true;
    }
}

template
void CmdStream::SetContextRollDetected<true>();
template
void CmdStream::SetContextRollDetected<false>();

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
// Allows the caller to temporarily disable the PM4 optimizer if some PM4 must be written.
void CmdStream::TempSetPm4OptimizerMode(
    bool isEnabled)
{
    if (m_pPm4Optimizer != nullptr)
    {
        m_pPm4Optimizer->TempSetPm4OptimizerMode(isEnabled);
    }
}

// =====================================================================================================================
uint32* CmdStream::WriteDynamicLaunchDesc(
    gpusize     launchDescGpuVa,
    uint32*     pCmdSpace)
{
    if (m_cmdUtil.HasEnhancedLoadShRegIndex())
    {
        pCmdSpace += m_cmdUtil.BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__indirect_addr__GFX103COREPLUS,
                                                    data_format__pfp_load_sh_reg_index__offset_and_data,
                                                    launchDescGpuVa,
                                                    0,
                                                    DynamicCsLaunchDescRegCount,
                                                    Pm4ShaderType::ShaderCompute,
                                                    pCmdSpace);
    }
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    if (m_pPm4Optimizer != nullptr)
    {
        m_pPm4Optimizer->HandleDynamicLaunchDesc();
    }

    return pCmdSpace;
}
} // Gfx9
} // Pal
