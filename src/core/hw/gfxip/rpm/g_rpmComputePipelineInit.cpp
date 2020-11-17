/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/rpm/g_rpmComputePipelineInit.h"
#include "core/hw/gfxip/rpm/g_rpmComputePipelineBinaries.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Helper function to create compute pipelines.
Result CreateRpmComputePipeline(
    RpmComputePipeline    pipelineType,
    GfxDevice*            pDevice,
    const PipelineBinary* pTable,
    ComputePipeline**     pPipelineMem)
{
    const uint32 index = static_cast<uint32>(pipelineType);

    ComputePipelineCreateInfo pipeInfo = { };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 631
    pipeInfo.flags.overrideGpuHeap     = 1;
    pipeInfo.preferredHeapType         = GpuHeap::GpuHeapLocal;
#endif
    pipeInfo.pPipelineBinary           = pTable[index].pBuffer;
    pipeInfo.pipelineBinarySize        = pTable[index].size;

    PAL_ASSERT((pipeInfo.pPipelineBinary != nullptr) && (pipeInfo.pipelineBinarySize != 0));

    return pDevice->CreateComputePipelineInternal(
        pipeInfo,
        &pPipelineMem[index],
        AllocInternal);
}

// =====================================================================================================================
// Creates all compute pipeline objects required by RsrcProcMgr.
Result CreateRpmComputePipelines(
    GfxDevice*        pDevice,
    ComputePipeline** pPipelineMem)
{
    Result result = Result::Success;

    const GpuChipProperties& properties = pDevice->Parent()->ChipProperties();

    const PipelineBinary* pTable = nullptr;

    switch (properties.revision)
    {
#if PAL_BUILD_GFX6
    case AsicRevision::Tahiti:
    case AsicRevision::Pitcairn:
    case AsicRevision::Capeverde:
    case AsicRevision::Oland:
    case AsicRevision::Hainan:
        pTable = rpmComputeBinaryTableTahiti;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Spectre:
    case AsicRevision::Spooky:
        pTable = rpmComputeBinaryTableSpectre;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::HawaiiPro:
        pTable = rpmComputeBinaryTableHawaiiPro;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Hawaii:
        pTable = rpmComputeBinaryTableHawaii;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Kalindi:
    case AsicRevision::Bonaire:
    case AsicRevision::Godavari:
        pTable = rpmComputeBinaryTableKalindi;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Carrizo:
    case AsicRevision::Bristol:
    case AsicRevision::Fiji:
    case AsicRevision::Polaris10:
    case AsicRevision::Polaris11:
    case AsicRevision::Polaris12:
        pTable = rpmComputeBinaryTableCarrizo;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Iceland:
        pTable = rpmComputeBinaryTableIceland;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::TongaPro:
        pTable = rpmComputeBinaryTableTongaPro;
        break;
#endif

#if PAL_BUILD_GFX6
    case AsicRevision::Stoney:
        pTable = rpmComputeBinaryTableStoney;
        break;
#endif

    case AsicRevision::Vega10:
    case AsicRevision::Raven:
    case AsicRevision::Vega12:
        pTable = rpmComputeBinaryTableVega10;
        break;

    case AsicRevision::Vega20:
        pTable = rpmComputeBinaryTableVega20;
        break;

    case AsicRevision::Raven2:
    case AsicRevision::Renoir:
        pTable = rpmComputeBinaryTableRaven2;
        break;

    case AsicRevision::Navi10:
        pTable = rpmComputeBinaryTableNavi10;
        break;

    case AsicRevision::Navi14:
        pTable = rpmComputeBinaryTableNavi14;
        break;

    default:
        result = Result::ErrorUnknown;
        PAL_NOT_IMPLEMENTED();
        break;
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearBuffer, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage1d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage1dTexelScale, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage2dTexelScale, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ClearImage3dTexelScale, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyBufferByte, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyBufferDqword, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyBufferDword, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImage2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImage2dms2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImage2dms4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImage2dms8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImage2dShaderMipLevel, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImageGammaCorrect2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem1d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem2dms2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem2dms4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem2dms8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyImgToMem3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg1d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg2dms2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg2dms4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg2dms8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyMemToImg3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyTypedBuffer1d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyTypedBuffer2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::CopyTypedBuffer3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ExpandMaskRam, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ExpandMaskRamMs2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ExpandMaskRamMs4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ExpandMaskRamMs8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::FastDepthClear, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::FastDepthExpClear, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::FastDepthStExpClear, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::FillMem4xDword, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::FillMemDword, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::GenerateMipmaps, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::GenerateMipmapsLowp, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::HtileCopyAndFixUp, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::HtileSR4xUpdate, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::HtileSRUpdate, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskCopyImage, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskCopyImageOptimized, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskCopyImgToMem, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskExpand2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskExpand4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskExpand8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve1xEqaa, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2xEqaa, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2xEqaaMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2xEqaaMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve2xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4xEqaa, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4xEqaaMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4xEqaaMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve4xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8xEqaa, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8xEqaaMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8xEqaaMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskResolve8xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaFmaskScaledCopy, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve2x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve2xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve2xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve4x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve4xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve4xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve8x, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve8xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolve8xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil2xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil2xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil4xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil4xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil8xMax, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::MsaaResolveStencil8xMin, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::PackedPixelComposite, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ResolveOcclusionQuery, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ResolvePipelineStatsQuery, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ResolveStreamoutStatsQuery, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::RgbToYuvPacked, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::RgbToYuvPlanar, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ScaledCopyImage2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::ScaledCopyImage3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::YuvIntToRgb, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success)
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::YuvToRgb, pDevice, pTable, pPipelineMem);
    }

#if PAL_BUILD_GFX6
    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx6GenerateCmdDispatch, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp6)
#endif
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp7)
#endif
        || (properties.gfxLevel == GfxIpLevel::GfxIp8)
#if PAL_BUILD_GFX6
        || (properties.gfxLevel == GfxIpLevel::GfxIp8_1)
#endif
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx6GenerateCmdDraw, pDevice, pTable, pPipelineMem);
    }
#endif

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9BuildHtileLookupTable, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearDccMultiSample2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearDccOptimized2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearDccSingleSample2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearDccSingleSample3d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearHtileFast, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearHtileMultiSample, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearHtileOptimized2d, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9ClearHtileSingleSample, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9Fill4x4Dword, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9GenerateCmdDispatch, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9GenerateCmdDraw, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9HtileCopyAndFixUp, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp9)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx9InitCmask, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixel, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixelMsaa, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx10GenerateCmdDispatch, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx10GenerateCmdDispatchTaskMesh, pDevice, pTable, pPipelineMem);
    }

    if (result == Result::Success && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        ))
    {
        result = CreateRpmComputePipeline(
            RpmComputePipeline::Gfx10GenerateCmdDraw, pDevice, pTable, pPipelineMem);
    }

    return result;
}

} // Pal
