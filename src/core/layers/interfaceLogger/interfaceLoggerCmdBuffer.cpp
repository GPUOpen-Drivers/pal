/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
    m_funcTable.pfnCmdDispatchDynamic           = CmdDispatchDynamic;
    m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMesh;
    m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMulti;
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferBegin;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = m_pNextLayer->Begin(NextCmdBufferBuildInfo(info));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("info", info);
        pLogContext->EndInput();

        pLogContext->BeginOutput();
        pLogContext->KeyAndEnum("result", result);
        pLogContext->EndOutput();

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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferEnd;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = m_pNextLayer->End();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferReset;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    const Result result   = m_pNextLayer->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 803
uint32 CmdBuffer::GetLargeEmbeddedDataLimit() const
{
    // This function is not logged because it doesn't modify the command buffer.
    return m_pNextLayer->GetLargeEmbeddedDataLimit();
}
#endif

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindPipeline;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindPipeline(NextPipelineBindParams(params));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindMsaaState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindMsaaState(NextMsaaState(pMsaaState));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("msaaState", pMsaaState);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSaveGraphicsState()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSaveGraphicsState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSaveGraphicsState();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreGraphicsState()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdRestoreGraphicsState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdRestoreGraphicsState();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdPrimeGpuCaches;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdPrimeGpuCaches(rangeCount, pRanges);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindColorBlendState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindColorBlendState(NextColorBlendState(pColorBlendState));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindDepthStencilState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindDepthStencilState(NextDepthStencilState(pDepthStencilState));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetDepthBounds;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetDepthBounds(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDuplicateUserData;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdDuplicateUserData(source, dest);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetKernelArguments;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetKernelArguments(firstArg, argCount, ppValues);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    uint32                firstBuffer,
    uint32                bufferCount,
    const BufferViewInfo* pBuffers)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetVertexBuffers;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetVertexBuffers(firstBuffer, bufferCount, pBuffers);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstBuffer", firstBuffer);
        pLogContext->KeyAndValue("bufferCount", bufferCount);
        pLogContext->KeyAndBeginList("buffers", false);

        for (uint32 idx = 0; idx < bufferCount; ++idx)
        {
            pLogContext->Struct(pBuffers[idx]);
        }

        pLogContext->EndList();
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindIndexData;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindIndexData(gpuAddr, indexCount, indexType);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindTargets;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindTargets(nextParams);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindStreamOutTargets;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindStreamOutTargets(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetPerDrawVrsRate;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetPerDrawVrsRate(rateParams);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetVrsCenterState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetVrsCenterState(centerState);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindSampleRateImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindSampleRateImage(NextImage(pImage));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdResolvePrtPlusImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdResolvePrtPlusImage(*NextImage(&srcImage),
                                         srcImageLayout,
                                         *NextImage(&dstImage),
                                         dstImageLayout,
                                         resolveType,
                                         regionCount,
                                         pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetBlendConst;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetBlendConst(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetInputAssemblyState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetInputAssemblyState(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetTriangleRasterState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetTriangleRasterState(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetPointLineRasterState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetPointLineRasterState(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId = InterfaceFunc::CmdBufferCmdSetLineStippleState;
    funcInfo.objectId = m_objectId;
    funcInfo.preCallTime = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetLineStippleState(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetDepthBiasState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetDepthBiasState(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetStencilRefMasks;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetStencilRefMasks(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetUserClipPlanes;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetClipRects;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetClipRects(clipRule, rectCount, pRectList);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetMsaaQuadSamplePattern;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetViewports;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetViewports(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetScissorRects;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetScissorRects(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetGlobalScissor;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetGlobalScissor(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
// =====================================================================================================================
void CmdBuffer::CmdSetColorWriteMask(
    const ColorWriteMaskParams& params)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetColorWriteMask;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetColorWriteMask(params);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("params", params);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetRasterizerDiscardEnable(
    bool rasterizerDiscardEnable)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetRasterizerDiscardEnable;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetRasterizerDiscardEnable(rasterizerDiscardEnable);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("rasterizerDiscardEnable", rasterizerDiscardEnable);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}
#endif

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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBarrier;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdBarrier(nextBarrierInfo);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdRelease;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        syncToken = m_pNextLayer->CmdRelease(nextReleaseInfo);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdAcquire;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdAcquire(nextAcquireInfo, syncTokenCount, pSyncTokens);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdReleaseEvent;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdReleaseEvent(nextReleaseInfo, NextGpuEvent(pGpuEvent));
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdAcquireEvent;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdAcquireEvent(nextAcquireInfo, gpuEventCount, &nextGpuEvents[0]);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdReleaseThenAcquire;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdReleaseThenAcquire(nextBarrierInfo);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyMemory(*NextGpuMemory(&srcGpuMemory), *NextGpuMemory(&dstGpuMemory), regionCount, pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyMemoryByGpuVa;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyMemoryByGpuVa(srcGpuVirtAddr, dstGpuVirtAddr, regionCount, pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyImage(*NextImage(&srcImage),
                               srcImageLayout,
                               *NextImage(&dstImage),
                               dstImageLayout,
                               regionCount,
                               pRegions,
                               pScissorRect,
                               flags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyMemoryToImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyMemoryToImage(*NextGpuMemory(&srcGpuMemory),
                                       *NextImage(&dstImage),
                                       dstImageLayout,
                                       regionCount,
                                       pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyImageToMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyImageToMemory(*NextImage(&srcImage),
                                       srcImageLayout,
                                       *NextGpuMemory(&dstGpuMemory),
                                       regionCount,
                                       pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyMemoryToTiledImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyMemoryToTiledImage(*NextGpuMemory(&srcGpuMemory),
                                            *NextImage(&dstImage),
                                            dstImageLayout,
                                            regionCount,
                                            pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyTiledImageToMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyTiledImageToMemory(*NextImage(&srcImage),
                                            srcImageLayout,
                                            *NextGpuMemory(&dstGpuMemory),
                                            regionCount,
                                            pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyTypedBuffer;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyTypedBuffer(*NextGpuMemory(&srcGpuMemory),
                                     *NextGpuMemory(&dstGpuMemory),
                                     regionCount,
                                     pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdScaledCopyTypedBufferToImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdScaledCopyTypedBufferToImage(*NextGpuMemory(&srcGpuMemory),
                                                  *NextImage(&dstImage),
                                                  dstImageLayout,
                                                  regionCount,
                                                  pRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCopyRegisterToMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCopyRegisterToMemory(srcRegisterOffset, *NextGpuMemory(&dstGpuMemory), dstOffset);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdScaledCopyImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdScaledCopyImage(nextCopyInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdGenerateMipmaps;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdGenerateMipmaps(nextGenInfo);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdColorSpaceConversionCopy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdColorSpaceConversionCopy(*NextImage(&srcImage),
                                              srcImageLayout,
                                              *NextImage(&dstImage),
                                              dstImageLayout,
                                              regionCount,
                                              pRegions,
                                              filter,
                                              cscTable);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCloneImageData;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCloneImageData(*NextImage(&srcImage), *NextImage(&dstImage));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdUpdateMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdUpdateMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, dataSize, pData);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdUpdateBusAddressableMemoryMarker;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdUpdateBusAddressableMemoryMarker(*NextGpuMemory(&dstGpuMemory), offset, value);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdFillMemory;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdFillMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, fillSize, data);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearColorBuffer;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdClearColorBuffer(*NextGpuMemory(&gpuMemory),
                                      color,
                                      bufferFormat,
                                      bufferOffset,
                                      bufferExtent,
                                      rangeCount,
                                      pRanges);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearBoundColorTargets;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdClearBoundColorTargets(colorTargetCount, pBoundColorTargets, regionCount, pClearRegions);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearColorImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdClearColorImage(*NextImage(&image),
                                     imageLayout,
                                     color,
                                     clearFormat,
                                     rangeCount,
                                     pRanges,
                                     boxCount,
                                     pBoxes,
                                     flags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearBoundDepthStencilTargets;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();

    m_pNextLayer->CmdClearBoundDepthStencilTargets(depth,
                                                   stencil,
                                                   stencilWriteMask,
                                                   samples,
                                                   fragments,
                                                   flag,
                                                   regionCount,
                                                   pClearRegions);

    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearDepthStencil;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
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
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearBufferView;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdClearBufferView(*NextGpuMemory(&gpuMemory), color, pBufferViewSrd, rangeCount, pRanges);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdClearImageView;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdClearImageView(*NextImage(&image), imageLayout, color, pImageViewSrd, rectCount, pRects);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdResolveImage;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdResolveImage(*NextImage(&srcImage),
                                  srcImageLayout,
                                  *NextImage(&dstImage),
                                  dstImageLayout,
                                  resolveMode,
                                  regionCount,
                                  pRegions,
                                  flags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    HwPipePoint      setPoint)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetEvent;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetEvent(*NextGpuEvent(&gpuEvent), setPoint);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuEvent", &gpuEvent);
        pLogContext->KeyAndEnum("setPoint", setPoint);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      resetPoint)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdResetEvent;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdResetEvent(*NextGpuEvent(&gpuEvent), resetPoint);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuEvent", &gpuEvent);
        pLogContext->KeyAndEnum("resetPoint", resetPoint);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdPredicateEvent;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdPredicateEvent(*NextGpuEvent(&gpuEvent));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdMemoryAtomic;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdMemoryAtomic(*NextGpuMemory(&dstGpuMemory), dstOffset, srcData, atomicOp);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBeginQuery;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBeginQuery(*NextQueryPool(&queryPool), queryType, slot, flags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdEndQuery;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdEndQuery(*NextQueryPool(&queryPool), queryType, slot);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdResolveQuery;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdResolveQuery(*NextQueryPool(&queryPool),
                                  flags,
                                  queryType,
                                  startQuery,
                                  queryCount,
                                  *NextGpuMemory(&dstGpuMemory),
                                  dstOffset,
                                  dstStride);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdResetQueryPool;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdResetQueryPool(*NextQueryPool(&queryPool), startQuery, queryCount);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdWriteTimestamp;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdWriteTimestamp(pipePoint, *NextGpuMemory(&dstGpuMemory), dstOffset);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("pipePoint", pipePoint);
        pLogContext->KeyAndObject("dstGpuMemory", &dstGpuMemory);
        pLogContext->KeyAndValue("dstOffset", dstOffset);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    const gpusize address)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId = InterfaceFunc::CmdBufferCmdWriteImmediate;
    funcInfo.objectId = m_objectId;
    funcInfo.preCallTime = m_pPlatform->GetTime();
    m_pNextLayer->CmdWriteImmediate(pipePoint, data, dataSize, address);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndEnum("pipePoint", pipePoint);
        pLogContext->KeyAndValue("data", data);
        pLogContext->KeyAndEnum("dataSize", dataSize);
        pLogContext->KeyAndValue("address", address);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdLoadBufferFilledSizes;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdLoadBufferFilledSizes(gpuVirtAddr);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSaveBufferFilledSizes;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSaveBufferFilledSizes(gpuVirtAddr);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetBufferFilledSize;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetBufferFilledSize(bufferId, offset);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdBindBorderColorPalette;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdBindBorderColorPalette(pipelineBindPoint, NextBorderColorPalette(pPalette));
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetPredication;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetPredication(NextQueryPool(pQueryPool),
                                    slot,
                                    pGpuMemory,
                                    offset,
                                    predType,
                                    predPolarity,
                                    waitResults,
                                    accumulateData);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSuspendPredication;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSuspendPredication(suspend);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdIf;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdIf(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdElse;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdElse();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdEndIf;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdEndIf();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdWhile;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdWhile(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdEndWhile;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdEndWhile();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdWaitRegisterValue;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdWaitMemoryValue;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdWaitMemoryValue(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId = InterfaceFunc::CmdBufferCmdWaitBusAddressableMemoryMarker;
    funcInfo.objectId = m_objectId;
    funcInfo.preCallTime = m_pPlatform->GetTime();
    m_pNextLayer->CmdWaitBusAddressableMemoryMarker(*NextGpuMemory(&gpuMemory), data, mask, compareFunc);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId = InterfaceFunc::CmdUpdateHiSPretests;
    funcInfo.objectId = m_objectId;
    funcInfo.preCallTime = m_pPlatform->GetTime();
    m_pNextLayer->CmdUpdateHiSPretests(NextImage(pImage), pretests, firstMip, numMips);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdLoadCeRam;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdLoadCeRam(*NextGpuMemory(&srcGpuMemory), memOffset, ramOffset, dwordSize);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDumpCeRam;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdDumpCeRam(*NextGpuMemory(&dstGpuMemory), memOffset, ramOffset, dwordSize, currRingPos, ringSize);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdWriteCeRam;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdWriteCeRam(pSrcData, ramOffset, dwordSize);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdAllocateEmbeddedData;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    uint32*const pCpuAddr = m_pNextLayer->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 803
uint32* CmdBuffer::CmdAllocateLargeEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId = InterfaceFunc::CmdBufferCmdAllocateLargeEmbeddedData;
    funcInfo.objectId = m_objectId;
    funcInfo.preCallTime = m_pPlatform->GetTime();
    uint32* const pCpuAddr = m_pNextLayer->CmdAllocateLargeEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
#endif

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

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::CmdBufferCmdExecuteNestedCmdBuffers;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = m_pPlatform->GetTime();
        m_pNextLayer->CmdExecuteNestedCmdBuffers(cmdBufferCount, &nextCmdBuffers[0]);
        funcInfo.postCallTime = m_pPlatform->GetTime();

        LogContext* pLogContext = nullptr;
        if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
        {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSaveComputeState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSaveComputeState(stateFlags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdRestoreComputeState;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdRestoreComputeState(stateFlags);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndComputeStateFlags("stateFlags", stateFlags);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdExecuteIndirectCmds;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdExecuteIndirectCmds(*NextIndirectCmdGenerator(&generator),
                                         *NextGpuMemory(&gpuMemory),
                                         offset,
                                         maximumCount,
                                         countGpuAddr);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("generator", &generator);
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
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
    CmdPostProcessFrameInfo nextPostProcessInfo = {};
    bool addedGpuWork = false;

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdPostProcessFrame;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdPostProcessFrame(*NextCmdPostProcessFrameInfo(postProcessInfo, &nextPostProcessInfo),
                                      &addedGpuWork);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdCommentString;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdCommentString(pComment);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdNop;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdNop(pPayload, payloadSize);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdStartGpuProfilerLogging;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdStartGpuProfilerLogging();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdStopGpuProfilerLogging;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdStopGpuProfilerLogging();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdXdmaWaitFlipPending;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdXdmaWaitFlipPending();
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::Destroy()
{
    // Note that we can't time a Destroy call.
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferDestroy;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    funcInfo.postCallTime = funcInfo.preCallTime;

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetUserData;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstEntry", firstEntry);
        pLogContext->KeyAndBeginList("values", false);

        for (uint32 idx = 0; idx < entryCount; ++idx)
        {
            pLogContext->Value(pEntryValues[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetUserData;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstEntry", firstEntry);
        pLogContext->KeyAndBeginList("values", false);

        for (uint32 idx = 0; idx < entryCount; ++idx)
        {
            pLogContext->Value(pEntryValues[idx]);
        }

        pLogContext->EndList();
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDraw;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();

    pThis->m_pNextLayer->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount, drawId);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstVertex", firstVertex);
        pLogContext->KeyAndValue("vertexCount", vertexCount);
        pLogContext->KeyAndValue("firstInstance", firstInstance);
        pLogContext->KeyAndValue("instanceCount", instanceCount);
        pLogContext->KeyAndValue("drawId", drawId);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDrawOpaque;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("streamOutFilledSizeVa", streamOutFilledSizeVa);
        pLogContext->KeyAndValue("streamOutOffset",       streamOutOffset);
        pLogContext->KeyAndValue("stride",                stride);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDrawIndexed;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount, drawId);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("firstIndex", firstIndex);
        pLogContext->KeyAndValue("indexCount", indexCount);
        pLogContext->KeyAndValue("vertexOffset", vertexOffset);
        pLogContext->KeyAndValue("firstInstance", firstInstance);
        pLogContext->KeyAndValue("instanceCount", instanceCount);
        pLogContext->KeyAndValue("drawId", drawId);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDrawIndirectMulti;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("stride", stride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDrawIndexedIndirectMulti;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                     offset,
                                                     stride,
                                                     maximumCount,
                                                     countGpuAddr);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("stride", stride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDispatch;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDispatch(size);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("size", size);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDispatchIndirect;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdDispatchOffset;
    funcInfo.objectId     = pThis->m_objectId;
    funcInfo.preCallTime  = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDispatchOffset(offset, launchSize, logicalSize);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("offset",      offset);
        pLogContext->KeyAndStruct("launchSize",  launchSize);
        pLogContext->KeyAndStruct("logicalSize", logicalSize);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchDynamic(
    ICmdBuffer*  pCmdBuffer,
    gpusize      gpuVa,
    DispatchDims size)
{
    auto* const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo = {};
    funcInfo.funcId      = InterfaceFunc::CmdBufferCmdDispatchIndirect;
    funcInfo.objectId    = pThis->m_objectId;
    funcInfo.preCallTime = pThis->m_pPlatform->GetTime();

    pThis->m_pNextLayer->CmdDispatchDynamic(gpuVa, size);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("gpuVa", gpuVa);
        pLogContext->KeyAndStruct("size", size);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdDispatchMesh(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo = {};
    funcInfo.funcId        = InterfaceFunc::CmdBufferCmdDispatchMesh;
    funcInfo.objectId      = pThis->m_objectId;
    funcInfo.preCallTime   = pThis->m_pPlatform->GetTime();

    pThis->m_pNextLayer->CmdDispatchMesh(size);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndStruct("size", size);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
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
    auto*const pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    BeginFuncInfo funcInfo = {};
    funcInfo.funcId        = InterfaceFunc::CmdBufferCmdDispatchMeshIndirectMulti;
    funcInfo.objectId      = pThis->m_objectId;
    funcInfo.preCallTime   = pThis->m_pPlatform->GetTime();
    pThis->m_pNextLayer->CmdDispatchMeshIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                      offset,
                                                      stride,
                                                      maximumCount,
                                                      countGpuAddr);
    funcInfo.postCallTime = pThis->m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (pThis->m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndObject("gpuMemory", &gpuMemory);
        pLogContext->KeyAndValue("offset", offset);
        pLogContext->KeyAndValue("stride", stride);
        pLogContext->KeyAndValue("maximumCount", maximumCount);
        pLogContext->KeyAndValue("countGpuAddr", countGpuAddr);
        pLogContext->EndInput();

        pThis->m_pPlatform->LogEndFunc(pLogContext);
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    BeginFuncInfo funcInfo;
    funcInfo.funcId       = InterfaceFunc::CmdBufferCmdSetViewInstanceMask;
    funcInfo.objectId     = m_objectId;
    funcInfo.preCallTime  = m_pPlatform->GetTime();
    m_pNextLayer->CmdSetViewInstanceMask(mask);
    funcInfo.postCallTime = m_pPlatform->GetTime();

    LogContext* pLogContext = nullptr;
    if (m_pPlatform->LogBeginFunc(funcInfo, &pLogContext))
    {
        pLogContext->BeginInput();
        pLogContext->KeyAndValue("mask", mask);
        pLogContext->EndInput();

        m_pPlatform->LogEndFunc(pLogContext);
    }
}

} // InterfaceLogger
} // Pal

#endif
