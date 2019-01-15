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

#include "palColorTargetView.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MaskRam.h"

namespace Pal
{

struct ColorTargetViewInternalCreateInfo;
struct GraphicsState;
struct ImageLayout;

namespace Gfx9
{

class CmdStream;
class CmdUtil;
class Device;

// =====================================================================================================================
// Gfx9+ HW-specific base implementation of the Pal::IColorTargetView interface
class ColorTargetView : public Pal::IColorTargetView
{
public:
    ColorTargetView(
        const Device*                            pDevice,
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo);

    virtual uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const = 0;

    void UpdateDccStateMetadata(
        CmdStream*    pCmdStream,
        ImageLayout   imageLayout) const;

    bool IsVaLocked() const { return m_flags.viewVaLocked; }
    bool WaitOnMetadataMipTail() const { return m_flags.waitOnMetadataMipTail; }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_subresource.mipLevel; }

    static uint32* WriteUpdateFastClearColor(
        uint32       slot,
        const uint32 color[4],
        CmdStream*   pCmdStream,
        uint32*      pCmdSpace);

    static uint32* HandleBoundTargetsChanged(
        const Device& device,
        uint32*       pCmdSpace);

protected:
    virtual ~ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    template <typename Pm4ImgType>
    void CommonBuildPm4Headers(Pm4ImgType* pPm4Img) const;

    template <typename Pm4ImgType, typename CbColorViewType>
    void InitCommonBufferView(
        const ColorTargetViewCreateInfo& createInfo,
        Pm4ImgType*                      pPm4Img,
        CbColorViewType*                 pCbColorView) const;

    template <typename FmtInfoType>
    regCB_COLOR0_INFO InitCommonCbColorInfo(
        const ColorTargetViewCreateInfo& createInfo,
        const FmtInfoType*               pFmtInfo) const;

    template <typename Pm4ImgType, typename CbColorViewType>
    void InitCommonImageView(
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo,
        const Extent3d&                          baseExtent,
        const Extent3d&                          extent,
        Pm4ImgType*                              pPm4Img,
        regCB_COLOR0_INFO*                       pCbColorInfo,
        CbColorViewType*                         pCbColorView) const;

    template <typename Pm4ImgType>
    void UpdateImageVa(Pm4ImgType* pPm4Img) const;

    union
    {
        struct
        {
            uint32 isBufferView          :  1; // Indicates that this is a buffer view instead of an image view. Note
                                               // that none of the metadata flags will be set if isBufferView is set.
            uint32 viewVaLocked          :  1; // Whether the view's VA range is locked and won't change. This will
                                               // always be set for buffer views.
            uint32 hasCmaskFmask         :  1; // set if the associated image contains fMask and cMask meta data
            uint32 hasDcc                :  1; // set if the associated image contains DCC meta data
            uint32 hasDccStateMetaData   :  1; // set if the associated image contains DCC state metadata.
            uint32 isDccDecompress       :  1; // Indicates if dcc metadata need to be set to decompress state.
            uint32 waitOnMetadataMipTail :  1; // Set if the CmdBindTargets should insert a stall when binding this
                                               // view object.
            uint32 reserved              : 25;
        };

        uint32 u32All;
    } m_flags;

    const Device*const  m_pDevice;
    const Image* const  m_pImage;

    SubresId  m_subresource;
    uint32    m_arraySize;

    ColorLayoutToState  m_layoutToState;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ColorTargetView);
};

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write a GcnColorTargetView to GFX9 hardware. The required
// register writes are grouped into sets based on sequential register addresses, so that we can minimize the amount
// of PM4 space needed by setting several regs in each packet.
struct Gfx9ColorTargetViewPm4Img
{
    PM4PFP_SET_CONTEXT_REG        hdrCbColorBase;
    regCB_COLOR0_BASE             cbColorBase;
    regCB_COLOR0_BASE_EXT         cbColorBaseExt;
    regCB_COLOR0_ATTRIB2          cbColorAttrib2;
    regCB_COLOR0_VIEW             cbColorView;

    PM4ME_CONTEXT_REG_RMW         cbColorInfo;

    PM4PFP_SET_CONTEXT_REG        hdrCbColorAttrib;
    regCB_COLOR0_ATTRIB           cbColorAttrib;
    regCB_COLOR0_DCC_CONTROL      cbColorDccControl;
    regCB_COLOR0_CMASK            cbColorCmask;
    regCB_COLOR0_CMASK_BASE_EXT   cbColorCmaskBaseExt;
    regCB_COLOR0_FMASK            cbColorFmask;
    regCB_COLOR0_FMASK_BASE_EXT   cbColorFmaskBaseExt;
    regCB_COLOR0_CLEAR_WORD0      cbColorClearWord0;
    regCB_COLOR0_CLEAR_WORD1      cbColorClearWord1;
    regCB_COLOR0_DCC_BASE         cbColorDccBase;
    regCB_COLOR0_DCC_BASE_EXT     cbColorDccBaseExt;

    PM4PFP_SET_CONTEXT_REG        hdrPaScGenericScissor;
    regPA_SC_GENERIC_SCISSOR_TL   paScGenericScissorTl;
    regPA_SC_GENERIC_SCISSOR_BR   paScGenericScissorBr;

    PM4PFP_SET_CONTEXT_REG        hdrCbMrtEpitch;
    regCB_MRT0_EPITCH             cbMrtEpitch;

    // PM4 load context regs packet to load the Image's fast-clear meta-data.  This must be the last packet in the
    // image because it is either absent or present depending on compression state.
    PM4PFP_LOAD_CONTEXT_REG_INDEX loadMetaDataIndex;

    // Command space needed for compressed and decomrpessed rendering, in DWORDs.  These fields must always be last
    // in the structure to not interfere w/ the actual commands contained within.
    size_t  spaceNeeded;
    size_t  spaceNeededDecompressed;
};

// =====================================================================================================================
// Gfx9 specific extension of the base Pal::Gfx9::ColorTargetView class
class Gfx9ColorTargetView : public ColorTargetView
{
public:
    Gfx9ColorTargetView(
        const Device*                            pDevice,
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo);

    virtual uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const;

protected:
    virtual ~Gfx9ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

private:
    void BuildPm4Headers();
    void InitRegisters(
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo);

    // Image of PM4 commands used to write this View to hardware for buffer views or for image views with full
    // compression enabled.
    Gfx9ColorTargetViewPm4Img  m_pm4Cmds;

    PAL_DISALLOW_COPY_AND_ASSIGN(Gfx9ColorTargetView);
};

} // Gfx9
} // Pal
