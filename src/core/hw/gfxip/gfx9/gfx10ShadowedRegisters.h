/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_PS - PERSISTENT_SPACE_START),
        1
    },
    {
        (mmSPI_SHADER_PGM_LO_PS - PERSISTENT_SPACE_START),
        (mmSPI_SHADER_USER_DATA_PS_31 - mmSPI_SHADER_PGM_LO_PS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_PS_0 + 1),
    },
    {
        (Gfx10::mmSPI_SHADER_PGM_CHKSUM_VS - PERSISTENT_SPACE_START),
        1
    },
    {
        (Gfx09_10::mmSPI_SHADER_LATE_ALLOC_VS - PERSISTENT_SPACE_START),
        (Gfx09_10::mmSPI_SHADER_USER_DATA_VS_31 - Gfx09_10::mmSPI_SHADER_LATE_ALLOC_VS + 1),
    },
    {
        (Gfx10::mmSPI_SHADER_USER_ACCUM_VS_0 - PERSISTENT_SPACE_START),
        (Gfx10::mmSPI_SHADER_USER_ACCUM_VS_3 - Gfx10::mmSPI_SHADER_USER_ACCUM_VS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_ES - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_ES - Gfx10Plus::mmSPI_SHADER_PGM_LO_ES + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_PGM_LO_LS - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_PGM_HI_LS - Gfx10Plus::mmSPI_SHADER_PGM_LO_LS + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_GS - PERSISTENT_SPACE_START),
        1,
    },
    {
        (mmSPI_SHADER_PGM_RSRC3_GS - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_GS_31 - mmSPI_SHADER_PGM_RSRC3_GS + 1),
    },
    {
        (mmSPI_SHADER_USER_DATA_ADDR_LO_GS - PERSISTENT_SPACE_START),
        (mmSPI_SHADER_USER_DATA_ADDR_HI_GS - mmSPI_SHADER_USER_DATA_ADDR_LO_GS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_ESGS_0 + 1),
    },
    {
        (Apu09_1xPlus::mmSPI_SHADER_PGM_CHKSUM_HS - PERSISTENT_SPACE_START),
        1,
    },
    {
        (mmSPI_SHADER_PGM_RSRC3_HS - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_USER_DATA_HS_31 - mmSPI_SHADER_PGM_RSRC3_HS + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 - PERSISTENT_SPACE_START),
        (Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_3 - Gfx10Plus::mmSPI_SHADER_USER_ACCUM_LSHS_0 + 1),
    },
    {
        (Gfx10Plus::mmSPI_SHADER_REQ_CTRL_PS - PERSISTENT_SPACE_START),
        1,
    },
    {
        (Gfx10::mmSPI_SHADER_REQ_CTRL_VS - PERSISTENT_SPACE_START),
        1,
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
