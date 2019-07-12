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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9FormatInfo.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/addrMgr/addrMgr2/addrMgr2.h"
#include "palMath.h"
#include "palAutoBuffer.h"

#include <limits.h>

using namespace Util;
using namespace Pal::Formats;
using namespace Pal::Formats::Gfx9;
using namespace Pal::AddrMgr2;

namespace Pal  {
namespace Gfx9 {

static ADDR2_META_FLAGS GetMetaFlags(const Image&  image);

//=============== Implementation for Gfx9MaskRam: ======================================================================

// =====================================================================================================================
// Constructor for the Gfx9MaskRam class
Gfx9MaskRam::Gfx9MaskRam(
    const Image&  image,
    int32         metaDataSizeLog2,
    uint32        firstUploadBit)
    :
    MaskRam(),
    m_image(image),
    m_pGfxDevice(static_cast<const Device*>(image.Parent()->GetDevice()->GetGfxDevice())),
    m_meta(m_pGfxDevice, 27, "meta"),
    m_pipe(m_pGfxDevice, 27, "pipe"),
    m_metaDataWordSizeLog2(metaDataSizeLog2),
    m_dataOffset(m_pGfxDevice, 27, "dataOffset"),
    // The RB equation can't have more bits than we have RBs
    m_rb(m_pGfxDevice, m_pGfxDevice->GetNumShaderEnginesLog2() + m_pGfxDevice->GetNumRbsPerSeLog2(), "rb"),
    m_firstUploadBit(firstUploadBit),
    m_metaEquationValid(false),
    m_effectiveSamples(1), // assume single-sampled image
    m_rbAppendedWithPipeBits(0)
{
    memset(&m_eqGpuAccess,   0, sizeof(m_eqGpuAccess));
    memset(&m_addrMipOutput, 0, sizeof(m_addrMipOutput));
    memset(&m_metaEqParam,   0, sizeof(m_metaEqParam));

    static_assert(MetaDataAddrEquation::MaxNumMetaDataAddrBits  <= (sizeof(m_rbAppendedWithPipeBits) * 8),
                  "Must increase size of m_rbAppendedWithPipeBits storage!");
}

// =====================================================================================================================
// Builds a buffer view for accessing the meta equation from the GPU
void Gfx9MaskRam::BuildEqBufferView(
    BufferViewInfo*  pBufferView
    ) const
{
    PAL_ASSERT (m_eqGpuAccess.size != 0);

    pBufferView->swizzledFormat = UndefinedSwizzledFormat;
    pBufferView->stride         = MetaDataAddrCompNumTypes * sizeof(uint32);
    pBufferView->range          = (m_meta.GetNumValidBits() - m_firstUploadBit) *
                                  MetaDataAddrCompNumTypes                      *
                                  sizeof (uint32);
    pBufferView->gpuAddr        = m_image.Parent()->GetGpuVirtualAddr() + m_eqGpuAccess.offset;
}

// =====================================================================================================================
// Populates a buffer view info object which wraps the mask-ram sub-allocation
void Gfx9MaskRam::BuildSurfBufferView(
    BufferViewInfo*  pViewInfo    // [out] The buffer view
    ) const
{
    pViewInfo->gpuAddr        = m_image.Parent()->GetBoundGpuMemory().GpuVirtAddr() + MemoryOffset();
    pViewInfo->range          = TotalSize();
    pViewInfo->stride         = 1;
    pViewInfo->swizzledFormat = UndefinedSwizzledFormat;
}

// =====================================================================================================================
// Calculates the data offset equation for this mask-ram.
void Gfx9MaskRam::CalcDataOffsetEquation()
{
    const auto*            pParent        = m_image.Parent();
    const AddrSwizzleMode  swizzleMode    = GetSwizzleMode();
    const uint32           blockSizeLog2  = Log2(AddrMgr2::GetBlockSize(swizzleMode));
    const uint32           bppLog2        = GetBytesPerPixelLog2();
    const uint32           numSamplesLog2 = GetNumSamplesLog2();

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
                m_dataOffset.SetBit(bitPos, MetaDataAddrCompX, cx.compPos);
                cx.compPos++;
            }

            // Fill in 2 bits of y and then z
            for (uint32 bitPos = 4; bitPos < 6; bitPos++)
            {
                m_dataOffset.SetBit(bitPos, MetaDataAddrCompY, cy.compPos);
                cy.compPos++;
            }

            for (uint32 bitPos = 6; bitPos < 8; bitPos++)
            {
                m_dataOffset.SetBit(bitPos, MetaDataAddrCompZ, cz.compPos);
                cz.compPos++;
            }

            if (bppLog2 < 2)
            {
                // fill in z & y bit
                m_dataOffset.SetBit(8, cz.compType, cz.compPos++);
                m_dataOffset.SetBit(9, cy.compType, cy.compPos++);
            }
            else if (bppLog2 == 2)
            {
                // fill in y and x bit
                m_dataOffset.SetBit(8, cy.compType, cy.compPos++);
                m_dataOffset.SetBit(9, cx.compType, cx.compPos++);
            } else
            {
                // fill in 2 x bits
                m_dataOffset.SetBit(8, cx.compType, cx.compPos++);
                m_dataOffset.SetBit(9, cx.compType, cx.compPos++);
            }
        }
        else
        {
            // Z 3d swizzle
            const uint32 m2dEnd = (bppLog2 == 0) ? 3 : ((bppLog2 < 4) ? 4 : 5);
            const uint32 numZs  = ((bppLog2 == 0) || (bppLog2 == 4)) ? 2 : ((bppLog2 == 1) ? 3 : 1);

            m_dataOffset.Mort2d(&cx, &cy, bppLog2, m2dEnd);
            for(uint32 bitPos = m2dEnd + 1; bitPos <= m2dEnd + numZs; bitPos++ )
            {
                m_dataOffset.SetBit(bitPos, cz.compType, cz.compPos++);
            }

            if ((bppLog2 == 0) || (bppLog2 == 3))
            {
                // add an x and z
                m_dataOffset.SetBit(6, cx.compType, cx.compPos++);
                m_dataOffset.SetBit(7, cz.compType, cz.compPos++);
            }
            else if (bppLog2 == 2)
            {
                // add a y and z
                m_dataOffset.SetBit(6, cy.compType, cy.compPos++);
                m_dataOffset.SetBit(7, cz.compType, cz.compPos++);
            }

            // add y and x
            m_dataOffset.SetBit(8, cy.compType, cy.compPos++);
            m_dataOffset.SetBit(9, cx.compType, cx.compPos++);
        }

        // Fill in bit 10 and up
        m_dataOffset.Mort3d(&cz, &cy, &cx, 10);
    }
    else if (IsColor())
    {
        // Color 2D
        const uint32  microYBits     = (8 - bppLog2) / 2;
        const uint32  tileSplitStart = blockSizeLog2 - numSamplesLog2;

        // Fill in bottom x bits
        for (uint32 i = bppLog2; i < 4; i++)
        {
            m_dataOffset.SetBit(i, MetaDataAddrCompX, cx.compPos);
            cx.compPos++;
        }

        // Fill in bottom y bits
        for (uint32 i = 4; i < 4 + microYBits; i++)
        {
            m_dataOffset.SetBit(i, MetaDataAddrCompY, cy.compPos);
            cy.compPos++;
        }

        // Fill in last of the micro_x bits
        for(uint32 i = 4 + microYBits; i < 8; i++)
        {
            m_dataOffset.SetBit(i, MetaDataAddrCompX, cx.compPos);
            cx.compPos++;
        }

        // Fill in x/y bits below sample split
        m_dataOffset.Mort2d(&cy, &cx, 8, tileSplitStart - 1);

        // Fill in sample bits
        for (uint32 bitPos = 0; bitPos < numSamplesLog2; bitPos++)
        {
            m_dataOffset.SetBit(tileSplitStart + bitPos, MetaDataAddrCompS, bitPos);
        }

        // Fill in x/y bits above sample split
        if (((numSamplesLog2 & 1) ^ (blockSizeLog2 & 1)) != 0)
        {
            m_dataOffset.Mort2d(&cx, &cy, blockSizeLog2);
        }
        else
        {
            m_dataOffset.Mort2d(&cy, &cx, blockSizeLog2);
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
            m_dataOffset.SetBit(bppLog2 + s, MetaDataAddrCompS, s);
        }

        // Put in the x-major order pixel bits
        m_dataOffset.Mort2d (&cx, &cy, pixelStart, yMajStart - 1);

        // Put in the y-major order pixel bits
        m_dataOffset.Mort2d (&cy, &cx, yMajStart);
    }

    m_dataOffset.PrintEquation(pParent->GetDevice());
}

// =====================================================================================================================
// Calculates the pipe equation for this mask-ram.
void Gfx9MaskRam::CalcPipeEquation(
    uint32  numPipesLog2)
{
    const int32            numSamplesLog2     = GetNumSamplesLog2();
    const AddrSwizzleMode  swizzleMode        = GetSwizzleMode();
    const uint32           blockSizeLog2      = Log2(AddrMgr2::GetBlockSize(swizzleMode));
    const uint32           pipeInterleaveLog2 = m_pGfxDevice->GetPipeInterleaveLog2();

    CompPair  tileMin = { MetaDataAddrCompX, 3};
    MetaDataAddrEquation  dataOffsetLocal(m_pGfxDevice, m_dataOffset.GetNumValidBits(), "dataOffsetLocal");

    // For color, filter out sample bits only
    // otherwise filter out everything under an 8x8 tile
    if (IsColor())
    {
        tileMin.compPos = 0;
    }

    m_dataOffset.Copy(&dataOffsetLocal);
    // Z/stencil is no longer tile split
    if (IsColor() && (numSamplesLog2 > 0))
    {
        dataOffsetLocal.Shift(-numSamplesLog2, blockSizeLog2 - numSamplesLog2);
    }

    dataOffsetLocal.Copy(&m_pipe, pipeInterleaveLog2, numPipesLog2);

    // If the pipe bit is below the comp block size, then keep moving up the address until we find a bit that is above
    uint32  pipe = 0;
    while (true)
    {
        const CompPair localPair = dataOffsetLocal.Get(pipeInterleaveLog2 + pipe);
        if (MetaDataAddrEquation::CompareCompPair(localPair, tileMin, MetaDataAddrCompareLt))
        {
            pipe++;
        }
        else
        {
            break;
        }
    }

    // if pipe is 0, then the first pipe bit is above the comp block size, so we don't need to do anything
    if (pipe != 0)
    {
        uint32 j = pipe;
        for(uint32 i = 0; i < numPipesLog2; i++)
        {
            // Copy the jth bit above pipe interleave to the current pipe equation bit
            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                m_pipe.ClearBits(i, compType, 0);
                m_pipe.SetMask(i, compType, dataOffsetLocal.Get(pipeInterleaveLog2 + j, compType));
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
        MetaDataAddrEquation  xorMask(m_pGfxDevice, numPipesLog2, "xorMask");
        MetaDataAddrEquation  xorMask2(m_pGfxDevice, numPipesLog2, "xorMask2");

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
            dataOffsetLocal.Copy(&xorMask, pipeInterleaveLog2 + pipe + numPipesLog2, numPipesLog2);
            if ((numSamplesLog2 == 0) && (AddrMgr2::IsPrtSwizzle(swizzleMode) == false))
            {
                // if 1xaa and not prt, then xor in the z bits
                for (uint32 localPipe = 0; localPipe < numPipesLog2; localPipe++)
                {
                    xorMask2.SetBit(localPipe, MetaDataAddrCompZ, numPipesLog2 - 1 - localPipe);
                }

                m_pipe.XorIn(&xorMask2);
            }
        }

        xorMask.Reverse();

        m_pipe.XorIn(&xorMask);
    }

    m_pipe.PrintEquation(m_pGfxDevice->Parent());
}

