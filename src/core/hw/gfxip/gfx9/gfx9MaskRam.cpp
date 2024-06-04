/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "palMath.h"
#include "palAutoBuffer.h"
#include "palIterator.h"
#include "palGpuMemory.h"

#include <limits.h>

using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx9;
using namespace Pal::AddrMgr2;

namespace Pal  {
namespace Gfx9 {

//=============== Implementation for Gfx9MaskRam: ======================================================================

// =====================================================================================================================
// Constructor for the Gfx9MaskRam class
Gfx9MaskRam::Gfx9MaskRam(
    const Image&  image,
    void*         pPlacementAddr,
    int32         metaDataSizeLog2,
    uint32        firstUploadBit)
    :
    m_pEqGenerator(nullptr),
    m_image(image),
    m_pGfxDevice(static_cast<const Device*>(image.Parent()->GetDevice()->GetGfxDevice()))
{
    if (pPlacementAddr != nullptr)
    {
        m_pEqGenerator =  PAL_PLACEMENT_NEW(pPlacementAddr) Gfx9MetaEqGenerator(this, metaDataSizeLog2, firstUploadBit);
    }
}

// =====================================================================================================================
ADDR2_META_FLAGS Gfx9MaskRam::GetMetaFlags() const
{
    const Pal::Image*const  pParent    = m_image.Parent();
    const Pal::Device*      pDevice    = pParent->GetDevice();
    const Device*           pGfxDevice = static_cast<Device*>(pDevice->GetGfxDevice());

    ADDR2_META_FLAGS  metaFlags = {};

    // Pipe aligned surfaces are aligned for optimal access from the texture block.  All our surfaces are texture
    // fetchable as anything can be copied through RPM.
    // For case MSAA Z/MSAA color/Stencil, metadata is not pipe aligned.
    metaFlags.pipeAligned = PipeAligned();
    metaFlags.rbAligned   = pGfxDevice->IsRbAligned();

    return metaFlags;
}

// =====================================================================================================================
uint32 Gfx9MaskRam::GetBytesPerPixelLog2() const
{
    const SubResourceInfo*const pBaseSubResInfo = m_image.Parent()->SubresourceInfo(0);
    return Log2(BitsPerPixel(pBaseSubResInfo->format.format) / 8);
}

// =====================================================================================================================
AddrSwizzleMode Gfx9MaskRam::GetSwizzleMode() const
{
    // We always want to use the swizzle mode associated with the first sub-resource.
    //   1) For color images, the swizzle mode is constant across all sub-resources.
    //   2) For depth+stencil images, the meta equation is generated based on the swizzle mode of the depth plane
    //      (which will always be the first plane).
    //   3) For stencil-only or Z-only images, there is only one plane, so it will be first.
    const SubResourceInfo*const pBaseSubResInfo = m_image.Parent()->SubresourceInfo(0);

    return m_image.GetAddrSettings(pBaseSubResInfo).swizzleMode;
}

// =====================================================================================================================
gpusize Gfx9MaskRam::SliceOffset(
    uint32  arraySlice
    ) const
{
    // Base implementation only "works" for slice 0.
    PAL_ASSERT(arraySlice == 0);

    return 0;
}

// =====================================================================================================================
// Populates a buffer view info object which wraps the mask-ram sub-allocation
void Gfx9MaskRam::BuildSurfBufferView(
    BufferViewInfo*  pViewInfo    // [out] The buffer view
    ) const
{
    const auto* pPublicSettings = m_pGfxDevice->Parent()->GetPublicSettings();

    pViewInfo->gpuAddr        = m_image.Parent()->GetBoundGpuMemory().GpuVirtAddr() + MemoryOffset();
    pViewInfo->range          = TotalSize();
    pViewInfo->stride         = 1;
    pViewInfo->swizzledFormat = UndefinedSwizzledFormat;
    pViewInfo->flags.bypassMallRead =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pViewInfo->flags.bypassMallWrite =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Retrieves the pipe-bank xor setting for the image associated with this mask ram.
uint32 Gfx9MaskRam::GetPipeBankXor(
    uint32 plane
    ) const
{
    // Check for cMask; cMask can't be here as the pipe-bank xor setting for cMask is the pipe-bank xor setting
    // associated with the fMask surface.  cMask needs to override this function.
    PAL_ASSERT (IsColor() || IsDepth());

    // The pipeBankXor setting for an image is expected to be a constant across all mips / slices of one plane.
    const auto*    pParent      = m_image.Parent();
    const SubresId baseSubResId = { plane, 0, 0 };

    const uint32   pipeBankXor  = AddrMgr2::GetTileInfo(pParent, baseSubResId)->pipeBankXor;

    return AdjustPipeBankXorForSwizzle(pipeBankXor);
}

// =====================================================================================================================
// The supplied pipeBankXor is the pipe-bank-xor value associated with the "owner" of this mask-ram surface.  That is,
// image data owns DCC memory and fMask owns cMask memory.  The owner's pipe-bank-xor value might not be compatible
// with the tiling (swizzle) mode of the mask-ram, so perform any necessary adjustments here.
uint32 Gfx9MaskRam::AdjustPipeBankXorForSwizzle(
    uint32  pipeBankXor
    ) const
{
    const auto&  chipProps = m_pGfxDevice->Parent()->ChipProperties();

    if (IsGfx10(chipProps.gfxLevel))
    {
        // On GFX10, the mask ram and the image itself might have different tile block sizes (i.e., usually the
        // image will be 64kB, but the meta data will usually be 4kB).  For a 64kB block image, the low 16 bits
        // will always be zero, but for a 4kB block image, only the low 12 bits will be zero.  The low eight
        // bits are never programmed (i.e., assumed by HW to be zero), so we really have:
        //    64kB = low 16 bits are zero --> 8 bits for pipeBankXor
        //     4kB = low 12 bits are zero --> 4 bits for pipeBankXor
        //
        // The "alignment" parameter of the mask ram essentially defines the block size of the mask-ram.
        // The low eight bits are never programmed and assumed by HW to be zero
        //
        const uint32  numBitsForPipeBankXor = LowPart(Util::Log2(Alignment())) - 8;
        const uint32  pipeBankXorMask       = ((1 << numBitsForPipeBankXor) - 1);

        // Whack off any bits that we can't use.
        pipeBankXor = pipeBankXor & pipeBankXorMask;
    }

    return pipeBankXor;
}

// =====================================================================================================================
// Mainly to use this function to get the block size.
// For calculating the meta-block dimensions(pExtent), GetMetaBlkSize in address library can provide the same result.
//
uint32 Gfx9MaskRam::GetMetaBlockSize(
    Gfx9MaskRamBlockSize* pExtent
    ) const
{
    PAL_ASSERT(HasMetaEqGenerator());

    uint32 blockSize = 0, blockBits = 0;

    const Pal::Device*     pPalDevice            = m_pGfxDevice->Parent();
    const AddrSwizzleMode  swizzleMode           = GetSwizzleMode();
    const uint32           bppLog2               = GetBytesPerPixelLog2();
    const uint32           numSamplesLog2        = GetNumSamplesLog2();
    uint32                 numPipesLog2          = m_pGfxDevice->GetNumPipesLog2();
    const uint32           numShaderArraysLog2   = IsGfx103PlusExclusive(*pPalDevice)
                                                   ? m_pEqGenerator->GetNumShaderArrayLog2()
                                                   : 0;
    const uint32           effectiveNumPipesLog2 = m_pEqGenerator->GetEffectiveNumPipes();
    const uint32           pipeInterleaveLog2    = m_pGfxDevice->GetPipeInterleaveLog2();
    const uint32           maxFragsLog2          = m_pGfxDevice->GetMaxFragsLog2();
    const uint32           maxCompFragsLog2      = Min(numSamplesLog2, maxFragsLog2);
    const uint32           metaElementSize       = m_pEqGenerator->GetMetaDataWordSizeLog2() - 1;
    const uint32           metaCachelineSize     = GetMetaCachelineSize();
    const uint32           pipeRotateAmount      = m_pEqGenerator->GetPipeRotateAmount();
    const uint32           compBlockSize         = IsColor() ? 8 : (6 + numSamplesLog2 + bppLog2);
    const uint32           samplesInMetaBlock    = m_pEqGenerator->IsZSwizzle(swizzleMode)
                                                    ? numSamplesLog2
                                                    : maxCompFragsLog2;

    if (IsGfx103Plus(*pPalDevice)                   &&
        pPalDevice->ChipProperties().gfx9.rbPlus    &&
        (numPipesLog2 == (numShaderArraysLog2 + 1)) &&
        (numPipesLog2 > 1))
    {
        numPipesLog2++;
    }

    if (numPipesLog2 >= 4)
    {
        blockSize = metaCachelineSize + m_pEqGenerator->GetMetaOverlap() + numPipesLog2;

        blockSize = Max(pipeInterleaveLog2 + numPipesLog2, blockSize);

        if ((m_pEqGenerator->GetPipeDist() == PipeDist16x16) &&
            IsRotatedSwizzle(swizzleMode) &&
            (numPipesLog2   == 6)         &&
            (numSamplesLog2 == 3)         &&
            (maxFragsLog2   == 3)         &&
            (blockSize      < 15))
        {
            blockSize = 15;
        }
    }
    else
    {
        blockSize = pipeInterleaveLog2 + numPipesLog2;
        blockSize = Max(12u, blockSize);
    }

    if (IsRotatedSwizzle(swizzleMode) &&
        (maxCompFragsLog2 > 1)        &&
        (pipeRotateAmount >= 1))
    {
        uint32 newBlockSize = 8 + numPipesLog2 + ((pipeRotateAmount > (maxCompFragsLog2 - 1)) ?
                                                    pipeRotateAmount : (maxCompFragsLog2 - 1));
        blockSize = Max(newBlockSize, blockSize);
    }

    blockBits = blockSize + compBlockSize - bppLog2 - samplesInMetaBlock - metaElementSize;

    pExtent->width  = (blockBits >> 1) + (blockBits & 1);
    pExtent->height = (blockBits >> 1);
    pExtent->depth  = 0;

    return blockSize;
}

// =====================================================================================================================
// Returns the dimensions, in pixels, of a block that gets compressed to one mask-ram equivalent unit.  This is easy
// for hTile and cMask.  DCC is a pain.
void Gfx9MaskRam::GetXyzInc(
    uint32*  pXinc, // [out] Num X pixels that get compressed into one mask-ram dword (htile) or nibble (cMask)
    uint32*  pYinc, // [out] Num Y pixels that get compressed into one mask-ram dword (htile) or nibble (cMask)
    uint32*  pZinc  // [out] Num Z pixels that get compressed into one mask-ram dword (htile) or nibble (cMask)
    ) const
{
    *pXinc = 8;
    *pYinc = 8;
    *pZinc = 1;
}

//=============== Implementation for Gfx9MetaEqGenerator: ==============================================================

// =====================================================================================================================
// Constructor for the Gfx9MetaEqGenerator class
Gfx9MetaEqGenerator::Gfx9MetaEqGenerator(
    const Gfx9MaskRam* pParent,
    int32              metaDataSizeLog2,
    uint32             firstUploadBit)
    :
    m_pipeDist(pParent->GetGfxDevice()->Parent()->ChipProperties().gfx9.rbPlus ? PipeDist16x16 : PipeDist8x8),
    m_meta(27),
    m_metaDataWordSizeLog2(metaDataSizeLog2),
    m_pParent(pParent),
    m_firstUploadBit(firstUploadBit),
    m_metaEquationValid(false),
    m_effectiveSamples(1), // assume single-sampled image
    m_rbAppendedWithPipeBits(0)
{
    memset(&m_eqGpuAccess,   0, sizeof(m_eqGpuAccess));
    memset(&m_metaEqParam,   0, sizeof(m_metaEqParam));

    static_assert(MetaDataAddrEquation::MaxNumMetaDataAddrBits  <= (sizeof(m_rbAppendedWithPipeBits) * 8),
                  "Must increase size of m_rbAppendedWithPipeBits storage!");
}

// =====================================================================================================================
// Builds a buffer view for accessing the meta equation from the GPU
void Gfx9MetaEqGenerator::BuildEqBufferView(
    BufferViewInfo*  pBufferView
    ) const
{
    PAL_ASSERT (m_eqGpuAccess.size != 0);
    const auto* pPublicSettings = m_pParent->GetGfxDevice()->Parent()->GetPublicSettings();

    pBufferView->swizzledFormat = UndefinedSwizzledFormat;
    pBufferView->stride         = MetaDataAddrCompNumTypes * sizeof(uint32);
    pBufferView->range          = (m_meta.GetNumValidBits() - m_firstUploadBit) *
                                  MetaDataAddrCompNumTypes                      *
                                  sizeof (uint32);
    pBufferView->gpuAddr        = m_pParent->GetImage().Parent()->GetGpuVirtualAddr() + m_eqGpuAccess.offset;
    pBufferView->flags.bypassMallRead =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnRead);
    pBufferView->flags.bypassMallWrite =
        TestAnyFlagSet(pPublicSettings->rpmViewsBypassMall, RpmViewsBypassMallOnWrite);
}

// =====================================================================================================================
// Calculates the data offset equation for this mask-ram.
void Gfx9MetaEqGenerator::CalcDataOffsetEquation(
    MetaDataAddrEquation* pDataOffset)
{
    const auto*            pParent        = m_pParent->GetImage().Parent();
    const auto*            pGfxDevice     = m_pParent->GetGfxDevice();
    const AddrSwizzleMode  swizzleMode    = m_pParent->GetSwizzleMode();
    const uint32           blockSizeLog2  = Log2(pGfxDevice->Parent()->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32           bppLog2        = m_pParent->GetBytesPerPixelLog2();
    const uint32           numSamplesLog2 = m_pParent->GetNumSamplesLog2();

    CompPair  cx = {};
    cx.compType = MetaDataAddrCompX;
    CompPair  cy = {};
    cy.compType = MetaDataAddrCompY;

    if (IsThick())
    {
        CompPair  cz = { MetaDataAddrCompZ, 0 };

        // Color 3d (_S and _Z modes; _D is same as color 2d)
        if (AddrMgr2::IsStandardSwzzle(swizzleMode))
        {
            // Standard 3d swizzle
            // Fill in bottom x bits
            for(uint32 bitPos = bppLog2; bitPos < 4; bitPos++)
            {
                pDataOffset->SetBit(bitPos, MetaDataAddrCompX, cx.compPos);
                cx.compPos++;
            }

            // Fill in 2 bits of y and then z
            for (uint32 bitPos = 4; bitPos < 6; bitPos++)
            {
                pDataOffset->SetBit(bitPos, MetaDataAddrCompY, cy.compPos);
                cy.compPos++;
            }

            for (uint32 bitPos = 6; bitPos < 8; bitPos++)
            {
                pDataOffset->SetBit(bitPos, MetaDataAddrCompZ, cz.compPos);
                cz.compPos++;
            }

            if (bppLog2 < 2)
            {
                // fill in z & y bit
                pDataOffset->SetBit(8, cz.compType, cz.compPos++);
                pDataOffset->SetBit(9, cy.compType, cy.compPos++);
            }
            else if (bppLog2 == 2)
            {
                // fill in y and x bit
                pDataOffset->SetBit(8, cy.compType, cy.compPos++);
                pDataOffset->SetBit(9, cx.compType, cx.compPos++);
            } else
            {
                // fill in 2 x bits
                pDataOffset->SetBit(8, cx.compType, cx.compPos++);
                pDataOffset->SetBit(9, cx.compType, cx.compPos++);
            }
        }
        else
        {
            // Z 3d swizzle
            const uint32 m2dEnd = (bppLog2 == 0) ? 3 : ((bppLog2 < 4) ? 4 : 5);
            const uint32 numZs  = ((bppLog2 == 0) || (bppLog2 == 4)) ? 2 : ((bppLog2 == 1) ? 3 : 1);

            pDataOffset->Mort2d(pGfxDevice, &cx, &cy, bppLog2, m2dEnd);
            for(uint32 bitPos = m2dEnd + 1; bitPos <= m2dEnd + numZs; bitPos++ )
            {
                pDataOffset->SetBit(bitPos, cz.compType, cz.compPos++);
            }

            if ((bppLog2 == 0) || (bppLog2 == 3))
            {
                // add an x and z
                pDataOffset->SetBit(6, cx.compType, cx.compPos++);
                pDataOffset->SetBit(7, cz.compType, cz.compPos++);
            }
            else if (bppLog2 == 2)
            {
                // add a y and z
                pDataOffset->SetBit(6, cy.compType, cy.compPos++);
                pDataOffset->SetBit(7, cz.compType, cz.compPos++);
            }

            // add y and x
            pDataOffset->SetBit(8, cy.compType, cy.compPos++);
            pDataOffset->SetBit(9, cx.compType, cx.compPos++);
        }

        // Fill in bit 10 and up
        pDataOffset->Mort3d(&cz, &cy, &cx, 10);
    }
    else if (m_pParent->IsColor())
    {
        // Color 2D
        const uint32  microYBits     = (8 - bppLog2) / 2;
        const uint32  tileSplitStart = blockSizeLog2 - numSamplesLog2;

        // Fill in bottom x bits
        for (uint32 i = bppLog2; i < 4; i++)
        {
            pDataOffset->SetBit(i, MetaDataAddrCompX, cx.compPos);
            cx.compPos++;
        }

        // Fill in bottom y bits
        for (uint32 i = 4; i < 4 + microYBits; i++)
        {
            pDataOffset->SetBit(i, MetaDataAddrCompY, cy.compPos);
            cy.compPos++;
        }

        // Fill in last of the micro_x bits
        for(uint32 i = 4 + microYBits; i < 8; i++)
        {
            pDataOffset->SetBit(i, MetaDataAddrCompX, cx.compPos);
            cx.compPos++;
        }

        // Fill in x/y bits below sample split
        pDataOffset->Mort2d(pGfxDevice, &cy, &cx, 8, tileSplitStart - 1);

        // Fill in sample bits
        for (uint32 bitPos = 0; bitPos < numSamplesLog2; bitPos++)
        {
            pDataOffset->SetBit(tileSplitStart + bitPos, MetaDataAddrCompS, bitPos);
        }

        // Fill in x/y bits above sample split
        if (((numSamplesLog2 & 1) ^ (blockSizeLog2 & 1)) != 0)
        {
            pDataOffset->Mort2d(pGfxDevice, &cx, &cy, blockSizeLog2);
        }
        else
        {
            pDataOffset->Mort2d(pGfxDevice, &cy, &cx, blockSizeLog2);
        }
    }
    else
    {
        // Z, stencil or fmask
        // First, figure out where each section of bits starts
        const uint32 pixelStart = bppLog2 + numSamplesLog2;
        const uint32 yMajStart  = 6 + numSamplesLog2;

        // Put in sample bits
        for(uint32 s = 0; s < numSamplesLog2; s++)
        {
            pDataOffset->SetBit(bppLog2 + s, MetaDataAddrCompS, s);
        }

        // Put in the x-major order pixel bits
        pDataOffset->Mort2d(pGfxDevice, &cx, &cy, pixelStart, yMajStart - 1);

        // Put in the y-major order pixel bits
        pDataOffset->Mort2d(pGfxDevice, &cy, &cx, yMajStart);
    }
}

// =====================================================================================================================
// Calculates the pipe equation for this mask-ram.
void Gfx9MetaEqGenerator::CalcPipeEquation(
    MetaDataAddrEquation* pPipe,
    MetaDataAddrEquation* pDataOffset,
    uint32                numPipesLog2)
{
    const int32            numSamplesLog2     = m_pParent->GetNumSamplesLog2();
    const AddrSwizzleMode  swizzleMode        = m_pParent->GetSwizzleMode();
    const Pal::Device*     pDevice            = m_pParent->GetGfxDevice()->Parent();
    const uint32           blockSizeLog2      = Log2(pDevice->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32           pipeInterleaveLog2 = m_pParent->GetGfxDevice()->GetPipeInterleaveLog2();

    CompPair  tileMin = { MetaDataAddrCompX, 3};
    MetaDataAddrEquation  dataOffsetLocal(pDataOffset->GetNumValidBits());

    // For color, filter out sample bits only
    // otherwise filter out everything under an 8x8 tile
    if (m_pParent->IsColor())
    {
        tileMin.compPos = 0;
    }

    pDataOffset->Copy(&dataOffsetLocal);
    // Z/stencil is no longer tile split
    if (m_pParent->IsColor() && (numSamplesLog2 > 0))
    {
        dataOffsetLocal.Shift(-numSamplesLog2, blockSizeLog2 - numSamplesLog2);
    }

    dataOffsetLocal.Copy(pPipe, pipeInterleaveLog2, numPipesLog2);

    // If the pipe bit is below the comp block size, then keep moving up the address until we find a bit that is above
    uint32 pipeBit = 0;
    while (true)
    {
        const CompPair localPair = dataOffsetLocal.Get(pipeInterleaveLog2 + pipeBit);
        if (MetaDataAddrEquation::CompareCompPair(localPair, tileMin, MetaDataAddrCompareLt))
        {
            pipeBit++;
        }
        else
        {
            break;
        }
    }

    // if pipe is 0, then the first pipe bit is above the comp block size, so we don't need to do anything
    if (pipeBit != 0)
    {
        uint32 j = pipeBit;
        for(uint32 i = 0; i < numPipesLog2; i++)
        {
            // Copy the jth bit above pipe interleave to the current pipe equation bit
            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                pPipe->ClearBits(i, compType, 0);
                pPipe->SetMask(i, compType, dataOffsetLocal.Get(pipeInterleaveLog2 + j, compType));
            }

            j++;
        }
    }

    // Clear out bits above the block size if prt's are enabled
    if (AddrMgr2::IsPrtSwizzle(swizzleMode))
    {
        for (uint32  bitPos = blockSizeLog2; bitPos < m_meta.GetNumValidBits(); bitPos++)
        {
            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                m_meta.ClearBits(bitPos, compType, 0);
            }
        }
    }

    if (AddrMgr2::IsXorSwizzle(swizzleMode) || AddrMgr2::IsPrtSwizzle(swizzleMode))
    {
        MetaDataAddrEquation  xorMask(numPipesLog2);
        MetaDataAddrEquation  xorMask2(numPipesLog2);

        if (IsThick())
        {
            dataOffsetLocal.Copy(&xorMask2, pipeInterleaveLog2 + numPipesLog2, 2 * numPipesLog2);
            for (uint32 localPipe = 0; localPipe < numPipesLog2; localPipe++)
            {
                for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
                {
                    xorMask.SetMask(localPipe, compType, xorMask2.Get(2 * localPipe,     compType));
                    xorMask.SetMask(localPipe, compType, xorMask2.Get(2 * localPipe + 1, compType));
                }
            }
        }
        else
        {
            // Xor in the bits above the pipe+gpu bits
            dataOffsetLocal.Copy(&xorMask, pipeInterleaveLog2 + pipeBit + numPipesLog2, numPipesLog2);
            if ((numSamplesLog2 == 0) && (AddrMgr2::IsPrtSwizzle(swizzleMode) == false))
            {
                // if 1xaa and not prt, then xor in the z bits
                for (uint32 localPipe = 0; localPipe < numPipesLog2; localPipe++)
                {
                    xorMask2.SetBit(localPipe, MetaDataAddrCompZ, numPipesLog2 - 1 - localPipe);
                }

                pPipe->XorIn(&xorMask2);
            }
        }

        xorMask.Reverse();

        pPipe->XorIn(&xorMask);
    }
}

// =====================================================================================================================
// Calculate the pipe-bank XOR value as used by the meta-data equation.
uint32 Gfx9MetaEqGenerator::CalcPipeXorMask(
    uint32 plane
    ) const
{
    const uint32  pipeInterleaveLog2 = m_pParent->GetGfxDevice()->GetPipeInterleaveLog2();
    const uint32  numPipesLog2       = CapPipe();
    const uint32  pipeBankXor        = m_pParent->GetPipeBankXor(plane);

    const uint32 pipeXorMaskNibble = (pipeBankXor & ((1 << numPipesLog2) - 1)) << (pipeInterleaveLog2 + 1);

    // Make sure all the bits that we expect to be able to ignore are zero!
    PAL_ASSERT ((pipeXorMaskNibble & ((1 << m_firstUploadBit) - 1)) == 0);

    // Ensure we either have a zero pipe-bank-xor value or we have a swizzle mode that supports non-zero XOR values.
    PAL_ASSERT((pipeXorMaskNibble == 0) || AddrMgr2::IsXorSwizzle(m_pParent->GetSwizzleMode()));

    // Our shaders always (eventually) compute byte addresses, so return this in terms of bytes for easy use by the CS
    return (pipeXorMaskNibble >> 1);
}

// =====================================================================================================================
uint32 Gfx9MetaEqGenerator::GetRbAppendedBit(
    uint32  bitPos
    ) const
{
    return ((m_rbAppendedWithPipeBits & (1u << bitPos)) >> bitPos);
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::SetRbAppendedBit(
    uint32  bitPos,
    uint32  bitVal)
{
    m_rbAppendedWithPipeBits &= ~(1u << bitPos);
    m_rbAppendedWithPipeBits |= (bitVal << bitPos);
}

// =====================================================================================================================
// Set the meta equation size to the minimum required to support the actual size of the mask-ram
void Gfx9MetaEqGenerator::FinalizeMetaEquation(
    gpusize  addressableSizeBytes)
{
    // The address is actually a nibble-address at this point, so multiply the maximum addressible size
    // by two to convert from bytes to nibbles.
    const uint32  requiredNumEqBits = Log2(Pow2Pad(addressableSizeBytes * 2));

    // The idea here is to *shrink* the equation to the number of bits required to actually address the meta-data
    // surface.  If the "SetEquationSize" call would instead *increase* the size of the equation, then something
    // has gone horribly wrong.
    PAL_ASSERT(requiredNumEqBits <= m_meta.GetNumValidBits());

    m_meta.SetEquationSize(requiredNumEqBits, false);

    // Determine how many sample bits are needed to process this equation.
    m_effectiveSamples = m_meta.GetNumSamples();

    // Mark this equation as valid
    m_metaEquationValid = true;
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::CalcRbEquation(
    MetaDataAddrEquation* pRb,
    uint32                numSesLog2,
    uint32                numRbsPerSeLog2)
{
    const Pal::Device*      pDevice         = m_pParent->GetGfxDevice()->Parent();
    const Gfx9PalSettings&  settings        = GetGfx9Settings(*pDevice);
    const uint32            numTotalRbsLog2 = numSesLog2 + numRbsPerSeLog2;

    // We will only ever have an X and a Y component, but it's easier to just declare an array
    // of all possible meta-data component types
    uint32  rbRegion[MetaDataAddrCompNumTypes] = {};

    // RB's are distributed on 16x16, except when we have 1 rb per se, in which case its 32x32
    rbRegion[MetaDataAddrCompX] = ((numRbsPerSeLog2 == 0) ? 5 : 4);
    rbRegion[MetaDataAddrCompY] = rbRegion[MetaDataAddrCompX];

    uint32 start = 0;
    if ((numSesLog2 > 0) && (numRbsPerSeLog2 == 1))
    {
        // Special case when more than 1 SE, and only 1 RB per SE
        pRb->SetBit(0, MetaDataAddrCompX, rbRegion[MetaDataAddrCompX]);
        pRb->SetBit(0, MetaDataAddrCompY, rbRegion[MetaDataAddrCompY]);

        rbRegion[MetaDataAddrCompX]++;
        rbRegion[MetaDataAddrCompY]++;
        start++;
    }

    for (uint32 i = 0; i < (2 * (numTotalRbsLog2 - start)); i++)
    {
        const uint32                     index    = start + (((start + i) >= numTotalRbsLog2)
                                                    ? (2 * (numTotalRbsLog2 - start) - i - 1)
                                                    : i);
        const MetaDataAddrComponentType  compType = (((i % 2) == 1) ? MetaDataAddrCompX : MetaDataAddrCompY);

        pRb->SetBit(index, compType, rbRegion[compType]);
        rbRegion[compType]++;
    }
}

// =====================================================================================================================
uint32 Gfx9MetaEqGenerator::CapPipe() const
{
    const AddrSwizzleMode  swizzleMode        = m_pParent->GetSwizzleMode();
    const auto*            pGfxDevice         = m_pParent->GetGfxDevice();
    const uint32           blockSizeLog2      = Log2(pGfxDevice->Parent()->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32           numSesLog2         = pGfxDevice->GetNumShaderEnginesLog2();
    const uint32           pipeInterleaveLog2 = pGfxDevice->GetPipeInterleaveLog2();

    uint32  numPipesLog2 = pGfxDevice->GetNumPipesLog2();

    // pipes+SEs can't exceed 32 for now
    PAL_ASSERT((numPipesLog2 + numSesLog2) <= 5);

    // Since we are not supporting SE affinity anymore, just add nu_ses to num_pipes
    numPipesLog2 += numSesLog2;

    return Min(blockSizeLog2 - pipeInterleaveLog2, numPipesLog2);
}

// =====================================================================================================================
// Initialize this object's "m_eqGpuAccess" with data used to eventually upload this equation to GPU accessible memory
void Gfx9MetaEqGenerator::InitEqGpuAccess(
    gpusize*      pGpuSize)
{
    if (IsMetaEquationValid())
    {
        // The GPU version of the meta equation will be accessed by a buffer view which can address any alignment.  Make
        // it dword-aligned here just to be nice.
        m_eqGpuAccess.offset = Pow2Align(*pGpuSize, sizeof(uint32));
        m_eqGpuAccess.size   = m_meta.GetGpuSize();

        *pGpuSize            = (m_eqGpuAccess.offset + m_eqGpuAccess.size);
    }
}

// =====================================================================================================================
// Returns true for swizzle modes that are the equivalent of the old "thick" tiling modes on pre-gfx9 HW.
bool Gfx9MetaEqGenerator::IsThick() const
{
    const auto&            createInfo  = m_pParent->GetImage().Parent()->GetImageCreateInfo();
    const AddrSwizzleMode  swizzleMode = m_pParent->GetSwizzleMode();

    return  ((createInfo.imageType == ImageType::Tex3d) &&
             (IsStandardSwzzle(swizzleMode) ||
              IsZSwizzle(swizzleMode)));
}

// =====================================================================================================================
// Iterate through each pipe bits from lsb to msb, and remove the smallest coordinate contributing to that bit's
// equation.  Remove these bits from the metadata address, and the RB equations.
//
//      The idea is this: we first start with the lsb of the rb_id, find the smallest component, and remove it from
//      the metadata address, and also from all upper rb_id bits that have this component.  For the rb_id bits, if we
//      removed that component, then we add back all of the other components that contributed to the lsb of rb_id:
void Gfx9MetaEqGenerator::MergePipeAndRbEq(
    MetaDataAddrEquation* pRb,
    MetaDataAddrEquation* pPipe)
{
    const Pal::Device*  pDevice = m_pParent->GetGfxDevice()->Parent();

    for (uint32  pipeAddrBit = 0; pipeAddrBit < pPipe->GetNumValidBits(); pipeAddrBit++)
    {
        // Find the lowest coordinate within this pipeAddrBit that is contributing.
        CompPair  lowPipe;
        if (pPipe->FindSmallComponent(pipeAddrBit, &lowPipe))
        {
            const uint32 lowPosMask = 1 << lowPipe.compPos;
            const uint32 oldSize    = m_meta.GetNumValidBits();
            m_meta.Filter(lowPipe, MetaDataAddrCompareEq);
            PAL_ASSERT(m_meta.GetNumValidBits() == (oldSize - 1));

            pPipe->Remove(lowPipe);

            for (uint32  rbAddrBit = 0; rbAddrBit < pRb->GetNumValidBits(); rbAddrBit++)
            {
                const uint32  rbData = pRb->Get(rbAddrBit, lowPipe.compType);
                if (TestAnyFlagSet (rbData, lowPosMask))
                {
                    pRb->ClearBits(rbAddrBit, lowPipe.compType, ~lowPosMask);

                    // if we actually removed something from this bit, then add the remaining
                    // channel bits, as these can be removed for this bit
                    for (uint32  localPipeCompType = 0;
                                 localPipeCompType < MetaDataAddrCompNumTypes;
                                 localPipeCompType++)
                    {
                        uint32  eqData = pPipe->Get(pipeAddrBit, localPipeCompType);

                        for (uint32 lowPipeBit : BitIter32(eqData))
                        {
                            const CompPair  localPipePair =
                                    MetaDataAddrEquation::SetCompPair(localPipeCompType, lowPipeBit);

                            if (MetaDataAddrEquation::CompareCompPair(localPipePair,
                                                                      lowPipe,
                                                                      MetaDataAddrCompareEq) == false)
                            {
                                pRb->SetBit(rbAddrBit, localPipePair.compType, localPipePair.compPos);
                                SetRbAppendedBit(rbAddrBit, 1);
                            }
                        }
                    }
                }
            } // end loop through all the rb bits
        } // end check for a non-empty pipe equation
    } // end loop through all 32 bits in the equation
}

// =====================================================================================================================
// Iterate through the remaining RB bits, from lsb to msb, taking the smallest coordinate of each bit, and removing it
// from the metadata equation, and the remaining upper RB bits.  Like for the pipe bits, if an RB bit gets a component
// removed, then we add in all other terms not already present from the Rb bit that did the removal.
uint32 Gfx9MetaEqGenerator::RemoveSmallRbBits(
    MetaDataAddrEquation* pRb)
{
    const Pal::Device*  pDevice    = m_pParent->GetGfxDevice()->Parent();
    uint32              rbBitsLeft = 0;

    for (uint32  rbAddrBit = 0; rbAddrBit < pRb->GetNumValidBits(); rbAddrBit++)
    {
        const uint32  neededNumComponents = (GetRbAppendedBit(rbAddrBit) != 0);

        // Find the lowest coordinate within this pipeAddrBit that is contributing.
        CompPair  lowRb;
        if ((pRb->GetNumComponents(rbAddrBit) > neededNumComponents) &&
            pRb->FindSmallComponent(rbAddrBit, &lowRb))
        {
            const uint32  lowRbMask = 1 << lowRb.compPos;

            rbBitsLeft++;

            m_meta.Filter(lowRb, MetaDataAddrCompareEq);

            // We need to find any other RB bits that have lowRb{AddrType,Position} in their equation
            for (uint32  scanHiRbAddrBit = rbAddrBit + 1;
                         scanHiRbAddrBit < pRb->GetNumValidBits();
                         scanHiRbAddrBit++)
            {
                if (pRb->IsSet(scanHiRbAddrBit, lowRb.compType, lowRbMask))
                {
                    // Don't forget to eliminate this component.
                    pRb->ClearBits(scanHiRbAddrBit, lowRb.compType, ~lowRbMask);

                    // Loop through all the elements in rb[rbAddrBit].  Add everything that isn't equivalent to
                    // "lowRb" into rb[scanHiRbAddrBit]
                    for (uint32  localRbAddrType = 0; localRbAddrType < MetaDataAddrCompNumTypes; localRbAddrType++)
                    {
                        uint32  rbData = pRb->Get(rbAddrBit, localRbAddrType);
                        if (localRbAddrType == lowRb.compType)
                        {
                            rbData &= ~lowRbMask;
                        }

                        if (rbData != 0)
                        {
                            pRb->SetMask(scanHiRbAddrBit, localRbAddrType, rbData);
                            SetRbAppendedBit(scanHiRbAddrBit, GetRbAppendedBit(rbAddrBit));
                        }
                    }
                } // end check for the higher RB bit containing a reference to the just-found "low bit"
            } // end loop through the "higher" RB bits
        } // end check for a valid small component of this RB bit
    } // end loop through all the RB bits

    return rbBitsLeft;
}

// =====================================================================================================================
// Uploads the meta-equation associated with this mask ram to GPU accessible memory.
void Gfx9MetaEqGenerator::UploadEq(
    CmdBuffer*  pCmdBuffer
    ) const
{
    const Pal::Image*   pParentImg = m_pParent->GetImage().Parent();
    const Pal::Device*  pDevice    = m_pParent->GetGfxDevice()->Parent();

    if (IsMetaEquationValid())
    {
        // If this trips, that implies that InitEqGpuAccess() wasn't called during the creation of this
        // mask ram object.
        PAL_ASSERT (m_eqGpuAccess.offset != 0);

        const auto&    boundMem = pParentImg->GetBoundGpuMemory();
        const Gfx9PalSettings& settings = GetGfx9Settings(*pDevice);

        const gpusize  offset = boundMem.Offset() + m_eqGpuAccess.offset;
        m_meta.Upload(pDevice, pCmdBuffer, *boundMem.Memory(), offset, m_firstUploadBit);
    }
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::CpuUploadEq(
    void*  pCpuMem // Pointer to the base of the image memory that contains this mask ram object
    ) const
{
    if (IsMetaEquationValid())
    {
        uint32*        pWriteMem       = reinterpret_cast<uint32*>(VoidPtrInc(pCpuMem, LowPart(m_eqGpuAccess.offset)));
        const uint32*  pEquation       = m_meta.GetEquation(m_firstUploadBit);
        const uint32   numUploadEqBits = m_meta.GetNumValidBits() - m_firstUploadBit;
        const uint32   numUploadBytes  = MetaDataAddrCompNumTypes * numUploadEqBits * sizeof(uint32);

        memcpy(pWriteMem, pEquation, numUploadBytes);
    }
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::AddMetaPipeBits(
    MetaDataAddrEquation* pPipe,
    int32                 offset)
{
    Gfx9MaskRamBlockSize microBlockLog2 = {};
    GetMicroBlockSize(&microBlockLog2);

    Gfx9MaskRamBlockSize compBlockLog2 = {};
    m_pParent->CalcCompBlkSizeLog2(&compBlockLog2);

    Gfx9MaskRamBlockSize pixelBlockLog2 = {};
    GetPixelBlockSize(&pixelBlockLog2);

    const Pal::Device*     pPalDevice                  = m_pParent->GetGfxDevice()->Parent();
    const AddrSwizzleMode  swizzleMode                 = m_pParent->GetSwizzleMode();
    const uint32           blockSizeLog2               = Log2(pPalDevice->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32           bppLog2                     = m_pParent->GetBytesPerPixelLog2();
    const uint32           effectiveNumPipesLog2       = GetEffectiveNumPipes();
    const uint32           numPipesLog2                = m_pParent->GetGfxDevice()->GetNumPipesLog2();
    const uint32           numSamplesLog2              = m_pParent->GetNumSamplesLog2();
    const uint32           numShaderArraysLog2         = IsGfx103PlusExclusive(*pPalDevice) ? GetNumShaderArrayLog2() : 0;
    const uint32           pipeInterleaveLog2          = m_pParent->GetGfxDevice()->GetPipeInterleaveLog2();
    const uint32           startBase                   = 8;
    const uint32           endBase                     = startBase + effectiveNumPipesLog2;
    const uint32           isEvenEffectiveNumPipesLog2 = ((effectiveNumPipesLog2 % 2) == 0);

    CompPair cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, microBlockLog2.width);
    CompPair cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, microBlockLog2.height);

    if (pPalDevice->ChipProperties().gfx9.rbPlus)
    {
        int32  s       = (IsZSwizzle(swizzleMode) ? 0 : numSamplesLog2) - 1;
        uint32 pos     = 0;
        uint32 tileOrd = 4;
        bool   xBit    = false;
        bool   sBit    = false;

        Data2dParamsNew  params = {};
        GetData2DParamsNew(&params);

        PAL_ASSERT(8 <= params.restart);

        uint32 numZBits      = params.restart - startBase;
        uint32 sampleBits    = (IsZSwizzle(swizzleMode) && (bppLog2 <= 2)) ? 3 : numSamplesLog2;
        int32  zBitsToRotate = static_cast<int32>(numZBits + sampleBits) - 6;

        if ((effectiveNumPipesLog2 + sampleBits) >= 6)
        {
            zBitsToRotate++;
        }

        zBitsToRotate = Max(0, zBitsToRotate);

        PAL_ASSERT(zBitsToRotate <= static_cast<int32>(numZBits));
        CompPair cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, numZBits - zBitsToRotate);

        // This hack is to ensure that htile addressing is the same regardless of bpp
        // without it the pipe can differ in z bits.  This only addresses bpp <= 2 (which covers all depth modes)
        // and when pipes are less than or equal to 64 pipes.  Greater than this will mean that x/y bits can also
        // affect the pipe, and can be different based on bpp, so we cannot make this assurance
        uint32 maxZPos = endBase;
        if (IsZSwizzle(swizzleMode) &&
            (bppLog2 <= 2))
        {
            PAL_ASSERT(zBitsToRotate <= static_cast<int32>(8 + numZBits));

            maxZPos = 8 + numZBits - zBitsToRotate;
        }

        uint32 ord = cx.compPos;
        if ((ord >= tileOrd) && (ord < params.pipeAnchorWidthLog2))
        {
            if ((ord == 4)  ||
                ((ord == 3) && (params.skipY3 == false)))
            {
                cx.compPos++;
                ord++;
            }

            if (ord > 4)
            {
                cx = MetaDataAddrEquation::SetCompPair(cx.compType, params.pipeAnchorWidthLog2);
            }
        }

        ord = cy.compPos;
        if ((ord >= tileOrd) && (ord < params.pipeAnchorHeightLog2))
        {
            if ((ord == 3) && params.skipY3)
            {
                cy.compPos++;
                ord++;
            }

            if (ord > 4)
            {
                cy = MetaDataAddrEquation::SetCompPair(cy.compType, params.pipeAnchorHeightLog2);
            }
        }

        const uint32 tileSplitStart = endBase;

        uint32 subTileXyBits = 0;
        if (IsZSwizzle(swizzleMode) &&
            ((microBlockLog2.width + microBlockLog2.height) <= 6))
        {
            subTileXyBits = 6 - microBlockLog2.width - microBlockLog2.height;
        }

        const uint32 maxPipeRotateBit  = ((params.pipeRotateAmount <= 1)
                                          ? params.pipeRotateBit0
                                          : Max(params.pipeRotateBit0, params.pipeRotateBit1))
                                         + 1;

        uint32  end     = params.tileSplitBits + Max(endBase, maxPipeRotateBit);
        bool    pipeBit = false;

        for(uint32 i = startBase; i < end; i++)
        {
            sBit = false;

            pos = i;
            if (pos >= (tileSplitStart + params.tileSplitBits))
            {
                pos -= params.tileSplitBits;
            }
            else if (pos >= tileSplitStart)
            {
                pos += blockSizeLog2 - tileSplitStart - params.tileSplitBits;
            }

            if ((params.tileSplitBits >= 2u) && (params.yBias >= params.tileSplitBits - 1))
            {
                // swap last 2 bits below block_size
                if (pos == (blockSizeLog2 - 1))
                {
                    pos--;
                }
                else if (pos == (blockSizeLog2 - 2))
                {
                    pos++;
                }
            }

            if (pos < endBase)
            {
                if (params.flipPipeFill != 0)
                {
                    if (pos == params.restart)
                    {
                        pos += params.flipPipeFill;
                    }
                    else if (pos == (params.restart + params.flipPipeFill))
                    {
                        pos -= params.flipPipeFill;
                    }
                }
            }

            if ((numSamplesLog2 == 3)      &&
                (i < blockSizeLog2)        &&
                (subTileXyBits == 3)       &&
                (params.tileSplitBits > 0) &&
                (params.tileSplitBits < subTileXyBits))
            {
                // swap the first and last bits
                if (pos == params.restart)
                {
                    pos = blockSizeLog2 - 1;
                }
                else if (pos == (blockSizeLog2 - 1))
                {
                    pos = params.restart;
                }
            }

            if (IsGfx103Plus(*pPalDevice)                   &&
                (numSamplesLog2 == 3)                       &&
                (i < blockSizeLog2)                         &&
                (subTileXyBits == 3)                        &&
                (numPipesLog2 == (numShaderArraysLog2 + 1)) &&
                (params.restart < (8 + numPipesLog2 - 1)))
            {
                if (pos == params.restart)
                {
                    pos = 8 + numPipesLog2 - 1;
                }
                else if (pos == (8 + numPipesLog2 - 1))
                {
                    pos = params.restart;
                }
            }

            const bool withinBit0 = (pos <= params.pipeRotateBit0);
            const bool withinBit1 = (pos <= params.pipeRotateBit1);
            const bool onBit0     = (pos == params.pipeRotateBit0);
            const bool onBit1     = (pos == params.pipeRotateBit1);

            if ((params.pipeRotateAmount > 0) &&
                withinBit0                    &&
                (onBit1 == false))
            {
                if (onBit0)
                {
                    pos = 8 + params.pipeRotateAmount - 1;
                }
                else
                {
                    pos++;
                }
            }

            if ((params.pipeRotateAmount > 1) &&
                withinBit1                    &&
                (onBit0 == false))
            {
                if (onBit1)
                {
                    pos = 8 + params.pipeRotateAmount - 2;
                }
                else
                {
                    pos++;
                }
            }

            pipeBit = (pos < (8 + numPipesLog2));
            if (pipeBit)
            {
                pos += (pipeInterleaveLog2 - 8);
            }

            pos += offset;

            if (i == endBase)
            {
                s = 0;
            }

            xBit = (((i - params.restart - params.yBias) & 1) == 0);
            if (i < endBase)
            {
                sBit = ((i >= params.restart) && (s >= 0));
            }
            else
            {
                sBit = ((blockSizeLog2 - i) <= params.upperSampleBits);
            }

            if (i < (params.restart + params.yBias))
            {
                xBit = false;
            }

            // Don't do 3 y bits in a row.  Do 2 y bits, followed by a 2 y bits again
            if ((params.yBias == 3)         &&
                (params.tileSplitBits == 3) &&
                ((i == params.restart + 2)  || (i == params.restart + 3)))
            {
                xBit = (xBit ? false : true);
            }

            if (i >= params.restart)
            {
                if (sBit)
                {
                    if (i < endBase)
                    {
                        s--;
                    }
                    else
                    {
                        s++;
                    }
                }
                else
                {
                    if (xBit)
                    {
                        if (pipeBit && (cx.compPos >= compBlockLog2.width))
                        {
                            pPipe->SetBit(pos, cx);
                        }

                        cx.compPos++;
                        ord = cx.compPos;
                        if ((ord >= tileOrd) && (ord < params.pipeAnchorWidthLog2))
                        {
                            if ((ord == 4) || ((ord == 3) && (params.skipY3 == false)))
                            {
                                cx.compPos++;
                                ord++;
                            }

                            if (ord > 4)
                            {
                                cx = MetaDataAddrEquation::SetCompPair(cx.compType, params.pipeAnchorWidthLog2);
                            }
                        }
                    }
                    else
                    {
                        if (pipeBit && (cy.compPos >= compBlockLog2.height))
                        {
                            pPipe->SetBit(pos, cy);
                        }

                        cy.compPos++;
                        ord = cy.compPos;
                        if ((ord >= tileOrd) && (ord < params.pipeAnchorHeightLog2))
                        {
                            if ((ord == 3) && params.skipY3)
                            {
                                cy.compPos++;
                                ord++;
                            }

                            if (ord > 4)
                            {
                                cy = MetaDataAddrEquation::SetCompPair(cy.compType, params.pipeAnchorHeightLog2);
                            }
                        }
                    }
                }
            }
            else if (i < maxZPos)
            {
                if (cz.compPos > 0)
                {
                    cz.compPos--;
                }
                else
                {
                    cz.compPos = static_cast<uint8>(numZBits - 1);
                }

                if (pipeBit)
                {
                    pPipe->SetBit(pos, cz);
                }
            }

            if (onBit0 || onBit1)
            {
                const uint32  pipeBlockSize = GetEffectiveNumPipes() + 4;

                int32                     pipeBlockOrd  = pipeBlockSize;
                const CompPair            firstCompPair = pPipe->GetFirst(pos);
                MetaDataAddrComponentType compType      = firstCompPair.compType;

                if (IsZSwizzle(swizzleMode) &&
                    (firstCompPair.compPos < 3))
                {
                    compType = (((numSamplesLog2 & 1) != 0) &&
                                (numShaderArraysLog2 < ((numSamplesLog2 < 2) ? 3 : 1))
                                ? MetaDataAddrCompY
                                : MetaDataAddrCompX);

                    if ((numSamplesLog2 == 3)  && ((blockSizeLog2 - numPipesLog2) == 11))
                    {
                       compType = MetaDataAddrCompY;
                    }

                    const uint32  temp = ((compType == MetaDataAddrCompY) ? 1 : 2);

                    pPipe->SetBit(pos,
                                  MetaDataAddrEquation::SetCompPair(compType,
                                                                    ((numShaderArraysLog2 < temp) ? 5 : 6)));
                }

                if (onBit0 || pPipe->IsEmpty(pos))
                {
                    compType = MetaDataAddrCompZ;
                }

                if (onBit1)
                {
                    if (IsZSwizzle(swizzleMode))
                    {
                        if ((numPipesLog2 == 5)          ||
                            ((numPipesLog2 < 6)          &&
                             ((numPipesLog2 & 1) == 0)   &&
                             ((numSamplesLog2 & 1) != 0) &&
                             ((10 + numPipesLog2 + numSamplesLog2) < blockSizeLog2)))
                        {
                            pipeBlockOrd--;
                        }
                    }
                    else
                    {
                        pipeBlockOrd++;
                        compType = MetaDataAddrCompZ;
                    }
                }

                if (compType != MetaDataAddrCompX)
                {
                    pPipe->SetBit(pos, MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, pipeBlockOrd));
                }

                if (compType != MetaDataAddrCompY)
                {
                    pPipe->SetBit(pos, MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, pipeBlockOrd));
                }
            }
        }
    }
    else
    {
        Data2dParams  params = {};
        GetData2DParams(&params);

        uint32 sRestart   = (params.xRestart < params.yRestart) ? params.xRestart : params.yRestart;
        uint32 maxRestart = (params.xRestart > params.yRestart) ? params.xRestart : params.yRestart;

        uint32 restart = 0;
        uint32 s       = 0;
        uint32 pos     = 0;

        bool pipeXBit = false;
        bool xBit     = false;
        bool sBit     = false;

        PAL_ASSERT(startBase <= sRestart);
        uint32 numZBits = sRestart - startBase;

        if ((maxRestart - sRestart) > 1)
        {
            numZBits++;
        }

        int32 zBitsToRotate = numZBits + numSamplesLog2 - 6;

        // If rbPlus is the only condition which results in PipeDist16x16, then the following
        // block of logic would never happen, keep here for awareness
        if ((m_pipeDist == PipeDist16x16) &&
            ((effectiveNumPipesLog2 + numSamplesLog2) >= 6))
        {
            zBitsToRotate++;
        }

        zBitsToRotate = Max(0, zBitsToRotate);

        PAL_ASSERT(numZBits >= (uint32)(zBitsToRotate));
        CompPair cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, numZBits - zBitsToRotate);

        // This hack is to ensure that htile addressing is the same regardless of bpp
        // without it the pipe can differ in z bits.  This only addresses bpp <= 2 (which covers all depth modes)
        // and when pipes are less than or equal to 64 pipes.  Greater than this will mean that x/y bits can also
        // affect the pipe, and can be different based on bpp, so we cannot make this assurance
        uint32 maxZPos = endBase;

        if (m_pParent->IsDepth()         &&
            (effectiveNumPipesLog2 <= 6) &&
            (bppLog2 <= 2))
        {
            PAL_ASSERT(numSamplesLog2 <= 14); // This assert should never failed, but in case.
            maxZPos = 14 - numSamplesLog2;
        }

        uint32 ord = cx.compPos;
        if ((ord == 3) && params.flipX3Y3)
        {
            cx.compPos++;
            ord++;
        }
        if ((ord == 4) && (params.flipX4Y4 == false))
        {
            cx.compPos++;
            ord++;
        }

        ord = cy.compPos;
        if ((ord == 3) && (params.flipX3Y3 == false))
        {
            cy.compPos++;
            ord++;
        }
        if ((ord == 4) && params.flipX4Y4)
        {
            cy.compPos++;
            ord++;
        }

        for (uint32 i = startBase; i < endBase; i++)
        {
            pipeXBit = ((i & 1) != 0);
            xBit     = pipeXBit;

            if (params.flipPipeXY)
            {
                xBit = (pipeXBit == false);
            }

            sBit = ((i >= sRestart)                  &&
                    (s < numSamplesLog2)             &&
                    (IsZSwizzle(swizzleMode) == false));

            if (params.flipYBias &&
                ((i >= (endBase + isEvenEffectiveNumPipesLog2)) || (sRestart < endBase)))
            {
                xBit = (xBit == false);
            }

            pos = i - 8 + pipeInterleaveLog2 + offset;

            if (params.flipPipeFill != 0)
            {
                if (i == sRestart)
                {
                    pos += params.flipPipeFill;
                }
                else if (i == (sRestart + params.flipPipeFill))
                {
                    pos -= params.flipPipeFill;
                }
            }
            restart = pipeXBit ? params.xRestart : params.yRestart;

            if (i >= restart)
            {
                if (sBit)
                {
                    s++;
                }
                else
                {
                    if (xBit)
                    {
                        ord = cx.compPos;
                        if (ord >= microBlockLog2.width)
                        {
                            pPipe->SetBit(pos, cx); //meta_data_eq[pos].add(cx);
                        }
                        cx.compPos++;
                        ord = cx.compPos;
                        if ((ord == 3) && params.flipX3Y3)
                        {
                            cx.compPos++;
                            ord++;
                        }
                        if ((ord == 4) && (params.flipX4Y4 == false))
                        {
                            cx.compPos++;
                            ord++;
                        }
                    }
                    else
                    {
                        ord = cy.compPos;
                        if (ord >= microBlockLog2.height)
                        {
                            if (params.flipY1Y2)
                            {
                                switch (ord)
                                {
                                case 1:
                                    cy.compPos++;
                                    break;
                                case 2:
                                    cy.compPos--;
                                    break;
                                }
                            }
                            pPipe->SetBit(pos, cy);
                            if (params.flipY1Y2)
                            {
                                switch (ord)
                                {
                                case 1:
                                    cy.compPos--;
                                    break;
                                case 2:
                                    cy.compPos++;
                                    break;
                                }
                            }
                        }
                        cy.compPos++;
                        ord = cy.compPos;
                        if ((ord == 3) && (params.flipX3Y3 == false))
                        {
                            cy.compPos++;
                            ord++;
                        }
                        if ((ord == 4) && params.flipX4Y4)
                        {
                            cy.compPos++;
                            ord++;
                        }
                    }
                }
            }
            else if (i < maxZPos)
            {
                if (cz.compPos > 0)
                {
                    cz.compPos--;
                }
                else
                {
                    PAL_ASSERT(numZBits >= 1);
                    cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, numZBits - 1);
                }
                pPipe->SetBit(pos, cz);
            }
        }
    }

