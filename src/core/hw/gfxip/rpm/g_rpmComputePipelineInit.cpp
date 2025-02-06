/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
// Helper function that returns a compute pipeline table for a given gfxIP
static const PipelineBinary*const GetRpmComputePipelineTable(
    const GpuChipProperties& properties)
{
    const PipelineBinary* pTable = nullptr;
    switch (uint32(properties.gfxTriple))
    {
    case Pal::IpTriple({ 10, 1, 0 }):
    case Pal::IpTriple({ 10, 1, 1 }):
    case Pal::IpTriple({ 10, 1, 2 }):
        pTable = rpmComputeBinaryTableNavi10;
        break;

    case Pal::IpTriple({ 10, 3, 0 }):
    case Pal::IpTriple({ 10, 3, 1 }):
    case Pal::IpTriple({ 10, 3, 2 }):
    case Pal::IpTriple({ 10, 3, 4 }):
    case Pal::IpTriple({ 10, 3, 5 }):
        pTable = rpmComputeBinaryTableNavi21;
        break;

    case Pal::IpTriple({ 10, 3, 6 }):
        pTable = rpmComputeBinaryTableRaphael;
        break;

    case Pal::IpTriple({ 11, 0, 0 }):
    case Pal::IpTriple({ 11, 0, 1 }):
        pTable = rpmComputeBinaryTableNavi31;
        break;

    case Pal::IpTriple({ 11, 0, 2 }):
        pTable = rpmComputeBinaryTableNavi33;
        break;

    case Pal::IpTriple({ 11, 0, 3 }):
        pTable = rpmComputeBinaryTablePhoenix1;
        break;

#if PAL_BUILD_STRIX1
    case Pal::IpTriple({ 11, 5, 0 }):
    case Pal::IpTriple({ 11, 5, 65535 }):
        pTable = rpmComputeBinaryTableStrix1;
        break;
#endif

    }

    return pTable;
}

// =====================================================================================================================
// Creates all compute pipeline objects required by RsrcProcMgr.
Result CreateRpmComputePipelines(
    GfxDevice*        pDevice,
    ComputePipeline** pPipelineMem)
{
    Result result = Result::Success;

    const GpuChipProperties& properties = pDevice->Parent()->ChipProperties();
    const PipelineBinary*const pTable   = GetRpmComputePipelineTable(properties);

    if (pTable == nullptr)
    {
        PAL_NOT_IMPLEMENTED();
        return Result::ErrorUnknown;
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ClearBuffer);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ClearImage96Bpp);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ClearImage);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ClearImageMsaaPlanar);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ClearImageMsaaSampleMajor);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyBufferByte);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyBufferDqword);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyBufferDword);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dMorton2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dMorton4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dMorton8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dms2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dms4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dms8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImage2dShaderMipLevel);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImageGammaCorrect2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem1d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem2dms2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem2dms4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem2dms8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyImgToMem3d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg1d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg2dms2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg2dms4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg2dms8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyMemToImg3d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyTypedBuffer1d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyTypedBuffer2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::CopyTypedBuffer3d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ExpandMaskRam);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ExpandMaskRamMs2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ExpandMaskRamMs4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ExpandMaskRamMs8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::FastDepthClear);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::FastDepthExpClear);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::FastDepthStExpClear);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::FillMem4xDword);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        pipeInfo.interleaveSize = DispatchInterleaveSize::Disable;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::FillMemDword);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        pipeInfo.interleaveSize = DispatchInterleaveSize::Disable;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::GenerateMipmaps);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::GenerateMipmapsLowp);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::HtileCopyAndFixUp);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::HtileSR4xUpdate);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::HtileSRUpdate);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskCopyImage);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskCopyImageOptimized);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskCopyImgToMem);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskExpand2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskExpand4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskExpand8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve1xEqaa);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2xEqaa);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2xEqaaMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2xEqaaMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve2xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4xEqaa);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4xEqaaMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4xEqaaMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve4xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8xEqaa);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8xEqaaMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8xEqaaMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskResolve8xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaFmaskScaledCopy);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve2xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve2xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve4xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve4xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve8xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolve8xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil2xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil2xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil4xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil4xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil8xMax);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaResolveStencil8xMin);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::MsaaScaledCopyImage2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ResolveOcclusionQuery);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ResolvePipelineStatsQuery);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ResolveStreamoutStatsQuery);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::RgbToYuvPacked);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::RgbToYuvPlanar);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyImage2d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyImage2dMorton2x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyImage2dMorton4x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyImage2dMorton8x);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyImage3d);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::ScaledCopyTypedBufferToImg2D);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::YuvIntToRgb);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if (result == Result::Success)
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::YuvToRgb);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx9EchoGlobalTable);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        pipeInfo.interleaveSize = DispatchInterleaveSize::Disable;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10BuildDccLookupTable);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixel);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixelMsaa);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10GenerateCmdDispatch);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10GenerateCmdDispatchTaskMesh);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_1)
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10GenerateCmdDraw);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10GfxDccToDisplayDcc);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapDecode);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapEncode);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10PrtPlusResolveSamplingStatusMap);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp10_3)
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx10VrsHtile);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    if ((result == Result::Success) && (false
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_0)
#if PAL_BUILD_STRIX1
        || (properties.gfxLevel == GfxIpLevel::GfxIp11_5)
#endif
        ))
    {
        constexpr uint32 Index = uint32(RpmComputePipeline::Gfx11GenerateCmdDispatchTaskMesh);

        ComputePipelineCreateInfo pipeInfo = { };
        pipeInfo.pPipelineBinary           = pTable[Index].pBuffer;
        pipeInfo.pipelineBinarySize        = pTable[Index].size;

        result = pDevice->CreateComputePipelineInternal(pipeInfo, &pPipelineMem[Index], AllocInternal);
    }

    return result;
}

} // Pal
