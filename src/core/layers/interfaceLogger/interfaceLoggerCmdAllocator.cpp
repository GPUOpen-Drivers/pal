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

#include "core/layers/interfaceLogger/interfaceLoggerCmdAllocator.h"
#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
CmdAllocator::CmdAllocator(
    ICmdAllocator* pNextCmdAllocator,
    const Device*  pDevice,
    uint32         objectId)
    :
    CmdAllocatorDecorator(pNextCmdAllocator),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result CmdAllocator::Reset(
    bool freeMemory)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdAllocatorReset);
    const Result result = CmdAllocatorDecorator::Reset(freeMemory);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("freeMemory", freeMemory);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result CmdAllocator::Trim(
    uint32 allocTypeMask,
    uint32 dynamicThreshold)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdAllocatorTrim);
    const Result result = CmdAllocatorDecorator::Trim(allocTypeMask, dynamicThreshold);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("allocTypeMask", allocTypeMask);
        pLogContext->KeyAndValue("dynamicThreshold", dynamicThreshold);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void CmdAllocator::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdAllocatorDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    CmdAllocatorDecorator::Destroy();
}

} // InterfaceLogger
} // Pal

#endif
