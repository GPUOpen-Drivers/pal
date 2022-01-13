/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palInlineFuncs.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx10ShadowedRegisters_nv10.h"
#include "core/hw/gfxip/gfx9/gfx10ShadowedRegisters_gfx103.h"

namespace Pal
{
namespace Gfx9
{

// Defines the set of ranges of GFX SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx10ShShadowRange[] =
{
    // mmSPI_SHADER_PGM_RSRC4_PS left out as it's written via SET_SH_REG_INDEX. // 0x2C01
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS - PERSISTENT_SPACE_START),    // 0x2C06 - 0x2C06
        1
    },
    // mmSPI_SHADER_PGM_RSRC3_PS left out as it's written via SET_SH_REG_INDEX. // 0x2C07
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),                      // 0x2C08 - 0x2C2B
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_REQ_CTRL_PS - PERSISTENT_SPACE_START),         // 0x2C30 - 0x2C30
        1,
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 - PERSISTENT_SPACE_START),     // 0x2C32 - 0x2C35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 + 1),
    },
    // mmSPI_SHADER_PGM_RSRC4_VS left out as it's written via SET_SH_REG_INDEX. // 0x2C41
    {
        (Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS - PERSISTENT_SPACE_START),           // 0x2C45 - 0x2C45
        1
    },
    // mmSPI_SHADER_PGM_RSRC3_VS left out as it's written via SET_SH_REG_INDEX. // 0x2C46
    {
        (HasHwVs::mmSPI_SHADER_LATE_ALLOC_VS - PERSISTENT_SPACE_START),        // 0x2C47 - 0x2C6B
        (HasHwVs::mmSPI_SHADER_USER_DATA_VS_31 - HasHwVs::mmSPI_SHADER_LATE_ALLOC_VS + 1),
    },
    {
        (Gfx10::mmSPI_SHADER_REQ_CTRL_VS - PERSISTENT_SPACE_START),             // 0x2C70 - 0x2C70
        1,
    },
    {
        (Gfx10::mmSPI_SHADER_USER_ACCUM_VS_0 - PERSISTENT_SPACE_START),         // 0x2C72 - 0x2C75
        (Gfx10::mmSPI_SHADER_USER_ACCUM_VS_3 - Gfx10::mmSPI_SHADER_USER_ACCUM_VS_0 + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS - PERSISTENT_SPACE_START),    // 0x2C80 - 0x2C80
        1,
    },
    // mmSPI_SHADER_PGM_RSRC4_GS left out as it's written via SET_SH_REG_INDEX. // 0x2C81
    // mmSPI_SHADER_PGM_RSRC3_GS left out as it's written via SET_SH_REG_INDEX. // 0x2C87
    {
        (mmSPI_SHADER_PGM_LO_GS - PERSISTENT_SPACE_START),                      // 0x2C88 - 0x2CAB
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_GS_31 - mmSPI_SHADER_PGM_LO_GS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 - PERSISTENT_SPACE_START),   // 0x2CB2 - 0x2CB5
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_ES - PERSISTENT_SPACE_START),           // 0x2CC8 - 0x2CC9
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_ES - Gfx10Plus::mmSPI_SHADER_PGM_LO_ES + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS - PERSISTENT_SPACE_START),    // 0x2D00 - 0x2D00
        1,
    },
    // mmSPI_SHADER_PGM_RSRC4_HS left out as it's written via SET_SH_REG_INDEX. // 0x2D01
    // mmSPI_SHADER_PGM_RSRC3_HS left out as it's written via SET_SH_REG_INDEX. // 0x2D07
    {
        (mmSPI_SHADER_PGM_LO_HS - PERSISTENT_SPACE_START),                      // 0x2D08 - 0x2D2B
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_HS_31 - mmSPI_SHADER_PGM_LO_HS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 - PERSISTENT_SPACE_START),   // 0x2D32 - 0x2D35
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_LS - PERSISTENT_SPACE_START),           // 0x2D48 - 0x2D49
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_LS - Gfx10Plus::mmSPI_SHADER_PGM_LO_LS + 1),
    },
};

// Defines the set of ranges of CS SH registers we shadow when mid command buffer preemption is enabled.
const RegisterRange Gfx10CsShShadowRange[] =
{
    {
        (mmCOMPUTE_START_X - PERSISTENT_SPACE_START),                           // 0x2E04 - 0x2E09
        (mmCOMPUTE_NUM_THREAD_Z - mmCOMPUTE_START_X + 1),
    },
    {
        (mmCOMPUTE_PERFCOUNT_ENABLE - PERSISTENT_SPACE_START),                  // 0x2E0B - 0x2EOD
        (mmCOMPUTE_PGM_HI - mmCOMPUTE_PERFCOUNT_ENABLE + 1),
    },
    {
        (mmCOMPUTE_PGM_RSRC1 - PERSISTENT_SPACE_START),                         // 0x2E12 - 0x2E13
        (mmCOMPUTE_PGM_RSRC2 - mmCOMPUTE_PGM_RSRC1 + 1),
    },
    {
        (mmCOMPUTE_RESOURCE_LIMITS - PERSISTENT_SPACE_START),                   // 0x2E15
        1,
    },
    {
        (mmCOMPUTE_TMPRING_SIZE - PERSISTENT_SPACE_START),                      // 0x2E18
        1,
    },
    {
        (mmCOMPUTE_THREAD_TRACE_ENABLE - PERSISTENT_SPACE_START),               // 0x2E1E
        1,
    },
    {
        (Gfx10Plus::mmCOMPUTE_USER_ACCUM_0 - PERSISTENT_SPACE_START),               // 0x2E24 - 0x2E28
        (Gfx10Plus::mmCOMPUTE_PGM_RSRC3 - Gfx10Plus::mmCOMPUTE_USER_ACCUM_0 + 1),
    },
    {
        (Gfx10Plus::mmCOMPUTE_SHADER_CHKSUM - PERSISTENT_SPACE_START),              // 0x2E2A
        1
    },
    {
        (mmCOMPUTE_USER_DATA_0 - PERSISTENT_SPACE_START),                       // 0x2E40 - 0x2E4F
        (mmCOMPUTE_USER_DATA_15 - mmCOMPUTE_USER_DATA_0 + 1),
    },
    {
        (Gfx10Plus::mmCOMPUTE_DISPATCH_TUNNEL - PERSISTENT_SPACE_START),            // 0x2E7D
        1,
    },
};

} // Gfx9
} // Pal
