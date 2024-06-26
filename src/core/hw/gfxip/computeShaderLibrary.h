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

#include "core/device.h"
#include "core/hw/gfxip/shaderLibrary.h"

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

// =====================================================================================================================
// Hardware independent compute library class. Implements all details of a compute library that are common across
// all hardware types but distinct from a graphics library.
class ComputeShaderLibrary : public ShaderLibrary
{
public:
    virtual void Destroy() override { this->~ComputeShaderLibrary(); }

    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const override;

    static void GetFunctionGpuVirtAddrs(
        const PipelineUploader&    uploader,
        ShaderLibraryFunctionInfo* pFuncInfoList,
        uint32                     funcCount);

    uint32 GetMaxStackSizeInBytes() const { return m_maxStackSizeInBytes; }
    UploadFenceToken GetUploadFenceToken() const { return m_uploadFenceToken; }
    uint64 GetPagingFenceVal() const { return m_pagingFenceVal; }

    virtual const Util::Span<const ShaderLibraryFunctionInfo> GetShaderLibFunctionInfos() const override
    {
        return m_functionList;
    }

protected:
    // internal Constructor.
    explicit ComputeShaderLibrary(Device* pDevice);

    // internal Destructor.
    virtual ~ComputeShaderLibrary();

    virtual Result PostInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pReader) override;

    Result PerformRelocationsAndUploadToGpuMemory(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const GpuHeap&                          clientPreferredHeap,
        PipelineUploader*                       pUploader);

    Result InitFunctionListFromMetadata(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pReader);

    BoundGpuMemory  m_gpuMem;
    gpusize         m_gpuMemSize;
    gpusize         m_gpuMemOffset;
    uint32          m_maxStackSizeInBytes;

    UploadFenceToken  m_uploadFenceToken;
    uint64            m_pagingFenceVal;

    Util::Vector<ShaderLibraryFunctionInfo, 4, Platform> m_functionList;

private:
    BoundGpuMemory  m_perfDataMem;
    gpusize         m_perfDataGpuMemSize;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeShaderLibrary);
};

} // namespace Pal
