/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"
#include "core/hw/gfxip/universalCmdBuffer.h"

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
        ColorTargetViewInternalCreateInfo internalInfo);

    virtual uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const = 0;

    bool IsVaLocked() const { return m_flags.viewVaLocked; }
    bool WaitOnMetadataMipTail() const { return m_flags.waitOnMetadataMipTail; }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_subresource.mipLevel; }

    static uint32* WriteUpdateFastClearColor(
        uint32       slot,
        const uint32 color[4],
        CmdStream*   pCmdStream,
        uint32*      pCmdSpace);

    static uint32* HandleBoundTargetsChanged(uint32* pCmdSpace);

    TargetExtent2d GetExtent() const { return m_extent; }

    bool IsRotatedSwizzleOverwriteCombinerDisabled() const { return m_flags.disableRotateSwizzleOC != 0; }

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
            uint32 waitOnMetadataMipTail  :  1; // Set if the CmdBindTargets should insert a stall when binding this
                                                // view object.
            uint32 useSubresBaseAddr      :  1; // Indicates that this view's base address is subresource based.
            uint32 disableRotateSwizzleOC :  1; // Indicate that the for the assocaited image, whether the
                                                // Overwrite Combiner (OC) needs to be disabled
            uint32 colorBigPage           :  1; // This view supports setting CB_RMI_GLC2_CACHE_CONTROL.COLOR_BIG_PAGE.
                                                // Only valid for buffer views or image views with viewVaLocked set.
            uint32 fmaskBigPage           :  1; // This view supports setting CB_RMI_GLC2_CACHE_CONTROL.FMASK_BIG_PAGE.
                                                // Only valid if viewVaLocked is set.
            uint32 placeholder1           :  1;
            uint32 reserved               : 21;
        };

        uint32 u32All;
    } m_flags;

    const Image* const  m_pImage;
    SubresId            m_subresource;
    uint32              m_arraySize;
    SwizzledFormat      m_swizzledFormat;
    TargetExtent2d      m_extent;
    ColorLayoutToState  m_layoutToState;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorTargetView);
};

// Set of context registers associated with a color-target view object.
struct Gfx9ColorTargetViewRegs
{
    regCB_COLOR0_BASE           cbColorBase;
    regCB_COLOR0_BASE_EXT       cbColorBaseExt;
    regCB_COLOR0_ATTRIB2        cbColorAttrib2;
    regCB_COLOR0_VIEW           cbColorView;
    regCB_COLOR0_INFO           cbColorInfo;
    regCB_COLOR0_ATTRIB         cbColorAttrib;
    regCB_COLOR0_DCC_CONTROL    cbColorDccControl;
    regCB_COLOR0_CMASK          cbColorCmask;
    regCB_COLOR0_CMASK_BASE_EXT cbColorCmaskBaseExt;
    regCB_COLOR0_FMASK          cbColorFmask;
    regCB_COLOR0_FMASK_BASE_EXT cbColorFmaskBaseExt;
    regCB_COLOR0_DCC_BASE       cbColorDccBase;
    regCB_COLOR0_DCC_BASE_EXT   cbColorDccBaseExt;
    regCB_MRT0_EPITCH           cbMrtEpitch;

    gpusize  fastClearMetadataGpuVa;
};

// =====================================================================================================================
// Gfx9 specific extension of the base Pal::Gfx9::ColorTargetView class
class Gfx9ColorTargetView : public ColorTargetView
{
public:
    Gfx9ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo);

    virtual uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const override;

protected:
    virtual ~Gfx9ColorTargetView()
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

    Gfx9ColorTargetViewRegs  m_regs;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9ColorTargetView);
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

    gpusize  fastClearMetadataGpuVa;
};

// =====================================================================================================================
// Gfx10 specific extension of the base Pal::Gfx9::ColorTargetView class
class Gfx10ColorTargetView : public ColorTargetView
{
public:
    Gfx10ColorTargetView(
        const Device*                     pDevice,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo);

    virtual uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const override;

    bool IsColorBigPage() const;
    bool IsFmaskBigPage() const;

    void GetImageSrd(const Device& device, void* pOut) const;

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

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx10ColorTargetView);
};

} // Gfx9
} // Pal
