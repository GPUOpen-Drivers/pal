/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/gpuDebug/gpuDebugCmdBuffer.h"
#include "core/layers/gpuDebug/directDrawSurface.h"
#include "core/layers/gpuDebug/gpuDebugColorBlendState.h"
#include "core/layers/gpuDebug/gpuDebugColorTargetView.h"
#include "core/layers/gpuDebug/gpuDebugDepthStencilView.h"
#include "core/layers/gpuDebug/gpuDebugDevice.h"
#include "core/layers/gpuDebug/gpuDebugImage.h"
#include "core/layers/gpuDebug/gpuDebugPipeline.h"
#include "core/layers/gpuDebug/gpuDebugQueue.h"
#include "g_platformSettings.h"
#include "palAutoBuffer.h"
#include "palFile.h"
#include "palFormatInfo.h"
#include "palHsaAbiMetadata.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include "palLiterals.h"
#include <cinttypes>

// This is required because we need the definition of the D3D12DDI_PRESENT_0003 struct in order to make a copy of the
// data in it for the tokenization.

using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace GpuDebug
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferDecorator(pNextCmdBuffer, static_cast<DeviceDecorator*>(pDevice->GetNextLayer())),
    m_pDevice(pDevice),
    m_allocator(1_MiB),
    m_supportsComments(Device::SupportsCommentString(createInfo.queueType)),
    m_singleStep(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.singleStep),
    m_cacheFlushInvOnAction(pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.cacheFlushInvOnAction),
    m_breakOnDrawDispatchCount(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.submitOnActionCount),
    m_pTimestamp(nullptr),
    m_timestampAddr(0),
    m_counter(0),
    m_engineType(createInfo.engineType),
    m_verificationOptions(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.verificationOptions),
    m_pBoundPipelines{},
    m_boundTargets(),
    m_pBoundBlendState(nullptr),
    m_pTokenStream(nullptr),
    m_tokenStreamSize(m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.tokenAllocatorSize),
    m_tokenWriteOffset(0),
    m_tokenReadOffset(0),
    m_tokenStreamResult(Result::Success),
    m_buildInfo(),
    m_pLastTgtCmdBuffer(nullptr),
    m_numReleaseTokens(0),
    m_releaseTokenList(static_cast<Platform*>(m_pDevice->GetPlatform()))
{
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]   = &CmdBuffer::CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)]  = &CmdBuffer::CmdSetUserDataGfx;

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
    m_surfaceCapture.actionIdStart        = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureStart;
    m_surfaceCapture.actionIdCount        = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureCount;
    m_surfaceCapture.hash                 = pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig.surfaceCaptureHash;
    m_surfaceCapture.blitImgOpEnabledMask = pDevice->GetPlatform()->PlatformSettings().
                                            gpuDebugConfig.blitSurfaceCaptureBitmask;
    m_surfaceCapture.filenameHashType     = pDevice->GetPlatform()->PlatformSettings().
                                            gpuDebugConfig.surfaceCaptureFilenameHashType;
}

// =====================================================================================================================
CmdBuffer::~CmdBuffer()
{
    PAL_FREE(m_pTokenStream, m_pDevice->GetPlatform());

    DestroySurfaceCaptureData();

    if (m_surfaceCapture.pActions != nullptr)
    {
        PAL_SAFE_FREE(m_surfaceCapture.pActions, m_pDevice->GetPlatform());
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
        if (result == Result::Success)
        {
            m_surfaceCapture.pActions = static_cast<ActionInfo*>(
                PAL_CALLOC(sizeof(ActionInfo) * m_surfaceCapture.actionIdCount,
                           m_pDevice->GetPlatform(),
                           AllocInternal));

            if (m_surfaceCapture.pActions == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        const uint32 colorSurfCount = m_surfaceCapture.actionIdCount * MaxColorTargets;
        const uint32 depthSurfCount = m_surfaceCapture.actionIdCount * MaxDepthTargetPlanes;
        const uint32 maxBlitImgCaptureNum = m_surfaceCapture.actionIdCount;
        const uint32 totalSurfCount = colorSurfCount + depthSurfCount + maxBlitImgCaptureNum;
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

    CmdWriteImmediate(PipelineStageTopOfPipe,
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
// Returns true if surface capture is active at the current point of recording in this command buffer.
// If checkMask is 0, then it means we want to check whether the draw capture is active.
// If checkMask is not 0, then we would check whether it matches an active blt capture option.
bool CmdBuffer::IsSurfaceCaptureActive(
    EnabledBlitOperations checkMask) const
{
    bool res = false;

    if (checkMask == NoBlitCapture)
    {
        res = ((m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart) &&
               (m_surfaceCapture.actionId < (m_surfaceCapture.actionIdStart + m_surfaceCapture.actionIdCount)) &&
               m_surfaceCapture.pipelineMatch);
    }
    else
    {
        res = ((m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart) &&
               (m_surfaceCapture.actionId < (m_surfaceCapture.actionIdStart + m_surfaceCapture.actionIdCount)) &&
               (checkMask & m_surfaceCapture.blitImgOpEnabledMask));
    }

    return res;
}

// =====================================================================================================================
// Determines if the current pipeline hash matches the surface capture hash
void CmdBuffer::SurfaceCaptureHashMatch()
{
    m_surfaceCapture.pipelineMatch = false;

    if (IsSurfaceCaptureEnabled())
    {
        if (m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Graphics)] != nullptr)
        {
            const PipelineInfo& pipeInfo =
                m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Graphics)]->GetInfo();

            m_surfaceCapture.pipelineMatch |=
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
}

// =====================================================================================================================
// Creates images and memory for surface capture and copies data to those images
void CmdBuffer::CaptureSurfaces(
    Developer::DrawDispatchType drawDispatchType)
{
    PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);

    const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
    PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

    ActionInfo* pAction         = &m_surfaceCapture.pActions[actionIndex];
    pAction->drawId             = m_surfaceCapture.drawId;
    pAction->drawDispatchType   = drawDispatchType;

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
                    LayoutColorTarget,
                    LayoutUniversalEngine,
                    CoherColorTarget,
                    ctvCreateInfo.imageInfo.baseSubRes,
                    ctvCreateInfo.imageInfo.arraySize,
                    true,
                    &pDstImage);

                if (result == Result::Success)
                {
                    // Store the image object pointer in our array of capture data
                    PAL_ASSERT(pAction->pColorTargetDsts[mrt] == nullptr);
                    pAction->pColorTargetDsts[mrt] = static_cast<Image*>(pDstImage);
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

            SubresRange range = { };
            result = pSrcImage->GetFullSubresourceRange(&range);
            if (result == Result::Success)
            {
                numPlanes = range.numPlanes;
            }

            for (uint32 plane = 0; plane < numPlanes; plane++)
            {
                IImage* pDstImage = nullptr;

                SubresId subresId   = { };
                subresId.plane      = plane;
                subresId.mipLevel   = dsvCreateInfo.mipLevel;
                subresId.arraySlice = dsvCreateInfo.baseArraySlice;

                result = CaptureImageSurface(
                    pSrcImage,
                    LayoutDepthStencilTarget,
                    LayoutUniversalEngine,
                    CoherDepthStencilTarget,
                    subresId,
                    dsvCreateInfo.arraySize,
                    true,
                    &pDstImage);

                if (result == Result::Success)
                {
                    PAL_ASSERT(pAction->pDepthTargetDsts[plane] == nullptr);
                    pAction->pDepthTargetDsts[plane] = static_cast<Image*>(pDstImage);
                }
                else
                {
                    PAL_DPWARN("Failed to capture DSV Plane:%d, Error:0x%x", plane, result);
                }
            }
        }
    }

    if (m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Graphics)] != nullptr)
    {
        constexpr ShaderType FilenameHashTypeToShaderType[] =
        {
            ShaderType::Task, // Not used
            ShaderType::Task, // Not used
            ShaderType::Task,
            ShaderType::Vertex,
            ShaderType::Hull,
            ShaderType::Domain,
            ShaderType::Geometry,
            ShaderType::Mesh,
            ShaderType::Pixel,
        };
        static_assert(sizeof(PipelineHash) == sizeof(ShaderHash), "");
        static_assert(ArrayLen(FilenameHashTypeToShaderType) == FilenameHashPs + 1, "");
        const PipelineInfo& pipeInfo = m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Graphics)]->GetInfo();

        if (m_surfaceCapture.filenameHashType == FilenameHashPipeline)
        {
            pAction->actionHash = pipeInfo.internalPipelineHash;
        }
        else if ((m_surfaceCapture.filenameHashType >= FilenameHashTask) &&
                 (m_surfaceCapture.filenameHashType <= FilenameHashPs))
        {
            memcpy(&pAction->actionHash,
                   &pipeInfo.shader[static_cast<uint32_t>(
                       FilenameHashTypeToShaderType[m_surfaceCapture.filenameHashType])].hash,
                  sizeof(PipelineHash));
        }
        else
        {
            pAction->actionHash = {};
        }
    }
}