// =====================================================================================================================
// Calculate the pipe-bank XOR value as used by the meta-data equation.
uint32 Gfx9MaskRam::CalcPipeXorMask(
    ImageAspect   aspect
    ) const
{
    const uint32  pipeInterleaveLog2 = m_pGfxDevice->GetPipeInterleaveLog2();
    const uint32  numPipesLog2       = CapPipe();
    const uint32  pipeBankXor        = GetPipeBankXor(aspect);

    const uint32 pipeXorMaskNibble = (pipeBankXor & ((1 << numPipesLog2) - 1)) << (pipeInterleaveLog2 + 1);

    // Make sure all the bits that we expect to be able to ignore are zero!
    PAL_ASSERT ((pipeXorMaskNibble & ((1 << m_firstUploadBit) - 1)) == 0);

    // Ensure we either have a zero pipe-bank-xor value or we have a swizzle mode that supports non-zero XOR values.
    PAL_ASSERT((pipeXorMaskNibble == 0) || AddrMgr2::IsXorSwizzle(GetSwizzleMode()));

    // Our shaders always (eventually) compute byte addresses, so return this in terms of bytes for easy use by the CS
    return (pipeXorMaskNibble >> 1);
}

// =====================================================================================================================
uint32 Gfx9MaskRam::GetRbAppendedBit(
    uint32  bitPos
    ) const
{
    return ((m_rbAppendedWithPipeBits & (1u << bitPos)) >> bitPos);
}

// =====================================================================================================================
void Gfx9MaskRam::SetRbAppendedBit(
    uint32  bitPos,
    uint32  bitVal)
{
    const Gfx9PalSettings&  settings = GetGfx9Settings(*(m_pGfxDevice->Parent()));

    // There's no need for this setting unless this workaround is enabled.  Other code depends on the
    // "m_rbAppendedWithPipeBits" setting remaining zero if this workaround is disabled.
    if (settings.waMetaAliasingFixEnabled)
    {
        m_rbAppendedWithPipeBits &= ~(1u << bitPos);
        m_rbAppendedWithPipeBits |= (bitVal << bitPos);
    }
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
void Gfx9MaskRam::CalcMetaEquation()
{
    const Pal::Image*  pParent = m_image.Parent();
    const Pal::Device* pDevice = m_pGfxDevice->Parent();

    // GFX9 is the only GPU that utilizes the meta-data addressing equation...
    if (pDevice->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
    {
        const ADDR2_META_FLAGS  metaFlags          = GetMetaFlags(m_image);
        const auto*             pCreateInfo        = &pParent->GetImageCreateInfo();
        const uint32            numSamplesLog2     = GetNumSamplesLog2();
        const uint32            maxFragsLog2       = m_pGfxDevice->GetMaxFragsLog2();
        const uint32            pipeInterleaveLog2 = m_pGfxDevice->GetPipeInterleaveLog2();
        const uint32            bppLog2            = GetBytesPerPixelLog2();
        const Gfx9PalSettings&  settings           = GetGfx9Settings(*pDevice);

        // Min metablock size if thick is 64KB, otherwise 4KB
        uint32 minMetaBlockSizeLog2     = (IsThick() ? 16 : 12);
        uint32 metaDataWordsPerPageLog2 = minMetaBlockSizeLog2 - m_metaDataWordSizeLog2;
        uint32 numSesLog2               = m_pGfxDevice->GetNumShaderEnginesLog2();
        uint32 numRbsLog2               = m_pGfxDevice->GetNumRbsPerSeLog2();

        // Get the total # of RB's before modifying due to rb align
        const uint32 numTotalRbsPreRbAlignLog2 = numSesLog2 + numRbsLog2;

        uint32  numPipesLog2   = CapPipe();
        uint32  numSesDataLog2 = numSesLog2;      // Cap the pipe bits to block size

        int32 compFragLog2 = (IsColor() && (numSamplesLog2 > maxFragsLog2))
                             ? maxFragsLog2
                             : numSamplesLog2;
        int32 uncompFragLog2 = numSamplesLog2 - compFragLog2;

        CalcDataOffsetEquation();

        // if not pipe aligned, reduce the working number of pipes and SEs
        if (metaFlags.pipeAligned == false)
        {
            numPipesLog2   = 0;
            numSesDataLog2 = 0;
        }

        // if not rb aligned, reduce the number of SEs and RBs to 0; note, this is done after generating the
        // data equation
        if (metaFlags.rbAligned == false)
        {
            numSesLog2 = 0;
            numRbsLog2 = 0;
        }

        CalcPipeEquation(numPipesLog2);
        CalcRbEquation(numSesLog2, numRbsLog2);

        uint32 numTotalRbsLog2 = numSesLog2 + numRbsLog2;

        int32                compBlkSizeLog2 = 8;
        Gfx9MaskRamBlockSize compBlkDimsLog2 = {};
        CalcCompBlkSizeLog2(&compBlkDimsLog2);
        if (IsColor())
        {
            metaDataWordsPerPageLog2 -= numSamplesLog2;  // factor out num fragments for color surfaces
        }
        else
        {
            compBlkSizeLog2 = 6 + numSamplesLog2 + bppLog2;
        }

        // Compute meta block width and height
        uint32 numCompBlksPerMetaBlk = metaDataWordsPerPageLog2;
        if ((numPipesLog2 != 0) ||
            (numSesLog2   != 0) ||
            (numRbsLog2   != 0))
        {
            const uint32  thinImageAdder = ((settings.waMetaAliasingFixEnabled  == false)
                                            ? 10
                                            : Max(10u, pipeInterleaveLog2));
            numCompBlksPerMetaBlk        = numTotalRbsPreRbAlignLog2 + (IsThick() ? 18 : thinImageAdder);

            if ((numCompBlksPerMetaBlk + compBlkSizeLog2) > (27 + bppLog2))
            {
                numCompBlksPerMetaBlk = 27 + bppLog2 - compBlkSizeLog2;
            }

            numCompBlksPerMetaBlk = Max(numCompBlksPerMetaBlk, metaDataWordsPerPageLog2);
        }

        Gfx9MaskRamBlockSize  metaBlockSizeLog2 = {};
        CalcMetaBlkSizeLog2(&metaBlockSizeLog2);

        // Use the growing square or growing cube order for thick as a starting point for the metadata address
        if (IsThick())
        {
            CompPair  cx = { MetaDataAddrCompX, 0};
            CompPair  cy = { MetaDataAddrCompY, 0};
            CompPair  cz = { MetaDataAddrCompZ, 0};

            if (pCreateInfo->mipLevels > 1)
            {
                m_meta.Mort3d(&cy, &cx, &cz);
            }
            else
            {
                m_meta.Mort3d(&cx, &cy, &cz);
            }
        }
        else
        {
            CompPair  cx = { MetaDataAddrCompX, 0};
            CompPair  cy = { MetaDataAddrCompY, 0};

            if (pCreateInfo->mipLevels > 1)
            {
                m_meta.Mort2d(&cy, &cx, compFragLog2);
            }
            else
            {
                m_meta.Mort2d(&cx, &cy, compFragLog2);
            }

            // Put the compressible fragments at the lsb
            // the uncompressible frags will be at the msb of the micro address
            for(int32 s = 0; s < compFragLog2; s++)
            {
                m_meta.SetBit(s, MetaDataAddrCompS, s);
            }
        }

        // Keep a copy of the pipe and rb equations
        MetaDataAddrEquation  origRbEquation(m_pGfxDevice, m_rb.GetNumValidBits(), "origRbEquation");
        m_rb.Copy(&origRbEquation);
        MetaDataAddrEquation  origPipeEquation(m_pGfxDevice, m_pipe.GetNumValidBits(), "origPipeEquation");
        m_pipe.Copy(&origPipeEquation);

        // filter out everything under the compressed block size
        CompPair  cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, compBlkDimsLog2.width);
        m_meta.Filter(cx, MetaDataAddrCompareLt, 0, cx.compType);

        CompPair  cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, compBlkDimsLog2.height);
        m_meta.Filter(cy, MetaDataAddrCompareLt, 0, cy.compType);

        CompPair  cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, compBlkDimsLog2.depth);
        m_meta.Filter(cz, MetaDataAddrCompareLt, 0, cz.compType);

        // For non-color, filter out sample bits
        if (IsColor() == false)
        {
            CompPair  co = { MetaDataAddrCompX, 0 };

            m_meta.Filter(co, MetaDataAddrCompareLt, 0, MetaDataAddrCompS);
        }

        // filter out everything above the metablock size
        cx = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompX, metaBlockSizeLog2.width - 1);
        m_meta.Filter(cx, MetaDataAddrCompareGt, 0, cx.compType);
        m_pipe.Filter(cx, MetaDataAddrCompareGt, 0, cx.compType);

        cy = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompY, metaBlockSizeLog2.height - 1);
        m_meta.Filter(cy, MetaDataAddrCompareGt, 0, cy.compType);
        m_pipe.Filter(cy, MetaDataAddrCompareGt, 0, cy.compType);

        cz = MetaDataAddrEquation::SetCompPair(MetaDataAddrCompZ, metaBlockSizeLog2.depth - 1);
        m_meta.Filter(cz, MetaDataAddrCompareGt, 0, cz.compType);
        m_pipe.Filter(cz, MetaDataAddrCompareGt, 0, cz.compType);

        // Make sure we still have the same number of channel bits
        PAL_ASSERT(m_pipe.GetNumValidBits() == numPipesLog2);

        // Loop through all channel and rb bits, and make sure these components exist in the metadata address
        for (uint32 bitPos = 0; bitPos < numPipesLog2; bitPos++)
        {
            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                const uint32  pipeData = m_pipe.Get(bitPos, compType);
                const uint32  rbData   = m_rb.Get(bitPos, compType);

                PAL_ASSERT(m_meta.Exists(compType, pipeData));
                PAL_ASSERT(m_meta.Exists(compType, rbData));
            }
        }

        // Loop through each rb id bit; if it is equal to any of the filtered channel bits, clear it
        for (uint32 i = 0; i < numTotalRbsLog2; i++)
        {
            for (uint32 j = 0; j < numPipesLog2; j++)
            {
                bool  rbEqualsPipe = true;

                if (settings.waMetaAliasingFixEnabled == false)
                {
                    rbEqualsPipe = m_pipe.IsEqual(m_rb, j, i);
                }
                else
                {
                    CompPair             compPair = { MetaDataAddrCompZ, MinMetaEqCompPos };
                    MetaDataAddrEquation filteredPipeEq(m_pGfxDevice, 1, "filtered");

                    m_pipe.Copy(&filteredPipeEq, j, 1);
                    filteredPipeEq.Filter(compPair, MetaDataAddrCompareGt, 0, MetaDataAddrCompZ);
                    rbEqualsPipe = m_rb.IsEqual(filteredPipeEq, i, 0);
                }

                for (uint32  compType = 0; rbEqualsPipe && (compType < MetaDataAddrCompNumTypes); compType++)
                {
                    m_rb.ClearBits(i, compType, 0);
                }
            }
        }

        // Loop through each bit of the channel, get the smallest coordinate, and remove it from the metaaddr,
        // and the rb_equation
        MergePipeAndRbEq();

        // Loop through the rb bits and see what remain; filter out the smallest coordinate if it remains
        const uint32  rbBitsLeft = RemoveSmallRbBits();

        // capture the size of the metaaddr
        uint32  metaEquationSize = m_meta.GetNumValidBits();

        // resize to 32 bits...make this a nibble address
        m_meta.SetEquationSize(32);

        // Concatenate the macro address above the current address
        for(uint32 j = 0; metaEquationSize < m_meta.GetNumValidBits(); metaEquationSize++, j++)
        {
            m_meta.SetBit(metaEquationSize, MetaDataAddrCompM, j);
        }

        // Multiply by meta element size (in nibbles)
        if (IsColor())
        {
            m_meta.Shift(1); // Byte size element
        }
        else if (pCreateInfo->usageFlags.depthStencil)
        {
            m_meta.Shift(3); // 4 Byte size elements
        }

        // Note the pipe_interleave_log2+1 is because address is a nibble address
        // Shift up from pipe interleave number of channel and rb bits left, and uncompressed fragments
        m_meta.Shift(numPipesLog2 + rbBitsLeft + uncompFragLog2, pipeInterleaveLog2 + 1);

        for (uint32 i = 0; i < numPipesLog2; i++ )
        {
            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                const uint32                     origPipeData = origPipeEquation.Get(i, compType);
                const uint32                     metaBitPos   = pipeInterleaveLog2 + 1 + i;
                const MetaDataAddrComponentType  addrCompType = static_cast<MetaDataAddrComponentType>(compType);

                m_meta.ClearBits(metaBitPos,  addrCompType, 0);
                m_meta.SetMask(metaBitPos, addrCompType, origPipeData);
            }
        }

        // Put in remaining rb bits
        for (uint32 i = 0, j = 0; j < rbBitsLeft; i = (i + 1) % numTotalRbsLog2)
        {
            const uint32  numComponents  = m_rb.GetNumComponents(i);
            const bool    isRbEqAppended = (numComponents > GetRbAppendedBit(i));

            if (isRbEqAppended)
            {
                for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
                {
                    const uint32  origRbData = origRbEquation.Get(i, compType);

                    m_meta.SetMask(pipeInterleaveLog2 + 1 + numPipesLog2 + j,
                                   static_cast<MetaDataAddrComponentType>(compType),
                                   origRbData);
                }

                j++;
            }
        }

        // Put in the uncompressed fragment bits
        for (uint32 i = 0; i < static_cast<uint32>(uncompFragLog2); i++)
        {
            m_meta.SetBit(pipeInterleaveLog2 + 1 + numPipesLog2 + rbBitsLeft + i,
                          MetaDataAddrCompS,
                          compFragLog2 + i);
        }

        // Ok, we always calculate the meta-equation to be 32-bits long, but that's enough to address 4Gnibbles.
        // Trim this down to be no bigger than log2(mask-ram-size)
        FinalizeMetaEquation(TotalSize());

        // After meta equation calculation is done extract meta equation parameter information
        m_meta.GenerateMetaEqParamConst(m_image, maxFragsLog2, m_firstUploadBit, &m_metaEqParam);

        // For some reason, the number of samples addressed by the equation sometimes differs from the number of
        // samples associated with the data-surface.  Still seems to work...
        PAL_ALERT (m_effectiveSamples != (1u << numSamplesLog2));
    }
}

