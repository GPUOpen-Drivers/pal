/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "core/hw/gfxip/colorBlendState.h"
#include "core/hw/gfxip/computePipeline.h"
#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/graphicsPipeline.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/msaaState.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilView.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9UniversalCmdBuffer.h"
#include "core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.h"
#include "core/hw/gfxip/rpm/rpmUtil.h"
#include "palAutoBuffer.h"
#include "palDepthStencilView.h"
#include "palGpuMemory.h"

#include <float.h>

using namespace Pal::Formats::Gfx9;
using namespace Util;

namespace Pal
{
namespace Gfx9
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
        uint32 noWait            : 1;
        uint32 onlyPrimNeeded    : 1;
        uint32 reserved          : 25;
    };
    uint32     value;
};

// Constants that hint which registers HwlBeginGraphicsCopy modified.
constexpr uint32 PaScTileSteeringOverrideMask  = 0x1;

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
    const Image& gfx9Image      = static_cast<const Image&>(dstImage);
    const auto&  settings       = GetGfx9Settings(*(gfx9Image.Parent()->GetDevice()));
    const auto&  createInfo     = gfx9Image.Parent()->GetImageCreateInfo();
    const bool   isMultiSample  = (createInfo.samples > 1);
    const uint32 imagePixelSize = createInfo.extent.width *
                                  createInfo.extent.height *
                                  createInfo.extent.depth;
    // According to the experiment at the Vega10, compute and graphics clear has a
    // performance critical point, the critical value is 2048*2048 image size for
    // multiple sample image, and 1024*2048 image size for single sample image.
    const uint32 imagePixelCriticalSize = isMultiSample ?
                                          settings.depthStencilFastClearComputeThresholdMultiSampled :
                                          settings.depthStencilFastClearComputeThresholdSingleSampled;

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
    TwoCompGreenRed
};

// =====================================================================================================================
// This method implements the helper function called CompSetting() for the shader export mode derivation algorithm
static CompSetting ComputeCompSetting(
    ColorFormat    hwColorFmt,
    SwizzledFormat format)
{
    CompSetting       compSetting = CompSetting::Invalid;
    const SurfaceSwap surfSwap    = ColorCompSwap(format);

    switch (hwColorFmt)
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
// Derives the hardware pixel shader export format for a particular RT view slot.  Value should be used to determine
// programming for SPI_SHADER_COL_FORMAT.
//
//
// Currently, we always use the default setting as specified in the spreadsheet, ignoring the optional settings.
const SPI_SHADER_EX_FORMAT RsrcProcMgr::DeterminePsExportFmt(
    SwizzledFormat format,
    bool           blendEnabled,
    bool           shaderExportsAlpha,
    bool           blendSrcAlphaToColor,
    bool           enableAlphaToCoverage
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    const bool isUnorm = Formats::IsUnorm(format.format);
    const bool isSnorm = Formats::IsSnorm(format.format);
    const bool isFloat = Formats::IsFloat(format.format);
    const bool isUint  = Formats::IsUint(format.format);
    const bool isSint  = Formats::IsSint(format.format);
    const bool isSrgb  = Formats::IsSrgb(format.format);

    const uint32      maxCompSize = Formats::MaxComponentBitCount(format.format);
    const ColorFormat hwColorFmt  = m_pDevice->GetHwColorFmt(format);
    PAL_ASSERT(hwColorFmt != COLOR_INVALID);

    bool foundSwizzles[4] = {};
    SwizzledFormat pipelineFormat = format;
    for (uint32 i = 0; i < 4; i++)
    {
        const ChannelSwizzle swizzle = pipelineFormat.swizzle.swizzle[i];
        if ((uint32(ChannelSwizzle::X) <= uint32(swizzle)) &&
            (uint32(swizzle) <= uint32(ChannelSwizzle::W)))
        {
            const uint32 swizzleIndex = uint32(swizzle) - uint32(ChannelSwizzle::X);
            if (foundSwizzles[swizzleIndex] == false)
            {
                foundSwizzles[swizzleIndex] = true;
            }
            else
            {
                pipelineFormat.swizzle.swizzle[i] = ChannelSwizzle::Zero;
            }
        }
    }

    const CompSetting compSetting = ComputeCompSetting(hwColorFmt, pipelineFormat);
    const bool hasAlpha = Formats::HasAlpha(pipelineFormat);
    const bool isDepth  = ((hwColorFmt == COLOR_8_24) ||
                           (hwColorFmt == COLOR_24_8) ||
                           (hwColorFmt == COLOR_X24_8_32_FLOAT));

    const bool alphaExport = (shaderExportsAlpha && (hasAlpha || blendSrcAlphaToColor || enableAlphaToCoverage));

    // Start by assuming SPI_FORMAT_ZERO (no exports).
    SPI_SHADER_EX_FORMAT spiShaderExFormat = SPI_SHADER_ZERO;

    if ((compSetting == CompSetting::OneCompRed) &&
        (alphaExport == false)                   &&
        (isSrgb == false)                        &&
        ((chipProps.gfx9.rbPlus == 0) || (maxCompSize == 32)))
    {
        // When RBPlus is enalbed, R8-UNORM and R16 UNORM shouldn't use SPI_SHADER_32_R, instead SPI_SHADER_FP16_ABGR
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
    else if (isSint && (maxCompSize <= 16) && (enableAlphaToCoverage == false))
    {
        // 8bpp SINT is supposed to be use SPI_SHADER_SINT16_ABGR per HW document
        spiShaderExFormat = SPI_SHADER_SINT16_ABGR;
    }
    else if (isSnorm && (maxCompSize == 16) && (blendEnabled == false))
    {
        spiShaderExFormat = SPI_SHADER_SNORM16_ABGR;
    }
    else if (isUint && (maxCompSize <= 16) && (enableAlphaToCoverage == false))
    {
        // 8bpp UINT is supposed to be use SPI_SHADER_UINT16_ABGR per HW document
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
// The function checks HW specific conditions to see if allow clone copy,
//   - For both image with metadata case, if source image's layout is compatible with dst image's layout.
bool RsrcProcMgr::UseImageCloneCopy(
    GfxCmdBuffer*          pCmdBuffer,
    const Pal::Image&      srcImage,
    ImageLayout            srcImageLayout,
    const Pal::Image&      dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags
    ) const
{
    bool useCloneCopy = Pal::RsrcProcMgr::UseImageCloneCopy(pCmdBuffer, srcImage, srcImageLayout, dstImage,
                                                            dstImageLayout, regionCount, pRegions, flags);

    // Check src image is enough as both images should have the same metadata info if useCloneCopy == true.
    if (useCloneCopy && srcImage.HasMetadata())
    {
        const auto& gfx9SrcImage = static_cast<const Image&>(*srcImage.GetGfxImage());
        const auto& gfx9DstImage = static_cast<const Image&>(*dstImage.GetGfxImage());

        if (srcImage.IsDepthStencilTarget())
        {
            const uint32 numPlanes = srcImage.GetImageInfo().numPlanes;

            // DepthStencilLayoutToState may change with different plane but not mipLevel or slice.
            // Currently clone copy only supports full copy, so loop all planes here.
            for (uint32 plane = 0; useCloneCopy && (plane < numPlanes); plane++)
            {
                const SubresId                  subres           = Subres(plane, 0, 0);
                const DepthStencilLayoutToState srcLayoutToState = gfx9SrcImage.LayoutToDepthCompressionState(subres);
                const DepthStencilLayoutToState dstLayoutToState = gfx9DstImage.LayoutToDepthCompressionState(subres);

                const DepthStencilCompressionState srcState = ImageLayoutToDepthCompressionState(srcLayoutToState,
                                                                                                 srcImageLayout);
                const DepthStencilCompressionState dstState = ImageLayoutToDepthCompressionState(dstLayoutToState,
                                                                                                 dstImageLayout);

                // Only support clone copy if source layout is compatible with destination layout.
                if (srcState != DepthStencilDecomprWithHiZ)
                {
                    useCloneCopy &= (srcState == dstState);
                }
                //else if (srcState == DepthStencilDecomprWithHiZ), always support clone copy.
            }
        }
        else
        {
            const ColorLayoutToState srcLayoutToState = gfx9SrcImage.LayoutToColorCompressionState();
            const ColorLayoutToState dstLayoutToState = gfx9DstImage.LayoutToColorCompressionState();

            const ColorCompressionState srcState = ImageLayoutToColorCompressionState(srcLayoutToState, srcImageLayout);
            const ColorCompressionState dstState = ImageLayoutToColorCompressionState(dstLayoutToState, dstImageLayout);

            // Only support clone copy if source layout is compatible with destination layout.
            useCloneCopy &= (srcState <= dstState);
        }
    }

    return useCloneCopy;
}

// =====================================================================================================================
// Clones the image data from the source image while preserving its state and avoiding decompressing.
void RsrcProcMgr::CmdCloneImageData(
    GfxCmdBuffer*     pCmdBuffer,
    const Pal::Image& srcImage,
    const Pal::Image& dstImage
    ) const
{
    const Image& gfx9SrcImage = static_cast<const Image&>(*srcImage.GetGfxImage());

    // Check our assumptions:
    // 1. Both images need to be cloneable.
    // 2. Both images must have been created with identical create info.
    // 3. Both images must have been created with identical memory layout.
    PAL_ASSERT(srcImage.IsCloneable() && dstImage.IsCloneable());
    PAL_ASSERT(srcImage.GetImageCreateInfo() == dstImage.GetImageCreateInfo());
    PAL_ASSERT(srcImage.GetGpuMemSize() == dstImage.GetGpuMemSize());

    // dstImgMemLayout metadata size comparison to srcImgMemLayout is checked by caller.
    const ImageMemoryLayout& srcImgMemLayout = srcImage.GetMemoryLayout();
    const bool               hasMetadata     = (srcImgMemLayout.metadataSize != 0);

    if (srcImgMemLayout.metadataHeaderSize != 0)
    {
        // If has metadata
        // First copy header by PFP
        // We always read and write the metadata header using the PFP so the copy must also use the PFP.
        PfpCopyMetadataHeader(pCmdBuffer,
                              dstImage.GetBoundGpuMemory().GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset,
                              srcImage.GetBoundGpuMemory().GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset,
                              static_cast<uint32>(srcImgMemLayout.metadataHeaderSize),
                              gfx9SrcImage.HasDccLookupTable());
    }

    // Do the rest copy
    // If has metadata, copy all of the source image (including metadata, excluding metadata header) to the dest image.
    // If no metadata, copy the whole memory.
    Pal::MemoryCopyRegion copyRegion = {};

    copyRegion.srcOffset = srcImage.GetBoundGpuMemory().Offset();
    copyRegion.dstOffset = dstImage.GetBoundGpuMemory().Offset();
    copyRegion.copySize  = hasMetadata ? srcImgMemLayout.metadataHeaderOffset : dstImage.GetGpuMemSize();

    CopyMemoryCs(pCmdBuffer,
                 *srcImage.GetBoundGpuMemory().Memory(),
                 *dstImage.GetBoundGpuMemory().Memory(),
                 1,
                 &copyRegion);

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
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
    Pal::CmdStream* const pStream = pCmdBuffer->GetMainCmdStream();
    PAL_ASSERT(pStream != nullptr);

    // Prepare to issue one or more DMA_DATA packets. Start the dstAddr at the beginning of the dst buffer. The srcAddr
    // and numBytes will be set in the loop.
    //
    // We want to read and write through L2 because it's faster and expected by CoherCopy.
    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel  = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel  = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.dstAddr = dstMem.Desc().gpuVirtAddr + dstOffset;
    dmaDataInfo.sync    = false;
    dmaDataInfo.usePfp  = false;

    const uint32     embeddedDataLimit = pCmdBuffer->GetEmbeddedDataLimit() * sizeof(uint32);
    constexpr uint32 EmbeddedDataAlign = 1;

    // Loop until we've submitted enough DMA_DATA packets to upload the whole src buffer.
    const void* pRemainingSrcData = pData;
    uint32      remainingDataSize = static_cast<uint32>(dataSize);
    while (remainingDataSize > 0)
    {
        // Create the embedded video memory space for the next section of the src buffer.
        dmaDataInfo.numBytes = Min(remainingDataSize, embeddedDataLimit);

        uint32* pBufStart = pCmdBuffer->CmdAllocateEmbeddedData(dmaDataInfo.numBytes / sizeof(uint32),
                                                                EmbeddedDataAlign,
                                                                &dmaDataInfo.srcAddr);

        memcpy(pBufStart, pRemainingSrcData, dmaDataInfo.numBytes);

        // Write the DMA_DATA packet to the command stream.
        uint32* pCmdSpace = pStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildDmaData<false, false>(dmaDataInfo, pCmdSpace);
        pStream->CommitCommands(pCmdSpace);

        // Update all variable addresses and sizes except for srcAddr and numBytes which will be reset above.
        pRemainingSrcData    = VoidPtrInc(pRemainingSrcData, dmaDataInfo.numBytes);
        remainingDataSize   -= dmaDataInfo.numBytes;
        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
    }

    pCmdBuffer->SetCpBltState(true);
    pCmdBuffer->SetCpBltWriteCacheState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = pCmdBuffer, .bltType = Developer::RpmBltType::CpDmaUpdate };
    m_pDevice->Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
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
    // Does not work for BinaryOcclusion.
    if ((queryType == QueryType::Occlusion) &&
        pCmdBuffer->IsGraphicsSupported()   &&
        ((flags == OptCaseWait64) || (flags == OptCaseWait64Accum)))
    {
        // Condition above would be false due to the flags check for equality:
        PAL_ASSERT((flags & QueryResultPreferShaderPath) == 0);

        Pal::CmdStream* const pStream = pCmdBuffer->GetMainCmdStream();
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

        // Note that SetCpBltState() only applies to CP DMA so we don't need to call it here.
        if (remainingResolves > 0)
        {
            pCmdBuffer->SetCpBltWriteCacheState(true);
        }

        // If QueryResultAccumulate is not set, we need to write the result to 0 first.
        const uint64 zero              = 0;
        const uint32 writeDataSize     = NumBytesToNumDwords(sizeof(zero));
        const uint32 writeDataPktSize  = CmdUtil::WriteDataSizeDwords + writeDataSize;

        const uint32 resolvePerCommit  =
            doAccumulate
            ? pStream->ReserveLimit() / CmdUtil::OcclusionQuerySizeDwords
            : pStream->ReserveLimit() / (CmdUtil::OcclusionQuerySizeDwords + writeDataPktSize);

        while (remainingResolves > 0)
        {
            // Write all of the queries or as many queries as we can fit in a reserve buffer.
            uint32  resolvesToWrite = Min(remainingResolves, resolvePerCommit);

            pCmdSpace          = pStream->ReserveCommands();
            remainingResolves -= resolvesToWrite;

            while (resolvesToWrite-- > 0)
            {
                gpusize queryPoolAddr  = 0;
                gpusize resolveDstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset + queryIndex * dstStride;
                Result  result         = queryPool.GetQueryGpuAddress(queryIndex + startQuery, &queryPoolAddr);

                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    if (doAccumulate == false)
                    {
                        WriteDataInfo writeData = {};
                        writeData.engineType = pCmdBuffer->GetEngineType();
                        writeData.dstAddr    = resolveDstAddr;
                        writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
                        writeData.dstSel     = dst_sel__pfp_write_data__memory;

                        pCmdSpace += m_cmdUtil.BuildWriteData(writeData,
                                                              writeDataSize,
                                                              reinterpret_cast<const uint32*>(&zero),
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
    auto*const pStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());
    PAL_ASSERT(pStream != nullptr);

    if (TestAnyFlagSet(flags, QueryResultWait) && queryPool.HasTimestamps())
    {
        // Wait for the query data to get to memory if it was requested.
        // The shader is required to implement the wait if the query pool doesn't have timestamps.
        queryPool.WaitForSlots(pCmdBuffer, pStream, startQuery, queryCount);
    }

    // On GFX9, we don't need to invalidate the L2, as DB writes timestamps directly to it.
    // It should be safe to launch our compute shader now. Select the correct pipeline.
    const Pal::ComputePipeline* pPipeline = nullptr;

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
    controlFlags.onlyPrimNeeded    = TestAnyFlagSet(flags, QueryResultOnlyPrimNeeded);

    uint32 constData[4]    = { controlFlags.value, queryCount, static_cast<uint32>(dstStride), 0 };
    uint32 constEntryCount = 0;

    switch (queryPool.CreateInfo().queryPoolType)
    {
    case QueryPoolType::Occlusion:
        // The occlusion query shader needs the stride of a set of zPass counters.
        pPipeline       = GetPipeline(RpmComputePipeline::ResolveOcclusionQuery);
        constData[3]    = static_cast<uint32>(queryPool.GetGpuResultSizeInBytes(1));
        constEntryCount = 4;

        PAL_ASSERT((queryType == QueryType::Occlusion) || (queryType == QueryType::BinaryOcclusion));
        break;

    case QueryPoolType::PipelineStats:
        // The pipeline stats query shader needs the mask of enabled pipeline stats.
        pPipeline       = GetPipeline(RpmComputePipeline::ResolvePipelineStatsQuery);
        constData[3]    = queryPool.CreateInfo().enabledStats;
        constEntryCount = 4;

        // Note that accumulation was not implemented for this query pool type because no clients support it.
        PAL_ASSERT(TestAnyFlagSet(flags, QueryResultAccumulate) == false);
        PAL_ASSERT(queryType == QueryType::PipelineStats);

        // Pipeline stats query doesn't implement shader-based wait.
        PAL_ASSERT(controlFlags.noWait == 1);
        break;

    case QueryPoolType::StreamoutStats:
        PAL_ASSERT((flags & QueryResultWait) != 0);

        pPipeline    = GetPipeline(RpmComputePipeline::ResolveStreamoutStatsQuery);

        constEntryCount = 3;

        PAL_ASSERT((queryType == QueryType::StreamoutStats) ||
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
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Create an embedded user-data table and bind it to user data 0-1. We need buffer views for the source and dest.
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

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, constEntryCount, constData);

    // Issue a dispatch with one thread per query slot.
    const uint32 threadGroups = RpmUtil::MinThreadGroups(queryCount, pPipeline->ThreadsPerGroup());
    pCmdBuffer->CmdDispatch({threadGroups, 1, 1}, {});

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image range by updating the Image's HTile buffer.
void RsrcProcMgr::FastDepthStencilClearComputeCommon(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::Image*  pPalImage,
    uint32             clearMask   // bitmask of HtilePlaneMask enumerations
    ) const
{
    const auto*  pHtile = static_cast<const Image*>(pPalImage->GetGfxImage())->GetHtile();

    // NOTE: When performing a stencil-only or depth-only clear on an Image which has both planes, we have a
    // potential problem because the two separate planes may utilize the same HTile memory. Single-plane clears
    // perform a read-modify-write of HTile memory, which can cause synchronization issues later-on because no
    // resource transition is needed on the depth plane when clearing stencil (and vice-versa). The solution
    // is to add a CS_PARTIAL_FLUSH and an L1 cache flush after executing a susceptible clear.
    if ((TestAllFlagsSet(clearMask, HtilePlaneDepth | HtilePlaneStencil) == false) &&
        (pPalImage->GetImageInfo().numPlanes == 2) &&
        (pHtile->TileStencilDisabled() == false))
    {
        // Note that it's not possible for us to handle all necessary synchronization corner-cases here. PAL allows our
        // clients to do things like this:
        // - Init both planes, clear then, and render to them.
        // - Transition stencil to shader read (perhaps on the compute queue).
        // - Do some additional rendering to depth only.
        // - Clear the stencil plane.
        //
        // The last two steps will populate the DB metadata caches and shader caches with conflicting HTile data.
        // We can't think of any efficient methods to handle cases like these and the inefficient methods are still
        // of questionable correctness.

        const EngineType engineType = pCmdBuffer->GetEngineType();
        auto*const       pCmdStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());

        PAL_ASSERT(pCmdStream != nullptr);

        AcquireMemGeneric acquireInfo = {};
        acquireInfo.cacheSync  = SyncGl1Inv | SyncGlvInv | SyncGlkInv;
        acquireInfo.engineType = engineType;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace  = pCmdBuffer->WriteWaitCsIdle(pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// ====================================================================================================================
// Returns the number of slices (for 2D images) or the depth of a 3D image that should be cleared for the specified
// mip level.
uint32 RsrcProcMgr::GetClearDepth(
    const Image& dstImage,
    uint32       plane,
    uint32       numSlices,
    uint32       mipLevel
    ) const
{
    const Pal::Image*  pPalImage   = dstImage.Parent();
    const auto&        createInfo  = pPalImage->GetImageCreateInfo();
    const bool         is3dImage   = (createInfo.imageType == ImageType::Tex3d);
    const SubresId     subresId    = Subres(plane, mipLevel, 0);
    const auto*        pSubResInfo = pPalImage->SubresourceInfo(subresId);

    return (is3dImage ? pSubResInfo->extentTexels.depth : numSlices);
}

// =====================================================================================================================
// Issues the dispatch call for the specified dimensions
void RsrcProcMgr::MetaDataDispatch(
    GfxCmdBuffer*      pCmdBuffer,       // command buffer used for the dispatch call
    const Gfx9MaskRam* pMaskRam,         // mask ram the dispatch will access
    uint32             width,            // width of the mip level being cleared
    uint32             height,           // height of the mip-level being cleared
    uint32             depth,            // number of slices (either array or volume slices) being cleared
    DispatchDims       threadsPerGroup)  // The number of threads per group in each dimension.
{
    // The compression ratio of image pixels into mask-ram blocks changes based on the mask-ram
    // type and image info.
    uint32  xInc = 0;
    uint32  yInc = 0;
    uint32  zInc = 0;

    pMaskRam->GetXyzInc(&xInc, &yInc, &zInc);

    // Calculate the size of the specified region in terms of the meta-block being compressed.  i.e,. an 8x8 block
    // of color pixels is a 1x1 "block" of DCC "pixels".  Remember that fractional blocks still count as a "full"'
    // block in compressed pixels.
    const uint32  x = Pow2Align(width, xInc) / xInc;
    const uint32  y = Pow2Align(height, yInc) / yInc;
    const uint32  z = Pow2Align(depth, zInc) / zInc;

    // Now that we have the dimensions in terms of compressed pixels, launch as many thread groups as we need to
    // get to them all.
    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz({x, y, z}, threadsPerGroup), {});
}

// =====================================================================================================================
// Issues a compute shader blt to initialize the Mask RAM allocatons for an Image.
// Returns "true" if the compute engine was used for the InitMaskRam operation.
bool RsrcProcMgr::InitMaskRam(
    GfxCmdBuffer*                 pCmdBuffer,
    Pal::CmdStream*               pCmdStream,
    const Image&                  dstImage,
    const SubresRange&            range,
    ImageLayout                   layout
    ) const
{
    const auto&       settings   = GetGfx9Settings(*dstImage.Parent()->GetDevice());
    const Pal::Image* pParentImg = dstImage.Parent();

    // If we're in this function, we know this surface has meta-data.  Most of the meta-data init functions use compute
    // so assume that by default.
    bool usedCompute = true;
    bool isDccInitCompressed = false;

    // If any of following conditions is met, that means we are going to use PFP engine to update the metadata
    // (e.g. UpdateColorClearMetaData(); UpdateDccStateMetaData() etc.)
    if (pCmdBuffer->IsGraphicsSupported()       &&
        (dstImage.HasDccStateMetaData(range)    ||
         dstImage.HasFastClearMetaData(range)   ||
         dstImage.HasHiSPretestsMetaData()      ||
         dstImage.HasFastClearEliminateMetaData(range)))
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
        const auto*  pHtile = dstImage.GetHtile();

        // We're transitioning out of "uninitialized" state here, so take advantage of this one-time opportunity
        // to upload the meta-equation so our upcoming compute shader knows what to do.
        PAL_ASSERT(pHtile->HasMetaEqGenerator());
        pHtile->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

        InitHtile(pCmdBuffer, pCmdStream, dstImage, range);
    }
    else
    {
        if (dstImage.HasDccData())
        {
            uint8 initialDccVal = Gfx9Dcc::DecompressedValue;
            for (SubresId subresId = range.startSubres;
                 subresId.plane < (range.startSubres.plane + range.numPlanes);
                 subresId.plane++)
            {
                const Gfx9Dcc* pDcc = dstImage.GetDcc(subresId.plane);
                initialDccVal = pDcc->GetInitialValue(layout);

                PAL_ASSERT(pDcc->HasMetaEqGenerator());
                pDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

                if (dstImage.HasDisplayDccData())
                {
                    const Gfx9Dcc* pDispDcc = dstImage.GetDisplayDcc(subresId.plane);

                    PAL_ASSERT(pDispDcc->HasMetaEqGenerator());
                    pDispDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

                }
            }
            isDccInitCompressed = (initialDccVal != Gfx9Dcc::DecompressedValue);

            const bool dccClearUsedCompute = ClearDcc(pCmdBuffer,
                                                      pCmdStream,
                                                      dstImage,
                                                      range,
                                                      initialDccVal,
                                                      DccClearPurpose::Init,
                                                      true);

            // Even if we cleared DCC using graphics, we will always clear CMask below using compute.
            usedCompute = dccClearUsedCompute || dstImage.HasFmaskData();
        }

        if (dstImage.HasFmaskData())
        {
            // If we have fMask, then we have cMask
            PAL_ASSERT(dstImage.GetCmask()->HasMetaEqGenerator());
            dstImage.GetCmask()->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

            // The docs state that we only need to initialize either cMask or fMask data.  Init the cMask data
            // since we have a meta-equation for that one.
            InitCmask(pCmdBuffer, pCmdStream, dstImage, range, dstImage.GetCmask()->GetInitialValue(), true);

            // It's possible that this image will be resolved with fMask pipeline later, so the fMask must be cleared
            // here.
            pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
            ClearFmask(pCmdBuffer, dstImage, range, Gfx9Fmask::GetPackedExpandedValue(dstImage));
            pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
        }
    }

    if (dstImage.HasFastClearMetaData(range))
    {
        if (dstImage.HasDsMetadata())
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

    if (dstImage.HasHiSPretestsMetaData() && pParentImg->HasStencilPlane(range))
    {
        ClearHiSPretestsMetaData(pCmdBuffer, pCmdStream, dstImage, range);
    }

    if (dstImage.HasDccLookupTable())
    {
        BuildDccLookupTable(pCmdBuffer, dstImage, range);
        usedCompute = true;
    }

    if (dstImage.HasDccStateMetaData(range))
    {
        // We need to initialize the Image's DCC state metadata to indicate that the Image can become DCC compressed
        // (or not) in upcoming operations.
        bool canCompress = ImageLayoutCanCompressColorData(dstImage.LayoutToColorCompressionState(), layout);

        // Client can force this, but keep dcc state coherent.
        canCompress |= isDccInitCompressed;

        // If the new layout is one which can write compressed DCC data,  then we need to update the Image's DCC state
        // metadata to indicate that the image will become DCC compressed in upcoming operations.
        dstImage.UpdateDccStateMetaData(pCmdStream,
                                        range,
                                        canCompress,
                                        pCmdBuffer->GetEngineType(),
                                        PredDisable);
    }

    // We need to initialize the Image's FCE(fast clear eliminate) metadata to ensure that if we don't perform fast clear
    // then FCE command should not be truely executed.
    if (dstImage.HasFastClearEliminateMetaData(range))
    {
        const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace = dstImage.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    return usedCompute;
}

// =====================================================================================================================
// Some blts need to use GFXIP-specific algorithms to pick the proper graphics pipeline. The basePipeline is the first
// graphics state in a series of states that vary only on target format.
const Pal::GraphicsPipeline* RsrcProcMgr::GetGfxPipelineByFormat(
    RpmGfxPipeline basePipeline,
    SwizzledFormat format
    ) const
{
    // There are only 6 ranges of pipelines that vary by export format and these are their bases.
    PAL_ASSERT((basePipeline == Gfx11ResolveGraphics_32ABGR) ||
               (basePipeline == Copy_32ABGR)                 ||
               (basePipeline == ResolveFixedFunc_32ABGR)     ||
               (basePipeline == SlowColorClear_32ABGR)      ||
               (basePipeline == ScaledCopy2d_32ABGR)         ||
               (basePipeline == ScaledCopy3d_32ABGR));

    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    const int32 pipelineOffset = ExportStateMapping[exportFormat];
    PAL_ASSERT(pipelineOffset >= 0);

    return GetGfxPipeline(static_cast<RpmGfxPipeline>(basePipeline + pipelineOffset));
}

// =====================================================================================================================
// Returns true if there is graphics pipeline that can copy specified format.
const bool RsrcProcMgr::IsGfxPipelineForFormatSupported(
    SwizzledFormat format
    ) const
{
    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    return ExportStateMapping[exportFormat] >= 0;
}

// =====================================================================================================================
// Function to expand (decompress) hTile data associated with the given image / range.  Supports use of a compute
// queue expand for ASICs that support texture compatability of depth surfaces.  Falls back to the independent layer
// implementation for other ASICs
bool RsrcProcMgr::ExpandDepthStencil(
    GfxCmdBuffer*                pCmdBuffer,
    const Pal::Image&            image,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);

    const auto&  device      = *m_pDevice->Parent();
    const auto*  pGfxImage   = reinterpret_cast<const Image*>(image.GetGfxImage());
    bool         usedCompute = false;

    if (WillDecompressDepthStencilWithCompute(pCmdBuffer, *pGfxImage, range))
    {
        const auto&       createInfo        = image.GetImageCreateInfo();
        const auto*       pPipeline         = GetComputeMaskRamExpandPipeline(image);
        const auto*       pHtile            = pGfxImage->GetHtile();
        Pal::CmdStream*   pComputeCmdStream = pCmdBuffer->GetMainCmdStream();
        const EngineType  engineType        = pCmdBuffer->GetEngineType();

        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Compute the number of thread groups needed to launch one thread per texel.
        const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

        bool earlyExit = false;
        for (uint32 mipIdx = 0; ((earlyExit == false) && (mipIdx < range.numMips)); mipIdx++)
        {
            const SubresId  mipBaseSubResId = Subres(range.startSubres.plane, range.startSubres.mipLevel + mipIdx, 0);
            const auto*     pBaseSubResInfo = image.SubresourceInfo(mipBaseSubResId);

            // a mip level may not have metadata thus supportMetaDataTexFetch is 0 and expand is not necessary at all
            if (pBaseSubResInfo->flags.supportMetaDataTexFetch == 0)
            {
                break;
            }

            const DispatchDims threadGroups =
            {
                RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.width,  threadsPerGroup.x),
                RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.height, threadsPerGroup.y),
                1
            };

            const uint32 constData[] =
            {
                // start cb0[0]
                pBaseSubResInfo->extentElements.width,
                pBaseSubResInfo->extentElements.height,
            };

            // Embed the constant buffer in user-data right after the SRD table.
            pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, ArrayLen32(constData), constData);

            SubresRange viewRange = SingleSubresRange(mipBaseSubResId);
            for (uint32 sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
            {
                viewRange.startSubres.arraySlice = uint16(range.startSubres.arraySlice + sliceIdx);

                // Create an embedded user-data table and bind it to user data 0. We will need two views.
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                           SrdDwordAlignment() * 2,
                                                                           SrdDwordAlignment(),
                                                                           PipelineBindPoint::Compute,
                                                                           0);

                ImageViewInfo imageView[2] = {};
                RpmUtil::BuildImageViewInfo(&imageView[0],
                                            image,
                                            viewRange,
                                            createInfo.swizzledFormat,
                                            RpmUtil::DefaultRpmLayoutRead,
                                            device.TexOptLevel(),
                                            false); // src
                RpmUtil::BuildImageViewInfo(&imageView[1],
                                            image,
                                            viewRange,
                                            createInfo.swizzledFormat,
                                            RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                            device.TexOptLevel(),
                                            true);  // dst
                device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

                // Execute the dispatch.
                pCmdBuffer->CmdDispatch(threadGroups, {});
            } // end loop through all the slices
        } // end loop through all the mip levels

        // Allow the rewrite of depth data to complete
        uint32* pComputeCmdSpace = pComputeCmdStream->ReserveCommands();
        pComputeCmdSpace = pCmdBuffer->WriteWaitCsIdle(pComputeCmdSpace);
        pComputeCmdStream->CommitCommands(pComputeCmdSpace);

        // Restore the compute state here as the "initHtile" function is going to push the compute state again
        // for its own purposes.
        pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
        pCmdBuffer->SetCsBltIndirectWriteMisalignedMdState(image.HasMisalignedMetadata());

        usedCompute = true;
    }
    else
    {
        // The issue could be triggered when clear non-tile aligned and non-TC compatible stencil that goes through
        // graphics fast clear path has the following steps.
        //
        // This could be a DB cache issue since it looks like DB fail to eliminate all fast clear codes when do
        // the expand operation.
        if ((image.SubresourceInfo(range.startSubres)->flags.supportMetaDataTexFetch == 0) &&
            image.IsStencilPlane(range.startSubres.plane))
        {
            Pal::CmdStream* const pCmdStream = pCmdBuffer->GetMainCmdStream();
            PAL_ASSERT(pCmdStream != nullptr);
            const EngineType engineType = pCmdBuffer->GetEngineType();
            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(DB_CACHE_FLUSH_AND_INV, engineType, pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }

        // Do the expand the legacy way.
        PAL_ASSERT(range.numPlanes == 1);
        PAL_ASSERT(image.IsDepthStencilTarget());
        PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
        // Don't expect GFX Blts on Nested unless targets not inherited.
        PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pal::UniversalCmdBuffer*>(
            pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

        const auto*                pPublicSettings  = m_pDevice->Parent()->GetPublicSettings();
        const StencilRefMaskParams stencilRefMasks  = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF };

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
        viewportInfo.depthRange            = DepthRange::ZeroToOne;

        ScissorRectParams scissorInfo      = { };
        scissorInfo.count                  = 1;
        scissorInfo.scissors[0].offset.x   = 0;
        scissorInfo.scissors[0].offset.y   = 0;

        DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };
        depthViewInfoInternal.flags.isExpand = 1;

        DepthStencilViewCreateInfo depthViewInfo = { };
        depthViewInfo.pImage              = &image;
        depthViewInfo.arraySize           = 1;
        depthViewInfo.flags.imageVaLocked = 1;
        depthViewInfo.flags.bypassMall    = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                           RpmViewsBypassMallOnCbDbWrite);

        if (image.IsDepthPlane(range.startSubres.plane))
        {
            depthViewInfo.flags.readOnlyStencil = 1;
        }
        else
        {
            depthViewInfo.flags.readOnlyDepth = 1;
        }

        BindTargetParams bindTargetsInfo = { };
        bindTargetsInfo.depthTarget.pDepthStencilView     = nullptr;
        bindTargetsInfo.depthTarget.depthLayout.usages    = LayoutDepthStencilTarget;
        bindTargetsInfo.depthTarget.depthLayout.engines   = LayoutUniversalEngine;
        bindTargetsInfo.depthTarget.stencilLayout.usages  = LayoutDepthStencilTarget;
        bindTargetsInfo.depthTarget.stencilLayout.engines = LayoutUniversalEngine;

        // Save current command buffer state and bind graphics state which is common for all subresources.
        pCmdBuffer->CmdSaveGraphicsState();
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, });
        BindCommonGraphicsState(pCmdBuffer);
        pCmdBuffer->CmdBindDepthStencilState(m_pDepthExpandState);
        pCmdBuffer->CmdBindMsaaState(GetMsaaState(image.GetImageCreateInfo().samples,
                                                  image.GetImageCreateInfo().fragments));

        if (pQuadSamplePattern != nullptr)
        {
            pCmdBuffer->CmdSetMsaaQuadSamplePattern(image.GetImageCreateInfo().samples, *pQuadSamplePattern);
        }

        pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

        RpmUtil::WriteVsZOut(pCmdBuffer, 1.0f);

        const uint32 lastMip   = (range.startSubres.mipLevel   + range.numMips   - 1);
        const uint32 lastSlice = (range.startSubres.arraySlice + range.numSlices - 1);

        for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
             depthViewInfo.mipLevel <= lastMip;
             ++depthViewInfo.mipLevel)
        {
            if (pGfxImage->CanMipSupportMetaData(depthViewInfo.mipLevel))
            {
                LinearAllocatorAuto<VirtualLinearAllocator> mipAlloc(pCmdBuffer->Allocator(), false);

                const SubresId mipSubres  = Subres(range.startSubres.plane, depthViewInfo.mipLevel, 0);
                const auto&    subResInfo = *image.SubresourceInfo(mipSubres);

                // All slices of the same mipmap level can re-use the same viewport/scissor state.
                viewportInfo.viewports[0].width  = static_cast<float>(subResInfo.extentTexels.width);
                viewportInfo.viewports[0].height = static_cast<float>(subResInfo.extentTexels.height);

                scissorInfo.scissors[0].extent.width  = subResInfo.extentTexels.width;
                scissorInfo.scissors[0].extent.height = subResInfo.extentTexels.height;

                pCmdBuffer->CmdSetViewports(viewportInfo);
                pCmdBuffer->CmdSetScissorRects(scissorInfo);

                for (depthViewInfo.baseArraySlice  = range.startSubres.arraySlice;
                     depthViewInfo.baseArraySlice <= lastSlice;
                     ++depthViewInfo.baseArraySlice)
                {
                    LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

                    // Create and bind a depth stencil view of the current subresource.
                    IDepthStencilView* pDepthView = nullptr;
                    void* pDepthViewMem =
                        PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

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

                        bindTargetsInfo.depthTarget.pDepthStencilView = pDepthView;
                        pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                        // Draw a fullscreen quad.
                        pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                        PAL_SAFE_FREE(pDepthViewMem, &sliceAlloc);

                        // Unbind the depth view and destroy it.
                        bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                        pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                    }
                }
            }
        }

        // Restore command buffer state.
        pCmdBuffer->CmdRestoreGraphicsStateInternal();
        pCmdBuffer->SetGfxBltDirectWriteMisalignedMdState(image.HasMisalignedMetadata());
    }

    return usedCompute;
}

// =====================================================================================================================
bool RsrcProcMgr::WillDecompressColorWithCompute(
    const GfxCmdBuffer* pCmdBuffer,
    const Image&        gfxImage,
    const SubresRange&  range
    ) const
{
    const bool supportsComputePath = gfxImage.SupportsComputeDecompress(range);

    return ((pCmdBuffer->GetEngineType() == EngineTypeCompute)           ||
            (gfxImage.Parent()->IsRenderTarget() == false)               ||
            (supportsComputePath && TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways)));
}

// =====================================================================================================================
bool RsrcProcMgr::WillDecompressDepthStencilWithCompute(
    const GfxCmdBuffer* pCmdBuffer,
    const Image&        gfxImage,
    const SubresRange&  range
    ) const
{
    const bool supportsComputePath = gfxImage.SupportsComputeDecompress(range);

    // To do a compute expand, we need to either
    //   a) Be on the compute queue.  In this case we can't do a gfx decompress because it'll hang.
    //   b) Have a compute-capable image and- have the "compute" path forced through settings.

    return ((pCmdBuffer->IsGraphicsSupported() == false) ||
            (supportsComputePath && TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways)));
}

