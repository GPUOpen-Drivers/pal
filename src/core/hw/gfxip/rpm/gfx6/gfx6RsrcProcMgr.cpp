/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/platform.h"
#include "core/g_palSettings.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/msaaState.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6ColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6DepthStencilView.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6FormatInfo.h"
#include "core/hw/gfxip/gfx6/gfx6GraphicsPipeline.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx6/gfx6UniversalCmdBuffer.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "core/hw/gfxip/rpm/gfx6/gfx6RsrcProcMgr.h"
#include "palAutoBuffer.h"

#include <float.h>

using namespace Pal::Formats::Gfx6;
using namespace Util;

namespace Pal
{
namespace Gfx6
{

// Maps export formats to graphics state enum offsets. The offsets are relative to RpmGfxPipeline::Copy_32ABGR and
// RpmGfxPipeline::SlowColorClear(X)_32ABGR. The offset -1 indicates that there is no pipeline for a given format.
constexpr int32 ExportStateMapping[] =
{
    -1, // SPI_SHADER_ZERO is not supported.
    static_cast<int32>(Copy_32R)     - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_32GR)    - static_cast<int32>(Copy_32ABGR),
    -1, // SPI_SHADER_32_AR is not supported.
    static_cast<int32>(Copy_FP16)    - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_UNORM16) - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_SNORM16) - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_UINT16)  - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_SINT16)  - static_cast<int32>(Copy_32ABGR),
    static_cast<int32>(Copy_32ABGR)  - static_cast<int32>(Copy_32ABGR),
};

// Array of fully expanded FMASK values, arranged by [Log2(#fragments)][Log2(#samples)].
constexpr uint64 FmaskExpandedValues[MaxLog2AaFragments+1][MaxLog2AaSamples+1] =
{
    // Fragment counts down the right, sample counts along the top. Note: 1 fragment/1 sample is invalid.
    // 1    2    4     8           16
    {  0x0, 0x2, 0xE,  0xFE,       0xFFFE             }, // 1
    {  0x0, 0x2, 0xA4, 0xAAA4,     0xAAAAAAA4         }, // 2
    {  0x0, 0x0, 0xE4, 0x44443210, 0x4444444444443210 }, // 4
    {  0x0, 0x0, 0x0,  0x76543210, 0x8888888876543210 }  // 8
};

// Constants that hint which raster config register is modified.
constexpr uint32 PaScRasterConfigModifiedMask  = 0x1;
constexpr uint32 PaScRasterConfig1ModifiedMask = 0x2;

// The resolve query shaders have their own control flags that are based on QueryResultFlags.
union ResolveQueryControl
{
    struct
    {
        uint32 resultsAre64Bit   : 1;
        uint32 availability      : 1;
        uint32 partialResults    : 1;
        uint32 accumulateResults : 1;
        uint32 booleanResults    : 1;
        uint32 noWait            : 1;  // This should always be set to 1 on GFX6. Only GFX9 supports shader-based wait.
        uint32 reserved          : 26;
    };
    uint32     value;
};

// =====================================================================================================================
// For subresources below a certain size threshold in a depth/stencil target layout,
// we should prefer a graphics-based fast depth/stencil clear to minimize the
// synchronization overhead of switching between compute and graphics.
// For small surfaces, the synchronization overhead becomes a bottleneck,
// while for large surfaces that overhead is worth it for the extra clear throughput.
static bool PreferFastDepthStencilClearGraphics(
    const GfxImage& dstImage,
    ImageLayout     depthLayout,
    ImageLayout     stencilLayout)
{
    bool         preferGraphics = false;
    const Image& gfx6Image      = static_cast<const Image&>(dstImage);
    const auto&  createInfo     = gfx6Image.Parent()->GetImageCreateInfo();
    const uint32 sampleCount    = createInfo.samples;
    const uint32 imagePixelSize = createInfo.extent.width *
                                  createInfo.extent.height *
                                  createInfo.extent.depth;
    // According to the experiment at the Fiji, compute and graphics clear has a
    // performance critical point, the critical value is 2048*2048 image size for
    // 4X and 8X image, and 1024*2048 image size for single sample image and 2X.
    const uint32 imagePixelCriticalSize = (sampleCount > 2)? 2048 * 2048 : 1024 * 2048;

    if (TestAnyFlagSet(depthLayout.usages, Pal::LayoutDepthStencilTarget) ||
        TestAnyFlagSet(stencilLayout.usages, Pal::LayoutDepthStencilTarget))
    {
        preferGraphics = (imagePixelSize <= imagePixelCriticalSize);
    }

    return preferGraphics;
}

// =====================================================================================================================
RsrcProcMgr::RsrcProcMgr(
    Device* pDevice)
    :
    Pal::RsrcProcMgr(pDevice),
    m_pDevice(pDevice),
    m_cmdUtil(pDevice->CmdUtil())
{
}

// CompSetting is a "helper" enum used in the CB's algorithm for deriving an ideal SPI_SHADER_EX_FORMAT
enum class CompSetting : uint32
{
    Invalid,
    OneCompRed,
    OneCompAlpha,
    TwoCompAlphaRed,
    TwoCompGreenRed,
};

// =====================================================================================================================
// This method implements the helper function called CompSetting() for the shader export mode derivation algorithm
static CompSetting ComputeCompSetting(
    const MergedFmtInfo* pFmtInfo,
    SwizzledFormat       format)
{
    CompSetting compSetting = CompSetting::Invalid;

    const SurfaceSwap surfSwap = ColorCompSwap(format);

    switch (HwColorFmt(pFmtInfo, format.format))
    {
    case COLOR_8:
    case COLOR_16:
    case COLOR_32:
        if (surfSwap == SWAP_STD)
        {
            compSetting = CompSetting::OneCompRed;
        }
        else if (surfSwap == SWAP_ALT_REV)
        {
            compSetting = CompSetting::OneCompAlpha;
        }
        break;
    case COLOR_8_8:
    case COLOR_16_16:
    case COLOR_32_32:
        if ((surfSwap == SWAP_STD) || (surfSwap == SWAP_STD_REV))
        {
            compSetting = CompSetting::TwoCompGreenRed;
        }
        else if ((surfSwap == SWAP_ALT) || (surfSwap == SWAP_ALT_REV))
        {
            compSetting = CompSetting::TwoCompAlphaRed;
        }
        break;
    default:
        compSetting = CompSetting::Invalid;
        break;
    }

    return compSetting;
}

// =====================================================================================================================
// Derives the hardware pixel shader export format for a particular RT view slot.
//
const SPI_SHADER_EX_FORMAT RsrcProcMgr::DeterminePsExportFmt(
    SwizzledFormat format,
    bool           blendEnabled,
    bool           shaderExportsAlpha,
    bool           blendSrcAlphaToColor,
    bool           enableAlphaToCoverage
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const bool waCbNoLt16BitIntClamp   = m_pDevice->WaCbNoLt16BitIntClamp();

    const MergedFmtInfo*const pFmtInfo = MergedChannelFmtInfoTbl(chipProps.gfxLevel);

    const bool isUnorm = Formats::IsUnorm(format.format);
    const bool isSnorm = Formats::IsSnorm(format.format);
    const bool isFloat = Formats::IsFloat(format.format);
    const bool isUint  = Formats::IsUint(format.format);
    const bool isSint  = Formats::IsSint(format.format);
    const bool isSrgb  = Formats::IsSrgb(format.format);

    const uint32 maxCompSize = Formats::MaxComponentBitCount(format.format);
    const bool  alphaExport  = (shaderExportsAlpha &&
                                (Formats::HasAlpha(format) || blendSrcAlphaToColor || enableAlphaToCoverage));
    const bool  isDepth      = ((HwColorFmt(pFmtInfo, format.format) == COLOR_8_24) ||
                                (HwColorFmt(pFmtInfo, format.format) == COLOR_24_8) ||
                                (HwColorFmt(pFmtInfo, format.format) == COLOR_X24_8_32_FLOAT));

    const CompSetting compSetting = ComputeCompSetting(pFmtInfo, format);

    // Start by assuming SPI_FORMAT_ZERO (no exports).
    SPI_SHADER_EX_FORMAT spiShaderExFormat = SPI_SHADER_ZERO;

    if ((compSetting == CompSetting::OneCompRed) &&
        (alphaExport == false)                   &&
        (isSrgb == false)                        &&
        ((chipProps.gfx6.rbPlus == 0) || (maxCompSize == 32)))
    {
        // When RBPlus is enabled, R8-UNORM and R16 UNORM shouldn't use SPI_SHADER_32_R, instead SPI_SHADER_FP16_ABGR
        // and SPI_SHADER_UNORM16_ABGR should be used for 2X exporting performance.
        // This setting is invalid in some cases when CB_COLOR_CONTROL.DEGAMMA_ENABLE is set, but PAL never uses that
        // legacy bit.
        spiShaderExFormat = SPI_SHADER_32_R;
    }
    else if (((isUnorm || isSnorm) && (maxCompSize <= 10)) ||
             ((isFloat           ) && (maxCompSize <= 16)) ||
             ((isSrgb            ) && (maxCompSize == 8)))
    {
        spiShaderExFormat = SPI_SHADER_FP16_ABGR;
    }
    else if (isSint &&
             ((maxCompSize == 16) || ((waCbNoLt16BitIntClamp == false) && (maxCompSize < 16))) &&
             (enableAlphaToCoverage == false))
    {
        // NOTE: On some hardware, the CB will not properly clamp its input if the shader export format is UINT16
        // SINT16 and the CB format is less than 16 bits per channel. On such hardware, the workaround is picking
        // an appropriate 32-bit export format. If this workaround isn't necessary (cbNoLt16BitIntClamp == 0),
        // then we can choose this higher performance 16-bit export format in this case.
        spiShaderExFormat = SPI_SHADER_SINT16_ABGR;
    }
    else if (isSnorm && (maxCompSize == 16) && (blendEnabled == false))
    {
        spiShaderExFormat = SPI_SHADER_SNORM16_ABGR;
    }
    else if (isUint &&
             ((maxCompSize == 16) || ((waCbNoLt16BitIntClamp == false) && (maxCompSize < 16))) &&
             (enableAlphaToCoverage == false))
    {
        // NOTE: On some hardware, the CB will not properly clamp its input if the shader export format is UINT16/
        // SINT16 and the CB format is less than 16 bits per channel. On such hardware, the workaround is picking
        // an appropriate 32-bit export format. If this workaround isn't necessary (cbNoLt16BitIntClamp == 0),
        // then we can choose this higher performance 16-bit export format in this case.
        spiShaderExFormat = SPI_SHADER_UINT16_ABGR;
    }
    else if (isUnorm && (maxCompSize == 16) && (blendEnabled == false))
    {
        spiShaderExFormat = SPI_SHADER_UNORM16_ABGR;
    }
    else if ((((isUint  || isSint )                       ) ||
              ((isFloat           ) && (maxCompSize >  16)) ||
              ((isUnorm || isSnorm) && (maxCompSize == 16)))  &&
             ((compSetting == CompSetting::OneCompRed) ||
              (compSetting == CompSetting::OneCompAlpha) ||
              (compSetting == CompSetting::TwoCompAlphaRed)))
    {
        spiShaderExFormat = SPI_SHADER_32_AR;
    }
    else if ((((isUint  || isSint )                       ) ||
              ((isFloat           ) && (maxCompSize >  16)) ||
              ((isUnorm || isSnorm) && (maxCompSize == 16)))  &&
             (compSetting == CompSetting::TwoCompGreenRed) && (alphaExport == false))
    {
        spiShaderExFormat = SPI_SHADER_32_GR;
    }
    else if (((isUnorm || isSnorm) && (maxCompSize == 16)) ||
             ((isUint  || isSint )                       ) ||
             ((isFloat           ) && (maxCompSize >  16)) ||
             (isDepth))
    {
        spiShaderExFormat = SPI_SHADER_32_ABGR;
    }

    PAL_ASSERT(spiShaderExFormat != SPI_SHADER_ZERO);
    return spiShaderExFormat;
}

// =====================================================================================================================
// Some blts need to use GFXIP-specific algorithms to pick the proper graphics state. The basePipeline is the first
// graphics state in a series of states that vary only on target format and target index.
const Pal::GraphicsPipeline* RsrcProcMgr::GetGfxPipelineByTargetIndexAndFormat(
    RpmGfxPipeline basePipeline,
    uint32         targetIndex,
    SwizzledFormat format
    ) const
{
    // There are only two ranges of pipelines that vary by export format and these are their bases.
    PAL_ASSERT((basePipeline == Copy_32ABGR) || (basePipeline == SlowColorClear0_32ABGR));

    // Note: Nonzero targetIndex has not been support for Copy_32R yet!
    PAL_ASSERT((basePipeline == SlowColorClear0_32ABGR) || (targetIndex == 0));

    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    const int32 pipelineOffset = ExportStateMapping[exportFormat];
    PAL_ASSERT(pipelineOffset >= 0);

    return GetGfxPipeline(static_cast<RpmGfxPipeline>(basePipeline + pipelineOffset + targetIndex * NumExportFormats));
}

// =====================================================================================================================
const Pal::ComputePipeline* RsrcProcMgr::GetCmdGenerationPipeline(
    const Pal::IndirectCmdGenerator& generator,
    const CmdBuffer&                 cmdBuffer
    ) const
{
    RpmComputePipeline pipeline = RpmComputePipeline::Count;

    switch (generator.Type())
    {
    case GeneratorType::Draw:
    case GeneratorType::DrawIndexed:
        PAL_ASSERT(cmdBuffer.GetEngineType() == EngineTypeUniversal);
        pipeline = RpmComputePipeline::Gfx6GenerateCmdDraw;
        break;

    case GeneratorType::Dispatch:
        pipeline = RpmComputePipeline::Gfx6GenerateCmdDispatch;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return GetPipeline(pipeline);
}

// =====================================================================================================================
// Clones the image data from the source image while preserving its state and avoiding decompressing.
void RsrcProcMgr::CmdCloneImageData(
    GfxCmdBuffer* pCmdBuffer,
    const Image&  srcImage,
    const Image&  dstImage
    ) const
{
    const Pal::Image& srcParent = *srcImage.Parent();
    const Pal::Image& dstParent = *dstImage.Parent();

    // Check our assumptions:
    // 1. Since the source image can be in any state we need a universal command buffer.
    // 2. Both images need to be cloneable.
    // 3. Both images must have been created with identical create info.
    PAL_ASSERT(pCmdBuffer->GetEngineType() == EngineTypeUniversal);
    PAL_ASSERT(srcParent.IsCloneable() && dstParent.IsCloneable());
    PAL_ASSERT(memcmp(&srcParent.GetImageCreateInfo(), &dstParent.GetImageCreateInfo(), sizeof(ImageCreateInfo)) == 0);

    auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::CpDma);
    PAL_ASSERT(pCmdStream != nullptr);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    // Construct a DMA_DATA packet to copy all of the source image (including metadata) to the destination image.
    const gpusize srcBaseAddr = srcParent.GetGpuVirtualAddr();
    const gpusize srcBaseSize = srcParent.GetGpuMemSize();
    const gpusize dstBaseAddr = dstParent.GetGpuVirtualAddr();
    const gpusize dstBaseSize = dstParent.GetGpuMemSize();
    PAL_ASSERT((srcBaseSize == dstBaseSize) && (HighPart(srcBaseSize) == 0));

    // We want to read and write through L2 because it's faster and expected by CoherCopy but if it isn't supported
    // we need to fall back to a memory-to-memory copy.
    const bool supportsL2 = (m_pDevice->Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6);

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel   = supportsL2 ? CPDMA_DST_SEL_DST_ADDR_USING_L2 : CPDMA_DST_SEL_DST_ADDR;
    dmaDataInfo.srcSel   = supportsL2 ? CPDMA_SRC_SEL_SRC_ADDR_USING_L2 : CPDMA_SRC_SEL_SRC_ADDR;
    dmaDataInfo.dstAddr  = dstBaseAddr;
    dmaDataInfo.srcAddr  = srcBaseAddr;
    dmaDataInfo.numBytes = LowPart(srcBaseSize);
    dmaDataInfo.sync     = false;
    dmaDataInfo.usePfp   = false;

    pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);

    pCmdBuffer->SetGfxCmdBufCpBltState(true);

    if (supportsL2)
    {
        pCmdBuffer->SetGfxCmdBufCpBltWriteCacheState(true);
    }
    else
    {
        pCmdBuffer->SetGfxCmdBufCpMemoryWriteL2CacheStaleState(true);
    }
}

