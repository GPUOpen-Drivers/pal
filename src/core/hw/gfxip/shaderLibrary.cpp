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

#include "core/hw/gfxip/shaderLibrary.h"

using namespace Util;

namespace Pal
{
// =====================================================================================================================
ShaderLibrary::ShaderLibrary(
    Device* pDevice)
    :
    Pal::IShaderLibrary(),
    m_pDevice(pDevice),
    m_info{},
    m_flags{},
    m_pCodeObjectBinary(nullptr),
    m_codeObjectBinaryLen(0)
{
}

// =====================================================================================================================
// First of two steps of ShaderLibrary Init. This function copies the code object data to memory owned by
// this ShaderLibrary object. This function must be called before InitFromCodeObjectBinary.
Result ShaderLibrary::InitializeCodeObject(
    const ShaderLibraryCreateInfo& createInfo)
{
    Result result = Result::Success;

    if ((createInfo.pCodeObject != nullptr) && (createInfo.codeObjectSize != 0))
    {
        m_flags               = createInfo.flags;
        m_codeObjectBinaryLen = createInfo.codeObjectSize;
        m_pCodeObjectBinary   = PAL_MALLOC(m_codeObjectBinaryLen, m_pDevice->GetPlatform(), AllocInternal);

        if (m_pCodeObjectBinary == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pCodeObjectBinary, createInfo.pCodeObject, m_codeObjectBinaryLen);
        }
    }
    else
    {
        result = Result::ErrorInvalidPointer;
    }
    return result;
}

// =====================================================================================================================
// Second of two steps of ShaderLibrary init. Initializes this library from the library binary data
// stored in this object. Must be called after InitializeCodeObject.
Result ShaderLibrary::InitFromCodeObjectBinary(
    const ShaderLibraryCreateInfo&    createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    PAL_ASSERT((m_pCodeObjectBinary != nullptr) && (m_codeObjectBinaryLen != 0));

    ExtractLibraryInfo(metadata);
    DumpLibraryElf(createInfo.flags.isGraphics? "LibraryGraphics": "LibraryCs", metadata.pipeline.name);

    Result result = HwlInit(createInfo, abiReader, metadata, pMetadataReader);

    if (result == Result::Success)
    {
        result = PostInit(metadata, pMetadataReader);
    }
    return result;
}

// =====================================================================================================================
// Helper function for extracting the pipeline hash and per-shader hashes from pipeline metadata.
void ShaderLibrary::ExtractLibraryInfo(
    const PalAbi::CodeObjectMetadata& metadata)
{
    m_info.internalLibraryHash =
        { metadata.pipeline.internalPipelineHash[0], metadata.pipeline.internalPipelineHash[1] };

    // We don't expect the pipeline ABI to report a hash of zero.
    PAL_ALERT((metadata.pipeline.internalPipelineHash[0] | metadata.pipeline.internalPipelineHash[1]) == 0);
}

// =====================================================================================================================
// Extracts the shader library's code object ELF binary.
Result ShaderLibrary::GetCodeObject(
    uint32*    pSize,
    void*      pBuffer
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pSize != nullptr)
    {
        if ((m_pCodeObjectBinary != nullptr) && (m_codeObjectBinaryLen != 0))
        {
            if (pBuffer == nullptr)
            {
                (*pSize) = static_cast<uint32>(m_codeObjectBinaryLen);
                result = Result::Success;
            }
            else if ((*pSize) >= static_cast<uint32>(m_codeObjectBinaryLen))
            {
                memcpy(pBuffer, m_pCodeObjectBinary, m_codeObjectBinaryLen);
                result = Result::Success;
            }
            else
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            result = Result::ErrorUnavailable;
        }
    }

    return result;
}

