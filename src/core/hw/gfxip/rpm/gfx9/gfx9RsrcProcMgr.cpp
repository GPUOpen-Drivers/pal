/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
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
        uint32 reserved          : 26;
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
// Retrieves the hardware color-buffer format for a given PAL format type.
static PAL_INLINE ColorFormat HwColorFormat(
    const Device*    pDevice,
    Pal::ChNumFormat format)
{
    ColorFormat hwColorFmt = COLOR_INVALID;
    GfxIpLevel gfxLevel = pDevice->Parent()->ChipProperties().gfxLevel;

    switch (gfxLevel)
    {
    case GfxIpLevel::GfxIp9:
        hwColorFmt = HwColorFmt(MergedChannelFmtInfoTbl(gfxLevel, &pDevice->GetPlatform()->PlatformSettings()),
                                format);
        break;

    case GfxIpLevel::GfxIp10_1:
    case GfxIpLevel::GfxIp10_3:
        hwColorFmt = HwColorFmt(MergedChannelFlatFmtInfoTbl(gfxLevel, &pDevice->GetPlatform()->PlatformSettings()),
                                format);
        break;

    default:
        PAL_NEVER_CALLED();
        break;
    }

    PAL_ASSERT(hwColorFmt != COLOR_INVALID);
    return hwColorFmt;
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

    const uint32 maxCompSize = Formats::MaxComponentBitCount(format.format);

    const ColorFormat hwColorFmt  = HwColorFormat(m_pDevice, format.format);
    const CompSetting compSetting = ComputeCompSetting(hwColorFmt, format);

    const bool hasAlpha = Formats::HasAlpha(format);
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());
    PAL_ASSERT(srcParent.IsCloneable() && dstParent.IsCloneable());
    PAL_ASSERT(memcmp(&srcParent.GetImageCreateInfo(), &dstParent.GetImageCreateInfo(), sizeof(ImageCreateInfo)) == 0);
    PAL_ASSERT(srcParent.GetGpuMemSize() == dstParent.GetGpuMemSize());

    // Copy all of the source image (including metadata) to the destination image.
    Pal::MemoryCopyRegion copyRegion = {};

    copyRegion.srcOffset = srcParent.GetBoundGpuMemory().Offset();
    copyRegion.dstOffset = dstParent.GetBoundGpuMemory().Offset();
    copyRegion.copySize  = dstParent.GetGpuMemSize();

    pCmdBuffer->CmdCopyMemory(*srcParent.GetBoundGpuMemory().Memory(),
                              *dstParent.GetBoundGpuMemory().Memory(),
                              1,
                              &copyRegion);
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
    // Force the compute shader copy path if any regions couldn't be executed by the CPDMA copy path:
    //
    //     - Size exceeds the maximum supported by CPDMA.
    //     - Source or destination are virtual resources (CP would halt).
    bool useCsCopy = srcGpuMemory.IsVirtual() || dstGpuMemory.IsVirtual();

    for (uint32 i = 0; !useCsCopy && (i < regionCount); i++)
    {
        if (pRegions[i].copySize > m_pDevice->Parent()->GetPublicSettings()->cpDmaCmdCopyMemoryMaxBytes)
        {
            // We will copy this region later on.
            useCsCopy = true;
        }
    }

    if (useCsCopy)
    {
        CopyMemoryCs(pCmdBuffer, srcGpuMemory, dstGpuMemory, regionCount, pRegions);
    }
    else
    {
        bool p2pBltInfoRequired = m_pDevice->Parent()->IsP2pBltWaRequired(dstGpuMemory);

        uint32 newRegionCount = 0;
        if (p2pBltInfoRequired)
        {
            m_pDevice->P2pBltWaModifyRegionListMemory(dstGpuMemory,
                                                      regionCount,
                                                      pRegions,
                                                      &newRegionCount,
                                                      nullptr,
                                                      nullptr);
        }

        AutoBuffer<MemoryCopyRegion, 32, Platform> newRegions(newRegionCount, m_pDevice->GetPlatform());
        AutoBuffer<gpusize, 32, Platform> chunkAddrs(newRegionCount, m_pDevice->GetPlatform());
        if (p2pBltInfoRequired)
        {
            if ((newRegions.Capacity() >= newRegionCount) && (chunkAddrs.Capacity() >= newRegionCount))
            {
                m_pDevice->P2pBltWaModifyRegionListMemory(dstGpuMemory,
                                                          regionCount,
                                                          pRegions,
                                                          &newRegionCount,
                                                          &newRegions[0],
                                                          &chunkAddrs[0]);
                regionCount = newRegionCount;
                pRegions    = &newRegions[0];

                pCmdBuffer->P2pBltWaCopyBegin(&dstGpuMemory, regionCount, &chunkAddrs[0]);
            }
            else
            {
                pCmdBuffer->NotifyAllocFailure();
                p2pBltInfoRequired = false;
            }
        }

        for (uint32 i = 0; i < regionCount; i++)
        {
            if (p2pBltInfoRequired)
            {
                pCmdBuffer->P2pBltWaCopyNextRegion(chunkAddrs[i]);
            }

            const gpusize dstAddr = dstGpuMemory.Desc().gpuVirtAddr + pRegions[i].dstOffset;
            const gpusize srcAddr = srcGpuMemory.Desc().gpuVirtAddr + pRegions[i].srcOffset;

            pCmdBuffer->CpCopyMemory(dstAddr, srcAddr, pRegions[i].copySize);
        }

        if (p2pBltInfoRequired)
        {
            pCmdBuffer->P2pBltWaCopyEnd();
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
        pCmdSpace += m_cmdUtil.BuildDmaData(dmaDataInfo, pCmdSpace);
        pStream->CommitCommands(pCmdSpace);

        // Update all variable addresses and sizes except for srcAddr and numBytes which will be reset above.
        pRemainingSrcData    = VoidPtrInc(pRemainingSrcData, dmaDataInfo.numBytes);
        remainingDataSize   -= dmaDataInfo.numBytes;
        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
    }

    pCmdBuffer->SetGfxCmdBufCpBltState(true);
    pCmdBuffer->SetGfxCmdBufCpBltWriteCacheState(true);
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
    if (Pal::Image::UseCpPacketOcclusionQuery                 &&
        // BinaryOcclusion might also go inside this path but CP cannot handle that.
        (queryType == QueryType::Occlusion)                   &&
        pCmdBuffer->IsGraphicsSupported()                     &&
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
        PAL_ASSERT(pCmdBuffer->IsComputeSupported());

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
    auto*const pStream = static_cast<CmdStream*>(pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute));
    PAL_ASSERT(pStream != nullptr);

    if (TestAnyFlagSet(flags, QueryResultWait) && queryPool.HasTimestamps())
    {
        // Wait for the query data to get to memory if it was requested.
        // The shader is required to implement the wait if the query pool doesn't have timestamps.
        queryPool.WaitForSlots(pStream, startQuery, queryCount);
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
    pCmdBuffer->CmdDispatch(threadGroups, 1, 1);

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// ====================================================================================================================
// Replicates the input code across all four bytes of a dword.
uint32 RsrcProcMgr::ExpandClearCodeToDword(
    uint8  clearCode)
{
    return (clearCode | (clearCode << 8) | (clearCode << 16) | (clearCode << 24));
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
        auto*const       pCmdStream =
            static_cast<CmdStream*>(pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute));

        PAL_ASSERT(pCmdStream != nullptr);

        AcquireMemInfo acquireInfo = {};
        acquireInfo.flags.invSqK$ = 1;
        acquireInfo.tcCacheOp     = TcCacheOp::InvL1;
        acquireInfo.engineType    = engineType;
        acquireInfo.baseAddress   = FullSyncBaseAddr;
        acquireInfo.sizeBytes     = FullSyncSize;

        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuffer->TimestampGpuVirtAddr(), pCmdSpace);
        pCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);

        if (engineType == EngineTypeUniversal)
        {
            pCmdStream->SetContextRollDetected<false>();
        }
    }
}

// ====================================================================================================================
// Returns the number of slices (for 2D images) or the depth of a 3D image that should be cleared for the specified
// mip level.
uint32 RsrcProcMgr::GetClearDepth(
    const Image& dstImage,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresRange& clearRange,
#else
    uint32       plane,
    uint32       numSlices,
#endif
    uint32       mipLevel
    ) const
{
    const Pal::Image*  pPalImage   = dstImage.Parent();
    const auto&        createInfo  = pPalImage->GetImageCreateInfo();
    const bool         is3dImage   = (createInfo.imageType == ImageType::Tex3d);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId     subResId    = { clearRange.startSubres.aspect, mipLevel, 0 };
#else
    const SubresId     subResId    = { plane, mipLevel, 0 };
#endif
    const auto*        pSubResInfo = pPalImage->SubresourceInfo(subResId);

    return (is3dImage
            ? pSubResInfo->extentTexels.depth
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            : clearRange.numSlices);
#else
            : numSlices);
#endif
}

// =====================================================================================================================
// Issues the dispatch call for the specified dimensions
void RsrcProcMgr::MetaDataDispatch(
    GfxCmdBuffer*       pCmdBuffer,        // command buffer used for the dispatch call
    const Gfx9MaskRam*  pMaskRam,          // mask ram the dispatch will access
    uint32              width,             // width of the mip level being cleared
    uint32              height,            // height of the mip-level being cleared
    uint32              depth,             // number of slices (either array or volume slices) being cleared
    const uint32*       pThreadsPerGroup)  // three-deep array indicating the number of threads per group
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
    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(x, pThreadsPerGroup[0]),
                            RpmUtil::MinThreadGroups(y, pThreadsPerGroup[1]),
                            RpmUtil::MinThreadGroups(z, pThreadsPerGroup[2]));
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

    // If any of following conditions is met, that means we are going to use PFP engine to update the metadata
    // (e.g. UpdateColorClearMetaData(); UpdateDccStateMetaData() etc.)
    if (pCmdBuffer->IsGraphicsSupported()       &&
        (dstImage.HasDccStateMetaData(range)    ||
         dstImage.HasFastClearMetaData(range)   ||
         dstImage.HasHiSPretestsMetaData()      ||
         dstImage.HasWaTcCompatZRangeMetaData() ||
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
            SubresId subresId = range.startSubres;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            for (; subresId.plane < (range.startSubres.plane + range.numPlanes); subresId.plane++)
#endif
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                const Gfx9Dcc* pDcc = dstImage.GetDcc(subresId.aspect);
#else
                const Gfx9Dcc* pDcc = dstImage.GetDcc(subresId.plane);
#endif

                PAL_ASSERT(pDcc->HasMetaEqGenerator());
                pDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

                if (dstImage.HasDisplayDccData())
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                    const Gfx9Dcc* pDispDcc = dstImage.GetDisplayDcc(subresId.aspect);
#else
                    const Gfx9Dcc* pDispDcc = dstImage.GetDisplayDcc(subresId.plane);
#endif

                    PAL_ASSERT(pDispDcc->HasMetaEqGenerator());
                    pDispDcc->GetMetaEqGenerator()->UploadEq(pCmdBuffer);

                }
            }

            const bool dccClearUsedCompute = ClearDcc(pCmdBuffer,
                                                      pCmdStream,
                                                      dstImage,
                                                      range,
                                                      Gfx9Dcc::InitialValue,
                                                      DccClearPurpose::Init);

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
            InitCmask(pCmdBuffer, pCmdStream, dstImage, range, dstImage.GetCmask()->GetInitialValue());

            // It's possible that this image will be resolved with fMask pipeline later, so the fMask must be cleared
            // here.
            pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
            ClearFmask(pCmdBuffer, dstImage, range, Gfx9Fmask::GetPackedExpandedValue(dstImage));
            pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    if (dstImage.HasHiSPretestsMetaData() && (range.startSubres.aspect == ImageAspect::Stencil))
#else
    if (dstImage.HasHiSPretestsMetaData() && pParentImg->HasStencilPlane(range))
#endif
    {
        ClearHiSPretestsMetaData(pCmdBuffer, pCmdStream, dstImage, range);
    }

    // Temporary we don't provide a fullrange path for htile lookup table build, only if we consider a cpu path in
    // the future.
    if (dstImage.HasHtileLookupTable())
    {
        // Htile lookup table will be built by cs blt, thus prompt to perform cs partial flush
        // since it could be followed by an immediate accessing.
        BuildHtileLookupTable(pCmdBuffer, dstImage, range);
        usedCompute = true;
    }

    if (dstImage.HasDccLookupTable())
    {
        BuildDccLookupTable(pCmdBuffer, dstImage, range);
        usedCompute = true;
    }

    // The metadata is used as a COND_EXEC condition, init ZRange meta data with 0(e.g, depth value is 1.0 by default)
    // to indicate DB_Z_INFO.ZRANGE_PRECISION register filed should not be overwrote via this workaround metadata.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    if (dstImage.HasWaTcCompatZRangeMetaData() && (range.startSubres.aspect == ImageAspect::Depth))
#else
    if (dstImage.HasWaTcCompatZRangeMetaData() && pParentImg->HasDepthPlane(range))
#endif
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        const Pm4Predicate packetPredicate = static_cast<Pm4Predicate>(
                        pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

        pCmdSpace = dstImage.UpdateWaTcCompatZRangeMetaData(range, 1.0f, packetPredicate, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }

    if (dstImage.HasDccStateMetaData(range))
    {
        // We need to initialize the Image's DCC state metadata to indicate that the Image can become DCC compressed
        // (or not) in upcoming operations.
        const bool canCompress = ImageLayoutCanCompressColorData(dstImage.LayoutToColorCompressionState(), layout);

        // If the new layout is one which can write compressed DCC data,  then we need to update the Image's DCC state
        // metadata to indicate that the image will become DCC compressed in upcoming operations.
        dstImage.UpdateDccStateMetaData(pCmdStream,
                                        range,
                                        canCompress,
                                        pCmdBuffer->GetEngineType(),
                                        PredDisable);
    }

    return usedCompute;
}

// =====================================================================================================================
// Issues a compute shader blt to initialize the htile lookup table for a image.
void RsrcProcMgr::BuildHtileLookupTable(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range
    ) const
{
    const Pal::Image*      pParentImg = dstImage.Parent();
    const Pal::Device*     pParentDev = pParentImg->GetDevice();
    const Gfx9PalSettings& settings = GetGfx9Settings(*pParentDev);

    const auto&     createInfo = pParentImg->GetImageCreateInfo();
    const auto*     pBaseHtile = dstImage.GetHtile();
    PAL_ASSERT(pBaseHtile->HasMetaEqGenerator());
    const auto*     pEqGenerator = pBaseHtile->GetMetaEqGenerator();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32    pipeBankXor = pEqGenerator->CalcPipeXorMask(range.startSubres.aspect);
#else
    const uint32    pipeBankXor = pEqGenerator->CalcPipeXorMask(range.startSubres.plane);
#endif
    const auto&     hTileAddrOutput = pBaseHtile->GetAddrOutput();
    const uint32    log2MetaBlkWidth = Log2(hTileAddrOutput.metaBlkWidth);
    const uint32    log2MetaBlkHeight = Log2(hTileAddrOutput.metaBlkHeight);
    const uint32    sliceSize = (hTileAddrOutput.pitch * hTileAddrOutput.height) >>
        (log2MetaBlkWidth + log2MetaBlkHeight);
    const uint32    effectiveSamples = pEqGenerator->GetNumEffectiveSamples();
    BufferSrd       bufferSrds[2] = {};
    uint32          threadsPerGroup[3] = {};

    if (m_pDevice->GetHwStencilFmt(createInfo.swizzledFormat.format) != STENCIL_INVALID)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        PAL_ASSERT(pipeBankXor == pEqGenerator->CalcPipeXorMask(ImageAspect::Stencil));
#else
        PAL_ASSERT(pipeBankXor == pEqGenerator->CalcPipeXorMask(dstImage.GetStencilPlane()));
#endif
    }

    const Pal::ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::Gfx9BuildHtileLookupTable);

    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // Save the command buffer's state
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Bind Compute Pipeline used for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Create a view of the hTile equation so that the shader can access it.
    BufferViewInfo hTileEqBufferView = {};
    pEqGenerator->BuildEqBufferView(&hTileEqBufferView);
    pParentDev->CreateUntypedBufferViewSrds(1, &hTileEqBufferView, &bufferSrds[1]);

    const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
    SubresId subresId = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    subresId.aspect = range.startSubres.aspect;
#else
    subresId.plane = range.startSubres.plane;
#endif
    for (uint32 mipLevel = range.startSubres.mipLevel; mipLevel <= lastMip; ++mipLevel)
    {
        // Fid the lookup table view for specified mip level
        BufferViewInfo hTileLookupTableBuferView = {};
        dstImage.BuildMetadataLookupTableBufferView(&hTileLookupTableBuferView, mipLevel);
        pParentDev->CreateUntypedBufferViewSrds(1, &hTileLookupTableBuferView, &bufferSrds[0]);

        const auto&   hTileMipInfo = pBaseHtile->GetAddrMipInfo(mipLevel);

        subresId.mipLevel = mipLevel;
        subresId.arraySlice = range.startSubres.arraySlice;
        uint32 mipLevelWidth = dstImage.Parent()->SubresourceInfo(subresId)->extentTexels.width;
        uint32 mipLevelHeight = dstImage.Parent()->SubresourceInfo(subresId)->extentTexels.height;

        const uint32 constData[] =
        {
            // start cb0[0]
            hTileMipInfo.startX,
            hTileMipInfo.startY,
            range.startSubres.arraySlice,
            sliceSize,
            // start cb0[1]
            log2MetaBlkWidth,
            log2MetaBlkHeight,
            0, // depth surfaces are always 2D
            hTileAddrOutput.pitch >> log2MetaBlkWidth,
            // start cb0[2]
            mipLevelWidth,
            mipLevelHeight,
            0,
            0,
            // start cb0[3]
            pipeBankXor,
            effectiveSamples,
            Pow2Align(mipLevelWidth, 8u) / 8u,
            Pow2Align(mipLevelHeight, 8u) / 8u
        };

        // Create an embedded user-data table and bind it to user data 0.
        static const uint32 sizeBufferSrdDwords = NumBytesToNumDwords(sizeof(BufferSrd));
        static const uint32 sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   (sizeBufferSrdDwords * 2) + sizeConstDataDwords,
                                                                   sizeBufferSrdDwords,
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Put the SRDs for the hTile buffer and hTile equation into shader-accessible memory
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Provide the shader with all kinds of fun dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        MetaDataDispatch(pCmdBuffer,
                         pBaseHtile,
                         mipLevelWidth,
                         mipLevelHeight,
                         range.numSlices,
                         threadsPerGroup);
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Some blts need to use GFXIP-specific algorithms to pick the proper graphics pipeline. The basePipeline is the first
// graphics state in a series of states that vary only on target format and target index.
const Pal::GraphicsPipeline* RsrcProcMgr::GetGfxPipelineByTargetIndexAndFormat(
    RpmGfxPipeline basePipeline,
    uint32         targetIndex,
    SwizzledFormat format
    ) const
{
    // There are only four ranges of pipelines that vary by export format and these are their bases.
    PAL_ASSERT((basePipeline == Copy_32ABGR)             ||
               (basePipeline == ResolveFixedFunc_32ABGR) ||
               (basePipeline == SlowColorClear0_32ABGR)  ||
               (basePipeline == ScaledCopy2d_32ABGR)     ||
               (basePipeline == ScaledCopy3d_32ABGR));

    const SPI_SHADER_EX_FORMAT exportFormat = DeterminePsExportFmt(format,
                                                                   false,  // Blend disabled
                                                                   true,   // Alpha is exported
                                                                   false,  // Blend Source Alpha disabled
                                                                   false); // Alpha-to-Coverage disabled

    const int32 pipelineOffset = ExportStateMapping[exportFormat];
    PAL_ASSERT(pipelineOffset >= 0);

    return GetGfxPipeline(static_cast<RpmGfxPipeline>(basePipeline + pipelineOffset + targetIndex * NumExportFormats));
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
// =====================================================================================================================
// Returns the image aspect that corresponds to the supplied base address.
ImageAspect RsrcProcMgr::DecodeImageViewSrdAspect(
    const Pal::Image&  image,
    gpusize            srdBaseAddr
    ) const
{
    ImageAspect  aspect = ImageAspect::Color;

    const auto& imageCreateInfo = image.GetImageCreateInfo();
    if (Formats::IsYuv(imageCreateInfo.swizzledFormat.format))
    {
        if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
        {
            const auto*  pGfxImage            = image.GetGfxImage();
            const auto&  imageInfo            = image.GetImageInfo();
            const auto   subResourcesPerPlane = imageCreateInfo.mipLevels * imageCreateInfo.arraySize;

            // For Planar YUV, loop through each plane and compare the address with SRD to determine which subresrouce
            // this SRD represents. On Gfx9, the base address (as programmed into the SRD) is always the address of
            // mip 0 / slice 0 for the aspect. Therefore, we only need to loop through each Plane.
            for (uint32 i = 0; i < imageInfo.numPlanes; ++i)
            {
                const auto     subResIdx      = i * subResourcesPerPlane;
                const auto*    pSubResInfo    = image.SubresourceInfo(subResIdx);
                const gpusize  aspectBaseAddr = pGfxImage->GetAspectBaseAddr(pSubResInfo->subresId.aspect);
                const auto     subResAddr     = Get256BAddrLo(aspectBaseAddr);

                if (srdBaseAddr == subResAddr)
                {
                    aspect = pSubResInfo->subresId.aspect;
                    break;
                }
            }

            PAL_ASSERT(aspect != ImageAspect::Color);
        }
        else
        {
            // For Packed YUV, it is always subresource 0
            aspect = image.SubresourceInfo(0)->subresId.aspect;
        }
    }

    return aspect;
}
#else
// =====================================================================================================================
// Returns the image plane that corresponds to the supplied base address.
uint32 RsrcProcMgr::DecodeImageViewSrdPlane(
    const Pal::Image&  image,
    gpusize            srdBaseAddr
    ) const
{
    uint32  plane = 0;

    const auto& imageCreateInfo = image.GetImageCreateInfo();

    if (Formats::IsYuvPlanar(imageCreateInfo.swizzledFormat.format))
    {
        const auto*  pGfxImage = image.GetGfxImage();
        const auto&  imageInfo = image.GetImageInfo();

        // For Planar YUV, loop through each plane and compare the address with SRD to determine which subresrouce
        // this SRD represents. On Gfx9, the base address (as programmed into the SRD) is always the address of
        // mip 0 / slice 0 for the plane. Therefore, we only need to loop through each Plane.
        for (uint32 planeIdx = 0; planeIdx < imageInfo.numPlanes; ++planeIdx)
        {
            const gpusize  planeBaseAddr = pGfxImage->GetPlaneBaseAddr(planeIdx);
            const auto     subResAddr    = Get256BAddrLo(planeBaseAddr);

            if (srdBaseAddr == subResAddr)
            {
                plane = planeIdx;
                break;
            }
        }
    }

    return plane;
}
#endif

// =====================================================================================================================
// Function to expand (decompress) hTile data associated with the given image / range.  Supports use of a compute
// queue expand for ASICs that support texture compatability of depth surfaces.  Falls back to the independent layer
// implementation for other ASICs
bool RsrcProcMgr::ExpandDepthStencil(
    GfxCmdBuffer*                pCmdBuffer,
    const Pal::Image&            image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    const auto&  device              = *m_pDevice->Parent();
    const auto&  settings            = m_pDevice->Settings();
    const auto*  pGfxImage           = reinterpret_cast<const Image*>(image.GetGfxImage());
    const bool   supportsComputePath = pCmdBuffer->IsComputeSupported() &&
                                       pGfxImage->SupportsComputeDecompress(range);

    bool usedCompute = false;

    // To do a compute expand, we need to either
    //   a) Be on the compute queue.  In this case we can't do a gfx decompress because it'll hang.
    //   b) Have a compute-capable image and- have the "compute" path forced through settings.
    if ((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
        (supportsComputePath && (TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways))))
    {
        const auto&       createInfo        = image.GetImageCreateInfo();
        const auto*       pPipeline         = GetComputeMaskRamExpandPipeline(image);
        const auto*       pHtile            = pGfxImage->GetHtile();
        auto*             pComputeCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
        const EngineType  engineType        = pCmdBuffer->GetEngineType();

        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Compute the number of thread groups needed to launch one thread per texel.
        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        bool earlyExit = false;
        for (uint32  mipIdx = 0; ((earlyExit == false) && (mipIdx < range.numMips)); mipIdx++)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId  mipBaseSubResId =  { range.startSubres.aspect, range.startSubres.mipLevel + mipIdx, 0 };
#else
            const SubresId  mipBaseSubResId =  { range.startSubres.plane, range.startSubres.mipLevel + mipIdx, 0 };
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                const SubresId     subResId =  { mipBaseSubResId.aspect,
#else
                const SubresId     subResId =  { mipBaseSubResId.plane,
#endif
                                                 mipBaseSubResId.mipLevel,
                                                 range.startSubres.arraySlice + sliceIdx };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                const SubresRange  viewRange = { subResId, 1, 1 };
#else
                const SubresRange  viewRange = { subResId, 1, 1, 1 };
#endif

                // Create an embedded user-data table and bind it to user data 0. We will need two views.
                uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
                                        pCmdBuffer,
                                        2 * SrdDwordAlignment() + sizeConstDataDwords,
                                        SrdDwordAlignment(),
                                        PipelineBindPoint::Compute,
                                        0);

                ImageViewInfo imageView[2] = {};
                RpmUtil::BuildImageViewInfo(&imageView[0],
                                            image,
                                            viewRange,
                                            createInfo.swizzledFormat,
                                            RpmUtil::DefaultRpmLayoutRead,
                                            device.TexOptLevel()); // src
                RpmUtil::BuildImageViewInfo(&imageView[1],
                                            image,
                                            viewRange,
                                            createInfo.swizzledFormat,
                                            RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                            device.TexOptLevel());  // dst
                device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

                pSrdTable += 2 * SrdDwordAlignment();
                memcpy(pSrdTable, constData, sizeof(constData));

                // Execute the dispatch.
                pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
            } // end loop through all the slices
        } // end loop through all the mip levels

        // Allow the rewrite of depth data to complete
        uint32* pComputeCmdSpace = pComputeCmdStream->ReserveCommands();
        pComputeCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuffer->TimestampGpuVirtAddr(), pComputeCmdSpace);
        pComputeCmdStream->CommitCommands(pComputeCmdSpace);

        // Restore the compute state here as the "initHtile" function is going to push the compute state again
        // for its own purposes.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

        // GFX10 supports shader-writes to an image with mask-ram; i.e., the HW automagically kept the mask-ram
        // and the image data in sync, so there's no need to mark the hTile data as expanded.  Doing so would
        // lead to corruption if compressed shader writes were enabled.
        if (IsGfx9(device))
        {
            // Mark all the hTile data as fully expanded
            InitHtile(pCmdBuffer, pComputeCmdStream, *pGfxImage, range);

            // And wait for that to finish...
            pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
            pComputeCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType,
                                                          pCmdBuffer->TimestampGpuVirtAddr(),
                                                          pComputeCmdSpace);
            pComputeCmdStream->CommitCommands(pComputeCmdSpace);

            usedCompute = true;
        }
    }
    else
    {
#if PAL_AMDGPU_BUILD
        // The issue could be triggered when clear non-tile aligned and non-TC compatible stencil that goes through
        // graphics fast clear path has the following steps.
        //
        // This could be a DB cache issue since it looks like DB fail to eliminate all fast clear codes when do
        // the expand operation.
        if ((image.SubresourceInfo(range.startSubres)->flags.supportMetaDataTexFetch == 0) &&
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            (range.startSubres.aspect == ImageAspect::Stencil))
#else
            image.IsStencilPlane(range.startSubres.plane))
