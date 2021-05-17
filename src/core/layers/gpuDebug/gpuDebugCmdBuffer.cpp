/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_GPU_DEBUG

#include "core/layers/gpuDebug/gpuDebugCmdBuffer.h"
#include "core/layers/gpuDebug/gpuDebugColorBlendState.h"
#include "core/layers/gpuDebug/gpuDebugColorTargetView.h"
#include "core/layers/gpuDebug/gpuDebugDepthStencilView.h"
#include "core/layers/gpuDebug/gpuDebugDevice.h"
#include "core/layers/gpuDebug/gpuDebugImage.h"
#include "core/layers/gpuDebug/gpuDebugPipeline.h"
#include "core/layers/gpuDebug/gpuDebugQueue.h"
#include "core/g_palPlatformSettings.h"
#include "palAutoBuffer.h"
#include "palFile.h"
#include "palFormatInfo.h"
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
#include "palVectorImpl.h"
#endif
#include "palSysUtil.h"
#include "util/directDrawSurface.h"
#include <cinttypes>

// This is required because we need the definition of the D3D12DDI_PRESENT_0003 struct in order to make a copy of the
// data in it for the tokenization.

using namespace Util;

namespace Pal
{
namespace GpuDebug
{

constexpr uint32 MaxDepthTargetPlanes = 2;

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferDecorator(pNextCmdBuffer, static_cast<DeviceDecorator*>(pDevice->GetNextLayer())),
    m_pDevice(pDevice),
    m_allocator(1 * 1024 * 1024),
    m_supportsComments(Device::SupportsCommentString(createInfo.queueType)),
    m_singleStep(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.singleStep),
    m_cacheFlushInvOnAction(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.cacheFlushInvOnAction),
    m_breakOnDrawDispatchCount(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.submitOnActionCount),
    m_pTimestamp(nullptr),
    m_timestampAddr(0),
    m_counter(0),
    m_engineType(createInfo.engineType),
    m_verificationOptions(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.verificationOptions),
    m_pBoundPipeline(nullptr),
    m_boundTargets(),
    m_pBoundBlendState(nullptr),
    m_pTokenStream(nullptr),
    m_tokenStreamSize(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.tokenAllocatorSize),
    m_tokenWriteOffset(0),
    m_tokenReadOffset(0),
    m_tokenStreamResult(Result::Success),
    m_buildInfo(),
    m_pLastTgtCmdBuffer(nullptr)
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    ,
    m_numReleaseTokens(0),
    m_releaseTokenList(static_cast<Platform*>(m_pDevice->GetPlatform()))
#endif
{
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]  = &CmdBuffer::CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] = &CmdBuffer::CmdSetUserDataGfx;

    m_funcTable.pfnCmdDraw                      = CmdDraw;
    m_funcTable.pfnCmdDrawOpaque                = CmdDrawOpaque;
    m_funcTable.pfnCmdDrawIndexed               = CmdDrawIndexed;
    m_funcTable.pfnCmdDrawIndirectMulti         = CmdDrawIndirectMulti;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti  = CmdDrawIndexedIndirectMulti;
    m_funcTable.pfnCmdDispatch                  = CmdDispatch;
    m_funcTable.pfnCmdDispatchIndirect          = CmdDispatchIndirect;
    m_funcTable.pfnCmdDispatchOffset            = CmdDispatchOffset;
    m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMesh;
    m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMulti;

    memset(&m_flags, 0, sizeof(m_flags));
    m_flags.nested = createInfo.flags.nested;

    memset(&m_surfaceCapture, 0, sizeof(m_surfaceCapture));
    m_surfaceCapture.actionIdStart = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureDrawStart;
    m_surfaceCapture.actionIdCount = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureDrawCount;
    m_surfaceCapture.hash          = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureHash;
}

