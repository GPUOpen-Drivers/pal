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

#include "core/hw/ossip/oss2_4/oss2_4Device.h"
#include "core/hw/ossip/oss2_4/oss2_4DmaCmdBuffer.h"
#include "core/engine.h"
#include "core/queue.h"
#include "core/queueContext.h"
#include "palAssert.h"
#include "palSysMemory.h"

#include "core/hw/amdgpu_asic.h"

namespace Pal
{
namespace Oss2_4
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
    Result  result = Result::ErrorOutOfMemory;
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
// Determines the size of the QueueContext object needed for OSSIP2+ hardware. Only supported on DMA Queues.
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
        (*ppQueueContext) = PAL_PLACEMENT_NEW(pPlacementAddr) QueueContext(Parent());
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
// Determines the OSSIP level of a GPU supported by the OSS2.4 hardware layer. The return value will be OssIpLevel::None
// if the GPU is unsupported by this HWL.
OssIpLevel DetermineIpLevel(
    uint32 familyId, // Hardware Family ID.
    uint32 eRevId)   // Software Revision ID.
{
    OssIpLevel level = OssIpLevel::None;

    switch (familyId)
    {
    case FAMILY_VI:
    case FAMILY_CZ:
        level = OssIpLevel::OssIp2_4;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return level;
}

// =====================================================================================================================
// Initialize default values for the GPU engine properties for OSSIP 2.4 hardware.
void InitializeGpuEngineProperties(
    GpuEngineProperties* pInfo)
{
    auto*const pDma = &pInfo->perEngine[EngineTypeDma];

    pDma->flags.timestampSupport               = 1;
    pDma->flags.memoryPredicationSupport       = 1;

    // For gfx8(ossip 2.4) APUs(Carrizo, Bristol, ...), on SDMA queue that supports SVM mode, UMD has to
    // use SVM mode. That is said, non-SVM mode is not supported on SDMA queue that supports SVM mode. The
    // limitation does not exist on other queues and will not exsist on gfx9 APUs. So we don't report SDMA
    // queues that supports SVM mode(like SDMA1 on Bristol) as available for ossip 2.4 asics if client does
    // not switch on SVM mode. In Device::InitMemoryProperties we'll skip reporting the queue if
    // mustUseSvmIfSupported has been set to 1.
    pDma->flags.mustUseSvmIfSupported          = 1;
    pDma->minTiledImageCopyAlignment.width     = 8;
    pDma->minTiledImageCopyAlignment.height    = 8;
    pDma->minTiledImageCopyAlignment.depth     = 8;
    pDma->minTiledImageMemCopyAlignment.width  = 4;
    pDma->minTiledImageMemCopyAlignment.height = 1;
    pDma->minTiledImageMemCopyAlignment.depth  = 1;
    pDma->minLinearMemCopyAlignment.width      = 1;
    pDma->minLinearMemCopyAlignment.height     = 1;
    pDma->minLinearMemCopyAlignment.depth      = 1;
    pDma->minTimestampAlignment                = 32; // The OSSIP 2.4 spec requires 256-bit alignment.
    pDma->availableGdsSize                     = 0;
    pDma->gdsSizePerEngine                     = 0;
    pDma->queueSupport                         = SupportQueueTypeDma;
}

} // Oss2_4
} // Pal