// =====================================================================================================================
void ShaderLibrary::DumpLibraryElf(
    Util::StringView<char> prefix,
    Util::StringView<char> name     // Optional: Can be the empty string if a human-readable filename is not desired.
    ) const
{
    m_pDevice->LogCodeObjectToDisk(
        prefix,
        name,
        m_info.internalLibraryHash,
        false,
        m_pCodeObjectBinary,
        m_codeObjectBinaryLen);
}

// =====================================================================================================================
// Obtains the shader pre and post compilation stats/params for the specified shader.
Result ShaderLibrary::GetShaderFunctionInfos(
    Util::StringView<char> shaderExportName,
    ShaderLibStats* pShaderStats,
    const AbiReader& abiReader,
    Util::MsgPackReader* pMetadataReader,
    PalAbi::CodeObjectMetadata& metadata
) const
{
    Result result = Result::Success;
    // We can re-parse the saved pipeline ELF binary to extract shader statistics.
    const Elf::SymbolTableEntry* pSymbol = abiReader.GetGenericSymbol(shaderExportName);
    if (pSymbol != nullptr)
    {
        pShaderStats->isaSizeInBytes = static_cast<size_t>(pSymbol->st_size);
    }
    pShaderStats->palInternalLibraryHash = m_info.internalLibraryHash;

    result = UnpackShaderFunctionStats(shaderExportName,
        metadata,
        pMetadataReader,
        pShaderStats);

    return result;
}

// =====================================================================================================================
// Obtains the shader function stack frame size
Result ShaderLibrary::UnpackShaderFunctionStats(
    Util::StringView<char>            shaderExportName,
    const PalAbi::CodeObjectMetadata& metadata,
    Util::MsgPackReader* pMetadataReader,
    ShaderLibStats* pShaderStats
) const
{
    Result result = pMetadataReader->Seek(metadata.pipeline.shaderFunctions);
    if (result == Result::Success)
    {
        result = (pMetadataReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;
        const auto& item = pMetadataReader->Get().as;

        for (uint32 i = item.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            StringView<char> symbolName;
            result = pMetadataReader->UnpackNext(&symbolName);

            if (result == Result::Success)
            {
                result = pMetadataReader->Next(CWP_ITEM_MAP);
            }

            for (uint32 j = item.map.size; ((result == Result::Success) && (j > 0)); --j)
            {
                result = pMetadataReader->Next(CWP_ITEM_STR);

                if (result == Result::Success)
                {
                    if (shaderExportName == symbolName)
                    {
                        switch (HashString(static_cast<const char*>(item.str.start), item.str.length))
                        {
                        case HashLiteralString(".stack_frame_size_in_bytes"):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->stackFrameSizeInBytes);
                        }
                        break;
                        case HashLiteralString(PalAbi::ShaderMetadataKey::ShaderSubtype):
                        {
                            Abi::ApiShaderSubType shaderSubType;
                            result = PalAbi::Metadata::DeserializeEnum(pMetadataReader, &shaderSubType);
                            pShaderStats->shaderSubType = ShaderSubType(shaderSubType);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::VgprCount):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.numUsedVgprs);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::SgprCount):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.numUsedSgprs);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::LdsSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->common.ldsUsageSizeInBytes);
                        }
                        break;
                        case HashLiteralString(PalAbi::ShaderMetadataKey::ApiShaderHash):
                        {
                            uint64 shaderHash[2] = {};
                            result = pMetadataReader->UnpackNext(&shaderHash);
                            pShaderStats->shaderHash = { shaderHash[0], shaderHash[1] };
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::FrontendStackSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->cpsStackSizes.frontendSize);
                        }
                        break;
                        case HashLiteralString(PalAbi::HardwareStageMetadataKey::BackendStackSize):
                        {
                            result = pMetadataReader->UnpackNext(&pShaderStats->cpsStackSizes.backendSize);
                        }
                        break;
                        default:
                            result = pMetadataReader->Skip(1);
                            break;
                        }

                    }
                    else
                    {
                        result = pMetadataReader->Skip(1);
                    }
                }
            }

        }
    }

    return result;
}

} // Pal
