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

#include "palDepthStencilView.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{

struct DepthStencilViewInternalCreateInfo;

namespace Gfx12
{

class CmdStream;
class Device;
class Image;

// =====================================================================================================================
// Gfx12 implementation of the Pal::IDepthStencilView interface.
class DepthStencilView final : public Pal::IDepthStencilView
{
public:
    DepthStencilView(
        const Device*                      pDevice,
        const DepthStencilViewCreateInfo&  createInfo,
        DepthStencilViewInternalCreateInfo internalInfo,
        uint32                             viewId);

    DepthStencilView(const DepthStencilView&) = default;
    DepthStencilView& operator=(const DepthStencilView&) = default;
    DepthStencilView(DepthStencilView&&) = default;
    DepthStencilView& operator=(DepthStencilView&&) = default;

    uint32* CopyRegPairsToCmdSpace(
        ImageLayout depthLayout,
        ImageLayout stencilLayout,
        uint32*     pCmdSpace,
        bool*       pWriteCbDbHighBaseRegs) const;
    static uint32* CopyNullRegPairsToCmdSpace(uint32* pCmdSpace, bool writeMinimumRegSet);

    Extent2d Extent() const;

    bool SZValid() const { return m_flags.szValid; }
    bool ZReadOnly() const { return DsRegs::GetC<mmDB_DEPTH_VIEW1, DB_DEPTH_VIEW1>(m_regs).bits.Z_READ_ONLY; }
    bool SReadOnly() const { return DsRegs::GetC<mmDB_DEPTH_VIEW1, DB_DEPTH_VIEW1>(m_regs).bits.STENCIL_READ_ONLY; }
    uint32 NumSamples() const { return DsRegs::GetC<mmDB_Z_INFO, DB_Z_INFO>(m_regs).bits.NUM_SAMPLES; }
    bool HiSZEnabled() const { return m_flags.hiSZEnabled; }

    DB_Z_INFO          DbZInfo()          const { return DsRegs::GetC<mmDB_Z_INFO, DB_Z_INFO>(m_regs); }
    DB_STENCIL_INFO    DbStencilInfo()    const { return DsRegs::GetC<mmDB_STENCIL_INFO, DB_STENCIL_INFO>(m_regs); }
    DB_DEPTH_VIEW1     DbDepthView1()     const { return DsRegs::GetC<mmDB_DEPTH_VIEW1, DB_DEPTH_VIEW1>(m_regs); }
    DB_RENDER_CONTROL  DbRenderControl()  const { return DsRegs::GetC<mmDB_RENDER_CONTROL, DB_RENDER_CONTROL>(m_regs); }
    DB_RENDER_OVERRIDE DbRenderOverride() const
        { return DsRegs::GetC<mmDB_RENDER_OVERRIDE, DB_RENDER_OVERRIDE>(m_regs); }

    bool Equals(const DepthStencilView* pOther) const;

    uint32 OverrideHiZHiSEnable(
        bool              enable,
        DB_SHADER_CONTROL dbShaderControl,
        bool              noForceReZ,
        uint32*           pCmdSpace) const;

    const Image* GetImage() const { return m_pImage; }
    SubresRange ViewRange() const { return m_viewRange; }

protected:
    virtual ~DepthStencilView()
    {
        // This destructor, and the destructors of all member and base classes, must always be empty: PAL depth stencil
        // views guarantee to the client that they do not have to be explicitly destroyed.
        PAL_NEVER_CALLED();
    }

    static constexpr uint32 Registers[] =
    {
        mmDB_RENDER_CONTROL,
        mmDB_DEPTH_VIEW,
        mmDB_DEPTH_VIEW1,
        mmDB_RENDER_OVERRIDE,
        mmDB_RENDER_OVERRIDE2,
        mmDB_DEPTH_SIZE_XY,
        mmDB_Z_INFO,
        mmDB_STENCIL_INFO,
        mmPA_SC_HIZ_INFO,
        mmPA_SC_HIS_INFO,
        mmPA_SC_HIZ_BASE,
        mmPA_SC_HIZ_SIZE_XY,
        mmPA_SC_HIS_BASE,
        mmPA_SC_HISZ_RENDER_OVERRIDE,
        mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
        mmDB_Z_WRITE_BASE,
        mmDB_Z_READ_BASE,
        mmDB_STENCIL_WRITE_BASE,
        mmDB_STENCIL_READ_BASE,
        mmDB_GL1_INTERFACE_CONTROL,
        mmPA_SC_HIS_SIZE_XY,

        // High address bits
        mmDB_Z_READ_BASE_HI,
        mmDB_STENCIL_READ_BASE_HI,
        mmDB_Z_WRITE_BASE_HI,
        mmDB_STENCIL_WRITE_BASE_HI,
        mmPA_SC_HIS_BASE_EXT,
        mmPA_SC_HIZ_BASE_EXT,
    };
    using DsRegs = RegPairHandler<decltype(Registers), Registers>;

    static_assert(DsRegs::Size() == DsRegs::NumContext(), "Only context registers expected!");

    RegisterValuePair m_regs[DsRegs::Size()];

    static constexpr uint32 NullStateRegisters[] =
    {
        mmDB_Z_INFO,
        mmDB_STENCIL_INFO,
        mmPA_SC_HIZ_INFO,
        mmPA_SC_HIS_INFO,
        mmDB_RENDER_CONTROL,
        mmDB_RENDER_OVERRIDE,
        mmDB_RENDER_OVERRIDE2,
        mmPA_SC_HISZ_RENDER_OVERRIDE,
        mmPA_SU_POLY_OFFSET_DB_FMT_CNTL,
    };

    using NullDsRegs = RegPairHandler<decltype(NullStateRegisters), NullStateRegisters>;

    static_assert(NullDsRegs::Size() == NullDsRegs::NumContext(), "Only context regs expected.");

    template<class RegType>
    static uint32* CopyNullRegPairsToCmdSpaceInternal(uint32* pCmdSpace);

    struct
    {
        uint32 szValid                :  1;
        uint32 hiSZEnabled            :  1; // If HiZ or HiS is enabled.
        uint32 hasNonZeroHighBaseBits :  1; // Does this DSV have non-zero bits in any high addresses?
        uint32 reserved               : 29;
    } m_flags;

    ImageLayout m_hizValidLayout;
    ImageLayout m_hisValidLayout;

    uint32       m_uniqueId;
    const Image* m_pImage;
    SubresRange  m_viewRange;

private:
};

} // namespace Gfx12
} // namespace Pal;