// =====================================================================================================================
bool RsrcProcMgr::WillResummarizeWithCompute(
    const GfxCmdBuffer* pCmdBuffer,
    const Pal::Image&   image
    ) const
{
    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    // Use compute if:
    //   - We're on the compute engine
    //   - or we should force ExpandHiZRange for resummarize and we support compute operations
    //   - or we have a workaround which indicates if we need to use the compute path.
    const auto& createInfo = image.GetImageCreateInfo();
    const bool  z16Unorm1xAaDecompressUninitializedActive =
        (m_pDevice->Settings().waZ16Unorm1xAaDecompressUninitialized &&
        (createInfo.samples == 1) &&
        ((createInfo.swizzledFormat.format == ChNumFormat::X16_Unorm) ||
        (createInfo.swizzledFormat.format == ChNumFormat::D16_Unorm_S8_Uint)));

    return ((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
            pPublicSettings->expandHiZRangeForResummarize ||
            z16Unorm1xAaDecompressUninitializedActive);
}

// =====================================================================================================================
// Performs a fast-clear on a color image by updating the image's DCC buffer.
void RsrcProcMgr::HwlFastColorClear(
    GfxCmdBuffer*         pCmdBuffer,
    const GfxImage&       dstImage,
    const uint32*         pConvertedColor,
    const SwizzledFormat& clearFormat,
    const SubresRange&    clearRange,
    bool                  trackBltActiveFlags
    ) const
{
    PAL_ASSERT(clearRange.numPlanes == 1);

    const Image& gfx9Image = static_cast<const Image&>(dstImage);

    PAL_ASSERT(gfx9Image.HasDccData());

    auto*const pCmdStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());

    PAL_ASSERT(pCmdStream != nullptr);

    bool fastClearElimRequired = false;
    const uint8 fastClearCode =
        Gfx9Dcc::GetFastClearCode(gfx9Image, clearRange, pConvertedColor, &fastClearElimRequired);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (gfx9Image.GetFastClearEliminateMetaDataAddr(clearRange.startSubres) != 0)
    {
        const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

        // Update the image's FCE meta-data.
        pCmdSpace = gfx9Image.UpdateFastClearEliminateMetaData(pCmdBuffer,
                                                               clearRange,
                                                               fastClearElimRequired,
                                                               packetPredicate,
                                                               pCmdSpace);
    }

    const SwizzledFormat planeFormat = clearFormat.format == ChNumFormat::Undefined ?
                                       dstImage.Parent()->SubresourceInfo(clearRange.startSubres)->format :
                                       clearFormat;

    uint32 swizzledColor[4] = {};
    Formats::SwizzleColor(planeFormat, pConvertedColor, &swizzledColor[0]);

    uint32 packedColor[4] = {};
    Formats::PackRawClearColor(planeFormat, swizzledColor, &packedColor[0]);

    const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

    // When the fast clear color depends on the clear reg, we must store the color for later FCE and update the
    // current clear color.
    // On GFX10 and later, the CB will get the fast clear value from the location indicated by the clear code.
    // So the clear reg should only be updated when we use ClearColorReg.
    if (fastClearCode == static_cast<uint8>(Gfx9DccClearColor::ClearColorCompToReg))
    {
        // Stash the clear color with the image so that it can be restored later.
        pCmdSpace = gfx9Image.UpdateColorClearMetaData(clearRange,
                                                       packedColor,
                                                       packetPredicate,
                                                       pCmdSpace);

        // In case the cleared image is already bound as a color target, we need to update the color clear value
        // registers to the newly-cleared values.
        if (pCmdBuffer->IsGraphicsSupported())
        {
            pCmdSpace = UpdateBoundFastClearColor(pCmdBuffer,
                                                  dstImage,
                                                  clearRange.startSubres.mipLevel,
                                                  clearRange.numMips,
                                                  packedColor,
                                                  pCmdStream,
                                                  pCmdSpace);
        }
    }

    pCmdStream->CommitCommands(pCmdSpace);

    ClearDcc(pCmdBuffer, pCmdStream, gfx9Image, clearRange, fastClearCode, DccClearPurpose::FastClear,
             trackBltActiveFlags, packedColor);

    if (gfx9Image.HasFmaskData())
    {
        // If DCC is enabled on an MSAA surface, CMask fast clears should not be used
        // instead fast clearing CMask to "0xCC" which is 1 fragment
        //
        // NOTE:  On Gfx9, if an image has fMask it will also have cMask.
        InitCmask(pCmdBuffer, pCmdStream, gfx9Image, clearRange, Gfx9Cmask::FastClearValueDcc, trackBltActiveFlags);
    }
}

// =====================================================================================================================
bool RsrcProcMgr::IsAc01ColorClearCode(
    const GfxImage&       dstImage,
    const uint32*         pConvertedColor,
    const SwizzledFormat& clearFormat,
    const SubresRange&    clearRange
    ) const
{
    const Image& gfx9Image = static_cast<const Image&>(dstImage);
    PAL_ASSERT(gfx9Image.HasDccData());

    bool fastClearElimRequired = false;
    bool isClearColorSupported = false;

    Gfx9Dcc::GetFastClearCode(gfx9Image,
                              clearRange,
                              pConvertedColor,
                              &fastClearElimRequired,
                              &isClearColorSupported);

    return isClearColorSupported;
}

// =====================================================================================================================
// An optimized copy does a memcpy of the source fmask and cmask data to the destination image after it is finished.
// See the HwlFixupCopyDstImageMetadata function.  For this to work, the layout needs to be exactly the same between
// the two -- including the swizzle modes and pipe-bank XOR values associated with the fmask data.
bool RsrcProcMgr::HwlUseFMaskOptimizedImageCopy(
    const Pal::Image&      srcImage,
    ImageLayout            srcImageLayout,
    const Pal::Image&      dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions
    ) const
{
    const auto&              srcCreateInfo   = srcImage.GetImageCreateInfo();
    const auto&              dstCreateInfo   = dstImage.GetImageCreateInfo();
    const ImageMemoryLayout& srcImgMemLayout = srcImage.GetMemoryLayout();
    const ImageMemoryLayout& dstImgMemLayout = dstImage.GetMemoryLayout();
    const Image*             pGfxSrcImage    = static_cast<const Image*>(srcImage.GetGfxImage());
    const Image*             pGfxDstImage    = static_cast<const Image*>(dstImage.GetGfxImage());

    // Src image and dst image should be fully identical size.
    bool useFmaskOptimizedCopy = (srcCreateInfo.extent.width  == dstCreateInfo.extent.width) &&
                                 (srcCreateInfo.extent.height == dstCreateInfo.extent.height) &&
                                 (srcCreateInfo.extent.depth  == dstCreateInfo.extent.depth) &&
                                 (srcCreateInfo.mipLevels     == dstCreateInfo.mipLevels) &&
                                 (srcCreateInfo.arraySize     == dstCreateInfo.arraySize);

    // FmaskOptimizedImageCopy must be a whole image copy.
    if (useFmaskOptimizedCopy)
    {
        if ((regionCount != 1) ||
            (memcmp(&pRegions->srcSubres, &pRegions->dstSubres, sizeof(pRegions->srcSubres)) != 0) ||
            (pRegions->srcSubres.mipLevel   != 0) ||
            (pRegions->srcSubres.arraySlice != 0) ||
            (memcmp(&pRegions->srcOffset, &pRegions->dstOffset, sizeof(pRegions->srcOffset)) != 0) ||
            (pRegions->srcOffset.x != 0) ||
            (pRegions->srcOffset.y != 0) ||
            (pRegions->srcOffset.z != 0) ||
            (memcmp(&pRegions->extent, &srcCreateInfo.extent, sizeof(pRegions->extent)) != 0) ||
            (pRegions->numSlices != srcCreateInfo.arraySize))
        {
            useFmaskOptimizedCopy = false;
        }
    }

    if (useFmaskOptimizedCopy)
    {
        // If memory sizes differ it could be due to copying between resources with different shader compat
        // compression modes (1 TC compat, other not).  For RT Src will need to be decompressed which means
        // we can't take advanatge of optimized copy since we keep fmask compressed. Moreover, there are
        // metadata layout differences between gfxip8 and below and gfxip9.
        if ((dstImgMemLayout.metadataSize       != srcImgMemLayout.metadataSize) ||
            (dstImgMemLayout.metadataHeaderSize != srcImgMemLayout.metadataHeaderSize))
        {
            useFmaskOptimizedCopy = false;
        }
    }

    if (useFmaskOptimizedCopy)
    {
        const auto* pSrcFmask = pGfxSrcImage->GetFmask();
        const auto* pDstFmask = pGfxDstImage->GetFmask();

        if ((pSrcFmask != nullptr) && (pDstFmask != nullptr))
        {
            if ((pSrcFmask->GetSwizzleMode() != pDstFmask->GetSwizzleMode()) ||
                (pSrcFmask->GetPipeBankXor() != pDstFmask->GetPipeBankXor()))
            {
                useFmaskOptimizedCopy = false;
            }
        }
    }

    if (useFmaskOptimizedCopy)
    {
        const auto& srcImgLayoutToState = pGfxSrcImage->LayoutToColorCompressionState();
        const auto& dstImgLayoutToState = pGfxDstImage->LayoutToColorCompressionState();

        // Src and dst's layout compression states should be compatible, dst image must not be
        // less compressed than src image.
        if (ImageLayoutToColorCompressionState(srcImgLayoutToState, srcImageLayout) >
            ImageLayoutToColorCompressionState(dstImgLayoutToState, dstImageLayout))
        {
            useFmaskOptimizedCopy = false;
        }
    }

    return useFmaskOptimizedCopy;
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

    const UniversalCmdBuffer* pUnivCmdBuf = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

    // We should be inspecting the main graphics state and not a pushed copy
    PAL_ASSERT(pUnivCmdBuf->GetCmdBufState().flags.isGfxStatePushed == 0);

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
    PAL_ASSERT(range.numPlanes == 1);
    // Only gfx command buffers can have bound render targets / DS attachments.  Fast clears through compute command
    // buffers do not have to worry about updating fast clear value register state.
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

    const UniversalCmdBuffer* pUnivCmdBuf = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

    // We should be inspecting the main graphics state and not a pushed copy
    PAL_ASSERT(pUnivCmdBuf->GetCmdBufState().flags.isGfxStatePushed == 0);

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
            CmdStream* pStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());

            uint32* pCmdSpace = pStream->ReserveCommands();
            pCmdSpace = pView->WriteUpdateFastClearDepthStencilValue(metaDataClearFlags, depth, stencil,
                                                                     pStream, pCmdSpace);
            pStream->CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image by updating the Image's HTile buffer.
void RsrcProcMgr::HwlDepthStencilClear(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    dstImage,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    uint32             rangeCount,
    const SubresRange* pRanges,
    bool               fastClear,
    bool               clearAutoSync,
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
    const Image& gfx9Image = static_cast<const Image&>(dstImage);

    bool needPreComputeSync  = clearAutoSync;
    bool needPostComputeSync = false;

    if (gfx9Image.Parent()->IsDepthStencilTarget() &&
       (fastClear || pCmdBuffer->IsGraphicsSupported()))
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
        // for both planes.
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
            // the cleared image, the bind operation will load the new clear value from image meta-data memory
            // (although this is not as efficient as just directly writing the register).
            bool clearedViaGfx = false;

            // Before we start issuing fast clears, tell the Image to update its fast-clear meta-data.
            uint32 metaDataClearFlags = 0;

            // Fast clear only, prepare fastClearMethod, ClearFlags and update metaData.
            if (fastClear)
            {
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
                    // Fast depth clear method is the same for all subresources, so we can just check the first.
                    const SubResourceInfo& subResInfo = *gfx9Image.Parent()->SubresourceInfo(pRanges[idx].startSubres);
                    fastClearMethod[idx] = subResInfo.clearMethod;
                }

                Pal::CmdStream* const pCmdStream = pCmdBuffer->GetMainCmdStream();
                PAL_ASSERT(pCmdStream != nullptr);

                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
                    const uint32 currentClearFlag = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane) ?
                                                    HtilePlaneDepth : HtilePlaneStencil;

                    metaDataClearFlags |= currentClearFlag;

                    const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

                    uint32* pCmdSpace = pCmdStream->ReserveCommands();
                    pCmdSpace = gfx9Image.UpdateDepthClearMetaData(pRanges[idx],
                                                                   currentClearFlag,
                                                                   depth,
                                                                   stencil,
                                                                   packetPredicate,
                                                                   pCmdSpace);
                    pCmdStream->CommitCommands(pCmdSpace);
                }

            }

            // We can optimize this process by clearing matching depth and stencil ranges at once. To do this, we need
            // another autobuffer to track which ranges have already been cleared.
            AutoBuffer<bool, 2 * MaxImageMipLevels, Platform> isRangeProcessed(rangeCount, m_pDevice->GetPlatform());

            // Notify the command buffer that the AutoBuffer allocation has failed.
            if (isRangeProcessed.Capacity() < rangeCount)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
                    isRangeProcessed[idx] = false;
                }

                // Now issue fast or slow clears to all ranges, grouping identical depth/stencil pairs if possible.
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
                    // No need to clear a range twice.
                    if (isRangeProcessed[idx])
                    {
                        continue;
                    }

                    uint32 clearFlags = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane) ?
                                        HtilePlaneDepth : HtilePlaneStencil;

                    // Search the range list to see if there is a matching range which span the other plane.
                    for (uint32 forwardIdx = idx + 1; forwardIdx < rangeCount; ++forwardIdx)
                    {
                        PAL_ASSERT(pRanges[forwardIdx].numPlanes == 1);
                        if ((pRanges[forwardIdx].startSubres.plane != pRanges[idx].startSubres.plane) &&
                            (pRanges[forwardIdx].startSubres.mipLevel == pRanges[idx].startSubres.mipLevel) &&
                            (pRanges[forwardIdx].numMips == pRanges[idx].numMips) &&
                            (pRanges[forwardIdx].startSubres.arraySlice == pRanges[idx].startSubres.arraySlice) &&
                            (pRanges[forwardIdx].numSlices == pRanges[idx].numSlices) &&
                            ((fastClear == false) || (fastClearMethod[forwardIdx] == fastClearMethod[idx])))
                        {
                            // We found a matching range that for the other plane, clear them both at once.
                            clearFlags = HtilePlaneDepth | HtilePlaneStencil;
                            isRangeProcessed[forwardIdx] = true;
                            break;
                        }
                    }

                    FastDepthStencilClearMode fastDepthStencilClearMode =
                        gfx9Image.Parent()->GetDevice()->GetPublicSettings()->fastDepthStencilClearMode;

                    if (fastDepthStencilClearMode == FastDepthStencilClearMode::Default)
                    {
                        // DepthStencilClearGraphics() implements both fast and slow clears. For fast clears,
                        // if the image layout supports depth/stencil target usage and the image size is too small,
                        // the synchronization overhead of switching to compute and back is a performance bottleneck,
                        // prefer the graphics path for this case. While the image size is over this critical value,
                        // compute path has a good performance advantage, prefer the compute path for this.
                        fastDepthStencilClearMode =
                            ((fastClearMethod[idx] == ClearMethod::DepthFastGraphics) ||
                             (fastClear == false)                                     ||
                             PreferFastDepthStencilClearGraphics(dstImage, depthLayout, stencilLayout))
                            ? FastDepthStencilClearMode::Graphics
                            : FastDepthStencilClearMode::Compute;
                    }

                    if (fastDepthStencilClearMode == FastDepthStencilClearMode::Graphics)
                    {
                        DepthStencilClearGraphics(pCmdBuffer,
                                                  gfx9Image,
                                                  pRanges[idx],
                                                  depth,
                                                  stencil,
                                                  stencilWriteMask,
                                                  clearFlags,
                                                  fastClear,
                                                  depthLayout,
                                                  stencilLayout,
                                                  (needPreComputeSync == false),
                                                  boxCnt,
                                                  pBox);
                        clearedViaGfx = true;
                    }
                    else
                    {
                        // Compute fast clear
                        PAL_ASSERT(fastClear);

                        if (needPreComputeSync)
                        {
                            const bool isDepth = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane);
                            PreComputeDepthStencilClearSync(pCmdBuffer,
                                                            gfx9Image,
                                                            pRanges[idx],
                                                            isDepth ? depthLayout : stencilLayout);

                            needPostComputeSync = true;
                        }

                        // Evaluate the mask and value for updating the HTile buffer.
                        const Gfx9Htile*const pHtile = gfx9Image.GetHtile();
                        PAL_ASSERT(pHtile != nullptr);

                        FastDepthStencilClearCompute(pCmdBuffer,
                                                     gfx9Image,
                                                     pRanges[idx],
                                                     pHtile->GetClearValue(depth),
                                                     clearFlags,
                                                     stencil,
                                                     (needPreComputeSync == false));
                    }

                    isRangeProcessed[idx] = true;

                    // In case the cleared image is possibly already bound as a depth target, we need to update the
                    // depth/stencil clear value registers do the new cleared values.  We can skip this if any of
                    // the clears used a gfx blt (see description above), for fast clear only.
                    if (fastClear && pCmdBuffer->IsGraphicsSupported() && (clearedViaGfx == false))
                    {
                        UpdateBoundFastClearDepthStencil(pCmdBuffer,
                                                         dstImage,
                                                         pRanges[idx],
                                                         metaDataClearFlags,
                                                         depth,
                                                         stencil);
                    }

                    if (needPostComputeSync)
                    {
                        const bool isDepth = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane);
                        PostComputeDepthStencilClearSync(pCmdBuffer,
                                                         gfx9Image,
                                                         pRanges[idx],
                                                         isDepth ? depthLayout : stencilLayout,
                                                         true);
                        needPostComputeSync = false;
                    }
                }
            } // Range Processed AutoBuffer alloc succeeded.
        } // Fast method AutoBuffer alloc succeeded.
    } // Fast clear OR Universal.
    else
    {
        // This code path is only compute-based slow clear

        const Pal::Image* pParent = gfx9Image.Parent();

        for (uint32 idx = 0; idx < rangeCount; ++idx)
        {
            PAL_ASSERT(pRanges[idx].numPlanes == 1);

            const auto& createInfo = pParent->GetImageCreateInfo();
            const bool  isDepth    = m_pDevice->Parent()->SupportsDepth(createInfo.swizzledFormat.format,
                                                                        ImageTiling::Optimal);
            const SubResourceInfo*const  pSubResInfo = pParent->SubresourceInfo(pRanges[idx].startSubres);
            const SwizzledFormat         format      = pSubResInfo->format;

            // If it's PRT tiled mode, tile info for depth and stencil end up being different,
            // compute slow clear uses stencil tile info for stencil clear but later when bound
            // as target, depth tile info will be used, which leads to problem. The similar
            // assert need to be added in elsewhere as needed.
            const ADDR2_GET_PREFERRED_SURF_SETTING_OUTPUT& surfSetting = gfx9Image.GetAddrSettings(pSubResInfo);
            PAL_ASSERT(isDepth || (AddrMgr2::IsPrtSwizzle(surfSetting.swizzleMode) == false));

            ClearColor clearColor = {};

            DepthStencilLayoutToState layoutToState = gfx9Image.LayoutToDepthCompressionState(pRanges[idx].startSubres);

            if (isDepth && (pRanges[idx].startSubres.plane == 0))
            {
                // Expand first if depth plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, depthLayout) != DepthStencilDecomprNoHiZ)
                {
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, pRanges[idx]);
                }

                // For Depth slow clears, we use a float clear color.
                clearColor.type = ClearColorType::Float;
                clearColor.f32Color[0] = depth;
            }
            else
            {
                PAL_ASSERT(m_pDevice->Parent()->SupportsStencil(createInfo.swizzledFormat.format,
                                                                ImageTiling::Optimal));
                // Expand first if stencil plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, stencilLayout) != DepthStencilDecomprNoHiZ)
                {
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, pRanges[idx]);
                }

                // For Stencil plane we use the stencil value directly.
                clearColor.type                = ClearColorType::Uint;
                clearColor.u32Color[0]         = stencil;
                clearColor.disabledChannelMask = ~stencilWriteMask;
            }

            if (needPreComputeSync)
            {
                PreComputeDepthStencilClearSync(pCmdBuffer,
                                                gfx9Image,
                                                pRanges[idx],
                                                isDepth ? depthLayout : stencilLayout);

                needPostComputeSync = true;
            }

            SlowClearCompute(pCmdBuffer,
                             *pParent,
                             isDepth ? depthLayout : stencilLayout,
                             clearColor,
                             format,
                             pRanges[idx],
                             (needPreComputeSync == false),
                             boxCnt,
                             pBox);

            if (needPostComputeSync)
            {
                PostComputeDepthStencilClearSync(pCmdBuffer,
                                                 gfx9Image,
                                                 pRanges[idx],
                                                 isDepth ? depthLayout : stencilLayout,
                                                 false);
                needPostComputeSync = false;
            }
        }
    }
}

// =====================================================================================================================
// Executes a image resolve by performing fixed-func depth copy or stencil copy
void RsrcProcMgr::ResolveImageDepthStencilCopy(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    ImageLayout               srcImageLayout,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags) const
{
    PAL_ASSERT(srcImage.IsDepthStencilTarget() && dstImage.IsDepthStencilTarget());
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pal::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    const auto& srcCreateInfo   = srcImage.GetImageCreateInfo();
    const auto& dstCreateInfo   = dstImage.GetImageCreateInfo();

    ViewportParams viewportInfo = {};
    viewportInfo.count = 1;

    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;

    viewportInfo.horzClipRatio    = FLT_MAX;
    viewportInfo.horzDiscardRatio = 1.0f;
    viewportInfo.vertClipRatio    = FLT_MAX;
    viewportInfo.vertDiscardRatio = 1.0f;
    viewportInfo.depthRange       = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = {};
    scissorInfo.count = 1;

    DepthStencilViewCreateInfo srcDepthViewInfo = {};
    srcDepthViewInfo.pImage                = &srcImage;
    srcDepthViewInfo.arraySize             = 1;
    srcDepthViewInfo.flags.readOnlyDepth   = 1;
    srcDepthViewInfo.flags.readOnlyStencil = 1;
    srcDepthViewInfo.flags.imageVaLocked   = 1;
    srcDepthViewInfo.flags.bypassMall      = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                            RpmViewsBypassMallOnCbDbWrite);

    ColorTargetViewCreateInfo dstColorViewInfo = {};
    dstColorViewInfo.imageInfo.pImage    = &dstImage;
    dstColorViewInfo.imageInfo.arraySize = 1;
    dstColorViewInfo.flags.imageVaLocked = 1;
    dstColorViewInfo.flags.bypassMall    = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                          RpmViewsBypassMallOnCbDbWrite);

    BindTargetParams bindTargetsInfo = {};
    bindTargetsInfo.colorTargetCount = 1;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;
    bindTargetsInfo.colorTargets[0].imageLayout.usages = LayoutColorTarget;
    bindTargetsInfo.colorTargets[0].imageLayout.engines = LayoutUniversalEngine;

    bindTargetsInfo.depthTarget.depthLayout.usages = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.depthLayout.engines = LayoutUniversalEngine;
    bindTargetsInfo.depthTarget.stencilLayout.usages = LayoutDepthStencilTarget;
    bindTargetsInfo.depthTarget.stencilLayout.engines = LayoutUniversalEngine;

    // Save current command buffer state and bind graphics state which is common for all regions.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(1u, 1u));
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);

    // Put ImageResolveInvertY value in user data 0 used by VS.
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 0, 1, &flags);

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> regionAlloc(pCmdBuffer->Allocator(), false);

        dstColorViewInfo.imageInfo.baseSubRes.mipLevel = pRegions[idx].dstMipLevel;

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        // srcOffset and dstOffset have to be exactly same
        PAL_ASSERT((pRegions[idx].srcOffset.x == pRegions[idx].dstOffset.x) &&
                   (pRegions[idx].srcOffset.y == pRegions[idx].dstOffset.y));
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].srcOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].srcOffset.y);
        viewportInfo.viewports[0].width = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x = pRegions[idx].srcOffset.x;
        scissorInfo.scissors[0].offset.y = pRegions[idx].srcOffset.y;
        scissorInfo.scissors[0].extent.width = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        if (srcCreateInfo.flags.sampleLocsAlwaysKnown != 0)
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern != nullptr);
            pCmdBuffer->CmdSetMsaaQuadSamplePattern(srcCreateInfo.samples, *pRegions[idx].pQuadSamplePattern);
        }
        else
        {
            PAL_ASSERT(pRegions[idx].pQuadSamplePattern == nullptr);
        }

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            DepthStencilViewInternalCreateInfo depthViewInfoInternal = {};
            ColorTargetViewInternalCreateInfo  colorViewInfoInternal = {};
            colorViewInfoInternal.flags.depthStencilCopy = 1;

            srcDepthViewInfo.baseArraySlice = (pRegions[idx].srcSlice + slice);
            dstColorViewInfo.imageInfo.baseSubRes.arraySlice = (pRegions[idx].dstSlice + slice);

            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);

            IDepthStencilView* pSrcDepthView = nullptr;
            IColorTargetView* pDstColorView = nullptr;

            void* pSrcDepthViewMem =
                PAL_MALLOC(m_pDevice->GetDepthStencilViewSize(nullptr), &sliceAlloc, AllocInternalTemp);
            void* pDstColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if ((pDstColorViewMem == nullptr) || (pSrcDepthViewMem == nullptr))
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                dstColorViewInfo.imageInfo.baseSubRes.plane = pRegions[idx].dstPlane;

                SubresId dstSubresId   = {};
                dstSubresId.mipLevel   = pRegions[idx].dstMipLevel;
                dstSubresId.arraySlice = (pRegions[idx].dstSlice + slice);
                dstSubresId.plane      = pRegions[idx].dstPlane;

                dstColorViewInfo.swizzledFormat.format = dstImage.SubresourceInfo(dstSubresId)->format.format;

                if (dstImage.IsDepthPlane(pRegions[idx].dstPlane))
                {
                    depthViewInfoInternal.flags.isDepthCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        {ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One};
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(ResolveDepthCopy),
                                                  InternalApiPsoHash, });
                }
                else if (dstImage.IsStencilPlane(pRegions[idx].dstPlane))
                {
                    // Fixed-func stencil copies stencil value from db to g chanenl of cb.
                    // Swizzle the stencil plance to 0X00.
                    depthViewInfoInternal.flags.isStencilCopy = 1;

                    dstColorViewInfo.swizzledFormat.swizzle =
                        { ChannelSwizzle::Zero, ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::One };
                    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics,
                                                  GetGfxPipeline(ResolveStencilCopy),
                                                  InternalApiPsoHash, });
                }
                else
                {
                    PAL_ASSERT_ALWAYS();
                }

                Result result = m_pDevice->CreateDepthStencilView(srcDepthViewInfo,
                                                                  depthViewInfoInternal,
                                                                  pSrcDepthViewMem,
                                                                  &pSrcDepthView);
                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    result = m_pDevice->CreateColorTargetView(dstColorViewInfo,
                                                              colorViewInfoInternal,
                                                              pDstColorViewMem,
                                                              &pDstColorView);
                    PAL_ASSERT(result == Result::Success);
                }

                if (result == Result::Success)
                {
                    bindTargetsInfo.colorTargetCount = 1;
                    bindTargetsInfo.colorTargets[0].pColorTargetView = pDstColorView;
                    bindTargetsInfo.depthTarget.pDepthStencilView = pSrcDepthView;

                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                    // Draw a fullscreen quad.
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                    // Unbind the color-target and depth-stencil target view and destroy them.
                    bindTargetsInfo.colorTargetCount = 0;
                    bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                    pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                }
            }

            PAL_SAFE_FREE(pSrcDepthViewMem, &sliceAlloc);
            PAL_SAFE_FREE(pDstColorViewMem, &sliceAlloc);
        } // End for each slice.
    } // End for each region.

      // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsStateInternal();
    pCmdBuffer->SetGfxBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
}