// =====================================================================================================================
// Set the meta equation size to the minimum required to support the actual size of the mask-ram
void Gfx9MaskRam::FinalizeMetaEquation(
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

    m_meta.PrintEquation(m_pGfxDevice->Parent());

    // Determine how many sample bits are needed to process this equation.
    m_effectiveSamples = m_meta.GetNumSamples();

    // Mark this equation as valid
    m_metaEquationValid = true;
}

// =====================================================================================================================
void Gfx9MaskRam::CalcRbEquation(
    uint32  numSesLog2,
    uint32  numRbsPerSeLog2)
{
    const Pal::Device*      pDevice         = m_pGfxDevice->Parent();
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
        m_rb.SetBit(0, MetaDataAddrCompX, rbRegion[MetaDataAddrCompX]);
        m_rb.SetBit(0, MetaDataAddrCompY, rbRegion[MetaDataAddrCompY]);

        rbRegion[MetaDataAddrCompX]++;
        rbRegion[MetaDataAddrCompY]++;

        if (settings.waMetaAliasingFixEnabled == false)
        {
            m_rb.SetBit(0, MetaDataAddrCompY, rbRegion[MetaDataAddrCompY]);
        }

        start++;
    }

    for (uint32 i = 0; i < (2 * (numTotalRbsLog2 - start)); i++)
    {
        const uint32                     index    = start + (((start + i) >= numTotalRbsLog2)
                                                    ? (2 * (numTotalRbsLog2 - start) - i - 1)
                                                    : i);
        const MetaDataAddrComponentType  compType = (((i % 2) == 1) ? MetaDataAddrCompX : MetaDataAddrCompY);

        m_rb.SetBit(index, compType, rbRegion[compType]);
        rbRegion[compType]++;
    }

    m_rb.PrintEquation(pDevice);
}

// =====================================================================================================================
uint32 Gfx9MaskRam::CapPipe() const
{
    const AddrSwizzleMode  swizzleMode        = GetSwizzleMode();
    const uint32           blockSizeLog2      = Log2(AddrMgr2::GetBlockSize(swizzleMode));
    const uint32           numSesLog2         = m_pGfxDevice->GetNumShaderEnginesLog2();
    const uint32           pipeInterleaveLog2 = m_pGfxDevice->GetPipeInterleaveLog2();

    uint32  numPipesLog2 = m_pGfxDevice->GetNumPipesLog2();

    // pipes+SEs can't exceed 32 for now
    PAL_ASSERT((numPipesLog2 + numSesLog2) <= 5);

    // Since we are not supporting SE affinity anymore, just add nu_ses to num_pipes
    numPipesLog2 += numSesLog2;

    return Min(blockSizeLog2 - pipeInterleaveLog2, numPipesLog2);
}

// =====================================================================================================================
uint32  Gfx9MaskRam::GetBytesPerPixelLog2() const
{
    const SubResourceInfo*const pBaseSubResInfo = m_image.Parent()->SubresourceInfo(0);
    return Log2(BitsPerPixel(pBaseSubResInfo->format.format) / 8);
}

// =====================================================================================================================
AddrSwizzleMode Gfx9MaskRam::GetSwizzleMode() const
{
    // We always want to use the swizzle mode associated with the first sub-resource.
    //   1) For color images, the swizzle mode is constant across all sub-resources.
    //   2) For depth+stencil images, the meta equation is generated based on the swizzle mode of the depth aspect
    //      (which will always be the first aspect).
    //   3) For stencil-only or Z-only images, there is only one aspect, so it will be first.
    const SubResourceInfo*const pBaseSubResInfo = m_image.Parent()->SubresourceInfo(0);

    return m_image.GetAddrSettings(pBaseSubResInfo).swizzleMode;
}

// =====================================================================================================================
// Retrieves the pipe-bank xor setting for the image associated with this mask ram.
uint32  Gfx9MaskRam::GetPipeBankXor(
    ImageAspect   aspect
    ) const
{
    // Check for cMask; cMask can't be here as the pipe-bank xor setting for cMask is the pipe-bank xor setting
    // associated with the fMask surface.  cMask needs to override this function.
    PAL_ASSERT (m_firstUploadBit != 0);

    // The pipeBankXor setting for an image is expected to be a constant across all mips / slices of one aspect.
    const auto*    pParent      = m_image.Parent();
    const SubresId baseSubResId = { aspect, 0, 0 };

    const uint32   pipeBankXor  = AddrMgr2::GetTileInfo(pParent, baseSubResId)->pipeBankXor;

    return AdjustPipeBankXorForSwizzle(pipeBankXor);
}

