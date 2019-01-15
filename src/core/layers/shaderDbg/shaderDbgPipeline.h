/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "palPipelineAbi.h"

// Forward decl's.
enum Sdl_HwShaderStage;
namespace Util { class File;  }

namespace Pal
{
namespace ShaderDbg
{

// Forward decl's.
class CmdBuffer;
class Device;
class Platform;

struct ShaderDumpInfo
{
    const CmdBuffer*  pCmdBuffer;
    Sdl_HwShaderStage hwStage;
    uint64            pipelineHash;
    uint64            compilerHash;
    bool              isDraw;
    uint32            uniqueId;
    uint32            submitId;
    Util::File*       pFile;
};

// =====================================================================================================================
class Pipeline : public PipelineDecorator
{
public:
    Pipeline(IPipeline* pNextPipeline, const Device* pDevice);
    Result Init(
        const void* pPipelineBinary,
        size_t      pipelineBinarySize);

    bool OpenUniqueDumpFile(
        const ShaderDumpInfo& dumpInfo) const;

    uint32 HwShaderDbgMask() const { return m_hwShaderDbgMask; }

private:
    virtual ~Pipeline() { }

    const Device*                    m_pDevice;
    Platform*                        m_pPlatform;
    uint32                           m_hwShaderDbgMask;
    Util::Abi::ApiHwShaderMapping    m_apiHwMapping;

    PAL_DISALLOW_DEFAULT_CTOR(Pipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

} // ShaderDbg
} // Pal