    AddRbBits(pPipe, offset);
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::AddRbBits(
    MetaDataAddrEquation* pPipe,
    int32                 offset)
{
    CompPair  cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, 3);
    CompPair  cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, 3);

    const int32  numPipesLog2       = GetEffectiveNumPipes();
    const uint32 pipeInterleaveLog2 = m_pParent->GetGfxDevice()->GetPipeInterleaveLog2();
    int32 scStart                   = 2;
    const uint32 pipeRotateAmount   = GetPipeRotateAmount();

    if (m_pipeDist == PipeDist16x16)
    {
        cx.compPos++;
        cy.compPos++;
        scStart = 1;
    }

    int32 pos = pipeInterleaveLog2 + offset + pipeRotateAmount;
    for (int32 i = 0; ((i < numPipesLog2) && (i < scStart)); i++)
    {
        pPipe->SetBit(pos, cx);
        pPipe->SetBit(pos, cy);

        cx.compPos++;
        cy.compPos++;
        pos++;
    }

    if (numPipesLog2 > scStart)
    {
        int32 start = pipeInterleaveLog2 + scStart      + offset + pipeRotateAmount;
        int32 end   = pipeInterleaveLog2 + numPipesLog2 + offset + pipeRotateAmount;

        pPipe->Mort2d(m_pParent->GetGfxDevice(), &cy, &cx, start, end - 1);

        int32 oddPipe = (numPipesLog2 & 1);
        if (m_pipeDist == PipeDist16x16)
        {
            oddPipe = (~oddPipe) & 0x1;
        }

        if (oddPipe == 1)
        {
            pPipe->Mort2d(m_pParent->GetGfxDevice(), &cx, &cy, end - 1, start);
        }
        else
        {
            pPipe->Mort2d(m_pParent->GetGfxDevice(), &cy, &cx, end - 1, start);
        }
    }
}