// =====================================================================================================================
// Initialize this object's "m_eqGpuAccess" with data used to eventually upload this equation to GPU accessible memory
void Gfx9MaskRam::InitEqGpuAccess(
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
bool Gfx9MaskRam::IsPipeAligned(
    const Image*  pImage)
{
    return GetMetaFlags(*pImage).pipeAligned;
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

// =====================================================================================================================
bool Gfx9MaskRam::IsRbAligned(
    const Image*  pImage)
{
    return GetMetaFlags(*pImage).rbAligned;
}

// =====================================================================================================================
// Returns true for swizzle modes that are the equivalent of the old "thick" tiling modes on pre-gfx9 HW.
bool Gfx9MaskRam::IsThick() const
{
    const auto&            createInfo  = m_image.Parent()->GetImageCreateInfo();
    const AddrSwizzleMode  swizzleMode = GetSwizzleMode();

    return  ((createInfo.imageType == ImageType::Tex3d) &&
             (IsStandardSwzzle(swizzleMode) ||
              IsZSwizzle(swizzleMode)));
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
// Iterate through each pipe bits from lsb to msb, and remove the smallest coordinate contributing to that bit's
// equation.  Remove these bits from the metadata address, and the RB equations.
//
//      The idea is this: we first start with the lsb of the rb_id, find the smallest component, and remove it from
//      the metadata address, and also from all upper rb_id bits that have this component.  For the rb_id bits, if we
//      removed that component, then we add back all of the other components that contributed to the lsb of rb_id:
void Gfx9MaskRam::MergePipeAndRbEq()
{
    const Pal::Device*  pDevice = m_pGfxDevice->Parent();

    for (uint32  pipeAddrBit = 0; pipeAddrBit < m_pipe.GetNumValidBits(); pipeAddrBit++)
    {
        // Find the lowest coordinate within this pipeAddrBit that is contributing.
        CompPair  lowPipe;
        if (m_pipe.FindSmallComponent(pipeAddrBit, &lowPipe))
        {
            const uint32 lowPosMask = 1 << lowPipe.compPos;
            const uint32 oldSize    = m_meta.GetNumValidBits();
            m_meta.Filter(lowPipe, MetaDataAddrCompareEq);
            PAL_ASSERT(m_meta.GetNumValidBits() == (oldSize - 1));

            m_pipe.Remove(lowPipe);

            for (uint32  rbAddrBit = 0; rbAddrBit < m_rb.GetNumValidBits(); rbAddrBit++)
            {
                const uint32  rbData = m_rb.Get(rbAddrBit, lowPipe.compType);
                if (TestAnyFlagSet (rbData, lowPosMask))
                {
                    m_rb.ClearBits(rbAddrBit, lowPipe.compType, ~lowPosMask);

                    // if we actually removed something from this bit, then add the remaining
                    // channel bits, as these can be removed for this bit
                    for (uint32  localPipeCompType = 0;
                                 localPipeCompType < MetaDataAddrCompNumTypes;
                                 localPipeCompType++)
                    {
                        uint32  eqData = m_pipe.Get(pipeAddrBit, localPipeCompType);
                        uint32  lowPipeBit;

                        while (BitMaskScanForward(&lowPipeBit, eqData))
                        {
                            const CompPair  localPipePair =
                                    MetaDataAddrEquation::SetCompPair(localPipeCompType, lowPipeBit);

                            if (MetaDataAddrEquation::CompareCompPair(localPipePair,
                                                                      lowPipe,
                                                                      MetaDataAddrCompareEq) == false)
                            {
                                m_rb.SetBit(rbAddrBit, localPipePair.compType, localPipePair.compPos);
                                SetRbAppendedBit(rbAddrBit, 1);
                            }

                            eqData &= ~(1 << lowPipeBit);
                        }
                    }
                }
            } // end loop through all the rb bits
        } // end check for a non-empty pipe equation
    } // end loop through all 32 bits in the equation

    m_rb.PrintEquation(pDevice);
    m_pipe.PrintEquation(pDevice);
    m_meta.PrintEquation(pDevice);
}

// =====================================================================================================================
// Iterate through the remaining RB bits, from lsb to msb, taking the smallest coordinate of each bit, and removing it
// from the metadata equation, and the remaining upper RB bits.  Like for the pipe bits, if an RB bit gets a component
// removed, then we add in all other terms not already present from the Rb bit that did the removal.
uint32 Gfx9MaskRam::RemoveSmallRbBits()
{
    const Pal::Device*  pDevice    = m_pGfxDevice->Parent();
    uint32              rbBitsLeft = 0;

    for (uint32  rbAddrBit = 0; rbAddrBit < m_rb.GetNumValidBits(); rbAddrBit++)
    {
        const uint32  neededNumComponents = (GetRbAppendedBit(rbAddrBit) != 0);

        // Find the lowest coordinate within this pipeAddrBit that is contributing.
        CompPair  lowRb;
        if ((m_rb.GetNumComponents(rbAddrBit) > neededNumComponents) &&
            m_rb.FindSmallComponent(rbAddrBit, &lowRb))
        {
            const uint32  lowRbMask = 1 << lowRb.compPos;

            rbBitsLeft++;

            m_meta.Filter(lowRb, MetaDataAddrCompareEq);

            // We need to find any other RB bits that have lowRb{AddrType,Position} in their equation
            for (uint32  scanHiRbAddrBit = rbAddrBit + 1;
                         scanHiRbAddrBit < m_rb.GetNumValidBits();
                         scanHiRbAddrBit++)
            {
                if (m_rb.IsSet(scanHiRbAddrBit, lowRb.compType, lowRbMask))
                {
                    // Don't forget to eliminate this component.
                    m_rb.ClearBits(scanHiRbAddrBit, lowRb.compType, ~lowRbMask);

                    // Loop through all the elements in m_rb[rbAddrBit].  Add everything that isn't equivalent to
                    // "lowRb" into m_rb[scanHiRbAddrBit]
                    for (uint32  localRbAddrType = 0; localRbAddrType < MetaDataAddrCompNumTypes; localRbAddrType++)
                    {
                        uint32  rbData = m_rb.Get(rbAddrBit, localRbAddrType);
                        if (localRbAddrType == lowRb.compType)
                        {
                            rbData &= ~lowRbMask;
                        }

                        if (rbData != 0)
                        {
                            m_rb.SetMask(scanHiRbAddrBit, localRbAddrType, rbData);
                            SetRbAppendedBit(scanHiRbAddrBit, GetRbAppendedBit(rbAddrBit));
                        }
                    }
                } // end check for the higher RB bit containing a reference to the just-found "low bit"
            } // end loop through the "higher" RB bits
        } // end check for a valid small component of this RB bit
    } // end loop through all the RB bits

    m_rb.PrintEquation(pDevice);
    m_meta.PrintEquation(pDevice);

    return rbBitsLeft;
}

// =====================================================================================================================
// Uploads the meta-equation associated with this mask ram to GPU accessible memory.
void Gfx9MaskRam::UploadEq(
    CmdBuffer*  pCmdBuffer
    ) const
{
    const Pal::Image*   pParentImg = m_image.Parent();
    const Pal::Device*  pDevice    = m_pGfxDevice->Parent();

    if (IsMetaEquationValid())
    {
        // If this trips, that implies that InitEqGpuAccess() wasn't called during the creation of this
        // mask ram object.
        PAL_ASSERT (m_eqGpuAccess.offset != 0);

        const auto&    boundMem = pParentImg->GetBoundGpuMemory();
        const gpusize  offset   = boundMem.Offset() + m_eqGpuAccess.offset;

        m_meta.Upload(pDevice, pCmdBuffer, *boundMem.Memory(), offset, m_firstUploadBit);
    }
}

// =====================================================================================================================
void Gfx9MaskRam::CpuUploadEq(
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
// Determines if the given Image object should use fast color clears.
bool Gfx9MaskRam::SupportFastColorClear(
    const Pal::Device& device,
    const Image&       image,
    AddrSwizzleMode    swizzleMode)
{
    const Pal::Image*const pParent    = image.Parent();
    const ImageCreateInfo& createInfo = pParent->GetImageCreateInfo();
    const Gfx9PalSettings& settings   = GetGfx9Settings(device);
    const GfxIpLevel       gfxLevel   = pParent->GetDevice()->ChipProperties().gfxLevel;

    // Choose which fast-clear setting to examine based on the type of Image we have.
    const bool fastColorClearEnable = (createInfo.imageType == ImageType::Tex2d) ?
                                       settings.fastColorClearEnable : settings.fastColorClearOn3dEnable;

    // Sometimes shader-writeable surfaces still allow fast-clears...  check that status here
    const bool allowShaderWriteableSurfaces =
            // If ths image isn't shader-writeable at all, then there isn't a problem
            ((pParent->IsShaderWritable() == false) ||
            // GFX10 doesn't have a problem with shader-writeable surfaces anyway
             IsGfx10(gfxLevel)                      ||
            // Enable Fast Clear Support if some mips are not shader writable.
             (pParent->FirstShaderWritableMip() != 0));

    // Only enable fast color clear iff:
    // - The Image's format supports it.
    // - The Image is a Color Target - (ensured by caller)
    // - If the image is shader write-able, it's shader-writeable in a good way
    // - The Image is not linear tiled.
    PAL_ASSERT(pParent->IsRenderTarget());

    return (fastColorClearEnable                       == true)  &&
           allowShaderWriteableSurfaces                          &&
           (AddrMgr2::IsLinearSwizzleMode(swizzleMode) == false) &&
           (SupportsFastColorClear(createInfo.swizzledFormat.format));
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
    if (pParent->IsDepthStencil())
    {
        const auto& settings   = GetGfx9Settings(device);
        const auto& createInfo = pParent->GetImageCreateInfo();

        static const uint32 MinHtileWidth  = 8;
        static const uint32 MinHtileHeight = 8;

        hTileUsage.dsMetadata = ((pParent->IsShared()                   == false)         &&
                                 (pParent->IsMetadataDisabledByClient() == false)         &&
                                 (settings.htileEnable                  == true)          &&
                                 (createInfo.extent.width               >= MinHtileWidth) &&
                                 (createInfo.extent.height              >= MinHtileHeight));

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

    }

    return hTileUsage;
}

// =====================================================================================================================
Gfx9Htile::Gfx9Htile(
    const Image&     image,
    HtileUsageFlags  hTileUsage)
    :
    Gfx9MaskRam(image,
                2, // hTile uses 32-bit (4 byte) quantities
                3), // Equation is nibble addressed, so the low three bits will be zero for a dword quantity
    m_hTileUsage(hTileUsage)
{
    memset(&m_addrOutput,      0, sizeof(m_addrOutput));
    memset(m_dbHtileSurface,   0, sizeof(m_dbHtileSurface));
    memset(m_dbPreloadControl, 0, sizeof(m_dbPreloadControl));

    m_addrOutput.pMipInfo = &m_addrMipOutput[0];
    m_addrOutput.size     = sizeof(m_addrOutput);
    m_flags.value         = 0;
}

// =====================================================================================================================
// Returns the pipe-bank xor setting for this hTile sruface.
uint32 Gfx9Htile::GetPipeBankXor(
    ImageAspect   aspect
    ) const
{
    const Pal::Device*     pDevice  = m_pGfxDevice->Parent();
    const Gfx9PalSettings& settings = GetGfx9Settings(*pDevice);

    // Due to a HW bug, some GPU's don't support the use of a pipe-bank xor value for hTile surfaces.
    return (settings.waHtilePipeBankXorMustBeZero
            ? 0
            : Gfx9MaskRam::GetPipeBankXor(aspect));
}

// =====================================================================================================================
uint32 Gfx9Htile::GetNumSamplesLog2() const
{
    return Log2(m_image.Parent()->GetImageCreateInfo().samples);
}

// =====================================================================================================================
// Builds the PM4 packet headers for an image of PM4 commands used to write this View object to hardware.
void Gfx9Htile::SetupHtilePreload(
    uint32       mipLevel)
{
    const  Pal::Image*       pParent         = m_image.Parent();
    const  Pal::Device*      pDevice         = m_pGfxDevice->Parent();
    const  Gfx9PalSettings&  settings        = GetGfx9Settings(*pDevice);
    const  SubresId          baseSubResource = pParent->GetBaseSubResource();

    m_dbHtileSurface[mipLevel].most.PREFETCH_WIDTH  = 0;
    m_dbHtileSurface[mipLevel].most.PREFETCH_HEIGHT = 0;

    if (settings.dbPreloadEnable && (settings.waDisableHtilePrefetch == 0))
    {
        const uint32            activeRbCount     = pDevice->ChipProperties().gfx9.numActiveRbs;
        const SubresId          subResId          = { baseSubResource.aspect, mipLevel, 0 };
        const SubResourceInfo*  pSubResInfo       = pParent->SubresourceInfo(subResId);
        const uint32            imageSizeInPixels = (pSubResInfo->actualExtentTexels.width *
                                                     pSubResInfo->actualExtentTexels.height);

        // Note: For preloading to be enabled efficiently, the DB_PRELOAD_CONTROL register needs to be set-up. The
        // ideal setting is the largest rectangle of the Image's aspect ratio which can completely fit within the
        // DB cache (centered in the Image). The preload rectangle doesn't need to be exact.
        const uint32 cacheSizeInPixels = (DbHtileCacheSizeInPixels * activeRbCount);
        const uint32 width             = pSubResInfo->extentTexels.width;
        const uint32 height            = pSubResInfo->extentTexels.height;

        // DB Preload window is in 64 pixel increments both horizontally & vertically.
        constexpr uint32 BlockWidth  = 64;
        constexpr uint32 BlockHeight = 64;

        regDB_PRELOAD_CONTROL*  pPreloadControl = &m_dbPreloadControl[mipLevel];

        m_dbHtileSurface[mipLevel].most.HTILE_USES_PRELOAD_WIN = settings.dbPreloadWinEnable;
        m_dbHtileSurface[mipLevel].most.PRELOAD                = 1;

        if (imageSizeInPixels <= cacheSizeInPixels)
        {
            // The entire Image fits into the DB cache!
            pPreloadControl->bits.START_X = 0;
            pPreloadControl->bits.START_Y = 0;
            pPreloadControl->bits.MAX_X   = ((width  - 1) / BlockWidth);
            pPreloadControl->bits.MAX_Y   = ((height - 1) / BlockHeight);
        }
        else
        {
            // Image doesn't fit into the DB cache; compute the largest centered rectangle, while preserving the
            // Image's aspect ratio.
            //
            // From DXX:
            //      w*h = cacheSize, where w = aspectRatio*h
            // Thus,
            //      aspectRatio*(h^2) = cacheSize
            // so,
            //      h = sqrt(cacheSize/aspectRatio)
            const float ratio = static_cast<float>(width) / static_cast<float>(height);

            // Compute the height in blocks first; assume there will be more width than height, giving the width
            // decision a lower granularity, and by doing it second typically more cache will be utilized.
            const uint32 preloadWinHeight = static_cast<uint32>(Math::Sqrt(cacheSizeInPixels / ratio));
            // Round up, but not beyond the window size.
            const uint32 preloadWinHeightInBlocks = Min((preloadWinHeight + BlockHeight - 1) / BlockHeight,
                                                         height / BlockHeight);

            // Accurate width can now be derived from the height.
            const uint32 preloadWinWidth = Min(cacheSizeInPixels / (preloadWinHeightInBlocks * BlockHeight), width);
            // Round down, to ensure that the size is smaller than the DB cache.
            const uint32 preloadWinWidthInBlocks = (preloadWinWidth / BlockWidth);

            PAL_ASSERT(cacheSizeInPixels >=
                (preloadWinWidthInBlocks * BlockWidth * preloadWinHeightInBlocks * BlockHeight));

            // Program the preload window, offsetting the preloaded area towards the middle of the Image. Round down
            // to ensure the area is positioned partially outside the Image. (Rounding to nearest would position the
            // rectangle more evenly, but would not guarantee the whole rectangle is inside the Image.)
            pPreloadControl->bits.START_X = ((width - preloadWinWidthInBlocks * BlockWidth) / 2) / BlockWidth;
            pPreloadControl->bits.START_Y = ((height - preloadWinHeightInBlocks * BlockHeight) / 2) / BlockHeight;
            pPreloadControl->bits.MAX_X   = (pPreloadControl->bits.START_X + preloadWinWidthInBlocks);
            pPreloadControl->bits.MAX_Y   = (pPreloadControl->bits.START_Y + preloadWinHeightInBlocks);
        }
    }
}

// =====================================================================================================================
// Initializes this HTile object for the given Image and mipmap level.
Result Gfx9Htile::Init(
    gpusize*  pGpuOffset,    // [in,out] Current GPU memory offset & size
    bool      hasEqGpuAccess)
{
    const Pal::Device&       device           = *(m_pGfxDevice->Parent());
    const  Gfx9PalSettings&  settings         = GetGfx9Settings(device);
    const  Pal::Image*const  pParent          = m_image.Parent();
    const  ImageCreateInfo&  imageCreateInfo  = pParent->GetImageCreateInfo();
    const  auto&             chipProps        = device.ChipProperties();
    const  uint32            activeRbCount    = chipProps.gfx9.numActiveRbs;

    m_flags.compressZ = settings.depthCompressEnable   && (m_hTileUsage.dsMetadata != 0);
    m_flags.compressS = settings.stencilCompressEnable && (m_hTileUsage.dsMetadata != 0);

    // NOTE: Default ZRANGE_PRECISION to 1, since this is typically the optimal value for DX applications, since they
    // usually clear Z to 1.0f and use a < depth comparison for their depth testing. But we change ZRANGE_PRECISION
    // to 0 via UpdateZRangePrecision() when we detect there is a clear Z to 0.0f. We want more precision on the
    // far Z plane.
    m_flags.zrangePrecision = 1;

    // If the associated image is a Z-only format, then setup hTile to not include stencil data.
    m_flags.tileStencilDisable = ((m_image.IsHtileDepthOnly()
                                  ) ? 1 : 0);

    // Determine the subResource ID of the base slice and mip-map for this aspect
    const SubresId  baseSubResource = pParent->GetBaseSubResource();

    // Htile control registers vary per mip-level.  Compute those here.
    for (uint32  mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
    {
        const SubresId              subResId          = { baseSubResource.aspect, mipLevel, 0 };
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

        // Setup HTile preload if it's supported.
        if ((chipProps.gfxLevel == GfxIpLevel::GfxIp9)
            )
        {
            SetupHtilePreload(mipLevel);
        }
    }

    // Call the address library to compute the HTile properties.
    const SubResourceInfo*  pBaseSubResInfo = pParent->SubresourceInfo(baseSubResource);
    Result                  result          = ComputeHtileInfo(pBaseSubResInfo);
    if (result == Result::Success)
    {
        // Compute our aligned GPU memory offset and update the caller-provided running total.  Don't update the
        // overall image size with every mip level as the entire size of hTile is computed all at once.
        UpdateGpuMemOffset(pGpuOffset);

        // The addressing equation is the same for all sub-resources, so only bother to calculate it once
        CalcMetaEquation();

        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the hTile equation
            InitEqGpuAccess(pGpuOffset);
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
    addrHtileIn.depthFlags        = pAddrMgr->DetermineSurfaceFlags(*pParent, pSubResInfo->subresId.aspect);
    addrHtileIn.hTileFlags        = GetMetaFlags(m_image);
    addrHtileIn.firstMipIdInTail  = pParentSurfAddrOut->firstMipIdInTail;

    const ADDR_E_RETURNCODE addrRet = Addr2ComputeHtileInfo(device.AddrLibHandle(), &addrHtileIn, &m_addrOutput);
    PAL_ASSERT(addrRet == ADDR_OK);

    if (addrRet == ADDR_OK)
    {
        // HW needs to be programmed to the same parameters the surface was created with.
        for (uint32  mipLevel = 0; mipLevel < imageCreateInfo.mipLevels; mipLevel++)
        {
            m_dbHtileSurface[mipLevel].bits.PIPE_ALIGNED = addrHtileIn.hTileFlags.pipeAligned;

            if (device.ChipProperties().gfxLevel == GfxIpLevel::GfxIp9)
            {
                m_dbHtileSurface[mipLevel].gfx09.RB_ALIGNED = addrHtileIn.hTileFlags.rbAligned;
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
    pBlockSize->depth  = 3;
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
    float  depthValue
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
// Computes a mask for updating the specified aspects of the HTile buffer
uint32 Gfx9Htile::GetAspectMask(
    uint32  aspectFlags
    ) const
{
    uint32 mask = 0;

    if (TileStencilDisabled() == false)
    {
        if (TestAnyFlagSet(aspectFlags, HtileAspectDepth))
        {
            mask |= Gfx9HtileDepthMask;
        }
        if (TestAnyFlagSet(aspectFlags, HtileAspectStencil))
        {
            mask |= Gfx9HtileStencilMask;
        }
    }
    else if (TestAnyFlagSet(aspectFlags, HtileAspectDepth))
    {
        // All bits are used for depth when tile stencil is disabled
        mask = UINT_MAX;
    }

    return mask;
}

// =====================================================================================================================
// Computes a mask and value for updating the HTile buffer for a "fast" resummarize operation. The "fast" resummarize is
// quicker than a normal resummarize, but less precise because we are updating HTile to indicate the full zRange is
// included in each tile.
uint32 Gfx9Htile::ComputeResummarizeData() const
{
    constexpr uint32 Uint14Max = 0x3FFF; // Maximum value of a 14bit integer.

    // Convert the trivial z bounds to 14-bit zmin/zmax uint values.
    constexpr uint32 ZMin = 0;
    constexpr uint32 ZMax = Uint14Max;

    // The depth buffer was expanded at some point prior to this being executed, so we need to set the HTile's zMask
    // to indicate that no z planes are stored (each depth value is directly stored in the surface).
    constexpr uint32 ZMask = 15;

    uint32 htileData = 0;

    if (TileStencilDisabled() == false)
    {
        // If stencil is present, each HTILE is laid out as-follows, according to the DB spec:
        // |31       12|11 10|9    8|7   6|5   4|3     0|
        // +-----------+-----+------+-----+-----+-------+
        // |  Z Range  |     | SMem | SR1 | SR0 | ZMask |

        // The base value for zRange is either zMax or zMin, depending on ZRANGE_PRECISION. Currently, PAL programs
        // ZRANGE_PRECISION to 1 (zMax is the base) because there's no easy way to track that state across command
        // buffers build on many threads.
        //
        // zRange is encoded as follows: the high 14 bits are the base z value (zMax in our case). The low 6 bits
        // are a code represending the abs(zBase - zOther). In our case, we need to select a delta code representing
        // abs(zMax - zMin), which is always 0x3FFF (maximum 14 bit uint value). According to setion 9.1.3 of the DB
        // spec, the delta code in our case would be 0x3F (all 6 bits set).
        constexpr uint32 Delta  = 0x3F;
        constexpr uint32 ZRange = ((ZMax << 6) | Delta);
        // We set SMem to 0x3 to indicate the stencil buffer was expanded.
        constexpr uint32 SMem   = 0x3;
        // In case of resummarize, we set both sr0 and sr1 to 0x3, which means both may_pass bit and may_fail bit.
        constexpr uint32 SR1    = 0x3;
        constexpr uint32 SR0    = 0x3;

        htileData = ( ((ZRange & 0xFFFFF) << 12) |
                      ((ZMask  &     0xF) <<  0) |
                      ((SMem   &     0x3) <<  8) |
                      ((SR1    &     0x3) <<  6) |
                      ((SR0    &     0x3) <<  4));
    }
    else
    {
        // If stencil is absent, each HTILE is laid out as follows, according to the DB spec:
        // |31     18|17      4|3     0|
        // +---------+---------+-------+
        // |  Max Z  |  Min Z  | ZMask |

        htileData = ( ((ZMax  & Uint14Max) << 18) |
                      ((ZMin  & Uint14Max) <<  4) |
                      ((ZMask &       0xF) <<  0) );
    }
    return htileData;
}

// =====================================================================================================================
// Computes the initial value of the htile which depends on whether or not tile stencil is disabled.
uint32 Gfx9Htile::GetInitialValue() const
{
    // These are the possible layouts of hTile memory:

    // Z and stencil:
    //      |31       12|11 10|9    8|7   6|5   4|3     0|
    //      +-----------+-----+------+-----+-----+-------+
    //      |  Z Range  |     | SMem | SR1 | SR0 | ZMask |
    //
    //
    // Z only (no stencil):
    //      |31     18|17      4|3     0|
    //      +---------+---------+-------+
    //      |  Max Z  |  Min Z  | ZMask |
    //

    // Initial values for a fully decompressed/expanded htile
    constexpr uint32 ZMaskExpanded            = 0xf;
    constexpr uint32 SMemExpanded             = 0x3;
    constexpr uint32 SR1                      = 0x3;
    constexpr uint32 SR0                      = 0x3;
    constexpr uint32 InitialValueDepthOnly    = (ZMaskExpanded  << 0);
    constexpr uint32 InitialValueDepthStencil = ((SMemExpanded  << 8) |
                                                 (SR1           << 6) |
                                                 (SR0           << 4) |
                                                 (ZMaskExpanded << 0));

    uint32 initialValue;

    if (TileStencilDisabled())
    {
        initialValue = InitialValueDepthOnly;
    }
    else
    {
        initialValue = InitialValueDepthStencil;

    }

    return initialValue;
}

//=============== Implementation for Gfx9Dcc: ==========================================================================

// =====================================================================================================================
Gfx9Dcc::Gfx9Dcc(
    const Image&  image)
    :
    Gfx9MaskRam(image,
                0,  // DCC uses 1-byte quantities, log2(1) = 0
                1), // ignore the first bit of a nibble equation
    m_dccControl()
{
    memset(&m_addrOutput, 0, sizeof(m_addrOutput));

    m_addrOutput.size     = sizeof(m_addrOutput);
    m_addrOutput.pMipInfo = &m_addrMipOutput[0];
}

// =====================================================================================================================
uint32 Gfx9Dcc::GetNumEffectiveSamples(
    DccClearPurpose  clearPurpose
    ) const
{
    // If this is an init, then we want to write every pixel that the equation can address.  the number of samples
    // addressed by the equation isn't necessarily the same as the number of samples contained in the image (I
    // don't understand that either...).
    uint32  numSamples = Gfx9MaskRam::GetNumEffectiveSamples();
    if (clearPurpose == DccClearPurpose::FastClear)
    {
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
    const uint32          bppLog2     = GetBytesPerPixelLog2();
    const ImageType       imageType   = m_image.GetOverrideImageType();
    const AddrSwizzleMode swizzleMode = GetSwizzleMode();

    // There are instances where 3D images use the 2D layout.
    bool  isEffective2d    = AddrMgr2::IsDisplayableSwizzle(swizzleMode);
    bool  useZswizzleFor3d = AddrMgr2::IsZSwizzle(swizzleMode);

    const Pal::Device& palDevice = *(m_pGfxDevice->Parent());

    if (IsGfx10(palDevice) && (imageType == ImageType::Tex3d))
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

    if ((imageType == ImageType::Tex2d) || isEffective2d)
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
    }
    else if (imageType == ImageType::Tex3d)
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
                {  2, 4, 4 },  // 64bpp
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

        // 1D images...  should never get here.  GFX9 1D images don't support DCC.
    }
}

// =====================================================================================================================
Result Gfx9Dcc::Init(
    gpusize*  pGpuOffset,
    bool      hasEqGpuAccess)
{
    Result result = ComputeDccInfo();

    if (result == Result::Success)
    {
        // Compute our aligned GPU memory offset and update the caller-provided running total.
        UpdateGpuMemOffset(pGpuOffset);

        SetControlReg();

        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the DCC equation
            InitEqGpuAccess(pGpuOffset);
        }
    }

    return result;
}

// =====================================================================================================================
// Calls into AddrLib to compute DCC info for a subresource
Result Gfx9Dcc::ComputeDccInfo()
{
    const Pal::Image*const      pParent         = m_image.Parent();
    const Pal::Device*const     pDevice         = pParent->GetDevice();
    const Pal::ImageCreateInfo& imageCreateInfo = pParent->GetImageCreateInfo();
    const auto*const            pAddrMgr        = static_cast<const AddrMgr2::AddrMgr2*>(pDevice->GetAddrMgr());

    // The Addr2 interface computes all DCC info off of the base level information, so setup a sub-res pointer to
    // the base of the color aspect here.
    const SubresId         subResId           = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo* pSubResInfo        = pParent->SubresourceInfo(subResId);
    const auto&            surfSettings       = m_image.GetAddrSettings(pSubResInfo);
    const auto*            pParentSurfAddrOut = m_image.GetAddrOutput(pSubResInfo);

    ADDR2_COMPUTE_DCCINFO_INPUT  dccInfoInput = {};
    Result                       result       = Result::ErrorInitializationFailed;

    dccInfoInput.size             = sizeof(dccInfoInput);
    dccInfoInput.dccKeyFlags      = GetMetaFlags(m_image);
    dccInfoInput.colorFlags       = pAddrMgr->DetermineSurfaceFlags(*pParent, subResId.aspect);
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

        CalcMetaEquation();

        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Returns the optimal value of DCC_CONTROL.MIN_COMPERSSED_BLOCK_SIZE
uint32 Gfx9Dcc::GetMinCompressedBlockSize() const
{
    const auto&  chipProp = m_pGfxDevice->Parent()->ChipProperties();

    //    [min-compressed-block-size] should be set to 32 for dGPU and 64 for APU because all of our APUs to date
    //    use DIMMs which have a request granularity size of 64B while all other chips have a 32B request size
    //
    //    "The recommended solution is to limit the minimum compression to 64 B".
    //
    // So, for Raven (an APU) using 64-byte min-block-size is both a good idea and a requirement.
    return static_cast<uint32>((chipProp.gpuType == GpuType::Integrated)
                               ? Gfx9DccMinBlockSize::BlockSize64B
                               : Gfx9DccMinBlockSize::BlockSize32B);
}

// =====================================================================================================================
// Calculates the value for the CB_DCC_CONTROL register
void Gfx9Dcc::SetControlReg()
{
    const SubresId          subResId    = { ImageAspect::Color, 0, 0 };
    const Pal::Image*       pParent     = m_image.Parent();
    const SubResourceInfo*  pSubResInfo = pParent->SubresourceInfo(subResId);
    const Pal::Device*      pDevice     = m_pGfxDevice->Parent();
    const GfxIpLevel        gfxLevel    = pDevice->ChipProperties().gfxLevel;
    const ImageCreateInfo&  createInfo  = pParent->GetImageCreateInfo();
    const auto&             settings    = GetGfx9Settings(*pDevice);

    // Setup DCC control registers with suggested value from spec
    m_dccControl.bits.KEY_CLEAR_ENABLE = 0; // not supported on VI

    // MAX_UNCOMPRESSED_BLOCK_SIZE 3:2 none Sets the maximum amount of data that may be compressed into one block. Some
    // other clients may not be able to handle larger sizes. CB_RESOLVEs cannot have this setting larger than the size
    // of one sample's data.
    // 64B (Set for 8bpp 2+ fragment surfaces needing HW resolves)
    // 128B (Set for 16bpp 2+ fragment surfaces needing HW resolves)
    // 256B (default)
    m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE = static_cast<unsigned int>(Gfx9DccMaxBlockSize::BlockSize256B);
    if ((gfxLevel == GfxIpLevel::GfxIp9) && (createInfo.samples >= 2))
    {
        const uint32 bitsPerPixel = BitsPerPixel(createInfo.swizzledFormat.format);

        if (bitsPerPixel == 8)
        {
            m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE =
                static_cast<unsigned int>(Gfx9DccMaxBlockSize::BlockSize64B);
        }
        else if (bitsPerPixel == 16)
        {
            m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE =
                static_cast<unsigned int>(Gfx9DccMaxBlockSize::BlockSize128B);
        }
    }

    m_dccControl.bits.MIN_COMPRESSED_BLOCK_SIZE = GetMinCompressedBlockSize();
    m_dccControl.bits.COLOR_TRANSFORM           = DCC_CT_AUTO;
    m_dccControl.bits.LOSSY_RGB_PRECISION       = 0;
    m_dccControl.bits.LOSSY_ALPHA_PRECISION     = 0;

    // If this DCC surface is potentially going to be used in texture fetches though, we need some special settings.
    if (pSubResInfo->flags.supportMetaDataTexFetch)
    {
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS    = 1;
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = static_cast<unsigned int>(Gfx9DccMaxBlockSize::BlockSize64B);

        if (IsGfx10(gfxLevel))
        {
            m_dccControl.bits.INDEPENDENT_64B_BLOCKS     = 0;
            m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE  = static_cast<unsigned int>(
                                                                  Gfx9DccMaxBlockSize::BlockSize128B);
            m_dccControl.gfx10.INDEPENDENT_128B_BLOCKS   = 1;

            PAL_ASSERT(m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE <= m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE);
        }
        else
        {
            // Verify that we only have DCC memory for shader writeable surfaces on GFX10
            PAL_ASSERT(createInfo.usageFlags.shaderWrite == 0);
        }
    }
    else
    {
        m_dccControl.bits.INDEPENDENT_64B_BLOCKS    = 0;

        // Note that MAX_UNCOMPRESSED_BLOCK_SIZE must >= MAX_COMPRESSED_BLOCK_SIZE
        // Set MAX_COMPRESSED_BLOCK_SIZE as big as possible for better compression ratio
        m_dccControl.bits.MAX_COMPRESSED_BLOCK_SIZE = m_dccControl.bits.MAX_UNCOMPRESSED_BLOCK_SIZE;
    }

    if (IsGfx10(gfxLevel) && m_image.Gfx10UseCompToSingleFastClears())
    {
        if (TestAnyFlagSet(settings.useCompToSingle, Gfx10DisableCompToReg))
        {
            // If the image was potentially fast-cleared to comp-to-single mode (i.e., no fast-clear-eliminate is
            // required), then we can't allow the CB / DCC to render in comp-to-reg mode because that *will* require
            // an FCE operation.
            //
            // The other option here is to not touch this bit at all and to allow the fast-clear-eliminate to proceed
            // anyway.  Because CB_DCC_CONTROL.DISABLE_ELIMFC_SKIP_OF_SINGLE=0, any DCC codes left at comp-to-
            // single will be skipped during the FCE and it will (theoretically) be a super-fast-fast-clear-eliminate.
            // Theoretically.
            m_dccControl.gfx09_1xPlus.DISABLE_CONSTANT_ENCODE_REG = 1;
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
    const Pal::Device*const pDevice      = pParent->GetDevice();
    const Gfx9PalSettings&  settings     = GetGfx9Settings(*pDevice);
    const auto              pPalSettings = pDevice->GetPublicSettings();

    // Assume that DCC is available; check for conditions where it won't work.
    bool useDcc         = true;
    bool mustDisableDcc = false;

    bool allMipsShaderWritable = pParent->IsShaderWritable();

    allMipsShaderWritable = (allMipsShaderWritable && (pParent->FirstShaderWritableMip() == 0));

    const bool isNotARenderTarget = (pParent->IsRenderTarget() == false);
    const bool isDepthStencil     = pParent->IsDepthStencil();
    const bool isGfx9             = (pDevice->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9);

    if (pParent->IsMetadataDisabledByClient())
    {
        // Don't use DCC if the caller asked that we allocate no metadata.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (pParent->GetDccFormatEncoding() == DccFormatEncoding::Incompatible)
    {
        // Don't use DCC if the caller can switch between view formats that are not DCC compatible with each other.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (isDepthStencil || ((isNotARenderTarget || allMipsShaderWritable) && isGfx9))
    {
        // DCC does not make sense for non-render targets or for UAVs (all mips are shader writeable) in
        // gfx9. It also does not make sense for any depth/stencil image because they have Htile data.
        useDcc = false;
        mustDisableDcc = true;
    }
    else if (isNotARenderTarget && (allMipsShaderWritable == false))
    {
        // DCC should always be off for a resource that is not a UAV and is not a render target.
        useDcc = false;
        mustDisableDcc = true;
    }
    // Msaa image with resolveSrc usage flag will go through shader based resolve if fixed function resolve is not
    // preferred, the image will be readable by a shader.
    else if ((pParent->IsShaderReadable() ||
              (pParent->IsResolveSrc() && (pParent->PreferCbResolve() == false))) &&
             (metaDataTexFetchSupported == false) &&
             (TestAnyFlagSet(settings.useDcc, Gfx9UseDccNonTcCompatShaderRead) == false))
    {
        // Disable DCC for shader read resource that cannot be made TC compat, this avoids DCC decompress
        // for RT->SR barrier.
        useDcc = false;
    }
    else if (pParent->IsShared() || pParent->IsPresentable() || pParent->IsFlippable())
    {
        // DCC is never available for shared, presentable, or flippable images.
        useDcc = false;
        mustDisableDcc = true;
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
        // Here we are either a color image or a UAV resource, so determine the swizzle mode. GFX9+ resources have
        // the same swizzle mode for all mip-levels and slices, so just look at the base level.
        const SubresId              subResId     = pParent->GetBaseSubResource();
        const SubResourceInfo*const pSubResInfo  = pParent->SubresourceInfo(subResId);
        const auto&                 surfSettings = image.GetAddrSettings(pSubResInfo);
        const AddrSwizzleMode       swizzleMode  = surfSettings.swizzleMode;

        if (IsGfx10(*pDevice))
        {
            //   1) SW_D does not support DCC.
            //   2) The restriction [on SW_S] is that you can't bind it to all CBs for a single draw.  If you want
            //      SW_S + DCC to be bound to the CB, it would have to go through a single CB (that is, change
            //      raster_config to route all traffic to 1 CB).  You could also use a SW compressor, and bind it
            //      to texture (say for static image)
            if (AddrMgr2::IsStandardSwzzle(swizzleMode) || AddrMgr2::IsDisplayableSwizzle(swizzleMode))
            {
                useDcc = false;
                mustDisableDcc = true;
                // The above check for "standard swizzle" is a bit of a misnomer; it's really a check for "S"
                // swizzle modes.  On GFX10, the "R" modes (render targets, not rotated) are expected to be the
                // most commonly used.  Still, throw an indication if we're faling into this code path with any
                // sort of frequency
                PAL_ALERT_ALWAYS();
            }
            else if (allMipsShaderWritable)
            {
                if (isNotARenderTarget)
                {
                    // If we are a UAV and not a render target, check our setting
                    useDcc = TestAnyFlagSet(settings.useDcc, Gfx10UseDccNonRenderTargetUav);
                }
                else
                {
                    // If we are a UAV and a render target, check our other setting.
                    useDcc = TestAnyFlagSet(settings.useDcc, Gfx10UseDccRenderTargetUav);
                }
            }
        }

        if (AddrMgr2::IsLinearSwizzleMode(swizzleMode))
        {
            // If the tile-mode is linear, then this surface has no chance of using DCC memory.
            useDcc = false;
            mustDisableDcc = true;
        }
        else
        {
            // Make sure the settings allow use of DCC surfaces for sRGB Images.
            if (Formats::IsSrgb(createInfo.swizzledFormat.format) &&
                (TestAnyFlagSet(settings.useDcc, Gfx9UseDccSrgb) == false))
            {
                useDcc = false;
            }
            else if (Formats::IsYuv(createInfo.swizzledFormat.format))
            {
                // DCC isn't useful for YUV formats, since those are usually accessed heavily by the multimedia engines.
                useDcc = false;
                mustDisableDcc = true;
            }
            else if ((createInfo.flags.prt == 1) && (TestAnyFlagSet(settings.useDcc, Gfx9UseDccPrt) == false))
            {
                // Make sure the settings allow use of DCC surfaces for PRT.
                useDcc = false;
            }
            else if (createInfo.samples > 1)
            {
                // Make sure the settings allow use of DCC surfaces for MSAA.
                if (createInfo.samples == 2)
                {
                    useDcc &= TestAnyFlagSet(settings.useDcc, Gfx9UseDccMultiSample2x);
                }
                else if (createInfo.samples == 4)
                {
                    useDcc &= TestAnyFlagSet(settings.useDcc, Gfx9UseDccMultiSample4x);
                }
                else if (createInfo.samples == 8)
                {
                    useDcc &= TestAnyFlagSet(settings.useDcc, Gfx9UseDccMultiSample8x);
                }

                if (createInfo.samples != createInfo.fragments)
                {
                    useDcc &= TestAnyFlagSet(settings.useDcc, Gfx9UseDccEqaa);
                }
            }
            else
            {
                // Make sure the settings allow use of DCC surfaces for single-sampled surfaces
                useDcc &= TestAnyFlagSet(settings.useDcc, Gfx9UseDccSingleSample);
            }

            if (useDcc && (TestAnyFlagSet(settings.useDcc, Gfx9UseDccForNonReadableFormats) == false))
            {
                // Ok, if we have a format that's renderable (or shader-writeable), but it's not readable, then this
                // will trigger a format replacement with RPM-based copy operations which in turn cause big headaches
                // with partial rectangles that paritally cover DCC tiles.  The exception is SRGB formats  as those
                // are copied via UINT format anyway.
                useDcc = image.ImageSupportsShaderReadsAndWrites();
            }

            // TODO: Re-evaulate the performance of DCC with multi-mip / multi-slice images on GFX9.  Clearing
            //       these is not a problem on GFX9 (it was on GFX8).
        }
    }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 496
    if ((mustDisableDcc == false) && (createInfo.metadataMode == MetadataMode::ForceEnabled))
    {
        useDcc = true;
    }
#endif

    return useDcc;
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
    const Image&            image,
    const Pal::SubresRange& clearRange,
    const uint32*           pConvertedColor,
    bool*                   pNeedFastClearElim) // [out] true if this surface will require a fast-clear-eliminate pass
                                                //       before it can be used as a texture
{
    // Fast-clear code that is valid for images that won't be texture fetched.
    const Pal::Image*   pParent             = image.Parent();
    Gfx9DccClearColor   clearCode           = Gfx9DccClearColor::ClearColorReg;
    const auto&         settings            = GetGfx9Settings(*image.Parent()->GetDevice());
    const Pal::SubresId baseSubResource     = { clearRange.startSubres.aspect,
                                                clearRange.startSubres.mipLevel,
                                                clearRange.startSubres.arraySlice };
    const SubResourceInfo*const pSubResInfo = pParent->SubresourceInfo(baseSubResource);

    if ((pSubResInfo->flags.supportMetaDataTexFetch != 0) && (settings.forceRegularClearCode == false))
    {
        // Surfaces that are fast cleared to one of the following colors may be texture fetched:
        //      1) ARGB(0, 0, 0, 0)
        //      2) ARGB(1, 0, 0, 0)
        //      3) ARGB(0, 1, 1, 1)
        //      4) ARGB(1, 1, 1, 1)
        //
        // If the clear-color is *not* one of those colors, then this routine will produce the "default"
        // clear-code.  The default clear-code is not understood by the TC and a fast-clear-eliminate pass must be
        // issued prior to using this surface as a texture.
        const ImageCreateInfo& createInfo    = pParent->GetImageCreateInfo();
        const uint32           numComponents = NumComponents(createInfo.swizzledFormat.format);
        const SurfaceSwap      surfSwap      = Formats::Gfx9::ColorCompSwap(createInfo.swizzledFormat);
        const ChannelSwizzle*  pSwizzle      = &createInfo.swizzledFormat.swizzle.swizzle[0];

        uint32                 color[4] = {};
        uint32                 ones[4]  = {};
        uint32                 cmpIdx   = 0;
        uint32                 rgbaIdx  = 0;

        switch(numComponents)
        {
        case 1:
            while ((pSwizzle[rgbaIdx] != ChannelSwizzle::X) && (rgbaIdx < 4))
            {
                rgbaIdx++;
            }

            PAL_ASSERT(pSwizzle[rgbaIdx] == ChannelSwizzle::X);

            color[0] =
            color[1] =
            color[2] =
            color[3] = pConvertedColor[rgbaIdx];

            ones[0] =
            ones[1] =
            ones[2] =
            ones[3] = image.TranslateClearCodeOneToNativeFmt(0);
            break;

        // Formats with two channels are special. Value from X channel represents color in clear code,
        // and value from Y channel represents alpha in clear code.
        case 2:
            color[0] =
            color[1] =
            color[2] = pConvertedColor[0];

            PAL_ASSERT(pSwizzle[0] >= ChannelSwizzle::X);

            cmpIdx  = static_cast<uint32>(pSwizzle[0]) - static_cast<uint32>(ChannelSwizzle::X);
            ones[0] =
            ones[1] =
            ones[2] = image.TranslateClearCodeOneToNativeFmt(cmpIdx);

            // In SWAP_STD case, clear color (in RGBA) has swizzle format of XY--. Clear code is RRRG.
            // In SWAP_STD_REV case, clear color (in RGBA) has swizzle format of YX--. Clear code is GGGR.
            if ((surfSwap == SWAP_STD) || (surfSwap == SWAP_STD_REV))
            {
                color[3] = pConvertedColor[1];

                PAL_ASSERT(pSwizzle[1] >= ChannelSwizzle::X);

                cmpIdx   = static_cast<uint32>(pSwizzle[1]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[3]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
            }
            // In SWAP_ALT case, clear color (in RGBA) has swizzle format of X--Y. Clear code is RRRA.
            // In SWAP_ALT_REV case, clear color (in RGBA) has swizzle format of Y--X. Clear code is AAAR.
            else if ((surfSwap == SWAP_ALT) || (surfSwap == SWAP_ALT_REV))
            {
                color[3] = pConvertedColor[3];

                PAL_ASSERT(pSwizzle[3] >= ChannelSwizzle::X);

                cmpIdx   = static_cast<uint32>(pSwizzle[3]) - static_cast<uint32>(ChannelSwizzle::X);
                ones[3]  = image.TranslateClearCodeOneToNativeFmt(cmpIdx);
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

        if ((color[0] == 0) &&
            (color[1] == 0) &&
            (color[2] == 0) &&
            (color[3] == 0))
        {
            clearCode = Gfx9DccClearColor::ClearColor0000;
        }
        else if (image.Parent()->GetDccFormatEncoding() == DccFormatEncoding::SignIndependent)
        {
            // cant allow special clear color code because the formats do not support DCC Constant
            // encoding. This happens when we mix signed and unsigned formats. There is no problem with
            // clearcolor0000.The issue is only seen when there is a 1 in any of the channels
            *pNeedFastClearElim = true;
        }
        else if ((color[0] == 0) &&
                 (color[1] == 0) &&
                 (color[2] == 0) &&
                 (color[3] == ones[3]))
        {
            clearCode = Gfx9DccClearColor::ClearColor0001;
        }
        else if ((color[0] == ones[0]) &&
                 (color[1] == ones[1]) &&
                 (color[2] == ones[2]) &&
                 (color[3] == 0))
        {
            clearCode = Gfx9DccClearColor::ClearColor1110;
        }
        else if ((color[0] == ones[0]) &&
                 (color[1] == ones[1]) &&
                 (color[2] == ones[2]) &&
                 (color[3] == ones[3]))
        {
            clearCode = Gfx9DccClearColor::ClearColor1111;
        }
        else
        {
            *pNeedFastClearElim = true;
        }
    }
    else
    {
        // Even though it won't be texture feched, it is still safer to unconditionally do FCE to guarantee the base
        // data is coherent with prior clears
        *pNeedFastClearElim = true;
    }

    if (IsGfx10(*pParent->GetDevice())                  &&
        (clearCode == Gfx9DccClearColor::ClearColorReg) &&
        image.Gfx10UseCompToSingleFastClears())
    {
        // If we're going to write the clear color into the image data during the fast-clear, then we need to
        // use a clear-code that indicates that's what we've done...  Otherwise the DCC block gets seriously
        // confused especially if the next rendering operation happens to render constant pixel data that
        // perfectly matches the clear color stored in the CB_COLORx_CLEAR_WORD* registers.
        clearCode = Gfx9DccClearColor::ClearColorCompToSingle;
    }

    return static_cast<uint8>(clearCode);
}

//=============== Implementation for Gfx9Cmask: ========================================================================
Gfx9Cmask::Gfx9Cmask(
    const Image&  image)
    :
    Gfx9MaskRam(image,
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

    // Only need the sub-res info for the aspect...
    const SubresId              subResId    = { ImageAspect::Color, 0, 0 };
    const SubResourceInfo*const pSubResInfo = pParent->SubresourceInfo(subResId);

    const auto*                     pFmask     = m_image.GetFmask();
    ADDR2_COMPUTE_CMASK_INFO_INPUT  cMaskInput = {};
    Result                          result     = Result::ErrorInitializationFailed;

    cMaskInput.size            = sizeof(cMaskInput);
    cMaskInput.unalignedWidth  = createInfo.extent.width;
    cMaskInput.unalignedHeight = createInfo.extent.height;
    cMaskInput.numSlices       = createInfo.arraySize;
    cMaskInput.resourceType    = m_image.GetAddrSettings(pSubResInfo).resourceType;
    cMaskInput.colorFlags      = pAddrMgr->DetermineSurfaceFlags(*pParent, subResId.aspect);
    cMaskInput.swizzleMode     = pFmask->GetSwizzleMode();
    cMaskInput.cMaskFlags      = GetMetaFlags(m_image);

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
    ImageAspect   aspect
    ) const
{
    const uint32 pipeBankXor = m_image.GetFmask()->GetPipeBankXor();

    return AdjustPipeBankXorForSwizzle(pipeBankXor);
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

        // The addressing equation is the same for all sub-resources, so only bother to calculate it once
        CalcMetaEquation();

        if (hasEqGpuAccess)
        {
            // Calculate info as to where the GPU can find the cMask equation
            InitEqGpuAccess(pGpuOffset);
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
    else
    {
        // Forcing CMask usage forces FMask usage, which is required for EQAA.
        useCmask = (pParent->IsEqaa() ||
                    (pParent->IsRenderTarget()                        &&
                     (pParent->IsShared()                   == false) &&
                     (pParent->IsMetadataDisabledByClient() == false) &&
                     (pParent->GetImageCreateInfo().samples > 1)));
    }

    return useCmask;
}

//=============== Implementation for Gfx9Fmask: ========================================================================

// =====================================================================================================================
Gfx9Fmask::Gfx9Fmask(
    const Image&  image)
    :
    MaskRam(),
    m_pipeBankXor(0),
    m_image(image)
{
    memset(&m_surfSettings, 0, sizeof(m_surfSettings));
    memset(&m_addrOutput,    0, sizeof(m_addrOutput));

    m_addrOutput.size   = sizeof(m_addrOutput);
    m_surfSettings.size = sizeof(m_surfSettings);
}

// =====================================================================================================================
// Determines the Image format used by SRD's which access an image's fMask allocation.
regSQ_IMG_RSRC_WORD1 Gfx9Fmask::Gfx9FmaskFormat(
    uint32  samples,
    uint32  fragments,
    bool    isUav       // Is the fmask being setup as a UAV
    ) const
{
    IMG_DATA_FORMAT dataFmt = IMG_DATA_FORMAT_8;
    uint32          numFmt  = IMG_NUM_FORMAT_UINT;

    if (isUav)
    {
        switch (m_addrOutput.bpp)
        {
        case 8:
            dataFmt = IMG_DATA_FORMAT_8;
            break;
        case 16:
            dataFmt = IMG_DATA_FORMAT_16;
            break;
        case 32:
            dataFmt = IMG_DATA_FORMAT_32;
            break;
        case 64:
            dataFmt = IMG_DATA_FORMAT_32_32;
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
        constexpr IMG_NUM_FORMAT_FMASK FMaskFormatTbl[4][4] =
        {
            // Two-sample formats
            { IMG_NUM_FORMAT_FMASK_8_2_1,         // One fragment
              IMG_NUM_FORMAT_FMASK_8_2_2, },      // Two fragments

            // Four-sample formats
            { IMG_NUM_FORMAT_FMASK_8_4_1,         // One fragment
              IMG_NUM_FORMAT_FMASK_8_4_2,         // Two fragments
              IMG_NUM_FORMAT_FMASK_8_4_4, },      // Four fragments

            // Eight-sample formats
            { IMG_NUM_FORMAT_FMASK_8_8_1,         // One fragment
              IMG_NUM_FORMAT_FMASK_16_8_2,        // Two fragments
              IMG_NUM_FORMAT_FMASK_32_8_4,        // Four fragments
              IMG_NUM_FORMAT_FMASK_32_8_8, },     // Eight fragments

            // Sixteen-sample formats
            { IMG_NUM_FORMAT_FMASK_16_16_1,       // One fragment
              IMG_NUM_FORMAT_FMASK_32_16_2,       // Two fragments
              IMG_NUM_FORMAT_FMASK_64_16_4,       // Four fragments
              IMG_NUM_FORMAT_FMASK_64_16_8, },    // Eight fragments
        };

        const uint32 log2Samples   = Log2(samples);
        const uint32 log2Fragments = Log2(fragments);

        PAL_ASSERT((log2Samples  >= 1) && (log2Samples <= 4));
        PAL_ASSERT(log2Fragments <= 3);

        numFmt = FMaskFormatTbl[log2Samples - 1][log2Fragments];

        dataFmt = IMG_DATA_FORMAT_FMASK__GFX09;
    }

    regSQ_IMG_RSRC_WORD1 word1 = {};
    word1.bits.DATA_FORMAT = dataFmt;
    word1.bits.NUM_FORMAT  = numFmt;

    return word1;
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
            imgFmt = IMG_FMT_32_32_UINT;
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
            { IMG_FMT_FMASK8_S2_F1,         // One fragment
              IMG_FMT_FMASK8_S2_F2, },      // Two fragments

            // Four-sample formats
            { IMG_FMT_FMASK8_S4_F1,         // One fragment
              IMG_FMT_FMASK8_S4_F2,         // Two fragments
              IMG_FMT_FMASK8_S4_F4, },      // Four fragments

            // Eight-sample formats
            { IMG_FMT_FMASK8_S8_F1,         // One fragment
              IMG_FMT_FMASK16_S8_F2,        // Two fragments
              IMG_FMT_FMASK32_S8_F4,        // Four fragments
              IMG_FMT_FMASK32_S8_F8, },     // Eight fragments

            // Sixteen-sample formats
            { IMG_FMT_FMASK16_S16_F1,       // One fragment
              IMG_FMT_FMASK32_S16_F2,       // Two fragments
              IMG_FMT_FMASK64_S16_F4,       // Four fragments
              IMG_FMT_FMASK64_S16_F8, },    // Eight fragments
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
    gpusize*  pGpuOffset)
{
    Result result = ComputeFmaskInfo();

    if (result == Result::Success)
    {
        // fMask surfaces have a pipe/bank xor value which is independent of the image's pipe-bank xor value.
        result = m_image.ComputePipeBankXor(ImageAspect::Fmask,
                                            &m_surfSettings,
                                            &m_pipeBankXor);
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
Result Gfx9Fmask::ComputeFmaskInfo()
{
    const Pal::Image&       parent   = *(m_image.Parent());
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

//=============== Some helper functions here ====================================

// =====================================================================================================================
static ADDR2_META_FLAGS GetMetaFlags(
    const Image&  image)
{
    const Pal::Image*const  pParent    = image.Parent();
    const Pal::Device*      pDevice    = pParent->GetDevice();
    const Device*           pGfxDevice = static_cast<Device*>(pDevice->GetGfxDevice());

    ADDR2_META_FLAGS  metaFlags = {};

    // Pipe aligned surfaces are aligned for optimal access from the texture block.  All our surfaces are texture
    // fetchable as anything can be copied through RPM.
    // For case MSAA Z/MSAA color/Stencil, metadata is not pipe aligned.
    metaFlags.pipeAligned = 1;

    //            rbAligned must be true for ASICs with > 1 RBs, otherwise there would be access violation
    //            between different RBs
    metaFlags.rbAligned   = ((pGfxDevice->GetNumRbsPerSeLog2() + pGfxDevice->GetNumShaderEnginesLog2()) != 0);

    return metaFlags;
}

} // Gfx9
} // Pal
