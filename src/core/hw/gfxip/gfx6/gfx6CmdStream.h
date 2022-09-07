/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdStream.h"
#include "core/hw/gfxip/pm4CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"

namespace Pal
{
namespace Gfx6
{

class  Device;
class  Pm4Optimizer;
struct UserDataEntryMap;

// =====================================================================================================================
// This is a specialization of CmdStream that has special knowledge of PM4 on GFX6-8 hardware. It implements conditional
// execution and chunk chaining. This class is also responsible for invoking the PM4 optimizer if it is enabled. Callers
// should use the "write" functions below when applicable as they may be necessary to hook into the PM4 optimizer.
//
// This class defines a command block as a sequential set of PM4 commands. Execution may begin at any point in the block
// but must run to the end. The block must be terminated with a chaining packet unless it is the last block. The base
// class has no command block concept, it simply doles out command space; however it must notify its subclasses when it
// switches to a new command chunk so that they have a chance to chain the old chunk's final command block to the first
// block of the new chunk.
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

    uint32 GetChainSizeInDwords(const Device& device, bool isNested) const;

    template <bool pm4OptImmediate>
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);
    uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteSetIaMultiVgtParam(regIA_MULTI_VGT_PARAM iaMultiVgtParam, uint32* pCmdSpace);
    template <bool pm4OptImmediate>
    uint32* WriteSetVgtLsHsConfig(regVGT_LS_HS_CONFIG vgtLsHsConfig, uint32* pCmdSpace);

    uint32* WriteSetPaScRasterConfig(regPA_SC_RASTER_CONFIG paScRasterConfig, uint32* pCmdSpace);

    uint32* WriteSetOneConfigReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <bool pm4OptImmediate>
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    uint32* WriteSetOneContextRegNoOpt(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteSetBase(PM4ShaderType shaderType, uint32 baseIndex, gpusize baseAddr, uint32* pCmdSpace);
    uint32* WriteSetBase(PM4ShaderType shaderType, uint32 baseIndex, gpusize baseAddr, uint32* pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteLoadSeqContextRegs(
        bool    useIndexVersion,
        uint32  startRegAddr,
        uint32  regCount,
        gpusize dataVirtAddr,
        uint32* pCmdSpace);
    uint32* WriteLoadSeqContextRegs(
        bool    useIndexVersion,
        uint32  startRegAddr,
        uint32  regCount,
        gpusize dataVirtAddr,
        uint32* pCmdSpace);

    uint32* WriteSetOnePerfCtrReg(uint32 regAddr, uint32 value, uint32* pCmdSpace);
    uint32* WriteSetOnePrivilegedConfigReg(uint32 regAddr, uint32 value, uint32* pCmdSpace);

    template <PM4ShaderType shaderType, bool pm4OptImmediate>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);
    template <PM4ShaderType shaderType>
    uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    uint32* WriteSetOneShRegIndex(
        uint32        regAddr,
        uint32        regData,
        PM4ShaderType shaderType,
        uint32        index,
        uint32*       pCmdSpace);
    uint32* WriteSetSeqShRegsIndex(
        uint32        startRegAddr,
        uint32        endRegAddr,
        PM4ShaderType shaderType,
        const void*   pData,
        uint32        index,
        uint32*       pCmdSpace);

    template <bool pm4OptImmediate>
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);
    uint32* WriteSetSeqShRegs(
        uint32        startRegAddr,
        uint32        endRegAddr,
        PM4ShaderType shaderType,
        const void*   pData,
        uint32*       pCmdSpace);
    uint32* WriteSetSeqConfigRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);

    template <bool IgnoreDirtyFlags, PM4ShaderType shaderType>
    uint32* WriteUserDataEntriesToSgprs(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);
    template <bool IgnoreDirtyFlags, PM4ShaderType shaderType, bool Pm4OptImmediate>
    uint32* WriteUserDataEntriesToSgprs(
        const UserDataEntryMap& entryMap,
        const UserDataEntries&  entries,
        uint32*                 pCmdSpace);

    uint32* WriteSetVgtPrimitiveType(regVGT_PRIMITIVE_TYPE vgtPrimitiveType, uint32* pCmdSpace);

    // In rare cases some packets will modify register state behind the scenes (e.g., DrawIndirect). This function must
    // be called in those cases to ensure that immediate mode PM4 optimization invalidates its copy of the register.
    void NotifyIndirectShRegWrite(uint32 regAddr);

    void NotifyNestedCmdBufferExecute();

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
        uint32*  pCmdSpace) const override { return m_cmdUtil.BuildNop(numDwords, pCmdSpace); }

    virtual void PatchCondIndirectBuffer(
        ChainPatch*  pPatch,
        gpusize      address,
        uint32       ibSizeDwords) const override;

private:
    virtual void CleanupTempObjects() override;
    virtual void EndCurrentChunk(bool atEndOfStream) override;

    const CmdUtil& m_cmdUtil;

    Pm4Optimizer*  m_pPm4Optimizer;     // This will only be created if optimization is enabled for this stream.

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
};

} // Gfx6
} // Pal