// =====================================================================================================================
// Adds commands to pCmdBuffer to copy data between srcGpuMemory and dstGpuMemory. Note that this function requires a
// command buffer that supports CP DMA workloads.
void RsrcProcMgr::CmdCopyMemory(
    GfxCmdBuffer*           pCmdBuffer,
    const GpuMemory&        srcGpuMemory,
    const GpuMemory&        dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions
    ) const
{
    if (srcGpuMemory.IsVirtual())
    {
        // CP DMA will not read zero from unmapped virtual memory. We need to use CS copy to achieve this.
        CopyMemoryCs(pCmdBuffer, srcGpuMemory, dstGpuMemory, regionCount, pRegions);
    }
    else
    {
        // In practice, most copy ranges are smaller than maxCpDmaSize which means we will use CP DMA to copy them.
        // Prepare some state up-front which will be reused each time we build a DMA DATA packet.
        const gpusize maxCpDmaSize = m_pDevice->Parent()->GetPublicSettings()->cpDmaCmdCopyMemoryMaxBytes;

        // If the caller gives us any ranges bigger than maxCpDmaSize we must copy them using CopyMemoryCs later on.
        bool hasBigCopyRegions = false;

        for (uint32 i = 0; i < regionCount; i++)
        {
            if (pRegions[i].copySize > maxCpDmaSize)
            {
                // We will copy this region later on.
                hasBigCopyRegions = true;
            }
            else
            {
                const gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + pRegions[i].dstOffset;
                const gpusize srcAddr = srcGpuMemory.Desc().gpuVirtAddr + pRegions[i].srcOffset;

                pCmdBuffer->CpCopyMemory(dstAddr, srcAddr, pRegions[i].copySize);
            }
        }

        if (hasBigCopyRegions)
        {
            // Copy the big regions into a new AutoBuffer of regions, this was written assuming that the CPU/GPU
            // overhead of individual calls to CopyMemoryCs outweights the CPU overhead of creating and filling the
            // AutoBuffer.
            uint32 bigRegionCount = 0;
            AutoBuffer<MemoryCopyRegion, 32, Platform> bigRegions(regionCount, m_pDevice->GetPlatform());

            if (bigRegions.Capacity() < regionCount)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                for (uint32 i = 0; i < regionCount; i++)
                {
                    if (pRegions[i].copySize > maxCpDmaSize)
                    {
                        memcpy(&bigRegions[bigRegionCount], &pRegions[i], sizeof(pRegions[i]));
                        bigRegionCount++;
                    }
                }

                PAL_ASSERT(bigRegionCount > 0);
                CopyMemoryCs(pCmdBuffer, srcGpuMemory, dstGpuMemory, bigRegionCount, &bigRegions[0]);
            }
        }
    }
}

// =====================================================================================================================
// Adds commands to pCmdBuffer to copy the provided data into the specified GPU memory location. Note that this
// function requires a command buffer that supports CP DMA workloads.
void RsrcProcMgr::CmdUpdateMemory(
    GfxCmdBuffer*    pCmdBuffer,
    const GpuMemory& dstMem,
    gpusize          dstOffset,  // Byte offset within the memory to copy the data
    gpusize          dataSize,   // Size (in bytes) of the provided data
    const uint32*    pData
    ) const
{
    // Verify the command buffer supports the CPDMA engine.
    auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::CpDma);
    PAL_ASSERT(pStream != nullptr);

    // Prepare to issue one or more DMA_DATA packets. Start the dstAddr at the beginning of the dst buffer.
    // The srcAddr and numBytes will be set in the loop.
    //
    // We want to read and write through L2 because it's faster and expected by CoherCopy but if it isn't supported
    // we need to fall back to a memory-to-memory copy.
    const bool supportsL2 = (m_pDevice->Parent()->ChipProperties().gfxLevel > GfxIpLevel::GfxIp6);

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel   = supportsL2 ? CPDMA_DST_SEL_DST_ADDR_USING_L2 : CPDMA_DST_SEL_DST_ADDR;
    dmaDataInfo.srcSel   = supportsL2 ? CPDMA_SRC_SEL_SRC_ADDR_USING_L2 : CPDMA_SRC_SEL_SRC_ADDR;
    dmaDataInfo.dstAddr = dstMem.Desc().gpuVirtAddr + dstOffset;
    dmaDataInfo.sync    = false;
    dmaDataInfo.usePfp  = false;

    const uint32 embeddedDataLimit = pCmdBuffer->GetEmbeddedDataLimit() * sizeof(uint32);
    const uint32 embeddedDataAlign = m_pDevice->Settings().cpDmaSrcAlignment / sizeof(uint32);

    // Loop until we've submitted enough DMA_DATA packets to upload the whole src buffer.
    const void* pRemainingSrcData = pData;
    uint32      remainingDataSize = static_cast<uint32>(dataSize);
    while (remainingDataSize > 0)
    {
        // Create the embedded video memory space for the next section of the src buffer.
        dmaDataInfo.numBytes = Min(remainingDataSize, embeddedDataLimit);

        uint32* pBufStart = pCmdBuffer->CmdAllocateEmbeddedData(dmaDataInfo.numBytes / sizeof(uint32),
                                                                embeddedDataAlign,
                                                                &dmaDataInfo.srcAddr);

        memcpy(pBufStart, pRemainingSrcData, dmaDataInfo.numBytes);

        // Write the DMA_DATA packet to the command stream.
        uint32* pCmdSpace = pStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
        pStream->CommitCommands(pCmdSpace);

        // Update all variable addresses and sizes except for srcAddr and numBytes which will be reset above.
        pRemainingSrcData    = VoidPtrInc(pRemainingSrcData, dmaDataInfo.numBytes);
        remainingDataSize   -= dmaDataInfo.numBytes;
        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
    }

    pCmdBuffer->SetGfxCmdBufCpBltState(true);

    if (supportsL2)
    {
        pCmdBuffer->SetGfxCmdBufCpBltWriteCacheState(true);
    }
    else
    {
        pCmdBuffer->SetGfxCmdBufCpMemoryWriteL2CacheStaleState(true);
    }
}

// =====================================================================================================================
// Adds commands to pCmdBuffer to resolve a range of query slots in a query pool to the given GPU memory location.
void RsrcProcMgr::CmdResolveQuery(
    GfxCmdBuffer*    pCmdBuffer,
    const QueryPool& queryPool,
    QueryResultFlags flags,
    QueryType        queryType,
    uint32           startQuery,
    uint32           queryCount,
    const GpuMemory& dstGpuMemory,
    gpusize          dstOffset,
    gpusize          dstStride
    ) const
{
    constexpr uint32 OptCaseWait64      = QueryResult64Bit | QueryResultWait;
    constexpr uint32 OptCaseWait64Accum = QueryResult64Bit | QueryResultWait | QueryResultAccumulate;

    // We can only use the cp packet to do the query resolve in graphics queue also it needs to be an occlusion query
    // with the two flags set. OCCLUSION_QUERY packet resolves a single occlusion query slot.
    if (Pal::Image::UseCpPacketOcclusionQuery                              &&
        // BinaryOcclusion might also go inside this path but CP cannot handle that.
        (queryType == QueryType::Occlusion)                                &&
        (pCmdBuffer->GetEngineType() == EngineTypeUniversal)               &&
        ((flags == OptCaseWait64) || (flags == OptCaseWait64Accum)))
    {
        auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
        PAL_ASSERT(pStream != nullptr);

        uint32*      pCmdSpace         = nullptr;
        uint32       remainingResolves = queryCount;
        const bool   doAccumulate      = TestAnyFlagSet(flags, QueryResultAccumulate);
        uint32       queryIndex        = 0;

        if (doAccumulate == false)
        {
            // We are using PFP WriteData to zero out the memory so it will not accumulate. We need to make sure
            // PFP is not running ahead of previous commands.
            pCmdSpace = pStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
            pStream->CommitCommands(pCmdSpace);
        }

        // If QueryResultAccumulate is not set, we need to write the result to 0 first.
        const uint64 zero              = 0;
        const uint32 writeDataSize     = NumBytesToNumDwords(sizeof(zero));
        const uint32 writeDataPktSize  = CmdUtil::GetWriteDataHeaderSize() + writeDataSize;

        const uint32 resolvePerCommit  =
            doAccumulate
            ? pStream->ReserveLimit() / CmdUtil::GetOcclusionQuerySize()
            : pStream->ReserveLimit() / (CmdUtil::GetOcclusionQuerySize() + writeDataPktSize);

        while (remainingResolves > 0)
        {
            // Write all of the queries or as many queries as we can fit in a reserve buffer.
            uint32 resolvesToWrite = Min(remainingResolves, resolvePerCommit);

            pCmdSpace          = pStream->ReserveCommands();
            remainingResolves -= resolvesToWrite;

            while (resolvesToWrite-- > 0)
            {
                gpusize queryPoolAddr  = 0;
                gpusize resolveDstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset + queryIndex * dstStride;
                Result  result         = queryPool.GetQueryGpuAddress(queryIndex + startQuery, &queryPoolAddr);

                PAL_ASSERT(result == Result::Success);

                if(result == Result::Success)
                {
                    if (doAccumulate == false)
                    {
                        pCmdSpace += m_cmdUtil.BuildWriteData(resolveDstAddr,
                                                              writeDataSize,
                                                              WRITE_DATA_ENGINE_PFP,
                                                              WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                              true,
                                                              reinterpret_cast<const uint32*>(&zero),
                                                              PredDisable,
                                                              pCmdSpace);
                    }

                    pCmdSpace += m_cmdUtil.BuildOcclusionQuery(queryPoolAddr,
                                                               resolveDstAddr,
                                                               pCmdSpace);
                }
                queryIndex++;
            }
            pStream->CommitCommands(pCmdSpace);
        }
    }
    else
    {
        CmdResolveQueryComputeShader(pCmdBuffer,
                                     queryPool,
                                     flags,
                                     queryType,
                                     startQuery,
                                     queryCount,
                                     dstGpuMemory,
                                     dstOffset,
                                     dstStride);
    }
}

// =====================================================================================================================
// Resolve the query with compute shader.
void RsrcProcMgr::CmdResolveQueryComputeShader(
    GfxCmdBuffer*         pCmdBuffer,
    const Pal::QueryPool& queryPool,
    QueryResultFlags      flags,
    QueryType             queryType,
    uint32                startQuery,
    uint32                queryCount,
    const GpuMemory&      dstGpuMemory,
    gpusize               dstOffset,
    gpusize               dstStride
    ) const
{
    auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
    PAL_ASSERT(pStream != nullptr);

    // We want to use the uncached MTYPE to read the query data directly from memory, but only GFX8+ supports this
    // MTYPE. In testing, GFX7 does not appear to support MTYPE_UC properly, even though has some MTYPE support.
    const bool supportsUncached = (m_pDevice->Parent()->ChipProperties().gfxLevel >= GfxIpLevel::GfxIp8);

    if (TestAnyFlagSet(flags, QueryResultWait) && queryPool.HasTimestamps())
    {
        // Wait for the query data to get to memory if it was requested.
        // The shader is required to implement the wait if the query pool doesn't have timestamps.
        queryPool.WaitForSlots(pStream, startQuery, queryCount);
    }

    if (!supportsUncached)
    {
        // Invalidate the L2 if we can't skip it using the uncached MTYPE because it might contain stale source data
        // from a previous resolve. We have to do this in RPM because we do not require barriers for "normal" PAL
        // objects like IQueryPool.
        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All = CP_COHER_CNTL__TC_ACTION_ENA_MASK;

        gpusize startAddr = 0;
        Result  result    = queryPool.GetQueryGpuAddress(startQuery, &startAddr);
        PAL_ASSERT(result == Result::Success);

        uint32* pCmdSpace = pStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                                SURFACE_SYNC_ENGINE_ME,
                                                startAddr,
                                                queryPool.GetGpuResultSizeInBytes(queryCount),
                                                pCmdBuffer->GetEngineType() == EngineTypeCompute,
                                                pCmdSpace);
        pStream->CommitCommands(pCmdSpace);
    }

    // It should be safe to launch our compute shader now.
    // Select the correct pipeline and pipeline-specific constant buffer data.
    const ComputePipeline* pPipeline    = nullptr;
    uint32                 pipelineData = 0;

    // Translate the result flags and query type into the flags that the shader expects.
    ResolveQueryControl controlFlags;
    controlFlags.value             = 0;
    controlFlags.resultsAre64Bit   = TestAnyFlagSet(flags, QueryResult64Bit);
    controlFlags.availability      = TestAnyFlagSet(flags, QueryResultAvailability);
    controlFlags.partialResults    = TestAnyFlagSet(flags, QueryResultPartial);
    controlFlags.accumulateResults = TestAnyFlagSet(flags, QueryResultAccumulate);
    controlFlags.booleanResults    = (queryType == QueryType::BinaryOcclusion);
    // We should only use shader-based wait if the query pool doesn't already use timestamps.
    controlFlags.noWait            = ((TestAnyFlagSet(flags, QueryResultWait) == false) || queryPool.HasTimestamps());

    uint32 constData[4]    = { controlFlags.value, queryCount, static_cast<uint32>(dstStride), 0 };
    uint32 constEntryCount = 0;

    switch (queryPool.CreateInfo().queryPoolType)
    {
    case QueryPoolType::Occlusion:
        // The occlusion query shader needs the stride of a set of zPass counters.
        pPipeline    = GetPipeline(RpmComputePipeline::ResolveOcclusionQuery);
        pipelineData = static_cast<uint32>(queryPool.GetGpuResultSizeInBytes(1));

        constData[3]    = pipelineData;
        constEntryCount = 4;

        PAL_ASSERT((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion));
        break;

    case QueryPoolType::PipelineStats:
        // The pipeline stats query shader needs the mask of enabled pipeline stats.
        pPipeline    = GetPipeline(RpmComputePipeline::ResolvePipelineStatsQuery);
        pipelineData = queryPool.CreateInfo().enabledStats;

        constData[3]    = pipelineData;
        constEntryCount = 4;

        // Note that accumulation was not implemented for this query pool type because no clients support it.
        PAL_ASSERT(TestAnyFlagSet(flags, QueryResultAccumulate) == false);
        PAL_ASSERT(queryType == QueryType::PipelineStats);

        // Pipeline stats query doesn't implement shader-based wait.
        PAL_ASSERT(controlFlags.noWait == 1);
        break;

    case QueryPoolType::StreamoutStats:

        PAL_ASSERT(flags == (QueryResult64Bit | QueryResultWait));

        pPipeline = GetPipeline(RpmComputePipeline::ResolveStreamoutStatsQuery);

        constData[0]    = queryCount;
        constData[1]    = static_cast<uint32>(dstStride);
        constEntryCount = 2;

        PAL_ASSERT((queryType == QueryType::StreamoutStats)  ||
                   (queryType == QueryType::StreamoutStats1) ||
                   (queryType == QueryType::StreamoutStats2) ||
                   (queryType == QueryType::StreamoutStats3));

        // Streamout stats query doesn't implement shader-based wait.
        PAL_ASSERT(controlFlags.noWait == 1);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    PAL_ASSERT(pPipeline != nullptr);

    // Save current command buffer state and bind the pipeline.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

    // Create an embedded user-data table and bind it to user data 0. We need buffer views for the source and dest.
    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                               SrdDwordAlignment() * 2,
                                                               SrdDwordAlignment(),
                                                               PipelineBindPoint::Compute,
                                                               0);

    // Populate the table with raw buffer views, by convention the destination is placed before the source.
    BufferViewInfo rawBufferView = {};
    RpmUtil::BuildRawBufferViewInfo(&rawBufferView, dstGpuMemory, dstOffset);
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);
    pSrdTable += SrdDwordAlignment();

    RpmUtil::BuildRawBufferViewInfo(&rawBufferView, queryPool.GpuMemory(), queryPool.GetQueryOffset(startQuery));
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &rawBufferView, pSrdTable);

    if (supportsUncached)
    {
        // We need to use the uncached MTYPE to skip the L2 because the query data is written directly to memory.
        auto* pSrcSrd = reinterpret_cast<BufferSrd*>(pSrdTable);
        pSrcSrd->word3.bits.MTYPE__CI__VI = MTYPE_UC;
    }

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, constEntryCount, constData);

    // Issue a dispatch with one thread per query slot.
    const uint32 threadGroups = RpmUtil::MinThreadGroups(queryCount, pPipeline->ThreadsPerGroup());
    pCmdBuffer->CmdDispatch(threadGroups, 1, 1);

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Function to expand (decompress) hTile data associated with the given image / range.  Supports use of a compute
// queue expand for ASICs that support texture compatability of depth surfaces.  Falls back to the independent layer
// implementation for other ASICs
void RsrcProcMgr::ExpandDepthStencil(
    GfxCmdBuffer*                pCmdBuffer,
    const Pal::Image&            image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    const auto&  device              = *m_pDevice->Parent();
    const auto&  settings            = m_pDevice->Settings();
    const auto*  pGfxImage           = reinterpret_cast<const Image*>(image.GetGfxImage());
    const bool   supportsComputePath = pGfxImage->SupportsComputeDecompress(range.startSubres);

    // Make sure we support compute decompress if we're here on a compute queue.
    PAL_ASSERT(supportsComputePath || (pCmdBuffer->GetEngineType() == EngineTypeUniversal));

    // To do a compute expand, we need to either
    //   a) Be on the compute queue.  In this case we can't do a gfx decompress because it'll hang.
    //   b) Have a compute-capable image and- have the "compute" path forced through settings.
    if ((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
        (supportsComputePath && (TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways))))
    {
        const auto&  createInfo        = image.GetImageCreateInfo();
        const auto*  pPipeline         = GetComputeMaskRamExpandPipeline(image);
        const auto*  pHtile            = pGfxImage->GetHtile(range.startSubres);
        auto*        pComputeCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);

        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

        // Compute the number of thread groups needed to launch one thread per texel.
        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        bool         earlyExit      = false;
        SubresRange  remainingRange = range;
        for (uint32  mipIdx = 0; ((earlyExit == false) && (mipIdx < range.numMips)); mipIdx++)
        {
            const SubresId  mipBaseSubResId =  { range.startSubres.aspect, range.startSubres.mipLevel + mipIdx, 0 };
            const auto*     pBaseSubResInfo = image.SubresourceInfo(mipBaseSubResId);

            PAL_ASSERT(pBaseSubResInfo->flags.supportMetaDataTexFetch);

            const uint32  threadGroupsX = RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.width,
                                                                   threadsPerGroup[0]);
            const uint32  threadGroupsY = RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.height,
                                                                   threadsPerGroup[1]);

            const uint32 constData[] =
            {
                // start cb0[0]
                pBaseSubResInfo->extentElements.width,
                pBaseSubResInfo->extentElements.height,
            };

            const uint32 sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));

            for (uint32  sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
            {
                const SubresId     subResId =  { mipBaseSubResId.aspect,
                                                 mipBaseSubResId.mipLevel,
                                                 range.startSubres.arraySlice + sliceIdx };
                const SubresRange  viewRange = { subResId, 1, 1 };

                // Create an embedded user-data table and bind it to user data 0. We will need two views.
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
                                        pCmdBuffer,
                                        2 * SrdDwordAlignment() + sizeConstDataDwords,
                                        SrdDwordAlignment(),
                                        PipelineBindPoint::Compute,
                                        0);

                ImageViewInfo imageView[2] = {};
                RpmUtil::BuildImageViewInfo(
                    &imageView[0], image, viewRange, createInfo.swizzledFormat, false, device.TexOptLevel()); // src
                RpmUtil::BuildImageViewInfo(
                    &imageView[1], image, viewRange, createInfo.swizzledFormat, true, device.TexOptLevel());  // dst
                device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

                pSrdTable += 2 * SrdDwordAlignment();
                memcpy(pSrdTable, constData, sizeof(constData));

                // Execute the dispatch.
                pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
            } // end loop through all the slices
        } // end loop through all the mip levels

        // Allow the rewrite of depth data to complete
        uint32* pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
        pComputeCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pComputeCmdSpace);
        pComputeCmdStream->CommitCommands(pComputeCmdSpace);

        // Mark all the hTile data as fully expanded
        ClearHtile(pCmdBuffer, *pGfxImage, range, pHtile->GetInitialValue());

        // And wait for that to finish...
        pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
        pComputeCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pComputeCmdSpace);
        pComputeCmdStream->CommitCommands(pComputeCmdSpace);

        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
    else
    {
        // Do the expand the legacy way.
        Pal::RsrcProcMgr::ExpandDepthStencil(pCmdBuffer, image, pMsaaState, pQuadSamplePattern, range);
    }
}