// =====================================================================================================================
// Sets up an optimized shader for GFX11 that uses a pixel shader to do the resolve
void RsrcProcMgr::HwlResolveImageGraphics(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    ImageLayout               srcImageLayout,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    // This path only supports gfx11.
    PAL_ASSERT(IsGfx11(*m_pDevice->Parent()));
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    // Don't expect GFX Blts on Nested unless targets not inherited.
    PAL_ASSERT((pCmdBuffer->IsNested() == false) || (static_cast<Pal::UniversalCmdBuffer*>(
        pCmdBuffer)->GetGraphicsState().inheritedState.stateFlags.targetViewState == 0));

    const GfxImage* pSrcGfxImage = srcImage.GetGfxImage();
    const Image* pGfx9Image      = static_cast<const Pal::Gfx9::Image*>(pSrcGfxImage);
    const auto& device           = *m_pDevice->Parent();
    const auto& settings         = device.Settings();
    const auto& dstCreateInfo    = dstImage.GetImageCreateInfo();
    const auto& srcCreateInfo    = srcImage.GetImageCreateInfo();
    const auto& srcImageInfo     = srcImage.GetImageInfo();
    const auto* pPublicSettings  = device.GetPublicSettings();

    LateExpandShaderResolveSrc(pCmdBuffer,
                               srcImage,
                               srcImageLayout,
                               pRegions,
                               regionCount,
                               srcImageInfo.resolveMethod,
                               false);

    const StencilRefMaskParams stencilRefMasks = { 0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0x01, 0xFF, };

    // Initialize some structures we will need later on.
    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo = { };
    scissorInfo.count = 1;

    const ColorTargetViewInternalCreateInfo colorViewInfoInternal = { };

    ColorTargetViewCreateInfo colorViewInfo = { };
    colorViewInfo.imageInfo.pImage    = &dstImage;
    colorViewInfo.imageInfo.arraySize = 1;
    colorViewInfo.flags.imageVaLocked = 1;
    colorViewInfo.flags.bypassMall    = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    if (dstCreateInfo.imageType == ImageType::Tex3d)
    {
        colorViewInfo.zRange.extent = 1;
        colorViewInfo.flags.zRangeValid = true;
    }

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.colorTargets[0].imageLayout = dstImageLayout;
    bindTargetsInfo.colorTargets[0].pColorTargetView = nullptr;

    // Save current command buffer state.
    pCmdBuffer->CmdSaveGraphicsState();
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdBindColorBlendState(m_pBlendDisableState);
    pCmdBuffer->CmdBindDepthStencilState(m_pDepthDisableState);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstCreateInfo.samples, dstCreateInfo.fragments));
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    // Keep track of the previous graphics pipeline to reduce the pipeline switching overhead.
    const Pal::GraphicsPipeline* pPreviousPipeline = nullptr;

    // Each region needs to be resolved individually.
    for (uint32 idx = 0; idx < regionCount; ++idx)
    {
        const SubresId dstSubres = Subres(pRegions[idx].dstPlane, pRegions[idx].dstMipLevel, pRegions[idx].dstSlice);
        const SubresId srcSubres = Subres(pRegions[idx].srcPlane, 0, pRegions[idx].srcSlice);

        SwizzledFormat dstFormat = dstImage.SubresourceInfo(dstSubres)->format;
        SwizzledFormat srcFormat = srcImage.SubresourceInfo(srcSubres)->format;

        // Override the formats with the caller's "reinterpret" format.
        if (Formats::IsUndefined(pRegions[idx].swizzledFormat.format) == false)
        {
            // We require that the channel formats match.
            PAL_ASSERT(Formats::ShareChFmt(srcFormat.format, pRegions[idx].swizzledFormat.format));
            PAL_ASSERT(Formats::ShareChFmt(dstFormat.format, pRegions[idx].swizzledFormat.format));

            // If the specified format exactly matches the image formats the resolve will always work. Otherwise, the
            // images must support format replacement.
            PAL_ASSERT(Formats::HaveSameNumFmt(srcFormat.format, pRegions[idx].swizzledFormat.format) ||
                srcImage.GetGfxImage()->IsFormatReplaceable(srcSubres, srcImageLayout, false));

            PAL_ASSERT(Formats::HaveSameNumFmt(dstFormat.format, pRegions[idx].swizzledFormat.format) ||
                dstImage.GetGfxImage()->IsFormatReplaceable(dstSubres, dstImageLayout, true));

            srcFormat = pRegions[idx].swizzledFormat;
            dstFormat = pRegions[idx].swizzledFormat;
        }

        // Non-SRGB can be treated as SRGB when copying to non-srgb image
        if (TestAnyFlagSet(flags, ImageResolveDstAsSrgb))
        {
            dstFormat.format = Formats::ConvertToSrgb(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }
        // SRGB can be treated as Non-SRGB when copying to srgb image
        else if (TestAnyFlagSet(flags, ImageResolveDstAsNorm))
        {
            dstFormat.format = Formats::ConvertToUnorm(dstFormat.format);
            PAL_ASSERT(Formats::IsUndefined(dstFormat.format) == false);
        }

        // SRGB can be treated as Non-SRGB when copying from srgb image
        if (TestAnyFlagSet(flags, ImageResolveSrcAsNorm))
        {
            srcFormat.format = Formats::ConvertToUnorm(srcFormat.format);
            PAL_ASSERT(Formats::IsUndefined(srcFormat.format) == false);
        }

        colorViewInfo.swizzledFormat = dstFormat;

        // Only switch to the appropriate graphics pipeline if it differs from the previous region's pipeline.
        const Pal::GraphicsPipeline* const pPipeline = GetGfxPipelineByFormat(
                                                            RpmGfxPipeline::Gfx11ResolveGraphics_32ABGR, dstFormat);
        if (pPreviousPipeline != pPipeline)
        {
            pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, pPipeline, InternalApiPsoHash, });
            pCmdBuffer->CmdOverwriteColorExportInfoForBlits(dstFormat, 0);
            pPreviousPipeline = pPipeline;
        }

        // Setup the viewport and scissor to restrict rendering to the destination region being copied.
        viewportInfo.viewports[0].originX = static_cast<float>(pRegions[idx].dstOffset.x);
        viewportInfo.viewports[0].originY = static_cast<float>(pRegions[idx].dstOffset.y);
        viewportInfo.viewports[0].width   = static_cast<float>(pRegions[idx].extent.width);
        viewportInfo.viewports[0].height  = static_cast<float>(pRegions[idx].extent.height);

        scissorInfo.scissors[0].offset.x      = pRegions[idx].dstOffset.x;
        scissorInfo.scissors[0].offset.y      = pRegions[idx].dstOffset.y;
        scissorInfo.scissors[0].extent.width  = pRegions[idx].extent.width;
        scissorInfo.scissors[0].extent.height = pRegions[idx].extent.height;

        // Store the necessary region independent user data values in slot 1. Shader expects the following layout:
        // 1 - Num Samples
        uint32 isSingleSample = (Formats::IsSint(srcFormat.format) || Formats::IsUint(srcFormat.format));
        const uint32 numSamples = (isSingleSample) ? 1u : srcImage.GetImageCreateInfo().samples;

        const uint32 psData[4] = { numSamples, 0u, 0u, 0u, };
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 1, 4, &psData[0]);

        // Handle Y inversion in vertex shader.
        const bool invertY = TestAnyFlagSet(flags, ImageResolveInvertY);
        const float bottom = (invertY) ? static_cast<float>(pRegions[idx].extent.height + pRegions[idx].srcOffset.y) :
            static_cast<float>(pRegions[idx].srcOffset.y);
        const float top = (invertY) ? static_cast<float>(pRegions[idx].srcOffset.y) :
            static_cast<float>(pRegions[idx].extent.height + pRegions[idx].srcOffset.y);

        const float vsData[4] =
        {
            // srcTexCoord, [left, bottom, right, top]
            static_cast<float>(pRegions[idx].srcOffset.x),
            bottom,
            static_cast<float>(pRegions[idx].extent.width + pRegions[idx].srcOffset.x),
            top,
        };

        // Can't directly cast from float* to uint32*
        const uint32* vsDataUint = static_cast<const uint32*>(static_cast<const void*>(&vsData[0]));
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Graphics, 5, 4, vsDataUint);

        pCmdBuffer->CmdSetViewports(viewportInfo);
        pCmdBuffer->CmdSetScissorRects(scissorInfo);

        const SubresId dstStartSubres =
        {
            uint8(pRegions[idx].dstPlane),
            uint8(pRegions[idx].dstMipLevel),
            uint16(pRegions[idx].dstSlice)
        };

        for (uint32 slice = 0; slice < pRegions[idx].numSlices; ++slice)
        {
            const SubresId srcSubresSlice = Subres(pRegions[idx].srcPlane, 0, pRegions[idx].srcSlice + slice);

            // Create an embedded user-data table and bind it to user data 1. We only need one image view.
            uint32* pUserData = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment(),
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Graphics,
                                                                       0);

            // Populate the table with an image view of the source image.
            ImageViewInfo     imageView = { };
            const SubresRange viewRange = SingleSubresRange(srcSubresSlice);
            RpmUtil::BuildImageViewInfo(&imageView,
                                        srcImage,
                                        viewRange,
                                        srcFormat,
                                        srcImageLayout,
                                        device.TexOptLevel(),
                                        false);

            device.CreateImageViewSrds(1, &imageView, pUserData);

            colorViewInfo.imageInfo.baseSubRes = dstStartSubres;
            if (dstCreateInfo.imageType == ImageType::Tex3d)
            {
                colorViewInfo.zRange.offset = pRegions[idx].dstOffset.z + slice;
            }
            else
            {
                colorViewInfo.imageInfo.baseSubRes.arraySlice = dstStartSubres.arraySlice + slice;
            }

            // create and bind a color target view of the destination region
            LinearAllocatorAuto<VirtualLinearAllocator> sliceAlloc(pCmdBuffer->Allocator(), false);
            IColorTargetView* pColorView = nullptr;
            void* pColorViewMem =
                PAL_MALLOC(m_pDevice->GetColorTargetViewSize(nullptr), &sliceAlloc, AllocInternalTemp);

            if (pColorViewMem == nullptr)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                // Since our color target view can only bind 1 slice at a time, we have to issue a separate draw for
                // each slice in extent.z. We can keep the same src image view since we pass the explicit slice to
                // read from in user data, but we'll need to create a new color target view each time.
                Result result = m_pDevice->CreateColorTargetView(colorViewInfo,
                                                                 colorViewInfoInternal,
                                                                 pColorViewMem,
                                                                 &pColorView);
                PAL_ASSERT(result == Result::Success);
                bindTargetsInfo.colorTargets[0].pColorTargetView = pColorView;
                bindTargetsInfo.colorTargetCount = 1;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                // Draw a fullscreen quad.
                pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);

                // Unbind the color-target view.
                bindTargetsInfo.colorTargetCount = 0;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);
                PAL_SAFE_FREE(pColorViewMem, &sliceAlloc);
            }
        } // End for each slice.
    } // End for each region.

    // Restore original command buffer state.
    pCmdBuffer->CmdRestoreGraphicsStateInternal();

    pCmdBuffer->SetGfxBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());

    FixupLateExpandShaderResolveSrc(pCmdBuffer,
        srcImage,
        srcImageLayout,
        pRegions,
        regionCount,
        srcImageInfo.resolveMethod,
        false);
}

// =====================================================================================================================
// Check if for all the regions, the format and swizzle mode matches for src and dst image.
// If all regions match, we can do a fixed function resolve. Otherwise return false.
bool RsrcProcMgr::HwlCanDoFixedFuncResolve(
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions) const
{
    const Image*  pGfxSrcImage = reinterpret_cast<const Image*>(srcImage.GetGfxImage());
    const Image*  pGfxDstImage = reinterpret_cast<const Image*>(dstImage.GetGfxImage());

    bool canDoFixedFuncResolve = true;
    for (uint32 region = 0; region < regionCount; ++region)
    {
        const ImageResolveRegion& imageRegion = pRegions[region];
        const SubresId srcSubResId = Subres(imageRegion.srcPlane, imageRegion.dstMipLevel, imageRegion.srcSlice);
        const SubresId dstSubResId = Subres(imageRegion.dstPlane, imageRegion.dstMipLevel, imageRegion.dstSlice);

        const auto* pSrcSubResInfo  = srcImage.SubresourceInfo(srcSubResId);
        const auto* pSrcTileToken   = reinterpret_cast<const AddrMgr2::TileToken*>(&pSrcSubResInfo->tileToken);
        const auto& srcAddrSettings = pGfxSrcImage->GetAddrSettings(pSrcSubResInfo);

        const auto* pDstSubResInfo  = dstImage.SubresourceInfo(dstSubResId);
        const auto* pDstTileToken   = reinterpret_cast<const AddrMgr2::TileToken*>(&pDstSubResInfo->tileToken);
        const auto& dstAddrSettings = pGfxDstImage->GetAddrSettings(pDstSubResInfo);

        canDoFixedFuncResolve =
            ((memcmp(&pSrcSubResInfo->format, &pDstSubResInfo->format, sizeof(SwizzledFormat)) == 0) &&
             (memcmp(&imageRegion.srcOffset, &imageRegion.dstOffset, sizeof(Offset3d)) == 0)         &&
             (pSrcTileToken->bits.swizzleMode == pDstTileToken->bits.swizzleMode)                    &&
             // CB ignores the slice_start field in MRT1, and instead uses the value from MRT0 when writing to MRT1.
             (srcSubResId.arraySlice == dstSubResId.arraySlice));

        if (canDoFixedFuncResolve == false)
        {
            PAL_ALERT_ALWAYS();
            break;
        }
    }

    // Hardware only has support for Average resolves, so we can't perform a fixed function resolve if we're using
    // Minimum or Maximum resolves.
    if (resolveMode != ResolveMode::Average)
    {
        canDoFixedFuncResolve = false;
    }

    return canDoFixedFuncResolve;
}

// =====================================================================================================================
// Before fixfunction or compute shader resolve, we do an optimization that we skip expanding DCC if dst image will be
// fully overwritten in the comming resolve. It means the DCC of dst image needs to be fixed up to expand state after
// the resolve.
void RsrcProcMgr::HwlFixupResolveDstImage(
    GfxCmdBuffer*             pCmdBuffer,
    const GfxImage&           dstImage,
    ImageLayout               dstImageLayout,
    const ImageResolveRegion* pRegions,
    uint32                    regionCount,
    bool                      computeResolve
    ) const
{
    const Image& gfx9Image             = static_cast<const Image&>(dstImage);
    bool         canDoFixupForDstImage = true;

    if (dstImage.Parent()->IsDepthStencilTarget() == true)
    {
        for (uint32 i = 0; i< regionCount; i++)
        {
            // DepthStencilCompressed needs fixup after resolve.
            // DepthStencilDecomprWithHiZ needs fixup the values of HiZ.
            const SubresId subresId = Subres(pRegions->dstPlane, pRegions->dstMipLevel, pRegions->dstSlice);
            const DepthStencilLayoutToState& layoutToState = gfx9Image.LayoutToDepthCompressionState(subresId);

            if (ImageLayoutToDepthCompressionState(layoutToState, dstImageLayout) == DepthStencilDecomprNoHiZ)
            {
                canDoFixupForDstImage = false;
                break;
            }
        }
    }
    else
    {
        canDoFixupForDstImage = (ImageLayoutToColorCompressionState(gfx9Image.LayoutToColorCompressionState(),
                                                                    dstImageLayout) == ColorCompressed);
    }

    // For Gfx10, we only need do fixup after fixed function resolve.
    if (canDoFixupForDstImage && (computeResolve == false))
    {
        AutoBuffer<ImgBarrier, 32, Platform> imgBarriers(regionCount, m_pDevice->GetPlatform());

        if (imgBarriers.Capacity() >= regionCount)
        {
            memset(&imgBarriers[0], 0, sizeof(ImgBarrier) * regionCount);

            for (uint32 i = 0; i < regionCount; i++)
            {
                const SubresId subresId = Subres(pRegions[i].dstPlane, pRegions[i].dstMipLevel, pRegions[i].dstSlice);

                imgBarriers[i].pImage        = dstImage.Parent();
                imgBarriers[i].subresRange   = SubresourceRange(subresId, 1, 1, pRegions[i].numSlices);
                imgBarriers[i].srcStageMask  = PipelineStageTopOfPipe;
                imgBarriers[i].dstStageMask  = PipelineStageBottomOfPipe;
                imgBarriers[i].srcAccessMask = CoherResolveDst;
                imgBarriers[i].dstAccessMask = CoherResolveDst;
                imgBarriers[i].oldLayout     = { .usages  = LayoutUninitializedTarget,
                                                 .engines = dstImageLayout.engines };
                imgBarriers[i].newLayout     = dstImageLayout;

                if (dstImage.Parent()->GetImageCreateInfo().flags.sampleLocsAlwaysKnown != 0)
                {
                    PAL_ASSERT(pRegions[i].pQuadSamplePattern != nullptr);
                }
                else
                {
                    PAL_ASSERT(pRegions[i].pQuadSamplePattern == nullptr);
                }
                imgBarriers[i].pQuadSamplePattern = pRegions[i].pQuadSamplePattern;
            }

            AcquireReleaseInfo acqRelInfo = {};
            acqRelInfo.imageBarrierCount  = regionCount;
            acqRelInfo.pImageBarriers     = &imgBarriers[0];
            acqRelInfo.reason             = Developer::BarrierReasonUnknown;

            pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
        }
        else
        {
            pCmdBuffer->NotifyAllocFailure();
        }
    }
}

// =====================================================================================================================
// Builds PM4 commands into the command buffer which will initialize the value of HiSPretests meta data.
void RsrcProcMgr::ClearHiSPretestsMetaData(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& range
    ) const
{
    PAL_ASSERT(pCmdStream != nullptr);

    const ImageCreateInfo& createInfo = dstImage.Parent()->GetImageCreateInfo();

    // Not sure if the metaDataRange.startSubres.arraySlice has to be zero as it is in depthClearMetadata.
    PAL_ALERT((range.startSubres.arraySlice + range.numSlices) > createInfo.arraySize);

    SubresRange metaDataRange;
    metaDataRange.startSubres.plane      = range.startSubres.plane;
    metaDataRange.startSubres.mipLevel   = range.startSubres.mipLevel;
    metaDataRange.startSubres.arraySlice = 0;
    metaDataRange.numPlanes              = range.numPlanes;
    metaDataRange.numMips                = range.numMips;
    metaDataRange.numSlices              = createInfo.arraySize;

    const Pm4Predicate packetPredicate   = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    HiSPretests defaultHiSPretests = {};
    pCmdSpace = dstImage.UpdateHiSPretestsMetaData(
                         metaDataRange,
                         defaultHiSPretests,
                         packetPredicate,
                         pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
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
    PAL_ALERT((range.startSubres.arraySlice + range.numSlices) > createInfo.arraySize);

    SubresRange metaDataRange;
    metaDataRange.startSubres.plane      = range.startSubres.plane;
    metaDataRange.startSubres.mipLevel   = range.startSubres.mipLevel;
    metaDataRange.startSubres.arraySlice = 0;
    metaDataRange.numPlanes              = range.numPlanes;
    metaDataRange.numMips                = range.numMips;
    metaDataRange.numSlices              = createInfo.arraySize;

    const uint32 metaDataInitFlags = (range.numPlanes == 2)
                                     ? (HtilePlaneDepth | HtilePlaneStencil)
                                     : (dstImage.Parent()->IsDepthPlane(range.startSubres.plane) ?
                                       HtilePlaneDepth : HtilePlaneStencil);

    const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

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
    PAL_ALERT((range.startSubres.arraySlice + range.numSlices) > dstImage.Parent()->GetImageCreateInfo().arraySize);

    constexpr uint32 PackedColor[4] = {0, 0, 0, 0};

    const Pm4Predicate packetPredicate =Pm4Predicate(pCmdBuffer->GetPacketPredicate());

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = dstImage.UpdateColorClearMetaData(range,
                                                  PackedColor,
                                                  packetPredicate,
                                                  pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Performs a fast or clear depth clear using the graphics engine.
void RsrcProcMgr::DepthStencilClearGraphics(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    uint32             clearMask,
    bool               fastClear,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    bool               trackBltActiveFlags,
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    PAL_ASSERT(dstImage.Parent()->IsDepthStencilTarget());
    PAL_ASSERT((fastClear == false) ||  dstImage.IsFastDepthStencilClearSupported(depthLayout,
                                                                                  stencilLayout,
                                                                                  depth,
                                                                                  stencil,
                                                                                  stencilWriteMask,
                                                                                  range));

    const auto* pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    const bool  clearDepth      = TestAnyFlagSet(clearMask, HtilePlaneDepth);
    const bool  clearStencil    = TestAnyFlagSet(clearMask, HtilePlaneStencil);
    PAL_ASSERT(clearDepth || clearStencil); // How did we get here if there's nothing to clear!?

    const StencilRefMaskParams stencilRefMasks =
        { stencil, 0xFF, stencilWriteMask, 0x01, stencil, 0xFF, stencilWriteMask, 0x01, 0xFF };

    ViewportParams viewportInfo = { };
    viewportInfo.count                 = 1;
    viewportInfo.viewports[0].origin   = PointOrigin::UpperLeft;
    viewportInfo.viewports[0].originX  = 0;
    viewportInfo.viewports[0].originY  = 0;
    viewportInfo.viewports[0].minDepth = 0.f;
    viewportInfo.viewports[0].maxDepth = 1.f;
    viewportInfo.horzClipRatio         = FLT_MAX;
    viewportInfo.horzDiscardRatio      = 1.0f;
    viewportInfo.vertClipRatio         = FLT_MAX;
    viewportInfo.vertDiscardRatio      = 1.0f;
    viewportInfo.depthRange            = DepthRange::ZeroToOne;

    ScissorRectParams scissorInfo      = { };
    scissorInfo.count                  = 1;
    scissorInfo.scissors[0].offset.x   = 0;
    scissorInfo.scissors[0].offset.y   = 0;

    // The DB defines some context registers as "surface" registers. If the DB has an active context for a surface or
    // has cache lines associated with a surface then you cannot set up a new context for that surface with different
    // surface register values unless you flush and invalidate the DB caches and wait for those contexts to be idle.
    // There is one exception: it's safe to change surface registers if the following draw will cover all surface
    // planes and the full surface X/Y extent (including internal padding).
    //
    // In PAL, we only change surface state if we switch fast clear values or z range precision values. We can't know
    // the previous surface state values so we must always flush the DB caches when we do a graphics fast clear. Note
    // that PAL currently does not include the padding so we never satisfy the exception.
    if (fastClear)
    {
        auto*const pCmdStream = static_cast<Gfx9::CmdStream*>(pCmdBuffer->GetMainCmdStream());
        PAL_ASSERT(pCmdStream != nullptr);

        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        // We should prefer using a pre_depth PWS wait when it's supported. WriteWaitEop will use PWS by default.
        // Moving the wait down to the pre_depth sync point should make the wait nearly free. Otherwise, the legacy
        // surf-sync support should be faster than a full EOP wait at the CP.
        if (IsGfx11(*m_pDevice->Parent()))
        {
            constexpr WriteWaitEopInfo WaitEopInfo = { .hwRbSync = SyncDbWbInv, .hwAcqPoint = AcquirePointPreDepth };

            pCmdSpace = pCmdBuffer->WriteWaitEop(WaitEopInfo, pCmdSpace);
        }
        else
        {
            AcquireMemGfxSurfSync acquireInfo = {};
            acquireInfo.rangeBase = dstImage.Parent()->GetGpuVirtualAddr();
            acquireInfo.rangeSize = dstImage.GetGpuMemSyncSize();
            acquireInfo.flags.dbTargetStall = 1;
            acquireInfo.flags.gfx10DbWbInv  = 1;

            pCmdSpace += m_cmdUtil.BuildAcquireMemGfxSurfSync(acquireInfo, pCmdSpace);
        }

        pCmdStream->CommitCommands(pCmdSpace);
    }

    DepthStencilViewInternalCreateInfo depthViewInfoInternal = { };
    depthViewInfoInternal.depthClearValue   = depth;
    depthViewInfoInternal.stencilClearValue = stencil;

    DepthStencilViewCreateInfo depthViewInfo = { };
    depthViewInfo.pImage              = dstImage.Parent();
    depthViewInfo.arraySize           = 1;
    depthViewInfo.flags.imageVaLocked = 1;
    depthViewInfo.flags.bypassMall    = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                       RpmViewsBypassMallOnCbDbWrite);

    // Depth-stencil targets must be used on the universal engine.
    PAL_ASSERT((clearDepth   == false) || TestAnyFlagSet(depthLayout.engines,   LayoutUniversalEngine));
    PAL_ASSERT((clearStencil == false) || TestAnyFlagSet(stencilLayout.engines, LayoutUniversalEngine));

    BindTargetParams bindTargetsInfo = { };
    bindTargetsInfo.depthTarget.depthLayout   = depthLayout;
    bindTargetsInfo.depthTarget.stencilLayout = stencilLayout;

    pCmdBuffer->CmdSaveGraphicsState();

    // Bind the depth expand state because it's just a full image quad and a zero PS (with no internal flags) which
    // is also what we need for the clear.
    PipelineBindParams bindParams = { PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, };
    if (clearDepth)
    {
        // Enable viewport clamping if depth values are in the [0, 1] range. This avoids writing expanded depth
        // when using a float depth format. DepthExpand pipeline disables clamping by default.
        const bool disableClamp = ((depth < 0.0f) || (depth > 1.0f));

        bindParams.gfxDynState.enable.depthClampMode = 1;
        bindParams.gfxDynState.depthClampMode        = disableClamp ? DepthClampMode::_None : DepthClampMode::Viewport;
    }
    pCmdBuffer->CmdBindPipeline(bindParams);
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstImage.Parent()->GetImageCreateInfo().samples,
                                              dstImage.Parent()->GetImageCreateInfo().fragments));
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

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

    // Box of partial clear is only valid when number of mip-map is equal to 1.
    PAL_ASSERT((boxCnt == 0) || ((pBox != nullptr) && (range.numMips == 1)));
    uint32 scissorCnt = (boxCnt > 0) ? boxCnt : 1;

    // Each mipmap level has to be fast-cleared individually because a depth target view can only be tied to a
    // single mipmap level of the destination Image.
    const uint32 lastMip = (range.startSubres.mipLevel + range.numMips - 1);
    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
        const SubresId         subres     = Subres(range.startSubres.plane, depthViewInfo.mipLevel, 0);
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
                    pCmdBuffer->CmdDraw(0, 3, 0, 1, 0);
                }

                // Unbind the depth view and destroy it.
                bindTargetsInfo.depthTarget.pDepthStencilView = nullptr;
                pCmdBuffer->CmdBindTargets(bindTargetsInfo);

                PAL_SAFE_FREE(pDepthViewMem, &sliceAllocator);
            }
        } // End for each slice.
    } // End for each mip.

    // Restore original command buffer state and destroy the depth/stencil state.
    pCmdBuffer->CmdRestoreGraphicsStateInternal(trackBltActiveFlags);

    pCmdBuffer->SetGfxBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
}

// =====================================================================================================================
// Returns "true" if the compute engine was used for the clear operation.
bool RsrcProcMgr::ClearDcc(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose,
    bool               trackBltActiveFlags,
    const uint32*      pPackedClearColor
    ) const
{
    const Pal::Image*      pParent  = dstImage.Parent();
    const Pal::Device*     pDevice  = pParent->GetDevice();
    const Gfx9PalSettings& settings = GetGfx9Settings(*pDevice);

    bool  usedCompute = true;

    switch (clearPurpose)
    {
    case DccClearPurpose::Init:
        if (pCmdBuffer->IsGraphicsSupported() &&
            (TestAnyFlagSet(settings.dccOnComputeEnable, Gfx9DccOnComputeInit) == false))
        {
            // Clear color doesn't really matter, we just want the CB to write something it understands into DCC.
            const ClearColor clearColor     = {};
            ImageLayout      dstImageLayout = {};
            dstImageLayout.engines = LayoutUniversalEngine;
            dstImageLayout.usages  = LayoutColorTarget;

            SlowClearGraphics(pCmdBuffer,
                              *pParent,
                              dstImageLayout,
                              clearColor,
                              pParent->GetImageCreateInfo().swizzledFormat,
                              clearRange,
                              trackBltActiveFlags,
                              0,
                              nullptr);
            usedCompute = false;
        }
        else
        {
            ClearDccCompute(pCmdBuffer, pCmdStream, dstImage, clearRange, clearCode, clearPurpose, trackBltActiveFlags);
        }
        break;

    case DccClearPurpose::FastClear:
        // Clears of DCC-images on the graphics queue should occur through the graphics engine, unless specifically
        // requeusted to occur on compute.
        PAL_ASSERT((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
                   TestAnyFlagSet(settings.dccOnComputeEnable, Gfx9DccOnComputeFastClear));

        ClearDccCompute(pCmdBuffer, pCmdStream, dstImage, clearRange, clearCode, clearPurpose,
                        trackBltActiveFlags, pPackedClearColor);
        break;

    default:
        // What is this?
        PAL_ASSERT_ALWAYS();
        break;
    }

    return usedCompute;
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
    PAL_ASSERT(range.numPlanes == 1);

    const auto&     device            = *m_pDevice->Parent();
    const auto&     parentImg         = *image.Parent();
    const auto*     pPipeline         = GetComputeMaskRamExpandPipeline(parentImg);
    Pal::CmdStream* pComputeCmdStream = pCmdBuffer->GetMainCmdStream();
    uint32*         pComputeCmdSpace  = nullptr;
    const auto&     createInfo        = parentImg.GetImageCreateInfo();

    // If this trips, we have a big problem...
    PAL_ASSERT(pComputeCmdStream != nullptr);

    // Compute the number of thread groups needed to launch one thread per texel.
    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
    const EngineType engineType = pCmdBuffer->GetEngineType();
    const uint32     lastMip    = range.startSubres.mipLevel + range.numMips - 1;
    bool             earlyExit  = false;

    for (uint32 mipLevel = range.startSubres.mipLevel; ((earlyExit == false) && (mipLevel <= lastMip)); mipLevel++)
    {
        const SubresId              mipBaseSubResId = Subres(range.startSubres.plane, mipLevel, 0);
        const SubResourceInfo*const pBaseSubResInfo = image.Parent()->SubresourceInfo(mipBaseSubResId);

        // After a certain point, mips may not have 'useful' DCC, thus supportMetaDataTexFetch is 0 and expand is not
        // necessary at all
        if (pBaseSubResInfo->flags.supportMetaDataTexFetch == 0)
        {
            break;
        }

        const DispatchDims threadGroups =
        {
            RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.width,  threadsPerGroup.x),
            RpmUtil::MinThreadGroups(pBaseSubResInfo->extentElements.height, threadsPerGroup.y),
            1
        };

        const uint32 constData[] =
        {
            // start cb0[0]
            pBaseSubResInfo->extentElements.width,
            pBaseSubResInfo->extentElements.height,
        };

        // Embed the constant buffer in user-data right after the SRD table.
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, ArrayLen32(constData), constData);

        SubresRange viewRange = SingleSubresRange(mipBaseSubResId);
        for (uint32 sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
        {
            viewRange.startSubres.arraySlice = uint16(range.startSubres.arraySlice + sliceIdx);

            // Create an embedded user-data table and bind it to user data 0. We will need two views.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       SrdDwordAlignment() * 2,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            ImageViewInfo imageView[2] = {};
            RpmUtil::BuildImageViewInfo(&imageView[0],
                                        parentImg,
                                        viewRange,
                                        createInfo.swizzledFormat,
                                        RpmUtil::DefaultRpmLayoutRead,
                                        device.TexOptLevel(),
                                        false); // src

            RpmUtil::BuildImageViewInfo(&imageView[1],
                                        parentImg,
                                        viewRange,
                                        createInfo.swizzledFormat,
                                        RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                        device.TexOptLevel(),
                                        true);  // dst

            device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(threadGroups, {});
        } // end loop through all the slices
    }

    if (image.HasDccStateMetaData(range))
    {
        // We have to mark this mip level as actually being DCC decompressed
        image.UpdateDccStateMetaData(pCmdStream, range, false, engineType, PredDisable);
    }

    // Make sure that the decompressed image data has been written before we start fixing up DCC memory.
    pComputeCmdSpace = pComputeCmdStream->ReserveCommands();
    pComputeCmdSpace = pCmdBuffer->WriteWaitCsIdle(pComputeCmdSpace);
    pComputeCmdStream->CommitCommands(pComputeCmdSpace);

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
    pCmdBuffer->SetCsBltIndirectWriteMisalignedMdState(image.HasMisalignedMetadata());
}

// =====================================================================================================================
// Performs a DCC decompress blt on the provided Image.
void RsrcProcMgr::DccDecompress(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);

    if (range.numMips > 0)
    {
        const bool  supportsComputePath = image.SupportsComputeDecompress(range);
        const auto& settings            = m_pDevice->Settings();
        const auto* pSubResInfo         = image.Parent()->SubresourceInfo(range.startSubres);
        const auto& addrSettings        = image.GetAddrSettings(pSubResInfo);

        if (WillDecompressColorWithCompute(pCmdBuffer, image, range))
        {
            // We should have already done a fast-clear-eliminate on the graphics engine when we transitioned to
            // whatever state we're now transitioning out of, so there's no need to do that again.
            DccDecompressOnCompute(pCmdBuffer, pCmdStream, image, range);
        }
        else
        {
            const bool alwaysDecompress = TestAnyFlagSet(settings.alwaysDecompress, DecompressDcc);
            // Disable metaData state condition for multi range case. Since current GenericColorBlit assume
            // metaDataAddr as mipmap level based but our metaData are contiguous in memory for slices in one mipmap
            // level.
            const bool multiRange = (range.numSlices > 1) || (range.numMips > 1);

            const GpuMemory* pGpuMem        = nullptr;
            gpusize          metaDataOffset = (alwaysDecompress || multiRange)
                                               ? 0
                                               : image.GetDccStateMetaDataOffset(range.startSubres);

            if (metaDataOffset)
            {
                pGpuMem = image.Parent()->GetBoundGpuMemory().Memory();
                metaDataOffset += image.Parent()->GetBoundGpuMemory().Offset();
            }

            // Execute a generic CB blit using the appropriate DCC decompress pipeline.
            GenericColorBlit(pCmdBuffer,
                             *image.Parent(),
                             range,
                             pQuadSamplePattern,
                             RpmGfxPipeline::DccDecompress,
                             pGpuMem,
                             metaDataOffset,
                             { });
        }

        if (image.HasDccStateMetaData(range))
        {
            // We have to mark this mip level as actually being DCC decompressed
            image.UpdateDccStateMetaData(pCmdStream, range, false, pCmdBuffer->GetEngineType(), PredDisable);
        }

        // Clear the FCE meta data over the given range because a DCC decompress implies a FCE. Note that it doesn't
        // matter that we're using the truncated range here because we mips that don't use DCC shouldn't need a FCE
        // because they must be slow cleared.
        if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
        {
            const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());
            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace = image.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
// Performs a fast color clear eliminate blt on the provided Image. Returns true if work (blt) is submitted to GPU.
void RsrcProcMgr::FastClearEliminate(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);

    const bool alwaysFce = TestAnyFlagSet(m_pDevice->Settings().alwaysDecompress, DecompressFastClear);

    const GpuMemory* pGpuMem = nullptr;
    gpusize metaDataOffset = alwaysFce ? 0 : image.GetFastClearEliminateMetaDataOffset(range.startSubres);
    if (metaDataOffset)
    {
        pGpuMem = image.Parent()->GetBoundGpuMemory().Memory();
        metaDataOffset += image.Parent()->GetBoundGpuMemory().Offset();
    }

    // Execute a generic CB blit using the fast-clear Eliminate pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range,
        pQuadSamplePattern, RpmGfxPipeline::FastClearElim, pGpuMem, metaDataOffset, { });

    // Clear the FCE meta data over the given range because those mips must now be FCEd.
    if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

        pCmdSpace = image.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Gives the hardware layers some influence over GetCopyImageCsInfo.
