/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/computeShaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "palMsgPack.h"

namespace Pal
{

namespace Gfx9
{
class Device;

// Structure describing the HW-specific information about a compute shader library.
struct LibraryHwInfo
{
    struct
    {
        // Persistent-state register values. These are the only HW regs needed for a shader library.
        regCOMPUTE_PGM_RSRC1  computePgmRsrc1;
        regCOMPUTE_PGM_RSRC2  computePgmRsrc2;
        regCOMPUTE_PGM_RSRC3  computePgmRsrc3;
    } libRegs;
};

// =====================================================================================================================
// GFX9 Shader Library class: implements GFX9 specific functionality for the ComputeShaderLibrary class.
class ComputeShaderLibrary final : public Pal::ComputeShaderLibrary
{
public:
    explicit ComputeShaderLibrary(Device* pDevice);

    const ComputePipelineSignature& Signature() const { return m_signature; }
    bool IsWave32() const { return m_signature.flags.isWave32; }

    const LibraryHwInfo& HwInfo() const { return m_hwInfo; }

    virtual Result GetShaderFunctionCode(
        Util::StringView<char> shaderExportName,
        size_t*                pSize,
        void*                  pBuffer) const override;

    virtual Result GetShaderFunctionStats(
        Util::StringView<char> shaderExportName,
        ShaderLibStats*        pShaderStats) const override;

protected:
    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    // Update m_hwInfo afer HwlInit
    void UpdateHwInfo();

private:

    Device*const  m_pDevice;

    ComputePipelineSignature  m_signature;
    PipelineChunkCs           m_chunkCs;
    ShaderStageInfo           m_stageInfoCs;
    LibraryHwInfo             m_hwInfo;

    // Disable the default constructor and assignment operator
    PAL_DISALLOW_DEFAULT_CTOR(ComputeShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeShaderLibrary);
};

} // namespace Gfx9
} // namespace Pal
