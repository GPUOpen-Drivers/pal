/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/swapChain.h"

namespace Pal
{
namespace Linux
{

class Device;
class PresentFence;
class WindowSystem;

// =====================================================================================================================
// The Linux SwapChain creates a WindowSystem which is necessary to create the swap chain's presentable images.
class SwapChain : public Pal::SwapChain
{
public:
    // The swap chain is a PAL interface object so it requires the Create/Destroy pattern.
    static size_t GetSize(const SwapChainCreateInfo& createInfo, const Device& device);

    static Result Create(
        const SwapChainCreateInfo& createInfo,
        Device*                    pDevice,
        void*                      pPlacementAddr,
        ISwapChain**               ppSwapChain);

    virtual Result PresentComplete(IQueue* pQueue, uint32 imageIndex) override;

    WindowSystem* GetWindowSystem() const { return m_pWindowSystem; }
    PresentFence* PresentIdleFence(uint32 imageIndex) { return m_pPresentIdle[imageIndex]; }

private:
    SwapChain(const SwapChainCreateInfo& createInfo, Device* pDevice);
    virtual ~SwapChain();

    virtual Result Init(void* pPlacementMem) override;

    virtual Result ReclaimUnusedImages(uint64 timeout) override;

    WindowSystem* m_pWindowSystem;
    PresentFence* m_pPresentIdle[MaxSwapChainLength]; // Signaled when each image is idle in the windowing system.

    PAL_DISALLOW_DEFAULT_CTOR(SwapChain);
    PAL_DISALLOW_COPY_AND_ASSIGN(SwapChain);
};

} // Linux
} // Pal
