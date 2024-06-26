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
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
GpuMemory::GpuMemory(
    IGpuMemory*   pNextGpuMemory,
    const Device* pDevice,
    uint32        objectId)
    :
    GpuMemoryDecorator(pNextGpuMemory, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
Result GpuMemory::SetPriority(
    GpuMemPriority       priority,
    GpuMemPriorityOffset priorityOffset)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::GpuMemorySetPriority);
    const Result result = GpuMemoryDecorator::SetPriority(priority, priorityOffset);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("priority", priority);
        pLogContext->KeyAndEnum("priorityOffset", priorityOffset);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::Map(
    void** ppData)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::GpuMemoryMap);
    const Result result = GpuMemoryDecorator::Map(ppData);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::Unmap()
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::GpuMemoryUnmap);
    const Result result = GpuMemoryDecorator::Unmap();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result GpuMemory::SetSdiRemoteBusAddress(
    gpusize surfaceBusAddr,
    gpusize markerBusAddr)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::GpuMemorySetSdiRemoteBusAddress);
    const Result result = GpuMemoryDecorator::SetSdiRemoteBusAddress(surfaceBusAddr, markerBusAddr);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("surfaceBusAddr", surfaceBusAddr);
        pLogContext->KeyAndValue("markerBusAddr", markerBusAddr);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
void GpuMemory::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::GpuMemoryDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    GpuMemoryDecorator::Destroy();
}

} // InterfaceLogger
} // Pal

#endif
