/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#if PAL_BUILD_RPM_GFX_SHADERS
#include "core/hw/gfxip/rpm/g_rpmGfxPipelineBinaries.h"
#endif

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
#if PAL_BUILD_RPM_GFX_SHADERS
    GraphicsPipelineCreateInfo               pipeInfo         = { };
    GraphicsPipelineInternalCreateInfo       internalInfo     = { };
    const GraphicsPipelineInternalCreateInfo NullInternalInfo = { };

    const GpuChipProperties& properties = pDevice->Parent()->ChipProperties();

    const PipelineBinary* pTable = nullptr;

    switch (uint32(properties.gfxTriple))
    {
    case Pal::IpTriple({ 10, 1, 0 }):
    case Pal::IpTriple({ 10, 1, 1 }):
        pTable = rpmGfxBinaryTableNavi10;
        break;

    case Pal::IpTriple({ 10, 1, 2 }):
        pTable = rpmGfxBinaryTableNavi14;
        break;

    case Pal::IpTriple({ 10, 3, 0 }):
    case Pal::IpTriple({ 10, 3, 1 }):
    case Pal::IpTriple({ 10, 3, 2 }):
    case Pal::IpTriple({ 10, 3, 4 }):
    case Pal::IpTriple({ 10, 3, 5 }):
        pTable = rpmGfxBinaryTableNavi21;
        break;

    case Pal::IpTriple({ 10, 3, 6 }):
        pTable = rpmGfxBinaryTableRaphael;
        break;

    case Pal::IpTriple({ 11, 0, 0 }):
    case Pal::IpTriple({ 11, 0, 1 }):
    case Pal::IpTriple({ 11, 0, 2 }):
        pTable = rpmGfxBinaryTableNavi31;
        break;

    case Pal::IpTriple({ 11, 0, 3 }):
        pTable = rpmGfxBinaryTablePhoenix1;
        break;

    default:
        result = Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_32ABGR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_32ABGR].size;

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
            &pPipelineMem[SlowColorClear_32ABGR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_32GR].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_32GR].size;

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
            &pPipelineMem[SlowColorClear_32GR],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_32R].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_32R].size;

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
            &pPipelineMem[SlowColorClear_32R],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_FP16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_FP16].size;

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
            &pPipelineMem[SlowColorClear_FP16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_SINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_SINT16].size;

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
            &pPipelineMem[SlowColorClear_SINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_SNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_SNORM16].size;

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
            &pPipelineMem[SlowColorClear_SNORM16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_UINT16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_UINT16].size;

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
            &pPipelineMem[SlowColorClear_UINT16],
            AllocInternal);
    }

    if (result == Result::Success)
    {
        pipeInfo = { };
        pipeInfo.pPipelineBinary       = pTable[SlowColorClear_UNORM16].pBuffer;
        pipeInfo.pipelineBinarySize    = pTable[SlowColorClear_UNORM16].size;

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
            &pPipelineMem[SlowColorClear_UNORM16],
            AllocInternal);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
        ))
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

#endif
    return result;
}

} // Pal
