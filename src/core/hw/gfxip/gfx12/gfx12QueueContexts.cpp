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

#include "core/cmdAllocator.h"
#include "core/device.h"
#include "core/queue.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12QueueContexts.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRing.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"

#include "palDequeImpl.h"
#include "palInlineFuncs.h"
#include "palVectorImpl.h"

using namespace Pal::Gfx12::Chip;
using namespace Util;
using namespace Util::Literals;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
uint32* WriteStaticComputeRegisters(
    const Pal::Device& device,
    uint32*            pCmdSpace)
{
    constexpr RegisterValuePair StaticRegs[] =
    {
        { mmCOMPUTE_PGM_HI - PERSISTENT_SPACE_START,                 0x00000000 },
        { mmCOMPUTE_DISPATCH_PKT_ADDR_LO - PERSISTENT_SPACE_START,   0x00000000 },
        { mmCOMPUTE_DISPATCH_PKT_ADDR_HI - PERSISTENT_SPACE_START,   0x00000000 },
        { mmCOMPUTE_USER_ACCUM_0 - PERSISTENT_SPACE_START,           0x00000000 },
        { mmCOMPUTE_USER_ACCUM_1 - PERSISTENT_SPACE_START,           0x00000000 },
        { mmCOMPUTE_USER_ACCUM_2 - PERSISTENT_SPACE_START,           0x00000000 },
        { mmCOMPUTE_USER_ACCUM_3 - PERSISTENT_SPACE_START,           0x00000000 },
        { mmCOMPUTE_DISPATCH_TUNNEL - PERSISTENT_SPACE_START,        0x00000000 },
    };

    pCmdSpace = CmdStream::WriteSetShPairs<ShaderCompute>(&StaticRegs[0], uint32(ArrayLen(StaticRegs)), pCmdSpace);

    const Device* pGfxDevice       = static_cast<Device*>(device.GetGfxDevice());
    const uint32  cuLimitMask      = pGfxDevice->Settings().csCuEnLimitMask;
    const uint16  cuEnableMask     = pGfxDevice->GetCuEnableMask(0, cuLimitMask);
    const uint32  numShaderEngines = device.ChipProperties().gfx9.numShaderEngines;

    COMPUTE_STATIC_THREAD_MGMT_SE0 computeStaticThreadMgmtPerSe = { };
    computeStaticThreadMgmtPerSe.bits.SA0_CU_EN = cuEnableMask;
    computeStaticThreadMgmtPerSe.bits.SA1_CU_EN = cuEnableMask;

    const uint32 masksPerSe[] =
    {
        computeStaticThreadMgmtPerSe.u32All,
        ((numShaderEngines >= 2) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 3) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 4) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 5) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 6) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 7) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 8) ? computeStaticThreadMgmtPerSe.u32All : 0),
        ((numShaderEngines >= 9) ? computeStaticThreadMgmtPerSe.u32All : 0),
    };

    static_assert(Util::CheckSequential({ mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
                                          mmCOMPUTE_STATIC_THREAD_MGMT_SE1, }),
                  "ComputeStaticThreadMgmtSe registers are not sequential!");

    static_assert(Util::CheckSequential({ mmCOMPUTE_STATIC_THREAD_MGMT_SE2,
                                          mmCOMPUTE_STATIC_THREAD_MGMT_SE3, }),
                  "ComputeStaticThreadMgmtSe registers are not sequential!");

    static_assert(Util::CheckSequential({ mmCOMPUTE_STATIC_THREAD_MGMT_SE4,
                                          mmCOMPUTE_STATIC_THREAD_MGMT_SE5,
                                          mmCOMPUTE_STATIC_THREAD_MGMT_SE6,
                                          mmCOMPUTE_STATIC_THREAD_MGMT_SE7, }),
                  "ComputeStaticThreadMgmtSe registers are not sequential!");

    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderCompute>(mmCOMPUTE_STATIC_THREAD_MGMT_SE0,
                                                                 mmCOMPUTE_STATIC_THREAD_MGMT_SE1,
                                                                 &masksPerSe[0],
                                                                 index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                 pCmdSpace);
    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderCompute>(mmCOMPUTE_STATIC_THREAD_MGMT_SE2,
                                                                 mmCOMPUTE_STATIC_THREAD_MGMT_SE3,
                                                                 &masksPerSe[2],
                                                                 index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                 pCmdSpace);
    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderCompute>(mmCOMPUTE_STATIC_THREAD_MGMT_SE4,
                                                                 mmCOMPUTE_STATIC_THREAD_MGMT_SE7,
                                                                 &masksPerSe[4],
                                                                 index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                 pCmdSpace);
    pCmdSpace = CmdStream::WriteSetOneShRegIndex<ShaderCompute>(mmCOMPUTE_STATIC_THREAD_MGMT_SE8,
                                                                masksPerSe[8],
                                                                index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                pCmdSpace);

    StartingPerfcounterState perfctrBehavior = pGfxDevice->CoreSettings().startingPerfcounterState;
    if (perfctrBehavior != StartingPerfcounterStateUntouched)
    {
        // If SPM interval spans across gfx and ace, we need to manually set COMPUTE_PERFCOUNT_ENABLE for the pipes.
        // But if not using SPM/counters, we want to have the hardware not count our workload (could affect perf)
        // By default, set it based on if GpuProfiler or DevDriver are active.
        regCOMPUTE_PERFCOUNT_ENABLE computeEnable = {};
        computeEnable.bits.PERFCOUNT_ENABLE = uint32(pGfxDevice->Parent()->EnablePerfCountersInPreamble());
        pCmdSpace = CmdStream::WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PERFCOUNT_ENABLE,
                                                               computeEnable.u32All,
                                                               pCmdSpace);
    }
    return pCmdSpace;
}

constexpr uint32 GfxShRegisters[] =
{
    mmSPI_SHADER_PGM_HI_LS,
    mmSPI_SHADER_PGM_HI_ES,
    mmSPI_SHADER_PGM_HI_PS,
    mmSPI_SHADER_REQ_CTRL_PS,
    mmSPI_SHADER_USER_ACCUM_LSHS_0,
    mmSPI_SHADER_USER_ACCUM_LSHS_1,
    mmSPI_SHADER_USER_ACCUM_LSHS_2,
    mmSPI_SHADER_USER_ACCUM_LSHS_3,
    mmSPI_SHADER_USER_ACCUM_ESGS_0,
    mmSPI_SHADER_USER_ACCUM_ESGS_1,
    mmSPI_SHADER_USER_ACCUM_ESGS_2,
    mmSPI_SHADER_USER_ACCUM_ESGS_3,
    mmSPI_SHADER_USER_ACCUM_PS_0,
    mmSPI_SHADER_USER_ACCUM_PS_1,
    mmSPI_SHADER_USER_ACCUM_PS_2,
    mmSPI_SHADER_USER_ACCUM_PS_3,
};

constexpr uint32 ContextRegisters[] =
{
    mmPA_SU_LINE_STIPPLE_SCALE,
    mmVGT_TESS_DISTRIBUTION,
    mmPA_SU_SMALL_PRIM_FILTER_CNTL,
    mmPA_SC_SCREEN_SCISSOR_TL,
    mmPA_SC_SCREEN_SCISSOR_BR,
    mmPA_SC_NGG_MODE_CNTL,
    mmDB_HTILE_SURFACE,
    mmSX_PS_DOWNCONVERT_CONTROL,
    mmPA_SC_EDGERULE,
    mmPA_CL_POINT_X_RAD,
    mmPA_CL_POINT_Y_RAD,
    mmPA_CL_POINT_SIZE,
    mmPA_CL_POINT_CULL_RAD,
    mmPA_CL_NANINF_CNTL,
    mmPA_SU_PRIM_FILTER_CNTL,
    mmPA_SU_OVER_RASTERIZATION_CNTL,
    mmPA_SC_CLIPRECT_RULE,
    mmPA_SC_BINNER_CNTL_1,
    mmPA_SC_BINNER_CNTL_2,
    mmDB_MEM_TEMPORAL,
    mmSC_MEM_TEMPORAL,
    mmPA_SC_TILE_STEERING_OVERRIDE,
};

