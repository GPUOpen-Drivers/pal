/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerShaderLibrary.h"

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
ShaderLibrary::ShaderLibrary(
    IShaderLibrary* pNextShaderLibrary,
    const Device*   pDevice,
    uint32          objectId)
    :
    ShaderLibraryDecorator(pNextShaderLibrary, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId)
{
}

// =====================================================================================================================
void ShaderLibrary::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::ShaderLibraryDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    ShaderLibraryDecorator::Destroy();
}

} // InterfaceLogger
} // Pal

#endif