// =====================================================================================================================
// Helper function for CaptureSurfaces()
// Allocates a destination image and backing memory. Then copies from the src to the dst
// This function captures a single plane and mip level.
// All array slices included in the baseSubres and arraySize are captured
Result CmdBuffer::CaptureImageSurface(
    const IImage*                  pSrcImage,      // [in] pointer to the surface to capture.
    const ImageLayoutUsageFlags    srcLayoutUsages,
    const ImageLayoutEngineFlags   srcLayoutEngine,
    const CacheCoherencyUsageFlags srcCoher,
    const SubresId&                baseSubres,     // Specifies the plane, mip level, and base array slice.
    const uint32                   arraySize,
    bool                           isDraw,
    IImage**                       ppDstImage)
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
        OverrideDepthFormat(&imageCreateInfo.swizzledFormat, pSrcImage, 0);
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
        // Send Barrier to prepare for upcoming copy
        // Transition 0 : Src from the given Target to Copy Src
        // Transition 1 : Dst from Uninitialized to Copy Dst
        BarrierInfo preCopyBarrier = {};

        BarrierTransition preCopyTransitions[2]                 = {};
        preCopyTransitions[0].srcCacheMask                      = srcCoher;
        preCopyTransitions[0].dstCacheMask                      = CoherCopySrc;
        preCopyTransitions[0].imageInfo.pImage                  = pSrcImage;
        preCopyTransitions[0].imageInfo.subresRange.startSubres = baseSubres;
        preCopyTransitions[0].imageInfo.subresRange.numPlanes   = 1;
        preCopyTransitions[0].imageInfo.subresRange.numMips     = 1;
        preCopyTransitions[0].imageInfo.subresRange.numSlices   = arraySize;
        preCopyTransitions[0].imageInfo.oldLayout.usages        = srcLayoutUsages;
        preCopyTransitions[0].imageInfo.oldLayout.engines       = srcLayoutEngine;
        preCopyTransitions[0].imageInfo.newLayout.usages        = LayoutCopySrc;
        preCopyTransitions[0].imageInfo.newLayout.engines       = LayoutUniversalEngine;

        preCopyTransitions[1].srcCacheMask                      = 0;
        preCopyTransitions[1].dstCacheMask                      = CoherCopyDst;
        preCopyTransitions[1].imageInfo.pImage                  = pDstImage;
        preCopyTransitions[1].imageInfo.subresRange.startSubres = baseSubres;
        preCopyTransitions[1].imageInfo.subresRange.startSubres.plane = 0;
        preCopyTransitions[1].imageInfo.subresRange.numPlanes   = 1;
        preCopyTransitions[1].imageInfo.subresRange.numMips     = 1;
        preCopyTransitions[1].imageInfo.subresRange.numSlices   = arraySize;
        preCopyTransitions[1].imageInfo.oldLayout.usages        = LayoutUninitializedTarget;
        preCopyTransitions[1].imageInfo.oldLayout.engines       = LayoutUniversalEngine;
        preCopyTransitions[1].imageInfo.newLayout.usages        = LayoutCopyDst | LayoutUncompressed;
        preCopyTransitions[1].imageInfo.newLayout.engines       = LayoutUniversalEngine;

        preCopyBarrier.waitPoint            = HwPipePreBlt;

        const HwPipePoint blitWaitForPipePoint = HwPipePostBlt;
        if (isDraw)
        {
            preCopyBarrier.rangeCheckedTargetWaitCount = 1;
            preCopyBarrier.ppTargets = &pSrcImage;
        }
        else
        {
            preCopyBarrier.pipePointWaitCount = 1;
            preCopyBarrier.pPipePoints = &blitWaitForPipePoint;
        }

        preCopyBarrier.transitionCount  = 2;
        preCopyBarrier.pTransitions     = &preCopyTransitions[0];

        CmdBarrier(preCopyBarrier);

        // Send Copy Cmd
        ImageLayout srcLayout = { 0 };
        srcLayout.usages  = isDraw ? (srcLayoutUsages | LayoutCopySrc) : LayoutCopySrc;
        srcLayout.engines = LayoutUniversalEngine;

        ImageLayout dstLayout = { 0 };
        dstLayout.usages  = LayoutCopyDst | LayoutUncompressed;
        dstLayout.engines = LayoutUniversalEngine;

        ImageCopyRegion region;
        memset(&region, 0, sizeof(region));
        region.srcSubres        = baseSubres;
        region.dstSubres        = baseSubres;
        region.dstSubres.plane  = 0;
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

        // Send Barrier for post copy transitions
        // Transition Src from Copy Src to Color/Depth Target
        BarrierInfo postCopyBarrier = {};

        BarrierTransition postCopyTransition                 = {};
        postCopyTransition.srcCacheMask                      = CoherCopySrc;
        postCopyTransition.dstCacheMask                      = srcCoher;
        postCopyTransition.imageInfo.pImage                  = pSrcImage;
        postCopyTransition.imageInfo.subresRange.startSubres = baseSubres;
        postCopyTransition.imageInfo.subresRange.numPlanes   = 1;
        postCopyTransition.imageInfo.subresRange.numMips     = 1;
        postCopyTransition.imageInfo.subresRange.numSlices   = arraySize;
        postCopyTransition.imageInfo.oldLayout.usages        = LayoutCopySrc;
        postCopyTransition.imageInfo.oldLayout.engines       = LayoutUniversalEngine;
        postCopyTransition.imageInfo.newLayout.usages        = srcLayoutUsages;
        postCopyTransition.imageInfo.newLayout.engines       = srcLayoutEngine;

        postCopyBarrier.waitPoint           = isDraw ? HwPipePreRasterization : HwPipePreBlt;

        const HwPipePoint postCopyPipePoint = HwPipePostBlt;
        postCopyBarrier.pipePointWaitCount  = 1;
        postCopyBarrier.pPipePoints         = &postCopyPipePoint;

        postCopyBarrier.transitionCount  = 1;
        postCopyBarrier.pTransitions     = &postCopyTransition;

        CmdBarrier(postCopyBarrier);
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
// Issues barrier calls to sync all of surface capture's output images
void CmdBuffer::SyncSurfaceCapture()
{
    BarrierInfo barrierInfo      = {};
    BarrierTransition transition = {};

    transition.srcCacheMask                                 = CoherCopyDst;
    transition.dstCacheMask                                 = CoherCpu;
    transition.imageInfo.subresRange.startSubres.plane      = 0;
    transition.imageInfo.subresRange.startSubres.mipLevel   = 0;
    transition.imageInfo.subresRange.startSubres.arraySlice = 0;
    transition.imageInfo.subresRange.numPlanes              = 1;
    transition.imageInfo.oldLayout.usages                   = LayoutCopyDst | LayoutUncompressed;
    transition.imageInfo.oldLayout.engines                  = LayoutUniversalEngine;
    transition.imageInfo.newLayout.usages                   = LayoutUncompressed;
    transition.imageInfo.newLayout.engines                  = LayoutUniversalEngine;

    barrierInfo.transitionCount  = 1;
    barrierInfo.pTransitions     = &transition;

    barrierInfo.waitPoint           = HwPipeTop;

    const HwPipePoint pipePoint     = HwPipeBottom;
    barrierInfo.pipePointWaitCount  = 1;
    barrierInfo.pPipePoints         = &pipePoint;

    if (m_surfaceCapture.pActions != nullptr)
    {
        for (uint32 action = 0; action < m_surfaceCapture.actionIdCount; action++)
        {
            ActionInfo* pAction = &m_surfaceCapture.pActions[action];

            for (uint32 mrt = 0; mrt < MaxColorTargets; mrt++)
            {
                Image* pImage = pAction->pColorTargetDsts[mrt];
                if (pImage != nullptr)
                {
                    ImageCreateInfo imageInfo                   = pImage->GetImageCreateInfo();

                    transition.imageInfo.pImage                 = pImage;
                    transition.imageInfo.subresRange.numMips    = imageInfo.mipLevels;
                    transition.imageInfo.subresRange.numSlices  = imageInfo.arraySize;
                    CmdBarrier(barrierInfo);
                }
            }

            for (uint32 plane = 0; plane < MaxDepthTargetPlanes; plane++)
            {
                Image* pImage = pAction->pDepthTargetDsts[plane];
                if (pImage != nullptr)
                {
                    ImageCreateInfo imageInfo                   = pImage->GetImageCreateInfo();

                    transition.imageInfo.pImage                 = pImage;
                    transition.imageInfo.subresRange.numMips    = imageInfo.mipLevels;
                    transition.imageInfo.subresRange.numSlices  = imageInfo.arraySize;
                    CmdBarrier(barrierInfo);
                }
             }

            if (pAction->pBlitImg != nullptr)
            {
                ImageCreateInfo imageInfo                   = pAction->pBlitImg->GetImageCreateInfo();

                transition.imageInfo.pImage                 = pAction->pBlitImg;
                transition.imageInfo.subresRange.numMips    = imageInfo.mipLevels;
                transition.imageInfo.subresRange.numSlices  = imageInfo.arraySize;
                CmdBarrier(barrierInfo);
            }
        }
    }
}