#endif
        {
            auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
            PAL_ASSERT(pCmdStream != nullptr);
            const EngineType engineType = pCmdBuffer->GetEngineType();
            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(DB_CACHE_FLUSH_AND_INV, engineType, pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }
#endif
        // Do the expand the legacy way.
        Pal::RsrcProcMgr::ExpandDepthStencil(pCmdBuffer, image, pMsaaState, pQuadSamplePattern, range);
    }

    return usedCompute;
}

// =====================================================================================================================
bool RsrcProcMgr::WillDecompressWithCompute(
    const GfxCmdBuffer* pCmdBuffer,
    const Image&        gfxImage,
    const SubresRange&  range
    ) const
{
    const bool supportsComputePath = gfxImage.SupportsComputeDecompress(range);

    return ((pCmdBuffer->IsGraphicsSupported() == false) ||
            (supportsComputePath && (TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways))));
}

// =====================================================================================================================
// Performs a fast-clear on a color image by updating the image's DCC buffer.
void RsrcProcMgr::HwlFastColorClear(
    GfxCmdBuffer*      pCmdBuffer,
    const GfxImage&    dstImage,
    const uint32*      pConvertedColor,
    const SubresRange& clearRange
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(clearRange.numPlanes == 1);
#endif

    const Image& gfx9Image = static_cast<const Image&>(dstImage);

    PAL_ASSERT(gfx9Image.HasDccData());

    auto*const pCmdStream = static_cast<CmdStream*>(
        pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics));
    PAL_ASSERT(pCmdStream != nullptr);

    bool fastClearElimRequired = false;
    const uint8 fastClearCode =
        Gfx9Dcc::GetFastClearCode(gfx9Image, clearRange, pConvertedColor, &fastClearElimRequired);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();

    if (gfx9Image.GetFastClearEliminateMetaDataAddr(clearRange.startSubres) != 0)
    {
        const Pm4Predicate packetPredicate =
            static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

        // Update the image's FCE meta-data.
        pCmdSpace = gfx9Image.UpdateFastClearEliminateMetaData(pCmdBuffer,
                                                               clearRange,
                                                               fastClearElimRequired,
                                                               packetPredicate,
                                                               pCmdSpace);
    }

    const SwizzledFormat planeFormat       = dstImage.Parent()->SubresourceInfo(clearRange.startSubres)->format;
    uint32               swizzledColor[4]  = {};
    Formats::SwizzleColor(planeFormat, pConvertedColor, &swizzledColor[0]);

    uint32 packedColor[4] = {};
    Formats::PackRawClearColor(planeFormat, swizzledColor, &packedColor[0]);

    const Pm4Predicate packetPredicate = static_cast<Pm4Predicate>(
        pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

    // When the fast clear color depends on the clear reg, we must store the color for later FCE and update the
    // current clear color.
    // On GFX9, the CB will always use the value in the fast clear reg even for other clear codes. So the clear
    // reg must always match the fast clear color.
    // On GFX10 and later, the CB will get the fast clear value from the location indicated by the clear code.
    // So the clear reg should only be updated when we use ClearColorReg.
    if ((fastClearCode == static_cast<uint8>(Gfx9DccClearColor::ClearColorCompToReg)) ||
        (IsGfx9(*m_pDevice->Parent())))
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

    ClearDcc(pCmdBuffer, pCmdStream, gfx9Image, clearRange, fastClearCode, DccClearPurpose::FastClear, packedColor);

    if (gfx9Image.HasFmaskData())
    {
        // If DCC is enabled on an MSAA surface, CMask fast clears should not be used
        // instead fast clearing CMask to "0xCC" which is 1 fragment
        //
        // NOTE:  On Gfx9, if an image has fMask it will also have cMask.
        InitCmask(pCmdBuffer, pCmdStream, gfx9Image, clearRange, Gfx9Cmask::FastClearValueDcc);
    }
}

// =====================================================================================================================
// An optimized copy does a memcpy of the source fmask and cmask data to the destination image after it is finished.
// See the HwlFixupCopyDstImageMetaData function.  For this to work, the layout needs to be exactly the same between
// the two -- including the swizzle modes and pipe-bank XOR values associated with the fmask data.
bool RsrcProcMgr::HwlUseOptimizedImageCopy(
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
        if ((dstImgMemLayout.metadataSize + dstImgMemLayout.metadataHeaderSize) !=
            (srcImgMemLayout.metadataSize + srcImgMemLayout.metadataHeaderSize))
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
// This function fixes up Cmask/Fmask metadata state: either copy from src image or fix up to uncompressed state.
// - For Fmask optimized MSAA copy where we we preserve fmask fragmentation, copy Cmask/Fmask from source image to dst.
// - For image is created with fullCopyDstOnly=1, fix up Cmask/Fmask to uncompressed state.
void RsrcProcMgr::HwlFixupCopyDstImageMetaData(
    GfxCmdBuffer*           pCmdBuffer,
    const Pal::Image*       pSrcImage, // Should be nullptr if isFmaskCopyOptimized = false
    const Pal::Image&       dstImage,
    ImageLayout             dstImageLayout,
    const ImageFixupRegion* pRegions,
    uint32                  regionCount,
    bool                    isFmaskCopyOptimized
    ) const
{
    const auto&  gfx9DstImage = static_cast<const Gfx9::Image&>(*dstImage.GetGfxImage());

    if (gfx9DstImage.HasFmaskData())
    {
        if (isFmaskCopyOptimized)
        {
            PAL_ASSERT(pSrcImage != nullptr);

            // On fmask msaa copy through compute shader we do an optimization where we preserve fmask fragmentation
            // while copying the data from src to dest, which means dst needs to have fmask of src.  Note that updates
            // to this function need to be reflected in HwlUseOptimizedImageCopy as well.

            // Copy the src fmask and cmask data to destination
            const auto&       gfx9SrcImage = static_cast<const Gfx9::Image&>(*pSrcImage->GetGfxImage());
            const auto*       pSrcFmask    = gfx9SrcImage.GetFmask();
            const auto&       srcBoundMem  = pSrcImage->GetBoundGpuMemory();
            const IGpuMemory* pSrcMemory   = reinterpret_cast<const IGpuMemory*>(srcBoundMem.Memory());

            const auto*       pDstFmask    = gfx9DstImage.GetFmask();
            const auto&       dstBoundMem  = dstImage.GetBoundGpuMemory();
            const IGpuMemory* pDstMemory   = reinterpret_cast<const IGpuMemory*>(dstBoundMem.Memory());

            // Our calculation of "srcCopySize" below assumes that fmask memory comes before the cmask memory in
            // our orginzation of the image data.
            PAL_ASSERT(pSrcFmask->MemoryOffset() < gfx9SrcImage.GetCmask()->MemoryOffset());
            PAL_ASSERT(pDstFmask->MemoryOffset() < gfx9DstImage.GetCmask()->MemoryOffset());

            // dstImgMemLayout metadata size comparison to srcImgMemLayout is checked by caller.
            const ImageMemoryLayout& srcImgMemLayout = pSrcImage->GetMemoryLayout();

            const gpusize srcCopySize =
                (srcImgMemLayout.metadataSize - (pSrcFmask->MemoryOffset() - srcImgMemLayout.metadataOffset)) +
                srcImgMemLayout.metadataHeaderSize;

            MemoryCopyRegion memcpyRegion = {};
            memcpyRegion.srcOffset = srcBoundMem.Offset() + pSrcFmask->MemoryOffset();
            memcpyRegion.dstOffset = dstBoundMem.Offset() + pDstFmask->MemoryOffset();
            memcpyRegion.copySize  = srcCopySize;

            // Do the copy
            pCmdBuffer->CmdCopyMemory(*pSrcMemory, *pDstMemory, 1, &memcpyRegion);
        }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
        else
        {
            auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);

            PAL_ASSERT(dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0);

            // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst";
            // if the copy isn't compressed copy, need fix up dst metadata to uncompressed state.
            for (uint32 idx = 0; idx < regionCount; ++idx)
            {
                const auto& clearRegion = pRegions[idx];
                const auto* pSubResInfo = dstImage.SubresourceInfo(clearRegion.subres);

                SubresRange range = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                range.startSubres.aspect     = clearRegion.subres.aspect;
#else
                range.startSubres.plane      = clearRegion.subres.plane;
#endif
                range.startSubres.mipLevel   = clearRegion.subres.mipLevel;
                range.startSubres.arraySlice = clearRegion.subres.arraySlice;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                range.numPlanes              = 1;
#endif
                range.numMips                = 1;
                range.numSlices              = clearRegion.numSlices;

                // Since color data is no longer compressed set CMask and FMask to fully uncompressed.
                InitCmask(pCmdBuffer, pStream, gfx9DstImage, range, gfx9DstImage.GetCmask()->GetInitialValue());

                pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);
                ClearFmask(pCmdBuffer, gfx9DstImage, range, Gfx9Fmask::GetPackedExpandedValue(gfx9DstImage));
                pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
            }
        }
#endif
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
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif
    // Only gfx command buffers can have bound render targets / DS attachments.  Fast clears through compute command
    // buffers do not have to worry about updating fast clear value register state.
    PAL_ASSERT(pCmdBuffer->IsGraphicsSupported());

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
            // checking the metadata because we know the fast clear value here. And we only need to Re-write for the
            // case that clear Z to 0.0f
            if ((depth == 0.0f) && ((metaDataClearFlags & HtilePlaneDepth) != 0))
            {
                pCmdSpace = pView->UpdateZRangePrecision(false, pStream, pCmdSpace);
            }

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
    bool               needComputeSync,
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
    const Image& gfx9Image = static_cast<const Image&>(dstImage);

    bool needPreComputeSync  = needComputeSync;
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
#endif
                    // Fast depth clear method is the same for all subresources, so we can just check the first.
                    const SubResourceInfo& subResInfo = *gfx9Image.Parent()->SubresourceInfo(pRanges[idx].startSubres);
                    fastClearMethod[idx] = subResInfo.clearMethod;
                }

                auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
                PAL_ASSERT(pCmdStream != nullptr);

                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
#endif
                    const uint32 currentClearFlag =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                        (pRanges[idx].startSubres.aspect == ImageAspect::Depth) ? HtilePlaneDepth : HtilePlaneStencil;
#else
                        gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane)
                            ? HtilePlaneDepth : HtilePlaneStencil;
