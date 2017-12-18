/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/layers/dbgOverlay/dbgOverlayCmdBuffer.h"
#include "core/layers/dbgOverlay/dbgOverlayDevice.h"
#include "core/layers/dbgOverlay/dbgOverlayFpsMgr.h"
#include "core/layers/dbgOverlay/dbgOverlayImage.h"
#include "core/layers/dbgOverlay/dbgOverlayPlatform.h"
#include "core/layers/dbgOverlay/dbgOverlayTextWriter.h"
#include "core/layers/dbgOverlay/dbgOverlayTimeGraph.h"

namespace Pal
{
namespace DbgOverlay
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*   pNextCmdBuffer,
    const Device* pDevice,
    QueueType     queueType)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pDevice),
    m_device(*pDevice),
    m_queueType(queueType),
    m_containsPresent(false)
{
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    m_containsPresent = false;

    return m_pNextLayer->Begin(info);
}

} // DbgOverlay
} // Pal