// =====================================================================================================================
static uint32 CalcBitsToFill(
    uint32  pixelBlockLog2,
    uint32  pipeAnchorLog2,
    uint32  microBlockLog2)
{
    PAL_ASSERT(microBlockLog2 <= 4);

    return ((pixelBlockLog2 < pipeAnchorLog2) ? 0 : pixelBlockLog2 - pipeAnchorLog2) - microBlockLog2 + 4;
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::GetData2DParams(
    Data2dParams* pParams
    ) const
{
    Gfx9MaskRamBlockSize microBlockLog2 = {};
    GetMicroBlockSize(&microBlockLog2);

    Gfx9MaskRamBlockSize pixelBlockLog2 = {};
    GetPixelBlockSize(&pixelBlockLog2);

    Extent2d pipeAnchorLog2 = {};
    GetPipeAnchorSize(&pipeAnchorLog2);

    const Pal::Device*     pPalDevice     = m_pParent->GetGfxDevice()->Parent();
    const AddrSwizzleMode  swizzleMode    = m_pParent->GetSwizzleMode();
    const uint32           bppLog2        = m_pParent->GetBytesPerPixelLog2();
    const uint32           numPipesLog2   = GetEffectiveNumPipes();
    const uint32           numSamplesLog2 = m_pParent->GetNumSamplesLog2();
    const uint32           blockSizeLog2  = Log2(pPalDevice->GetAddrMgr()->GetBlockSize(swizzleMode));

    if (m_pipeDist == PipeDist16x16)
    {
        if ((numPipesLog2 >= 2)                            &&
            ((numPipesLog2 & 1) == 0)                      &&
            (pipeAnchorLog2.width > pipeAnchorLog2.height) &&
            (pixelBlockLog2.width < pixelBlockLog2.height))
        {
            pipeAnchorLog2.width--;
            pipeAnchorLog2.height++;
        }
    }
    else
    {
        if ((numPipesLog2 > 2)                                   &&
            ((numPipesLog2 & 1) != 0)                            &&
            (pipeAnchorLog2.width == (pixelBlockLog2.width - 1)) &&
            (pixelBlockLog2.height < pipeAnchorLog2.height))
        {
            pipeAnchorLog2.width++;
            pipeAnchorLog2.height--;
        }
    }

    if (m_pipeDist == PipeDist16x16)
    {
        pParams->flipX4Y4 = ((numPipesLog2          >= 2) &&
                             ((numPipesLog2 & 1)    == 0) &&
                             (pixelBlockLog2.width  == 4) &&
                             (pixelBlockLog2.height == 5));
    }
    else
    {
        pParams->flipX4Y4 = ((numPipesLog2          >= 2) &&
                             (pixelBlockLog2.width  == 4) &&
                             (pixelBlockLog2.height == 5));
    }

    int32 xBitsToFill = ((pixelBlockLog2.width < pipeAnchorLog2.width)
                         ? 0
                         : (pixelBlockLog2.width - pipeAnchorLog2.width))
                        - microBlockLog2.width + 4;

    int32 yBitsToFill = ((pixelBlockLog2.height < pipeAnchorLog2.height)
                         ? 0
                         : (pixelBlockLog2.height - pipeAnchorLog2.height))
                        - microBlockLog2.height + 4 - pParams->flipX4Y4;

    if ((m_pipeDist == PipeDist16x16) && (numPipesLog2 > 1))
    {
        xBitsToFill++;
    }

    int32 xAnchorsSubsumed = 0;
    int32 yAnchorsSubsumed = 0;

    if (pixelBlockLog2.width < pipeAnchorLog2.width)
    {
        xAnchorsSubsumed = pipeAnchorLog2.width - pixelBlockLog2.width - pParams->flipX4Y4;
    }

    if (pixelBlockLog2.height < pipeAnchorLog2.height)
    {
        yAnchorsSubsumed = pipeAnchorLog2.height - pixelBlockLog2.height;
    }

    pParams->xRestart = 8 + numPipesLog2;
    pParams->yRestart = 8 + numPipesLog2;

    int32 jumpY = (numPipesLog2 & 1);
    if (xAnchorsSubsumed > 0)
    {
        pParams->xRestart -= (xAnchorsSubsumed * 2 - ((~jumpY) & 0x1));
    }

    if (yAnchorsSubsumed > 0)
    {
        pParams->yRestart -= (yAnchorsSubsumed * 2 - jumpY);
    }

    pParams->upperSampleBits = numSamplesLog2 - xAnchorsSubsumed - yAnchorsSubsumed;
    pParams->upperSampleBits = ((pParams->upperSampleBits < 0) || IsZSwizzle(swizzleMode))
                               ? 0
                               : pParams->upperSampleBits;

    pParams->tileSplitBits = 0;
    if (IsZSwizzle(swizzleMode))
    {
        pParams->tileSplitBits = (3 - microBlockLog2.width ) +
                                 (3 - microBlockLog2.height) -
                                 xAnchorsSubsumed            -
                                 yAnchorsSubsumed;

        pParams->tileSplitBits = Max(0, pParams->tileSplitBits);

        if (pParams->tileSplitBits > static_cast<int32>(numSamplesLog2))
        {
            pParams->tileSplitBits = numSamplesLog2;
        }
    }

    int32 xyRestartDiff, xyRestartGap;
    xyRestartDiff = pParams->xRestart - pParams->yRestart;
    xyRestartDiff = (xyRestartDiff < 0) ? -xyRestartDiff : xyRestartDiff;
    xyRestartDiff = (xyRestartDiff / 2);

    // in SW_R MSAA, the gap only affects the sample bits, so need to count it
    xyRestartGap = ((IsZSwizzle(swizzleMode) == false) && (numSamplesLog2 > 0)) ? 0 : xyRestartDiff;
    if (m_pipeDist == PipeDist16x16)
    {
        xyRestartGap = 0;
    }

    int32 oddSample = ((IsRotatedSwizzle(swizzleMode)) && ((numSamplesLog2 & 1) != 0)) ? 1 : 0;
    if (pParams->flipX4Y4)
    {
        oddSample++;
    }

    if ((m_pipeDist == PipeDist16x16) && (xBitsToFill == (yBitsToFill + oddSample)))
    {
        if (numSamplesLog2 & 1)
        {
            pParams->flipPipeFill = (xyRestartDiff == 0) ? 0 : 1;
        }
        else
        {
            pParams->flipPipeFill = (xyRestartDiff == 0) ? -1 : 0;
        }
    }

    pParams->flipPipeXY = (((blockSizeLog2 - pParams->upperSampleBits -
                           xBitsToFill - yBitsToFill - xyRestartGap) & 1) != 0)
                          ? (yBitsToFill > xBitsToFill)
                          : (xBitsToFill > yBitsToFill);

    if ((m_pipeDist  == PipeDist16x16)              &&
        (xBitsToFill == (yBitsToFill + oddSample))  &&
        ((numSamplesLog2 & 1) != 0))
    {
        pParams->flipPipeXY = true;
    }
    pParams->flipYBias = (IsZSwizzle(swizzleMode)                &&
                          (yBitsToFill > xBitsToFill)            &&
                          (((xBitsToFill + yBitsToFill) & 1) == 0));

    int32 posOffset = (static_cast<int32>(pParams->flipYBias)  ^
                       static_cast<int32>(pParams->flipPipeXY) ^
                       (pParams->upperSampleBits & 1));

    int32 posX3 = (blockSizeLog2 - pParams->upperSampleBits)   -
                  2 * (xBitsToFill - 3 + microBlockLog2.width) +
                  ((~posOffset) & 0x1);

    int32 posY4 = (blockSizeLog2 - pParams->upperSampleBits)    -
                  2 * (yBitsToFill - 3 + microBlockLog2.height) +
                  posOffset;

    int32 posXNext = (blockSizeLog2 - pParams->upperSampleBits)   -
                     2 * (xBitsToFill - 4 + microBlockLog2.width) +
                     ((~posOffset) & 0x1);

    if (m_pipeDist == PipeDist16x16)
    {
        bool temp = pParams->flipX4Y4;
        pParams->flipX3Y3 = false;
        pParams->flipX4Y4 |= ((numSamplesLog2 & 1)                        &&
                              (numPipesLog2   & 1)                        &&
                              ((xAnchorsSubsumed + yAnchorsSubsumed) & 1)) ? false : true;

        pParams->flipX4Y4 = ((IsRotatedSwizzle(swizzleMode))    &&
                             (numSamplesLog2 & 1)               &&
                             (numPipesLog2 == 6)                &&
                             ((bppLog2 == 2) || (bppLog2 == 4))) ? temp : pParams->flipX4Y4;
    }
    else
    {
        pParams->flipX3Y3 = ((numPipesLog2          > 1) &&
                             (microBlockLog2.width  < 4) &&
                             (microBlockLog2.height < 4) &&
                             (pixelBlockLog2.width  > 4) &&
                             ((posX3 - posY4)      == 1));

        pParams->flipX4Y4 |= pParams->flipX3Y3;

        if ((numPipesLog2 > 2)      &&
            (numPipesLog2 & 1)      &&
            ((posY4 - posXNext) == 1))
        {
            pipeAnchorLog2.width++;
            pipeAnchorLog2.height--;
            pParams->flipX4Y4 = true;
        }
    }

    pParams->flipY1Y2 = ((microBlockLog2.height <= 1) &&
                         ((xAnchorsSubsumed + yAnchorsSubsumed) > (2 * (int32)(1 ^ microBlockLog2.height))));

    pParams->pipeAnchorHeightLog2 = pipeAnchorLog2.height;
    pParams->pipeAnchorWidthLog2  = pipeAnchorLog2.width;
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::GetData2DParamsNew(
    Data2dParamsNew*  pParams
    ) const
{
    Gfx9MaskRamBlockSize microBlockLog2 = {};
    GetMicroBlockSize(&microBlockLog2);

    Gfx9MaskRamBlockSize pixelBlockLog2 = {};
    GetPixelBlockSize(&pixelBlockLog2);

    Extent2d  pipeAnchorLog2 = {};
    GetPipeAnchorSize(&pipeAnchorLog2);

    const AddrSwizzleMode swizzleMode = m_pParent->GetSwizzleMode();
    const auto*  pGfxDevice           = m_pParent->GetGfxDevice();
    const auto*  pPalDevice           = pGfxDevice->Parent();
    const uint32 blockSizeLog2        = Log2(pPalDevice->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32 bppLog2              = m_pParent->GetBytesPerPixelLog2();
    const uint32 numPipesLog2         = GetEffectiveNumPipes();
    const uint32 numSamplesLog2       = m_pParent->GetNumSamplesLog2();

    if (pipeAnchorLog2.width != pipeAnchorLog2.height)
    {
        if (pipeAnchorLog2.width > pipeAnchorLog2.height)
        {
            if ((pixelBlockLog2.width == (pipeAnchorLog2.width - 1)) &&
                (pixelBlockLog2.height >= (pipeAnchorLog2.height + 1)))
            {
                pipeAnchorLog2.height++;
                pipeAnchorLog2.width--;
            }
        }
        else
        {
            if ((pixelBlockLog2.height == (pipeAnchorLog2.height - 1)) &&
                (pixelBlockLog2.width  >= (pipeAnchorLog2.width + 1)))
            {
                pipeAnchorLog2.width++;
                pipeAnchorLog2.height--;
            }
        }
    }

    uint32 xBitsToFill = CalcBitsToFill(pixelBlockLog2.width,  pipeAnchorLog2.width,  microBlockLog2.width);
    uint32 yBitsToFill = CalcBitsToFill(pixelBlockLog2.height, pipeAnchorLog2.height, microBlockLog2.height);
    uint32 sBitsToFill = 0;

    if ((numPipesLog2 > 1) && (pixelBlockLog2.width > 4))
    {
        yBitsToFill++;
    }

    if (pixelBlockLog2.width < 4)
    {
        xBitsToFill -= 4u - pixelBlockLog2.width;
    }

    pParams->pipeRotateAmount = GetPipeRotateAmount();

    // Some hackery for 16Bpe 8xaa
    if ((numPipesLog2 >= 1) && (pixelBlockLog2.width == 4))
    {
        if (pParams->pipeRotateAmount > 0)
        {
            yBitsToFill++;
        }
    }

    pParams->restart = blockSizeLog2    - xBitsToFill - yBitsToFill - sBitsToFill;
    int32 overlap    = 8 + numPipesLog2 - pParams->restart;

    pParams->upperSampleBits = 0;

    int32 tempTileSplitBits = (3 - static_cast<int32>(microBlockLog2.width))  +
                              (3 - static_cast<int32>(microBlockLog2.height)) -
                              overlap;

    if (IsGfx11(*pPalDevice) && (blockSizeLog2 != 16))
    {
        tempTileSplitBits = overlap;
    }

    pParams->tileSplitBits   = static_cast<uint32>(Max(0, tempTileSplitBits));
    pParams->tileSplitBits   = Min(numSamplesLog2, pParams->tileSplitBits);

    int32 pipeSampleBits = 0;

    bool  restartPipeBaseIsX = ((pParams->restart & 1) == 0);
    int32 restartPipeOrd     = (pParams->restart == 8) ? 4 : 5 + (pParams->restart - 9) / 2;

    pParams->flipPipeFill = 0;
    int32 ord = (restartPipeBaseIsX) ? pixelBlockLog2.width : pixelBlockLog2.height;
    if ((ord == (restartPipeOrd + 1)) && (overlap > 0))
    {
        pParams->flipPipeFill = -1;
    }

    if (yBitsToFill > xBitsToFill)
    {
        pParams->yBias = yBitsToFill - xBitsToFill;
    }
    else
    {
        pParams->yBias = 0;
    }

    if ((pParams->yBias == 0) && (microBlockLog2.height < microBlockLog2.width))
    {
        pParams->yBias = 1;
    }

    pParams->skipY3 = false;

    pParams->pipeRotateBit0 = 0;
    pParams->pipeRotateBit1 = 0;

    if (pParams->pipeRotateAmount > 0)
    {
        pParams->pipeRotateBit0 = pParams->restart + pipeSampleBits - pParams->tileSplitBits;
        pParams->pipeRotateBit0 += ((4 - microBlockLog2.height) < pParams->yBias)
                                      ? (4 - microBlockLog2.height)
                                      : 1 + (2 * (4 - microBlockLog2.height) - pParams->yBias);

        // This prevents sub-tile bits from going into the pipe in SW_Z, and prevents going outside the block in SW_R
        if (pParams->pipeRotateBit0 >= blockSizeLog2 - pParams->tileSplitBits)
        {
            pParams->pipeRotateBit0 = blockSizeLog2 - pParams->tileSplitBits - 1;
        }
    }

    if (pParams->pipeRotateAmount > 1)
    {
        bool useY = ((numSamplesLog2 & 1) != 0);
        if (numPipesLog2 == 6)
        {
            useY = false;
        }

        // This is needed to make sure that pipe are identical up to 32bpp.  This is needed for htile writes by the GL2
        if (useY)
        {
            pParams->pipeRotateBit1 = pParams->restart + pipeSampleBits - pParams->tileSplitBits;
            pParams->pipeRotateBit1 += ((5 - microBlockLog2.height) < pParams->yBias)
                                        ? (5 - microBlockLog2.height)
                                        : 1 + (2 * (5 - microBlockLog2.height) - pParams->yBias);

            // This prevents sub-tile bits from going into the pipe in SW_Z
        }

        if ((useY == false)        ||
             ((numSamplesLog2 & 1) &&
              (bppLog2 <= 2)       &&
              (pParams->pipeRotateBit1 > (blockSizeLog2 - 1 - pParams->tileSplitBits - ((bppLog2 <= 0) ? 2 : 0)))))
        {
            pParams->pipeRotateBit1 = pParams->restart                +
                                      pipeSampleBits                  +
                                      2 * (4 - microBlockLog2.width ) +
                                      pParams->yBias                  -
                                      pParams->tileSplitBits;
        }

        // This is needed to make sure that pipe are identical up to 32bpp.  This is needed for htile writes by the GL2
        if (pParams->pipeRotateBit1 >= (blockSizeLog2 - pParams->tileSplitBits))
        {
            pParams->pipeRotateBit1 = blockSizeLog2 - 1;

            if ((numSamplesLog2 == 3)                &&
                (blockSizeLog2 - numPipesLog2 == 11) &&
                ((bppLog2 & 1) == 0))
            {
               pParams->pipeRotateBit1--;
            }

            if (IsGfx11(*pPalDevice) && (numSamplesLog2 == 3))
            {
                if ((blockSizeLog2 > 16) && (numPipesLog2 == 4) && (bppLog2 == 3))
                {
                    pParams->pipeRotateBit1 -= 2;
                }

                if ((blockSizeLog2 == 18) && (numPipesLog2 == 6) && (bppLog2 >= 2))
                {
                    pParams->pipeRotateBit1 -= 4;
                }
            }
        }

        // This makes sure that the msb sample bits get into the pipe if any sample bits are needed in SW_R
        if (pParams->pipeRotateBit1 >= (blockSizeLog2 - pParams->upperSampleBits))
        {
            pParams->pipeRotateBit1 = blockSizeLog2 - 1;

            // For block size > 64KB or VAR, don't use fragment bit
            if ((blockSizeLog2 > 16) || (blockSizeLog2 == 0))
            {
                pParams->pipeRotateBit1 = blockSizeLog2 - pParams->upperSampleBits - 1;
            }
        }

        // Don't use the same bit as pipe0
        if (pParams->pipeRotateBit1 == pParams->pipeRotateBit0)
        {
            pParams->pipeRotateBit1--;
        }
    }

    pParams->yBias += pipeSampleBits;

    pParams->pipeAnchorHeightLog2 = pipeAnchorLog2.height;
    pParams->pipeAnchorWidthLog2  = pipeAnchorLog2.width;
}

// =====================================================================================================================
// Calculates the pixel dimensions of one tile block of the image associated with this hTile.  i.e., what are the
// pixel dimensions of a 64KB tile block?
//
void Gfx9MetaEqGenerator::GetPixelBlockSize(
    Gfx9MaskRamBlockSize* pBlockSize
    ) const
{
    const AddrSwizzleMode  swizzleMode = m_pParent->GetSwizzleMode();
    const uint32 blockSizeLog2  = Log2(m_pParent->GetGfxDevice()->Parent()->GetAddrMgr()->GetBlockSize(swizzleMode));
    const uint32 bppLog2        = m_pParent->GetBytesPerPixelLog2();
    const uint32 numSamplesLog2 = m_pParent->GetNumSamplesLog2();

    // 2^19 =    0001_00011 => 0000_1001  = 9 - 1 = 8 for width
    pBlockSize->width  = (blockSizeLog2 >> 1) - (bppLog2 >> 1) - (numSamplesLog2 >> 1) - (numSamplesLog2 &  1);
    pBlockSize->height = (blockSizeLog2 >> 1) - (bppLog2 >> 1) - (bppLog2 & 1)         - (numSamplesLog2 >> 1);
    pBlockSize->depth  = 0;

    if ((blockSizeLog2 & 1) != 0)
    {
        // Odd block sizes need to add 1 to either width or height
        if (pBlockSize->height < pBlockSize->width)
        {
            pBlockSize->height++;
        }
        else
        {
            pBlockSize->width++;
        }
    }
}

// =====================================================================================================================
uint32 Gfx9MetaEqGenerator::GetPipeBlockSize() const
{
    return GetEffectiveNumPipes() + 4;
}

// =====================================================================================================================
uint32 Gfx9MetaEqGenerator::GetEffectiveNumPipes() const
{
    const uint32 numPipesLog2 = m_pParent->GetGfxDevice()->GetNumPipesLog2();
    const uint32 numSaLog2    = IsGfx103PlusExclusive(*(m_pParent->GetGfxDevice()->Parent())) ? GetNumShaderArrayLog2() : 0;

    uint32 effectiveNumPipes = 0;

    if (m_pipeDist == PipeDist8x8)
    {
        effectiveNumPipes = numPipesLog2;
    }
    else if(numSaLog2 >= (numPipesLog2 - 1))
    {
        effectiveNumPipes = numPipesLog2;
    }
    else
    {
        effectiveNumPipes = numSaLog2 + 1;
    }

    return effectiveNumPipes;
}

// =====================================================================================================================
int32 Gfx9MetaEqGenerator::GetMetaOverlap() const
{
    const auto*            pPalDevice     = m_pParent->GetGfxDevice()->Parent();
    const uint32           bppLog2        = m_pParent->GetBytesPerPixelLog2();
    const uint32           numSamplesLog2 = m_pParent->GetNumSamplesLog2();
    const AddrSwizzleMode  swizzleMode    = m_pParent->GetSwizzleMode();
    const uint32           blockSizeLog2  = Log2(pPalDevice->GetAddrMgr()->GetBlockSize(swizzleMode));

    Gfx9MaskRamBlockSize compBlockLog2  = {};
    m_pParent->CalcCompBlkSizeLog2(&compBlockLog2);
    const uint32 compSize = compBlockLog2.width + compBlockLog2.height + compBlockLog2.depth;

    Gfx9MaskRamBlockSize microBlockLog2 = {};
    GetMicroBlockSize(&microBlockLog2);
    const uint32 microSize = microBlockLog2.width + microBlockLog2.height + microBlockLog2.depth;

    const uint32 maxSize = Max(compSize, microSize);
    int32 numPipesLog2 = GetEffectiveNumPipes();

    int32 overlap = numPipesLog2 - maxSize;
    if ((numPipesLog2 > 1) && (m_pipeDist == PipeDist16x16))
    {
        overlap++;
    }

    // In 16Bpp 8xaa, we lose 1 overlap bit because the block size reduction eats into a pipe anchor bit (y4)
    if (IsGfx11(*pPalDevice))
    {
        if ((bppLog2 == 4) && (numSamplesLog2 == 3) && (blockSizeLog2 == 16))
        {
            overlap--;
        }

        overlap += 16 - blockSizeLog2;
    }
    else
    {
        if ((bppLog2 == 4) && (numSamplesLog2 == 3))
        {
            overlap--;
        }
    }

    return Max(0, overlap);
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::GetMetaPipeAnchorSize(
    Extent2d*  pAnchorSize
    ) const
{
    const uint32 numPipesLog2 = GetEffectiveNumPipes();

    pAnchorSize->width  = 4 + (numPipesLog2 >> 1) + (numPipesLog2 & 1);
    pAnchorSize->height = 4 + (numPipesLog2 >> 1);

    if ((m_pipeDist == PipeDist16x16) && (numPipesLog2 > 1))
    {
        if (pAnchorSize->height < pAnchorSize->width)
        {
            pAnchorSize->height++;
        }
        else
        {
            pAnchorSize->width++;
        }
    }

    if (numPipesLog2 == 0)
    {
        pAnchorSize->width  = 0;
        pAnchorSize->height = 0;
    }
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::GetMicroBlockSize(
    Gfx9MaskRamBlockSize* pMicroBlockSize
    ) const
{
    const uint32           bppLog2        = m_pParent->GetBytesPerPixelLog2();
    const uint32           numSamplesLog2 = m_pParent->GetNumSamplesLog2();
    const AddrSwizzleMode  swizzleMode    = m_pParent->GetSwizzleMode();
    uint32                 blockBits      = 8 - bppLog2;

    if (IsZSwizzle(swizzleMode))
    {
        blockBits -= numSamplesLog2;
    }

    pMicroBlockSize->width  = (blockBits >> 1) + (blockBits & 1);
    pMicroBlockSize->height = (blockBits >> 1);
    pMicroBlockSize->depth  = 0;
}

// =====================================================================================================================
// Returns log2 of the total number of shader arrays on the GPU.
uint32 Gfx9MetaEqGenerator::GetNumShaderArrayLog2() const
{
    return m_pParent->GetGfxDevice()->Gfx103PlusExclusiveGetNumActiveShaderArraysLog2();
}

// =====================================================================================================================
void Gfx9MetaEqGenerator::GetPipeAnchorSize(
    Extent2d*  pAnchorSize
    ) const
{
    const uint32       numPipesLog2 = GetEffectiveNumPipes();
    const Pal::Device* pPalDevice   = m_pParent->GetGfxDevice()->Parent();

    pAnchorSize->width  = 4 + (numPipesLog2 >> 1);
    pAnchorSize->height = pAnchorSize->width;

    if (pPalDevice->ChipProperties().gfx9.rbPlus)
    {
        pAnchorSize->width += (numPipesLog2 & 1);

        if ((m_pipeDist == PipeDist16x16) && (numPipesLog2 > 1))
        {
            pAnchorSize->height++;
        }
    }
    else
    {
        pAnchorSize->height += (numPipesLog2 & 1);

        if ((m_pipeDist == PipeDist16x16) && (numPipesLog2 > 1))
        {
            pAnchorSize->width++;
        }

        if (numPipesLog2 == 0)
        {
            pAnchorSize->width  = 0;
            pAnchorSize->height = 0;
        }
    }
}

// =====================================================================================================================
uint32 Gfx9MetaEqGenerator::GetPipeRotateAmount() const
{
    const Pal::Device*  pPalDevice   = m_pParent->GetGfxDevice()->Parent();
    const uint32        numPipesLog2 = m_pParent->GetGfxDevice()->GetNumPipesLog2();
    const uint32        numSaLog2    = IsGfx103PlusExclusive(*pPalDevice) ? GetNumShaderArrayLog2() : 0;
    uint32  pipeRotateAmount = 0;

    if (IsGfx103Plus(*pPalDevice))
    {
        if ((numPipesLog2 >= (numSaLog2 + 1)) && (numPipesLog2 > 1))
        {
            if (numPipesLog2 == (numSaLog2 + 1))
            {
                pipeRotateAmount = 1;
            }
            else
            {
                pipeRotateAmount = numPipesLog2 - (numSaLog2 + 1);
            }
        }
    }
    else
    {
        if ((m_pipeDist == PipeDist16x16) && (numPipesLog2 >= (numSaLog2 + 1)))
        {
            pipeRotateAmount = numPipesLog2 - (numSaLog2 + 1);
        }
        else
        {
            pipeRotateAmount = 0;
        }
    }

    return pipeRotateAmount;
}

// =====================================================================================================================
// Calculates the meta equation for this mask-ram.  The meta-equation is ultimately used by a compute shader for
// determining the real location of any coordinates within the meta-data.
//
// Compute shader pseudo-code:
//      metaOffset = 0
//      for (n = 0; n < numBitsInEquation; n++)
//      {
//          // Yes, there is an IL instruction called "countBits".  It does exactly what we need.  :-)
//          b =        countBits(m_equation[n][MetaDataAddrCompX] & x) & 0x1
//          b = b XOR (countBits(m_equation[n][MetaDataAddrCompY] & y) & 0x1)
//          b = b XOR (countBits(m_equation[n][MetaDataAddrCompZ] & z) & 0x1)
//          b = b XOR (countBits(m_equation[n][MetaDataAddrCompS] & s) & 0x1)
//          b = b XOR (countBits(m_equation[n][MetaDataAddrCompM] & m) & 0x1)
//
//          metaOffset |= (b << n)
//      }
void Gfx9MetaEqGenerator::CalcMetaEquation()
{
    Gfx9MaskRamBlockSize   metaBlockSizeLog2 = {};

    const Pal::Device*     pPalDevice            = m_pParent->GetGfxDevice()->Parent();
    const AddrSwizzleMode  swizzleMode           = m_pParent->GetSwizzleMode();
    const uint32           blockSizeLog2         = m_pParent->GetMetaBlockSize(&metaBlockSizeLog2);
    const uint32           bppLog2               = m_pParent->GetBytesPerPixelLog2();
    const uint32           numSamplesLog2        = m_pParent->GetNumSamplesLog2();
    const uint32           numPipesLog2          = m_pParent->GetGfxDevice()->GetNumPipesLog2();
    uint32                 modNumPipesLog2       = numPipesLog2;
    const uint32           numShaderArraysLog2   = IsGfx103PlusExclusive(*pPalDevice) ? GetNumShaderArrayLog2() : 0;
    const uint32           effectiveNumPipesLog2 = GetEffectiveNumPipes();
    const uint32           cachelineSize         = m_pParent->GetMetaCachelineSize();
    const uint32           pipeInterleaveLog2    = m_pParent->GetGfxDevice()->GetPipeInterleaveLog2();
    const uint32           maxFragsLog2          = m_pParent->GetGfxDevice()->GetMaxFragsLog2();
    const uint32           maxCompFragsLog2      = (numSamplesLog2 < maxFragsLog2) ? numSamplesLog2 : maxFragsLog2;

    MetaDataAddrEquation  pipe(27);

    constexpr uint32 nibbleOffset = 1;
    PAL_ASSERT(m_meta.GetNumValidBits() >= (blockSizeLog2 + nibbleOffset));

    Gfx9MaskRamBlockSize compBlockLog2 = {};
    m_pParent->CalcCompBlkSizeLog2(&compBlockLog2);

    Gfx9MaskRamBlockSize pixelBlockLog2 = {};
    GetPixelBlockSize(&pixelBlockLog2);

    Extent2d  pipeAnchorLog2 = {};
    GetMetaPipeAnchorSize(&pipeAnchorLog2);

    int32 metaOverlap = GetMetaOverlap();

    CompPair  cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, compBlockLog2.width);
    CompPair  cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, compBlockLog2.height);
    CompPair  cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, compBlockLog2.depth);
    CompPair  cs = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompS, 0);

    uint32  start  = 0;
    uint32  end    = 0;
    bool    flipXY = false;

    if (IsStandardSwzzle(swizzleMode) || IsDisplayableSwizzle(swizzleMode) ||
        IsRotatedSwizzle(swizzleMode) || IsZSwizzle(swizzleMode))
    {
        start = m_metaDataWordSizeLog2 + nibbleOffset;
        end = start;

        if (IsZSwizzle(swizzleMode) == false)
        {
            end = start + maxCompFragsLog2;
            for (uint32 i = start; i < end; i++)
            {
                m_meta.SetBit(i, cs);
                cs.compPos++;
            }
        }

        PAL_ASSERT((IsStandardSwzzle(swizzleMode) == false) && (IsDisplayableSwizzle(swizzleMode) == false));
        // (!p.pipe_aligned || p.sw == SW_S || p.sw == SW_D) == (!p.pipe_aligned) since SW_S/SW_D wont be used:
        // 1) HTILE can not be used on a SW_S/SW_D image.
        // 2) Gfx9Dcc::UseDccForImage() disabled Dcc on SW_S/SW_D.
        if (m_pParent->PipeAligned() == false)
        {
            start = end;
            if ((numPipesLog2 > 0) &&
                ((compBlockLog2.width + compBlockLog2.height) > 6) &&
                (m_pipeDist == PipeDist8x8))
            {
                cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, 3);
                cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, 3);

                // Pal only uses PipeUnaligned for DisplayDcc which only has a single sample
                end = start + 2 - bppLog2;
                for (uint32 i = start; i < end; i++)
                {
                    m_meta.SetBit(i, cx);
                    cx.compPos++;
                    if ((i == start) || (numPipesLog2 > 1))
                    {
                        m_meta.SetBit(i, cy);
                    }
                    cy.compPos++;
                }
                start = end;
            }
            end = blockSizeLog2 + nibbleOffset;
            if (cy.compPos < cx.compPos)
            {
                m_meta.Mort2d(m_pParent->GetGfxDevice(), &cy, &cx, start, end - 1);
            }
            else
            {
                m_meta.Mort2d(m_pParent->GetGfxDevice(), &cx, &cy, start, end - 1);
            }
        }
        else
        {
            PAL_ASSERT(pipe.GetNumValidBits() >= (blockSizeLog2 + nibbleOffset));

            AddMetaPipeBits(&pipe, nibbleOffset);

            if (IsGfx103Plus(*pPalDevice)                   &&
                (numPipesLog2 == (numShaderArraysLog2 + 1)) &&
                (numPipesLog2 > 1))
            {
                modNumPipesLog2++;
            }

            uint32 pos1 = pipeInterleaveLog2 + nibbleOffset;

            start = end;
            end = (8 - compBlockLog2.width - compBlockLog2.height - compBlockLog2.depth) +
                  (((2 - static_cast<int32>(effectiveNumPipesLog2)) < 0) ?
                   0 : 2 - effectiveNumPipesLog2);

            if ((m_pipeDist == PipeDist16x16) && (effectiveNumPipesLog2 > 1))
            {
                end++;
            }

            end += start;
            flipXY = (compBlockLog2.height < compBlockLog2.width) ? true : false;

            const uint32 pipeRotateAmount = GetPipeRotateAmount();

            // This is needed to make htile pipe the same for all bpp modes under 32bpp
            if ((pipeRotateAmount >= 2)           &&
                (pipe.GetNumComponents(pos1) > 2) &&
                m_pParent->IsDepth()              &&
                (bppLog2 <= 2)                    &&
                (((numSamplesLog2 == 1) && (modNumPipesLog2 >= 6)) ||
                 ((numSamplesLog2 == 2) && (modNumPipesLog2 >= 5))))
            {
                if (IsGfx103Plus(*pPalDevice))
                {
                    if (blockSizeLog2 <= 16)
                    {
                        pipe.Remove(pipe.GetFirst(pos1), pos1);
                    }
                }
                else
                {
                    pipe.Remove(pipe.GetFirst(pos1), pos1);
                }
            }

            // In 16Bpe 8xaa, we have an extra overlap bit
            if ((pipeRotateAmount > 0) && (pixelBlockLog2.width == 4))
            {
                if (IsGfx103Plus(*pPalDevice))
                {
                    if (IsZSwizzle(swizzleMode) || (effectiveNumPipesLog2 > 3))
                    {
                        metaOverlap++;
                    }
                }
                else
                {
                    metaOverlap++;
                }
            }

            // Because y bits get into the overlap before x bits, we may need to swap an x/y bit pair
            // where the overlap bits end
            // 128 pipe is a little funky because although y3 starts the overlap, x3 is used in its place
            // in the equation, so no need to swap
            // In 16Bpp 8xaa, we lose 1 overlap bit, which is y2, so we don't need to do this swap
            bool swapOverlapEnd = ((metaOverlap & 1) != 0)                           &&
                                  (flipXY == false)                                  &&
                                  (effectiveNumPipesLog2 != 7)                       &&
                                  (((bppLog2 == 4) && (numSamplesLog2 == 3)) == false);

            // This is to handle a particular case SW_Z DCC 8 Bpp in 4xaa in 32 pipes
            // This is the only case where we have "gap" in the overlap which requires two y bits, instead of
            // interleaving y and x.  Other cases that have a gap end in y3/x4 which naturally works out with
            // interleaved x/y's
            swapOverlapEnd = swapOverlapEnd            ||
                             (IsZSwizzle(swizzleMode)  &&
                              (bppLog2 == 3)           &&
                              (numSamplesLog2 == 2)    &&
                              m_pParent->IsColor()     &&
                              (effectiveNumPipesLog2 == 5));

            // Note: this section of flipY1Y2 only affects SW_Z DCC MSAA addressing (which is not needed in gfx10)
            int32 posY1 = 3 - compBlockLog2.width - compBlockLog2.height;
            int32 posY2 = posY1 + 2;
            if (swapOverlapEnd && (posY1 <= 1))
            {
                posY1 = 1 ^ posY1;
            }

            bool flipY1Y2 = ((compBlockLog2.height <  2    ) &&
                             (metaOverlap          >  posY1) &&
                             (metaOverlap          <= posY2));

            int32 tileSplitBits = 0;
            if (m_pipeDist == PipeDist16x16)
            {
                flipY1Y2 = false;

                if (IsGfx103Plus(*pPalDevice))
                {
                    int32 subTileXYBits = 6 - compBlockLog2.width - compBlockLog2.height;
                    if ((subTileXYBits < 0) || (IsZSwizzle(swizzleMode) == false))
                    {
                        subTileXYBits = 0;
                    }

                    if (IsGfx10(*pPalDevice))
                    {
                        tileSplitBits = (3 - compBlockLog2.width ) +
                                        (3 - compBlockLog2.height) -
                                        metaOverlap;

                        tileSplitBits = Max(0, tileSplitBits);

                        tileSplitBits = Min((int32)numSamplesLog2, tileSplitBits);
                    }

                    if ((numSamplesLog2  == 3)             &&
                        (subTileXYBits   == 3)             &&
                        (tileSplitBits   >  0)             &&
                        (tileSplitBits   <  subTileXYBits) &&
                        (modNumPipesLog2 <  6))
                    {
                        flipY1Y2 = true;
                    }

                    if (IsGfx11(*pPalDevice)  &&
                        (blockSizeLog2 == 18) &&
                        (numSamplesLog2 == 3) &&
                        (bppLog2 == 2)        &&
                        (numPipesLog2 == 6)   &&
                        (numShaderArraysLog2 == 4))
                    {
                        flipY1Y2 = true;
                    }
                }
            }

            uint32 tileOrd = (m_pipeDist == PipeDist8x8) ? 3 : 4;

            for (uint32 i = start; i < end; i++)
            {
                if (static_cast<bool>((i - start) & 1) == flipXY)
                {
                    if (IsGfx103Plus(*pPalDevice)   &&
                        (cx.compPos == 4)           &&
                        (effectiveNumPipesLog2 > 0) &&
                        (m_pipeDist == PipeDist16x16))
                    {
                        // use y4 instead x4
                        m_meta.SetBit(i, cy);
                        cy.compPos++;
                    }
                    else
                    {
                        if ((cx.compPos == 4)           &&
                            (effectiveNumPipesLog2 > 1) &&
                            (m_pipeDist == PipeDist8x8))
                        {
                            cx.compPos++;
                        }
                        m_meta.SetBit(i, cx);
                        cx.compPos++;
                    }
                }
                else
                {
                    uint32 ord = cy.compPos;
                    if ((ord == tileOrd) && (effectiveNumPipesLog2 > 0))
                    {
                        cy.compPos++;
                    }

                    if (flipY1Y2)
                    {
                        switch (ord)
                        {
                        case 1:
                            cy.compPos++;
                            break;
                        case 2:
                            cy.compPos--;
                            break;
                        }
                    }

                    m_meta.SetBit(i, cy);

                    if (flipY1Y2)
                    {
                        switch (ord)
                        {
                        case 1:
                            cy.compPos--;
                            break;
                        case 2:
                            cy.compPos++;
                            break;
                        }
                    }
                    cy.compPos++;
                }
            }

            start = end;
            end   = blockSizeLog2 - effectiveNumPipesLog2 + nibbleOffset;
            cx    = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, Max(5u, pipeAnchorLog2.width));
            cy    = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, Max(5u, pipeAnchorLog2.height));

            uint32 shiftBit0 = 0;
            uint32 shiftBit1 = 0;

            if (IsGfx103Plus(*pPalDevice))
            {
                if (IsGfx103Plus(*pPalDevice)                              &&
                    (blockSizeLog2 == (pipeInterleaveLog2 + numPipesLog2)) &&
                    ((modNumPipesLog2 - numPipesLog2) == 1))
                {
                    cx.compPos--;
                    end++;
                }
                uint32 samples = IsRotatedSwizzle(swizzleMode) ? maxCompFragsLog2 : numSamplesLog2;

                shiftBit0 = (m_pParent->IsColor() == false)
                            ? (2 + m_metaDataWordSizeLog2 + nibbleOffset)
                            : (bppLog2 + samples + nibbleOffset); // start + 2*(pipe_block_ord - cy.getord());
                shiftBit1 = 0;

                CompPair c = pipe.GetFirst(pos1);

                if (c.compPos < 4u)
                {
                    if (IsZSwizzle(swizzleMode))
                    {
                        samples = 0;
                    }

                    const int32 compBlockOrd = (c.compType == MetaDataAddrCompX)
                                               ? compBlockLog2.width
                                               : compBlockLog2.height;

                    shiftBit1 = 2 * (c.compPos - compBlockOrd) + samples + nibbleOffset;
                    if (((c.compType          == MetaDataAddrCompX)     &&
                         (compBlockLog2.width >  compBlockLog2.height)) ||
                        ((c.compType == MetaDataAddrCompY) && (compBlockLog2.height > compBlockLog2.width)))
                    {
                        shiftBit1++;
                    }
                }
                else
                {
                    int32 ord = (c.compType == MetaDataAddrCompX) ? cx.compPos : cy.compPos;
                    if ((ord > c.compPos) && (c.compType == MetaDataAddrCompX))
                    {
                        ord = cy.compPos;
                        c   = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, ord);
                    }

                    shiftBit1 = start + 2 * (c.compPos - ord);
                    if (c.compType == MetaDataAddrCompY)
                    {
                        if (cy.compPos >= cx.compPos)
                        {
                            shiftBit1++;
                        }
                    }
                    else if (cx.compPos > cy.compPos)
                    {
                        shiftBit1++;
                    }
                }

                if (end > start)
                {
                    if (m_pipeDist == PipeDist8x8)
                    {
                        if (cy.compPos < cx.compPos)
                        {
                            m_meta.Mort2d(m_pParent->GetGfxDevice(), &cy, &cx, start, end - 1);
                        }
                        else
                        {
                            m_meta.Mort2d(m_pParent->GetGfxDevice(), &cx, &cy, start, end - 1);
                        }
                    }
                    else
                    {
                        if (cy.compPos < cx.compPos)
                        {
                            cx.compPos--;
                            m_meta.SetBit(start, cx);
                            cx.compPos++;
                            cy.compPos++;
                            start++;
                        }

                        if (IsGfx103Plus(*pPalDevice)               &&
                            ((modNumPipesLog2 - numPipesLog2) == 1) &&
                            (cx.compPos < cy.compPos))
                        {
                            m_meta.SetBit(start, cx);
                            cx.compPos++;
                            start++;
                        }
                        if (end > start)
                        {
                            m_meta.Mort2d(m_pParent->GetGfxDevice(), &cx, &cy, start, end - 1);
                        }
                    }
                }
            }
            else
            {
                const uint32 pipe_block_ord = GetPipeBlockSize();
                shiftBit0 = start + 2 * (pipe_block_ord - cy.compPos); // start + 2*(pipe_block_ord - cy.getord());
                shiftBit1 = shiftBit0;

                if (end > start)
                {
                    if (cy.compPos < cx.compPos)
                    {
                        m_meta.Mort2d(m_pParent->GetGfxDevice(), &cy, &cx, start, end - 1);
                        shiftBit1++;
                    }
                    else
                    {
                        m_meta.Mort2d(m_pParent->GetGfxDevice(), &cx, &cy, start, end - 1);
                        shiftBit0++;
                    }
                }

                if (shiftBit0 == (blockSizeLog2 - numPipesLog2 + pipeRotateAmount + nibbleOffset))
                {
                    shiftBit0--;
                }

                if (shiftBit0 >= (blockSizeLog2 - numPipesLog2 + pipeRotateAmount + nibbleOffset))
                {
                    shiftBit0 = 2 * (4 - compBlockLog2.width) + nibbleOffset;
                    if (compBlockLog2.height < compBlockLog2.width)
                    {
                        shiftBit0++;
                    }
                    if (IsRotatedSwizzle(swizzleMode))
                    {
                        shiftBit0 += maxFragsLog2;
                    }
                }

                if (shiftBit1 >= (blockSizeLog2 - numPipesLog2 + pipeRotateAmount + nibbleOffset))
                {
                    CompPair c = pipe.GetFirst(pos1);
                    if (c.compType == MetaDataAddrCompX)
                    {
                        shiftBit1 = 2 * (c.compPos - compBlockLog2.width) + nibbleOffset;
                        if (compBlockLog2.height < compBlockLog2.width)
                        {
                            shiftBit1++;
                        }
                    }
                    else
                    {
                        shiftBit1 = 2 * (c.compPos - compBlockLog2.height) + nibbleOffset;
                        if (compBlockLog2.height >= compBlockLog2.width)
                        {
                            shiftBit1++;
                        }
                    }
                    if (IsRotatedSwizzle(swizzleMode))
                    {
                        shiftBit1 += maxFragsLog2;
                    }
                }
            }

            if ((pipeRotateAmount >= 2) && (shiftBit1 > shiftBit0))
            {
                Swap(shiftBit0, shiftBit1);
            }

            if (pipeRotateAmount > 0)
            {
                m_meta.Shift(-1, shiftBit0);
            }

            if (pipeRotateAmount > 1)
            {
                m_meta.Shift(-1, shiftBit1);
            }

            start = m_metaDataWordSizeLog2 + nibbleOffset + (IsZSwizzle(swizzleMode) ? 0 : maxCompFragsLog2);

            if (IsGfx103Plus(*pPalDevice) && (m_pipeDist == PipeDist16x16))
            {
                CompPair co = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, 0);

                const uint32  location = start + metaOverlap - 1;

                if (m_meta.IsEmpty(location) == false)
                {
                    co = m_meta.Get(location);
                }

                const uint32              rotatedSwizzleCount = (IsRotatedSwizzle(swizzleMode) ? numSamplesLog2 : 0);
                MetaDataAddrComponentType dim                 = ((pixelBlockLog2.width == 4) &&
                                                                 (effectiveNumPipesLog2 & 1) &&
                                                                 (effectiveNumPipesLog2 > 1) &&
                                                                 ((metaOverlap + rotatedSwizzleCount) & 1) == 0)
                                                                ? MetaDataAddrCompX
                                                                : MetaDataAddrCompY;

                if ((numSamplesLog2  == 3)  &&
                    (metaOverlap     == 2)  &&
                    (bppLog2         == 2)  &&
                    (modNumPipesLog2 <  6))
                {
                    dim = MetaDataAddrCompX;
                }

                swapOverlapEnd = ((co.compType != dim) && (metaOverlap > 0));
            }
            if (swapOverlapEnd)
            {
                m_meta.Rotate(1, start + metaOverlap - 1, start + metaOverlap);
            }

            bool swapB0B1 = false;
            bool swapB1B2 = false;
            bool swapB0B5 = false;
            bool swapB0B4 = false;
            bool swapB0B2 = false;
            bool swapB1B3 = false;
            if (IsGfx11(*pPalDevice) && m_pParent->IsColor())
            {
                // ugly...hardcode the table
                swapB0B1 =
                    ((blockSizeLog2 == 18) &&
                     (((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3))));

                swapB1B2 =
                    ((blockSizeLog2 == 18) &&
                     (((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 2) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 0) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 2) && (numSamplesLog2 == 3))));

                swapB1B2 =
                    ((blockSizeLog2 == 16) &&
                     (((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 0) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 0) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 0) && (numSamplesLog2 == 1))));

                swapB0B1 =
                    ((blockSizeLog2 == 16) &&
                     (((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 1) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 1) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 0) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 1) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3))));

                swapB0B4 =
                    ((blockSizeLog2 == 16) &&
                     (((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3))));

                swapB0B5 =
                    ((blockSizeLog2 == 16) &&
                     (((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 1)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 1) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 1) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 2) && (numShaderArraysLog2 == 0) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 3) && (numShaderArraysLog2 == 0) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 1) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 0) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 1) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 2) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3))));

                swapB0B2 =
                    ((blockSizeLog2 == 18) &&
                     (((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 3))));

                swapB1B3 =
                    ((blockSizeLog2 == 18) &&
                     (((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 4) && (numSamplesLog2 == 2)) ||
                      ((numPipesLog2 == 4) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 5) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 3) && (bppLog2 == 4) && (numSamplesLog2 == 3)) ||
                      ((numPipesLog2 == 6) && (numShaderArraysLog2 == 4) && (bppLog2 == 3) && (numSamplesLog2 == 3))));
            } // end check for GFX11 and color surfaces

            end = cachelineSize + metaOverlap;

            if (IsGfx103Plus(*pPalDevice) && (end > (blockSizeLog2 - modNumPipesLog2)))
            {
                end = blockSizeLog2 - modNumPipesLog2;
            }

            end += nibbleOffset;

            m_meta.Rotate(-metaOverlap, start, end - 1);

            start = pipeInterleaveLog2 + nibbleOffset;
            m_meta.Shift(modNumPipesLog2, start);

            m_meta.XorIn(&pipe);
            if (IsGfx103Plus(*pPalDevice))
            {
                if (pipeRotateAmount > 1)
                {
                    m_meta.AdjustPipe(modNumPipesLog2,
                                      pipeInterleaveLog2 + nibbleOffset);
                }

            if (IsGfx11(*pPalDevice))
            {
                if (m_pParent->IsColor())
                {
                    m_meta.Rotate(-tileSplitBits, 1, cachelineSize);
                }

                if (swapB0B1)
                {
                    m_meta.Rotate(1, 1, 2);
                }

                if (swapB1B2)
                {
                    m_meta.Rotate(1, 2, 3);
                }

                if (swapB0B2)
                {
                    m_meta.Swap(1, 3);
                }

                if (swapB1B3)
                {
                    m_meta.Swap(2, 4);
                }

                if (swapB0B4)
                {
                    m_meta.Swap(1, 5);
                }

                if (swapB0B5)
                {
                    m_meta.Swap(1, 6);
                }

                if (m_pParent->IsColor() && IsZSwizzle(swizzleMode) && (numSamplesLog2 == 1) && (bppLog2 == 0))
                {
                    if (((numPipesLog2 == 3) && (numShaderArraysLog2 == 0)) ||
                        ((numPipesLog2 == 5) && (numShaderArraysLog2 == 2)))
                    {
                        PAL_NOT_IMPLEMENTED();
                    }

                    if ((numPipesLog2 == 4) && (numShaderArraysLog2 == 1))
                    {
                        PAL_NOT_IMPLEMENTED();
                    }
                }
            }

                if (m_pParent->IsDepth() && IsGfx103Plus(*pPalDevice))
                {
                    // for htile, move the top RB bit outside the 2KB cacheline region
                    m_meta.Rotate(numPipesLog2 - modNumPipesLog2,
                                  pipeInterleaveLog2 + numPipesLog2 + nibbleOffset,
                                  -1);
                }
            }
        }
    }

    // The equation is currently 32-bits long, but on GFX10, the equation is an offset into one meta-block
    // (unlike on GFX9 where the equation is an offset into the entire mask-ram), so trim this down to the
    // the log2 of one meta-block.
    FinalizeMetaEquation(pPalDevice->GetAddrMgr()->GetBlockSize(swizzleMode));
}