#endif

                    metaDataClearFlags |= currentClearFlag;

                    const Pm4Predicate packetPredicate = static_cast<Pm4Predicate>(
                        pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

                    uint32* pCmdSpace = pCmdStream->ReserveCommands();
                    pCmdSpace = gfx9Image.UpdateDepthClearMetaData(pRanges[idx],
                                                                   currentClearFlag,
                                                                   depth,
                                                                   stencil,
                                                                   packetPredicate,
                                                                   pCmdSpace);

                    // Update the metadata for the waTcCompatZRange workaround
                    if (gfx9Image.HasWaTcCompatZRangeMetaData() && ((currentClearFlag & HtilePlaneDepth) != 0))
                    {
                        pCmdSpace = gfx9Image.UpdateWaTcCompatZRangeMetaData(pRanges[idx],
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

            // Notify the command buffer that the AutoBuffer allocation has failed.
            if (isRangeProcessed.Capacity() < rangeCount)
            {
                pCmdBuffer->NotifyAllocFailure();
            }
            else
            {
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
#endif
                    isRangeProcessed[idx] = false;
                }

                // Now issue fast or slow clears to all ranges, grouping identical depth/stencil pairs if possible.
                for (uint32 idx = 0; idx < rangeCount; idx++)
                {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                    PAL_ASSERT(pRanges[idx].numPlanes == 1);
#endif
                    // No need to clear a range twice.
                    if (isRangeProcessed[idx])
                    {
                        continue;
                    }

                    uint32 clearFlags =
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                        (pRanges[idx].startSubres.aspect == ImageAspect::Depth) ? HtilePlaneDepth : HtilePlaneStencil;
#else
                        gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane)
                            ? HtilePlaneDepth : HtilePlaneStencil;
#endif

                    // Search the range list to see if there is a matching range which span the other plane.
                    for (uint32 forwardIdx = idx + 1; forwardIdx < rangeCount; ++forwardIdx)
                    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                        PAL_ASSERT(pRanges[forwardIdx].numPlanes == 1);
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                        if ((pRanges[forwardIdx].startSubres.aspect != pRanges[idx].startSubres.aspect) &&
#else
                        if ((pRanges[forwardIdx].startSubres.plane != pRanges[idx].startSubres.plane) &&
#endif
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

                    // DepthStencilClearGraphics() implements both fast and slow clears. For fast clears,
                    // if the image layout supports depth/stencil target usage and the image size is too small,
                    // the synchronization overhead of switching to compute and back is main performance bottleneck,
                    // prefer the graphics path for this case. While the image size is over this critical value,
                    // compute path has a good performance advantage, prefer the compute path for this.
                    if ((pCmdBuffer->IsComputeSupported() == false)              ||
                        (fastClearMethod[idx] == ClearMethod::DepthFastGraphics) ||
                        (fastClear == false)                                     ||
                        (PreferFastDepthStencilClearGraphics(dstImage, depthLayout, stencilLayout)))
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                            const ImageAspect aspect  = pRanges[idx].startSubres.aspect;
                            const bool        isDepth = (aspect == ImageAspect::Depth);
#else
                            const bool isDepth = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane);
#endif
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
                                                     stencil);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                        const ImageAspect aspect  = pRanges[idx].startSubres.aspect;
                        const bool        isDepth = (aspect == ImageAspect::Depth);
#else
                        const bool isDepth = gfx9Image.Parent()->IsDepthPlane(pRanges[idx].startSubres.plane);
#endif
                        PostComputeDepthStencilClearSync(pCmdBuffer,
                                                         gfx9Image,
                                                         pRanges[idx],
                                                         isDepth ? depthLayout : stencilLayout);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            PAL_ASSERT(pRanges[idx].numPlanes == 1);
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const ImageAspect            aspect      = pRanges[idx].startSubres.aspect;
            const bool                   isDepth     = (aspect == ImageAspect::Depth);
#else
            const auto& createInfo = pParent->GetImageCreateInfo();
            const bool  isDepth    = m_pDevice->Parent()->SupportsDepth(createInfo.swizzledFormat.format,
                                                                        ImageTiling::Optimal);
#endif
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

            if (isDepth)
            {
                // Expand first if depth plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, depthLayout) != DepthStencilDecomprNoHiZ)
                {
                    // No MSAA state is necessary here because this is a compute path.
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, nullptr, pRanges[idx]);
                }

                // For Depth slow clears, we use a float clear color.
                clearColor.type = ClearColorType::Float;
                clearColor.f32Color[0] = depth;
            }
            else
            {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                PAL_ASSERT(aspect == ImageAspect::Stencil);
#else
                PAL_ASSERT(m_pDevice->Parent()->SupportsStencil(createInfo.swizzledFormat.format,
                                                                ImageTiling::Optimal));
#endif
                // Expand first if stencil plane is not fully expanded.
                if (ImageLayoutToDepthCompressionState(layoutToState, stencilLayout) != DepthStencilDecomprNoHiZ)
                {
                    // No MSAA state is necessary here because this is a compute path.
                    ExpandDepthStencil(pCmdBuffer, *pParent, nullptr, nullptr, pRanges[idx]);
                }

                // For Stencil plane we use the stencil value directly.
                clearColor.type = ClearColorType::Uint;
                clearColor.u32Color[0] = stencil;
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
                             format,
                             &clearColor,
                             pRanges[idx],
                             boxCnt,
                             pBox);

            if (needPostComputeSync)
            {
                PostComputeDepthStencilClearSync(pCmdBuffer,
                                                 gfx9Image,
                                                 pRanges[idx],
                                                 isDepth ? depthLayout : stencilLayout);
                needPostComputeSync = false;
            }
        }
    }
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId srcSubResId = { imageRegion.srcAspect,
#else
        const SubresId srcSubResId = { imageRegion.srcPlane,
#endif
                                       imageRegion.dstMipLevel,
                                       imageRegion.srcSlice };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId dstSubResId = { imageRegion.dstAspect,
#else
        const SubresId dstSubResId = { imageRegion.dstPlane,
#endif
                                       imageRegion.dstMipLevel,
                                       imageRegion.dstSlice };

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
// Check if for all the regions, the format and swizzle mode are compatible for src and dst image.
// If all regions are compatible, we can do a fixed function resolve. Otherwise return false.
bool RsrcProcMgr::HwlCanDoDepthStencilCopyResolve(
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions
    ) const
{
    const ImageCreateInfo& srcCreateInfo = srcImage.GetImageCreateInfo();
    const ImageCreateInfo& dstCreateInfo = dstImage.GetImageCreateInfo();

    PAL_ASSERT(srcCreateInfo.imageType == dstCreateInfo.imageType);
    PAL_ASSERT(srcCreateInfo.imageType != ImageType::Tex3d);

    const Image*  pGfxSrcImage = reinterpret_cast<const Image*>(srcImage.GetGfxImage());
    const Image*  pGfxDstImage = reinterpret_cast<const Image*>(dstImage.GetGfxImage());

    AutoBuffer<const ImageResolveRegion*, 2 * MaxImageMipLevels, Platform>
        fixUpRegionList(regionCount, m_pDevice->GetPlatform());

    bool canDoDepthStencilCopyResolve = ((pGfxSrcImage->HasDsMetadata() && pGfxSrcImage->HasHtileLookupTable()) ||
                                         (pGfxDstImage->HasDsMetadata() && pGfxDstImage->HasHtileLookupTable()));

    if (fixUpRegionList.Capacity() >= regionCount)
    {
        uint32 mergedCount = 0;

        const auto* const pAddrMgr = static_cast<const AddrMgr2::AddrMgr2*>(m_pDevice->Parent()->GetAddrMgr());

        for (uint32 region = 0; canDoDepthStencilCopyResolve && (region < regionCount); ++region)
        {
            const ImageResolveRegion& imageRegion = pRegions[region];
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId srcSubResId = { imageRegion.srcAspect,
#else
            const SubresId srcSubResId = { imageRegion.srcPlane,
#endif
                                           0,
                                           imageRegion.srcSlice };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId dstSubResId = { imageRegion.dstAspect,
#else
            const SubresId dstSubResId = { imageRegion.dstPlane,
#endif
                                           imageRegion.dstMipLevel,
                                           imageRegion.dstSlice };

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            PAL_ASSERT(imageRegion.srcAspect == imageRegion.dstAspect);
#else
            PAL_ASSERT(imageRegion.srcPlane == imageRegion.dstPlane);
#endif

            const auto*   pSrcSubResInfo = srcImage.SubresourceInfo(srcSubResId);
            const auto&   srcAddrSettings = pGfxSrcImage->GetAddrSettings(pSrcSubResInfo);

            const auto*   pDstSubResInfo = dstImage.SubresourceInfo(dstSubResId);
            const auto&   dstAddrSettings = pGfxDstImage->GetAddrSettings(pDstSubResInfo);

            PAL_ASSERT(AddrMgr2::IsZSwizzle(srcAddrSettings.swizzleMode) &&
                AddrMgr2::IsZSwizzle(dstAddrSettings.swizzleMode));

            canDoDepthStencilCopyResolve &=
                ((memcmp(&pSrcSubResInfo->format, &pDstSubResInfo->format, sizeof(SwizzledFormat)) == 0) &&
                    (pAddrMgr->GetBlockSize(srcAddrSettings.swizzleMode) ==
                        pAddrMgr->GetBlockSize(dstAddrSettings.swizzleMode)));

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
                canDoDepthStencilCopyResolve &= (imageRegion.srcOffset.x == imageRegion.dstOffset.x);
                canDoDepthStencilCopyResolve &= (imageRegion.srcOffset.y == imageRegion.dstOffset.y);

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
            for (uint32 index = 0; canDoDepthStencilCopyResolve && (index < mergedCount); ++index)
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
    const Image& gfx9Image                      = static_cast<const Image&>(dstImage);
    const auto&  dstCreateInfo                  = dstImage.Parent()->GetImageCreateInfo();
    const auto&  device                         = *m_pDevice->Parent();
    bool         canDoFixupForDstImage          = true;

    if (dstImage.Parent()->IsDepthStencilTarget() == true)
    {
        for (uint32 i = 0; i< regionCount; i++)
        {
            // DepthStencilCompressed needs fixup after resolve.
            // DepthStencilDecomprWithHiZ needs fixup the values of HiZ.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId subResId = { pRegions->dstAspect, pRegions->dstMipLevel, pRegions->dstSlice };
#else
            const SubresId subResId = { pRegions->dstPlane, pRegions->dstMipLevel, pRegions->dstSlice };
#endif
            const DepthStencilLayoutToState& layoutToState = gfx9Image.LayoutToDepthCompressionState(subResId);

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

    // For Gfx9, we need do fixup after fixfuction or compute shader resolve.
    if (canDoFixupForDstImage && ((computeResolve == false) || IsGfx9(device)))
    {
        BarrierInfo      barrierInfo = {};
        Pal::SubresRange range       = {};
        AutoBuffer<BarrierTransition, 32, Platform> transition(regionCount, m_pDevice->GetPlatform());

        for (uint32 i = 0; i < regionCount; i++)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            range.startSubres.aspect     = pRegions[i].dstAspect;
#else
            range.startSubres.plane      = pRegions[i].dstPlane;
#endif
            range.startSubres.arraySlice = pRegions[i].dstSlice;
            range.startSubres.mipLevel   = pRegions[i].dstMipLevel;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            range.numPlanes              = 1;
#endif
            range.numMips                = 1;
            range.numSlices              = pRegions[i].numSlices;

            transition[i].imageInfo.pImage             = dstImage.Parent();
            transition[i].imageInfo.oldLayout.usages   = Pal::LayoutUninitializedTarget;
            transition[i].imageInfo.oldLayout.engines  = dstImageLayout.engines;

            transition[i].imageInfo.newLayout.usages   = dstImageLayout.usages;
            transition[i].imageInfo.newLayout.engines  = dstImageLayout.engines;
            transition[i].imageInfo.subresRange        = range;

            if (dstImage.Parent()->GetImageCreateInfo().flags.sampleLocsAlwaysKnown != 0)
            {
                PAL_ASSERT(pRegions[i].pQuadSamplePattern != nullptr);
            }
            else
            {
                PAL_ASSERT(pRegions[i].pQuadSamplePattern == nullptr);
            }
            transition[i].imageInfo.pQuadSamplePattern = pRegions[i].pQuadSamplePattern;
            transition[i].srcCacheMask                 = Pal::CoherResolve;
            transition[i].dstCacheMask                 = Pal::CoherResolve;
        }

        barrierInfo.pTransitions    = transition.Data();
        barrierInfo.transitionCount = regionCount;

        pCmdBuffer->CmdBarrier(barrierInfo);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    metaDataRange.startSubres.aspect     = range.startSubres.aspect;
#else
    metaDataRange.startSubres.plane      = range.startSubres.plane;
#endif
    metaDataRange.startSubres.mipLevel   = range.startSubres.mipLevel;
    metaDataRange.startSubres.arraySlice = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    metaDataRange.numPlanes              = range.numPlanes;
#endif
    metaDataRange.numMips                = range.numMips;
    metaDataRange.numSlices              = createInfo.arraySize;

    const Pm4Predicate packetPredicate   = static_cast<Pm4Predicate>(
                                            pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    metaDataRange.startSubres.aspect     = range.startSubres.aspect;
#else
    metaDataRange.startSubres.plane      = range.startSubres.plane;
#endif
    metaDataRange.startSubres.mipLevel   = range.startSubres.mipLevel;
    metaDataRange.startSubres.arraySlice = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    metaDataRange.numPlanes              = range.numPlanes;
#endif
    metaDataRange.numMips                = range.numMips;
    metaDataRange.numSlices              = createInfo.arraySize;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32 metaDataInitFlags = (range.startSubres.aspect == ImageAspect::Depth) ?
                                     HtilePlaneDepth : HtilePlaneStencil;
#else
    const uint32 metaDataInitFlags = (range.numPlanes == 2)
                                     ? (HtilePlaneStencil | HtilePlaneStencil)
                                     : (dstImage.Parent()->IsDepthPlane(range.startSubres.plane)
                                        ? HtilePlaneDepth : HtilePlaneStencil);
#endif

    const Pm4Predicate packetPredicate =
        static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

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

    const uint32 packedColor[4] = {0, 0, 0, 0};

    const Pm4Predicate packetPredicate =
        static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace = dstImage.UpdateColorClearMetaData(range,
                                                  packedColor,
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
    uint32             boxCnt,
    const Box*         pBox
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif
    PAL_ASSERT(dstImage.Parent()->IsDepthStencilTarget());
    PAL_ASSERT((fastClear == false) ||  dstImage.IsFastDepthStencilClearSupported(depthLayout,
                                                                                  stencilLayout,
                                                                                  depth,
                                                                                  stencil,
                                                                                  stencilWriteMask,
                                                                                  range));

    const bool clearDepth   = TestAnyFlagSet(clearMask, HtilePlaneDepth);
    const bool clearStencil = TestAnyFlagSet(clearMask, HtilePlaneStencil);
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

    pCmdBuffer->PushGraphicsState();

    // Bind the depth expand state because it's just a full image quad and a zero PS (with no internal flags) which
    // is also what we need for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Graphics, GetGfxPipeline(DepthExpand), InternalApiPsoHash, });
    pCmdBuffer->CmdBindMsaaState(GetMsaaState(dstImage.Parent()->GetImageCreateInfo().samples,
                                              dstImage.Parent()->GetImageCreateInfo().fragments));
    BindCommonGraphicsState(pCmdBuffer);
    pCmdBuffer->CmdSetStencilRefMasks(stencilRefMasks);

    if (clearDepth && ((depth >= 0.0f) && (depth <= 1.0f)))
    {
        // Enable viewport clamping if depth values are in the [0, 1] range. This avoids writing expanded depth
        // when using a float depth format. DepthExpand pipeline disables clamping by default.
        pCmdBuffer->CmdOverwriteDisableViewportClampForBlits(false);
    }

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

    // The DB assumes the driver won't change surface state registers once it binds a depth target. We must flush
    // the DB caches if we wish to change surface state. In PAL, we only change surface state if we switch fast clear
    // values or z range precision values. We can't know the previous surface state values so we must always flush the
    // DB caches when we do a graphics fast clear.
    if (fastClear)
    {
        auto*const pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
        PAL_ASSERT(pCmdStream != nullptr);
        const EngineType engineType = pCmdBuffer->GetEngineType();
        uint32* pCmdSpace = pCmdStream->ReserveCommands();
        pCmdSpace += m_cmdUtil.BuildNonSampleEventWrite(DB_CACHE_FLUSH_AND_INV, engineType, pCmdSpace);
        pCmdStream->CommitCommands(pCmdSpace);
    }

    // Each mipmap level has to be fast-cleared individually because a depth target view can only be tied to a
    // single mipmap level of the destination Image.
    const uint32 lastMip = (range.startSubres.mipLevel + range.numMips - 1);
    for (depthViewInfo.mipLevel  = range.startSubres.mipLevel;
         depthViewInfo.mipLevel <= lastMip;
         ++depthViewInfo.mipLevel)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId         subres     = { range.startSubres.aspect, depthViewInfo.mipLevel, 0 };
#else
        const SubresId         subres     = { range.startSubres.plane, depthViewInfo.mipLevel, 0 };
#endif
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
    pCmdBuffer->PopGraphicsState();
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

            SlowClearGraphics(pCmdBuffer, *pParent, dstImageLayout, &clearColor, clearRange, 0, nullptr);
            usedCompute = false;
        }
        else
        {
            ClearDccCompute(pCmdBuffer, pCmdStream, dstImage, clearRange, clearCode, clearPurpose);
        }
        break;

    case DccClearPurpose::FastClear:
        // Clears of DCC-images on the graphics queue should occur through the graphics engine, unless specifically
        // requeusted to occur on compute.
        PAL_ASSERT((pCmdBuffer->GetEngineType() == EngineTypeCompute) ||
                   TestAnyFlagSet(settings.dccOnComputeEnable, Gfx9DccOnComputeFastClear));

        ClearDccCompute(pCmdBuffer, pCmdStream, dstImage, clearRange, clearCode, clearPurpose, pPackedClearColor);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

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
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });
    const EngineType engineType = pCmdBuffer->GetEngineType();
    const uint32     lastMip    = range.startSubres.mipLevel + range.numMips - 1;
    bool             earlyExit  = false;

    for (uint32  mipLevel = range.startSubres.mipLevel; ((earlyExit == false) && (mipLevel <= lastMip)); mipLevel++)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId              mipBaseSubResId = { range.startSubres.aspect, mipLevel, 0 };
#else
        const SubresId              mipBaseSubResId = { range.startSubres.plane, mipLevel, 0 };
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId     subResId =  { mipBaseSubResId.aspect,
#else
            const SubresId     subResId =  { mipBaseSubResId.plane,
#endif
                                             mipBaseSubResId.mipLevel,
                                             range.startSubres.arraySlice + sliceIdx };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresRange  viewRange = { subResId, 1, 1 };
#else
            const SubresRange  viewRange = { subResId, 1, 1, 1 };
#endif

            // Create an embedded user-data table and bind it to user data 0. We will need two views.
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                       2 * SrdDwordAlignment() + sizeConstDataDwords,
                                                                       SrdDwordAlignment(),
                                                                       PipelineBindPoint::Compute,
                                                                       0);

            ImageViewInfo imageView[2] = {};
            RpmUtil::BuildImageViewInfo(&imageView[0],
                                        parentImg,
                                        viewRange,
                                        createInfo.swizzledFormat,
                                        RpmUtil::DefaultRpmLayoutRead,
                                        device.TexOptLevel()); // src

            RpmUtil::BuildImageViewInfo(&imageView[1],
                                        parentImg,
                                        viewRange,
                                        createInfo.swizzledFormat,
                                        RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                        device.TexOptLevel());  // dst

            device.CreateImageViewSrds(2, &imageView[0], pSrdTable);

            pSrdTable += 2 * SrdDwordAlignment();
            memcpy(pSrdTable, constData, sizeof(constData));

            // Execute the dispatch.
            pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
        } // end loop through all the slices
    }

    // We have to mark this mip level as actually being DCC decompressed
    image.UpdateDccStateMetaData(pCmdStream, range, false, engineType, PredDisable);

    // Make sure that the decompressed image data has been written before we start fixing up DCC memory.
    pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
    pComputeCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuffer->TimestampGpuVirtAddr(), pComputeCmdSpace);
    pComputeCmdStream->CommitCommands(pComputeCmdSpace);

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    if (IsGfx10Plus(device))
    {
        // The SRD is setup so that writing the decompressed value into the destination image will automagically
        // update DCC memory with the correct initial value.  So there's no need to do it again.
    }
    else
    {
        // Put DCC memory itself back into a "fully decompressed" state, since only compressed fragments needed
        // to be written, as initialization of dcc memory will write to uncompressed fragment and hence
        // they don't need to be written here. Change from init to fastclear.
        ClearDcc(pCmdBuffer, pCmdStream, image, range, Gfx9Dcc::InitialValue, DccClearPurpose::FastClear);
    }

    // And let the DCC fixup finish as well
    pComputeCmdSpace  = pComputeCmdStream->ReserveCommands();
    pComputeCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuffer->TimestampGpuVirtAddr(), pComputeCmdSpace);
    pComputeCmdStream->CommitCommands(pComputeCmdSpace);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    if (range.numMips > 0)
    {
        const bool  supportsComputePath = image.SupportsComputeDecompress(range);
        const auto& settings            = m_pDevice->Settings();
        const auto* pSubResInfo         = image.Parent()->SubresourceInfo(range.startSubres);
        const auto& addrSettings        = image.GetAddrSettings(pSubResInfo);

        if ((pCmdBuffer->GetEngineType() == EngineTypeCompute)           ||
            AddrMgr2::IsSwizzleModeComputeOnly(addrSettings.swizzleMode) ||
            (supportsComputePath && (TestAnyFlagSet(Image::UseComputeExpand, UseComputeExpandAlways))))
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
                             *pMsaaState,
                             pQuadSamplePattern,
                             RpmGfxPipeline::DccDecompress,
                             pGpuMem,
                             metaDataOffset);
        }

        // We have to mark this mip level as actually being DCC decompressed
        image.UpdateDccStateMetaData(pCmdStream, range, false, pCmdBuffer->GetEngineType(), PredDisable);

        // Clear the FCE meta data over the given range because a DCC decompress implies a FCE. Note that it doesn't
        // matter that we're using the truncated range here because we mips that don't use DCC shouldn't need a FCE
        // because they must be slow cleared.
        if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
        {
            const Pm4Predicate packetPredicate =
                static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);
            uint32* pCmdSpace = pCmdStream->ReserveCommands();
            pCmdSpace = image.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);
            pCmdStream->CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
// Performs a fast color clear eliminate blt on the provided Image. Returns true if work (blt) is submitted to GPU.
bool RsrcProcMgr::FastClearEliminate(
    GfxCmdBuffer*                pCmdBuffer,
    Pal::CmdStream*              pCmdStream,
    const Image&                 image,
    const IMsaaState*            pMsaaState,
    const MsaaQuadSamplePattern* pQuadSamplePattern,
    const SubresRange&           range
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    const bool    alwaysFce    = TestAnyFlagSet(m_pDevice->Settings().alwaysDecompress, DecompressFastClear);

    const GpuMemory* pGpuMem = nullptr;
    gpusize metaDataOffset = alwaysFce ? 0 : image.GetFastClearEliminateMetaDataOffset(range.startSubres);
    if (metaDataOffset)
    {
        pGpuMem = image.Parent()->GetBoundGpuMemory().Memory();
        metaDataOffset += image.Parent()->GetBoundGpuMemory().Offset();
    }

    // Execute a generic CB blit using the fast-clear Eliminate pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range, *pMsaaState,
                     pQuadSamplePattern, RpmGfxPipeline::FastClearElim, pGpuMem, metaDataOffset);
    // Clear the FCE meta data over the given range because those mips must now be FCEd.
    if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const Pm4Predicate packetPredicate =
            static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

        pCmdSpace = image.UpdateFastClearEliminateMetaData(pCmdBuffer, range, 0, packetPredicate, pCmdSpace);

        pCmdStream->CommitCommands(pCmdSpace);
    }

    return true;
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(clearRange.numPlanes == 1);
#endif

    const auto*  pParent            = dstImage.Parent();
    const auto*  pFmask             = dstImage.GetFmask();
    const auto&  fMaskAddrOutput    = pFmask->GetAddrOutput();
    const auto&  imageCreateInfo    = pParent->GetImageCreateInfo();
    const auto   pPipeline          = GetPipeline(RpmComputePipeline::ClearImage2d);
    uint32       threadsPerGroup[3] = {};

    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // NOTE: MSAA Images do not support multiple mipmpap levels, so we can make some assumptions here.
    PAL_ASSERT(imageCreateInfo.mipLevels == 1);
    PAL_ASSERT((clearRange.startSubres.mipLevel == 0) && (clearRange.numMips == 1));

    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // The shader will saturate the fmask value to the fmask view format's size. so we mask-off clearValue to fit it.
    const uint64 validBitsMask    = (1ULL << fMaskAddrOutput.bpp) - 1ULL;
    const uint64 maskedClearValue = clearValue & validBitsMask;

    const uint32  userData[] =
    {
        // color
        LowPart(maskedClearValue), HighPart(maskedClearValue), 0, 0,
        // (x,y) offset, (width,height)
        0, 0, imageCreateInfo.extent.width, imageCreateInfo.extent.height,
        // ignored
        0, 0, 0
    };

    const uint32  DataDwords = NumBytesToNumDwords(sizeof(userData));

    // Create an embedded user-data table and bind it to user data 0.
    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                               SrdDwordAlignment() + DataDwords,
                                                               SrdDwordAlignment(),
                                                               PipelineBindPoint::Compute,
                                                               0);

    // We need an image view for the fMask surface
    FmaskViewInfo fmaskBufferView        = { };
    fmaskBufferView.pImage               = pParent;
    fmaskBufferView.baseArraySlice       = clearRange.startSubres.arraySlice;
    fmaskBufferView.arraySize            = clearRange.numSlices;
    fmaskBufferView.flags.shaderWritable = 1;

    FmaskViewInternalInfo fmaskViewInternal = {};
    fmaskViewInternal.flags.fmaskAsUav = 1;

    m_pDevice->CreateFmaskViewSrdsInternal(1, &fmaskBufferView, &fmaskViewInternal, pSrdTable);
    pSrdTable += SrdDwordAlignment();
    memcpy(pSrdTable, &userData[0], sizeof(userData));

    // And hit the "go" button...
    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(imageCreateInfo.extent.width,  threadsPerGroup[0]),
                            RpmUtil::MinThreadGroups(imageCreateInfo.extent.height, threadsPerGroup[1]),
                            RpmUtil::MinThreadGroups(clearRange.numSlices,          threadsPerGroup[2]));
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif
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
        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        const uint32 threadGroupsX = RpmUtil::MinThreadGroups(createInfo.extent.width,  threadsPerGroup[0]);
        const uint32 threadGroupsY = RpmUtil::MinThreadGroups(createInfo.extent.height, threadsPerGroup[1]);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        SubresRange  viewRange = { range.startSubres, 1, 1 };
#else
        SubresRange  viewRange = { range.startSubres, 1, 1, 1 };
#endif
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
                                        device.TexOptLevel());
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
            pCmdBuffer->CmdDispatch(threadGroupsX, threadGroupsY, 1);
        }
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif
    // Only MSAA Images should ever need an FMask Decompress and they only support a single mipmap level.
    PAL_ASSERT((range.startSubres.mipLevel == 0) && (range.numMips == 1));

    // Execute a generic CB blit using the appropriate FMask Decompress pipeline.
    GenericColorBlit(pCmdBuffer, *image.Parent(), range, *pMsaaState,
                     pQuadSamplePattern, RpmGfxPipeline::FmaskDecompress, nullptr, 0);

    // Clear the FCE meta data over the given range because an FMask decompress implies a FCE.
    if (image.GetFastClearEliminateMetaDataAddr(range.startSubres) != 0)
    {
        uint32* pCmdSpace = pCmdStream->ReserveCommands();

        const Pm4Predicate packetPredicate =
            static_cast<Pm4Predicate>(pCmdBuffer->GetGfxCmdBufState().flags.packetPredicate);

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

    PAL_ASSERT((m_pDevice->Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9) ||
               (m_pDevice->Parent()->ChipProperties().gfx9.validPaScTileSteeringOverride));

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    else if (clearRange.numPlanes == 2)
    {
        clearMask = HtilePlaneDepth | HtilePlaneStencil;
    }
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    else if (clearRange.startSubres.aspect == ImageAspect::Depth)
#else
    else if (pParentImg->IsDepthPlane(clearRange.startSubres.plane))
#endif
    {
        clearMask = HtilePlaneDepth;
    }
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    else if ((clearRange.startSubres.aspect == ImageAspect::Stencil) && (pHtile->TileStencilDisabled() == false))
#else
    else if (pParentImg->IsStencilPlane(clearRange.startSubres.plane) && (pHtile->TileStencilDisabled() == false))
#endif
    {
        clearMask = HtilePlaneStencil;
    }

    return clearMask;
}

