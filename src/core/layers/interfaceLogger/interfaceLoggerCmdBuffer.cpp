/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/interfaceLogger/interfaceLoggerCmdAllocator.h"
#include "core/layers/interfaceLogger/interfaceLoggerCmdBuffer.h"
#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuEvent.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerImage.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "palAutoBuffer.h"
#include "palHsaAbiMetadata.h"
#include "palStringUtil.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*   pNextCmdBuffer,
    const Device* pDevice,
    uint32        objectId)
    :
    CmdBufferDecorator(pNextCmdBuffer, pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_objectId(objectId),
    m_pBoundPipelines{}
{
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]   = CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)]  = CmdSetUserDataGfx;

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
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferBegin);
    const Result result = m_pNextLayer->Begin(NextCmdBufferBuildInfo(info));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("info", info);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        pLogContext->KeyAndValue("frame", m_pPlatform->FrameCount());

        m_pPlatform->LogEndFunc(pLogContext);
    }

    // Reset our internal state tracking.
    for (uint32 idx = 0; idx < static_cast<uint32>(PipelineBindPoint::Count); ++idx)
    {
        m_pBoundPipelines[idx] = nullptr;
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferEnd);
    const Result result = m_pNextLayer->End();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    const bool   active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferReset);
    const Result result = m_pNextLayer->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("pCmdAllocator", pCmdAllocator);
        pLogContext->KeyAndValue("returnGpuMemory", returnGpuMemory);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return result;
}

// =====================================================================================================================
uint32 CmdBuffer::GetEmbeddedDataLimit() const
{
    // This function is not logged because it doesn't modify the command buffer.
    return m_pNextLayer->GetEmbeddedDataLimit();
}

// =====================================================================================================================
uint32 CmdBuffer::GetLargeEmbeddedDataLimit() const
{
    // This function is not logged because it doesn't modify the command buffer.
    return m_pNextLayer->GetLargeEmbeddedDataLimit();
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindPipeline);

    m_pNextLayer->CmdBindPipeline(NextPipelineBindParams(params));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    // We may need this pipeline in a later function call.
    m_pBoundPipelines[static_cast<uint32>(params.pipelineBindPoint)] = params.pPipeline;
}

