/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerPipeline.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerShaderLibrary.h"
#include "palAutoBuffer.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
Pipeline::Pipeline(
    IPipeline*    pNextPipeline,
    const Device* pDevice,
    uint32        objectId)
    :
    PipelineDecorator(pNextPipeline, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
void Pipeline::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PipelineDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    PipelineDecorator::Destroy();
}

// =====================================================================================================================
Result Pipeline::LinkWithLibraries(
    const IShaderLibrary*const* ppLibraryList,
    uint32                      libraryCount)
{
    Result result = Result::Success;

    AutoBuffer<IShaderLibrary*, 16, Platform> nextLibraryList(libraryCount, m_pPlatform);
    if (nextLibraryList.Capacity() < libraryCount)
    {
        result = Result::ErrorOutOfMemory;
    }
    else
    {
        for (uint32 i = 0; i < libraryCount; i++)
        {
            nextLibraryList[i] = NextShaderLibrary(ppLibraryList[i]);
        }

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::PipelineLinkWithLibraries);

        result = m_pNextLayer->LinkWithLibraries(&nextLibraryList[0], libraryCount);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndBeginList("libraries", false);
            for (uint32 i = 0; i < libraryCount; ++i)
            {
                pLogContext->Object(ppLibraryList[i]);
            }
            pLogContext->EndList();
            pLogContext->EndInput();

            pLogContext->BeginOutput();
            pLogContext->KeyAndEnum("result", result);
            pLogContext->EndOutput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }

    return result;
}

} // InterfaceLogger
} // Pal

#endif