// =====================================================================================================================
// Performs a fast-clear on a Color Target Image by updating the Image's CMask buffer and/or DCC buffer.
void RsrcProcMgr::HwlFastColorClear(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    dstImage,
    const uint32*      pConvertedColor,
    const SubresRange& clearRange
    ) const
{
    const Image& gfx6Image = static_cast<const Image&>(dstImage);

    CmdStream* const pStream = static_cast<CmdStream*>(pCmdBuffer->GetCmdStreamByEngine(
        CmdBufferEngineSupport::Compute));

    PAL_ASSERT(pStream != nullptr);

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
        pCmdBuffer->GetGfxCmdBufState().packetPredicate);

    if (gfx6Image.HasCmaskData())
    {
        // Clear the Image's CMask sub-allocation(s) to indicate the fast-cleared state.
        ClearCmask(pCmdBuffer, gfx6Image, clearRange, Gfx6Cmask::GetFastClearCode(gfx6Image));
    }

    if (gfx6Image.HasDccData())
    {
        bool fastClearElimRequired = false;
        const uint32 fastClearCode =
            Gfx6Dcc::GetFastClearCode(gfx6Image, clearRange, pConvertedColor, &fastClearElimRequired);

        if (gfx6Image.GetFastClearEliminateMetaDataAddr(0) != 0)
        {
            // Update the image's FCE meta-data.
            uint32* pCmdSpace = pStream->ReserveCommands();
            pCmdSpace = gfx6Image.UpdateFastClearEliminateMetaData(clearRange,
                                                                   fastClearElimRequired,
                                                                   packetPredicate,
                                                                   pCmdSpace);
            pStream->CommitCommands(pCmdSpace);
        }

        ClearDcc(pCmdBuffer, pStream, gfx6Image, clearRange, fastClearCode, DccClearPurpose::FastClear);
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    const SwizzledFormat aspectFormat      = dstImage.Parent()->SubresourceInfo(clearRange.startSubres)->format;
    uint32               swizzledColor[4]  = {};
    Formats::SwizzleColor(aspectFormat, pConvertedColor, &swizzledColor[0]);

    uint32 packedColor[4] = {};
    Formats::PackRawClearColor(aspectFormat, swizzledColor, &packedColor[0]);

    // Finally, tell the Image to issue commands which update its fast-clear meta-data.
    uint32* pCmdSpace = pStream->ReserveCommands();

    pCmdSpace = gfx6Image.UpdateColorClearMetaData(clearRange.startSubres.mipLevel,
                                                   clearRange.numMips,
                                                   packedColor,
                                                   packetPredicate,
                                                   pCmdSpace);

    // In case the cleared image is already bound as a color target, we need to update the color clear value
    // registers to the newly-cleared values.
    if (pCmdBuffer->GetEngineType() == EngineTypeUniversal)
    {
        pCmdSpace = UpdateBoundFastClearColor(pCmdBuffer,
                                              dstImage,
                                              clearRange.startSubres.mipLevel,
                                              clearRange.numMips,
                                              packedColor,
                                              pStream,
                                              pCmdSpace);
    }

    pStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// On fmask msaa copy through compute shader we do an optimization where we preserve fmask fragmentation while copying
// the data from src to dest, which means dst needs to have fmask of src and dcc needs to be set to uncompressed since
// dest color data is no longer dcc compressed after copy.
void RsrcProcMgr::HwlUpdateDstImageMetaData(
    GfxCmdBuffer*          pCmdBuffer,
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags) const
{
    // This code doesn't work correctly. Needs to be re-worked.
    PAL_ASSERT_ALWAYS();
    auto* const pStream      = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
    const auto& gfx6SrcImage = static_cast<const Gfx6::Image&>(*srcImage.GetGfxImage());
    const auto& gfx6DstImage = static_cast<const Gfx6::Image&>(*dstImage.GetGfxImage());

    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        ImageCopyRegion copyRegion = pRegions[idx];
        // Since color data is no longer dcc compressed set it to fully uncompressed.
        if (gfx6DstImage.HasDccData())
        {
            SubresRange range = {};

            range.startSubres.aspect     = copyRegion.dstSubres.aspect;
            range.startSubres.mipLevel   = copyRegion.dstSubres.mipLevel;
            range.startSubres.arraySlice = copyRegion.dstSubres.arraySlice;
            range.numMips                = 1;
            range.numSlices              = copyRegion.numSlices;
            ClearDcc(pCmdBuffer, pStream, gfx6DstImage, range, Gfx6Dcc::InitialValue, DccClearPurpose::FastClear);
        }

        // Copy the src fmask and cmask data to destination
        if (gfx6DstImage.HasFmaskData())
        {
            const auto*   pSrcCmask = gfx6SrcImage.GetCmask(copyRegion.srcSubres);
            const auto*   pSrcFmask = gfx6SrcImage.GetFmask(copyRegion.srcSubres);
            const auto*   pDstCmask = gfx6DstImage.GetCmask(copyRegion.dstSubres);
            const auto*   pDstFmask = gfx6DstImage.GetFmask(copyRegion.dstSubres);

            // Memory
            const IGpuMemory* pSrcMemory = reinterpret_cast<const IGpuMemory*>(srcImage.GetBoundGpuMemory().Memory());
            const IGpuMemory* pDstMemory = reinterpret_cast<const IGpuMemory*>(dstImage.GetBoundGpuMemory().Memory());

            MemoryCopyRegion memcpyRegion;

            memcpyRegion.srcOffset = pSrcFmask->MemoryOffset();
            memcpyRegion.dstOffset = pDstFmask->MemoryOffset();
            memcpyRegion.copySize  = pSrcFmask->TotalSize();

            // Do the copy
            pCmdBuffer->CmdCopyMemory(*pSrcMemory, *pDstMemory, 1, &memcpyRegion);

            // cmask copy
            memcpyRegion.srcOffset = pSrcCmask->MemoryOffset();
            memcpyRegion.dstOffset = pDstCmask->MemoryOffset();
            memcpyRegion.copySize  = pSrcCmask->TotalSize();

            // Do the copy
            pCmdBuffer->CmdCopyMemory(*pSrcMemory, *pDstMemory, 1, &memcpyRegion);
        }
    }
}

// =====================================================================================================================
// After a fixed-func depth/stencil copy resolve, src htile will be copied to dst htile and set the zmask or smask to
// expanded. Depth part and stencil part share same htile. So the depth part and stencil part will be merged (if
// necessary) and one cs blt will be launched for each merged region to copy and fixup the htile.
void RsrcProcMgr::HwlHtileCopyAndFixUp(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions) const
{
    PAL_ASSERT(srcImage.IsDepthStencil() && dstImage.IsDepthStencil());

    const Gfx6::Image& gfx6SrcImage = static_cast<const Gfx6::Image&>(*srcImage.GetGfxImage());
    const Gfx6::Image& gfx6DstImage = static_cast<const Gfx6::Image&>(*dstImage.GetGfxImage());

    // Merge depth and stencil regions in which htile fix up could be performed together.
    // Although depth and stencil htile fix-up could theoretically performed respectively, cs partial flush is
    // required to ensure coherency. So we perform the depth and stencil htile fix-up simultaneously.
    struct FixUpRegion
    {
        const ImageResolveRegion* pResolveRegion;
        bool resolveDepth;
        bool resolveStencil;

        void FillAspect(ImageAspect aspect)
        {
            if (aspect == ImageAspect::Depth)
            {
                PAL_ASSERT(resolveDepth == false);
                resolveDepth = true;
            }
            else if (aspect == ImageAspect::Stencil)
            {
                PAL_ASSERT(resolveStencil == false);
                resolveStencil = true;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }
        }
    };

    AutoBuffer<FixUpRegion, 2 * MaxImageMipLevels, Platform> fixUpRegionList(regionCount, m_pDevice->GetPlatform());
    uint32 mergedCount = 0u;

    if (fixUpRegionList.Capacity() < regionCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        for (uint32 regionIndex = 0; regionIndex < regionCount; ++regionIndex)
        {
            const ImageResolveRegion& curResolveRegion = pRegions[regionIndex];
            bool inserted = false;

            for (uint32 listIndex = 0; listIndex < mergedCount; ++listIndex)
            {
                FixUpRegion& listRegion = fixUpRegionList[listIndex];

                if ((curResolveRegion.dstMipLevel == listRegion.pResolveRegion->dstMipLevel) &&
                    (curResolveRegion.dstSlice == listRegion.pResolveRegion->dstSlice))
                {
                    PAL_ASSERT(curResolveRegion.srcSlice == listRegion.pResolveRegion->srcSlice);
                    PAL_ASSERT(curResolveRegion.numSlices == listRegion.pResolveRegion->numSlices);
                    PAL_ASSERT(curResolveRegion.dstAspect != listRegion.pResolveRegion->dstAspect);

                    listRegion.FillAspect(curResolveRegion.dstAspect);
                    inserted = true;
                    break;
                }
            }

            if (inserted == false)
            {
                FixUpRegion fixUpRegion = {};
                fixUpRegion.pResolveRegion = &curResolveRegion;
                fixUpRegion.FillAspect(curResolveRegion.dstAspect);

                fixUpRegionList[mergedCount++] = fixUpRegion;
            } // End of if
        } // End of for
    } // End of if

    if (gfx6SrcImage.HasHtileData() && gfx6DstImage.HasHtileData())
    {
        // Save the command buffer's state.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

        // Use the HtileCopyAndFixUp shader
        const ComputePipeline*const pPipeline = GetPipeline(RpmComputePipeline::HtileCopyAndFixUp);

        // Bind the pipeline.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

        SubresId dstSubresId = {};

        for (uint32 i = 0; i < mergedCount; ++i)
        {
            const ImageResolveRegion* pCurRegion = fixUpRegionList[i].pResolveRegion;

            uint32 dstMipLevel = pCurRegion->dstMipLevel;
            dstSubresId.aspect = pCurRegion->dstAspect;
            dstSubresId.mipLevel = dstMipLevel;
            dstSubresId.arraySlice = pCurRegion->dstSlice;
            const SubResourceInfo* pDstSubresInfo = dstImage.SubresourceInfo(dstSubresId);
            const Gfx6Htile* pDstHtile = gfx6DstImage.GetHtile(dstSubresId);

            uint32 htileMask = 0;
            uint32 htileDecompressValue = 0;

            if (fixUpRegionList[i].resolveDepth)
            {
                uint32 htileDataDepth = 0;
                uint32 htileMaskDepth = 0;

                pDstHtile->GetAspectInitialValue(ImageAspect::Depth, &htileDataDepth, &htileMaskDepth);

                htileDecompressValue |= htileDataDepth;
                htileMask |= htileMaskDepth;
            }

            if (fixUpRegionList[i].resolveStencil)
            {
                uint32 htileDataStencil = 0;
                uint32 htileMaskStencil = 0;

                pDstHtile->GetAspectInitialValue(ImageAspect::Stencil, &htileDataStencil, &htileMaskStencil);

                htileDecompressValue |= htileDataStencil;
                htileMask |= htileMaskStencil;
            }

            PAL_ASSERT(pCurRegion->srcOffset.x == pCurRegion->dstOffset.x);
            PAL_ASSERT(pCurRegion->srcOffset.y == pCurRegion->dstOffset.y);

            PAL_ASSERT(pCurRegion->dstOffset.x == 0);
            PAL_ASSERT(pCurRegion->dstOffset.y == 0);

            PAL_ASSERT(pCurRegion->extent.width == pDstSubresInfo->extentTexels.width);
            PAL_ASSERT(pCurRegion->extent.height == pDstSubresInfo->extentTexels.height);

            GpuMemory* pSrcGpuMemory = nullptr;
            gpusize    srcOffset = 0;
            gpusize    srcDataSize = 0;

            gfx6SrcImage.GetHtileBufferInfo(0,
                                            pCurRegion->srcSlice,
                                            pCurRegion->numSlices,
                                            HtileBufferUsage::Clear,
                                            &pSrcGpuMemory,
                                            &srcOffset,
                                            &srcDataSize);

            GpuMemory* pDstGpuMemory = nullptr;
            gpusize    dstOffset = 0;
            gpusize    dstDataSize = 0;

            gfx6DstImage.GetHtileBufferInfo(pCurRegion->dstMipLevel,
                                            pCurRegion->dstSlice,
                                            pCurRegion->numSlices,
                                            HtileBufferUsage::Clear,
                                            &pDstGpuMemory,
                                            &dstOffset,
                                            &dstDataSize);

            // It is expected that src htile and dst htile has exactly same layout, so dataSize shall be same at least.
            PAL_ASSERT(srcDataSize == dstDataSize);

            BufferViewInfo htileBufferView[2] = {};

            htileBufferView[0].gpuAddr = pDstGpuMemory->Desc().gpuVirtAddr + dstOffset;
            htileBufferView[0].range = dstDataSize;
            htileBufferView[0].stride = 1;
            htileBufferView[0].swizzledFormat = UndefinedSwizzledFormat;

            htileBufferView[1].gpuAddr = pSrcGpuMemory->Desc().gpuVirtAddr + srcOffset;
            htileBufferView[1].range = srcDataSize;
            htileBufferView[1].stride = 1;
            htileBufferView[1].swizzledFormat = UndefinedSwizzledFormat;

            BufferSrd srd[2] = {};
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(2, htileBufferView, srd);

            const uint32 constData[] =
            {
                htileDecompressValue, // zsDecompressedValue
                htileMask,            // htileMask
                0u,                   // padding
                0u                    // padding
            };

            static const uint32 sizeBufferSrdDwords = NumBytesToNumDwords(sizeof(BufferSrd));
            static const uint32 sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));

            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                sizeBufferSrdDwords * 2 + sizeConstDataDwords,
                sizeBufferSrdDwords,
                PipelineBindPoint::Compute,
                0);

            // Put the SRDs for the hTile buffer into shader-accessible memory
            memcpy(pSrdTable, &srd[0], sizeof(srd));
            pSrdTable += sizeBufferSrdDwords * 2;

            // Provide the shader with all kinds of fun dimension info
            memcpy(pSrdTable, &constData, sizeof(constData));

            // Issue a dispatch with one thread per HTile DWORD.
            const uint32 htileDwords = static_cast<uint32>(dstDataSize / sizeof(uint32));
            // We'll launch cs thread that does not check boundary. So let the driver be the safe guard.
            PAL_ASSERT(IsPow2Aligned(htileDwords, 64) && (htileDwords >= 64));
            const uint32 threadGroups = RpmUtil::MinThreadGroups(htileDwords, pPipeline->ThreadsPerGroup());
            pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
        } // End of for

        // Restore the command buffer's state.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