//=============== Implementation for Gfx9Htile: ========================================================================

// =====================================================================================================================
// Determimes if the given Image object should use HTILE metadata.
HtileUsageFlags Gfx9Htile::UseHtileForImage(
    const Pal::Device& device,
    const Image&       image)
{
    const Pal::Image*const pParent    = image.Parent();
    HtileUsageFlags        hTileUsage = {};

    // If this isn't a depth buffer, then hTile is a non-starter.
    if (pParent->IsDepthStencilTarget())
    {
        const auto& settings   = GetGfx9Settings(device);
        const auto& createInfo = pParent->GetImageCreateInfo();

        hTileUsage.dsMetadata = ((pParent->IsShared()                   == false) &&
                                 (pParent->IsMetadataDisabledByClient() == false) &&
                                 (pParent->IsTmz()                      == false) &&
                                 (settings.htileEnable                  == true));

        //
        // Verify that hTile should be allowed (i.e, minus the workaround) to silence the below assert for
        // non-depth images.  i.e., this function is called for all images not just depth images.
        if ((hTileUsage.dsMetadata != 0)                                                          &&
             settings.waForceZonlyHtileForMipmaps                                                 &&
             (device.SupportsDepth(createInfo.swizzledFormat.format, createInfo.tiling) == false) &&
             (createInfo.mipLevels > 1))
        {
            PAL_ASSERT(device.SupportsStencil(createInfo.swizzledFormat.format, createInfo.tiling));

            hTileUsage.dsMetadata = 0;
        }

        if (pParent->GetImageInfo().internalCreateInfo.flags.vrsOnlyDepth == 1)
        {
            // If we're creating an hTile-only depth buffer, then don't allow depth-stencil metadata
            // as there isn't any depth/stencil data to manage in this case.
            hTileUsage.dsMetadata = 0;
        }

        // If this device supports VRS and the client has indicated that this depth image will potentially
        // be bound during a VRS-enabled draw, then
        // Does this device support VRS?
        if (device.ChipProperties().gfxip.supportsVrs &&
            // Does this device record VRS data into hTile memory?
            IsGfx10(device)                           &&
            // Has the client indicated that this depth image will potentially be bound during a VRS-enabled draw?
            (createInfo.usageFlags.vrsDepth != 0))
        {
            // Ok, hTile memory *must* be allocated for this image so that there will ultimately be a place to store
            // the shading-rate data...  which doesn't mean that the hTile surface will be used to store compression
            // data.
            hTileUsage.vrs = 1;
        }
    }

    return hTileUsage;
}