// =====================================================================================================================
// For copies to non-local destinations, it is faster (although very unintuitive) to disable all but one of the RBs.
// All of the RBs banging away on the PCIE bus produces more traffic than the write-combiner can efficiently handle,
// so if we detect a write to non-local memory here, then disable RBs for the duration of the copy.  They will get
// restored in the HwlEndGraphicsCopy function.
uint32 Gfx9RsrcProcMgr::HwlBeginGraphicsCopy(
    Pal::GfxCmdBuffer*           pCmdBuffer,
    const Pal::GraphicsPipeline* pPipeline,
    const Pal::Image&            dstImage,
    uint32                       bpp
    ) const
{
    Pal::CmdStream*const pCmdStream   = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    const GpuMemory*     pGpuMem      = dstImage.GetBoundGpuMemory().Memory();
    const auto&          coreSettings = m_pDevice->Parent()->Settings();
    const auto&          settings     = m_pDevice->Settings();
    uint32               modifiedMask = 0;

    if (pGpuMem != nullptr)
    {
        const GpuHeap preferredHeap = pGpuMem->PreferredHeap();

        if ((((preferredHeap == GpuHeapGartUswc) || (preferredHeap == GpuHeapGartCacheable)) ||
             pGpuMem->IsPeer()) &&
            (coreSettings.nonlocalDestGraphicsCopyRbs >= 0))
        {
            const auto&  chipProps = m_pDevice->Parent()->ChipProperties().gfx9;

            // A setting of zero RBs implies that the driver should use the optimal number.  For now, assume the
            // optimal number is one.  Also don't allow more RBs than actively exist.
            const uint32 numNeededTotalRbs = Min(Max(1u,
                                                     static_cast<uint32>(coreSettings.nonlocalDestGraphicsCopyRbs)),
                                                 chipProps.numActiveRbs);

            // We now have the total number of RBs that we need...  However, the ASIC divides RBs up between the
            // various SEs, so calculate how many SEs we need to involve and how many RBs each SE should use.
            const uint32  numNeededSes      = Max(1u, (numNeededTotalRbs / chipProps.activeNumRbPerSe));
            const uint32  numNeededRbsPerSe = numNeededTotalRbs / numNeededSes;

            // Write the new register value to the command stream
            regPA_SC_TILE_STEERING_OVERRIDE  paScTileSteeringOverride = {};
            paScTileSteeringOverride.bits.ENABLE         = 1;
            paScTileSteeringOverride.most.NUM_SE        = Log2(numNeededSes);
            paScTileSteeringOverride.most.NUM_RB_PER_SE = Log2(numNeededRbsPerSe);
            CommitBeginEndGfxCopy(pCmdStream, paScTileSteeringOverride.u32All);

            // Let EndGraphcisCopy know that it has work to do
            modifiedMask |= PaScTileSteeringOverrideMask;
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
void Gfx9RsrcProcMgr::HwlEndGraphicsCopy(
    Pal::CmdStream* pCmdStream,
    uint32          restoreMask
    ) const
{
    // Did HwlBeginGraphicsCopy do anything? If not, there's nothing to do here.
    if (TestAnyFlagSet(restoreMask, PaScTileSteeringOverrideMask))
    {
        // Writing a zero to the PA_SC_TILE_STEERING_OVERRIDE register clears the "enable" bit which restores the
        // full number of RBs and SEs for regular rendering.
        CommitBeginEndGfxCopy(pCmdStream, 0);
    }
}

// ====================================================================================================================
// Returns the maximum size that would be copied for the specified sub-resource-id via the SRD used by the default
// copy image<->memory functions.
Extent3d RsrcProcMgr::GetCopyViaSrdCopyDims(
    const Pal::Image&  image,
    const SubresId&    subResId,
    bool               includePadding)
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId  baseMipSubResId  = { subResId.aspect, 0, subResId.arraySlice };
#else
    const SubresId  baseMipSubResId  = { subResId.plane, 0, subResId.arraySlice };
#endif
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
    hwCopyDims.width  = Max(1u, programmedExtent.width  >> subResId.mipLevel);
    hwCopyDims.height = Max(1u, programmedExtent.height >> subResId.mipLevel);
    hwCopyDims.depth  = Max(1u, programmedExtent.depth  >> subResId.mipLevel);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId  baseMipSubResId  = { subresId.aspect, 0, subresId.arraySlice };
#else
    const SubresId  baseMipSubResId  = { subresId.plane, 0, subresId.arraySlice };
#endif
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
}

// ====================================================================================================================
// Returns true if the CmdCopyMemoryFromToImageViaPixels function needs to be used
bool RsrcProcMgr::UsePixelCopy(
    const Pal::Image&             image,
    const MemoryImageCopyRegion&  region)
{
    const Extent3d  hwCopyDims = GetCopyViaSrdCopyDims(image, region.imageSubres, true);

    // If the default implementation copy dimensions did not cover the region specified by this region, then
    // we need to copy the remaining pixels the slow way.
    const bool  usePixelCopy = ((hwCopyDims.width  < (region.imageOffset.x + region.imageExtent.width))  ||
                                (hwCopyDims.height < (region.imageOffset.y + region.imageExtent.height)) ||
                                (hwCopyDims.depth  < (region.imageOffset.z + region.imageExtent.depth)));

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        dstImage.IsAspectValid(ImageAspect::Stencil))
#else
        dstImage.HasStencilPlane())
#endif
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
        for (uint32  regionIdx = 0; regionIdx < regionCount; regionIdx++)
        {
            const auto&  region = pRegions[regionIdx];

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            if (pRegions[regionIdx].imageSubres.aspect == ImageAspect::Stencil)
#else
            if (dstImage.IsStencilPlane(pRegions[regionIdx].imageSubres.plane))
#endif
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
    const auto&  createInfo = srcImage.GetImageCreateInfo();

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
        bool  issuedCsPartialFlush = false;

        for (uint32  regionIdx = 0; regionIdx < regionCount; regionIdx++)
        {
            const auto&  region = pRegions[regionIdx];

            if (UsePixelCopy(srcImage, region))
            {
                // We have to wait for the compute shader invoked above to finish...  Otherwise, it will be writing
                // zeroes into the destination memory that correspond to pixels that it couldn't read.  This only
                // needs to be done once before the first pixel-level copy.
                if (issuedCsPartialFlush == false)
                {
                    Pal::CmdStream*  pPalCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);
                    CmdStream*       pGfxCmdStream = static_cast<CmdStream*>(pPalCmdStream);
                    uint32*          pCmdSpace     = pGfxCmdStream->ReserveCommands();
                    const EngineType engineType    = pGfxCmdStream->GetEngineType();

                    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(engineType, pCmdBuffer->TimestampGpuVirtAddr(), pCmdSpace);

                    // Two things can happen next. We will either be copying the leftover pixels with CPDMA or with
                    // more CS invocations. CPDMA is preferred, but we will fallback on CS if the copy is too large
                    // (unlikely in this case), or if one of the resources has virtual memory (e.g. sparse).
                    // We check the latter here and assume the former won't happen.
                    const auto pImageMem  = srcImage.GetBoundGpuMemory().Memory();
                    const bool needCsCopy = pImageMem->IsVirtual() || dstGpuMemory.IsVirtual();

                    if (needCsCopy)
                    {
                        // Even though we have waited for the CS to finish, we may still run into a write after write
                        // hazard. We need to flush and invalidate the L2 cache as well.
                        AcquireMemInfo acquireInfo = {};
                        acquireInfo.engineType  = engineType;
                        acquireInfo.tcCacheOp   = TcCacheOp::WbInvL1L2;
                        acquireInfo.baseAddress = dstGpuMemory.Desc().gpuVirtAddr;
                        acquireInfo.sizeBytes   = dstGpuMemory.Desc().size;

                        pCmdSpace += m_cmdUtil.BuildAcquireMem(acquireInfo, pCmdSpace);
                    }

                    pGfxCmdStream->CommitCommands(pCmdSpace);

                    issuedCsPartialFlush = true;
                }

                CmdCopyMemoryFromToImageViaPixels(pCmdBuffer, srcImage, dstGpuMemory, region, includePadding, true);
            }
        } // end loop through copy regions
    } // end check for trivial case
}

// ====================================================================================================================
// Executes a compute shader for fast-clearing the specified image / range.  The image's associated DCC memory is
// updated to "clearCode" for all bytes corresponding to "clearRange".
void Gfx9RsrcProcMgr::ClearDccCompute(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose,
    const uint32*      pPackedClearColor
    ) const
{
    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const Pal::Image*      pPalImage               = dstImage.Parent();
    const Pal::Device*     pDevice                 = pPalImage->GetDevice();
    const Gfx9PalSettings& settings                = GetGfx9Settings(*pDevice);
    const auto&            createInfo              = pPalImage->GetImageCreateInfo();
    // Allow optimized fast clears for either single-sample images, or MSAA images with CMask and FMaskData
    const bool             allowOptimizedFastClear = (createInfo.fragments == 1) || (dstImage.HasFmaskData());
    // For now just find out here if this resource can do Optimized DCC clear. Since OptimizedFastClear only
    // clears compressed Fragments and doesn't touch uncompressed fragments, the optimized path must only be
    // used for fast clear.
    const bool optimizedPrecheck = ((clearPurpose == DccClearPurpose::FastClear) &&
                                    TestAnyFlagSet(settings.optimizedFastClear, Gfx9OptimizedFastClearColorDcc) &&
                                    allowOptimizedFastClear);

    SubresRange splitRange = clearRange;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    splitRange.numPlanes   = 1;
    for (;
         splitRange.startSubres.plane < (clearRange.startSubres.plane + clearRange.numPlanes);
         splitRange.startSubres.plane++)
#endif
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const auto* pDcc                   = dstImage.GetDcc(splitRange.startSubres.aspect);
#else
        const auto* pDcc                   = dstImage.GetDcc(splitRange.startSubres.plane);
#endif
        const auto& dccMipInfo             = pDcc->GetAddrMipInfo(splitRange.startSubres.mipLevel);

        // Optimized DCC clear works for all kinds of color surfaces 2D/3D/mips/singlesample/multisample etc. except
        // those whose metadata is part of miptail
        if (optimizedPrecheck && (dccMipInfo.inMiptail == 0))
        {
            DoOptimizedFastClear(pCmdBuffer, pCmdStream, dstImage, splitRange, clearCode, clearPurpose);
        }
        else
        {
            DoFastClear(pCmdBuffer, pCmdStream, dstImage, splitRange, clearCode, clearPurpose);
        }
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// ====================================================================================================================
// Executes a Optimized compute shader for fast-clearing the specified image / range. The image's associated Htile
// memory is updated to "htileValue" for all bytes.
void Gfx9RsrcProcMgr::ClearHtileAllBytes(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             htileValue
    ) const
{
    const Pal::Image*      pPalImage       = dstImage.Parent();
    const Pal::Device*     pDevice         = pPalImage->GetDevice();
    const Gfx9PalSettings& settings        = GetGfx9Settings(*pDevice);
    const auto&            createInfo      = pPalImage->GetImageCreateInfo();
    const auto*            pHtile          = dstImage.GetHtile();
    const auto&            hTileAddrOutput = pHtile->GetAddrOutput();

    // Get Compute Shader constants based on Meta Equation
    const auto&  metaClearConstEqParam = dstImage.GetMetaDataClearConst(MetaDataHtile);

    const uint32 sliceStart  = range.startSubres.arraySlice;
    const uint32 numSlices   = range.numSlices;

    // Check if meta is interleaved, if it is not we can directly fill the surface with
    // htileValues. In this case addressing is Metablock[all], CombinedOffset[all]
    if ((metaClearConstEqParam.metaInterleaved == false) && (createInfo.mipLevels == 1))
    {
        // Bind the GFX9 Fill 4x4 Dword pipeline
        uint32 threadsPerGroup[3] = {};

        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9Fill4x4Dword);

        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // On GFX9, we create a single view of the hTile buffer that points to the base mip level.  It's
        // up to the equation to "find" each mip level and slice from that base location.
        BufferViewInfo hTileSurfBufferView = {};
        pHtile->BuildSurfBufferView(&hTileSurfBufferView);
        // Make it Structured
        hTileSurfBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        hTileSurfBufferView.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        hTileSurfBufferView.stride = sizeof(uint32)* 4;

        if (sliceStart > 0)
        {
            uint32 metaOffsetInBytes     = hTileAddrOutput.sliceSize * sliceStart;
            hTileSurfBufferView.gpuAddr += metaOffsetInBytes;
            PAL_ASSERT(hTileSurfBufferView.range > metaOffsetInBytes);
            hTileSurfBufferView.range   -= metaOffsetInBytes;
        }

        PAL_ASSERT((hTileSurfBufferView.range & 0xf) == 0);

        uint32 clearBytes = hTileAddrOutput.sliceSize * numSlices;
        // Divide by 16 since we clear 4 Dwords in each compute thread
        uint32 metaThreadX = clearBytes >> 4;

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &hTileSurfBufferView, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            htileValue,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the htile buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information related to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, 1, 1);
    }
    else
    {
        // In this case metablock is mixed in memory which means it is split into metaBlock[Hi] and metaBlock[Lo]
        PAL_ASSERT(metaClearConstEqParam.metaInterleaved);

        // Bind the optimized dcc pipeline which clears 4Dwords of data given a meta data addressing
        // parameters
        uint32        threadsPerGroup[3] = {};

        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9ClearDccOptimized2d);

        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the htile surface itself. This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo hTileSurfBufferView = {};
        pHtile->BuildSurfBufferView(&hTileSurfBufferView);
        // Make it Structured
        hTileSurfBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        hTileSurfBufferView.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        hTileSurfBufferView.stride = sizeof(uint32)* 4;

        uint32 metaBlockOffset = 0;
        uint32 metaThreadX     = 0;
        uint32 metaThreadY     = 1;
        uint32 metaThreadZ     = 1;

        uint32 mipChainPitchInMetaBlk  = 0;
        uint32 mipChainHeightInMetaBlk = 0;
        uint32 mipSlicePitchInMetaBlk  = 0;

        if (createInfo.mipLevels == 1)
        {
            // Check if we need to add any offset to our metablock address calculation
            metaBlockOffset   = sliceStart * hTileAddrOutput.metaBlkNumPerSlice;
            uint32 clearBytes = hTileAddrOutput.sliceSize * numSlices;

            // Divide by 16 since we clear 4 Dwords in each compute thread
            metaThreadX = clearBytes >> 4;
        }
        else
        {
            mipChainPitchInMetaBlk  = hTileAddrOutput.pitch / hTileAddrOutput.metaBlkWidth;
            mipChainHeightInMetaBlk = hTileAddrOutput.height / hTileAddrOutput.metaBlkHeight;

            PAL_ASSERT((mipChainPitchInMetaBlk * mipChainHeightInMetaBlk) == hTileAddrOutput.metaBlkNumPerSlice);

            const auto&   hTileMipInfo = pHtile->GetAddrMipInfo(range.startSubres.mipLevel);

            uint32 mipStartZInBlk = hTileMipInfo.startZ;
            uint32 mipStartYInBlk = hTileMipInfo.startY / hTileAddrOutput.metaBlkHeight;
            uint32 mipStartXInBlk = hTileMipInfo.startX / hTileAddrOutput.metaBlkWidth;

            metaBlockOffset = (mipStartZInBlk + sliceStart) * hTileAddrOutput.metaBlkNumPerSlice +
                               mipStartYInBlk * mipChainPitchInMetaBlk +
                               mipStartXInBlk;

            mipSlicePitchInMetaBlk = mipChainPitchInMetaBlk * mipChainHeightInMetaBlk;

            uint32 metaBlkSize = hTileAddrOutput.sliceSize / hTileAddrOutput.metaBlkNumPerSlice;

            metaThreadX = (hTileMipInfo.width / hTileAddrOutput.metaBlkWidth) * metaBlkSize >> 4;
            metaThreadY = hTileMipInfo.height / hTileAddrOutput.metaBlkHeight;
            metaThreadZ = numSlices;
        }

        PAL_ASSERT((hTileSurfBufferView.range & 0xf) == 0);

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &hTileSurfBufferView, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            htileValue,
            // start cb0[1]
            metaClearConstEqParam.metablockSizeLog2,
            metaClearConstEqParam.metablockSizeLog2BitMask,
            metaClearConstEqParam.combinedOffsetLowBits,
            metaClearConstEqParam.combinedOffsetLowBitsMask,
            // start cb0[2]
            metaClearConstEqParam.metaBlockLsb,
            metaClearConstEqParam.metaBlockLsbBitMask,
            metaClearConstEqParam.metaBlockHighBitShift,
            metaClearConstEqParam.combinedOffsetHighBitShift,
            // start cb0[3]
            metaBlockOffset,
            mipChainPitchInMetaBlk,
            mipSlicePitchInMetaBlk,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the DCC buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information realted to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        uint32 numThreadGroupsY = 1;
        uint32 numThreadGroupsZ = 1;

        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
            numThreadGroupsY = RpmUtil::MinThreadGroups(metaThreadY, threadsPerGroup[1]);
            numThreadGroupsZ = RpmUtil::MinThreadGroups(metaThreadZ, threadsPerGroup[2]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
    }
}

// ====================================================================================================================
// Executes a Optimized compute shader for fast-clearing the specified image / range. The image's associated Htile
// memory is updated to "htileValue" for selected bytes. Bits specified by htileMask are kept.
void Gfx9RsrcProcMgr::ClearHtileSelectedBytes(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             htileValue,
    uint32             htileMask
    ) const
{
    const Pal::Image*      pPalImage       = dstImage.Parent();
    const Pal::Device*     pDevice         = pPalImage->GetDevice();
    const Gfx9PalSettings& settings        = GetGfx9Settings(*pDevice);
    const auto&            createInfo      = pPalImage->GetImageCreateInfo();
    const auto*            pHtile          = dstImage.GetHtile();
    const auto&            hTileAddrOutput = pHtile->GetAddrOutput();

    // Get Compute Shader constants based on Meta Equation
    const auto&  metaClearConstEqParam = dstImage.GetMetaDataClearConst(MetaDataHtile);

    const uint32 sliceStart = range.startSubres.arraySlice;
    const uint32 numSlices  = range.numSlices;

    // Check if meta is interleaved, if it is not we can directly fill the surface with
    // htileValues and keep bits selected by htileMask. In this case addressing is
    // Metablock[all], CombinedOffset[all]
    if ((metaClearConstEqParam.metaInterleaved == false) && (createInfo.mipLevels == 1))
    {
        // Bind the simple pipeline since all offsetbits are under metablock bits
        uint32 threadsPerGroup[3] = {};

        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9ClearHtileFast);

        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // On GFX9, we create a single view of the hTile buffer that points to the base mip level.  It's
        // up to the equation to "find" each mip level and slice from that base location.
        BufferViewInfo hTileSurfBufferView = {};
        pHtile->BuildSurfBufferView(&hTileSurfBufferView);
        // Make it Structured
        hTileSurfBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        hTileSurfBufferView.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        hTileSurfBufferView.stride = sizeof(uint32)* 4;

        if (sliceStart > 0)
        {
            uint32 metaOffsetInBytes = hTileAddrOutput.sliceSize * sliceStart;
            hTileSurfBufferView.gpuAddr += metaOffsetInBytes;
            PAL_ASSERT(hTileSurfBufferView.range > metaOffsetInBytes);
            hTileSurfBufferView.range -= metaOffsetInBytes;
        }

        PAL_ASSERT((hTileSurfBufferView.range & 0xf) == 0);

        uint32 clearBytes = hTileAddrOutput.sliceSize * numSlices;
        // Divide by 16 since we clear 4 Dwords in each compute thread
        uint32 metaThreadX = clearBytes >> 4;

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &hTileSurfBufferView, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            htileValue & htileMask,
            ~htileMask,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the htile buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information realted to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, 1, 1);
    }
    else
    {
        // In this case metablock is mixed in memory which means it is split into metaBlock[Hi] and metaBlock[Lo]
        PAL_ASSERT(metaClearConstEqParam.metaInterleaved);

        uint32        threadsPerGroup[3] = {};

        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9ClearHtileOptimized2d);

        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the htile surface itself.  This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo hTileSurfBufferView = {};
        pHtile->BuildSurfBufferView(&hTileSurfBufferView);
        // Make it Structured
        hTileSurfBufferView.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        hTileSurfBufferView.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        hTileSurfBufferView.stride = sizeof(uint32)* 4;

        uint32 metaBlockOffset = 0;
        uint32 metaThreadX     = 0;
        uint32 metaThreadY     = 1;
        uint32 metaThreadZ     = 1;

        uint32 mipChainPitchInMetaBlk  = 0;
        uint32 mipChainHeightInMetaBlk = 0;
        uint32 mipSlicePitchInMetaBlk  = 0;

        if (createInfo.mipLevels == 1)
        {
            // Check if we need to add any offset to our metablock address calculation
            metaBlockOffset   = sliceStart * hTileAddrOutput.metaBlkNumPerSlice;
            uint32 clearBytes = hTileAddrOutput.sliceSize * numSlices;

            // Divide by 16 since we clear 4 Dwords in each compute thread
            metaThreadX = clearBytes >> 4;
        }
        else
        {
            // This path is not yet tested since Microbench doesn't expose it and neither does apps I tested
            // But this is expected to work. So, for now just put an assert.
            PAL_NOT_TESTED();

            mipChainPitchInMetaBlk  = hTileAddrOutput.pitch / hTileAddrOutput.metaBlkWidth;
            mipChainHeightInMetaBlk = hTileAddrOutput.height / hTileAddrOutput.metaBlkHeight;

            PAL_ASSERT((mipChainPitchInMetaBlk * mipChainHeightInMetaBlk) == hTileAddrOutput.metaBlkNumPerSlice);

            const auto&   hTileMipInfo = pHtile->GetAddrMipInfo(range.startSubres.mipLevel);

            uint32 mipStartZInBlk = hTileMipInfo.startZ;
            uint32 mipStartYInBlk = hTileMipInfo.startY / hTileAddrOutput.metaBlkHeight;
            uint32 mipStartXInBlk = hTileMipInfo.startX / hTileAddrOutput.metaBlkWidth;

            metaBlockOffset = (mipStartZInBlk + sliceStart) * hTileAddrOutput.metaBlkNumPerSlice +
                               mipStartYInBlk * mipChainPitchInMetaBlk +
                               mipStartXInBlk;

            mipSlicePitchInMetaBlk = mipChainPitchInMetaBlk * mipChainHeightInMetaBlk;

            uint32 metaBlkSize = hTileAddrOutput.sliceSize / hTileAddrOutput.metaBlkNumPerSlice;

            metaThreadX = (hTileMipInfo.width / hTileAddrOutput.metaBlkWidth) * metaBlkSize >> 4;
            metaThreadY = hTileMipInfo.height / hTileAddrOutput.metaBlkHeight;
            metaThreadZ = numSlices;
        }

        PAL_ASSERT((hTileSurfBufferView.range & 0xf) == 0);

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &hTileSurfBufferView, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            htileValue & htileMask,
            ~htileMask,
            // start cb0[1]
            metaClearConstEqParam.metablockSizeLog2,
            metaClearConstEqParam.metablockSizeLog2BitMask,
            metaClearConstEqParam.combinedOffsetLowBits,
            metaClearConstEqParam.combinedOffsetLowBitsMask,
            // start cb0[2]
            metaClearConstEqParam.metaBlockLsb,
            metaClearConstEqParam.metaBlockLsbBitMask,
            metaClearConstEqParam.metaBlockHighBitShift,
            metaClearConstEqParam.combinedOffsetHighBitShift,
            // start cb0[3]
            metaBlockOffset,
            mipChainPitchInMetaBlk,
            mipSlicePitchInMetaBlk,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the DCC buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information realted to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        uint32 numThreadGroupsY = 1;
        uint32 numThreadGroupsZ = 1;

        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
            numThreadGroupsY = RpmUtil::MinThreadGroups(metaThreadY, threadsPerGroup[1]);
            numThreadGroupsZ = RpmUtil::MinThreadGroups(metaThreadZ, threadsPerGroup[2]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
    }
}

