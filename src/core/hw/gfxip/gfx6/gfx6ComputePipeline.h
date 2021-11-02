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
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"

namespace Pal
{

class Platform;

namespace Gfx6
{

class CmdStream;

// =====================================================================================================================
// GFX6 compute pipeline class: implements GFX6 specific functionality for the ComputePipeline class.
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
        bool         getDisassemblySize) const override;

    const ComputePipelineSignature& Signature() const { return m_signature; }

    virtual void SetStackSizeInBytes(
        uint32 stackSizeInBytes) override;

protected:
    virtual Result HwlInit(
        const ComputePipelineCreateInfo&        createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

private:
    uint32* WriteShCommandsSetPath(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    uint32 CalcMaxWavesPerSh(float maxWavesPerCu) const;

    void UpdateRingSizes(uint32 scratchMemorySize);

    void SetupSignatureFromElf(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const RegisterVector&                   registers);

    Device*const  m_pDevice;

    struct
    {
        regCOMPUTE_NUM_THREAD_X  computeNumThreadX;
        regCOMPUTE_NUM_THREAD_Y  computeNumThreadY;
        regCOMPUTE_NUM_THREAD_Z  computeNumThreadZ;
        regCOMPUTE_PGM_LO        computePgmLo;
        regCOMPUTE_PGM_HI        computePgmHi;
        regCOMPUTE_PGM_RSRC1     computePgmRsrc1;
        regCOMPUTE_USER_DATA_0   computeUserDataLo;

        struct
        {
            regCOMPUTE_PGM_RSRC2        computePgmRsrc2;
            regCOMPUTE_RESOURCE_LIMITS  computeResourceLimits;
        } dynamic; // Contains state which depends on bind-time parameters.
    }  m_regs;

    PipelinePrefetchPm4       m_prefetch;
    ComputePipelineSignature  m_signature;

    PAL_DISALLOW_DEFAULT_CTOR(ComputePipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputePipeline);
};

} // Gfx6
} // Pal
