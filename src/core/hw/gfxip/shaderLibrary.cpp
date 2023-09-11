/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Initialize this shader library based on the provided creation info.
Result ShaderLibrary::Initialize(
    const ShaderLibraryCreateInfo&    createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
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

    if (result == Result::Success)
    {
        PAL_ASSERT(m_pCodeObjectBinary != nullptr);
        result = InitFromCodeObjectBinary(createInfo, abiReader, metadata, pMetadataReader);
    }

    return result;
}

// =====================================================================================================================
// Initializes this library from the library binary data stored in this object.
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

} // Pal