// If it is possible that a fast-cleared image is currently also bound as a target within the same command buffer, we
// need to immediately reload the new fast clear color for all such targets by calling this function.
//
// Note that this step is separate from the always-mandatory update of the fast-cleared image's meta data vidmem
// containing the new clear color.  This extra step is necessary because, if the image was bound before the clear
// operation, the current clear value in the register is now stale.
uint32* RsrcProcMgr::UpdateBoundFastClearColor(
    GfxCmdBuffer*   pCmdBuffer,
    const GfxImage& dstImage,
    uint32          startMip,
    uint32          numMips,
    const uint32    color[4],
    CmdStream*      pStream,
    uint32*         pCmdSpace
    ) const
{
    // Only gfx command buffers can have bound render targets / DS attachments.  Fast clears through compute command
    // buffers do not have to worry about updating fast clear value register state.
    PAL_ASSERT(pCmdBuffer->GetEngineType() == EngineTypeUniversal);

    const UniversalCmdBuffer* pUnivCmdBuf = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

#if PAL_ENABLE_PRINTS_ASSERTS
    // We should be inspecting the main graphics state and not a pushed copy
    PAL_ASSERT(pUnivCmdBuf->IsGraphicsStatePushed() == false);
#endif

    const GraphicsState& graphicsState = pUnivCmdBuf->GetGraphicsState();

    // Look for this image in the bound color target views and in such a case update the fast clear color in that
    // target.
    const Image* const pImage = static_cast<const Image*>(&dstImage);

    for (uint32 slot = 0; slot < graphicsState.bindTargets.colorTargetCount; ++slot)
    {
        const ColorTargetBindInfo&   bindInfo = graphicsState.bindTargets.colorTargets[slot];
        const ColorTargetView* const pView    = static_cast<const ColorTargetView*>(bindInfo.pColorTargetView);

        // If the bound image matches the cleared image, reprogram the clear color in that slot
        if ((pView != nullptr)              &&
            (pView->GetImage() == pImage)   &&
            (pView->MipLevel() >= startMip) &&
            (pView->MipLevel() < startMip + numMips))
        {
            pCmdSpace = pView->WriteUpdateFastClearColor(slot, color, pStream, pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// This is the depth-stencil equivalent of UpdateBoundFastClearColor().
void RsrcProcMgr::UpdateBoundFastClearDepthStencil(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    dstImage,
    const SubresRange& range,
    uint32             metaDataClearFlags,
    float              depth,
    uint8              stencil
    ) const
{
    // Only gfx command buffers can have bound render targets / DS attachments.  Fast clears through compute command
    // buffers do not have to worry about updating fast clear value register state.
    PAL_ASSERT(pCmdBuffer->GetEngineType() == EngineTypeUniversal);

    const UniversalCmdBuffer* pUnivCmdBuf = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

#if PAL_ENABLE_PRINTS_ASSERTS
    // We should be inspecting the main graphics state and not a pushed copy
    PAL_ASSERT(pUnivCmdBuf->IsGraphicsStatePushed() == false);
#endif

    const GraphicsState& graphicsState = pUnivCmdBuf->GetGraphicsState();

    // Look for this image in the bound depth stencil target and in such a case update the fast clear depth/stencil
    // value.
    if (graphicsState.bindTargets.depthTarget.pDepthStencilView != nullptr)
    {
        const Image* const pImage = static_cast<const Image*>(&dstImage);

        const DepthStencilView* const pView =
            static_cast<const DepthStencilView*>(graphicsState.bindTargets.depthTarget.pDepthStencilView);

        // If the bound image matches the cleared image, reprogram the bound clear value registers
        if ((pView->GetImage() == pImage)   &&
            (pView->MipLevel() >= range.startSubres.mipLevel) &&
            (pView->MipLevel() < range.startSubres.mipLevel + range.numMips))
        {
            CmdStream* pStream = static_cast<CmdStream*>(
                pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics));

            uint32* pCmdSpace = pStream->ReserveCommands();

            pCmdSpace = pView->WriteUpdateFastClearDepthStencilValue(metaDataClearFlags, depth, stencil,
                pStream, pCmdSpace);

            // Re-write the ZRANGE_PRECISION value for the waTcCompatZRange workaround. Does not require a COND_EXEC
            // checking the metadata because we know the fast clear value here.
            if (((metaDataClearFlags & HtileAspectDepth) != 0) && (depth == 0.0f))
            {
                pCmdSpace = pView->UpdateZRangePrecision(false, pStream, pCmdSpace);
            }

            pStream->CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
// Performs a fast or slow clear on a Depth/Stencil using graphics engine or compute engine
void RsrcProcMgr::HwlDepthStencilClear(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    dstImage,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint32             rangeCount,
    const SubresRange* pRanges,
    bool               fastClear,
    bool               needComputeSync,
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
    const Image& gfx6Image = static_cast<const Image&>(dstImage);

    bool needPreComputeSync  = needComputeSync;
    bool needPostComputeSync = false;

    if (gfx6Image.Parent()->IsDepthStencil() &&
       (fastClear || (pCmdBuffer->GetEngineType() == EngineTypeUniversal)))
    {
        // This code path is for:
        // 1. fast clear using gfx engine if fast clear is requested and gfx is preferred, or
        // 2. fast clear using compute engine if fast clear is requested and compute is preferred, or
        // 3. slow clear using gfx engine if slow clear is requested and current queue type is universal, the benefits
        //    compared to compute-based slow clear includes:
        //    - No worries on that PRT tiled mode causes different tile info between depth and stencil which leads
        //      to issues when stencil tile info is used by tex block while depth tile info used in DB block.
        //    - No need to do D/S expand when D/S is in compressed state.
        //   although it's not clear about the performance difference between them.

        // Fast clears can be done on either the compute or graphics engine, but the compute engine has some
        // restrictions on it. Determine what sort of clear needs to be done for each range. We must use an AutoBuffer
        // here because rangeCount is technically unbounded; in practice it likely won't be more than a full mip chain
        // for both aspects.
        AutoBuffer<ClearMethod, 2 * MaxImageMipLevels, Platform> fastClearMethod(rangeCount, m_pDevice->GetPlatform());

        // Notify the command buffer that the AutoBuffer allocation has failed.
        if (fastClearMethod.Capacity() < rangeCount)
        {
            pCmdBuffer->NotifyAllocFailure();
        }
        else
        {
            // Track whether any of the ranges on the image were fast-cleared via graphics.  We can use this later to
            // avoid updating bound target values, because we know that a gfx fast clear pushes and pops graphics state,
            // and the pop will re-bind the old (main) DSV.  When that happens, even if the bound image is the same as
            // the cleared image, the bind operation will load the new clear value from image meta-data memory (although
            // this is not as efficient as just directly writing the register).
            bool clearedViaGfx = false;

            // Before we start issuing fast clears, tell the Image to update its fast-clear meta-data.
            uint32 metaDataClearFlags = 0;

            // Fast clear only, prepare fastClearMethod, ClearFlags and update metaData.
            if (fastClear)
            {
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    // Fast depth clear method is the same for all subresources, so we can just check the first.
                    const SubResourceInfo& subResInfo = *gfx6Image.Parent()->SubresourceInfo(pRanges[idx].startSubres);
                    fastClearMethod[idx] = subResInfo.clearMethod;
                }

                auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
                PAL_ASSERT(pCmdStream != nullptr);

                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    const uint32 currentClearFlag =
                        (pRanges[idx].startSubres.aspect == ImageAspect::Depth) ? HtileAspectDepth : HtileAspectStencil;
                    metaDataClearFlags |= currentClearFlag;

                    const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
                        pCmdBuffer->GetGfxCmdBufState().packetPredicate);

                    uint32* pCmdSpace = pCmdStream->ReserveCommands();
                    pCmdSpace = gfx6Image.UpdateDepthClearMetaData(pRanges[idx],
                                                                   currentClearFlag,
                                                                   depth,
                                                                   stencil,
                                                                   packetPredicate,
                                                                   pCmdSpace);

                    // Update the metadata for the waTcCompatZRange workaround
                    if (m_pDevice->WaTcCompatZRange() &&
                        ((currentClearFlag & HtileAspectDepth) != 0) &&
                        GetMetaDataTexFetchSupport(gfx6Image.Parent(),
                            gfx6Image.Parent()->GetBaseSubResource().aspect,
                            gfx6Image.Parent()->GetBaseSubResource().mipLevel))
                    {
                        pCmdSpace = gfx6Image.UpdateWaTcCompatZRangeMetaData(pRanges[idx],
                                                                             depth,
                                                                             packetPredicate,
                                                                             pCmdSpace);
                    }

                    pCmdStream->CommitCommands(pCmdSpace);
                }

            }

            // We can optimize this process by clearing matching depth and stencil ranges at once. To do this, we need
            // another autobuffer to track which ranges have already been cleared.
            AutoBuffer<bool, 2 * MaxImageMipLevels, Platform> isRangeProcessed(rangeCount, m_pDevice->GetPlatform());
            PAL_ASSERT(isRangeProcessed.Capacity() >= rangeCount);

            // Notify the command buffer that the AutoBuffer allocation has failed.
            if (isRangeProcessed.Capacity() < rangeCount)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    isRangeProcessed[idx] = false;
                }

                // Now issue fast or slow clears to all ranges, grouping identical depth/stencil pairs if possible.
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    // No need to clear a range twice.
                    if (isRangeProcessed[idx])
                    {
                        continue;
                    }

                    uint32 clearFlags =
                        (pRanges[idx].startSubres.aspect == ImageAspect::Depth) ? HtileAspectDepth : HtileAspectStencil;

                    // Search the range list to see if there is a matching range which span the other aspect.
                    for (uint32 forwardIdx = idx + 1; forwardIdx < rangeCount; ++forwardIdx)
                    {
                        if ((pRanges[forwardIdx].startSubres.aspect != pRanges[idx].startSubres.aspect) &&
                            (pRanges[forwardIdx].startSubres.mipLevel == pRanges[idx].startSubres.mipLevel) &&
                            (pRanges[forwardIdx].numMips == pRanges[idx].numMips) &&
                            (pRanges[forwardIdx].startSubres.arraySlice == pRanges[idx].startSubres.arraySlice) &&
                            (pRanges[forwardIdx].numSlices == pRanges[idx].numSlices) &&
                            ((fastClear == false) || (fastClearMethod[forwardIdx] == fastClearMethod[idx])))
                        {
                            // We found a matching range that for the other aspect, clear them both at once.
                            clearFlags = HtileAspectDepth | HtileAspectStencil;
                            isRangeProcessed[forwardIdx] = true;
                            break;
                        }
                    }

                    // DepthStencilClearGraphics() implements both fast and slow clears. For fast clears,
                    // if the image layout supports depth/stencil target usage and the image size is too small,
                    // the synchronization overhead of switching to compute and back is main performance bottleneck,
                    // prefer the graphics path for this case. While the image size is over this critical value,
                    // compute path has a good performance advantage, prefer the compute path for this.
                    if ((fastClearMethod[idx] == ClearMethod::DepthFastGraphics) ||
                        fastClear == false                                       ||
                        (PreferFastDepthStencilClearGraphics(dstImage, depthLayout, stencilLayout)))
                    {
                        DepthStencilClearGraphics(pCmdBuffer,
                                                  gfx6Image,
                                                  pRanges[idx],
                                                  depth,
                                                  stencil,
                                                  clearFlags,
                                                  fastClear,
                                                  depthLayout,
                                                  stencilLayout,
                                                  boxCnt,
                                                  pBox);
                        clearedViaGfx = true;
                    }
                    else
                    {
                        if (needPreComputeSync)
                        {
                            const ImageAspect aspect  = pRanges[idx].startSubres.aspect;
                            const bool        isDepth = (aspect == ImageAspect::Depth);
                            PreComputeDepthStencilClearSync(pCmdBuffer,
                                                            gfx6Image,
                                                            pRanges[idx],
                                                            isDepth ? depthLayout : stencilLayout);

                            needPreComputeSync  = false;
                            needPostComputeSync = true;
                        }

                        FastDepthStencilClearCompute(pCmdBuffer, gfx6Image, pRanges[idx], depth, stencil, clearFlags);
                    }

                    isRangeProcessed[idx] = true;

                    // In case the cleared image is possibly already bound as a depth target, we need to update the
                    // depth/stencil clear value registers do the new cleared values.  We can skip this if any of
                    // the clears used a gfx blt (see description above), for fast clear only.
                    if (fastClear && (pCmdBuffer->GetEngineType() == EngineTypeUniversal) && (clearedViaGfx == false))
                    {
                        UpdateBoundFastClearDepthStencil(pCmdBuffer,
                                                         dstImage,
                                                         pRanges[idx],
                                                         metaDataClearFlags,
                                                         depth,
                                                         stencil);
                    }
                }

            } // isRangeProcessed AutoBuffer alloc succeeded.
        } // Fast clear AutoBuffer alloc succeeded.
    } // Fast clear OR Universal queue.
    else
    {
        // This code path is only compute-based slow clear

        const Pal::Image* pParent = gfx6Image.Parent();

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            const ImageAspect    aspect  = pRanges[idx].startSubres.aspect;
            const bool           isDepth = (aspect == ImageAspect::Depth);
            const SwizzledFormat format  = gfx6Image.Parent()->SubresourceInfo(pRanges[idx].startSubres)->format;

            // If it's PRT tiled mode, tile info for depth and stencil end up being different,
            // compute slow clear uses stencil tile info for stencil clear but later when bound
            // as target, depth tile info will be used, which leads to problem. The similar
            // assert need to be added in elsewhere as needed.
            PAL_ASSERT(isDepth ||
                       (AddrMgr1::IsPrtTiled(gfx6Image.GetSubResourceTileMode(pRanges[idx].startSubres)) == false));

            ClearColor clearColor = {};

            DepthStencilLayoutToState layoutToState = gfx6Image.LayoutToDepthCompressionState(pRanges[idx].startSubres);

            if (isDepth)
            {
                // Expand first if depth plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, depthLayout) != DepthStencilDecomprNoHiZ)
                {
                    // MSAA state is unnecessary because this is a compute expand.
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, nullptr, pRanges[idx]);
                }

                // For Depth slow clears, we use a float clear color.
                clearColor.type = ClearColorType::Float;
                clearColor.f32Color[0] = depth;
            }
            else
            {
                PAL_ASSERT(aspect == ImageAspect::Stencil);

                // Expand first if stencil plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, stencilLayout) != DepthStencilDecomprNoHiZ)
                {
                    // MSAA state is unnecessary because this is a compute expand.
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, nullptr, pRanges[idx]);
                }

                // For Stencil aspect we use the stencil value directly.
                clearColor.type = ClearColorType::Uint;
                clearColor.u32Color[0] = stencil;
            }

            if (needPreComputeSync)
            {
                PreComputeDepthStencilClearSync(pCmdBuffer,
                                                gfx6Image,
                                                pRanges[idx],
                                                isDepth ? depthLayout : stencilLayout);

                needPreComputeSync  = false;
                needPostComputeSync = true;
            }

            SlowClearCompute(pCmdBuffer,
                             *pParent,
                             isDepth ? depthLayout : stencilLayout,
                             format,
                             &clearColor,
                             pRanges[idx],
                             boxCnt,
                             pBox);
        }
    }

    if (needPostComputeSync)
    {
        PostComputeDepthStencilClearSync(pCmdBuffer);
    }
}

// =====================================================================================================================
// Check if for all the regions, the format, tile mode and tile type matches for src and dst image.
// If all regions match, we can do a fixed function resolve. Otherwise return false.
bool RsrcProcMgr::HwlCanDoFixedFuncResolve(
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions
    ) const
{
    bool ret = false;
    for (uint32 region = 0; region < regionCount; ++region)
    {
        const ImageResolveRegion& imageRegion = pRegions[region];
        const SubresId srcSubResId = { imageRegion.srcAspect,
                                       imageRegion.dstMipLevel,
                                       imageRegion.srcSlice };
        const SubresId dstSubResId = { imageRegion.dstAspect,
                                       imageRegion.dstMipLevel,
                                       imageRegion.dstSlice };

        const SubResourceInfo*const    pSrcSubRsrcInfo = srcImage.SubresourceInfo(srcSubResId);
        const SubResourceInfo*const    pDstSubRsrcInfo = dstImage.SubresourceInfo(dstSubResId);
        const AddrMgr1::TileInfo*const pSrcTileInfo    = AddrMgr1::GetTileInfo(&srcImage, srcSubResId);
        const AddrMgr1::TileInfo*const pDstTileInfo    = AddrMgr1::GetTileInfo(&dstImage, dstSubResId);

        ret = ((memcmp(&pSrcSubRsrcInfo->format, &pDstSubRsrcInfo->format, sizeof(SwizzledFormat)) == 0) &&
               (pSrcTileInfo->tileMode == pDstTileInfo->tileMode)                                        &&
               (pSrcTileInfo->tileType == pDstTileInfo->tileType));
        if (ret == false)
        {
            PAL_ALERT_ALWAYS();
            break;
        }
    }

    // Hardware only has support for Average resolves, so we can't perform a fixed function resolve if we're using
    // Minimum or Maximum resolves.
    if (resolveMode != ResolveMode::Average)
    {
        ret = false;
    }

    return ret;
}

