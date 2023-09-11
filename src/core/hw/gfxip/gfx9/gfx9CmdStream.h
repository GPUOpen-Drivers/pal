/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/pm4CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{
class Pm4CmdBuffer;

namespace Gfx9
{

class CmdUtil;
class Device;
class Pm4Optimizer;

// =====================================================================================================================
// This is a specialization of CmdStream that has special knowledge of PM4 on GFX9 hardware. It implements conditional
// execution and chunk chaining. This class is also responsible for invoking the PM4 optimizer if it is enabled. Callers
// should use the "write" functions below when applicable as they may be necessary to hook into the PM4 optimizer.
class CmdStream final : public Pm4::CmdStream
{
public:
    CmdStream(
        const Device&  device,
        ICmdAllocator* pCmdAllocator,
        EngineType     engineType,
        SubEngineType  subEngineType,
        CmdStreamUsage cmdStreamUsage,
        bool           isNested);
    virtual ~CmdStream() {}

    virtual Result Begin(CmdStreamBeginFlags flags, Util::VirtualLinearAllocator* pMemAllocator) override;
    virtual void   Reset(CmdAllocator* pNewAllocator, bool returnGpuMemory) override;

    uint32 GetChainSizeInDwords(const Device& device, EngineType engineType, bool isNested) const;

    template <bool pm4OptImmediate>
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    template <bool isPerfCtr = false>
    uint32* WriteSetOneConfigReg(
        uint32                                regAddr,
        uint32                                regData,
        uint32*                               pCmdSpace,
        PFP_SET_UCONFIG_REG_INDEX_index_enum  index = index__pfp_set_uconfig_reg_index__default);

    template <bool pm4OptImmediate>
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextRegNoOpt(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    uint32* WriteSetOnePerfCtrReg(uint32 regAddr, uint32 value, uint32* pCmdSpace);
    uint32* WriteCopyPerfCtrRegToMemory(uint32 srcReg, gpusize dstGpuVa, uint32* pCmdSpace);

    template <Pm4ShaderType shaderType, bool pm4OptImmediate>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <Pm4ShaderType shaderType>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    uint32* WriteSetOneShRegIndex(
        uint32                          regAddr,
        uint32                          regData,
        Pm4ShaderType                   shaderType,
        PFP_SET_SH_REG_INDEX_index_enum index,
        uint32*                         pCmdSpace);
    uint32* WriteSetSeqShRegsIndex(
        uint32                          startRegAddr,
        uint32                          endRegAddr,
        Pm4ShaderType                   shaderType,
        const void*                     pData,
        PFP_SET_SH_REG_INDEX_index_enum index,
        uint32*                         pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteLoadSeqContextRegs(uint32 startRegAddr, uint32 regCount, gpusize dataVirtAddr, uint32* pCmdSpace);
    uint32* WriteLoadSeqContextRegs(uint32 startRegAddr, uint32 regCount, gpusize dataVirtAddr, uint32* pCmdSpace);

    uint32* WriteSetSeqShRegs(
        uint32        startRegAddr,
        uint32        endRegAddr,
        Pm4ShaderType shaderType,
        const void*   pData,
        uint32*       pCmdSpace);
    uint32* WriteSetZeroSeqShRegs(
        uint32        startRegAddr,
        uint32        endRegAddr,
        Pm4ShaderType shaderType,
        uint32* pCmdSpace);
    template <bool isPerfCtr = false>
    uint32* WriteSetSeqConfigRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    template <bool isPerfCtr = false>
    uint32* WriteSetZeroSeqConfigRegs(uint32 startRegAddr, uint32 endRegAddr, uint32* pCmdSpace);
    template <bool pm4OptImmediate>
    uint32* WriteSetVgtLsHsConfig(regVGT_LS_HS_CONFIG vgtLsHsConfig, uint32* pCmdSpace);

    template <bool IgnoreDirtyFlags, Pm4ShaderType shaderType>
    uint32* WriteUserDataEntriesToSgprs(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);
    template <bool IgnoreDirtyFlags, Pm4ShaderType shaderType, bool Pm4OptImmediate>
    uint32* WriteUserDataEntriesToSgprs(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);

#if PAL_BUILD_GFX11
    template <bool IgnoreDirtyFlags>
    static void AccumulateUserDataEntriesForSgprs(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        const uint16            baseUserDataReg,
        PackedRegisterPair*     pValidRegPairs,
        UserDataEntryLookup*    pRegLookup,
        uint32                  minLookupVal,
        uint32*                 pNumValidRegs);

    template <Pm4ShaderType ShaderType, bool Pm4OptImmediate>
    uint32* WriteSetShRegPairs(PackedRegisterPair* pRegPairs,
                               uint32              numRegs,
                               uint32*             pCmdSpace);
    template <Pm4ShaderType ShaderType>
    uint32* WriteSetShRegPairs(PackedRegisterPair* pRegPairs,
                               uint32              numRegs,
                               uint32*             pCmdSpace);
    template <bool Pm4OptImmediate>
    uint32* WriteSetContextRegPairs(PackedRegisterPair* pRegPairs,
                                    uint32              numRegs,
                                    uint32*             pCmdSpace);
    uint32* WriteSetContextRegPairs(PackedRegisterPair* pRegPairs,
                                    uint32              numRegs,
                                    uint32*             pCmdSpace);
#endif

    template <bool Pm4OptEnabled>
    uint32* WriteSetBase(
        gpusize                         address,
        PFP_SET_BASE_base_index_enum    baseIndex,
        Pm4ShaderType                   shaderType,
        uint32*                         pCmdSpace);
    uint32* WriteSetBase(
        gpusize                         address,
        PFP_SET_BASE_base_index_enum    baseIndex,
        Pm4ShaderType                   shaderType,
        uint32*                         pCmdSpace);

    uint32* WriteClearState(
        PFP_CLEAR_STATE_cmd_enum  clearMode,
        uint32*                   pCmdSpace);

    uint32* WriteDynamicLaunchDesc(
        gpusize launchDescGpuVa,
        uint32* pCmdSpace);

    // In rare cases some packets will modify register state behind the scenes (e.g., DrawIndirect). This function must
    // be called in those cases to ensure that immediate mode PM4 optimization invalidates its copy of the register.
    void NotifyIndirectShRegWrite(uint32 regAddr);

    void NotifyNestedCmdBufferExecute();

    void ResetDrawTimeState();
    template <bool canBeOptimized>
    void SetContextRollDetected();
    bool ContextRollDetected() const { return m_contextRollDetected; }

#if PAL_DEVELOPER_BUILD
    void IssueHotRegisterReport(Pm4CmdBuffer* pCmdBuf) const;
#endif

    void TempSetPm4OptimizerMode(bool isEnabled);

protected:
    virtual size_t BuildCondIndirectBuffer(
        CompareFunc compareFunc,
        gpusize     compareGpuAddr,
        uint64      data,
        uint64      mask,
        uint32*     pPacket) const override;

    virtual size_t BuildIndirectBuffer(
        gpusize  ibAddr,
        uint32   ibSize,
        bool     preemptionEnabled,
        bool     chain,
        uint32*  pPacket) const override;

    virtual size_t BuildNop(
        uint32   numDwords,
        uint32*  pPacket) const override { return m_cmdUtil.BuildNop(numDwords, pPacket); }

    virtual void PatchCondIndirectBuffer(
        ChainPatch*  pPatch,
        gpusize      address,
        uint32       ibSizeDwords) const override;

private:
    virtual void CleanupTempObjects() override;
    virtual void BeginCurrentChunk() override;
    virtual void EndCurrentChunk(bool atEndOfStream) override;

    const CmdUtil& m_cmdUtil;
    Pm4Optimizer*  m_pPm4Optimizer;       // This will only be created if optimization is enabled for this stream.
    uint32*        m_pChunkPreamble;      // If non-null, the current chunk preamble was allocated here.
    bool           m_contextRollDetected; // This will only be set if a context roll has been detected since the
                                          // last draw.

#if PAL_BUILD_GFX11
    const bool     m_supportsHoleyOptimization;
#endif

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
};

} // Gfx9
} // Pal