bool RsrcProcMgr::CopyImageCsUseMsaaMorton(
    const Pal::Image& dstImage
    ) const
{
    // Our HW has stored depth/stencil samples sequentially for many generations and gfx10+ explicitly stores pixels
    // within a micro-tile in Morton/Z order. The Morton shaders were written with gfx10 in mind but performance
    // profiling showed they help on all GPUs. This makes sense as reading and writing samples sequentially is the
    // primary benefit to using the Morton path over the old path (Morton is just a snazzier name than Sequential).
    //
    // In gfx11, all MSAA swizzle modes were made identical to gfx10's "Z" swizzle modes. That means all gfx11
    // MSAA images store their samples sequentially and store pixels in micro-tiles in Morton/Z order.
    return IsGfx11(*m_pDevice->Parent()) || Pal::RsrcProcMgr::CopyImageCsUseMsaaMorton(dstImage);
}

// =====================================================================================================================
// Make a special writeable FMask image SRD which covers the entire clear range.
static void ClearFmaskCreateSrdCallback(
    const GfxDevice&   device,
    const Pal::Image&  image,
    const SubresRange& viewRange,
    const void*        pContext,  // Unused.
    void*              pSrd,      // [out] Place the image SRD here.
    Extent3d*          pExtent)   // [out] Fill this out with the maximum extent of the start subresource.
{
    FmaskViewInfo fmaskBufferView        = {};
    fmaskBufferView.pImage               = &image;
    fmaskBufferView.baseArraySlice       = viewRange.startSubres.arraySlice;
    fmaskBufferView.arraySize            = viewRange.numSlices;
    fmaskBufferView.flags.shaderWritable = 1;

    FmaskViewInternalInfo fmaskViewInternal = {};
    fmaskViewInternal.flags.fmaskAsUav = 1;

    static_cast<const Device&>(device).CreateFmaskViewSrdsInternal(1, &fmaskBufferView, &fmaskViewInternal, pSrd);

    // There's one FMask "texel" per color texel, just use the image's normal extent for our FMask extent.
    *pExtent = image.SubresourceInfo(viewRange.startSubres)->extentTexels;
}

// =====================================================================================================================
// Memsets an Image's FMask sub-allocations with the specified clear value.
// This function does not save or restore the Command Buffer's state, that responsibility lies with the caller!
void RsrcProcMgr::ClearFmask(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint64             clearValue
    ) const
{
    PAL_ASSERT(clearRange.numPlanes == 1);

    const ADDR2_COMPUTE_FMASK_INFO_OUTPUT& fMaskAddrOutput = dstImage.GetFmask()->GetAddrOutput();

    // The shader will saturate the fmask value to the fmask view format's size. so we mask-off clearValue to fit it.
    const uint64 validBitsMask    = fMaskAddrOutput.bpp < 64 ? (1ULL << fMaskAddrOutput.bpp) - 1ULL : UINT64_MAX;
    const uint64 maskedClearValue = clearValue & validBitsMask;

    // Ask for a typical 2D image slow clear with a 8x8 thread pattern. The only odd parts are that it must use FMask
    // views and that FMask is effectively single-sample despite the image being MSAA/EQAA.
    ClearImageCsInfo info = {};
    info.pipelineEnum   = RpmComputePipeline::ClearImage;
    info.groupShape     = {8, 8, 1};
    info.clearFragments = 1;
    info.packedColor[0] = LowPart(maskedClearValue);
    info.packedColor[1] = HighPart(maskedClearValue);
    info.pSrdCallback   = ClearFmaskCreateSrdCallback;

    ClearImageCs(pCmdBuffer, info, *dstImage.Parent(), clearRange, 0, nullptr);

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
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
    PAL_ASSERT(range.numPlanes == 1);
    // MSAA images can only have 1 mip level.
    PAL_ASSERT((range.startSubres.mipLevel == 0) && (range.numMips == 1));
    PAL_ASSERT(image.HasFmaskData());

    const auto& device     = *m_pDevice->Parent();
    const auto& createInfo = image.Parent()->GetImageCreateInfo();

    const uint32 log2Fragments = Log2(createInfo.fragments);
    const uint32 log2Samples   = Log2(createInfo.samples);

    const uint32 numFmaskBits = RpmUtil::CalculatNumFmaskBits(createInfo.fragments, createInfo.samples);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // For single fragment images, we simply need to fixup the FMask.
    if (createInfo.fragments == 1)
    {
        ClearFmask(pCmdBuffer, image, range, Gfx9Fmask::GetPackedExpandedValue(image));
    }
    else
    {
        // Select the correct pipeline for the given number of fragments.
        const Pal::ComputePipeline* pPipeline = nullptr;
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
        const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();
        const DispatchDims threadGroups =
        {
            RpmUtil::MinThreadGroups(createInfo.extent.width,  threadsPerGroup.x),
            RpmUtil::MinThreadGroups(createInfo.extent.height, threadsPerGroup.y),
            1
        };

        // Save current command buffer state and bind the pipeline.

        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
        // Select the appropriate value to indicate that FMask is fully expanded and place it in user data 8-9.
        // Put the low part in user data 8 and the high part in user data 9.
        // The fmask bits is placed in user data 10
        const uint32 expandedValueData[3] =
        {
            LowPart(FmaskExpandedValues[log2Fragments][log2Samples]),
            HighPart(FmaskExpandedValues[log2Fragments][log2Samples]),
            numFmaskBits
        };

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 1, 3, expandedValueData);

        // Because we are setting up the MSAA surface as a 3D UAV, we need to have a separate dispatch for each slice.
        SubresRange  viewRange = { range.startSubres, 1, 1, 1 };
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
            RpmUtil::BuildImageViewInfo(&imageView,
                                        *image.Parent(),
                                        viewRange,
                                        format,
                                        RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                        device.TexOptLevel(),
                                        true);
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

            m_pDevice->CreateFmaskViewSrdsInternal(1, &fmaskView, &fmaskViewInternal, pSrdTable);

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(threadGroups, {});
        }

        pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(image.HasMisalignedMetadata());
        pCmdBuffer->SetCsBltIndirectWriteMisalignedMdState(image.HasMisalignedMetadata());
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Performs an FMask decompress blt on the provided Image.
void RsrcProcMgr::FmaskDecompress(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);
    // Only MSAA Images should ever need an FMask Decompress and they only support a single mipmap level.
    PAL_ASSERT((range.startSubres.mipLevel == 0) && (range.numMips == 1));

    // Execute a generic CB blit using the appropriate FMask Decompress pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range,
        pQuadSamplePattern, RpmGfxPipeline::FmaskDecompress, nullptr, 0, { });

    // Clear the FCE meta data over the given range because an FMask decompress implies a FCE.
    if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
    {
        uint32* pCmdSpace  = pCmdStream->ReserveCommands();

        const Pm4Predicate packetPredicate = Pm4Predicate(pCmdBuffer->GetPacketPredicate());

        pCmdSpace = image.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
// Helper function for HwlBeginGraphicsCopy and HwlEndGraphicsCopy.  Writes the PM4 data that these functions require
// to the specified command stream.
void RsrcProcMgr::CommitBeginEndGfxCopy(
    Pal::CmdStream*  pCmdStream,
    uint32           paScTileSteeringOverride
    ) const
{
    CmdStream*  pGfxCmdStream = reinterpret_cast<CmdStream*>(pCmdStream);
    uint32*     pCmdSpace     = pCmdStream->ReserveCommands();

    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfx9.validPaScTileSteeringOverride);

    pCmdSpace = pGfxCmdStream->WriteSetOneContextReg(mmPA_SC_TILE_STEERING_OVERRIDE,
                                                     paScTileSteeringOverride,
                                                     pCmdSpace);

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Returns a union of the HtilePlaneMask enumerations that indicate which planes need to be cleared.  A return value
// of zero indicates that the initialization of hTile is a NOP for this particular clear range.
uint32 RsrcProcMgr::GetInitHtileClearMask(
    const Image&       dstImage,
    const SubresRange& clearRange
    ) const
{
    const Pal::Image*      pParentImg = dstImage.Parent();
    const ImageCreateInfo& createInfo = pParentImg->GetImageCreateInfo();
    const Gfx9Htile*       pHtile     = dstImage.GetHtile();

    uint32 clearMask = 0;

    // If all these conditions are true:
    //    1) This depth image has both depth and stencil planes
    //    2) The client did not request separate initialization of the depth and stencil planes
    //    3) hTile supports both depth and stencil
    //
    // Then we need to initialize both planes here.
    if ((pParentImg->GetImageInfo().numPlanes == 2) &&
        (createInfo.flags.perSubresInit == 0)       &&
        (pHtile->TileStencilDisabled() == false))
    {
        clearMask = HtilePlaneDepth | HtilePlaneStencil;
    }
    else if (clearRange.numPlanes == 2)
    {
        clearMask = HtilePlaneDepth | HtilePlaneStencil;
    }
    else if (pParentImg->IsDepthPlane(clearRange.startSubres.plane))
    {
        clearMask = HtilePlaneDepth;
    }
    else if (pParentImg->IsStencilPlane(clearRange.startSubres.plane) && (pHtile->TileStencilDisabled() == false))
    {
        clearMask = HtilePlaneStencil;
    }

    return clearMask;
}

// =====================================================================================================================
// Helper function to build a DMA packet to copy metadata header by PFP
void RsrcProcMgr::PfpCopyMetadataHeader(
    GfxCmdBuffer* pCmdBuffer,
    gpusize       dstAddr,
    gpusize       srcAddr,
    uint32        size,
    bool          hasDccLookupTable
    ) const
{
    Pal::CmdStream* pCmdStream = pCmdBuffer->GetMainCmdStream();

    DmaDataInfo dmaDataInfo = {};
    dmaDataInfo.dstSel      = dst_sel__pfp_dma_data__dst_addr_using_l2;
    dmaDataInfo.srcSel      = src_sel__pfp_dma_data__src_addr_using_l2;
    dmaDataInfo.sync        = true;
    dmaDataInfo.usePfp      = true;
    dmaDataInfo.predicate   = Pm4Predicate(pCmdBuffer->GetPacketPredicate());
    dmaDataInfo.dstAddr     = dstAddr;
    dmaDataInfo.srcAddr     = srcAddr;
    dmaDataInfo.numBytes    = size;

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (hasDccLookupTable)
    {
        // The DCC lookup table is accessed by the ME (really, by shaders) so we need to wait for prior ME work.
        pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
    }

    pCmdSpace += CmdUtil::BuildDmaData<false, false>(dmaDataInfo, pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);

    pCmdBuffer->SetCpBltWriteCacheState(true);
}

// ====================================================================================================================
// Returns the maximum size that would be copied for the specified sub-resource-id via the SRD used by the default
// copy image<->memory functions.
Extent3d RsrcProcMgr::GetCopyViaSrdCopyDims(
    const Pal::Image&  image,
    SubresId           subresId,
    bool               includePadding)
{
    const SubresId  baseMipSubResId  = { subresId.plane, 0, subresId.arraySlice };
    const auto*     pBaseSubResInfo  = image.SubresourceInfo(baseMipSubResId);
    Extent3d        programmedExtent = (includePadding
                                        ? pBaseSubResInfo->actualExtentElements
                                        : pBaseSubResInfo->extentElements);

    const SwizzledFormat swizzledFormat = image.GetImageCreateInfo().swizzledFormat;

    // Pal view X8Y8_Z8Y8 as X16 for raw copy, need to use texels extent here to match with Gfx[10|9]CreateImageViewSrds
    if (Formats::IsMacroPixelPackedRgbOnly(swizzledFormat.format))
    {
        programmedExtent = (includePadding
                            ? pBaseSubResInfo->actualExtentTexels
                            : pBaseSubResInfo->extentTexels);
    }

    Extent3d  hwCopyDims = {};

    // Ok, the HW is programmed in terms of the dimensions specified in "actualExtentElements" found in the
    // pBaseSubResInfo structure.  The HW will do a simple ">> 1" for each subsequent mip level.
    hwCopyDims.width  = Max(1u, programmedExtent.width  >> subresId.mipLevel);
    hwCopyDims.height = Max(1u, programmedExtent.height >> subresId.mipLevel);
    hwCopyDims.depth  = Max(1u, programmedExtent.depth  >> subresId.mipLevel);

    return hwCopyDims;
}

// ====================================================================================================================
// Helper function to generate ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT structure
static void FillAddr2ComputeSurfaceAddrFromCoord(
    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT* pInput,
    const Pal::Image&                          image,
    SubresId                                   subresId)
{
    const auto&     createInfo       = image.GetImageCreateInfo();
    const bool      i3dImage         = (createInfo.imageType == ImageType::Tex3d);
    const auto*     pSubResInfo      = image.SubresourceInfo(subresId);
    const auto*     pGfxImage        = static_cast<const Image*>(image.GetGfxImage());
    const auto&     surfSetting      = pGfxImage->GetAddrSettings(pSubResInfo);
    const auto*     pTileInfo        = Pal::AddrMgr2::GetTileInfo(&image, pSubResInfo->subresId);
    const SubresId  baseMipSubResId  = { subresId.plane, 0, subresId.arraySlice };
    const auto*     pBaseSubResInfo  = image.SubresourceInfo(baseMipSubResId);

    pInput->size            = sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT);
    pInput->sample          = 0;
    pInput->mipId           = subresId.mipLevel;
    pInput->unalignedWidth  = pBaseSubResInfo->extentElements.width;
    pInput->unalignedHeight = pBaseSubResInfo->extentElements.height;
    pInput->numSlices       = i3dImage ? createInfo.extent.depth : createInfo.arraySize;
    pInput->numMipLevels    = createInfo.mipLevels;
    pInput->numSamples      = createInfo.samples;
    pInput->numFrags        = createInfo.fragments;
    pInput->swizzleMode     = surfSetting.swizzleMode;
    pInput->resourceType    = surfSetting.resourceType;
    pInput->pipeBankXor     = pTileInfo->pipeBankXor;
    pInput->bpp             = Formats::BitsPerPixel(createInfo.swizzledFormat.format);
}

// =====================================================================================================================
// Check if need copy missing pixels per pixel in CmdCopyImage.
bool RsrcProcMgr::NeedPixelCopyForCmdCopyImage(
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    const ImageCopyRegion* pRegions,
    uint32                 regionCount
    ) const
{
    const ImageCreateInfo& srcInfo = srcImage.GetImageCreateInfo();
    const ImageCreateInfo& dstInfo = dstImage.GetImageCreateInfo();

    bool needCopy = false;

    if (((Formats::IsBlockCompressed(srcInfo.swizzledFormat.format) ||
          Formats::IsMacroPixelPackedRgbOnly(srcInfo.swizzledFormat.format)) && (srcInfo.mipLevels > 1)) ||
        ((Formats::IsBlockCompressed(dstInfo.swizzledFormat.format) ||
          Formats::IsMacroPixelPackedRgbOnly(dstInfo.swizzledFormat.format)) && (dstInfo.mipLevels > 1)))
    {
        for (uint32 i = 0; i < regionCount; i++)
        {
            if (UsePixelCopyForCmdCopyImage(srcImage, dstImage, pRegions[i]))
            {
                needCopy = true;
                break;
            }
        }
    }

    return needCopy;
}

// ====================================================================================================================
// Implement a horribly inefficient copy on a pixel-by-pixel basis of the pixels that were missed by the standard
// copy algorithm.
void RsrcProcMgr::HwlImageToImageMissingPixelCopy(
    GfxCmdBuffer*          pCmdBuffer,
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    const ImageCopyRegion& region) const
{
    if (UsePixelCopyForCmdCopyImage(srcImage, dstImage, region))
    {
        CmdCopyImageToImageViaPixels(pCmdBuffer, srcImage, dstImage, region);
    }
}

// ====================================================================================================================
// The only potential CP DMA copy usage on image is CmdCopyMemory() calls in CmdCopyMemoryFromToImageViaPixels() and
// CmdCopyImageToImageViaPixels(). Wait CP DMA copy done post these copies to simplify the barrier BLT flags management.
// e.g. GfxCmdBufferState.flags.cpBltActive would be for buffer BLT only.
static void SyncImageCpDmaCopy(
    const CmdUtil& cmdUtil,
    GfxCmdBuffer*  pCmdBuffer)
{
    if (pCmdBuffer->GetCmdBufState().flags.cpBltActive)
    {
        auto* pCmdStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += cmdUtil.BuildWaitDmaData(pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);

        pCmdBuffer->SetCpBltState(false);
    }
}

// ====================================================================================================================
// Implement a horribly inefficient copy on a pixel-by-pixel basis of the pixels that were missed by the standard
// copy algorithm.
void RsrcProcMgr::CmdCopyMemoryFromToImageViaPixels(
    GfxCmdBuffer*                 pCmdBuffer,
    const Pal::Image&             image,
    const GpuMemory&              memory,
    const MemoryImageCopyRegion&  region,
    bool                          includePadding,
    bool                          imageIsSrc
    ) const
{
    const auto&     createInfo       = image.GetImageCreateInfo();
    const auto*     pPalDevice       = m_pDevice->Parent();
    const auto*     pSrcMem          = ((imageIsSrc) ? image.GetBoundGpuMemory().Memory() : &memory);
    const auto*     pDstMem          = ((imageIsSrc) ? &memory : image.GetBoundGpuMemory().Memory());
    const Extent3d  hwCopyDims       = GetCopyViaSrdCopyDims(image, region.imageSubres, includePadding);
    const bool      is3dImage        = (createInfo.imageType == ImageType::Tex3d);
    const uint32    sliceOffset      = (is3dImage ? region.imageOffset.z : region.imageSubres.arraySlice);
    const uint32    sliceDepth       = (is3dImage ? region.imageExtent.depth : region.numSlices);
    ADDR_HANDLE     hAddrLib         = pPalDevice->AddrLibHandle();

    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT input = {};

    FillAddr2ComputeSurfaceAddrFromCoord(&input, image, region.imageSubres);

    for (uint32  sliceIdx = 0; sliceIdx < sliceDepth; sliceIdx++)
    {
        // the slice input is used for both 2D arrays and 3D slices.
        input.slice = sliceOffset + sliceIdx;

        for (uint32  yIdx = 0; yIdx < region.imageExtent.height; yIdx++)
        {
            input.y = yIdx + region.imageOffset.y;

            // If the default copy algorithm (done previously) has already seen this scanline, then we can bias
            // the starting X coordinate over to skip the region already copied by the default copy implementation.
            // If this entire scanline was invisible to default copy function though, we have to do the entire thing.
            const uint32  startX = ((input.y < hwCopyDims.height)
                                    ? hwCopyDims.width
                                    : 0);

            // It's possible that the default copy algorithm already handled an entire scanline of this region.  If
            // so, there's nothing to do here.
            if (startX < region.imageExtent.width)
            {
                // Batch up all the copies in the "X" direction in one auto-buffer that we can submit in
                // one swell foop
                AutoBuffer<MemoryCopyRegion, 32, Platform> newRegions(region.imageExtent.width,
                                                                      m_pDevice->GetPlatform());

                uint32  newRegionsIdx = 0;
                for (uint32  xIdx = startX; xIdx < region.imageExtent.width; xIdx++)
                {
                    input.x = xIdx + region.imageOffset.x;

                    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT  output = {};
                    output.size = sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT);

                    const ADDR_E_RETURNCODE retCode = Addr2ComputeSurfaceAddrFromCoord(
                                                        hAddrLib,
                                                        &input,
                                                        &output);

                    if (retCode == ADDR_OK)
                    {
                        const gpusize  imgOffset = image.GetBoundGpuMemory().Offset() + output.addr;
                        const gpusize  memOffset = region.gpuMemoryOffset                +
                                                   sliceIdx * region.gpuMemoryDepthPitch +
                                                   yIdx     * region.gpuMemoryRowPitch   +
                                                   xIdx     * (input.bpp >> 3);

                        newRegions[newRegionsIdx].srcOffset = (imageIsSrc ? imgOffset : memOffset);
                        newRegions[newRegionsIdx].dstOffset = (imageIsSrc ? memOffset : imgOffset);
                        newRegions[newRegionsIdx].copySize  = input.bpp >> 3;

                        newRegionsIdx++;
                    }
                    else
                    {
                        // What happens?
                        PAL_ASSERT_ALWAYS();
                    }
                } // End loop through "x" pixels

                CmdCopyMemory(pCmdBuffer, *pSrcMem, *pDstMem, newRegionsIdx, &newRegions[0]);
            }
        } // End loop through "y" pixels
    } // end loop through the slices

    // Wait image CP DMA blt done explicitly to simplify GfxCmdBufferState.flags.cpBltActive handling.
    SyncImageCpDmaCopy(m_cmdUtil, pCmdBuffer);
}

// ====================================================================================================================
// Returns true if the CmdCopyMemoryFromToImageViaPixels function needs to be used
bool RsrcProcMgr::UsePixelCopy(
    const Pal::Image&             image,
    const MemoryImageCopyRegion&  region)
{
    bool usePixelCopy = true;

    const AddrSwizzleMode swizzleMode =
                static_cast<AddrSwizzleMode>(image.GetGfxImage()->GetSwTileMode(image.SubresourceInfo(0)));

    if (AddrMgr2::IsNonBcViewCompatible(swizzleMode, image.GetImageCreateInfo().imageType))
    {
        usePixelCopy = false;
    }

    if (usePixelCopy)
    {
        const Extent3d  hwCopyDims = GetCopyViaSrdCopyDims(image, region.imageSubres, true);

        // If the default implementation copy dimensions did not cover the region specified by this region, then
        // we need to copy the remaining pixels the slow way.
        usePixelCopy = ((hwCopyDims.width  < (region.imageOffset.x + region.imageExtent.width))  ||
                        (hwCopyDims.height < (region.imageOffset.y + region.imageExtent.height)) ||
                        (hwCopyDims.depth  < (region.imageOffset.z + region.imageExtent.depth)));
    }

    return usePixelCopy;
}

// ====================================================================================================================
// Implement a horribly inefficient copy on a pixel-by-pixel basis of the pixels that were missed by the standard
// copy algorithm.
void RsrcProcMgr::CmdCopyImageToImageViaPixels(
    GfxCmdBuffer*          pCmdBuffer,
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    const ImageCopyRegion& region) const
{
    const auto*     pPalDevice       = m_pDevice->Parent();
    const auto&     srcCreateInfo    = srcImage.GetImageCreateInfo();
    const auto&     dstCreateInfo    = dstImage.GetImageCreateInfo();
    const auto*     pSrcMem          = srcImage.GetBoundGpuMemory().Memory();
    const auto*     pDstMem          = dstImage.GetBoundGpuMemory().Memory();

    PAL_ASSERT(srcCreateInfo.imageType == dstCreateInfo.imageType);

    const bool      is3dImage        = (srcCreateInfo.imageType == ImageType::Tex3d);
    const uint32    srcSliceOffset   = (is3dImage ? region.srcOffset.z : region.srcSubres.arraySlice);
    const uint32    dstSliceOffset   = (is3dImage ? region.dstOffset.z : region.dstSubres.arraySlice);
    const uint32    sliceDepth       = (is3dImage ? region.extent.depth : region.numSlices);
    const Extent3d  hwSrcCopyDims    = GetCopyViaSrdCopyDims(srcImage, region.srcSubres, true);
    const Extent3d  hwDstCopyDims    = GetCopyViaSrdCopyDims(dstImage, region.dstSubres, true);
    ADDR_HANDLE     hAddrLib         = pPalDevice->AddrLibHandle();

    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT srcInput = {};
    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_INPUT dstInput = {};

    FillAddr2ComputeSurfaceAddrFromCoord(&srcInput, srcImage, region.srcSubres);
    FillAddr2ComputeSurfaceAddrFromCoord(&dstInput, dstImage, region.dstSubres);

    constexpr uint32 TotalNewRegions = 32;
    MemoryCopyRegion newRegions[TotalNewRegions];
    uint32 newRegionsIdx = 0;

    for (uint32 sliceIdx = 0; sliceIdx < sliceDepth; sliceIdx++)
    {
        // the slice input is used for both 2D arrays and 3D slices.
        srcInput.slice = srcSliceOffset + sliceIdx;
        dstInput.slice = dstSliceOffset + sliceIdx;

        for (uint32  yIdx = 0; yIdx < region.extent.height; yIdx++)
        {
            srcInput.y = yIdx + region.srcOffset.y;
            dstInput.y = yIdx + region.dstOffset.y;

            for (uint32 xIdx = 0; xIdx < region.extent.width; xIdx++)
            {
                srcInput.x = xIdx + region.srcOffset.x;
                dstInput.x = xIdx + region.dstOffset.x;

                const bool srcPixelMissing = ((hwSrcCopyDims.width  <= srcInput.x) ||
                                              (hwSrcCopyDims.height <= srcInput.y));
                const bool dstPixelMissing = ((hwDstCopyDims.width  <= dstInput.x) ||
                                              (hwDstCopyDims.height <= dstInput.y));

                if (srcPixelMissing || dstPixelMissing)
                {
                    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT srcOutput = {};
                    srcOutput.size = sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT);

                    ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT dstOutput = {};
                    dstOutput.size = sizeof(ADDR2_COMPUTE_SURFACE_ADDRFROMCOORD_OUTPUT);

                    const ADDR_E_RETURNCODE retSrcCode = Addr2ComputeSurfaceAddrFromCoord(
                                                        hAddrLib,
                                                        &srcInput,
                                                        &srcOutput);

                    const ADDR_E_RETURNCODE retDstCode = Addr2ComputeSurfaceAddrFromCoord(
                                                        hAddrLib,
                                                        &dstInput,
                                                        &dstOutput);

                    if ((retSrcCode == ADDR_OK) && (retDstCode == ADDR_OK))
                    {
                        PAL_ASSERT(srcInput.bpp == dstInput.bpp);

                        newRegions[newRegionsIdx].srcOffset = srcImage.GetBoundGpuMemory().Offset() + srcOutput.addr;
                        newRegions[newRegionsIdx].dstOffset = dstImage.GetBoundGpuMemory().Offset() + dstOutput.addr;
                        newRegions[newRegionsIdx].copySize  = srcInput.bpp >> 3;

                        newRegionsIdx++;

                        if (newRegionsIdx >= TotalNewRegions)
                        {
                            CmdCopyMemory(pCmdBuffer, *pSrcMem, *pDstMem, newRegionsIdx, &newRegions[0]);
                            newRegionsIdx = 0;
                        }
                    }
                    else
                    {
                        // Incorrect offset.
                        PAL_ASSERT_ALWAYS();
                    }
                }
            }
        }
    }

    if (newRegionsIdx > 0)
    {
        CmdCopyMemory(pCmdBuffer, *pSrcMem, *pDstMem, newRegionsIdx, &newRegions[0]);
    }

    // Wait image CP DMA blt done explicitly to simplify GfxCmdBufferState.flags.cpBltActive handling.
    SyncImageCpDmaCopy(m_cmdUtil, pCmdBuffer);
}

// ====================================================================================================================
// Returns true if the CmdCopyImageToImageViaPixels function needs to be used
bool RsrcProcMgr::UsePixelCopyForCmdCopyImage(
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    const ImageCopyRegion& region)
{
    const Extent3d hwSrcCopyDims = GetCopyViaSrdCopyDims(srcImage, region.srcSubres, true);
    const Extent3d hwDstCopyDims = GetCopyViaSrdCopyDims(dstImage, region.dstSubres, true);

    // Check If the default implementation copy dimensions did not cover the region
    const bool srcPixelOutOfDims = ((hwSrcCopyDims.width  < (region.srcOffset.x + region.extent.width))  ||
                                    (hwSrcCopyDims.height < (region.srcOffset.y + region.extent.height)) ||
                                    (hwSrcCopyDims.depth  < (region.srcOffset.z + region.extent.depth)));

    const bool dstPixelOutOfDims = ((hwDstCopyDims.width  < (region.dstOffset.x + region.extent.width))  ||
                                    (hwDstCopyDims.height < (region.dstOffset.y + region.extent.height)) ||
                                    (hwDstCopyDims.depth  < (region.dstOffset.z + region.extent.depth)));

    return (srcPixelOutOfDims || dstPixelOutOfDims);
}

// =====================================================================================================================
// Some products need HW workarounds if the stencil buffer bound to the rendering pipeline is copied into via shader
// image stores.
bool RsrcProcMgr::CopyDstBoundStencilNeedsWa(
    const GfxCmdBuffer* pCmdBuffer,
    const Pal::Image&   dstImage
    ) const
{
    bool  copyDstIsBoundStencil = false;

    const auto* pPalDevice = m_pDevice->Parent();
    const auto& settings   = GetGfx9Settings(*pPalDevice);

    // Workaround is only needed if the HW supports VRS
    if ((pPalDevice->ChipProperties().gfxip.supportsVrs != 0) &&
        // And this HW is affected by the bug...
        (settings.waVrsStencilUav != WaVrsStencilUav::NoFix)  &&
        // We only need to fix things on command buffers that support gfx.  If this is a compute-only command buffer
        // then the VRS data will get corrupted but we'll fix it when the image is bound as a depth view in the next
        // universal command buffer as that will trigger an RPM fixup copy of hTile's VRS.
        pCmdBuffer->IsGraphicsSupported()                     &&
        // If there isn't a stencil plane to this image, then the problem can't happen
        dstImage.HasStencilPlane())
    {
        const auto*  pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);
        const auto&  graphicsState       = pUniversalCmdBuffer->GetGraphicsState();
        const auto&  boundDepthTarget    = graphicsState.bindTargets.depthTarget;
        const auto*  pBoundDepthView     = static_cast<const DepthStencilView*>(boundDepthTarget.pDepthStencilView);
        const auto*  pBoundDepthImage    = ((pBoundDepthView != nullptr) ? pBoundDepthView->GetImage() : nullptr);
        const auto*  pGfxDstImage        = static_cast<Image*>(dstImage.GetGfxImage());
        const auto*  pDstHtile           = pGfxDstImage->GetHtile();

        // Are we copying into the currently bound stencil image?  If not, then the copy can corrupt the VRS data as
        // VRS will be fixed when this image is next bound as a depth view.
        if ((pBoundDepthImage == pGfxDstImage) &&
            // Does our destination image have hTile data with a VRS component at all?  If not, there's nothing
            // to get corrupted.
            (pDstHtile != nullptr)             &&
            (pDstHtile->GetHtileUsage().vrs != 0))
        {
            copyDstIsBoundStencil = true;
        }
    }

    return copyDstIsBoundStencil;
}

// ====================================================================================================================
void RsrcProcMgr::CmdCopyMemoryToImage(
    GfxCmdBuffer*                pCmdBuffer,
    const GpuMemory&             srcGpuMemory,
    const Pal::Image&            dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    Pal::RsrcProcMgr::CmdCopyMemoryToImage(pCmdBuffer,
                                           srcGpuMemory,
                                           dstImage,
                                           dstImageLayout,
                                           regionCount,
                                           pRegions,
                                           includePadding);

    const ImageCreateInfo& createInfo = dstImage.GetImageCreateInfo();

    if (Formats::IsBlockCompressed(createInfo.swizzledFormat.format) &&
        (createInfo.mipLevels > 1))
    {
        // Unlike in the image-to-memory counterpart function, we don't have to wait for the above compute shader
        // to finish because the unaddressable image pixels can not be written, so there's no write conflicts.

        // The default copy-memory-to-image algorithm copies BCn images as 32-32-uint.  This leads to the SRDs
        // being setup in terms of block dimensions (as opposed to expanded pixel dimensions), which in turn can
        // ultimately lead to a mismatch of mip level sizes.
        for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
        {
            const auto& region = pRegions[regionIdx];

            if (UsePixelCopy(dstImage, region))
            {
                CmdCopyMemoryFromToImageViaPixels(pCmdBuffer, dstImage, srcGpuMemory, region, includePadding, false);
            }
        } // end loop through copy regions
    } // end check for trivial case

    // If there's no VRS support, then there's no need to check this.
    if (CopyDstBoundStencilNeedsWa(pCmdBuffer, dstImage))
    {
        for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
        {
            if (dstImage.IsStencilPlane(pRegions[regionIdx].imageSubres.plane))
            {
                // Mark the VRS dest image as dirty to force an update of Htile on the next draw.
                pCmdBuffer->DirtyVrsDepthImage(&dstImage);

                // No need to loop through all the regions; they all affect the same image.
                break;
            }
        }
    }
}

