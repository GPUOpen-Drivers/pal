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

#pragma once

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{
class GfxCmdBuffer;

namespace Gfx9
{

class CmdUtil;
class Device;
class Pm4Optimizer;

// =====================================================================================================================
// This is a specialization of CmdStream that has special knowledge of PM4 on GFX9 hardware. It implements conditional
// execution and chunk chaining. This class is also responsible for invoking the PM4 optimizer if it is enabled. Callers
// should use the "write" functions below when applicable as they may be necessary to hook into the PM4 optimizer.
class CmdStream final : public GfxCmdStream
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

    uint32* WriteRegisters(uint32 startAddr, uint32 count, const uint32* pRegData, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    template <bool isPerfCtr = false>
    uint32* WriteSetOneConfigReg(
        uint32                                regAddr,
        uint32                                regData,
        uint32*                               pCmdSpace,
        PFP_SET_UCONFIG_REG_INDEX_index_enum  index = index__pfp_set_uconfig_reg_index__default) const;

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

    template <bool Pm4OptImmediate>
    uint32* WriteSetOneGfxShRegIndexApplyCuMask(
        uint32  regAddr,
        uint32  regData,
        uint32* pCmdSpace);
    uint32* WriteSetOneGfxShRegIndexApplyCuMask(
        uint32  regAddr,
        uint32  regData,
        uint32* pCmdSpace);
    uint32* WriteSetSeqShRegsIndex(
        uint32                          startRegAddrIn,
        uint32                          endRegAddrIn,
        Pm4ShaderType                   shaderType,
        const void*                     pDataIn,
        PFP_SET_SH_REG_INDEX_index_enum index,
        uint32*                         pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqContextRegs(uint32 startRegAddrIn, uint32 endRegAddrIn, const void* pDataIn, uint32* pCmdSpace);

    uint32* WriteLoadSeqContextRegs(uint32 startRegAddr, uint32 regCount, gpusize dataVirtAddr, uint32* pCmdSpace);

    template <bool Pm4OptImmediate>
    uint32* WriteSetSeqShRegs(
        uint32        startRegAddrIn,
        uint32        endRegAddrIn,
        Pm4ShaderType shaderType,
        const void*   pDataIn,
        uint32*       pCmdSpace);
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

    template <Pm4ShaderType ShaderType, bool Pm4OptImmediate>
    uint32* WriteSetShRegPairs(PackedRegisterPair* pRegPairs,
                               uint32              numRegs,
                               uint32*             pCmdSpace);
    template <Pm4ShaderType ShaderType>
    uint32* WriteSetShRegPairs(PackedRegisterPair* pRegPairs,
                               uint32              numRegs,
                               uint32*             pCmdSpace);
    template <Pm4ShaderType ShaderType, bool Pm4OptEnabled>
    uint32* WriteSetConstShRegPairs(const PackedRegisterPair* pRegPairs,
                                    uint32                    numRegs,
                                    uint32*                   pCmdSpace);

    // These methods can modify the contents on pRegPairs!
    template <bool Pm4OptImmediate>
    uint32* WriteSetContextRegPairs(PackedRegisterPair* pRegPairs,
                                    uint32              numRegs,
                                    uint32*             pCmdSpace);
    uint32* WriteSetContextRegPairs(PackedRegisterPair* pRegPairs,
                                    uint32              numRegs,
                                    uint32*             pCmdSpace);

    // These methods will not modify the contents of pRegPairs and the caller MUST ensure alignment (even count)!
    template <bool Pm4OptImmediate>
    uint32* WriteSetConstContextRegPairs(const PackedRegisterPair* pRegPairs,
                                         uint32                    numRegs,
                                         uint32*                   pCmdSpace);
    uint32* WriteSetConstContextRegPairs(const PackedRegisterPair* pRegPairs,
                                         uint32                    numRegs,
                                         uint32*                   pCmdSpace);

    template <bool Pm4OptImmediate>
    uint32* WriteSetContextRegPairs(const RegisterValuePair* pRegPairs,
                                    uint32                   numRegPairs,
                                    uint32*                  pCmdSpace);
    uint32* WriteSetContextRegPairs(const RegisterValuePair* pRegPairs,
                                    uint32                   numRegPairs,
                                    uint32*                  pCmdSpace);

    template <Pm4ShaderType ShaderType>
    uint32* WriteSetShRegPairs(const RegisterValuePair* pRegPairs,
                               uint32                   numRegPairs,
                               uint32*                  pCmdSpace);
    template <Pm4ShaderType ShaderType, bool Pm4OptImmediate>
    uint32* WriteSetShRegPairs(const RegisterValuePair* pRegPairs,
                               uint32                   numRegPairs,
                               uint32*                  pCmdSpace);

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

    // In rare cases some packets will modify register state behind the scenes (e.g., DrawIndirect). This function must
    // be called in those cases to ensure that immediate mode PM4 optimization invalidates its copy of the register.
    template <bool Pm4OptEnabled>
    void NotifyIndirectShRegWrite(uint32 regAddr, uint32 numRegsToInvalidate = 1);
    void NotifyIndirectShRegWrite(uint32 regAddr, uint32 numRegsToInvalidate = 1);

    void NotifyNestedCmdBufferExecute();

#if PAL_DEVELOPER_BUILD
    void IssueHotRegisterReport(GfxCmdBuffer* pCmdBuf) const;
#endif

    uint32* WritePerfCounterWindow(bool enableWindow, uint32* pCmdSpace);

    bool MustKeepSetShReg(uint32 userDataAddr, uint32 userDataValue);

    uint32* WritePrimeGpuCaches(const PrimeGpuCacheRange& primeGpuCacheRange,
                                EngineType                engineType,
                                uint32*                   pCmdSpace)
    {
        return pCmdSpace + m_cmdUtil.BuildPrimeGpuCaches(primeGpuCacheRange, engineType, pCmdSpace);
    }

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
    bool           m_perfCounterWindowEnabled;

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
};

} // Gfx9
} // Pal
