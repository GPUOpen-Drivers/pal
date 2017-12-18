/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "core/dmaCmdBuffer.h"
#include "core/hw/ossip/oss2_4/oss2_4Device.h"
#include "core/addrMgr/addrMgr1/addrMgr1.h"

// Forward decl's
struct SDMA_PKT_COPY_TILED_SUBWIN_TAG;

namespace Pal
{
namespace Oss2_4
{

// =====================================================================================================================
// OSS2.4 hardware-specific functionality for DMA command buffer execution.
class DmaCmdBuffer : public Pal::DmaCmdBuffer
{
public:
    DmaCmdBuffer(Device* pDevice, const CmdBufferCreateInfo& createInfo);

    virtual void CmdWriteTimestamp(HwPipePoint pipePoint, const IGpuMemory& dstGpuMemory, gpusize dstOffset) override;

    virtual void CmdWriteImmediate(
        HwPipePoint        pipePoint,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

protected:
    virtual ~DmaCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void ResetState() override;
    virtual uint32* WritePredicateCmd(size_t predicateDwords, uint32* pCmdSpace) const override;
    virtual void PatchPredicateCmd(size_t predicateDwords, void* pPredicateCmd) const override;

    virtual uint32* WriteCopyGpuMemoryCmd(
        gpusize      srcGpuAddr,
        gpusize      dstGpuAddr,
        gpusize      copySize,
        DmaCopyFlags copyFlags,
        uint32*      pCmdSpace,
        gpusize*     pBytesCopied) const override;

    virtual void    WriteCopyImageLinearToLinearCmd(const DmaImageCopyInfo& imageCopyInfo) override;
    virtual void    WriteCopyImageLinearToTiledCmd(const DmaImageCopyInfo& imageCopyInfo) override;
    virtual void    WriteCopyImageTiledToLinearCmd(const DmaImageCopyInfo& imageCopyInfo) override;
    virtual void    WriteCopyImageTiledToTiledCmd(const DmaImageCopyInfo& imageCopyInfo) override;

    virtual uint32* WriteCopyTypedBuffer(
        const DmaTypedBufferCopyInfo& dmaCopyInfo,
        uint32*                       pCmdSpace) const override;

    virtual uint32* WriteCopyMemToLinearImageCmd(
        const GpuMemory&             srcGpuMemory,
        const DmaImageInfo&          dstImage,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const override;

    virtual uint32* WriteCopyMemToTiledImageCmd(
        const GpuMemory&             srcGpuMemory,
        const DmaImageInfo&          dstImage,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const override
    {
        return CopyImageMemTiledTransform(dstImage, srcGpuMemory, rgn, false, pCmdSpace);
    }

    virtual uint32* WriteCopyLinearImageToMemCmd(
        const DmaImageInfo&          srcImage,
        const GpuMemory&             dstGpuMemory,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const override;

    virtual uint32* WriteCopyTiledImageToMemCmd(
        const DmaImageInfo&          srcImage,
        const GpuMemory&             dstGpuMemory,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const override
    {
        return CopyImageMemTiledTransform(srcImage, dstGpuMemory, rgn, true, pCmdSpace);
    }

    virtual uint32* WriteFillMemoryCmd(
        gpusize  dstAddr,
        gpusize  byteSize,
        uint32   data,
        uint32*  pCmdSpace,
        gpusize* pBytesCopied) const override;

    virtual uint32* WriteWaitEventSet(
        const GpuEvent& gpuEvent,
        uint32*         pCmdSpace) const override;

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, HwPipePoint pipePoint, uint32 data) override;

    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override;

    virtual gpusize GetSubresourceBaseAddr(const Image& image, const SubresId& subresource) const override;

    virtual bool UseT2tScanlineCopy(const DmaImageCopyInfo& imageCopyInfo) const override;

private:
    static uint32* CopyImageLinearTiledTransform(
        const DmaImageCopyInfo&  copyInfo,
        const DmaImageInfo&      linearImg,
        const DmaImageInfo&      tiledImg,
        bool                     deTile,
        uint32*                  pCmdSpace);

    static uint32* CopyImageMemTiledTransform(
        const DmaImageInfo&          image,
        const GpuMemory&             gpuMemory,
        const MemoryImageCopyRegion& rgn,
        bool                         deTile,
        uint32*                      pCmdSpace);

    static void SetDw6TilingInfo(const DmaImageInfo& imgInfo, SDMA_PKT_COPY_TILED_SUBWIN_TAG* pPacket);

    // Note that the linear pitches must be expressed in units of pixels, minus one.
    static uint32 GetLinearRowPitch(gpusize rowPitch, uint32 bytesPerPixel)
    {
        PAL_ASSERT(rowPitch % bytesPerPixel == 0);
        return static_cast<uint32>(rowPitch / bytesPerPixel) - 1;
    }

    static uint32 GetLinearDepthPitch(gpusize depthPitch, uint32 bytesPerPixel)
    {
        PAL_ASSERT(depthPitch % bytesPerPixel == 0);
        return static_cast<uint32>(depthPitch / bytesPerPixel) - 1;
    }

    static uint32 GetLinearRowPitch(const DmaImageInfo& imageInfo)
        { return GetLinearRowPitch(imageInfo.pSubresInfo->rowPitch, imageInfo.bytesPerPixel); }

    static uint32 GetLinearDepthPitch(const DmaImageInfo& imageInfo)
        { return GetLinearDepthPitch(imageInfo.pSubresInfo->depthPitch, imageInfo.bytesPerPixel); }

    static uint32 GetPitchTileMax(const DmaImageInfo& imageInfo)
        { return imageInfo.actualExtent.width / TileWidth - 1; }

    static uint32 GetSliceTileMax(const DmaImageInfo& imageInfo)
        { return (imageInfo.actualExtent.width * imageInfo.actualExtent.height) / TilePixels - 1; }

    void WriteTimestampCmd(gpusize dstAddr);

    PAL_DISALLOW_COPY_AND_ASSIGN(DmaCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(DmaCmdBuffer);
};

} // Oss2_4
} // Pal
