/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/decorators.h"
#include "palList.h"
#include "palMutex.h"

namespace Pal
{

namespace ShaderDbg
{

// =====================================================================================================================
class Device : public DeviceDecorator
{
public:
    Device(PlatformDecorator* pPlatform, IDevice* pNextDevice);

    // Public IDevice interface methods:
    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo,
        Result*                    pResult) const override;
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        ICmdBuffer**               ppCmdBuffer) override;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;

    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IPipeline**                      ppPipeline) override;

    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        Result*                           pResult) const override;

    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo& createInfo,
        void*                             pPlacementAddr,
        IPipeline**                       ppPipeline) override;

    virtual size_t GetQueueSize(
        const QueueCreateInfo& createInfo,
        Result*                pResult) const override;

    virtual Result CreateQueue(
        const QueueCreateInfo& createInfo,
        void*                  pPlacementAddr,
        IQueue**               ppQueue) override;

    virtual Result CommitSettingsAndInit() override;
    virtual Result Finalize(const DeviceFinalizeInfo& finalizeInfo) override;
    virtual Result Cleanup() override;

    Result GetMemoryChunk(IGpuMemory** ppGpuMemory);
    Result ReleaseMemoryChunk(IGpuMemory* pGpuMemory);

    const PalPublicSettings* PublicSettings() const { return m_pPublicSettings; }
    const DeviceProperties&  DeviceProps() const { return m_deviceProperties; }

    const ShaderDbgSettings& ShaderDbgSettings() const { return m_shaderDbgSettings; }

private:
    virtual ~Device();

    const PalPublicSettings* m_pPublicSettings;
    DeviceProperties         m_deviceProperties;
    Pal::ShaderDbgSettings   m_shaderDbgSettings;

    bool                                        m_initialized;
    Util::Mutex                                 m_gpuMemoryLock;
    Util::List<IGpuMemory*, PlatformDecorator>  m_freeGpuMemory;
    Util::List<IGpuMemory*, PlatformDecorator>  m_usedGpuMemory;

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

} // ShaderDbg
} // Pal