// ====================================================================================================================
void RsrcProcMgr::CmdCopyImageToMemory(
    GfxCmdBuffer*                pCmdBuffer,
    const Pal::Image&            srcImage,
    ImageLayout                  srcImageLayout,
    const GpuMemory&             dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions,
    bool                         includePadding
    ) const
{
    const auto& createInfo   = srcImage.GetImageCreateInfo();

    Pal::RsrcProcMgr::CmdCopyImageToMemory(pCmdBuffer,
                                           srcImage,
                                           srcImageLayout,
                                           dstGpuMemory,
                                           regionCount,
                                           pRegions,
                                           includePadding);

    // The default copy-image-to-memory algorithm copies BCn images as 32-32-uint.  This leads to the SRDs
    // being setup in terms of block dimensions (as opposed to expanded pixel dimensions), which in turn can
    // ultimately lead to a mismatch of mip level sizes.  Look through all the regions to see if something "bad"
    // happened.
    if (Formats::IsBlockCompressed(createInfo.swizzledFormat.format) &&
        (createInfo.mipLevels > 1))
    {
        bool issuedCsPartialFlush = false;

        for (uint32 regionIdx = 0; regionIdx < regionCount; regionIdx++)
        {
            const auto& region = pRegions[regionIdx];

            if (UsePixelCopy(srcImage, region))
            {
                // We have to wait for the compute shader invoked above to finish...  Otherwise, it will be writing
                // zeroes into the destination memory that correspond to pixels that it couldn't read.  This only
                // needs to be done once before the first pixel-level copy.
                if (issuedCsPartialFlush == false)
                {
                    Pal::CmdStream*  pPalCmdStream = pCmdBuffer->GetMainCmdStream();
                    CmdStream*       pGfxCmdStream = static_cast<CmdStream*>(pPalCmdStream);
                    uint32*          pCmdSpace     = pGfxCmdStream->ReserveCommands();
                    const EngineType engineType    = pGfxCmdStream->GetEngineType();

                    pCmdSpace = pCmdBuffer->WriteWaitCsIdle(pCmdSpace);

                    // Two things can happen next. We will either be copying the leftover pixels with CPDMA or with
                    // more CS invocations. CPDMA is preferred, but we will fallback on CS if the copy is too large.
                    // That's very unlikely since we're copying pixels individually; the largest possible copy size
                    // is just 16 bytes! Basically, it should only happen if the client sets this setting to zero to
                    // disable CPDMA.
                    if (m_pDevice->Parent()->GetPublicSettings()->cpDmaCmdCopyMemoryMaxBytes < 16)
                    {
                        // Even though we have waited for the CS to finish, we may still run into a write after write
                        // hazard. We need to flush and invalidate the L2 cache as well.
                        AcquireMemGeneric acquireInfo = {};
                        acquireInfo.engineType = engineType;
                        acquireInfo.cacheSync  = SyncGlkInv | SyncGlvInv | SyncGl1Inv | SyncGlmInv | SyncGl2WbInv;
                        acquireInfo.rangeBase  = dstGpuMemory.Desc().gpuVirtAddr;
                        acquireInfo.rangeSize  = dstGpuMemory.Desc().size;

                        pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireInfo, pCmdSpace);
                    }

                    pGfxCmdStream->CommitCommands(pCmdSpace);

                    issuedCsPartialFlush = true;
                }

                CmdCopyMemoryFromToImageViaPixels(pCmdBuffer, srcImage, dstGpuMemory, region, includePadding, true);
            }
        } // end loop through copy regions
    } // end check for trivial case
}

// =====================================================================================================================
// The queue preamble streams set COMPUTE_USER_DATA_0 to the address of the global internal table, as required by the
// PAL compute pipeline ABI. If we overwrite that register in a command buffer we need some way to restore it the next
// time we bind a compute pipeline. We don't know the address of the internal table at the time we build command
// buffers so we must query it dynamically on the GPU. Unfortunately the CP can't read USER_DATA registers so we must
// use a special pipeline to simply read the table address from user data and write it to a known GPU address.
//
// This function binds and executes that special compute pipeline. It will write the low 32-bits of the global internal
// table address to dstAddr. Later on, we can tell the CP to read those bits and write them to COMPUTE_USER_DATA_0.
void RsrcProcMgr::EchoGlobalInternalTableAddr(
    GfxCmdBuffer* pCmdBuffer,
    gpusize       dstAddr
    ) const
{
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    const Pal::ComputePipeline*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9EchoGlobalTable);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash});

    // Note we start at userdata2 here because the pipeline is special and userdata0/1 are marked unused but
    // overlap the global table.
    const uint32 userData[2] = { LowPart(dstAddr), HighPart(dstAddr) };
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 2, userData );
    pCmdBuffer->CmdDispatch({1, 1, 1}, {});
    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);

    // We need a CS wait-for-idle before we try to restore the global internal table user data. There are a few ways
    // we could acomplish that, but the most simple way is to just do a wait for idle right here. We only need to
    // call this function once per command buffer (and only if we use a non-PAL ABI pipeline) so it should be fine.
    Pal::CmdStream* pCmdStream = pCmdBuffer->GetMainCmdStream();
    uint32*         pCmdSpace  = pCmdStream->ReserveCommands();

    pCmdSpace = pCmdBuffer->WriteWaitCsIdle(pCmdSpace);

    if (pCmdBuffer->IsGraphicsSupported())
    {
        // Note that we also need a PFP_SYNC_ME on any graphics queues because the PFP loads from this memory.
        pCmdSpace += m_cmdUtil.BuildPfpSyncMe(pCmdSpace);
    }

    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Return true if the image has FMask and provided layout is in ColorCompressed state.
static bool IsImageWithFMaskAndInCompressedState(
    const Pal::Image& dstImage,
    ImageLayout       dstImageLayout)
{
    const auto&                 gfx9Image     = static_cast<const Gfx9::Image&>(*dstImage.GetGfxImage());
    const ColorLayoutToState    layoutToState = gfx9Image.LayoutToColorCompressionState();
    const ColorCompressionState newState      = ImageLayoutToColorCompressionState(layoutToState, dstImageLayout);

    return gfx9Image.HasFmaskData() && (newState == ColorCompressed);
}

// =====================================================================================================================
// This must be called before and after each compute copy. The pre-copy call will insert any required metadata
// decompresses and the post-copy call will fixup any metadata that needs updating. In practice these barriers are
// required in cases where we treat CopyDst as compressed but RPM can't actually write compressed data directly from
// the compute shader.
void RsrcProcMgr::FixupMetadataForComputeCopyDst(
    GfxCmdBuffer*           pCmdBuffer,
    const Pal::Image&       dstImage,
    ImageLayout             dstImageLayout,
    uint32                  regionCount,
    const ImageFixupRegion* pRegions,
    bool                    beforeCopy,
    const Pal::Image*       pFmaskOptimizedCopySrcImage // Copy src image pointer if FMask optimized copy otherwise
                                                        // should be nullptr.
    ) const
{
    const GfxImage* pGfxImage = static_cast<GfxImage*>(dstImage.GetGfxImage());

    if (pGfxImage->HasHtileData())
    {
        // There is a Hiz issue on gfx10 with compressed depth writes so we need an htile resummarize blt.
        const bool enableCompressedDepthWriteTempWa = IsGfx10(*m_pDevice->Parent());

        // If enable temp workaround for comrpessed depth write, always need barriers for before and after copy.
        bool needBarrier = enableCompressedDepthWriteTempWa;
        for (uint32 i = 0; (needBarrier == false) && (i < regionCount); i++)
        {
            needBarrier = pGfxImage->ShaderWriteIncompatibleWithLayout(pRegions[i].subres, dstImageLayout);
        }

        if (needBarrier)
        {
            AutoBuffer<ImgBarrier, 32, Platform> imgBarriers(regionCount, m_pDevice->GetPlatform());

            if (imgBarriers.Capacity() >= regionCount)
            {
                const uint32 shaderWriteLayout =
                    (enableCompressedDepthWriteTempWa ? (LayoutShaderWrite | LayoutUncompressed) : LayoutShaderWrite);

                memset(&imgBarriers[0], 0, sizeof(ImgBarrier) * regionCount);

                for (uint32 i = 0; i < regionCount; i++)
                {
                    imgBarriers[i].pImage       = &dstImage;
                    imgBarriers[i].subresRange  = SubresourceRange(pRegions[i].subres, 1, 1, pRegions[i].numSlices);
                    imgBarriers[i].srcStageMask = beforeCopy ? PipelineStageBottomOfPipe : PipelineStageCs;
                    imgBarriers[i].dstStageMask = PipelineStageBlt;
                    imgBarriers[i].oldLayout    = dstImageLayout;
                    imgBarriers[i].newLayout    = dstImageLayout;

                    // The first barrier must prepare the image for shader writes, perhaps by decompressing metadata.
                    // The second barrier is required to undo those changes, perhaps by resummarizing the metadata.
                    if (beforeCopy)
                    {
                        // Can optimize depth expand to lighter Barrier with UninitializedTarget for full subres copy.
                        const SubResourceInfo* pSubresInfo = dstImage.SubresourceInfo(pRegions[i].subres);

                        if (BoxesCoverWholeExtent(pSubresInfo->extentElements, 1, &pRegions[i].dstBox))
                        {
                            imgBarriers[i].oldLayout.usages = LayoutUninitializedTarget;
                        }

                        imgBarriers[i].newLayout.usages |= shaderWriteLayout;
                        imgBarriers[i].srcAccessMask     = CoherCopyDst;
                        imgBarriers[i].dstAccessMask     = CoherShader;
                    }
                    else // After copy
                    {
                        imgBarriers[i].oldLayout.usages |= shaderWriteLayout;
                        imgBarriers[i].srcAccessMask     = CoherShader;
                        imgBarriers[i].dstAccessMask     = CoherCopyDst;
                    }
                }

                // Operations like resummarizes might read the blit's output so we can't optimize the wait point.
                AcquireReleaseInfo acqRelInfo = {};
                acqRelInfo.pImageBarriers    = &imgBarriers[0];
                acqRelInfo.imageBarrierCount = regionCount;
                acqRelInfo.reason            = Developer::BarrierReasonUnknown;

                pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
            }
            else
            {
                pCmdBuffer->NotifyAllocFailure();
            }
        }
    }

    // Check to see if need fix up CopyDst metadata before and after copy.
    //
    // Color MSAA copy always goes through compute copy. In InitLayoutStateMasks(), we may set color MSAA image with
    // supporting compressed copy if (supportMetaDataTexFetch == 1) and (DoesImageSupportCopyCompression() == true),
    // but compute copy doesn't update FMask/CMask for CopyDst image, need extra steps to maintain data consistence
    // with FMak if CopyDst is in ColorCompressed state after copy. Generally speaking, need force a color expand
    // before copy but it's heavy; can optimize a bit as below,
    //   1). For windowed copy, do color expand before copy.
    //   2). For full copy, fix up FMask/CMask to expanded state after copy as an optimization.
    //
    // FMask optimized copy and image created with `fullCopyDstOnly` flag need fix up metadata after copy.
    //   1). For `fullCopyDstOnly` flag case, LayoutCopyDst is added in compressedWriteLayout and there will be no
    //       expand in barrier before copy. Need fix up metadata to expanded state after copy.
    //   2). For FMask optimized copy, need copy src image's metadata to dst image's metadata as raw copy.
    if (beforeCopy)
    {
        // Do color expand on color MSAA image for windowed copy if needed.
        if (IsImageWithFMaskAndInCompressedState(dstImage, dstImageLayout) &&
            (UseOptimizedFixupMsaaImageAfterCopy(dstImage, pRegions, regionCount) == false))
        {
            AutoBuffer<ImgBarrier, 8, Platform> imgBarriers(regionCount, m_pDevice->GetPlatform());

            if (imgBarriers.Capacity() >= regionCount)
            {
                memset(&imgBarriers[0], 0, sizeof(ImgBarrier) * regionCount);

                // The CopyDst should be in PipelineStageBlt and CoherCopyDst state before the copy. Issue a barrier
                // to do in-place color expand without state transition.
                for (uint32 i = 0; i < regionCount; i++)
                {
                    imgBarriers[i].pImage        = &dstImage;
                    imgBarriers[i].subresRange   = SubresourceRange(pRegions[i].subres, 1, 1, pRegions[i].numSlices);
                    imgBarriers[i].srcStageMask  = PipelineStageBlt;
                    imgBarriers[i].dstStageMask  = PipelineStageBlt;
                    imgBarriers[i].srcAccessMask = CoherCopyDst;
                    imgBarriers[i].dstAccessMask = CoherCopyDst;
                    imgBarriers[i].oldLayout     = dstImageLayout;
                    imgBarriers[i].newLayout     = dstImageLayout;

                    imgBarriers[i].newLayout.usages |= LayoutUncompressed; // Force color expand.
                }

                AcquireReleaseInfo acqRelInfo = {};
                acqRelInfo.pImageBarriers    = &imgBarriers[0];
                acqRelInfo.imageBarrierCount = regionCount;
                acqRelInfo.reason            = Developer::BarrierReasonUnknown;

                pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
            }
            else
            {
                pCmdBuffer->NotifyAllocFailure();
            }
        }
    }
    else // After copy
    {
        const bool isFmaskCopyOptimized = (pFmaskOptimizedCopySrcImage != nullptr);

        if (isFmaskCopyOptimized ||
            (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0) ||
            (IsImageWithFMaskAndInCompressedState(dstImage, dstImageLayout) &&
             UseOptimizedFixupMsaaImageAfterCopy(dstImage, pRegions, regionCount)))
        {
            HwlFixupCopyDstImageMetadata(pCmdBuffer, pFmaskOptimizedCopySrcImage, dstImage, dstImageLayout, pRegions,
                                         regionCount, isFmaskCopyOptimized);
        }
    }
}

// =====================================================================================================================
// Executes a compute shader which generates a PM4 command buffer which can later be executed. If the number of indirect
// commands being generated will not fit into a single command-stream chunk, this will issue multiple dispatches, one
// for each command chunk to generate.
void RsrcProcMgr::CmdGenerateIndirectCmds(
    const IndirectCmdGenerateInfo& genInfo,
    CmdStreamChunk**               ppChunkLists[],
    uint32*                        pNumGenChunks
    ) const
{
    const auto*                      pPublicSettings = m_pDevice->Parent()->GetPublicSettings();
    const auto&                      chipProps       = m_pDevice->Parent()->ChipProperties();
    const gpusize                    argsGpuAddr     = genInfo.argsGpuAddr;
    const gpusize                    countGpuAddr    = genInfo.countGpuAddr;
    const Pipeline*                  pPipeline       = genInfo.pPipeline;
    const Pal::IndirectCmdGenerator& generator       = genInfo.generator;
    const auto&                      gfx9Generator   = static_cast<const IndirectCmdGenerator&>(generator);
    GfxCmdBuffer*                    pCmdBuffer      = genInfo.pCmdBuffer;
    uint32                           indexBufSize    = genInfo.indexBufSize;
    uint32                           maximumCount    = genInfo.maximumCount;

    const Pal::ComputePipeline* pGenerationPipeline = GetCmdGenerationPipeline(generator, *pCmdBuffer);
    const DispatchDims          threadsPerGroup     = pGenerationPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pGenerationPipeline, InternalApiPsoHash, });

    // The command-generation pipelines expect the following descriptor-table layout for the resources which are the
    // same for each command-stream chunk being generated:
    //  + Raw-buffer SRD for the indirect argument data (4 DW)
    //  + Structured-buffer SRD for the command parameter data (4 DW)
    //  + Typed buffer SRD for the user-data entry mapping table for each shader stage (4 DW)
    //  + Structured-buffer SRD for the pipeline signature (4 DW)
    //  + Structured-buffer SRD for the second pipeline signature (4 DW)
    //  + Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the main chunk. (4 DW)
    //  + Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the task chunk. (4 DW)
    //  + Constant buffer SRD for the command-generator properties (4 DW)
    //  + Constant buffer SRD for the properties of the ExecuteIndirect() invocation (4 DW)
    //  + GPU address of the memory containing the count of commands to generate (2 DW)
    //  + Issue THREAD_TRACE_MARKER after draw or dispatch (1 DW)
    //  + Task Shader Enabled flag (1 DW)

    constexpr uint32 SrdDwords = 4;
    PAL_ASSERT((chipProps.srdSizes.typedBufferView   == sizeof(uint32) * SrdDwords) &&
               (chipProps.srdSizes.untypedBufferView == sizeof(uint32) * SrdDwords));

    const bool taskShaderEnabled = ((generator.Type() == GeneratorType::DispatchMesh) &&
                                    (static_cast<const Pal::GraphicsPipeline*>(pPipeline)->HasTaskShader()));

    // The generation pipelines expect the descriptor table's GPU address to be written to user-data #0-1.
    gpusize tableGpuAddr = 0uLL;

    uint32* pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((9 * SrdDwords) + 4), 1, &tableGpuAddr);
    PAL_ASSERT(pTableMem != nullptr);

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

    // Raw-buffer SRD for the indirect-argument data:
    BufferViewInfo viewInfo = { };
    viewInfo.gpuAddr        = argsGpuAddr;
    viewInfo.swizzledFormat = UndefinedSwizzledFormat;
    viewInfo.range          = (generator.Properties().argBufStride * maximumCount);
    viewInfo.stride         = 1;
    viewInfo.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    viewInfo.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
#if PAL_BUILD_GFX12
    viewInfo.compressionMode       = CompressionMode::ReadEnableWriteDisable;
#endif
    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
    pTableMem += SrdDwords;

    // Structured-buffer SRD for the command parameter data:
    gfx9Generator.PopulateParameterBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Typed-buffer SRD for the user-data entry mappings:
    gfx9Generator.PopulateUserDataMappingBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Structured buffer SRD for the pipeline signature:
    gfx9Generator.PopulateSignatureBuffer(pCmdBuffer, pPipeline, pTableMem);
    if (generator.Type() == GeneratorType::DispatchMesh)
    {
        // In the case of DispatchMesh, PopulateSignatureBuffer will allocate an additional SRD hence the increment.
        pTableMem += SrdDwords;
    }
    pTableMem += SrdDwords;

    // Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the main chunk.
    uint32* pReturnIbAddrTableMem = pTableMem;
    memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
    pTableMem += SrdDwords;

    // Raw-buffer SRD pointing to return-to-caller INDIRECT_BUFFER packet location for the task chunk.
    uint32* pReturnTaskIbAddrTableMem = pTableMem;
    if (generator.Type() == GeneratorType::DispatchMesh)
    {
        memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
        pTableMem += SrdDwords;
    }

    // Constant buffer SRD for the command-generator properties:
    gfx9Generator.PopulatePropertyBuffer(pCmdBuffer, pPipeline, pTableMem);
    pTableMem += SrdDwords;

    // Constant buffer SRD for the properties of the ExecuteIndirect() invocation:
    gfx9Generator.PopulateInvocationBuffer(pCmdBuffer,
                                           pPipeline,
                                           taskShaderEnabled,
                                           argsGpuAddr,
                                           maximumCount,
                                           indexBufSize,
                                           pTableMem);
    pTableMem += SrdDwords;

    // GPU address of the memory containing the actual command count to generate:
    memcpy(pTableMem, &countGpuAddr, sizeof(countGpuAddr));
    pTableMem += 2;

    // Flag to decide whether to issue THREAD_TRACE_MARKER following generated draw/dispatch commands.
    pTableMem[0] = m_pDevice->Parent()->IssueSqttMarkerEvents();
    pTableMem[1] = taskShaderEnabled;

    // These will be used for tracking the postamble size of the main and task chunks respectively.
    uint32 postambleDwords    = 0;
    uint32 postambleDwordsAce = 0;

    uint32 commandIdOffset = 0;
    while (commandIdOffset < maximumCount)
    {
        // Obtain a command-stream chunk for generating commands into. This also sets-up the padding requirements
        // for the chunk and determines the number of commands which will safely fit. We'll need to build a raw-
        // buffer SRD so the shader can access the command buffer as a UAV.
        ChunkOutput output[2]  = {};
        const uint32 numChunks = (taskShaderEnabled) ? 2 : 1;
        pCmdBuffer->GetChunkForCmdGeneration(generator,
                                             *pPipeline,
                                             (maximumCount - commandIdOffset),
                                             numChunks,
                                             output);

        ChunkOutput& mainChunk          = output[0];
        ppChunkLists[0][*pNumGenChunks] = mainChunk.pChunk;

        postambleDwords = mainChunk.chainSizeInDwords;

        // The command generation pipeline also expects the following descriptor-table layout for the resources
        // which change between each command-stream chunk being generated:
        //  + Raw buffer UAV SRD for the command-stream chunk to generate (4 DW)
        //  + Raw buffer UAV SRD for the embedded data segment to use for the spill table (4 DW)
        //  + Raw buffer UAV SRD pointing to current chunk's INDIRECT_BUFFER packet that chains to the next chunk (4 DW)
        //  + Command ID offset for the current command-stream-chunk (1 DW)
        //  + Low half of the GPU virtual address of the spill table's embedded data segment (1 DW)
        //  + Low half of the GPU virtual address of the spill table's embedded data segment for task shader (1 DW)

        // The generation pipelines expect the descriptor table's GPU address to be written to user-data #2-3.
        pTableMem = pCmdBuffer->CmdAllocateEmbeddedData(((3 * SrdDwords) + 3), 1, &tableGpuAddr);
        PAL_ASSERT(pTableMem != nullptr);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 2, reinterpret_cast<uint32*>(&tableGpuAddr));

        // UAV buffer SRD for the command-stream-chunk to generate:
        viewInfo.gpuAddr        = mainChunk.pChunk->GpuVirtAddr();
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = (mainChunk.commandsInChunk * gfx9Generator.CmdBufStride(pPipeline));
        viewInfo.stride         = 1;
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        pTableMem += SrdDwords;

        // UAV buffer SRD for the embedded-data spill table:
        if (mainChunk.embeddedDataSize != 0)
        {
            viewInfo.gpuAddr        = mainChunk.embeddedDataAddr;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (sizeof(uint32) * mainChunk.embeddedDataSize);
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        }
        else
        {
            // If we're not using the embedded-data spill table, we still need to clear the srd to 0.
            // This prevents hangs on older hardware caused by the shader attempting to read an invalid srd.
            memset(pTableMem, 0, (sizeof(uint32) * SrdDwords));
        }

        pTableMem += SrdDwords;

        // UAV buffer SRD pointing to current chunk's INDIRECT_BUFFER packet that chains to the next chunk.
        const gpusize chainIbAddress = mainChunk.pChunk->GpuVirtAddr() +
                                       ((mainChunk.pChunk->CmdDwordsToExecute() - postambleDwords) * sizeof(uint32));

        viewInfo.gpuAddr        = chainIbAddress;
        viewInfo.swizzledFormat = UndefinedSwizzledFormat;
        viewInfo.range          = postambleDwords * sizeof(uint32);
        viewInfo.stride         = 1;
        // Value stored for this chunk's "commandBufChainIb" in the shader.
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        pTableMem += SrdDwords;

        // Command ID offset for the current command stream-chunk
        pTableMem[0] = commandIdOffset;
        // Low portion of the spill table's GPU virtual address
        pTableMem[1] = LowPart(mainChunk.embeddedDataAddr);

        // The command generation pipeline also expects the following descriptor-table layout for the resources
        // which change between each command-stream chunk being generated:
        // + Raw buffer UAV SRD for the command-stream chunk to generate (4 DW)
        // + Raw buffer UAV SRD for the embedded data segment to use for the spill table (4 DW)
        // + Raw buffer UAV SRD pointing to current task chunk's INDIRECT_BUFFER packet that chains to the next chunk
        // + (4 DW)
        if (taskShaderEnabled)
        {
            ChunkOutput& taskChunk          = output[1];
            ppChunkLists[1][*pNumGenChunks] = taskChunk.pChunk;

            postambleDwordsAce = taskChunk.chainSizeInDwords;
            // This assert validates that the following dispatch contains equivalent commands for both the DE and ACE
            // engines for this DispatchMesh pipeline.
            PAL_ASSERT(taskChunk.commandsInChunk == mainChunk.commandsInChunk);
            pTableMem[2] = LowPart(taskChunk.embeddedDataAddr);

            pTableMem = pCmdBuffer->CmdAllocateEmbeddedData((3 * SrdDwords), 1, &tableGpuAddr);
            PAL_ASSERT(pTableMem != nullptr);

            // UAV buffer SRD for the command-stream-chunk to generate:
            viewInfo.gpuAddr        = taskChunk.pChunk->GpuVirtAddr();
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (taskChunk.commandsInChunk * gfx9Generator.CmdBufStride(pPipeline));
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
            pTableMem += SrdDwords;

            // UAV buffer SRD for the embedded-data spill table:
            viewInfo.gpuAddr        = taskChunk.embeddedDataAddr;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = (sizeof(uint32) * taskChunk.embeddedDataSize);
            viewInfo.stride         = 1;
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
            pTableMem += SrdDwords;

            // UAV buffer SRD pointing to current task chunk's INDIRECT_BUFFER packet that chains to the next task
            // chunk:
            const gpusize taskChainIbAddress = taskChunk.pChunk->GpuVirtAddr() +
                                               ((taskChunk.pChunk->CmdDwordsToExecute() - postambleDwordsAce) *
                                                sizeof(uint32));

            viewInfo.gpuAddr        = taskChainIbAddress;
            viewInfo.swizzledFormat = UndefinedSwizzledFormat;
            viewInfo.range          = postambleDwordsAce * sizeof(uint32);
            viewInfo.stride         = 1;
            // Value stored for this chunk's "taskCommandBufChainIb" in the shader.
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pTableMem);
        }

        // We use the ACE for IndirectCmdGeneration only for this very special case. It has to be a UniversalCmdBuffer,
        // ganged ACE is supported, and we are not using the ACE for Task Shader work.
        bool cmdGenUseAce = pCmdBuffer->IsGraphicsSupported() &&
                            (chipProps.gfxip.supportAceOffload != 0) &&
                            (pPublicSettings->disableExecuteIndirectAceOffload != true) &&
                            (taskShaderEnabled == false);

        if (cmdGenUseAce)
        {
            pCmdBuffer->CmdDispatchAce({RpmUtil::MinThreadGroups(generator.ParameterCount(), threadsPerGroup.x),
                                        RpmUtil::MinThreadGroups(mainChunk.commandsInChunk,  threadsPerGroup.y), 1});
        }
        else
        {
            pCmdBuffer->CmdDispatch({RpmUtil::MinThreadGroups(generator.ParameterCount(), threadsPerGroup.x),
                                     RpmUtil::MinThreadGroups(mainChunk.commandsInChunk,  threadsPerGroup.y), 1},
                                     {});
        }

        (*pNumGenChunks)++;
        commandIdOffset += mainChunk.commandsInChunk;
    }

    // This will calculate the IB's return addresses that will be helpful for the CP jump/ short-circuit over possibly
    // executing long chains of NOPs.
    if (*pNumGenChunks > 0)
    {
        const CmdStreamChunk* pLastChunk = ppChunkLists[0][(*pNumGenChunks) - 1];
        const gpusize pReturnChainIbAddress = pLastChunk->GpuVirtAddr() +
                                              ((pLastChunk->CmdDwordsToExecute() - postambleDwords) * sizeof(uint32));
        viewInfo.gpuAddr               = pReturnChainIbAddress;
        viewInfo.swizzledFormat        = UndefinedSwizzledFormat;
        viewInfo.range                 = postambleDwords * sizeof(uint32);
        viewInfo.stride                = 1;
        // Value stored in "cmdBufReturningChainIb" in the shader.
        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pReturnIbAddrTableMem);

        if (taskShaderEnabled)
        {
            const CmdStreamChunk* pLastTaskChunk = ppChunkLists[1][(*pNumGenChunks) - 1];
            const gpusize pReturnTaskChainIbAddress = pLastTaskChunk->GpuVirtAddr() +
                                                      ((pLastTaskChunk->CmdDwordsToExecute() - postambleDwordsAce) *
                                                       sizeof(uint32));
            viewInfo.gpuAddr               = pReturnTaskChainIbAddress;
            viewInfo.swizzledFormat        = UndefinedSwizzledFormat;
            viewInfo.range                 = postambleDwordsAce * sizeof(uint32);
            viewInfo.stride                = 1;
            // Value stored in "taskCmdBufReturningChainIb" in the shader.
            m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &viewInfo, pReturnTaskIbAddrTableMem);
        }
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Does a compute-based fast-clear of the specified image / range.  The image's associated DCC memory is updated to
// "clearCode" for all bytes corresponding to "clearRange".
void RsrcProcMgr::ClearDccCompute(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose,
    bool               trackBltActiveFlags,
    const uint32*      pPackedClearColor
    ) const
{
    const Pal::Image*    pPalImage     = dstImage.Parent();
    const Pal::Device*   pDevice       = pPalImage->GetDevice();
    const auto&          createInfo    = pPalImage->GetImageCreateInfo();
    const uint32         startSlice    = ((createInfo.imageType == ImageType::Tex3d)
                                          ? 0
                                          : clearRange.startSubres.arraySlice);
    const uint32         clearColor    = ReplicateByteAcrossDword(clearCode);
    const auto*          pSubResInfo   = dstImage.Parent()->SubresourceInfo(clearRange.startSubres);
    const SwizzledFormat planeFormat   = pSubResInfo->format;
    const uint32         bytesPerPixel = Formats::BytesPerPixel(planeFormat.format);
    const auto*          pAddrOutput   = dstImage.GetAddrOutput(pSubResInfo);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    for (uint32 plane = clearRange.startSubres.plane;
         plane < (clearRange.startSubres.plane + clearRange.numPlanes);
         plane++)
    {
        const auto* pDcc          = dstImage.GetDcc(plane);
        const auto& dccAddrOutput = pDcc->GetAddrOutput();

        bool  clearedLastMip = false;
        for (uint32  mipIdx = 0; ((clearedLastMip == false) && (mipIdx < clearRange.numMips)); mipIdx++)
        {
            const uint32 absMipLevel = clearRange.startSubres.mipLevel + mipIdx;
            const auto&  dccMipInfo  = pDcc->GetAddrMipInfo(absMipLevel);

            // The sliceSize will be set to zero for mipLevel's that can't use DCC
            if (dccMipInfo.sliceSize != 0)
            {
                // The number of slices for 2D images is the number of slices; for 3D images, it's the depth
                // of the image for the current mip level.
                const uint32  numSlices = GetClearDepth(dstImage, plane, clearRange.numSlices, absMipLevel);

                // The "metaBlkDepth" parameter is the number of slices that the "dccRamSliceSize" covers.  For non-3D
                // images, this should always be 1 (i.e., one addrlib slice is one PAL API slice).  For 3D images, this
                // can be way more than the number of PAL API slices.
                const uint32  numSlicesToClear = Max(1u, numSlices / dccAddrOutput.metaBlkDepth);

                // GetMaskRamBaseAddr doesn't compute the base address of a mip level (only a slice offset), so
                // we have to do the math here ourselves. However, DCC memory is contiguous and traversed upon by
                // slice size, so we only need the first slice offset and the total size of all slices calculated by
                // num slices * ram slice size (if the ram slice size is identical to the mip's slice size - see below).
                const gpusize maskRamBaseAddr = dstImage.GetMaskRamBaseAddr(pDcc, 0);
                gpusize sliceOffset = startSlice * dccAddrOutput.dccRamSliceSize;
                gpusize clearAddr   = maskRamBaseAddr + sliceOffset + dccMipInfo.offset;

                // On gfx10+, metadata for all mips in each slice are packed together.
                // For an image with 3 mips and 3 slices, this is packing order from smallest offset to largest:
                //     S0M2 S0M1 S0M0 S1M2 S1M1 S1M0 S2M2 S2M1 S2M0
                // dccRamSliceSize is the distance between SN and SN+1, the size of the full mip chain. So although DCC
                // memory is contiguous per subresource, the offset of each slice is traversed by an interval of
                // dccRamSliceSize, though written to with mip slice size. Thus, we may dispatch a clear once only if
                // the two sizes match.
                const bool canDispatchSingleClear = dccMipInfo.sliceSize == dccAddrOutput.dccRamSliceSize;

                if (canDispatchSingleClear)
                {
                    const gpusize totalSize = numSlicesToClear * dccMipInfo.sliceSize;

                    CmdFillMemory(pCmdBuffer,
                                  false,         // don't save / restore the compute state
                                  trackBltActiveFlags,
                                  clearAddr,
                                  totalSize,
                                  clearColor);
                }
                else
                {
                    for (uint32  sliceIdx = 0; sliceIdx < numSlicesToClear; sliceIdx++)
                    {
                        // Get the mem offset for each slice
                        const uint32 absSlice = startSlice + sliceIdx;
                        sliceOffset = absSlice * dccAddrOutput.dccRamSliceSize;
                        clearAddr   = maskRamBaseAddr + sliceOffset + dccMipInfo.offset;

                        CmdFillMemory(pCmdBuffer,
                                      false,         // don't save / restore the compute state
                                      trackBltActiveFlags,
                                      clearAddr,
                                      dccMipInfo.sliceSize,
                                      clearColor);
                    }
                }

                if ((clearCode == uint8(Gfx9DccClearColor::Gfx10ClearColorCompToSingle)) ||
                    (clearCode == uint8(Gfx9DccClearColor::Gfx11ClearColorCompToSingle)))
                {
                    // If this image doesn't support comp-to-single fast clears, then how did we wind up with the
                    // comp-to-single clear code???
                    PAL_ASSERT(dstImage.Gfx10UseCompToSingleFastClears());

                    // If we're not doing a fast clear then how did we wind up with a fast-clear related code???
                    PAL_ASSERT(clearPurpose == DccClearPurpose::FastClear);

                    ClearDccComputeSetFirstPixelOfBlock(pCmdBuffer,
                                                        dstImage,
                                                        plane,
                                                        absMipLevel,
                                                        startSlice,
                                                        numSlices,
                                                        bytesPerPixel,
                                                        pPackedClearColor);
                }
            }
            else
            {
                // There's nothing left to do...  the mip levels are only going higher and none of them
                // will have accessible DCC memory anyway.
                clearedLastMip = true;

                // Image setup (see the Gfx9::Image::Finalize function) should have prevented the use of
                // fast-clears for any mip levels with zero-sized slices.  We can still get here for inits
                // though.
                PAL_ASSERT(clearPurpose == DccClearPurpose::Init);
            }
        }
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData, trackBltActiveFlags);

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
}

