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

#pragma once

#include "core/device.h"
#include "core/hw/gfxip/pipeline.h"
#include "palMsgPack.h"
#include "palPipelineAbi.h"
#include "palShaderLibrary.h"
#include "palVector.h"

namespace Pal
{
class ShaderLibraryUploader;

// Shorthand for a pipeline ABI reader.
using AbiReader = Util::Abi::PipelineAbiReader;

// Structure describing the shader function statistics.
struct ShaderFuncStats
{
    const char*    pSymbolName;
    uint32         symbolNameLength;
    uint32         stackFrameSizeInBytes;
    ShaderSubType  shaderSubType;
};

using ShaderFuncStatsList = Util::Vector<ShaderFuncStats, 10, Platform>;

// =====================================================================================================================
// Hardware independent compute pipeline class.  Implements all details of a compute pipeline that are common across
// all hardware types but distinct from a graphics pipeline.
class ShaderLibrary : public IShaderLibrary
{
public:
    virtual void Destroy() override { this->~ShaderLibrary(); }

    Result Initialize(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader);

    virtual const LibraryInfo& GetInfo() const override { return m_info; }

    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const override;

    virtual Result GetCodeObject(
        uint32*  pSize,
        void*    pBuffer) const override;

    virtual Result GetShaderFunctionCode(
        const char*  pShaderExportName,
        size_t*      pSize,
        void*        pBuffer) const override;

    virtual Result GetShaderFunctionStats(
        const char*      pShaderExportName,
        ShaderLibStats*  pShaderStats) const override;

    virtual const ShaderLibraryFunctionInfo* GetShaderLibFunctionList() const override { return nullptr; }

    virtual uint32 GetShaderLibFunctionCount() const override { return 0; }

    static void GetFunctionGpuVirtAddrs(
        const PipelineUploader&    uploader,
        ShaderLibraryFunctionInfo* pFuncInfoList,
        uint32                     funcCount);

    uint32 GetMaxStackSizeInBytes() const { return m_maxStackSizeInBytes; }
    UploadFenceToken GetUploadFenceToken() const { return m_uploadFenceToken; }
    uint64 GetPagingFenceVal() const { return m_pagingFenceVal; }

    const void* GetCodeObject() const { return m_pCodeObjectBinary; }

protected:
    // internal Constructor.
    explicit ShaderLibrary(Device* pDevice);

    // internal Destructor.
    virtual ~ShaderLibrary()
    {
        PAL_SAFE_FREE(m_pCodeObjectBinary, m_pDevice->GetPlatform());
    }

    virtual Result HwlInit(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader) = 0;

    Result PerformRelocationsAndUploadToGpuMemory(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const GpuHeap&                          clientPreferredHeap,
        PipelineUploader*                       pUploader);

    void ExtractLibraryInfo(
        const Util::PalAbi::CodeObjectMetadata& metadata);

    Result ExtractShaderFunctions(
        Util::MsgPackReader* pReader);

    Device*const    m_pDevice;

    LibraryInfo     m_info;                 // Public info structure available to the client.
    void*           m_pCodeObjectBinary;    // Buffer containing the code object binary data (Pipeline ELF ABI).
    size_t          m_codeObjectBinaryLen;  // Size of code object binary data, in bytes.

    BoundGpuMemory  m_gpuMem;
    gpusize         m_gpuMemSize;
    uint32          m_maxStackSizeInBytes;

    UploadFenceToken  m_uploadFenceToken;
    uint64            m_pagingFenceVal;

private:
    Result InitFromCodeObjectBinary(
        const ShaderLibraryCreateInfo&          createInfo,
        const AbiReader&                        abiReader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pMetadataReader);

    void DumpLibraryElf(
        Util::StringView<char> prefix,
        Util::StringView<char> name) const;

    BoundGpuMemory  m_perfDataMem;
    gpusize         m_perfDataGpuMemSize;

    PAL_DISALLOW_DEFAULT_CTOR(ShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderLibrary);
};

} // namespace Pal
