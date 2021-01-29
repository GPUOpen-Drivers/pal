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

#pragma once

#include "core/hw/ossip/ossDevice.h"
#include "core/cmdBuffer.h"
#include "core/cmdStream.h"

namespace Pal
{

// Forward decl's
class GpuEvent;
class GpuMemory;
class IImage;

// Bitmask values that can be ORed together to specify special properties of DMA copies.
enum DmaCopyFlags : uint32
{
    None       = 0x00000000,  ///< No flags specified
    TmzCopy    = 0x00000002,  ///< Whether the copy source is in TMZ memory. Results are undefined if the destination
                              ///  is not in TMZ memory.
};

// DmaImageInfo contains all necessary information about a single subresource for an image copy.
struct DmaImageInfo
{
    const IImage*          pImage;      // Everything there is to know about the image
    const SubResourceInfo* pSubresInfo; // The image subresource that this structure is referring to.
    gpusize                baseAddr;    // The GPU virt addr of this subresource with any tile swizzle applied.

    // The following information is in terms of either texels or elements, the units are irrelevant to the DMA engine
    // as long as all values use the same units and the value of bytesPerPixel indicates the size of the unit.

    Offset3d    offset;        // Where to begin copying texels/elements within this subresource.
    Extent3d    extent;        // The user-defined size of this subresource in texels/elements.
    Extent3d    actualExtent;  // The padded size of this subresource in texels/elements.
    uint32      bytesPerPixel; // The size of each texel/element referred to above.
    ImageLayout imageLayout;
};

// DmaImageCopyInfo defines everything regarding an image to image copy. Note that the offset, extent, etc. within the
// source and destination image infos must be in terms of the same units (e.g., R32G32 texels or 32-bit elements).
struct DmaImageCopyInfo
{
    Extent3d     copyExtent; // The region being copied, in the units described above.
    DmaImageInfo src;        // Everything about the source image.
    DmaImageInfo dst;        // Everything about where it's going.
};

// DmaTypedBufferRegion defines information needed for writing copy commands in OSSIP DMA for typed buffers
struct DmaTypedBufferRegion
{
    gpusize   baseAddr;           // base address of region.
    uint32    bytesPerElement;    // bytes per element of the given format.
    uint32    linearRowPitch;     // Offset in elements between the same X position of two consecutive lines.
    uint32    linearDepthPitch;   // Offset in elements between the same X,Y position of two consecutive slices.

};

// Defines needed parameters for a typed buffer region copy. The offset, extent, etc. within the
// source and destination regions must be in terms of the same units (e.g., R32G32 texels or 32-bit elements).
struct DmaTypedBufferCopyInfo
{
    Extent3d             copyExtent; // Extent in-terms of elements (based on per-pixel or block size of format).
    DmaTypedBufferRegion src;
    DmaTypedBufferRegion dst;
    DmaCopyFlags         flags;
};

// =====================================================================================================================
// Abstract class for executing basic hardware-specific functionality common to OSSIP DMA command buffers.
class DmaCmdBuffer : public CmdBuffer
{
public:
    virtual Result Init(const CmdBufferInternalCreateInfo& internalInfo) override;

    virtual Result Begin(const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;

    virtual void CmdBarrier(const BarrierInfo& barrier) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 648
    virtual uint32 CmdRelease(
        const AcquireReleaseInfo& releaseInfo) override;

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    syncTokenCount,
        const uint32*             pSyncTokens) override;
#else
    virtual void CmdRelease(
        const AcquireReleaseInfo& releaseInfo,
        const IGpuEvent*          pGpuEvent) override;

    virtual void CmdAcquire(
        const AcquireReleaseInfo& acquireInfo,
        uint32                    gpuEventCount,
        const IGpuEvent*const*    ppGpuEvents) override;
#endif

    virtual void CmdReleaseThenAcquire(const AcquireReleaseInfo& barrierInfo) override;

    virtual void CmdCopyMemory(
        const IGpuMemory&       srcGpuMemory,
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions) override;

    virtual void CmdCopyImage(
        const IImage&          srcImage,
        ImageLayout            srcImageLayout,
        const IImage&          dstImage,
        ImageLayout            dstImageLayout,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        const Rect*            pScissorRect,
        uint32                 flags) override;

    virtual void CmdCopyMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const IImage&                dstImage,
        ImageLayout                  dstImageLayout,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override;

    virtual void CmdCopyImageToMemory(
        const IImage&                srcImage,
        ImageLayout                  srcImageLayout,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions) override;