// =====================================================================================================================
// Use a compute shader to write the clear color to the first byte of each 256B block.
void RsrcProcMgr::ClearDccComputeSetFirstPixelOfBlock(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    uint32             plane,
    uint32             absMipLevel,
    uint32             startSlice,  // 0 for 3d or start array slice for 2d array.
    uint32             numSlices,   // depth for 3d or number of array slices for 2d array.
    uint32             bytesPerPixel,
    const uint32*      pPackedClearColor
    ) const
{
    PAL_ASSERT(pPackedClearColor != nullptr);

    const Pal::Image*        pPalImage  = dstImage.Parent();
    const ImageCreateInfo&   createInfo = pPalImage->GetImageCreateInfo();
    const Gfx9Dcc*           pDcc       = dstImage.GetDcc(plane);
    const RpmComputePipeline pipeline   = (((createInfo.samples == 1) ||
                                            //   On GFX11, MSAA surfaces are sample interleaved, the way depth always
                                            //   has been.
                                            //
                                            // Since "GetXyzInc" already incorporates the # of samples,  we don't
                                            // have to store the clear color for each sample anymore.
                                            IsGfx11(*(m_pDevice->Parent())))
                                                ? RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixel
                                                : RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixelMsaa);
    const Pal::ComputePipeline*const pPipeline = GetPipeline(pipeline);

    // Bind Compute Pipeline used for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    uint32 xInc = 0;
    uint32 yInc = 0;
    uint32 zInc = 0;
    pDcc->GetXyzInc(&xInc, &yInc, &zInc);

    SwizzledFormat planeFormat = {};
    planeFormat.swizzle.r = ChannelSwizzle::X;
    planeFormat.swizzle.g = ChannelSwizzle::Zero;
    planeFormat.swizzle.b = ChannelSwizzle::Zero;
    planeFormat.swizzle.a = ChannelSwizzle::One;

    switch (bytesPerPixel)
    {
    case 1:
        planeFormat.format = ChNumFormat::X8_Uint;

        // With an 8bpp format, one DCC byte covers a 16x16 pixel region.  However, for reasons
        // of GFX10 addressing weirdness, writing the clear color once for every (16,16) region
        // isn't sufficient...  so write it every (8,8) instead
        xInc = Util::Min(8u, xInc);
        yInc = Util::Min(8u, yInc);
        break;
    case 2:
        planeFormat.format = ChNumFormat::X16_Uint;
        break;
    case 4:
        planeFormat.format = ChNumFormat::X32_Uint;
        break;
    case 8:
        planeFormat.format = ChNumFormat::X32Y32_Uint;

        // This is the only dual channel export, so the "Y" becomes important
        planeFormat.swizzle.g = ChannelSwizzle::Y;
        break;
    case 16:
        // We can't fast clear a surface with more than 64bpp, so we shouldn't get here.
        PAL_ASSERT_ALWAYS();
        break;
    default:
        PAL_ASSERT_ALWAYS();
    }

    const SubresId  subresId       = Subres(0, absMipLevel, startSlice);
    const Extent3d& extentTexels   = pPalImage->SubresourceInfo(subresId)->extentTexels;
    const uint32    mipLevelWidth  = extentTexels.width;
    const uint32    mipLevelHeight = extentTexels.height;
    const uint32    mipLevelDepth  = numSlices;

    // We carefully built this constant buffer so that we can fit all constants plus an image SRD in fast user-data.
    constexpr uint32 ConstCount = 6;
    const uint32 constData[ConstCount] =
    {
        // start cb0[0]
        mipLevelWidth,
        mipLevelHeight,
        mipLevelDepth,
        RpmUtil::PackFourBytes(xInc, yInc, zInc, createInfo.samples),
        // start cb0[1]
        // Because we can't fast clear a surface with more than 64bpp, there's no need to pass in
        // pPackedClearColor[2] and pPackedClearColor[3].
        pPackedClearColor[0],
        pPackedClearColor[1]
    };

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, ConstCount, constData);

    const Pal::Device& device   = *pPalImage->GetDevice();
    const SubresRange viewRange =
        SubresourceRange(subresId, 1, 1, (createInfo.imageType == ImageType::Tex3d) ? 1 : numSlices);
    ImageViewInfo     imageView = {};
    RpmUtil::BuildImageViewInfo(&imageView,
                                *dstImage.Parent(),
                                viewRange,
                                planeFormat,
                                RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                device.TexOptLevel(),
                                true);

    sq_img_rsrc_t srd = {};
    device.CreateImageViewSrds(1, &imageView, &srd);

    // We want to unset this bit because we are writing the real clear color to the first pixel of each DCC block,
    // So it doesn't need to be compressed. Currently this is the only place we unset this bit in GFX10.

    srd.compression_en = 0;

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, ConstCount, ArrayLen32(srd.u32All), &srd.u32All[0]);

    // How many blocks are there for this miplevel in X/Y/Z dimension.
    // We'll need one thread for each block, which writes clear value to the first byte.
    const DispatchDims blocks =
    {
        (mipLevelWidth  + xInc - 1) / xInc,
        (mipLevelHeight + yInc - 1) / yInc,
        (mipLevelDepth  + zInc - 1) / zInc
    };

    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(blocks, pPipeline->ThreadsPerGroupXyz()), {});
}

// =====================================================================================================================
// Performs a "fast" depth and stencil resummarize operation by updating the Image's HTile buffer to represent a fully
// open HiZ range and set ZMask and SMem to expanded state.
void RsrcProcMgr::HwlResummarizeHtileCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    image,
    const SubresRange& range
    ) const
{
    // Evaluate the mask and value for updating the HTile buffer.
    const Gfx9::Image& gfx9Image = static_cast<const Gfx9::Image&>(image);
    const Gfx9Htile*const pHtile = gfx9Image.GetHtile();
    PAL_ASSERT(pHtile != nullptr);

    const uint32 hTileValue = pHtile->GetInitialValue();
    uint32 hTileMask        = pHtile->GetPlaneMask(range);

    // If this hTile uses four-bit VRS encoding, SR1 has been repurposed to reflect VRS information.
    // If stencil is present, each HTILE is laid out as-follows, according to the DB spec:
        // |31       12|11 10|9    8|7   6|5   4|3     0|
        // +-----------+-----+------+-----+-----+-------+
        // |  Z Range  |     | SMem | SR1 | SR0 | ZMask |
    if (gfx9Image.HasVrsMetadata() &&
        (GetGfx9Settings(*(m_pDevice->Parent())).vrsHtileEncoding == VrsHtileEncoding::Gfx10VrsHtileEncodingFourBit))
    {
        hTileMask &= (~Gfx9Htile::Sr1Mask);
    }

    InitHtileData(pCmdBuffer, gfx9Image, range, hTileValue, hTileMask);
}

// =====================================================================================================================
void RsrcProcMgr::InitHtileData(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             hTileMask
    ) const
{
    const Pal::Image*  pPalImage       = dstImage.Parent();
    const auto*        pHtile          = dstImage.GetHtile();
    const auto&        hTileAddrOut    = pHtile->GetAddrOutput();
    const gpusize      hTileBaseAddr   = dstImage.GetMaskRamBaseAddr(pHtile, 0);
    const auto*        pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Determine which pipeline to use for this clear.  The "GetLinearHtileClearPipeline" will return nullptr if
    // the mask value is UINT_MAX (i.e., don't keep any existing values, just write hTileValue directly).  However,
    // the FastDepthClear pipeline will still work for this case.
    const Pal::ComputePipeline* pPipeline = ((hTileMask != UINT_MAX)
                                        ? GetLinearHtileClearPipeline(m_pDevice->Settings().dbPerTileExpClearEnable,
                                                                      pHtile->TileStencilDisabled(),
                                                                      hTileMask)
                                        : GetPipeline(RpmComputePipeline::FastDepthClear));

    PAL_ASSERT(pPipeline != nullptr);

    // Bind the pipeline.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // The shaders assume that the SRDs are eight dwords long (i.e,. worst case) as future-proofness.
    // Put the new HTile data in user data 8 and the old HTile data mask in user data 9.
    const uint32 hTileUserData[2] = { hTileValue & hTileMask, ~hTileMask };
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                               SrdDwordAlignment(),
                               NumBytesToNumDwords(sizeof(hTileUserData)),
                               hTileUserData);

    bool  wroteLastMipLevel = false;
    for (uint32 mipIdx = 0; ((mipIdx < range.numMips) && (wroteLastMipLevel == false)); mipIdx++)
    {
        const uint32  absMip       = mipIdx + range.startSubres.mipLevel;
        const auto&   hTileMipInfo = pHtile->GetAddrMipInfo(absMip);

        // A slice size of zero indicates that this subresource isn't compressible and that there's
        // nothing to do
        if (hTileMipInfo.sliceSize != 0)
        {
            for (uint32 sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
            {
                const  uint32 absSlice        = sliceIdx + range.startSubres.arraySlice;
                const gpusize hTileSubResAddr = hTileBaseAddr +
                                                hTileAddrOut.sliceSize * absSlice +
                                                hTileMipInfo.offset;

                BufferViewInfo hTileBufferView         = {};
                hTileBufferView.gpuAddr                = hTileSubResAddr;
                hTileBufferView.range                  = hTileMipInfo.sliceSize;
                hTileBufferView.stride                 = sizeof(uint32);
                hTileBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
                hTileBufferView.swizzledFormat.swizzle =
                    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
                hTileBufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                       RpmViewsBypassMallOnRead);
                hTileBufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                       RpmViewsBypassMallOnWrite);

                BufferSrd srd = { };
                m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &hTileBufferView, &srd);

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, DwordsPerBufferSrd, &srd.u32All[0]);

                // Issue a dispatch with one thread per HTile DWORD.
                const uint32 hTileDwords  = static_cast<uint32>(hTileBufferView.range / sizeof(uint32));
                const uint32 threadGroups = RpmUtil::MinThreadGroups(hTileDwords, pPipeline->ThreadsPerGroup());
                pCmdBuffer->CmdDispatch({threadGroups, 1, 1}, {});
            } // end loop through slices
        }
        else
        {
            // If this mip level isn't compressible, then no smaller mip levels will be either.
            wroteLastMipLevel = true;
        }
    } // end loop through mip levels

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
}

// =====================================================================================================================
void RsrcProcMgr::WriteHtileData(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             hTileMask,
    uint8              stencil,
    bool               trackBltActiveFlags
    ) const
{
    const Pal::Image*  pPalImage       = dstImage.Parent();
    const auto*        pHtile          = dstImage.GetHtile();
    const auto&        hTileAddrOut    = pHtile->GetAddrOutput();
    const gpusize      hTileBaseAddr   = dstImage.GetMaskRamBaseAddr(pHtile, range.startSubres.arraySlice);
    const auto*        pPublicSettings = m_pDevice->Parent()->GetPublicSettings();

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const bool expClearEnable      = m_pDevice->Settings().dbPerTileExpClearEnable;
    const bool tileStencilDisabled = pHtile->TileStencilDisabled();
    bool       wroteLastMipLevel   = false;

    const Pal::ComputePipeline* pPipeline = nullptr;

    for (uint32 mipIdx = 0; ((mipIdx < range.numMips) && (wroteLastMipLevel == false)); mipIdx++)
    {
        const uint32  absMip       = mipIdx + range.startSubres.mipLevel;
        const auto&   hTileMipInfo = pHtile->GetAddrMipInfo(absMip);

        // A slice size of zero indicates that this subresource isn't compressible and that there's
        // nothing to do
        if (hTileMipInfo.sliceSize != 0)
        {
            for (uint32 sliceIdx = 0; sliceIdx < range.numSlices; sliceIdx++)
            {
                const  uint32 absSlice        = sliceIdx + range.startSubres.arraySlice;
                const gpusize hTileSubResAddr = hTileBaseAddr +
                                                hTileAddrOut.sliceSize * absSlice +
                                                hTileMipInfo.offset;

                pPipeline                              = nullptr;
                BufferViewInfo hTileBufferView         = {};
                hTileBufferView.gpuAddr                = hTileSubResAddr;
                hTileBufferView.range                  = hTileMipInfo.sliceSize;
                hTileBufferView.stride                 = sizeof(uint32);
                hTileBufferView.swizzledFormat.format  = ChNumFormat::X32_Uint;
                hTileBufferView.swizzledFormat.swizzle =
                    { ChannelSwizzle::X, ChannelSwizzle::Zero, ChannelSwizzle::Zero, ChannelSwizzle::One };
                hTileBufferView.flags.bypassMallRead  = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                       RpmViewsBypassMallOnRead);
                hTileBufferView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                       RpmViewsBypassMallOnWrite);
                BufferSrd htileSurfSrd                 = { };
                uint32 hTileUserData[4]                = { };
                uint32 numConstDwords                  = 4;
                bool useHisPretests                    = false;

                // Number of bytes of all htiles within a sub resouce can be divided by 4.
                PAL_ASSERT((hTileMipInfo.sliceSize % sizeof(uint32) == 0));
                const uint32 hTileDwords = static_cast<uint32>(hTileMipInfo.sliceSize / sizeof(uint32));
                uint32 minThreads        = hTileDwords;

                if (expClearEnable)
                {
                    // If Exp/Clear is enabled, fast clears require using a special Exp/Clear shader.
                    // One such shader exists for depth/stencil Images and for depth-only Images.
                    if (tileStencilDisabled == false)
                    {
                        pPipeline = GetPipeline(RpmComputePipeline::FastDepthStExpClear);
                    }
                    else
                    {
                        pPipeline = GetPipeline(RpmComputePipeline::FastDepthExpClear);
                    }
                    hTileUserData[0] = hTileValue & hTileMask;
                    hTileUserData[1] = ~hTileMask;
                    numConstDwords = 2;
                    m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &hTileBufferView, &htileSurfSrd);
                }
                else
                {
                    // In two cases, we use FastDepthClear pipeline for fast clear.
                    // One case is that htile is of depth only format.
                    // The other is that htile is of depth stencil format, but client clear depth plane only.
                    if ((tileStencilDisabled == true) ||
                        ((pHtile->GetPlaneMask(HtilePlaneStencil) & hTileMask) == 0) ||
                        (dstImage.HasHiSPretestsMetaData() == false))
                    {
                        // If the htile is of pure depth format (i.e., no stencil fields), and hTileMask is 0,
                        // we'll also take this path. This will happen when the range is
                        // of stencil plane, but the the htile is of pure depth format.
                        pPipeline = GetPipeline(RpmComputePipeline::FastDepthClear);
                        hTileUserData[0] = hTileValue & hTileMask;
                        hTileUserData[1] = ~hTileMask;
                        numConstDwords   = 2;
                        m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &hTileBufferView, &htileSurfSrd);
                    }
                    else
                    {
                        // Clear both depth and stencil plane, or clear stencil plane only.
                        // In case of stencil only or D+S, we have to update SR0/1 fields based on given fast
                        // clear stencil value and HiS pretests meta data stored in the image.
                        if ((hTileDwords % 4) == 0)
                        {
                            pPipeline  = GetPipeline(RpmComputePipeline::HtileSR4xUpdate);
                            minThreads = minThreads / 4;
                        }
                        else
                        {
                            pPipeline = GetPipeline(RpmComputePipeline::HtileSRUpdate);
                        }
                        hTileBufferView.stride         = 1;
                        hTileBufferView.swizzledFormat = UndefinedSwizzledFormat;
                        m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &hTileBufferView, &htileSurfSrd);
                        hTileUserData[0] = hTileValue; // The htile value written to the htile surf.
                        hTileUserData[1] = hTileMask; // It determines which plane of htileValue will be used.
                        hTileUserData[2] = stencil; // Fast clear stencil value.
                        numConstDwords   = 4; // This shader expects four values and the last one is padding.
                        useHisPretests   = true;
                    }
                }

                pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                           0,
                                           DwordsPerBufferSrd,
                                           &htileSurfSrd.u32All[0]);

                // The fast-depth-clear shaders assume the SRD is eight dwords long as feature-proofness
                // for future GPUs.  The SRDs aren't really that long, but space the constant data out as if
                // it were.
                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                           (numConstDwords == 2) ? SrdDwordAlignment() : DwordsPerBufferSrd,
                                           numConstDwords,
                                           hTileUserData);

                // HiS metadata is only needed if we use HiStencil shaders to do fast DS clear.
                if (useHisPretests)
                {
                    BufferSrd metadataSrd = {};

                    // BufferView for the HiStencil meta data.
                    BufferViewInfo metadataView = {  };
                    // We may replace absMip with 0, as HiS meta data is each sub resouce is same.
                    metadataView.gpuAddr        = dstImage.HiSPretestsMetaDataAddr(absMip);
                    // HiStencil meta data size for one mip.
                    metadataView.range          = dstImage.HiSPretestsMetaDataSize(1);
                    metadataView.stride         = 1;
                    metadataView.swizzledFormat = UndefinedSwizzledFormat;
                    metadataView.flags.bypassMallRead = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                       RpmViewsBypassMallOnRead);
                    metadataView.flags.bypassMallWrite = TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall,
                                                                        RpmViewsBypassMallOnWrite);
                    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &metadataView, &metadataSrd);

                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                               DwordsPerBufferSrd + numConstDwords,
                                               DwordsPerBufferSrd,
                                               &metadataSrd.u32All[0]);
                }

                // Issue a dispatch with one thread per HTile DWORD or a dispatch every 4 Htile DWORD.
                const uint32 threadGroups = RpmUtil::MinThreadGroups(minThreads, pPipeline->ThreadsPerGroup());
                pCmdBuffer->CmdDispatch({threadGroups, 1, 1}, {});

            } // end loop through slices
        }
        else
        {
            // If this mip level isn't compressible, then no smaller mip levels will be either.
            wroteLastMipLevel = true;
        }
    } // end loop through mip levels

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData, trackBltActiveFlags);

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image range by updating the Image's HTile buffer.
void RsrcProcMgr::FastDepthStencilClearCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             clearMask,
    uint8              stencil,
    bool               trackBltActiveFlags
    ) const
{
    const auto* pHtile = dstImage.GetHtile();
    uint32   hTileMask = pHtile->GetPlaneMask(clearMask);

    // If this hTile uses four-bit VRS encoding, SR1 has been repurposed to reflect VRS information.
    // If stencil is present, each HTILE is laid out as-follows, according to the DB spec:
        // |31       12|11 10|9    8|7   6|5   4|3     0|
        // +-----------+-----+------+-----+-----+-------+
        // |  Z Range  |     | SMem | SR1 | SR0 | ZMask |
    if (dstImage.HasVrsMetadata() &&
        (GetGfx9Settings(*(m_pDevice->Parent())).vrsHtileEncoding == VrsHtileEncoding::Gfx10VrsHtileEncodingFourBit))
    {
        hTileMask &= (~Gfx9Htile::Sr1Mask);
    }

    WriteHtileData(pCmdBuffer, dstImage, range, hTileValue, hTileMask, stencil, trackBltActiveFlags);

    FastDepthStencilClearComputeCommon(pCmdBuffer, dstImage.Parent(), clearMask);
}

// =====================================================================================================================
const Pal::ComputePipeline* RsrcProcMgr::GetCmdGenerationPipeline(
    const Pal::IndirectCmdGenerator& generator,
    const GfxCmdBuffer&              cmdBuffer
    ) const
{
    RpmComputePipeline pipeline   = RpmComputePipeline::Count;
    const EngineType   engineType = cmdBuffer.GetEngineType();

    switch (generator.Type())
    {
    case GeneratorType::Draw:
    case GeneratorType::DrawIndexed:
        // We use a compute pipeline to generate PM4 for executing graphics draws...  This command buffer needs
        // to be able to support both operations.  This will be a problem for GFX10-graphics only rings.
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType) &&
                   Pal::Device::EngineSupportsCompute(engineType));

        pipeline = RpmComputePipeline::Gfx10GenerateCmdDraw;
        break;

    case GeneratorType::Dispatch:
        PAL_ASSERT(Pal::Device::EngineSupportsCompute(engineType));

        pipeline = RpmComputePipeline::Gfx10GenerateCmdDispatch;
        break;
    case GeneratorType::DispatchMesh:
        PAL_ASSERT(Pal::Device::EngineSupportsGraphics(engineType) &&
                   Pal::Device::EngineSupportsCompute(engineType));

        pipeline = (IsGfx11(generator.Properties().gfxLevel)) ? RpmComputePipeline::Gfx11GenerateCmdDispatchTaskMesh
                                                              : RpmComputePipeline::Gfx10GenerateCmdDispatchTaskMesh;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return GetPipeline(pipeline);
}

// =====================================================================================================================
// Check if for all the regions, the format and swizzle mode are compatible for src and dst image.
// If all regions are compatible, we can do a fixed function resolve. Otherwise return false.
bool RsrcProcMgr::HwlCanDoDepthStencilCopyResolve(
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions
    ) const
{

    const Gfx9PalSettings& settings      = GetGfx9Settings(*m_pDevice->Parent());
    const ImageCreateInfo& srcCreateInfo = srcImage.GetImageCreateInfo();
    const ImageCreateInfo& dstCreateInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT(srcCreateInfo.imageType == dstCreateInfo.imageType);
    PAL_ASSERT(srcCreateInfo.imageType != ImageType::Tex3d);

    const Image* const  pGfxSrcImage = static_cast<const Image*>(srcImage.GetGfxImage());
    const Image* const  pGfxDstImage = static_cast<const Image*>(dstImage.GetGfxImage());

    AutoBuffer<const ImageResolveRegion*, 2 * MaxImageMipLevels, Platform>
        fixUpRegionList(regionCount, m_pDevice->GetPlatform());

    bool canDoDepthStencilCopyResolve =
        settings.allowDepthCopyResolve &&
        (pGfxSrcImage->HasDsMetadata() || pGfxDstImage->HasDsMetadata());

    if (fixUpRegionList.Capacity() >= regionCount)
    {
        uint32 mergedCount = 0;

        const auto* const pAddrMgr = static_cast<const AddrMgr2::AddrMgr2*>(m_pDevice->Parent()->GetAddrMgr());

        for (uint32 region = 0; canDoDepthStencilCopyResolve && (region < regionCount); ++region)
        {
            const ImageResolveRegion& imageRegion = pRegions[region];
            const SubresId srcSubResId = Subres(imageRegion.srcPlane, 0, imageRegion.srcSlice);
            const SubresId dstSubResId = Subres(imageRegion.dstPlane, imageRegion.dstMipLevel, imageRegion.dstSlice);

            PAL_ASSERT(imageRegion.srcPlane == imageRegion.dstPlane);

            const auto*   pSrcSubResInfo = srcImage.SubresourceInfo(srcSubResId);
            const auto&   srcAddrSettings = pGfxSrcImage->GetAddrSettings(pSrcSubResInfo);

            const auto*   pDstSubResInfo = dstImage.SubresourceInfo(dstSubResId);
            const auto&   dstAddrSettings = pGfxDstImage->GetAddrSettings(pDstSubResInfo);

            canDoDepthStencilCopyResolve &=
                ((memcmp(&pSrcSubResInfo->format, &pDstSubResInfo->format, sizeof(SwizzledFormat)) == 0) &&
                    (pAddrMgr->GetBlockSize(srcAddrSettings.swizzleMode) ==
                        pAddrMgr->GetBlockSize(dstAddrSettings.swizzleMode)) &&
                  AddrMgr2::IsZSwizzle(srcAddrSettings.swizzleMode) &&
                  AddrMgr2::IsZSwizzle(dstAddrSettings.swizzleMode));

            static const uint32 HtileTexelAlignment = 8;

            // Htile copy and fixup will be performed simultaneously for depth and stencil part in depth-stencil copy
            // resolve. Each mip level/dstSlice is only allowed to be appeared once for each plane, while resolve
            // offset and and resolve extent shall be exactly same. Otherwise, we don't track more and just let it
            // switch pixel-shader resolve path.
            bool inserted = false;
            for (uint32 other = 0; other < mergedCount; ++other)
            {
                const ImageResolveRegion& otherRegion = *fixUpRegionList[other];
                if ((imageRegion.dstMipLevel == otherRegion.dstMipLevel) &&
                    (imageRegion.dstSlice == otherRegion.dstSlice))
                {
                    canDoDepthStencilCopyResolve &= ((otherRegion.srcOffset.x == imageRegion.srcOffset.x) &&
                                                     (otherRegion.srcOffset.y == imageRegion.srcOffset.y) &&
                                                     (otherRegion.dstOffset.x == imageRegion.dstOffset.x) &&
                                                     (otherRegion.dstOffset.y == imageRegion.dstOffset.y) &&
                                                     (otherRegion.extent.width == imageRegion.extent.width) &&
                                                     (otherRegion.extent.height == imageRegion.extent.height) &&
                                                     (otherRegion.numSlices == imageRegion.numSlices) &&
                                                     (otherRegion.srcSlice == imageRegion.srcSlice));
                    inserted = true;
                    break;
                }
            }

            if (inserted == false)
            {
                fixUpRegionList[mergedCount++] = &imageRegion;

                // srcOffset and dstOffset have to match for a depth/stencil copy
                canDoDepthStencilCopyResolve &= ((imageRegion.srcOffset.x == imageRegion.dstOffset.x) &&
                                                 (imageRegion.srcOffset.y == imageRegion.dstOffset.y));

                PAL_ASSERT((imageRegion.dstOffset.x >= 0) && (imageRegion.dstOffset.y >= 0));

                canDoDepthStencilCopyResolve &=
                    (IsPow2Aligned(imageRegion.dstOffset.x, HtileTexelAlignment)                                     &&
                     IsPow2Aligned(imageRegion.dstOffset.y, HtileTexelAlignment)                                     &&
                     (IsPow2Aligned(imageRegion.extent.width, HtileTexelAlignment) ||
                      ((imageRegion.extent.width + imageRegion.dstOffset.x) == pDstSubResInfo->extentTexels.width))  &&
                     (IsPow2Aligned(imageRegion.extent.height, HtileTexelAlignment) ||
                      ((imageRegion.extent.height + imageRegion.dstOffset.y) == pDstSubResInfo->extentTexels.height)));
            }
        }

        if (canDoDepthStencilCopyResolve)
        {
            // Check if there's any array slice overlap. If there's array slice overlap,
            // switch to pixel-shader resolve.
            for (uint32 index = 0; index < mergedCount; ++index)
            {
                for (uint32 other = (index + 1); other < mergedCount; ++other)
                {
                    if ((fixUpRegionList[index]->dstMipLevel == fixUpRegionList[other]->dstMipLevel) &&
                        (fixUpRegionList[index]->dstSlice <
                            (fixUpRegionList[other]->dstSlice + fixUpRegionList[other]->numSlices)) &&
                        (fixUpRegionList[other]->dstSlice <
                            (fixUpRegionList[index]->dstSlice + fixUpRegionList[index]->numSlices)))
                    {
                        canDoDepthStencilCopyResolve = false;
                        break;
                    }
                }
            }
        }
    }
    else
    {
        canDoDepthStencilCopyResolve = false;
    }

    return canDoDepthStencilCopyResolve;
}

// =====================================================================================================================
// After a fixed-func depth/stencil copy resolve, src htile will be copied to dst htile and set the zmask or smask to
// expanded. Depth part and stencil part share same htile. So the depth part and stencil part will be merged (if
// necessary) and one cs blt will be launched for each merged region to copy and fixup the htile.
void RsrcProcMgr::HwlHtileCopyAndFixUp(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    bool                      computeResolve) const
{
    HwlFixupResolveDstImage(pCmdBuffer,
                            *dstImage.GetGfxImage(),
                            dstImageLayout,
                            pRegions,
                            regionCount,
                            computeResolve);
}

// =====================================================================================================================
// Initializes the requested range of cMask memory for the specified image.
void RsrcProcMgr::InitCmask(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       image,
    const SubresRange& range,
    const uint8        initValue,
    bool               trackBltActiveFlags
    ) const
{
    PAL_ASSERT(range.numPlanes == 1);

    const Pal::Image*const                 pParentImg   = image.Parent();
    const ImageCreateInfo&                 createInfo   = pParentImg->GetImageCreateInfo();
    const Gfx9Cmask*const                  pCmask       = image.GetCmask();
    const ADDR2_COMPUTE_CMASK_INFO_OUTPUT& cMaskAddrOut = pCmask->GetAddrOutput();

    // MSAA images can't have mipmaps
    PAL_ASSERT(createInfo.mipLevels == 1);

    const uint32 startSlice = (createInfo.imageType == ImageType::Tex3d) ? 0 : range.startSubres.arraySlice;
    const uint32 numSlices  = GetClearDepth(image, range.startSubres.plane, range.numSlices, 0);

    const gpusize cMaskBaseAddr = pParentImg->GetBoundGpuMemory().GpuVirtAddr() + pCmask->MemoryOffset();
    const gpusize sliceAddr     = cMaskBaseAddr + startSlice * cMaskAddrOut.sliceSize;

    CmdFillMemory(pCmdBuffer,
                  true,
                  trackBltActiveFlags,
                  sliceAddr,
                  numSlices * cMaskAddrOut.sliceSize,
                  ReplicateByteAcrossDword(initValue));

    pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(image.HasMisalignedMetadata());
}

// =====================================================================================================================
// Use the compute engine to initialize hTile memory that corresponds to the specified clearRange
void RsrcProcMgr::InitHtile(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange
    ) const
{
    const Pal::Image*      pParentImg = dstImage.Parent();
    const ImageCreateInfo& createInfo = pParentImg->GetImageCreateInfo();
    const auto*            pHtile     = dstImage.GetHtile();
    const uint32           clearMask  = GetInitHtileClearMask(dstImage, clearRange);

    // There shouldn't be any 3D images with HTile allocations.
    PAL_ASSERT(createInfo.imageType != ImageType::Tex3d);

    if (clearMask != 0)
    {
        const uint32  initValue = pHtile->GetInitialValue();
        const uint32  hTileMask = pHtile->GetPlaneMask(clearMask);

        InitHtileData(pCmdBuffer, dstImage, clearRange, initValue, hTileMask);
        FastDepthStencilClearComputeCommon(pCmdBuffer, pParentImg, clearMask);
    }
}

// =====================================================================================================================
// This function fixes up Dcc/Cmask/Fmask metadata state: either copy from src image or fix up to uncompressed state.
// - For Fmask optimized MSAA copy where we we preserve fmask fragmentation, copy Cmask/Fmask from source image to dst.
// - For image is created with fullCopyDstOnly=1, fix up Cmask/Fmask to uncompressed state.
void RsrcProcMgr::HwlFixupCopyDstImageMetadata(
    GfxCmdBuffer*           pCmdBuffer,
    const Pal::Image*       pSrcImage, // Should be nullptr if isFmaskCopyOptimized = false
    const Pal::Image&       dstImage,
    ImageLayout             dstImageLayout,
    const ImageFixupRegion* pRegions,
    uint32                  regionCount,
    bool                    isFmaskCopyOptimized
    ) const
{
    PAL_ASSERT((pSrcImage == nullptr) || isFmaskCopyOptimized);

    // On GFX9, the HW will not update the DCC memory on a shader-write.  GFX10 changes the rules.  There are a couple
    // possibilities:
    //   1) If the dst image was marked as shader-writeable, then the HW compressed the copied image data as the shader
    //      wrote it...  in this case, do *not* fix up the DCC memory or corruption will result!
    //   2) If the dst image was marked as shader-readable (but not writeable), then the HW wrote 0xFF (the DCC
    //      "decompressed" code) into the DCC memory as the image data was being copied, so there's no need to do it
    //      again here.
    //   3) If the dst image is not meta-fetchable at all and with fullColorMsaaCopyDstOnly=0, then it should have been
    //      decompressed on the transition to"LayoutCopyDst", at which point there's no need to fix up DCC.
    //   4) If the dst image is not meta-fetchable at all and with fullColorMsaaCopyDstOnly=1, then it will not be
    //      expanded on the transition to"LayoutCopyDst", at which point there's need to fix up DCC.
    const auto&  gfx9DstImage = static_cast<const Gfx9::Image&>(*dstImage.GetGfxImage());

    if (gfx9DstImage.HasDccData() && (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0))
    {
        Pal::CmdStream* const pStream = pCmdBuffer->GetMainCmdStream();

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            const auto& clearRegion = pRegions[idx];
            const auto* pSubResInfo = dstImage.SubresourceInfo(clearRegion.subres);

            if (pSubResInfo->flags.supportMetaDataTexFetch == 0)
            {
                SubresRange range = {};

                range.startSubres.plane      = clearRegion.subres.plane;
                range.startSubres.mipLevel   = clearRegion.subres.mipLevel;
                range.startSubres.arraySlice = clearRegion.subres.arraySlice;
                range.numPlanes              = 1;
                range.numMips                = 1;
                range.numSlices              = clearRegion.numSlices;

                // Since color data is no longer dcc compressed set Dcc to fully uncompressed.
                ClearDcc(pCmdBuffer,
                         pStream,
                         gfx9DstImage,
                         range,
                         Gfx9Dcc::DecompressedValue,
                         DccClearPurpose::FastClear,
                         true);
            }
        }
    }

    if (gfx9DstImage.HasFmaskData())
    {
        if (isFmaskCopyOptimized)
        {
            PAL_ASSERT(pSrcImage != nullptr);

            // On fmask msaa copy through compute shader we do an optimization where we preserve fmask fragmentation
            // while copying the data from src to dest, which means dst needs to have fmask of src.  Note that updates
            // to this function need to be reflected in HwlUseFMaskOptimizedImageCopy as well.

            // Copy the src fmask and cmask data to destination
            const auto&       gfx9SrcImage = static_cast<const Gfx9::Image&>(*pSrcImage->GetGfxImage());
            const auto*       pSrcFmask    = gfx9SrcImage.GetFmask();
            const auto&       srcBoundMem  = pSrcImage->GetBoundGpuMemory();

            const auto*       pDstFmask    = gfx9DstImage.GetFmask();
            const auto&       dstBoundMem  = dstImage.GetBoundGpuMemory();

            // Our calculation of "srcCopySize" below assumes that fmask memory comes before the cmask memory in
            // our orginzation of the image data.
            PAL_ASSERT(pSrcFmask->MemoryOffset() < gfx9SrcImage.GetCmask()->MemoryOffset());
            PAL_ASSERT(pDstFmask->MemoryOffset() < gfx9DstImage.GetCmask()->MemoryOffset());

            // dstImgMemLayout metadata size comparison to srcImgMemLayout is checked by caller.
            const ImageMemoryLayout& srcImgMemLayout = pSrcImage->GetMemoryLayout();

            // First copy header by PFP
            // We always read and write the metadata header using the PFP so the copy must also use the PFP.
            PfpCopyMetadataHeader(pCmdBuffer,
                                  dstBoundMem.GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset,
                                  srcBoundMem.GpuVirtAddr() + srcImgMemLayout.metadataHeaderOffset,
                                  static_cast<uint32>(srcImgMemLayout.metadataHeaderSize),
                                  gfx9SrcImage.HasDccLookupTable());

            // Do the rest copy
            const gpusize srcCopySize =
                (srcImgMemLayout.metadataSize - (pSrcFmask->MemoryOffset() - srcImgMemLayout.metadataOffset));

            MemoryCopyRegion memcpyRegion = {};
            memcpyRegion.srcOffset = srcBoundMem.Offset() + pSrcFmask->MemoryOffset();
            memcpyRegion.dstOffset = dstBoundMem.Offset() + pDstFmask->MemoryOffset();
            memcpyRegion.copySize  = srcCopySize;

            CopyMemoryCs(pCmdBuffer,
                         *srcBoundMem.Memory(),
                         *dstBoundMem.Memory(),
                         1,
                         &memcpyRegion);

            pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(dstImage.HasMisalignedMetadata());
        }
        else
        {
            Pal::CmdStream* const pStream = pCmdBuffer->GetMainCmdStream();

            // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst";
            // if the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
            for (uint32 idx = 0; idx < regionCount; ++idx)
            {
                const auto& clearRegion = pRegions[idx];
                const auto* pSubResInfo = dstImage.SubresourceInfo(clearRegion.subres);

                SubresRange range = {};

                range.startSubres.plane      = clearRegion.subres.plane;
                range.startSubres.mipLevel   = clearRegion.subres.mipLevel;
                range.startSubres.arraySlice = clearRegion.subres.arraySlice;
                range.numPlanes              = 1;
                range.numMips                = 1;
                range.numSlices              = clearRegion.numSlices;

                // Since color data is no longer compressed set CMask and FMask to fully uncompressed.
                InitCmask(pCmdBuffer, pStream, gfx9DstImage, range, gfx9DstImage.GetCmask()->GetInitialValue(), true);

                pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
                ClearFmask(pCmdBuffer, gfx9DstImage, range, Gfx9Fmask::GetPackedExpandedValue(gfx9DstImage));
                pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
            }
        }
    }
    else if (dstImage.IsDepthStencilTarget() && gfx9DstImage.HasHtileData())
    {
        // Depth compute copies can get here (see CmdCopyMemoryToImage).  So long as all of the
        // subresources being copied are meta-fetchable, the SRD will have kept hTile in sync
        // with the image data.  If not, then we have a problem.
        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            const ImageFixupRegion* pRegion     = &pRegions[idx];
            const SubResourceInfo*  pSubResInfo = dstImage.SubresourceInfo(pRegion->subres);

            PAL_ASSERT(pSubResInfo->flags.supportMetaDataTexFetch != 0);
        }
    }
}

