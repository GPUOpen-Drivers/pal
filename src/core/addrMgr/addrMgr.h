/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "addrinterface.h"
#include "palDevice.h"
#include "palImage.h"

namespace Pal
{

class  Device;
class  Image;
struct SubResourceInfo;
struct SwizzleEquation;

// =====================================================================================================================
// Base class for abstracting address library support.
class AddrMgr
{
public:
    virtual Result Init();

    // Initializes the subresource properties for an Image object.
    virtual Result InitSubresourcesForImage(
        Image*             pImage,
        gpusize*           pGpuMemSize,
        gpusize*           pGpuMemAlignment,
        ImageMemoryLayout* pGpuMemLayout,
        SubResourceInfo*   pSubResInfoList,
        void*              pSubResTileInfoList,
        bool*              pDccUnsupported) const = 0;

    // Computes information about a PRT Image's packed mip tail.
    void ComputePackedMipInfo(
        const Image&       image,
        ImageMemoryLayout* pGpuMemLayout) const;

    // Destroys the AddrMgr object without freeing the system memory the object occupies.
    void Destroy() { this->~AddrMgr(); }

    ADDR_HANDLE AddrLibHandle() const { return m_hAddrLib; }
    const Device* GetDevice() const { return m_pDevice; }

    const SwizzleEquation* SwizzleEquations() const { return m_pSwizzleEquations; }
    uint32 NumSwizzleEquations() const { return m_numSwizzleEquations; }

    // Returns the size, in bytes, of the amount of per-subresource tiling information needed.
    size_t TileInfoBytes() const { return m_tileInfoBytes; }

    // Returns the tile swizzle value for a particular subresource of an Image.
    virtual uint32 GetTileSwizzle(const Image* pImage, SubresId subresource) const = 0;

    virtual uint32 GetBlockSize(AddrSwizzleMode swizzleMode) const
    {
        PAL_NEVER_CALLED();
        return 0;
    }

protected:
    AddrMgr(
        const Device* pDevice,
        size_t        tileInfoBytes);

    virtual ~AddrMgr();

    uint32 CalcBytesPerElement(const SubResourceInfo* pSubResInfo) const;

    // Computes the size (in PRT tiles) of the mip tail for a particular Image aspect.
    virtual void ComputeTilesInMipTail(
        const Image&       image,
        ImageAspect        aspect,
        ImageMemoryLayout* pGpuMemLayout) const = 0;

    // Determine the 0-based plane index of a given aspect
    uint32 PlaneIndex(ImageAspect aspect) const
    {
        return ((aspect == ImageAspect::Stencil) ||
                (aspect == ImageAspect::CbCr) ||
                (aspect == ImageAspect::Cb)) ? 1 : ((aspect == ImageAspect::Cr) ? 2 : 0);
    }

    const Device*const  m_pDevice;
    const GfxIpLevel    m_gfxLevel;

private:
    ADDR_HANDLE         m_hAddrLib;
    SwizzleEquation*    m_pSwizzleEquations;        // List of swizzle equations supported by the Device
    uint32              m_numSwizzleEquations;      // Number of supporter swizzle equations

    const size_t        m_tileInfoBytes;            // Per-subresource stride used for tiling information

    PAL_DISALLOW_DEFAULT_CTOR(AddrMgr);
    PAL_DISALLOW_COPY_AND_ASSIGN(AddrMgr);
};

// =====================================================================================================================
// Helper class which iterates over the subresources in an Image. This is used by the child classes of AddrMgr when
// initializing subresources for an Image.
class SubResIterator
{
public:
    explicit SubResIterator(
        const Image& image);
    ~SubResIterator() { }

    bool Next();

    uint32 Index() const { return m_subResIndex; }

    // Subresource index for the base mipmap level within the current array slice & aspect.
    uint32 BaseIndex() const { return m_baseSubResIndex; }

private:
    const Image&  m_image;

    uint32  m_plane;
    uint32  m_mipLevel;
    uint32  m_arraySlice;
    uint32  m_subResIndex;
    uint32  m_baseSubResIndex;

    PAL_DISALLOW_DEFAULT_CTOR(SubResIterator);
    PAL_DISALLOW_COPY_AND_ASSIGN(SubResIterator);
};

namespace AddrMgr1
{
// Called to instantiate and initialize an Address Manager
extern Result Create(
    const Device*  pDevice,
    void*          pPlacementAddr,
    AddrMgr**      ppAddrMgr);
// Returns the size, in bytes, of an address manager
extern size_t GetSize();
}

namespace AddrMgr2
{
// Called to instantiate and initialize an Address Manager
extern Result Create(
    const Device*  pDevice,
    void*          pPlacementAddr,
    AddrMgr**      ppAddrMgr);
// Returns the size, in bytes, of an address manager
extern size_t GetSize();
}

} // Pal
