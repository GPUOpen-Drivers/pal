/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6Pm4Optimizer.h"
#include "palDequeImpl.h"
#include "palLinearAllocator.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// Helper function for determining the command buffer chain size (in DWORD's). This value can be affected by workarounds
// for hardware issues.
static PAL_INLINE uint32 GetChainSizeInDwords(
    const Device& device,
    bool          isNested)
{
    uint32 chainSize = CmdUtil::GetChainSizeInDwords();

    if (isNested && (device.WaCpIb2ChainingUnsupported() != false))
    {
        // Some GPU's do not support chaining between the chunks of an IB2. This means that we cannot use chaining
        // for nested command buffers on these chips.  When executing a nested command buffer using IB2's on these
        // GPU's, we will use a separate IB2 packet for each chunk rather than issuing a single IB2 for the head
        // chunk.
        chainSize = 0;
    }

    return chainSize;
}

// =====================================================================================================================
CmdStream::CmdStream(
    const Device&  device,
    ICmdAllocator* pCmdAllocator,
    EngineType     engineType,
    SubEngineType  subEngineType,
    CmdStreamUsage cmdStreamUsage,
    bool           isNested)
    :
    Pal::GfxCmdStream(device,
                      pCmdAllocator,
                      engineType,
                      subEngineType,
                      cmdStreamUsage,
                      GetChainSizeInDwords(device, isNested),
                      device.CmdUtil().GetMinNopSizeInDwords(),
                      CmdUtil::GetCondIndirectBufferSize(),
                      isNested),
    m_cmdUtil(device.CmdUtil()),
    m_pPm4Optimizer(nullptr)
{
}