// =====================================================================================================================
// For copies to non-local destinations, it is faster (although very unintuitive) to disable all but one of the RBs.
// All of the RBs banging away on the PCIE bus produces more traffic than the write-combiner can efficiently handle,
// so if we detect a write to non-local memory here, then disable RBs for the duration of the copy.  They will get
// restored in the HwlEndGraphicsCopy function.
uint32 RsrcProcMgr::HwlBeginGraphicsCopy(
    GfxCmdBuffer*                pCmdBuffer,
    const Pal::GraphicsPipeline* pPipeline,
    const Pal::Image&            dstImage,
    uint32                       bpp
    ) const
{
    Pal::CmdStream*const pCmdStream   = pCmdBuffer->GetMainCmdStream();
    const GpuMemory*     pGpuMem      = dstImage.GetBoundGpuMemory().Memory();
    const auto&          palDevice    = *(m_pDevice->Parent());
    const auto&          coreSettings = palDevice.Settings();
    uint32               modifiedMask = 0;

    // Virtual memory objects don't have a defined heap preference, so skip this optimization for virtual memory.
    if ((pGpuMem != nullptr) && (pGpuMem->IsVirtual() == false))
    {
        const GpuHeap preferredHeap = pGpuMem->PreferredHeap();
        const bool    isNonLocal    = ((preferredHeap == GpuHeapGartUswc) ||
                                       (preferredHeap == GpuHeapGartCacheable)) ||
                                      pGpuMem->IsPeer();

        if (isNonLocal)
        {
            if (IsGfx101(palDevice) && (coreSettings.nonlocalDestGraphicsCopyRbs >= 0))
            {
                regPA_SC_TILE_STEERING_OVERRIDE  defaultPaRegVal = {};
                const auto&                      chipProps = m_pDevice->Parent()->ChipProperties().gfx9;

                defaultPaRegVal.u32All  = chipProps.paScTileSteeringOverride;
                const uint32 maxRbPerSc = (1 << defaultPaRegVal.bits.NUM_RB_PER_SC);

                // A setting of zero RBs implies that the driver should use the optimal number.  For now, assume the
                // optimal number is one.  Also don't allow more RBs than actively exist.
                const uint32 numNeededTotalRbs =
                    Min(Max(1u, static_cast<uint32>(coreSettings.nonlocalDestGraphicsCopyRbs)),
                        chipProps.numActiveRbs);

                // We now have the total number of RBs that we need...  However, the ASIC divides RBs up between the
                // various SEs, so calculate how many SEs we need to involve and how many RBs each SE should use.
                const uint32  numNeededScs      = Max(1u, (numNeededTotalRbs / maxRbPerSc));
                const uint32  numNeededRbsPerSc = numNeededTotalRbs / numNeededScs;

                //   - SC typically supports the following:
                //       - Non-RB+ chip
                //           -- 1-2 (base 10) packers
                //           -- Each packer has 2 (base 10) RBs
                //       - RB+ chip
                //           -- 1-2 (base 10) packers
                //           -- Each packer has 1 (base 10) RB
                //           -- Each packer again has 1 (base 10) RB
                //
                //   - For a non-RB+ chip, we can support 1 RB per packer.  A non-RB+ chip, always has
                //     2 RBs per packer.   RB+, is restricted to 1 RB per packer

                // Write the new register value to the command stream
                regPA_SC_TILE_STEERING_OVERRIDE  paScTileSteeringOverride = {};

                // LOG2 of the effective number of scan-converters desired. Must not be programmed to greater than the
                // number of active SCs present in the chip
                paScTileSteeringOverride.bits.NUM_SC        = Log2(numNeededScs);

                // LOG2 of the effective NUM_RB_PER_SC desired. Must not be programmed to greater than the number of
                // active RBs per SC present in the chip.
                paScTileSteeringOverride.bits.NUM_RB_PER_SC = Log2(numNeededRbsPerSc);

                // LOG2 of the effective NUM_PACKER_PER_SC desired. This is strictly for test purposes, otherwise
                // noramlly would be set to match the number of physical packers active in the design configuration.
                // Must not be programmed to greater than the number of active packers per SA (SC) present in the chip
                // configuration. Must be 0x1 if NUM_RB_PER_SC = 0x2.
                paScTileSteeringOverride.gfx101.NUM_PACKER_PER_SC =
                    Min(paScTileSteeringOverride.bits.NUM_RB_PER_SC, defaultPaRegVal.gfx101.NUM_PACKER_PER_SC);

                CommitBeginEndGfxCopy(pCmdStream, paScTileSteeringOverride.u32All);

                // Let EndGraphcisCopy know that it has work to do
                modifiedMask |= PaScTileSteeringOverrideMask;
            }
            else if (IsGfx101(palDevice) == false)
            {
                PAL_ALERT_ALWAYS_MSG("Non-local copies should prefer compute");
            }
        }
    }

    // CreateCopyStates does not specify CompoundStateCrateInfo.pTriangleRasterParams and it is set here.
    const TriangleRasterStateParams triangleRasterState
    {
        FillMode::Solid,        // frontface fillMode
        FillMode::Solid,        // backface fillMode
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
// Undoes whatever HwlBeginGraphicsCopy did.
void RsrcProcMgr::HwlEndGraphicsCopy(
    GfxCmdStream* pCmdStream,
    uint32        restoreMask
    ) const
{
    // Did HwlBeginGraphicsCopy do anything? If not, there's nothing to do here.
    if (TestAnyFlagSet(restoreMask, PaScTileSteeringOverrideMask))
    {
        CommitBeginEndGfxCopy(pCmdStream, m_pDevice->Parent()->ChipProperties().gfx9.paScTileSteeringOverride);
    }
}

// =====================================================================================================================
ImageCopyEngine RsrcProcMgr::GetImageToImageCopyEngine(
    const GfxCmdBuffer*    pCmdBuffer,
    const Pal::Image&      srcImage,
    const Pal::Image&      dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 copyFlags
    ) const
{
    // Get the default engine type for the copy here.
    ImageCopyEngine copyEngine = PreferComputeForNonLocalDestCopy(dstImage) ? ImageCopyEngine::Compute :
        Pal::RsrcProcMgr::GetImageToImageCopyEngine(pCmdBuffer, srcImage, dstImage, regionCount, pRegions, copyFlags);

    // Profiling shows that gfx11's draw-based copy performance craters when you go from 4xAA to 8xAA on either of
    // the two-plane depth+stencil formats. The single-plane depth-only formats seem unaffected.
    //
    // We don't have a proper root-cause for this but we suspect that running a per-sample PS with 8xAA fills up the
    // OREO scoreboard. Waiting for the scoreboard to drain becomes a serious bottleneck making the copy DB-bound.
    // We'll run much, much faster if we force these cases back to the compute path.
    if ((copyEngine == ImageCopyEngine::Graphics)    &&
        IsGfx11(*m_pDevice->Parent())                &&
        dstImage.IsDepthStencilTarget()              &&
        (dstImage.GetImageCreateInfo().samples == 8) &&
        (dstImage.GetImageInfo().numPlanes == 2))
    {
        copyEngine = ImageCopyEngine::Compute;
    }

    // If the copy will use the graphics engine anyway then there's no need to go through this as graphics
    // copies won't corrupt the VRS encoding.
    if ((copyEngine != ImageCopyEngine::Graphics) && CopyDstBoundStencilNeedsWa(pCmdBuffer, dstImage))
    {
        const auto& settings = GetGfx9Settings(*(m_pDevice->Parent()));

        bool  stencilPlaneFound = false;
        for (uint32  regionIdx = 0; ((stencilPlaneFound == false) && (regionIdx < regionCount)); regionIdx++)
        {
            const auto&  region = pRegions[regionIdx];

            // Is this region affecting the stencil plane of the destination image?
            if (dstImage.IsStencilPlane(region.dstSubres.plane))
            {
                // Ok, this copy will write into stencil data that has associated hTile data that in turn has
                // associated VRS data.  (Follow all that?)
                switch (settings.waVrsStencilUav)
                {
                case WaVrsStencilUav::GraphicsCopies:
                    // Use the graphics engine to do the copy which won't corrupt the VRS data.
                    copyEngine = ImageCopyEngine::Graphics;
                    break;

                case WaVrsStencilUav::ReCopyVrsData:
                    // Let the copy corrupt VRS.  It is the callers responsibility to mark the command buffer
                    // as having a dirty VRS source image.
                    copyEngine = ImageCopyEngine::ComputeVrsDirty;
                    break;

                default:
                    PAL_ASSERT_ALWAYS();
                    break;
                }

                // And break out of our loop.
                stencilPlaneFound = true;
            } // end check for stencil plane
        } // end loop through copy regions
    }

    return copyEngine;
}

// =====================================================================================================================
bool RsrcProcMgr::ScaledCopyImageUseGraphics(
    GfxCmdBuffer*         pCmdBuffer,
    const ScaledCopyInfo& copyInfo
    ) const
{
    bool useGraphicsCopy = Pal::RsrcProcMgr::ScaledCopyImageUseGraphics(pCmdBuffer, copyInfo);

    // Profiling shows that gfx11's draw-based copy performance craters when you go from 4xAA to 8xAA on either of
    // the two-plane depth+stencil formats. The single-plane depth-only formats seem unaffected.
    // ScaledCopyImageUseGraphics should have a Gfx9::RsrcProcMgr overload that optimizes the case where OREO runs slow
    // just like change in Gfx9::RsrcProcMgr::GetImageToImageCopyEngine.
    const auto pDstImage = static_cast<const Pal::Image*>(copyInfo.pDstImage);

    if (useGraphicsCopy                                         &&
        IsGfx11(*m_pDevice->Parent())                           &&
        pDstImage->IsDepthStencilTarget()                       &&
        (copyInfo.pSrcImage->GetImageCreateInfo().samples == 8) &&
        (pDstImage->GetImageCreateInfo().samples == 8)          &&
        (pDstImage->GetImageInfo().numPlanes == 2))
    {
        useGraphicsCopy = false;
    }

    return  useGraphicsCopy;
}

// =====================================================================================================================
// Use compute for all non-local copies on gfx10.2+ because graphics copies that use a single RB (see
// HwlBeginGraphicsCopy) are no longer preferable for PCIE transfers.
bool RsrcProcMgr::PreferComputeForNonLocalDestCopy(
    const Pal::Image& dstImage
    ) const
{
    const auto&  createInfo = dstImage.GetImageCreateInfo();

    bool preferCompute = false;

    const bool isMgpu = (m_pDevice->GetPlatform()->GetDeviceCount() > 1);

    if (IsGfx103Plus(*m_pDevice->Parent())                                        &&
        m_pDevice->Settings().gfx102NonLocalDestPreferCompute                     &&
        ((dstImage.IsDepthStencilTarget() == false) || (createInfo.samples == 1)) &&
        (isMgpu == false))
    {
        const GpuMemory* pGpuMem       = dstImage.GetBoundGpuMemory().Memory();

        // Virtual memory objects don't have a defined heap preference, so skip this optimization for virtual memory.
        if (pGpuMem->IsVirtual() == false)
        {
            const GpuHeap    preferredHeap = pGpuMem->PreferredHeap();

            if (((preferredHeap == GpuHeapGartUswc) || (preferredHeap == GpuHeapGartCacheable)) ||
                pGpuMem->IsPeer())
            {
                preferCompute = true;
            }
        }
    }

    return preferCompute;
}

// =====================================================================================================================
void RsrcProcMgr::LaunchOptimizedVrsCopyShader(
    GfxCmdBuffer*           pCmdBuffer,
    const DepthStencilView* pDsView,
    bool                    isClientDsv,
    const Extent3d&         depthExtent,
    const Pal::Image*       pSrcVrsImg,
    const Gfx9Htile*const   pHtile
    ) const
{
    const Pal::Device*const pPalDevice = m_pDevice->Parent();
    const Gfx9MetaEqGenerator* pEqGenerator = pHtile->GetMetaEqGenerator();
    const auto& createInfo = pSrcVrsImg->GetImageCreateInfo();

    // The shader we're about to execute makes these assumptions in its source. If these trip we can add more support.
    PAL_ASSERT(pHtile->PipeAligned() != 0);

    // Step 1.5: Pack the shader's user-data.
    // This shader has a carefully packed user-data layout that keeps everything in fast user-data entires.
    // We only have space for just two constant user-data!
    constexpr uint32 NumUserData = 2;

    // Constant buffer bit packing magic.
    //
    // The following constants are from GB_ADDR_CONFIG so their sizes are the same as their register fields:
    //       pipeInterleaveLog2, packersLog2 pipesLog2.
    //
    // The remaining constants each have a special story for their bit counts:
    // - capPipesLog2, metaBlkWidthLog2 and metaBlkHeightLog2: 5 bits is enough to store log2(UINT_MAX) so it's enough
    //       space for these. We could reduce this further but it's hard to find an upper-bound for these values.
    // - bankXor: The full pipeBankXor is mostly zeros on gfx10. The pipe and column bits are always zero, only the
    //       first four bank bits are ever set by addrlib. We will reconstruct the full pipeBankXor from them.
    // - pitchInMetaBlks: The HTile pitch is the width of the base resource's mip0 aligned to the meta block width.
    //       The largest mip0 width is MaxImageWidth (16K) and the smallest meta block width is 256 (found by reading
    //       addrlib, this occurs when we have a single pipe). Thus the largest possible pitch in units of meta block
    //       widths is 64, which fits in seven bits.
    // - lastHtileX and lastHtileY: The largest possible HTile x/y indices that we're writing to. These can be no
    //       larger than MaxImageWidth/Height (16K) divided by the HTile granularity (8 pixels wide/tall) minus one
    //       which is 2047. These can then fit in 11 bits. You can also think of this as the copy size minus one.
    // - sliceBits: Rather than use the whole slice index we only need the bits that are XORed into the HTile meta eqs.
    //       VRS should only be supported on RB+ ASICs and they only XOR the first two z bits. Rather than be exact
    //       and leave the last few bits unused we'll just roll them into this field.
    union
    {
        struct
        {
            // Constant #1
            uint32 pipeInterleaveLog2 : 3; // These are biased by 8, so 0 means log2(256) = 8.
            uint32 packersLog2        : 3;
            uint32 pipesLog2          : 3;
            uint32 capPipeLog2        : 5;
            uint32 metaBlkWidthLog2   : 5;
            uint32 metaBlkHeightLog2  : 5;
            uint32 pitchInMetaBlks    : 7;
            uint32 fourBitVrs         : 1; // A bool which tells the shader to use the four bit or two bit encodings.

            // Constant #2
            uint32 bankXor            : 4;
            uint32 lastHtileX         : 11;
            uint32 lastHtileY         : 11;
            uint32 sliceBits          : 6;
        };
        uint32 u32All[NumUserData]; // We will write the user data as an array of uint32s.
    } userData = {};

    static_assert(sizeof(userData) == sizeof(uint32) * NumUserData, "CopyVrsIntoHtile user data packing issue.");

    const ADDR2_COMPUTE_HTILE_INFO_OUTPUT& hTileAddrOutput = pHtile->GetAddrOutput();
    const ADDR2_META_MIP_INFO&             hTileMipInfo    = pHtile->GetAddrMipInfo(pDsView->MipLevel());
    const regGB_ADDR_CONFIG                gbAddrConfig    = m_pDevice->GetGbAddrConfig();

    Gfx9MaskRamBlockSize metaBlkExtentLog2 = {};
    const uint32         metaBlockSizeLog2 = pHtile->GetMetaBlockSize(&metaBlkExtentLog2);
    const uint32         pipeBankXor       = pHtile->GetPipeBankXor(0);

#if PAL_ENABLE_PRINTS_ASSERTS
    // Verify that we can actually store the pitch in terms of meta blocks.
    PAL_ASSERT(IsPow2Aligned(hTileAddrOutput.pitch, 1ull << metaBlkExtentLog2.width));

    // The shader will compute the meta block size from the extents. There's a conversion to do here because the size
    // is total bytes and the extents are in depth texels. We must multiply the size by 64 (1 << 6) and divide by 4
    // (1 >> 2) to convert to the texel area. That's the same thing as adding four in log2 math.
    constexpr uint32 HtileTexelsPerByteLog2 = 4;
    PAL_ASSERT(metaBlockSizeLog2 + HtileTexelsPerByteLog2 == metaBlkExtentLog2.width + metaBlkExtentLog2.height);

    // As stated above, we're only passing in the first few slice bits because we don't have enough space. This should
    // be fine because VRS should only be supported on GPUs with RB+ support which only uses a couple of slice bits
    // in HTile addressing. This complex assert verifies this assumption.
    constexpr uint32 SliceBitsMustBeZero = ~((1u << 6) - 1);
    for (uint32 eqBitPos = 0; eqBitPos < pEqGenerator->GetMetaEquation().GetNumValidBits(); eqBitPos++)
    {
        const uint32 eqData = pEqGenerator->GetMetaEquation().Get(eqBitPos, MetaDataAddrCompZ);
        PAL_ASSERT(TestAnyFlagSet(eqData, SliceBitsMustBeZero) == false);
    }
#endif

    // Extract the bankXor bits and verify that none of the other bits are set. See Gfx10Lib::HwlComputePipeBankXor.
    constexpr uint32 ColumnBits   = 2;
    constexpr uint32 BankXorBits  = 4;
    const     uint32 bankXorShift = ColumnBits + gbAddrConfig.bits.NUM_PIPES;
    const     uint32 bankXorMask  = ((1 << BankXorBits) - 1) << bankXorShift;
    const     uint32 bankXor      = (pipeBankXor & bankXorMask) >> bankXorShift;

    PAL_ASSERT((pipeBankXor & ~bankXorMask) == 0);

    // The width and height of the copy area in units of HTile elements, rounded up.
    const uint32 copyWidth  = RoundUpQuotient<uint32>(depthExtent.width,  8);
    const uint32 copyHeight = RoundUpQuotient<uint32>(depthExtent.height, 8);

    // Note that we pass our values through RpmUtil::PackBits to make sure that they actually fit.
    // An assert will trip if one of the assumptions outlined above is actually false.
    userData.pipeInterleaveLog2 = RpmUtil::PackBits<3>(gbAddrConfig.bits.PIPE_INTERLEAVE_SIZE);
    userData.packersLog2        = RpmUtil::PackBits<3>(gbAddrConfig.gfx103Plus.NUM_PKRS);
    userData.pipesLog2          = RpmUtil::PackBits<3>(gbAddrConfig.bits.NUM_PIPES);
    userData.capPipeLog2        = RpmUtil::PackBits<5>(pEqGenerator->CapPipe());
    userData.metaBlkWidthLog2   = RpmUtil::PackBits<5>(metaBlkExtentLog2.width);
    userData.metaBlkHeightLog2  = RpmUtil::PackBits<5>(metaBlkExtentLog2.height);
    userData.bankXor            = RpmUtil::PackBits<4>(bankXor);
    userData.pitchInMetaBlks    = RpmUtil::PackBits<7>(hTileAddrOutput.pitch >> metaBlkExtentLog2.width);
    userData.lastHtileX         = RpmUtil::PackBits<11>(copyWidth - 1);
    userData.lastHtileY         = RpmUtil::PackBits<11>(copyHeight - 1);
    userData.fourBitVrs         = (m_pDevice->Settings().vrsHtileEncoding ==
                                   VrsHtileEncoding::Gfx10VrsHtileEncodingFourBit);

    // Step 2: Execute the rate image to VRS copy shader.
    PAL_ASSERT(pPalDevice->ChipProperties().gfx9.rbPlus != 0);
    const Pal::ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::Gfx10VrsHtile);

    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // If no source image was provided we should bind a null image SRD because the shader treats out-of-bounds reads
    // as a 1x1 shading rate.
    uint32 rateImageSrd[8] = {};
    static_assert(sizeof(ImageSrd) == sizeof(rateImageSrd), "We assume an image SRD is eight DWORDs.");

    if (pSrcVrsImg != nullptr)
    {
        // The image SRD is only accessed by the shader if the extents are not zero, so create the image SRD
        // here where we know we have a valid source image.  By API definition, the source image can only have
        // a single slice and single mip level.
        const SubresRange  viewRange = { {}, 1, 1, 1 };
        ImageViewInfo      imageView = {};
        RpmUtil::BuildImageViewInfo(&imageView,
                                    *pSrcVrsImg,
                                    viewRange,
                                    pSrcVrsImg->GetImageCreateInfo().swizzledFormat,
                                    RpmUtil::DefaultRpmLayoutRead,
                                    pPalDevice->TexOptLevel(),
                                    false);
        pPalDevice->CreateImageViewSrds(1, &imageView, rateImageSrd);
    }
    else
    {
        const GpuChipProperties& chipProps = pPalDevice->ChipProperties();
        memcpy(rateImageSrd, chipProps.nullSrds.pNullImageView, chipProps.srdSizes.imageView);
    }

    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 6, 8, rateImageSrd);

    for (uint32 sliceOffset = 0; sliceOffset < pDsView->ArraySize(); ++sliceOffset)
    {
        // Update the slice user-data. No assert this time because we're purposely cutting off high slice bits.
        const uint32 slice = pDsView->BaseArraySlice() + sliceOffset;
        userData.sliceBits = slice & BitfieldGenMask<uint32>(6);

        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 0, 2, userData.u32All);

        // We can save user-data if we adjust the HTile buffer view to point directly at the target subresource.
        BufferViewInfo bufferViewHtile = {};
        pHtile->BuildSurfBufferView(&bufferViewHtile);

        const gpusize hTileOffset = hTileMipInfo.offset + slice * hTileAddrOutput.sliceSize;
        bufferViewHtile.gpuAddr += hTileOffset;
        bufferViewHtile.range   -= hTileOffset;

        uint32 hTileSrd[4] = {};
        static_assert(sizeof(BufferSrd) == sizeof(hTileSrd), "We assume a buffer SRD is four DWORDs.");

        pPalDevice->CreateUntypedBufferViewSrds(1, &bufferViewHtile, hTileSrd);
        pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute, 2, 4, hTileSrd);

        // Launch one thread per HTile element we're copying in this slice.
        pCmdBuffer->CmdDispatch({RpmUtil::MinThreadGroups(copyWidth,  threadsPerGroup.x),
                                 RpmUtil::MinThreadGroups(copyHeight, threadsPerGroup.y), 1},
                                 {});
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);

    // For internal VRS DSV, it's always directly accessed and no need track the status here.
    if (isClientDsv)
    {
        pCmdBuffer->SetCsBltDirectWriteMisalignedMdState(pHtile->GetImage().HasMisalignedMetadata());
    }
}