// ====================================================================================================================
// Executes a compute shader for fast-clearing the specified image / range.  The image's associated DCC
// memory is updated to "clearCode" for all bytes corresponding to "clearRange". For best performance please use
// DoOptimizedFastClear.
void Gfx9RsrcProcMgr::DoFastClear(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(clearRange.numPlanes == 1);
#endif

    const Pal::Image*      pPalImage     = dstImage.Parent();
    const Pal::Device*     pDevice       = pPalImage->GetDevice();
    const Gfx9PalSettings& settings      = GetGfx9Settings(*pDevice);
    const auto&            createInfo    = pPalImage->GetImageCreateInfo();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*            pDcc          = dstImage.GetDcc(clearRange.startSubres.aspect);
#else
    const auto*            pDcc          = dstImage.GetDcc(clearRange.startSubres.plane);
#endif
    PAL_ASSERT(pDcc->HasMetaEqGenerator());
    const auto*            pEqGenerator  = pDcc->GetMetaEqGenerator();
    const auto&            dccAddrOutput = pDcc->GetAddrOutput();
    const auto*            pGfxDevice    = static_cast<const Device*>(pDevice->GetGfxDevice());
    const bool             is3dImage     = (createInfo.imageType == ImageType::Tex3d);

    const uint32           log2MetaBlkWidth  = Log2(dccAddrOutput.metaBlkWidth);
    const uint32           log2MetaBlkHeight = Log2(dccAddrOutput.metaBlkHeight);
    const uint32           sliceSize         = (dccAddrOutput.pitch * dccAddrOutput.height) >>
                                               (log2MetaBlkWidth + log2MetaBlkHeight);
    const uint32           effectiveSamples  = pDcc->GetNumEffectiveSamples(clearPurpose);

    const RpmComputePipeline  pipeline  = (is3dImage? RpmComputePipeline::Gfx9ClearDccSingleSample3d
                                          : ((effectiveSamples > 1) ? RpmComputePipeline::Gfx9ClearDccMultiSample2d
                                          : RpmComputePipeline::Gfx9ClearDccSingleSample2d));

    const auto*const pPipeline    = GetPipeline(pipeline);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32     pipeBankXor  = pEqGenerator->CalcPipeXorMask(clearRange.startSubres.aspect);
#else
    const uint32     pipeBankXor  = pEqGenerator->CalcPipeXorMask(clearRange.startSubres.plane);
#endif

    BufferSrd     bufferSrds[2] = {};
    uint32        xInc = 0;
    uint32        yInc = 0;
    uint32        zInc = 0;
    pDcc->GetXyzInc(&xInc, &yInc, &zInc);

    uint32        threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // Bind Compute Pipeline used for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // Create an SRD for the DCC surface itself.  This is a constant across all mip-levels as it's the shaders
    // job to calculate the proper address for each pixel of each mip level.
    BufferViewInfo bufferViewDccSurf = {};
    pDcc->BuildSurfBufferView(&bufferViewDccSurf);
    pDevice->CreateUntypedBufferViewSrds(1, &bufferViewDccSurf, &bufferSrds[0]);

    // Create an SRD for the DCC equation.  Again, this is a constant as there is only one equation
    BufferViewInfo bufferViewDccEq = {};
    pEqGenerator->BuildEqBufferView(&bufferViewDccEq);
    pDevice->CreateUntypedBufferViewSrds(1, &bufferViewDccEq, &bufferSrds[1]);

    // Clear each mip level invidually.  Create a constant buffer so the compute shader knows the
    // dimensions and location of each mip level.
    const uint32 lastMip = clearRange.startSubres.mipLevel + clearRange.numMips - 1;
    for (uint32 mipLevel = clearRange.startSubres.mipLevel; mipLevel <= lastMip; ++mipLevel)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId  subResId       = { clearRange.startSubres.aspect, mipLevel, 0 };
#else
        const SubresId  subResId       = { clearRange.startSubres.plane, mipLevel, 0 };
#endif
        const auto*     pSubResInfo    = pPalImage->SubresourceInfo(subResId);
        const auto&     dccMipInfo     = pDcc->GetAddrMipInfo(mipLevel);
        const uint32    mipLevelHeight = pSubResInfo->extentTexels.height;
        const uint32    mipLevelWidth  = pSubResInfo->extentTexels.width;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const uint32    depthToClear   = GetClearDepth(dstImage, clearRange, mipLevel);
#else
        const uint32    depthToClear   = GetClearDepth(dstImage,
                                                       clearRange.startSubres.plane,
                                                       clearRange.numSlices,
                                                       mipLevel);
#endif
        const uint32    firstSlice     = is3dImage ? dccMipInfo.startZ : clearRange.startSubres.arraySlice;

        const uint32 constData[] =
        {
            // start cb0[0]
            dccMipInfo.startX,
            dccMipInfo.startY,
            firstSlice,
            clearCode,
            // start cb0[1]
            log2MetaBlkWidth,
            log2MetaBlkHeight,
            Log2(dccAddrOutput.metaBlkDepth),
            dccAddrOutput.pitch >> log2MetaBlkWidth,
            // start cb0[2]
            mipLevelWidth,
            mipLevelHeight,
            depthToClear,
            sliceSize,
            // start cb0[3]
            Log2(xInc),
            Log2(yInc),
            Log2(zInc),
            // start cb0[4]
            pipeBankXor,
            effectiveSamples
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the DCC buffer and DCC equation
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // And give the shader all kinds of useful dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        MetaDataDispatch(pCmdBuffer,
                         pDcc,
                         mipLevelWidth,
                         mipLevelHeight,
                         depthToClear,
                         threadsPerGroup);
    }
}

// ====================================================================================================================
// Executes a compute shader for initializing cmask. The cmask memory is cleared with initialValue in an optimized
// way. This path is best for performance.
void Gfx9RsrcProcMgr::DoOptimizedCmaskInit(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       image,
    const SubresRange& range,
    uint8              initValue
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    const Pal::Image*      pPalImage       = image.Parent();
    const Pal::Device*     pDevice         = pPalImage->GetDevice();
    const Gfx9PalSettings& settings        = GetGfx9Settings(*pDevice);
    const auto&            createInfo      = pPalImage->GetImageCreateInfo();
    const auto*            pCmask          = image.GetCmask();
    const auto&            cmaskAddrOutput = pCmask->GetAddrOutput();

    // Get Compute Shader constants based on Meta Equation
    const auto&  metaClearConstEqParam = image.GetMetaDataClearConst(MetaDataCmask);

    const uint32 sliceStart = range.startSubres.arraySlice;
    const uint32 numSlices  = range.numSlices;

    // clearColor is 32 bit data and we clear 4Dwords in one thread
    const uint32 clearColor = ExpandClearCodeToDword(initValue);

    // MSAA surfaces don't have mipmaps.
    PAL_ASSERT(createInfo.mipLevels == 1);

    // Check if meta is interleaved, if it is not we can directly fill the surface with
    // cmask Clear Color. In this case addressing is Metablock[all], CombinedOffset[all]
    if (metaClearConstEqParam.metaInterleaved == false)
    {
        // Bind the GFX9 Fill 4x4 Dword pipeline
        uint32 threadsPerGroup[3] = {};
        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9Fill4x4Dword);
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the cmask surface itself.  This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo bufferViewCmaskSurf = {};
        pCmask->BuildSurfBufferView(&bufferViewCmaskSurf);
        // Make it Structured
        bufferViewCmaskSurf.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        bufferViewCmaskSurf.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        bufferViewCmaskSurf.stride = sizeof(uint32) * 4;

        if (sliceStart > 0)
        {
            uint32 metaOffsetInBytes     = cmaskAddrOutput.sliceSize * sliceStart;
            bufferViewCmaskSurf.gpuAddr += metaOffsetInBytes;
            PAL_ASSERT(bufferViewCmaskSurf.range > metaOffsetInBytes);
            bufferViewCmaskSurf.range   -= metaOffsetInBytes;
        }

        PAL_ASSERT((bufferViewCmaskSurf.range & 0xf) == 0);

        uint32 clearBytes  = cmaskAddrOutput.sliceSize * numSlices;
        // Divide by 16 since we clear 4 Dwords in each compute thread
        uint32 metaThreadX = clearBytes >> 4;

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &bufferViewCmaskSurf, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            clearColor,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the cmask buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information related to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
        }
        pCmdBuffer->CmdDispatch(numThreadGroupsX, 1, 1);
    }
    else
    {
        // In this case metablock is mixed in memory which means it is split into metaBlock[Hi] and metaBlock[Lo]
        PAL_ASSERT(metaClearConstEqParam.metaInterleaved);

        // Bind the Optimized DCC Pipeline which writes 4 Dwords to destination memory
        uint32  threadsPerGroup[3] = {};
        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9ClearDccOptimized2d);
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the cmask surface itself.  This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo bufferViewCmaskSurf = {};
        pCmask->BuildSurfBufferView(&bufferViewCmaskSurf);
        // Make it Structured
        bufferViewCmaskSurf.swizzledFormat.format  = Pal::ChNumFormat::X32Y32Z32W32_Uint;
        bufferViewCmaskSurf.swizzledFormat.swizzle =
            {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        bufferViewCmaskSurf.stride = sizeof(uint32) * 4;

        // Check if we need to add any offset to our metablock address calculation
        uint32 metaBlockOffset = sliceStart * cmaskAddrOutput.metaBlkNumPerSlice;
        uint32 clearBytes      = cmaskAddrOutput.sliceSize * numSlices;
        // Divide by 16 since we clear 4 Dwords in each compute thread
        uint32 metaThreadX     = clearBytes >> 4;

        PAL_ASSERT((bufferViewCmaskSurf.range & 0xf) == 0);

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &bufferViewCmaskSurf, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            clearColor,
            // start cb0[1]
            metaClearConstEqParam.metablockSizeLog2,
            metaClearConstEqParam.metablockSizeLog2BitMask,
            metaClearConstEqParam.combinedOffsetLowBits,
            metaClearConstEqParam.combinedOffsetLowBitsMask,
            // start cb0[2]
            metaClearConstEqParam.metaBlockLsb,
            metaClearConstEqParam.metaBlockLsbBitMask,
            metaClearConstEqParam.metaBlockHighBitShift,
            metaClearConstEqParam.combinedOffsetHighBitShift,
            // start cb0[3]
            metaBlockOffset,
            0,
            0,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the DCC buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information realted to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;

        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, 1, 1);
    }
}

// ====================================================================================================================
// Executes a Optimized compute shader for fast-clearing the specified image / range.  The image's associated DCC
// memory is updated to "clearCode" for all bytes corresponding to "clearRange". This is best for performance
void Gfx9RsrcProcMgr::DoOptimizedFastClear(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(clearRange.numPlanes == 1);
#endif

    const Pal::Image*      pPalImage     = dstImage.Parent();
    const Pal::Device*     pDevice       = pPalImage->GetDevice();
    const Gfx9PalSettings& settings      = GetGfx9Settings(*pDevice);
    const auto&            createInfo    = pPalImage->GetImageCreateInfo();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*            pDcc          = dstImage.GetDcc(clearRange.startSubres.aspect);
#else
    const auto*            pDcc          = dstImage.GetDcc(clearRange.startSubres.plane);
#endif
    const auto&            dccAddrOutput = pDcc->GetAddrOutput();
    const bool             is3dImage     = (createInfo.imageType == ImageType::Tex3d);

    // Get Compute Shader constants based on Meta Equation
    const auto&  metaClearConstEqParam = dstImage.GetMetaDataClearConst(MetaDataDcc);

    // Calculate Slice Start and End for Arrays or 3D image
    uint32 sliceStart = is3dImage ? 0 : clearRange.startSubres.arraySlice;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    uint32 sliceEnd   = GetClearDepth(dstImage, clearRange, clearRange.startSubres.mipLevel);
#else
    uint32 sliceEnd   = GetClearDepth(dstImage,
                                      clearRange.startSubres.plane,
                                      clearRange.numSlices,
                                      clearRange.startSubres.mipLevel);
#endif

    sliceStart = Pow2AlignDown(sliceStart, dccAddrOutput.metaBlkDepth);
    sliceEnd   = Pow2Align(sliceEnd, dccAddrOutput.metaBlkDepth);

    sliceStart /= dccAddrOutput.metaBlkDepth;
    sliceEnd   /= dccAddrOutput.metaBlkDepth;

    const uint32 numSlices = sliceEnd - sliceStart;

    // clearColor is 32 bit data and we clear 4Dwords in one thread
    const uint32 clearColor = ExpandClearCodeToDword(clearCode);

    // Check if meta is interleaved, if it is not we can directly fill the surface with
    // dcc Clear Color. In this case addressing is Metablock[all], CombinedOffset[all]
    if ((metaClearConstEqParam.metaInterleaved == false) && (createInfo.mipLevels == 1))
    {
        // Bind the GFX9 Fill 4x4 Dword pipeline
        uint32 threadsPerGroup[3]  = {};
        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9Fill4x4Dword);
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the DCC surface itself.  This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo bufferViewDccSurf = {};
        pDcc->BuildSurfBufferView(&bufferViewDccSurf);
        // Make it Structured
        bufferViewDccSurf.swizzledFormat.format  = ChNumFormat::X32Y32Z32W32_Uint;
        bufferViewDccSurf.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        bufferViewDccSurf.stride = sizeof(uint32) * 4;

        if (sliceStart > 0)
        {
            uint32 metaOffsetInBytes = dccAddrOutput.fastClearSizePerSlice * sliceStart;
            bufferViewDccSurf.gpuAddr += metaOffsetInBytes;
            PAL_ASSERT(bufferViewDccSurf.range > metaOffsetInBytes);
            bufferViewDccSurf.range   -= metaOffsetInBytes;
        }

        PAL_ASSERT((bufferViewDccSurf.range & 0xf) == 0);

        uint32 clearBytes = dccAddrOutput.fastClearSizePerSlice * numSlices;
        // Divide by 16 since we clear 4 Dwords in each compute thread
        uint32 metaThreadX = clearBytes >> 4;

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &bufferViewDccSurf, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            clearColor,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the dcc buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information related to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
        }
        pCmdBuffer->CmdDispatch(numThreadGroupsX, 1, 1);
    }
    else
    {
        // In this case metablock is mixed in memory which means it is split into metaBlock[Hi] and metaBlock[Lo]
        PAL_ASSERT(metaClearConstEqParam.metaInterleaved);

        // Bind the Optimized DCC Pipeline
        uint32  threadsPerGroup[3] = {};
        const auto*const pPipeline = GetPipeline(RpmComputePipeline::Gfx9ClearDccOptimized2d);
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        // Create an SRD for the DCC surface itself.  This is a constant across all mip-levels as it's the shaders
        // job to calculate the proper address for each pixel of each mip level.
        BufferViewInfo bufferViewDccSurf = {};
        pDcc->BuildSurfBufferView(&bufferViewDccSurf);
        // Make it Structured
        bufferViewDccSurf.swizzledFormat.format = Pal::ChNumFormat::X32Y32Z32W32_Uint;
        bufferViewDccSurf.swizzledFormat.swizzle =
           {ChannelSwizzle::X, ChannelSwizzle::Y, ChannelSwizzle::Z, ChannelSwizzle::W};
        bufferViewDccSurf.stride = sizeof(uint32)* 4;

        uint32 metaBlockOffset = 0;
        uint32 metaThreadX     = 0;
        uint32 metaThreadY     = 1;
        uint32 metaThreadZ     = 1;

        uint32 mipChainPitchInMetaBlk  = 0;
        uint32 mipChainHeightInMetaBlk = 0;
        uint32 mipSlicePitchInMetaBlk  = 0;

        if (createInfo.mipLevels == 1)
        {
            // Check if we need to add any offset to our metablock address calculation
            metaBlockOffset   = sliceStart * dccAddrOutput.metaBlkNumPerSlice;
            uint32 clearBytes = dccAddrOutput.fastClearSizePerSlice * numSlices;

            // Divide by 16 since we clear 4 Dwords in each compute thread
            metaThreadX = clearBytes >> 4;
        }
        else
        {
            mipChainPitchInMetaBlk  = dccAddrOutput.pitch / dccAddrOutput.metaBlkWidth;
            mipChainHeightInMetaBlk = dccAddrOutput.height / dccAddrOutput.metaBlkHeight;

            PAL_ASSERT((mipChainPitchInMetaBlk * mipChainHeightInMetaBlk) == dccAddrOutput.metaBlkNumPerSlice);

            const auto&  dccMipInfo = pDcc->GetAddrMipInfo(clearRange.startSubres.mipLevel);

            uint32 mipStartZInBlk = dccMipInfo.startZ / dccAddrOutput.metaBlkDepth;
            uint32 mipStartYInBlk = dccMipInfo.startY / dccAddrOutput.metaBlkHeight;
            uint32 mipStartXInBlk = dccMipInfo.startX / dccAddrOutput.metaBlkWidth;

            metaBlockOffset = (mipStartZInBlk + sliceStart) * dccAddrOutput.metaBlkNumPerSlice +
                               mipStartYInBlk * mipChainPitchInMetaBlk +
                               mipStartXInBlk;

            mipSlicePitchInMetaBlk = mipChainPitchInMetaBlk * mipChainHeightInMetaBlk;

            uint32 metaBlkSize = dccAddrOutput.fastClearSizePerSlice / dccAddrOutput.metaBlkNumPerSlice;

            metaThreadX = (dccMipInfo.width / dccAddrOutput.metaBlkWidth) * metaBlkSize >> 4;
            metaThreadY = dccMipInfo.height / dccAddrOutput.metaBlkHeight;
            metaThreadZ = numSlices;
        }

        PAL_ASSERT((bufferViewDccSurf.range & 0xf) == 0);

        // Create Buffer Srds (UAV in our case)
        BufferSrd     bufferSrds[1] = {};
        pDevice->CreateTypedBufferViewSrds(1, &bufferViewDccSurf, &bufferSrds[0]);

        // Constant data
        const uint32 constData[] =
        {
            // start cb0[0]
            clearColor,
            // start cb0[1]
            metaClearConstEqParam.metablockSizeLog2,
            metaClearConstEqParam.metablockSizeLog2BitMask,
            metaClearConstEqParam.combinedOffsetLowBits,
            metaClearConstEqParam.combinedOffsetLowBitsMask,
            // start cb0[2]
            metaClearConstEqParam.metaBlockLsb,
            metaClearConstEqParam.metaBlockLsbBitMask,
            metaClearConstEqParam.metaBlockHighBitShift,
            metaClearConstEqParam.combinedOffsetHighBitShift,
            // start cb0[3]
            metaBlockOffset,
            mipChainPitchInMetaBlk,
            mipSlicePitchInMetaBlk,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Supply the shader with a copy of our SRDs for the DCC buffer
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Pass to shader all kinds of Information realted to meta data equation
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        uint32 numThreadGroupsX = 1;
        uint32 numThreadGroupsY = 1;
        uint32 numThreadGroupsZ = 1;

        if (metaThreadX != 0)
        {
            numThreadGroupsX = RpmUtil::MinThreadGroups(metaThreadX, threadsPerGroup[0]);
            numThreadGroupsY = RpmUtil::MinThreadGroups(metaThreadY, threadsPerGroup[1]);
            numThreadGroupsZ = RpmUtil::MinThreadGroups(metaThreadZ, threadsPerGroup[2]);
        }

        pCmdBuffer->CmdDispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
    }
}