// =====================================================================================================================
// Check if for all the regions, format/addressing/resolve-paramaters match pre-condition of depth/stencil copy.
// If all regions match, we can do a fixed-func depth/stencil copy resolve. Otherwise return false.
bool RsrcProcMgr::HwlCanDoDepthStencilCopyResolve(
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions
    ) const
{
    const auto& chipProps = m_pDevice->Parent()->ChipProperties();
    const ImageCreateInfo& srcCreateInfo = srcImage.GetImageCreateInfo();
    const ImageCreateInfo& dstCreateInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT(srcCreateInfo.imageType == dstCreateInfo.imageType);
    PAL_ASSERT(srcCreateInfo.imageType != ImageType::Tex3d);

    const Image*  pGfxSrcImage = reinterpret_cast<const Image*>(srcImage.GetGfxImage());
    const Image*  pGfxDstImage = reinterpret_cast<const Image*>(dstImage.GetGfxImage());

    // Htile copy and fix-up will be performed if dst image has htile data, so src image containing htile data is
    // referred as pre-condition of depth/stencil copy resolve if dst image has htile data.
    bool canDoDepthStencilCopyResolve = pGfxSrcImage->HasHtileData() || (pGfxDstImage->HasHtileData() == false);

    for (uint32 region = 0; canDoDepthStencilCopyResolve && (region < regionCount); ++region)
    {
        const ImageResolveRegion& imageRegion = pRegions[region];
        const SubresId srcSubResId = { imageRegion.srcAspect,
                                       0,
                                       imageRegion.srcSlice };
        const SubresId dstSubResId = { imageRegion.dstAspect,
                                       imageRegion.dstMipLevel,
                                       imageRegion.dstSlice };

        PAL_ASSERT(imageRegion.srcAspect == imageRegion.dstAspect);

        const SubResourceInfo*const    pSrcSubRsrcInfo = srcImage.SubresourceInfo(srcSubResId);
        const SubResourceInfo*const    pDstSubRsrcInfo = dstImage.SubresourceInfo(dstSubResId);
        const AddrMgr1::TileInfo*const pSrcTileInfo = AddrMgr1::GetTileInfo(&srcImage, srcSubResId);
        const AddrMgr1::TileInfo*const pDstTileInfo = AddrMgr1::GetTileInfo(&dstImage, dstSubResId);
        const Gfx6Htile*const          pSrcHtile = pGfxSrcImage->GetHtile(srcSubResId);
        const Gfx6Htile*const          pDstHtile = pGfxDstImage->GetHtile(dstSubResId);

        PAL_ASSERT((pSrcTileInfo->tileType == ADDR_DEPTH_SAMPLE_ORDER) &&
                   (pDstTileInfo->tileType == ADDR_DEPTH_SAMPLE_ORDER));

        if (chipProps.gfxLevel > GfxIpLevel::GfxIp6)
        {
            // To enable db and tc work properly simultaneously, address lib might split the depth surface to ensure
            // depth and stencil macro tile mode compatible(using tile mode 0) on Gfx7/Gfx8. Db uses split in bytes
            // while cb uses split in samples. So pre-condition of depth-copy resolve is depth surface not splitting.
            // Stencil-copy resolve always has chance to go on as stencil part will never split with sample 1.
            if ((imageRegion.dstAspect != ImageAspect::Stencil) &&
                (pSrcTileInfo->tileMode == ADDR_TM_2D_TILED_THIN1))
            {
                // 2D tiled depth surface should not be splitted for depth resolve dst on Gfx7/Gfx8.
                const uint32 tileSplitBytes = 1 << pDstTileInfo->tileSplitBytes;
                ZFormat zFormat =
                    HwZFmt(MergedChannelFmtInfoTbl(chipProps.gfxLevel), dstCreateInfo.swizzledFormat.format);
                PAL_ASSERT((zFormat == Z_16) || (zFormat == Z_32_FLOAT));
                const uint32 surfBytesPerPixel = (zFormat == (Z_32_FLOAT) ? 4 : 2);

                canDoDepthStencilCopyResolve &= (tileSplitBytes >= surfBytesPerPixel);
            }
        }
        else
        {
            PAL_ASSERT(chipProps.gfxLevel == GfxIpLevel::GfxIp6);

            canDoDepthStencilCopyResolve = false;
            break;
        }

        // SrcOffset and dstOffset have to match for a depth/stencil copy
        canDoDepthStencilCopyResolve &= ((imageRegion.srcOffset.x == imageRegion.dstOffset.x) &&
                                         (imageRegion.srcOffset.y == imageRegion.dstOffset.y));

        // Resolve region has to be full range of dst image, since we don't build a htile look-up table
        // for gfx6. A full range htile copy and fix-up is expected to be performed in the following stage.
        canDoDepthStencilCopyResolve &= ((imageRegion.dstOffset.x == 0)                                        &&
                                         (imageRegion.dstOffset.y == 0)                                        &&
                                         (srcCreateInfo.extent.width == pDstSubRsrcInfo->extentTexels.width)   &&
                                         (srcCreateInfo.extent.height == pDstSubRsrcInfo->extentTexels.height) &&
                                         (imageRegion.extent.width == pDstSubRsrcInfo->extentTexels.width)     &&
                                         (imageRegion.extent.height == pDstSubRsrcInfo->extentTexels.height));

        // Format of src and dst shall be same, since htile copy and fix-up requires that htile value
        // is compatible of src and dst.
        canDoDepthStencilCopyResolve &=
            (memcmp(&pSrcSubRsrcInfo->format, &pDstSubRsrcInfo->format, sizeof(SwizzledFormat)) == 0);

        PAL_ASSERT(pSrcTileInfo->pipeConfig == pDstTileInfo->pipeConfig);

        canDoDepthStencilCopyResolve &= ((pSrcTileInfo->tileMode == pDstTileInfo->tileMode) &&
                                         (pSrcTileInfo->tileType == pDstTileInfo->tileType));

        if (pGfxDstImage->HasHtileData())
        {
            const bool srcSupportMetaDataTexFetch = pSrcSubRsrcInfo->flags.supportMetaDataTexFetch;
            const bool dstSupportMetaDataTexFetch = pDstSubRsrcInfo->flags.supportMetaDataTexFetch;

            // Htile addressing is consistent to macro tile mode of surface, a raw htile copy and fix-up requires that
            // htile addressing is compatble of src and dst.
            canDoDepthStencilCopyResolve &= ((srcSupportMetaDataTexFetch == dstSupportMetaDataTexFetch)   &&
                                             (pSrcSubRsrcInfo->rowPitch == pDstSubRsrcInfo ->rowPitch)    &&
                                             (pSrcSubRsrcInfo->depthPitch == pDstSubRsrcInfo->depthPitch) &&
                                             (pSrcHtile->SliceSize() == pDstHtile->SliceSize())           &&
                                             (pSrcTileInfo->banks == pDstTileInfo->banks)                 &&
                                             (pSrcTileInfo->bankWidth == pDstTileInfo->bankWidth)         &&
                                             (pSrcTileInfo->bankHeight == pDstTileInfo->bankHeight)       &&
                                             (pSrcTileInfo->macroAspectRatio == pDstTileInfo->macroAspectRatio));
        }
    }

    // Check if there's any array slice overlap. If there's array slice overlap,
    // switch to pixel-shader resolve.
    if (canDoDepthStencilCopyResolve)
    {
        for (uint32 curIndex = 0; curIndex < regionCount; ++curIndex)
        {
            const ImageResolveRegion& curRegion = pRegions[curIndex];

            for (uint32 otherIndex = (curIndex + 1); otherIndex < regionCount; ++otherIndex)
            {
                const ImageResolveRegion& otherRegion = pRegions[otherIndex];

                if (curRegion.dstMipLevel == otherRegion.dstMipLevel)
                {
                    if (curRegion.dstSlice == otherRegion.dstSlice)
                    {
                        // Depth/stencil on the same array slice is allowed to perform fixed-func depth/stencil resolve
                        PAL_ASSERT(curRegion.dstAspect != otherRegion.dstAspect);
                        canDoDepthStencilCopyResolve &= ((curRegion.srcSlice == otherRegion.srcSlice) &&
                                                         (curRegion.numSlices == otherRegion.numSlices));
                    }
                    else
                    {
                        canDoDepthStencilCopyResolve &=
                            ((curRegion.dstSlice >= (otherRegion.dstSlice + otherRegion.numSlices)) ||
                             (otherRegion.dstSlice >= (curRegion.dstSlice + curRegion.numSlices)));
                    }
                }
            }
        }
    }

    return canDoDepthStencilCopyResolve;
}

// =====================================================================================================================
// Performs a "fast" depth resummarize operation by updating the depth Image's HTile buffer to represent a fully open
// HiZ range.
void RsrcProcMgr::HwlExpandHtileHiZRange(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    image,
    const SubresRange& range
    ) const
{
    const Gfx6::Image& gfx6Image = static_cast<const Gfx6::Image&>(image);

    // Evaluate the mask and value for updating the HTile buffer.
    const Gfx6Htile*const pBaseHtile = gfx6Image.GetHtile(range.startSubres);
    PAL_ASSERT(pBaseHtile != nullptr);

    uint32 htileValue = 0;
    uint32 htileMask  = 0;
    pBaseHtile->ComputeResummarizeData(&htileValue, &htileMask);

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function assumes that all mip levels must use the same Htile value and mask.
    SubresId nextMipSubres = range.startSubres;
    while (++nextMipSubres.mipLevel < (range.startSubres.mipLevel + range.numMips))
    {
        const Gfx6Htile*const pNextHtile = gfx6Image.GetHtile(nextMipSubres);
        PAL_ASSERT(pNextHtile != nullptr);

        uint32 nextHtileValue = 0;
        uint32 nextHtileMask  = 0;
        pNextHtile->ComputeResummarizeData(&nextHtileValue, &nextHtileMask);
        PAL_ASSERT((htileValue == nextHtileValue) && (htileMask == nextHtileMask));
    }
#endif

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    if (htileMask == UINT_MAX)
    {
        // If the HTile mask has all bits set, we can use the standard ClearHtile path.
        ClearHtile(pCmdBuffer, gfx6Image, range, htileValue);
    }
    else
    {
        // Use the depth-clear read-write shader.
        const ComputePipeline*const pPipeline = GetPipeline(RpmComputePipeline::FastDepthClear);

        // Bind the pipeline.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

        // Put the new HTile data in user data 4 and the old HTile data mask in user data 5.
        const uint32 htileUserData[2] = { htileValue & htileMask, ~htileMask };
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 4, 2, htileUserData);

        // For each mipmap level: create a temporary buffer object bound to the location in video memory where that
        // mip's HTile buffer resides. Then, issue a dispatch to update the HTile contents to reflect the
        // "full HiZ range" state.
        const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
        for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
        {
            GpuMemory* pGpuMemory = nullptr;
            gpusize    offset     = 0;
            gpusize    dataSize   = 0;

            gfx6Image.GetHtileBufferInfo(mip,
                                         range.startSubres.arraySlice,
                                         range.numSlices,
                                         HtileBufferUsage::Clear,
                                         &pGpuMemory,
                                         &offset,
                                         &dataSize);

            BufferViewInfo htileBufferView = {};
            htileBufferView.gpuAddr        = pGpuMemory->Desc().gpuVirtAddr + offset;
            htileBufferView.range          = dataSize;
            htileBufferView.stride         = sizeof(uint32);
            htileBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
            htileBufferView.swizzledFormat.swizzle =
                { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

            BufferSrd srd = { };
            m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &htileBufferView, &srd);

            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 4, &srd.word0.u32All);

            // Issue a dispatch with one thread per HTile DWORD.
            const uint32 htileDwords  = static_cast<uint32>(htileBufferView.range / sizeof(uint32));
            const uint32 threadGroups = RpmUtil::MinThreadGroups(htileDwords, pPipeline->ThreadsPerGroup());
            pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
        }
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image range by updating the Image's HTile buffer.
void RsrcProcMgr::FastDepthStencilClearCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    float              depth,
    uint8              stencil,
    uint32             clearMask
    ) const
{
    const auto& createInfo = dstImage.Parent()->GetImageCreateInfo();

    auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
    PAL_ASSERT(pCmdStream != nullptr);

    // Evaluate the mask and value for updating the HTile buffer.
    const Gfx6Htile*const pBaseHtile = dstImage.GetHtile(range.startSubres);
    PAL_ASSERT(pBaseHtile != nullptr);

    uint32 htileValue = 0;
    uint32 htileMask  = 0;
    pBaseHtile->ComputeClearData(clearMask, depth, &htileValue, &htileMask);

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function assumes that all mip levels must use the same Htile value and mask.
    SubresId nextMipSubres = range.startSubres;
    while (++nextMipSubres.mipLevel < (range.startSubres.mipLevel + range.numMips))
    {
        const Gfx6Htile*const pNextHtile = dstImage.GetHtile(nextMipSubres);
        PAL_ASSERT(pNextHtile != nullptr);

        uint32 nextHtileValue = 0;
        uint32 nextHtileMask  = 0;
        pNextHtile->ComputeClearData(clearMask, depth, &nextHtileValue, &nextHtileMask);
        PAL_ASSERT((htileValue == nextHtileValue) && (htileMask == nextHtileMask));
    }
#endif

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Determine which pipeline to use for this clear.
    const ComputePipeline* pPipeline = GetLinearHtileClearPipeline(m_pDevice->Settings().dbPerTileExpClearEnable,
                                                                   pBaseHtile->TileStencilDisabled(),
                                                                   htileMask);

    if (pPipeline != nullptr)
    {
        // Bind the pipeline.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

        // Put the new HTile data in user data 4 and the old HTile data mask in user data 5.
        const uint32 htileUserData[2] = { htileValue & htileMask, ~htileMask };
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 4, 2, htileUserData);

        // For each mipmap level: create a temporary buffer object bound to the location in video memory where that
        // mip's HTile buffer resides. Then, issue a dispatch to update the HTile contents to reflect the fast-cleared
        // state.
        const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
        for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
        {
            GpuMemory* pGpuMemory = nullptr;
            gpusize    offset     = 0;
            gpusize    dataSize   = 0;

            dstImage.GetHtileBufferInfo(mip,
                                        range.startSubres.arraySlice,
                                        range.numSlices,
                                        HtileBufferUsage::Clear,
                                        &pGpuMemory,
                                        &offset,
                                        &dataSize);

            BufferViewInfo htileBufferView = {};
            htileBufferView.gpuAddr        = pGpuMemory->Desc().gpuVirtAddr + offset;
            htileBufferView.range          = dataSize;
            htileBufferView.stride         = sizeof(uint32);
            htileBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
            htileBufferView.swizzledFormat.swizzle =
                { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

            BufferSrd srd = { };
            m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &htileBufferView, &srd);

            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 4, &srd.word0.u32All);

            // Issue a dispatch with one thread per HTile DWORD.
            const uint32 htileDwords  = static_cast<uint32>(htileBufferView.range / sizeof(uint32));
            const uint32 threadGroups = RpmUtil::MinThreadGroups(htileDwords, pPipeline->ThreadsPerGroup());
            pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
        }
    }
    else
    {
        ClearHtile(pCmdBuffer, dstImage, range, htileValue);
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    // Note: When performing a stencil-only or depth-only clear on an Image which has both aspects, we have a
    // potential problem because the two separate aspects may utilize the same HTile memory. Single-aspect clears
    // perform a read-modify-write of HTile memory, which can cause synchronization issues later-on because no
    // resource transition is needed on the depth aspect when clearing stencil (and vice-versa). The solution
    // is to add a CS_PARTIAL_FLUSH and a Texture Cache Flush after executing a susceptible clear.
    if ((TestAllFlagsSet(clearMask, HtileAspectDepth | HtileAspectStencil) == false) &&
        (pBaseHtile->GetHtileContents() == HtileContents::DepthStencil))
    {
        // Note that it's not possible for us to handle all necessary synchronization corner-cases here. PAL allows our
        // clients to do things like this:
        // - Init both aspects, clear then, and render to them.
        // - Transition stencil to shader read (perhaps on the compute queue).
        // - Do some additional rendering to depth only.
        // - Clear the stencil aspect.
        //
        // The last two steps will populate the DB metadata caches and shader caches with conflicting HTile data.
        // We can't think of any efficient methods to handle cases like these and the inefficient methods are still
        // of questionable correctness.

        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All = CpCoherCntlTexCacheMask;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                                SURFACE_SYNC_ENGINE_ME,
                                                FullSyncBaseAddr,
                                                FullSyncSize,
                                                pCmdStream->GetEngineType() == EngineTypeCompute,
                                                pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Performs a slow or fast depth clear using the graphics engine.
void RsrcProcMgr::DepthStencilClearGraphics(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    float              depth,
    uint8              stencil,
    uint32             clearMask,
    bool               fastClear,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
    PAL_ASSERT(dstImage.Parent()->IsDepthStencil());
    PAL_ASSERT((fastClear == false) ||
               dstImage.IsFastDepthStencilClearSupported(depthLayout,
                                                         stencilLayout,
                                                         depth,
                                                         stencil,
                                                         range));

    const auto& settings     = m_pDevice->Settings();
    const bool  clearDepth   = TestAnyFlagSet(clearMask, HtileAspectDepth);
    const bool  clearStencil = TestAnyFlagSet(clearMask, HtileAspectStencil);
    PAL_ASSERT(clearDepth || clearStencil); // How did we get here if there's nothing to clear!?

    const InputAssemblyStateParams   inputAssemblyState   = { PrimitiveTopology::RectList };
    const DepthBiasParams            depthBias            = { 0.0f, 0.0f, 0.0f };
    const PointLineRasterStateParams pointLineRasterState = { 1.0f, 1.0f };
    const StencilRefMaskParams       stencilRefMasks      =
        { stencil, 0xFF, 0xFF, 0x01, stencil, 0xFF, 0xFF, 0x01, 0xFF };
    const TriangleRasterStateParams  triangleRasterState  =
    {
        FillMode::Solid,        // fillMode
        CullMode::None,         // cullMode
        FaceOrientation::Cw,    // frontFace
        ProvokingVertex::First  // provokingVertex
    };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;

    ScissorRectParams scissorInfo    = { };
    scissorInfo.count                = 1;
    scissorInfo.scissors[0].offset.x = 0;
    scissorInfo.scissors[0].offset.y = 0;

    DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };
    depthViewInfoInternal.depthClearValue   = depth;
    depthViewInfoInternal.stencilClearValue = stencil;

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage    = dstImage.Parent();
    depthViewInfo.arraySize = 1;

    // Depth-stencil targets must be used on the universal engine.
    PAL_ASSERT((clearDepth   == false) || TestAnyFlagSet(depthLayout.engines,   LayoutUniversalEngine));
    PAL_ASSERT((clearStencil == false) || TestAnyFlagSet(stencilLayout.engines, LayoutUniversalEngine));

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.depthTarget.depthLayout   = depthLayout;
    bindTargetsInfo.depthTarget.stencilLayout = stencilLayout;

    // Save current command buffer state and bind graphics state which is common for all mipmap levels.
    pCmdBuffer->PushGraphicsState();

    // Bind the depth expand state because it's just a full image quad and a zero PS (with no internal flags) which
    // is also what we need for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), });
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstImage.Parent()->GetImageCreateInfo().samples,
                                              dstImage.Parent()->GetImageCreateInfo().fragments));
    pCmdBuffer->CmdSetDepthBiasState(depthBias);
    pCmdBuffer->CmdSetInputAssemblyState(inputAssemblyState);
    pCmdBuffer->CmdSetPointLineRasterState(pointLineRasterState);
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);
    pCmdBuffer->CmdSetTriangleRasterState(triangleRasterState);

    // Select a depth/stencil state object for this clear:
    if (clearDepth && clearStencil)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthStencilClearState);
    }
    else if (clearDepth)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthClearState);
    }
    else if (clearStencil)
    {
        pCmdBuffer->CmdBindDepthStencilState(m_pStencilClearState);
    }

    // All mip levels share the same depth export value, so only need to do it once.
    RpmUtil::WriteVsZOut(pCmdBuffer, depth);
    RpmUtil::WriteVsFirstSliceOffet(pCmdBuffer, 0);

    // Box of partial clear is only valid when number of mip-map is equal to 1.
    PAL_ASSERT((boxCnt == 0) || ((pBox != nullptr) && (range.numMips == 1)));
    uint32 scissorCnt = (boxCnt > 0) ? boxCnt : 1;

    // Each mipmap level has to be fast-cleared individually because a depth target view can only be tied to a
    //single mipmap level of the destination Image.
    const uint32 lastMip = (range.startSubres.mipLevel + range.numMips - 1);
    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        const SubresId         subres     = { range.startSubres.aspect, depthViewInfo.mipLevel, 0 };
        const SubResourceInfo& subResInfo = *dstImage.Parent()->SubresourceInfo(subres);

        // All slices of the same mipmap level can re-use the same viewport and scissor state.
        viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
        viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

        scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
        scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

        pCmdBuffer->CmdSetViewports(viewportInfo);

        // If these flags are set, then the DB will do a fast-clear.  With them not set, then we wind up doing a
        // slow clear with the Z-value being exported by the VS.
        //
        //     [If the surface can be bound as a texture, ] then we cannot do fast clears to a value that
        //     isn't 0.0 or 1.0.  In this case, you would need a medium rate clear, which can be done
        //     with CLEAR_DISALLOWED (assuming that feature works), or by setting CLEAR_ENABLE=0, and rendering
        //     a full screen rect that has the clear value this will become a set of fast_set tiles, which are
        //     faster than a slow clear, but not as fast as a real fast clear
        //
        //     Z_INFO and STENCIL_INFO CLEAR_DISALLOWED were never reliably working on GFX8 or 9.  Although the
        //     bit is not implemented, it does actually connect into logic.  In block regressions, some tests
        //     worked but many tests did not work using this bit.  Please do not set this bit

        depthViewInfoInternal.flags.isDepthClear   = (fastClear && clearDepth);
        depthViewInfoInternal.flags.isStencilClear = (fastClear && clearStencil);

        // Issue a fast clear draw for each slice of the current mip level.
        const uint32 lastSlice = (range.startSubres.arraySlice + range.numSlices - 1);
        for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
             depthViewInfo.baseArraySlice <= lastSlice;
             ++depthViewInfo.baseArraySlice)
        {
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAllocator(pCmdBuffer->Allocator(), false);

            IDepthStencilView* pDepthView = nullptr;
            void* pDepthViewMem =
                PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAllocator, AllocInternalTemp);

            if (pDepthViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                Result result = m_pDevice->CreateDepthStencilView(depthViewInfo,
                                                                  depthViewInfoInternal,
                                                                  pDepthViewMem,
                                                                  &pDepthView);
                PAL_ASSERT(result == Result::Success);

                // Bind the depth view for this mip and slice.
                bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                for (uint32 i = 0; i < scissorCnt; i++)
                {
                    if (boxCnt > 0)
                    {
                        scissorInfo.scissors[0].offset.x      = pBox[i].offset.x;
                        scissorInfo.scissors[0].offset.y      = pBox[i].offset.y;
                        scissorInfo.scissors[0].extent.width  = pBox[i].extent.width;
                        scissorInfo.scissors[0].extent.height = pBox[i].extent.height;
                    }

                    pCmdBuffer->CmdSetScissorRects(scissorInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1);
                }

                // Unbind the depth view and destroy it.
                bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                PAL_SAFE_FREE(pDepthViewMem, &sliceAllocator);
            }
        } // End for each slice.
    } // End for each mip.

    // Restore original command buffer state and destroy the depth/stencil state.
    pCmdBuffer->PopGraphicsState();
}