// =====================================================================================================================
// Updates hTile memory to reflect the VRS data supplied in the source image.
//
// Assumptions:  It is the callers responsibility to have bound a depth view that points to the supplied depth image!
//               This copy will work just fine if the depth image isn't bound, but the upcoming draw won't actually
//               utilize the newly copied VRS data if depth isn't bound.
void RsrcProcMgr::CopyVrsIntoHtile(
    GfxCmdBuffer*           pCmdBuffer,
    const DepthStencilView* pDsView,
    bool                    isClientDsv,
    const Extent3d&         depthExtent,
    const Pal::Image*       pSrcVrsImg
    ) const
{
    // What are we even doing here?
    PAL_ASSERT(m_pDevice->Parent()->ChipProperties().gfxip.supportsVrs);

    // If the client didn't bind a depth buffer how do they expect to use the results of this copy?
    PAL_ASSERT((pDsView != nullptr) && (pDsView->GetImage() != nullptr));

    // This function assumes it is only called on graphics command buffers.
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

    // This means that we either don't have hTile (so we don't have a destination for our copy) or this hTile
    // buffer wasn't created to receive VRS data.  Both of which would be bad.
    const Image*const          pDepthImg    = pDsView->GetImage();
    const Gfx9Htile*const      pHtile       = pDepthImg->GetHtile();
    PAL_ASSERT(pHtile->HasMetaEqGenerator());

    PAL_ASSERT((pHtile != nullptr) && (pHtile->GetHtileUsage().vrs != 0));

    auto*const pCmdStream = static_cast<CmdStream*>(pCmdBuffer->GetMainCmdStream());

    // Step 1: The internal pre-CS barrier. The depth image is already bound as a depth view so if we just launch the
    // shader right away we risk corrupting HTile. We need to be sure that any prior draws that reference the depth
    // image are idle, HTile writes have been flushed down from the DB, and all stale HTile data has been invalidated
    // in the shader caches.
    uint32*       pCmdSpace  = pCmdStream->ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(FLUSH_AND_INV_DB_META, pCmdBuffer->GetEngineType(), pCmdSpace);

    constexpr WriteWaitEopInfo WaitEopInfo = { .hwGlxSync  = SyncGlkInv | SyncGlvInv | SyncGl1Inv,
                                               .hwAcqPoint = AcquirePointMe };

    pCmdSpace  = pCmdBuffer->WriteWaitEop(WaitEopInfo, pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);

    LaunchOptimizedVrsCopyShader(pCmdBuffer, pDsView, isClientDsv, depthExtent, pSrcVrsImg, pHtile);

    // Step 3: The internal post-CS barrier. We must wait for the copy shader to finish. We invalidated the DB's
    // HTile cache in step 1 so we shouldn't need to touch the caches a second time.
    pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = pCmdBuffer->WriteWaitCsIdle(pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Gfx Dcc -> Display Dcc.
void RsrcProcMgr::CmdGfxDccToDisplayDcc(
    GfxCmdBuffer*     pCmdBuffer,
    const Pal::Image& image
    ) const
{
    const GfxImage* pGfxImage  = image.GetGfxImage();
    const Image*    pGfx9Image = static_cast<const Image*>(pGfxImage);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx10GfxDccToDisplayDcc);

    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    for (uint32  planeIdx = 0; planeIdx < image.GetImageInfo().numPlanes; planeIdx++)
    {
        const auto*     pSubResInfo       = image.SubresourceInfo(planeIdx);
        const Gfx9Dcc*  pDcc              = pGfx9Image->GetDcc(planeIdx);
        const Gfx9Dcc*  pDispDcc          = pGfx9Image->GetDisplayDcc(planeIdx);
        PAL_ASSERT(pDcc->HasMetaEqGenerator());
        const auto*     pEqGenerator      = pDcc->GetMetaEqGenerator();
        PAL_ASSERT(pDispDcc->HasMetaEqGenerator());
        const auto*     pDispEqGenerator  = pDispDcc->GetMetaEqGenerator();
        const auto&     dccAddrOutput     = pDcc->GetAddrOutput();
        const auto&     dispDccAddrOutput = pDispDcc->GetAddrOutput();

        const uint32 xInc = dccAddrOutput.compressBlkWidth;
        const uint32 yInc = dccAddrOutput.compressBlkHeight;
        const uint32 zInc = dccAddrOutput.compressBlkDepth;

        const uint32 inlineConstData[] =
        {
            // cb0[0]
            Log2(dccAddrOutput.metaBlkSize),
            Log2(dispDccAddrOutput.metaBlkSize),
            dccAddrOutput.metaBlkNumPerSlice,
            dispDccAddrOutput.metaBlkNumPerSlice,

            // cb0[1]
            Log2(dccAddrOutput.metaBlkWidth),
            Log2(dccAddrOutput.metaBlkHeight),
            Log2(dccAddrOutput.metaBlkDepth),
            dccAddrOutput.pitch / dccAddrOutput.metaBlkWidth,

            // cb0[2]
            Log2(dispDccAddrOutput.metaBlkWidth),
            Log2(dispDccAddrOutput.metaBlkHeight),
            Log2(dispDccAddrOutput.metaBlkDepth),
            dispDccAddrOutput.pitch / dispDccAddrOutput.metaBlkWidth,

            // cb0[3]
            Log2(xInc),
            Log2(yInc),
            Log2(zInc),
            0,

            // cb0[4]
            pSubResInfo->extentTexels.width,
            pSubResInfo->extentTexels.height,
            1,
            0,
        };

        constexpr uint32 BufferViewCount = 4;
        BufferViewInfo   bufferView[BufferViewCount] = {};
        BufferSrd        bufferSrds[BufferViewCount] = {};

        pDispDcc->BuildSurfBufferView(&bufferView[0]);       // Display Dcc
        pDcc->BuildSurfBufferView(&bufferView[1]);           // Gfx Dcc.
        pEqGenerator->BuildEqBufferView(&bufferView[2]);     // Gfx Dcc equation.
        pDispEqGenerator->BuildEqBufferView(&bufferView[3]); // Display Dcc equation.
        image.GetDevice()->CreateUntypedBufferViewSrds(BufferViewCount, &bufferView[0], &bufferSrds[0]);

        // Create an embedded user-data table and bind it to user data 0.
        constexpr uint32 InlineConstDataDwords = NumBytesToNumDwords(sizeof(inlineConstData));

        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
            pCmdBuffer,
            DwordsPerBufferSrd * BufferViewCount + InlineConstDataDwords,
            DwordsPerBufferSrd,
            PipelineBindPoint::Compute,
            0);

        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += DwordsPerBufferSrd * BufferViewCount;
        memcpy(pSrdTable, &inlineConstData[0], sizeof(inlineConstData));

        const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

        // How many tiles in X/Y/Z dimension. One thread for each tile.
        const uint32 numBlockX = (pSubResInfo->extentTexels.width  + xInc - 1) / xInc;
        const uint32 numBlockY = (pSubResInfo->extentTexels.height + yInc - 1) / yInc;
        const uint32 numBlockZ = 1;

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz({numBlockX, numBlockY, numBlockZ}, threadsPerGroup), {});
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void RsrcProcMgr::CmdDisplayDccFixUp(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::Image&  image
    ) const
{
    const GpuMemory*       pGpuMemory  = image.GetBoundGpuMemory().Memory();
    const ImageCreateInfo& createInfo  = image.GetImageCreateInfo();
    const Image*           pGfx9Image  = static_cast<const Image*>(image.GetGfxImage());
    constexpr uint32       ClearValue  = ReplicateByteAcrossDword(Gfx9Dcc::DecompressedValue);
    const Gfx9Dcc*         pDispDcc    = pGfx9Image->GetDisplayDcc(0);

    const auto&            dispDccAddrOutput = pDispDcc->GetAddrOutput();

    const SubresRange range = SubresourceRange(BaseSubres(0), 1, createInfo.mipLevels, createInfo.arraySize);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    for (uint32 mipIdx = 0; mipIdx < range.numMips; mipIdx++)
    {
        const uint32 absMipLevel = range.startSubres.mipLevel + mipIdx;
        const auto&  displayDccMipInfo = pDispDcc->GetAddrMipInfo(absMipLevel);

        if (displayDccMipInfo.sliceSize == 0)
        {
            // No further mip levels will have display DCC either so there's nothing left to do.
            break;
        }

        // The number of slices for 2D images is the number of slices; for 3D images, it's the depth
        // of the image for the current mip level.
        const uint32  numSlices = GetClearDepth(*pGfx9Image, range.startSubres.plane, range.numSlices, absMipLevel);

        // The "metaBlkDepth" parameter is the number of slices that the "dccRamSliceSize" covers.  For non-3D
        // images, this should always be 1 (i.e., one addrlib slice is one PAL API slice).  For 3D images, this
        // can be way more than the number of PAL API slices.
        const uint32  numSlicesToClear = Max(1u, numSlices / dispDccAddrOutput.metaBlkDepth);

        // GetMaskRamBaseAddr doesn't compute the base address of a mip level (only a slice offset), so we have to do
        // the math here ourselves. However, DCC memory is contiguous and traversed upon by  slice size, so we only need
        // the first slice offset and the total size of all slices calculated by num slices * ram slice size (if the ram
        // is identical to the mip's slice size).
        const gpusize maskRamBaseAddr = pGfx9Image->GetMaskRamBaseAddr(pDispDcc, 0);
        gpusize sliceOffset = range.startSubres.arraySlice * dispDccAddrOutput.dccRamSliceSize;
        gpusize clearAddr   = maskRamBaseAddr + sliceOffset + displayDccMipInfo.offset;

        // Although DCC memory is contiguous per subresource, the offset of each slice is traversed by an interval of
        // dccRamSliceSize, though written to with mip slice size. We can therfore dispatch a clear  once only if the
        // two sizes match. See also: Gfx9::RsrcProcMgr::ClearDccCompute() for a more detailed explanation.
        const bool canDispatchSingleClear = displayDccMipInfo.sliceSize == dispDccAddrOutput.dccRamSliceSize;

        if (canDispatchSingleClear)
        {
            const gpusize totalSize = numSlicesToClear * displayDccMipInfo.sliceSize;

            CmdFillMemory(pCmdBuffer,
                          false,         // don't save / restore the compute state
                          true,
                          clearAddr,
                          totalSize,
                          ClearValue);
        }
        else
        {
            for (uint32  sliceIdx = 0; sliceIdx < numSlicesToClear; sliceIdx++)
            {
                // Get the mem offset for each slice
                const uint32 absSlice = range.startSubres.arraySlice + sliceIdx;
                sliceOffset = absSlice * dispDccAddrOutput.dccRamSliceSize;
                clearAddr   = maskRamBaseAddr + sliceOffset + displayDccMipInfo.offset;

                CmdFillMemory(pCmdBuffer,
                              false,         // don't save / restore the compute state
                              true,
                              clearAddr,
                              displayDccMipInfo.sliceSize,
                              ClearValue);
            }
        }
    }

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Resolves a multisampled source Image into the single-sampled destination Image using the Image's resolve method.
void RsrcProcMgr::CmdResolveImage(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    ImageLayout               srcImageLayout,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    uint32                    flags
    ) const
{
    const ResolveMethod srcMethod = srcImage.GetImageInfo().resolveMethod;
    const ResolveMethod dstMethod = dstImage.GetImageInfo().resolveMethod;

    if (pCmdBuffer->GetEngineType() == EngineTypeCompute)
    {
        PAL_ASSERT((srcMethod.shaderCsFmask == 1) || (srcMethod.shaderCs == 1));
        ResolveImageCompute(pCmdBuffer,
                            srcImage,
                            srcImageLayout,
                            dstImage,
                            dstImageLayout,
                            resolveMode,
                            regionCount,
                            pRegions,
                            srcMethod,
                            flags);

        HwlFixupResolveDstImage(pCmdBuffer,
                                *dstImage.GetGfxImage(),
                                dstImageLayout,
                                pRegions,
                                regionCount,
                                true);
    }
    else
    {
        if ((srcMethod.fixedFunc == 1) && HwlCanDoFixedFuncResolve(srcImage,
                                                                   dstImage,
                                                                   resolveMode,
                                                                   regionCount,
                                                                   pRegions))
        {
            PAL_ASSERT(resolveMode == ResolveMode::Average);
            // this only support color resolves.
            ResolveImageFixedFunc(pCmdBuffer,
                                  srcImage,
                                  srcImageLayout,
                                  dstImage,
                                  dstImageLayout,
                                  regionCount,
                                  pRegions,
                                  flags);

            HwlFixupResolveDstImage(pCmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    false);
        }
        else if ((srcMethod.depthStencilCopy == 1) && (dstMethod.depthStencilCopy == 1) &&
                 (resolveMode == ResolveMode::Average) &&
                 (TestAnyFlagSet(flags, ImageResolveInvertY) == false) &&
                  HwlCanDoDepthStencilCopyResolve(srcImage, dstImage, regionCount, pRegions))
        {
            ResolveImageDepthStencilCopy(pCmdBuffer,
                                         srcImage,
                                         srcImageLayout,
                                         dstImage,
                                         dstImageLayout,
                                         regionCount,
                                         pRegions,
                                         flags);

            HwlHtileCopyAndFixUp(pCmdBuffer, srcImage, dstImage, dstImageLayout, regionCount, pRegions, false);
        }
        else if (dstMethod.shaderPs && (resolveMode == ResolveMode::Average))
        {
            if (dstImage.IsDepthStencilTarget())
            {
                // this only supports Depth/Stencil resolves.
                ResolveImageDepthStencilGraphics(pCmdBuffer,
                                                 srcImage,
                                                 srcImageLayout,
                                                 dstImage,
                                                 dstImageLayout,
                                                 regionCount,
                                                 pRegions,
                                                 flags);
            }
            else if (IsGfx11(*m_pDevice->Parent()))
            {
                HwlResolveImageGraphics(pCmdBuffer,
                                        srcImage,
                                        srcImageLayout,
                                        dstImage,
                                        dstImageLayout,
                                        regionCount,
                                        pRegions,
                                        flags);
            }
            else
            {
                PAL_NOT_IMPLEMENTED();
            }
        }
        else if ((srcMethod.shaderCsFmask == 1) || (srcMethod.shaderCs == 1))
        {
            ResolveImageCompute(pCmdBuffer,
                                srcImage,
                                srcImageLayout,
                                dstImage,
                                dstImageLayout,
                                resolveMode,
                                regionCount,
                                pRegions,
                                srcMethod,
                                flags);

            HwlFixupResolveDstImage(pCmdBuffer,
                                    *dstImage.GetGfxImage(),
                                    dstImageLayout,
                                    pRegions,
                                    regionCount,
                                    true);
        }
        else
        {
            PAL_NOT_IMPLEMENTED();
        }
    }
}

// =====================================================================================================================
void RsrcProcMgr::CmdResolvePrtPlusImage(
    GfxCmdBuffer*                    pCmdBuffer,
    const IImage&                    srcImage,
    ImageLayout                      srcImageLayout,
    const IImage&                    dstImage,
    ImageLayout                      dstImageLayout,
    PrtPlusResolveType               resolveType,
    uint32                           regionCount,
    const PrtPlusImageResolveRegion* pRegions
    ) const
{
    const auto*  pPalDevice    = m_pDevice->Parent();
    const auto&  srcPalImage   = static_cast<const Pal::Image&>(srcImage);
    const auto&  dstPalImage   = static_cast<const Pal::Image&>(dstImage);
    const auto&  srcCreateInfo = srcImage.GetImageCreateInfo();
    const auto&  dstCreateInfo = dstImage.GetImageCreateInfo();
    const auto   pipeline      = ((resolveType == PrtPlusResolveType::Decode)
                                  ? ((srcCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus)
                                     ? RpmComputePipeline::Gfx10PrtPlusResolveSamplingStatusMap
                                     : RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapDecode)
                                  : ((dstCreateInfo.prtPlus.mapType == PrtMapType::SamplingStatus)
                                     ? RpmComputePipeline::Gfx10PrtPlusResolveSamplingStatusMap
                                     : RpmComputePipeline::Gfx10PrtPlusResolveResidencyMapEncode));
    const auto*  pPipeline     = GetPipeline(pipeline);

    // DX spec requires that resolve source and destinations be 8bpp
    PAL_ASSERT((Formats::BitsPerPixel(dstCreateInfo.swizzledFormat.format) == 8) &&
               (Formats::BitsPerPixel(srcCreateInfo.swizzledFormat.format) == 8));

    // What are we even doing here?
    PAL_ASSERT(TestAnyFlagSet(pPalDevice->ChipProperties().imageProperties.prtFeatures,
                              PrtFeatureFlags::PrtFeaturePrtPlus));

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const DispatchDims threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    // Bind compute pipeline used for the resolve.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute,
                                  pPipeline,
                                  InternalApiPsoHash,
                                });

    for (uint32  regionIdx = 0; regionIdx < regionCount; regionIdx++)
    {
        const auto&  resolveRegion = pRegions[regionIdx];

        const uint32 constData[] =
        {
            // start cb0[0]
            uint32(resolveRegion.srcOffset.x),
            uint32(resolveRegion.srcOffset.y),
            uint32(resolveRegion.srcOffset.z),
            0u,
            // start cb0[1]
            uint32(resolveRegion.dstOffset.x),
            uint32(resolveRegion.dstOffset.y),
            uint32(resolveRegion.dstOffset.z),
            0u,
            // start cb0[2]
            resolveRegion.extent.width,
            resolveRegion.extent.height,
            ((srcCreateInfo.imageType == Pal::ImageType::Tex2d) ? resolveRegion.numSlices : resolveRegion.extent.depth),
            // cb0[2].w is ignored for residency maps
            ((resolveType == PrtPlusResolveType::Decode) ? 0xFFu : 0x01u),
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                    SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                    SrdDwordAlignment(),
                                                                    PipelineBindPoint::Compute,
                                                                    0);

        const SubresId    srcSubResId = Subres(0, resolveRegion.srcMipLevel, resolveRegion.srcSlice);
        const SubresRange srcRange    = SubresourceRange(srcSubResId, 1, 1, resolveRegion.numSlices);
        const SubresId    dstSubResId = Subres(0, resolveRegion.dstMipLevel, resolveRegion.dstSlice);
        const SubresRange dstRange    = SubresourceRange(dstSubResId, 1, 1, resolveRegion.numSlices);

        // For the sampling status shader, the format doesn't matter that much as it's just doing a "0" or "1"
        // comparison, but the residency map shader is decoding the bits, so we need the raw unfiltered data.
        constexpr SwizzledFormat X8Uint =
        {
            ChNumFormat::X8_Uint,
            {
                ChannelSwizzle::X,
                ChannelSwizzle::Zero,
                ChannelSwizzle::Zero,
                ChannelSwizzle::One
            }
        };

        ImageViewInfo   imageView[2] = {};
        const SwizzledFormat  srcFormat = ((resolveType == PrtPlusResolveType::Decode)
                                           ? X8Uint
                                           : srcCreateInfo.swizzledFormat);
        const SwizzledFormat  dstFormat = ((resolveType == PrtPlusResolveType::Decode)
                                           ? dstCreateInfo.swizzledFormat
                                           : X8Uint);
        RpmUtil::BuildImageViewInfo(&imageView[0],
                                    srcPalImage,
                                    srcRange,
                                    srcFormat,
                                    srcImageLayout,
                                    pPalDevice->TexOptLevel(),
                                    false);

        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    dstPalImage,
                                    dstRange,
                                    dstFormat,
                                    dstImageLayout,
                                    pPalDevice->TexOptLevel(),
                                    true);

        pPalDevice->CreateImageViewSrds(2, &imageView[0], pSrdTable);
        pSrdTable += Util::NumBytesToNumDwords(2 * sizeof(ImageSrd));

        // And give the shader all kinds of useful dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        const DispatchDims threads =
        {
            resolveRegion.extent.width,
            resolveRegion.extent.height,
            ((srcCreateInfo.imageType == Pal::ImageType::Tex2d) ? resolveRegion.numSlices : resolveRegion.extent.depth)
        };

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz(threads, threadsPerGroup), {});
    } // end loop through the regions

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);

    pCmdBuffer->SetCsBltIndirectWriteMisalignedMdState(dstPalImage.HasMisalignedMetadata());
}

// =====================================================================================================================
// Generate dcc lookup table.
void RsrcProcMgr::BuildDccLookupTable(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       srcImage,
    const SubresRange& range
    ) const
{
    const Pal::Image*  pPalImage    = srcImage.Parent();
    const GfxImage*    pSrcGfxImage = pPalImage->GetGfxImage();
    const Pal::Device* pDevice      = pPalImage->GetDevice();
    const auto&        createInfo   = pPalImage->GetImageCreateInfo();
    const Image*       pGfx9Image   = static_cast<const Image*>(pSrcGfxImage);

    const auto*        pBaseDcc          = pGfx9Image->GetDcc(range.startSubres.plane);
    PAL_ASSERT(pBaseDcc->HasMetaEqGenerator());
    const auto*        pEqGenerator      = pBaseDcc->GetMetaEqGenerator();
    const auto&        dccAddrOutput     = pBaseDcc->GetAddrOutput();
    const uint32       log2MetaBlkWidth  = Log2(dccAddrOutput.metaBlkWidth);
    const uint32       log2MetaBlkHeight = Log2(dccAddrOutput.metaBlkHeight);

    uint32 xInc = dccAddrOutput.compressBlkWidth;
    uint32 yInc = dccAddrOutput.compressBlkHeight;
    uint32 zInc = dccAddrOutput.compressBlkDepth;

    pBaseDcc->GetXyzInc(&xInc, &yInc, &zInc);

    const Pal::ComputePipeline*const pPipeline       = GetPipeline(RpmComputePipeline::Gfx10BuildDccLookupTable);
    const DispatchDims               threadsPerGroup = pPipeline->ThreadsPerGroupXyz();

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
    pCmdBuffer->CmdBindPipeline({PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    constexpr uint32 BufferViewCount = 2;
    BufferViewInfo bufferViews[BufferViewCount] = {};
    BufferSrd      bufferSrds[BufferViewCount]  = {};

    // Create a view of dcc equation.
    pEqGenerator->BuildEqBufferView(&bufferViews[0]);
    // Create a view of dcc lookup table buffer.
    pGfx9Image->BuildDccLookupTableBufferView(&bufferViews[1]);

    pDevice->CreateUntypedBufferViewSrds(2, &bufferViews[0], &bufferSrds[0]);

    const uint32 worksX = dccAddrOutput.metaBlkWidth  / dccAddrOutput.compressBlkWidth;
    const uint32 worksY = dccAddrOutput.metaBlkHeight / dccAddrOutput.compressBlkHeight;
    const uint32 worksZ = createInfo.arraySize;

    const uint32 eqConstData[] =
    {
        // cb0[0]
        range.startSubres.arraySlice,
        pBaseDcc->GetNumEffectiveSamples(DccClearPurpose::FastClear),
        worksX,
        worksX * worksY,

        // cb0[1]
        log2MetaBlkWidth,
        log2MetaBlkHeight,
        Log2(dccAddrOutput.metaBlkDepth),
        0,

        // cb0[2]
        Log2(xInc),
        Log2(yInc),
        Log2(zInc),
        0,

        // cb0[3]
        dccAddrOutput.metaBlkWidth,
        dccAddrOutput.metaBlkHeight,
        createInfo.arraySize,
        0
    };

    constexpr uint32 SizeEqConstDataDwords = NumBytesToNumDwords(sizeof(eqConstData));

    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
                            pCmdBuffer,
                            DwordsPerBufferSrd * BufferViewCount + SizeEqConstDataDwords,
                            DwordsPerBufferSrd,
                            PipelineBindPoint::Compute,
                            0);

    memcpy(pSrdTable, &bufferSrds[0], sizeof(BufferSrd) * BufferViewCount);
    pSrdTable += DwordsPerBufferSrd * BufferViewCount;
    memcpy(pSrdTable, &eqConstData[0], sizeof(eqConstData));

    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroupsXyz({worksX, worksY, worksZ}, threadsPerGroup), {});

    pCmdBuffer->CmdRestoreComputeStateInternal(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a color target.
// Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
void RsrcProcMgr::PreComputeColorClearSync(
    ICmdBuffer*        pCmdBuffer,
    const IImage*      pImage,
    const SubresRange& subres,
    ImageLayout        layout)
{
    ImgBarrier imgBarrier = {};

    imgBarrier.srcStageMask  = PipelineStageColorTarget;
    // Fast clear path may have CP to update metadata state/values, wait at BLT/ME stage for safe.
    imgBarrier.dstStageMask  = PipelineStageBlt;
    imgBarrier.srcAccessMask = CoherColorTarget;
    imgBarrier.dstAccessMask = CoherShader;
    imgBarrier.subresRange   = subres;
    imgBarrier.pImage        = pImage;
    imgBarrier.oldLayout     = layout;
    imgBarrier.newLayout     = layout;

    AcquireReleaseInfo acqRelInfo = {};

    acqRelInfo.imageBarrierCount = 1;
    acqRelInfo.pImageBarriers    = &imgBarrier;
    acqRelInfo.reason            = Developer::BarrierReasonPreComputeColorClear;

    pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// color target.  Only necessary when the client specifies the ColorClearAutoSync flag for a color clear.
void RsrcProcMgr::PostComputeColorClearSync(
    ICmdBuffer*        pCmdBuffer,
    const IImage*      pImage,
    const SubresRange& subres,
    ImageLayout        layout,
    bool               csFastClear)
{
    ImgBarrier imgBarrier = {};

    // Optimization: For post CS fast Clear to ColorTarget transition, no need flush DST caches and invalidate
    //               SRC caches. Both cs fast clear and ColorTarget access metadata in direct mode, so no need
    //               L2 flush/inv even if the metadata is misaligned. See GetCacheSyncOps() for more details.
    //               Safe to pass 0 here, so no cache operation and PWS can wait at PreColor.
    imgBarrier.srcStageMask  = PipelineStageCs;
    imgBarrier.dstStageMask  = PipelineStageColorTarget;
    imgBarrier.srcAccessMask = csFastClear ? 0 : CoherShader;
    imgBarrier.dstAccessMask = csFastClear ? 0 : CoherColorTarget;
    imgBarrier.subresRange   = subres;
    imgBarrier.pImage        = pImage;
    imgBarrier.oldLayout     = layout;
    imgBarrier.newLayout     = layout;

    AcquireReleaseInfo acqRelInfo = {};

    acqRelInfo.imageBarrierCount = 1;
    acqRelInfo.pImageBarriers    = &imgBarrier;
    acqRelInfo.reason            = Developer::BarrierReasonPostComputeColorClear;

    pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
}

// =====================================================================================================================
// Inserts barrier needed before issuing a compute clear when the target image is currently bound as a depth/stencil
// target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void RsrcProcMgr::PreComputeDepthStencilClearSync(
    ICmdBuffer*        pCmdBuffer,
    const GfxImage&    gfxImage,
    const SubresRange& subres,
    ImageLayout        layout)
{
    PAL_ASSERT(subres.numPlanes == 1);

    ImgBarrier imgBarrier    = {};
    imgBarrier.pImage        = gfxImage.Parent();
    imgBarrier.subresRange   = subres;
    imgBarrier.srcStageMask  = PipelineStageDsTarget;
    imgBarrier.dstStageMask  = PipelineStageCs;
    imgBarrier.srcAccessMask = CoherDepthStencilTarget;
    imgBarrier.dstAccessMask = CoherShader;
    imgBarrier.oldLayout     = layout;
    imgBarrier.newLayout     = layout;

    AcquireReleaseInfo acqRelInfo = {};
    acqRelInfo.pImageBarriers    = &imgBarrier;
    acqRelInfo.imageBarrierCount = 1;
    acqRelInfo.reason            = Developer::BarrierReasonPreComputeDepthStencilClear;

    pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
}

// =====================================================================================================================
// Inserts barrier needed after issuing a compute clear when the target image will be immediately re-bound as a
// depth/stencil target.  Only necessary when the client specifies the DsClearAutoSync flag for a depth/stencil clear.
void RsrcProcMgr::PostComputeDepthStencilClearSync(
    ICmdBuffer*        pCmdBuffer,
    const GfxImage&    gfxImage,
    const SubresRange& subres,
    ImageLayout        layout,
    bool               csFastClear)
{
    const IImage* pImage     = gfxImage.Parent();
    ImgBarrier    imgBarrier = {};

    // Optimization: For post CS fast Clear to DepthStencilTarget transition, no need flush DST caches and
    //               invalidate SRC caches. Both cs fast clear and DepthStencilTarget access metadata in direct
    //               mode, so no need L2 flush/inv even if the metadata is misaligned. See GetCacheSyncOps() for
    //               more details. Safe to pass 0 here, so no cache operation and PWS can wait at PreDepth.
    imgBarrier.srcStageMask  = PipelineStageCs;
    imgBarrier.dstStageMask  = PipelineStageDsTarget;
    imgBarrier.srcAccessMask = csFastClear ? 0 : CoherShader;
    imgBarrier.dstAccessMask = csFastClear ? 0 : CoherDepthStencilTarget;
    imgBarrier.subresRange   = subres;
    imgBarrier.pImage        = pImage;
    imgBarrier.oldLayout     = layout;
    imgBarrier.newLayout     = layout;

    AcquireReleaseInfo acqRelInfo = {};

    acqRelInfo.imageBarrierCount = 1;
    acqRelInfo.pImageBarriers    = &imgBarrier;
    acqRelInfo.reason            = Developer::BarrierReasonPostComputeDepthStencilClear;

    pCmdBuffer->CmdReleaseThenAcquire(acqRelInfo);
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Return the bytes per block (element) of the format. For formats like YUY2, this function goes by the description of
// e.g: VK_FORMAT_G8B8G8R8_422_UNORM. This currently differs from how Pal thinks about such formats elsewhere.
//
// Examples:
//
// X32_Uint,          YUY2       ->  4 (1x1, 2x1 TexelsPerlock)
// X32Y32_Uint,       BC1_Unorm  ->  8 (1x1, 4x4 TexelsPerlock)
// X32Y32Z32W32_Uint, BC7_Unorm  -> 16 (1x1, 4x4 TexelsPerlock)
//
// NOTE: this function is incomplete. However, it is only used in an ASSERT, and what is implemented suffices for it.
static uint32 BytesPerBlock(
    ChNumFormat format)
{
    // Each plane may have a different BytesPerBlock, so passing a planar format in here doesn't make total sense.
    // Planes should mostly be handled one at a time.
    PAL_ASSERT(Formats::IsYuvPlanar(format) == false);

    uint32 value = Formats::BytesPerPixel(format);
    switch (format)
    {
    case ChNumFormat::UYVY:
    case ChNumFormat::VYUY:
    case ChNumFormat::YUY2:
    case ChNumFormat::YVY2:
        value = 4;
        break;
    default:
        PAL_ASSERT((Formats::IsMacroPixelPacked(format) == false) &&
                   (Formats::IsYuvPacked(format) == false));
        break;
    }
    return value;
}

// =====================================================================================================================
static void CheckImagePlaneSupportsRtvOrUavFormat(
    const GfxDevice&      device,
    const Pal::Image&     dstImage,
    const SwizzledFormat& imagePlaneFormat,
    const SwizzledFormat& viewFormat)
{
    const ChNumFormat actualViewFormat =
        (viewFormat.format == Pal::ChNumFormat::Undefined) ? imagePlaneFormat.format : viewFormat.format;

    // There is no well-defined way to interpret a clear color for a block-compressed view format.
    // If the image format is block-compressed, the view format must be a regular color format of matching
    // bytes per block, like R32G32_UINT on BC1.
    PAL_ASSERT(Formats::IsBlockCompressed(actualViewFormat) == false);
    PAL_ASSERT(Formats::IsYuvPlanar(actualViewFormat) == false);

    if (actualViewFormat != imagePlaneFormat.format)
    {
        PAL_ASSERT(BytesPerBlock(viewFormat.format) == BytesPerBlock(imagePlaneFormat.format));

        const bool hasMetadata = (dstImage.GetMemoryLayout().metadataSize != 0);

        const DccFormatEncoding
            computedPlaneViewEncoding = device.ComputeDccFormatEncoding(imagePlaneFormat, &viewFormat, 1),
            imageEncoding             = dstImage.GetImageInfo().dccFormatEncoding;

        const bool relaxedCheck = Formats::IsMacroPixelPacked(imagePlaneFormat.format) ||
                                  Formats::IsYuvPacked(imagePlaneFormat.format)        ||
                                  Formats::IsBlockCompressed(imagePlaneFormat.format);

        // Check a view format that is potentially different than the image plane's format is
        // compatible with the image's selected DCC encoding. This should guard against compression-related corruption,
        // and should always be true if the clearFormat is one of the pViewFormat's specified at image-creation time.
        //
        // For views on image formats like YUY2 or BC1, just check the image has no metadata;
        // equal BytesPerBlock (tested above) should be enough.
        PAL_ASSERT(relaxedCheck ?
                   (hasMetadata == false) :
                   (computedPlaneViewEncoding >= dstImage.GetImageInfo().dccFormatEncoding));
    }
}
#endif

// =====================================================================================================================
// Builds commands to clear the specified ranges of an image to the given color data.
void RsrcProcMgr::CmdClearColorImage(
    GfxCmdBuffer*         pCmdBuffer,
    const Pal::Image&     dstImage,
    ImageLayout           dstImageLayout,
    const ClearColor&     color,
    const SwizzledFormat& clearFormat,
    uint32                rangeCount,
    const SubresRange*    pRanges,
    uint32                boxCount,
    const Box*            pBoxes,
    uint32                flags
    ) const
{
    GfxImage*              pGfxImage  = dstImage.GetGfxImage();
    const ImageCreateInfo& createInfo = dstImage.GetImageCreateInfo();

    const bool sameChNumFormat = (clearFormat.format == ChNumFormat::Undefined) ||
                                 (clearFormat.format == createInfo.swizzledFormat.format);
    // The (boxCount == 1) calculation is not accurate for cases of a view on a nonzero mip, nonzero plane, or
    // VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT-like cases (including e.g: X32_Uint on YUY2).
    // However, this is fine as we only use this to decide to fast-clear.
    const bool clearBoxCoversWholeImage = BoxesCoverWholeExtent(createInfo.extent, boxCount, pBoxes);

    const bool skipIfSlow          = TestAnyFlagSet(flags, ColorClearSkipIfSlow);
    const bool needPreComputeSync  = TestAnyFlagSet(flags, ColorClearAutoSync);
    bool       needPostComputeSync = false;
    bool       csFastClear         = false;

    for (uint32 rangeIdx = 0; rangeIdx < rangeCount; ++rangeIdx)
    {
        PAL_ASSERT(pRanges[rangeIdx].numPlanes == 1);

        SubresRange        minSlowClearRange = { };
        const SubresRange* pSlowClearRange   = &minSlowClearRange;
        const SubresRange& clearRange        = pRanges[rangeIdx];

        const SwizzledFormat& subresourceFormat = dstImage.SubresourceInfo(pRanges[rangeIdx].startSubres)->format;
        const SwizzledFormat& viewFormat        = sameChNumFormat ? subresourceFormat : clearFormat;
        ClearMethod           slowClearMethod   = m_pDevice->GetDefaultSlowClearMethod(dstImage.GetImageCreateInfo(),
                                                                                       subresourceFormat);

#if PAL_ENABLE_PRINTS_ASSERTS
        CheckImagePlaneSupportsRtvOrUavFormat(*m_pDevice, dstImage, subresourceFormat, viewFormat);
#endif

        uint32 convertedColor[4] = { };
        if (color.type == ClearColorType::Float)
        {
            Formats::ConvertColor(viewFormat, &color.f32Color[0], &convertedColor[0]);
        }
        else
        {
            memcpy(&convertedColor[0], &color.u32Color[0], sizeof(convertedColor));
        }

        // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes.  Futher, we only
        // can store one fast clear color per mip level, and therefore can only support fast clears when a range covers
        // all slices.
        // Fast clear is only usable when all channels of the color are being written.
        if ((color.disabledChannelMask == 0) &&
            clearBoxCoversWholeImage         &&
            // If the client is requesting slow clears, then we don't want to do a fast clear here.
            (TestAnyFlagSet(flags, ClearColorImageFlags::ColorClearForceSlow) == false) &&
            pGfxImage->IsFastColorClearSupported(pCmdBuffer, dstImageLayout, &convertedColor[0], clearRange))
        {
            // Assume that all portions of the original range can be fast cleared.
            SubresRange fastClearRange = clearRange;

            // Assume that no portion of the original range needs to be slow cleared.
            minSlowClearRange.startSubres = clearRange.startSubres;
            minSlowClearRange.numPlanes   = clearRange.numPlanes;
            minSlowClearRange.numSlices   = clearRange.numSlices;
            minSlowClearRange.numMips     = 0;

            for (uint32 mipIdx = 0; mipIdx < clearRange.numMips; ++mipIdx)
            {
                const SubresId subres =
                    Subres(clearRange.startSubres.plane, clearRange.startSubres.mipLevel + mipIdx, 0);
                ClearMethod clearMethod = dstImage.SubresourceInfo(subres)->clearMethod;
                if (clearMethod == ClearMethod::FastUncertain)
                {
                    if ((Formats::BitsPerPixel(clearFormat.format) == 128) &&
                        (convertedColor[0] == convertedColor[1]) &&
                        (convertedColor[0] == convertedColor[2]))
                    {
                        const bool isAc01 = IsAc01ColorClearCode(*pGfxImage,
                                                                 &convertedColor[0],
                                                                 clearFormat,
                                                                 fastClearRange);
                        if (isAc01)
                        {
                            // AC01 path check
                            clearMethod = ClearMethod::Fast;
                        }
                        else if ((convertedColor[0] == convertedColor[3]) && IsGfx10(*m_pDevice->Parent()))
                        {
                            // comp-to-reg check for non {0, 1}: make sure all clear values are equal,
                            // simplest way to support 128BPP fastclear based on current code
                            clearMethod = ClearMethod::Fast;
                        }
                        else
                        {
                            clearMethod = slowClearMethod;
                        }
                    }
                    else
                    {
                       clearMethod = slowClearMethod;
                    }
                }

                if (clearMethod != ClearMethod::Fast)
                {
                    fastClearRange.numMips = uint8(mipIdx);

                    minSlowClearRange.startSubres.mipLevel = subres.mipLevel;
                    minSlowClearRange.numMips              = clearRange.numMips - uint8(mipIdx);
                    slowClearMethod                        = clearMethod;
                    break;
                }
            }

            if (fastClearRange.numMips != 0)
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pCmdBuffer, &dstImage, pRanges[rangeIdx], dstImageLayout);

                    needPostComputeSync = true;
                    csFastClear         = true;
                }

                HwlFastColorClear(pCmdBuffer,
                                  *pGfxImage,
                                  &convertedColor[0],
                                  clearFormat,
                                  fastClearRange,
                                  (needPreComputeSync == false));
            }
        }
        else
        {
            // Since fast clears aren't available, the slow-clear range is everything the caller asked for.
            pSlowClearRange = &clearRange;
        }

        // If we couldn't fast clear every range, then we need to slow clear whatever is left over.
        if ((pSlowClearRange->numMips != 0) && (skipIfSlow == false))
        {
            if ((slowClearMethod == ClearMethod::NormalGraphics) && pCmdBuffer->IsGraphicsSupported())
            {
                SlowClearGraphics(pCmdBuffer,
                                  dstImage,
                                  dstImageLayout,
                                  color,
                                  clearFormat,
                                  *pSlowClearRange,
                                  (needPreComputeSync == false),
                                  boxCount,
                                  pBoxes);
            }
            else
            {
                if (needPreComputeSync)
                {
                    PreComputeColorClearSync(pCmdBuffer, &dstImage, pRanges[rangeIdx], dstImageLayout);

                    needPostComputeSync = true;
                }

                // Raw format clears are ok on the compute engine because these won't affect the state of DCC memory.
                SlowClearCompute(pCmdBuffer,
                                 dstImage,
                                 dstImageLayout,
                                 color,
                                 clearFormat,
                                 *pSlowClearRange,
                                 (needPreComputeSync == false),
                                 boxCount,
                                 pBoxes);
            }
        }

        if (needPostComputeSync)
        {
            PostComputeColorClearSync(pCmdBuffer, &dstImage, pRanges[rangeIdx], dstImageLayout, csFastClear);

            needPostComputeSync = false;
        }
    }
}

// =====================================================================================================================
// Builds commands to clear the specified ranges of a depth/stencil image to the specified values.
void RsrcProcMgr::CmdClearDepthStencil(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::Image&  dstImage,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint8              stencilWriteMask,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags
    ) const
{
    const GfxImage& gfxImage   = *dstImage.GetGfxImage();
    const auto&     createInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT((rectCount == 0) || (pRects != nullptr));

    // Clear groups of ranges on "this group is fast clearable = true/false" boundaries
    uint32 rangesCleared = 0;

    // Convert the Rects to Boxes. We use an AutoBuffer instead of the virtual linear allocator because
    // we may need to allocate more boxes than will fit in the fixed virtual space.
    AutoBuffer<Box, 16, Platform> boxes(rectCount, m_pDevice->GetPlatform());

    // Notify the command buffer if AutoBuffer allocation has failed.
    if (boxes.Capacity() < rectCount)
    {
        pCmdBuffer->NotifyAllocFailure();
    }
    else
    {
        for (uint32 i = 0; i < rectCount; i++)
        {
            boxes[i].offset.x      = pRects[i].offset.x;
            boxes[i].offset.y      = pRects[i].offset.y;
            boxes[i].offset.z      = 0;
            boxes[i].extent.width  = pRects[i].extent.width;
            boxes[i].extent.height = pRects[i].extent.height;
            boxes[i].extent.depth  = 1;
        }

        const bool clearRectCoversWholeImage = BoxesCoverWholeExtent(createInfo.extent, rectCount, boxes.Data());

        while (rangesCleared < rangeCount)
        {
            const uint32 groupBegin = rangesCleared;

            // Note that fast clears don't support sub-rect clears so we skip them if we have any boxes. Further,
            // we only can store one fast clear color per mip level, and therefore can only support fast clears
            // when a range covers all slices.
            const bool groupFastClearable = (clearRectCoversWholeImage &&
                                             gfxImage.IsFastDepthStencilClearSupported(
                                                 depthLayout,
                                                 stencilLayout,
                                                 depth,
                                                 stencil,
                                                 stencilWriteMask,
                                                 pRanges[groupBegin]));

            // Find as many other ranges that also support/don't support fast clearing so that they can be grouped
            // together into a single clear operation.
            uint32 groupEnd = groupBegin + 1;

            while ((groupEnd < rangeCount)     &&
                   ((clearRectCoversWholeImage &&
                     gfxImage.IsFastDepthStencilClearSupported(depthLayout,
                                                               stencilLayout,
                                                               depth,
                                                               stencil,
                                                               stencilWriteMask,
                                                               pRanges[groupEnd]))
                    == groupFastClearable))
            {
                ++groupEnd;
            }

            // Either fast clear or slow clear this group of ranges.
            rangesCleared = groupEnd;
            const uint32 clearRangeCount = groupEnd - groupBegin; // NOTE: end equals one past the last range in group.

            HwlDepthStencilClear(pCmdBuffer,
                                 gfxImage,
                                 depthLayout,
                                 stencilLayout,
                                 depth,
                                 stencil,
                                 stencilWriteMask,
                                 clearRangeCount,
                                 &pRanges[groupBegin],
                                 groupFastClearable,
                                 TestAnyFlagSet(flags, DsClearAutoSync),
                                 rectCount,
                                 boxes.Data());
        }
    }
}

} // Gfx9
} // Pal