// =====================================================================================================================
Gfx9Htile::Gfx9Htile(
    const Image&     image,
    void*            pPlacementAddr,
    HtileUsageFlags  hTileUsage)
    :
    Gfx9MaskRam(image,
                pPlacementAddr,
                2,  // hTile uses 32-bit (4 byte) quantities
                3), // Equation is nibble addressed, so the low three bits will be zero for a dword quantity
    m_hTileUsage(hTileUsage)
{
    memset(&m_addrMipOutput,   0, sizeof(m_addrMipOutput));
    memset(&m_addrOutput,      0, sizeof(m_addrOutput));
    memset(m_dbHtileSurface,   0, sizeof(m_dbHtileSurface));

    m_addrOutput.pMipInfo = &m_addrMipOutput[0];
    m_addrOutput.size     = sizeof(m_addrOutput);
    m_flags.value         = 0;
}

// =====================================================================================================================
uint32 Gfx9Htile::GetNumSamplesLog2() const
{
    return Log2(m_image.Parent()->GetImageCreateInfo().samples);
}

// =====================================================================================================================
// Initializes this HTile object for the given Image and mipmap level.
Result Gfx9Htile::Init(
    gpusize*  pGpuOffset,    // [in,out] Current GPU memory offset & size
    bool      hasEqGpuAccess)
{
    const  Pal::Device&      device           = *(m_pGfxDevice->Parent());
    const  Gfx9PalSettings&  settings         = GetGfx9Settings(device);
    const  Pal::Image*const  pParent          = m_image.Parent();
    const  ImageCreateInfo&  imageCreateInfo  = pParent->GetImageCreateInfo();
    const  auto&             chipProps        = device.ChipProperties();
    const  uint32            activeRbCount    = chipProps.gfx9.numActiveRbs;

    m_flags.compressZ = settings.depthCompressEnable   && (m_hTileUsage.dsMetadata != 0);
    m_flags.compressS = settings.stencilCompressEnable && (m_hTileUsage.dsMetadata != 0);

    // NOTE: Default ZRANGE_PRECISION to 1, since this is typically the optimal value for DX applications, since they
    // usually clear Z to 1.0f and use a < depth comparison for their depth testing.
    m_flags.zrangePrecision = 1;

    // If the associated image is a Z-only format, then setup hTile to not include stencil data.
    // However, if this hTile data will contain VRS information, then the stencil
    // data must be included again, even if it won't be used.
    m_flags.tileStencilDisable = (m_image.IsHtileDepthOnly() && (m_hTileUsage.vrs == 0));

    // Htile control registers vary per mip-level.  Compute those here.
    for (uint32  mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
    {
        const SubresId              subResId          = { 0, mipLevel, 0 };
        const SubResourceInfo*const pSubResInfo       = pParent->SubresourceInfo(subResId);
        const uint32                imageSizeInPixels = (pSubResInfo->actualExtentTexels.width *
                                                         pSubResInfo->actualExtentTexels.height);
        const uint32                pixelsPerRb       = imageSizeInPixels / activeRbCount;

        if (pixelsPerRb <= (256 * 1024)) // <= 256K pixels
        {
            m_dbHtileSurface[mipLevel].bits.FULL_CACHE = 0;
        }
        else
        {
            m_dbHtileSurface[mipLevel].bits.FULL_CACHE = 1;
        }

        m_dbHtileSurface[mipLevel].bits.DST_OUTSIDE_ZERO_TO_ONE = 0;

    }

    // Call the address library to compute the HTile properties.
    const SubResourceInfo*  pBaseSubResInfo = pParent->SubresourceInfo(0);
    Result                  result          = ComputeHtileInfo(pBaseSubResInfo);
    if (result == Result::Success)
    {
        // Compute our aligned GPU memory offset and update the caller-provided running total.  Don't update the
        // overall image size with every mip level as the entire size of hTile is computed all at once.
        UpdateGpuMemOffset(pGpuOffset);

        // The addressing equation is the same for all sub-resources, so only bother to calculate it once
        PAL_ASSERT(HasMetaEqGenerator());
        m_pEqGenerator->CalcMetaEquation();

        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the hTile equation
            PAL_ASSERT(HasMetaEqGenerator());
            m_pEqGenerator->InitEqGpuAccess(pGpuOffset);
        }
    }

    return result;
}

// =====================================================================================================================
// Calls into AddrLib to compute HTILE info for a subresource
Result Gfx9Htile::ComputeHtileInfo(
    const SubResourceInfo* pSubResInfo)
{
    const Pal::Device&     device             = *(m_pGfxDevice->Parent());
    const Gfx9PalSettings& settings           = GetGfx9Settings(device);
    const Pal::Image*const pParent            = m_image.Parent();
    const ImageCreateInfo& imageCreateInfo    = pParent->GetImageCreateInfo();
    const auto&            surfSettings       = m_image.GetAddrSettings(pSubResInfo);
    const auto*const       pAddrMgr           = static_cast<const AddrMgr2::AddrMgr2*>(device.GetAddrMgr());
    const auto*            pParentSurfAddrOut = m_image.GetAddrOutput(pSubResInfo);

    Result result = Result::ErrorInitializationFailed;

    ADDR2_COMPUTE_HTILE_INFO_INPUT addrHtileIn = {};
    addrHtileIn.size              = sizeof(addrHtileIn);
    addrHtileIn.swizzleMode       = surfSettings.swizzleMode;
    addrHtileIn.unalignedWidth    = imageCreateInfo.extent.width;
    addrHtileIn.unalignedHeight   = imageCreateInfo.extent.height;
    addrHtileIn.numSlices         = imageCreateInfo.arraySize;
    addrHtileIn.numMipLevels      = imageCreateInfo.mipLevels;
    addrHtileIn.depthFlags        = pAddrMgr->DetermineSurfaceFlags(*pParent, pSubResInfo->subresId.plane, false);
    addrHtileIn.hTileFlags        = GetMetaFlags();
    addrHtileIn.firstMipIdInTail  = pParentSurfAddrOut->firstMipIdInTail;

    const ADDR_E_RETURNCODE addrRet = Addr2ComputeHtileInfo(device.AddrLibHandle(), &addrHtileIn, &m_addrOutput);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        // HW needs to be programmed to the same parameters the surface was created with.
        for (uint32  mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
        {
            m_dbHtileSurface[mipLevel].bits.PIPE_ALIGNED = addrHtileIn.hTileFlags.pipeAligned;

            if (device.ChipProperties().gfxip.supportsVrs)
            {
                // If the hTile surface has VRS data (either specified by the client or an internally created
                // "image" that has VRS data), then setup the regs to indicate VRS encoding in hTile.
                if (m_hTileUsage.vrs != 0)
                {

                    // 1:  two bit encoding in hTile
                    // 2:  four bit encoding in hTile
                    //
                    m_dbHtileSurface[mipLevel].gfx10Vrs.VRS_HTILE_ENCODING = settings.vrsHtileEncoding;
                }
                else if (IsGfx10(device))
                {
                    // This hTile buffer does not contain any VRS data.
                    m_dbHtileSurface[mipLevel].gfx10Vrs.VRS_HTILE_ENCODING = 0;
                }
            }
        }

        m_alignment = m_addrOutput.baseAlign;
        m_sliceSize = m_addrOutput.sliceSize;
        m_totalSize = m_addrOutput.htileBytes;

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
void Gfx9Htile::CalcCompBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    // For non-color surfaces, compressed block size is always 8x8
    pBlockSize->width  = 3;
    pBlockSize->height = 3;
    pBlockSize->depth  = 0;
}

// =====================================================================================================================
void Gfx9Htile::CalcMetaBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkWidth));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkHeight));

    pBlockSize->width  = Log2(m_addrOutput.metaBlkWidth);
    pBlockSize->height = Log2(m_addrOutput.metaBlkHeight);
    pBlockSize->depth  = 0;  // No 3D depth, depth is zero; log2(1) == 0)
}

