/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Pm4Optimizer.h"
#include "core/hw/gfxip/gfx9/gfx9UserDataTableImpl.h"
#include "palLinearAllocator.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// Helper function for determining the command buffer chain size (in DWORD's). Early versions of the CP microcode did
// not properly support IB2 chaining, so we need to check the ucode version before enabling chaining for IB2's.
static PAL_INLINE uint32 GetChainSizeInDwords(
    const Device& device,
    EngineType    engineType,
    bool          isNested)
{
    const Pal::Device*  pPalDevice = device.Parent();
    uint32              chainSize  = CmdUtil::ChainSizeInDwords(engineType);

    constexpr uint32 UcodeVersionWithIb2ChainingFix = 31;
    if (isNested                                                      &&
        (pPalDevice->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9) &&
        (pPalDevice->EngineProperties().cpUcodeVersion < UcodeVersionWithIb2ChainingFix))
    {
        // Disable chaining for nested command buffers if the microcode version does not support the IB2 chaining fix.
        chainSize = 0;
    }

    return chainSize;
}

// =====================================================================================================================
CmdStream::CmdStream(
    const Device&  device,
    ICmdAllocator* pCmdAllocator,
    EngineType     engineType,
    SubQueueType   subqueueType,
    bool           isNested,
    bool           disablePreemption)
    :
    GfxCmdStream(device,
                 pCmdAllocator,
                 engineType,
                 subqueueType,
                 GetChainSizeInDwords(device, engineType, isNested),
                 CmdUtil::MinNopSizeInDwords,
                 CmdUtil::CondIndirectBufferSize,
                 isNested,
                 disablePreemption),
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
    if (IsConstantEngine())
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
            // Don't modify the prefetchCommands flag, it came from the command buffer build info.
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
template <bool pm4OptImmediate>
uint32* CmdStream::WriteContextRegRmw(
    uint32  regAddr,
    uint32  regMask,
    uint32  regData,
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if ((pm4OptImmediate == false) || m_pPm4Optimizer->MustKeepContextRegRmw(regAddr, regMask, regData))
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
    if (m_flags.optModeImmediate)
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
// Copies the given PM4 image into the given buffer. The PM4 optimizer may strip out extra packets.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WritePm4Image(
    size_t      sizeInDwords,
    const void* pImage,
    uint32*     pCmdSpace)
{
    if (m_flags.optModeImmediate == 1)
    {
        uint32 optSize = static_cast<uint32>(sizeInDwords);
        m_contextRollDetected |= m_pPm4Optimizer->OptimizePm4Commands(static_cast<const uint32*>(pImage),
                                                                      pCmdSpace,
                                                                      &optSize);
        pCmdSpace += optSize;
    }
    else
    {
        // NOTE: We'll use other state tracking to determine whether a context roll occurred for non-immediate-mode
        //       optimizations.
        memcpy(pCmdSpace, pImage, sizeInDwords * sizeof(uint32));
        pCmdSpace += sizeInDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool pm4OptImmediate>
uint32* CmdStream::WriteSetVgtLsHsConfig(
    regVGT_LS_HS_CONFIG vgtLsHsConfig,
    uint32*             pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if ((pm4OptImmediate == false) ||
        m_pPm4Optimizer->MustKeepSetContextReg(mmVGT_LS_HS_CONFIG, vgtLsHsConfig.u32All))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneContextReg(mmVGT_LS_HS_CONFIG,
                                                                   pCmdSpace,
                                                                   index__pfp_set_context_reg__vgt_ls_hs_config);
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
uint32* CmdStream::WriteSetOneConfigReg(
    uint32                         regAddr,
    uint32                         regData,
    uint32*                        pCmdSpace,
    PFP_SET_UCONFIG_REG_index_enum index)
{
    const size_t totalDwords = m_cmdUtil.BuildSetOneConfigReg(regAddr, pCmdSpace, index);
    pCmdSpace[CmdUtil::ConfigRegSizeDwords] = regData;

    return pCmdSpace + totalDwords;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given set of sequential config registers.  Returns a pointer to the next unused DWORD
// in pCmdSpace.
uint32* CmdStream::WriteSetSeqConfigRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    const size_t totalDwords = m_cmdUtil.BuildSetSeqConfigRegs(startRegAddr, endRegAddr, pCmdSpace);

    memcpy(&pCmdSpace[CmdUtil::ConfigRegSizeDwords],
           pData,
           (totalDwords - CmdUtil::ConfigRegSizeDwords) * sizeof(uint32));

    return (pCmdSpace + totalDwords);
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool pm4OptImmediate>
uint32* CmdStream::WriteSetOneContextReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if ((pm4OptImmediate == false) || m_pPm4Optimizer->MustKeepSetContextReg(regAddr, regData))
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
    if (m_flags.optModeImmediate)
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
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType shaderType, bool pm4OptImmediate>
uint32* CmdStream::WriteSetOneShReg(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if ((pm4OptImmediate == false) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
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
    if (m_flags.optModeImmediate)
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
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    Pm4ShaderType shaderType,
    const void*   pData,
    uint32*       pCmdSpace)
{
    if (m_flags.optModeImmediate == 1)
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
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType shaderType, bool pm4OptImmediate>
uint32* CmdStream::WriteSetShRegDataOffset(
    uint32        regAddr,
    uint32        dataOffset,
    uint32*       pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if (pm4OptImmediate)
    {
        PM4PFP_SET_SH_REG_OFFSET setShRegOffset;

        const size_t totalDwords = m_cmdUtil.BuildSetShRegDataOffset(regAddr,
                                                                     dataOffset,
                                                                     shaderType,
                                                                     &setShRegOffset);
        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetShShRegOffset(setShRegOffset, totalDwords, pCmdSpace);
    }
    else
    {
        pCmdSpace += m_cmdUtil.BuildSetShRegDataOffset(regAddr,
                                                       dataOffset,
                                                       shaderType,
                                                       pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Wrapper for the real WriteSetShRegDataOffset() for when the caller doesn't know if the immediate pm4 optimizer is
// enabled.
template <Pm4ShaderType shaderType>
uint32* CmdStream::WriteSetShRegDataOffset(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace)
{
    if (m_flags.optModeImmediate)
    {
        pCmdSpace = WriteSetShRegDataOffset<shaderType, true>(regAddr, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetShRegDataOffset<shaderType, false>(regAddr, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate the template for the linker.
template
uint32* CmdStream::WriteSetShRegDataOffset<ShaderGraphics>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);
template
uint32* CmdStream::WriteSetShRegDataOffset<ShaderCompute>(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace);

// =====================================================================================================================
// Helper function to route to fast path for writing one user-data entry to an SPI register or to the general path for
// writing many.
uint32* CmdStream::WriteUserDataRegisters(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    Pm4ShaderType           shaderType,
    uint32*                 pCmdSpace)
{
    if (pUserDataArgs->entryCount == 1)
    {
        pCmdSpace = WriteUserDataRegistersOne(entryMap, pUserDataArgs, shaderType, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteUserDataRegistersMany(entryMap, pUserDataArgs, shaderType, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function to write one user-data entry which has been remapped to a SPI user-data register. Returns a
// pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteUserDataRegistersOne(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    Pm4ShaderType           shaderType,
    uint32*                 pCmdSpace)
{
    uint32 regAddr = entryMap.regAddr[pUserDataArgs->firstEntry];

    if (regAddr != UserDataNotMapped)
    {
        if (m_flags.optModeImmediate == 1)
        {
            PM4_ME_SET_SH_REG setShReg;

            const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(regAddr, regAddr, shaderType, &setShReg);
            PAL_ASSERT(totalDwords == (1 + CmdUtil::ShRegSizeDwords));

            pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg,
                                                                    &(pUserDataArgs->pEntryValues[0]),
                                                                    pCmdSpace);
        }
        else
        {
            uint32*const pCmdPayload = (pCmdSpace + CmdUtil::ShRegSizeDwords);

            const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(regAddr, regAddr, shaderType, pCmdSpace);
            PAL_ASSERT(totalDwords == (1 + CmdUtil::ShRegSizeDwords));

            pCmdPayload[0] = pUserDataArgs->pEntryValues[0];

            // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
            PAL_ASSERT(totalDwords == (1 + CmdUtil::ShRegSizeDwords));
            pCmdSpace += totalDwords;
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function to write a group of user-data entries which have been remapped to SPI user-data registers. Returns a
// pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteUserDataRegistersMany(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    Pm4ShaderType           shaderType,
    uint32*                 pCmdSpace)
{
    // Virtualized user-data entries are always remapped to a consecutive sequence of SPI user-data registers. Because
    // the entries are remapped to consecutive registers, we can always assume that this call will result in a sequence
    // of zero or more SPI registers being written.
    //
    // NOTE: We'll track the last register address written and the count of registers written rather than the starting
    // register address to prevent unnecessary branching in the loop below.

    uint32 firstEntry          = pUserDataArgs->firstEntry;
    uint32 entryCount          = pUserDataArgs->entryCount;
    const uint32* pEntryValues = pUserDataArgs->pEntryValues;

    uint32 endRegAddr = 0;
    uint32 count = 0;

    // This loop will copy all of the mapped user-data entries' values into the data buffer following the PM4 command
    // header.  If we are using the optimizer, we need to write into cacheable memory because the optimizer will read
    // from the data.

    uint32 scratchMem[MaxUserDataEntries];

    uint32*const pCmdPayload = (m_flags.optModeImmediate == 1) ? scratchMem : (pCmdSpace + CmdUtil::ShRegSizeDwords);
    for (uint32 e = 0; e < entryCount; ++e)
    {
        const uint32 currRegAddr = entryMap.regAddr[e + firstEntry];
        if (currRegAddr != UserDataNotMapped)
        {
            pCmdPayload[count] = pEntryValues[e];

            PAL_ASSERT((endRegAddr == 0) || (endRegAddr == (currRegAddr - 1)));
            endRegAddr = currRegAddr;
            ++count;
        }
    }

    if (count >= 1)
    {
        // If we copied any registers at all to the output buffer, we need to assemble the correct packet for setting
        // a group of sequential SPI user-data registers.

        if (m_flags.optModeImmediate == 1)
        {
            PM4_ME_SET_SH_REG setShReg;

            m_cmdUtil.BuildSetSeqShRegs((endRegAddr - count + 1), endRegAddr, shaderType, &setShReg);

            pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg, pCmdPayload, pCmdSpace);
        }
        else
        {
            const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs((endRegAddr - count + 1),
                                                                   endRegAddr,
                                                                   shaderType,
                                                                   pCmdSpace);

            // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
            PAL_ASSERT(totalDwords == (count + CmdUtil::ShRegSizeDwords));
            pCmdSpace += totalDwords;
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function to write one indirect user-data entry which has been remapped to a SPI user-data register. Returns a
// pointer to the next unused DWORD in pCmdSpace.
template <Pm4ShaderType shaderType>
uint32* CmdStream::WriteUserDataRegisterOffset(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    uint32*                 pCmdSpace)
{
    const uint32 regAddr = entryMap.regAddr[pUserDataArgs->firstEntry];

    if (regAddr != UserDataNotMapped)
    {
        if (m_flags.optModeImmediate == 1)
        {
            pCmdSpace = WriteSetShRegDataOffset<shaderType, true>(regAddr, pUserDataArgs->pEntryValues[0], pCmdSpace);
        }
        else
        {
            pCmdSpace = WriteSetShRegDataOffset<shaderType, false>(regAddr, pUserDataArgs->pEntryValues[0], pCmdSpace);
        }
    }

    return pCmdSpace;
}

// Instsantiate template for linker.
template
uint32* CmdStream::WriteUserDataRegisterOffset<ShaderGraphics>(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    uint32*                 pCmdSpace);

template
uint32* CmdStream::WriteUserDataRegisterOffset<ShaderCompute>(
    const UserDataEntryMap& entryMap,
    const UserDataArgs*     pUserDataArgs,
    uint32*                 pCmdSpace);

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool pm4OptImmediate>
uint32* CmdStream::WriteSetSeqContextRegs(
    uint32      startRegAddr,
    uint32      endRegAddr,
    const void* pData,
    uint32*     pCmdSpace)
{
    PAL_ASSERT(m_flags.optModeImmediate == pm4OptImmediate);

    if (pm4OptImmediate)
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
    if (m_flags.optModeImmediate)
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
// If immediate mode optimizations are active, tell the optimizer to invalidate its copy of this particular SH register.
void CmdStream::NotifyIndirectShRegWrite(
    uint32 regAddr)
{
    if (m_flags.optModeImmediate == 1)
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
    return m_cmdUtil.BuildCondIndirectBuffer(compareFunc, compareGpuAddr, data, mask, IsConstantEngine(), pPacket);
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
                                         IsConstantEngine(),
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
    PM4PFP_COND_INDIRECT_BUFFER* pPacket = static_cast<PM4PFP_COND_INDIRECT_BUFFER*>(pPatch->pPacket);

    switch (pPatch->type)
    {
    case ChainPatchType::CondIndirectBufferPass:
        // The PM4 spec says that the first IB base/size are used if the conditional passes.
        pPacket->ordinal9    = LowPart(address);
        pPacket->ib_base1_hi = HighPart(address);
        PAL_ASSERT (pPacket->bitfields9.reserved4 == 0);

        pPacket->bitfields11.ib_size1 = ibSizeDwords;
        break;

    case ChainPatchType::CondIndirectBufferFail:
        // The PM4 spec says that the second IB base/size are used if the conditional fails.
        pPacket->ordinal12   = LowPart(address);
        pPacket->ib_base2_hi = HighPart(address);
        PAL_ASSERT (pPacket->bitfields12.reserved7 == 0);

        pPacket->bitfields14.ib_size2 = ibSizeDwords;
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
    if (m_flags.optModeFinalized == 1)
    {
        // "Finalized" mode optimizations must be done now because we must know the final command size before we end the
        // current command block; otherwise we must patch chaining commands in the optimizer which will be hard.
        //
        // By accessing the chunk address and size directly, we implicitly assume that PM4 optimization will be disabled
        // whenever start placing multiple command blocks in a single command chunk.
        PAL_ASSERT(CmdBlockOffset() == 0);

        auto*const   pChunk   = m_chunkList.Back();
        uint32*const pCmdAddr = pChunk->GetRmwWriteAddr();
        uint32*const pCmdSize = pChunk->GetRmwUsedDwords();

        m_pPm4Optimizer->OptimizePm4Commands(pCmdAddr, pCmdAddr, pCmdSize);
    }

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
// Simply apply the generic PM4 image optimizer to the given commands. See the comment in cmdStream.h for more details.
bool CmdStream::OptimizedCommit(
    const uint32* pSrcBuffer,
    uint32*       pDstBuffer,
    uint32*       pNumDwords)
{
    m_pPm4Optimizer->OptimizePm4Commands(pSrcBuffer, pDstBuffer, pNumDwords);

    return true;
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

    if (m_cmdUtil.IsPrivilegedConfigReg(regAddr))
    {
        // Protected register: use our COPY_DATA backdoor to write the register.
        pReturnVal = WriteSetOnePrivilegedConfigReg(regAddr, value, pCmdSpace);
    }
    else
    {
        // Non-protected register: use a normal SET_DATA command.
        pReturnVal = WriteSetOneConfigReg(regAddr, value, pCmdSpace);
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
    // Note: On Gfx7+, all privileged registers need to be written with the DST_SYS_PERF_COUNTER dest-select. On Gfx6,
    // only certain MC registers require this.
    const ME_COPY_DATA_dst_sel_enum dstSelect = (m_cmdUtil.IsPrivilegedConfigReg(regAddr)
                                                 ? dst_sel__me_copy_data__perfcounters
                                                 : dst_sel__me_copy_data__mem_mapped_register);

    return pCmdSpace + m_cmdUtil.BuildCopyDataGraphics(engine_sel__me_copy_data__micro_engine,
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
    if (m_flags.optModeImmediate == 1)
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

    if (m_flags.optModeImmediate == 1)
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
    if ((m_flags.optModeImmediate == 1) && canBeOptimized)
    {
        m_contextRollDetected = m_pPm4Optimizer->GetContextRollState();
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

} // Gfx9
} // Pal
