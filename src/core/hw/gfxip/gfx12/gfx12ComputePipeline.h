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

#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12PipelineChunkCs.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"
#include <cstddef>

namespace Pal
{
namespace Gfx12
{

class CmdStream;
class ComputeUserDataLayout;
class Device;

// =====================================================================================================================
// Gfx12 specific implementation of a compute pipeline. The compute pipeline state descriptors are hardware independent;
// no HW-specific implementation should be necessary.
class ComputePipeline final : public Pal::ComputePipeline
{
public:
    ComputePipeline(Device* pDevice, bool isInternal);

    uint32* WriteCommands(
        const ComputePipeline*          pPrevPipeline,
        const DynamicComputeShaderInfo& dynamicInfo,
        bool                            prefetch,
        uint32*                         pCmdSpace,
        CmdStream*                      pCmdStream) const
    {
        return m_chunkCs.WriteCommands(
            (pPrevPipeline == nullptr) ? nullptr : &pPrevPipeline->m_chunkCs,
            dynamicInfo,
            prefetch,
            pCmdSpace,
            pCmdStream);
    }

    uint32* WriteUpdatedLdsSize(
        uint32* pCmdSpace,
        uint32  ldsBytesPerTg) const
    {
        return m_chunkCs.WriteShCommandsLdsSize(pCmdSpace, ldsBytesPerTg);
    }

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDisassemblySize) const override;

    virtual Result LinkWithLibraries(
        const IShaderLibrary*const* ppLibraryList,
        uint32                      libraryCount) override;

    const ComputeUserDataLayout* UserDataLayout() const
        { return m_chunkCs.UserDataLayout(); }

    bool IsWave32()                    const { return m_flags.isWave32;                    }
    bool PingPongEn()                  const { return m_flags.pingPongEn;                  }
    bool Is2dDispatchInterleave()      const { return m_flags.is2dDispatchInterleave;      }
    bool IsDefaultDispatchInterleave() const { return m_flags.isDefaultDispatchInterleave; }

    regCOMPUTE_DISPATCH_INTERLEAVE ComputeDispatchInterleave() const { return m_chunkCs.ComputeDispatchInterleave(); }

    uint32 Get2dDispachInterleaveSize() const { return m_chunkCs.Get2dDispachInterleaveSize(); }

    static uint32 CalcScratchMemSize(const Util::PalAbi::CodeObjectMetadata& metadata);

    static uint32 CalcScratchMemSize(const Util::HsaAbi::CodeObjectMetadata& metadata);

    size_t GetRingSizeComputeScratch() const { return m_ringSizeComputeScratch; }

    size_t GetDvgprExtraAceScratch() const { return m_dvgprExtraAceScratch; }

private:
    virtual ~ComputePipeline() {}

    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::HsaAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader,
        const Extent3d&                         groupSize) override;

    void UpdateRingSizeComputeScratch(uint32 scratchMemorySizeInDword);

    PipelineChunkCs m_chunkCs;

    union
    {
        struct
        {
            uint32 isWave32                    :  1;
            uint32 pingPongEn                  :  1;
            uint32 is2dDispatchInterleave      :  1;
            uint32 isDefaultDispatchInterleave :  1;
            uint32 enableGroupLaunchGuarantee  :  1;
            uint32 reserved                    : 27;
        };
        uint32 value; // Flags packed as a uint32.
    } m_flags;

    size_t m_ringSizeComputeScratch;
    size_t m_dvgprExtraAceScratch; // Additional scratch memory when dVGPRs are used in ACE compute queues.

    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // namespace Gfx12
} // namespace Pal