// =====================================================================================================================
// Computes a value for updating the HTile buffer for a fast depth clear.
uint32 Gfx9Htile::GetClearValue(
    float depthValue
    ) const
{
    // Maximum 14-bit UINT value.
    constexpr uint32 MaxZVal = 0x3FFF;

    // For clears, Zmask and Smem will always be set to zero.
    constexpr uint32 ZMask = 0;
    constexpr uint32 SMem  = 0;

    // Convert depthValue to 14-bit zmin/zmax uint values:
    const uint32 zMin = static_cast<uint32>((depthValue * MaxZVal) + 0.5f);
    const uint32 zMax = zMin;

    uint32 htileValue = 0;

    if (TileStencilDisabled() == false)
    {
        // If stencil is present, each HTILE is laid out as-follows, according to the DB spec:
        // |31       12|11 10|9    8|7   6|5   4|3     0|
        // +-----------+-----+------+-----+-----+-------+
        // |  Z Range  |     | SMem | SR1 | SR0 | ZMask |

        // The base value for zRange is either zMax or zMin, depending on ZRANGE_PRECISION. For a fast clear,
        // zMin == zMax == clearValue. This means that the base will always be the clear value (converted to 14-bit
        // UINT).
        //
        // When abs(zMax-zMin) < 16, the delta is equal to the difference. In the case of fast clears, where
        // zMax == zMin, the delta is always zero.
        constexpr uint32 Delta = 0;
        const uint32 zRange    = ((zMax << 6) | Delta);

        // SResults 0 & 1 are set based on the stencil compare state.
        // For fast-clear, the default value of sr0 and sr1 are both 0x3.
        constexpr uint32 SResults = 0xf;

        htileValue = ( ((zRange   & 0xFFFFF) << 12) |
                       ((SMem     &     0x3) <<  8) |
                       ((SResults &     0xF) <<  4) |
                       ((ZMask    &     0xF) <<  0) );
    }
    else
    {
        // If stencil is absent, each HTILE is laid out as follows, according to the DB spec:
        // |31     18|17      4|3     0|
        // +---------+---------+-------+
        // |  Max Z  |  Min Z  | ZMask |

        htileValue = ( ((zMax  & 0x3FFF) << 18) |
                       ((zMin  & 0x3FFF) <<  4) |
                       ((ZMask &    0xF) <<  0) );
    }

    return htileValue;
}

// =====================================================================================================================
// Computes a mask for updating the specified planes of the HTile buffer
uint32 Gfx9Htile::GetPlaneMask(
    uint32  planeFlags
    ) const
{
    uint32 mask = 0;

    if (TileStencilDisabled() == false)
    {
        if (TestAnyFlagSet(planeFlags, HtilePlaneDepth))
        {
            mask |= Gfx9HtileDepthMask;
        }
        if (TestAnyFlagSet(planeFlags, HtilePlaneStencil))
        {
            mask |= Gfx9HtileStencilMask;
        }
    }
    else if (TestAnyFlagSet(planeFlags, HtilePlaneDepth))
    {
        // All bits are used for depth when tile stencil is disabled
        mask = UINT_MAX;
    }

    return mask;
}

// =====================================================================================================================
// A helper function for when the caller just wants the plane mask for an entire subresource range.
uint32 Gfx9Htile::GetPlaneMask(
    const SubresRange& range
    ) const
{
    uint32 htileMask = 0;

    if (range.numPlanes == 2)
    {
        htileMask = (HtilePlaneDepth | HtilePlaneStencil);
    }
    else
    {
        htileMask = m_image.Parent()->IsDepthPlane(range.startSubres.plane) ? HtilePlaneDepth : HtilePlaneStencil;
    }

    return GetPlaneMask(htileMask);
}

// =====================================================================================================================
// Computes the initial value of the htile which depends on whether or not tile stencil is disabled. We want this
// initial value to disable all HTile-based optimizations so that the image is in a trivially valid state. This should
// work well for inits and also for "fast" resummarize blits where we just want the HW to see the base data values.
uint32 Gfx9Htile::GetInitialValue() const
{
    constexpr uint32 Uint14Max = 0x3FFF; // Maximum value of a 14bit integer.

    // Convert the trivial z bounds to 14-bit zmin/zmax uint values. These values will give us HiZ bounds that cover
    // all Z values, effectively disabling HiZ.
    constexpr uint32 ZMin  = 0;
    constexpr uint32 ZMax  = Uint14Max;
    constexpr uint32 ZMask = 0xf;        // No Z compression.

    uint32 initialValue;

    if (TileStencilDisabled())
    {
        // Z only (no stencil):
        //      |31     18|17      4|3     0|
        //      +---------+---------+-------+
        //      |  Max Z  |  Min Z  | ZMask |

        initialValue = (((ZMax  & Uint14Max) << 18) |
                        ((ZMin  & Uint14Max) <<  4) |
                        ((ZMask &       0xF) <<  0));
    }
    else
    {
        // Z and stencil:
        //      |31       12|11 10|9    8|7   6|5   4|3     0|
        //      +-----------+-----+------+-----+-----+-------+
        //      |  Z Range  |     | SMem | SR1 | SR0 | ZMask |
        //
        // Z, stencil, 2 bit VRS encoding:
        //      |31       12|       11     |      10      |9    8|7   6|5   4|3     0|
        //      +-----------+--------------+--------------+------+-----+-----+-------+
        //      |  Z Range  | VRS Y-rate 0 | VRS X-rate 0 | SMem | SR1 | SR0 | ZMask |
        //
        // i.e., same as Z+stencil with unused bits used to encode the LSB of the VRS X and Y rates.
        //
        // Z, stencil, 4 bit VRS encoding:
        //      |31       12| 11      10 |9    8|7         6 |5   4|3     0|
        //      +-----------+------------+------+------------+-----+-------+
        //      |  Z Range  | VRS Y-rate | SMem | VRS X-rate | SR0 | ZMask |
        //
        // i.e., same as Z+stencil with SR1 overloaded bo be the VRS x-rate data.

        // The base value for zRange is either zMax or zMin, depending on ZRANGE_PRECISION. Currently, PAL programs
        // ZRANGE_PRECISION to 1 (zMax is the base) by default. Sometimes we switch to 0 if we detect a fast-clear to
        // Z = 0 but that will rewrite HTile so we can ignore that case when we compute our initial value.
        //
        // zRange is encoded as follows: the high 14 bits are the base z value (zMax in our case). The low 6 bits
        // are a code represending the abs(zBase - zOther). In our case, we need to select a delta code representing
        // abs(zMax - zMin), which is always 0x3FFF (maximum 14 bit uint value). The delta code in our case would be
        // 0x3F (all 6 bits set).
        constexpr uint32 Delta  = 0x3F;
        constexpr uint32 ZRange = ((ZMax << 6) | Delta);
        constexpr uint32 SMem   = 0x3; // No stencil compression.
        constexpr uint32 SR1    = 0x3; // Unknown stencil test result.
        constexpr uint32 SR0    = 0x3; // Unknown stencil test result.

        initialValue = (((ZRange & 0xFFFFF) << 12) |
                        ((SMem   &     0x3) <<  8) |
                        ((SR1    &     0x3) <<  6) |
                        ((SR0    &     0x3) <<  4) |
                        ((ZMask  &     0xF) <<  0));

        const     auto&  settings = GetGfx9Settings(*(m_pGfxDevice->Parent()));
        // If hTile uses four-bit VRS encoding, then the init-value needs to leave the SR1 bits at zero
        // so that VRS interprets the X-rate as 1 sample.
        // Two-bit VRS encoding uses previously unused bits, so there's no conern in that situation.
        if ((m_hTileUsage.vrs == 1) &&
            (settings.vrsHtileEncoding == VrsHtileEncoding::Gfx10VrsHtileEncodingFourBit))
        {
            initialValue &= (~Sr1Mask);
        }
    }

    return initialValue;
}

// =====================================================================================================================
// Calculates the meta-block dimensions.
//
uint32 Gfx9Htile::GetMetaBlockSize(
    Gfx9MaskRamBlockSize* pExtent
    ) const
{
    CalcMetaBlkSizeLog2(pExtent);

    return Log2(m_addrOutput.baseAlign);
}
//=============== Implementation for Gfx9Dcc: ==========================================================================

// =====================================================================================================================
Gfx9Dcc::Gfx9Dcc(
    const Image& image,
    void*        pPlacementAddr,
    bool         displayDcc)
    :
    Gfx9MaskRam(image,
                pPlacementAddr,
                0,  // DCC uses 1-byte quantities, log2(1) = 0
                1), // ignore the first bit of a nibble equation
    m_dccControl(),
    m_displayDcc(displayDcc)
{
    memset(&m_addrMipOutput, 0, sizeof(m_addrMipOutput));
    memset(&m_addrOutput,    0, sizeof(m_addrOutput));

    m_addrOutput.size     = sizeof(m_addrOutput);
    m_addrOutput.pMipInfo = &m_addrMipOutput[0];
}

// =====================================================================================================================
uint32 Gfx9Dcc::PipeAligned() const
{
    const AddrSwizzleMode  swizzleMode = GetSwizzleMode();

    // most surfaces are pipe-aligned for more performant access.
    // Display Dcc: from UMDKMDIF_GET_PRIMARYSURF_INFO_OUTPUT::KeyPipeAligned, 0 for Gfx10. Has to be unaligned to reach
    // here, aligned cases are filtered out in Device::CreateImage.
    uint32 pipeAligned = (m_displayDcc == false);

    //     Meta surfaces with SW_256B_D and SW_256B_S data swizzle mode must not be pipe aligned
    if (Is256BSwizzle(swizzleMode))
    {
        pipeAligned = 0;
    }

    return pipeAligned;
}

// =====================================================================================================================
uint32 Gfx9Dcc::GetMetaBlockSize(
    Gfx9MaskRamBlockSize* pExtent
    ) const
{
    CalcMetaBlkSizeLog2(pExtent);

    return Log2(m_addrOutput.metaBlkSize);
}

// =====================================================================================================================
gpusize Gfx9Dcc::SliceOffset(
    uint32  arraySlice
    ) const
{
    // Always assume mip zero.  The HW is expecting the base address of the allocation; on GFX10, mip 0 is not
    // necessarily where the allocation begins.  Adding in the offset to where "mip 0" actually begins is bad.
    constexpr uint32           MipLevel    = 0;
    const ADDR2_META_MIP_INFO& addrMipInfo = GetAddrMipInfo(MipLevel); // assume mip zero

    // We should only really be requesting either the base subresource (i.e., slice = mip = 0) or a distinct slice
    // (for YUV surfaces only).
    PAL_ASSERT((arraySlice == 0) || IsYuv(m_image.Parent()->GetImageCreateInfo().swizzledFormat.format));

    // Because we're always requesting mip level 0, we don't have to add in "addrMipInfo.offset" here; doing so is
    // actually bad on GFX10 as mip 0 can have a non-zero offset, which causes the HW address block issues as we
    // would no longer be pointing to the start of the allocation (i.e., good) but would point to MIP 0 itself (bad).
    return arraySlice * addrMipInfo.sliceSize;
}

// =====================================================================================================================
uint8 Gfx9Dcc::GetInitialValue(
    ImageLayout layout
    ) const
{
    // If nothing else applies, initialize to "uncompressed"
    uint8 initialValue = Gfx9Dcc::DecompressedValue;

    const DccInitialClearKind clearKind =
        static_cast<DccInitialClearKind>(m_pGfxDevice->Parent()->GetPublicSettings()->dccInitialClearKind);
    bool isForceEnabled = TestAnyFlagSet(static_cast<uint32>(clearKind),
                                         static_cast<uint32>(DccInitialClearKind::ForceBit));

    if ((clearKind != DccInitialClearKind::Uncompressed) &&
        ((ImageLayoutToColorCompressionState(m_image.LayoutToColorCompressionState(), layout) != ColorDecompressed) ||
         isForceEnabled))
    {
        uint32 color[4] = {};
        constexpr uint32 Ones[4] = {1, 1, 1, 1};

        switch (clearKind)
        {
            case DccInitialClearKind::ForceOpaqueBlack:
            case DccInitialClearKind::OpaqueBlack:
                color[3] = Ones[3];
                break;
            case DccInitialClearKind::ForceOpaqueWhite:
            case DccInitialClearKind::OpaqueWhite:
                color[0] = Ones[0];
                color[1] = Ones[1];
                color[2] = Ones[2];
                color[3] = Ones[3];
                break;
            default:
                PAL_ASSERT_ALWAYS();
        }

        GetBlackOrWhiteClearCode(m_image.Parent(), color, Ones, &initialValue);
        PAL_ALERT_MSG(initialValue == Gfx9Dcc::DecompressedValue,
                      "Failed to get compatible clear color for InitMaskRam");
    }

    return initialValue;
}

// =====================================================================================================================
uint32 Gfx9Dcc::GetNumEffectiveSamples(
    DccClearPurpose  clearPurpose
    ) const
{
    // If this is an init, then we want to write every pixel that the equation can address.  the number of samples
    // addressed by the equation isn't necessarily the same as the number of samples contained in the image (I
    // don't understand that either...).
    PAL_ASSERT(HasMetaEqGenerator());
    uint32  numSamples = m_pEqGenerator->GetNumEffectiveSamples();
    if (clearPurpose == DccClearPurpose::FastClear)
    {
        // Using max_compressed_frag on MSAA image requires CMask / FMask always be on.
        PAL_ASSERT((numSamples == 1) || m_image.HasFmaskData());

        //    The idea of max_compressed_frag is we lose a lot of benefit from the DCC compression when we go beyond
        //    compressing the first fragment or two.   Beyond a certain fragment we're unlikely to have a lot of
        //    pixels touching it.   Thus any compression will likely be poor compression (e.g. 8:7 compression).
        //    This poor compression will ultimately require us to use more bandwidth reading data for this fragment
        //    then we might otherwise need to read.  For instance we only need to read 32Bytes for the few pixels
        //    using fragment 3, but because it's been combined with and compressed to 7 blocks of 32, we end up having
        //    to read them all.  Thus we want to place a limit on how much CB will compress.
        //
        //    However, this limit also requires that region beyond be initialized to uncompressed.  This is to make
        //    sure that the DCC Keys are consistent with anything other than CB that may look at DCC compressed
        //    surfaces.  CB RTL itself will not read DCC Keys for fragments beyond max compressed frag.   This saves
        //    bandwidth and RTL merely reads and writes the fragments uncompressed.   (EMU reads the keys to do this
        //    check and make sure things are initialized properly.)   It's not clear that any other clients that may
        //    use DCC surfaces will employ the max_compressed_frag setting and thus need to see 0xff so as not to
        //    corrupt the data.
        numSamples = Min(numSamples, 1u << m_pGfxDevice->GetMaxFragsLog2());
    }

    return numSamples;
}

// =====================================================================================================================
uint32 Gfx9Dcc::GetNumSamplesLog2() const
{
    // The number of samples, which is used for the calculation of the DCC equation is set as this:
    //      input_num_samples   = (pCS->mode_resolve && mrt>0) ? 1 : pCS->num_fragments[mrt];
    return Log2(m_image.Parent()->GetImageCreateInfo().fragments);
}

// =====================================================================================================================
// Returns the dimensions, in pixels, of a block that gets compressed to one DCC byte.
void Gfx9Dcc::GetXyzInc(
    uint32*  pXinc, // [out] Num X pixels that get compressed into one DCC byte
    uint32*  pYinc, // [out] Num Y pixels that get compressed into one DCC byte
    uint32*  pZinc  // [out] Num Z pixels that get compressed into one DCC byte
    ) const
{
    const uint32           bppLog2     = GetBytesPerPixelLog2();
    const ImageCreateInfo& createInfo  = m_image.Parent()->GetImageCreateInfo();
    const AddrSwizzleMode  swizzleMode = GetSwizzleMode();

    // There are instances where 3D images use the 2D layout.
    bool  isEffective2d    = AddrMgr2::IsDisplayableSwizzle(swizzleMode);
    bool  useZswizzleFor3d = AddrMgr2::IsZSwizzle(swizzleMode);

    const Pal::Device& palDevice = *(m_pGfxDevice->Parent());

    if (createInfo.imageType == ImageType::Tex3d)
    {
        //   SW_Z and SW_R are use the same 256B block dimensions, whether 1d, 2d, or 3d.
        //   3D_D mode has the same dimensions as what's shown for 3D_Z.
        //
        // SW_64KB_Z_X
        //    This option organize 3D volume maps the same as a 2DArray using SW_64KB_Z_X.  See the 2D block
        //    swizzle section of this doc for details.
        //
        // SW_64KB_R_X
        //    This option organizes 3D volume maps the same as a 2DArray using SW_64KB_R_X.
        isEffective2d    = AddrMgr2::IsZSwizzle(swizzleMode) || AddrMgr2::IsRotatedSwizzle(swizzleMode);

        // This should never be true, courtesy of the UseDccForImage function.  _S and _D modes have so
        // many restrictions on their use with DCC that we never allocate it for those swizzle modes.
        // So this is for completeness only.
        useZswizzleFor3d = AddrMgr2::IsDisplayableSwizzle(swizzleMode);
    }

    if ((createInfo.imageType == ImageType::Tex2d) || isEffective2d)
    {
        constexpr uint32 XyzIncSizes[][3]=
        {
            { 16, 16, 1 },  // 8bpp
            { 16,  8, 1 },  // 16bpp
            {  8,  8, 1 },  // 32bpp
            {  8,  4, 1 },  // 64bpp
            {  4,  4, 1 },  // 128bpp
        };

        *pXinc = XyzIncSizes[bppLog2][0];
        *pYinc = XyzIncSizes[bppLog2][1];
        *pZinc = XyzIncSizes[bppLog2][2];

        if (IsGfx11(palDevice))
        {
            uint32 numSamples = createInfo.samples;

            // Note that we can't have MSAA for 3D images.
            //    Color MSAA surfaces are sample interleaved, the way depth always has been.  So each 256 Bytes region
            //    is smaller than [it was previously].  It will halve in Morton order.
            while (numSamples > 1)
            {
                // The general rule is that when dividing a square, divide the height, otherwise, divide the width
                if (*pXinc == *pYinc)
                {
                    *pYinc /= 2;
                }
                else
                {
                    *pXinc /= 2;
                }

                numSamples = numSamples / 2;
            }
        }
    }
    else if (createInfo.imageType == ImageType::Tex3d)
    {
        if (useZswizzleFor3d)
        {
            constexpr uint32 XyzIncSizes[][3]=
            {
                { 8, 4, 8 },  // 8bpp
                { 4, 4, 8 },  // 16bpp
                { 4, 4, 4 },  // 32bpp
                { 4, 2, 4 },  // 64bpp
                { 2, 2, 4 },  // 128bpp
            };

            *pXinc = XyzIncSizes[bppLog2][0];
            *pYinc = XyzIncSizes[bppLog2][1];
            *pZinc = XyzIncSizes[bppLog2][2];
        }
        else if (AddrMgr2::IsStandardSwzzle(swizzleMode))
        {
            constexpr uint32 XyzIncSizes[][3]=
            {
                { 16, 4, 4 },  // 8bpp
                {  8, 4, 4 },  // 16bpp
                {  4, 4, 4 },  // 32bpp
                {  4, 2, 4 },  // 64bpp
                {  1, 4, 4 },  // 128bpp
            };

            *pXinc = XyzIncSizes[bppLog2][0];
            *pYinc = XyzIncSizes[bppLog2][1];
            *pZinc = XyzIncSizes[bppLog2][2];
        }
        else
        {
            // 3D surfaces of other swizzle modes should have had isEffective2d set.
            PAL_ASSERT_ALWAYS();
        }
    }
    else
    {
        PAL_ASSERT_ALWAYS();

        // 1D images should never get here.
    }
}

// =====================================================================================================================
Result Gfx9Dcc::Init(
    const SubresId&  subResId,
    gpusize*         pGpuOffset,
    bool             hasEqGpuAccess)
{
    Result result = ComputeDccInfo(subResId);

    if (result == Result::Success)
    {
        if (IsGfx11(*(m_pGfxDevice->Parent())))
        {
            const Gfx9PalSettings& settings = GetGfx9Settings(*(m_pGfxDevice->Parent()));

            m_alignment  = Max(m_alignment, static_cast<gpusize>(settings.gfx11OverrideMetadataAlignment));
            m_totalSize *= settings.gfx11MetadataSizeMultiplier;
        }

        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);

        SetControlReg(subResId);

        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the DCC equation
            PAL_ASSERT(HasMetaEqGenerator());
            m_pEqGenerator->InitEqGpuAccess(pGpuOffset);
        }
    }

    return result;
}