// =====================================================================================================================
CmdBuffer::~CmdBuffer()
{
    PAL_FREE(m_pTokenStream, m_pDevice->GetPlatform());

    DestroySurfaceCaptureData();

    if (m_surfaceCapture.ppColorTargetDsts != nullptr)
    {
        PAL_SAFE_FREE(m_surfaceCapture.ppColorTargetDsts, m_pDevice->GetPlatform());
    }

    if (m_surfaceCapture.ppDepthTargetDsts != nullptr)
    {
        PAL_SAFE_FREE(m_surfaceCapture.ppDepthTargetDsts, m_pDevice->GetPlatform());
    }

    if (m_surfaceCapture.ppGpuMem != nullptr)
    {
        PAL_SAFE_FREE(m_surfaceCapture.ppGpuMem, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
void* CmdBuffer::AllocTokenSpace(
    size_t numBytes,
    size_t alignment)
{
    void*        pTokenSpace        = nullptr;
    const size_t alignedWriteOffset = Pow2Align(m_tokenWriteOffset, alignment);
    const size_t nextWriteOffset    = alignedWriteOffset + numBytes;

    if (nextWriteOffset > m_tokenStreamSize)
    {
        // Double the size of the token stream until we have enough space.
        size_t newStreamSize = m_tokenStreamSize * 2;

        while (nextWriteOffset > newStreamSize)
        {
            newStreamSize *= 2;
        }

        // Allocate the new buffer and copy the current tokens over.
        void* pNewStream = PAL_MALLOC(newStreamSize, m_pDevice->GetPlatform(), AllocInternal);

        if (pNewStream != nullptr)
        {
            memcpy(pNewStream, m_pTokenStream, m_tokenWriteOffset);
            PAL_FREE(m_pTokenStream, m_pDevice->GetPlatform());

            m_pTokenStream    = pNewStream;
            m_tokenStreamSize = newStreamSize;
        }
        else
        {
            // We've run out of memory, this stream is now invalid.
            m_tokenStreamResult = Result::ErrorOutOfMemory;
        }
    }

    // Return null if we've previously encountered an error or just failed to reallocate the token stream. Otherwise,
    // return a properly aligned write pointer and update the write offset to point at the end of the allocated space.
    if (m_tokenStreamResult == Result::Success)
    {
        // Malloc is required to give us memory that is aligned high enough for any variable, but let's double check.
        PAL_ASSERT(IsPow2Aligned(reinterpret_cast<uint64>(m_pTokenStream), alignment));

        pTokenSpace        = VoidPtrInc(m_pTokenStream, alignedWriteOffset);
        m_tokenWriteOffset = nextWriteOffset;
    }

    return pTokenSpace;
}

// =====================================================================================================================
Result CmdBuffer::Init()
{
    Result result = m_allocator.Init();

    if ((result == Result::Success) && IsTimestampingActive())
    {
        DeviceProperties deviceProps = {};
        result = m_pDevice->GetProperties(&deviceProps);

        if (result == Result::Success)
        {
            GpuMemoryCreateInfo createInfo = {};
            createInfo.size                = sizeof(CmdBufferTimestampData);
            createInfo.alignment           = sizeof(uint64);
            createInfo.vaRange             = VaRange::Default;
            createInfo.priority            = GpuMemPriority::VeryLow;
            createInfo.heapCount           = 1;
            createInfo.heaps[0]            = GpuHeap::GpuHeapInvisible;
            createInfo.flags.virtualAlloc  = 1;

            m_pTimestamp = static_cast<IGpuMemory*>(PAL_MALLOC(m_pDevice->GetGpuMemorySize(createInfo, &result),
                                                               m_pDevice->GetPlatform(),
                                                               AllocInternal));

            if (m_pTimestamp != nullptr)
            {
                result = m_pDevice->CreateGpuMemory(createInfo, static_cast<void*>(m_pTimestamp), &m_pTimestamp);

                if (result != Result::Success)
                {
                    PAL_SAFE_FREE(m_pTimestamp, m_pDevice->GetPlatform());
                }
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            GpuMemoryRef memRef = {};
            memRef.pGpuMemory   = m_pTimestamp;
            result = m_pDevice->AddGpuMemoryReferences(1, &memRef, nullptr, GpuMemoryRefCantTrim);
        }

        if (result == Result::Success)
        {
            m_timestampAddr = m_pTimestamp->Desc().gpuVirtAddr;
        }
    }

    if (IsSurfaceCaptureEnabled())
    {
        const uint32 colorSurfCount = m_surfaceCapture.actionIdCount * MaxColorTargets;
        if (result == Result::Success)
        {
            m_surfaceCapture.ppColorTargetDsts = static_cast<Image**>(
                PAL_CALLOC(sizeof(Image*) * colorSurfCount,
                           m_pDevice->GetPlatform(),
                           AllocInternal));

            if (m_surfaceCapture.ppColorTargetDsts == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        const uint32 depthSurfCount = m_surfaceCapture.actionIdCount * MaxDepthTargetPlanes;
        if (result == Result::Success)
        {
            m_surfaceCapture.ppDepthTargetDsts = static_cast<Image**>(
                PAL_CALLOC(sizeof(Image*) * depthSurfCount,
                           m_pDevice->GetPlatform(),
                           AllocInternal));

            if (m_surfaceCapture.ppDepthTargetDsts == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        const uint32 totalSurfCount = colorSurfCount + depthSurfCount;
        if (result == Result::Success)
        {
            m_surfaceCapture.ppGpuMem = static_cast<IGpuMemory**>(
                PAL_CALLOC(sizeof(IGpuMemory*) * totalSurfCount,
                           m_pDevice->GetPlatform(),
                           AllocInternal));

            if (m_surfaceCapture.ppGpuMem == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::Destroy()
{
    if (IsTimestampingActive() && (m_pTimestamp != nullptr))
    {
        m_pDevice->RemoveGpuMemoryReferences(1, &m_pTimestamp, nullptr);
        m_pTimestamp->Destroy();
        PAL_SAFE_FREE(m_pTimestamp, m_pDevice->GetPlatform());
    }

    ICmdBuffer* pNextLayer = m_pNextLayer;
    this->~CmdBuffer();
    pNextLayer->Destroy();
}

// =====================================================================================================================
void CmdBuffer::AddTimestamp(
    gpusize     timestampAddr,
    uint32*     pCounter)
{
    (*pCounter)++;

    if (m_supportsComments)
    {
        char desc[256] = {};
        Snprintf(&desc[0],
                 sizeof(desc),
                 "Incrementing counter for the next event with counter value 0x%08x.",
                 (*pCounter));
        CmdCommentString(&desc[0]);
    }

    CmdWriteImmediate(HwPipePoint::HwPipeTop,
                      (*pCounter),
                      ImmediateDataWidth::ImmediateData32Bit,
                      timestampAddr + offsetof(CmdBufferTimestampData, counter));
}

// =====================================================================================================================
void CmdBuffer::AddSingleStepBarrier(
    uint32 counter)
{
    if (m_supportsComments)
    {
        char desc[256] = {};
        Snprintf(&desc[0], sizeof(desc), "Waiting for the previous event with counter value 0x%08x.", m_counter);
        CmdCommentString(&desc[0]);
    }

    BarrierInfo barrier = {};
    barrier.waitPoint   = HwPipePoint::HwPipeTop;

    constexpr HwPipePoint PipePoints[] =
    {
        HwPipePoint::HwPipeBottom,
        HwPipePoint::HwPipePostCs
    };
    barrier.pPipePoints        = &PipePoints[0];
    barrier.pipePointWaitCount = static_cast<uint32>(ArrayLen(PipePoints));
    CmdBarrierInternal(barrier);
}

// =====================================================================================================================
void CmdBuffer::AddCacheFlushInv()
{
    BarrierInfo barrierInfo = {};

    barrierInfo.waitPoint = HwPipePoint::HwPipeTop;

    const HwPipePoint pipePoint    = HwPipePoint::HwPipeBottom;
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints        = &pipePoint;

    BarrierTransition transition = {};
    transition.srcCacheMask = CoherAllUsages;
    transition.dstCacheMask = CoherAllUsages;

    barrierInfo.transitionCount = 1;
    barrierInfo.pTransitions    = &transition;

    CmdBarrierInternal(barrierInfo);
}

// =====================================================================================================================
// Returns true if surface capture is active at the current point of recording in this command buffer
bool CmdBuffer::IsSurfaceCaptureActive() const
{
    return ((m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart) &&
            (m_surfaceCapture.actionId < (m_surfaceCapture.actionIdStart + m_surfaceCapture.actionIdCount)) &&
            m_surfaceCapture.pipelineMatch);
}

// =====================================================================================================================
// Determines if the current pipeline hash matches the surface capture hash
void CmdBuffer::SurfaceCaptureHashMatch()
{
    m_surfaceCapture.pipelineMatch = false;

    if (IsSurfaceCaptureEnabled() &&
        (m_pBoundPipeline != nullptr))
    {
        const PipelineInfo& pipeInfo = m_pBoundPipeline->GetInfo();

        m_surfaceCapture.pipelineMatch =
            (m_surfaceCapture.hash == 0) ||
            (pipeInfo.internalPipelineHash.stable == m_surfaceCapture.hash) ||
            (pipeInfo.internalPipelineHash.unique == m_surfaceCapture.hash);

        for (uint32 i = 0; i < NumShaderTypes; i++)
        {
            m_surfaceCapture.pipelineMatch |= (pipeInfo.shader[i].hash.lower == m_surfaceCapture.hash);
            m_surfaceCapture.pipelineMatch |= (pipeInfo.shader[i].hash.upper == m_surfaceCapture.hash);
        }
    }
}

// =====================================================================================================================
// Creates images and memory for surface capture and copies data to those images
void CmdBuffer::CaptureSurfaces()
{
    for (uint32 mrt = 0; mrt < m_boundTargets.colorTargetCount; mrt++)
    {
        const ColorTargetView* pCtv =
            static_cast<const ColorTargetView*>(m_boundTargets.colorTargets[mrt].pColorTargetView);

        if (pCtv != nullptr)
        {
            const ColorTargetViewCreateInfo& ctvCreateInfo = pCtv->GetCreateInfo();

            if (ctvCreateInfo.flags.isBufferView == false)
            {
                const IImage* pSrcImage = ctvCreateInfo.imageInfo.pImage;

                IImage* pDstImage = nullptr;
                Result result = CaptureImageSurface(
                    pSrcImage,
                    ctvCreateInfo.imageInfo.baseSubRes,
                    ctvCreateInfo.imageInfo.arraySize,
                    &pDstImage);

                if (result == Result::Success)
                {
                    // Store the image object pointer in our array of capture data
                    PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
                    const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
                    PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

                    const uint32 idx = (actionIndex * MaxColorTargets) + mrt;

                    PAL_ASSERT(m_surfaceCapture.ppColorTargetDsts[idx] == nullptr);
                    m_surfaceCapture.ppColorTargetDsts[idx] = static_cast<Image*>(pDstImage);
                }
                else
                {
                    PAL_DPWARN("Failed to capture RT%d, Error:0x%x", mrt, result);
                }
            }
            else
            {
                // Buffer view of RTV
                PAL_DPWARN("Failed to capture RT%d. Capture of buffer views of RTs is not supported", mrt);
            }
        }
    }

    if (m_boundTargets.depthTarget.pDepthStencilView != nullptr)
    {
        const DepthStencilView* pDsv =
            static_cast<const DepthStencilView*>(m_boundTargets.depthTarget.pDepthStencilView);

        if (pDsv != nullptr)
        {
            const DepthStencilViewCreateInfo& dsvCreateInfo = pDsv->GetCreateInfo();

            const IImage* pSrcImage = dsvCreateInfo.pImage;

            uint32 numPlanes = 1;
            Result result = Result::Success;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            SubresRange range = { };
            result = pSrcImage->GetFullSubresourceRange(&range);
            if (result == Result::Success)
            {
                numPlanes = range.numPlanes;
            }
#endif

            for (uint32 plane = 0; plane < numPlanes; plane++)
            {
                IImage* pDstImage = nullptr;

                SubresId subresId   = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                subresId.plane      = plane;
#endif
                subresId.mipLevel   = dsvCreateInfo.mipLevel;
                subresId.arraySlice = dsvCreateInfo.baseArraySlice;

                result = CaptureImageSurface(
                    pSrcImage,
                    subresId,
                    dsvCreateInfo.arraySize,
                    &pDstImage);

                if (result == Result::Success)
                {
                    // Store the image object pointer in our array of capture data
                    PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
                    const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
                    PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

                    const uint32 idx = (actionIndex * MaxDepthTargetPlanes) + plane;

                    PAL_ASSERT(m_surfaceCapture.ppDepthTargetDsts[idx] == nullptr);
                    m_surfaceCapture.ppDepthTargetDsts[idx] = static_cast<Image*>(pDstImage);
                }
                else
                {
                    PAL_DPWARN("Failed to capture DSV Plane:%d, Error:0x%x", plane, result);
                }
            }
        }
    }
}

// =====================================================================================================================
// Helper function for CaptureSurfaces()
// Allocates a destination image and backing memory. Then copies from the src to the dst
Result CmdBuffer::CaptureImageSurface(
    const IImage*   pSrcImage,      // [in] pointer to the surface to capture
    const SubresId& baseSubres,     // Specifies the plane, mip level, and base array slice.
                                    // The plane portion is ignored. All planes are captured.
    uint32          arraySize,
    IImage**        ppDstImage)     // [out] pointer to the surface that has the capture data
{
    PAL_ASSERT(pSrcImage != nullptr);

    // Create the image object which will hold our captured data
    ImageCreateInfo imageCreateInfo         = pSrcImage->GetImageCreateInfo();
    imageCreateInfo.flags.u32All            = 0;
    imageCreateInfo.usageFlags.u32All       = 0;
    imageCreateInfo.usageFlags.colorTarget  = 1;
    imageCreateInfo.tiling                  = ImageTiling::Linear;
    imageCreateInfo.viewFormatCount         = AllCompatibleFormats;
    imageCreateInfo.pViewFormats            = nullptr;

    if (pSrcImage->GetImageCreateInfo().usageFlags.depthStencil == 1)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
        const uint32 plane = baseSubres.plane;
#else
        const uint32 plane = 0;
#endif
        OverrideDepthFormat(&imageCreateInfo.swizzledFormat, pSrcImage, plane);
    }

    Result result = Result::Success;
    const size_t imageSize = m_pDevice->GetImageSize(imageCreateInfo, &result);

    void* pDstImageMem = nullptr;

    if (result == Result::Success)
    {
        pDstImageMem = PAL_MALLOC(imageSize, m_pDevice->GetPlatform(), AllocInternal);

        if (pDstImageMem == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    IImage* pDstImage = nullptr;

    if (result == Result::Success)
    {
        result = m_pDevice->CreateImage(imageCreateInfo, pDstImageMem, &pDstImage);

        if (result != Result::Success)
        {
            PAL_SAFE_FREE(pDstImageMem, m_pDevice->GetPlatform());
        }
    }

    if (result == Result::Success)
    {
        // Create the backing memory for our image and attach it
        PAL_ASSERT(pDstImageMem == pDstImage);

        GpuMemoryRequirements gpuMemReqs = { 0 };
        pDstImage->GetGpuMemoryRequirements(&gpuMemReqs);

        GpuMemoryCreateInfo gpuMemCreateInfo = { 0 };
        gpuMemCreateInfo.size       = gpuMemReqs.size;
        gpuMemCreateInfo.alignment  = gpuMemReqs.alignment;
        gpuMemCreateInfo.vaRange    = VaRange::Default;
        gpuMemCreateInfo.priority   = GpuMemPriority::Normal;
        gpuMemCreateInfo.heapCount  = 3;
        gpuMemCreateInfo.heaps[0]   = GpuHeap::GpuHeapLocal;
        gpuMemCreateInfo.heaps[1]   = GpuHeap::GpuHeapGartUswc;
        gpuMemCreateInfo.heaps[2]   = GpuHeap::GpuHeapGartCacheable;
        const size_t gpuMemSize = m_pDevice->GetGpuMemorySize(gpuMemCreateInfo, &result);

        void* pGpuMemMem = nullptr;
        pGpuMemMem = PAL_MALLOC(gpuMemSize, m_pDevice->GetPlatform(), AllocInternal);

        if (pGpuMemMem == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            IGpuMemory* pGpuMem = nullptr;
            result = m_pDevice->CreateGpuMemory(gpuMemCreateInfo, pGpuMemMem, &pGpuMem);

            if (result == Result::Success)
            {
                result = pDstImage->BindGpuMemory(pGpuMem, 0);

                if (result == Result::Success)
                {
                    m_surfaceCapture.ppGpuMem[m_surfaceCapture.gpuMemObjsCount] = pGpuMem;
                    m_surfaceCapture.gpuMemObjsCount++;
                }
            }
            else
            {
                PAL_SAFE_FREE(pGpuMemMem, m_pDevice->GetPlatform());
            }
        }
    }

    // Copy
    if (result == Result::Success)
    {
        ImageLayout srcLayout = { 0 };
        srcLayout.usages  = LayoutColorTarget | LayoutCopySrc;
        srcLayout.engines = LayoutUniversalEngine;

        ImageLayout dstLayout = { 0 };
        dstLayout.usages  = LayoutCopyDst;
        dstLayout.engines = LayoutUniversalEngine;

        ImageCopyRegion region;
        memset(&region, 0, sizeof(region));
        region.srcSubres        = baseSubres;
        region.dstSubres        = baseSubres;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
        region.dstSubres.plane  = 0;
#endif
        region.numSlices        = arraySize;

        const uint32 mipDivisor = (1 << baseSubres.mipLevel);

        region.extent.width  = imageCreateInfo.extent.width / mipDivisor;
        region.extent.height = imageCreateInfo.extent.height / mipDivisor;
        region.extent.depth  = imageCreateInfo.extent.depth / mipDivisor;

        CmdCopyImage(*pSrcImage,
                     srcLayout,
                     *pDstImage,
                     dstLayout,
                     1,
                     &region,
                     nullptr,
                     0);
    }

    *ppDstImage = pDstImage;

    return result;
}

// =====================================================================================================================
// Changes the input format to a format that matches the component of the input plane.
// This function is only valid on depth/stencil images
void CmdBuffer::OverrideDepthFormat(
    SwizzledFormat*     pSwizzledFormat,
    const IImage*       pSrcImage,
    uint32              plane)
{
    PAL_ASSERT(pSwizzledFormat != nullptr);

    if (plane < Pal::Formats::NumComponents(pSwizzledFormat->format))
    {
        const uint32 planeBitCount =
            Pal::Formats::ComponentBitCounts(pSwizzledFormat->format)[plane];

        if (planeBitCount == 8)
        {
            pSwizzledFormat->format   = ChNumFormat::X8_Uint;
            pSwizzledFormat->swizzle.swizzle[0] = ChannelSwizzle::X;
            pSwizzledFormat->swizzle.swizzle[1] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[2] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[3] = ChannelSwizzle::Zero;
        }
        else if (planeBitCount == 16)
        {
            pSwizzledFormat->format   = ChNumFormat::X16_Unorm;
            pSwizzledFormat->swizzle.swizzle[0] = ChannelSwizzle::X;
            pSwizzledFormat->swizzle.swizzle[1] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[2] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[3] = ChannelSwizzle::Zero;
        }
        else if (planeBitCount == 32)
        {
            pSwizzledFormat->format   = ChNumFormat::X32_Float;
            pSwizzledFormat->swizzle.swizzle[0] = ChannelSwizzle::X;
            pSwizzledFormat->swizzle.swizzle[1] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[2] = ChannelSwizzle::Zero;
            pSwizzledFormat->swizzle.swizzle[3] = ChannelSwizzle::Zero;
        }
    }
}

// =====================================================================================================================
// Writes the data for surface capture to disk.
// This function must be called only after this command buffer has finished execution.
void CmdBuffer::OutputSurfaceCapture()
{
    Result result = Result::Success;
    char filePath[256] = {};

    result = m_pDevice->GetPlatform()->CreateLogDir(
        m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureLogDirectory);

    if (result == Result::Success)
    {
        Snprintf(filePath, sizeof(filePath), "%s", m_pDevice->GetPlatform()->LogDirPath());

        result = MkDir(&filePath[0]);
    }

    const size_t endOfString = strlen(&filePath[0]);

    if ((result == Result::Success) || (result == Result::AlreadyExists))
    {
        for (uint32 action = 0; action < m_surfaceCapture.actionIdCount; action++)
        {
            char fileName[256] = {};

            if (m_surfaceCapture.ppColorTargetDsts != nullptr)
            {
                // Output render targets
                for (uint32 mrt = 0; mrt < MaxColorTargets; mrt++)
                {
                    const uint32 idx = (action * MaxColorTargets) + mrt;
                    Image* pImage    = m_surfaceCapture.ppColorTargetDsts[idx];

                    if (pImage != nullptr)
                    {
                        Snprintf(fileName,
                                 sizeof(fileName),
                                 "Draw%d_RT%d__TS0x%llx",
                                 m_surfaceCapture.actionIdStart + action,
                                 mrt,
                                 GetPerfCpuTime());

                        OutputSurfaceCaptureImage(
                            pImage,
                            &filePath[0],
                            &fileName[0]);
                    }
                }
            }

            if (m_surfaceCapture.ppDepthTargetDsts != nullptr)
            {
                // Output depth stencil
                for (uint32 plane = 0; plane < 2; plane++)
                {
                    const uint32 idx = (action * MaxDepthTargetPlanes) + plane;
                    Image* pImage = m_surfaceCapture.ppDepthTargetDsts[idx];

                    if (pImage != nullptr)
                    {
                        Snprintf(fileName,
                                 sizeof(fileName),
                                 "Draw%d_DSV%d__TS0x%llx",
                                 m_surfaceCapture.actionIdStart + action,
                                 plane,
                                 GetPerfCpuTime());

                        OutputSurfaceCaptureImage(
                            pImage,
                            &filePath[0],
                            &fileName[0]);
                    }
                }
            }
        }
    }
}

// =====================================================================================================================
// Outputs the input image to disk using the path and file name. If possible, the image is output as a .DDS image
void CmdBuffer::OutputSurfaceCaptureImage(
    Image*      pImage,
    const char* pFilePath,
    const char* pFileName
    ) const
{
    PAL_ASSERT(pImage != nullptr);
    PAL_ASSERT(pFilePath != nullptr);
    PAL_ASSERT(pFileName != nullptr);

    void*  pImageMap = nullptr;
    Result result = pImage->GetBoundMemory()->Map(&pImageMap);

    if (result == Result::Success)
    {
        ImageCreateInfo imageInfo = pImage->GetImageCreateInfo();

        bool            canUseDds = false;
        DdsHeaderFull   ddsHeader  = {0};
        size_t          ddsHeaderSize = 0;

        if (imageInfo.mipLevels == 1)
        {
            SubresId subresId = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            subresId.aspect     = ImageAspect::Color;
#else
            subresId.plane      = 0;
#endif
            subresId.mipLevel   = 0;
            subresId.arraySlice = 0;

            SubresLayout subresLayout = {0};
            pImage->GetSubresourceLayout(subresId, &subresLayout);

            Result ddsResult = GetDdsHeader(&ddsHeader,
                                            &ddsHeaderSize,
                                            imageInfo.imageType,
                                            imageInfo.swizzledFormat,
                                            imageInfo.arraySize,
                                            &subresLayout);

            if (ddsResult == Result::Success)
            {
                canUseDds = true;
            }
        }

        char filePathNameExt[512] = {};

        Snprintf(filePathNameExt,
                 sizeof(filePathNameExt),
                 "%s/%s.%s",
                 pFilePath,
                 pFileName,
                 canUseDds ? "dds" : "bin");

        File outFile;
        result = outFile.Open(&(filePathNameExt[0]), FileAccessBinary | FileAccessWrite);

        if ((result == Result::Success) && outFile.IsOpen())
        {
            if (canUseDds)
            {
                outFile.Write(&ddsHeader, ddsHeaderSize);
            }

            outFile.Write(pImageMap, static_cast<size_t>(pImage->GetMemoryLayout().dataSize));

            outFile.Flush();
            outFile.Close();
        }

        pImage->GetBoundMemory()->Unmap();
    }

    if (result != Result::Success)
    {
        PAL_DPWARN("Surface Capture failed to output image 0xllx%x, Error:0x%x", pImage, result);
    }
}

// =====================================================================================================================
// Deallocates the memory created to hold captured surfaces
void CmdBuffer::DestroySurfaceCaptureData()
{
    if (m_surfaceCapture.ppColorTargetDsts != nullptr)
    {
        for (uint32 i = 0; i < (m_surfaceCapture.actionIdCount * MaxColorTargets); i++)
        {
            if (m_surfaceCapture.ppColorTargetDsts[i] != nullptr)
            {
                m_surfaceCapture.ppColorTargetDsts[i]->Destroy();
                PAL_SAFE_FREE(m_surfaceCapture.ppColorTargetDsts[i], m_pDevice->GetPlatform());
            }
        }
    }

    if (m_surfaceCapture.ppDepthTargetDsts != nullptr)
    {
        for (uint32 i = 0; i < m_surfaceCapture.actionIdCount * MaxDepthTargetPlanes; i++)
        {
            if (m_surfaceCapture.ppDepthTargetDsts[i] != nullptr)
            {
                m_surfaceCapture.ppDepthTargetDsts[i]->Destroy();
                PAL_SAFE_FREE(m_surfaceCapture.ppDepthTargetDsts[i], m_pDevice->GetPlatform());
            }
        }
    }

    if (m_surfaceCapture.ppGpuMem != nullptr)
    {
        for (uint32 i = 0; i < m_surfaceCapture.gpuMemObjsCount; i++)
        {
            if (m_surfaceCapture.ppGpuMem[i] != nullptr)
            {
                m_surfaceCapture.ppGpuMem[i]->Destroy();
                PAL_SAFE_FREE(m_surfaceCapture.ppGpuMem[i], m_pDevice->GetPlatform());
            }
        }

        m_surfaceCapture.gpuMemObjsCount = 0;
    }
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    m_counter           = 0;
    m_pLastTgtCmdBuffer = nullptr;
    m_pBoundPipeline    = nullptr;
    m_pBoundBlendState  = nullptr;
    memset(&m_boundTargets, 0, sizeof(m_boundTargets));
    memset(&m_buildInfo, 0, sizeof(m_buildInfo));
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    m_releaseTokenList.Clear();
    m_numReleaseTokens = 0;
#endif

    m_surfaceCapture.actionId = 0;

    DestroySurfaceCaptureData();

    return GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    m_pLastTgtCmdBuffer = nullptr;
    m_counter           = 0;

    // Reset the token stream state so that we can reuse our old token stream buffer.
    m_tokenWriteOffset = 0;
    m_tokenReadOffset = 0;
    m_tokenStreamResult = Result::Success;

    // We lazy allocate the first token stream during the first Begin() call to avoid allocating a lot of extra
    // memory if the client creates a ton of command buffers but doesn't use them.
    if (m_pTokenStream == nullptr)
    {
        m_pTokenStream = PAL_MALLOC(m_tokenStreamSize, m_pDevice->GetPlatform(), AllocInternal);

        if (m_pTokenStream == nullptr)
        {
            m_tokenStreamResult = Result::ErrorOutOfMemory;
        }
    }

    m_buildInfo                 = info;
    m_buildInfo.pInheritedState = {};

    InsertToken(CmdBufCallId::Begin);
    InsertToken(info);
    if (info.pInheritedState != nullptr)
    {
        InsertToken(*info.pInheritedState);
    }

    // We should return an error immediately if we couldn't allocate enough token memory for the Begin call.
    Result result = m_tokenStreamResult;

    if (result == Result::Success)
    {
        // Note that Begin() is immediately forwarded to the next layer.  This is only necessary in order to support
        // clients that use CmdAllocateEmbeddedData().  They immediately need a CPU address corresponding to GPU memory
        // with the lifetime of this command buffer, so it is easiest to just let it go through the normal path.
        // The core layer's command buffer will be filled entirely with embedded data.
        //
        // This is skipped for command buffers based on VideoEncodeCmdBuffers because those command buffers do not
        // reset their state (or even really build the command buffer) until that command buffer is submitted.  The GPU
        // profiler layer instead internally replaces and submits a different command buffer which leaves this one
        // permanently in Building state the next time Begin() is called on it.
        {
            result = m_pNextLayer->Begin(NextCmdBufferBuildInfo(info));
        }
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::ReplayBegin(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    auto info   = ReadTokenVal<CmdBufferBuildInfo>();

    InheritedStateParams inheritedState = {};
    if (info.pInheritedState != nullptr)
    {
        PAL_ASSERT(m_pLastTgtCmdBuffer == nullptr);
        inheritedState = ReadTokenVal<InheritedStateParams>();
        info.pInheritedState = &inheritedState;
    }

    if (m_pLastTgtCmdBuffer != nullptr)
    {
        // If we have a record of the last targeted command buffer and we've seen a BeginToken, we need to
        // attempt to inherit the state from it.
        info.pStateInheritCmdBuffer = m_pLastTgtCmdBuffer;
    }

    // Now that we've used our last targeted command buffer for inheritance, we can update the record with the new
    // command buffer.
    m_pLastTgtCmdBuffer = pTgtCmdBuffer;

    // We must remove the client's external allocator because PAL can only use it during command building from the
    // client's perspective. By batching and replaying command building later on we're breaking that rule. The good news
    // is that we can replace it with our queue's command buffer replay allocator because replaying is thread-safe with
    // respect to each queue.
    info.pMemAllocator = pQueue->ReplayAllocator();

    Result result = pTgtCmdBuffer->Begin(info);// NextCmdBufferBuildInfo(info));

    if (IsTimestampingActive())
    {
        if (m_supportsComments)
        {
            char buffer[256] = {};
            Snprintf(&buffer[0], sizeof(buffer),
                     "Updating CmdBuffer Hash to 0x%016llX.", reinterpret_cast<uint64>(this));
            pTgtCmdBuffer->CmdCommentString(&buffer[0]);
            Snprintf(&buffer[0], sizeof(buffer), "Resetting counter to 0.");
            pTgtCmdBuffer->CmdCommentString(&buffer[0]);
        }

        pTgtCmdBuffer->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                         reinterpret_cast<uint64>(this),
                                         ImmediateDataWidth::ImmediateData64Bit,
                                         m_timestampAddr + offsetof(CmdBufferTimestampData, cmdBufferHash));
        pTgtCmdBuffer->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                         0,
                                         ImmediateDataWidth::ImmediateData32Bit,
                                         m_timestampAddr + offsetof(CmdBufferTimestampData, counter));
    }

    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    InsertToken(CmdBufCallId::End);

    // See CmdBuffer::Begin() for comment on why Begin()/End() are immediately passed to the next layer.
    Result result = GetNextLayer()->End();

    // If no errors occured during End() perhaps an error occured while we were recording tokens. If so that means
    // the token stream and this command buffer are both invalid.
    if (result == Result::Success)
    {
        result = m_tokenStreamResult;
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::ReplayEnd(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    PAL_ASSERT(m_numReleaseTokens == m_releaseTokenList.NumElements());
#endif

    Result result = pTgtCmdBuffer->End();
    pTgtCmdBuffer->SetLastResult(result);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    m_pBoundPipeline = params.pPipeline;

    SurfaceCaptureHashMatch();

    InsertToken(CmdBufCallId::CmdBindPipeline);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindPipeline(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    const PipelineBindParams params = ReadTokenVal<PipelineBindParams>();
    pTgtCmdBuffer->CmdBindPipeline(params);
}

// =====================================================================================================================
void CmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    InsertToken(CmdBufCallId::CmdBindMsaaState);
    InsertToken(pMsaaState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindMsaaState(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindMsaaState(ReadTokenVal<IMsaaState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    m_pBoundBlendState = pColorBlendState;

    InsertToken(CmdBufCallId::CmdBindColorBlendState);
    InsertToken(pColorBlendState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindColorBlendState(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindColorBlendState(ReadTokenVal<IColorBlendState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    InsertToken(CmdBufCallId::CmdBindDepthStencilState);
    InsertToken(pDepthStencilState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindDepthStencilState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindDepthStencilState(ReadTokenVal<IDepthStencilState*>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    InsertToken(CmdBufCallId::CmdBindIndexData);
    InsertToken(gpuAddr);
    InsertToken(indexCount);
    InsertToken(indexType);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindIndexData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto gpuAddr    = ReadTokenVal<gpusize>();
    auto indexCount = ReadTokenVal<uint32>();
    auto indexType  = ReadTokenVal<IndexType>();

    pTgtCmdBuffer->CmdBindIndexData(gpuAddr, indexCount, indexType);
}

// =====================================================================================================================
void CmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    m_boundTargets = params;

    InsertToken(CmdBufCallId::CmdBindTargets);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindTargets(ReadTokenVal<BindTargetParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    InsertToken(CmdBufCallId::CmdBindStreamOutTargets);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindStreamOutTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindStreamOutTargets(ReadTokenVal<BindStreamOutTargetParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    InsertToken(CmdBufCallId::CmdBindBorderColorPalette);
    InsertToken(pipelineBindPoint);
    InsertToken(pPalette);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindBorderColorPalette(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipelineBindPoint = ReadTokenVal<PipelineBindPoint>();
    auto pPalette          = ReadTokenVal<IBorderColorPalette*>();

    pTgtCmdBuffer->CmdBindBorderColorPalette(pipelineBindPoint, pPalette);
}

// =====================================================================================================================
void CmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    InsertToken(CmdBufCallId::CmdPrimeGpuCaches);
    InsertTokenArray(pRanges, rangeCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdPrimeGpuCaches(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const PrimeGpuCacheRange* pRanges    = nullptr;
    const auto                rangeCount = ReadTokenArray(&pRanges);

    pTgtCmdBuffer->CmdPrimeGpuCaches(rangeCount, pRanges);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    pCmdBuf->InsertToken(CmdBufCallId::CmdSetUserData);
    pCmdBuf->InsertToken(PipelineBindPoint::Compute);
    pCmdBuf->InsertToken(firstEntry);
    pCmdBuf->InsertTokenArray(pEntryValues, entryCount);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    pCmdBuf->InsertToken(CmdBufCallId::CmdSetUserData);
    pCmdBuf->InsertToken(PipelineBindPoint::Graphics);
    pCmdBuf->InsertToken(firstEntry);
    pCmdBuf->InsertTokenArray(pEntryValues, entryCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetUserData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pipelineBindPoint = ReadTokenVal<PipelineBindPoint>();
    auto          firstEntry        = ReadTokenVal<uint32>();
    const uint32* pEntryValues      = nullptr;
    auto          entryCount        = ReadTokenArray(&pEntryValues);

    pTgtCmdBuffer->CmdSetUserData(pipelineBindPoint, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void CmdBuffer::CmdSetVertexBuffers(
    uint32                firstBuffer,
    uint32                bufferCount,
    const BufferViewInfo* pBuffers)
{
    InsertToken(CmdBufCallId::CmdSetVertexBuffers);
    InsertToken(firstBuffer);
    InsertTokenArray(pBuffers, bufferCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetVertexBuffers(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const BufferViewInfo* pBuffers = nullptr;
    const auto firstBuffer = ReadTokenVal<uint32>();
    const auto bufferCount = ReadTokenArray(&pBuffers);

    pTgtCmdBuffer->CmdSetVertexBuffers(firstBuffer, bufferCount, pBuffers);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    InsertToken(CmdBufCallId::CmdSetBlendConst);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams& rateParams)
{
    InsertToken(CmdBufCallId::CmdSetPerDrawVrsRate);
    InsertToken(rateParams);
}
// =====================================================================================================================
void CmdBuffer::ReplayCmdSetPerDrawVrsRate(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetPerDrawVrsRate(ReadTokenVal<VrsRateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState& centerState)
{
    InsertToken(CmdBufCallId::CmdSetVrsCenterState);
    InsertToken(centerState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetVrsCenterState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetVrsCenterState(ReadTokenVal<VrsCenterState>());
}

// =====================================================================================================================
void CmdBuffer::CmdBindSampleRateImage(
    const IImage* pImage)
{
    InsertToken(CmdBufCallId::CmdBindSampleRateImage);
    InsertToken(pImage);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindSampleRateImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindSampleRateImage(ReadTokenVal<const IImage*>());
}

// =====================================================================================================================
void CmdBuffer::CmdResolvePrtPlusImage(
    const IImage&                    srcImage,
    ImageLayout                      srcImageLayout,
    const IImage&                    dstImage,
    ImageLayout                      dstImageLayout,
    PrtPlusResolveType               resolveType,
    uint32                           regionCount,
    const PrtPlusImageResolveRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdResolvePrtPlusImage);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertToken(resolveType);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResolvePrtPlusImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const IImage&                    srcImage       = *ReadTokenVal<const IImage*>();
    ImageLayout                      srcImageLayout = ReadTokenVal<ImageLayout>();
    const IImage&                    dstImage       = *ReadTokenVal<const IImage*>();
    ImageLayout                      dstImageLayout = ReadTokenVal<ImageLayout>();
    PrtPlusResolveType               resolveType    = ReadTokenVal<PrtPlusResolveType>();
    const PrtPlusImageResolveRegion* pRegions       = nullptr;
    uint32                           regionCount    = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdResolvePrtPlusImage(srcImage,
                                          srcImageLayout,
                                          dstImage,
                                          dstImageLayout,
                                          resolveType,
                                          regionCount,
                                          pRegions);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetBlendConst(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetBlendConst(ReadTokenVal<BlendConstParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetInputAssemblyState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetInputAssemblyState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetInputAssemblyState(ReadTokenVal<InputAssemblyStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetTriangleRasterState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetTriangleRasterState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetTriangleRasterState(ReadTokenVal<TriangleRasterStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetPointLineRasterState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetPointLineRasterState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetPointLineRasterState(ReadTokenVal<PointLineRasterStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetLineStippleState(
    const LineStippleStateParams& params)
{
    InsertToken(CmdBufCallId::CmdSetLineStippleState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetLineStippleState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetLineStippleState(ReadTokenVal<LineStippleStateParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    InsertToken(CmdBufCallId::CmdSetDepthBiasState);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetDepthBiasState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetDepthBiasState(ReadTokenVal<DepthBiasParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    InsertToken(CmdBufCallId::CmdSetDepthBounds);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetDepthBounds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetDepthBounds(ReadTokenVal<DepthBoundsParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    InsertToken(CmdBufCallId::CmdSetStencilRefMasks);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetStencilRefMasks(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetStencilRefMasks(ReadTokenVal<StencilRefMaskParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    InsertToken(CmdBufCallId::CmdSetMsaaQuadSamplePattern);
    InsertToken(numSamplesPerPixel);
    InsertToken(quadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetMsaaQuadSamplePattern(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    auto numSamplesPerPixel = ReadTokenVal<uint32>();
    auto quadSamplePattern  = ReadTokenVal<MsaaQuadSamplePattern>();

    pTgtCmdBuffer->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    InsertToken(CmdBufCallId::CmdSetViewports);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetViewports(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetViewports(ReadTokenVal<ViewportParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    InsertToken(CmdBufCallId::CmdSetScissorRects);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetScissorRects(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetScissorRects(ReadTokenVal<ScissorRectParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    InsertToken(CmdBufCallId::CmdSetGlobalScissor);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetGlobalScissor(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetGlobalScissor(ReadTokenVal<GlobalScissorParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdSetColorWriteMask(
    const ColorWriteMaskParams& params)
{
    InsertToken(CmdBufCallId::CmdSetColorWriteMask);
    InsertToken(params);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetColorWriteMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSetColorWriteMask(ReadTokenVal<ColorWriteMaskParams>());
}

// =====================================================================================================================
void CmdBuffer::CmdBarrierInternal(
    const BarrierInfo& barrierInfo)
{
    InsertToken(CmdBufCallId::CmdBarrier);
    InsertToken(barrierInfo);
    InsertTokenArray(barrierInfo.pPipePoints,  barrierInfo.pipePointWaitCount);
    InsertTokenArray(barrierInfo.ppGpuEvents,  barrierInfo.gpuEventWaitCount);
    InsertTokenArray(barrierInfo.ppTargets,    barrierInfo.rangeCheckedTargetWaitCount);
    InsertTokenArray(barrierInfo.pTransitions, barrierInfo.transitionCount);
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    HandleBarrierBlt(true, true);
    CmdBarrierInternal(barrierInfo);
    HandleBarrierBlt(true, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBarrier(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    BarrierInfo barrierInfo                 = ReadTokenVal<BarrierInfo>();
    barrierInfo.pipePointWaitCount          = ReadTokenArray(&barrierInfo.pPipePoints);
    barrierInfo.gpuEventWaitCount           = ReadTokenArray(&barrierInfo.ppGpuEvents);
    barrierInfo.rangeCheckedTargetWaitCount = ReadTokenArray(&barrierInfo.ppTargets);
    barrierInfo.transitionCount             = ReadTokenArray(&barrierInfo.pTransitions);

    pTgtCmdBuffer->CmdBarrier(barrierInfo);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
uint32 CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdRelease);
    InsertToken(releaseInfo.srcStageMask);
    InsertToken(releaseInfo.dstStageMask);
    InsertToken(releaseInfo.srcGlobalAccessMask);
    InsertToken(releaseInfo.dstGlobalAccessMask);
    InsertTokenArray(releaseInfo.pMemoryBarriers, releaseInfo.memoryBarrierCount);
    InsertTokenArray(releaseInfo.pImageBarriers, releaseInfo.imageBarrierCount);
    InsertToken(releaseInfo.reason);

    const uint32 releaseIdx = m_numReleaseTokens++;
    InsertToken(releaseIdx);

    HandleBarrierBlt(true, false);

    // If this layer is enabled, the return value from the layer is a release index generated and managed by this layer.
    // The layer maintains an array of release tokens, and uses release index to retrieve token value from the array.
    return releaseIdx;
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
void CmdBuffer::ReplayCmdRelease(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo releaseInfo;

    releaseInfo.srcStageMask        = ReadTokenVal<uint32>();
    releaseInfo.dstStageMask        = ReadTokenVal<uint32>();
    releaseInfo.srcGlobalAccessMask = ReadTokenVal<uint32>();
    releaseInfo.dstGlobalAccessMask = ReadTokenVal<uint32>();
    releaseInfo.memoryBarrierCount  = ReadTokenArray(&releaseInfo.pMemoryBarriers);
    releaseInfo.imageBarrierCount   = ReadTokenArray(&releaseInfo.pImageBarriers);
    releaseInfo.reason              = ReadTokenVal<uint32>();

    const uint32 releaseIdx         = ReadTokenVal<uint32>();
    PAL_ASSERT(releaseIdx == m_releaseTokenList.NumElements());

    const uint32 releaseToken = pTgtCmdBuffer->CmdRelease(releaseInfo);
    m_releaseTokenList.PushBack(releaseToken);
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdAcquire);
    InsertToken(acquireInfo.srcStageMask);
    InsertToken(acquireInfo.dstStageMask);
    InsertToken(acquireInfo.srcGlobalAccessMask);
    InsertToken(acquireInfo.dstGlobalAccessMask);
    InsertTokenArray(acquireInfo.pMemoryBarriers, acquireInfo.memoryBarrierCount);
    InsertTokenArray(acquireInfo.pImageBarriers, acquireInfo.imageBarrierCount);
    InsertToken(acquireInfo.reason);

    InsertTokenArray(pSyncTokens, syncTokenCount);

    HandleBarrierBlt(true, false);
}
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
// =====================================================================================================================
void CmdBuffer::ReplayCmdAcquire(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo acquireInfo;

    acquireInfo.srcStageMask        = ReadTokenVal<uint32>();
    acquireInfo.dstStageMask        = ReadTokenVal<uint32>();
    acquireInfo.srcGlobalAccessMask = ReadTokenVal<uint32>();
    acquireInfo.dstGlobalAccessMask = ReadTokenVal<uint32>();
    acquireInfo.memoryBarrierCount  = ReadTokenArray(&acquireInfo.pMemoryBarriers);
    acquireInfo.imageBarrierCount   = ReadTokenArray(&acquireInfo.pImageBarriers);
    acquireInfo.reason              = ReadTokenVal<uint32>();

    // The release tokens this layer's CmdAcquire receives are internal release token indices. They need to be
    // translated to the real release token values.
    const uint32* pReleaseIndices = nullptr;
    const uint32  syncTokenCount    = ReadTokenArray(&pReleaseIndices);

    auto*const pPlatform = static_cast<Platform*>(m_pDevice->GetPlatform());
    AutoBuffer<uint32, 1, Platform> releaseTokens(syncTokenCount, pPlatform);

    for (uint32 i = 0; i < syncTokenCount; i++)
    {
        releaseTokens[i] = m_releaseTokenList.At(pReleaseIndices[i]);
    }

    pTgtCmdBuffer->CmdAcquire(acquireInfo, syncTokenCount, &releaseTokens[0]);
}
#endif

// =====================================================================================================================
void CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdReleaseEvent);
    InsertToken(releaseInfo.srcStageMask);
    InsertToken(releaseInfo.dstStageMask);
    InsertToken(releaseInfo.srcGlobalAccessMask);
    InsertToken(releaseInfo.dstGlobalAccessMask);
    InsertTokenArray(releaseInfo.pMemoryBarriers, releaseInfo.memoryBarrierCount);
    InsertTokenArray(releaseInfo.pImageBarriers, releaseInfo.imageBarrierCount);
    InsertToken(releaseInfo.reason);

    HandleBarrierBlt(true, false);
}

// =====================================================================================================================
void CmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdAcquireEvent);
    InsertToken(acquireInfo.srcStageMask);
    InsertToken(acquireInfo.dstStageMask);
    InsertToken(acquireInfo.srcGlobalAccessMask);
    InsertToken(acquireInfo.dstGlobalAccessMask);
    InsertTokenArray(acquireInfo.pMemoryBarriers, acquireInfo.memoryBarrierCount);
    InsertTokenArray(acquireInfo.pImageBarriers, acquireInfo.imageBarrierCount);
    InsertToken(acquireInfo.reason);

    InsertTokenArray(ppGpuEvents, gpuEventCount);

    HandleBarrierBlt(true, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdReleaseEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo releaseInfo;

    releaseInfo.srcStageMask        = ReadTokenVal<uint32>();
    releaseInfo.dstStageMask        = ReadTokenVal<uint32>();
    releaseInfo.srcGlobalAccessMask = ReadTokenVal<uint32>();
    releaseInfo.dstGlobalAccessMask = ReadTokenVal<uint32>();
    releaseInfo.memoryBarrierCount  = ReadTokenArray(&releaseInfo.pMemoryBarriers);
    releaseInfo.imageBarrierCount   = ReadTokenArray(&releaseInfo.pImageBarriers);
    releaseInfo.reason              = ReadTokenVal<uint32>();

    auto pGpuEvent                  = ReadTokenVal<IGpuEvent*>();
    pTgtCmdBuffer->CmdReleaseEvent(releaseInfo, pGpuEvent);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdAcquireEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo acquireInfo;

    acquireInfo.srcStageMask        = ReadTokenVal<uint32>();
    acquireInfo.dstStageMask        = ReadTokenVal<uint32>();
    acquireInfo.srcGlobalAccessMask = ReadTokenVal<uint32>();
    acquireInfo.dstGlobalAccessMask = ReadTokenVal<uint32>();
    acquireInfo.memoryBarrierCount  = ReadTokenArray(&acquireInfo.pMemoryBarriers);
    acquireInfo.imageBarrierCount   = ReadTokenArray(&acquireInfo.pImageBarriers);
    acquireInfo.reason              = ReadTokenVal<uint32>();

    IGpuEvent** ppGpuEvents         = nullptr;
    uint32      gpuEventCount       = ReadTokenArray(&ppGpuEvents);

    pTgtCmdBuffer->CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdReleaseThenAcquire);
    InsertToken(barrierInfo.srcStageMask);
    InsertToken(barrierInfo.dstStageMask);
    InsertToken(barrierInfo.srcGlobalAccessMask);
    InsertToken(barrierInfo.dstGlobalAccessMask);
    InsertTokenArray(barrierInfo.pMemoryBarriers, barrierInfo.memoryBarrierCount);
    InsertTokenArray(barrierInfo.pImageBarriers, barrierInfo.imageBarrierCount);
    InsertToken(barrierInfo.reason);

    HandleBarrierBlt(true, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdReleaseThenAcquire(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo barrierInfo;

    barrierInfo.srcStageMask        = ReadTokenVal<uint32>();
    barrierInfo.dstStageMask        = ReadTokenVal<uint32>();
    barrierInfo.srcGlobalAccessMask = ReadTokenVal<uint32>();
    barrierInfo.dstGlobalAccessMask = ReadTokenVal<uint32>();
    barrierInfo.memoryBarrierCount  = ReadTokenArray(&barrierInfo.pMemoryBarriers);
    barrierInfo.imageBarrierCount   = ReadTokenArray(&barrierInfo.pImageBarriers);
    barrierInfo.reason              = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdReleaseThenAcquire(barrierInfo);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitRegisterValue);
    InsertToken(registerOffset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitRegisterValue(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto registerOffset = ReadTokenVal<uint32>();
    auto data           = ReadTokenVal<uint32>();
    auto mask           = ReadTokenVal<uint32>();
    auto compareFunc    = ReadTokenVal<CompareFunc>();
    pTgtCmdBuffer->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitMemoryValue);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitMemoryValue(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint32>();
    auto mask        = ReadTokenVal<uint32>();
    auto compareFunc = ReadTokenVal<CompareFunc>();
    pTgtCmdBuffer->CmdWaitMemoryValue(*pGpuMemory, offset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitBusAddressableMemoryMarker);
    InsertToken(&gpuMemory);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitBusAddressableMemoryMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto data = ReadTokenVal<uint32>();
    auto mask = ReadTokenVal<uint32>();
    auto compareFunc = ReadTokenVal<CompareFunc>();
    pTgtCmdBuffer->CmdWaitBusAddressableMemoryMarker(*pGpuMemory, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::VerifyBoundDrawState() const
{
    if (TestAnyFlagSet(m_verificationOptions, VerificationBoundTargets))
    {
        // Some verification logic to ensure that the currently bound pipeline matches with the currently bound
        // render targets.

        // We can assume that because this is a draw that the bound pipeline is a GraphicsPipeline. Since we only
        // decorate GraphicsPipelines with the GpuDebug::Pipeline object, we can safely make this cast.
        const GraphicsPipelineCreateInfo& pipeInfo = static_cast<const Pipeline*>(m_pBoundPipeline)->CreateInfo();

        const uint32 numBoundColorTargets = m_boundTargets.colorTargetCount;
        for (uint32 i = 0; i < numBoundColorTargets; i++)
        {
            const ColorTargetView* pTarget    =
                static_cast<const ColorTargetView*>(m_boundTargets.colorTargets[i].pColorTargetView);
            const SwizzledFormat pipeFormat      = pipeInfo.cbState.target[i].swizzledFormat;
            const SwizzledFormat ctvFormat       = (pTarget != nullptr) ? pTarget->Format() : UndefinedSwizzledFormat;

            const bool formatsMatch = (memcmp(&pipeFormat, &ctvFormat, sizeof(SwizzledFormat)) == 0);

            if (formatsMatch)
            {
                // If the formats already match, there is no cause for concern.
                continue;
            }

            // This alert is for 100% compliance. The formats provided to both the Pipeline and the ColorTargetView
            // should always match to be conformant to both API and PAL requirements. However, there are many
            // applications that do not do this properly but get away with it because the hardware *just works*. So this
            // is an alert because otherwise it would trigger very frequently.
            PAL_ALERT_ALWAYS_MSG("The format provided for the bound PAL render target does not match the expected "
                                 "format described when the pipeline was created! This is not expected, but it's not "
                                 "a fatal error.");

            // There are certain format conversions which we can consider safe when blending is enabled. If the two
            // formats share the same number of components and the same numeric "type" (that is, a floating point type
            // vs an integer type), and the number of bits going from Pipeline export to ColorTargetView for each
            // channel is guaranteed to be equivalent or an up-convert, then these formats are safe.

            const ChNumFormat pipeChNumFormat = pipeFormat.format;
            const ChNumFormat ctvChNumFormat  = ctvFormat.format;

            const bool formatsUndefined = (pipeChNumFormat == ChNumFormat::Undefined) ||
                                          (ctvChNumFormat  == ChNumFormat::Undefined);

            const bool pipeIsFloatType = Formats::IsUnorm(pipeChNumFormat) ||
                                         Formats::IsSnorm(pipeChNumFormat) ||
                                         Formats::IsFloat(pipeChNumFormat) ||
                                         Formats::IsSrgb(pipeChNumFormat);
            const bool ctvIsFloatType = Formats::IsUnorm(ctvChNumFormat) ||
                                        Formats::IsSnorm(ctvChNumFormat) ||
                                        Formats::IsFloat(ctvChNumFormat) ||
                                        Formats::IsSrgb(ctvChNumFormat);

            const bool similarNumType  = pipeIsFloatType == ctvIsFloatType;
            const bool shareChFmt      = Formats::ShareChFmt(pipeChNumFormat, ctvChNumFormat);
            const bool shareComponents = Formats::ComponentMask(pipeChNumFormat) ==
                                         Formats::ComponentMask(ctvChNumFormat);
            const bool shareNumBits    = Formats::BitsPerPixel(pipeChNumFormat) ==
                                         Formats::BitsPerPixel(ctvChNumFormat);

            const uint32* pPipeCompNumBits = Formats::ComponentBitCounts(pipeChNumFormat);
            const uint32* pCtvCompNumBits  = Formats::ComponentBitCounts(ctvChNumFormat);

            bool safeComponentUpConversion = true;
            for (uint32 comp = 0; comp < 4; comp++)
            {
                safeComponentUpConversion &= (pPipeCompNumBits[comp] <= pCtvCompNumBits[comp]);
            }

            const bool blendStateBlendEnable =
                (m_pBoundBlendState != nullptr) ?
                static_cast<const ColorBlendState*>(m_pBoundBlendState)->CreateInfo().targets[i].blendEnable :
                false;

            // This assert here only cares about the following situations:
            //   - When blending is enabled, and
            //   - When the pipeline is exporting to a render target, and
            //   - When the render target is bound, and
            //   - The formats of the render target and pipeline's export are safe for conversion, which is defined as:
            //   |-- The same numeric type (floating point type or integer type), and
            //   |-- The same channel format, or the same number of components, the same pixel bit width, and that the
            //       pipeline's components are always equal or up-converting to the render targets components.
            PAL_ASSERT_MSG((blendStateBlendEnable == false) ||
                           formatsUndefined                 ||
                           (similarNumType &&
                            (shareChFmt || (shareComponents && shareNumBits && safeComponentUpConversion))),
                           "Blending is enabled and the format conversion between the pipeline's exports and the " \
                           "bound render target are possibly incompatible. Some hardware may see corruption with this " \
                           "combination, and the application or client driver should work to fix this illegal issue.");
        }

        // The PAL IPipeline object does not know what the DepthStencilView format is, so we cannot check it against
        // the bound render targets.
    }
}

// =====================================================================================================================
// Adds single-step and timestamp logic for any internal draws/dispatches that the internal PAL core might do.
// Also adds draw/dispatch info (shader IDs) to the command stream prior to the draw/dispatch.
void CmdBuffer::HandleDrawDispatch(
    Developer::DrawDispatchType drawDispatchType,
    bool                        preAction)
{
    const bool isDraw = (drawDispatchType < Developer::DrawDispatchType::FirstDispatch);

    if (preAction)
    {
        VerifyBoundDrawState();

        const bool cacheFlushInv = (isDraw) ? TestAnyFlagSet(m_cacheFlushInvOnAction, BeforeDraw) :
                                              TestAnyFlagSet(m_cacheFlushInvOnAction, BeforeDispatch);

        if (cacheFlushInv)
        {
            AddCacheFlushInv();
        }
    }
    else
    {
        bool cacheFlushInv = (isDraw) ? TestAnyFlagSet(m_cacheFlushInvOnAction, AfterDraw) :
                                        TestAnyFlagSet(m_cacheFlushInvOnAction, AfterDispatch);

        if (isDraw)
        {
            if (IsSurfaceCaptureActive())
            {
                cacheFlushInv = true;
            }
        }

        if (cacheFlushInv)
        {
            AddCacheFlushInv();
        }

        if (IsSurfaceCaptureActive())
        {
            CaptureSurfaces();
        }

        if (isDraw && m_surfaceCapture.pipelineMatch)
        {
            m_surfaceCapture.actionId++;
        }

        const bool timestampAndWait = (isDraw) ? TestAnyFlagSet(m_singleStep, TimestampAndWaitDraws) :
                                                 TestAnyFlagSet(m_singleStep, TimestampAndWaitDispatches);

        if (timestampAndWait)
        {
            AddSingleStepBarrier(m_counter);
        }

        if ((m_breakOnDrawDispatchCount > 0) && ((m_counter % m_breakOnDrawDispatchCount) == 0))
        {
            if (m_flags.nested == 0)
            {
                InsertToken(CmdBufCallId::End);

                InsertToken(CmdBufCallId::Begin);
                InsertToken(m_buildInfo);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("%s", "Nested CmdBuffers \"split on action count\" is not supported by " \
                                           "GpuDebug layer features.");
            }
        }

        if (timestampAndWait)
        {
            AddTimestamp(m_timestampAddr, &m_counter);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::HandleBarrierBlt(
    bool isBarrier,
    bool preAction)
{
    if (preAction)
    {
        const bool cacheFlushInv = (isBarrier) ? TestAnyFlagSet(m_cacheFlushInvOnAction, BeforeBarrier) :
                                                 TestAnyFlagSet(m_cacheFlushInvOnAction, BeforeBlt);

        if (cacheFlushInv)
        {
            AddCacheFlushInv();
        }
    }
    else
    {
        const bool cacheFlushInv = (isBarrier) ? TestAnyFlagSet(m_cacheFlushInvOnAction, AfterBarrier) :
                                                 TestAnyFlagSet(m_cacheFlushInvOnAction, AfterBlt);

        if (cacheFlushInv)
        {
            AddCacheFlushInv();
        }

        const bool timestampAndWait = (isBarrier) ? TestAnyFlagSet(m_singleStep, TimestampAndWaitBarriers) :
                                                    TestAnyFlagSet(m_singleStep, TimestampAndWaitBlts);
        if (timestampAndWait)
        {
            AddSingleStepBarrier(m_counter);
        }

        if ((m_breakOnDrawDispatchCount > 0) && ((m_counter % m_breakOnDrawDispatchCount) == 0))
        {
            if (m_flags.nested == 0)
            {
                InsertToken(CmdBufCallId::End);

                InsertToken(CmdBufCallId::Begin);
                InsertToken(m_buildInfo);
            }
            else
            {
                PAL_ALERT_ALWAYS_MSG("%s", "Nested CmdBuffers \"split on action count\" is not supported by " \
                                           "GpuDebug layer features.");
            }
        }

        if (timestampAndWait)
        {
            AddTimestamp(m_timestampAddr, &m_counter);
        }
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDraw, true);

    pThis->InsertToken(CmdBufCallId::CmdDraw);
    pThis->InsertToken(firstVertex);
    pThis->InsertToken(vertexCount);
    pThis->InsertToken(firstInstance);
    pThis->InsertToken(instanceCount);
    pThis->InsertToken(drawId);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDraw, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDraw(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto firstVertex   = ReadTokenVal<uint32>();
    auto vertexCount   = ReadTokenVal<uint32>();
    auto firstInstance = ReadTokenVal<uint32>();
    auto instanceCount = ReadTokenVal<uint32>();
    auto drawId        = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount, drawId);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawOpaque, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawOpaque);
    pThis->InsertToken(streamOutFilledSizeVa);
    pThis->InsertToken(streamOutOffset);
    pThis->InsertToken(stride);
    pThis->InsertToken(firstInstance);
    pThis->InsertToken(instanceCount);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawOpaque, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawOpaque(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto streamOutFilledSizeVa = ReadTokenVal<gpusize>();
    auto streamOutOffset       = ReadTokenVal<uint32>();
    auto stride                = ReadTokenVal<uint32>();
    auto firstInstance         = ReadTokenVal<uint32>();
    auto instanceCount         = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexed, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndexed);
    pThis->InsertToken(firstIndex);
    pThis->InsertToken(indexCount);
    pThis->InsertToken(vertexOffset);
    pThis->InsertToken(firstInstance);
    pThis->InsertToken(instanceCount);
    pThis->InsertToken(drawId);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexed, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndexed(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto firstIndex    = ReadTokenVal<uint32>();
    auto indexCount    = ReadTokenVal<uint32>();
    auto vertexOffset  = ReadTokenVal<int32>();
    auto firstInstance = ReadTokenVal<uint32>();
    auto instanceCount = ReadTokenVal<uint32>();
    auto drawId        = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount, drawId);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndirectMulti);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
    pThis->InsertToken(stride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory   = ReadTokenVal<IGpuMemory*>();
    auto offset       = ReadTokenVal<gpusize>();
    auto stride       = ReadTokenVal<uint32>();
    auto maximumCount = ReadTokenVal<uint32>();
    auto countGpuAddr = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDrawIndirectMulti(*pGpuMemory, offset, stride, maximumCount, countGpuAddr);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndexedIndirectMulti);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
    pThis->InsertToken(stride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndexedIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory   = ReadTokenVal<IGpuMemory*>();
    auto offset       = ReadTokenVal<gpusize>();
    auto stride       = ReadTokenVal<uint32>();
    auto maximumCount = ReadTokenVal<uint32>();
    auto countGpuAddr = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDrawIndexedIndirectMulti(*pGpuMemory, offset, stride, maximumCount, countGpuAddr);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatch, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatch);
    pThis->InsertToken(x);
    pThis->InsertToken(y);
    pThis->InsertToken(z);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatch, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatch(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto x = ReadTokenVal<uint32>();
    auto y = ReadTokenVal<uint32>();
    auto z = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDispatch(x, y, z);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchIndirect, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchIndirect);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchIndirect, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchIndirect(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto offset     = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDispatchIndirect(*pGpuMemory, offset);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchOffset, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchOffset);
    pThis->InsertToken(xOffset);
    pThis->InsertToken(yOffset);
    pThis->InsertToken(zOffset);
    pThis->InsertToken(xDim);
    pThis->InsertToken(yDim);
    pThis->InsertToken(zDim);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchOffset, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchOffset(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto xOffset = ReadTokenVal<uint32>();
    auto yOffset = ReadTokenVal<uint32>();
    auto zOffset = ReadTokenVal<uint32>();
    auto xDim    = ReadTokenVal<uint32>();
    auto yDim    = ReadTokenVal<uint32>();
    auto zDim    = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMesh(
    ICmdBuffer* pCmdBuffer,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    CmdBuffer* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMesh, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchMesh);
    pThis->InsertToken(xDim);
    pThis->InsertToken(yDim);
    pThis->InsertToken(zDim);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMesh, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchMesh(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto x = ReadTokenVal<uint32>();
    auto y = ReadTokenVal<uint32>();
    auto z = ReadTokenVal<uint32>();
    pTgtCmdBuffer->CmdDispatchMesh(x, y, z);
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMeshIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    CmdBuffer* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchMeshIndirectMulti);
    pThis->InsertToken(&gpuMemory);
    pThis->InsertToken(offset);
    pThis->InsertToken(stride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchMeshIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory      = ReadTokenVal<IGpuMemory*>();
    auto offset          = ReadTokenVal<gpusize>();
    uint32  stride       = ReadTokenVal<uint32>();
    uint32  maximumCount = ReadTokenVal<uint32>();
    gpusize countGpuAddr = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDispatchMeshIndirectMulti(*pGpuMemory, offset, stride, maximumCount, countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdUpdateMemory);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertTokenArray(pData, static_cast<uint32>(dataSize / sizeof(uint32)));

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto          dstOffset     = ReadTokenVal<gpusize>();
    const uint32* pData         = nullptr;
    auto          dataSize      = ReadTokenArray(&pData) * sizeof(uint32);

    pTgtCmdBuffer->CmdUpdateMemory(*pDstGpuMemory, dstOffset, dataSize, pData);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdUpdateBusAddressableMemoryMarker);
    InsertToken(&dstGpuMemory);
    InsertToken(offset);
    InsertToken(value);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateBusAddressableMemoryMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto          offset = ReadTokenVal<uint32>();
    auto          value = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdUpdateBusAddressableMemoryMarker(*pDstGpuMemory, offset, value);
}

// =====================================================================================================================
void CmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdFillMemory);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(fillSize);
    InsertToken(data);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFillMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto fillSize      = ReadTokenVal<gpusize>();
    auto data          = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdFillMemory(*pDstGpuMemory, dstOffset, fillSize, data);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyMemory);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                    pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto                    pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    const MemoryCopyRegion* pRegions      = nullptr;
    auto                    regionCount   = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyMemory(*pSrcGpuMemory, *pDstGpuMemory, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyTypedBuffer);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyTypedBuffer(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto                         pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    const TypedBufferCopyRegion* pRegions      = nullptr;
    auto                         regionCount   = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyTypedBuffer(*pSrcGpuMemory, *pDstGpuMemory, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyRegisterToMemory);
    InsertToken(srcRegisterOffset);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyRegisterToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto srcRegisterOffset = ReadTokenVal<uint32>();
    auto pDstGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto dstOffset         = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdCopyRegisterToMemory(srcRegisterOffset, *pDstGpuMemory, dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    const Rect*            pScissorRect,
    uint32                 flags)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyImage);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(pScissorRect);
    InsertToken(flags);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                   pSrcImage      = ReadTokenVal<IImage*>();
    auto                   srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                   pDstImage      = ReadTokenVal<IImage*>();
    auto                   dstImageLayout = ReadTokenVal<ImageLayout>();
    const ImageCopyRegion* pRegions       = nullptr;
    auto                   regionCount    = ReadTokenArray(&pRegions);
    auto                   pScissorRect   = ReadTokenVal<Rect*>();
    auto                   flags          = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdCopyImage(*pSrcImage,
                                srcImageLayout,
                                *pDstImage,
                                dstImageLayout,
                                regionCount,
                                pRegions,
                                pScissorRect,
                                flags);
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo& copyInfo)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdScaledCopyImage);
    InsertToken(copyInfo.pSrcImage);
    InsertToken(copyInfo.srcImageLayout);
    InsertToken(copyInfo.pDstImage);
    InsertToken(copyInfo.dstImageLayout);
    InsertTokenArray(copyInfo.pRegions, copyInfo.regionCount);
    InsertToken(copyInfo.filter);
    InsertToken(copyInfo.rotation);
    InsertToken(copyInfo.flags);
    if (copyInfo.flags.srcColorKey || copyInfo.flags.dstColorKey)
    {
        InsertTokenArray(copyInfo.pColorKey,1);
    }

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdScaledCopyImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    ScaledCopyInfo copyInfo = {};

    copyInfo.pSrcImage      = ReadTokenVal<IImage*>();
    copyInfo.srcImageLayout = ReadTokenVal<ImageLayout>();
    copyInfo.pDstImage      = ReadTokenVal<IImage*>();
    copyInfo.dstImageLayout = ReadTokenVal<ImageLayout>();
    copyInfo.regionCount    = ReadTokenArray(&copyInfo.pRegions);
    copyInfo.filter         = ReadTokenVal<TexFilter>();
    copyInfo.rotation       = ReadTokenVal<ImageRotation>();
    copyInfo.flags          = ReadTokenVal<ScaledCopyFlags>();
    if (copyInfo.flags.srcColorKey || copyInfo.flags.dstColorKey)
    {
        ReadTokenArray(&copyInfo.pColorKey);
    }
    else
    {
        copyInfo.pColorKey = nullptr;
    }

    pTgtCmdBuffer->CmdScaledCopyImage(copyInfo);
}

// =====================================================================================================================
void CmdBuffer::CmdGenerateMipmaps(
    const GenMipmapsInfo& genInfo)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdGenerateMipmaps);
    InsertToken(genInfo);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdGenerateMipmaps(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    GenMipmapsInfo genInfo = ReadTokenVal<GenMipmapsInfo>();

    pTgtCmdBuffer->CmdGenerateMipmaps(genInfo);
}

// =====================================================================================================================
void CmdBuffer::CmdColorSpaceConversionCopy(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdColorSpaceConversionCopy);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(filter);
    InsertToken(cscTable);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdColorSpaceConversionCopy(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                              pSrcImage      = ReadTokenVal<IImage*>();
    auto                              srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                              pDstImage      = ReadTokenVal<IImage*>();
    auto                              dstImageLayout = ReadTokenVal<ImageLayout>();
    const ColorSpaceConversionRegion* pRegions       = nullptr;
    auto                              regionCount    = ReadTokenArray(&pRegions);
    auto                              filter         = ReadTokenVal<TexFilter>();
    auto                              cscTable       = ReadTokenVal<ColorSpaceConversionTable>();

    pTgtCmdBuffer->CmdColorSpaceConversionCopy(*pSrcImage,
                                               srcImageLayout,
                                               *pDstImage,
                                               dstImageLayout,
                                               regionCount,
                                               pRegions,
                                               filter,
                                               cscTable);
}

// =====================================================================================================================
void CmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCloneImageData);
    InsertToken(&srcImage);
    InsertToken(&dstImage);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCloneImageData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcImage = ReadTokenVal<IImage*>();
    auto pDstImage = ReadTokenVal<IImage*>();

    pTgtCmdBuffer->CmdCloneImageData(*pSrcImage, *pDstImage);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyMemoryToImage);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemoryToImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto                         pDstImage      = ReadTokenVal<IImage*>();
    auto                         dstImageLayout = ReadTokenVal<ImageLayout>();
    const MemoryImageCopyRegion* pRegions       = nullptr;
    auto                         regionCount    = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyMemoryToImage(*pSrcGpuMemory, *pDstImage, dstImageLayout, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyImageToMemory);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImageToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                         pSrcImage      = ReadTokenVal<IImage*>();
    auto                         srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                         pDstGpuMemory  = ReadTokenVal<IGpuMemory*>();
    const MemoryImageCopyRegion* pRegions       = nullptr;
    auto                         regionCount    = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyImageToMemory(*pSrcImage, srcImageLayout, *pDstGpuMemory, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyMemoryToTiledImage);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemoryToTiledImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcGpuMemory                         = ReadTokenVal<IGpuMemory*>();
    auto pDstImage                             = ReadTokenVal<IImage*>();
    auto dstImageLayout                        = ReadTokenVal<ImageLayout>();
    const MemoryTiledImageCopyRegion* pRegions = nullptr;
    auto                         regionCount = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyMemoryToTiledImage(*pSrcGpuMemory, *pDstImage, dstImageLayout, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyTiledImageToMemory);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstGpuMemory);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyTiledImageToMemory(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcImage                             = ReadTokenVal<IImage*>();
    auto srcImageLayout                        = ReadTokenVal<ImageLayout>();
    auto pDstGpuMemory                         = ReadTokenVal<IGpuMemory*>();
    const MemoryTiledImageCopyRegion* pRegions = nullptr;
    auto regionCount                           = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyTiledImageToMemory(*pSrcImage, srcImageLayout, *pDstGpuMemory, regionCount, pRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorBuffer(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearColorBuffer);
    InsertToken(&gpuMemory);
    InsertToken(color);
    InsertToken(bufferFormat);
    InsertToken(bufferOffset);
    InsertToken(bufferExtent);
    InsertTokenArray(pRanges, rangeCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearColorBuffer(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto         pGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto         color      = ReadTokenVal<ClearColor>();
    auto         format     = ReadTokenVal<SwizzledFormat>();
    auto         offset     = ReadTokenVal<uint32>();
    auto         extent     = ReadTokenVal<uint32>();
    const Range* pRanges    = nullptr;
    auto         rangeCount = ReadTokenArray(&pRanges);

    pTgtCmdBuffer->CmdClearColorBuffer(*pGpuMemory, color, format, offset, extent, rangeCount, pRanges);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundColorTargets(
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearBoundColorTargets);
    InsertTokenArray(pBoundColorTargets, colorTargetCount);
    InsertTokenArray(pClearRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBoundColorTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const BoundColorTarget*         pBoundColorTargets      = nullptr;
    auto                            colorTargetCount        = ReadTokenArray(&pBoundColorTargets);
    const ClearBoundTargetRegion*   pClearRegions           = nullptr;
    auto                            regionCount             = ReadTokenArray(&pClearRegions);

    pTgtCmdBuffer->CmdClearBoundColorTargets(colorTargetCount,
                                             pBoundColorTargets,
                                             regionCount,
                                             pClearRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorImage(
    const IImage&      image,
    ImageLayout        imageLayout,
    const ClearColor&  color,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             boxCount,
    const Box*         pBoxes,
    uint32             flags)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearColorImage);
    InsertToken(&image);
    InsertToken(imageLayout);
    InsertToken(color);
    InsertTokenArray(pRanges, rangeCount);
    InsertTokenArray(pBoxes, boxCount);
    InsertToken(flags);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearColorImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto               pImage      = ReadTokenVal<IImage*>();
    auto               imageLayout = ReadTokenVal<ImageLayout>();
    auto               color       = ReadTokenVal<ClearColor>();
    const SubresRange* pRanges     = nullptr;
    auto               rangeCount  = ReadTokenArray(&pRanges);
    const Box*         pBoxes      = nullptr;
    auto               boxCount    = ReadTokenArray(&pBoxes);
    auto               flags       = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdClearColorImage(*pImage, imageLayout, color, rangeCount, pRanges, boxCount, pBoxes, flags);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundDepthStencilTargets(
    float                         depth,
    uint8                         stencil,
    uint8                         stencilWriteMask,
    uint32                        samples,
    uint32                        fragments,
    DepthStencilSelectFlags       flag,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearBoundDepthStencilTargets);
    InsertToken(depth);
    InsertToken(stencil);
    InsertToken(stencilWriteMask);
    InsertToken(samples);
    InsertToken(fragments);
    InsertToken(flag);
    InsertTokenArray(pClearRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBoundDepthStencilTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                          depth            = ReadTokenVal<float>();
    auto                          stencil          = ReadTokenVal<uint8>();
    auto                          stencilWriteMask = ReadTokenVal<uint8>();
    auto                          samples          = ReadTokenVal<uint32>();
    auto                          fragments        = ReadTokenVal<uint32>();
    auto                          flag             = ReadTokenVal<DepthStencilSelectFlags>();
    const ClearBoundTargetRegion* pClearRegions    = nullptr;
    auto                          regionCount      = ReadTokenArray(&pClearRegions);

    pTgtCmdBuffer->CmdClearBoundDepthStencilTargets(depth,
                                                    stencil,
                                                    stencilWriteMask,
                                                    samples,
                                                    fragments,
                                                    flag,
                                                    regionCount,
                                                    pClearRegions);
}

// =====================================================================================================================
void CmdBuffer::CmdClearDepthStencil(
    const IImage&      image,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearDepthStencil);
    InsertToken(&image);
    InsertToken(depthLayout);
    InsertToken(stencilLayout);
    InsertToken(depth);
    InsertToken(stencil);
    InsertToken(stencilWriteMask);
    InsertTokenArray(pRanges, rangeCount);
    InsertTokenArray(pRects, rectCount);
    InsertToken(flags);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearDepthStencil(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto               pImage           = ReadTokenVal<IImage*>();
    auto               depthLayout      = ReadTokenVal<ImageLayout>();
    auto               stencilLayout    = ReadTokenVal<ImageLayout>();
    auto               depth            = ReadTokenVal<float>();
    auto               stencil          = ReadTokenVal<uint8>();
    auto               stencilWriteMask = ReadTokenVal<uint8>();
    const SubresRange* pRanges          = nullptr;
    auto               rangeCount       = ReadTokenArray(&pRanges);
    const Rect*        pRects           = nullptr;
    auto               rectCount        = ReadTokenArray(&pRects);
    auto               flags            = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdClearDepthStencil(*pImage,
                                        depthLayout,
                                        stencilLayout,
                                        depth,
                                        stencil,
                                        stencilWriteMask,
                                        rangeCount,
                                        pRanges,
                                        rectCount,
                                        pRects,
                                        flags);
}

// =====================================================================================================================
void CmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearBufferView);
    InsertToken(&gpuMemory);
    InsertToken(color);
    InsertTokenArray(static_cast<const uint32*>(pBufferViewSrd), m_pDevice->BufferSrdDwords());
    InsertTokenArray(pRanges, rangeCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearBufferView(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto          color          = ReadTokenVal<ClearColor>();
    const uint32* pBufferViewSrd = nullptr;
    ReadTokenArray(&pBufferViewSrd);
    const Range*  pRanges        = nullptr;
    auto          rangeCount     = ReadTokenArray(&pRanges);

    pTgtCmdBuffer->CmdClearBufferView(*pGpuMemory, color, pBufferViewSrd, rangeCount, pRanges);
}

// =====================================================================================================================
void CmdBuffer::CmdClearImageView(
    const IImage&     image,
    ImageLayout       imageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearImageView);
    InsertToken(&image);
    InsertToken(imageLayout);
    InsertToken(color);
    InsertTokenArray(static_cast<const uint32*>(pImageViewSrd), m_pDevice->ImageSrdDwords());
    InsertTokenArray(pRects, rectCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearImageView(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto          pImage        = ReadTokenVal<IImage*>();
    auto          imageLayout   = ReadTokenVal<ImageLayout>();
    auto          color         = ReadTokenVal<ClearColor>();
    const uint32* pImageViewSrd = nullptr;
    ReadTokenArray(&pImageViewSrd);
    const Rect*   pRects        = nullptr;
    auto          rectCount     = ReadTokenArray(&pRects);

    pTgtCmdBuffer->CmdClearImageView(*pImage, imageLayout, color, pImageViewSrd, rectCount, pRects);
}

// =====================================================================================================================
void CmdBuffer::CmdResolveImage(
    const IImage&             srcImage,
    ImageLayout               srcImageLayout,
    const IImage&             dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdResolveImage);
    InsertToken(&srcImage);
    InsertToken(srcImageLayout);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
    InsertToken(resolveMode);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(flags);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResolveImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                      pSrcImage      = ReadTokenVal<IImage*>();
    auto                      srcImageLayout = ReadTokenVal<ImageLayout>();
    auto                      pDstImage      = ReadTokenVal<IImage*>();
    auto                      dstImageLayout = ReadTokenVal<ImageLayout>();
    auto                      resolveMode    = ReadTokenVal<ResolveMode>();
    const ImageResolveRegion* pRegions       = nullptr;
    auto                      regionCount    = ReadTokenArray(&pRegions);
    auto                      flags          = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdResolveImage(*pSrcImage,
                                   srcImageLayout,
                                   *pDstImage,
                                   dstImageLayout,
                                   resolveMode,
                                   regionCount,
                                   pRegions,
                                   flags);
}

// =====================================================================================================================
void CmdBuffer::CmdSetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      setPoint)
{
    InsertToken(CmdBufCallId::CmdSetEvent);
    InsertToken(&gpuEvent);
    InsertToken(setPoint);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent = ReadTokenVal<IGpuEvent*>();
    auto setPoint  = ReadTokenVal<HwPipePoint>();

    pTgtCmdBuffer->CmdSetEvent(*pGpuEvent, setPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      resetPoint)
{
    InsertToken(CmdBufCallId::CmdResetEvent);
    InsertToken(&gpuEvent);
    InsertToken(resetPoint);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent  = ReadTokenVal<IGpuEvent*>();
    auto resetPoint = ReadTokenVal<HwPipePoint>();

    pTgtCmdBuffer->CmdResetEvent(*pGpuEvent, resetPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    InsertToken(CmdBufCallId::CmdPredicateEvent);
    InsertToken(&gpuEvent);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdPredicateEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent  = ReadTokenVal<IGpuEvent*>();

    pTgtCmdBuffer->CmdPredicateEvent(*pGpuEvent);
}

// =====================================================================================================================
void CmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    InsertToken(CmdBufCallId::CmdMemoryAtomic);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(srcData);
    InsertToken(atomicOp);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdMemoryAtomic(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto srcData       = ReadTokenVal<uint64>();
    auto atomicOp      = ReadTokenVal<AtomicOp>();

    pTgtCmdBuffer->CmdMemoryAtomic(*pDstGpuMemory, dstOffset, srcData, atomicOp);
}

// =====================================================================================================================
void CmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    InsertToken(CmdBufCallId::CmdResetQueryPool);
    InsertToken(&queryPool);
    InsertToken(startQuery);
    InsertToken(queryCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResetQueryPool(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto startQuery = ReadTokenVal<uint32>();
    auto queryCount = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdResetQueryPool(*pQueryPool, startQuery, queryCount);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    InsertToken(CmdBufCallId::CmdBeginQuery);
    InsertToken(&queryPool);
    InsertToken(queryType);
    InsertToken(slot);
    InsertToken(flags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBeginQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto queryType  = ReadTokenVal<QueryType>();
    auto slot       = ReadTokenVal<uint32>();
    auto flags      = ReadTokenVal<QueryControlFlags>();

    pTgtCmdBuffer->CmdBeginQuery(*pQueryPool, queryType, slot, flags);
}

// =====================================================================================================================
void CmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    InsertToken(CmdBufCallId::CmdEndQuery);
    InsertToken(&queryPool);
    InsertToken(queryType);
    InsertToken(slot);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool = ReadTokenVal<IQueryPool*>();
    auto queryType  = ReadTokenVal<QueryType>();
    auto slot       = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdEndQuery(*pQueryPool, queryType, slot);
}

// =====================================================================================================================
void CmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdResolveQuery);
    InsertToken(&queryPool);
    InsertToken(flags);
    InsertToken(queryType);
    InsertToken(startQuery);
    InsertToken(queryCount);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
    InsertToken(dstStride);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResolveQuery(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool    = ReadTokenVal<IQueryPool*>();
    auto flags         = ReadTokenVal<QueryResultFlags>();
    auto queryType     = ReadTokenVal<QueryType>();
    auto startQuery    = ReadTokenVal<uint32>();
    auto queryCount    = ReadTokenVal<uint32>();
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();
    auto dstStride     = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdResolveQuery(*pQueryPool,
                                   flags,
                                   queryType,
                                   startQuery,
                                   queryCount,
                                   *pDstGpuMemory,
                                   dstOffset,
                                   dstStride);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPredication(
    IQueryPool*         pQueryPool,
    uint32              slot,
    const IGpuMemory*   pGpuMemory,
    gpusize             offset,
    PredicateType       predType,
    bool                predPolarity,
    bool                waitResults,
    bool                accumulateData)
{
    InsertToken(CmdBufCallId::CmdSetPredication);
    InsertToken(pQueryPool);
    InsertToken(slot);
    InsertToken(pGpuMemory);
    InsertToken(offset);
    InsertToken(predType);
    InsertToken(predPolarity);
    InsertToken(waitResults);
    InsertToken(accumulateData);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetPredication(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pQueryPool     = ReadTokenVal<IQueryPool*>();
    auto slot           = ReadTokenVal<uint32>();
    auto pGpuMemory     = ReadTokenVal<IGpuMemory*>();
    auto offset         = ReadTokenVal<gpusize>();
    auto predType       = ReadTokenVal<PredicateType>();
    auto predPolarity   = ReadTokenVal<bool>();
    auto waitResults    = ReadTokenVal<bool>();
    auto accumData      = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdSetPredication(pQueryPool, slot, pGpuMemory, offset, predType, predPolarity,
                                     waitResults, accumData);
}

// =====================================================================================================================
void CmdBuffer::CmdSuspendPredication(
    bool suspend)
{
    InsertToken(CmdBufCallId::CmdSuspendPredication);
    InsertToken(suspend);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSuspendPredication(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto suspend = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdSuspendPredication(suspend);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    InsertToken(CmdBufCallId::CmdWriteTimestamp);
    InsertToken(pipePoint);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteTimestamp(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipePoint     = ReadTokenVal<HwPipePoint>();
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteTimestamp(pipePoint, *pDstGpuMemory, dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    InsertToken(CmdBufCallId::CmdWriteImmediate);
    InsertToken(pipePoint);
    InsertToken(data);
    InsertToken(dataSize);
    InsertToken(address);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteImmediate(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pipePoint = ReadTokenVal<HwPipePoint>();
    auto data      = ReadTokenVal<uint64>();
    auto dataSize  = ReadTokenVal<ImmediateDataWidth>();
    auto address   = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteImmediate(pipePoint, data, dataSize, address);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    InsertToken(CmdBufCallId::CmdLoadBufferFilledSizes);
    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        InsertToken(gpuVirtAddr[i]);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdLoadBufferFilledSizes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    gpusize gpuVirtAddrs[MaxStreamOutTargets];

    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        gpuVirtAddrs[i] = ReadTokenVal<gpusize>();
    }

    pTgtCmdBuffer->CmdLoadBufferFilledSizes(gpuVirtAddrs);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    InsertToken(CmdBufCallId::CmdSaveBufferFilledSizes);
    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        InsertToken(gpuVirtAddr[i]);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSaveBufferFilledSizes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    gpusize gpuVirtAddrs[MaxStreamOutTargets];

    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        gpuVirtAddrs[i] = ReadTokenVal<gpusize>();
    }

    pTgtCmdBuffer->CmdSaveBufferFilledSizes(gpuVirtAddrs);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBufferFilledSize(
    uint32  bufferId,
    uint32  offset)
{
    InsertToken(CmdBufCallId::CmdSetBufferFilledSize);
    InsertToken(bufferId);
    InsertToken(offset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetBufferFilledSize(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto bufferId = ReadTokenVal<uint32>();
    auto offset   = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdSetBufferFilledSize(bufferId, offset);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize)
{
    InsertToken(CmdBufCallId::CmdLoadCeRam);
    InsertToken(&srcGpuMemory);
    InsertToken(memOffset);
    InsertToken(ramOffset);
    InsertToken(dwordSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdLoadCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pSrcGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto memOffset     = ReadTokenVal<gpusize>();
    auto ramOffset     = ReadTokenVal<uint32>();
    auto dwordSize     = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdLoadCeRam(*pSrcGpuMemory, memOffset, ramOffset, dwordSize);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,
    uint32      dwordSize)
{
    InsertToken(CmdBufCallId::CmdWriteCeRam);
    InsertTokenArray(static_cast<const uint32*>(pSrcData), dwordSize);
    InsertToken(ramOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32* pSrcData  = nullptr;
    auto          dwordSize = ReadTokenArray(&pSrcData);
    auto          ramOffset = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdWriteCeRam(pSrcData, ramOffset, dwordSize);
}

// =====================================================================================================================
void CmdBuffer::CmdDumpCeRam(
    const IGpuMemory& dstGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize,
    uint32            currRingPos,
    uint32            ringSize)
{
    InsertToken(CmdBufCallId::CmdDumpCeRam);
    InsertToken(&dstGpuMemory);
    InsertToken(memOffset);
    InsertToken(ramOffset);
    InsertToken(dwordSize);
    InsertToken(currRingPos);
    InsertToken(ringSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDumpCeRam(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto memOffset     = ReadTokenVal<gpusize>();
    auto ramOffset     = ReadTokenVal<uint32>();
    auto dwordSize     = ReadTokenVal<uint32>();
    auto currRingPos   = ReadTokenVal<uint32>();
    auto ringSize      = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdDumpCeRam(*pDstGpuMemory, memOffset, ramOffset, dwordSize, currRingPos, ringSize);
}

// =====================================================================================================================
uint32 CmdBuffer::GetEmbeddedDataLimit() const
{
    return GetNextLayer()->GetEmbeddedDataLimit();
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    return GetNextLayer()->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
}

// =====================================================================================================================
Result CmdBuffer::AllocateAndBindGpuMemToEvent(
    IGpuEvent* pGpuEvent)
{
    return GetNextLayer()->AllocateAndBindGpuMemToEvent(NextGpuEvent(pGpuEvent));
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    InsertToken(CmdBufCallId::CmdExecuteNestedCmdBuffers);
    InsertTokenArray(ppCmdBuffers, cmdBufferCount);
}

// =====================================================================================================================
// Nested command buffers are treated similarly to root-level command buffers.  The recorded commands are replayed
// with instrumentation into queue-owned command buffers and those command buffers are the ones inserted into the final
// command stream. In the future, we could support breaking them apart as well.
void CmdBuffer::ReplayCmdExecuteNestedCmdBuffers(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    ICmdBuffer*const* ppCmdBuffers   = nullptr;
    const uint32      cmdBufferCount = ReadTokenArray(&ppCmdBuffers);
    auto*const        pPlatform      = static_cast<Platform*>(m_pDevice->GetPlatform());

    AutoBuffer<ICmdBuffer*, 32, Platform> tgtCmdBuffers(cmdBufferCount, pPlatform);

    if (tgtCmdBuffers.Capacity() < cmdBufferCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < cmdBufferCount; i++)
        {
            auto*const pNestedCmdBuffer    = static_cast<CmdBuffer*>(ppCmdBuffers[i]);
            auto*const pNestedTgtCmdBuffer = pQueue->AcquireCmdBuf(nullptr, pTgtCmdBuffer->GetSubQueueIdx(), true);
            tgtCmdBuffers[i]               = pNestedTgtCmdBuffer;
            pNestedCmdBuffer->Replay(pQueue, nullptr, 0, pNestedTgtCmdBuffer);
        }

        pTgtCmdBuffer->CmdExecuteNestedCmdBuffers(cmdBufferCount, &tgtCmdBuffers[0]);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    InsertToken(CmdBufCallId::CmdExecuteIndirectCmds);
    InsertToken(&generator);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(maximumCount);
    InsertToken(countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdExecuteIndirectCmds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto*const pGenerator   = ReadTokenVal<const IIndirectCmdGenerator*>();
    const auto*const pGpuMemory   = ReadTokenVal<const IGpuMemory*>();
    const gpusize    offset       = ReadTokenVal<gpusize>();
    const uint32     maximumCount = ReadTokenVal<uint32>();
    const gpusize    countGpuAddr = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdExecuteIndirectCmds(*pGenerator, *pGpuMemory, offset, maximumCount, countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdIf);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdIf(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint64>();
    auto mask        = ReadTokenVal<uint64>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    pTgtCmdBuffer->CmdIf(*pGpuMemory, offset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdElse()
{
    InsertToken(CmdBufCallId::CmdElse);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdElse(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdElse();
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    InsertToken(CmdBufCallId::CmdEndIf);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndIf(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndIf();
}

// =====================================================================================================================
void CmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    InsertToken(CmdBufCallId::CmdWhile);
    InsertToken(&gpuMemory);
    InsertToken(offset);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWhile(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto offset      = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint64>();
    auto mask        = ReadTokenVal<uint64>();
    auto compareFunc = ReadTokenVal<CompareFunc>();

    pTgtCmdBuffer->CmdWhile(*pGpuMemory, offset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdEndWhile()
{
    InsertToken(CmdBufCallId::CmdEndWhile);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndWhile(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndWhile();
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToPackedPixelImage(
    const IImage&          srcImage,
    const IImage&          dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    Pal::PackedPixelType   packPixelType)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyImageToPackedPixelImage);
    InsertToken(&srcImage);
    InsertToken(&dstImage);
    InsertTokenArray(pRegions, regionCount);
    InsertToken(packPixelType);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyImageToPackedPixelImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                   pSrcImage      = ReadTokenVal<IImage*>();
    auto                   pDstImage      = ReadTokenVal<IImage*>();
    const ImageCopyRegion* pRegions       = nullptr;
    auto                   regionCount    = ReadTokenArray(&pRegions);
    auto                   packPixelType  = ReadTokenVal<Pal::PackedPixelType>();

    pTgtCmdBuffer->CmdCopyImageToPackedPixelImage(*pSrcImage,
                                                  *pDstImage,
                                                  regionCount,
                                                  pRegions,
                                                  packPixelType);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    InsertToken(CmdBufCallId::CmdSetViewInstanceMask);
    InsertToken(mask);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetViewInstanceMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto mask = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdSetViewInstanceMask(mask);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    InsertToken(CmdBufCallId::CmdUpdateHiSPretests);
    InsertToken(pImage);
    InsertToken(pretests);
    InsertToken(firstMip);
    InsertToken(numMips);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateHiSPretests(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const IImage*     pImage   = ReadTokenVal<IImage*>();
    const HiSPretests pretests = ReadTokenVal<HiSPretests>();
    uint32            firstMip = ReadTokenVal<uint32>();
    uint32            numMips  = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdUpdateHiSPretests(pImage, pretests, firstMip, numMips);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglSync()
{
    InsertToken(CmdBufCallId::CmdFlglSync);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglSync(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdFlglSync();
}

// =====================================================================================================================
void CmdBuffer::CmdFlglEnable()
{
    InsertToken(CmdBufCallId::CmdFlglEnable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglEnable(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
     pTgtCmdBuffer->CmdFlglEnable();
}

// =====================================================================================================================
void  CmdBuffer::CmdFlglDisable()
{
    InsertToken(CmdBufCallId::CmdFlglDisable);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdFlglDisable(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdFlglDisable();
}

// =====================================================================================================================
void CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    InsertToken(CmdBufCallId::CmdBeginPerfExperiment);
    InsertToken(pPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBeginPerfExperiment(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBeginPerfExperiment(ReadTokenVal<IPerfExperiment*>());
}

// =====================================================================================================================
void CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    InsertToken(CmdBufCallId::CmdUpdatePerfExperimentSqttTokenMask);
    InsertToken(pPerfExperiment);
    InsertToken(sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdatePerfExperimentSqttTokenMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    IPerfExperiment*        pPerfExperiment = ReadTokenVal<IPerfExperiment*>();
    const ThreadTraceTokenConfig sqttConfig = ReadTokenVal<ThreadTraceTokenConfig>();
    pTgtCmdBuffer->CmdUpdatePerfExperimentSqttTokenMask(pPerfExperiment, sqttConfig);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    InsertToken(CmdBufCallId::CmdUpdateSqttTokenMask);
    InsertToken(sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdUpdateSqttTokenMask(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdUpdateSqttTokenMask(ReadTokenVal<ThreadTraceTokenConfig>());
}

// =====================================================================================================================
void CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    InsertToken(CmdBufCallId::CmdEndPerfExperiment);
    InsertToken(pPerfExperiment);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdEndPerfExperiment(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdEndPerfExperiment(ReadTokenVal<IPerfExperiment*>());
}

// =====================================================================================================================
void CmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    InsertToken(CmdBufCallId::CmdInsertTraceMarker);
    InsertToken(markerType);
    InsertToken(markerData);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertTraceMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto markerType = ReadTokenVal<PerfTraceMarkerType>();
    auto markerData = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdInsertTraceMarker(markerType, markerData);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    InsertToken(CmdBufCallId::CmdInsertRgpTraceMarker);
    InsertTokenArray(static_cast<const uint32*>(pData), numDwords);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertRgpTraceMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32* pData = nullptr;
    uint32 numDwords = ReadTokenArray(&pData);

    pTgtCmdBuffer->CmdInsertRgpTraceMarker(numDwords, pData);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    InsertToken(CmdBufCallId::CmdSaveComputeState);
    InsertToken(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSaveComputeState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSaveComputeState(ReadTokenVal<uint32>());
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    InsertToken(CmdBufCallId::CmdRestoreComputeState);
    InsertToken(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdRestoreComputeState(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdRestoreComputeState(ReadTokenVal<uint32>());
}

// =====================================================================================================================
void CmdBuffer::CmdCommentString(
    const char* pComment)
{
    InsertToken(CmdBufCallId::CmdCommentString);
    InsertTokenArray(pComment, static_cast<uint32>(strlen(pComment)) + 1);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCommentString(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const char* pComment = nullptr;
    uint32 commentLength = ReadTokenArray(&pComment);
    pTgtCmdBuffer->CmdCommentString(pComment);
}

// =====================================================================================================================
void CmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    InsertToken(CmdBufCallId::CmdNop);
    InsertTokenArray(static_cast<const uint32*>(pPayload), payloadSize);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdNop(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32* pPayload = nullptr;
    uint32 payloadSize = ReadTokenArray(&pPayload);

    pTgtCmdBuffer->CmdNop(pPayload, payloadSize);
}

// =====================================================================================================================
uint32 CmdBuffer::CmdInsertExecutionMarker()
{
    InsertToken(CmdBufCallId::CmdInsertExecutionMarker);

    // We need to let this call go downwards to have the appropriate value to return back to the client.
    return m_pNextLayer->CmdInsertExecutionMarker();
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertExecutionMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdInsertExecutionMarker();
}

// =====================================================================================================================
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    InsertToken(CmdBufCallId::CmdPostProcessFrame);
    InsertToken(postProcessInfo);
    InsertToken((pAddedGpuWork != nullptr) ? *pAddedGpuWork : false);

    // Pass this command on to the next layer.  Clients depend on the pAddedGpuWork output parameter.
    CmdPostProcessFrameInfo nextPostProcessInfo = {};
    GetNextLayer()->CmdPostProcessFrame(*NextCmdPostProcessFrameInfo(postProcessInfo, &nextPostProcessInfo),
                                        pAddedGpuWork);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdPostProcessFrame(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto postProcessInfo = ReadTokenVal<CmdPostProcessFrameInfo>();
    auto addedGpuWork    = ReadTokenVal<bool>();

    pTgtCmdBuffer->CmdPostProcessFrame(postProcessInfo, &addedGpuWork);
}

// =====================================================================================================================
void CmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    InsertToken(CmdBufCallId::CmdSetUserClipPlanes);
    InsertToken(firstPlane);
    InsertTokenArray(pPlanes, planeCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetUserClipPlanes(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const UserClipPlane* pPlanes    = nullptr;
    auto                 firstPlane = ReadTokenVal<uint32>();
    auto                 planeCount = ReadTokenArray(&pPlanes);

    pTgtCmdBuffer->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);
}

// =====================================================================================================================
void CmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    InsertToken(CmdBufCallId::CmdSetClipRects);
    InsertToken(clipRule);
    InsertTokenArray(pRectList, rectCount);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetClipRects(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const Rect* pRectList = nullptr;
    auto        clipRule  = ReadTokenVal<uint16>();
    auto        rectCount = ReadTokenArray(&pRectList);

    pTgtCmdBuffer->CmdSetClipRects(clipRule, rectCount, pRectList);
}

// =====================================================================================================================
void CmdBuffer::CmdStartGpuProfilerLogging()
{
    InsertToken(CmdBufCallId::CmdStartGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdStartGpuProfilerLogging(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdStartGpuProfilerLogging();
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    InsertToken(CmdBufCallId::CmdStopGpuProfilerLogging);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdStopGpuProfilerLogging(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdStopGpuProfilerLogging();
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    InsertToken(CmdBufCallId::CmdXdmaWaitFlipPending);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdXdmaWaitFlipPending(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdXdmaWaitFlipPending();
}

// =====================================================================================================================
// Replays the commands that were recorded on this command buffer into a separate, target command buffer while adding
// additional commands for GPU profiling purposes.
Result CmdBuffer::Replay(
    Queue*            pQueue,
    const CmdBufInfo* pCmdBufInfo,
    uint32            subQueueIdx,
    TargetCmdBuffer*  pNestedTgtCmdBuffer)
{
    typedef void (CmdBuffer::* ReplayFunc)(Queue*, TargetCmdBuffer*);

    constexpr ReplayFunc ReplayFuncTbl[] =
    {
        &CmdBuffer::ReplayBegin,
        &CmdBuffer::ReplayEnd,
        &CmdBuffer::ReplayCmdBindPipeline,
        &CmdBuffer::ReplayCmdPrimeGpuCaches,
        &CmdBuffer::ReplayCmdBindMsaaState,
        &CmdBuffer::ReplayCmdBindColorBlendState,
        &CmdBuffer::ReplayCmdBindDepthStencilState,
        &CmdBuffer::ReplayCmdBindIndexData,
        &CmdBuffer::ReplayCmdBindTargets,
        &CmdBuffer::ReplayCmdBindStreamOutTargets,
        &CmdBuffer::ReplayCmdBindBorderColorPalette,
        &CmdBuffer::ReplayCmdSetUserData,
        &CmdBuffer::ReplayCmdSetVertexBuffers,
        &CmdBuffer::ReplayCmdSetBlendConst,
        &CmdBuffer::ReplayCmdSetInputAssemblyState,
        &CmdBuffer::ReplayCmdSetTriangleRasterState,
        &CmdBuffer::ReplayCmdSetPointLineRasterState,
        &CmdBuffer::ReplayCmdSetLineStippleState,
        &CmdBuffer::ReplayCmdSetDepthBiasState,
        &CmdBuffer::ReplayCmdSetDepthBounds,
        &CmdBuffer::ReplayCmdSetStencilRefMasks,
        &CmdBuffer::ReplayCmdSetMsaaQuadSamplePattern,
        &CmdBuffer::ReplayCmdSetViewports,
        &CmdBuffer::ReplayCmdSetScissorRects,
        &CmdBuffer::ReplayCmdSetGlobalScissor,
        &CmdBuffer::ReplayCmdSetColorWriteMask,
        &CmdBuffer::ReplayCmdBarrier,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
        &CmdBuffer::ReplayCmdRelease,
        &CmdBuffer::ReplayCmdAcquire,
#endif
        &CmdBuffer::ReplayCmdReleaseEvent,
        &CmdBuffer::ReplayCmdAcquireEvent,
        &CmdBuffer::ReplayCmdReleaseThenAcquire,
        &CmdBuffer::ReplayCmdWaitRegisterValue,
        &CmdBuffer::ReplayCmdWaitMemoryValue,
        &CmdBuffer::ReplayCmdWaitBusAddressableMemoryMarker,
        &CmdBuffer::ReplayCmdDraw,
        &CmdBuffer::ReplayCmdDrawOpaque,
        &CmdBuffer::ReplayCmdDrawIndexed,
        &CmdBuffer::ReplayCmdDrawIndirectMulti,
        &CmdBuffer::ReplayCmdDrawIndexedIndirectMulti,
        &CmdBuffer::ReplayCmdDispatch,
        &CmdBuffer::ReplayCmdDispatchIndirect,
        &CmdBuffer::ReplayCmdDispatchOffset,
        &CmdBuffer::ReplayCmdDispatchMesh,
        &CmdBuffer::ReplayCmdDispatchMeshIndirectMulti,
        &CmdBuffer::ReplayCmdUpdateMemory,
        &CmdBuffer::ReplayCmdUpdateBusAddressableMemoryMarker,
        &CmdBuffer::ReplayCmdFillMemory,
        &CmdBuffer::ReplayCmdCopyMemory,
        &CmdBuffer::ReplayCmdCopyTypedBuffer,
        &CmdBuffer::ReplayCmdCopyRegisterToMemory,
        &CmdBuffer::ReplayCmdCopyImage,
        &CmdBuffer::ReplayCmdScaledCopyImage,
        &CmdBuffer::ReplayCmdGenerateMipmaps,
        &CmdBuffer::ReplayCmdColorSpaceConversionCopy,
        &CmdBuffer::ReplayCmdCloneImageData,
        &CmdBuffer::ReplayCmdCopyMemoryToImage,
        &CmdBuffer::ReplayCmdCopyImageToMemory,
        &CmdBuffer::ReplayCmdClearColorBuffer,
        &CmdBuffer::ReplayCmdClearBoundColorTargets,
        &CmdBuffer::ReplayCmdClearColorImage,
        &CmdBuffer::ReplayCmdClearBoundDepthStencilTargets,
        &CmdBuffer::ReplayCmdClearDepthStencil,
        &CmdBuffer::ReplayCmdClearBufferView,
        &CmdBuffer::ReplayCmdClearImageView,
        &CmdBuffer::ReplayCmdResolveImage,
        &CmdBuffer::ReplayCmdSetEvent,
        &CmdBuffer::ReplayCmdResetEvent,
        &CmdBuffer::ReplayCmdPredicateEvent,
        &CmdBuffer::ReplayCmdMemoryAtomic,
        &CmdBuffer::ReplayCmdResetQueryPool,
        &CmdBuffer::ReplayCmdBeginQuery,
        &CmdBuffer::ReplayCmdEndQuery,
        &CmdBuffer::ReplayCmdResolveQuery,
        &CmdBuffer::ReplayCmdSetPredication,
        &CmdBuffer::ReplayCmdSuspendPredication,
        &CmdBuffer::ReplayCmdWriteTimestamp,
        &CmdBuffer::ReplayCmdWriteImmediate,
        &CmdBuffer::ReplayCmdLoadBufferFilledSizes,
        &CmdBuffer::ReplayCmdSaveBufferFilledSizes,
        &CmdBuffer::ReplayCmdSetBufferFilledSize,
        &CmdBuffer::ReplayCmdLoadCeRam,
        &CmdBuffer::ReplayCmdWriteCeRam,
        &CmdBuffer::ReplayCmdDumpCeRam,
        &CmdBuffer::ReplayCmdExecuteNestedCmdBuffers,
        &CmdBuffer::ReplayCmdExecuteIndirectCmds,
        &CmdBuffer::ReplayCmdIf,
        &CmdBuffer::ReplayCmdElse,
        &CmdBuffer::ReplayCmdEndIf,
        &CmdBuffer::ReplayCmdWhile,
        &CmdBuffer::ReplayCmdEndWhile,
        &CmdBuffer::ReplayCmdFlglSync,
        &CmdBuffer::ReplayCmdFlglEnable,
        &CmdBuffer::ReplayCmdFlglDisable,
        &CmdBuffer::ReplayCmdBeginPerfExperiment,
        &CmdBuffer::ReplayCmdUpdatePerfExperimentSqttTokenMask,
        &CmdBuffer::ReplayCmdUpdateSqttTokenMask,
        &CmdBuffer::ReplayCmdEndPerfExperiment,
        &CmdBuffer::ReplayCmdInsertTraceMarker,
        &CmdBuffer::ReplayCmdInsertRgpTraceMarker,
        &CmdBuffer::ReplayCmdSaveComputeState,
        &CmdBuffer::ReplayCmdRestoreComputeState,
        &CmdBuffer::ReplayCmdSetUserClipPlanes,
        &CmdBuffer::ReplayCmdCommentString,
        &CmdBuffer::ReplayCmdNop,
        &CmdBuffer::ReplayCmdInsertExecutionMarker,
        &CmdBuffer::ReplayCmdXdmaWaitFlipPending,
        &CmdBuffer::ReplayCmdCopyMemoryToTiledImage,
        &CmdBuffer::ReplayCmdCopyTiledImageToMemory,
        &CmdBuffer::ReplayCmdCopyImageToPackedPixelImage,
        &CmdBuffer::ReplayCmdStartGpuProfilerLogging,
        &CmdBuffer::ReplayCmdStopGpuProfilerLogging,
        &CmdBuffer::ReplayCmdSetViewInstanceMask,
        &CmdBuffer::ReplayCmdUpdateHiSPretests,
        &CmdBuffer::ReplayCmdSetPerDrawVrsRate,
        &CmdBuffer::ReplayCmdSetVrsCenterState,
        &CmdBuffer::ReplayCmdBindSampleRateImage,
        &CmdBuffer::ReplayCmdResolvePrtPlusImage,
        &CmdBuffer::ReplayCmdSetClipRects,
        &CmdBuffer::ReplayCmdPostProcessFrame,
    };

    static_assert(ArrayLen(ReplayFuncTbl) == static_cast<size_t>(CmdBufCallId::Count),
                  "Replay table must be updated!");

    Result result = Result::Success;

    // Don't even try to replay the stream if some error occured during recording.
    if (m_tokenStreamResult == Result::Success)
    {
        // Start reading from the beginning of the token stream.
        m_tokenReadOffset = 0;

        CmdBufCallId     callId;
        TargetCmdBuffer* pTgtCmdBuffer = pNestedTgtCmdBuffer;

        do
        {
            callId = ReadTokenVal<CmdBufCallId>();

            // If pNestedTgtCmdBuffer is non-null then this replay is for a nested execute, and no splitting
            // tokens have been inserted into the token stream. Otherwise, aquire a non-nested target command
            // buffer for replay on seeing a Begin token, which are used to split primary command buffers here.
            if ((pNestedTgtCmdBuffer == nullptr) && (callId == CmdBufCallId::Begin))
            {
                pTgtCmdBuffer = pQueue->AcquireCmdBuf(pCmdBufInfo, subQueueIdx, false);
            }

            PAL_ASSERT(pTgtCmdBuffer != nullptr);

            (this->*ReplayFuncTbl[static_cast<uint32>(callId)])(pQueue, pTgtCmdBuffer);

            result = pTgtCmdBuffer->GetLastResult();
        } while ((m_tokenReadOffset != m_tokenWriteOffset) && (result == Result::Success));
    }

    // In the event that the command buffer is replayed multiple times, we have to reset the inherited state here.
    m_pLastTgtCmdBuffer = nullptr;

    return result;
}

// =====================================================================================================================
TargetCmdBuffer::TargetCmdBuffer(
    const CmdBufferCreateInfo& createInfo,
    ICmdBuffer*                pNextCmdBuffer,
    const DeviceDecorator*     pNextDevice)
    :
    CmdBufferFwdDecorator(pNextCmdBuffer, pNextDevice),
#if (PAL_COMPILE_TYPE == 32)
    m_allocator(2 * 1024 * 1024),
#else
    m_allocator(8 * 1024 * 1024),
#endif
    m_pAllocatorStream(nullptr),
    m_queueType(createInfo.queueType),
    m_engineType(createInfo.engineType),
    m_supportTimestamps(false),
    m_result(Result::Success),
    m_nestedCmdBufCount(0),
    m_subQueueIdx(BadSubQueueIdx),
    m_pCmdBufInfo(nullptr)
{
}

// =====================================================================================================================
Result TargetCmdBuffer::Init()
{
    Result result = m_allocator.Init();

    if (result == Result::Success)
    {
        m_pAllocatorStream = m_allocator.Current();
    }

    DeviceProperties info;
    if (result == Result::Success)
    {
        result = m_pDevice->GetProperties(&info);
    }

    if (result == Result::Success)
    {
        m_supportTimestamps = info.engineProperties[m_engineType].flags.supportsTimestamps ? true : false;
    }

    return result;
}

// =====================================================================================================================
Result TargetCmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    // Rewind the allocator to the beginning, overwriting any data stored from the last time this command buffer was
    // recorded.
    m_allocator.Rewind(m_pAllocatorStream, false);

    m_result = CmdBufferFwdDecorator::Begin(info);
    return m_result;
}

// =====================================================================================================================
// Set the last result - do not allow Success to override non-Success
void TargetCmdBuffer::SetLastResult(
    Result result)
{
    if (m_result == Result::Success)
    {
        m_result = result;
    }
}

// =====================================================================================================================
void TargetCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    m_nestedCmdBufCount += cmdBufferCount;

    CmdBufferFwdDecorator::CmdExecuteNestedCmdBuffers(cmdBufferCount, ppCmdBuffers);
}

// =====================================================================================================================
Result TargetCmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    m_nestedCmdBufCount = 0;
    m_subQueueIdx       = BadSubQueueIdx;
    m_pCmdBufInfo       = nullptr;

    return CmdBufferFwdDecorator::Reset(pCmdAllocator, returnGpuMemory);
}

} // GpuDebug
} // Pal

#endif
