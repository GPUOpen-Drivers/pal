/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxBarrier.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/device.h"
#include "core/image.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
GfxBarrierMgr::GfxBarrierMgr(
    GfxDevice* pGfxDevice)
    :
    m_pGfxDevice(pGfxDevice),
    m_pDevice(pGfxDevice->Parent()),
    m_pPlatform(m_pDevice->GetPlatform())
{
}

// =====================================================================================================================
// Describes the image barrier to the above layers but only if we're a developer build. Clears the BarrierOperations
// passed in after calling back in case of layout transitions. This function is expected to be called only on layout
// transitions.
void GfxBarrierMgr::DescribeBarrier(
    GfxCmdBuffer*                 pGfxCmdBuf,
    const BarrierTransition*      pTransition,
    Developer::BarrierOperations* pOperations
    ) const
{
    constexpr BarrierTransition NullTransition = {};
    Developer::BarrierData data                = {};

    data.pCmdBuffer    = pGfxCmdBuf;
    data.transition    = (pTransition != nullptr) ? (*pTransition) : NullTransition;
    data.hasTransition = (pTransition != nullptr);

    PAL_ASSERT(pOperations != nullptr);
    // The callback is expected to be made only on layout transitions.
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    // Callback to the above layers if there is a transition and clear the BarrierOperations.
    m_pDevice->DeveloperCb(Developer::CallbackType::ImageBarrier, &data);
    memset(pOperations, 0, sizeof(Developer::BarrierOperations));
}

// =====================================================================================================================
// Call back to above layers before starting the barrier execution.
void GfxBarrierMgr::DescribeBarrierStart(
    GfxCmdBuffer*          pGfxCmdBuf,
    uint32                 reason,
    Developer::BarrierType type
    ) const
{
    Developer::BarrierData data = {};

    data.pCmdBuffer = pGfxCmdBuf;

    // Make sure we have an acceptable barrier reason.
    PAL_ALERT_MSG((m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled() &&
                  (reason == Developer::BarrierReasonInvalid)),
                  "Invalid barrier reason codes are not allowed!");

    data.reason = reason;
    data.type   = type;

    m_pDevice->DeveloperCb(Developer::CallbackType::BarrierBegin, &data);
}

// =====================================================================================================================
// Callback to above layers with summary information at end of barrier execution.
void GfxBarrierMgr::DescribeBarrierEnd(
    GfxCmdBuffer*                 pGfxCmdBuf,
    Developer::BarrierOperations* pOperations
    ) const
{
    Developer::BarrierData data  = {};

    // Set the barrier type to an invalid type.
    data.pCmdBuffer = pGfxCmdBuf;

    PAL_ASSERT(pOperations != nullptr);
    memcpy(&data.operations, pOperations, sizeof(Developer::BarrierOperations));

    m_pDevice->DeveloperCb(Developer::CallbackType::BarrierEnd, &data);
}

}