// =====================================================================================================================
// Calls into AddrLib to compute DCC info for a subresource
Result Gfx9Dcc::ComputeDccInfo(
    const SubresId&  subResId)
{
    const Pal::Image*const      pParent            = m_image.Parent();
    const Pal::Device*const     pDevice            = pParent->GetDevice();
    const Pal::ImageCreateInfo& imageCreateInfo    = pParent->GetImageCreateInfo();
    const auto*const            pAddrMgr           = static_cast<const AddrMgr2::AddrMgr2*>(pDevice->GetAddrMgr());
    const SubResourceInfo*      pSubResInfo        = pParent->SubresourceInfo(subResId);
    const auto&                 surfSettings       = m_image.GetAddrSettings(pSubResInfo);
    const auto*                 pParentSurfAddrOut = m_image.GetAddrOutput(pSubResInfo);

    ADDR2_COMPUTE_DCCINFO_INPUT  dccInfoInput = {};
    Result                       result       = Result::ErrorInitializationFailed;

    dccInfoInput.size             = sizeof(dccInfoInput);
    dccInfoInput.dccKeyFlags      = GetMetaFlags();
    dccInfoInput.colorFlags       = pAddrMgr->DetermineSurfaceFlags(*pParent, subResId.plane, false);
    dccInfoInput.resourceType     = m_image.GetAddrSettings(pSubResInfo).resourceType;
    dccInfoInput.swizzleMode      = surfSettings.swizzleMode;
    dccInfoInput.bpp              = BitsPerPixel(pSubResInfo->format.format);
    dccInfoInput.unalignedWidth   = imageCreateInfo.extent.width;
    dccInfoInput.unalignedHeight  = imageCreateInfo.extent.height;
    dccInfoInput.numFrags         = imageCreateInfo.fragments;
    dccInfoInput.numSlices        = ((imageCreateInfo.imageType != ImageType::Tex3d)
                                     ? imageCreateInfo.arraySize
                                     : imageCreateInfo.extent.depth);
    dccInfoInput.numMipLevels     = imageCreateInfo.mipLevels;
    dccInfoInput.dataSurfaceSize  = static_cast<UINT_32>(m_image.GetAddrOutput(pSubResInfo)->surfSize);
    dccInfoInput.firstMipIdInTail = pParentSurfAddrOut->firstMipIdInTail;

    const ADDR_E_RETURNCODE addrRet = Addr2ComputeDccInfo(pDevice->AddrLibHandle(), &dccInfoInput, &m_addrOutput);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        m_alignment = m_addrOutput.dccRamBaseAlign;
        m_sliceSize = 0; // todo, how to set this?
        m_totalSize = m_addrOutput.dccRamSize;

        PAL_ASSERT(HasMetaEqGenerator());
        m_pEqGenerator->CalcMetaEquation();

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Calculates the value for the CB_DCC_CONTROL register
void Gfx9Dcc::SetControlReg(
    const SubresId&  subResId)
{
    const Pal::Image*              pParent      = m_image.Parent();
    const SubResourceInfo*         pSubResInfo  = pParent->SubresourceInfo(subResId);
    const Pal::Device*             pDevice      = m_pGfxDevice->Parent();
    const GfxIpLevel               gfxLevel     = pDevice->ChipProperties().gfxLevel;
    const ImageCreateInfo&         createInfo   = pParent->GetImageCreateInfo();
    const auto&                    settings     = GetGfx9Settings(*pDevice);
    const ImageInternalCreateInfo& internalInfo = pParent->GetInternalCreateInfo();
    const DisplayDccCaps&          dispDcc      = internalInfo.displayDcc;

    // Setup DCC control registers with suggested value from spec
    m_dccControl.gfx10.KEY_CLEAR_ENABLE           = 0;
    m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE = uint32(Gfx9DccMaxBlockSize::BlockSize256B);
    m_dccControl.bits.MIN_COMPRESSED_BLOCK_SIZE   = settings.minDccCompressedBlockSize;

    static_assert(DCC_CT_AUTO == 0, "ColorTransform Enum values change!");
    static_assert(DCC_CT_NONE == 1, "ColorTransform Enum values change!");
    m_dccControl.bits.COLOR_TRANSFORM        = settings.colorTransform;
    m_dccControl.gfx10.LOSSY_RGB_PRECISION   = 0;
    m_dccControl.gfx10.LOSSY_ALPHA_PRECISION = 0;

    // If this DCC surface is potentially going to be used in texture fetches though, we need some special settings.
    // - MAX_UNCOMPRESSED_BLOCK_SIZE is always 256B unless dcc_128_128_unconstrained is exclusively set.
    // - MAX_COMPRESSED_BLOCK_SIZE is decided by DisplayDccCaps, with priority 256B > 128B > 64B.
    // - INDEPENDENT_128B_BLOCKS is always set to 1, either in gfx10 field or gfx11.
    // - INDEPENDENT_64B_BLOCKS is set to 1 only if MAX_COMPRESSED_BLOCK_SIZE is 64B, i.e., 256B or 128B is unsupported.
    if (pSubResInfo->flags.supportMetaDataTexFetch)
    {
        if ((dispDcc.dcc_128_128_unconstrained == 1) &&
            (dispDcc.dcc_256_256_unconstrained == 0) &&
            (dispDcc.dcc_256_128_128           == 0) &&
            (dispDcc.dcc_256_64_64             == 0)
            )
        {
            m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE =
                static_cast<unsigned int>(Gfx9DccMaxBlockSize::BlockSize128B);
        }

        if (dispDcc.dcc_256_64_64 == 0)
        {
            m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = uint32(Gfx9DccMaxBlockSize::BlockSize128B);
        }
        else
        {
            m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = uint32(Gfx9DccMaxBlockSize::BlockSize64B);
        }

        if (IsGfx10(gfxLevel))
        {
            m_dccControl.gfx10.INDEPENDENT_128B_BLOCKS = 1;
        }
        else if (IsGfx11(gfxLevel))
        {
            m_dccControl.gfx11.INDEPENDENT_128B_BLOCKS = 1;
        }

        if ((dispDcc.dcc_256_64_64 == 1) &&
            (m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE == uint32(Gfx9DccMaxBlockSize::BlockSize64B)))
        {
            m_dccControl.bits.INDEPENDENT_64B_BLOCKS = 1;
        }
        else
        {
            m_dccControl.bits.INDEPENDENT_64B_BLOCKS = 0;
        }

        PAL_ASSERT(m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE <= m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE);
    }
    else
    {
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS    = 0;

        // Note that MAX_UNCOMPRESSED_BLOCK_SIZE must >= MAX_COMPRESSED_BLOCK_SIZE
        // Set MAX_COMPRESSED_BLOCK_SIZE as big as possible for better compression ratio
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;
    }

    if (IsGfx10(gfxLevel)                        &&
        m_image.Gfx10UseCompToSingleFastClears() &&
        (TestAnyFlagSet(settings.useCompToSingle, Gfx10DisableCompToReg)))
    {
        // If the image was potentially fast-cleared to comp-to-single mode (i.e., no fast-clear-eliminate is
        // required), then we can't allow the CB / DCC to render in comp-to-reg mode because that *will* require
        // an FCE operation.
        //
        // The other option here is to not touch this bit at all and to allow the fast-clear-eliminate to proceed
        // anyway.  Because CB_DCC_CONTROL.DISABLE_ELIMFC_SKIP_OF_SINGLE=0, any DCC codes left at comp-to-
        // single will be skipped during the FCE and it will (theoretically) be a super-fast-fast-clear-eliminate.
        // Theoretically.
        m_dccControl.bits.DISABLE_CONSTANT_ENCODE_REG = 1;
    }

    if (internalInfo.flags.useSharedDccState)
    {
        // Use the shared control register values when available
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE    = internalInfo.gfx9.sharedDccState.maxCompressedBlockSize;
        m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE  = internalInfo.gfx9.sharedDccState.maxUncompressedBlockSize;
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS       = internalInfo.gfx9.sharedDccState.independentBlk64B;
        if (IsGfx10(gfxLevel))
        {
            m_dccControl.gfx10.INDEPENDENT_128B_BLOCKS = internalInfo.gfx9.sharedDccState.independentBlk128B;
        }
        else
        {
            m_dccControl.gfx11.INDEPENDENT_128B_BLOCKS = internalInfo.gfx9.sharedDccState.independentBlk128B;
        }
    }
}

// =====================================================================================================================
// Determines if the given Image object should use DCC (delta color compression) metadata.
bool Gfx9Dcc::UseDccForImage(
    const Image&  image,                     // image that will own the proposed DCC surface
    bool          metaDataTexFetchSupported) // If meta data tex fetch is suppported
{
    const Pal::Image*const  pParent      = image.Parent();
    const auto&             createInfo   = pParent->GetImageCreateInfo();
    const auto&             internalInfo = pParent->GetInternalCreateInfo();
    const Pal::Device*const pDevice      = pParent->GetDevice();
    const PalSettings&      settings     = pDevice->Settings();
    const auto              pPalSettings = pDevice->GetPublicSettings();

    // Assume that DCC is available; check for conditions where it won't work.
    bool useDcc         = true;
    bool mustDisableDcc = false;

    // If the device supports the MM formats and we are a format that would use them, then we don't
    // have to disable DCC

    bool allMipsShaderWritable = pParent->IsShaderWritable();

    allMipsShaderWritable = (allMipsShaderWritable && (pParent->FirstShaderWritableMip() == 0));

    const bool isNotARenderTarget = (pParent->IsRenderTarget() == false);
    const bool isDepthStencil     = pParent->IsDepthStencilTarget();

    // GFX9+ resources have the same swizzle mode for all mip-levels and slices, so just look at the base level.
    const SubResourceInfo*const pSubResInfo = pParent->SubresourceInfo(0);
    const AddrSwizzleMode       swizzleMode = image.GetAddrSettings(pSubResInfo).swizzleMode;

    if (pParent->IsMetadataDisabledByClient())
    {
        // Don't use DCC if the caller asked that we allocate no metadata.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if ((createInfo.metadataMode == MetadataMode::FmaskOnly) &&
             (createInfo.samples > 1) &&
             (pParent->IsRenderTarget() == true))
    {
        // Don't use DCC if the caller asked that we allocate color msaa image with Fmask metadata only.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (pParent->GetDccFormatEncoding() == DccFormatEncoding::Incompatible)
    {
        // Don't use DCC if the caller can switch between view formats that are not DCC compatible with each other.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (createInfo.usageFlags.vrsRateImage != 0)
    {
        // The HW has no ability to handle compression on VRS source rate images.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (Is256BSwizzle(swizzleMode))
    {
        // If using 256B swizzle, don't use DCC as perf hit is too great.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (AddrMgr2::IsLinearSwizzleMode(swizzleMode))
    {
        // If the tile-mode is linear, then this surface has no chance of using DCC memory.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (AddrMgr2::IsStandardSwzzle(swizzleMode) || AddrMgr2::IsDisplayableSwizzle(swizzleMode))
    {
        //   1) SW_D does not support DCC.
        //   2) The restriction [on SW_S] is that you can't bind it to all CBs for a single draw.  If you want
        //      SW_S + DCC to be bound to the CB, it would have to go through a single CB (that is, change
        //      raster_config to route all traffic to 1 CB).  You could also use a SW compressor, and bind it
        //      to texture (say for static image)
        useDcc = false;
        mustDisableDcc = true;

        // The above check for "standard swizzle" is a bit of a misnomer; it's really a check for "S"
        // swizzle modes.  On GFX10, the "R" modes (render targets, not rotated) are expected to be the
        // most commonly used.  Still, throw an indication if we're faling into this code path with any
        // sort of frequency
        PAL_ALERT(isNotARenderTarget == false);
    }
    else if (isDepthStencil)
    {
        // DCC does not make sense for any depth/stencil image because they have Htile data.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (isNotARenderTarget               &&
             (allMipsShaderWritable == false) &&
             // YUV surfaces aren't (normally) render targets or UAV destinations, but they still potentially benefit
             // from DCC compression.
             // Also disable DCC if this image does not use X8_MM/X8Y8_MM and it does not use X16_MM/X16Y16_MM formats.
             ((Formats::IsYuvPlanar(createInfo.swizzledFormat.format) == false) ||
              (TestAnyFlagSet(settings.useDcc, UseDccYuvPlanar)       == false) ||
              (pParent->UsesMmFormat() == false)))
    {
        // DCC should always be off for a resource that is not a UAV and is not a render target.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (pParent->IsShared())
    {
        // DCC is never available for shared images.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if ((pParent->IsPresentable() || pParent->IsFlippable()) &&
             (pParent->GetInternalCreateInfo().displayDcc.enabled == false))
    {
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (createInfo.prtPlus.mapType != PrtMapType::None)
    {
        // Do not allow compression on PRT+ map images.  They are expected to be tiny and they will be
        // updated almost exclusively via shader writes anyway which are known to be problematic.
        useDcc = false;
        mustDisableDcc = true;
    }
    // Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function resolve is not
    // preferred, the image will be readable by a shader.
    else if ((pParent->IsShaderReadable() ||
              (pParent->IsResolveSrc() && (pParent->PreferCbResolve() == false))) &&
             (metaDataTexFetchSupported == false) &&
             (TestAnyFlagSet(settings.useDcc, UseDccNonTcCompatShaderRead) == false))
    {
        // Disable DCC for shader read resource that cannot be made TC compat, this avoids DCC decompress
        // for RT->SR barrier.
        useDcc = false;
    }
    else if (((createInfo.extent.width * createInfo.extent.height) <=
             (pPalSettings->hintDisableSmallSurfColorCompressionSize *
              pPalSettings->hintDisableSmallSurfColorCompressionSize))
            )
    {
        // DCC should be disabled if the client has indicated that they want to disable color compression on small
        // surfaces and this surface qualifies.
        useDcc = false;
    }
    else if (pPalSettings->dccBitsPerPixelThreshold > BitsPerPixel(createInfo.swizzledFormat.format))
    {
        useDcc = false;
    }
    else
    {
        if (allMipsShaderWritable)
        {
            if (isNotARenderTarget)
            {
                // If we are a UAV and not a render target, check our setting
                useDcc = TestAnyFlagSet(settings.useDcc, UseDccNonRenderTargetUav);
            }
            else
            {
                // If we are a UAV and a render target, check our other setting.
                useDcc = TestAnyFlagSet(settings.useDcc, UseDccRenderTargetUav);
            }
        }

        // Make sure the settings allow use of DCC surfaces for sRGB Images.
        if (Formats::IsSrgb(createInfo.swizzledFormat.format) &&
            (TestAnyFlagSet(settings.useDcc, UseDccSrgb) == false))
        {
            useDcc = false;
        }
        else if (Formats::IsYuvPacked(createInfo.swizzledFormat.format))
        {
            // DCC isn't useful for packed YUV formats, since those are usually accessed heavily
            // by the multimedia engines.
            useDcc = false;
            mustDisableDcc = true;
        }
        else if ((createInfo.flags.prt == 1) && (TestAnyFlagSet(settings.useDcc, UseDccPrt) == false))
        {
            // Make sure the settings allow use of DCC surfaces for PRT.
            useDcc = false;
        }
        else if (createInfo.samples > 1)
        {
            // Make sure the settings allow use of DCC surfaces for MSAA.
            if (createInfo.samples == 2)
            {
                useDcc &= TestAnyFlagSet(settings.useDcc, UseDccMultiSample2x);
            }
            else if (createInfo.samples == 4)
            {
                useDcc &= TestAnyFlagSet(settings.useDcc, UseDccMultiSample4x);
            }
            else if (createInfo.samples == 8)
            {
                useDcc &= TestAnyFlagSet(settings.useDcc, UseDccMultiSample8x);
            }

            if (createInfo.samples != createInfo.fragments)
            {
                useDcc &= TestAnyFlagSet(settings.useDcc, UseDccEqaa);
            }
        }
        else
        {
            // Make sure the settings allow use of DCC surfaces for single-sampled surfaces
            useDcc &= TestAnyFlagSet(settings.useDcc, UseDccSingleSample);
        }

        if (useDcc && (createInfo.arraySize > 1) && (createInfo.mipLevels > 1) &&
            (TestAnyFlagSet(settings.useDcc, UseDccMipMappedArrays) == false))
        {
            useDcc = false;
        }
    }

    if ((mustDisableDcc == false) &&
        (TestAnyFlagSet(settings.useDcc, UseDccAllowForceEnable)) &&
        (createInfo.metadataMode == MetadataMode::ForceEnabled))
    {
        useDcc = true;
    }

    if (internalInfo.flags.useForcedDcc != 0)
    {
        useDcc = (internalInfo.gfx9.sharedDccState.isDccForceEnabled != 0) ? true : false;
        PAL_ASSERT((mustDisableDcc == false) || (useDcc == false));
    }

    return useDcc;
}

// =====================================================================================================================
// Determines if the given Image object should use fast color clears.
bool Gfx9Dcc::SupportFastColorClearWithoutFormatCheck(
    const Pal::Device& device,
    const Image&       image,
    AddrSwizzleMode    swizzleMode)
{
    const ImageCreateInfo& createInfo = image.Parent()->GetImageCreateInfo();
    const Gfx9PalSettings& settings   = GetGfx9Settings(device);

    // Choose which fast-clear setting to examine based on the type of Image we have.
    const bool fastColorClearEnable = (createInfo.imageType == ImageType::Tex2d) ?
                                       settings.fastColorClearEnable : settings.fastColorClearOn3dEnable;

    // The only fast color clear mode available on GFX11 is comp-to-single; if this has been disabled, then we can't
    // do a fast clear. Note that this is not strictly true as fast clears to "black" or "white" are always possible.
    const bool enableAc01Clears   = (static_cast<Device*>(device.GetGfxDevice())->DisableAc01ClearCodes() == false);
    const bool fastClearAvailable = (IsGfx10(device) || enableAc01Clears || image.Gfx10UseCompToSingleFastClears());

    // Fast clears must be: 1. enabled, 2: available for this image, 3: used on non-linear swizzle modes.
    return (fastColorClearEnable && fastClearAvailable && (AddrMgr2::IsLinearSwizzleMode(swizzleMode) == false));
}

// =====================================================================================================================
bool Gfx9Dcc::SupportFastColorClearOnlyCheckFormat(
    const Image&	image)
{
    const Pal::Image*const pParent    = image.Parent();
    const ImageCreateInfo& createInfo = pParent->GetImageCreateInfo();

    return SupportsFastColorClear(createInfo.swizzledFormat.format);
}

// =====================================================================================================================
void Gfx9Dcc::CalcCompBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.compressBlkWidth));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.compressBlkHeight));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.compressBlkDepth));

    pBlockSize->width  = Log2(m_addrOutput.compressBlkWidth);
    pBlockSize->height = Log2(m_addrOutput.compressBlkHeight);
    pBlockSize->depth  = Log2(m_addrOutput.compressBlkDepth);
}

// =====================================================================================================================
void Gfx9Dcc::CalcMetaBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkWidth));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkHeight));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkDepth));

    pBlockSize->width  = Log2(m_addrOutput.metaBlkWidth);
    pBlockSize->height = Log2(m_addrOutput.metaBlkHeight);
    pBlockSize->depth  = Log2(m_addrOutput.metaBlkDepth);
}

