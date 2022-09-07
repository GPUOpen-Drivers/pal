/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palColorTargetView.h"
#include "core/hw/gfxip/gfx6/gfx6Image.h"
#include "core/hw/gfxip/gfx6/gfx6MaskRam.h"
#include "core/hw/gfxip/pm4UniversalCmdBuffer.h"

namespace Pal
{

namespace Pm4
{
struct GraphicsState;
}

namespace Gfx6
{

class CmdUtil;
class Device;
class Image;

// Set of context registers associated with a color-target view object.
struct ColorTargetViewRegs
{
    regCB_COLOR0_BASE             cbColorBase;
    regCB_COLOR0_PITCH            cbColorPitch;
    regCB_COLOR0_SLICE            cbColorSlice;
    regCB_COLOR0_VIEW             cbColorView;
    regCB_COLOR0_INFO             cbColorInfo;
    regCB_COLOR0_ATTRIB           cbColorAttrib;
    regCB_COLOR0_DCC_CONTROL__VI  cbColorDccControl;
    regCB_COLOR0_CMASK            cbColorCmask;
    regCB_COLOR0_CMASK_SLICE      cbColorCmaskSlice;
    regCB_COLOR0_FMASK            cbColorFmask;
    regCB_COLOR0_FMASK_SLICE      cbColorFmaskSlice;
    regCB_COLOR0_DCC_BASE__VI     cbColorDccBase;

    gpusize  fastClearMetadataGpuVa;
    gpusize  dccStateMetadataGpuVa;
};

// =====================================================================================================================
// Gfx6 HW-specific implementation of the Pal::IColorTargetView interface
class ColorTargetView final : public Pal::IColorTargetView
{
public:
    ColorTargetView(
        const Device&                     device,
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo);

    static uint32* WriteUpdateFastClearColor(
        uint32       slot,
        const uint32 color[4],
        CmdStream*   pCmdStream,
        uint32*      pCmdSpace);

    uint32* WriteCommands(
        uint32      slot,
        ImageLayout imageLayout,
        CmdStream*  pCmdStream,
        uint32*     pCmdSpace) const;

    bool IsVaLocked() const { return m_flags.viewVaLocked; }

    const Image* GetImage() const { return m_pImage; }
    uint32 MipLevel() const { return m_subresource.mipLevel; }

    bool IsDccEnabled(ImageLayout imageLayout) const;

    Pm4::TargetExtent2d GetExtent() const { return m_extent; }

    bool IsRotatedSwizzleOverwriteCombinerDisabled() const { return m_flags.disableRotateSwizzleOC != 0; }

private:
    virtual ~ColorTargetView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL color target
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    void InitRegisters(const Device&                     device,
                       const ColorTargetViewCreateInfo&  createInfo,
                       ColorTargetViewInternalCreateInfo internalInfo);
    void UpdateImageVa(ColorTargetViewRegs* pRegs) const;

    union
    {
        struct
        {
            uint32 isBufferView           :  1; // Indicates that this is a buffer view instead of an image view. Note
                                                // that none of the metadata flags will be set if isBufferView is set.
            uint32 viewVaLocked           :  1; // Whether the view's VA range is locked and won't change. This will
                                                // always be set for buffer views.
            uint32 hasCmask               :  1;
            uint32 hasFmask               :  1;
            uint32 hasDcc                 :  1;
            uint32 hasDccStateMetaData    :  1;
            uint32 fastClearSupported     :  1; // Fast clears are supported using the CLEAR_COLOR registers.
            uint32 dccCompressionEnabled  :  1; // DCC can be disabled per-mip even if the image has DCC memory.
            uint32 isDccDecompress        :  1; // Set if this view is used for DCC decompress blits
            uint32 usesLoadRegIndexPkt    :  1; // Set if LOAD_CONTEXT_REG_INDEX is used instead of LOAD_CONTEXT_REG.
            uint32 isGfx7OrHigher         :  1;
            uint32 disableRotateSwizzleOC :  1; // Indicate that the for the assocaited image, whether the
                                                // Overwrite Combiner (OC) needs to be disabled or not.
            uint32 reserved               : 20;
        };

        uint32 u32All;
    } m_flags;

    // If this is an image view, these members give the bound image and its base subresource.
    const Image*        m_pImage;
    SubresId            m_subresource;
    Pm4::TargetExtent2d m_extent;

    ColorLayoutToState   m_layoutToState;
    ColorTargetViewRegs  m_regs;
    // Value of CB_COLOR_ATTRIB used when binding this target for non-compressed rendering.
    regCB_COLOR0_ATTRIB  m_cbColorAttribDecompressed;

    PAL_DISALLOW_COPY_AND_ASSIGN(ColorTargetView);
};

} // Gfx6
} // Pal
