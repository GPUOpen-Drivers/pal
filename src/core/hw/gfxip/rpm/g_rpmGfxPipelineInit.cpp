/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/internalMemMgr.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineInit.h"
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineBinaries.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Creates all graphics pipeline objects required by RsrcProcMgr.
Result CreateRpmGraphicsPipelines(
    GfxDevice*         pDevice,
    GraphicsPipeline** pPipelineMem)
{
    Result result = Result::Success;

    GraphicsPipelineCreateInfo               pipeInfo         = { };
    GraphicsPipelineInternalCreateInfo       internalInfo     = { };
    const GraphicsPipelineInternalCreateInfo NullInternalInfo = { };

    const GpuChipProperties& properties = pDevice->Parent()->ChipProperties();

    const PipelineBinary* pTable = nullptr;

    switch (properties.revision)
    {
    case AsicRevision::Polaris10:
    case AsicRevision::Polaris11:
    case AsicRevision::Polaris12:
        pTable = rpmGfxBinaryTablePolaris10;
        break;

    case AsicRevision::Vega10:
    case AsicRevision::Raven:
    case AsicRevision::Vega12:
    case AsicRevision::Vega20:
        pTable = rpmGfxBinaryTableVega10;
        break;

    case AsicRevision::Raven2:
    case AsicRevision::Renoir:
        pTable = rpmGfxBinaryTableRaven2;
        break;

    case AsicRevision::Navi10:
        pTable = rpmGfxBinaryTableNavi10;
        break;

    case AsicRevision::Navi12:
        pTable = rpmGfxBinaryTableNavi12;
        break;

    case AsicRevision::Navi14:
        pTable = rpmGfxBinaryTableNavi14;
        break;

    case AsicRevision::Navi21:
    case AsicRevision::Navi22:
    case AsicRevision::Navi23:
        pTable = rpmGfxBinaryTableNavi21;
        break;

    case AsicRevision::Navi24:
        pTable = rpmGfxBinaryTableNavi24;
        break;

    case AsicRevision::Rembrandt:
        pTable = rpmGfxBinaryTableRembrandt;
        break;

    case AsicRevision::Raphael:
        pTable = rpmGfxBinaryTableRaphael;
        break;

#if PAL_BUILD_NAVI31
    case AsicRevision::Navi31:
        pTable = rpmGfxBinaryTableNavi31;
        break;
#endif

    default:
        result = Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyDepth].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyMsaaDepth].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyMsaaDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyMsaaDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyMsaaDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyMsaaDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyMsaaDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyMsaaStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyMsaaStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyMsaaStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[CopyStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[CopyStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[CopyStencil],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[DccDecompress].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[DccDecompress].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.dccDecompress = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[DccDecompress],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[DepthExpand].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[DepthExpand].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthExpand],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[DepthResummarize].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[DepthResummarize].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthResummarize],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[DepthSlowDraw].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[DepthSlowDraw].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[DepthSlowDraw],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[FastClearElim].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[FastClearElim].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.fastClearElim = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[FastClearElim],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[FmaskDecompress].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[FmaskDecompress].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.fmaskDecompress = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[FmaskDecompress],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Copy_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Copy_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Copy_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32GR],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_32R],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_FP16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_SINT16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_UINT16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveFixedFunc_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveFixedFunc_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        internalInfo = { };
        internalInfo.flags.resolveFixedFunc = true;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            internalInfo,
            &pPipelineMem[ResolveFixedFunc_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy2d_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy2d_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy2d_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopy3d_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopy3d_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopy3d_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear0_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear0_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear0_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear1_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear1_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[1].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[1].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[1].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear1_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear2_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear2_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[2].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[2].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[2].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear2_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear3_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear3_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[3].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[3].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[3].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear3_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear4_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear4_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[4].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[4].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[4].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear4_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear5_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear5_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[5].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[5].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[5].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear5_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear6_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear6_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[6].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[6].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[6].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear6_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear7_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear7_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[7].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[7].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[7].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[SlowColorClear7_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveDepth].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveDepthCopy].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveDepthCopy].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Float;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveDepthCopy],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ResolveStencilCopy].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ResolveStencilCopy].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x2;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::Y, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ResolveStencilCopy],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyDepth].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyImageColorKey].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyImageColorKey].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyImageColorKey],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyMsaaDepth].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyMsaaDepth].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyMsaaDepth],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyMsaaDepthStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyMsaaDepthStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyMsaaDepthStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyMsaaStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyMsaaStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyMsaaStencil],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[ScaledCopyStencil].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[ScaledCopyStencil].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[ScaledCopyStencil],
            AllocInternal);
    }