constexpr uint32 GfxUConfigRegs[] =
{
    mmGE_GS_ORDERED_ID_BASE,
    mmVGT_PRIMITIVEID_RESET,
    mmGE_USER_VGPR_EN,
    mmGE_MAX_VTX_INDX,
    mmGE_MIN_VTX_INDX,
    mmGE_INDX_OFFSET,
    mmGE_GS_THROTTLE,
    mmSPI_GS_THROTTLE_CNTL1,
    mmSPI_GS_THROTTLE_CNTL2,
    mmSPI_GRP_LAUNCH_GUARANTEE_ENABLE,
    mmSPI_GRP_LAUNCH_GUARANTEE_CTRL,
};

// =====================================================================================================================
uint32* WriteStaticGraphicsRegisters(
    uint32*                  pCmdSpace,
    const Pal::Device* const pDevice)
{
    const PalPublicSettings* pPublicSettings = pDevice->GetPublicSettings();
    const Gfx12PalSettings&  gfx12Settings   = GetGfx12Settings(pDevice);

    // GFX SH Registers
    {
        using Regs = RegPairHandler<decltype(GfxShRegisters), GfxShRegisters>;
        static_assert(Regs::Size() == Regs::NumSh(), "Unexpected registers found!");

        RegisterValuePair regs[Regs::Size()];
        Regs::Init(regs);

        auto* pSpiShaderReqCtrlPs = Regs::Get<mmSPI_SHADER_REQ_CTRL_PS, SPI_SHADER_REQ_CTRL_PS>(regs);
        pSpiShaderReqCtrlPs->bits.SOFT_GROUPING_EN          = 1;
        pSpiShaderReqCtrlPs->bits.NUMBER_OF_REQUESTS_PER_CU = 3;

        pCmdSpace = CmdStream::WriteSetShPairs(regs, Regs::Size(), pCmdSpace);
    }

    // GFX Context Registers
    {
        using Regs = RegPairHandler<decltype(ContextRegisters), ContextRegisters>;
        static_assert(Regs::Size() == Regs::NumContext(), "Unexpected registers found!");

        RegisterValuePair regs[Regs::Size()];
        Regs::Init(regs);

        Regs::Get<mmPA_SU_LINE_STIPPLE_SCALE, PA_SU_LINE_STIPPLE_SCALE>(regs)->f32All = 1.0f;

        auto* pVgtTessDistribution = Regs::Get<mmVGT_TESS_DISTRIBUTION, VGT_TESS_DISTRIBUTION>(regs);
        pVgtTessDistribution->bits.ACCUM_ISOLINE = 128;
        pVgtTessDistribution->bits.ACCUM_TRI     = 128;
        pVgtTessDistribution->bits.ACCUM_QUAD    = 128;
        pVgtTessDistribution->bits.DONUT_SPLIT   = 24;
        pVgtTessDistribution->bits.TRAP_SPLIT    = 6;

        auto* pPaSuSmallPrimFilterCntl = Regs::Get<mmPA_SU_SMALL_PRIM_FILTER_CNTL, PA_SU_SMALL_PRIM_FILTER_CNTL>(regs);
        pPaSuSmallPrimFilterCntl->bits.SMALL_PRIM_FILTER_ENABLE     = 1;
        pPaSuSmallPrimFilterCntl->bits.SC_1XMSAA_COMPATIBLE_DISABLE = 1;

        auto* pPaScScreenScissorBr = Regs::Get<mmPA_SC_SCREEN_SCISSOR_BR, PA_SC_SCREEN_SCISSOR_BR>(regs);
        pPaScScreenScissorBr->bits.BR_X = USHRT_MAX;
        pPaScScreenScissorBr->bits.BR_Y = USHRT_MAX;

        Regs::Get<mmPA_SC_NGG_MODE_CNTL, PA_SC_NGG_MODE_CNTL>(regs)->bits.MAX_DEALLOCS_IN_WAVE = 64;

        auto* pSxPsDownconvertControl = Regs::Get<mmSX_PS_DOWNCONVERT_CONTROL, SX_PS_DOWNCONVERT_CONTROL>(regs);
        pSxPsDownconvertControl->bits.MRT0_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT1_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT2_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT3_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT4_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT5_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT6_FMT_MAPPING_DISABLE = 1;
        pSxPsDownconvertControl->bits.MRT7_FMT_MAPPING_DISABLE = 1;

        Regs::Get<mmPA_SC_EDGERULE, PA_SC_EDGERULE>(regs)->u32All = 0xAA99AAAA;

        Regs::Get<mmPA_SC_CLIPRECT_RULE, PA_SC_CLIPRECT_RULE>(regs)->bits.CLIP_RULE = USHRT_MAX;

        auto* pPaScBinnerCntl1 = Regs::Get<mmPA_SC_BINNER_CNTL_1, PA_SC_BINNER_CNTL_1>(regs);
        pPaScBinnerCntl1->bits.MAX_ALLOC_COUNT    = 254;
        // On gfx12, HW limits max 512 primitives per batch.
        constexpr uint32 BinningMaxPrimPerBatch = 512;
        PAL_ASSERT(pPublicSettings->binningMaxPrimPerBatch <= BinningMaxPrimPerBatch);
        const uint32 maxPrimPerBatch = Util::Min(pPublicSettings->binningMaxPrimPerBatch, BinningMaxPrimPerBatch);
        pPaScBinnerCntl1->bits.MAX_PRIM_PER_BATCH = (maxPrimPerBatch > 0) ? (maxPrimPerBatch - 1) : 0;

        auto* pPaScBinnerCntl2 = Regs::Get<mmPA_SC_BINNER_CNTL_2, PA_SC_BINNER_CNTL_2>(regs);
        pPaScBinnerCntl2->bits.ENABLE_PING_PONG_BIN_ORDER = gfx12Settings.enablePbbPingPongBinOrder;

        auto* pDbMemTemporal = Regs::Get<mmDB_MEM_TEMPORAL, DB_MEM_TEMPORAL>(regs);
        pDbMemTemporal->bits.Z_TEMPORAL_READ        = gfx12Settings.gfx12TemporalHintsZRead;
        pDbMemTemporal->bits.Z_TEMPORAL_WRITE       = gfx12Settings.gfx12TemporalHintsZWrite;
        pDbMemTemporal->bits.STENCIL_TEMPORAL_READ  = gfx12Settings.gfx12TemporalHintsSRead;
        pDbMemTemporal->bits.STENCIL_TEMPORAL_WRITE = gfx12Settings.gfx12TemporalHintsSWrite;

        auto* pScMemTemporal = Regs::Get<mmSC_MEM_TEMPORAL, SC_MEM_TEMPORAL>(regs);
        pScMemTemporal->bits.VRS_TEMPORAL_READ  = uint32(MemoryLoadTemporalHint::Rt);
        pScMemTemporal->bits.VRS_TEMPORAL_WRITE = uint32(MemoryStoreTemporalHint::Rt);
        pScMemTemporal->bits.HIZ_TEMPORAL_READ  = uint32(MemoryLoadTemporalHint::Rt);
        pScMemTemporal->bits.HIZ_TEMPORAL_WRITE = uint32(MemoryStoreTemporalHint::Rt);
        pScMemTemporal->bits.HIS_TEMPORAL_READ  = uint32(MemoryLoadTemporalHint::Rt);
        pScMemTemporal->bits.HIS_TEMPORAL_WRITE = uint32(MemoryStoreTemporalHint::Rt);

        pCmdSpace = CmdStream::WriteSetContextPairs(&regs[0], Regs::Size(), pCmdSpace);
    }

    // GFX UConfig Regs
    {
        using Regs = RegPairHandler<decltype(GfxUConfigRegs), GfxUConfigRegs>;
        static_assert(Regs::Size() == Regs::NumOther(), "Unexpected registers found!");

        RegisterValuePair regs[Regs::Size()];
        Regs::Init(regs);

        Regs::Get<mmGE_MAX_VTX_INDX, GE_MAX_VTX_INDX>(regs)->bits.MAX_INDX = UINT32_MAX;

        Regs::Get<mmGE_GS_THROTTLE, GE_GS_THROTTLE>(regs)->u32All = gfx12Settings.gfx12GeGsThrottle;

        Regs::Get<mmSPI_GS_THROTTLE_CNTL1, SPI_GS_THROTTLE_CNTL1>(regs)->u32All = gfx12Settings.gfx12SpiGsThrottleCntl1;
        Regs::Get<mmSPI_GS_THROTTLE_CNTL2, SPI_GS_THROTTLE_CNTL2>(regs)->u32All = gfx12Settings.gfx12SpiGsThrottleCntl2;

        Regs::Get<mmSPI_GRP_LAUNCH_GUARANTEE_ENABLE, SPI_GRP_LAUNCH_GUARANTEE_ENABLE>(regs)->u32All =
            gfx12Settings.gfx12SpiGrpLaunchGuaranteeEnable;
        Regs::Get<mmSPI_GRP_LAUNCH_GUARANTEE_CTRL, SPI_GRP_LAUNCH_GUARANTEE_CTRL>(regs)->u32All =
            gfx12Settings.gfx12SpiGrpLaunchGuaranteeCtrl;

        auto* pSpiGrpLaunchGuaranteeEnable =
            Regs::Get<mmSPI_GRP_LAUNCH_GUARANTEE_ENABLE, SPI_GRP_LAUNCH_GUARANTEE_ENABLE>(regs);

        constexpr SPI_GRP_LAUNCH_GUARANTEE_ENABLE expectedSpiGrpLaunchGuaranteeEnable =
            { .bits = {.ENABLE           = 1,
                       .HS_ASSIST_EN     = 0,
                       .GS_ASSIST_EN     = 1,
                       .MRT_ASSIST_EN    = 1,
                       .GFX_NUM_LOCK_WGP = 2,
                       .CS_NUM_LOCK_WGP  = 2,
                       .LOCK_PERIOD      = 1,
                       .LOCK_MAINT_COUNT = 1} };
        PAL_ALERT(pSpiGrpLaunchGuaranteeEnable->u32All != expectedSpiGrpLaunchGuaranteeEnable.u32All);

        // Workaround for HW bug requires that static state be programmed a specific way.
        // Since we're already programming it to 0, add this assert in case someone accidentally adds it later.
        PAL_ASSERT((pSpiGrpLaunchGuaranteeEnable->bits.CS_GLG_DISABLE == 0) ||
                   (gfx12Settings.waCsGlgDisableOff == false));

        auto* pSpiGrpLaunchGuaranteeCtrl =
            Regs::Get<mmSPI_GRP_LAUNCH_GUARANTEE_CTRL, SPI_GRP_LAUNCH_GUARANTEE_CTRL>(regs);

        constexpr SPI_GRP_LAUNCH_GUARANTEE_CTRL expectedSpiGrpLaunchGuaranteeCtrl =
            { .bits = {.NUM_MRT_THRESHOLD       = 3,
                       .GFX_PENDING_THRESHOLD   = 4,
                       .PRIORITY_LOST_THRESHOLD = 4,
                       .ALLOC_SUCCESS_THRESHOLD = 4,
                       .CS_WAVE_THRESHOLD_HIGH  = 8} };
        PAL_ALERT(pSpiGrpLaunchGuaranteeCtrl->u32All != expectedSpiGrpLaunchGuaranteeCtrl.u32All);

        pCmdSpace = CmdStream::WriteSetUConfigPairs(regs, Regs::Size(), pCmdSpace);
    }

    // GFX SH Regs via SetShRegIndex packet.
    SPI_SHADER_PGM_RSRC3_HS spiShaderPgmRsrc3Hs = {};
    SPI_SHADER_PGM_RSRC3_GS spiShaderPgmRsrc3Gs = {};
    SPI_SHADER_PGM_RSRC3_PS spiShaderPgmRsrc3Ps = {};

    const uint32 gsCuEnMask = gfx12Settings.waScpcBackPressure ?
                              0xfffffdfd :
                              (SPI_SHADER_PGM_RSRC3_GS__CU_EN_MASK >> SPI_SHADER_PGM_RSRC3_GS__CU_EN__SHIFT);

    spiShaderPgmRsrc3Hs.bits.CU_EN = gfx12Settings.hsCuEnLimitMask;
    spiShaderPgmRsrc3Gs.bits.CU_EN = gfx12Settings.gsCuEnLimitMask & gsCuEnMask;
    spiShaderPgmRsrc3Ps.bits.CU_EN = gfx12Settings.psCuEnLimitMask;

    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderGraphics>(mmSPI_SHADER_PGM_RSRC3_HS, mmSPI_SHADER_PGM_RSRC3_HS,
                                                                  &(spiShaderPgmRsrc3Hs.u32All),
                                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                  pCmdSpace);
    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderGraphics>(mmSPI_SHADER_PGM_RSRC3_GS, mmSPI_SHADER_PGM_RSRC3_GS,
                                                                  &(spiShaderPgmRsrc3Gs.u32All),
                                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                  pCmdSpace);
    pCmdSpace = CmdStream::WriteSetSeqShRegsIndex<ShaderGraphics>(mmSPI_SHADER_PGM_RSRC3_PS, mmSPI_SHADER_PGM_RSRC3_PS,
                                                                  &(spiShaderPgmRsrc3Ps.u32All),
                                                                  index__pfp_set_sh_reg_index__apply_kmd_cu_and_mask,
                                                                  pCmdSpace);

    const Device* pGfxDevice = static_cast<Device*>(pDevice->GetGfxDevice());
    StartingPerfcounterState perfctrBehavior = pGfxDevice->CoreSettings().startingPerfcounterState;
    if (perfctrBehavior != StartingPerfcounterStateUntouched)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(PERFCOUNTER_START,
                                                       EngineTypeUniversal,
                                                       pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
QueueContext::QueueContext(
    Device*    pDevice,
    EngineType engineType)
    :
    Pal::QueueContext(pDevice->Parent()),
    m_device(*pDevice),
    m_queueContextUpdateCounter(0),
    m_queueContextUpdateCounterTmz(0),
    m_currentStackSizeDw(0),
    m_perSubmitPreambleCmdStream(*pDevice,
                                 pDevice->Parent()->InternalUntrackedCmdAllocator(),
                                 engineType,
                                 SubEngineType::Primary,
                                 CmdStreamUsage::Preamble,
                                 false),
    m_perSubmitPostambleCmdStream(*pDevice,
                                  pDevice->Parent()->InternalUntrackedCmdAllocator(),
                                  engineType,
                                  SubEngineType::Primary,
                                  CmdStreamUsage::Postamble,
                                  false),
    m_sharedInternalCmdStream(*pDevice,
                              pDevice->Parent()->InternalUntrackedCmdAllocator(),
                              engineType,
                              SubEngineType::Primary,
                              CmdStreamUsage::Preamble,
                              false),
    m_deferCmdStreamChunks(pDevice->GetPlatform()),
    m_engineType(engineType),
    m_executeIndirectMemAce(),
    m_executeIndirectMemGfx()
{
    PAL_ASSERT((engineType == EngineTypeCompute) || (engineType == EngineTypeUniversal));

}

// =====================================================================================================================
QueueContext::~QueueContext()
{

}

// =====================================================================================================================
Result QueueContext::Init()
{
    const Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    Result result = CreateTimestampMem(false);

    if (result == Result::Success)
    {
        result = m_perSubmitPreambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        result = m_perSubmitPostambleCmdStream.Init();
    }

    if (result == Result::Success)
    {
        result = m_sharedInternalCmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
// Initialize object that require queue to be finished with Init
Result QueueContext::LateInit()
{
    return RecordPrePostAmbleCmdStreams();
}

// =====================================================================================================================
// Allocate a Buffer in GpuMemory to store the ExecuteIndirect V2 PM4 commands.
Result QueueContext::AllocateExecuteIndirectBuffer(
    BoundGpuMemory* pExecuteIndirectMem)
{
    // Global SpillTable for Firmware to store data for a large number of Cmds (Draws/Dispatches) MaxCmdsInFlight
    // represents an approximation for how many Cmd's data can be stored assuming 1KB data (could be more or less in
    // practice) per Cmd.
    constexpr uint32 MaxCmdsInFlight  = 1_KiB;
    constexpr uint32 HwWaPadding      = 32;   // 32kb padding for a HW workaround.
    constexpr uint32 AllocSizeInBytes = (MaxCmdsInFlight + HwWaPadding) * 1024;

    GpuMemoryCreateInfo createInfo = {};
    createInfo.vaRange    = VaRange::DescriptorTable;
    createInfo.alignment  = EiSpillTblStrideAlignmentBytes;
    createInfo.size       = AllocSizeInBytes;
    createInfo.priority   = GpuMemPriority::Normal;
    createInfo.heapAccess = GpuHeapAccess::GpuHeapAccessCpuNoAccess;

    GpuMemoryInternalCreateInfo internalCreateInfo = {};
    internalCreateInfo.flags.alwaysResident = 1;

    GpuMemory* pMemObj   = nullptr;
    gpusize    memOffset = 0;

    Result result = m_pDevice->MemMgr()->AllocateGpuMem(createInfo,
                                                        internalCreateInfo,
                                                        false,
                                                        &pMemObj,
                                                        &memOffset);

    if (result == Result::Success)
    {
        pExecuteIndirectMem->Update(pMemObj, memOffset);
    }

    return result;
}

// =====================================================================================================================
void QueueContext::ResetCommandStream(
    CmdStream*          pCmdStream,
    QueueDeferFreeList* pList,
    uint32*             pIndex,
    uint64              lastTimeStamp)
{
    // pIndex should always be less than the number of CmdStreams in the Context (see QueueCmdStreamNum)
    PAL_ASSERT(*pIndex < ArrayLen(pList->pChunk));

    if (lastTimeStamp == 0)
    {
        // the very first submission the Queue.
        pCmdStream->Reset(nullptr, true);
    }
    else
    {
        pCmdStream->Reset(nullptr, false);

        Pal::ChunkRefList deferList(m_pDevice->GetPlatform());
        Result result = pCmdStream->TransferRetainedChunks(&deferList);

        // PushBack used in TransferRetainedChunks should never fail,
        // since here only require at most 3 entries,
        // and by default the Vector used in ChunkRefList has 16 entried
        PAL_ASSERT(result == Result::Success);

        // the command streams in the queue context should only have 1 chunk each.
        PAL_ASSERT(deferList.NumElements() <= 1);
        if (deferList.NumElements() == 1)
        {
            deferList.PopBack(&pList->pChunk[*pIndex]);
            *pIndex = *pIndex + 1;
        }
    }
}

// =====================================================================================================================
// Adds memory owned by a command stream to a deferred free list after which the CmdStream can
// safely be deleted or reused.
void QueueContext::ReleaseCmdStreamMemory(
    CmdStream* pCmdStream)
{
    const uint64 lastTimestamp = m_pParentQueue->GetSubmissionContext()->LastTimestamp();

    uint32 chunkIdx = 0;
    QueueDeferFreeList deferFreeChunkList;
    for (uint32 i = 0; i < QueueCmdStreamNum; ++i)
    {
        deferFreeChunkList.pChunk[i] = nullptr;
    }
    deferFreeChunkList.timestamp = lastTimestamp;
    ResetCommandStream(pCmdStream, &deferFreeChunkList, &chunkIdx, lastTimestamp);

    if (chunkIdx > 0)
    {
        Pal::Result result = m_deferCmdStreamChunks.PushBack(deferFreeChunkList);
        PAL_ASSERT(result == Pal::Result::Success);
    }
}

// =====================================================================================================================
void QueueContext::ClearDeferredMemory()
{
    // Note: this function is non-virtual but derived queue contexts override

    PAL_ASSERT(m_pParentQueue != nullptr);
    SubmissionContext* pSubContext = m_pParentQueue->GetSubmissionContext();

    if (pSubContext != nullptr)
    {
        ChunkRefList chunksToReturn(m_pDevice->GetPlatform());

        for (uint32 i = 0; i < m_deferCmdStreamChunks.NumElements(); i++)
        {
            const QueueDeferFreeList& item = m_deferCmdStreamChunks.Front();
            if (pSubContext->IsTimestampRetired(item.timestamp) == false)
            {
                // Any timestamp in the list more recent than this must also still be in-flight, so end the search.
                break;
            }

            QueueDeferFreeList list = {};
            m_deferCmdStreamChunks.PopFront(&list);

            for (uint32 idx = 0; idx < Util::ArrayLen(list.pChunk); ++idx)
            {
                if (list.pChunk[idx] != nullptr)
                {
                    chunksToReturn.PushBack(list.pChunk[idx]);
                }
            }
        }

        // Now return the chunks to command allocator
        if (chunksToReturn.IsEmpty() == false)
        {
            m_pDevice->InternalUntrackedCmdAllocator()->ReuseChunks(
                CommandDataAlloc, false, chunksToReturn.Begin());
        }
    }
}

// =====================================================================================================================
uint32* QueueContext::WritePerSubmitPreambleCmds(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{

    return pCmdSpace;
};

// =====================================================================================================================
uint32* QueueContext::WritePerSubmitPostambleCmds(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    // When the pipeline has emptied, write the timestamp back to zero so that the next submission can execute.
    // We also use this pipelined event to flush and invalidate the L2 and shader vector L0 caches.
    ReleaseMemGeneric releaseInfo = {};
    // CACHE_FLUSH_AND_INV_TS_EVENT flushes/invalidates CB/DB caches which doesn't exist on compute queue.
    releaseInfo.vgtEvent = (pCmdStream->GetEngineType() == EngineTypeUniversal) ? CACHE_FLUSH_AND_INV_TS_EVENT
                                                                                : BOTTOM_OF_PIPE_TS;
    releaseInfo.dstAddr  = m_exclusiveExecTs.GpuVirtAddr();
    releaseInfo.dataSel  = data_sel__me_release_mem__send_32_bit_low;
    releaseInfo.data     = 0;

    releaseInfo.cacheSync.gl2Inv = 1;
    releaseInfo.cacheSync.gl2Wb  = 1;
    releaseInfo.cacheSync.glvInv = 1;

    pCmdSpace += m_device.CmdUtil().BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
uint32* QueueContext::WriteInitialSubmitPreambleCmds(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{

    return pCmdSpace;
}

// =====================================================================================================================
uint32* QueueContext::WriteFinalSubmitPostambleCmds(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{

    return pCmdSpace;
}

// =====================================================================================================================
// Records the per submit pre and post amble cmd streams
Result QueueContext::RecordPrePostAmbleCmdStreams()
{
    Result result = Result::Success;

    // Initialize the per-submit preamble
    if (result == Result::Success)
    {
        result = m_perSubmitPreambleCmdStream.Begin({}, nullptr);

        if (result == Result::Success)
        {
            uint32* pCmdSpace = m_perSubmitPreambleCmdStream.ReserveCommands();

            pCmdSpace = WritePerSubmitPreambleCmds(&m_perSubmitPreambleCmdStream, pCmdSpace);

            m_perSubmitPreambleCmdStream.CommitCommands(pCmdSpace);

            result = m_perSubmitPreambleCmdStream.End();
        }
    }

    // Initialize the per-submit postamble which will follow every client submission that don't need queue context
    // updates.
    if (result == Result::Success)
    {
        result = m_perSubmitPostambleCmdStream.Begin({}, nullptr);

        if (result == Result::Success)
        {
            uint32* pCmdSpace = m_perSubmitPostambleCmdStream.ReserveCommands();

            pCmdSpace = WritePerSubmitPostambleCmds(&m_perSubmitPostambleCmdStream, pCmdSpace);

            m_perSubmitPostambleCmdStream.CommitCommands(pCmdSpace);

            result = m_perSubmitPostambleCmdStream.End();

            m_perSubmitPostambleCmdStream.EnableDropIfSameContext(false);
        }
    }

    return result;
}

// =====================================================================================================================
// Processes the initial submit for a queue. Returns Success if the processing was required and needs to be submitted.
// Returns Unsupported otherwise.
Result QueueContext::ProcessInitialSubmit(
    InternalSubmitInfo* pSubmitInfo)
{
    const uint64 lastTimestamp = m_pParentQueue->GetSubmissionContext()->LastTimestamp();

    uint32 chunkIdx = 0;
    QueueDeferFreeList deferFreeChunkList { .timestamp = lastTimestamp };

    ResetCommandStream(&m_sharedInternalCmdStream, &deferFreeChunkList, &chunkIdx, lastTimestamp);

    Result result = m_sharedInternalCmdStream.Begin({}, nullptr);

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_sharedInternalCmdStream.ReserveCommands();

        pCmdSpace = WriteInitialSubmitPreambleCmds(&m_sharedInternalCmdStream, pCmdSpace);

        m_sharedInternalCmdStream.CommitCommands(pCmdSpace);

        result = m_sharedInternalCmdStream.End();
    }

    return result;
}

// =====================================================================================================================
// Processes the final submit for a queue. Returns Success if the processing was required and needs to be submitted.
// Returns Unsupported otherwise.
Result QueueContext::ProcessFinalSubmit(
    InternalSubmitInfo* pSubmitInfo)
{
    Result result = Result::Unsupported;

    return result;
}

// =====================================================================================================================
UniversalQueueContext::UniversalQueueContext(
    Device* pDevice)
    :
    QueueContext(pDevice, EngineTypeUniversal),
    m_ringSet(pDevice, false),
    m_tmzRingSet(pDevice, true),
    m_cmdsUseTmzRing(false),
    m_firstSubmit(true),
    m_supportsAceGang(pDevice->Parent()->EngineProperties().perEngine[EngineTypeCompute].numAvailable != 0),
    m_pAcePostambleCmdStream(nullptr),
    m_pAcePreambleCmdStream(nullptr)
{
}

// =====================================================================================================================
UniversalQueueContext::~UniversalQueueContext()
{
    PAL_SAFE_DELETE(m_pAcePreambleCmdStream, m_pDevice->GetPlatform());
    PAL_SAFE_DELETE(m_pAcePostambleCmdStream, m_pDevice->GetPlatform());

    if (m_executeIndirectMemGfx.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_executeIndirectMemGfx.Memory(), m_executeIndirectMemGfx.Offset());
        m_executeIndirectMemGfx.Update(nullptr, 0);
    }
    if (m_executeIndirectMemAce.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_executeIndirectMemAce.Memory(), m_executeIndirectMemAce.Offset());
        m_executeIndirectMemAce.Update(nullptr, 0);
    }
}

// =====================================================================================================================
Result UniversalQueueContext::Init()
{
    Result result = QueueContext::Init();

    if (result == Result::Success)
    {
        result = m_ringSet.Init();
    }

    if (result == Result::Success)
    {
        result = m_tmzRingSet.Init();
    }

    return result;
}

// =====================================================================================================================
// Called before each submit to give the QueueContext and opportunity to specify preamble/postamble command streams
// that should be submitted along with the client command buffers.
Result UniversalQueueContext::PreProcessSubmit(
    InternalSubmitInfo*      pSubmitInfo,
    uint32                   cmdBufferCount,
    const ICmdBuffer* const* ppCmdBuffers)
{
    const Gfx12PalSettings& gfx12Settings = GetGfx12Settings(m_pDevice);
    const uint64 lastTimestamp            = m_pParentQueue->GetSubmissionContext()->LastTimestamp();
    Result result                         = Result::Success;
    bool updatePerContextState            = false;
    const bool hasAce                     = pSubmitInfo->implicitGangedSubQueues > 0;
    bool hasInitAce                       = false;

    if (hasAce && (m_pAcePreambleCmdStream == nullptr) && m_supportsAceGang)
    {
        hasInitAce = true;
    }

    uint32 chunkIdx = 0;
    QueueDeferFreeList deferFreeChunkList { .timestamp = lastTimestamp };

    if (result == Result::Success)
    {
        // We only need to rebuild the command stream if the user submits at least one command buffer.
        if ((cmdBufferCount != 0) || (m_firstSubmit == true))
        {
            const bool isTmz = (pSubmitInfo->flags.isTmzEnabled != 0);

            // Check if anything has happened since the last submit on this queue that requires a new shader ring set.
            //   If we do need to update the shader ring set, update the queue context preamble and submit it.
            result = UpdatePerContextDependencies(&updatePerContextState,
                                                  isTmz,
                                                  pSubmitInfo->stackSizeInDwords,
                                                  lastTimestamp,
                                                  cmdBufferCount,
                                                  ppCmdBuffers,
                                                  hasAce,
                                                  hasInitAce);

            // The first submit always needs to send the queue context preamble. We expect UpdateRingSet will always
            // report the ring set was updated on the first submit.
            PAL_ASSERT(updatePerContextState || (m_firstSubmit == false));

            const bool cmdStreamResetNeeded = (updatePerContextState || (m_cmdsUseTmzRing != isTmz) || hasInitAce);

            if ((result == Result::Success) && cmdStreamResetNeeded)
            {
                ResetCommandStream(&m_perSubmitPreambleCmdStream, &deferFreeChunkList, &chunkIdx, lastTimestamp);

                if (hasAce && (m_pAcePreambleCmdStream != nullptr))
                {
                    ResetCommandStream(m_pAcePreambleCmdStream, &deferFreeChunkList, &chunkIdx, lastTimestamp);
                }

                result = RebuildPerSubmitPreambleCmdStream(isTmz, hasAce);
                m_cmdsUseTmzRing = isTmz;
            }
        }
    }

    if (result == Result::Success)
    {
        uint32 preambleCount  = 0;

        pSubmitInfo->pPreambleCmdStream[preambleCount++] = &m_perSubmitPreambleCmdStream;

        if ((m_pAcePreambleCmdStream != nullptr) && hasAce)
        {
            pSubmitInfo->pPreambleCmdStream[preambleCount++] = m_pAcePreambleCmdStream;
        }

        pSubmitInfo->numPreambleCmdStreams = preambleCount;

        uint32 postambleCount = 0;

        pSubmitInfo->pPostambleCmdStream[postambleCount++] = &m_perSubmitPostambleCmdStream;

        pSubmitInfo->numPostambleCmdStreams = postambleCount;

        pSubmitInfo->pagingFence = m_pDevice->InternalUntrackedCmdAllocator()->LastPagingFence();
    }

    if (chunkIdx > 0)
    {
        // Should have a valid timestamp if there are commnd chunks saved for later to return
        PAL_ASSERT(deferFreeChunkList.timestamp > 0);
        result = m_deferCmdStreamChunks.PushBack(deferFreeChunkList);
    }

    return result;
}

// =====================================================================================================================
// Called after each submit to give the QueueContext an opportunity for cleanup/bookkeeping.
void QueueContext::PostProcessSubmit()
{
}

// =====================================================================================================================
// Called after each submit to give the QueueContext an opportunity for cleanup/bookkeeping.
void UniversalQueueContext::PostProcessSubmit()
{
    ClearDeferredMemory();

    QueueContext::PostProcessSubmit();
}

// =====================================================================================================================
// Determine if any updates are necessary for this queue context's state that depends on dynamic state in the device,
// such as this queue context's shader ring set.
Result UniversalQueueContext::UpdatePerContextDependencies(
    bool*                    pHasChanged,
    bool                     isTmz,
    uint32                   overrideStackSize,
    uint64                   lastTimeStamp,
    uint32                   cmdBufferCount,
    const ICmdBuffer* const* ppCmdBuffers,
    bool                     hasAce,
    bool                     hasInitAce)
{
    PAL_ALERT(pHasChanged == nullptr);
    PAL_ASSERT(m_pParentQueue != nullptr);

    Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    Result result = Result::Success;

    // Obtain current watermark for the sample-pos palette to validate against.
    const uint32 currentSamplePaletteId = pDevice->QueueContextUpdateCounter();
    uint32* pSamplePosPaletteId = isTmz ? &m_queueContextUpdateCounterTmz : &m_queueContextUpdateCounter;
    const bool samplePosPalette = (currentSamplePaletteId > *pSamplePosPaletteId);

    // Check whether the stack size is required to be overridden
    const bool needStackSizeOverride = (m_currentStackSizeDw < overrideStackSize);
    m_currentStackSizeDw             = needStackSizeOverride ? overrideStackSize : m_currentStackSizeDw;

    UniversalRingSet* pRingSet = isTmz ? &m_tmzRingSet : &m_ringSet;

    ShaderRingItemSizes      ringSizes        = {};
    bool                     needRingSetAlloc = false;
    const ShaderRing* const* ppRings          = pRingSet->GetRings();

    bool hasChanged = m_firstSubmit;

    for (uint32 ndxCmd = 0; ndxCmd < cmdBufferCount; ++ndxCmd)
    {
        const UniversalCmdBuffer* pCmdBuf = static_cast<const UniversalCmdBuffer*>(ppCmdBuffers[ndxCmd]);

        // Check if any of the CmdBuffers uses ExecuteIndirectV2 and if required make the allocation of
        // ExecuteIndirectV2 buffer here. This will only be done once per queue context.
        if (pCmdBuf->ExecuteIndirectV2NeedsGlobalSpill() >= ContainsExecuteIndirectV2)
        {
            if (m_executeIndirectMemGfx.IsBound() == false)
            {
                result = AllocateExecuteIndirectBuffer(&m_executeIndirectMemGfx);
            }

            if ((pCmdBuf->ExecuteIndirectV2NeedsGlobalSpill() == ContainsExecuteIndirectV2WithTask) &&
                (m_executeIndirectMemAce.IsBound() == false))
            {
                result = AllocateExecuteIndirectBuffer(&m_executeIndirectMemAce);
            }

            hasChanged = true;
        }

        const ShaderRingItemSizes& cmdRingSizes = pCmdBuf->GetShaderRingSize();

        for (uint32 ring = 0; ring < static_cast<uint32>(ShaderRingType::NumUniversal); ++ring)
        {
            if (cmdRingSizes.itemSize[ring] > ringSizes.itemSize[ring])
            {
                ringSizes.itemSize[ring] = cmdRingSizes.itemSize[ring];
            }
        }

        if (hasAce && m_ringSet.HasAceRingSet())
        {
            const size_t scratchSize = ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)];
            ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] =
                Util::Max(scratchSize, pCmdBuf->GetAceScratchSize());
        }
    }

    for (size_t ring = 0; ring < m_ringSet.NumRings(); ++ring)
    {
        if (ringSizes.itemSize[ring] > ppRings[ring]->ItemSizeMax())
        {
            needRingSetAlloc = true;
            break;
        }
    }

    if (hasAce && m_ringSet.HasAceRingSet() && (needRingSetAlloc == false))
    {
        const ShaderRing* const* ppRingsAce = m_ringSet.GetAceRingSet()->GetRings();
        for (size_t ring = 0; ring < m_ringSet.GetAceRingSet()->NumRings(); ++ring)
        {
            if (ringSizes.itemSize[ring] > ppRingsAce[ring]->ItemSizeMax())
            {
                needRingSetAlloc = true;
                break;
            }
        }
    }

    // The first gang submit requires we build and send its preamble
    if (hasAce && (m_pAcePreambleCmdStream == nullptr))
    {
        hasChanged = true;
    }

    if (samplePosPalette || needStackSizeOverride|| needRingSetAlloc || hasInitAce)
    {
        if (samplePosPalette)
        {
            *pSamplePosPaletteId = currentSamplePaletteId;
            ringSizes.itemSize[static_cast<uint32>(ShaderRingType::SamplePos)] = MaxSamplePatternPaletteEntries;
        }

        // We only want the size of scratch ring is grown locally.
        ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)] =
            Util::Max(static_cast<size_t>(m_currentStackSizeDw),
                      ringSizes.itemSize[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        if (m_needWaitIdleOnRingResize && (m_pParentQueue->IsStalled() == false))
        {
            m_pParentQueue->WaitIdle();
        }

        // The queues are idle, so it is safe to validate the rest of the RingSet.
        if (result == Result::Success)
        {
            SamplePatternPalette palette;
            pDevice->GetSamplePatternPalette(&palette);

            result = pRingSet->Validate(ringSizes,
                                        palette,
                                        lastTimeStamp,
                                        hasAce);
        }

        hasChanged = true;
    }

    (*pHasChanged) = hasChanged;

    return result;
}

// =====================================================================================================================
uint32* UniversalQueueContext::WritePerSubmitPreambleCmds(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = QueueContext::WritePerSubmitPreambleCmds(pCmdStream, pCmdSpace);

    // Wait for a prior submission on this context to be idle before executing the command buffer streams.
    // The timestamp memory is initialized to zero so the first submission on this context will not wait.
    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__pfp_wait_reg_mem__memory_space,
                                          function__pfp_wait_reg_mem__equal_to_the_reference_value,
                                          engine_sel__pfp_wait_reg_mem__prefetch_parser,
                                          m_exclusiveExecTs.GpuVirtAddr(),
                                          0,
                                          UINT_MAX,
                                          pCmdSpace);

    WriteDataInfo writeData = {};
    writeData.engineType    = EngineTypeUniversal;
    writeData.dstAddr       = m_exclusiveExecTs.GpuVirtAddr();
    writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
    writeData.dstSel        = dst_sel__pfp_write_data__memory;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, 1, pCmdSpace);

    const Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    pCmdSpace += CmdUtil::BuildContextControl(pDevice->GetContextControl(), pCmdSpace);
    pCmdSpace = WriteStaticGraphicsRegisters(pCmdSpace, m_pDevice);
    pCmdSpace = WriteStaticComputeRegisters(*m_pDevice, pCmdSpace);

    // Occlusion query control event, specifies that we want one counter to dump out every 128 bits for every
    // DB that the HW supports.

    // NOTE: Despite the structure definition in the HW doc, the instance_enable variable is 36 bits long, not 8.
    union PixelPipeStatControl
    {
        struct
        {
            uint64 reserved1       :  3;
            uint64 counterId       :  6; // Mask of which counts to dump
            uint64 stride          :  2; // PixelPipeStride enum (how far apart each enabled instance must dump from
                                         // each other)
            uint64 instanceEnable  : 36; // Mask of which of the RBs must dump the data.
            uint64 reserved2       : 17;
        } bits;

        uint64 u64All;
    };

    // Our occlusion query data is in pairs of [begin, end], each pair being 128 bits.
    // To emulate the deprecated ZPASS_DONE, we specify COUNT_0, a stride of 128 bits, and all RBs enabled.
    PixelPipeStatControl pixelPipeStatControl = {};
    pixelPipeStatControl.bits.counterId       = PIXEL_PIPE_OCCLUSION_COUNT_0;
    pixelPipeStatControl.bits.stride          = PIXEL_PIPE_STRIDE_128_BITS;
    pixelPipeStatControl.bits.instanceEnable  = (~m_pDevice->ChipProperties().gfx9.backendDisableMask) &
                                                 ((1 << m_pDevice->ChipProperties().gfx9.numTotalRbs) - 1);

    pCmdSpace += CmdUtil::BuildSampleEventWrite(PIXEL_PIPE_STAT_CONTROL,
                                                event_index__me_event_write__pixel_pipe_stat_control_or_dump,
                                                EngineTypeUniversal,
                                                samp_plst_cntr_mode__mec_event_write__legacy_mode,
                                                pixelPipeStatControl.u64All,
                                                pCmdSpace);

    // Issue an acquire mem packet to invalidate all SQ caches (SQ I-cache and SQ K-cache).
    AcquireMemGeneric acquireInfo = {};
    acquireInfo.cacheSync  = SyncGlkInv | SyncGliInv;
    acquireInfo.engineType = EngineTypeUniversal;

    pCmdSpace += m_device.CmdUtil().BuildAcquireMemGeneric(acquireInfo, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Re-record queue context commands to point at the new set of shader rings.
Result UniversalQueueContext::RebuildPerSubmitPreambleCmdStream(
    bool isTmz,
    bool hasAce)
{
    const GpuChipProperties& chipProps = m_pDevice->ChipProperties();

    Result result = m_perSubmitPreambleCmdStream.Begin({}, nullptr);

    if (result == Result::Success)
    {
        // Start by inserting the standard submit preamble commands.
        uint32* pCmdSpace = m_perSubmitPreambleCmdStream.ReserveCommands();
        pCmdSpace = WritePerSubmitPreambleCmds(&m_perSubmitPreambleCmdStream, pCmdSpace);
        m_perSubmitPreambleCmdStream.CommitCommands(pCmdSpace);

        m_firstSubmit = false;

        pCmdSpace = m_perSubmitPreambleCmdStream.ReserveCommands();

        if (m_executeIndirectMemGfx.IsBound())
        {
            const gpusize bufferVa = m_executeIndirectMemGfx.GpuVirtAddr();

            // The ExecuteIndirectMem V2 Buffer is unified or ShaderType agnostic. We assign ShaderGraphics here
            // even though it doesn't matter just because the SetBase PM4 requires it.
            pCmdSpace += CmdUtil::BuildSetBase<ShaderGraphics>(bufferVa,
                                                               base_index__pfp_set_base__execute_indirect_v2,
                                                               pCmdSpace);

            // Disable MCBP for SET_BASE of EI V2 PM4 in this CmdStream submission before the fix went in.
            if (chipProps.pfpUcodeVersion < EiV2McbpFixPfpVersion)
            {
                m_perSubmitPreambleCmdStream.DisablePreemption();
            }
        }
        // Write the shader ring-set's commands after the command stream's normal preamble.
        // to make sure that the attribute buffer has been fully deallocated before the registers are updated.
        pCmdSpace += m_device.CmdUtil().BuildWaitEopPws(AcquirePointMe, false, SyncGlxNone, SyncRbNone, pCmdSpace);

        if (isTmz)
        {
            pCmdSpace = m_tmzRingSet.WriteCommands(pCmdSpace);
        }
        else
        {
            pCmdSpace = m_ringSet.WriteCommands(pCmdSpace);
        }

        // PFP version after which the UPDATE_DB_SUMMARIZER_TIMEOUT packet exists.
        constexpr uint32 DbUpdateSummarizerTimeoutPfpVersion = 2680;

        // This must be done after an idle, which we do before writing the ring sets.
        if (chipProps.pfpUcodeVersion >= DbUpdateSummarizerTimeoutPfpVersion)
        {
            const uint32 timeout = m_device.Settings().hiZsDbSummarizerTimeouts;
            pCmdSpace += CmdUtil::BuildUpdateDbSummarizerTimeouts(timeout, pCmdSpace);
        }

        m_perSubmitPreambleCmdStream.CommitCommands(pCmdSpace);

        result = m_perSubmitPreambleCmdStream.End();
    }

    if (hasAce && (result == Result::Success))
    {
        if (m_pAcePreambleCmdStream == nullptr)
        {
            result = InitAcePreambleCmdStream();
        }

        if (result == Result::Success)
        {
            result = m_pAcePreambleCmdStream->Begin({}, nullptr);
        }

        if (result == Result::Success)
        {
            uint32* pCmdSpace = m_pAcePreambleCmdStream->ReserveCommands();
            pCmdSpace = WriteStaticComputeRegisters(*m_pDevice, pCmdSpace);
            pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, pCmdSpace);
            pCmdSpace = m_ringSet.WriteComputeCommands(pCmdSpace);

            if (m_executeIndirectMemAce.IsBound())
            {
                const gpusize bufferVa = m_executeIndirectMemAce.GpuVirtAddr();

                // The ExecuteIndirectMem V2 Buffer is unified or ShaderType agnostic. We assign ShaderCompute here
                // even though it doesn't matter just because the SetBase PM4 requires it.
                pCmdSpace += CmdUtil::BuildSetBase<ShaderCompute>(bufferVa,
                                                                   base_index__pfp_set_base__execute_indirect_v2,
                                                                   pCmdSpace);
            }

            m_pAcePreambleCmdStream->CommitCommands(pCmdSpace);

            result = m_pAcePreambleCmdStream->End();
        }

    }

    return result;
}

// =====================================================================================================================
// Free deferred memory including old rings and command chunks.
void UniversalQueueContext::ClearDeferredMemory()
{
    // Note: this is an override of a non-virtual function

    PAL_ASSERT(m_pParentQueue != nullptr);
    SubmissionContext* pSubContext = m_pParentQueue->GetSubmissionContext();

    if (pSubContext != nullptr)
    {
        m_tmzRingSet.ClearDeferredFreeMemory(pSubContext);
        m_ringSet.ClearDeferredFreeMemory(pSubContext);
    }

    QueueContext::ClearDeferredMemory();
}

// =====================================================================================================================
// Creates and initializes the ACE CmdStream
Result UniversalQueueContext::InitAcePreambleCmdStream()
{
    PAL_ASSERT(m_pAcePreambleCmdStream == nullptr);

    Result result = Result::Unsupported;

    if (m_supportsAceGang)
    {
        m_pAcePreambleCmdStream = PAL_NEW(CmdStream, m_pDevice->GetPlatform(), AllocInternal)(
            static_cast<const Device&>(*m_pDevice->GetGfxDevice()),
            m_pDevice->InternalUntrackedCmdAllocator(),
            EngineTypeCompute,
            SubEngineType::AsyncCompute,
            CmdStreamUsage::Preamble,
            false);

        if (m_pAcePreambleCmdStream != nullptr)
        {
            result = m_pAcePreambleCmdStream->Init();
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }

        // Creation of the Ace CmdStream failed.
        PAL_ASSERT(result == Result::Success);
    }

    return result;
}

// =====================================================================================================================
// Creates and initializes the ACE CmdStream
Result UniversalQueueContext::InitAcePostambleCmdStream()
{
    PAL_ASSERT(m_pAcePostambleCmdStream == nullptr);

    Result result = Result::Unsupported;

    if (m_supportsAceGang)
    {
        m_pAcePostambleCmdStream = PAL_NEW(CmdStream, m_pDevice->GetPlatform(), AllocInternal)(
            static_cast<const Device&>(*m_pDevice->GetGfxDevice()),
            m_pDevice->InternalUntrackedCmdAllocator(),
            EngineTypeCompute,
            SubEngineType::AsyncCompute,
            CmdStreamUsage::Postamble,
            false);

        if (m_pAcePostambleCmdStream != nullptr)
        {
            result = m_pAcePostambleCmdStream->Init();
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
ComputeQueueContext::ComputeQueueContext(
    Device* pDevice,
    bool    isTmz)
    :
    QueueContext(pDevice, EngineTypeCompute),
    m_ringSet(pDevice, isTmz)
{
}

// =====================================================================================================================
ComputeQueueContext::~ComputeQueueContext()
{
    if (m_executeIndirectMemAce.IsBound())
    {
        m_pDevice->MemMgr()->FreeGpuMem(m_executeIndirectMemAce.Memory(), m_executeIndirectMemAce.Offset());
        m_executeIndirectMemAce.Update(nullptr, 0);
    }

}

// =====================================================================================================================
Result ComputeQueueContext::Init()
{
    Result result = QueueContext::Init();

    if (result == Result::Success)
    {
        result = m_ringSet.Init();
    }

    return result;
}

// =====================================================================================================================
// Called before each submit to give the QueueContext and opportunity to specify preamble/postamble command streams
// that should be submitted along with the client command buffers.
Result ComputeQueueContext::PreProcessSubmit(
    InternalSubmitInfo*      pSubmitInfo,
    uint32                   cmdBufferCount,
    const ICmdBuffer* const* ppCmdBuffers)
{
    const uint64 lastTimestamp = m_pParentQueue->GetSubmissionContext()->LastTimestamp();

    uint32 chunkIdx = 0;
    QueueDeferFreeList deferFreeChunkList;
    for (uint32 i = 0; i < QueueCmdStreamNum; ++i)
    {
        deferFreeChunkList.pChunk[i] = nullptr;
    }
    deferFreeChunkList.timestamp = lastTimestamp;

    // Check if anything has happened since the last submit on this queue that requires a new shader ring set.  If we
    // do need to update the shader ring set, update in the per-submit preamble and submit it.
    bool updatePerContextState = false;
    Result result = UpdatePerContextDependencies(&updatePerContextState,
                                                 pSubmitInfo->stackSizeInDwords,
                                                 lastTimestamp,
                                                 cmdBufferCount,
                                                 ppCmdBuffers);

    if ((result == Result::Success) && updatePerContextState)
    {
        // Compute queue has no state shadowing support. Shader rings have to be updated in each submission's preamble.
        ResetCommandStream(&m_perSubmitPreambleCmdStream, &deferFreeChunkList, &chunkIdx, lastTimestamp);
        result = RebuildPerSubmitPreambleCmdStream();
    }

    if (result == Result::Success)
    {
        pSubmitInfo->pPreambleCmdStream[0]  = &m_perSubmitPreambleCmdStream;
        pSubmitInfo->numPreambleCmdStreams  = 1;

        pSubmitInfo->pPostambleCmdStream[0] = &m_perSubmitPostambleCmdStream;
        pSubmitInfo->numPostambleCmdStreams = 1;

        pSubmitInfo->pagingFence = m_pDevice->InternalUntrackedCmdAllocator()->LastPagingFence();
    }

    if (chunkIdx > 0)
    {
        // Should have a valid timestamp if there are commnd chunks saved for later to return
        PAL_ASSERT(deferFreeChunkList.timestamp > 0);
        result = m_deferCmdStreamChunks.PushBack(deferFreeChunkList);
    }

    return result;
}

// =====================================================================================================================
// Called after each submit to give the QueueContext an opportunity for cleanup/bookkeeping.
void ComputeQueueContext::PostProcessSubmit()
{
    ClearDeferredMemory();

    QueueContext::PostProcessSubmit();
}

// =====================================================================================================================
// Determine if any updates are necessary to this queue context's shader ring set or any other per-context state
// that is dependent on device-wide state that has changed since the last submit.
Result ComputeQueueContext::UpdatePerContextDependencies(
    bool*                    pHasChanged,
    uint32                   overrideStackSize,
    uint64                   lastTimeStamp,
    uint32                   cmdBufferCount,
    const ICmdBuffer* const* ppCmdBuffers)
{
    PAL_ALERT(pHasChanged == nullptr);

    Device* pDevice = static_cast<Device*>(m_pDevice->GetGfxDevice());

    Result result = Result::Success;

    // Obtain current watermark for the sample-pos palette to validate against.
    const uint32 currentSamplePaletteId = pDevice->QueueContextUpdateCounter();
    const bool samplePosPalette         = (currentSamplePaletteId > m_queueContextUpdateCounter);

    // Check whether the stack size is required to be overridden
    const bool needStackSizeOverride = (m_currentStackSizeDw < overrideStackSize);
    m_currentStackSizeDw             = needStackSizeOverride ? overrideStackSize : m_currentStackSizeDw;

    ShaderRingItemSizes      ringSizes         = {};
    bool                     needRingSetAlloc  = false;
    const ShaderRing* const* ppRings           = m_ringSet.GetRings();
    const uint32             computeScratchNdx = static_cast<uint32>(ShaderRingType::ComputeScratch);
    const uint32             samplePosNdx      = static_cast<uint32>(ShaderRingType::SamplePos);

    bool hasChanged = false;

    for (uint32 ndxCmd = 0; ndxCmd < cmdBufferCount; ++ndxCmd)
    {
        const ComputeCmdBuffer* pCmdBuf = static_cast<const ComputeCmdBuffer*>(ppCmdBuffers[ndxCmd]);
        // Check if any of the CmdBuffers uses ExecuteIndirectV2 and if required make the allocation of
        // ExecuteIndirectV2 buffer here. This will only be done once per queue context. We don't need to worry about
        // Task Shader required EIMemAce here as that is handled as part of the UniversalQueueContext. From the
        // HybridPipeline's standpoint that is the context to which the Task+Mesh submission happens.
        if ((pCmdBuf->ExecuteIndirectV2NeedsGlobalSpill() == ContainsExecuteIndirectV2) &&
            (m_executeIndirectMemAce.IsBound() == false))
        {
            result     = AllocateExecuteIndirectBuffer(&m_executeIndirectMemAce);
            hasChanged = true;
        }

        const size_t sizeComputeScratch = pCmdBuf->GetRingSizeComputeScratch();

        if (sizeComputeScratch > ringSizes.itemSize[computeScratchNdx])
        {
            ringSizes.itemSize[computeScratchNdx] = sizeComputeScratch;
        }
    }

    if (ringSizes.itemSize[computeScratchNdx] > ppRings[computeScratchNdx]->ItemSizeMax())
    {
        needRingSetAlloc = true;
    }

    if (samplePosPalette || needStackSizeOverride || needRingSetAlloc)
    {
        if (samplePosPalette)
        {
            m_queueContextUpdateCounter = currentSamplePaletteId;
            ringSizes.itemSize[samplePosNdx] = MaxSamplePatternPaletteEntries;
        }

        // We only want the size of scratch ring is grown locally.
        ringSizes.itemSize[computeScratchNdx] =
            Util::Max(static_cast<size_t>(m_currentStackSizeDw), ringSizes.itemSize[computeScratchNdx]);

        if (m_needWaitIdleOnRingResize && (m_pParentQueue->IsStalled() == false))
        {
            m_pParentQueue->WaitIdle();
        }

        // The queues are idle, so it is safe to validate the rest of the RingSet.
        if (result == Result::Success)
        {
            SamplePatternPalette palette;
            pDevice->GetSamplePatternPalette(&palette);

            result = m_ringSet.Validate(ringSizes,
                                        palette,
                                        lastTimeStamp);
        }

        hasChanged = true;
    }

    (*pHasChanged) = hasChanged;

    return result;
}

// =====================================================================================================================
uint32* ComputeQueueContext::WritePerSubmitPreambleCmds(
    CmdStream*            pCmdStream,
    uint32*               pCmdSpace
    ) const
{
    return WritePerSubmitPreambleCmds(m_ringSet, pCmdStream, pCmdSpace);
}

// =====================================================================================================================
uint32* ComputeQueueContext::WritePerSubmitPreambleCmds(
    const ComputeRingSet& ringSet,
    CmdStream*            pCmdStream,
    uint32*               pCmdSpace
    ) const
{
    pCmdSpace = QueueContext::WritePerSubmitPreambleCmds(pCmdStream, pCmdSpace);

    // Wait for a prior submission on this context to be idle before executing the command buffer streams.
    // The timestamp memory is initialized to zero so the first submission on this context will not wait.
    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                                          mem_space__mec_wait_reg_mem__memory_space,
                                          function__mec_wait_reg_mem__equal_to_the_reference_value,
                                          0,
                                          m_exclusiveExecTs.GpuVirtAddr(),
                                          0,
                                          UINT_MAX,
                                          pCmdSpace);

    WriteDataInfo writeData = {};
    writeData.engineType    = EngineTypeCompute;
    writeData.dstAddr       = m_exclusiveExecTs.GpuVirtAddr();
    writeData.dstSel        = dst_sel__mec_write_data__memory;

    pCmdSpace += CmdUtil::BuildWriteData(writeData, 1, pCmdSpace);

    // Issue an acquire mem packet to invalidate all SQ caches (SQ I-cache and SQ K-cache).
    AcquireMemGeneric acquireInfo = {};
    acquireInfo.cacheSync  = SyncGlkInv | SyncGliInv;
    acquireInfo.engineType = EngineTypeCompute;

    pCmdSpace += m_device.CmdUtil().BuildAcquireMemGeneric(acquireInfo, pCmdSpace);

    pCmdSpace = WriteStaticComputeRegisters(*m_pDevice, pCmdSpace);

    if (m_executeIndirectMemAce.IsBound())
    {
        const gpusize bufferVa = m_executeIndirectMemAce.GpuVirtAddr();

        // The ExecuteIndirectMem V2 Buffer is unified or ShaderType agnostic. We assign ShaderCompute here
        // even though it doesn't matter just because the SetBase PM4 requires it.
        pCmdSpace += CmdUtil::BuildSetBase<ShaderCompute>(bufferVa,
                                                          base_index__pfp_set_base__execute_indirect_v2,
                                                          pCmdSpace);
    }
    // Write the shader ring-set's commands after the command stream's normal preamble.  If the ring sizes have
    // changed, the hardware requires a CS partial flush to operate properly.
    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeCompute, pCmdSpace);

    // Compute queue has no state shadowing support. Shader rings have to be updated in each submission's preamble.
    pCmdSpace = ringSet.WriteCommands(pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Re-record queue context commands to point at the new set of shader rings.
Result ComputeQueueContext::RebuildPerSubmitPreambleCmdStream()
{
    m_perSubmitPreambleCmdStream.Reset(nullptr, true);
    Result result = m_perSubmitPreambleCmdStream.Begin({}, nullptr);

    if (result == Result::Success)
    {
        uint32* pCmdSpace = m_perSubmitPreambleCmdStream.ReserveCommands();
        pCmdSpace = WritePerSubmitPreambleCmds(m_ringSet, &m_perSubmitPreambleCmdStream, pCmdSpace);

        m_perSubmitPreambleCmdStream.CommitCommands(pCmdSpace);

        result = m_perSubmitPreambleCmdStream.End();
    }

    return result;
}

// =====================================================================================================================
// Free deferred memory including old rings and command chunks.
void ComputeQueueContext::ClearDeferredMemory()
{
    // Note: this is an override of a non-virtual function

    PAL_ASSERT(m_pParentQueue != nullptr);
    SubmissionContext* pSubContext = m_pParentQueue->GetSubmissionContext();

    if (pSubContext != nullptr)
    {
        m_ringSet.ClearDeferredFreeMemory(pSubContext);
    }

    QueueContext::ClearDeferredMemory();
}

} // namespace Gfx12
} // namespace Pal