// ====================================================================================================================
// Executes a Optimized compute shader for fast-clearing the specified image / range. The image's associated Htile
// memory is updated to "htileValue" for all bytes except those bits which are kept by htileMask. This is best for
// performance but Doesn't do any post-equation synchronization.
void Gfx9RsrcProcMgr::DoOptimizedHtileFastClear(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             htileValue,
    uint32             htileMask  // set bits are kept
    ) const
{
    // Save the command buffer's state
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    if (htileMask == UINT_MAX)
    {
        // If the HTile mask has all bits set, we can use pipelines which fills 4 dwords of data
        ClearHtileAllBytes(pCmdBuffer, dstImage, range, htileValue);
    }
    else
    {
        // Otherwise clear only those bytes which are allowed. Keep those bits which are part of htileMask.
        ClearHtileSelectedBytes(pCmdBuffer, dstImage, range, htileValue, htileMask);
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Routine for executing the hTile equation on the specified range.  Doesn't do any post-equation synchronization.
void Gfx9RsrcProcMgr::ExecuteHtileEquation(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             htileValue,
    uint32             htileMask  // set bits are kept
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    const Pal::Image*      pParentImg = dstImage.Parent();
    const Pal::Device*     pParentDev = pParentImg->GetDevice();
    const Gfx9PalSettings& settings   = GetGfx9Settings(*pParentDev);

    const auto&     createInfo         = pParentImg->GetImageCreateInfo();
    const auto*     pBaseHtile         = dstImage.GetHtile();
    PAL_ASSERT(pBaseHtile->HasMetaEqGenerator());
    const auto*     pEqGenerator       = pBaseHtile->GetMetaEqGenerator();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32    pipeBankXor        = pEqGenerator->CalcPipeXorMask(range.startSubres.aspect);
#else
    const uint32    pipeBankXor        = pEqGenerator->CalcPipeXorMask(range.startSubres.plane);
#endif
    const auto&     hTileAddrOutput    = pBaseHtile->GetAddrOutput();
    const uint32    log2MetaBlkWidth   = Log2(hTileAddrOutput.metaBlkWidth);
    const uint32    log2MetaBlkHeight  = Log2(hTileAddrOutput.metaBlkHeight);
    const uint32    sliceSize          = (hTileAddrOutput.pitch * hTileAddrOutput.height) >>
                                         (log2MetaBlkWidth + log2MetaBlkHeight);
    const uint32    effectiveSamples   = pEqGenerator->GetNumEffectiveSamples();
    BufferSrd       bufferSrds[2]      = {};
    uint32          threadsPerGroup[3] = {};

    // TODO: need to obey the "dbPerTileExpClearEnable" setting here.
    const Pal::ComputePipeline* pPipeline = GetPipeline((effectiveSamples > 1)
                                                   ? RpmComputePipeline::Gfx9ClearHtileMultiSample
                                                   : RpmComputePipeline::Gfx9ClearHtileSingleSample);

    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    // Save the command buffer's state
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    // Bind Compute Pipeline used for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    // On GFX9, we create a single view of the hTile buffer that points to the base mip level.  It's
    // up to the equation to "find" each mip level and slice from that base location.
    BufferViewInfo hTileSurfBufferView = { };
    pBaseHtile->BuildSurfBufferView(&hTileSurfBufferView);
    pParentDev->CreateUntypedBufferViewSrds(1, &hTileSurfBufferView, &bufferSrds[0]);

    // Create a view of the hTile equation so that the shader can access it.
    BufferViewInfo hTileEqBufferView = { };
    pEqGenerator->BuildEqBufferView(&hTileEqBufferView);
    pParentDev->CreateUntypedBufferViewSrds(1, &hTileEqBufferView, &bufferSrds[1]);

    const uint32 lastMip = range.startSubres.mipLevel + range.numMips - 1;
    for (uint32 mipLevel = range.startSubres.mipLevel; mipLevel <= lastMip; ++mipLevel)
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId  subResId       = { range.startSubres.aspect, mipLevel, 0 };
#else
        const SubresId  subResId       = { range.startSubres.plane, mipLevel, 0 };
#endif
        const auto*     pSubResInfo    = pParentImg->SubresourceInfo(subResId);
        const auto&     hTileMipInfo   = pBaseHtile->GetAddrMipInfo(mipLevel);
        const uint32    mipLevelHeight = pSubResInfo->extentTexels.height;
        const uint32    mipLevelWidth  = pSubResInfo->extentTexels.width;

        const uint32 constData[] =
        {
            // start cb0[0]
            hTileMipInfo.startX,
            hTileMipInfo.startY,
            range.startSubres.arraySlice,
            sliceSize,
            // start cb0[1]
            log2MetaBlkWidth,
            log2MetaBlkHeight,
            0, // depth surfaces are always 2D
            hTileAddrOutput.pitch >> log2MetaBlkWidth,
            // start cb0[2]
            mipLevelWidth,
            mipLevelHeight,
            htileValue & htileMask,
            ~htileMask,
            // start cb0[3]
            pipeBankXor,
            effectiveSamples,
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // Put the SRDs for the hTile buffer and hTile equation into shader-accessible memory
        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += Util::NumBytesToNumDwords(sizeof(bufferSrds));

        // Provide the shader with all kinds of fun dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        MetaDataDispatch(pCmdBuffer,
                         pBaseHtile,
                         mipLevelWidth,
                         mipLevelHeight,
                         range.numSlices,
                         threadsPerGroup);
    }

    // Restore the command buffer's state.
    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
const Pal::ComputePipeline* Gfx9RsrcProcMgr::GetCmdGenerationPipeline(
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
        pipeline = RpmComputePipeline::Gfx9GenerateCmdDraw;
        break;

    case GeneratorType::Dispatch:
        pipeline = RpmComputePipeline::Gfx9GenerateCmdDispatch;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return GetPipeline(pipeline);
}

// =====================================================================================================================
// Performs a "fast" depth and stencil resummarize operation by updating the Image's HTile buffer to represent a fully
// open HiZ range and set ZMask and SMem to expanded state.
void Gfx9RsrcProcMgr::HwlResummarizeHtileCompute(
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

    SubresRange splitRange = range;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    splitRange.numPlanes   = 1;
    for (;
        splitRange.startSubres.plane < (range.startSubres.plane + range.numPlanes);
        splitRange.startSubres.plane++)
#endif
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const uint32 hTileMask  = pHtile->GetPlaneMask(splitRange.startSubres.aspect);
#else
        const uint32 hTileMask  = pHtile->GetPlaneMask(splitRange);
#endif

        ExecuteHtileEquation(pCmdBuffer,
                             gfx9Image,
                             splitRange,
                             hTileValue,
                             hTileMask);
    }
}

// =====================================================================================================================
// After a fixed-func depth/stencil copy resolve, src htile will be copied to dst htile and set the zmask or smask to
// expanded. Depth part and stencil part share same htile. So the depth part and stencil part will be merged (if
// necessary) and one cs blt will be launched for each merged region to copy and fixup the htile.
void Gfx9RsrcProcMgr::HwlHtileCopyAndFixUp(
    GfxCmdBuffer*             pCmdBuffer,
    const Pal::Image&         srcImage,
    const Pal::Image&         dstImage,
    ImageLayout               dstImageLayout,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions,
    bool                      computeResolve) const
{
    PAL_ASSERT(srcImage.IsDepthStencilTarget() && dstImage.IsDepthStencilTarget());

    // Merge depth and stencil regions in which htile fix up could be performed together.
    // Although depth and stencil htile fix-up could theoretically performed respectively, cs partial flush is
    // required to ensure coherency. So we perform the depth and stencil htile fix-up simultaneously.
    struct FixUpRegion
    {
        const ImageResolveRegion* pResolveRegion;
        uint32 planeFlags;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        void FillAspect(ImageAspect aspect)
        {
            if (aspect == ImageAspect::Depth)
            {
                PAL_ASSERT(TestAnyFlagSet(planeFlags, HtilePlaneDepth) == false);
                planeFlags |= HtilePlaneDepth;
            }
            else if (aspect == ImageAspect::Stencil)
            {
                PAL_ASSERT(TestAnyFlagSet(planeFlags, HtilePlaneStencil) == false);
                planeFlags |= HtilePlaneStencil;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }
        }
#else
        void FillPlane(const Pal::Image& image, uint32 plane)
        {
            if (image.IsDepthPlane(plane))
            {
                PAL_ASSERT(TestAnyFlagSet(planeFlags, HtilePlaneDepth) == false);
                planeFlags |= HtilePlaneDepth;
            }
            else if (image.IsStencilPlane(plane))
            {
                PAL_ASSERT(TestAnyFlagSet(planeFlags, HtilePlaneStencil) == false);
                planeFlags |= HtilePlaneStencil;
            }
            else
            {
                PAL_ASSERT_ALWAYS();
            }
        }
#endif
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                    listRegion.FillAspect(curResolveRegion.dstAspect);
#else
                    listRegion.FillPlane(dstImage, curResolveRegion.dstPlane);
#endif

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                    PAL_ASSERT(curResolveRegion.dstAspect != listRegion.pResolveRegion->dstAspect);
#else
                    PAL_ASSERT(curResolveRegion.dstPlane != listRegion.pResolveRegion->dstPlane);
#endif

                    PAL_ASSERT((curResolveRegion.srcOffset.x == listRegion.pResolveRegion->srcOffset.x) &&
                        (curResolveRegion.srcOffset.y == listRegion.pResolveRegion->srcOffset.y) &&
                        (curResolveRegion.dstOffset.x == listRegion.pResolveRegion->dstOffset.x) &&
                        (curResolveRegion.dstOffset.y == listRegion.pResolveRegion->dstOffset.y) &&
                        (curResolveRegion.extent.width == listRegion.pResolveRegion->extent.width) &&
                        (curResolveRegion.extent.height == listRegion.pResolveRegion->extent.height) &&
                        (curResolveRegion.numSlices == listRegion.pResolveRegion->numSlices) &&
                        (curResolveRegion.srcSlice == listRegion.pResolveRegion->srcSlice));

                    inserted = true;
                    break;
                }
            }

            if (inserted == false)
            {
                FixUpRegion fixUpRegion = {};
                fixUpRegion.pResolveRegion = &curResolveRegion;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                fixUpRegion.FillAspect(curResolveRegion.dstAspect);
#else
                fixUpRegion.FillPlane(dstImage, curResolveRegion.dstPlane);
#endif

                fixUpRegionList[mergedCount++] = fixUpRegion;
            } // End of if
        } // End of for
    } // End of if

    const Image* pGfxSrcImage = reinterpret_cast<const Image*>(srcImage.GetGfxImage());
    const Image* pGfxDstImage = reinterpret_cast<const Image*>(dstImage.GetGfxImage());

    if (pGfxSrcImage->HasDsMetadata() && pGfxDstImage->HasDsMetadata())
    {
        SubresId dstSubresId = {};

        const auto& srcCreateInfo = srcImage.GetImageCreateInfo();
        const auto& dstCreateInfo = dstImage.GetImageCreateInfo();

        // Save the command buffer's state
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

        const Pal::ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::Gfx9HtileCopyAndFixUp);

        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        for (uint32 i = 0; i < mergedCount; ++i)
        {
            BufferViewInfo bufferView[4] = {};
            BufferSrd      bufferSrds[4] = {};

            const ImageResolveRegion* pCurRegion = fixUpRegionList[i].pResolveRegion;
            uint32 dstMipLevel = pCurRegion->dstMipLevel;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            dstSubresId.aspect = pCurRegion->dstAspect;
#else
            dstSubresId.plane = pCurRegion->dstPlane;
#endif
            dstSubresId.mipLevel = dstMipLevel;
            dstSubresId.arraySlice = pCurRegion->dstSlice;
            const SubResourceInfo* pDstSubresInfo = dstImage.SubresourceInfo(dstSubresId);

            // Dst htile surface
            const Gfx9Htile* pDstHtile = pGfxDstImage->GetHtile();
            pDstHtile->BuildSurfBufferView(&bufferView[0]);
            dstImage.GetDevice()->CreateUntypedBufferViewSrds(1, &bufferView[0], &bufferSrds[0]);

            // Src htile surface
            const Gfx9Htile* pSrcHtile = pGfxSrcImage->GetHtile();
            pSrcHtile->BuildSurfBufferView(&bufferView[1]);
            srcImage.GetDevice()->CreateUntypedBufferViewSrds(1, &bufferView[1], &bufferSrds[1]);

            // Src htile lookup table
            pGfxSrcImage->BuildMetadataLookupTableBufferView(&bufferView[2], 0);
            srcImage.GetDevice()->CreateUntypedBufferViewSrds(1, &bufferView[2], &bufferSrds[2]);

            // Dst htile lookup table
            pGfxDstImage->BuildMetadataLookupTableBufferView(&bufferView[3], dstMipLevel);
            dstImage.GetDevice()->CreateUntypedBufferViewSrds(1, &bufferView[3], &bufferSrds[3]);

            static const uint32 HtileTexelAlign = 8;

            // Htile copy and fixup require offset and extent to be 8 pixel alignment, or the copy region
            // covers full right-bottom part of dst image.
            PAL_ASSERT(IsPow2Aligned(pCurRegion->srcOffset.x, HtileTexelAlign));
            PAL_ASSERT(IsPow2Aligned(pCurRegion->srcOffset.y, HtileTexelAlign));

            PAL_ASSERT(IsPow2Aligned(pCurRegion->dstOffset.x, HtileTexelAlign));
            PAL_ASSERT(IsPow2Aligned(pCurRegion->dstOffset.y, HtileTexelAlign));

            PAL_ASSERT(IsPow2Aligned(pCurRegion->extent.width, HtileTexelAlign) ||
                       ((pCurRegion->extent.width + pCurRegion->dstOffset.x) == pDstSubresInfo->extentTexels.width));

            PAL_ASSERT(IsPow2Aligned(pCurRegion->extent.height, HtileTexelAlign) ||
                       ((pCurRegion->extent.height + pCurRegion->dstOffset.y) == pDstSubresInfo->extentTexels.height));
            PAL_ASSERT((pCurRegion->dstOffset.x >= 0) && (pCurRegion->dstOffset.y >= 0));

            const uint32  htileExtentX = (Pow2Align(pCurRegion->extent.width, HtileTexelAlign) / HtileTexelAlign);
            const uint32  htileExtentY = (Pow2Align(pCurRegion->extent.height, HtileTexelAlign) / HtileTexelAlign);
            const uint32  htileExtentZ = pCurRegion->numSlices;
            const uint32  htileMask    = pDstHtile->GetPlaneMask(fixUpRegionList[i].planeFlags);

            const uint32 constData[] =
            {
                // start cb1[0]
                pCurRegion->srcOffset.x / HtileTexelAlign,                                   //srcHtileOffset.x
                pCurRegion->srcOffset.y / HtileTexelAlign,                                   //srcHtileOffset.y
                pCurRegion->srcSlice,                                                        //srcHtileOffset.z
                htileExtentX,                                                                //resolveExtentX
                // start cb1[1]
                pCurRegion->dstOffset.x / HtileTexelAlign,                                   //dstHtileOffset.x
                pCurRegion->dstOffset.y / HtileTexelAlign,                                   //dstHtileOffset.y
                pCurRegion->dstSlice,                                                        //dstHtileOffset.z
                htileExtentY,                                                                //resolveExtentY,
                // start cb1[2]
                Pow2Align(srcCreateInfo.extent.width, HtileTexelAlign) / HtileTexelAlign,    //srcMipLevelHtileDim.x
                Pow2Align(srcCreateInfo.extent.height, HtileTexelAlign) / HtileTexelAlign,   //srcMipLevelHtileDim.y
                Pow2Align(pDstSubresInfo->extentTexels.width, HtileTexelAlign)
                    / HtileTexelAlign,                                                       //dstMipLevelHtileDim.x
                Pow2Align(pDstSubresInfo->extentTexels.height, HtileTexelAlign)
                    / HtileTexelAlign,                                                       //dstMipLevelHtileDim.y
                // start cb1[3]
                pDstHtile->GetInitialValue() & htileMask,                                    //zsDecompressedValue
                htileMask,                                                                   //htileMask
                0u,                                                                          //Padding
                0u,                                                                          //Padding
            };

            // Create an embedded user-data table and bind it to user data 0.
            static const uint32 sizeBufferSrdDwords = NumBytesToNumDwords(sizeof(BufferSrd));
            static const uint32 sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
            uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                sizeBufferSrdDwords * 4 + sizeConstDataDwords,
                sizeBufferSrdDwords,
                PipelineBindPoint::Compute,
                0);

            // Put the SRDs for the hTile buffer and hTile lookup table into shader-accessible memory
            memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
            pSrdTable += sizeBufferSrdDwords * 4;

            // Provide the shader with all kinds of fun dimension info
            memcpy(pSrdTable, &constData, sizeof(constData));

            // Now that we have the dimensions in terms of compressed pixels, launch as many thread groups as we need to
            // get to them all.
            pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(htileExtentX, threadsPerGroup[0]),
                RpmUtil::MinThreadGroups(htileExtentY, threadsPerGroup[1]),
                RpmUtil::MinThreadGroups(htileExtentZ, threadsPerGroup[2]));
        } // End of for

        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    } // End of if
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image range by updating the Image's HTile buffer.
void Gfx9RsrcProcMgr::FastDepthStencilClearCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             htileValue,
    uint32             clearMask,   // bitmask of HtilePlaneMask enumerations
    uint8              stencil
    ) const
{
    const Pal::Image*      pPalImage    = dstImage.Parent();
    const Pal::Device*     pDevice      = pPalImage->GetDevice();
    const Gfx9PalSettings& settings     = GetGfx9Settings(*pDevice);
    const auto&            createInfo   = pPalImage->GetImageCreateInfo();
    const auto*            pHtile       = dstImage.GetHtile();
    const auto&            hTileMipInfo = pHtile->GetAddrMipInfo(range.startSubres.mipLevel);

    // We haven't implemented the ExpClear paths yet in gfx9
    PAL_ASSERT(settings.dbPerTileExpClearEnable == false);

    // For now just find out here if this resource can do Optimized Htile clear (for all depth surfaces except
    // those whose meta data is in mip tail)
    const bool canDoHtileOptimizedClear = ((hTileMipInfo.inMiptail == 0) &&
                                           (TestAnyFlagSet(settings.optimizedFastClear, Gfx9OptimizedFastClearDepth)));

    const uint32 htileMask = pHtile->GetPlaneMask(clearMask);

    if (canDoHtileOptimizedClear)
    {
        DoOptimizedHtileFastClear(pCmdBuffer, dstImage, range, htileValue, htileMask);
    }
    else
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
        // ExecuteHtileEquation is set up to work on single plane ranges, so split the multi plane range into
        // single plane ranges just for this path.
        if (range.numPlanes == 2)
        {
            SubresRange splitRange = range;
            splitRange.numPlanes   = 1;
            for (;
                 splitRange.startSubres.plane < (range.startSubres.plane + range.numPlanes);
                 splitRange.startSubres.plane++)
            {
                const uint32 splitClearMask = GetInitHtileClearMask(dstImage, splitRange);
                const uint32 splitHtileMask = pHtile->GetPlaneMask(splitClearMask);
                ExecuteHtileEquation(pCmdBuffer, dstImage, splitRange, htileValue, splitHtileMask);
            }
        }
        else
#endif
        {
            ExecuteHtileEquation(pCmdBuffer, dstImage, range, htileValue, htileMask);
        }
    }

    FastDepthStencilClearComputeCommon(pCmdBuffer, pPalImage, clearMask);
}

