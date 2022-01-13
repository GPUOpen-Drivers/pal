/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/shaderLibrary.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9PipelineChunkCs.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{

namespace Gfx9
{
class Device;
class MsgPackWriter;

// Structure describing the HW-specific information about a compute shader library.
struct LibraryHwInfo
{
    struct
    {
        // Persistent-state register values. These are the only HW regs needed for a shader library.
        regCOMPUTE_PGM_RSRC1     computePgmRsrc1;
        regCOMPUTE_PGM_RSRC2     computePgmRsrc2;
        regCOMPUTE_PGM_RSRC3     computePgmRsrc3;
    } libRegs;

    union
    {
        struct
        {
            uint32  isWave32  :  1;  // GFX10 setting; indicates wave32 vs. wave64
            uint32  reserved  : 31;
        };
        uint32  value;
    }flags;
};

// =====================================================================================================================
// GFX9 Shader Library class: implements GFX9 specific functionality for the ShaderLibrary class.
class ShaderLibrary final : public Pal::ShaderLibrary
{
public:
    explicit ShaderLibrary(Device* pDevice);

    virtual ~ShaderLibrary();

    void SetIsWave32(const Util::PalAbi::CodeObjectMetadata& metadata);
    bool IsWave32() const { return m_hwInfo.flags.isWave32; }

    const LibraryHwInfo& HwInfo() const {  return m_hwInfo; }

    virtual Result GetShaderFunctionCode(
        const char*  pShaderExportName,
        size_t*      pSize,
        void*        pBuffer) const override;

    virtual Result GetShaderFunctionStats(
        const char*      pShaderExportName,
        ShaderLibStats*  pShaderStats) const override;

    virtual const ShaderLibraryFunctionInfo* GetShaderLibFunctionList() const override { return m_pFunctionList; }

    virtual uint32 GetShaderLibFunctionCount() const override { return m_funcCount; }
protected:

    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) override;

    Result UnpackShaderFunctionStats(
        const char*                             pShaderExportName,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader,
        ShaderLibStats*                         pShaderStats) const;

    // Update m_hwInfo afer HwlInit
    void UpdateHwInfo();

private:
    /// @internal Client data pointer.
    void*  m_pClientData;

    Device*const       m_pDevice;
    LibraryChunkCs     m_chunkCs;
    LibraryHwInfo      m_hwInfo;

    ShaderLibraryFunctionInfo*  m_pFunctionList;
    uint32                      m_funcCount;

    // disable the default constructor and assignment operator
    PAL_DISALLOW_DEFAULT_CTOR(ShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderLibrary);
};

} // namespace Gfx9

} // namespace Pal