// =====================================================================================================================
// Calculates the 8-bit value which represents the value the DCC surface should be cleared to.
// NOTE:
//    Surfaces that will not be texture-fetched can be fast-cleared to any color.  These will always return a clear
//    code that corresponds to "Gfx9DccClearColor::Reg".  Surfaces that will potentially be texture-fetched though can
//    only be fast-cleared to one of four HW-defined colors.
uint8 Gfx9Dcc::GetFastClearCode(
    const Image&       image,
    const SubresRange& clearRange,
    const uint32*      pConvertedColor,
    bool*              pNeedFastClearElim, // [out] true if this surface will require a fast-clear-eliminate pass
                                           //       before it can be used as a texture
    bool*              pBlackOrWhite)      // [out] true if this clear color is either black or white and corresponds
                                           //       to one of the "special" clear codes.
{
    PAL_ASSERT(clearRange.numPlanes == 1);

    // Fast-clear code that is valid for images that won't be texture fetched.
    const Pal::Image*  const     pParent         = image.Parent();
    const Pal::Device* const     pDevice         = pParent->GetDevice();
    const Device*const           pGfxDevice      = static_cast<Device*>(pDevice->GetGfxDevice());
    const ImageCreateInfo&       createInfo      = pParent->GetImageCreateInfo();
    Gfx9DccClearColor            clearCode       = Gfx9DccClearColor::ClearColorInvalid;
    const auto&                  settings        = GetGfx9Settings(*pDevice);
    const SubresId               baseSubResource = clearRange.startSubres;
    const SubResourceInfo* const pSubResInfo     = pParent->SubresourceInfo(baseSubResource);

    // Assume this is non-black/white clear color
    bool  blackOrWhite = false;

    // If we use the MM formats, then we can't use the special clear codes. When a client clears
    // to color(0,0,0,0), it is unclear whether they want all 0s or black and it is most intuitive
    // to take their request literally and clear to all 0s. With MM formats, a clear code of 0000
    // will end up writing out 16s because the hardware will take 0000 to mean black (16 in YUV land).
    if ((pSubResInfo->flags.supportMetaDataTexFetch != 0) &&
        (pGfxDevice->DisableAc01ClearCodes() == false)    &&
        (pParent->UsesMmFormat() == false))
    {
        // Surfaces that are fast cleared to one of the following colors may be texture fetched:
        //      1) AC00 : Alpha     0, Color     0
        //      2) AC01 : Alpha "one", Color     0
        //      3) AC10 : Alpha     0, Color "one"
        //      4) AC11 : Alpha "one", Color "one"
        //
        // If the clear-color is *not* one of those colors, then this routine will produce the "default"
        // clear-code.  The default clear-code is not understood by the TC and a fast-clear-eliminate pass must be
        // issued prior to using this surface as a texture.
        const uint32           numComponents = NumComponents(createInfo.swizzledFormat.format);
        const SurfaceSwap      surfSwap      = Formats::Gfx9::ColorCompSwap(createInfo.swizzledFormat);
        const ChannelSwizzle*  pSwizzle      = &createInfo.swizzledFormat.swizzle.swizzle[0];

        uint32 color[4] = {};
        uint32 ones[4]  = {};
        uint32 cmpIdx   = 0;
        uint32 rgbaIdx  = 0;

        switch(numComponents)
        {
        case 1:
            while ((pSwizzle[rgbaIdx] != ChannelSwizzle::X) && (rgbaIdx < 4))
            {
                rgbaIdx++;
            }

            PAL_ASSERT(pSwizzle[rgbaIdx] == ChannelSwizzle::X);

            if (IsGfx10(*pDevice))
            {

                // For Gfx10 ASICs and for single-component format:
                // alpha_is_on_msbs == 1, the component is alpha.
                // alpha_is_on_msbs == 0, the component is color.
                const bool alphaOnMsb = (surfSwap == SWAP_ALT_REV);

                color[2] = alphaOnMsb ? 0 : pConvertedColor[rgbaIdx];
                color[3] = alphaOnMsb ? pConvertedColor[rgbaIdx] : 0;
            }
            else
            {
                color[2] =
                color[3] = pConvertedColor[rgbaIdx];
            }

            color[0] =
            color[1] = color[2];

            ones[0] =
            ones[1] =
            ones[2] =
            ones[3] = image.TranslateClearCodeOneToNativeFmt(0);

            break;
        case 2:
            {
                // revComponentOrder (i.e. component order is reversed) decides if first/last component represents alpha
                // revComponentOrder == false - Y component is alpha, X component is color.
                // revComponentOrder == true  - X component is alpha, Y component is color.
                const bool revComponentOrder = (surfSwap == SWAP_STD_REV) || (surfSwap == SWAP_ALT_REV);

                cmpIdx = static_cast<uint32>(pSwizzle[0]) - static_cast<uint32>(ChannelSwizzle::X);

                for (rgbaIdx = 0; rgbaIdx < 4; rgbaIdx++)
                {
                    if (pSwizzle[rgbaIdx] == (revComponentOrder ? ChannelSwizzle::X : ChannelSwizzle::Y))
                    {
                        color[3] = pConvertedColor[rgbaIdx];

                        ones[3]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
                    }
                    else if (pSwizzle[rgbaIdx] == (revComponentOrder ? ChannelSwizzle::Y : ChannelSwizzle::X))
                    {
                        color[0] =
                        color[1] =
                        color[2] = pConvertedColor[rgbaIdx];

                        ones[0]  =
                        ones[1]  =
                        ones[2]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
                    }
                }
            }
            break;
        case 3:
            for (rgbaIdx = 0; rgbaIdx < 3; rgbaIdx++)
            {
                color[rgbaIdx] = pConvertedColor[rgbaIdx];

                PAL_ASSERT(pSwizzle[rgbaIdx] >= ChannelSwizzle::X);

                cmpIdx = static_cast<uint32>(pSwizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[rgbaIdx] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
            }
            color[3] = 0;
            ones[3]  = 0;
            break;
        case 4:
            for (rgbaIdx = 0; rgbaIdx < 4; rgbaIdx++)
            {
                color[rgbaIdx] = pConvertedColor[rgbaIdx];

                if (pSwizzle[rgbaIdx] == ChannelSwizzle::One)
                {
                    // Only for swizzle format XYZ1 / ZYX1
                    PAL_ASSERT(rgbaIdx == 3);

                    color[rgbaIdx] = color[2];
                    ones[rgbaIdx]  = ones[2];
                }
                else
                {
                    PAL_ASSERT(pSwizzle[rgbaIdx] != ChannelSwizzle::Zero);

                    cmpIdx = static_cast<uint32>(pSwizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);
                    ones[rgbaIdx] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
                }
            }
            break;
        default:
            break;
        }

        *pNeedFastClearElim = false;

        static_assert(sizeof(uint8) == sizeof(clearCode),
                      "Bad cast from clearCode to uint8. Size mismatch");
        // The HW specically optimizes clears to black and white and assigns special clear codes to these. Check
        // for that condition here.
        GetBlackOrWhiteClearCode(pParent,
                                 color,
                                 ones,
                                 reinterpret_cast<uint8*>(&clearCode));

        if (clearCode == Gfx9DccClearColor::ClearColorInvalid)
        {
            // Ok, it didn't find "black or white", so we'll need to do more.
            *pNeedFastClearElim = true;
        }
        else
        {
            blackOrWhite = true;
        }
    }
    else
    {
        // Even though it won't be texture feched, it is still safer to unconditionally do FCE to guarantee the base
        // data is coherent with prior clears
        *pNeedFastClearElim = true;
    }

    // If this is *not* one of the hardcoded clear colors then we need to decide between comp-to-reg and
    // comp-to-single modes.
    if (blackOrWhite == false)
    {
        // If we support comp-to-single for this image then switch the clear color to comp-to-single.  i.e., prefer
        // comp-to-single over comp-to-reg as the former doesn't require fast-clear-eliminate operations.
        if (image.Gfx10UseCompToSingleFastClears())
        {
            // If we're going to write the clear color into the image data during the fast-clear, then we need to
            // use a clear-code that indicates that's what we've done...  Otherwise the DCC block gets seriously
            // confused especially if the next rendering operation happens to render constant pixel data that
            // perfectly matches the clear color stored in the CB_COLORx_CLEAR_WORD* registers.
            if (IsGfx10(*pDevice))
            {
                clearCode = Gfx9DccClearColor::Gfx10ClearColorCompToSingle;
            }
            else
            {
                clearCode = Gfx9DccClearColor::Gfx11ClearColorCompToSingle;
            }
        }
        else
        {
            // Ok, we don't support comp-to-single, this isn't one of the magic black / white colors,
            // so we have to use comp-to-reg.
            clearCode = Gfx9DccClearColor::ClearColorCompToReg;

            // Navi3x has deprecated comp-to-reg support
            // one normal scenario to be here:
            // 128bpp dcc fastclear needs to know the clear value in advance, see func call
            // CmdClearColorImage->IsAc01ColorClearCode->GetFastClearCode
            // 128bpp comp-to-single is not supported.If clear value is not AC01, we are here
            if (IsGfx11(*pDevice))
            {
                clearCode = Gfx9DccClearColor::ClearColorInvalid;
            }
        }
    }

    // If the client asked, then let them know that this is a hardcoded clear color.
    if (pBlackOrWhite != nullptr)
    {
        *pBlackOrWhite = blackOrWhite;
    }

    return static_cast<uint8>(clearCode);
}

// =====================================================================================================================
void Gfx9Dcc::GetBlackOrWhiteClearCode(
    const Pal::Image*  pImage,          // Image being cleared
    const uint32       color[],         // The clear color
    const uint32       ones[],          // Sentinel values for "1", typically in the image format
    uint8*             pClearCode)      // [in, out] the clear code corresponding to the supplied clear color if it's
                                        //           black or white...  Othewrise unchanged from input.
{
    if (IsGfx11(*pImage->GetDevice()))
    {
        const ChNumFormat createFormat = pImage->GetImageCreateInfo().swizzledFormat.format;

        // GFX11 changes the fast clear codes.  :-(
        if ((color[0] == 0) &&
            (color[1] == 0) &&
            (color[2] == 0) &&
            (color[3] == 0))
        {
            // Always works for all formats.
            *pClearCode = 0x00;
        }
        else if (pImage->GetDccFormatEncoding() != DccFormatEncoding::SignIndependent)
        {
            if ((color[0] == ones[0]) &&
                (color[1] == ones[1]) &&
                (color[2] == ones[2]) &&
                (color[3] == ones[3]))
            {
                // Even though this is "white", there are formats (specifically sint, snorm and FP11) that don't
                // have clear codes associated with "white".  This should fall back to to the caller and
                // wind up in "comp to single" mode instead.
                if (Formats::IsUint(createFormat)  ||
                    Formats::IsUnorm(createFormat) ||
                    Formats::IsSrgb(createFormat))
                {
                    *pClearCode = 0x02;
                }
                else if (Formats::IsFloat(createFormat))
                {
                    switch (createFormat)
                    {
                    case ChNumFormat::X16_Float:
                    case ChNumFormat::X16Y16_Float:
                    case ChNumFormat::X16Y16Z16W16_Float:
                        *pClearCode = 0x04;
                        break;
                    case ChNumFormat::X32_Float:
                    case ChNumFormat::X32Y32_Float:
                        *pClearCode = 0x06;
                        break;
                    default:
                        break;
                    }
                }
            }
            // the 0-0-0-1 and 1-1-1-0 formats only work with UINT/UNORM/SRGB/USCALED formats..
            else if (Formats::IsUint(createFormat)    ||
                     Formats::IsUnorm(createFormat)   ||
                     Formats::IsUscaled(createFormat) ||
                     Formats::IsSrgb(createFormat))
            {
                // And then the AC01/10 clear codes only work with specific bpp arrangements with these formats.
                const bool is88       = Formats::ShareChFmt(createFormat, ChNumFormat::X8Y8_Unorm);
                const bool is8888     = Formats::ShareChFmt(createFormat, ChNumFormat::X8Y8Z8W8_Unorm);
                const bool is16161616 = Formats::ShareChFmt(createFormat, ChNumFormat::X16Y16Z16W16_Unorm);

                if (is88 || is8888 || is16161616)
                {
                    if ((color[0] == 0) &&
                        (color[1] == 0) &&
                        (color[2] == 0) &&
                        (color[3] == ones[3]))
                    {
                        *pClearCode = 0x08;
                    }
                    else if ((color[0] == ones[0]) &&
                             (color[1] == ones[1]) &&
                             (color[2] == ones[2]) &&
                             (color[3] == 0))
                    {
                        *pClearCode = 0x0A;
                    }
                }
            }
        }
    }
    else
    {
        constexpr uint8 ClearColor0000 = 0x00;
        constexpr uint8 ClearColor0001 = 0x40;
        constexpr uint8 ClearColor1110 = 0x80;
        constexpr uint8 ClearColor1111 = 0xC0;

        if ((color[0] == 0) &&
            (color[1] == 0) &&
            (color[2] == 0) &&
            (color[3] == 0))
        {
            *pClearCode = ClearColor0000;
        }
        else if (pImage->GetDccFormatEncoding() != DccFormatEncoding::SignIndependent)
        {
            // cant allow special clear color code because the formats do not support DCC Constant
            // encoding. This happens when we mix signed and unsigned formats. There is no problem with
            // clearcolor0000.The issue is only seen when there is a 1 in any of the channels
            if ((color[0] == 0) &&
                (color[1] == 0) &&
                (color[2] == 0) &&
                (color[3] == ones[3]))
            {
                *pClearCode = ClearColor0001;
            }
            else if ((color[0] == ones[0]) &&
                     (color[1] == ones[1]) &&
                     (color[2] == ones[2]) &&
                     (color[3] == 0))
            {
                *pClearCode = ClearColor1110;
            }
            else if ((color[0] == ones[0]) &&
                     (color[1] == ones[1]) &&
                     (color[2] == ones[2]) &&
                     (color[3] == ones[3]))
            {
                *pClearCode = ClearColor1111;
            }
        }
    }
}

// =====================================================================================================================
void Gfx9Dcc::GetState(
    DccState* pState
    ) const
{
    pState->maxCompressedBlockSize   = m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE;
    pState->maxUncompressedBlockSize = m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;
    pState->independentBlk64B        = m_dccControl.bits.INDEPENDENT_64B_BLOCKS;

    if (IsGfx11(*(m_pGfxDevice->Parent())))
    {
        pState->independentBlk128B   = m_dccControl.gfx11.INDEPENDENT_128B_BLOCKS;
    }
    else
    {
        pState->independentBlk128B   = m_dccControl.gfx10.INDEPENDENT_128B_BLOCKS;
    }

    pState->primaryOffset            = m_offset;
    pState->secondaryOffset          = 0;
    pState->size                     = m_addrOutput.dccRamSize;
    pState->pitch                    = m_addrOutput.pitch;
}

//=============== Implementation for Gfx9Cmask: ========================================================================
Gfx9Cmask::Gfx9Cmask(
    const Image&  image,
    void*         pPlacementAddr)
    :
    Gfx9MaskRam(image,
                pPlacementAddr,
                -1, // cMask uses nibble quantities
                0)  // no bits can be ignored
{
    memset(&m_addrOutput, 0, sizeof(m_addrOutput));
    m_addrOutput.size = sizeof(m_addrOutput);
}

// =====================================================================================================================
void Gfx9Cmask::CalcCompBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    // For non-color surfaces, compessed block size is always 8x8.
    //
    // Note, cMask is only for MSAA surfaces, and we can't have 3D MSAA surfaces, so the "depth" component is
    //       always zero.
    //
    // "non-color" refers to the color/dcc surface pair only.
    // cMask is considered the meta surface for fMask, so it's "non-color".
    pBlockSize->width  = 3;
    pBlockSize->height = 3;
    pBlockSize->depth  = 0;
}

// =====================================================================================================================
void Gfx9Cmask::CalcMetaBlkSizeLog2(
    Gfx9MaskRamBlockSize*  pBlockSize
    ) const
{
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkWidth));
    PAL_ASSERT(IsPowerOfTwo(m_addrOutput.metaBlkHeight));

    pBlockSize->width  = Log2(m_addrOutput.metaBlkWidth);
    pBlockSize->height = Log2(m_addrOutput.metaBlkHeight);
    pBlockSize->depth  = 0;  // No 3D msaa, depth is zero; log2(1) == 0)
}

// =====================================================================================================================
Result Gfx9Cmask::ComputeCmaskInfo()
{
    const Pal::Image*const  pParent    = m_image.Parent();
    const Pal::Device*const pDevice    = pParent->GetDevice();
    const ImageCreateInfo&  createInfo = pParent->GetImageCreateInfo();
    const auto*const        pAddrMgr   = static_cast<const AddrMgr2::AddrMgr2*>(pDevice->GetAddrMgr());

    // Only need the sub-res info for the plane...
    const SubResourceInfo*const pSubResInfo = pParent->SubresourceInfo(0);

    const auto*                     pFmask     = m_image.GetFmask();
    ADDR2_COMPUTE_CMASK_INFO_INPUT  cMaskInput = {};
    Result                          result     = Result::ErrorInitializationFailed;

    cMaskInput.size            = sizeof(cMaskInput);
    cMaskInput.unalignedWidth  = createInfo.extent.width;
    cMaskInput.unalignedHeight = createInfo.extent.height;
    cMaskInput.numSlices       = createInfo.arraySize;
    cMaskInput.resourceType    = m_image.GetAddrSettings(pSubResInfo).resourceType;
    cMaskInput.colorFlags      = pAddrMgr->DetermineSurfaceFlags(*pParent, pSubResInfo->subresId.plane, false);
    cMaskInput.swizzleMode     = pFmask->GetSwizzleMode();
    cMaskInput.cMaskFlags      = GetMetaFlags();

    const ADDR_E_RETURNCODE  addrRet = Addr2ComputeCmaskInfo(pDevice->AddrLibHandle(),
                                                             &cMaskInput,
                                                             &m_addrOutput);

    if (addrRet == ADDR_OK)
    {
        result = Result::Success;

        m_alignment = m_addrOutput.baseAlign;
        m_totalSize = m_addrOutput.cmaskBytes;
    }

    return result;
}

// =====================================================================================================================
// The bytes-per-pixel of the cMask surface is the bpp of the associated fMask surface
uint32 Gfx9Cmask::GetBytesPerPixelLog2() const
{
    return Log2(m_image.GetFmask()->GetAddrOutput().bpp / 8);
}

// =====================================================================================================================
// Gets the pipe-bank xor value for the data surface associated with this meta surface.  For a cMask meta surface, the
// associated data surface is fMask.
uint32 Gfx9Cmask::GetPipeBankXor(
    uint32 plane
    ) const
{
    PAL_ASSERT(plane == 0);

    const uint32 pipeBankXor = m_image.GetFmask()->GetPipeBankXor();

    return AdjustPipeBankXorForSwizzle(pipeBankXor);
}

// =====================================================================================================================
// Here we want to give a value to correctly indicate that CMask is in expanded state, According to cb.doc, the Cmask
// Encoding for AA without fast clear is bits 3:2(2'b11) and bits 1:0(compression mode).
uint8 Gfx9Cmask::GetInitialValue() const
{
    const auto& imgCreateInfo = m_image.Parent()->GetImageCreateInfo();
    // We need enough bits to fit all fragments, plus an extra bit for EQAA support.
    const bool   isEqaa       = (imgCreateInfo.fragments != imgCreateInfo.samples);
    const uint32 numBits      = Log2(imgCreateInfo.fragments) + isEqaa;
    uint8        cmaskValue   = 0xFF;

    switch (numBits)
    {
    case 1:
        cmaskValue = 0xDD;     // bits 3:2(2'b11)   bits 1:0(2'b01)
        break;
    case 2:
        cmaskValue = 0xEE;     // bits 3:2(2'b11)   bits 1:0(2'b10)
        break;
    case 3:
    case 4:                    // 8f16s EQAA also has a 0xFF clear value
        cmaskValue = 0xFF;     // bits 3:2(2'b11)   bits 1:0(2'b11)
        break;
    default:
        // Note: 1 fragment/1 sample is invalid
        PAL_ASSERT_ALWAYS();
        break;
    };

    return cmaskValue;
}

// =====================================================================================================================
// Returns the swizzle mode of the associated fmask surface
AddrSwizzleMode Gfx9Cmask::GetSwizzleMode() const
{
    return m_image.GetFmask()->GetSwizzleMode();
}

// =====================================================================================================================
Result Gfx9Cmask::Init(
    gpusize*  pGpuOffset,
    bool      hasEqGpuAccess)
{
    Result result = ComputeCmaskInfo();

    if (result == Result::Success)
    {
        // Compute our aligned GPU memory offset and update the caller-provided running total.  Don't update the
        // overall image size with every mip level as the entire size of cMask is computed all at once.
        UpdateGpuMemOffset(pGpuOffset);

        // GFX10 has no use for the cmask addressing equation, so save the CPU cycles needed to generate it.
        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the cMask equation
            PAL_ASSERT(HasMetaEqGenerator());
            m_pEqGenerator->InitEqGpuAccess(pGpuOffset);
        }
    }

    return result;
}

// =====================================================================================================================
// Determines if the given Image object should use CMask metadata.
bool Gfx9Cmask::UseCmaskForImage(
    const Pal::Device& device,
    const Image&       image)
{
    const Pal::Image*const pParent = image.Parent();

    bool useCmask = false;

    if (pParent->GetInternalCreateInfo().flags.useSharedMetadata)
    {
        useCmask = (pParent->GetInternalCreateInfo().sharedMetadata.cmaskOffset != 0);
    }
    else if (IsGfx10(device))
    {
        // Forcing CMask usage forces FMask usage, which is required for EQAA.
        useCmask = (pParent->IsEqaa() ||
                    (pParent->IsRenderTarget()                       &&
                    (pParent->IsShared()                   == false) &&
                    (pParent->IsMetadataDisabledByClient() == false) &&
                    (pParent->GetImageCreateInfo().samples > 1)));
    }
    else
    {
        // GFX11 products have no concept of cMask or fMask
    }

    return useCmask;
}

//=============== Implementation for Gfx9Fmask: ========================================================================

// =====================================================================================================================
Gfx9Fmask::Gfx9Fmask()
    :
    MaskRam(),
    m_pipeBankXor(0)
{
    memset(&m_surfSettings, 0, sizeof(m_surfSettings));
    memset(&m_addrOutput,    0, sizeof(m_addrOutput));

    m_addrOutput.size   = sizeof(m_addrOutput);
    m_surfSettings.size = sizeof(m_surfSettings);
}

// =====================================================================================================================
// Determines the Image format used by SRD's which access an image's fMask allocation.
IMG_FMT Gfx9Fmask::Gfx10FmaskFormat(
    uint32  samples,
    uint32  fragments,
    bool    isUav
    ) const
{
    IMG_FMT  imgFmt = IMG_FMT_INVALID;

    if (isUav)
    {
        switch (m_addrOutput.bpp)
        {
        case 8:
            imgFmt = IMG_FMT_8_UINT;
            break;
        case 16:
            imgFmt = IMG_FMT_16_UINT;
            break;
        case 32:
            imgFmt = IMG_FMT_32_UINT;
            break;
        case 64:
            imgFmt = IMG_FMT_32_32_UINT__GFX10;
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }
    else
    {
        // Lookup table of FMask Image Data Formats:
        // The table is indexed by: [log_2(samples) - 1][log_2(fragments)].
        constexpr IMG_FMT FMaskFormatTbl[4][4] =
        {
            // Two-sample formats
            { IMG_FMT_FMASK8_S2_F1__GFX10,         // One fragment
              IMG_FMT_FMASK8_S2_F2__GFX10, },      // Two fragments

            // Four-sample formats
            { IMG_FMT_FMASK8_S4_F1__GFX10,         // One fragment
              IMG_FMT_FMASK8_S4_F2__GFX10,         // Two fragments
              IMG_FMT_FMASK8_S4_F4__GFX10, },      // Four fragments

            // Eight-sample formats
            { IMG_FMT_FMASK8_S8_F1__GFX10,         // One fragment
              IMG_FMT_FMASK16_S8_F2__GFX10,        // Two fragments
              IMG_FMT_FMASK32_S8_F4__GFX10,        // Four fragments
              IMG_FMT_FMASK32_S8_F8__GFX10, },     // Eight fragments

            // Sixteen-sample formats
            { IMG_FMT_FMASK16_S16_F1__GFX10,       // One fragment
              IMG_FMT_FMASK32_S16_F2__GFX10,       // Two fragments
              IMG_FMT_FMASK64_S16_F4__GFX10,       // Four fragments
              IMG_FMT_FMASK64_S16_F8__GFX10, },    // Eight fragments
        };

        const uint32 log2Samples   = Log2(samples);
        const uint32 log2Fragments = Log2(fragments);

        PAL_ASSERT((log2Samples  >= 1) && (log2Samples <= 4));
        PAL_ASSERT(log2Fragments <= 3);

        imgFmt = FMaskFormatTbl[log2Samples - 1][log2Fragments];
    }

    return imgFmt;
}

// =====================================================================================================================
Result Gfx9Fmask::Init(
    const Image& image,
    gpusize*     pGpuOffset)
{
    Result result = ComputeFmaskInfo(image);

    if (result == Result::Success)
    {
        // fMask surfaces have a pipe/bank xor value which is independent of the image's pipe-bank xor value.
        result = image.ComputePipeBankXor(0, true, &m_surfSettings, &m_pipeBankXor);
    }

    if (result == Result::Success)
    {
        // Compute our aligned GPU memory offset and update the caller-provided running total.  Don't update the
        // overall image size with every mip level as the entire size of fMask is computed all at once.
        UpdateGpuMemOffset(pGpuOffset);

        //      Fmask buffer is considered a data surface, not a metadata surface for the purposes of addressing,
        //      so it should just use the standard data addressing.
    }

    return result;
}

// =====================================================================================================================
// Determines the 32-bit value that the fmask memory associated with the provided image should be initialized to
uint32 Gfx9Fmask::GetPackedExpandedValue(
    const Image& image)
{
    // Packed version of fully expanded FMASK value. This should be used by ClearFmask.
    constexpr uint64 PackedFmaskExpandedValues[MaxLog2AaFragments + 1][MaxLog2AaSamples + 1] =
    {
        // Fragment counts down the right, sample counts along the top. Note: 1 fragment/1 sample is invalid.
        // 1   2                   4                   8                   16
        { 0x0, 0x0202020202020202, 0x0E0E0E0E0E0E0E0E, 0xFEFEFEFEFEFEFEFE, 0xFFFEFFFEFFFEFFFE }, // 1
        { 0x0, 0x0202020202020202, 0xA4A4A4A4A4A4A4A4, 0xAAA4AAA4AAA4AAA4, 0xAAAAAAA4AAAAAAA4 }, // 2
        { 0x0, 0x0               , 0xE4E4E4E4E4E4E4E4, 0x4444321044443210, 0x4444444444443210 }, // 4
        { 0x0, 0x0               , 0x0,                0x7654321076543210, 0x8888888876543210 }  // 8
    };

    const uint32 log2Fragments = Log2(image.Parent()->GetImageCreateInfo().fragments);
    const uint32 log2Samples = Log2(image.Parent()->GetImageCreateInfo().samples);

    // 4/8 fragments + 16 samples has double DWORD memory pattern and can't represent by a single uint32.
    PAL_ASSERT((log2Samples < 4) || (log2Fragments < 2));

    return LowPart(PackedFmaskExpandedValues[log2Fragments][log2Samples]);
}

// =====================================================================================================================
Result Gfx9Fmask::ComputeFmaskInfo(
    const Image& image)
{
    const Pal::Image&       parent   = *(image.Parent());
    const Pal::Device*const pDevice  = parent.GetDevice();
    const auto*const        pAddrMgr = static_cast<const AddrMgr2::AddrMgr2*>(pDevice->GetAddrMgr());

    Result result = pAddrMgr->ComputeFmaskSwizzleMode(parent, &m_surfSettings);
    if (result == Result::Success)
    {
        const auto& createInfo = parent.GetImageCreateInfo();

        ADDR2_COMPUTE_FMASK_INFO_INPUT  fMaskInput = {};

        fMaskInput.size                = sizeof(fMaskInput);
        fMaskInput.unalignedWidth      = createInfo.extent.width;
        fMaskInput.unalignedHeight     = createInfo.extent.height;
        fMaskInput.numSlices           = createInfo.arraySize;
        fMaskInput.numSamples          = createInfo.samples;
        fMaskInput.numFrags            = createInfo.fragments;
        fMaskInput.fMaskFlags.resolved = 0; // because the addrinterface.h header says so
        fMaskInput.swizzleMode         = m_surfSettings.swizzleMode;

        const ADDR_E_RETURNCODE  addrRet = Addr2ComputeFmaskInfo(pDevice->AddrLibHandle(),
                                                                 &fMaskInput,
                                                                 &m_addrOutput);

        if (addrRet == ADDR_OK)
        {
            m_alignment = m_addrOutput.baseAlign;
            m_totalSize = m_addrOutput.fmaskBytes;
        }
        else
        {
            result = Result::ErrorUnknown;
        }
    }

    return result;
}

// =====================================================================================================================
bool Gfx9MetaEqGenerator::IsZSwizzle(
    AddrSwizzleMode  swizzleMode
    ) const
{
    bool isZ = AddrMgr2::IsZSwizzle(swizzleMode);

    if (IsGfx11(*m_pParent->GetGfxDevice()->Parent()))
    {
        //    The data organization (addressing equations) for SW_*_Z_X and SW_*_R_X are now identical
        isZ |= AddrMgr2::IsRotatedSwizzle(swizzleMode);
    }

    return isZ;
}

// =====================================================================================================================
bool Gfx9MetaEqGenerator::IsRotatedSwizzle(
    AddrSwizzleMode  swizzleMode
    ) const
{
    bool isR = AddrMgr2::IsRotatedSwizzle(swizzleMode);

    if (IsGfx11(*m_pParent->GetGfxDevice()->Parent()))
    {
        //    The data organization (addressing equations) for SW_*_Z_X and SW_*_R_X are now identical
        //
        // However, due to how the HW addrlib was implemented, "R" modes are changed to "Z", so any check for "SW_R"
        // is now *false*.
        isR = false;
    }

    return isR;
}

} // Gfx9
} // Pal
