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

#pragma once

#include "palAssert.h"
#include "palColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"

namespace Pal
{

struct ColorTargetViewInternalCreateInfo;
struct GraphicsState;
struct ImageLayout;

namespace Gfx9
{

class CmdStream;
class Device;

// =====================================================================================================================
// Gfx9+ HW-specific base implementation of the Pal::IColorTargetView interface
class ColorTargetView : public Pal::IColorTargetView
{
public:
    ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        uint32                            uniqueId);

    ColorTargetView(const ColorTargetView&) = default;
    ColorTargetView& operator=(const ColorTargetView&) = default;
    ColorTargetView(ColorTargetView&&) = default;
    ColorTargetView& operator=(ColorTargetView&&) = default;

    virtual uint32* WriteCommands(
        uint32             slot,
        ImageLayout        imageLayout,
        CmdStream*         pCmdStream,
        uint32*            pCmdSpace,
        regCB_COLOR0_INFO* pCbColorInfo) const = 0;

    bool IsVaLocked() const { return m_flags.viewVaLocked; }

    virtual bool IsColorBigPage() const { return false; };
    virtual bool IsFmaskBigPage() const { return false; }
    virtual bool BypassMall()     const { return false; }

    virtual void GetImageSrd(const Device& device, void* pOut) const { PAL_NEVER_CALLED(); }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_subresource.mipLevel; }

    uint32* WriteUpdateFastClearColor(
        uint32       slot,
        const uint32 color[4],
        CmdStream*   pCmdStream,
        uint32*      pCmdSpace) const;

    static uint32* HandleBoundTargetsChanged(const CmdUtil& cmdUtil, uint32* pCmdSpace);

    Extent2d GetExtent() const { return m_extent; }

    bool HasDcc() const { return m_flags.hasDcc != 0; }

    bool HasMultipleFragments() const { return m_flags.hasMultipleFragments != 0; }

    // Mask of CB_COLORn_INFO bits owned by the ColorTargetView classes.
    static const uint32 CbColorInfoMask = static_cast<uint32>(~(CB_COLOR0_INFO__BLEND_OPT_DONT_RD_DST_MASK |
                                                                CB_COLOR0_INFO__BLEND_OPT_DISCARD_PIXEL_MASK));

     bool Equals(const ColorTargetView* pOther) const;

protected:
    virtual ~ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    template <typename RegistersType, typename CbColorViewType>
    void InitCommonBufferView(
        const Device&                    device,
        const ColorTargetViewCreateInfo& createInfo,
        RegistersType*                   pRegs,
        CbColorViewType*                 pCbColorView) const;

    template <typename FmtInfoType>
    regCB_COLOR0_INFO InitCbColorInfo(
        const Device&      device,
        const FmtInfoType* pFmtInfo) const;

    template <typename RegistersType, typename CbColorViewType>
    void InitCommonImageView(
        const Device&                     device,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        const Extent3d&                   baseExtent,
        RegistersType*                    pRegs,
        CbColorViewType*                  pCbColorView) const;

    template <typename RegistersType>
    void UpdateImageVa(RegistersType* pRegs) const;

    template <typename RegistersType>
    uint32* WriteCommandsCommon(
        uint32         slot,
        ImageLayout    imageLayout,
        CmdStream*     pCmdStream,
        uint32*        pCmdSpace,
        RegistersType* pRegs) const;

    void SetupExtents(
        const SubresId                   baseSubRes,
        const ColorTargetViewCreateInfo& createInfo,
        Extent3d*                        pBaseExtent,
        Extent3d*                        pExtent,
        bool*                            pModifiedYuvExtent) const;

    union
    {
        struct
        {
            uint32 isBufferView           :  1; // Indicates that this is a buffer view instead of an image view. Note
                                                // that none of the metadata flags will be set if isBufferView is set.
            uint32 viewVaLocked           :  1; // Whether the view's VA range is locked and won't change. This will
                                                // always be set for buffer views.
            uint32 hasCmaskFmask          :  1; // set if the associated image contains fMask and cMask meta data
            uint32 hasDcc                 :  1; // set if the associated image contains DCC meta data
            uint32 isDccDecompress        :  1; // Indicates if dcc metadata need to be set to decompress state.
            uint32 useSubresBaseAddr      :  1; // Indicates that this view's base address is subresource based.
            uint32 colorBigPage           :  1; // This view supports setting CB_RMI_GLC2_CACHE_CONTROL.COLOR_BIG_PAGE.
                                                // Only valid for buffer views or image views with viewVaLocked set.
            uint32 fmaskBigPage           :  1; // This view supports setting CB_RMI_GLC2_CACHE_CONTROL.FMASK_BIG_PAGE.
                                                // Only valid if viewVaLocked is set.
            uint32 hasMultipleFragments   :  1; // Is this view MSAA/EQAA?
            uint32 bypassMall             :  1; // Set to bypass the MALL for this surface.  Only meaningful on GPUs
                                                // which suppport the MALL (memory access last level cache).
            uint32 reserved               : 22;
        };

        uint32 u32All;
    } m_flags;

    const Image*        m_pImage;
    GfxIpLevel          m_gfxLevel;
    SubresId            m_subresource;
    uint32              m_arraySize;
    uint32              m_uniqueId;
    SwizzledFormat      m_swizzledFormat;
    Extent2d            m_extent;
    ColorLayoutToState  m_layoutToState;
};