// =====================================================================================================================
void Gfx9RsrcProcMgr::HwlDecodeBufferViewSrd(
    const void*      pBufferViewSrd,
    BufferViewInfo*  pViewInfo
    ) const
{
    const auto& srd = *(static_cast<const Gfx9BufferSrd*>(pBufferViewSrd));

    // Verify that we have a buffer view SRD.
    PAL_ASSERT(srd.word3.bits.TYPE == SQ_RSRC_BUF);

    // Reconstruct the buffer view info struct.
    pViewInfo->gpuAddr = (static_cast<gpusize>(srd.word1.bits.BASE_ADDRESS_HI) << 32ull) + srd.word0.bits.BASE_ADDRESS;
    pViewInfo->range   = srd.word2.bits.NUM_RECORDS;
    pViewInfo->stride  = srd.word1.bits.STRIDE;

    if (pViewInfo->stride > 1)
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

// =====================================================================================================================
// GFX9-specific function for extracting the subresource range and format information from the supplied SRD and image
void Gfx9RsrcProcMgr::HwlDecodeImageViewSrd(
    const void*          pImageViewSrd,
    const Pal::Image&    dstImage,
    SwizzledFormat*      pSwizzledFormat,
    SubresRange*         pSubresRange
    ) const
{
    const Gfx9ImageSrd&  srd = *static_cast<const Gfx9ImageSrd*>(pImageViewSrd);

    // Verify that we have an image view SRD.
    PAL_ASSERT((srd.word3.bits.TYPE >= SQ_RSRC_IMG_1D) && (srd.word3.bits.TYPE <= SQ_RSRC_IMG_2D_MSAA_ARRAY));

    const gpusize  srdBaseAddr = (static_cast<gpusize>(srd.word1.bits.BASE_ADDRESS_HI) << 32ull) +
                                  srd.word0.bits.BASE_ADDRESS;

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
    // that it's looking at the color plane and that it's not block compressed.
    PAL_ASSERT(Formats::IsBlockCompressed(pSwizzledFormat->format) == false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    pSubresRange->startSubres.aspect = DecodeImageViewSrdAspect(dstImage, srdBaseAddr);
#else
    pSubresRange->startSubres.plane = DecodeImageViewSrdPlane(dstImage, srdBaseAddr);
    pSubresRange->numPlanes         = 1;
#endif

    // The PAL interface can not individually address the slices of a 3D resource.  "numSlices==1" is assumed to
    // mean all of them and we have to start from the first slice.
    if (dstImage.GetImageCreateInfo().imageType == ImageType::Tex3d)
    {
        pSubresRange->numSlices              = 1;
        pSubresRange->startSubres.arraySlice = 0;
    }
    else
    {
        pSubresRange->numSlices              = srd.word4.bitfields.DEPTH - srd.word5.bits.BASE_ARRAY + 1;
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
// Initializes the requested range of cMask memory for the specified image.
void Gfx9RsrcProcMgr::InitCmask(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       image,
    const SubresRange& initRange,
    const uint8        initValue
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(initRange.numPlanes == 1);
#endif

    const Pal::Image*      pPalImage  = image.Parent();
    const Pal::Device*     pDevice    = pPalImage->GetDevice();
    const Gfx9PalSettings& settings   = GetGfx9Settings(*pDevice);
    const auto&            createInfo = pPalImage->GetImageCreateInfo();

    // Why are we allocating cMask for a single-sample surface?
    PAL_ASSERT (createInfo.samples > 1);

    // MSAA images can't have mipmaps
    PAL_ASSERT (createInfo.mipLevels == 1);

    // check if this resource can do Optimized cmask clear
    const bool canDoCmaskOptimizedClear = TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearColorCmask);

    if (canDoCmaskOptimizedClear)
    {
        // Save the command buffer's state.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

        DoOptimizedCmaskInit(pCmdBuffer, pCmdStream, image, initRange, initValue);

        // Restore the command buffer's state.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }
    else
    {
        const auto*   pCmask             = image.GetCmask();
        PAL_ASSERT(pCmask->HasMetaEqGenerator());
        const auto*   pEqGenerator       = pCmask->GetMetaEqGenerator();
        const auto&   cMaskAddrOutput    = pCmask->GetAddrOutput();
        const uint32  log2MetaBlkWidth   = Log2(cMaskAddrOutput.metaBlkWidth);
        const uint32  log2MetaBlkHeight  = Log2(cMaskAddrOutput.metaBlkHeight);
        const uint32  sliceSize          = (cMaskAddrOutput.pitch * cMaskAddrOutput.height) >>
                                           (log2MetaBlkWidth + log2MetaBlkHeight);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const uint32  pipeBankXor        = pEqGenerator->CalcPipeXorMask(initRange.startSubres.aspect);
#else
        const uint32  pipeBankXor        = pEqGenerator->CalcPipeXorMask(initRange.startSubres.plane);
#endif
        uint32        threadsPerGroup[3] = {};

        // Does cMask *ever* depend on the number of samples?  If so, our shader is going to need some tweaking.
        PAL_ASSERT (pEqGenerator->GetNumEffectiveSamples() == 1);
        const auto*const pPipeline  = GetPipeline(RpmComputePipeline::Gfx9InitCmask);

        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // Save the command buffer's state.
        pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

        // Bind Compute Pipeline used for the clear.
        pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

        BufferViewInfo bufferViewCmaskSurf = { };
        pCmask->BuildSurfBufferView(&bufferViewCmaskSurf);
        BufferViewInfo bufferViewCmaskEq   = { };
        pEqGenerator->BuildEqBufferView(&bufferViewCmaskEq);

        const uint32 constData[] =
        {
            // start cb0[0]
            log2MetaBlkWidth,
            log2MetaBlkHeight,
            0, // 2D image, no depth
            cMaskAddrOutput.pitch >> log2MetaBlkWidth,
            // start cb0[1]
            createInfo.extent.width,
            createInfo.extent.height,
            sliceSize,
            pipeBankXor,
            // start cb0[2]
            static_cast<uint32>(initValue & 0xF), 0, 0, 0
        };

        // Create an embedded user-data table and bind it to user data 0.
        const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));
        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                                   SrdDwordAlignment() * 2 + sizeConstDataDwords,
                                                                   SrdDwordAlignment(),
                                                                   PipelineBindPoint::Compute,
                                                                   0);

        // We need buffer views for both the DCC surface and the addressing equation
        pDevice->CreateUntypedBufferViewSrds(1, &bufferViewCmaskSurf, pSrdTable);
        pSrdTable += SrdDwordAlignment();
        pDevice->CreateUntypedBufferViewSrds(1, &bufferViewCmaskEq, pSrdTable);
        pSrdTable += SrdDwordAlignment();
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        MetaDataDispatch(pCmdBuffer,
                         pCmask,
                         createInfo.extent.width,
                         createInfo.extent.height,
                         initRange.numSlices,
                         threadsPerGroup);

        // Restore the command buffer's state.
        pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
    }

}

// =====================================================================================================================
// Use the compute engine to initialize hTile memory that corresponds to the specified clearRange
void Gfx9RsrcProcMgr::InitHtile(
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
    PAL_ASSERT(pCmdBuffer->IsComputeSupported());

    if (clearMask != 0)
    {
        const uint32  initValue = pHtile->GetInitialValue();

        // We pass a dummy stencil value as the last parameter.
        // The dummy stencil value won't be used in Gfx9RsrcProcMgr::FastDepthStencilClearCompute.
        FastDepthStencilClearCompute(pCmdBuffer,
                                     dstImage,
                                     clearRange,
                                     initValue,
                                     clearMask,
                                     0);
    }
}

// =====================================================================================================================
void Gfx9RsrcProcMgr::HwlFixupCopyDstImageMetaData(
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

    // If image is created with fullCopyDstOnly=1, there will be no expand when transition to "LayoutCopyDst"; if
    // this is not a compressed compute copy, need fix up dst metadata to uncompressed state. Otherwise if image is
    // created with fullCopyDstOnly=0, there will be expand when transition to "LayoutCopyDst"; metadata is
    // in uncompressed state and no need fixup here.
    const auto& gfx9DstImage = static_cast<const Gfx9::Image&>(*dstImage.GetGfxImage());

    // Copy to depth should go through gfx path and not to here.
    PAL_ASSERT(gfx9DstImage.Parent()->IsDepthStencilTarget() == false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
    if (gfx9DstImage.HasDccData() && (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0))
    {
        auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            const auto& clearRegion = pRegions[idx];

            SubresRange range = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            range.startSubres.aspect     = clearRegion.subres.aspect;
#else
            range.startSubres.plane      = clearRegion.subres.plane;
#endif
            range.startSubres.mipLevel   = clearRegion.subres.mipLevel;
            range.startSubres.arraySlice = clearRegion.subres.arraySlice;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
            range.numPlanes              = 1;
#endif
            range.numMips                = 1;
            range.numSlices              = clearRegion.numSlices;

            // Since color data is no longer dcc compressed set Dcc to fully uncompressed.
            ClearDcc(pCmdBuffer, pStream, gfx9DstImage, range, Gfx9Dcc::InitialValue, DccClearPurpose::FastClear);
        }
    }
#endif

    RsrcProcMgr::HwlFixupCopyDstImageMetaData(pCmdBuffer, pSrcImage, dstImage, dstImageLayout,
                                              pRegions, regionCount, isFmaskCopyOptimized);
}

// =====================================================================================================================
// Does a compute-based fast-clear of the specified image / range.  The image's associated DCC memory is updated to
// "clearCode" for all bytes corresponding to "clearRange".
void Gfx10RsrcProcMgr::ClearDccCompute(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       dstImage,
    const SubresRange& clearRange,
    uint8              clearCode,
    DccClearPurpose    clearPurpose,
    const uint32*      pPackedClearColor
    ) const
{
    const Pal::Image*    pPalImage     = dstImage.Parent();
    const Pal::Device*   pDevice       = pPalImage->GetDevice();
    const auto&          createInfo    = pPalImage->GetImageCreateInfo();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*          pDcc          = dstImage.GetDcc(clearRange.startSubres.aspect);
    const SubresId       baseSubResId  = { clearRange.startSubres.aspect, 0 , 0 };
    const auto&          dccAddrOutput = pDcc->GetAddrOutput();
#endif
    const auto*          pBoundMemory  = pPalImage->GetBoundGpuMemory().Memory();
    const uint32         startSlice    = ((createInfo.imageType == ImageType::Tex3d)
                                          ? 0
                                          : clearRange.startSubres.arraySlice);
    const uint32         clearColor    = ExpandClearCodeToDword(clearCode);
    const auto*          pSubResInfo   = dstImage.Parent()->SubresourceInfo(clearRange.startSubres);
    const SwizzledFormat planeFormat   = pSubResInfo->format;
    const uint32         bytesPerPixel = Formats::BytesPerPixel(planeFormat.format);
    const auto*          pAddrOutput   = dstImage.GetAddrOutput(pSubResInfo);

    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    for (uint32 plane = clearRange.startSubres.plane;
         plane < (clearRange.startSubres.plane + clearRange.numPlanes);
         plane++)
#endif
    {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
        const auto* pDcc          = dstImage.GetDcc(plane);
        const auto& dccAddrOutput = pDcc->GetAddrOutput();
#endif

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                const uint32 numSlices = GetClearDepth(dstImage, clearRange, absMipLevel);
#else
                const uint32 numSlices = GetClearDepth(dstImage, plane, clearRange.numSlices, absMipLevel);
#endif

                // The "metaBlkDepth" parameter is the number of slices that the "dccRamSliceSize" covers.  For non-3D
                // images, this should always be 1 (i.e., one addrlib slice is one PAL API slice).  For 3D images, this
                // can be way more than the number of PAL API slices.
                const uint32 numSlicesToClear = Max(1u, numSlices / dccAddrOutput.metaBlkDepth);

                for (uint32  sliceIdx = 0; sliceIdx < numSlicesToClear; sliceIdx++)
                {
                    // GetMaskRamBaseAddr doesn't compute the base address of a mip level (only a slice offset), so
                    // we have to do the math here ourselves.
                    const uint32    absSlice      = startSlice + sliceIdx;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                    const gpusize   clearBaseAddr = dstImage.GetMaskRamBaseAddr(pDcc, baseSubResId) +
#else
                    const gpusize   clearBaseAddr = dstImage.GetMaskRamBaseAddr(pDcc, 0) +
#endif
                                                    absSlice * dccAddrOutput.dccRamSliceSize        +
                                                    dccMipInfo.offset;
                    const gpusize   clearOffset   = clearBaseAddr - pBoundMemory->Desc().gpuVirtAddr;

                    CmdFillMemory(pCmdBuffer,
                                  false,            // don't save / restore the compute state
                                  *pBoundMemory,
                                  clearOffset,
                                  dccMipInfo.sliceSize,
                                  clearColor);
                }

                if ((clearCode == static_cast<uint8>(Gfx9DccClearColor::Gfx10ClearColorCompToSingle))
                   )
                {
                    // If this image doesn't support comp-to-single fast clears, then how did we wind up with the
                    // comp-to-single clear code???
                    PAL_ASSERT(dstImage.Gfx10UseCompToSingleFastClears());

                    // If we're not doing a fast clear then how did we wind up with a fast-clear related code???
                    PAL_ASSERT(clearPurpose == DccClearPurpose::FastClear);

                    ClearDccComputeSetFirstPixelOfBlock(pCmdBuffer,
                                                        dstImage,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                                                        clearRange.startSubres.aspect,
#else
                                                        plane,
#endif
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

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Use a compute shader to write the clear color to the first byte of each 256B block.
void Gfx10RsrcProcMgr::ClearDccComputeSetFirstPixelOfBlock(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    ImageAspect        aspect,
#else
    uint32             plane,
#endif
    uint32             absMipLevel,
    uint32             startSlice,  // 0 for 3d or start array slice for 2d array.
    uint32             numSlices,   // depth for 3d or number of array slices for 2d array.
    uint32             bytesPerPixel,
    const uint32*      pPackedClearColor
    ) const
{
    PAL_ASSERT(pPackedClearColor != nullptr);
    const Pal::Image*        pPalImage  = dstImage.Parent();
    const auto&              createInfo = pPalImage->GetImageCreateInfo();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*              pDcc       = dstImage.GetDcc(aspect);
#else
    const auto*              pDcc       = dstImage.GetDcc(plane);
#endif
    const RpmComputePipeline pipeline   = ((createInfo.samples == 1)
                                            ? RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixel
                                            : RpmComputePipeline::Gfx10ClearDccComputeSetFirstPixelMsaa);
    const auto*const         pPipeline  = GetPipeline(pipeline);

    // Bind Compute Pipeline used for the clear.
    pCmdBuffer->CmdBindPipeline({ PipelineBindPoint::Compute, pPipeline, InternalApiPsoHash, });

    uint32 xInc = 0;
    uint32 yInc = 0;
    uint32 zInc = 0;
    pDcc->GetXyzInc(&xInc, &yInc, &zInc);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId subResId     = { ImageAspect::Color, absMipLevel, startSlice };
#else
    const SubresId subResId     = { 0, absMipLevel, startSlice };
#endif
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
        xInc = 8;
        yInc = 8;
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

    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

    const auto*    pSubResInfo    = pPalImage->SubresourceInfo(subResId);
    const uint32   mipLevelWidth  = pSubResInfo->extentTexels.width;
    const uint32   mipLevelHeight = pSubResInfo->extentTexels.height;
    const uint32   mipLevelDepth  = numSlices;

    // How many blocks are there for this miplevel in X/Y/Z dimension.
    // We'll need one thread for each block, which writes clear value to the first byte.
    const uint32 numBlockX = (mipLevelWidth  + xInc - 1) / xInc;
    const uint32 numBlockY = (mipLevelHeight + yInc - 1) / yInc;
    const uint32 numBlockZ = (mipLevelDepth  + zInc - 1) / zInc;

    const uint32 constData[] =
    {
        // start cb0[0]
        mipLevelWidth,
        mipLevelHeight,
        mipLevelDepth,
        // start cb0[1]
        // Because we can't fast clear a surface with more than 64bpp, there's no need to pass in
        // pPackedClearColor[2] and pPackedClearColor[3].
        pPackedClearColor[0],
        pPackedClearColor[1],
        // single-sample shader ignores this entry
        createInfo.samples,
        // start cb0[2]
        xInc,
        yInc,
        zInc
    };

    const uint32  sizeConstDataDwords = NumBytesToNumDwords(sizeof(constData));

    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(pCmdBuffer,
                                                               SrdDwordAlignment() + sizeConstDataDwords,
                                                               SrdDwordAlignment(),
                                                               PipelineBindPoint::Compute,
                                                               0);

    ImageViewInfo  imageView    = {};
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    SubresRange    viewRange    = { subResId, 1, (createInfo.imageType == ImageType::Tex3d) ? 1 : numSlices };
#else
    SubresRange    viewRange    = { subResId, 1, 1, (createInfo.imageType == ImageType::Tex3d) ? 1 : numSlices };
#endif

    RpmUtil::BuildImageViewInfo(&imageView,
                                *dstImage.Parent(),
                                viewRange,
                                planeFormat,
                                RpmUtil::DefaultRpmLayoutShaderWriteRaw,
                                pPalImage->GetDevice()->TexOptLevel());

    sq_img_rsrc_t srd = {};

    pPalImage->GetDevice()->CreateImageViewSrds(1, &imageView, &srd);

    // We want to unset this bit because we are writing the real clear color to the first pixel of each DCC block,
    // So it doesn't need to be compressed. Currently this is the only place we unset this bit in GFX10.

    srd.compression_en = 0;

    memcpy(pSrdTable, &srd, sizeof(srd));

    pSrdTable += SrdDwordAlignment();

    // And give the shader all kinds of useful dimension info
    memcpy(pSrdTable, &constData[0], sizeof(constData));

    uint32 numThreadGroupsX = RpmUtil::MinThreadGroups(numBlockX, threadsPerGroup[0]);
    uint32 numThreadGroupsY = RpmUtil::MinThreadGroups(numBlockY, threadsPerGroup[1]);
    uint32 numThreadGroupsZ = RpmUtil::MinThreadGroups(numBlockZ, threadsPerGroup[2]);

    pCmdBuffer->CmdDispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
}

// =====================================================================================================================
// Performs a "fast" depth and stencil resummarize operation by updating the Image's HTile buffer to represent a fully
// open HiZ range and set ZMask and SMem to expanded state.
void Gfx10RsrcProcMgr::HwlResummarizeHtileCompute(
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    uint32 hTileMask        = pHtile->GetPlaneMask(range.startSubres.aspect);
#else
    uint32 hTileMask        = pHtile->GetPlaneMask(range);
#endif

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
void Gfx10RsrcProcMgr::InitHtileData(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             hTileMask
    ) const
{
    const Pal::Image*  pPalImage     = dstImage.Parent();
    const auto*        pHtile        = dstImage.GetHtile();
    const auto&        hTileAddrOut  = pHtile->GetAddrOutput();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const SubresId     baseSubResId  = { range.startSubres.aspect, 0 , 0 };
    const gpusize      hTileBaseAddr = dstImage.GetMaskRamBaseAddr(pHtile, baseSubResId);
#else
    const gpusize      hTileBaseAddr = dstImage.GetMaskRamBaseAddr(pHtile, 0);
#endif
    const auto&        settings      = m_pDevice->Parent()->Settings();

    PAL_ASSERT(pCmdBuffer->IsComputeSupported());

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

    // Put the new HTile data in user data 4 and the old HTile data mask in user data 5.
    const uint32 hTileUserData[2] = { hTileValue & hTileMask, ~hTileMask };
    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                               NumBytesToNumDwords(sizeof(sq_buf_rsrc_t)),
                               NumBytesToNumDwords(sizeof(hTileUserData)),
                               hTileUserData);

    bool  wroteLastMipLevel = false;
    for (uint32 mipIdx = 0; mipIdx < range.numMips; mipIdx++)
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
#if  PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 558
                hTileBufferView.flags.bypassMallRead  = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                       Gfx10RpmViewsBypassMallOnRead);
                hTileBufferView.flags.bypassMallWrite = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                       Gfx10RpmViewsBypassMallOnWrite);
#endif

                BufferSrd srd = { };
                m_pDevice->Parent()->CreateTypedBufferViewSrds(1, &hTileBufferView, &srd);

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                           0,
                                           NumBytesToNumDwords(sizeof(sq_buf_rsrc_t)),
                                           reinterpret_cast<uint32*>(&srd.gfx10));

                // Issue a dispatch with one thread per HTile DWORD.
                const uint32 hTileDwords  = static_cast<uint32>(hTileBufferView.range / sizeof(uint32));
                const uint32 threadGroups = RpmUtil::MinThreadGroups(hTileDwords, pPipeline->ThreadsPerGroup());
                pCmdBuffer->CmdDispatch(threadGroups, 1, 1);
            } // end loop through slices
        }
        else
        {
            // If this mip level isn't compressible, then no smaller mip levels will be either.
            wroteLastMipLevel = true;
        }
    } // end loop through mip levels

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void Gfx10RsrcProcMgr::WriteHtileData(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             hTileMask,
    uint8              stencil
    ) const
{
    const Pal::Image*  pPalImage     = dstImage.Parent();
    const auto*        pHtile        = dstImage.GetHtile();
    const auto&        hTileAddrOut  = pHtile->GetAddrOutput();
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const gpusize      hTileBaseAddr = dstImage.GetMaskRamBaseAddr(pHtile, range.startSubres);
#else
    const gpusize      hTileBaseAddr = dstImage.GetMaskRamBaseAddr(pHtile, range.startSubres.arraySlice);
#endif
    const auto&        settings      = m_pDevice->Parent()->Settings();

    PAL_ASSERT(pCmdBuffer->IsComputeSupported());

    // Save the command buffer's state.
    pCmdBuffer->CmdSaveComputeState(ComputeStatePipelineAndUserData);

    const bool expClearEnable        = m_pDevice->Settings().dbPerTileExpClearEnable;
    const bool tileStencilDisabled   = pHtile->TileStencilDisabled();
    const uint32 numBufferSrdDwords  = NumBytesToNumDwords(sizeof(sq_buf_rsrc_t));
    bool  wroteLastMipLevel          = false;

    const Pal::ComputePipeline* pPipeline = nullptr;

    for (uint32 mipIdx = 0; mipIdx < range.numMips; mipIdx++)
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
#if  PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 558
                hTileBufferView.flags.bypassMallRead  = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                       Gfx10RpmViewsBypassMallOnRead);
                hTileBufferView.flags.bypassMallWrite = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                       Gfx10RpmViewsBypassMallOnWrite);
#endif
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
                                           numBufferSrdDwords,
                                           reinterpret_cast<uint32*>(&htileSurfSrd.gfx10));

                pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                           numBufferSrdDwords,
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
#if  PAL_CLIENT_INTERFACE_MAJOR_VERSION>= 558
                    metadataView.flags.bypassMallRead  = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                        Gfx10RpmViewsBypassMallOnRead);
                    metadataView.flags.bypassMallWrite = TestAnyFlagSet(settings.rpmViewsBypassMall,
                                                                        Gfx10RpmViewsBypassMallOnWrite);
#endif
                    m_pDevice->Parent()->CreateUntypedBufferViewSrds(1, &metadataView, &metadataSrd);

                    pCmdBuffer->CmdSetUserData(PipelineBindPoint::Compute,
                                               numBufferSrdDwords + numConstDwords,
                                               numBufferSrdDwords,
                                               reinterpret_cast<uint32*>(&metadataSrd.gfx10));
                }

                // Issue a dispatch with one thread per HTile DWORD or a dispatch every 4 Htile DWORD.
                const uint32 threadGroups = RpmUtil::MinThreadGroups(minThreads, pPipeline->ThreadsPerGroup());
                pCmdBuffer->CmdDispatch(threadGroups, 1, 1);

            } // end loop through slices
        }
        else
        {
            // If this mip level isn't compressible, then no smaller mip levels will be either.
            wroteLastMipLevel = true;
        }
    } // end loop through mip levels

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
// Performs a fast-clear on a Depth/Stencil Image range by updating the Image's HTile buffer.
void Gfx10RsrcProcMgr::FastDepthStencilClearCompute(
    GfxCmdBuffer*      pCmdBuffer,
    const Image&       dstImage,
    const SubresRange& range,
    uint32             hTileValue,
    uint32             clearMask,
    uint8              stencil
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

    WriteHtileData(pCmdBuffer, dstImage, range, hTileValue, hTileMask, stencil);

    FastDepthStencilClearComputeCommon(pCmdBuffer, dstImage.Parent(), clearMask);
}

// =====================================================================================================================
const Pal::ComputePipeline* Gfx10RsrcProcMgr::GetCmdGenerationPipeline(
    const Pal::IndirectCmdGenerator& generator,
    const CmdBuffer&                 cmdBuffer
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
        pipeline = RpmComputePipeline::Gfx10GenerateCmdDispatchTaskMesh;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return GetPipeline(pipeline);
}

// =====================================================================================================================
template <typename SrdType>
static uint32 RetrieveHwFmtFromSrd(
    const Pal::Device&   palDevice,
    const SrdType*       pSrd)
{
    uint32  hwFmt = 0;

    {
        PAL_ASSERT(IsGfx10(palDevice));

        hwFmt = pSrd->gfx10Core.format;
    }

    return hwFmt;
}

// =====================================================================================================================
void Gfx10RsrcProcMgr::HwlDecodeBufferViewSrd(
    const void*      pBufferViewSrd,
    BufferViewInfo*  pViewInfo
    ) const
{
    const auto*    pSrd  = static_cast<const sq_buf_rsrc_t*>(pBufferViewSrd);
    const BUF_FMT  hwFmt = static_cast<BUF_FMT>(RetrieveHwFmtFromSrd(*(m_pDevice->Parent()), pSrd));

    // Verify that we have a buffer view SRD.
    PAL_ASSERT(pSrd->type == SQ_RSRC_BUF);

    // Reconstruct the buffer view info struct.
    pViewInfo->gpuAddr = pSrd->base_address;
    pViewInfo->range   = pSrd->num_records;
    pViewInfo->stride  = pSrd->stride;

    if (pViewInfo->stride > 1)
    {
        pViewInfo->range *= pViewInfo->stride;
    }

    pViewInfo->swizzledFormat.format    = FmtFromHwBufFmt(hwFmt,
                                                          m_pDevice->Parent()->ChipProperties().gfxLevel);
    pViewInfo->swizzledFormat.swizzle.r =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_x));
    pViewInfo->swizzledFormat.swizzle.g =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_y));
    pViewInfo->swizzledFormat.swizzle.b =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_z));
    pViewInfo->swizzledFormat.swizzle.a =
        ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_w));

    // Verify that we have a valid format.
    PAL_ASSERT(pViewInfo->swizzledFormat.format != ChNumFormat::Undefined);
}

// =====================================================================================================================
// GFX10-specific function for extracting the subresource range and format information from the supplied SRD and image
void Gfx10RsrcProcMgr::HwlDecodeImageViewSrd(
    const void*           pImageViewSrd,
    const Pal::Image&     dstImage,
    SwizzledFormat*       pSwizzledFormat,
    SubresRange*          pSubresRange
    ) const
{
    const ImageCreateInfo&  createInfo = dstImage.GetImageCreateInfo();

    const auto*    pSrd  = static_cast<const sq_img_rsrc_t*>(pImageViewSrd);
    const IMG_FMT  hwFmt = static_cast<IMG_FMT>(RetrieveHwFmtFromSrd(*(m_pDevice->Parent()), pSrd));

    // Verify that we have an image view SRD.
    PAL_ASSERT((pSrd->type >= SQ_RSRC_IMG_1D) && (pSrd->type <= SQ_RSRC_IMG_2D_MSAA_ARRAY));

    const gpusize  srdBaseAddr = pSrd->base_address;

    pSwizzledFormat->format    = FmtFromHwImgFmt(hwFmt, m_pDevice->Parent()->ChipProperties().gfxLevel);
    pSwizzledFormat->swizzle.r = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_x));
    pSwizzledFormat->swizzle.g = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_y));
    pSwizzledFormat->swizzle.b = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_z));
    pSwizzledFormat->swizzle.a = ChannelSwizzleFromHwSwizzle(static_cast<SQ_SEL_XYZW01>(pSrd->dst_sel_w));

    // Verify that we have a valid format.
    PAL_ASSERT(pSwizzledFormat->format != ChNumFormat::Undefined);

    // Next, recover the original subresource range. We can't recover the exact range in all cases so we must assume
    // that it's looking at the color plane and that it's not block compressed.
    PAL_ASSERT(Formats::IsBlockCompressed(pSwizzledFormat->format) == false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    pSubresRange->startSubres.aspect = DecodeImageViewSrdAspect(dstImage, srdBaseAddr);
#else
    pSubresRange->startSubres.plane = DecodeImageViewSrdPlane(dstImage, srdBaseAddr);
    pSubresRange->numPlanes         = 1;
#endif

    // The PAL interface can not individually address the slices of a 3D resource.  "numSlices==1" is assumed to
    // mean all of them and we have to start from the first slice.
    if (createInfo.imageType == ImageType::Tex3d)
    {
        pSubresRange->numSlices              = 1;
        pSubresRange->startSubres.arraySlice = 0;
    }
    else
    {
        pSubresRange->numSlices              = LowPart(pSrd->depth - pSrd->base_array + 1);
        pSubresRange->startSubres.arraySlice = LowPart(pSrd->base_array);
    }

    if (pSrd->type == SQ_RSRC_IMG_2D_MSAA_ARRAY)
    {
        // MSAA textures cannot be mipmapped; the BASE_LEVEL and LAST_LEVEL fields indicate the texture's sample count.
        pSubresRange->startSubres.mipLevel = 0;
        pSubresRange->numMips              = 1;
    }
    else
    {
        pSubresRange->startSubres.mipLevel = LowPart(pSrd->base_level);
        pSubresRange->numMips              = LowPart(pSrd->last_level - pSrd->base_level + 1);
    }

    if ((pSubresRange->startSubres.mipLevel + pSubresRange->numMips) > createInfo.mipLevels)
    {
        // The only way that we should have an SRD that references non-existent mip-levels is with PRT+ residency
        // maps.  The Microsoft spec creates residency maps with the same number of mip levels as the parent image
        // which is unnecessary in our implementation.  Doing so wastes memory, so DX12 created only a single mip
        // level residency map (i.e, ignoreed the API request).
        //
        // Unfortunately, the SRD created here went through DX12's "CreateSamplerFeedbackUnorderedAccessView" entry
        // point (which in turn went into PAL's "Gfx10UpdateLinkedResourceViewSrd" function), so we have a hybrid SRD
        // here that references both the map image and the parent image and thus has the "wrong" number of mip levels.
        //
        // Fix up the SRD here to reference the "correct" number of mip levels owned by the image.
        PAL_ASSERT(createInfo.prtPlus.mapType == PrtMapType::Residency);

        pSubresRange->startSubres.mipLevel = 0;
        pSubresRange->numMips              = 1;
    }
}