// =====================================================================================================================
Result CmdStream::Begin(
    CmdStreamBeginFlags     flags,
    VirtualLinearAllocator* pMemAllocator)
{
    // We simply can't enable PM4 optimization without an allocator because we need to dynamically allocate a
    // Pm4Optimizer. We also shouldn't optimize CE streams because Pm4Optimizer has no optimizations for them.
    flags.optimizeCommands &= (pMemAllocator != nullptr) && (m_subEngineType != SubEngineType::ConstantEngine);

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
    if (m_flags.optimizeCommands == 0)
    {
        pCmdSpace = WriteContextRegRmw<false>(regAddr, regMask, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteContextRegRmw<true>(regAddr, regMask, regData, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteSetIaMultiVgtParam(
    regIA_MULTI_VGT_PARAM iaMultiVgtParam,
    uint32*               pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) ||
        m_pPm4Optimizer->MustKeepSetContextReg(mmIA_MULTI_VGT_PARAM, iaMultiVgtParam.u32All))
    {
        const size_t totalDwords =
            m_cmdUtil.BuildSetOneContextReg(mmIA_MULTI_VGT_PARAM, pCmdSpace, SET_CONTEXT_INDEX_MULTI_VGT_PARAM);

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = iaMultiVgtParam.u32All;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteSetIaMultiVgtParam<true>(
    regIA_MULTI_VGT_PARAM iaMultiVgtParam,
    uint32*               pCmdSpace);

template
uint32* CmdStream::WriteSetIaMultiVgtParam<false>(
    regIA_MULTI_VGT_PARAM iaMultiVgtParam,
    uint32*               pCmdSpace);

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
        const size_t totalDwords =
            m_cmdUtil.BuildSetOneContextReg(mmVGT_LS_HS_CONFIG, pCmdSpace, SET_CONTEXT_INDEX_VGT_LS_HS_CONFIG);

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = vgtLsHsConfig.u32All;
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
// Builds a PM4 packet to the given register and returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetPaScRasterConfig(
    regPA_SC_RASTER_CONFIG paScRasterConfig,
    uint32*             pCmdSpace)
{
    if (m_device.Parent()->ChipProperties().gfx6.rbReconfigureEnabled)
    {
        const size_t totalDwords =
            m_cmdUtil.BuildSetOneContextReg(mmPA_SC_RASTER_CONFIG, pCmdSpace, SET_CONTEXT_INDEX_PA_SC_RASTER_CONFIG);

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = paScRasterConfig.u32All;
        pCmdSpace += totalDwords;
    }
    else
    {
        pCmdSpace = WriteSetOneContextReg(mmPA_SC_RASTER_CONFIG,
                                          paScRasterConfig.u32All,
                                          pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetOneConfigReg(
    uint32  regAddr,
    uint32  regData,
    uint32* pCmdSpace)
{
    const size_t totalDwords = m_cmdUtil.BuildSetOneConfigReg(regAddr, pCmdSpace);
    pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = regData;

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

    memcpy(&pCmdSpace[PM4_CMD_SET_DATA_DWORDS], pData, totalDwords * sizeof(uint32) - sizeof(PM4CMDSETDATA));

    return (pCmdSpace + totalDwords);
}

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

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = regData;
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
    if (m_flags.optimizeCommands == 0)
    {
        pCmdSpace = WriteSetOneContextReg<false>(regAddr, regData, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteSetOneContextReg<true>(regAddr, regData, pCmdSpace);
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

    pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = regData;
    pCmdSpace += totalDwords;

    return pCmdSpace;
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
    uint32  regAddr,
    uint32  value,
    uint32* pCmdSpace) // [out] Build the PM4 packet in this buffer.
{
    // Note: On Gfx7+, all privileged registers need to be written with the DST_SYS_PERF_COUNTER dest-select. On Gfx6,
    // only certain MC registers require this.
    const uint32 dstSelect = (m_cmdUtil.IsPrivilegedConfigReg(regAddr)
                                ? COPY_DATA_SEL_DST_SYS_PERF_COUNTER
                                : COPY_DATA_SEL_REG);

    return pCmdSpace + m_cmdUtil.BuildCopyData(dstSelect,
                                               regAddr,
                                               COPY_DATA_SEL_SRC_IMME_DATA,
                                               value,
                                               COPY_DATA_SEL_COUNT_1DW,
                                               COPY_DATA_ENGINE_ME,
                                               COPY_DATA_WR_CONFIRM_NO_WAIT,
                                               pCmdSpace);
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <PM4ShaderType shaderType, bool Pm4OptEnabled>
uint32* CmdStream::WriteSetOneShReg(
    uint32        regAddr,
    uint32        regData,
    uint32*       pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);

    if ((Pm4OptEnabled == false) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneShReg(regAddr, shaderType, pCmdSpace);

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = regData;
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
template <PM4ShaderType shaderType>
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
    uint32        regAddr,
    uint32        regData,
    PM4ShaderType shaderType,
    uint32        index,
    uint32*       pCmdSpace)
{
    if ((m_flags.optimizeCommands == 0) || m_pPm4Optimizer->MustKeepSetShReg(regAddr, regData))
    {
        const size_t totalDwords = m_cmdUtil.BuildSetOneShRegIndex(regAddr, shaderType, index, pCmdSpace);

        pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = regData;
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to load a single group of consecutive context registers from an indirect video memory offset.
// Returns a pointer to the next unused DWORD in pCmdSpace.
template <bool Pm4OptEnabled>
uint32* CmdStream::WriteLoadSeqContextRegs(
    bool    useIndexVersion,
    uint32  startRegAddr,
    uint32  regCount,
    gpusize dataVirtAddr,
    uint32* pCmdSpace)
{
    PAL_ASSERT(m_flags.optimizeCommands == Pm4OptEnabled);
    PAL_ASSERT(useIndexVersion == (m_device.Parent()->ChipProperties().gfx6.supportLoadRegIndexPkt != false));

    if (useIndexVersion)
    {
        const size_t packetSizeDwords = m_cmdUtil.BuildLoadContextRegsIndex<true>(dataVirtAddr,
                                                                                  startRegAddr,
                                                                                  regCount,
                                                                                  pCmdSpace);
        if (Pm4OptEnabled)
        {
            m_pPm4Optimizer->HandleLoadContextRegsIndex(reinterpret_cast<const PM4CMDLOADDATAINDEX&>(*pCmdSpace));
        }
        pCmdSpace += packetSizeDwords;
    }
    else
    {
        uint32 regOffset  = startRegAddr - CONTEXT_SPACE_START;
        dataVirtAddr     -= (sizeof(uint32) * regOffset);

        const size_t packetSizeDwords = m_cmdUtil.BuildLoadContextRegs(dataVirtAddr,
                                                                       startRegAddr,
                                                                       regCount,
                                                                       pCmdSpace);
        if (Pm4OptEnabled)
        {
            m_pPm4Optimizer->HandleLoadContextRegs(reinterpret_cast<const PM4CMDLOADDATA&>(*pCmdSpace));
        }
        pCmdSpace += packetSizeDwords;
    }

    return pCmdSpace;
}

template
uint32* CmdStream::WriteLoadSeqContextRegs<true>(
    bool    useIndexVersion,
    uint32  startRegAddr,
    uint32  regCount,
    gpusize dataVirtAddr,
    uint32* pCmdSpace);
template
uint32* CmdStream::WriteLoadSeqContextRegs<false>(
    bool    useIndexVersion,
    uint32  startRegAddr,
    uint32  regCount,
    gpusize dataVirtAddr,
    uint32* pCmdSpace);

// =====================================================================================================================
// Wrapper for the real WriteLoadSeqContextRegs() for when the caller doesn't know if the immediate mode pm4 optimizer
// is enabled.
uint32* CmdStream::WriteLoadSeqContextRegs(
    bool    useIndexVersion,
    uint32  startRegAddr,
    uint32  regCount,
    gpusize dataVirtAddr,
    uint32* pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        pCmdSpace = WriteLoadSeqContextRegs<true>(useIndexVersion, startRegAddr, regCount, dataVirtAddr, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteLoadSeqContextRegs<false>(useIndexVersion, startRegAddr, regCount, dataVirtAddr, pCmdSpace);
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
        PM4CMDSETDATA setData;
        m_cmdUtil.BuildSetSeqContextRegs(startRegAddr, endRegAddr, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqContextRegs(setData,
                                                                     static_cast<const uint32*>(pData),
                                                                     pCmdSpace);
    }
    else
    {
        const size_t totalDwords = m_cmdUtil.BuildSetSeqContextRegs(startRegAddr, endRegAddr, pCmdSpace);

        memcpy(&pCmdSpace[PM4_CMD_SET_DATA_DWORDS], pData, totalDwords * sizeof(uint32) - sizeof(PM4CMDSETDATA));
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
    if (m_flags.optimizeCommands == 1)
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
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegs(
    uint32        startRegAddr,
    uint32        endRegAddr,
    PM4ShaderType shaderType,
    const void*   pData,
    uint32*       pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        PM4CMDSETDATA setData;
        m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setData,
                                                                static_cast<const uint32*>(pData),
                                                                pCmdSpace);
    }
    else
    {
        const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(startRegAddr, endRegAddr, shaderType, pCmdSpace);

        memcpy(&pCmdSpace[PM4_CMD_SET_DATA_DWORDS], pData, totalDwords * sizeof(uint32) - sizeof(PM4CMDSETDATA));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given registers unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetSeqShRegsIndex(
    uint32        startRegAddr,
    uint32        endRegAddr,
    PM4ShaderType shaderType,
    const void*   pData,
    uint32        index,
    uint32*       pCmdSpace)
{
    if (m_flags.optimizeCommands == 1)
    {
        PM4CMDSETDATA setData;
        m_cmdUtil.BuildSetSeqShRegsIndex(startRegAddr, endRegAddr, shaderType, index, &setData);

        pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setData,
                                                                static_cast<const uint32*>(pData),
                                                                pCmdSpace);
    }
    else
    {
        const size_t totalDwords =
            m_cmdUtil.BuildSetSeqShRegsIndex(startRegAddr, endRegAddr, shaderType, index, pCmdSpace);

        memcpy(&pCmdSpace[PM4_CMD_SET_DATA_DWORDS], pData, totalDwords * sizeof(uint32) - sizeof(PM4CMDSETDATA));
        pCmdSpace += totalDwords;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function for writing the user-SGPR's mapped to user-data entries for a graphics shader stage.
template <bool IgnoreDirtyFlags>
uint32* CmdStream::WriteUserDataEntriesToSgprsGfx(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace)
{
    if (m_flags.optimizeCommands != 0)
    {
        pCmdSpace = WriteUserDataEntriesToSgprsGfx<IgnoreDirtyFlags, true>(entryMap, entries, pCmdSpace);
    }
    else
    {
        pCmdSpace = WriteUserDataEntriesToSgprsGfx<IgnoreDirtyFlags, false>(entryMap, entries, pCmdSpace);
    }

    return pCmdSpace;
}

// Instantiate template for linker.
template
uint32* CmdStream::WriteUserDataEntriesToSgprsGfx<false>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace);
template
uint32* CmdStream::WriteUserDataEntriesToSgprsGfx<true>(
    const UserDataEntryMap& entryMap,
    const UserDataEntries&  entries,
    uint32*                 pCmdSpace);

// =====================================================================================================================
// Helper function for writing the user-SGPR's mapped to user-data entries for a graphics shader stage.
template <bool IgnoreDirtyFlags, bool Pm4OptEnabled>
uint32* CmdStream::WriteUserDataEntriesToSgprsGfx(
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
    uint32* pCmdPayload = Pm4OptEnabled ? scratchMem : (pCmdSpace + PM4_CMD_SET_DATA_DWORDS);

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
                PM4CMDSETDATA setShReg;
                m_cmdUtil.BuildSetSeqShRegs(firstUserSgpr,
                                            (firstUserSgpr + userSgprCount - 1),
                                            ShaderGraphics,
                                            &setShReg);

                pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg, pCmdPayload, pCmdSpace);
            }
            else
            {
                const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(firstUserSgpr,
                                                                       (firstUserSgpr + userSgprCount - 1),
                                                                       ShaderGraphics,
                                                                       pCmdSpace);
                // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
                PAL_ASSERT(totalDwords == (userSgprCount + PM4_CMD_SET_DATA_DWORDS));
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
                    PM4CMDSETDATA setShReg;
                    m_cmdUtil.BuildSetSeqShRegs(packetFirstSgpr,
                                                (packetFirstSgpr + packetSgprCount - 1),
                                                ShaderGraphics,
                                                &setShReg);

                    pCmdSpace = m_pPm4Optimizer->WriteOptimizedSetSeqShRegs(setShReg, pCmdPayload, pCmdSpace);
                }
                else
                {
                    const size_t totalDwords = m_cmdUtil.BuildSetSeqShRegs(packetFirstSgpr,
                                                                           (packetFirstSgpr + packetSgprCount - 1),
                                                                           ShaderGraphics,
                                                                           pCmdSpace);
                    // The packet is complete and will not be optimized, fix-up pCmdSpace and we're done.
                    PAL_ASSERT(totalDwords == (packetSgprCount + PM4_CMD_SET_DATA_DWORDS));
                    pCmdSpace   += totalDwords;
                    pCmdPayload += totalDwords;
                }
            }
        } // for each mapped user-SGPR
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Builds a PM4 packet to set the given register unless the PM4 optimizer indicates that it is redundant.
// Returns a pointer to the next unused DWORD in pCmdSpace.
uint32* CmdStream::WriteSetVgtPrimitiveType(
    regVGT_PRIMITIVE_TYPE vgtPrimitiveType,
    uint32*               pCmdSpace)
{
    const bool   isGfx7plus  = m_device.Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp7;
    const uint32 regAddr     = isGfx7plus ? mmVGT_PRIMITIVE_TYPE__CI__VI : mmVGT_PRIMITIVE_TYPE__SI;
    const size_t totalDwords = m_cmdUtil.BuildSetOneConfigReg(regAddr, pCmdSpace, SET_UCONFIG_INDEX_PRIM_TYPE);
    pCmdSpace[PM4_CMD_SET_DATA_DWORDS] = vgtPrimitiveType.u32All;

    return pCmdSpace + totalDwords;
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
// Inserts a conditional indirect buffer packet into the specified address
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
// Inserts an indirect buffer packet into the specified address
size_t CmdStream::BuildIndirectBuffer(
    gpusize  ibAddr,
    uint32   ibSize,
    bool     preemptionEnabled,
    bool     chain,
    uint32*  pPacket
    ) const
{
    return m_cmdUtil.BuildIndirectBuffer(
        ibAddr, ibSize, chain, (m_subEngineType == SubEngineType::ConstantEngine), preemptionEnabled, pPacket);
}

// =====================================================================================================================
// Update the address contained within indirect buffer packets associated with the current command block
void CmdStream::PatchCondIndirectBuffer(
    ChainPatch*  pPatch,
    gpusize      address,
    uint32       ibSizeDwords
    ) const
{
    PM4CMDCONDINDIRECTBUFFER* pCondIndirectBuffer = static_cast<PM4CMDCONDINDIRECTBUFFER*>(pPatch->pPacket);

    switch (pPatch->type)
    {
    case ChainPatchType::CondIndirectBufferPass:
        // The PM4 spec says that the first IB base/size are used if the conditional passes.
        pCondIndirectBuffer->ibBase1Lo = LowPart(address);
        pCondIndirectBuffer->ibBase1Hi = HighPart(address);
        pCondIndirectBuffer->ibSize1   = ibSizeDwords;
        break;

    case ChainPatchType::CondIndirectBufferFail:
        // The PM4 spec says that the second IB base/size are used if the conditional fails.
        pCondIndirectBuffer->ibBase2Lo = LowPart(address);
        pCondIndirectBuffer->ibBase2Hi = HighPart(address);
        pCondIndirectBuffer->ibSize2   = ibSizeDwords;
        break;

    default:
        // Other patch types should be handled by the base class
        PAL_ASSERT_ALWAYS();
        break;
    } // end switch
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

} // Gfx6
} // Pal
