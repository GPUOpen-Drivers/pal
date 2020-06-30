/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuDmaUploadRing.h"
#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuPlatform.h"
#include "core/os/amdgpu/amdgpuQueue.h"
#include "core/os/amdgpu/amdgpuSyncobjFence.h"
#include "core/os/amdgpu/amdgpuTimestampFence.h"

namespace Pal
{
namespace Amdgpu
{

// =====================================================================================================================
DmaUploadRing::DmaUploadRing(Device* pDevice)
    :Pal::DmaUploadRing(pDevice)
{

}

// =====================================================================================================================
Result DmaUploadRing::WaitForPendingUpload(
    Pal::Queue* pWaiter,
    UploadFenceToken fenceValue)
{
    Result      result = Result::Success;
    SubmissionContext* pContext = static_cast<SubmissionContext*>(m_pDmaQueue->GetSubmissionContext());

    // Make sure something has been submitted before attempting to wait for idle!
    PAL_ASSERT((pContext != nullptr) && (pContext->LastTimestamp() > 0));
    struct amdgpu_cs_fence queryFence = {};

    queryFence.context     = pContext->Handle();
    queryFence.fence       = fenceValue;
    queryFence.ring        = pContext->EngineId();
    queryFence.ip_instance = 0;
    queryFence.ip_type     = pContext->IpType();

    result = static_cast<Device*>(m_pDevice)->QueryFenceStatus(&queryFence, AMDGPU_TIMEOUT_INFINITE);
    return result;
}

}
}
