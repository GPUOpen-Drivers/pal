/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/image.h"
#include "core/os/amdgpu/amdgpuWindowSystem.h"

namespace Pal
{
namespace Amdgpu
{

class Device;
class GpuMemory;
class WindowSystem;

constexpr uint32 InvalidImageIndex = -1;

// =====================================================================================================================
// Linux flavor of the Image class: primarily handles details regarding presentable and shared images
class Image : public Pal::Image
{
public:
    Image(
        Device*                        pDevice,
        const ImageCreateInfo&         createInfo,
        const ImageInternalCreateInfo& internalCreateInfo);

    virtual ~Image();

    static void GetImageSizes(
        const Device&                     device,
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult);

    static Result CreatePresentableImage(
        Device*                           pDevice,
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory);

    static void GetExternalSharedImageCreateInfo(
        const Device&                device,
        const ExternalImageOpenInfo& openInfo,
        const ExternalSharedInfo&    sharedInfo,
        ImageCreateInfo*             pCreateInfo);

    static Result CreateExternalSharedImage(
        Device*                       pDevice,
        const ExternalImageOpenInfo&  openInfo,
        const ExternalSharedInfo&     sharedInfo,
        void*                         pImagePlacementAddr,
        void*                         pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*          pMemCreateInfo,
        IImage**                      ppImage,
        IGpuMemory**                  ppGpuMemory);

    WindowSystemImageHandle GetPresentImageHandle() const { return m_presentImageHandle; }
    void SetPresentImageHandle(WindowSystemImageHandle handle) { m_presentImageHandle = handle; }

    static Result UpdateExternalImageInfo(
        Device*                             pDevice,
        const PresentableImageCreateInfo&   createInfo,
        Pal::GpuMemory*                     pGpuMemory,
        Pal::Image*                         pImage);

    static Result CreatePresentableMemoryObject(
        Device*                             pDevice,
        const PresentableImageCreateInfo&   createInfo,
        Image*                              pImage,
        void*                               pMemObjMem,
        Pal::GpuMemory**                    ppMemObjOut);

    SubResourceInfo* GetSubresourceInfo(uint32 subresId) const
    {
        return m_pSubResInfoList + subresId;
    }

    void* GetSubresourceTileInfo(uint32 subResId)
    {
        return Util::VoidPtrInc(m_pTileInfoList, (subResId * m_tileInfoBytes));
    }

    virtual void SetOptimalSharingLevel(MetadataSharingLevel level) override { PAL_NOT_IMPLEMENTED(); }
    virtual MetadataSharingLevel GetOptimalSharingLevel() const override { return MetadataSharingLevel::FullExpand; }

    void SetFramebufferId(uint32 fbId) { m_framebufferId = fbId; }
    uint32 GetFrameBufferId() const { return m_framebufferId; }

    void SetIdle(bool idle);
    bool GetIdle(void) const { return m_idle; }

    void SetWindowSystem(WindowSystem* pWindowSystem) { m_pWindowSystem = pWindowSystem; }

    SwapChain* GetSwapChain() { return m_pSwapChain; }
    void       SetSwapChain(SwapChain* pSwapChain) { m_pSwapChain = pSwapChain; }
    uint32     GetImageIndex() { return m_imageIndex; }
    void       SetImageIndex(uint32 imageIndex) { m_imageIndex = imageIndex; }

protected:
    virtual void UpdateMetaDataInfo(IGpuMemory* pGpuMemory) override;

private:

    // For X Window system it's a pixmap handle of the shared buffer used for presentation.
    // For direct rendering display, it's a handle of dma-buf.
    WindowSystemImageHandle  m_presentImageHandle;
    WindowSystem*            m_pWindowSystem;    // The window system that created the above handle.

    uint32        m_framebufferId;    // Framebuffer ID of the framebuffer to be presented.
    volatile bool m_idle;             // True indicates the image is not owned by Window System

    SwapChain*    m_pSwapChain;  // The swapchain the image is attached to.
    uint32        m_imageIndex;  // The image index in the swapchain.

    PAL_DISALLOW_DEFAULT_CTOR(Image);
    PAL_DISALLOW_COPY_AND_ASSIGN(Image);
};

} // Amdgpu
} // Pal
