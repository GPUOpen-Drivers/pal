/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include "core/dmaCmdBuffer.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
//
// OSS5 (GFX10) hardware-specific functionality for DMA command buffer execution.
class DmaCmdBuffer final : public Pal::DmaCmdBuffer
{
public:
    static size_t GetSize(const Device& device) { return sizeof(DmaCmdBuffer); }

    static uint32* BuildNops(uint32* pCmdSpace, uint32 numDwords);

    DmaCmdBuffer(Device& device, const CmdBufferCreateInfo& createInfo);

    virtual void CmdUpdateMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           dataSize,
        const uint32*     pData) override;

    virtual void CmdWriteTimestamp(
        uint32            stageMask,
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset) override;

    virtual void CmdWriteImmediate(
        uint32             stageMask,
        uint64             data,
        ImmediateDataWidth dataSize,
        gpusize            address) override;

    virtual void CmdNop(const void* pPayload, uint32 payloadSize) override;

protected:
    virtual ~DmaCmdBuffer() {}

    virtual Result AddPreamble() override;
    virtual Result AddPostamble() override;

    virtual void SetupDmaInfoExtent(DmaImageInfo*  pImageInfo) const override;

    virtual uint32* WriteSetupInternalPredicateMemoryCmd(
        gpusize predMemAddress,
        uint32  predCopyData,
        uint32* pCmdSpace) const override;

    virtual uint32* WritePredicateCmd(uint32* pCmdSpace) const override;
    virtual void PatchPredicateCmd(uint32* pPredicateCmd, uint32* pCurCmdSpace) const override;

    virtual uint32* WriteCopyGpuMemoryCmd(
        gpusize      srcGpuAddr,
        gpusize      dstGpuAddr,
        gpusize      copySize,
        DmaCopyFlags copyFlags,
        uint32*      pCmdSpace,
        gpusize*     pBytesCopied) const override;

    virtual uint32* WriteCopyTypedBuffer(
        const DmaTypedBufferCopyInfo&   dmaCopyInfo,
        uint32*                         pCmdSpace) const override;

    virtual uint32* WriteCopyImageLinearToLinearCmd(const DmaImageCopyInfo& imageCopyInfo, uint32* pCmdSpace) override;
    virtual uint32* WriteCopyImageLinearToTiledCmd(const DmaImageCopyInfo& imageCopyInfo, uint32* pCmdSpace) override;
    virtual uint32* WriteCopyImageTiledToLinearCmd(const DmaImageCopyInfo& imageCopyInfo, uint32* pCmdSpace) override;
    virtual uint32* WriteCopyImageTiledToTiledCmd(const DmaImageCopyInfo& imageCopyInfo, uint32* pCmdSpace) override;

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

    virtual void WriteEventCmd(const BoundGpuMemory& boundMemObj, uint32 stageMask, uint32 data) override;

    virtual uint32* WriteNops(uint32* pCmdSpace, uint32 numDwords) const override;

    virtual gpusize GetSubresourceBaseAddr(const Pal::Image& image, const SubresId& subresource) const override;

    virtual uint32 GetLinearRowPitchAlignment(uint32 bytesPerPixel) const override;

protected:
    virtual bool UseT2tScanlineCopy(const DmaImageCopyInfo& imageCopyInfo) const override;

    virtual DmaMemImageCopyMethod GetMemImageCopyMethod(bool                         isLinearImg,
                                                        const DmaImageInfo&          imageInfo,
                                                        const MemoryImageCopyRegion& region) const override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(DmaCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(DmaCmdBuffer);

    uint32 GetCachePolicy(Gfx10SdmaBypassMall  bypassFlag) const;
    bool   GetMallBypass(Gfx10SdmaBypassMall  bypassFlag) const;

    uint32 GetCpvFromLlcPolicy(uint32  llcPolicy) const;
    uint32 GetCpvFromCachePolicy(uint32  cachePolicy) const;

    uint32* CopyImageLinearTiledTransform(
        const DmaImageCopyInfo&  copyInfo,
        const DmaImageInfo&      linearImg,
        const DmaImageInfo&      tiledImg,
        bool                     deTile,
        uint32*                  pCmdSpace) const;

    uint32* CopyImageMemTiledTransform(
        const DmaImageInfo&          image,
        const GpuMemory&             gpuMemory,
        const MemoryImageCopyRegion& rgn,
        bool                         deTile,
        uint32*                      pCmdSpace) const;

    static uint32           GetHwDimension(const DmaImageInfo&  dmaImageInfo);
    static uint32           GetMaxMip(const DmaImageInfo&  dmaImageInfo);
    static AddrSwizzleMode  GetSwizzleMode(const DmaImageInfo&  dmaImageInfo);
    static uint32           GetPipeBankXor(const Pal::Image&  image, const SubresId& subresource);

    template <typename PacketName>
    static void SetupMetaData(const DmaImageInfo&  image, PacketName*  pPacket, bool  imageIsDst);

    static bool ImageHasMetaData(const DmaImageInfo& imageInfo);
    uint32* WriteCondExecCmd(uint32* pCmdSpace, gpusize predMemory, uint32 skipCountInDwords) const;
    uint32* WriteFenceCmd(uint32* pCmdSpace, gpusize memory, uint32 predCopyData) const;

    uint32 GetImageZ(const DmaImageInfo&  dmaImageInfo, uint32  offsetZ) const;

    uint32 GetImageZ(const DmaImageInfo&  dmaImageInfo) const
        { return GetImageZ(dmaImageInfo, dmaImageInfo.offset.z); }

    uint32 GetLinearRowPitch(gpusize rowPitch, uint32 bytesPerPixel) const;

    void ValidateLinearRowPitch(gpusize rowPitchInBytes, gpusize height, uint32 bytesPerPixel) const;

    static uint32 GetLinearDepthPitch(gpusize depthPitch, uint32 bytesPerPixel)
    {
        PAL_ASSERT(depthPitch % bytesPerPixel == 0);

        // Note that the linear pitches must be expressed in units of pixels, minus one.
        return static_cast<uint32>(depthPitch / bytesPerPixel) - 1;
    }

    uint32 GetLinearRowPitch(const DmaImageInfo& imageInfo) const
    {
        ValidateLinearRowPitch(imageInfo.pSubresInfo->rowPitch,
                               imageInfo.extent.height,
                               imageInfo.bytesPerPixel);

        return GetLinearRowPitch(imageInfo.pSubresInfo->rowPitch, imageInfo.bytesPerPixel);
    }

    static uint32 GetLinearDepthPitch(const DmaImageInfo& imageInfo)
        { return GetLinearDepthPitch(imageInfo.pSubresInfo->depthPitch, imageInfo.bytesPerPixel); }

    static uint32*  BuildUpdateMemoryPacket(
        gpusize        dstAddr,
        uint32         dwordsToWrite,
        const uint32*  pSrcData,
        uint32*        pCmdSpace);
    static uint32* UpdateImageMetaData(
        const DmaImageInfo&  image,
        uint32*              pCmdSpace);

    void WriteTimestampCmd(gpusize dstAddr);
};

} // Oss4
} // Pal
