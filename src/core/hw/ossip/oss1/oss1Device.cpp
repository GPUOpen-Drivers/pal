/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/ossip/oss1/oss1Device.h"
#include "core/hw/ossip/oss1/oss1DmaCmdBuffer.h"
#include "core/engine.h"
#include "core/queue.h"
#include "core/queueContext.h"
#include "palAssert.h"
#include "palSysMemory.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Oss1
{

// =====================================================================================================================
size_t GetDeviceSize()
{
    return sizeof(Device);
}

// =====================================================================================================================
Result CreateDevice(
    Pal::Device* pDevice,
    void*        pPlacementAddr,
    OssDevice**  ppGfxDevice)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppGfxDevice != nullptr));
    (*ppGfxDevice) = PAL_PLACEMENT_NEW(pPlacementAddr) Device(pDevice);

    return Result::Success;
}

// =====================================================================================================================
Result Device::CreateEngine(
    EngineType engineType,
    uint32     engineIndex,
    Engine**   ppEngine)
{
    Result  result  = Result::ErrorOutOfMemory;
    Engine* pEngine = nullptr;

    switch (engineType)
    {
    case EngineTypeDma:
        pEngine = PAL_NEW(Engine, Parent()->GetPlatform(), Util::AllocInternal)(*Parent(), engineType, engineIndex);
        break;
    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        result = Result::ErrorInvalidValue;
        break;
    }

    if (pEngine != nullptr)
    {
        result = pEngine->Init();
    }

    if (result == Result::Success)
    {
        (*ppEngine) = pEngine;
    }

    return result;
}

// =====================================================================================================================
// Determines the size of the QueueContext object needed for OSSIP1 hardware. Only supported on DMA Queues.
size_t Device::GetQueueContextSize(
    const QueueCreateInfo& createInfo
    ) const
{
    size_t size = 0;

    switch (createInfo.queueType)
    {
    case QueueTypeDma:
        size = sizeof(QueueContext);
        break;
    default:
        break;
    }

    return size;
}

// =====================================================================================================================
// Creates the QueueContext object for the specified Queue in preallocated memory. Only supported on DMA Queues.
Result Device::CreateQueueContext(
    Queue*         pQueue,
    void*          pPlacementAddr,
    QueueContext** ppQueueContext)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppQueueContext != nullptr));

    Result result = Result::Success;

    switch (pQueue->Type())
    {
    case QueueTypeDma:
        (*ppQueueContext) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueContext();
        break;
    default:
        result = Result::ErrorUnavailable;
        break;
    }

    return result;
}

// =====================================================================================================================
// Determines the type of storage needed for a CmdBuffer.
size_t Device::GetCmdBufferSize() const
{
    return sizeof(DmaCmdBuffer);
}

// =====================================================================================================================
// Constructs a new CmdBuffer object in preallocated memory.
Result Device::CreateCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    void*                      pPlacementAddr,
    CmdBuffer**                ppCmdBuffer)
{
    PAL_ASSERT((pPlacementAddr != nullptr) && (ppCmdBuffer != nullptr));

    (*ppCmdBuffer) = PAL_PLACEMENT_NEW(pPlacementAddr) DmaCmdBuffer(this, createInfo);

    return Result::Success;
}

// =====================================================================================================================
// Determines the OSSIP level of a GPU supported by the OSS1 hardware layer. The return value will be OssIpLevel::None
// if the GPU is unsupported by this HWL.
OssIpLevel DetermineIpLevel(
    uint32 familyId, // Hardware Family ID.
    uint32 eRevId)   // Software Revision ID.
{
    PAL_ASSERT(FAMILY_IS_SI(familyId));
    return FAMILY_IS_SI(familyId) ? OssIpLevel::OssIp1 : OssIpLevel::None;
}

// =====================================================================================================================
// Initialize default values for the GPU engine properties for OSSIP 1 hardware.
void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo)
{
    auto*const pDma = &pInfo->perEngine[EngineTypeDma];

    pDma->flags.memoryPredicationSupport       = 1;
    pDma->minTiledImageCopyAlignment.width     = 8;
    pDma->minTiledImageCopyAlignment.height    = 8;
    pDma->minTiledImageCopyAlignment.depth     = 1;
    pDma->minTiledImageMemCopyAlignment.width  = 4;
    pDma->minTiledImageMemCopyAlignment.height = 1;
    pDma->minTiledImageMemCopyAlignment.depth  = 1;
    pDma->minLinearMemCopyAlignment.width      = 4;
    pDma->minLinearMemCopyAlignment.height     = 1;
    pDma->minLinearMemCopyAlignment.depth      = 1;
    pDma->availableGdsSize                     = 0;
    pDma->gdsSizePerEngine                     = 0;
    pDma->queueSupport                         = SupportQueueTypeDma;
}

} // Oss1
} // Pal