// =====================================================================================================================
// Issues a compute shader blt to initialize the Mask RAM allocatons for an Image.
void RsrcProcMgr::InitMaskRam(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& range,
    SyncReqs*          pSyncReqs
    ) const
{
    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // If any of following conditions is met, that means we are going to use PFP engine to update the metadata
    // (e.g. UpdateColorClearMetaData(); UpdateDccStateMetaData() etc.)
    if (pCmdBuffer->IsGraphicsSupported() &&
        (dstImage.HasDccStateMetaData()         ||
         dstImage.HasFastClearMetaData()        ||
         dstImage.HasWaTcCompatZRangeMetaData() ||
         dstImage.HasFastClearEliminateMetaData()))
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // Stalls the PFP until the ME has processed all previous commands. Useful in cases that aliasing the memory
        // (i.e. ME and PFP can access the same memory). PFP need to stall execution until ME finish its previous
        // work.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    if (dstImage.HasHtileData())
    {
        const auto& hTile = *dstImage.GetHtile(range.startSubres);

        // Handle initialization of single aspect
        if (dstImage.RequiresSeparateAspectInit() && (hTile.GetHtileContents() == HtileContents::DepthStencil))
        {
            ClearHtileAspect(pCmdBuffer, dstImage, range);
        }
        // If this is the stencil aspect initialization pass and this hTile buffer doesn't support stencil then
        // there's nothing to do.
        else if ((range.startSubres.aspect != ImageAspect::Stencil) || (hTile.TileStencilDisabled() == false))
        {
            const uint32 value = hTile.GetInitialValue();

            ClearHtile(pCmdBuffer, dstImage, range, value);
        }
    }
    else
    {
        // Clear the Image's CMask sub-allocation(s). This should always be done since we expect all Images which come
        // down this path at least have CMask data.
        if (dstImage.HasCmaskData())
        {
            ClearCmask(pCmdBuffer, dstImage, range, Gfx6Cmask::GetInitialValue(dstImage));
        }

        if (dstImage.HasFmaskData())
        {
            ClearFmask(pCmdBuffer, dstImage, range, Gfx6Fmask::GetPackedExpandedValue(dstImage));
        }

        if (dstImage.HasDccData())
        {
            ClearDcc(pCmdBuffer, pCmdStream, dstImage, range, Gfx6Dcc::InitialValue, DccClearPurpose::Init);
        }
    }

    if (dstImage.HasFastClearMetaData())
    {
        if (dstImage.HasHtileData())
        {
            // The DB Tile Summarizer requires a TC compatible clear value of stencil,
            // because TC isn't aware of DB_STENCIL_CLEAR register.
            // Please note the clear value of depth is also initialized together,
            // although it might be unnecessary.
            InitDepthClearMetaData(pCmdBuffer, pCmdStream, dstImage, range);
        }
        else
        {
            // Initialize the clear value of color just as the way of depth/stencil.
            InitColorClearMetaData(pCmdBuffer, pCmdStream, dstImage, range);
        }
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    // After initializing Mask RAM, we need a CS_PARTIAL_FLUSH and a texture cache flush to guarantee the initialization
    // blt has finished, even if other Blts caused these operations to occur before any Blts were performed.
    pSyncReqs->csPartialFlush                   = 1;
    pSyncReqs->cpCoherCntl.bits.TCL1_ACTION_ENA = 1;
    pSyncReqs->cpCoherCntl.bits.TC_ACTION_ENA   = 1;
}

// =====================================================================================================================
// Memsets an Image's CMask sub-allocations with the specified clear value.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearCmask(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint32             clearValue
    ) const
{
    const ImageCreateInfo& createInfo = dstImage.Parent()->GetImageCreateInfo();

    // Get some useful information about the image.
    const bool is3dImage = (createInfo.imageType == ImageType::Tex3d);

    // For each mipmap level, issue a dispatch to fill the CMask buffer with the specified DWORD value.
    const uint32 lastMip = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    for (uint32 mip = clearRange.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        const SubresId         mipSubres  = { ImageAspect::Color, mip, 0 };
        const SubResourceInfo& subResInfo = *dstImage.Parent()->SubresourceInfo(mipSubres);

        // For 3D Images, always clear all depth slices of this mip level, otherwise use the range's slice info.
        const uint32 baseSlice = (is3dImage ? 0                             : clearRange.startSubres.arraySlice);
        const uint32 numSlices = (is3dImage ? subResInfo.extentTexels.depth : clearRange.numSlices);

        GpuMemory* pGpuMemory = nullptr;
        gpusize    dstOffset   = 0;
        gpusize    fillSize    = 0;
        dstImage.GetCmaskBufferInfo(mip, baseSlice, numSlices, &pGpuMemory, &dstOffset, &fillSize);

        CmdFillMemory(pCmdBuffer, false, *pGpuMemory, dstOffset, fillSize, clearValue);
    }
}

// =====================================================================================================================
// Memsets an Image's FMask sub-allocations with the specified clear value.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearFmask(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint32             clearValue
    ) const
{
    // Note: MSAA Images do not support multiple mipmpap levels, so we can make some assumptions here.
    PAL_ASSERT(dstImage.Parent()->GetImageCreateInfo().mipLevels == 1);
    PAL_ASSERT((clearRange.startSubres.mipLevel == 0) && (clearRange.numMips == 1));

    GpuMemory* pGpuMemory = nullptr;
    gpusize    dstOffset  = 0;
    gpusize    fillSize   = 0;

    dstImage.GetFmaskBufferInfo(clearRange.startSubres.arraySlice,
                                clearRange.numSlices,
                                &pGpuMemory,
                                &dstOffset,
                                &fillSize);

    CmdFillMemory(pCmdBuffer, false, *pGpuMemory, dstOffset, fillSize, clearValue);
}

