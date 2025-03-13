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

#pragma once

#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// =====================================================================================================================
// CmdStream specialization for Gfx12-specific implementation items like command buffer chaining.
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

    virtual ~CmdStream() { }

    virtual void Call(const Pal::CmdStream& targetStream, bool exclusiveSubmit, bool allowIb2Launch) override;

    uint32 GetChainSizeInDwords(const Device& device, EngineType engineType, bool isNested) const;

    // AllocateCommands API functions:
    // These functions combine an AllocateCommands call with one of the packet builder routines below. These functions
    // should only be used when you need to build exactly one packet. Otherwise consider a manual AllocateCommands call
    // to cover multiple packets or a typical ReserveCommands/CommitCommands pair.
    void AllocateAndBuildSetOneContextReg(uint32 regAddr, uint32 value)
    {
        WriteSetOneContextReg(regAddr, value, AllocateCommands(CmdUtil::SetOneContextRegSizeDwords));
    }

    void AllocateAndBuildSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData)
    {
        WriteSetSeqContextRegs(startRegAddr, endRegAddr, pData,
            AllocateCommands(CmdUtil::SetSeqContextRegsSizeDwords(startRegAddr, endRegAddr)));
    }

    void AllocateAndBuildSetContextPairs(const RegisterValuePair* pPairs, uint32 numPairs)
    {
        CmdUtil::BuildSetContextPairs(pPairs, numPairs,
            AllocateCommands(CmdUtil::SetContextPairsSizeDwords(numPairs)));
    }

    template <typename... Args>
    void AllocateAndBuildSetContextPairGroups(uint32 totalRegPairs, Args... args)
    {
        WriteSetContextPairGroups(AllocateCommands(CmdUtil::SetContextPairsSizeDwords(totalRegPairs)),
                                  totalRegPairs, args...);
    }

    template <bool isPerfCtr = false>
    void AllocateAndBuildSetOneUConfigReg(uint32 regAddr, uint32 regData)
    {
        WriteSetOneUConfigReg<isPerfCtr>(regAddr, regData, AllocateCommands(CmdUtil::SetOneUConfigRegSizeDwords));
    }

    template <bool isPerfCtr = false>
    void AllocateAndBuildSetSeqUConfigRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData)
    {
        WriteSetSeqUConfigRegs<isPerfCtr>(startRegAddr, endRegAddr, pData,
            AllocateCommands(CmdUtil::SetSeqUConfigRegsSizeDwords(startRegAddr, endRegAddr)));
    }

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    void AllocateAndBuildSetSeqShRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData)
    {
        WriteSetSeqShRegs<ShaderType>(startRegAddr, endRegAddr, pData,
            AllocateCommands(CmdUtil::SetSeqShRegsSizeDwords(startRegAddr, endRegAddr)));
    }

    // Register packet builder helpers:
    // These functions combine a CmdUtil register "Build" call with placing those registers into the packet body.
    // Note that most of this could be moved into CmdUtil if someone feels like doing so, this is a gfx9 anachronism.
    static uint32* WriteSetOneContextReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    static uint32* WriteContextRegRmw(uint32 regAddr, uint32 regMask, uint32 regData, uint32* pCmdSpace);

    uint32* WriteSetOnePerfCtrReg(uint32 regAddr, uint32 value, uint32* pCmdSpace);

    uint32* WriteCopyPerfCtrRegToMemory(uint32 srcReg, gpusize dstGpuVa, uint32* pCmdSpace);

    static uint32* WriteSetContextPairs(const RegisterValuePair* pPairs, uint32 numPairs, uint32* pCmdSpace);

    template <typename... Args>
    static uint32* WriteSetContextPairGroups(
        uint32* pCmdSpace,
        uint32  totalRegs,
        Args... args)
    {
        void* pDataStart = nullptr;

        pCmdSpace += CmdUtil::BuildSetContextPairsHeader(totalRegs, &pDataStart, pCmdSpace);

        WriteSetPairsInternal(pDataStart, args...);

        return pCmdSpace;
    }

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static uint32* WriteSetOneShRegIndex(
        uint32                          regAddr,
        uint32                          regData,
        PFP_SET_SH_REG_INDEX_index_enum index,
        uint32*                         pCmdSpace);
    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static uint32* WriteSetSeqShRegsIndex(
        uint32                          startRegAddr,
        uint32                          endRegAddr,
        const void*                     pData,
        PFP_SET_SH_REG_INDEX_index_enum index,
        uint32*                         pCmdSpace);

    static uint32* WriteSetSeqContextRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static uint32* WriteSetOneShReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace);

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static uint32* WriteSetSeqShRegs(uint32 startRegAddr, uint32 endRegAddr, const void* pData, uint32* pCmdSpace);

    template <Pm4ShaderType ShaderType = ShaderGraphics>
    static uint32* WriteSetShPairs(const RegisterValuePair* pPairs, uint32 numPairs, uint32* pCmdSpace);

    template <Pm4ShaderType ShaderType, typename... Args>
    static uint32* WriteSetShPairGroups(
        uint32* pCmdSpace,
        uint32  totalRegs,
        Args... args)
    {
        void* pDataStart = nullptr;

        pCmdSpace += CmdUtil::BuildSetShPairsHeader<ShaderType>(totalRegs, &pDataStart, pCmdSpace);

        WriteSetPairsInternal(pDataStart, args...);

        return pCmdSpace;
    }

    template <bool isPerfCtr = false>
    uint32* WriteSetOneUConfigReg(uint32 regAddr, uint32 regData, uint32* pCmdSpace) const;

    template <bool isPerfCtr = false>
    static uint32* WriteSetSeqUConfigRegs(
        uint32      startRegAddr,
        uint32      endRegAddr,
        const void* pData,
        uint32*     pCmdSpace);

    template <typename... Args>
    static uint32* WriteSetUConfigPairGroups(
        uint32* pCmdSpace,
        uint32  totalRegs,
        Args... args)
    {
        void* pDataStart = nullptr;

        pCmdSpace += CmdUtil::BuildSetUConfigPairsHeader(totalRegs, &pDataStart, pCmdSpace);

        WriteSetPairsInternal(pDataStart, args...);

        return pCmdSpace;
    }

    static uint32* WriteSetUConfigPairs(const RegisterValuePair* pPairs, uint32 numPairs, uint32* pCmdSpace);

    uint32* WritePerfCounterWindow(bool enableWindow, uint32* pCmdSpace);