// =====================================================================================================================
// Writes the data for surface capture to disk.
// This function must be called only after this command buffer has finished execution.
void CmdBuffer::OutputSurfaceCapture()
{
    Result result    = Result::Success;
    int64  captureTS = GetPerfCpuTime();

    char         filePath[256] = {};
    char         hashStr[128]  = {};
    const uint32 frameId       = static_cast<Platform*>(m_pDevice->GetPlatform())->FrameCount();
    auto*        pDebugConfig  = &m_pDevice->GetPlatform()->PlatformSettings().gpuDebugConfig;

    result = m_pDevice->GetPlatform()->CreateLogDir(pDebugConfig->surfaceCaptureLogDirectory);

    if (result == Result::Success)
    {
        Snprintf(filePath, sizeof(filePath), "%s", m_pDevice->GetPlatform()->LogDirPath());

        result = MkDir(&filePath[0]);
    }

    const size_t endOfString = strlen(&filePath[0]);

    if (((result == Result::Success) || (result == Result::AlreadyExists)) && (m_surfaceCapture.pActions != nullptr))
    {
        constexpr const char* HashTypePrefixStr[] = {"", "Pipeline", "Tash", "Vs", "Hs", "Ds", "Gs", "Mesh","Ps"};
        static_assert(ArrayLen(HashTypePrefixStr) == FilenameHashPs + 1, "");

        for (uint32 action = 0; action < m_surfaceCapture.actionIdCount; action++)
        {
            ActionInfo* pAction = &m_surfaceCapture.pActions[action];

            char fileName[256] = {};
            if (m_surfaceCapture.filenameHashType != FilenameHashNoHash)
            {
                Snprintf(hashStr,
                         sizeof(hashStr),
                         "%s_%08llx_%08llx_",
                         HashTypePrefixStr[m_surfaceCapture.filenameHashType],
                         pAction->actionHash.stable,
                         pAction->actionHash.unique);
            }

            const char* DrawNameStrs[] =
            {
                "CmdDraw",
                "CmdDrawOpaque",
                "CmdDrawIndexed",
                "CmdDrawIndirectMulti",
                "CmdDrawIndexedIndirectMulti",
                "CmdDispatchMesh",
                "CmdDispatchMeshIndirectMulti",
                "CmdGenExecuteIndirectDraw",
                "CmdGenExecuteIndirectDrawIndexed",
                "CmdGenExecuteIndirectDispatchMesh",
                "CmdDispatch",
                "CmdDispatchAce",
                "CmdDispatchIndirect",
                "CmdDispatchOffset",
                "CmdGenExecuteIndirectDispatch",
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 837
                "CmdDispatchDynamic",
#endif
            };
            static_assert(ArrayLen(DrawNameStrs) == static_cast<uint32>(Developer::DrawDispatchType::Count));

            const char* pDrawNameStr =
                (pAction->drawDispatchType < Developer::DrawDispatchType::Count) ?
                DrawNameStrs[uint32(pAction->drawDispatchType)] : "UnknownDrawType";

            // Output render targets
            for (uint32 mrt = 0; mrt < MaxColorTargets; mrt++)
            {
                Image* pImage    = pAction->pColorTargetDsts[mrt];

                if (pImage != nullptr)
                {
                    Snprintf(fileName,
                             sizeof(fileName),
                             "Frame%d_CmdBuf%d_Action%d_TS0x%llx_CaptureId%d_%s_%s_RT%d",
                             frameId,
                             UniqueId(),
                             pAction->drawId,
                             captureTS,
                             m_surfaceCapture.actionIdStart + action,
                             hashStr,
                             pDrawNameStr,
                             mrt);

                    OutputSurfaceCaptureImage(pImage, &filePath[0], &fileName[0]);
                }
            }

            // Output depth stencil
            for (uint32 plane = 0; plane < MaxDepthTargetPlanes; plane++)
            {
                Image* pImage = pAction->pDepthTargetDsts[plane];

                if (pImage != nullptr)
                {
                    Snprintf(fileName,
                             sizeof(fileName),
                             "Frame%d_CmdBuf%d_Action%d_TS0x%llx_CaptureId%d_%s_%s_DSV%d",
                             frameId,
                             UniqueId(),
                             pAction->drawId,
                             captureTS,
                             m_surfaceCapture.actionIdStart + action,
                             hashStr,
                             pDrawNameStr,
                             plane);

                    OutputSurfaceCaptureImage(pImage, &filePath[0], &fileName[0]);
                }
            }

            // Output blit images capture
            if (pAction->pBlitImg != nullptr)
            {
                char opNameStr[256] = {};
                switch (pAction->blitOpMask)
                {
                case EnableCmdCopyMemoryToImage:
                    Snprintf(opNameStr, sizeof(opNameStr), "%s", "CmdCopyMemoryToImage");
                    break;
                case EnableCmdClearColorImage:
                    Snprintf(opNameStr, sizeof(opNameStr), "%s", "CmdClearColorImage");
                    break;
                case EnableCmdClearDepthStencil:
                    Snprintf(opNameStr, sizeof(opNameStr), "%s", "CmdClearDepthStencil");
                    break;
                case EnableCmdCopyImageToMemory:
                    Snprintf(opNameStr, sizeof(opNameStr), "%s", "CmdCopyImageToMemory");
                    break;
                default:
                    // An unidentified blit operation mask.
                    PAL_ASSERT_ALWAYS();
                    break;
                }

                Snprintf(fileName,
                         sizeof(fileName),
                         "Frame%d_CmdBuf%d_TS0x%llx_CaptureId%d_%s",
                         frameId,
                         UniqueId(),
                         captureTS,
                         m_surfaceCapture.actionIdStart + action,
                         opNameStr);

                OutputSurfaceCaptureImage(pAction->pBlitImg, &filePath[0], &fileName[0]);
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
        DdsHeaderFull   ddsHeader  = {};
        size_t          ddsHeaderSize = 0;

        if (imageInfo.mipLevels == 1)
        {
            SubresId subresId = { };
            SubresLayout subresLayout = {};
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
    if (m_surfaceCapture.pActions != nullptr)
    {
        for (uint32 i = 0; i < m_surfaceCapture.actionIdCount; i++)
        {
            ActionInfo* pAction = &m_surfaceCapture.pActions[i];

            for (uint32 j = 0; j < MaxColorTargets; j++)
            {
                if (pAction->pColorTargetDsts[j] != nullptr)
                {
                    pAction->pColorTargetDsts[j]->Destroy();
                    PAL_SAFE_FREE(pAction->pColorTargetDsts[j], m_pDevice->GetPlatform());
                }
            }

            for (uint32 j = 0; j < MaxDepthTargetPlanes; j++)
            {
                if (pAction->pDepthTargetDsts[j] != nullptr)
                {
                    pAction->pDepthTargetDsts[j]->Destroy();
                    PAL_SAFE_FREE(pAction->pDepthTargetDsts[j], m_pDevice->GetPlatform());
                }
            }

            if (pAction->pBlitImg != nullptr)
            {
                pAction->pBlitImg->Destroy();
                PAL_SAFE_FREE(pAction->pBlitImg, m_pDevice->GetPlatform());
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
    // Reset is an optional call, if the app calls it we might as well clean up some of our heavier state objects.
    // Note that we'll do a full reset when they call Begin later on.
    m_surfaceCapture.actionId = 0;
    m_surfaceCapture.drawId = 0;
    DestroySurfaceCaptureData();

    return GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    // Reset any API state tracking.
    m_counter           = 0;
    m_pLastTgtCmdBuffer = nullptr;
    m_pBoundBlendState  = nullptr;

    for (uint32 idx = 0; idx < static_cast<uint32>(PipelineBindPoint::Count); ++idx)
    {
        m_pBoundPipelines[idx] = nullptr;
    }

    memset(&m_boundTargets, 0, sizeof(m_boundTargets));

    m_releaseTokenList.Clear();
    m_numReleaseTokens = 0;

    m_surfaceCapture.actionId = 0;
    m_surfaceCapture.drawId = 0;
    DestroySurfaceCaptureData();

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

        pTgtCmdBuffer->CmdWriteImmediate(PipelineStageTopOfPipe,
                                         reinterpret_cast<uint64>(this),
                                         ImmediateDataWidth::ImmediateData64Bit,
                                         m_timestampAddr + offsetof(CmdBufferTimestampData, cmdBufferHash));
        pTgtCmdBuffer->CmdWriteImmediate(PipelineStageTopOfPipe,
                                         0,
                                         ImmediateDataWidth::ImmediateData32Bit,
                                         m_timestampAddr + offsetof(CmdBufferTimestampData, counter));
    }

    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    SyncSurfaceCapture();

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
    PAL_ASSERT(m_numReleaseTokens == m_releaseTokenList.NumElements());

    Result result = pTgtCmdBuffer->End();
    pTgtCmdBuffer->SetLastResult(result);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    m_pBoundPipelines[static_cast<uint32>(params.pipelineBindPoint)] = params.pPipeline;

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
void CmdBuffer::CmdSaveGraphicsState()
{
    InsertToken(CmdBufCallId::CmdSaveGraphicsState);
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreGraphicsState()
{
    InsertToken(CmdBufCallId::CmdRestoreGraphicsState);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindMsaaState(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdBindMsaaState(ReadTokenVal<IMsaaState*>());
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSaveGraphicsState(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdSaveGraphicsState();
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdRestoreGraphicsState(
    Queue*            pQueue,
    TargetCmdBuffer*  pTgtCmdBuffer)
{
    pTgtCmdBuffer->CmdRestoreGraphicsState();
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

    // Views don't guarantee they'll outlive the submit, so copy them in.
    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        if (params.colorTargets[i].pColorTargetView != nullptr)
        {
            InsertTokenBuffer(params.colorTargets[i].pColorTargetView, m_pDevice->ColorViewSize(), alignof(void*));
        }
    }

    if (params.depthTarget.pDepthStencilView != nullptr)
    {
        InsertTokenBuffer(params.depthTarget.pDepthStencilView, m_pDevice->DepthViewSize(), alignof(void*));
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdBindTargets(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    BindTargetParams params = ReadTokenVal<BindTargetParams>();

    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        if (params.colorTargets[i].pColorTargetView != nullptr)
        {
            ReadTokenBuffer(reinterpret_cast<const void**>(&params.colorTargets[i].pColorTargetView), alignof(void*));
        }
    }

    if (params.depthTarget.pDepthStencilView != nullptr)
    {
        ReadTokenBuffer(reinterpret_cast<const void**>(&params.depthTarget.pDepthStencilView), alignof(void*));
    }

    pTgtCmdBuffer->CmdBindTargets(params);
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
void CmdBuffer::CmdDuplicateUserData(
    PipelineBindPoint source,
    PipelineBindPoint dest)
{
    InsertToken(CmdBufCallId::CmdDuplicateUserData);
    InsertToken(source);
    InsertToken(dest);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDuplicateUserData(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto sourceBindPoint = ReadTokenVal<PipelineBindPoint>();
    const auto destBindPoint   = ReadTokenVal<PipelineBindPoint>();

    pTgtCmdBuffer->CmdDuplicateUserData(sourceBindPoint, destBindPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdSetKernelArguments(
    uint32            firstArg,
    uint32            argCount,
    const void*const* ppValues)
{
    InsertToken(CmdBufCallId::CmdSetKernelArguments);
    InsertToken(firstArg);
    InsertToken(argCount);

    // There must be an HSA ABI pipeline bound if you call this function.
    const IPipeline*const pPipeline = m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Compute)];
    PAL_ASSERT((pPipeline != nullptr) && (pPipeline->GetInfo().flags.hsaAbi == 1));

    for (uint32 idx = 0; idx < argCount; ++idx)
    {
        const auto* pArgument = pPipeline->GetKernelArgument(firstArg + idx);
        PAL_ASSERT(pArgument != nullptr);

        const size_t valueSize = pArgument->size;
        PAL_ASSERT(valueSize > 0);

        InsertTokenArray(static_cast<const uint8*>(ppValues[idx]), static_cast<uint32>(valueSize));
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetKernelArguments(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const uint32 firstArg = ReadTokenVal<uint32>();
    const uint32 argCount = ReadTokenVal<uint32>();
    AutoBuffer<const void*, 16, Platform> values(argCount, static_cast<Platform*>(m_pDevice->GetPlatform()));

    if (values.Capacity() < argCount)
    {
        pTgtCmdBuffer->SetLastResult(Result::ErrorOutOfMemory);
    }
    else
    {
        for (uint32 idx = 0; idx < argCount; ++idx)
        {
            const uint8* pArray;
            ReadTokenArray(&pArray);
            values[idx] = pArray;
        }

        pTgtCmdBuffer->CmdSetKernelArguments(firstArg, argCount, values.Data());
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetVertexBuffers(
    const VertexBufferViews& bufferViews)
{
    InsertToken(CmdBufCallId::CmdSetVertexBuffers);
    InsertToken(bufferViews.firstBuffer);
    InsertToken(bufferViews.offsetMode);
    if (bufferViews.offsetMode)
    {
        InsertTokenArray(bufferViews.pVertexBufferViews, bufferViews.bufferCount);
    }
    else
    {
        InsertTokenArray(bufferViews.pBufferViewInfos, bufferViews.bufferCount);
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetVertexBuffers(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    VertexBufferViews bufferViews = {};
    bufferViews.firstBuffer = ReadTokenVal<uint32>();
    bufferViews.offsetMode = ReadTokenVal<bool>();
    if (bufferViews.offsetMode)
    {
        bufferViews.bufferCount = ReadTokenArray(&bufferViews.pVertexBufferViews);
    }
    else
    {
        bufferViews.bufferCount = ReadTokenArray(&bufferViews.pBufferViewInfos);
    }

    pTgtCmdBuffer->CmdSetVertexBuffers(bufferViews);
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

// =====================================================================================================================
uint32 CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdRelease);
    InsertToken(releaseInfo.srcGlobalStageMask);
    InsertToken(releaseInfo.dstGlobalStageMask);
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

// =====================================================================================================================
void CmdBuffer::ReplayCmdRelease(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo releaseInfo;

    releaseInfo.srcGlobalStageMask  = ReadTokenVal<uint32>();
    releaseInfo.dstGlobalStageMask  = ReadTokenVal<uint32>();
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

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdAcquire);
    InsertToken(acquireInfo.srcGlobalStageMask);
    InsertToken(acquireInfo.dstGlobalStageMask);
    InsertToken(acquireInfo.srcGlobalAccessMask);
    InsertToken(acquireInfo.dstGlobalAccessMask);
    InsertTokenArray(acquireInfo.pMemoryBarriers, acquireInfo.memoryBarrierCount);
    InsertTokenArray(acquireInfo.pImageBarriers, acquireInfo.imageBarrierCount);
    InsertToken(acquireInfo.reason);

    InsertTokenArray(pSyncTokens, syncTokenCount);

    HandleBarrierBlt(true, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdAcquire(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    AcquireReleaseInfo acquireInfo;

    acquireInfo.srcGlobalStageMask  = ReadTokenVal<uint32>();
    acquireInfo.dstGlobalStageMask  = ReadTokenVal<uint32>();
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

// =====================================================================================================================
void CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    HandleBarrierBlt(true, true);

    InsertToken(CmdBufCallId::CmdReleaseEvent);
    InsertToken(releaseInfo.srcGlobalStageMask);
    InsertToken(releaseInfo.dstGlobalStageMask);
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
    InsertToken(acquireInfo.srcGlobalStageMask);
    InsertToken(acquireInfo.dstGlobalStageMask);
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

    releaseInfo.srcGlobalStageMask  = ReadTokenVal<uint32>();
    releaseInfo.dstGlobalStageMask  = ReadTokenVal<uint32>();
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

    acquireInfo.srcGlobalStageMask  = ReadTokenVal<uint32>();
    acquireInfo.dstGlobalStageMask  = ReadTokenVal<uint32>();
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
    InsertToken(barrierInfo.srcGlobalStageMask);
    InsertToken(barrierInfo.dstGlobalStageMask);
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

    barrierInfo.srcGlobalStageMask  = ReadTokenVal<uint32>();
    barrierInfo.dstGlobalStageMask  = ReadTokenVal<uint32>();
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
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    InsertToken(CmdBufCallId::CmdWaitMemoryValue);
    InsertToken(gpuVirtAddr);
    InsertToken(data);
    InsertToken(mask);
    InsertToken(compareFunc);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWaitMemoryValue(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto gpuVirtAddr = ReadTokenVal<gpusize>();
    auto data        = ReadTokenVal<uint32>();
    auto mask        = ReadTokenVal<uint32>();
    auto compareFunc = ReadTokenVal<CompareFunc>();
    pTgtCmdBuffer->CmdWaitMemoryValue(gpuVirtAddr, data, mask, compareFunc);
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
        const IPipeline*const pPipeline = m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Graphics)];
        const GraphicsPipelineCreateInfo& pipeInfo = static_cast<const Pipeline*>(pPipeline)->CreateInfo();

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
        if (isDraw)
        {
            VerifyBoundDrawState();
        }

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

        if (isDraw
            )
        {
            if (IsSurfaceCaptureActive(NoBlitCapture))
            {
                cacheFlushInv = true;
            }
        }

        if (cacheFlushInv)
        {
            AddCacheFlushInv();
        }

        if (IsSurfaceCaptureActive(NoBlitCapture))
        {
            CaptureSurfaces(drawDispatchType);
        }

        if ((isDraw
            ) && m_surfaceCapture.pipelineMatch)
        {
            m_surfaceCapture.drawId++;
            if (m_surfaceCapture.pipelineMatch)
            {
                m_surfaceCapture.actionId++;
            }
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
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndirectMulti);
    pThis->InsertToken(gpuVirtAddrAndStride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto gpuVirtAddrAndStride = ReadTokenVal<GpuVirtAddrAndStride>();
    auto maximumCount               = ReadTokenVal<uint32>();
    auto countGpuAddr               = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDrawIndirectMulti(gpuVirtAddrAndStride,
                                        maximumCount,
                                        countGpuAddr);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDrawIndexedIndirectMulti);
    pThis->InsertToken(gpuVirtAddrAndStride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDrawIndexedIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto gpuVirtAddrAndStride = ReadTokenVal<GpuVirtAddrAndStride>();
    auto maximumCount               = ReadTokenVal<uint32>();
    auto countGpuAddr               = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDrawIndexedIndirectMulti(gpuVirtAddrAndStride,
                                               maximumCount,
                                               countGpuAddr);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatch, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatch);
    pThis->InsertToken(size);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatch, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatch(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto size = ReadTokenVal<DispatchDims>();

    pTgtCmdBuffer->CmdDispatch(size);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    gpusize           gpuVirtAddr
)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchIndirect, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchIndirect);
    pThis->InsertToken(gpuVirtAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchIndirect, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchIndirect(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto gpuVirtAddr = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdDispatchIndirect(gpuVirtAddr);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchOffset, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchOffset);
    pThis->InsertToken(offset);
    pThis->InsertToken(launchSize);
    pThis->InsertToken(logicalSize);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchOffset, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchOffset(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto offset      = ReadTokenVal<DispatchDims>();
    const auto launchSize  = ReadTokenVal<DispatchDims>();
    const auto logicalSize = ReadTokenVal<DispatchDims>();

    pTgtCmdBuffer->CmdDispatchOffset(offset, launchSize, logicalSize);
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMesh(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    CmdBuffer* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMesh, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchMesh);
    pThis->InsertToken(size);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMesh, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchMesh(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto size = ReadTokenVal<DispatchDims>();

    pTgtCmdBuffer->CmdDispatchMesh(size);
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMeshIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    CmdBuffer* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti, true);

    pThis->InsertToken(CmdBufCallId::CmdDispatchMeshIndirectMulti);
    pThis->InsertToken(gpuVirtAddrAndStride);
    pThis->InsertToken(maximumCount);
    pThis->InsertToken(countGpuAddr);

    pThis->HandleDrawDispatch(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdDispatchMeshIndirectMulti(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto gpuVirtAddrAndStride = ReadTokenVal<GpuVirtAddrAndStride>();
    uint32  maximumCount      = ReadTokenVal<uint32>();
    gpusize countGpuAddr      = ReadTokenVal<gpusize>();
    pTgtCmdBuffer->CmdDispatchMeshIndirectMulti(gpuVirtAddrAndStride, maximumCount, countGpuAddr);
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
void CmdBuffer::CmdCopyMemoryByGpuVa(
    gpusize                 srcGpuVirtAddr,
    gpusize                 dstGpuVirtAddr,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdCopyMemoryByGpuVa);
    InsertToken(srcGpuVirtAddr);
    InsertToken(dstGpuVirtAddr);
    InsertTokenArray(pRegions, regionCount);

    HandleBarrierBlt(false, false);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyMemoryByGpuVa(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                    srcGpuVirtAddr = ReadTokenVal<gpusize>();
    auto                    dstGpuVirtAddr = ReadTokenVal<gpusize>();
    const MemoryCopyRegion* pRegions       = nullptr;
    auto                    regionCount    = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdCopyMemoryByGpuVa(srcGpuVirtAddr, dstGpuVirtAddr, regionCount, pRegions);
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
void CmdBuffer::CmdScaledCopyTypedBufferToImage(
    const IGpuMemory&                       srcGpuMemory,
    const IImage&                           dstImage,
    ImageLayout                             dstImageLayout,
    uint32                                  regionCount,
    const TypedBufferImageScaledCopyRegion* pRegions)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdScaledCopyTypedBufferToImage);
    InsertToken(&srcGpuMemory);
    InsertToken(&dstImage);
    InsertToken(dstImageLayout);
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
void CmdBuffer::ReplayCmdCopyTypedBufferToImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto                                    pSrcGpuMemory  = ReadTokenVal<IGpuMemory*>();
    auto                                    pDstImage      = ReadTokenVal<IImage*>();
    auto                                    dstImageLayout = ReadTokenVal<ImageLayout>();
    const TypedBufferImageScaledCopyRegion* pRegions       = nullptr;
    auto                                    regionCount    = ReadTokenArray(&pRegions);

    pTgtCmdBuffer->CmdScaledCopyTypedBufferToImage(*pSrcGpuMemory, *pDstImage, dstImageLayout, regionCount, pRegions);
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

    if (IsSurfaceCaptureActive(EnableCmdCopyMemoryToImage))
    {
        SubresId  blitImgBaseSubres;
        blitImgBaseSubres.arraySlice = 0;
        blitImgBaseSubres.mipLevel   = 0;
        blitImgBaseSubres.plane      = 0;

        IImage* pDstImage = nullptr;
        Result result = CaptureImageSurface(&dstImage,
                static_cast<ImageLayoutUsageFlags>(dstImageLayout.usages),
                static_cast<ImageLayoutEngineFlags>(dstImageLayout.engines),
                CoherCopyDst,
                blitImgBaseSubres,
                dstImage.GetImageCreateInfo().arraySize,
                false,
                &pDstImage);

        if (result == Result::Success)
        {
            PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
            const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
            PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

            m_surfaceCapture.pActions[actionIndex].blitOpMask = EnableCmdCopyMemoryToImage;

            PAL_ASSERT(m_surfaceCapture.pActions[actionIndex].pBlitImg == nullptr);
            m_surfaceCapture.pActions[actionIndex].pBlitImg = static_cast<Image*>(pDstImage);

            m_surfaceCapture.actionId++;
        }
        else
        {
            PAL_DPWARN("Failed to capture CmdCopyMemoryToImage, Error:0x%x", result);
        }

        // Warning user if the image has multiple planes or the region has multiple mips
        SubresRange dstImgSubresRange = {};
        dstImage.GetFullSubresourceRange(&dstImgSubresRange);

        if ((dstImgSubresRange.numPlanes > 1) || (dstImgSubresRange.numMips > 1))
        {
            PAL_DPWARN("Dst image in CmdCopyMemoryToImage has multiple planes or mipmaps. \
                        Only capture plane 0, mip 0.");
        }
    }
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

    if (IsSurfaceCaptureActive(EnableCmdCopyImageToMemory))
    {
        SubresId  blitImgBaseSubres;
        blitImgBaseSubres.arraySlice = 0;
        blitImgBaseSubres.mipLevel   = 0;
        blitImgBaseSubres.plane      = 0;

        IImage* pDstImage = nullptr;
        Result result = CaptureImageSurface(&srcImage,
                static_cast<ImageLayoutUsageFlags>(srcImageLayout.usages),
                static_cast<ImageLayoutEngineFlags>(srcImageLayout.engines),
                CoherCopyDst,
                blitImgBaseSubres,
                srcImage.GetImageCreateInfo().arraySize,
                false,
                &pDstImage);

        if (result == Result::Success)
        {
            PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
            const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
            PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

            m_surfaceCapture.pActions[actionIndex].blitOpMask = EnableCmdCopyImageToMemory;

            PAL_ASSERT(m_surfaceCapture.pActions[actionIndex].pBlitImg == nullptr);
            m_surfaceCapture.pActions[actionIndex].pBlitImg = static_cast<Image*>(pDstImage);

            m_surfaceCapture.actionId++;
        }
        else
        {
            PAL_DPWARN("Failed to capture CmdCopyMemoryToImage, Error:0x%x", result);
        }

        // Warning user if the image has multiple planes or the region has multiple mips
        SubresRange srcImgSubresRange = {};
        srcImage.GetFullSubresourceRange(&srcImgSubresRange);

        if ((srcImgSubresRange.numPlanes > 1) || (srcImgSubresRange.numMips > 1))
        {
            PAL_DPWARN("Src image in CmdCopyImageToMemory has multiple planes or mipmaps. \
                        Only capture plane 0, mip 0.");
        }
    }
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
    const IImage&         image,
    ImageLayout           imageLayout,
    const ClearColor&     color,
    const SwizzledFormat& clearFormat,
    uint32                rangeCount,
    const SubresRange*    pRanges,
    uint32                boxCount,
    const Box*            pBoxes,
    uint32                flags)
{
    HandleBarrierBlt(false, true);

    InsertToken(CmdBufCallId::CmdClearColorImage);
    InsertToken(&image);
    InsertToken(imageLayout);
    InsertToken(color);
    InsertToken(clearFormat);
    InsertTokenArray(pRanges, rangeCount);
    InsertTokenArray(pBoxes, boxCount);
    InsertToken(flags);

    HandleBarrierBlt(false, false);

    if (IsSurfaceCaptureActive(EnableCmdClearColorImage))
    {
        SubresId  blitImgBaseSubres;
        blitImgBaseSubres.arraySlice = 0;
        blitImgBaseSubres.mipLevel   = 0;
        blitImgBaseSubres.plane      = 0;

        IImage* pDstImage = nullptr;
        Result result = CaptureImageSurface(&image,
            static_cast<ImageLayoutUsageFlags>(imageLayout.usages),
            static_cast<ImageLayoutEngineFlags>(imageLayout.engines),
            CoherColorTarget,
            blitImgBaseSubres,
            image.GetImageCreateInfo().arraySize,
            false,
            &pDstImage);

        if (result == Result::Success)
        {
            PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
            const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
            PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

            m_surfaceCapture.pActions[actionIndex].blitOpMask = EnableCmdClearColorImage;

            PAL_ASSERT(m_surfaceCapture.pActions[actionIndex].pBlitImg == nullptr);
            m_surfaceCapture.pActions[actionIndex].pBlitImg = static_cast<Image*>(pDstImage);

            m_surfaceCapture.actionId++;
        }
        else
        {
            PAL_DPWARN("Failed to capture CmdClearColorImage, Error:0x%x", result);
        }

        // Warning user if the image has multiple planes or the region has multiple mips
        SubresRange imgSubresRange = {};
        image.GetFullSubresourceRange(&imgSubresRange);

        if ((imgSubresRange.numPlanes > 1) || (imgSubresRange.numMips > 1))
        {
            PAL_DPWARN("Image in CmdClearColorImage has multiple planes or mipmaps. \
                        Only capture plane 0, mip 0.");
        }
    }
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdClearColorImage(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto               pImage      = ReadTokenVal<IImage*>();
    auto               imageLayout = ReadTokenVal<ImageLayout>();
    auto               color       = ReadTokenVal<ClearColor>();
    auto               clearFormat = ReadTokenVal<SwizzledFormat>();
    const SubresRange* pRanges     = nullptr;
    auto               rangeCount  = ReadTokenArray(&pRanges);
    const Box*         pBoxes      = nullptr;
    auto               boxCount    = ReadTokenArray(&pBoxes);
    auto               flags       = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdClearColorImage(*pImage,
                                      imageLayout,
                                      color,
                                      clearFormat,
                                      rangeCount,
                                      pRanges,
                                      boxCount,
                                      pBoxes,
                                      flags);
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

    if (IsSurfaceCaptureActive(EnableCmdClearDepthStencil))
    {
        SubresId  blitImgBaseSubres;
        blitImgBaseSubres.arraySlice = 0;
        blitImgBaseSubres.mipLevel   = 0;
        blitImgBaseSubres.plane      = 0;

        IImage* pDstImage = nullptr;
        Result result = CaptureImageSurface(&image,
            static_cast<ImageLayoutUsageFlags>(depthLayout.usages),
            static_cast<ImageLayoutEngineFlags>(depthLayout.engines),
            CoherClear,
            blitImgBaseSubres,
            image.GetImageCreateInfo().arraySize,
            false,
            &pDstImage);

        if (result == Result::Success)
        {
            PAL_ASSERT(m_surfaceCapture.actionId >= m_surfaceCapture.actionIdStart);
            const uint32 actionIndex = m_surfaceCapture.actionId - m_surfaceCapture.actionIdStart;
            PAL_ASSERT(actionIndex < m_surfaceCapture.actionIdCount);

            m_surfaceCapture.pActions[actionIndex].blitOpMask = EnableCmdClearDepthStencil;

            PAL_ASSERT(m_surfaceCapture.pActions[actionIndex].pBlitImg == nullptr);
            m_surfaceCapture.pActions[actionIndex].pBlitImg = static_cast<Image*>(pDstImage);

            m_surfaceCapture.actionId++;
        }
        else
        {
            PAL_DPWARN("Failed to capture CmdClearDepthStencil, Error:0x%x", result);
        }

        // Warning user if the image has multiple planes or the region has multiple mips
        SubresRange imgSubresRange = {};
        image.GetFullSubresourceRange(&imgSubresRange);

        if ((imgSubresRange.numPlanes > 1) || (imgSubresRange.numMips > 1))
        {
            PAL_DPWARN("Image in CmdClearDepthStencil has multiple planes or mipmaps. \
                        Only capture plane 0, mip 0.");
        }
    }
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
    uint32           stageMask)
{
    InsertToken(CmdBufCallId::CmdSetEvent);
    InsertToken(&gpuEvent);
    InsertToken(stageMask);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdSetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent = ReadTokenVal<IGpuEvent*>();
    auto stageMask = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdSetEvent(*pGpuEvent, stageMask);
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    uint32           stageMask)
{
    InsertToken(CmdBufCallId::CmdResetEvent);
    InsertToken(&gpuEvent);
    InsertToken(stageMask);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdResetEvent(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto pGpuEvent = ReadTokenVal<IGpuEvent*>();
    auto stageMask = ReadTokenVal<uint32>();

    pTgtCmdBuffer->CmdResetEvent(*pGpuEvent, stageMask);
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
    uint32            stageMask,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    InsertToken(CmdBufCallId::CmdWriteTimestamp);
    InsertToken(stageMask);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteTimestamp(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto stageMask     = ReadTokenVal<uint32>();
    auto pDstGpuMemory = ReadTokenVal<IGpuMemory*>();
    auto dstOffset     = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteTimestamp(stageMask, *pDstGpuMemory, dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    uint32             stageMask,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    InsertToken(CmdBufCallId::CmdWriteImmediate);
    InsertToken(stageMask);
    InsertToken(data);
    InsertToken(dataSize);
    InsertToken(address);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdWriteImmediate(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    auto stageMask = ReadTokenVal<uint32>();
    auto data      = ReadTokenVal<uint64>();
    auto dataSize  = ReadTokenVal<ImmediateDataWidth>();
    auto address   = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdWriteImmediate(stageMask, data, dataSize, address);
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
uint32 CmdBuffer::GetLargeEmbeddedDataLimit() const
{
    return GetNextLayer()->GetLargeEmbeddedDataLimit();
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
uint32* CmdBuffer::CmdAllocateLargeEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    return GetNextLayer()->CmdAllocateLargeEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
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
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    InsertToken(CmdBufCallId::CmdExecuteIndirectCmds);
    InsertToken(&generator);
    InsertToken(gpuVirtAddr);
    InsertToken(maximumCount);
    InsertToken(countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdExecuteIndirectCmds(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const auto*const pGenerator   = ReadTokenVal<const IIndirectCmdGenerator*>();
    const gpusize    gpuVirtAddr  = ReadTokenVal<gpusize>();
    const uint32     maximumCount = ReadTokenVal<uint32>();
    const gpusize    countGpuAddr = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdExecuteIndirectCmds(*pGenerator,
                                          gpuVirtAddr,
                                          maximumCount,
                                          countGpuAddr);
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
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    InsertToken(CmdBufCallId::CmdInsertRgpTraceMarker);
    InsertToken(subQueueFlags);
    InsertTokenArray(static_cast<const uint32*>(pData), numDwords);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertRgpTraceMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const RgpMarkerSubQueueFlags subQueueFlags = ReadTokenVal<RgpMarkerSubQueueFlags>();
    const uint32* pData = nullptr;
    uint32 numDwords = ReadTokenArray(&pData);

    pTgtCmdBuffer->CmdInsertRgpTraceMarker(subQueueFlags, numDwords, pData);
}

// =====================================================================================================================
uint32 CmdBuffer::CmdInsertExecutionMarker(
    bool        isBegin,
    uint8       sourceId,
    const char* pMarkerName,
    uint32      markerNameSize)
{
    InsertToken(CmdBufCallId::CmdInsertExecutionMarker);
    InsertToken(isBegin);
    InsertToken(sourceId);
    InsertTokenArray(pMarkerName, markerNameSize);
    return 0;
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdInsertExecutionMarker(
    Queue*           pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    bool  isBegin  = ReadTokenVal<bool>();
    uint8 sourceId = ReadTokenVal<uint8>();

    const char* pMarkerName = nullptr;
    uint32 markerNameSize   = ReadTokenArray(&pMarkerName);

    const uint32 marker = pTgtCmdBuffer->CmdInsertExecutionMarker(isBegin,
                                                                  sourceId,
                                                                  pMarkerName,
                                                                  markerNameSize);
    PAL_ASSERT_MSG(marker == 0, "Crash Analysis layer is unexpectedly enabled");
}

// =====================================================================================================================
void CmdBuffer::CmdCopyDfSpmTraceData(
    const IPerfExperiment& perfExperiment,
    const IGpuMemory&      dstGpuMemory,
    gpusize                dstOffset)
{
    InsertToken(CmdBufCallId::CmdCopyDfSpmTraceData);
    InsertToken(&perfExperiment);
    InsertToken(&dstGpuMemory);
    InsertToken(dstOffset);
}

// =====================================================================================================================
void CmdBuffer::ReplayCmdCopyDfSpmTraceData(
    Queue* pQueue,
    TargetCmdBuffer* pTgtCmdBuffer)
{
    const IPerfExperiment& perfExperiment  = *ReadTokenVal<IPerfExperiment*>();
    const IGpuMemory&      dstGpuMemory    = *ReadTokenVal<IGpuMemory*>();
    gpusize                dstOffset       = ReadTokenVal<gpusize>();

    pTgtCmdBuffer->CmdCopyDfSpmTraceData(perfExperiment, dstGpuMemory, dstOffset);
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
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    InsertToken(CmdBufCallId::CmdPostProcessFrame);
    InsertToken(postProcessInfo);
    InsertToken((pAddedGpuWork != nullptr) ? *pAddedGpuWork : false);

    // Pass this command on to the next layer.  Clients depend on the pAddedGpuWork output parameter.
    CmdPostProcessFrameInfo nextPostProcessInfo = postProcessInfo;
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
        &CmdBuffer::ReplayCmdSaveGraphicsState,
        &CmdBuffer::ReplayCmdRestoreGraphicsState,
        &CmdBuffer::ReplayCmdBindColorBlendState,
        &CmdBuffer::ReplayCmdBindDepthStencilState,
        &CmdBuffer::ReplayCmdBindIndexData,
        &CmdBuffer::ReplayCmdBindTargets,
        &CmdBuffer::ReplayCmdBindStreamOutTargets,
        &CmdBuffer::ReplayCmdBindBorderColorPalette,
        &CmdBuffer::ReplayCmdSetUserData,
        &CmdBuffer::ReplayCmdDuplicateUserData,
        &CmdBuffer::ReplayCmdSetKernelArguments,
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
        &CmdBuffer::ReplayCmdBarrier,
        &CmdBuffer::ReplayCmdRelease,
        &CmdBuffer::ReplayCmdAcquire,
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
        &CmdBuffer::ReplayCmdCopyMemoryByGpuVa,
        &CmdBuffer::ReplayCmdCopyTypedBuffer,
        &CmdBuffer::ReplayCmdCopyTypedBufferToImage,
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
        &CmdBuffer::ReplayCmdBeginPerfExperiment,
        &CmdBuffer::ReplayCmdUpdatePerfExperimentSqttTokenMask,
        &CmdBuffer::ReplayCmdUpdateSqttTokenMask,
        &CmdBuffer::ReplayCmdEndPerfExperiment,
        &CmdBuffer::ReplayCmdInsertTraceMarker,
        &CmdBuffer::ReplayCmdInsertRgpTraceMarker,
        &CmdBuffer::ReplayCmdInsertExecutionMarker,
        &CmdBuffer::ReplayCmdCopyDfSpmTraceData,
        &CmdBuffer::ReplayCmdSaveComputeState,
        &CmdBuffer::ReplayCmdRestoreComputeState,
        &CmdBuffer::ReplayCmdSetUserClipPlanes,
        &CmdBuffer::ReplayCmdCommentString,
        &CmdBuffer::ReplayCmdNop,
        &CmdBuffer::ReplayCmdXdmaWaitFlipPending,
        &CmdBuffer::ReplayCmdCopyMemoryToTiledImage,
        &CmdBuffer::ReplayCmdCopyTiledImageToMemory,
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
    m_allocator(2_MiB),
#else
    m_allocator(8_MiB),
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