// =====================================================================================================================
// Memsets an Image's DCC sub-allocations with the specified clear value.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearDcc(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint32             clearValue,
    DccClearPurpose    clearPurpose
    ) const
{
    // Get some useful information about the image.
    const bool is3dImage = (dstImage.Parent()->GetImageCreateInfo().imageType == ImageType::Tex3d);

    // For each mipmap level, issue a dispatch to fill the DCC buffer with the specified DWORD value.
    const uint32 lastMip = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    for (uint32 mip = clearRange.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        // For 3D Images, always clear all depth slices of this mip level (as its DCC memory is not "sliced" at creation
        // time, specifying baseSlice = 0, numSlices = 1 is enough). Otherwise use the range's slice info.
        const uint32 baseSlice = (is3dImage ? 0 : clearRange.startSubres.arraySlice);
        const uint32 numSlices = (is3dImage ? 1 : clearRange.numSlices);

        const uint32 slicesPerClear = ((clearPurpose == DccClearPurpose::FastClear) &&
                                       (dstImage.CanMergeClearDccSlices(mip) == false)) ? 1 : numSlices;

        for (uint32 slice = baseSlice; slice < (baseSlice + numSlices); slice += slicesPerClear)
        {
            GpuMemory* pGpuMemory = nullptr;
            gpusize    dstOffset  = 0;
            gpusize    fillSize   = 0;

            dstImage.GetDccBufferInfo(mip, slice, slicesPerClear, clearPurpose, &pGpuMemory, &dstOffset, &fillSize);

            // It's possible for the fill size to be zero so we should only continue if there's something to clear.
            if (fillSize > 0)
            {
                CmdFillMemory(pCmdBuffer, false, *pGpuMemory, dstOffset, fillSize, clearValue);
            }
            else
            {
                break;
            }
        }
    }

    const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
        pCmdBuffer->GetGfxCmdBufState().packetPredicate);

    // Since we're using a compute shader we have to update the DCC state metadata manually.
    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = dstImage.UpdateDccStateMetaData(clearRange,
                                                (clearPurpose == DccClearPurpose::FastClear),
                                                packetPredicate,
                                                pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Memsets an Image's HTile sub-allocations with the specified clear value.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearHtile(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint32             clearValue
    ) const
{
    // There shouldn't be any 3D images with HTile allocations.
    PAL_ASSERT(dstImage.Parent()->GetImageCreateInfo().imageType != ImageType::Tex3d);

    // For each mipmap level, issue a dispatch to fill the HTile buffer with the specified DWORD value.
    const uint32 lastMip = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    for (uint32 mip = clearRange.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        GpuMemory* pGpuMemory = nullptr;
        gpusize    dstOffset  = 0;
        gpusize    fillSize   = 0;

        dstImage.GetHtileBufferInfo(mip,
                                    clearRange.startSubres.arraySlice,
                                    clearRange.numSlices,
                                    HtileBufferUsage::Clear,
                                    &pGpuMemory,
                                    &dstOffset,
                                    &fillSize);

        CmdFillMemory(pCmdBuffer, false, *pGpuMemory, dstOffset, fillSize, clearValue);
    }
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will initialize this image's meta-data of depth/stencil
void RsrcProcMgr::InitDepthClearMetaData(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& range
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    const ImageCreateInfo& createInfo = dstImage.Parent()->GetImageCreateInfo();

    // This function may be called with a range that spans any number of array slices under the perSubresInit feature.
    // The fast clear metadata is shared by all slices in the same mip level which means that re-initializing a slice
    // whos mip was fast-cleared will clobber the fast clear value and cause corruption. However, we rely on this code
    // to guarantee that our TC-compatible images stay TC-compatible so removing it will require more decompresses.
    // For now we leave this as-is, knowing we will need to fix it if we run into a game that triggers bad behavior.
    PAL_ALERT(range.numSlices < createInfo.arraySize);

    SubresRange metaDataRange;
    metaDataRange.startSubres.aspect     = range.startSubres.aspect;
    metaDataRange.startSubres.mipLevel   = range.startSubres.mipLevel;
    metaDataRange.startSubres.arraySlice = 0;
    metaDataRange.numMips                = range.numMips;
    metaDataRange.numSlices              = createInfo.arraySize;

    const uint32 metaDataInitFlags = (range.startSubres.aspect == ImageAspect::Depth) ?
                                     HtileAspectDepth : HtileAspectStencil;

    const PM4Predicate packetPredicate = static_cast<PM4Predicate>(pCmdBuffer->GetGfxCmdBufState().packetPredicate);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = dstImage.UpdateDepthClearMetaData(metaDataRange,
                                                  metaDataInitFlags,
                                                  0.0f,
                                                  0,
                                                  packetPredicate,
                                                  pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will initialize this image's meta-data of color
void RsrcProcMgr::InitColorClearMetaData(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& range
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    // This function may be called with a range that spans any number of array slices under the perSubresInit feature.
    // The fast clear metadata is shared by all slices in the same mip level which means that re-initializing a slice
    // whos mip was fast-cleared will clobber the fast clear value and cause corruption. However, we rely on this code
    // to guarantee that our TC-compatible images stay TC-compatible so removing it will require more decompresses.
    // For now we leave this as-is, knowing we will need to fix it if we run into a game that triggers bad behavior.
    PAL_ALERT(range.numSlices < dstImage.Parent()->GetImageCreateInfo().arraySize);

    const uint32 packedColor[4] = {0, 0, 0, 0};

    const PM4Predicate packetPredicate = static_cast<PM4Predicate>(pCmdBuffer->GetGfxCmdBufState().packetPredicate);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = dstImage.UpdateColorClearMetaData(range.startSubres.mipLevel,
                                                  range.numMips,
                                                  packedColor,
                                                  packetPredicate,
                                                  pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Initializes one aspect of an Image's HTile sub-allocations.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearHtileAspect(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range
    ) const
{
    auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
    PAL_ASSERT(pCmdStream != nullptr);

    const Gfx6Htile*const pHtile = dstImage.GetHtile(range.startSubres);
    PAL_ASSERT(pHtile != nullptr);
    PAL_ASSERT(pHtile->TileStencilDisabled() == false);

    // Evaluate the mask and value for updating the HTile buffer.
    uint32 htileValue = 0;
    uint32 htileMask  = 0;
    pHtile->GetAspectInitialValue(range.startSubres.aspect, &htileValue, &htileMask);

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function assumes that all mip levels must use the same Htile value and mask.
    SubresId nextMipSubres = range.startSubres;
    while (++nextMipSubres.mipLevel < (range.startSubres.mipLevel + range.numMips))
    {
        const Gfx6Htile*const pNextHtile = dstImage.GetHtile(nextMipSubres);
        PAL_ASSERT(pNextHtile != nullptr);
        PAL_ASSERT(pNextHtile->TileStencilDisabled() == false);

        uint32 nextHtileValue = 0;
        uint32 nextHtileMask  = 0;
        pNextHtile->GetAspectInitialValue(range.startSubres.aspect, &htileValue, &htileMask);
        PAL_ASSERT((htileValue == nextHtileValue) && (htileMask == nextHtileMask));
    }
#endif

    // Use the fast depth clear pipeline.
    const ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::FastDepthClear);

    // Bind the pipeline.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

    // Put the new HTile data in user data 4 and the old HTile data mask in user data 5.
    const uint32 htileUserData[2] = { htileValue & htileMask, ~htileMask };
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 4, 2, htileUserData);

    // For each mipmap level: create a temporary buffer object bound to the location in video memory where that
    // mip's HTile buffer resides. Then, issue a dispatch to update the HTile contents to reflect the initialized
    // state.
    const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
    for (uint32 mip = range.startSubres.mipLevel; mip <= lastMip; ++mip)
    {
        GpuMemory* pGpuMemory = nullptr;
        gpusize    offset     = 0;
        gpusize    dataSize   = 0;

        dstImage.GetHtileBufferInfo(mip,
                                    range.startSubres.arraySlice,
                                    range.numSlices,
                                    HtileBufferUsage::Init,
                                    &pGpuMemory,
                                    &offset,
                                    &dataSize);

        BufferViewInfo htileBufferView = {};
        htileBufferView.gpuAddr        = pGpuMemory->Desc().gpuVirtAddr + offset;
        htileBufferView.range          = dataSize;
        htileBufferView.stride         = sizeof(uint32);
        htileBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
        htileBufferView.swizzledFormat.swizzle =
            { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };

        BufferSrd srd = {};
        m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &htileBufferView, &srd);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 4, &srd.word0.u32All);

        // Issue a dispatch with one thread per HTile DWORD.
        const uint32 htileDwords  = static_cast<uint32>(htileBufferView.range / sizeof(uint32));
        const uint32 threadGroups = RpmUtil::MinThreadGroups(htileDwords, pPipeline->ThreadsPerGroup());
        pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
    }

    // Note: When performing a stencil-only or depth-only initialization on an Image which has both aspects, we have a
    // potential problem because the two separate aspects utilize the same HTile memory. Single-aspect initializations
    // perform a read-modify-write of HTile memory, which can cause synchronization issues later-on because no
    // resource transition is needed on the depth aspect when initializing stencil (and vice-versa). The solution
    // is to add a CS_PARTIAL_FLUSH and a Texture Cache Flush after executing a single-aspect initialization.
    if (pHtile->GetHtileContents() == HtileContents::DepthStencil)
    {
        regCP_COHER_CNTL cpCoherCntl;
        cpCoherCntl.u32All = CpCoherCntlTexCacheMask;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildGenericSync(cpCoherCntl,
                                                SURFACE_SYNC_ENGINE_ME,
                                                FullSyncBaseAddr,
                                                FullSyncSize,
                                                pCmdStream->GetEngineType() == EngineTypeCompute,
                                                pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Performs a fast color clear eliminate blt on the provided Image.
void RsrcProcMgr::FastClearEliminate(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    const bool    alwaysFce    = TestAnyFlagSet(m_pDevice->Settings().gfx8AlwaysDecompress, DecompressFastClear);

    const GpuMemory* pGpuMem = nullptr;
    gpusize metaDataOffset = alwaysFce ? 0 : image.GetFastClearEliminateMetaDataOffset(range.startSubres.mipLevel);
    if (metaDataOffset)
    {
        pGpuMem = image.Parent()->GetBoundGpuMemory().Memory();
        metaDataOffset += image.Parent()->GetBoundGpuMemory().Offset();
    }

    // Execute a generic CB blit using the fast-clear Eliminate pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range, *pMsaaState,
                     pQuadSamplePattern, RpmGfxPipeline::FastClearElim, pGpuMem, metaDataOffset);

    // Clear the FCE meta data over the given range because those mips must now be FCEd.
    if (image.GetFastClearEliminateMetaDataAddr(0) != 0)
    {
        const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
            pCmdBuffer->GetGfxCmdBufState().packetPredicate);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = image.UpdateFastClearEliminateMetaData(range, 0, packetPredicate, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Performs an FMask decompress blt on the provided Image.
void RsrcProcMgr::FmaskDecompress(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    // Only MSAA Images should ever need an FMask Decompress and they only support a single mipmap level.
    PAL_ASSERT((range.startSubres.mipLevel == 0) && (range.numMips == 1));

    // Execute a generic CB blit using the appropriate FMask Decompress pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range, *pMsaaState,
                     pQuadSamplePattern, RpmGfxPipeline::FmaskDecompress, nullptr, 0);

    // Clear the FCE meta data over the given range because an FMask decompress implies a FCE.
    if (image.GetFastClearEliminateMetaDataAddr(0) != 0)
    {
        const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
            pCmdBuffer->GetGfxCmdBufState().packetPredicate);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = image.UpdateFastClearEliminateMetaData(range, 0, packetPredicate, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Performs a DCC decompress blt using the compute engine on the provided Image.  It is the caller's responsibility
// to verify that the specified "range" supports texture compatability.
void RsrcProcMgr::DccDecompressOnCompute(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       image,
    const SubresRange& range
    ) const
{
    const MipDccStateMetaData  zero = {};

    const auto&  device            = *m_pDevice->Parent();
    const auto&  parentImg         = *image.Parent();
    const auto*  pPipeline         = GetComputeMaskRamExpandPipeline(parentImg);
    auto*        pComputeCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
    uint32*      pComputeCmdSpace  = nullptr;
    const auto&  createInfo        = parentImg.GetImageCreateInfo();

    // If this trips, we have a big problem...
    PAL_ASSERT(pComputeCmdStream != nullptr);

    // Compute the number of thread groups needed to launch one thread per texel.
    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

    const uint32 lastMip    = range.startSubres.mipLevel + range.numMips - 1;
    bool         earlyExit  = false;

    for (uint32  mipLevel = range.startSubres.mipLevel; ((earlyExit == false) && (mipLevel <= lastMip)); mipLevel++)
    {
        const SubresId              mipBaseSubResId = { range.startSubres.aspect, mipLevel, 0 };
        const SubResourceInfo*const pBaseSubResInfo = image.Parent()->SubresourceInfo(mipBaseSubResId);

        // Blame the caller if this trips...
        PAL_ASSERT(pBaseSubResInfo->flags.supportMetaDataTexFetch);

        const uint32  threadGroupsX = RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.width,
                                                               threadsPerGroup[0]);
        const uint32  threadGroupsY = RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.height,
                                                               threadsPerGroup[1]);
        const uint32 constData[] =
        {
            // start cb0[0]
            pBaseSubResInfo->extentElements.width,
            pBaseSubResInfo->extentElements.height,
        };

        const uint32 sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));

        for (uint32  sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
        {
            const SubresId     subResId =  { mipBaseSubResId.aspect,
                                             mipBaseSubResId.mipLevel,
                                             range.startSubres.arraySlice + sliceIdx };
            const SubresRange  viewRange = { subResId, 1, 1 };

            // Create an embedded user-data table and bind it to user data 0. We will need two views.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                        2 * SrdDwordAlignment() + sizeConstDataDwords,
                                                                        SrdDwordAlignment(),
                                                                        PipelineBindPoint::Compute,
                                                                        0);

            ImageViewInfo imageView[2] = {};
            RpmUtil::BuildImageViewInfo(
                &imageView[0], parentImg, viewRange, createInfo.swizzledFormat, false, device.TexOptLevel()); // src
            RpmUtil::BuildImageViewInfo(
                &imageView[1], parentImg, viewRange, createInfo.swizzledFormat, true, device.TexOptLevel());  // dst
            device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

            pSrdTable += 2 * SrdDwordAlignment();
            memcpy(pSrdTable, constData, sizeof(constData));

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
        } // end loop through all the slices

        // We have to mark this mip level as actually being DCC decompressed
        pComputeCmdSpace = pComputeCmdStream->ReserveCommands();
        pComputeCmdSpace += m_cmdUtil.BuildWriteData(image.GetDccStateMetaDataAddr(mipLevel),
                                                     NumBytesToNumDwords(sizeof(MipDccStateMetaData)),
                                                     0,     // engine select, ignored for compute
                                                     WRITE_DATA_DST_SEL_MEMORY_ASYNC,
                                                     1,     // write confirm
                                                     reinterpret_cast<const uint32*>(&zero),
                                                     PredDisable,
                                                     pComputeCmdSpace);
        pComputeCmdStream->CommitCommands(pComputeCmdSpace);
    }

    // Make sure that the decompressed image data has been written before we start fixing up DCC memory.
    pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
    pComputeCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pComputeCmdSpace);
    pComputeCmdStream->CommitCommands(pComputeCmdSpace);

    // Put DCC memory itself back into a "fully decompressed" state.
    ClearDcc(pCmdBuffer, pCmdStream, image, range, Gfx6Dcc::InitialValue, DccClearPurpose::Init);

    // And let the DCC fixup finish as well
    pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
    pComputeCmdSpace += m_cmdUtil.BuildEventWrite(CS_PARTIAL_FLUSH, pComputeCmdSpace);
    pComputeCmdStream->CommitCommands(pComputeCmdSpace);

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Performs a DCC decompress blt on the provided Image.
void RsrcProcMgr::DccDecompress(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    const auto*  pParentImg = image.Parent();

    // Just because a subresource has DCC memory doesn't mean that it's actually being used. We only need to decompress
    // the DCC surfaces that can actually been used. Compute the range subset that actually needs to be decompressed.
    SubresRange decompressRange = range;
    SubresId    subResource     = { ImageAspect::Color, 0, 0 };

    const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
    for (subResource.mipLevel = range.startSubres.mipLevel; subResource.mipLevel <= lastMip; ++subResource.mipLevel)
    {
        // Note that all of the array levels associated with this subresource will be the same in terms of DCC usage
        // so there's no need to look at all of them.
        if (image.GetDcc(subResource)->IsCompressionEnabled() == false)
        {
            // If this mip doesn't use DCC memory, compute the number of mips to decompress and exit the loop. We can
            // do this because none of the subsequent mips will use DCC memory if this one doesn't.
            decompressRange.numMips = subResource.mipLevel - range.startSubres.mipLevel;
            break;
        }
    }

    if (decompressRange.numMips > 0)
    {
        const auto&  settings            = m_pDevice->Settings();
        const bool   supportsComputePath = image.SupportsComputeDecompress(range.startSubres);

        if ((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
            (supportsComputePath && (TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways))))
        {
            // We should have already done a fast-clear-eliminate on the graphics engine when we transitioned to
            // whatever state we're now transitioning out of, so there's no need to do that again.
            DccDecompressOnCompute(pCmdBuffer, pCmdStream, image, decompressRange);
        }
        else
        {
            const bool alwaysDecompress = TestAnyFlagSet(settings.gfx8AlwaysDecompress, DecompressDcc);

            const GpuMemory* pGpuMem = nullptr;
            gpusize metaDataOffset = alwaysDecompress ? 0 :
                                     image.GetDccStateMetaDataOffset(decompressRange.startSubres.mipLevel);
            if (metaDataOffset)
            {
                pGpuMem = image.Parent()->GetBoundGpuMemory().Memory();
                metaDataOffset += image.Parent()->GetBoundGpuMemory().Offset();
            }

            // Execute a generic CB blit using the appropriate DCC decompress pipeline.
            GenericColorBlit(pCmdBuffer, *pParentImg, decompressRange, *pMsaaState,
                             pQuadSamplePattern, RpmGfxPipeline::DccDecompress, pGpuMem, metaDataOffset);

            // Clear the FCE meta data over the given range because a DCC decompress implies a FCE. Note that it doesn't
            // matter that we're using the truncated range here because we mips that don't use DCC shouldn't need a FCE
            // because they must be slow cleared.
            if (image.GetFastClearEliminateMetaDataAddr(0) != 0)
            {
                const PM4Predicate packetPredicate = static_cast<PM4Predicate>(
                    pCmdBuffer->GetGfxCmdBufState().packetPredicate);

                uint32* pCmdSpace = pCmdStream->ReserveCommands();
                pCmdSpace = image.UpdateFastClearEliminateMetaData(decompressRange, 0, packetPredicate, pCmdSpace);
                pCmdStream->CommitCommands(pCmdSpace);
            }
        }
    }
}

// =====================================================================================================================
// Performs an MSAA color expand using FMask. It is assumed that the FMask has already been decompressed and the cache
// flushed prior to calling this function.
void RsrcProcMgr::FmaskColorExpand(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       image,
    const SubresRange& range
    ) const
{
    // MSAA images can only have 1 mip level.
    PAL_ASSERT((range.startSubres.mipLevel == 0) && (range.numMips == 1));

    const auto& device     = *m_pDevice->Parent();
    const auto& createInfo = image.Parent()->GetImageCreateInfo();

    const uint32 log2Fragments = Log2(createInfo.fragments);
    const uint32 log2Samples   = Log2(createInfo.samples);

    const uint32 numFmaskBits = RpmUtil::CalculatNumFmaskBits(createInfo.fragments, createInfo.samples);

    // For single fragment images, we simply need to fixup the FMask.
    if (createInfo.fragments == 1)
    {
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
        ClearFmask(pCmdBuffer, image, range, Gfx6Fmask::GetPackedExpandedValue(image));
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
    else
    {
        // Select the correct pipeline for the given number of fragments.
        const ComputePipeline* pPipeline = nullptr;
        switch (createInfo.fragments)
        {
        case 2:
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskExpand2x);
            break;

        case 4:
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskExpand4x);
            break;

        case 8:
            pPipeline = GetPipeline(RpmComputePipeline::MsaaFmaskExpand8x);
            break;

        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        PAL_ASSERT(pPipeline != nullptr);

        // Compute the number of thread groups needed to launch one thread per texel.
        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        const uint32 threadGroupsX = RpmUtil::MinThreadGroups(createInfo.extent.width,  threadsPerGroup[0]);
        const uint32 threadGroupsY = RpmUtil::MinThreadGroups(createInfo.extent.height, threadsPerGroup[1]);

        // Save current command buffer state and bind the pipeline.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, });

        // Select the appropriate value to indicate that FMask is fully expanded and place it in user data 8-9.
        // Put the low part is user data 8 and the high part in user data 9.
        // The fmask bits is placed in user data 10
        const uint32 expandedValueData[3] =
        {
            LowPart(FmaskExpandedValues[log2Fragments][log2Samples]),
            HighPart(FmaskExpandedValues[log2Fragments][log2Samples]),
            numFmaskBits
        };

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, 3, expandedValueData);

        // Because we are setting up the MSAA surface as a 3D UAV, we need to have a separate dispatch for each slice.
        SubresRange  viewRange = { range.startSubres, 1, 1 };
        const uint32 lastSlice = range.startSubres.arraySlice + range.numSlices - 1;

        SwizzledFormat format   = createInfo.swizzledFormat;
        // For srgb we will get wrong data for gamma correction, here we use unorm instead.
        if (Formats::IsSrgb(format.format))
        {
            format.format = Formats::ConvertToUnorm(format.format);
        }

        for (; viewRange.startSubres.arraySlice <= lastSlice; ++viewRange.startSubres.arraySlice)
        {
            // Create an embedded user-data table and bind it to user data 0. We will need two views.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment() * 2,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            // Populate the table with and image view and an FMask view for the current slice.
            ImageViewInfo imageView = {};
            RpmUtil::BuildImageViewInfo(&imageView, *image.Parent(), viewRange, format, true, device.TexOptLevel());
            imageView.viewType = ImageViewType::Tex2d;

            device.CreateImageViewSrds(1, &imageView, pSrdTable);
            pSrdTable += SrdDwordAlignment();

            FmaskViewInfo fmaskView = {};
            fmaskView.pImage               = image.Parent();
            fmaskView.baseArraySlice       = viewRange.startSubres.arraySlice;
            fmaskView.arraySize            = 1;
            fmaskView.flags.shaderWritable = 1;

            FmaskViewInternalInfo fmaskViewInternal = {};
            fmaskViewInternal.flags.fmaskAsUav = 1;

            m_pDevice->CreateFmaskViewSrds(1, &fmaskView, &fmaskViewInternal, pSrdTable);

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
        }

        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
}

// =====================================================================================================================
// Optimize some registers to make the graphics copy run faster. Returns a mask that tells HwlEndGraphicsCopy what
// registers to restore.
uint32 RsrcProcMgr::HwlBeginGraphicsCopy(
    Pal::GfxCmdBuffer*           pCmdBuffer,
    const Pal::GraphicsPipeline* pPipeline,
    const Pal::Image&            dstImage,
    uint32                       bpp
    ) const
{
    uint32 modifiedMask = 0;

    Pal::CmdStream*const pCmdStream     = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    const GpuMemory*     pGpuMem        = dstImage.GetBoundGpuMemory().Memory();
    const auto&          chipProps      = m_pDevice->Parent()->ChipProperties();
    const auto&          coreSettings   = m_pDevice->CoreSettings();
    auto*const           pGfx6CmdStream = static_cast<CmdStream*>(pCmdStream);
    uint32*              pCmdSpace      = pGfx6CmdStream->ReserveCommands();

    if (pGpuMem != nullptr)
    {
        const GpuHeap firstHeap = pGpuMem->Heap(0);

        if ((((firstHeap == GpuHeapGartUswc) || (firstHeap == GpuHeapGartCacheable)) ||
             (pGpuMem->IsPeer())) &&
            (coreSettings.nonlocalDestGraphicsCopyRbs >= 0))
        {
            // Writes optimized PA_SC_RASTER_CONFIG registers for copy to nonlocal destination. Raster config registers
            // are in command buffer preamble, so we must restore them if they are modified after the copy is done.

            regPA_SC_RASTER_CONFIG           paScRasterConfig;
            regPA_SC_RASTER_CONFIG_1__CI__VI paScRasterConfig1;
            paScRasterConfig.u32All  = chipProps.gfx6.paScRasterCfg;
            paScRasterConfig1.u32All = chipProps.gfx6.paScRasterCfg1;

            uint32 numExpectedRbs = coreSettings.nonlocalDestGraphicsCopyRbs;

            // 0 means driver chooses the optimal number of RBs.
            if (numExpectedRbs == 0)
            {
                const uint32 numActiveRbs = chipProps.gfx6.numActiveRbs;

                // The performance tests show that on a PCI express Gen 3 platform, using 2 RBs for <= 32 bpp image has
                // best performance while it is not always true on Gen 2 platform. Without knowing the PCI express
                // version, we have to limit using 2 RBs for 16 bpp on 8-RB asics.
                if ((numActiveRbs >= 8) && (bpp == 16))
                {
                    numExpectedRbs = 2;
                }
                // On Kaveri, using 2 RB for <= 16 bpp has better performance.
                else if ((chipProps.gpuType == GpuType::Integrated) && (numActiveRbs <= 2) && (bpp <= 16))
                {
                    numExpectedRbs = numActiveRbs;
                }
                else
                {
                    numExpectedRbs = 1;
                }
            }

            // Starting from choosing 1 RB
            uint32 selectedRbs = 1;

            if (chipProps.gfxLevel > GfxIpLevel::GfxIp6)
            {
                const uint32 sePairMap = paScRasterConfig1.bits.SE_PAIR_MAP;

                // Select single shader engine pair.
                if ((sePairMap == RASTER_CONFIG_SE_PAIR_MAP_1) || (sePairMap == RASTER_CONFIG_SE_PAIR_MAP_2))
                {
                    if (selectedRbs < numExpectedRbs)
                    {
                        selectedRbs <<= 1;
                    }
                    else
                    {
                        paScRasterConfig1.bits.SE_PAIR_MAP = RASTER_CONFIG_SE_PAIR_MAP_0;

                        pCmdSpace = pGfx6CmdStream->WriteSetOneContextReg(mmPA_SC_RASTER_CONFIG_1__CI__VI,
                                                                          paScRasterConfig1.u32All,
                                                                          pCmdSpace);
                        modifiedMask |= PaScRasterConfig1ModifiedMask;
                    }
                }
            }

            // Select single shader engine.
            const uint32 seMap = paScRasterConfig.bits.SE_MAP;

            if ((seMap == RASTER_CONFIG_SE_MAP_1) || (seMap == RASTER_CONFIG_SE_MAP_2))
            {
                if (selectedRbs < numExpectedRbs)
                {
                    selectedRbs <<= 1;
                }
                else
                {
                    // Select SE0 if SE_MAP has two shader engines enabled.
                    paScRasterConfig.bits.SE_MAP = RASTER_CONFIG_SE_MAP_0;
                    modifiedMask |= PaScRasterConfigModifiedMask;
                }
            }

            // Select single packer.
            const uint32 pkrMap = paScRasterConfig.bits.PKR_MAP;

            if ((pkrMap == RASTER_CONFIG_PKR_MAP_1) || (pkrMap == RASTER_CONFIG_PKR_MAP_2))
            {
                if (selectedRbs < numExpectedRbs)
                {
                    selectedRbs <<= 1;
                }
                else
                {
                    // Select PKR0 if PKR_MAP has two packers enabled.
                    paScRasterConfig.bits.PKR_MAP = RASTER_CONFIG_PKR_MAP_0;
                    modifiedMask |= PaScRasterConfigModifiedMask;
                }
            }

            // Select single render backend for PKR0, has no effect if PKR0 is disabled.
            const uint32 pkr0RbMap = paScRasterConfig.bits.RB_MAP_PKR0;

            if ((pkr0RbMap == RASTER_CONFIG_RB_MAP_1) || (pkr0RbMap == RASTER_CONFIG_RB_MAP_2))
            {
                if (selectedRbs < numExpectedRbs)
                {
                    selectedRbs <<= 1;
                }
                else
                {
                    // If both RBs are enabled, select PKR0_RB0.
                    paScRasterConfig.bits.RB_MAP_PKR0 = RASTER_CONFIG_RB_MAP_0;
                    modifiedMask |= PaScRasterConfigModifiedMask;
                }
            }

            // Select single render backend for PKR1, has no effect if PKR1 is disabled
            const uint32 pkr1RbMap = paScRasterConfig.bits.RB_MAP_PKR1;

            if ((pkr1RbMap == RASTER_CONFIG_RB_MAP_1) || (pkr1RbMap == RASTER_CONFIG_RB_MAP_2))
            {
                if (selectedRbs < numExpectedRbs)
                {
                    selectedRbs <<= 1;
                }
                else
                {
                    // If both RBs are enabled, select PKR1_RB0.
                    paScRasterConfig.bits.RB_MAP_PKR1 = RASTER_CONFIG_RB_MAP_0;
                    modifiedMask |= PaScRasterConfigModifiedMask;
                }
            }

            if (TestAnyFlagSet(modifiedMask, PaScRasterConfigModifiedMask))
            {
                pCmdSpace = pGfx6CmdStream->WriteSetPaScRasterConfig(paScRasterConfig, pCmdSpace);
            }
        }
    }

    pGfx6CmdStream->CommitCommands(pCmdSpace);

    // CreateCopyStates does not specify CompoundStateCrateInfo.pTriangleRasterParams and it is set here. Because we
    // don't know the destination image tiling until something is being copied.
    const TriangleRasterStateParams triangleRasterState =
    {
        FillMode::Solid,        // fillMode
        CullMode::None,         // cullMode
        FaceOrientation::Cw,    // frontFace
        ProvokingVertex::First  // provokingVertex
    };

    const bool optimizeLinearDestGfxCopy = RsrcProcMgr::OptimizeLinearDestGraphicsCopy &&
                                           (dstImage.GetImageCreateInfo().tiling == ImageTiling::Linear);

    static_cast<UniversalCmdBuffer*>(pCmdBuffer)->CmdSetTriangleRasterStateInternal(triangleRasterState,
                                                                                    optimizeLinearDestGfxCopy);

    return modifiedMask;
}

// =====================================================================================================================
// Restore the registers that HwlBeginGraphicsCopy has modified.
void RsrcProcMgr::HwlEndGraphicsCopy(
    Pal::CmdStream* pCmdStream,
    uint32          restoreMask
    ) const
{
    const auto& chipProps      = m_pDevice->Parent()->ChipProperties();
    auto*const  pGfx6CmdStream = static_cast<CmdStream*>(pCmdStream);
    uint32*     pCmdSpace      = pGfx6CmdStream->ReserveCommands();

    if (TestAnyFlagSet(restoreMask, PaScRasterConfigModifiedMask))
    {
        regPA_SC_RASTER_CONFIG paScRasterConfig;
        paScRasterConfig.u32All = chipProps.gfx6.paScRasterCfg;
        pCmdSpace = pGfx6CmdStream->WriteSetPaScRasterConfig(paScRasterConfig, pCmdSpace);
    }

    if (TestAnyFlagSet(restoreMask, PaScRasterConfig1ModifiedMask))
    {
        pCmdSpace = pGfx6CmdStream->WriteSetOneContextReg(mmPA_SC_RASTER_CONFIG_1__CI__VI,
                                                          chipProps.gfx6.paScRasterCfg1,
                                                          pCmdSpace);
    }

    pGfx6CmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Attempts to recover the original PAL format and subresource range from the given image view SRD.
void RsrcProcMgr::HwlDecodeImageViewSrd(
    const void*       pImageViewSrd,
    const Pal::Image& dstImage,
    SwizzledFormat*   pSwizzledFormat,
    SubresRange*      pSubresRange
    ) const
{
    const auto& srd = *static_cast<const ImageSrd*>(pImageViewSrd);

    // Verify that we have an image view SRD.
    PAL_ASSERT((srd.word3.bits.TYPE >= SQ_RSRC_IMG_1D) && (srd.word3.bits.TYPE <= SQ_RSRC_IMG_2D_MSAA_ARRAY));

    pSwizzledFormat->format    = FmtFromHwImgFmt(static_cast<IMG_DATA_FORMAT>(srd.word1.bits.DATA_FORMAT),
                                                 static_cast<IMG_NUM_FORMAT>(srd.word1.bits.NUM_FORMAT),
                                                 m_pDevice->Parent()->ChipProperties().gfxLevel);
    pSwizzledFormat->swizzle.r = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_X));
    pSwizzledFormat->swizzle.g = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_Y));
    pSwizzledFormat->swizzle.b = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_Z));
    pSwizzledFormat->swizzle.a = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_W));

    // Verify that we have a valid format.
    PAL_ASSERT(pSwizzledFormat->format != ChNumFormat::Undefined);

    // Next, recover the original subresource range. We can't recover the exact range in all cases so we must assume
    // that it's looking at the color aspect and that it's not block compressed.
    PAL_ASSERT(Formats::IsBlockCompressed(pSwizzledFormat->format) == false);

    pSubresRange->startSubres.aspect = ImageAspect::Color;

    const auto& imageCreateInfo = dstImage.GetImageCreateInfo();
    if (Formats::IsYuv(imageCreateInfo.swizzledFormat.format))
    {
        if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            // For Planar YUV, loop through each plane and compare the address with SRD to determine which subresrouce
            // this SRD represents.
            for (uint32 i = 0; i < dstImage.GetImageInfo().numSubresources; ++i)
            {
                const auto*    pTileInfo   = AddrMgr1::GetTileInfo(&dstImage, i);
                const auto     srdBaseAddr = (static_cast<gpusize>(srd.word1.bits.BASE_ADDRESS_HI) << 32ull) +
                                              srd.word0.bits.BASE_ADDRESS;
                const gpusize  subResAddr  = Get256BAddrSwizzled(dstImage.GetSubresourceBaseAddr(i),
                                                                 pTileInfo->tileSwizzle);

                if (srdBaseAddr == subResAddr)
                {
                    pSubresRange->startSubres.aspect = dstImage.SubresourceInfo(i)->subresId.aspect;
                    break;
                }
            }

            PAL_ASSERT(pSubresRange->startSubres.aspect != ImageAspect::Color);
        }
        else
        {
            // For Packed YUV, it is always subresource 0
            pSubresRange->startSubres.aspect = dstImage.SubresourceInfo(0)->subresId.aspect;
        }
    }

    // The PAL interface can not individually address the slices of a 3D resource.  "numSlices==1" is assumed to
    // mean all of them and we have to start from the first slice.
    if (dstImage.GetImageCreateInfo().imageType == ImageType::Tex3d)
    {
        pSubresRange->numSlices              = 1;
        pSubresRange->startSubres.arraySlice = 0;
    }
    else
    {
        pSubresRange->numSlices              = srd.word5.bits.LAST_ARRAY - srd.word5.bits.BASE_ARRAY + 1;
        pSubresRange->startSubres.arraySlice = srd.word5.bits.BASE_ARRAY;
    }

    if (srd.word3.bits.TYPE == SQ_RSRC_IMG_2D_MSAA_ARRAY)
    {
        // MSAA textures cannot be mipmapped; the BASE_LEVEL and LAST_LEVEL fields indicate the texture's sample count.
        pSubresRange->startSubres.mipLevel = 0;
        pSubresRange->numMips              = 1;
    }
    else
    {
        pSubresRange->startSubres.mipLevel = srd.word3.bits.BASE_LEVEL;
        pSubresRange->numMips              = srd.word3.bits.LAST_LEVEL - srd.word3.bits.BASE_LEVEL + 1;
    }
}