// Set of context registers associated with a color-target view object.
struct Gfx10ColorTargetViewRegs
{
    regCB_COLOR0_BASE         cbColorBase;
    uint32                    cbColorPitch;  // meaningless register in GFX10 addressing mode
    uint32                    cbColorSlice;  // meaningless register in GFX10 addressing mode
    regCB_COLOR0_VIEW         cbColorView;
    regCB_COLOR0_INFO         cbColorInfo;
    regCB_COLOR0_ATTRIB       cbColorAttrib;
    regCB_COLOR0_DCC_CONTROL  cbColorDccControl;
    regCB_COLOR0_CMASK        cbColorCmask;
    uint32                    cbColorCmaskSlice;  // meaningless register in GFX10 addressing mode
    regCB_COLOR0_FMASK        cbColorFmask;
    regCB_COLOR0_DCC_BASE     cbColorDccBase;
    regCB_COLOR0_ATTRIB2      cbColorAttrib2;
    regCB_COLOR0_ATTRIB3      cbColorAttrib3;

    // This four registers are added to support PAL's high-address bits on gfx10 CB
    regCB_COLOR0_BASE_EXT       cbColorBaseExt;
    regCB_COLOR0_DCC_BASE_EXT   cbColorDccBaseExt;
    regCB_COLOR0_FMASK_BASE_EXT cbColorFmaskBaseExt;
    regCB_COLOR0_CMASK_BASE_EXT cbColorCmaskBaseExt;

    gpusize  fastClearMetadataGpuVa;
};

// =====================================================================================================================
// Gfx10 specific extension of the base Pal::Gfx9::ColorTargetView class
class Gfx10ColorTargetView final : public ColorTargetView
{
public:
    Gfx10ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        uint32                            uniqueId);

    Gfx10ColorTargetView(const Gfx10ColorTargetView&) = default;
    Gfx10ColorTargetView& operator=(const Gfx10ColorTargetView&) = default;
    Gfx10ColorTargetView(Gfx10ColorTargetView&&) = default;
    Gfx10ColorTargetView& operator=(Gfx10ColorTargetView&&) = default;

    virtual uint32* WriteCommands(
        uint32             slot,
        ImageLayout        imageLayout,
        CmdStream*         pCmdStream,
        uint32*            pCmdSpace,
        regCB_COLOR0_INFO* pCbColorInfo) const override;

    virtual bool IsColorBigPage() const override;
    virtual bool IsFmaskBigPage() const override;

    virtual bool BypassMall() const override { return m_flags.bypassMall; }

    virtual void GetImageSrd(const Device& device, void* pOut) const override;

protected:
    virtual ~Gfx10ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void InitRegisters(
        const Device&                     device,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo);

    void UpdateImageSrd(const Device& device, void* pOut) const;

    Gfx10ColorTargetViewRegs  m_regs;

    // The view as a cached SRD, for use with the UAV export opt.  This must be generated on-the-fly if the VA is not
    //  known in advance.
    ImageSrd  m_uavExportSrd;
};

// Set of context registers associated with a color-target view object.
struct Gfx11ColorTargetViewRegs
{
    regCB_COLOR0_BASE          cbColorBase;
    regCB_COLOR0_VIEW          cbColorView;
    regCB_COLOR0_INFO          cbColorInfo;
    regCB_COLOR0_ATTRIB        cbColorAttrib;
    regCB_COLOR0_DCC_CONTROL   cbColorDccControl;
    regCB_COLOR0_DCC_BASE      cbColorDccBase;
    regCB_COLOR0_ATTRIB2       cbColorAttrib2;
    regCB_COLOR0_ATTRIB3       cbColorAttrib3;

    // These two registers are added to support PAL's high-address bits on gfx11 CB
    regCB_COLOR0_BASE_EXT      cbColorBaseExt;
    regCB_COLOR0_DCC_BASE_EXT  cbColorDccBaseExt;

    gpusize  fastClearMetadataGpuVa;
};

static_assert(Util::CheckSequential({
        mmCB_COLOR0_VIEW, mmCB_COLOR0_INFO, mmCB_COLOR0_ATTRIB, mmCB_COLOR0_DCC_CONTROL,
    }), "The ordering of the Gfx11ColorTargetViewRegs changed!");

// =====================================================================================================================
// Gfx11 specific extension of the base Pal::Gfx9::ColorTargetView class
class Gfx11ColorTargetView final : public ColorTargetView
{
public:
    Gfx11ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        uint32                            uniqueId);

    Gfx11ColorTargetView(const Gfx11ColorTargetView&) = default;
    Gfx11ColorTargetView& operator=(const Gfx11ColorTargetView&) = default;
    Gfx11ColorTargetView(Gfx11ColorTargetView&&) = default;
    Gfx11ColorTargetView& operator=(Gfx11ColorTargetView&&) = default;

    virtual uint32* WriteCommands(
        uint32             slot,
        ImageLayout        imageLayout,
        CmdStream*         pCmdStream,
        uint32*            pCmdSpace,
        regCB_COLOR0_INFO* pCbColorInfo) const override;

    virtual bool IsColorBigPage() const override;

    virtual bool BypassMall() const override { return m_flags.bypassMall; }

    virtual void GetImageSrd(const Device& device, void* pOut) const override;

protected:
    virtual ~Gfx11ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void InitRegisters(
        const Device&                     device,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo);

    void UpdateImageSrd(const Device& device, void* pOut) const;

    Gfx11ColorTargetViewRegs  m_regs;

    // The view as a cached SRD, for use with the UAV export opt.  This must be generated on-the-fly if the VA is not
    //  known in advance.
    ImageSrd  m_uavExportSrd;
};

} // Gfx9
} // Pal