#if PAL_BUILD_NAVI31
    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_32ABGR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_32GR].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x3;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32Y32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_32GR],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_32R].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0x1;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X32_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_32R],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_FP16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X8Y8Z8W8_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_FP16],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_SINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Sint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_SINT16],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_SNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Snorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_UINT16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Uint;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_UINT16],
            AllocInternal);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_NAVI31
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#endif
        ))
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[Gfx11ResolveGraphics_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[Gfx11ResolveGraphics_UNORM16].size;

        PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

        pipeInfo.iaState.topologyInfo.primitiveType = PrimitiveType::Rect;

        pipeInfo.cbState.target[0].channelWriteMask       = 0xF;
        pipeInfo.cbState.target[0].swizzledFormat.format  = ChNumFormat::X16Y16Z16W16_Unorm;
        pipeInfo.cbState.target[0].swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W };

        pipeInfo.viewportInfo.depthClipNearEnable = false;
        pipeInfo.viewportInfo.depthClipFarEnable  = false;
        pipeInfo.viewportInfo.depthRange = DepthRange::ZeroToOne;
        pipeInfo.cbState.logicOp         = LogicOp::Copy;
        pipeInfo.rsState.binningOverride = BinningOverride::Disable;
        pipeInfo.rsState.depthClampMode  = DepthClampMode::_None;

        result = pDevice->CreateGraphicsPipelineInternal(
            pipeInfo,
            NullInternalInfo,
            &pPipelineMem[Gfx11ResolveGraphics_UNORM16],
            AllocInternal);
    }
#endif

    return result;
}

// Make sure interface changes have been propagated to RPM.
static_assert((
    (offsetof(StencilRefMaskParams, frontRef)       == 0) &&
    (offsetof(StencilRefMaskParams, frontReadMask)  == 1) &&
    (offsetof(StencilRefMaskParams, frontWriteMask) == 2) &&
    (offsetof(StencilRefMaskParams, frontOpValue)   == 3) &&
    (offsetof(StencilRefMaskParams, backRef)        == 4) &&
    (offsetof(StencilRefMaskParams, backReadMask)   == 5) &&
    (offsetof(StencilRefMaskParams, backWriteMask)  == 6) &&
    (offsetof(StencilRefMaskParams, backOpValue)    == 7) &&
    (offsetof(StencilRefMaskParams, flags)          == 8)),
    "StencilRefMaskParams interface change not propagated. Update this file to match interface changes.");

static_assert((offsetof(InputAssemblyStateParams, topology) == 0),
    "PrimitiveTopologyParams interface change not propagated. Update this file to match interface changes.");

static_assert((
    (offsetof(DepthBiasParams, depthBias)            == 0) &&
    (offsetof(DepthBiasParams, depthBiasClamp)       == sizeof(float)) &&
    (offsetof(DepthBiasParams, slopeScaledDepthBias) == (2 * sizeof(float)))),
    "DepthBiasParams interface change not propagated. Update this file to match interface changes.");

static_assert((
    (offsetof(PointLineRasterStateParams, pointSize) == 0) &&
    (offsetof(PointLineRasterStateParams, lineWidth) == sizeof(float))),
    "PointLineRasterStateParams interface change not propagated. Update this file to match interface changes.");

static_assert((
    (offsetof(TriangleRasterStateParams, frontFillMode)   == 0) &&
    (offsetof(TriangleRasterStateParams, backFillMode)    == sizeof(Pal::FillMode)) &&
    (offsetof(TriangleRasterStateParams, cullMode)        == 2 * sizeof(Pal::FillMode)) &&
    (offsetof(TriangleRasterStateParams, frontFace)       == 2 * sizeof(Pal::FillMode) + sizeof(Pal::CullMode)) &&
    (offsetof(TriangleRasterStateParams, provokingVertex) == 2 * sizeof(Pal::FillMode) +
                                                             sizeof(Pal::CullMode) +
                                                             sizeof(Pal::FaceOrientation))),
    "TriangleRasterStateParams interface change not propagated. Update this file to match interface changes.");

} // Pal