// =====================================================================================================================
// Attempts to recover the original PAL BufferViewInfo from the given buffer view SRD.
void RsrcProcMgr::HwlDecodeBufferViewSrd(
    const void*     pBufferViewSrd,
    BufferViewInfo* pViewInfo) const
{
    const auto& srd      = *static_cast<const BufferSrd*>(pBufferViewSrd);
    const auto  gfxLevel = m_pDevice->Parent()->ChipProperties().gfxLevel;

    // Verify that we have a buffer view SRD.
    PAL_ASSERT(srd.word3.bits.TYPE == SQ_RSRC_BUF);

    // Reconstruct the buffer view info struct.
    pViewInfo->gpuAddr = (static_cast<gpusize>(srd.word1.bits.BASE_ADDRESS_HI) << 32ull) + srd.word0.bits.BASE_ADDRESS;
    pViewInfo->stride  = srd.word1.bits.STRIDE;

    // On GFX8+ GPUs, the units of the "num_records" field are always in terms of bytes; otherwise, if the stride is
    // non-zero, the units are in terms of the stride.
    pViewInfo->range = srd.word2.bits.NUM_RECORDS;

    if ((gfxLevel < GfxIpLevel::GfxIp8) && (pViewInfo->stride > 0))
    {
        pViewInfo->range *= pViewInfo->stride;
    }

    pViewInfo->swizzledFormat.format    = FmtFromHwBufFmt(static_cast<BUF_DATA_FORMAT>(srd.word3.bits.DATA_FORMAT),
                                                          static_cast<BUF_NUM_FORMAT>(srd.word3.bits.NUM_FORMAT),
                                                          m_pDevice->Parent()->ChipProperties().gfxLevel);
    pViewInfo->swizzledFormat.swizzle.r =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_X));
    pViewInfo->swizzledFormat.swizzle.g =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_Y));
    pViewInfo->swizzledFormat.swizzle.b =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_Z));
    pViewInfo->swizzledFormat.swizzle.a =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(srd.word3.bits.DST_SEL_W));

    // Verify that we have a valid format.
    PAL_ASSERT(pViewInfo->swizzledFormat.format != ChNumFormat::Undefined);
}

} // Gfx6
} // Pal
