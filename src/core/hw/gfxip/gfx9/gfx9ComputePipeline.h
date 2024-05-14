/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"

namespace Pal
{

class Platform;

namespace Gfx9
{

class CmdStream;

// =====================================================================================================================
// GFX9 compute pipeline class: implements GFX9 specific functionality for the ComputePipeline class.
class ComputePipeline final : public Pal::ComputePipeline
{
public:
    ComputePipeline(Device* pDevice, bool isInternal);
    virtual ~ComputePipeline() { }

    uint32* WriteCommands(
        CmdStream*                      pCmdStream,
        uint32*                         pCmdSpace,
        const DynamicComputeShaderInfo& csInfo,
        bool                            prefetch) const;

    virtual Result GetShaderStats(
        ShaderType   shaderType,
        ShaderStats* pShaderStats,
        bool         getDissassemblySize) const override;

    const ComputePipelineSignature& Signature() const { return m_signature; }

    size_t GetRingSizeComputeScratch() const { return m_ringSizeComputeScratch; }

    bool IsWave32() const { return m_signature.flags.isWave32; }

    static uint32 CalcMaxWavesPerSe(
        const GpuChipProperties& chipProps,
        float                    maxWavesPerCu);

    static uint32 CalcMaxWavesPerSh(
        const GpuChipProperties& chipProps,
        float                    maxWavesPerCu);

    virtual Result LinkWithLibraries(
        const IShaderLibrary*const* ppLibraryList,
        uint32                      libraryCount) override;

    virtual void SetStackSizeInBytes(
        uint32 stackSizeInBytes) override;

    bool DisablePartialPreempt() const { return m_disablePartialPreempt; }

    // Returns the scratch memory size in dwords
    static uint32 CalcScratchMemSize(
        GfxIpLevel                              gfxIpLevel,
        const Util::PalAbi::CodeObjectMetadata& metadata);

    // Returns the scratch memory size in dwords
    static uint32 CalcScratchMemSize(
        GfxIpLevel                              gfxIpLevel,
        const Util::HsaAbi::CodeObjectMetadata& metadata);

protected:
    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::HsaAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

private:
    void UpdateRingSizeComputeScratch(uint32 scratchMemorySize);

    Device*const m_pDevice;

    ComputePipelineSignature m_signature;
    size_t                   m_ringSizeComputeScratch;
    PipelineChunkCs          m_chunkCs;
    bool                     m_disablePartialPreempt;
    const bool               m_shPairsPacketSupportedCs;

    PAL_DISALLOW_DEFAULT_CTOR(ComputePipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // Gfx9
} // Pal
