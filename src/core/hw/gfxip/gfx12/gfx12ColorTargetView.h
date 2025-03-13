/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"
#include "palColorTargetView.h"

namespace Pal
{

struct ColorTargetViewInternalCreateInfo;

namespace Gfx12
{

class CmdStream;
class Device;

// =====================================================================================================================
// Gfx12 implementation of the Pal::IColorTargetView interface.
class ColorTargetView final : public Pal::IColorTargetView
{
public:
    ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        uint32                            viewId);

    ColorTargetView(const ColorTargetView&) = default;
    ColorTargetView& operator=(const ColorTargetView&) = default;
    ColorTargetView(ColorTargetView&&) = default;
    ColorTargetView& operator=(ColorTargetView&&) = default;

    Extent2d Extent() const;

    uint32* CopyRegPairsToCmdSpace(
        uint32             index,
        uint32*            pCmdSpace,
        bool*              pWriteCbDbHighBaseRegs,
        const Pal::Device& device) const;

    uint32 Log2NumFragments() const
        { return Regs::GetC<mmCB_COLOR0_ATTRIB, CB_COLOR0_ATTRIB>(m_regs).bits.NUM_FRAGMENTS; }
    Chip::ColorFormat Format() const
        { return Chip::ColorFormat(Regs::GetC<mmCB_COLOR0_INFO, CB_COLOR0_INFO>(m_regs).bits.FORMAT); }

    bool Equals(const ColorTargetView* pOther) const;

    const IImage* GetImage() const { return m_pImage; }

private:
    virtual ~ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    void BufferViewInit(
        const ColorTargetViewCreateInfo& createInfo,
        const Device*                    pDevice);
    void ImageViewInit(
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalCreateInfo);

    static constexpr uint32 Registers[] =
    {
        mmCB_COLOR0_BASE,
        mmCB_COLOR0_VIEW,
        mmCB_COLOR0_VIEW2,
        mmCB_COLOR0_ATTRIB,
        mmCB_COLOR0_FDCC_CONTROL,
        mmCB_COLOR0_INFO,
        mmCB_COLOR0_ATTRIB2,
        mmCB_COLOR0_ATTRIB3,
        mmCB_COLOR0_BASE_EXT,
    };
    using Regs = RegPairHandler<decltype(Registers), Registers>;

    static_assert(Regs::Size() == Regs::NumContext(), "Only Context regs expected.");

    RegisterValuePair m_regs[Regs::Size()];
    uint32            m_uniqueId;
    const IImage*     m_pImage; // Underlying Image for this view, nullptr if Buffer view.
};

}
}
