/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

class ComputePipelineUploader;
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

    bool IsWave32() const { return m_signature.flags.isWave32; }

    static uint32 CalcMaxWavesPerSe(
        const GpuChipProperties& chipProps,
        float                    maxWavesPerCu);

    static uint32 CalcMaxWavesPerSh(
        const GpuChipProperties& chipProps,
        float                    maxWavesPerCu);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 604
    static uint32 CalcMaxWavesPerSh(
        const GpuChipProperties& chipProps,
        uint32                   maxWavesPerCu)
    {
        return CalcMaxWavesPerSh(chipProps, static_cast<float>(maxWavesPerCu));
    }
#endif

    virtual Result LinkWithLibraries(
        const IShaderLibrary*const* ppLibraryList,
        uint32                      libraryCount) override;

    virtual void SetStackSizeInBytes(
        uint32 stackSizeInBytes) override;

    bool DisablePartialPreempt() const { return m_disablePartialPreempt; }

    // Returns the scratch memory size in dwords
    static uint32 CalcScratchMemSize(
        GfxIpLevel                gfxIpLevel,
        const CodeObjectMetadata& metadata);

protected:
    virtual Result HwlInit(
        const ComputePipelineCreateInfo& createInfo,
        const AbiReader&                 abiReader,
        const CodeObjectMetadata&        metadata,
        Util::MsgPackReader*             pMetadataReader) override;

private:
    void UpdateRingSizes(uint32 scratchMemorySize);

    Device*const  m_pDevice;

    ComputePipelineSignature  m_signature;
    PipelineChunkCs           m_chunkCs;
    bool                      m_disablePartialPreempt;

    PAL_DISALLOW_DEFAULT_CTOR(ComputePipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

// =====================================================================================================================
// Extension of the PipelineUploader helper class for Gfx9+ compute pipelines.
class ComputePipelineUploader final : public Pal::PipelineUploader
{
public:
    explicit ComputePipelineUploader(
        Device*          pDevice,
        const AbiReader& abiReader,
        uint32           shRegisterCount)
        :
        PipelineUploader(pDevice->Parent(), abiReader, 0, shRegisterCount)
        { }
    virtual ~ComputePipelineUploader() { }

    // Add a SH register to GPU memory for use with LOAD_SH_REG_INDEX.
    template <typename Register_t>
    PAL_INLINE void AddShReg(uint16 address, Register_t reg)
        { Pal::PipelineUploader::AddShRegister(address - PERSISTENT_SPACE_START, reg.u32All); }

private:
    PAL_DISALLOW_DEFAULT_CTOR(ComputePipelineUploader);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipelineUploader);
};

} // Gfx9
} // Pal
