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

#include "core/hw/gfxip/computeShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{

namespace Gfx12
{

class Device;
class ComputeUserDataLayout;

// =====================================================================================================================
// Structure describing the HW-specific information about a compute shader library.
struct LibraryHwInfo
{
    struct
    {
        // Persistent-state register values. These are the only HW regs needed for a shader library.
        COMPUTE_PGM_RSRC1 computePgmRsrc1;
        COMPUTE_PGM_RSRC2 computePgmRsrc2;
        COMPUTE_PGM_RSRC3 computePgmRsrc3;
    } libRegs;
};

// =====================================================================================================================
// GFX12 Shader Library class: implements GFX12 specific functionality for the ComputeShaderLibrary class.
class ComputeShaderLibrary final : public Pal::ComputeShaderLibrary
{
public:
    explicit ComputeShaderLibrary(Device* pDevice);
    ~ComputeShaderLibrary();

    bool IsWave32() const { return m_isWave32; }

    const LibraryHwInfo& HwInfo() const { return m_hwInfo; }

    virtual Result GetShaderFunctionCode(
        Util::StringView<char> shaderExportName,
        size_t*                pSize,
        void*                  pBuffer) const override;

    virtual Result GetShaderFunctionStats(
        Util::StringView<char> shaderExportName,
        ShaderLibStats*        pShaderStats) const override;

    const ComputeUserDataLayout& UserDataLayout() const { return *m_pUserDataLayout; }

    static constexpr uint32 Registers[] =
    {
        Chip::mmCOMPUTE_PGM_RSRC1,
        Chip::mmCOMPUTE_PGM_RSRC2,
        Chip::mmCOMPUTE_PGM_RSRC3,
    };

    using Regs = RegPairHandler<decltype(Registers), Registers>;

    static_assert(Regs::Size() == Regs::NumSh(), "Only SH regs expected.");

protected:
    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    LibraryHwInfo m_hwInfo;
    bool          m_isWave32;

    ComputeUserDataLayout*     m_pUserDataLayout;

    // Disable the default constructor and assignment operator
    PAL_DISALLOW_DEFAULT_CTOR(ComputeShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeShaderLibrary);
};

}
}
