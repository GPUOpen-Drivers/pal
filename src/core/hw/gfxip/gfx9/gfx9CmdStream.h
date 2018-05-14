/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"

namespace Pal
{
namespace Gfx9
{

class CmdUtil;
class Device;
class Pm4Optimizer;

// =====================================================================================================================
// This is a specialization of CmdStream that has special knowledge of PM4 on GFX9 hardware. It implements conditional
// execution and chunk chaining. This class is also responsible for invoking the PM4 optimizer if it is enabled. Callers
// should use the "write" functions below when applicable as they may be necessary to hook into the PM4 optimizer.
class CmdStream : public Pal::GfxCmdStream
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

    // Public command interface:
    // The command stream client should call these special functions whenever it wishes to copy pre-built PM4 images
    // to the reserve buffer or wishes to build any of the relevant packets directly in the reserve buffer. These
    // functions give the PM4 optimizer a chance to optimize before anything is written to the reserve buffer.
    uint32* WritePm4Image(size_t sizeInDwords, const void* pImage, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneConfigReg(
        uint32                          regAddr,
        uint32                          regData,
        uint32*                         pCmdSpace,
        PFP_SET_UCONFIG_REG_index_enum  index = index__pfp_set_uconfig_reg__default);

    template <bool pm4OptImmediate>
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextRegNoOpt(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    uint32* WriteSetOnePerfCtrReg(
        uint32  regAddr,
        uint32  value,
        uint32* pCmdSpace);
    uint32* WriteSetOnePrivilegedConfigReg(
        uint32     regAddr,
        uint32     value,
        uint32*    pCmdSpace);

    template <Pm4ShaderType shaderType, bool pm4OptImmediate>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <Pm4ShaderType shaderType>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <Pm4ShaderType shaderType, bool pm4OptImmediate>
    uint32* WriteSetShRegDataOffset(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <Pm4ShaderType shaderType>
    uint32* WriteSetShRegDataOffset(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <bool pm4OptImmediate>
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqShRegs(
        uint32        startRegAddr,
        uint32        endRegAddr,
        Pm4ShaderType shaderType,
        const void*   pData,
        uint32*       pCmdSpace);
    uint32* WriteSetSeqConfigRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    template <bool pm4OptImmediate>
    uint32* WriteSetVgtLsHsConfig(regVGT_LS_HS_CONFIG vgtLsHsConfig, uint32* pCmdSpace);

    template <bool IgnoreDirtyFlags>
    uint32* WriteUserDataEntriesToSgprsGfx(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);
    template <bool IgnoreDirtyFlags, bool Pm4OptImmediate>
    uint32* WriteUserDataEntriesToSgprsGfx(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);

    uint32* WriteClearState(
        PFP_CLEAR_STATE_cmd_enum  clearMode,
        uint32*                   pCmdSpace);

    // In rare cases some packets will modify register state behind the scenes (e.g., DrawIndirect). This function must
    // be called in those cases to ensure that immediate mode PM4 optimization invalidates its copy of the register.
    void NotifyIndirectShRegWrite(uint32 regAddr);

    void NotifyNestedCmdBufferExecute();

    void ResetDrawTimeState();
    template <bool canBeOptimized>
    void SetContextRollDetected();
    bool ContextRollDetected() const { return m_contextRollDetected; }

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

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
};

} // Gfx9
} // Pal
