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

#include "core/device.h"
#include "core/hw/gfxip/pipeline.h"
#include "palMsgPack.h"
#include "palPipelineAbi.h"
#include "palShaderLibrary.h"
#include "palVector.h"
#include "palSpan.h"

namespace Pal
{
// Shorthand for a pipeline ABI reader.
using AbiReader = Util::Abi::PipelineAbiReader;

// =====================================================================================================================
// Hardware independent shader library class.
class ShaderLibrary : public IShaderLibrary
{
public:
    virtual void Destroy() override { this->~ShaderLibrary(); }

    Result InitializeCodeObject(const ShaderLibraryCreateInfo& createInfo);

    Result InitFromCodeObjectBinary(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader);

    virtual const LibraryInfo& GetInfo() const override { return m_info; }

    virtual Result GetCodeObject(
        uint32*  pSize,
        void*    pBuffer) const override;

    virtual const Util::Span<const ShaderLibraryFunctionInfo> GetShaderLibFunctionInfos() const override
    {
        return Util::Span<const ShaderLibraryFunctionInfo>();
    }

    virtual Result GetShaderFunctionCode(
        Util::StringView<char> shaderExportName,
        size_t*                pSize,
        void*                  pBuffer) const override
    {
        return Result::ErrorUnavailable;
    }

    virtual Result GetShaderFunctionStats(
        Util::StringView<char>  shaderExportName,
        ShaderLibStats* pStats) const override
    {
        return Result::ErrorUnavailable;
    }

    Result GetAggregateFunctionStats(
        ShaderLibStats* pStats) const;

    Result GetShaderFunctionInfos(
        Util::StringView<char>  shaderExportName,
        ShaderLibStats* pStats,
        const AbiReader& abiReader,
        Util::MsgPackReader* pMetadataReader,
        Util::PalAbi::CodeObjectMetadata& metadata) const;

    Result UnpackShaderFunctionStats(
        Util::StringView<char>            shaderExportName,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader* pMetadataReader,
        ShaderLibStats* pShaderStats) const;

    Util::Span<const void> GetCodeObject() const { return m_codeObject; }

    bool IsInternal() const { return m_flags.clientInternal != 0; }
    bool IsGraphics() const { return m_flags.isGraphics != 0; }

    virtual UploadFenceToken GetUploadFenceToken() const = 0;
    virtual uint64           GetPagingFenceVal() const   = 0;

protected:
    // internal Constructor.
    explicit ShaderLibrary(Device* pDevice);

    // internal Destructor.
    virtual ~ShaderLibrary()
    {
        PAL_FREE(m_codeObject.Data(), m_pDevice->GetPlatform());
        m_codeObject = {};
    }

    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) = 0;

    virtual Result PostInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pReader) = 0;

    void ExtractLibraryInfo(
        const Util::PalAbi::CodeObjectMetadata& metadata);

    Device*const       m_pDevice;
    LibraryInfo        m_info;                 // Public info structure available to the client.
    LibraryCreateFlags m_flags;                // Creation flags.
    Util::Span<void>   m_codeObject;           // Buffer containing the code object binary data (Pipeline ELF ABI).

private:
    void DumpLibraryElf(
        Util::StringView<char> prefix,
        Util::StringView<char> name) const;

    PAL_DISALLOW_DEFAULT_CTOR(ShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderLibrary);
};
} // namespace Pal