// =====================================================================================================================
void CmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindMsaaState);

    m_pNextLayer->CmdBindMsaaState(NextMsaaState(pMsaaState));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("msaaState", pMsaaState);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSaveGraphicsState()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSaveGraphicsState);

    m_pNextLayer->CmdSaveGraphicsState();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreGraphicsState()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdRestoreGraphicsState);

    m_pNextLayer->CmdRestoreGraphicsState();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdPrimeGpuCaches);

    m_pNextLayer->CmdPrimeGpuCaches(rangeCount, pRanges);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("primeGpuCacheRange", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();

        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindColorBlendState);

    m_pNextLayer->CmdBindColorBlendState(NextColorBlendState(pColorBlendState));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("colorBlendState", pColorBlendState);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindDepthStencilState);

    m_pNextLayer->CmdBindDepthStencilState(NextDepthStencilState(pDepthStencilState));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("depthStencilState", pDepthStencilState);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetDepthBounds);

    m_pNextLayer->CmdSetDepthBounds(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdDuplicateUserData(
    PipelineBindPoint source,
    PipelineBindPoint dest)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdDuplicateUserData);

    m_pNextLayer->CmdDuplicateUserData(source, dest);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("source", source);
        pLogContext->KeyAndEnum("dest",   dest);
        pLogContext->EndInput();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetKernelArguments(
    uint32            firstArg,
    uint32            argCount,
    const void*const* ppValues)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetKernelArguments);

    m_pNextLayer->CmdSetKernelArguments(firstArg, argCount, ppValues);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstArg", firstArg);
        pLogContext->KeyAndValue("argCount", argCount);
        pLogContext->KeyAndBeginList("values", false);

        // There must be an HSA ABI pipeline bound if you call this function.
        const IPipeline*const pPipeline = m_pBoundPipelines[static_cast<uint32>(PipelineBindPoint::Compute)];
        PAL_ASSERT((pPipeline != nullptr) && (pPipeline->GetInfo().flags.hsaAbi == 1));

        for (uint32 idx = 0; idx < argCount; ++idx)
        {
            const auto*  pArgument = pPipeline->GetKernelArgument(firstArg + idx);
            PAL_ASSERT(pArgument != nullptr);

            const size_t valueSize = pArgument->size;
            PAL_ASSERT(valueSize > 0);

            // Convert the value to one long string of hexadecimal values. If the value size matches a fundemental
            // type use that block size, otherwise default to DWORDs.
            const size_t blockSize = (valueSize == 1) ? 1 : (valueSize == 2) ? 2 : (valueSize == 8) ? 8 : 4;
            const size_t blockLen  = 3 + blockSize * 2; // "0x" + 2 chars per byte + a null or space.
            const size_t numBlocks = RoundUpQuotient<size_t>(valueSize, blockSize);

            AutoBuffer<char, 256, Platform> string(numBlocks * blockLen, m_pPlatform);
            BytesToStr(string.Data(), string.Capacity(), ppValues[idx], valueSize, blockSize);

            pLogContext->Value(string.Data());
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetVertexBuffers(
    const VertexBufferViews& bufferViews)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetVertexBuffers);

    m_pNextLayer->CmdSetVertexBuffers(bufferViews);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("bufferViews", bufferViews);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindIndexData);

    m_pNextLayer->CmdBindIndexData(gpuAddr, indexCount, indexType);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("gpuAddr", gpuAddr);
        pLogContext->KeyAndValue("indexCount", indexCount);
        pLogContext->KeyAndEnum("indexType", indexType);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    BindTargetParams nextParams = params;

    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        nextParams.colorTargets[i].pColorTargetView = NextColorTargetView(params.colorTargets[i].pColorTargetView);
    }

    nextParams.depthTarget.pDepthStencilView = NextDepthStencilView(params.depthTarget.pDepthStencilView);

    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindTargets);

    m_pNextLayer->CmdBindTargets(nextParams);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindStreamOutTargets);

    m_pNextLayer->CmdBindStreamOutTargets(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams&  rateParams)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetPerDrawVrsRate);

    m_pNextLayer->CmdSetPerDrawVrsRate(rateParams);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("rateParams", rateParams);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState&  centerState)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetVrsCenterState);

    m_pNextLayer->CmdSetVrsCenterState(centerState);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("centerState", centerState);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindSampleRateImage(
    const IImage*  pImage)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindSampleRateImage);

    m_pNextLayer->CmdBindSampleRateImage(NextImage(pImage));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", pImage);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdResolvePrtPlusImage);

    m_pNextLayer->CmdResolvePrtPlusImage(*NextImage(&srcImage),
                                         srcImageLayout,
                                         *NextImage(&dstImage),
                                         dstImageLayout,
                                         resolveType,
                                         regionCount,
                                         pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndEnum("resolveType", resolveType);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetBlendConst);

    m_pNextLayer->CmdSetBlendConst(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetInputAssemblyState);

    m_pNextLayer->CmdSetInputAssemblyState(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetTriangleRasterState);

    m_pNextLayer->CmdSetTriangleRasterState(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetPointLineRasterState);

    m_pNextLayer->CmdSetPointLineRasterState(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetLineStippleState(
    const LineStippleStateParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetLineStippleState);

    m_pNextLayer->CmdSetLineStippleState(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetDepthBiasState);

    m_pNextLayer->CmdSetDepthBiasState(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetStencilRefMasks);

    m_pNextLayer->CmdSetStencilRefMasks(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetUserClipPlanes);

    m_pNextLayer->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstPlane", firstPlane);
        pLogContext->KeyAndBeginList("planes", false);

        for (uint32 idx = 0; idx < planeCount; ++idx)
        {
            pLogContext->Struct(pPlanes[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetClipRects);

    m_pNextLayer->CmdSetClipRects(clipRule, rectCount, pRectList);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("clipRule", clipRule);
        pLogContext->KeyAndValue("rectCount", rectCount);
        pLogContext->KeyAndBeginList("Rectangles", false);

        for (uint32 idx = 0; idx < rectCount; ++idx)
        {
            pLogContext->Struct(pRectList[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetMsaaQuadSamplePattern);

    m_pNextLayer->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("numSamplesPerPixel", numSamplesPerPixel);
        pLogContext->KeyAndStruct("quadSamplePattern", quadSamplePattern);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetViewports);

    m_pNextLayer->CmdSetViewports(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetScissorRects);

    m_pNextLayer->CmdSetScissorRects(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetGlobalScissor);

    m_pNextLayer->CmdSetGlobalScissor(params);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    AutoBuffer<const IGpuEvent*,  16, Platform> gpuEvents(barrierInfo.gpuEventWaitCount, m_pPlatform);
    AutoBuffer<const IImage*,     16, Platform> targets(barrierInfo.rangeCheckedTargetWaitCount, m_pPlatform);
    AutoBuffer<BarrierTransition, 32, Platform> transitions(barrierInfo.transitionCount, m_pPlatform);

    if ((gpuEvents.Capacity()   < barrierInfo.gpuEventWaitCount)           ||
        (targets.Capacity()     < barrierInfo.rangeCheckedTargetWaitCount) ||
        (transitions.Capacity() < barrierInfo.transitionCount))
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        BarrierInfo nextBarrierInfo = barrierInfo;

        for (uint32 i = 0; i < barrierInfo.gpuEventWaitCount; i++)
        {
            gpuEvents[i] = NextGpuEvent(barrierInfo.ppGpuEvents[i]);
        }
        nextBarrierInfo.ppGpuEvents = &gpuEvents[0];

        for (uint32 i = 0; i < barrierInfo.rangeCheckedTargetWaitCount; i++)
        {
            targets[i] = NextImage(barrierInfo.ppTargets[i]);
        }
        nextBarrierInfo.ppTargets = &targets[0];

        for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
        {
            transitions[i]                  = barrierInfo.pTransitions[i];
            transitions[i].imageInfo.pImage = NextImage(barrierInfo.pTransitions[i].imageInfo.pImage);
        }
        nextBarrierInfo.pTransitions = &transitions[0];

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBarrier);

        m_pNextLayer->CmdBarrier(nextBarrierInfo);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("barrierInfo", barrierInfo);
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
uint32 CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo)
{
    AutoBuffer<ImgBarrier, 32, Platform> imageBarriers(releaseInfo.imageBarrierCount, m_pPlatform);

    uint32 syncToken = 0;

    if (imageBarriers.Capacity() < releaseInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextReleaseInfo = releaseInfo;

        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = releaseInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(releaseInfo.pImageBarriers[i].pImage);
        }
        nextReleaseInfo.pImageBarriers = &imageBarriers[0];

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdRelease);

        syncToken = m_pNextLayer->CmdRelease(nextReleaseInfo);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("releaseInfo", releaseInfo);
            pLogContext->KeyAndValue("syncToken", syncToken);
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }

    return syncToken;
}

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
    const uint32*             pSyncTokens)
{
    AutoBuffer<ImgBarrier, 32, Platform> imageBarriers(acquireInfo.imageBarrierCount, m_pPlatform);

    if (imageBarriers.Capacity() < acquireInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextAcquireInfo = acquireInfo;

        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = acquireInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(acquireInfo.pImageBarriers[i].pImage);
        }
        nextAcquireInfo.pImageBarriers = &imageBarriers[0];

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdAcquire);

        m_pNextLayer->CmdAcquire(nextAcquireInfo, syncTokenCount, pSyncTokens);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("acquireInfo", acquireInfo);
            pLogContext->KeyAndBeginList("SyncTokens", false);

            for (uint32 idx = 0; idx < syncTokenCount; ++idx)
            {
                pLogContext->Value(pSyncTokens[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseEvent(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    AutoBuffer<ImgBarrier, 32, Platform> imageBarriers(releaseInfo.imageBarrierCount, m_pPlatform);

    if (imageBarriers.Capacity() < releaseInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextReleaseInfo = releaseInfo;

        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = releaseInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(releaseInfo.pImageBarriers[i].pImage);
        }
        nextReleaseInfo.pImageBarriers = &imageBarriers[0];

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdReleaseEvent);

        m_pNextLayer->CmdReleaseEvent(nextReleaseInfo, NextGpuEvent(pGpuEvent));

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("releaseInfo", releaseInfo);
            pLogContext->KeyAndObject("gpuEvent", pGpuEvent);
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    AutoBuffer<ImgBarrier, 32, Platform> imageBarriers(acquireInfo.imageBarrierCount, m_pPlatform);
    AutoBuffer<IGpuEvent*, 16, Platform> nextGpuEvents(gpuEventCount, m_pPlatform);

    if ((imageBarriers.Capacity() < acquireInfo.imageBarrierCount)   ||
        (nextGpuEvents.Capacity() < gpuEventCount))
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextAcquireInfo = acquireInfo;

        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = acquireInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(acquireInfo.pImageBarriers[i].pImage);
        }
        nextAcquireInfo.pImageBarriers = &imageBarriers[0];

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
            nextGpuEvents[i] = NextGpuEvent(ppGpuEvents[i]);
        }

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdAcquireEvent);

        m_pNextLayer->CmdAcquireEvent(nextAcquireInfo, gpuEventCount, &nextGpuEvents[0]);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("acquireInfo", acquireInfo);
            pLogContext->KeyAndBeginList("gpuEvents", false);

            for (uint32 idx = 0; idx < gpuEventCount; ++idx)
            {
                pLogContext->Object(nextGpuEvents[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    AutoBuffer<ImgBarrier, 32, Platform> imageBarriers(barrierInfo.imageBarrierCount, m_pPlatform);

    if (imageBarriers.Capacity() < barrierInfo.imageBarrierCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        AcquireReleaseInfo nextBarrierInfo = barrierInfo;

        for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
        {
            imageBarriers[i]        = barrierInfo.pImageBarriers[i];
            imageBarriers[i].pImage = NextImage(barrierInfo.pImageBarriers[i].pImage);
        }
        nextBarrierInfo.pImageBarriers = &imageBarriers[0];

        const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdReleaseThenAcquire);

        m_pNextLayer->CmdReleaseThenAcquire(nextBarrierInfo);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("barrierInfo", barrierInfo);
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyMemory);

    m_pNextLayer->CmdCopyMemory(*NextGpuMemory(&srcGpuMemory), *NextGpuMemory(&dstGpuMemory), regionCount, pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        // Suppress unnecessary info for barrier log only mode to reduce json file size.
        if (m_pPlatform->IsBarrierLogActive() == false)
        {
            pLogContext->BeginInput();
            pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
            pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
            pLogContext->KeyAndBeginList("regions", false);

            for (uint32 idx = 0; idx < regionCount; ++idx)
            {
                pLogContext->Struct(pRegions[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndInput();
        }

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryByGpuVa(
    gpusize                 srcGpuVirtAddr,
    gpusize                 dstGpuVirtAddr,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyMemoryByGpuVa);

    m_pNextLayer->CmdCopyMemoryByGpuVa(srcGpuVirtAddr, dstGpuVirtAddr, regionCount, pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("srcGpuVirtAddr", srcGpuVirtAddr);
        pLogContext->KeyAndValue("dstGpuVirtAddr", dstGpuVirtAddr);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyImage);

    m_pNextLayer->CmdCopyImage(*NextImage(&srcImage),
                               srcImageLayout,
                               *NextImage(&dstImage),
                               dstImageLayout,
                               regionCount,
                               pRegions,
                               pScissorRect,
                               flags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();

        if (pScissorRect != nullptr)
        {
            pLogContext->KeyAndStruct("scissorRect", *pScissorRect);
        }

        pLogContext->KeyAndCopyControlFlags("flags", flags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyMemoryToImage);

    m_pNextLayer->CmdCopyMemoryToImage(*NextGpuMemory(&srcGpuMemory),
                                       *NextImage(&dstImage),
                                       dstImageLayout,
                                       regionCount,
                                       pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        // Suppress unnecessary info for barrier log only mode to reduce json file size.
        if (m_pPlatform->IsBarrierLogActive() == false)
        {
            pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
            pLogContext->KeyAndBeginList("regions", false);

            for (uint32 idx = 0; idx < regionCount; ++idx)
            {
                pLogContext->Struct(pRegions[idx]);
            }

            pLogContext->EndList();
        }
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyImageToMemory);

    m_pNextLayer->CmdCopyImageToMemory(*NextImage(&srcImage),
                                       srcImageLayout,
                                       *NextGpuMemory(&dstGpuMemory),
                                       regionCount,
                                       pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyMemoryToTiledImage);

    m_pNextLayer->CmdCopyMemoryToTiledImage(*NextGpuMemory(&srcGpuMemory),
                                            *NextImage(&dstImage),
                                            dstImageLayout,
                                            regionCount,
                                            pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyTiledImageToMemory);

    m_pNextLayer->CmdCopyTiledImageToMemory(*NextImage(&srcImage),
                                            srcImageLayout,
                                            *NextGpuMemory(&dstGpuMemory),
                                            regionCount,
                                            pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyTypedBuffer);

    m_pNextLayer->CmdCopyTypedBuffer(*NextGpuMemory(&srcGpuMemory),
                                     *NextGpuMemory(&dstGpuMemory),
                                     regionCount,
                                     pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyTypedBufferToImage(
    const IGpuMemory&                       srcGpuMemory,
    const IImage&                           dstImage,
    ImageLayout                             dstImageLayout,
    uint32                                  regionCount,
    const TypedBufferImageScaledCopyRegion* pRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdScaledCopyTypedBufferToImage);

    m_pNextLayer->CmdScaledCopyTypedBufferToImage(*NextGpuMemory(&srcGpuMemory),
                                                  *NextImage(&dstImage),
                                                  dstImageLayout,
                                                  regionCount,
                                                  pRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCopyRegisterToMemory);

    m_pNextLayer->CmdCopyRegisterToMemory(srcRegisterOffset, *NextGpuMemory(&dstGpuMemory), dstOffset);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("srcRegisterOffset", srcRegisterOffset);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo& copyInfo)
{
    ScaledCopyInfo nextCopyInfo = copyInfo;
    nextCopyInfo.pSrcImage = NextImage(copyInfo.pSrcImage);
    nextCopyInfo.pDstImage = NextImage(copyInfo.pDstImage);

    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdScaledCopyImage);

    m_pNextLayer->CmdScaledCopyImage(nextCopyInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("copyInfo", copyInfo);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdGenerateMipmaps(
    const GenMipmapsInfo& genInfo)
{
    GenMipmapsInfo nextGenInfo = genInfo;
    nextGenInfo.pImage         = NextImage(genInfo.pImage);

    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdGenerateMipmaps);

    m_pNextLayer->CmdGenerateMipmaps(nextGenInfo);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("genInfo", genInfo);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdColorSpaceConversionCopy);

    m_pNextLayer->CmdColorSpaceConversionCopy(*NextImage(&srcImage),
                                              srcImageLayout,
                                              *NextImage(&dstImage),
                                              dstImageLayout,
                                              regionCount,
                                              pRegions,
                                              filter,
                                              cscTable);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndStruct("filter", filter);
        pLogContext->KeyAndStruct("cscTable", cscTable);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCloneImageData);

    m_pNextLayer->CmdCloneImageData(*NextImage(&srcImage), *NextImage(&dstImage));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdUpdateMemory);

    m_pNextLayer->CmdUpdateMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, dataSize, pData);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->KeyAndBeginList("data", false);

        for (gpusize idx = 0; idx < NumBytesToNumDwords(static_cast<uint32>(dataSize)); ++idx)
        {
            pLogContext->Value(pData[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdUpdateBusAddressableMemoryMarker);

    m_pNextLayer->CmdUpdateBusAddressableMemoryMarker(*NextGpuMemory(&dstGpuMemory), offset, value);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("value", value);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdFillMemory);

    m_pNextLayer->CmdFillMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, fillSize, data);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->KeyAndValue("fillSize", fillSize);
        pLogContext->KeyAndValue("data", data);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearColorBuffer);

    m_pNextLayer->CmdClearColorBuffer(*NextGpuMemory(&gpuMemory),
                                      color,
                                      bufferFormat,
                                      bufferOffset,
                                      bufferExtent,
                                      rangeCount,
                                      pRanges);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndStruct("color", color);
        pLogContext->KeyAndStruct("bufferFormat", bufferFormat);
        pLogContext->KeyAndValue("bufferOffset", bufferOffset);
        pLogContext->KeyAndValue("bufferExtent", bufferExtent);
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundColorTargets(
    uint32                        colorTargetCount,
    const BoundColorTarget*       pBoundColorTargets,
    uint32                        regionCount,
    const ClearBoundTargetRegion* pClearRegions)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearBoundColorTargets);

    m_pNextLayer->CmdClearBoundColorTargets(colorTargetCount, pBoundColorTargets, regionCount, pClearRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("boundColorTargets", false);

        for (uint32 idx = 0; idx < colorTargetCount; ++idx)
        {
            pLogContext->Struct(pBoundColorTargets[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pClearRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearColorImage);

    m_pNextLayer->CmdClearColorImage(*NextImage(&image),
                                     imageLayout,
                                     color,
                                     clearFormat,
                                     rangeCount,
                                     pRanges,
                                     boxCount,
                                     pBoxes,
                                     flags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", &image);
        pLogContext->KeyAndStruct("imageLayout", imageLayout);
        pLogContext->KeyAndStruct("color", color);
        pLogContext->KeyAndStruct("clearFormat", clearFormat);
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndBeginList("boxes", false);

        for (uint32 idx = 0; idx < boxCount; ++idx)
        {
            pLogContext->Struct(pBoxes[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndClearColorImageFlags("flags", flags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId,
                            InterfaceFunc::CmdBufferCmdClearBoundDepthStencilTargets);

    m_pNextLayer->CmdClearBoundDepthStencilTargets(depth,
                                                   stencil,
                                                   stencilWriteMask,
                                                   samples,
                                                   fragments,
                                                   flag,
                                                   regionCount,
                                                   pClearRegions);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("depth", depth);
        pLogContext->KeyAndValue("stencil", stencil);
        pLogContext->KeyAndValue("stencilWriteMask", stencilWriteMask);
        pLogContext->KeyAndValue("samples", samples);
        pLogContext->KeyAndValue("fragments", fragments);
        pLogContext->KeyAndStruct("flags", flag);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pClearRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearDepthStencil);

    m_pNextLayer->CmdClearDepthStencil(*NextImage(&image),
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

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", &image);
        pLogContext->KeyAndStruct("depthLayout", depthLayout);
        pLogContext->KeyAndStruct("stencilLayout", stencilLayout);
        pLogContext->KeyAndValue("depth", depth);
        pLogContext->KeyAndValue("stencil", stencil);
        pLogContext->KeyAndValue("stencilWriteMask", stencilWriteMask);
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndBeginList("rects", false);

        for (uint32 idx = 0; idx < rectCount; ++idx)
        {
            pLogContext->Struct(pRects[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndClearDepthStencilFlags("flags", flags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearBufferView);

    m_pNextLayer->CmdClearBufferView(*NextGpuMemory(&gpuMemory), color, pBufferViewSrd, rangeCount, pRanges);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndStruct("color", color);
        pLogContext->KeyAndBeginList("ranges", false);

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            pLogContext->Struct(pRanges[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdClearImageView);

    m_pNextLayer->CmdClearImageView(*NextImage(&image), imageLayout, color, pImageViewSrd, rectCount, pRects);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", &image);
        pLogContext->KeyAndStruct("imageLayout", imageLayout);
        pLogContext->KeyAndStruct("color", color);
        pLogContext->KeyAndBeginList("rects", false);

        for (uint32 idx = 0; idx < rectCount; ++idx)
        {
            pLogContext->Struct(pRects[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdResolveImage);

    m_pNextLayer->CmdResolveImage(*NextImage(&srcImage),
                                  srcImageLayout,
                                  *NextImage(&dstImage),
                                  dstImageLayout,
                                  resolveMode,
                                  regionCount,
                                  pRegions,
                                  flags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcImage", &srcImage);
        pLogContext->KeyAndStruct("srcImageLayout", srcImageLayout);
        pLogContext->KeyAndObject("dstImage", &dstImage);
        pLogContext->KeyAndStruct("dstImageLayout", dstImageLayout);
        pLogContext->KeyAndEnum("resolveMode", resolveMode);
        pLogContext->KeyAndBeginList("regions", false);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pLogContext->Struct(pRegions[idx]);
        }

        pLogContext->EndList();
        pLogContext->KeyAndResolveImageFlags("flags", flags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetEvent(
    const IGpuEvent& gpuEvent,
    uint32           stageMask)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetEvent);

    m_pNextLayer->CmdSetEvent(*NextGpuEvent(&gpuEvent), stageMask);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuEvent", &gpuEvent);
        pLogContext->KeyAndPipelineStageFlags("stageMask", stageMask);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    uint32           stageMask)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdResetEvent);

    m_pNextLayer->CmdResetEvent(*NextGpuEvent(&gpuEvent), stageMask);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuEvent", &gpuEvent);
        pLogContext->KeyAndPipelineStageFlags("stageMask", stageMask);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdPredicateEvent);

    m_pNextLayer->CmdPredicateEvent(*NextGpuEvent(&gpuEvent));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuEvent", &gpuEvent);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdMemoryAtomic);

    m_pNextLayer->CmdMemoryAtomic(*NextGpuMemory(&dstGpuMemory), dstOffset, srcData, atomicOp);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->KeyAndValue("srcData", srcData);
        pLogContext->KeyAndEnum("atomicOp", atomicOp);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBeginQuery);

    m_pNextLayer->CmdBeginQuery(*NextQueryPool(&queryPool), queryType, slot, flags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queryPool", &queryPool);
        pLogContext->KeyAndEnum("queryType", queryType);
        pLogContext->KeyAndValue("slot", slot);
        pLogContext->KeyAndStruct("flags", flags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdEndQuery);

    m_pNextLayer->CmdEndQuery(*NextQueryPool(&queryPool), queryType, slot);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queryPool", &queryPool);
        pLogContext->KeyAndEnum("queryType", queryType);
        pLogContext->KeyAndValue("slot", slot);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdResolveQuery);

    m_pNextLayer->CmdResolveQuery(*NextQueryPool(&queryPool),
                                  flags,
                                  queryType,
                                  startQuery,
                                  queryCount,
                                  *NextGpuMemory(&dstGpuMemory),
                                  dstOffset,
                                  dstStride);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queryPool", &queryPool);
        pLogContext->KeyAndQueryResultFlags("flags", flags);
        pLogContext->KeyAndEnum("queryType", queryType);
        pLogContext->KeyAndValue("startQuery", startQuery);
        pLogContext->KeyAndValue("queryCount", queryCount);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->KeyAndValue("dstStride", dstStride);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdResetQueryPool);

    m_pNextLayer->CmdResetQueryPool(*NextQueryPool(&queryPool), startQuery, queryCount);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queryPool", &queryPool);
        pLogContext->KeyAndValue("startQuery", startQuery);
        pLogContext->KeyAndValue("queryCount", queryCount);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWriteTimestamp);

    m_pNextLayer->CmdWriteTimestamp(stageMask, *NextGpuMemory(&dstGpuMemory), dstOffset);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndPipelineStageFlags("stageMask", stageMask);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    uint32             stageMask,
    uint64             data,
    ImmediateDataWidth dataSize,
    const gpusize address)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWriteImmediate);

    m_pNextLayer->CmdWriteImmediate(stageMask, data, dataSize, address);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        // Suppress unnecessary info for barrier log only mode to reduce json file size.
        if (m_pPlatform->IsBarrierLogActive() == false)
        {
            pLogContext->BeginInput();
            pLogContext->KeyAndPipelineStageFlags("stageMask", stageMask);
            pLogContext->KeyAndValue("data", data);
            pLogContext->KeyAndEnum("dataSize", dataSize);
            pLogContext->KeyAndValue("address", address);
            pLogContext->EndInput();
        }

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdLoadBufferFilledSizes);

    m_pNextLayer->CmdLoadBufferFilledSizes(gpuVirtAddr);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("gpuVirtAddr", false);

        for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
        {
            pLogContext->Value(gpuVirtAddr[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSaveBufferFilledSizes);

    m_pNextLayer->CmdSaveBufferFilledSizes(gpuVirtAddr);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndBeginList("gpuVirtAddr", false);

        for (uint32 idx = 0; idx < MaxStreamOutTargets; ++idx)
        {
            pLogContext->Value(gpuVirtAddr[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetBufferFilledSize(
    uint32  bufferId,
    uint32  offset)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetBufferFilledSize);

    m_pNextLayer->CmdSetBufferFilledSize(bufferId, offset);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("bufferId", bufferId);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdBindBorderColorPalette);

    m_pNextLayer->CmdBindBorderColorPalette(pipelineBindPoint, NextBorderColorPalette(pPalette));

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("pipelineBindPoint", pipelineBindPoint);
        pLogContext->KeyAndObject("palette", pPalette);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetPredication);

    m_pNextLayer->CmdSetPredication(NextQueryPool(pQueryPool),
                                    slot,
                                    pGpuMemory,
                                    offset,
                                    predType,
                                    predPolarity,
                                    waitResults,
                                    accumulateData);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("queryPool", pQueryPool);
        pLogContext->KeyAndValue("slot", slot);
        pLogContext->KeyAndObject("GpuMemory", pGpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndEnum("predType", predType);
        pLogContext->KeyAndValue("predPolarity", predPolarity);
        pLogContext->KeyAndValue("waitResults", waitResults);
        pLogContext->KeyAndValue("accumulateData", accumulateData);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSuspendPredication(
    bool suspend)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSuspendPredication);

    m_pNextLayer->CmdSuspendPredication(suspend);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("suspend", suspend);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdIf);

    m_pNextLayer->CmdIf(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->KeyAndEnum("compareFunc", compareFunc);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdElse()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdElse);

    m_pNextLayer->CmdElse();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdEndIf);

    m_pNextLayer->CmdEndIf();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWhile);

    m_pNextLayer->CmdWhile(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->KeyAndEnum("compareFunc", compareFunc);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdEndWhile()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdEndWhile);

    m_pNextLayer->CmdEndWhile();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWaitRegisterValue);

    m_pNextLayer->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("registerOffset", registerOffset);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->KeyAndEnum("compareFunc", compareFunc);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWaitMemoryValue(
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWaitMemoryValue);

    m_pNextLayer->CmdWaitMemoryValue(gpuVirtAddr, data, mask, compareFunc);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("gpuVirtAddr", gpuVirtAddr);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->KeyAndEnum("compareFunc", compareFunc);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId,
                            InterfaceFunc::CmdBufferCmdWaitBusAddressableMemoryMarker);

    m_pNextLayer->CmdWaitBusAddressableMemoryMarker(*NextGpuMemory(&gpuMemory), data, mask, compareFunc);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->KeyAndEnum("compareFunc", compareFunc);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateHiSPretests(
    const IImage*      pImage,
    const HiSPretests& pretests,
    uint32             firstMip,
    uint32             numMips)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdUpdateHiSPretests);

    m_pNextLayer->CmdUpdateHiSPretests(NextImage(pImage), pretests, firstMip, numMips);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("image", pImage);
        pLogContext->KeyAndStruct("pretests", pretests);
        pLogContext->KeyAndValue("firstMip", firstMip);
        pLogContext->KeyAndValue("numMips", numMips);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    // This function is not logged because it should only be called by other debug tools.
    m_pNextLayer->CmdBeginPerfExperiment(NextPerfExperiment(pPerfExperiment));
}

// =====================================================================================================================
void CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    // This function is not logged because it should only be called by other debug tools.
    GetNextLayer()->CmdUpdatePerfExperimentSqttTokenMask(NextPerfExperiment(pPerfExperiment), sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    // This function is not logged because it should only be called by other debug tools.
    GetNextLayer()->CmdUpdateSqttTokenMask(sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    // This function is not logged because it should only be called by other debug tools.
    m_pNextLayer->CmdEndPerfExperiment(NextPerfExperiment(pPerfExperiment));
}

// =====================================================================================================================
void CmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    // This function is not logged because it should only be called by other debug tools.
    m_pNextLayer->CmdInsertTraceMarker(markerType, markerData);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertRgpTraceMarker(
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    // This function is not logged because it should only be called by other debug tools.
    m_pNextLayer->CmdInsertRgpTraceMarker(subQueueFlags, numDwords, pData);
}

// =====================================================================================================================
uint32 CmdBuffer::CmdInsertExecutionMarker(
    bool        isBegin,
    uint8       sourceId,
    const char* pMarkerName,
    uint32      markerNameSize)
{
    // This function is not logged because it should only be called by other debug tools.
    return m_pNextLayer->CmdInsertExecutionMarker(isBegin, sourceId, pMarkerName, markerNameSize);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyDfSpmTraceData(
    const IPerfExperiment& perfExperiment,
    const IGpuMemory&      dstGpuMemory,
    gpusize                dstOffset)
{
    // This function is not logged because it should only be called by other debug tools.
    m_pNextLayer->CmdCopyDfSpmTraceData(*(NextPerfExperiment(&perfExperiment)), dstGpuMemory, dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdLoadCeRam);

    m_pNextLayer->CmdLoadCeRam(*NextGpuMemory(&srcGpuMemory), memOffset, ramOffset, dwordSize);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("srcGpuMemory", &srcGpuMemory);
        pLogContext->KeyAndValue("memOffset", memOffset);
        pLogContext->KeyAndValue("ramOffset", ramOffset);
        pLogContext->KeyAndValue("dwordSize", dwordSize);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
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
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdDumpCeRam);

    m_pNextLayer->CmdDumpCeRam(*NextGpuMemory(&dstGpuMemory), memOffset, ramOffset, dwordSize, currRingPos, ringSize);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("memOffset", memOffset);
        pLogContext->KeyAndValue("ramOffset", ramOffset);
        pLogContext->KeyAndValue("dwordSize", dwordSize);
        pLogContext->KeyAndValue("currRingPos", currRingPos);
        pLogContext->KeyAndValue("ringSize", ringSize);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,
    uint32      dwordSize)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdWriteCeRam);

    m_pNextLayer->CmdWriteCeRam(pSrcData, ramOffset, dwordSize);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("ramOffset", ramOffset);
        pLogContext->KeyAndBeginList("srcData", false);

        const uint32*const pSrcDwords = static_cast<const uint32*>(pSrcData);
        for (uint32 idx = 0; idx < dwordSize; ++idx)
        {
            pLogContext->Value(pSrcDwords[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    const bool   active   = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdAllocateEmbeddedData);
    uint32*const pCpuAddr = m_pNextLayer->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("sizeInDwords", sizeInDwords);
        pLogContext->KeyAndValue("alignmentInDwords", alignmentInDwords);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndValue("gpuAddress", *pGpuAddress);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return pCpuAddr;
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateLargeEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdAllocateLargeEmbeddedData);
    uint32*const pCpuAddr = m_pNextLayer->CmdAllocateLargeEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("sizeInDwords", sizeInDwords);
        pLogContext->KeyAndValue("alignmentInDwords", alignmentInDwords);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndValue("gpuAddress", *pGpuAddress);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    return pCpuAddr;
}

// =====================================================================================================================
Result CmdBuffer::AllocateAndBindGpuMemToEvent(
    IGpuEvent* pGpuEvent)
{
    // This function is not logged because it doesn't modify the command buffer.
    return GetNextLayer()->AllocateAndBindGpuMemToEvent(NextGpuEvent(pGpuEvent));
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    AutoBuffer<ICmdBuffer*, 16, Platform> nextCmdBuffers(cmdBufferCount, m_pPlatform);

    if (nextCmdBuffers.Capacity() < cmdBufferCount)
    {
        // If the layers become production code, we must set a flag here and return out of memory on End().
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        for (uint32 i = 0; i < cmdBufferCount; ++i)
        {
            nextCmdBuffers[i] = NextCmdBuffer(ppCmdBuffers[i]);
        }

        const bool active = m_pPlatform->ActivateLogging(m_objectId,
                                InterfaceFunc::CmdBufferCmdExecuteNestedCmdBuffers);

        m_pNextLayer->CmdExecuteNestedCmdBuffers(cmdBufferCount, &nextCmdBuffers[0]);

        if (active)
        {
            LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

            pLogContext->BeginInput();
            pLogContext->KeyAndBeginList("cmdBuffers", false);

            for (uint32 idx = 0; idx < cmdBufferCount; ++idx)
            {
                pLogContext->Object(ppCmdBuffers[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndInput();

            m_pPlatform->LogEndFunc(pLogContext);
        }
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSaveComputeState);

    m_pNextLayer->CmdSaveComputeState(stateFlags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndComputeStateFlags("stateFlags", stateFlags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdRestoreComputeState);

    m_pNextLayer->CmdRestoreComputeState(stateFlags);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndComputeStateFlags("stateFlags", stateFlags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdExecuteIndirectCmds);

    m_pNextLayer->CmdExecuteIndirectCmds(*NextIndirectCmdGenerator(&generator),
                                         gpuVirtAddr,
                                         maximumCount,
                                         countGpuAddr);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndObject("generator", &generator);
        pLogContext->KeyAndValue("gpuVirtAddr", gpuVirtAddr);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPostProcessFrame(
    const CmdPostProcessFrameInfo& postProcessInfo,
    bool*                          pAddedGpuWork)
{
    CmdPostProcessFrameInfo nextPostProcessInfo = postProcessInfo;
    bool addedGpuWork = false;

    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdPostProcessFrame);

    m_pNextLayer->CmdPostProcessFrame(*NextCmdPostProcessFrameInfo(postProcessInfo, &nextPostProcessInfo),
                                      &addedGpuWork);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("postProcessInfo", postProcessInfo);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndValue("addedGpuWork", addedGpuWork);
        pLogContext->EndOutput();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    if (addedGpuWork && (pAddedGpuWork != nullptr))
    {
        *pAddedGpuWork = true;
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCommentString(
    const char* pComment)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdCommentString);

    m_pNextLayer->CmdCommentString(pComment);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("comment", pComment);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}
// =====================================================================================================================
void CmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdNop);

    m_pNextLayer->CmdNop(pPayload, payloadSize);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        // Convert the payload to one long string of hexadecimal values.
        const uint32 blockLen  = 3 + sizeof(uint32) * 2; // "0x" + 2 chars per byte + a null or space.
        const uint32 numBlocks = RoundUpQuotient<uint32>(payloadSize, sizeof(uint32));

        AutoBuffer<char, 256, Platform> string(numBlocks * blockLen, m_pPlatform);
        BytesToStr(string.Data(), string.Capacity(), pPayload, payloadSize * sizeof(uint32), sizeof(uint32));

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("payload", string.Data());
        pLogContext->KeyAndValue("payloadSize", payloadSize);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdStartGpuProfilerLogging()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdStartGpuProfilerLogging);

    m_pNextLayer->CmdStartGpuProfilerLogging();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdStopGpuProfilerLogging);

    m_pNextLayer->CmdStopGpuProfilerLogging();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdXdmaWaitFlipPending);

    m_pNextLayer->CmdXdmaWaitFlipPending();

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::Destroy()
{
    // Note that we can't time Destroy calls nor track their callbacks.
    if (m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferDestroy))
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        m_pPlatform->LogEndFunc(pLogContext);
    }

    ICmdBuffer*const pNextLayer = m_pNextLayer;
    this->~CmdBuffer();
    pNextLayer->Destroy();
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdSetUserData);

    pThis->m_pNextLayer->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstEntry", firstEntry);
        pLogContext->KeyAndBeginList("values", false);

        for (uint32 idx = 0; idx < entryCount; ++idx)
        {
            pLogContext->Value(pEntryValues[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdSetUserData);

    pThis->m_pNextLayer->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstEntry", firstEntry);
        pLogContext->KeyAndBeginList("values", false);

        for (uint32 idx = 0; idx < entryCount; ++idx)
        {
            pLogContext->Value(pEntryValues[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
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
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDraw);

    pThis->m_pNextLayer->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount, drawId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstVertex", firstVertex);
        pLogContext->KeyAndValue("vertexCount", vertexCount);
        pLogContext->KeyAndValue("firstInstance", firstInstance);
        pLogContext->KeyAndValue("instanceCount", instanceCount);
        pLogContext->KeyAndValue("drawId", drawId);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDrawOpaque);

    pThis->m_pNextLayer->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("streamOutFilledSizeVa", streamOutFilledSizeVa);
        pLogContext->KeyAndValue("streamOutOffset",       streamOutOffset);
        pLogContext->KeyAndValue("stride",                stride);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDrawIndexed);

    pThis->m_pNextLayer->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount, drawId);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstIndex", firstIndex);
        pLogContext->KeyAndValue("indexCount", indexCount);
        pLogContext->KeyAndValue("vertexOffset", vertexOffset);
        pLogContext->KeyAndValue("firstInstance", firstInstance);
        pLogContext->KeyAndValue("instanceCount", instanceCount);
        pLogContext->KeyAndValue("drawId", drawId);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDrawIndirectMulti);

    pThis->m_pNextLayer->CmdDrawIndirectMulti(gpuVirtAddrAndStride, maximumCount, countGpuAddr);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("gpuVirtAddrAndStride", gpuVirtAddrAndStride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDrawIndexedIndirectMulti);

    pThis->m_pNextLayer->CmdDrawIndexedIndirectMulti(gpuVirtAddrAndStride, maximumCount, countGpuAddr);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("gpuVirtAddrAndStride", gpuVirtAddrAndStride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDispatch);

    pThis->m_pNextLayer->CmdDispatch(size);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("size", size);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer* pCmdBuffer,
    gpusize     gpuVirtAddr)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDispatchIndirect);

    pThis->m_pNextLayer->CmdDispatchIndirect(gpuVirtAddr);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();

        pLogContext->KeyAndValue("gpuVirtAddr", gpuVirtAddr);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDispatchOffset);

    pThis->m_pNextLayer->CmdDispatchOffset(offset, launchSize, logicalSize);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("offset",      offset);
        pLogContext->KeyAndStruct("launchSize",  launchSize);
        pLogContext->KeyAndStruct("logicalSize", logicalSize);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMesh(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId, InterfaceFunc::CmdBufferCmdDispatchMesh);

    pThis->m_pNextLayer->CmdDispatchMesh(size);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("size", size);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMeshIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*const     pThis     = static_cast<CmdBuffer*>(pCmdBuffer);
    Platform*const pPlatform = pThis->m_pPlatform;

    const bool active = pPlatform->ActivateLogging(pThis->m_objectId,
                            InterfaceFunc::CmdBufferCmdDispatchMeshIndirectMulti);

    pThis->m_pNextLayer->CmdDispatchMeshIndirectMulti(gpuVirtAddrAndStride, maximumCount, countGpuAddr);

    if (active)
    {
        LogContext*const pLogContext = pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("gpuVirtAddrAndStride", gpuVirtAddrAndStride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    const bool active = m_pPlatform->ActivateLogging(m_objectId, InterfaceFunc::CmdBufferCmdSetViewInstanceMask);

    m_pNextLayer->CmdSetViewInstanceMask(mask);

    if (active)
    {
        LogContext*const pLogContext = m_pPlatform->LogBeginFunc();

        pLogContext->BeginInput();
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

} // InterfaceLogger
} // Pal

#endif
