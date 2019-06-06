/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/os/amdgpu/amdgpuDevice.h"
#include "core/os/amdgpu/amdgpuGpuMemory.h"
#include "core/os/amdgpu/amdgpuImage.h"

namespace Pal
{
namespace Amdgpu
{

class Platform;

// =====================================================================================================================
// Android flavor of the Device class. Objects of this class are responsible for creating Android presentable image and
// implementing the factory methods exposed by the public IDevice interface which are specific to Android platforms.
class AndroidDevice : public Device
{
public:

    explicit AndroidDevice(const DeviceConstructorParams& constructorParams);

    virtual ~AndroidDevice() {}

    virtual Result Cleanup() override;

    virtual Result GetSwapchainGrallocUsage(
        SwizzledFormat             format,
        ImageUsageFlags            imageUsage,
        int*                       pGrallocUsage) override;

    virtual Result AssociateNativeFence(
        int                        nativeFenceFd,
        IQueueSemaphore*           pPalSemaphore,
        IFence*                    pPalFence) override;

    virtual Pal::Queue*  ConstructQueueObject(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr) override;

    virtual Result UpdateExternalImageInfo(
        const PresentableImageCreateInfo&  createInfo,
        Pal::GpuMemory*                    pGpuMemory,
        Pal::Image*                        pImage) override;

    virtual Result CreatePresentableMemoryObject(
        const PresentableImageCreateInfo&  createInfo,
        Image*                             pImage,
        void*                              pMemObjMem,
        Pal::GpuMemory**                   ppMemObjOut) override;

private:
    virtual Result OsLateInit() override;

    amdgpu_syncobj_handle    m_syncobjForExternalSignaledFence;
    amdgpu_syncobj_handle    m_syncobjForExternalActiveFence;

    PAL_DISALLOW_DEFAULT_CTOR(AndroidDevice);
    PAL_DISALLOW_COPY_AND_ASSIGN(AndroidDevice);
};

} // Amdgpu
} // Pal