    virtual void CmdCopyMemoryToTiledImage(
        const IGpuMemory&                 srcGpuMemory,
        const IImage&                     dstImage,
        ImageLayout                       dstImageLayout,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override;

    virtual void CmdCopyTiledImageToMemory(
        const IImage&                     srcImage,
        ImageLayout                       srcImageLayout,
        const IGpuMemory&                 dstGpuMemory,
        uint32                            regionCount,
        const MemoryTiledImageCopyRegion* pRegions) override;

    virtual void CmdCopyTypedBuffer(
        const IGpuMemory&            srcGpuMemory,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const TypedBufferCopyRegion* pRegions) override;

    virtual void CmdFillMemory(
        const IGpuMemory& dstGpuMemory,
        gpusize           dstOffset,
        gpusize           fillSize,
        uint32            data) override;

    virtual void CmdSetPredication(
        IQueryPool*         pQueryPool,
        uint32              slot,
        const IGpuMemory*   pGpuMemory,
        gpusize             offset,
        PredicateType       predType,
        bool                predPolarity,
        bool                waitResults,
        bool                accumulateData) override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    // Increments the submit-count of the command stream(s) contained in this command buffer.
    virtual void IncrementSubmitCount() override
        { m_cmdStream.IncrementSubmitCount(); }

#if PAL_ENABLE_PRINTS_ASSERTS
    // This function allows us to dump the contents of this command buffer to a file at submission time.
    virtual void DumpCmdStreamsToFile(Util::File* pFile, CmdBufDumpFormat mode) const override;
#endif

    // Returns the number of command streams associated with this command buffer.
    virtual uint32 NumCmdStreams() const override { return 1; }

    // Returns a pointer to the command stream specified by "cmdStreamIdx".
    virtual const CmdStream* GetCmdStream(uint32 cmdStreamIdx) const override
    {
        PAL_ASSERT(cmdStreamIdx < NumCmdStreams());
        return &m_cmdStream;
    }

    void CmdCopyImageToPackedPixelImage(
        const IImage&           srcImage,
        const IImage&           dstImage,
        uint32                  regionCount,
        const ImageCopyRegion*  pRegions,
        Pal::PackedPixelType    packPixelType) override
    {
        PAL_NEVER_CALLED();
    }

    virtual void CmdUpdateBusAddressableMemoryMarker(
        const IGpuMemory& dstGpuMemory,
        gpusize           offset,
        uint32            value) override;

    virtual uint32 GetUsedSize(CmdAllocType type) const override;

protected:
    enum class DmaMemImageCopyMethod
    {
        Native,
        DwordUnaligned
    };

    DmaCmdBuffer(Device* pDevice, const CmdBufferCreateInfo& createInfo, uint32 copyOverlapHazardSyncs);
    virtual ~DmaCmdBuffer() {}

    virtual void SetupDmaInfoExtent(DmaImageInfo*  pImageInfo) const;

    static  bool IsAlignedForT2t(const Extent3d&  appData, const Extent3d&  alignment);
    static  bool IsAlignedForT2t(const Offset3d&  appData, const Extent3d&  alignment);

    // Returns true for situations where the WriteCopyImageTiledToTiledCmd function won't work.
    virtual bool UseT2tScanlineCopy(const DmaImageCopyInfo& imageCopyInfo) const = 0;

    virtual uint32* WritePredicateCmd(size_t predicateDwords, uint32* pCmdSpace) const = 0;
    virtual void PatchPredicateCmd(size_t predicateDwords, void* pPredicateCmd) const = 0;

    virtual void WriteCopyImageTiledToTiledCmdChunkCopy(const DmaImageCopyInfo& imageCopyInfo);

    void AllocateEmbeddedT2tMemory();

    bool HandleImageTransition(
        const IImage* pImage,
        ImageLayout   oldLayout,
        ImageLayout   newLayout,
        SubresRange   subresRange);

    virtual DmaMemImageCopyMethod GetMemImageCopyMethod(
        bool                         linearImg,
        const DmaImageInfo&          imageInfo,
        const MemoryImageCopyRegion& region) const = 0;

    static bool AreMemImageXParamsDwordAligned(
        const DmaImageInfo&          imageInfo,
        const MemoryImageCopyRegion& region);

    void CopyMemoryRegion(const IGpuMemory& srcGpuMemory,
                          const IGpuMemory& dstGpuMemory,
                          const MemoryCopyRegion& region);

    virtual void WriteCopyMemImageDwordUnalignedCmd(
        bool                         memToImg,
        bool                         linearImg,
        const GpuMemory&             srcGpuMemory,
        const DmaImageInfo&          dstImage,
        const MemoryImageCopyRegion& rgn);

    virtual uint32* WriteCopyGpuMemoryCmd(
        gpusize      srcGpuAddr,
        gpusize      dstGpuAddr,
        gpusize      copySize,
        DmaCopyFlags copyFlags,
        uint32*      pCmdSpace,
        gpusize*     pBytesCopied) const = 0;

    virtual uint32* WriteCopyTypedBuffer(const DmaTypedBufferCopyInfo& dmaCopyInfo, uint32* pCmdSpace) const = 0;
    virtual void    WriteCopyImageLinearToLinearCmd(const DmaImageCopyInfo& imageCopyInfo) = 0;
    virtual void    WriteCopyImageLinearToTiledCmd(const DmaImageCopyInfo& imageCopyInfo) = 0;
    virtual void    WriteCopyImageTiledToLinearCmd(const DmaImageCopyInfo& imageCopyInfo) = 0;
    virtual void    WriteCopyImageTiledToTiledCmd(const DmaImageCopyInfo& imageCopyInfo) = 0;

    virtual uint32* WriteCopyMemToLinearImageCmd(
        const GpuMemory&             srcGpuMemory,
        const DmaImageInfo&          dstImage,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const = 0;

    virtual uint32* WriteCopyMemToTiledImageCmd(
        const GpuMemory&             srcGpuMemory,
        const DmaImageInfo&          dstImage,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const = 0;

    virtual uint32* WriteCopyLinearImageToMemCmd(
        const DmaImageInfo&          srcImage,
        const GpuMemory&             dstGpuMemory,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const = 0;

    virtual uint32* WriteCopyTiledImageToMemCmd(
        const DmaImageInfo&          srcImage,
        const GpuMemory&             dstGpuMemory,
        const MemoryImageCopyRegion& rgn,
        uint32*                      pCmdSpace) const = 0;

    virtual uint32* WriteFillMemoryCmd(
        gpusize  dstAddr,
        gpusize  byteSize,
        uint32   data,
        uint32*  pCmdSpace,
        gpusize* pBytesCopied) const = 0;

    virtual uint32* WriteWaitEventSet(
        const GpuEvent& gpuEvent,
        uint32*         pCmdSpace) const = 0;

    virtual Result BeginCommandStreams(CmdStreamBeginFlags cmdStreamFlags, bool doReset) override;

    virtual gpusize GetSubresourceBaseAddr(const Image& image, const SubresId& subresource) const = 0;

    virtual uint32 GetLinearRowPitchAlignment(uint32 bytesPerPixel) const = 0;

    virtual void P2pBltWaCopyNextRegion(gpusize chunkAddr) override
        { CmdBuffer::P2pBltWaCopyNextRegion(&m_cmdStream, chunkAddr); }

    static ImageType GetImageType(const IImage&  image);

    static bool IsImageTmzProtected(const DmaImageInfo& imageInfo)
    {
        const BoundGpuMemory& gpuMemory = static_cast<const Image*>(imageInfo.pImage)->GetBoundGpuMemory();
        return gpuMemory.IsBound() ? gpuMemory.Memory()->IsTmzProtected() : false;
    }

    Device*const m_pDevice;
    CmdStream    m_cmdStream;
    bool         m_predMemEnabled;           // Memory predication is enabled.
    const uint32 m_copyOverlapHazardSyncs;   // Bitmask that depons on image type (1D, 2D or 3D). The bit is set to 1
                                             // if we need to handle overlapping copy syncing during CmdBarrier.
    gpusize      m_predMemAddress;           // Memory predication will reference this address.
    gpusize      m_predInternalAddr;         // Internal Memory predication will reference this address.
    uint32       m_predCopyData;             // Predication copy data for "write data" cmd.

private:
    void SetupDmaInfoSurface(
        const IImage&     image,
        const SubresId&   subresource,
        const Offset3d&   offset,
        const ImageLayout imageLayout,
        DmaImageInfo*     pImageInfo,
        uint32*           pTexelScale) const;

    void SetupDmaTypedBufferCopyInfo(
        const IGpuMemory&       baseAddr,
        const TypedBufferInfo&  region,
        DmaTypedBufferRegion*   pCopyInfo,
        uint32*                 pTexelScale) const;

    GpuMemory*   m_pT2tEmbeddedGpuMemory;    // Temp memory used for scanline tile-to-tile copies.
    gpusize      m_t2tEmbeddedMemOffset;

    PAL_DISALLOW_COPY_AND_ASSIGN(DmaCmdBuffer);
    PAL_DISALLOW_DEFAULT_CTOR(DmaCmdBuffer);
};

} // Pal