private:
    virtual size_t BuildNop(uint32 numDwords, uint32* pCmdSpace) const override
        { return CmdUtil::BuildNop(numDwords, pCmdSpace); }

    virtual void BeginCurrentChunk() override;
    virtual void EndCurrentChunk(bool atEndOfStream) override;

    template <typename... Args>
    static void WriteSetPairsInternal(
        void*                    pCmdSpace,
        const RegisterValuePair* pPairs,
        uint32                   numPairs,
        Args...                  args)
    {
        const uint32 sizeToCopy = WriteSetPairsInternal(pCmdSpace, pPairs, numPairs);
        WriteSetPairsInternal(Util::VoidPtrInc(pCmdSpace, sizeToCopy), args...);
    }

    static uint32 WriteSetPairsInternal(
        void*                    pCmdSpace,
        const RegisterValuePair* pPairs,
        uint32                   numPairs)
    {
        const uint32 sizeToCopy = sizeof(RegisterValuePair) * numPairs;
        memcpy(pCmdSpace, pPairs, sizeToCopy);
        return sizeToCopy;
    }

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

    virtual void PatchCondIndirectBuffer(
        ChainPatch* pPatch,
        gpusize     address,
        uint32      ibSizeDwords) const override;

    const CmdUtil& m_cmdUtil;
    uint32*        m_pPerfCounterWindowLastPacket;
    bool           m_perfCounterWindowEnabled;

    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
};

} // namespace Gfx12
} // namespace Pal
