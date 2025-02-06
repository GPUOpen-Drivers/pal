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

#pragma once

#if PAL_DEVELOPER_BUILD

#include "core/layers/decorators.h"

namespace Pal
{

namespace GpuDebug
{

// Forward decls
class TargetCmdBuffer;

struct CmdBufferTimestampData
{
    uint64 cmdBufferHash;
    uint32 counter;
};

// =====================================================================================================================
class Device final : public DeviceDecorator
{
public:
    Device(PlatformDecorator* pPlatform, IDevice* pNextDevice);

    size_t ColorViewSize() const { return m_colorViewSize; }
    size_t DepthViewSize() const { return m_depthViewSize; }

    static bool SupportsCommentString(QueueType queueType)
        { return ((queueType == QueueTypeUniversal) || (queueType == QueueTypeCompute)); }

    static bool SupportsCommentString(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo);

    uint32 BufferSrdDwords() const { return m_deviceProperties.gfxipProperties.srdSizes.typedBufferView; }
    uint32 ImageSrdDwords() const { return m_deviceProperties.gfxipProperties.srdSizes.imageView; }

    // Public IDevice interface methods:
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;
    size_t GetTargetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const;
    Result CreateTargetCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        TargetCmdBuffer**          ppCmdBuffer);

    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result* pResult) const override;

    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void* pPlacementAddr,
        IPipeline** ppPipeline) override;

    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual size_t GetMultiQueueSize(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        Result*                pResult) const override;

    virtual Result CreateMultiQueue(
        uint32                 queueCount,
        const QueueCreateInfo* pCreateInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual bool ImagePrefersCloneCopy(
        const ImageCreateInfo& createInfo) const override;

    virtual Result CreateImage(
        const ImageCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IImage**               ppImage) override;

    virtual void GetPresentableImageSizes(
        const PresentableImageCreateInfo& createInfo,
        size_t*                           pImageSize,
        size_t*                           pGpuMemorySize,
        Result*                           pResult) const override;

    virtual Result CreatePresentableImage(
        const PresentableImageCreateInfo& createInfo,
        void*                             pImagePlacementAddr,
        void*                             pGpuMemoryPlacementAddr,
        IImage**                          ppImage,
        IGpuMemory**                      ppGpuMemory) override;

    virtual Result GetExternalSharedImageSizes(
        const ExternalImageOpenInfo& openInfo,
        size_t*                      pImageSize,
        size_t*                      pGpuMemorySize,
        ImageCreateInfo*             pImgCreateInfo) const override;

    virtual Result OpenExternalSharedImage(
        const ExternalImageOpenInfo& openInfo,
        void*                        pImagePlacementAddr,
        void*                        pGpuMemoryPlacementAddr,
        GpuMemoryCreateInfo*         pMemCreateInfo,
        IImage**                     ppImage,
        IGpuMemory**                 ppGpuMemory) override;

    virtual void GetPrivateScreenImageSizes(
        const PrivateScreenImageCreateInfo& createInfo,
        size_t*                             pImageSize,
        size_t*                             pGpuMemorySize,
        Result*                             pResult) const override;

    virtual Result CreatePrivateScreenImage(
        const PrivateScreenImageCreateInfo& createInfo,
        void*                               pImagePlacementAddr,
        void*                               pGpuMemoryPlacementAddr,
        IImage**                            ppImage,
        IGpuMemory**                        ppGpuMemory) override;

    virtual size_t GetColorTargetViewSize(
        Result* pResult) const override;

    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorTargetView**               ppColorTargetView) const override;

    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const override;

    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IDepthStencilView**               ppDepthStencilView) const override;

    virtual size_t GetColorBlendStateSize() const override;

    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const override;

    virtual Result CommitSettingsAndInit() override;
    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;
    virtual Result Cleanup() override;

    const PalPublicSettings* PublicSettings() const { return m_pPublicSettings; }
    const DeviceProperties&  DeviceProps() const { return m_deviceProperties; }

private:
    virtual ~Device();

    const PalPublicSettings* m_pPublicSettings;
    DeviceProperties         m_deviceProperties;

    bool                   m_initialized;

    size_t                 m_colorViewSize;
    size_t                 m_depthViewSize;

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // GpuDebug
} // Pal

#endif