// =====================================================================================================================
// Check if for all the regions, the format and swizzle mode are compatible for src and dst image.
// If all regions are compatible, we can do a fixed function resolve. Otherwise return false.
bool Gfx10RsrcProcMgr::HwlCanDoDepthStencilCopyResolve(
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId srcSubResId = { imageRegion.srcAspect,
#else
            const SubresId srcSubResId = { imageRegion.srcPlane,
#endif
                                           0,
                                           imageRegion.srcSlice };
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const SubresId dstSubResId = { imageRegion.dstAspect,
#else
            const SubresId dstSubResId = { imageRegion.dstPlane,
#endif
                                           imageRegion.dstMipLevel,
                                           imageRegion.dstSlice };

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            PAL_ASSERT(imageRegion.srcAspect == imageRegion.dstAspect);
#else
            PAL_ASSERT(imageRegion.srcPlane == imageRegion.dstPlane);
#endif

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
void Gfx10RsrcProcMgr::HwlHtileCopyAndFixUp(
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
void Gfx10RsrcProcMgr::InitCmask(
    GfxCmdBuffer*      pCmdBuffer,
    Pal::CmdStream*    pCmdStream,
    const Image&       image,
    const SubresRange& range,
    const uint8        initValue
    ) const
{
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    PAL_ASSERT(range.numPlanes == 1);
#endif

    const auto&            boundMem        = image.Parent()->GetBoundGpuMemory();
    const GpuMemory*       pGpuMemory      = boundMem.Memory();
    const ImageCreateInfo& createInfo      = image.Parent()->GetImageCreateInfo();
    const Gfx9Cmask*       pCmask          = image.GetCmask();
    const auto&            cMaskAddrOut    = pCmask->GetAddrOutput();
    const uint32           expandedInitVal = ExpandClearCodeToDword(initValue);
    const uint32           startSlice      = ((createInfo.imageType == ImageType::Tex3d)
                                              ? 0
                                              : range.startSubres.arraySlice);

    // This is the byte offset from the start of the memory bound to this image
    const gpusize          cMaskOffset  = boundMem.Offset() + pCmask->MemoryOffset();

    // MSAA images can't have mipmaps
    PAL_ASSERT (createInfo.mipLevels == 1);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32  numSlices = GetClearDepth(image, range, 0);
#else
    const uint32  numSlices = GetClearDepth(image, range.startSubres.plane, range.numSlices, 0);
#endif
    const gpusize offset    = cMaskOffset + startSlice * cMaskAddrOut.sliceSize;

    CmdFillMemory(pCmdBuffer,
                  true,
                  *pGpuMemory,
                  offset,
                  numSlices * cMaskAddrOut.sliceSize,
                  expandedInitVal);
}

// =====================================================================================================================
// Use the compute engine to initialize hTile memory that corresponds to the specified clearRange
void Gfx10RsrcProcMgr::InitHtile(
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
    PAL_ASSERT(pCmdBuffer->IsComputeSupported());

    if (clearMask != 0)
    {
        const uint32  initValue = pHtile->GetInitialValue();
        const uint32  hTileMask = pHtile->GetPlaneMask(clearMask);

        InitHtileData(pCmdBuffer, dstImage, clearRange, initValue, hTileMask);
        FastDepthStencilClearComputeCommon(pCmdBuffer, pParentImg, clearMask);
    }
}

// =====================================================================================================================
void Gfx10RsrcProcMgr::HwlFixupCopyDstImageMetaData(
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

    // Copy to depth should go through gfx path and not to here.
    PAL_ASSERT(gfx9DstImage.Parent()->IsDepthStencilTarget() == false);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 562
    if (gfx9DstImage.HasDccData() && (dstImage.GetImageCreateInfo().flags.fullCopyDstOnly != 0))
    {
        auto*const pStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Compute);

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            const auto& clearRegion = pRegions[idx];
            const auto* pSubResInfo = dstImage.SubresourceInfo(clearRegion.subres);

            if (pSubResInfo->flags.supportMetaDataTexFetch == 0)
            {
                SubresRange range = {};

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
                range.startSubres.aspect     = clearRegion.subres.aspect;
#else
                range.startSubres.plane      = clearRegion.subres.plane;
#endif
                range.startSubres.mipLevel   = clearRegion.subres.mipLevel;
                range.startSubres.arraySlice = clearRegion.subres.arraySlice;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
                range.numPlanes              = 1;
#endif
                range.numMips                = 1;
                range.numSlices              = clearRegion.numSlices;

                // Since color data is no longer dcc compressed set Dcc to fully uncompressed.
                ClearDcc(pCmdBuffer, pStream, gfx9DstImage, range, Gfx9Dcc::InitialValue, DccClearPurpose::FastClear);
            }
        }
    }
#endif

    RsrcProcMgr::HwlFixupCopyDstImageMetaData(pCmdBuffer, pSrcImage, dstImage, dstImageLayout,
                                              pRegions, regionCount, isFmaskCopyOptimized);
}

// =====================================================================================================================
// For copies to non-local destinations, it is faster (although very unintuitive) to disable all but one of the RBs.
// All of the RBs banging away on the PCIE bus produces more traffic than the write-combiner can efficiently handle,
// so if we detect a write to non-local memory here, then disable RBs for the duration of the copy.  They will get
// restored in the HwlEndGraphicsCopy function.
uint32 Gfx10RsrcProcMgr::HwlBeginGraphicsCopy(
    Pal::GfxCmdBuffer*           pCmdBuffer,
    const Pal::GraphicsPipeline* pPipeline,
    const Pal::Image&            dstImage,
    uint32                       bpp
    ) const
{
    Pal::CmdStream*const pCmdStream   = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);
    const GpuMemory*     pGpuMem      = dstImage.GetBoundGpuMemory().Memory();
    const auto&          palDevice    = *(m_pDevice->Parent());
    const auto&          coreSettings = palDevice.Settings();
    uint32               modifiedMask = 0;

    if (pGpuMem != nullptr)
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
                const uint32 maxRbPerSc = (1 << defaultPaRegVal.gfx10Plus.NUM_RB_PER_SC);

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
                paScTileSteeringOverride.gfx10Plus.NUM_SC        = Log2(numNeededScs);

                // LOG2 of the effective NUM_RB_PER_SC desired. Must not be programmed to greater than the number of
                // active RBs per SC present in the chip.
                paScTileSteeringOverride.gfx10Plus.NUM_RB_PER_SC = Log2(numNeededRbsPerSc);

                // LOG2 of the effective NUM_PACKER_PER_SC desired. This is strictly for test purposes, otherwise
                // noramlly would be set to match the number of physical packers active in the design configuration.
                // Must not be programmed to greater than the number of active packers per SA (SC) present in the chip
                // configuration. Must be 0x1 if NUM_RB_PER_SC = 0x2.
                paScTileSteeringOverride.gfx101.NUM_PACKER_PER_SC =
                    Min(paScTileSteeringOverride.gfx10Plus.NUM_RB_PER_SC,
                        defaultPaRegVal.gfx101.NUM_PACKER_PER_SC);

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
void Gfx10RsrcProcMgr::HwlEndGraphicsCopy(
    Pal::CmdStream* pCmdStream,
    uint32          restoreMask
    ) const
{
    // Did HwlBeginGraphicsCopy do anything? If not, there's nothing to do here.
    if (TestAnyFlagSet(restoreMask, PaScTileSteeringOverrideMask))
    {
        CommitBeginEndGfxCopy(pCmdStream, m_pDevice->Parent()->ChipProperties().gfx9.paScTileSteeringOverride);
    }
}

// =====================================================================================================================
ImageCopyEngine Gfx10RsrcProcMgr::GetImageToImageCopyEngine(
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            if (region.dstSubres.aspect == ImageAspect::Stencil)
#else
            if (dstImage.IsStencilPlane(region.dstSubres.plane))
#endif
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
// Use compute for all non-local copies on gfx10.2+ because graphics copies that use a single RB (see
// HwlBeginGraphicsCopy) are no longer preferable for PCIE transfers.
bool Gfx10RsrcProcMgr::PreferComputeForNonLocalDestCopy(
    const Pal::Image& dstImage
    ) const
{
    const auto&  createInfo = dstImage.GetImageCreateInfo();

    bool preferCompute = false;

    const bool isMgpu = (m_pDevice->GetPlatform()->GetDeviceCount() > 1);

    if (IsGfx103Plus(*m_pDevice->Parent())                                        &&
        m_pDevice->Settings().nonLocalDestPreferCompute                           &&
        ((dstImage.IsDepthStencilTarget() == false) || (createInfo.samples == 1)) &&
        (isMgpu == false))
    {
        const GpuMemory* pGpuMem       = dstImage.GetBoundGpuMemory().Memory();
        const GpuHeap    preferredHeap = pGpuMem->PreferredHeap();

        if (((preferredHeap == GpuHeapGartUswc) || (preferredHeap == GpuHeapGartCacheable)) ||
            pGpuMem->IsPeer())
        {
            preferCompute = true;
        }
    }

    return preferCompute;
}

// =====================================================================================================================
// Updates hTile memory to reflect the VRS data supplied in the source image.
//
// Assumptions:  It is the callers responsibility to have bound a depth view that points to the supplied depth image!
//               This copy will work just fine if the depth image isn't bound, but the upcoming draw won't actually
//               utilize the newly copied VRS data if depth isn't bound.
void Gfx10RsrcProcMgr::CopyVrsIntoHtile(
    GfxCmdBuffer*                 pCmdBuffer,
    const Gfx10DepthStencilView*  pDsView,
    const Extent3d&               depthExtent,
    const Pal::Image*             pSrcVrsImg
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
    const Gfx9MetaEqGenerator* pEqGenerator = pHtile->GetMetaEqGenerator();

    PAL_ASSERT((pHtile != nullptr) && (pHtile->GetHtileUsage().vrs != 0));

    const Pal::Device*const pPalDevice = m_pDevice->Parent();
    Pal::CmdStream*const    pCmdStream = pCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::Graphics);

    // Step 1: The internal pre-CS barrier. The depth image is already bound as a depth view so if we just launch the
    // shader right away we risk corrupting HTile. We need to be sure that any prior draws that reference the depth
    // image are idle, HTile writes have been flushed down from the DB, and all stale HTile data has been invalidated
    // in the shader caches.
    uint32* pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(FLUSH_AND_INV_DB_META, pCmdBuffer->GetEngineType(), pCmdSpace);
    pCmdSpace += m_cmdUtil.BuildWaitOnReleaseMemEvent(pCmdBuffer->GetEngineType(),
                                                      BOTTOM_OF_PIPE_TS,
                                                      TcCacheOp::InvL1,
                                                      pCmdBuffer->TimestampGpuVirtAddr(),
                                                      pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);

    // The shader we're about to execute makes these assumptions in its source. If these trip we can add more support.
    PAL_ASSERT(pHtile->PipeAligned() != 0);
    PAL_ASSERT(pPalDevice->ChipProperties().gfx9.rbPlus != 0);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const uint32         pipeBankXor       = pHtile->GetPipeBankXor(pDepthImg->Parent()->GetBaseSubResource().aspect);
#else
    const uint32         pipeBankXor       = pHtile->GetPipeBankXor(0);
#endif

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
    userData.pitchInMetaBlks    = RpmUtil::PackBits<6>(hTileAddrOutput.pitch >> metaBlkExtentLog2.width);
    userData.lastHtileX         = RpmUtil::PackBits<11>(copyWidth - 1);
    userData.lastHtileY         = RpmUtil::PackBits<11>(copyHeight - 1);
    userData.fourBitVrs         = (m_pDevice->Settings().vrsHtileEncoding ==
                                   VrsHtileEncoding::Gfx10VrsHtileEncodingFourBit);

    // Step 2: Execute the rate image to VRS copy shader.
    const Pal::ComputePipeline*const pPipeline = GetPipeline(RpmComputePipeline::Gfx10VrsHtile);

    uint32  threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresRange  viewRange = { pSrcVrsImg->GetBaseSubResource(), 1, 1 };
#else
        const SubresRange  viewRange = { {}, 1, 1, 1 };
#endif
        ImageViewInfo      imageView = {};
        RpmUtil::BuildImageViewInfo(&imageView,
                                    *pSrcVrsImg,
                                    viewRange,
                                    pSrcVrsImg->GetImageCreateInfo().swizzledFormat,
                                    RpmUtil::DefaultRpmLayoutRead,
                                    pPalDevice->TexOptLevel());
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
        userData.sliceBits = slice & BitfieldGenMask<uint32>(10);

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
        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(copyWidth,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(copyHeight, threadsPerGroup[1]),
                                1);
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);

    // Step 3: The internal post-CS barrier. We must wait for the copy shader to finish. We invalidated the DB's
    // HTile cache in step 1 so we shouldn't need to touch the caches a second time.
    pCmdSpace = pCmdStream->ReserveCommands();
    pCmdSpace += m_cmdUtil.BuildWaitCsIdle(pCmdBuffer->GetEngineType(), pCmdBuffer->TimestampGpuVirtAddr(), pCmdSpace);
    pCmdStream->CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Gfx Dcc -> Display Dcc.
void Gfx10RsrcProcMgr::HwlGfxDccToDisplayDcc(
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
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const Gfx9Dcc*  pDcc              = pGfx9Image->GetDcc(pSubResInfo->subresId.aspect);
        const Gfx9Dcc*  pDispDcc          = pGfx9Image->GetDisplayDcc(pSubResInfo->subresId.aspect);
#else
        const Gfx9Dcc*  pDcc              = pGfx9Image->GetDcc(planeIdx);
        const Gfx9Dcc*  pDispDcc          = pGfx9Image->GetDisplayDcc(planeIdx);
#endif
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
        static const uint32 bufferSrdDwords       = NumBytesToNumDwords(sizeof(BufferSrd));
        static const uint32 inlineConstDataDwords = NumBytesToNumDwords(sizeof(inlineConstData));

        uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
            pCmdBuffer,
            bufferSrdDwords * BufferViewCount + inlineConstDataDwords,
            bufferSrdDwords,
            PipelineBindPoint::Compute,
            0);

        memcpy(pSrdTable, &bufferSrds[0], sizeof(bufferSrds));
        pSrdTable += bufferSrdDwords * BufferViewCount;
        memcpy(pSrdTable, &inlineConstData[0], sizeof(inlineConstData));

        uint32 threadsPerGroup[3] = {};
        pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

        // How many tiles in X/Y/Z dimension. One thread for each tile.
        const uint32 numBlockX = (pSubResInfo->extentTexels.width  + xInc - 1) / xInc;
        const uint32 numBlockY = (pSubResInfo->extentTexels.height + yInc - 1) / yInc;
        const uint32 numBlockZ = 1;

        uint32 numThreadGroupsX = RpmUtil::MinThreadGroups(numBlockX, threadsPerGroup[0]);
        uint32 numThreadGroupsY = RpmUtil::MinThreadGroups(numBlockY, threadsPerGroup[1]);
        uint32 numThreadGroupsZ = RpmUtil::MinThreadGroups(numBlockZ, threadsPerGroup[2]);

        pCmdBuffer->CmdDispatch(numThreadGroupsX, numThreadGroupsY, numThreadGroupsZ);
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

// =====================================================================================================================
void Gfx10RsrcProcMgr::InitDisplayDcc(
    GfxCmdBuffer*      pCmdBuffer,
    const Pal::Image&  image
    ) const
{
    const GpuMemory*       pGpuMemory  = image.GetBoundGpuMemory().Memory();
    const ImageCreateInfo& createInfo  = image.GetImageCreateInfo();
    const Image*           pGfx9Image  = static_cast<const Image*>(image.GetGfxImage());
    const uint32           clearValue  = ExpandClearCodeToDword(Gfx9Dcc::InitialValue);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*            pSubResInfo = image.SubresourceInfo(0);
    const Gfx9Dcc*         pDispDcc    = pGfx9Image->GetDisplayDcc(pSubResInfo->subresId.aspect);
#else
    const Gfx9Dcc*         pDispDcc    = pGfx9Image->GetDisplayDcc(0);
#endif

    const auto&            dispDccAddrOutput = pDispDcc->GetAddrOutput();

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    SubresRange range = { pSubResInfo->subresId, createInfo.mipLevels, createInfo.arraySize };
#else
    SubresRange range = { {}, 1, createInfo.mipLevels, createInfo.arraySize };
#endif

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

        // Initialize DCC for this mip level
        // The number of slices for 2D images is the number of slices; for 3D images, it's the depth
        // of the image for the current mip level.
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const uint32 numSlices = GetClearDepth(*pGfx9Image, range, absMipLevel);
#else
        const uint32 numSlices = GetClearDepth(*pGfx9Image, range.startSubres.plane, range.numSlices, absMipLevel);
#endif

        // The "metaBlkDepth" parameter is the number of slices that the "dccRamSliceSize" covers.  For non-3D
        // images, this should always be 1 (i.e., one addrlib slice is one PAL API slice).  For 3D images, this
        // can be way more than the number of PAL API slices.
        const uint32 numSlicesToClear = Max(1u, numSlices / dispDccAddrOutput.metaBlkDepth);

        for (uint32 sliceIdx = 0; sliceIdx < numSlicesToClear; sliceIdx++)
        {
            // GetMaskRamBaseAddr doesn't compute the base address of a mip level (only a slice offset), so
            // we have to do the math here ourselves.
            const uint32    absSlice      = range.startSubres.arraySlice + sliceIdx;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
            const gpusize   clearBaseAddr = pGfx9Image->GetMaskRamBaseAddr(pDispDcc, pSubResInfo->subresId) +
#else
            const gpusize   clearBaseAddr = pGfx9Image->GetMaskRamBaseAddr(pDispDcc, 0) +
#endif
                                            absSlice * dispDccAddrOutput.dccRamSliceSize +
                                            displayDccMipInfo.offset;
            const gpusize   clearOffset   = clearBaseAddr - pGpuMemory->Desc().gpuVirtAddr;

            CmdFillMemory(pCmdBuffer,
                false,            // don't save / restore the compute state
                *pGpuMemory,
                clearOffset,
                displayDccMipInfo.sliceSize,
                clearValue);
        }
    }

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 554
// =====================================================================================================================
void Gfx10RsrcProcMgr::CmdResolvePrtPlusImage(
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

    uint32  threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

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
            static_cast<uint32>(resolveRegion.srcOffset.x),
            static_cast<uint32>(resolveRegion.srcOffset.y),
            static_cast<uint32>(resolveRegion.srcOffset.z),
            0u,
            // start cb0[1]
            static_cast<uint32>(resolveRegion.dstOffset.x),
            static_cast<uint32>(resolveRegion.dstOffset.y),
            static_cast<uint32>(resolveRegion.dstOffset.z),
            0u,
            // start cb0[2]
            resolveRegion.extent.width,
            resolveRegion.extent.height,
            resolveRegion.extent.depth,
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId     srcSubResId = { ImageAspect::Color, resolveRegion.srcMipLevel, resolveRegion.srcSlice };
        const SubresRange  srcRange    = { srcSubResId, 1, resolveRegion.numSlices };
#else
        const SubresId     srcSubResId = { 0, resolveRegion.srcMipLevel, resolveRegion.srcSlice };
        const SubresRange  srcRange    = { srcSubResId, 1, 1, resolveRegion.numSlices };
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
        const SubresId     dstSubResId = { ImageAspect::Color, resolveRegion.dstMipLevel, resolveRegion.dstSlice };
        const SubresRange  dstRange    = { dstSubResId, 1, resolveRegion.numSlices };
#else
        const SubresId     dstSubResId = { 0, resolveRegion.dstMipLevel, resolveRegion.dstSlice };
        const SubresRange  dstRange    = { dstSubResId, 1, 1, resolveRegion.numSlices };
#endif

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
                                    pPalDevice->TexOptLevel());

        RpmUtil::BuildImageViewInfo(&imageView[1],
                                    dstPalImage,
                                    dstRange,
                                    dstFormat,
                                    dstImageLayout,
                                    pPalDevice->TexOptLevel());

        pPalDevice->CreateImageViewSrds(2, &imageView[0], pSrdTable);
        pSrdTable += Util::NumBytesToNumDwords(2 * sizeof(ImageSrd));

        // And give the shader all kinds of useful dimension info
        memcpy(pSrdTable, &constData[0], sizeof(constData));

        pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(resolveRegion.extent.width,  threadsPerGroup[0]),
                                RpmUtil::MinThreadGroups(resolveRegion.extent.height, threadsPerGroup[1]),
                                RpmUtil::MinThreadGroups(resolveRegion.extent.depth,  threadsPerGroup[2]));
    } // end loop through the regions

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}
#endif

// =====================================================================================================================
// Generate dcc lookup table.
void Gfx10RsrcProcMgr::BuildDccLookupTable(
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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 642
    const auto*        pBaseDcc          = pGfx9Image->GetDcc(range.startSubres.aspect);
#else
    const auto*        pBaseDcc          = pGfx9Image->GetDcc(range.startSubres.plane);
#endif
    PAL_ASSERT(pBaseDcc->HasMetaEqGenerator());
    const auto*        pEqGenerator      = pBaseDcc->GetMetaEqGenerator();
    const auto&        dccAddrOutput     = pBaseDcc->GetAddrOutput();
    const uint32       log2MetaBlkWidth  = Log2(dccAddrOutput.metaBlkWidth);
    const uint32       log2MetaBlkHeight = Log2(dccAddrOutput.metaBlkHeight);

    uint32 xInc = dccAddrOutput.compressBlkWidth;
    uint32 yInc = dccAddrOutput.compressBlkHeight;
    uint32 zInc = dccAddrOutput.compressBlkDepth;

    pBaseDcc->GetXyzInc(&xInc, &yInc, &zInc);

    const Pal::ComputePipeline* pPipeline = GetPipeline(RpmComputePipeline::Gfx10BuildDccLookupTable);

    uint32 threadsPerGroup[3] = {};
    pPipeline->ThreadsPerGroupXyz(&threadsPerGroup[0], &threadsPerGroup[1], &threadsPerGroup[2]);

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

    static const uint32 sizeBufferSrdDwords   = NumBytesToNumDwords(sizeof(BufferSrd));
    static const uint32 sizeEqConstDataDwords = NumBytesToNumDwords(sizeof(eqConstData));

    uint32* pSrdTable = RpmUtil::CreateAndBindEmbeddedUserData(
                            pCmdBuffer,
                            sizeBufferSrdDwords * BufferViewCount + sizeEqConstDataDwords,
                            sizeBufferSrdDwords,
                            PipelineBindPoint::Compute,
                            0);

    memcpy(pSrdTable, &bufferSrds[0], sizeof(BufferSrd) * BufferViewCount);
    pSrdTable += sizeBufferSrdDwords * BufferViewCount;
    memcpy(pSrdTable, &eqConstData[0], sizeof(eqConstData));

    pCmdBuffer->CmdDispatch(RpmUtil::MinThreadGroups(worksX, threadsPerGroup[0]),
                            RpmUtil::MinThreadGroups(worksY, threadsPerGroup[1]),
                            RpmUtil::MinThreadGroups(worksZ, threadsPerGroup[2]));

    pCmdBuffer->CmdRestoreComputeState(ComputeStatePipelineAndUserData);
}

} // Gfx9
} // Pal
