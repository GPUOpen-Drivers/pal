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

#include "core/hw/gfxip/gfxImage.h"
#include "core/hw/gfxip/queryPool.h"
#include "core/hw/gfxip/gfx12/gfx12Barrier.h"
#include "core/hw/gfxip/gfx12/gfx12BorderColorPalette.h"
#include "core/hw/gfxip/gfx12/gfx12ColorBlendState.h"
#include "core/hw/gfxip/gfx12/gfx12ComputePipeline.h"
#include "core/hw/gfxip/gfx12/gfx12DepthStencilState.h"
#include "core/hw/gfxip/gfx12/gfx12DepthStencilView.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12HybridGraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12Image.h"
#include "core/hw/gfxip/gfx12/gfx12IndirectCmdGenerator.h"
#include "core/hw/gfxip/gfx12/gfx12MsaaState.h"
#include "core/hw/gfxip/gfx12/gfx12PerfExperiment.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "core/hw/gfxip/rpm/gfx12/gfx12RsrcProcMgr.h"

#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"

#include "palAutoBuffer.h"
#include "palHsaAbiMetadata.h"
#include "palInlineFuncs.h"
#include "palIntervalTreeImpl.h"
#include "palIterator.h"

#include <float.h>

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
UniversalCmdBuffer::UniversalCmdBuffer(
    const Device&                         device,
    const CmdBufferCreateInfo&            createInfo,
    const UniversalCmdBufferDeviceConfig& deviceConfig)
    :
    Pal::UniversalCmdBuffer(device,
                            createInfo,
                            device.BarrierMgr(),
                            &m_deCmdStream,
                            nullptr,
                            (deviceConfig.blendOptimizationsEnable != 0),
                            true),
    m_deviceConfig(deviceConfig),
    m_cmdUtil(device.CmdUtil()),
    m_rsrcProcMgr(device.RsrcProcMgr()),
    m_deCmdStream(device,
                  createInfo.pCmdAllocator,
                  EngineTypeUniversal,
                  SubEngineType::Primary,
                  CmdStreamUsage::Workload,
                  IsNested()),
    m_gfxState{},
    m_streamoutCtrlBuf(0),
    m_pPrevGfxUserDataLayoutValidatedWith(nullptr),
    m_pPrevComputeUserDataLayoutValidatedWith(nullptr),
    m_dispatchPingPongEn(false),
    m_indirectDispatchArgsValid(false),
    m_indirectDispatchArgsAddrHi(0),
    m_writeCbDbHighBaseRegs(false),
    m_activeOcclusionQueryWriteRanges(device.GetPlatform()),
    m_gangSubmitState{},
    m_pComputeStateAce(nullptr),
    m_ringSizes{},
    m_deferredPipelineStatsQueries(m_device.GetPlatform()),
    m_dvgprExtraAceScratch(0)
{
    memset(&m_vbTable,    0, sizeof(m_vbTable));
    memset(&m_streamOut,  0, sizeof(m_streamOut));
    memset(&m_spillTable, 0, sizeof(m_spillTable));
    memset(&m_nggTable,   0, sizeof(m_nggTable));

    SwitchDrawFunctions(false, false);

    SetDispatchFunctions(false);

    // Setup globally static parts of the batch binner state
    m_gfxState.batchBinnerState.paScBinnerCntl0.bits.DISABLE_START_OF_PRIM       =  1;
    m_gfxState.batchBinnerState.paScBinnerCntl0.bits.FPOVS_PER_BATCH             = 63;
    m_gfxState.batchBinnerState.paScBinnerCntl0.bits.OPTIMAL_BIN_SELECTION       =  1;
    m_gfxState.batchBinnerState.paScBinnerCntl0.bits.FLUSH_ON_BINNING_TRANSITION =  1;
    m_gfxState.batchBinnerState.paScBinnerCntl0.bits.BIN_MAPPING_MODE            =  0;
}

// =====================================================================================================================
UniversalCmdBuffer::~UniversalCmdBuffer()
{
    PAL_SAFE_DELETE(m_pAceCmdStream,    m_device.GetPlatform());
    PAL_SAFE_DELETE(m_pComputeStateAce, m_device.GetPlatform());
}

// =====================================================================================================================
Result UniversalCmdBuffer::Init(
    const CmdBufferInternalCreateInfo& internalInfo)
{
    constexpr uint32 nggTableBytes = Pow2Align<uint32>(sizeof(Abi::PrimShaderCullingCb), 256);
    m_nggTable.state.sizeInDwords  = NumBytesToNumDwords(nggTableBytes);
    m_nggTable.numSamples = 1;
    m_vbTable.gpuState.sizeInDwords = DwordsPerBufferSrd * MaxVertexBuffers;

    Result result = Pal::UniversalCmdBuffer::Init(internalInfo);

    if (result == Result::Success)
    {
        result = m_deCmdStream.Init();
    }

    return result;
}

// =====================================================================================================================
void UniversalCmdBuffer::ResetState()
{
    Pal::UniversalCmdBuffer::ResetState();

    // Assume PAL ABI compute pipelines by default.
    SetDispatchFunctions(false);

    // No graphics rpm blt on gfx12.
    m_cmdBufState.flags.gfxBltActive        = 0;
    m_cmdBufState.flags.gfxWriteCachesDirty = 0;

    m_activeOcclusionQueryWriteRanges.Clear();

    m_gfxState.validBits.u32All    = 0;
    m_gfxState.cbTargetMask.u32All = 0;
    m_gfxState.pipelinePsHash      = {};
    m_gfxState.primShaderCullingCb = {};
    m_gfxState.dbStencilWriteMask  = {};
    m_gfxState.dbRenderOverride    = {};
    m_gfxState.dbStencilControl    = {};

    m_gfxState.noForceReZ       = false;
    m_gfxState.dbShaderControl  = {};
    m_gfxState.dsLog2NumSamples = 0;
    m_gfxState.szValid          = false;

    memset(&m_currentTargetsMetadata,  0, sizeof(m_currentTargetsMetadata));
    memset(&m_previousTargetsMetadata, 0, sizeof(m_previousTargetsMetadata));

    m_graphicsState.bindTargets.colorTargetCount = 8;

    memset(&m_streamOut, 0, sizeof(m_streamOut));

    m_pPrevGfxUserDataLayoutValidatedWith     = nullptr;
    m_pPrevComputeUserDataLayoutValidatedWith = nullptr;

    m_dispatchPingPongEn = false;

    m_indirectDispatchArgsValid  = false;
    m_indirectDispatchArgsAddrHi = 0;
    m_writeCbDbHighBaseRegs      =
        ((m_deviceConfig.stateFilterFlags & Gfx12RedundantStateFilterCbDbHighBitsWhenZero) != 0) ? false : true;

    // Setup per-cmd buffer batch binner state
    Chip::PA_SC_BINNER_CNTL_0*const pCntl0 = &m_gfxState.batchBinnerState.paScBinnerCntl0;
    pCntl0->bits.CONTEXT_STATES_PER_BIN    = (m_contextStatesPerBin    > 0) ? (m_contextStatesPerBin    - 1) : 0;
    pCntl0->bits.PERSISTENT_STATES_PER_BIN = (m_persistentStatesPerBin > 0) ? (m_persistentStatesPerBin - 1) : 0;

    m_gfxState.vertexOffsetReg     = UserDataNotMapped;
    m_gfxState.drawIndexReg        = UserDataNotMapped;
    m_gfxState.meshDispatchDimsReg = UserDataNotMapped;
    m_gfxState.nggCullingDataReg   = UserDataNotMapped;
    m_gfxState.viewIdsReg.u32All   = 0;

    // If this is a non-nested cmd buffer, need set scissorRectsIn64K=1 by default in case CmdSetScissorRects
    // is not called.
    //
    // If this is a nested cmd buffer, we may not know below states of root cmd buffer from driver side.
    // These states are required to set register bit PA_SC_MODE_CNTL_1.WALK_ALIGNMENT and WALK_ALIGN8_PRIM_FITS_ST
    // (the two bits must be 0 if any of below states is 1) correctly at ValidateDraw time otherwise HW may hang.
    // Assume the worst case for safe.
    m_gfxState.paScWalkAlignState.u32All             = 0;
    m_gfxState.paScWalkAlignState.scissorRectsIn64K  = 1;
    m_gfxState.paScWalkAlignState.dirty              = 1; // Force dirty to write paScModeCntl1 in the first draw.
    if (IsNested())
    {
        m_gfxState.paScWalkAlignState.globalScissorIn64K = 1;
        m_gfxState.paScWalkAlignState.targetIn64K        = 1;
        m_gfxState.paScWalkAlignState.hasHiSZ            = 1;
        m_gfxState.paScWalkAlignState.hasVrsImage        = 1;
    }

    ResetUserDataTable(&m_spillTable.stateGfx);
    ResetUserDataTable(&m_spillTable.stateCompute);
    ResetUserDataTable(&m_spillTable.stateWg);
    ResetUserDataTable(&m_nggTable.state);
    ResetUserDataTable(&m_vbTable.gpuState);
    m_vbTable.watermarkInDwords = m_vbTable.gpuState.sizeInDwords;
    m_vbTable.modified  = 0;

    m_nggTable.numSamples = 1;

    m_gangSubmitState = {};

    m_hasOcclusionQueryActive = false;

    m_streamoutCtrlBuf       = 0;

    if (m_pComputeStateAce != nullptr)
    {
        memset(m_pComputeStateAce, 0, sizeof(ComputeState));
    }

    memset(const_cast<ShaderRingItemSizes*>(&m_ringSizes), 0, sizeof(m_ringSizes));

    m_deferredPipelineStatsQueries.Clear();

    m_dvgprExtraAceScratch = 0;
}

static constexpr uint32 CbDbBaseHighRegisters[] =
{
    // CTV
    mmCB_COLOR0_BASE_EXT,
    mmCB_COLOR1_BASE_EXT,
    mmCB_COLOR2_BASE_EXT,
    mmCB_COLOR3_BASE_EXT,
    mmCB_COLOR4_BASE_EXT,
    mmCB_COLOR5_BASE_EXT,
    mmCB_COLOR6_BASE_EXT,
    mmCB_COLOR7_BASE_EXT,

    // DSV
    mmDB_Z_READ_BASE_HI,
    mmDB_STENCIL_READ_BASE_HI,
    mmDB_Z_WRITE_BASE_HI,
    mmDB_STENCIL_WRITE_BASE_HI,
    mmPA_SC_HIS_BASE_EXT,
    mmPA_SC_HIZ_BASE_EXT,
};

// =====================================================================================================================
// Add any commands to restore state, etc. that are required at the beginning of every command buffer.
void UniversalCmdBuffer::AddPreamble()
{

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (m_deviceConfig.enablePreamblePipelineStats == 1)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal, pDeCmdSpace);
    }

    if (IsNested() == false)
    {
        pDeCmdSpace = UpdateDbCountControl(pDeCmdSpace);
    }

    {
        // Prime the CmdBuffer with 0 in the HIGH CB/DB base registers. It is rare to need these bits.
        using Regs = RegPairHandler<decltype(CbDbBaseHighRegisters), CbDbBaseHighRegisters>;
        RegisterValuePair regs[Regs::Size()];
        Regs::Init(regs);

        static_assert(Regs::Size() == Regs::NumContext(), "No other register types expected.");
        pDeCmdSpace = CmdStream::WriteSetContextPairs(regs, Regs::Size(), pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Adds a postamble to the end of a new command buffer.
void UniversalCmdBuffer::AddPostamble()
{

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if ((m_globalInternalTableAddr != 0) &&
        (m_computeState.pipelineState.pPipeline != nullptr) &&
        (static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline)->GetInfo().flags.hsaAbi != 0u))
    {
        // If we're ending this cmdbuf with an HSA pipeline bound, the global table may currently
        // be invalid and we need to restore it for any subsequent chained cmdbufs.
        // Note 'nullptr' is considered PAL ABI and the restore must have already happened if needed.
        pDeCmdSpace += CmdUtil::BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                     data_format__pfp_load_sh_reg_index__offset_and_size,
                                                     m_globalInternalTableAddr,
                                                     mmCOMPUTE_USER_DATA_0,
                                                     1,
                                                     Pm4ShaderType::ShaderCompute,
                                                     pDeCmdSpace);
    }

    // Wait for all other ganged ACE work to also complete (this uses a different fence). This is because we want to
    // guarantee that the DE does not increment the ACE command stream's done-count before the ACE has finished its
    // work.
    if (m_gangSubmitState.cmdStreamSemAddr != 0)
    {
        pDeCmdSpace = CmdDeWaitAce(pDeCmdSpace);
    }

    if (IsOneTimeSubmit() == false)
    {
        WriteDataInfo writeData = {};
        writeData.engineType    = GetEngineType();
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__pfp_write_data__memory;

        if (m_gangSubmitState.cmdStreamSemAddr != 0)
        {
            // If the memory contains any value, it is possible that with the ACE running ahead, it could get a value
            // for this semaphore which is >= the number it is waiting for and then just continue ahead before GFX has
            // a chance to write it to 0. The vice versa case could happen for "GFX waiting for ACE" semaphore as well.
            // To handle the case where we reuse a command buffer entirely, we'll have to perform a GPU-side write of this
            // memory in the postamble.
            constexpr uint32 SemZeroes[2] = {0u, 0u};

            writeData.dstAddr = m_gangSubmitState.cmdStreamSemAddr;

            pDeCmdSpace += CmdUtil::BuildWriteData(writeData, ArrayLen32(SemZeroes), SemZeroes, pDeCmdSpace);
        }

    }

    if (m_cmdBufState.flags.cpBltActive && (IsNested() == false))
    {
        // Stalls the CP ME until the CP's DMA engine has finished all previous "CP blts" (DMA_DATA commands
        // without the sync bit set). The ring won't wait for CP DMAs to finish so we need to do this manually.
        pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        SetCpBltState(false);
    }

    // The following ATOMIC_MEM packet increments the done-count for the command stream, so that we can probe
    // when the command buffer has completed execution on the GPU.
    // NOTE: Normally, we would need to flush the L2 cache to guarantee that this memory operation makes it out to
    // memory. However, since we're at the end of the command buffer, we can rely on the fact that the KMD inserts
    // an EOP event which flushes and invalidates the caches in between command buffers.
    if (m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr() != 0)
    {
        // We also need a wait-for-idle before the atomic increment because command memory might be read or written
        // by draws or dispatches. If we don't wait for idle then the driver might reset and write over that memory
        // before the shaders are done executing.
        constexpr WriteWaitEopInfo WaitEopInfo = { .hwAcqPoint = AcquirePointMe };

        pDeCmdSpace = WriteWaitEop(WaitEopInfo, pDeCmdSpace);

        pDeCmdSpace += CmdUtil::BuildAtomicMem(AtomicOp::AddInt32,
                                               m_deCmdStream.GetFirstChunk()->BusyTrackerGpuAddr(),
                                               1,
                                               pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    if (ImplicitGangedSubQueueCount() >= 1)
    {
        PAL_ASSERT(m_pAceCmdStream != nullptr);
        ComputeCmdBuffer::WritePostambleCommands(this, static_cast<CmdStream*>(m_pAceCmdStream));
    }
}

// =====================================================================================================================
size_t UniversalCmdBuffer::GetAceScratchSize() const
{
    return (m_ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)] + m_dvgprExtraAceScratch);
}

// =====================================================================================================================
void UniversalCmdBuffer::BindTaskShader(
    const GraphicsPipeline*          pNewPipeline,
    const DynamicGraphicsShaderInfo& dynamicInfo,
    uint64                           apiPsoHash)
{
    PAL_ASSERT(pNewPipeline->HasTaskShader());

    if (HasHybridPipeline() == false)
    {
        TryInitAceGangedSubmitResources();
        ReportHybridPipelineBind();

        // Updates the ring size for Task+Mesh pipelines.
        m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)] =
            Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)], 1);

        m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)] =
            Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)], 1);
    }

    if (m_pComputeStateAce->pipelineState.pPipeline != pNewPipeline)
    {
        m_pComputeStateAce->pipelineState.pPipeline           = pNewPipeline;
        m_pComputeStateAce->pipelineState.apiPsoHash          = apiPsoHash;
        m_pComputeStateAce->pipelineState.dirtyFlags.pipeline = 1;
    }

    if (m_pComputeStateAce->dynamicCsInfo.maxWavesPerCu != dynamicInfo.maxWavesPerCu)
    {
        m_pComputeStateAce->dynamicCsInfo.maxWavesPerCu = dynamicInfo.maxWavesPerCu;
    }

    if (m_pComputeStateAce->pipelineState.dirtyFlags.u32All != 0)
    {
        const HybridGraphicsPipeline* pHybridPipeline = static_cast<const HybridGraphicsPipeline*>(pNewPipeline);

        m_dvgprExtraAceScratch = Util::Max(m_dvgprExtraAceScratch, pHybridPipeline->GetDvgprExtraAceScratch());

        uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();
        pAceCmdSpace = pHybridPipeline->WriteTaskCommands(m_pComputeStateAce->dynamicCsInfo,
                                                          pAceCmdSpace,
                                                          static_cast<CmdStream*>(m_pAceCmdStream));
        m_pAceCmdStream->CommitCommands(pAceCmdSpace);
    }
}

// =====================================================================================================================
static inline bool IsAlphaToCoverageEnabled(
    const GraphicsPipeline*     pPipeline,
    const DynamicGraphicsState& dynamicGraphicsState)
{
    return (pPipeline != nullptr)
             ? (dynamicGraphicsState.enable.alphaToCoverageEnable
                 ? dynamicGraphicsState.alphaToCoverageEnable
                 : pPipeline->IsAlphaToCoverage())
             : false;
}

// =====================================================================================================================
// This function produces a draw developer callback based on current pipeline state.
void UniversalCmdBuffer::DescribeDraw(
    Developer::DrawDispatchType cmdType,
    bool                        includedGangedAce)
{
    uint32 firstVertexIdx   = UINT_MAX;
    uint32 startInstanceIdx = UINT_MAX;
    uint32 drawIndexIdx     = UINT_MAX;

    if ((cmdType != Developer::DrawDispatchType::CmdDispatchMesh) &&
        (cmdType != Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti))
    {
        // Compute register offsets of first vertex and start instance user data locations relative to
        // user data 0.
        PAL_ASSERT((GetVertexOffsetRegAddr() != 0) && (GetInstanceOffsetRegAddr() != 0));

        firstVertexIdx   = GetVertexOffsetRegAddr();
        startInstanceIdx = GetInstanceOffsetRegAddr();
    }

    if (GetDrawIndexRegAddr() != UserDataNotMapped)
    {
        drawIndexIdx = GetDrawIndexRegAddr();
    }

    RgpMarkerSubQueueFlags subQueueFlags { };
    subQueueFlags.includeMainSubQueue    = 1;
    subQueueFlags.includeGangedSubQueues = includedGangedAce;

    m_device.DescribeDraw(this, subQueueFlags, cmdType, firstVertexIdx, startInstanceIdx, drawIndexIdx);
}

// =====================================================================================================================
// This function writes SQTT userdata with draw information that needs synchronized to SE from CPWD
void UniversalCmdBuffer::AddDrawSqttMarkers(
    const ValidateDrawInfo& drawInfo)
{
    // Dword 0 of the two dword sequence to describe a draw
    union
    {
        struct
        {
            uint32 identifier : 4;  // Key used by tool to recognize this data in the marker
            uint32 instances  : 28; // NUM_INSTANCES as will be written by hardware
        } bits;

        uint32 u32All;
    } dw0 = {};

    // Dword 1 of the two dword sequence to describe a draw
    union
    {
        struct
        {
            uint32 indices; // NUM_INDICES as will be written by hardware
        } bits;

        uint32 u32All;
    } dw1 = {};

    constexpr uint32 DrawInfoIdentifier = 0xf;

    dw0.bits.identifier = DrawInfoIdentifier;
    dw0.bits.instances  = drawInfo.instanceCount;
    dw1.bits.indices    = drawInfo.vtxIdxCount;

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace = m_deCmdStream.WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_USERDATA_7, dw0.u32All, pCmdSpace);
    pCmdSpace = m_deCmdStream.WriteSetOnePerfCtrReg(mmSQ_THREAD_TRACE_USERDATA_7, dw1.u32All, pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
// Note: If targetIndex is set to UINT32_MAX, then the format will be as is defined in the pipeline packet.
void UniversalCmdBuffer::CmdBindPipelineWithOverrides(
    const PipelineBindParams& params,
    SwizzledFormat            swizzledFormat,
    uint32                    targetIndex)
{
    PAL_ASSERT(params.pPipeline != nullptr); // Caller is enforcing this

    const auto* pNewPipeline  = static_cast<const GraphicsPipeline*>(params.pPipeline);
    const auto* pPrevPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    const bool wasAlphaToCoverageEnabled = IsAlphaToCoverageEnabled(pPrevPipeline, m_graphicsState.dynamicState);
    const bool isAlphaToCoverageEnabled  = IsAlphaToCoverageEnabled(pNewPipeline, params.gfxDynState);

    Pal::UniversalCmdBuffer::CmdBindPipeline(params);

    const CB_TARGET_MASK oldCbTargetMask   = m_gfxState.cbTargetMask;
    const ShaderHash     oldPipelinePsHash = m_gfxState.pipelinePsHash;
    const bool           newHasTaskShader  = pNewPipeline->HasTaskShader();

    if (pNewPipeline != pPrevPipeline)
    {
        const bool newUsesViewInstancing = pNewPipeline->UsesViewInstancing();
        const bool oldUsesViewInstancing = (pPrevPipeline != nullptr) && pPrevPipeline->UsesViewInstancing();
        const bool oldHasTaskShader      = (pPrevPipeline != nullptr) && pPrevPipeline->HasTaskShader();

        if ((oldUsesViewInstancing != newUsesViewInstancing) || (oldHasTaskShader != newHasTaskShader)) [[unlikely]]
        {
            SwitchDrawFunctions(newUsesViewInstancing, newHasTaskShader);
        }

        SetShaderRingSize(pNewPipeline->GetShaderRingSize());
    }

    if (newHasTaskShader) [[unlikely]]
    {
        BindTaskShader(pNewPipeline, params.gfxShaderInfo.ts, params.apiPsoHash);
    }

#if PAL_DEVELOPER_BUILD
    const uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
#endif

    DepthClampMode newDepthClampMode = {};
    regPA_CL_CLIP_CNTL paClClipCntl  = {};

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    if (m_buildFlags.prefetchShaders)
    {
        pCmdSpace = pNewPipeline->Prefetch(m_deviceConfig.prefetchClampSize, pCmdSpace);
    }

    pCmdSpace = pNewPipeline->WriteContextAndUConfigCommands(params.gfxDynState,
                                                             &m_gfxState,
                                                             swizzledFormat,
                                                             targetIndex,
                                                             m_deviceConfig.stateFilterFlags,
                                                             &newDepthClampMode,
                                                             &paClClipCntl,
                                                             pCmdSpace);

    if (m_gfxState.primShaderCullingCb.paClClipCntl != paClClipCntl.u32All)
    {
        m_gfxState.primShaderCullingCb.paClClipCntl = paClClipCntl.u32All;
        m_nggTable.state.dirty                      = 1;
    }

    const bool cbTargetMaskChanged = (oldCbTargetMask.u32All != m_gfxState.cbTargetMask.u32All);
    bool       breakBatch          = cbTargetMaskChanged && (m_contextStatesPerBin > 1);

    if ((breakBatch == false) &&
        (m_deviceConfig.batchBreakOnNewPs || (m_contextStatesPerBin > 1) || (m_persistentStatesPerBin > 1)))
    {
        if ((pPrevPipeline == nullptr) || (ShaderHashesEqual(oldPipelinePsHash, m_gfxState.pipelinePsHash) == false))
        {
            breakBatch = true;
        }
    }

    if (breakBatch)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pCmdSpace);
    }

    const DynamicGraphicsState& dynamicState = params.gfxDynState;

    // Override the vertexBufferCount from dynamic state, if applicable.
    const uint32 vbTableDwords = (dynamicState.enable.vertexBufferCount != 0) ?
                                 (dynamicState.vertexBufferCount * DwordsPerBufferSrd) :
                                 (pNewPipeline->VertexBufferCount() * DwordsPerBufferSrd);

    // VB state is known because it is validated prior to normal draws and DispatchGraph.
    // It is also not modified during graph execution.
    if (vbTableDwords > m_vbTable.watermarkInDwords)
    {
        // If the current watermark is too small (size visible to the GPU), we must re-upload the table.
        m_vbTable.gpuState.dirty = 1;
    }

    m_vbTable.watermarkInDwords = vbTableDwords;

    BinSizeExtend binSizeXExtent;
    BinSizeExtend binSizeYExtent;

    if (IsNested() || (m_gfxState.validBits.batchBinnerState == 0))
    {
        // Nested cmdbuffers never call BindTargets and must call bind pipeline, just hardcode the bin size for now.
        binSizeXExtent = BIN_SIZE_128_PIXELS;
        binSizeYExtent = BIN_SIZE_128_PIXELS;
    }
    else
    {
        // Use the most recently computed bin sizes.
        binSizeXExtent = m_gfxState.batchBinnerState.binSizeX;
        binSizeYExtent = m_gfxState.batchBinnerState.binSizeY;
    }

    pCmdSpace = UpdateBatchBinnerState(pNewPipeline->IsBinningDisabled() ? BINNING_DISABLED : BINNING_ALLOWED,
                                       binSizeXExtent,
                                       binSizeYExtent,
                                       pCmdSpace);

    if (pNewPipeline->UserDataLayout()->GetStreamoutCtrlBuf().u32All != UserDataNotMapped) [[unlikely]]
    {
        // If we are using streamout, we want to make sure that the streamout control buffer has memory allocated.
        pCmdSpace = VerifyStreamoutCtrlBuf(pCmdSpace);
    }

    m_deCmdStream.CommitCommands(pCmdSpace);

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        const uint32 pipelineCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
    }
#endif

    const uint32 enableMultiViewport = (pNewPipeline->UsesMultipleViewports() == false) ? 0 : 1;

    // We only really need to re-validate due to PSO when going from single viewport -> multi viewport
    if (enableMultiViewport > m_graphicsState.enableMultiViewport)
    {
        m_graphicsState.dirtyFlags.viewports = 1;
        m_nggTable.state.dirty               = 1;
    }

#if PAL_BUILD_SUPPORT_DEPTHCLAMPMODE_ZERO_TO_ONE
    // Must re-validate viewports if we changed to or from DepthClampMode::ZeroToOne
    if ((newDepthClampMode == DepthClampMode::ZeroToOne) !=
        (static_cast<DepthClampMode>(m_graphicsState.depthClampMode) == DepthClampMode::ZeroToOne))
    {
        m_graphicsState.dirtyFlags.viewports = 1;
    }
#endif

    // Must re-validate blend register if A2C status changed.
    if (wasAlphaToCoverageEnabled != isAlphaToCoverageEnabled)
    {
        m_graphicsState.dirtyFlags.colorBlendState = 1;
    }

    m_graphicsState.depthClampMode      = static_cast<uint32>(newDepthClampMode);
    m_graphicsState.enableMultiViewport = enableMultiViewport;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if ((params.pPipeline != nullptr) && (params.pipelineBindPoint == PipelineBindPoint::Graphics))
    {
        // CmdBindPipelineWithOverrides will call UniversalCmdBuffer::CmdBindPipeline for us.
        CmdBindPipelineWithOverrides(params, {}, UINT32_MAX);
    }
    else
    {
        if (params.pipelineBindPoint == PipelineBindPoint::Compute)
        {
            auto* const pPrevPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
            auto* const pNewPipeline  = static_cast<const ComputePipeline*>(params.pPipeline);

            const bool newUsesHsaAbi = (pNewPipeline != nullptr) && (pNewPipeline->GetInfo().flags.hsaAbi != 0u);
            const bool oldUsesHsaAbi = (pPrevPipeline != nullptr) && (pPrevPipeline->GetInfo().flags.hsaAbi != 0u);

            if (oldUsesHsaAbi != newUsesHsaAbi) [[unlikely]]
            {
                // The HSA abi can clobber USER_DATA_0, which holds the global internal table address for PAL ABI,
                // so we must save the address to memory before switching to an HSA ABI
                // or restore it when switching back to PAL ABI
                if (newUsesHsaAbi && (m_globalInternalTableAddr == 0))
                {
                    m_globalInternalTableAddr = AllocateGpuScratchMem(1, 1);
                    m_rsrcProcMgr.EchoGlobalInternalTableAddr(this, m_globalInternalTableAddr);
                }
                else if (newUsesHsaAbi == false)
                {
                    CmdUtil::BuildLoadShRegsIndex(index__pfp_load_sh_reg_index__direct_addr,
                                                  data_format__pfp_load_sh_reg_index__offset_and_size,
                                                  m_globalInternalTableAddr,
                                                  mmCOMPUTE_USER_DATA_0,
                                                  1,
                                                  Pm4ShaderType::ShaderCompute,
                                                  m_deCmdStream.AllocateCommands(CmdUtil::LoadShRegsIndexSizeDwords));
                }
                SetDispatchFunctions(newUsesHsaAbi);
            }

            if (params.pPipeline != nullptr)
            {
                const auto* pPipeline = static_cast<const ComputePipeline*>(params.pPipeline);

#if PAL_DEVELOPER_BUILD
                const uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
#endif

                uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
                pCmdSpace = pPipeline->WriteCommands(
                    pPrevPipeline, params.cs, m_buildFlags.prefetchShaders, pCmdSpace, &m_deCmdStream);
                m_deCmdStream.CommitCommands(pCmdSpace);

#if PAL_DEVELOPER_BUILD
                if (m_deviceConfig.enablePm4Instrumentation)
                {
                    const uint32 pipelineCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
                    m_device.DescribeBindPipelineValidation(this, pipelineCmdLen);
                }
#endif

                m_ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)] =
                    Max(m_ringSizes.itemSize[static_cast<uint32>(ShaderRingType::ComputeScratch)],
                        pPipeline->GetRingSizeComputeScratch());

                if (m_pAceCmdStream != nullptr)
                {
                    m_dvgprExtraAceScratch = Util::Max(m_dvgprExtraAceScratch, pPipeline->GetDvgprExtraAceScratch());
                }
            }
        }

        Pal::UniversalCmdBuffer::CmdBindPipeline(params);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    const MsaaState* const pNewState = static_cast<const MsaaState*>(pMsaaState);

    if (pNewState != nullptr)
    {
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace         = pNewState->WriteCommands(pCmdSpace);
        m_deCmdStream.CommitCommands(pCmdSpace);

        m_nggTable.numSamples = (1 << pNewState->PaScAaConfig().bits.MSAA_NUM_SAMPLES);
        m_gfxState.primShaderCullingCb.enableConservativeRasterization =
            pNewState->PaScConsRastCntl().bits.OVER_RAST_ENABLE;
    }
    else
    {
        m_nggTable.numSamples                                          = 1;
        m_gfxState.primShaderCullingCb.enableConservativeRasterization = 0;
    }

    m_graphicsState.pMsaaState           = pMsaaState;
    m_graphicsState.dirtyFlags.msaaState = 1;
    m_nggTable.state.dirty               = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    if (pDepthStencilState != nullptr)
    {
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace = static_cast<const DepthStencilState*>(pDepthStencilState)->WriteCommands(pCmdSpace);
        m_deCmdStream.CommitCommands(pCmdSpace);

        m_gfxState.dbStencilControl = static_cast<const DepthStencilState*>(pDepthStencilState)->DbStencilControl();
    }

    m_graphicsState.pDepthStencilState           = pDepthStencilState;
    m_graphicsState.dirtyFlags.depthStencilState = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    if (pColorBlendState != nullptr)
    {
        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
        pCmdSpace = static_cast<const ColorBlendState*>(pColorBlendState)->WriteCommands(pCmdSpace);
        m_deCmdStream.CommitCommands(pCmdSpace);
    }

    m_graphicsState.pColorBlendState           = pColorBlendState;
    m_graphicsState.dirtyFlags.colorBlendState = 1;
}

// =====================================================================================================================
// Returns the HW X and Y shading rate values that correspond to the supplied enumeration.
static Offset2d GetHwShadingRate(
    VrsShadingRate  shadingRate)
{
    Offset2d  hwShadingRate = {};

    static constexpr Offset2d  HwShadingRateTable[] =
    {
        { -2, -2 }, // VrsShadingRate::_16xSsaa
        { -2, -1 }, // VrsShadingRate::_8xSsaa
        { -2,  0 }, // VrsShadingRate::_4xSsaa
        { -2,  1 }, // VrsShadingRate::_2xSsaa
        {  0,  0 }, // VrsShadingRate::_1x1
        {  0,  1 }, // VrsShadingRate::_1x2
        {  1,  0 }, // VrsShadingRate::_2x1
        {  1,  1 }, // VrsShadingRate::_2x2
    };

    // HW encoding is in 2's complement of the table values
    hwShadingRate.x = HwShadingRateTable[static_cast<uint32>(shadingRate)].x;
    hwShadingRate.y = HwShadingRateTable[static_cast<uint32>(shadingRate)].y;

    return hwShadingRate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetPerDrawVrsRate(
    const VrsRateParams& params)
{
    PA_CL_VRS_CNTL paClVrsCntl = {};
    GE_VRS_RATE    gsVrsRate   = {};

    paClVrsCntl.bits.VERTEX_RATE_COMBINER_MODE =
        uint32(params.combinerState[static_cast<uint32>(VrsCombinerStage::ProvokingVertex)]);
    paClVrsCntl.bits.PRIMITIVE_RATE_COMBINER_MODE =
        uint32(params.combinerState[static_cast<uint32>(VrsCombinerStage::Primitive)]);
    paClVrsCntl.bits.HTILE_RATE_COMBINER_MODE =
        uint32(params.combinerState[static_cast<uint32>(VrsCombinerStage::Image)]);
    paClVrsCntl.bits.SAMPLE_ITER_COMBINER_MODE=
        uint32(params.combinerState[static_cast<uint32>(VrsCombinerStage::PsIterSamples)]);
    paClVrsCntl.bits.EXPOSE_VRS_PIXELS_MASK   = params.flags.exposeVrsPixelsMask;
    paClVrsCntl.bits.SAMPLE_COVERAGE_ENCODING = params.flags.exposeVrsPixelsMask;

    // GE_VRS_RATE has an enable bit located in VGT_DRAW__PAYLOAD_CNTL.EN_VRS_RATE.  That register is owned
    // by the pipeline, but the pipeline should be permanently enabling that bit.
    const Offset2d hwShadingRate = GetHwShadingRate(params.shadingRate);

    gsVrsRate.bits.RATE_X = hwShadingRate.x;
    gsVrsRate.bits.RATE_Y = hwShadingRate.y;

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    // Purposely only filtering PA_CL_VRS_CNTL instead of just comparing the full params because the shading rate
    // stored in GE_VRS_RATE seems to change at a higher frequency than the other state set on this interface.
    // GE_VRS_RATE can also be redundant but the expectation is that reducing context rolls is where gains could be had.
    if ((m_buildFlags.optimizeGpuSmallBatch == 0) ||
        ((m_gfxState.paClVrsCntl.u32All != paClVrsCntl.u32All) ||
         (m_gfxState.validBits.paClVrsCntl == 0)))
    {
        pCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_CL_VRS_CNTL, paClVrsCntl.u32All, pCmdSpace);

        if (m_buildFlags.optimizeGpuSmallBatch)
        {
            m_gfxState.paClVrsCntl.u32All    = paClVrsCntl.u32All;
            m_gfxState.validBits.paClVrsCntl = 1;
        }
    }
    pCmdSpace = m_deCmdStream.WriteSetOneUConfigReg(mmGE_VRS_RATE, gsVrsRate.u32All, pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    m_graphicsState.vrsRateState             = params;
    m_graphicsState.dirtyFlags.vrsRateParams = 1;
}

constexpr uint32 VrsCenterStateRegs[] =
{
    mmDB_SPI_VRS_CENTER_LOCATION,
    mmSPI_BARYC_SSAA_CNTL,
};

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetVrsCenterState(
    const VrsCenterState& params)
{
    using Regs = RegPairHandler<decltype(VrsCenterStateRegs), VrsCenterStateRegs>;
    RegisterValuePair regs[Regs::Size()];
    Regs::Init(regs);

    auto* pDbSpiVrsCenterLocation = Regs::Get<mmDB_SPI_VRS_CENTER_LOCATION, DB_SPI_VRS_CENTER_LOCATION>(regs);

    pDbSpiVrsCenterLocation->bits.CENTER_X_OFFSET_1X1 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_1x1)].x;
    pDbSpiVrsCenterLocation->bits.CENTER_Y_OFFSET_1X1 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_1x1)].y;
    pDbSpiVrsCenterLocation->bits.CENTER_X_OFFSET_2X1 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_2x1)].x;
    pDbSpiVrsCenterLocation->bits.CENTER_Y_OFFSET_2X1 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_2x1)].y;
    pDbSpiVrsCenterLocation->bits.CENTER_X_OFFSET_1X2 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_1x2)].x;
    pDbSpiVrsCenterLocation->bits.CENTER_Y_OFFSET_1X2 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_1x2)].y;
    pDbSpiVrsCenterLocation->bits.CENTER_X_OFFSET_2X2 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_2x2)].x;
    pDbSpiVrsCenterLocation->bits.CENTER_Y_OFFSET_2X2 =
        params.centerOffset[static_cast<uint32>(VrsCenterRates::_2x2)].y;

    auto* pSpiBarycSsAaCntl = Regs::Get<mmSPI_BARYC_SSAA_CNTL, SPI_BARYC_SSAA_CNTL>(regs);
    pSpiBarycSsAaCntl->bits.CENTER_SSAA_MODE           = params.flags.overrideCenterSsaa;
    pSpiBarycSsAaCntl->bits.CENTROID_SSAA_MODE         = params.flags.overrideCentroidSsaa;
    pSpiBarycSsAaCntl->bits.COVERED_CENTROID_IS_CENTER = (params.flags.alwaysComputeCentroid ? 0 : 1);

    static_assert(Regs::Size() == Regs::NumContext(), "No other register types expected.");
    m_deCmdStream.AllocateAndBuildSetContextPairs(regs, Regs::Size());

    m_graphicsState.vrsCenterState            = params;
    m_graphicsState.dirtyFlags.vrsCenterState = 1;
}

constexpr uint32 SampleRateImageRegs[] =
{
    mmPA_SC_VRS_RATE_BASE,
    mmPA_SC_VRS_RATE_BASE_EXT,
    mmPA_SC_VRS_RATE_SIZE_XY,
    mmPA_SC_VRS_OVERRIDE_CNTL,
    mmPA_SC_VRS_INFO,
};

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindSampleRateImage(
    const IImage* pImage)
{
    using SampleRate = RegPairHandler<decltype(SampleRateImageRegs), SampleRateImageRegs>;

    RegisterValuePair regs[SampleRate::Size()];
    SampleRate::Init(regs);

    const bool hasVrsImage = (pImage != nullptr);

    if (hasVrsImage)
    {
        const Pal::Image*      pPalImage  = static_cast<const Pal::Image*>(pImage);
        const GfxImage*        pGfxImage  = static_cast<const GfxImage*>(pPalImage->GetGfxImage());
        const ImageCreateInfo& createInfo = pPalImage->GetImageCreateInfo();

        PAL_ASSERT(Formats::BitsPerPixel(createInfo.swizzledFormat.format) == 8);
        PAL_ASSERT(createInfo.mipLevels == 1);
        PAL_ASSERT(createInfo.arraySize == 1);
        PAL_ASSERT(createInfo.samples   == 1);
        PAL_ASSERT(createInfo.imageType == ImageType::Tex2d);
        PAL_ASSERT((HighPart(pPalImage->GetGpuVirtualAddr()) >> 8) == 0);

        constexpr SubresId     BaseSubresId = { };
        const SubResourceInfo* pSubresInfo  = pPalImage->SubresourceInfo(BaseSubresId);

        const gpusize gpuaddr256b = pGfxImage->GetSubresource256BAddr(BaseSubresId);

        SampleRate::Get<mmPA_SC_VRS_RATE_BASE, PA_SC_VRS_RATE_BASE>(regs)->bits.BASE_256B = LowPart(gpuaddr256b);
        SampleRate::Get<mmPA_SC_VRS_RATE_BASE_EXT, PA_SC_VRS_RATE_BASE_EXT>(regs)->bits.BASE_256B =
            HighPart(gpuaddr256b);

        auto* pSizeXy = SampleRate::Get<mmPA_SC_VRS_RATE_SIZE_XY, PA_SC_VRS_RATE_SIZE_XY>(regs);

        pSizeXy->bits.X_MAX = Min(createInfo.extent.width  - 1, m_deviceConfig.maxVrsRateCoord);
        pSizeXy->bits.Y_MAX = Min(createInfo.extent.height - 1, m_deviceConfig.maxVrsRateCoord);

        SampleRate::Get<mmPA_SC_VRS_OVERRIDE_CNTL, PA_SC_VRS_OVERRIDE_CNTL>(regs)->bits.VRS_SURFACE_ENABLE = 1;

        SampleRate::Get<mmPA_SC_VRS_INFO, PA_SC_VRS_INFO>(regs)->bits.RATE_SW_MODE =
            pGfxImage->GetSwTileMode(pSubresInfo);
    }

    static_assert(SampleRate::Size() == SampleRate::NumContext(), "No other register types expected here.");
    m_deCmdStream.AllocateAndBuildSetContextPairs(regs, SampleRate::Size());

    // Independent layer records the source image and marks our command buffer state as dirty.
    Pal::UniversalCmdBuffer::CmdBindSampleRateImage(pImage);

    auto* const pPaScWalkAlignState = &m_gfxState.paScWalkAlignState;

    if (pPaScWalkAlignState->hasVrsImage != uint32(hasVrsImage))
    {
        pPaScWalkAlignState->hasVrsImage = hasVrsImage ? 1 : 0;
        pPaScWalkAlignState->dirty       = 1;
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    m_graphicsState.depthBiasState            = params;
    m_graphicsState.dirtyFlags.depthBiasState = 1;

    struct DepthBiasStateRegs
    {
        PA_SU_POLY_OFFSET_CLAMP        paSuPolyOffsetClamp;
        PA_SU_POLY_OFFSET_FRONT_SCALE  paSuPolyOffsetFrontScale;
        PA_SU_POLY_OFFSET_FRONT_OFFSET paSuPolyOffsetFrontOffset;
        PA_SU_POLY_OFFSET_BACK_SCALE   paSuPolyOffsetBackScale;
        PA_SU_POLY_OFFSET_BACK_OFFSET  paSuPolyOffsetBackOffset;
    };

    static_assert((PA_SU_POLY_OFFSET_CLAMP__CLAMP_MASK         == UINT32_MAX) &&
                  (PA_SU_POLY_OFFSET_FRONT_SCALE__SCALE_MASK   == UINT32_MAX) &&
                  (PA_SU_POLY_OFFSET_FRONT_OFFSET__OFFSET_MASK == UINT32_MAX) &&
                  (PA_SU_POLY_OFFSET_BACK_SCALE__SCALE_MASK    == UINT32_MAX) &&
                  (PA_SU_POLY_OFFSET_BACK_OFFSET__OFFSET_MASK  == UINT32_MAX),
                  "PolyOffset reg bits are expected to be fully defined");

    // No need to zero init - all register bits defined and set
    DepthBiasStateRegs regs;

    static_assert(Util::CheckSequential({ mmPA_SU_POLY_OFFSET_CLAMP,
                                          mmPA_SU_POLY_OFFSET_FRONT_SCALE,
                                          mmPA_SU_POLY_OFFSET_FRONT_OFFSET,
                                          mmPA_SU_POLY_OFFSET_BACK_SCALE,
                                          mmPA_SU_POLY_OFFSET_BACK_OFFSET, }),
                  "DepthBiasStateRegs are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(DepthBiasStateRegs, paSuPolyOffsetClamp),
                                          offsetof(DepthBiasStateRegs, paSuPolyOffsetFrontScale),
                                          offsetof(DepthBiasStateRegs, paSuPolyOffsetFrontOffset),
                                          offsetof(DepthBiasStateRegs, paSuPolyOffsetBackScale),
                                          offsetof(DepthBiasStateRegs, paSuPolyOffsetBackOffset), },
                                        sizeof(uint32)),
                  "Storage order of DepthBiasStateRegs is important!");

    regs.paSuPolyOffsetFrontOffset.f32All = params.depthBias;
    regs.paSuPolyOffsetBackOffset.f32All  = params.depthBias;
    regs.paSuPolyOffsetClamp.f32All       = params.depthBiasClamp;

    // The multiplier to account for the factor of 1/16th to the Z gradients that HW applies.
    constexpr uint32 HwOffsetScaleMultiplier   = 0x00000010;
    const float slopeScaleDepthBias            = (params.slopeScaledDepthBias * HwOffsetScaleMultiplier);

    regs.paSuPolyOffsetFrontScale.f32All = slopeScaleDepthBias;
    regs.paSuPolyOffsetBackScale.f32All  = slopeScaleDepthBias;

    m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmPA_SU_POLY_OFFSET_CLAMP, mmPA_SU_POLY_OFFSET_BACK_OFFSET, &regs);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    if ((m_buildFlags.optimizeGpuSmallBatch == 0) ||
        ((memcmp(&params, &(m_graphicsState.depthBoundsState), sizeof(params)) != 0) ||
         (m_graphicsState.leakFlags.depthBoundsState == 0)))
    {
        m_graphicsState.depthBoundsState            = params;
        m_graphicsState.dirtyFlags.depthBoundsState = 1;

        static_assert((sizeof(DepthBoundsParams) == sizeof(uint32) * 2) &&
                      (offsetof(DepthBoundsParams, min) == 0) &&
                      (offsetof(DepthBoundsParams, max) == 4),
                      "Layout and defintion of DepthBoundsParams should exactly matches HW");
        static_assert(Util::CheckSequential({ mmDB_DEPTH_BOUNDS_MIN,
                                              mmDB_DEPTH_BOUNDS_MAX, }),
                      "DepthBounds regs are not sequential!");

        m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmDB_DEPTH_BOUNDS_MIN, mmDB_DEPTH_BOUNDS_MAX, &params);
    }
}

// =====================================================================================================================
// Constructs a virtual rectangle that surrounds all viewports in order to find a center point that must be written to
// PA_SU_HARDWARE_SCREEN_OFFSET so that the guardband originates from the rectangle's center rather than its origin.
// Also calculates scale factors, which is the factor by which the center rectangle can be scaled to fill the entire
// guardband region.
VportCenterRect UniversalCmdBuffer::GetViewportsCenterAndScale() const
{
    const ViewportParams& params = m_graphicsState.viewportState;

    float rectLeft   = 0;
    float rectRight  = 0;
    float rectTop    = 0;
    float rectBottom = 0;

    VportCenterRect centerRect = {};

    for (uint32 i = 0; i < params.count; i++)
    {
        const Viewport& viewport = params.viewports[i];

        // Calculate the left and rightmost coordinates of the surrounding rectangle
        float left  = viewport.originX;
        float right = viewport.originX + viewport.width;
        // Swap left and right to correct negSize and posSize if width is negative
        if (viewport.width < 0)
        {
            Swap(left, right);
        }
        rectLeft    = Min(left, rectLeft);
        rectRight   = Max(right, rectRight);

        // Calculate the top and bottommost coordinates of the surrounding rectangle
        float top    = viewport.originY;
        float bottom = viewport.originY + viewport.height;
        // Swap top and bottom to correct negSize and posSize if height is negative
        if (viewport.height < 0)
        {
            Swap(top, bottom);
        }
        rectTop      = Min(top, rectTop);
        rectBottom   = Max(bottom, rectBottom);
    }

    // Calculate accumulated viewport rectangle center point
    float centerX          = (rectLeft   + rectRight) / 2;
    float centerY          = (rectBottom + rectTop)   / 2;
    // We must clamp the center point coords to 0 in the corner case where viewports are centered in negative space
    centerRect.centerX     = centerX > 0 ? centerX : 0;
    centerRect.centerY     = centerY > 0 ? centerY : 0;

    constexpr float MaxHorzGuardbandSize = MaxHorzScreenCoord - MinHorzScreenCoord;
    constexpr float MaxVertGuardbandSize = MaxVertScreenCoord - MinVertScreenCoord;
    constexpr float MaxLineWidth         = 8192.0;

    // Keep guard band clip region with a margin offset equals max line width to ensure wide line render correct when
    // it's pixel coord exceeds max hardware screen coord. BTW, the clipFactor should be clamped to more than 1.0f.
    centerRect.xClipFactor = Max((MaxHorzGuardbandSize - MaxLineWidth) / (rectRight - rectLeft), 1.0f);
    centerRect.yClipFactor = Max((MaxVertGuardbandSize - MaxLineWidth) / (rectBottom - rectTop), 1.0f);

    return centerRect;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    const size_t     viewportSize  = (sizeof(params.viewports[0]) * params.count);
    constexpr size_t GuardbandSize = (sizeof(float) * 4);

    m_graphicsState.viewportState.count      = params.count;
    m_graphicsState.viewportState.depthRange = params.depthRange;

    memcpy(&m_graphicsState.viewportState.viewports[0],     &params.viewports[0],     viewportSize);
    memcpy(&m_graphicsState.viewportState.horzDiscardRatio, &params.horzDiscardRatio, GuardbandSize);

    m_graphicsState.dirtyFlags.viewports = 1;
    m_nggTable.state.dirty               = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::WriteViewports(
    uint32 viewportCount)
{
    const ViewportParams& vpParams = m_graphicsState.viewportState;

    struct Guardband
    {
        PA_CL_GB_VERT_CLIP_ADJ vertClipAdj;
        PA_CL_GB_VERT_DISC_ADJ vertDiscAdj;
        PA_CL_GB_HORZ_CLIP_ADJ horzClipAdj;
        PA_CL_GB_HORZ_DISC_ADJ horzDiscAdj;
    };

    struct Viewport
    {
        PA_CL_VPORT_XSCALE  xScale;
        PA_CL_VPORT_XOFFSET xOffset;
        PA_CL_VPORT_YSCALE  yScale;
        PA_CL_VPORT_YOFFSET yOffset;
        PA_CL_VPORT_ZSCALE  zScale;
        PA_CL_VPORT_ZOFFSET zOffset;
        PA_SC_VPORT_ZMIN_0  zMin;
        PA_SC_VPORT_ZMAX_0  zMax;
    };

    struct ViewportRegs
    {
        Guardband guardband;
        Viewport  vp[MaxViewports];
    };

    static_assert(Util::CheckSequential({ mmPA_CL_GB_VERT_CLIP_ADJ,
                                          mmPA_CL_GB_VERT_DISC_ADJ,
                                          mmPA_CL_GB_HORZ_CLIP_ADJ,
                                          mmPA_CL_GB_HORZ_DISC_ADJ,
                                          mmPA_CL_VPORT_XSCALE,
                                          mmPA_CL_VPORT_XOFFSET,
                                          mmPA_CL_VPORT_YSCALE,
                                          mmPA_CL_VPORT_YOFFSET,
                                          mmPA_CL_VPORT_ZSCALE,
                                          mmPA_CL_VPORT_ZOFFSET,
                                          mmPA_SC_VPORT_ZMIN_0,
                                          mmPA_SC_VPORT_ZMAX_0, }),
                  "Guardband and VP0 should be sequential!");
    static_assert((mmPA_SC_VPORT_ZMAX_15 - mmPA_CL_VPORT_XSCALE + 1) == (ViewportStride * MaxViewports),
                  "All viewport regs are expected to be sequential!");
    static_assert((sizeof(Viewport) / sizeof(uint32)) == ViewportStride,
                  "Viewport struct is not laid out properly!");
    static_assert(Util::CheckSequential({ offsetof(ViewportRegs, guardband),
                                          offsetof(ViewportRegs, vp), }, sizeof(Guardband)),
                  "Storage order of ViewportRegs is important!");
    static_assert(Util::CheckSequential({ offsetof(Guardband, vertClipAdj),
                                          offsetof(Guardband, vertDiscAdj),
                                          offsetof(Guardband, horzClipAdj),
                                          offsetof(Guardband, horzDiscAdj), }, sizeof(uint32)),
                  "Storage order of Guardband is important!");
    static_assert(Util::CheckSequential({ offsetof(Viewport, xScale),
                                          offsetof(Viewport, xOffset),
                                          offsetof(Viewport, yScale),
                                          offsetof(Viewport, yOffset),
                                          offsetof(Viewport, zScale),
                                          offsetof(Viewport, zOffset),
                                          offsetof(Viewport, zMin),
                                          offsetof(Viewport, zMax), }, sizeof(uint32)),
                  "Storage order of Viewport is important!");

    struct ViewportScissor
    {
        PA_SC_VPORT_0_TL tl;
        PA_SC_VPORT_0_BR br;
    };

    constexpr uint32 ViewportScissorStride = mmPA_SC_VPORT_1_TL - mmPA_SC_VPORT_0_TL;

    static_assert((mmPA_SC_VPORT_0_TL + 1 == mmPA_SC_VPORT_0_BR) &&
                  ((sizeof(ViewportScissor) / sizeof(uint32)) == ViewportScissorStride) &&
                  ((mmPA_SC_VPORT_15_BR - mmPA_SC_VPORT_0_TL + 1) == (ViewportScissorStride * MaxViewports)),
                  "Viewport scissor registers are expected to be tightly packed!");
    static_assert(Util::CheckSequential({ offsetof(ViewportScissor, tl),
                                          offsetof(ViewportScissor, br), }, sizeof(uint32)),
                  "Storage order of ViewportScissor is important!");

    static_assert((PA_CL_GB_VERT_CLIP_ADJ__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_GB_VERT_DISC_ADJ__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_GB_HORZ_CLIP_ADJ__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_GB_HORZ_DISC_ADJ__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_VPORT_XSCALE__VPORT_XSCALE_MASK      == UINT32_MAX) &&
                  (PA_CL_VPORT_XOFFSET__VPORT_XOFFSET_MASK    == UINT32_MAX) &&
                  (PA_CL_VPORT_YSCALE__VPORT_YSCALE_MASK      == UINT32_MAX) &&
                  (PA_CL_VPORT_YOFFSET__VPORT_YOFFSET_MASK    == UINT32_MAX) &&
                  (PA_CL_VPORT_ZSCALE__VPORT_ZSCALE_MASK      == UINT32_MAX) &&
                  (PA_CL_VPORT_ZOFFSET__VPORT_ZOFFSET_MASK    == UINT32_MAX) &&
                  (PA_SC_VPORT_ZMIN_0__VPORT_ZMIN_MASK        == UINT32_MAX) &&
                  (PA_SC_VPORT_ZMAX_0__VPORT_ZMAX_MASK        == UINT32_MAX),
                  "All Guardband and VP regs are expected to have all register bits defined.");
    static_assert(((PA_SC_VPORT_0_TL__TL_X_MASK | PA_SC_VPORT_0_TL__TL_Y_MASK) == UINT32_MAX) &&
                  ((PA_SC_VPORT_0_BR__BR_X_MASK | PA_SC_VPORT_0_BR__BR_Y_MASK) == UINT32_MAX),
                  "All scissor regs are expected to have all register bits defined");

    // Zero init of viewportRegs and scissorRegs can be skipped because all register bits are defined
    // and the code below sets all defined register fields.
    ViewportRegs                 viewportRegs;
    ViewportScissor              scissorRegs[MaxViewports];
    PA_SU_HARDWARE_SCREEN_OFFSET hwScreenOffset            = {};

    // VP Count = 0 is not technically illegal but it is unexpected! In this case, we will program whatever to VP[0].
    PAL_ALERT(viewportCount == 0);

    const uint32 vpCount = (viewportCount > 0) ? viewportCount : 1;

    PAL_ASSERT((vpParams.horzClipRatio    >= 1.f) &&
               (vpParams.horzDiscardRatio >= 1.f) &&
               (vpParams.vertClipRatio    >= 1.f) &&
               (vpParams.vertDiscardRatio >= 1.f));

    // Initialize guardband factors to client-specified values.  May be reduced based on viewport dimensions below.
    viewportRegs.guardband.horzDiscAdj.f32All = vpParams.horzDiscardRatio;
    viewportRegs.guardband.horzClipAdj.f32All = vpParams.horzClipRatio;
    viewportRegs.guardband.vertDiscAdj.f32All = vpParams.vertDiscardRatio;
    viewportRegs.guardband.vertClipAdj.f32All = vpParams.vertClipRatio;

    // Initialize guardband offsets paramemters. The rectangle here represents the minimum rectangle including all
    // viewports rectangles.
    float rectLeft   = FLT_MAX;
    float rectRight  = -FLT_MAX;
    float rectTop    = FLT_MAX;
    float rectBottom = -FLT_MAX;

    for (uint32 i = 0; i < vpCount; i++)
    {
        const auto& viewport = vpParams.viewports[i];
        auto* pNggViewports  = &m_gfxState.primShaderCullingCb.viewports[i];

        const float xScale = viewport.width * 0.5f;
        const float yScale = viewport.height * 0.5f;

        viewportRegs.vp[i].xScale.f32All  = xScale;
        viewportRegs.vp[i].xOffset.f32All = viewport.originX + xScale;
        viewportRegs.vp[i].yScale.f32All  = yScale * (viewport.origin == PointOrigin::UpperLeft ? 1.f : -1.f);
        viewportRegs.vp[i].yOffset.f32All = viewport.originY + yScale;

        // Calculate the left and rightmost coordinates of the surrounding rectangle
        float vpLeft  = viewport.originX;
        float vpRight = viewport.originX + viewport.width;
        // Swap vpLeft and vpRight to correct negSize and posSize if width is negative
        if (viewport.width < 0)
        {
            Swap(vpLeft, vpRight);
        }
        rectLeft    = Min(vpLeft, rectLeft);
        rectRight   = Max(vpRight, rectRight);

        // Calculate the top and bottommost coordinates of the surrounding rectangle
        float vpTop    = viewport.originY;
        float vpBottom = viewport.originY + viewport.height;
        // Swap top and bottom to correct negSize and posSize if height is negative
        if (viewport.height < 0)
        {
            Swap(vpTop, vpBottom);
        }
        rectTop      = Min(vpTop, rectTop);
        rectBottom   = Max(vpBottom, rectBottom);

        if (vpParams.depthRange == DepthRange::NegativeOneToOne)
        {
            viewportRegs.vp[i].zScale.f32All  = (viewport.maxDepth - viewport.minDepth) * 0.5f;
            viewportRegs.vp[i].zOffset.f32All = (viewport.maxDepth + viewport.minDepth) * 0.5f;
        }
        else
        {
            viewportRegs.vp[i].zScale.f32All  = viewport.maxDepth - viewport.minDepth;
            viewportRegs.vp[i].zOffset.f32All = viewport.minDepth;
        }

#if PAL_BUILD_SUPPORT_DEPTHCLAMPMODE_ZERO_TO_ONE
        if (static_cast<DepthClampMode>(m_graphicsState.depthClampMode) == DepthClampMode::ZeroToOne)
        {
            viewportRegs.vp[i].zMin.f32All = 0.0f;
            viewportRegs.vp[i].zMax.f32All = 1.0f;
        }
        else
#endif
        {
            viewportRegs.vp[i].zMin.f32All = Min(viewport.minDepth, viewport.maxDepth);
            viewportRegs.vp[i].zMax.f32All = Max(viewport.minDepth, viewport.maxDepth);
        }

        pNggViewports->paClVportXOffset = viewportRegs.vp[i].xOffset.u32All;
        pNggViewports->paClVportYOffset = viewportRegs.vp[i].yOffset.u32All;

        pNggViewports->paClVportXScale = viewportRegs.vp[i].xScale.u32All;
        pNggViewports->paClVportYScale = viewportRegs.vp[i].yScale.u32All;

        // Setup integer rectangles that drive implicit viewport scissoring.  Flush denorms to 0 to avoid potential
        // rounds to negative infinity.
        const int32 left   = static_cast<int32>(Math::FlushDenormToZero(viewport.originX));
        const int32 top    = static_cast<int32>(Math::FlushDenormToZero(viewport.originY));
        const int64 right  = static_cast<int64>(Math::FlushDenormToZero(viewport.originX + viewport.width));
        const int64 bottom = static_cast<int64>(Math::FlushDenormToZero(viewport.originY + viewport.height));

        // Null scissor is defined as (maxScissorCoord, 0)
        const uint32 maxScissorCoord = m_deviceConfig.maxScissorSize - 1;

        scissorRegs[i].tl.bits.TL_X = (viewport.width > 0)  ? Clamp<int32>(left, 0, maxScissorCoord) : maxScissorCoord;
        scissorRegs[i].tl.bits.TL_Y = (viewport.height > 0) ? Clamp<int32>(top, 0, maxScissorCoord)  : maxScissorCoord;
        scissorRegs[i].br.bits.BR_X = (viewport.width > 0)  ? uint32(Clamp<int64>(right - 1, 0, maxScissorCoord)) : 0;
        scissorRegs[i].br.bits.BR_Y = (viewport.height > 0) ? uint32(Clamp<int64>(bottom - 1, 0, maxScissorCoord)) : 0;
    }

    const VportCenterRect vpCenterRect = GetViewportsCenterAndScale();

    // Clients may pass specific clip ratios for perf/quality that *must* be used over our calculated clip factors as
    // long as they are < our clip factors
    viewportRegs.guardband.horzClipAdj.f32All = Min(vpCenterRect.xClipFactor, vpParams.horzClipRatio);
    viewportRegs.guardband.vertClipAdj.f32All = Min(vpCenterRect.yClipFactor, vpParams.vertClipRatio);

    m_gfxState.primShaderCullingCb.paClGbHorzClipAdj = viewportRegs.guardband.horzClipAdj.u32All;
    m_gfxState.primShaderCullingCb.paClGbHorzDiscAdj = viewportRegs.guardband.horzDiscAdj.u32All;
    m_gfxState.primShaderCullingCb.paClGbVertClipAdj = viewportRegs.guardband.vertClipAdj.u32All;
    m_gfxState.primShaderCullingCb.paClGbVertDiscAdj = viewportRegs.guardband.vertDiscAdj.u32All;

    // Write accumulated rectangle's center coords to PA_SU_HARDWARE_SCREEN_OFFSET to center guardband correctly.
    // Without doing this, there is fewer potential guardband region below and to the right of the viewport than
    // above and to the left.
    hwScreenOffset.bits.HW_SCREEN_OFFSET_X = static_cast<uint32>(vpCenterRect.centerX / 16.0f);
    hwScreenOffset.bits.HW_SCREEN_OFFSET_Y = static_cast<uint32>(vpCenterRect.centerY / 16.0f);

    // On GFX12, bit 0 must be 0 if VRS_SURFACE_ENABLE or RATE_HINT_WRITE_BACK_ENABLE are set. Thus, we must ensure that
    // the LSB for both screen offsets is set to 0. We do this globally for GFX12, which will result in a slightly
    // improperly centered guarband, though it should not matter much for performance or correctness.
    hwScreenOffset.bits.HW_SCREEN_OFFSET_X &= 0xFFE;
    hwScreenOffset.bits.HW_SCREEN_OFFSET_Y &= 0xFFE;

    const uint32 lastViewportReg = mmPA_SC_VPORT_ZMAX_0 + ((vpCount - 1) * ViewportStride);
    const uint32 lastScissorReg  = mmPA_SC_VPORT_0_BR   + ((vpCount - 1) * ViewportScissorStride);
    const uint32 totalCmdDwords  = (CmdUtil::SetSeqContextRegsSizeDwords(mmPA_CL_GB_VERT_CLIP_ADJ, lastViewportReg) +
                                    CmdUtil::SetSeqContextRegsSizeDwords(mmPA_SC_VPORT_0_TL, lastScissorReg) +
                                    CmdUtil::SetOneContextRegSizeDwords);

    uint32* pDeCmdSpace = m_deCmdStream.AllocateCommands(totalCmdDwords);

    pDeCmdSpace = CmdStream::WriteSetSeqContextRegs(mmPA_CL_GB_VERT_CLIP_ADJ,
                                                   lastViewportReg,
                                                   &viewportRegs,
                                                   pDeCmdSpace);
    pDeCmdSpace = CmdStream::WriteSetSeqContextRegs(mmPA_SC_VPORT_0_TL,
                                                    lastScissorReg,
                                                    &scissorRegs,
                                                    pDeCmdSpace);
    pDeCmdSpace = CmdStream::WriteSetOneContextReg(mmPA_SU_HARDWARE_SCREEN_OFFSET,
                                                   hwScreenOffset.u32All,
                                                   pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    bool scissorRectsIn64K = true; // Default 64K mode if params.count == 0.

    m_graphicsState.scissorRectState.count  = params.count;
    m_graphicsState.dirtyFlags.scissorRects = 1;

    if (params.count > 0)
    {
        const size_t scissorSize = (sizeof(params.scissors[0]) * params.count);
        memcpy(&m_graphicsState.scissorRectState.scissors[0], &params.scissors[0], scissorSize);

        struct ScissorRegs
        {
            PA_SC_VPORT_SCISSOR_0_TL tl;
            PA_SC_VPORT_SCISSOR_0_BR br;
        };

        // No need to initialize regs to 0
        static_assert(((PA_SC_VPORT_SCISSOR_0_BR__BR_X_MASK | PA_SC_VPORT_SCISSOR_0_BR__BR_Y_MASK) == UINT32_MAX) &&
                      ((PA_SC_VPORT_SCISSOR_0_TL__TL_X_MASK | PA_SC_VPORT_SCISSOR_0_TL__TL_Y_MASK) == UINT32_MAX),
                      "All register bits for the scissor are expected to be defined!");

        ScissorRegs regs[MaxViewports];

        constexpr uint32 ScissorRegOffset = mmPA_SC_VPORT_SCISSOR_1_TL - mmPA_SC_VPORT_SCISSOR_0_TL;

        static_assert(((mmPA_SC_VPORT_SCISSOR_0_TL + 1) == mmPA_SC_VPORT_SCISSOR_0_BR) &&
                      ((sizeof(ScissorRegs) / sizeof(uint32)) == ScissorRegOffset)     &&
                      ((ScissorRegOffset * MaxViewports) ==
                       ((mmPA_SC_VPORT_SCISSOR_15_BR - mmPA_SC_VPORT_SCISSOR_0_TL) + 1)),
                      "Scissor layout is unexpected!");

        scissorRectsIn64K = false;
        for (uint32 i = 0; i < params.count; i++)
        {
            auto* pTl = &(regs[i].tl);
            auto* pBr = &(regs[i].br);

            if (m_deviceConfig.tossPointMode != TossPointAfterSetup)
            {
                const Extent2d extent = params.scissors[i].extent;
                const int64    left   = params.scissors[i].offset.x;
                const int64    top    = params.scissors[i].offset.y;

                // E.g., left = 0, width = 0xFFFFFFFF, int32 will be - 1 and clamp to 0, so use int64 instead.
                const int64    right  = left + extent.width;
                const int64    bottom = top + extent.height;

                // Null scissor is defined as (maxScissorCoord, 0), a (TL, BR) pair of (0, 0) is not a null scissor.
                const uint32 maxScissorCoord = m_deviceConfig.maxScissorSize - 1;
                const bool   isValid         = (left <= maxScissorCoord) &&
                                               (top <= maxScissorCoord)  &&
                                               (right > 0)               &&
                                               (bottom > 0)              &&
                                               (extent.width > 0)        &&
                                               (extent.height > 0);

                pTl->bits.TL_X = (isValid) ? uint32(Clamp<int64>(left,       0, maxScissorCoord)) : maxScissorCoord;
                pTl->bits.TL_Y = (isValid) ? uint32(Clamp<int64>(top,        0, maxScissorCoord)) : maxScissorCoord;
                pBr->bits.BR_X = (isValid) ? uint32(Clamp<int64>(right - 1,  0, maxScissorCoord)) : 0;
                pBr->bits.BR_Y = (isValid) ? uint32(Clamp<int64>(bottom - 1, 0, maxScissorCoord)) : 0;

                scissorRectsIn64K |= TestAnyFlagSet((pBr->bits.BR_X | pBr->bits.BR_Y), 1u << 15);
            }
            else
            {
                pTl->bits.TL_X = 0;
                pTl->bits.TL_Y = 0;
                pBr->bits.BR_X = 0;
                pBr->bits.BR_Y = 0;
            }
        }

        const uint32 lastReg = mmPA_SC_VPORT_SCISSOR_0_BR + (params.count - 1) * ScissorRegOffset;
        m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmPA_SC_VPORT_SCISSOR_0_TL, lastReg, &regs);
    }

    auto* const pPaScWalkAlignState = &m_gfxState.paScWalkAlignState;

    if (pPaScWalkAlignState->scissorRectsIn64K != uint32(scissorRectsIn64K))
    {
        m_gfxState.paScWalkAlignState.scissorRectsIn64K = scissorRectsIn64K ? 1 : 0;
        m_gfxState.paScWalkAlignState.dirty             = 1;
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    m_graphicsState.globalScissorState            = params;
    m_graphicsState.dirtyFlags.globalScissorState = 1;

    struct GlobalScissorRegs
    {
        PA_SC_WINDOW_SCISSOR_TL paScWindowScissorTl;
        PA_SC_WINDOW_SCISSOR_BR paScWindowScissorBr;
    };

    static_assert(Util::CheckSequential({ mmPA_SC_WINDOW_SCISSOR_TL,
                                          mmPA_SC_WINDOW_SCISSOR_BR, }),
                  "GlobalScissor regs are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(GlobalScissorRegs, paScWindowScissorTl),
                                          offsetof(GlobalScissorRegs, paScWindowScissorBr), }, sizeof(uint32)),
                  "Storage order of GlobalScissorRegs is important!");

    static_assert(((PA_SC_WINDOW_SCISSOR_BR__BR_X_MASK | PA_SC_WINDOW_SCISSOR_BR__BR_Y_MASK) == UINT32_MAX) &&
                  ((PA_SC_WINDOW_SCISSOR_TL__TL_X_MASK | PA_SC_WINDOW_SCISSOR_TL__TL_Y_MASK) == UINT32_MAX),
                  "All register bits for the global scissor are expected to be defined!");

    // No need to zero init since all register bits are defined and set.
    GlobalScissorRegs regs;

    const Extent2d& extent = params.scissorRegion.extent;
    const Offset2d& offset = params.scissorRegion.offset;

    // Null scissor is defined as (maxScissorCoord, 0)
    const uint32 maxScissorCoord = m_deviceConfig.maxScissorSize - 1;

    regs.paScWindowScissorTl.bits.TL_X = (extent.width > 0)  ?
        Clamp<int32>(offset.x, 0, maxScissorCoord) : maxScissorCoord;
    regs.paScWindowScissorTl.bits.TL_Y = (extent.height > 0) ?
        Clamp<int32>(offset.y, 0, maxScissorCoord) : maxScissorCoord;

    // E.g., x = 0, width = 0xFFFFFFFF, int32 will be - 1 and clamp to 0, so use int64 clamp.
    regs.paScWindowScissorBr.bits.BR_X = (extent.width > 0)  ?
        uint32(Clamp<int64>(offset.x + extent.width - 1,  0, maxScissorCoord)) : 0;
    regs.paScWindowScissorBr.bits.BR_Y = (extent.height > 0) ?
        uint32(Clamp<int64>(offset.y + extent.height - 1, 0, maxScissorCoord)) : 0;

    m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmPA_SC_WINDOW_SCISSOR_TL, mmPA_SC_WINDOW_SCISSOR_BR, &regs);

    auto* const pPaScWalkAlignState = &m_gfxState.paScWalkAlignState;
    const bool  globalScissorIn64K  = TestAnyFlagSet(regs.paScWindowScissorBr.bits.BR_X |
                                                     regs.paScWindowScissorBr.bits.BR_Y, 1u << 15);

    if (pPaScWalkAlignState->globalScissorIn64K != uint32(globalScissorIn64K))
    {
        pPaScWalkAlignState->globalScissorIn64K = globalScissorIn64K ? 1 : 0;
        pPaScWalkAlignState->dirty              = 1;
    }
}

// Lookup table for converting PAL primitive topologies to VGT hardware enums.
constexpr VGT_DI_PRIM_TYPE TopologyToPrimTypeTable[] =
{
    DI_PT_POINTLIST,        // PointList
    DI_PT_LINELIST,         // LineList
    DI_PT_LINESTRIP,        // LineStrip
    DI_PT_TRILIST,          // TriangleList
    DI_PT_TRISTRIP,         // TriangleStrip
    DI_PT_RECTLIST,         // RectList
    DI_PT_QUADLIST,         // QuadList
    DI_PT_QUADSTRIP,        // QuadStrip
    DI_PT_LINELIST_ADJ,     // LineListAdj
    DI_PT_LINESTRIP_ADJ,    // LineStripAdj
    DI_PT_TRILIST_ADJ,      // TriangleListAdj
    DI_PT_TRISTRIP_ADJ,     // TriangleStripAdj
    DI_PT_PATCH,            // Patch
    DI_PT_TRIFAN,           // TriangleFan
    DI_PT_LINELOOP,         // LineLoop
    DI_PT_POLYGON,          // Polygon
    DI_PT_2D_RECTANGLE,     // TwoDRectList
};

constexpr uint32 IaRegOffsets[] =
{
    // UConfig
    mmGE_MULTI_PRIM_IB_RESET_EN,
    mmVGT_PRIMITIVE_TYPE,

    // Context
    mmVGT_MULTI_PRIM_IB_RESET_INDX,
    mmPA_SC_LINE_STIPPLE_RESET,
};

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    using IaRegs = RegPairHandler<decltype(IaRegOffsets), IaRegOffsets>;

    RegisterValuePair iaRegs[IaRegs::Size()];
    IaRegs::Init(iaRegs);

    auto* pGeMultiPrimIbResetEn = IaRegs::Get<mmGE_MULTI_PRIM_IB_RESET_EN, GE_MULTI_PRIM_IB_RESET_EN>(iaRegs);
    pGeMultiPrimIbResetEn->bits.RESET_EN = params.primitiveRestartEnable;

    // API difference.  When checking an index to see if it matches the resetIndx value, DX requires all 32 bits
    // are compared regardless of the index type (16-bit/32-bit) while Vulkan/OGL require only comparing the low
    // 16-bits if using 16-bit indices.
    pGeMultiPrimIbResetEn->bits.MATCH_ALL_BITS = params.primitiveRestartMatchAllBits;

    // API difference.  OpenGL requires the generated indices for an auto draw (i.e., CmdDraw()) cause a primitive
    // reset if they match the specified resetIndx.  Other APIs only enable this feature for indexed draws.
    pGeMultiPrimIbResetEn->bits.DISABLE_FOR_AUTO_INDEX = 1;

    const uint32 idx = static_cast<uint32>(params.topology);
    PAL_ASSERT(idx < ArrayLen(TopologyToPrimTypeTable));

    auto* pVgtPrimitiveType = IaRegs::Get<mmVGT_PRIMITIVE_TYPE, VGT_PRIMITIVE_TYPE>(iaRegs);
    pVgtPrimitiveType->bits.PRIM_TYPE = TopologyToPrimTypeTable[idx];

    PAL_ASSERT((params.topology == PrimitiveTopology::Patch) || (params.patchControlPoints == 0));
    pVgtPrimitiveType->bits.NUM_INPUT_CP = params.patchControlPoints;

    if (params.patchControlPoints > 0)
    {
        //  When patch input primitives are used without tessellation enabled, prim group size is only required
        //  to be (256 / patchControlPoints).
        pVgtPrimitiveType->bits.PRIMS_PER_SUBGROUP = (256 / params.patchControlPoints);
    }

    auto* pVgtMultiPrimIbResetIndx = IaRegs::Get<mmVGT_MULTI_PRIM_IB_RESET_INDX, VGT_MULTI_PRIM_IB_RESET_INDX>(iaRegs);
    pVgtMultiPrimIbResetIndx->bits.RESET_INDX = params.primitiveRestartIndex;

    const bool resetPrimitive = (params.topology == PrimitiveTopology::LineList) ||
                                (params.topology == PrimitiveTopology::LineListAdj);
    const uint32 autoResetMode = resetPrimitive ? 1 : 2;
    auto* pPaScLineStippleReset = IaRegs::Get<mmPA_SC_LINE_STIPPLE_RESET, PA_SC_LINE_STIPPLE_RESET>(iaRegs);
    pPaScLineStippleReset->bits.AUTO_RESET_CNTL = autoResetMode;

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
    static_assert(IaRegs::NumContext() != 0, "Expecting some number of context registers!");
    static_assert(IaRegs::NumOther()   != 0, "Expecting some number of uconfig registers!");
    static_assert(IaRegs::NumSh()      == 0, "Expecting no SH registers!");

    if ((m_buildFlags.optimizeGpuSmallBatch == 0) ||
        ((m_gfxState.paScLineStippleReset.u32All != pPaScLineStippleReset->u32All) ||
         (m_gfxState.vgtMultiPrimIbResetIndx.u32All != pVgtMultiPrimIbResetIndx->u32All) ||
         (m_gfxState.validBits.inputAssemblyCtxState == 0)))
    {
        pCmdSpace = CmdStream::WriteSetContextPairs(&iaRegs[IaRegs::FirstContextIdx()],
                                                    IaRegs::NumContext(),
                                                    pCmdSpace);

        if (m_buildFlags.optimizeGpuSmallBatch)
        {
            m_gfxState.paScLineStippleReset.u32All     = pPaScLineStippleReset->u32All;
            m_gfxState.vgtMultiPrimIbResetIndx.u32All  = pVgtMultiPrimIbResetIndx->u32All;
            m_gfxState.validBits.inputAssemblyCtxState = 1;
        }
    }

    pCmdSpace = CmdStream::WriteSetUConfigPairs(&iaRegs[IaRegs::FirstOtherIdx()], IaRegs::NumOther(), pCmdSpace);
    m_deCmdStream.CommitCommands(pCmdSpace);

    m_graphicsState.inputAssemblyState            = params;
    m_graphicsState.dirtyFlags.inputAssemblyState = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    PA_SU_SC_MODE_CNTL paSuScModeCntl = {};

    static_assert((static_cast<uint32>(FillMode::Points)    == 0) &&
                  (static_cast<uint32>(FillMode::Wireframe) == 1) &&
                  (static_cast<uint32>(FillMode::Solid)     == 2),
                  "FillMode vs. PA_SU_SC_MODE_CNTL.POLY_MODE mismatch");

    if (m_deviceConfig.tossPointMode != TossPointWireframe)
    {
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = uint32(params.frontFillMode);
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = uint32(params.backFillMode);
        paSuScModeCntl.bits.POLY_MODE            = (params.frontFillMode != Pal::FillMode::Solid) |
                                                   (params.backFillMode  != Pal::FillMode::Solid);
    }
    else
    {
        paSuScModeCntl.bits.POLYMODE_FRONT_PTYPE = uint32(FillMode::Wireframe);
        paSuScModeCntl.bits.POLYMODE_BACK_PTYPE  = uint32(FillMode::Wireframe);
        paSuScModeCntl.bits.POLY_MODE            = 1;
    }

    constexpr uint32 FrontCull = static_cast<uint32>(CullMode::Front);
    constexpr uint32 BackCull  = static_cast<uint32>(CullMode::Back);

    static_assert(((FrontCull | BackCull) == static_cast<uint32>(CullMode::FrontAndBack)),
                  "CullMode::FrontAndBack not a strict union of CullMode::Front and CullMode::Back");

    paSuScModeCntl.bits.CULL_FRONT = ((static_cast<uint32>(params.cullMode) & FrontCull) != 0);
    paSuScModeCntl.bits.CULL_BACK  = ((static_cast<uint32>(params.cullMode) & BackCull)  != 0);

    static_assert((static_cast<uint32>(FaceOrientation::Ccw) == 0) &&
                  (static_cast<uint32>(FaceOrientation::Cw)  == 1),
                  "FaceOrientation vs. PA_SU_SC_MODE_CNTL.FACE mismatch");

    paSuScModeCntl.bits.FACE = static_cast<uint32>(params.frontFace);

    static_assert((static_cast<uint32>(ProvokingVertex::First) == 0) &&
                  (static_cast<uint32>(ProvokingVertex::Last)  == 1),
                  "ProvokingVertex vs. PA_SU_SC_MODE_CNTL.PROVOKING_VTX_LAST mismatch");

    paSuScModeCntl.bits.PROVOKING_VTX_LAST = static_cast<uint32>(params.provokingVertex);

    paSuScModeCntl.bits.POLY_OFFSET_FRONT_ENABLE = params.flags.frontDepthBiasEnable;
    paSuScModeCntl.bits.POLY_OFFSET_BACK_ENABLE  = params.flags.backDepthBiasEnable;

    m_deCmdStream.AllocateAndBuildSetOneContextReg(mmPA_SU_SC_MODE_CNTL, paSuScModeCntl.u32All);

    m_graphicsState.triangleRasterState            = params;
    m_graphicsState.dirtyFlags.triangleRasterState = 1;

    m_gfxState.primShaderCullingCb.paSuScModeCntl = paSuScModeCntl.u32All;
    m_nggTable.state.dirty                        = 1;
}

// =====================================================================================================================
// Calculates the bytes per pixel of a Gfx12 color channel format
inline uint32 BytesPerPixel(
    ColorFormat format)
{
    uint32 bpp;

    switch (format)
    {
    case COLOR_8:
        bpp = 1;
        break;
    case COLOR_16:
    case COLOR_8_8:
    case COLOR_5_6_5:
    case COLOR_1_5_5_5:
    case COLOR_5_5_5_1:
    case COLOR_4_4_4_4:
        bpp = 2;
        break;
    case COLOR_32:
    case COLOR_16_16:
    case COLOR_10_11_11:
    case COLOR_11_11_10:
    case COLOR_10_10_10_2:
    case COLOR_2_10_10_10:
    case COLOR_8_8_8_8:
    case COLOR_5_9_9_9:
        bpp = 4;
        break;
    case COLOR_32_32:
    case COLOR_16_16_16_16:
        bpp = 8;
        break;
    case COLOR_32_32_32_32:
        bpp = 16;
        break;
    default:
        PAL_NEVER_CALLED();
        bpp = 4;
        break;
    }

    return bpp;
}

// =====================================================================================================================
// Calculate PBB bin sizes based on color target state
static Extent2d GetColorTargetBinSize(
    Extent2d                minBinSize,
    Extent2d                maxBinSizes,
    uint32                  colorBinSizeNumerator,
    const BindTargetParams& params)
{
    Extent2d binSize{};

    if (params.colorTargetCount > 0)
    {
        // Note: This logic is mostly copied from GFX9 HWL and is likely outdated for GFX11+
        uint32 cColor = 0;

        for (uint32 idx = 0; idx < params.colorTargetCount; idx++)
        {
            const auto* pColorView = static_cast<const ColorTargetView*>(params.colorTargets[idx].pColorTargetView);

            if (pColorView != nullptr)
            {
                // mMRT = (num_frag == 1) ? 1 : (ps_iter == 1) ? num_frag : 2
                //      - note: ps_iter is assumed to be 0 here to avoid cross-validation
                // cMRT = Bpp * mMRT
                // cColor = Sum(cMRT)
                const uint32 mmrt = (pColorView->Log2NumFragments() == 0 /* log2 */) ?
                                    1 : /* ps_iter == 0 */ 2;

                cColor += BytesPerPixel(pColorView->Format()) * mmrt;
            }
        }

        cColor = Max(cColor, 1u);  // cColor 0 to 1 uses cColor=1

        // The logic given to calculate the Color bin size is:
        //   colorBinArea = ((CcReadTags * totalNumRbs / totalNumPipes) * (CcTagSize * totalNumPipes)) / cColor
        // The numerator has been pre-calculated as m_colorBinSizeTagPart.
        const uint32 colorLog2Pixels = Log2(colorBinSizeNumerator / cColor);
        const uint32 colorBinSizeX = 1 << ((colorLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
        const uint32 colorBinSizeY = 1 << (colorLog2Pixels / 2);       // (Y_BIAS=false) round down height

        // Return size adjusted for minimum bin size
        binSize = { Max(colorBinSizeX, minBinSize.width), Max(colorBinSizeY, minBinSize.height) };
    }
    else
    {
        binSize = maxBinSizes;
    }

    return binSize;
}

// =====================================================================================================================
// Calculate PBB bin sizes based on depth stencil state
Extent2d GetDepthStencilBinSize(
    Extent2d                minBinSizes,
    Extent2d                maxBinSizes,
    uint32                  depthBinSizeTagPart,
    const BindTargetParams& params)
{
    // Note: This logic is mostly copied from GFX9 HWL and is likely outdated for GFX11+

    const auto* pDepthTargetView =
        static_cast<const DepthStencilView*>(params.depthTarget.pDepthStencilView);

    // This is as far as we'll go in figuring out if depth/stencil is enabled without cross-validation
    const bool depthEnabled = (params.depthTarget.pDepthStencilView != nullptr) &&
                              (params.depthTarget.depthLayout.usages != 0);

    const bool stencilEnabled = (params.depthTarget.pDepthStencilView != nullptr) &&
                                (params.depthTarget.depthLayout.usages != 0);

    Extent2d binSize;

    if (pDepthTargetView == nullptr ||
        (depthEnabled == false && stencilEnabled == false))
    {
        binSize = maxBinSizes;
    }
    else
    {
        // C_per_sample = ((z_enabled) ? 5 : 0) + ((stencil_enabled) ? 1 : 0)
        // cDepth = 4 * C_per_sample * num_samples

        const uint32 cPerDepthSample =
            (depthEnabled &&
             (pDepthTargetView->ZReadOnly() == false)) ? 5 : 0;
        const uint32 cPerStencilSample =
            (stencilEnabled &&
             ((pDepthTargetView->SReadOnly() == false))) ? 1 : 0;
        const uint32 cDepth = (cPerDepthSample + cPerStencilSample) * (1 << pDepthTargetView->NumSamples());

        // The logic for gfx10 bin sizes is based on a formula that accounts for the number of RBs
        // and Channels on the ASIC.  Since this a potentially large amount of combinations,
        // it is not practical to hardcode binning tables into the driver.
        // Note that final bin size is choosen from the minimum between Depth, Color and FMask.

        // The logic given to calculate the Depth bin size is:
        //   depthBinArea = ((ZsReadTags * totalNumRbs / totalNumPipes) * (ZsTagSize * totalNumPipes)) / cDepth
        // The numerator has been pre-calculated as m_depthBinSizeTagPart.
        // Note that cDepth 0 to 1 falls into cDepth=1 bucket
        const uint32 depthLog2Pixels = Log2(depthBinSizeTagPart / Max(cDepth, 1u));
        uint32       depthBinSizeX = 1 << ((depthLog2Pixels + 1) / 2); // (Y_BIAS=false) round up width
        uint32       depthBinSizeY = 1 << (depthLog2Pixels / 2);       // (Y_BIAS=false) round down height

        // Return size adjusted for minimum bin size
        binSize = { Max(depthBinSizeX, minBinSizes.width), Max(depthBinSizeY, minBinSizes.height) };
    }

    return binSize;
}

// =====================================================================================================================
// Calculate PBB bin sizes for a given combination of render targets
static Extent2d CalculatePbbBinSizes(
    Extent2d                minBinSizes,
    Extent2d                maxBinSizes,
    uint32                  colorBinSizeNumerator,
    uint32                  depthBinSizeNumerator,
    const BindTargetParams& params)
{
    Extent2d colorBinSizes = GetColorTargetBinSize(minBinSizes, maxBinSizes, colorBinSizeNumerator, params);
    Extent2d depthBinSizes = GetDepthStencilBinSize(minBinSizes, maxBinSizes, depthBinSizeNumerator, params);

    Extent2d binSizes;

    if (colorBinSizes.width * colorBinSizes.height <=
        depthBinSizes.width * depthBinSizes.height)
    {
        binSizes = colorBinSizes;
    }
    else
    {
        binSizes = depthBinSizes;
    }

    return binSizes;
}

// =====================================================================================================================
static BinSizeExtend BinSizeEnum(
    uint32 binSize)
{
    BinSizeExtend size;

    switch (binSize)
    {
    case 128:
        size = BIN_SIZE_128_PIXELS;
        break;
    case 256:
        size = BIN_SIZE_256_PIXELS;
        break;
    case 512:
        size = BIN_SIZE_512_PIXELS;
        break;
    default:
        PAL_NEVER_CALLED();
        size = BIN_SIZE_128_PIXELS;
        break;
    }

    return size;
}

// =====================================================================================================================
IColorTargetView* UniversalCmdBuffer::StoreColorTargetView(
    uint32                  slot,
    const BindTargetParams& params)
{
    PAL_ASSERT(params.colorTargets[slot].pColorTargetView != nullptr);

    IColorTargetView* pColorTargetView = nullptr;

    pColorTargetView = PAL_PLACEMENT_NEW(&m_colorTargetViewStorage[slot])
        ColorTargetView(*static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView));

    return pColorTargetView;
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyColorTargetViewStorage(
    ColorTargetViewStorage*       pColorTargetViewStorageDst,
    const ColorTargetViewStorage* pColorTargetViewStorageSrc,
    GraphicsState*                pGraphicsStateDst)
{
    if (pGraphicsStateDst->bindTargets.colorTargetCount > 0)
    {
        memcpy(pColorTargetViewStorageDst, pColorTargetViewStorageSrc,
            sizeof(ColorTargetViewStorage) * pGraphicsStateDst->bindTargets.colorTargetCount);

        for (uint32 slot = 0; slot < pGraphicsStateDst->bindTargets.colorTargetCount; ++slot)
        {
            // if the view pointer wasn't null, overwrite it with the new storage location
            if (pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView != nullptr)
            {
                pGraphicsStateDst->bindTargets.colorTargets[slot].pColorTargetView =
                    reinterpret_cast<IColorTargetView*>(&pColorTargetViewStorageDst[slot]);
            }
        }
    }
}

// =====================================================================================================================
IDepthStencilView* UniversalCmdBuffer::StoreDepthStencilView(
    const BindTargetParams& params)
{
    IDepthStencilView* pDepthStencilView = nullptr;

    if (params.depthTarget.pDepthStencilView != nullptr)
    {
        pDepthStencilView = PAL_PLACEMENT_NEW(&m_depthStencilViewStorage)
            DepthStencilView(*static_cast<const DepthStencilView*>(params.depthTarget.pDepthStencilView));
    }

    return pDepthStencilView;
}

// =====================================================================================================================
void UniversalCmdBuffer::CopyDepthStencilViewStorage(
    DepthStencilViewStorage*       pDepthStencilViewStorageDst,
    const DepthStencilViewStorage* pDepthStencilViewStorageSrc,
    GraphicsState*                 pGraphicsStateDst)
{
    if (pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView != nullptr)
    {
        memcpy(pDepthStencilViewStorageDst, pDepthStencilViewStorageSrc, sizeof(DepthStencilViewStorage));

        pGraphicsStateDst->bindTargets.depthTarget.pDepthStencilView =
                reinterpret_cast<IDepthStencilView*>(pDepthStencilViewStorageDst);
    }
}

constexpr uint32 GenericScissorRegs[] =
{
    mmPA_SC_GENERIC_SCISSOR_TL,
    mmPA_SC_GENERIC_SCISSOR_BR
};

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    m_previousTargetsMetadata = m_currentTargetsMetadata; // Save previous target bind metadata before updating current.

    using GenericScissor = RegPairHandler<decltype(GenericScissorRegs), GenericScissorRegs>;

    RegisterValuePair scissor[GenericScissor::Size()];
    GenericScissor::Init(scissor);

    // Default to fully open
    Extent2d targetsExtent    = { m_deviceConfig.maxScissorSize , m_deviceConfig.maxScissorSize };

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Insert a single packet for all context registers
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Save off a location for a single SET_PAIRS header for all Ctx regs written for this bind
    uint32* const pSetPairsHeader = pCmdSpace;
    pCmdSpace += 1;

    const BindTargetParams& bindTargets             = m_graphicsState.bindTargets;
    const uint32            colorTargetLimit        = Max(params.colorTargetCount, bindTargets.colorTargetCount);
    bool                    colorTargetsChanged     = false;
    uint32                  updatedColorTargetCount = 0;
    uint32                  newColorTargetMask      = 0;

    m_currentTargetsMetadata.numMrtsBound   = params.colorTargetCount;
    m_currentTargetsMetadata.patchedAlready = false;

    for (uint32 slot = 0; slot < colorTargetLimit; slot++)
    {
        const auto*      pNewView = static_cast<const ColorTargetView*>(params.colorTargets[slot].pColorTargetView);
        const auto*const pOldView = static_cast<const ColorTargetView*>(bindTargets.colorTargets[slot].pColorTargetView);

        if ((pOldView != nullptr) && (pOldView->Equals(pNewView) == false))
        {
            colorTargetsChanged = true;
        }

        if ((slot < params.colorTargetCount) && (pNewView != nullptr))
        {
            pCmdSpace = pNewView->CopyRegPairsToCmdSpace(slot, pCmdSpace,
                                                         &m_writeCbDbHighBaseRegs, GetDevice());

            Extent2d slotExtent = pNewView->Extent();

            // For mixed MRT resolutions, we must use the smallest resolution.
            // Otherwise, we are asking for bad memory accesses relative to the smaller resources.
            // Vulkan and D3D12 support mixed resolution render targets, while DX relaxed it's rules for VulkanOn12.
            targetsExtent.height = Min(targetsExtent.height, slotExtent.height);
            targetsExtent.width  = Min(targetsExtent.width , slotExtent.width);

            // Save updated bindTargets state
            //  For consistancy ensure we only save colorTargets within the valid target count specified, and set
            //  unbound target slots as empty/null.  This allows simple slot change comparisons above and elsewhere.
            //  Handle cases where callers may supply input like:
            //     colorTargetCount=4 {view, null, null,null} --> colorTargetCount=1 {view,null,...}
            //     colorTargetCount=0 {view1,view2,null,null} --> colorTargetCount=0 {null,null,...}
            m_graphicsState.bindTargets.colorTargets[slot].imageLayout      = params.colorTargets[slot].imageLayout;
            m_graphicsState.bindTargets.colorTargets[slot].pColorTargetView = StoreColorTargetView(slot, params);
            updatedColorTargetCount = slot + 1;  // track last actual bound slot
            newColorTargetMask |= 1 << slot;

            m_currentTargetsMetadata.pImage[slot] = pNewView->GetImage();
        }
        else
        {
            m_graphicsState.bindTargets.colorTargets[slot] = {};

            if (BitfieldIsSet(m_graphicsState.boundColorTargetMask, slot) &&
                // If optimizeDepthOnlyFmt - we only want to write CB_COLOR0_INFO if this is nested.
                ((m_deviceConfig.optimizeDepthOnlyFmt == 0) || (slot != 0) || IsNested()))
            {
                RegisterValuePair nullCbColorInfo;

                static_assert(Util::CheckSequential({
                    mmCB_COLOR0_INFO, mmCB_COLOR1_INFO, mmCB_COLOR2_INFO, mmCB_COLOR3_INFO,
                    mmCB_COLOR4_INFO, mmCB_COLOR5_INFO, mmCB_COLOR6_INFO, mmCB_COLOR7_INFO}),
                    "The ordering of the CB_COLOR#_INFO regs changed!");

                nullCbColorInfo.offset = (Chip::mmCB_COLOR0_INFO - Chip::CONTEXT_SPACE_START) + slot;
                nullCbColorInfo.value  = 0;
                memcpy(pCmdSpace, &nullCbColorInfo, sizeof(nullCbColorInfo));

                pCmdSpace += 2;
            }

            m_currentTargetsMetadata.pImage[slot] = nullptr;
        }
    }

    const auto*const pNewDepthView = static_cast<const DepthStencilView*>(params.depthTarget.pDepthStencilView);
    bool             hasHiSZ       = false;

    if (pNewDepthView != nullptr)
    {
        pCmdSpace = pNewDepthView->CopyRegPairsToCmdSpace(params.depthTarget.depthLayout,
                                                          params.depthTarget.stencilLayout,
                                                          pCmdSpace,
                                                          &m_writeCbDbHighBaseRegs);
        hasHiSZ   = pNewDepthView->HiSZEnabled();

        const Extent2d dsvExtent = pNewDepthView->Extent();
        PAL_ASSERT((dsvExtent.width > 0) && (dsvExtent.height > 0));

        targetsExtent.width  = Min(targetsExtent.width,  dsvExtent.width);
        targetsExtent.height = Min(targetsExtent.height, dsvExtent.height);

        m_gfxState.dbRenderOverride = pNewDepthView->DbRenderOverride();
        m_gfxState.szValid          = pNewDepthView->SZValid();

        // Despite the function name, this returns Log2NumSamples.
        m_gfxState.dsLog2NumSamples = pNewDepthView->NumSamples();
    }
    else
    {
        pCmdSpace = DepthStencilView::CopyNullRegPairsToCmdSpace(pCmdSpace,
            (m_deviceConfig.stateFilterFlags & Gfx12RedundantStateFilterNullDsvMinimumState) != 0);
        m_gfxState.dbRenderOverride = {};
        m_gfxState.dsLog2NumSamples = 0;
        m_gfxState.szValid          = false;
    }

    m_gfxState.validBits.dbRenderOverride = 1;

    static_assert(GenericScissor::Size() == GenericScissor::NumContext(), "No other register types expected.");

    auto* pBr = GenericScissor::Get<mmPA_SC_GENERIC_SCISSOR_BR, PA_SC_GENERIC_SCISSOR_BR>(scissor);

    pBr->bits.BR_X = targetsExtent.width  - 1;
    pBr->bits.BR_Y = targetsExtent.height - 1;

    auto* const pPaScWalkAlignState = &m_gfxState.paScWalkAlignState;
    const bool  targetIn64K         = TestAnyFlagSet(pBr->bits.BR_X | pBr->bits.BR_Y, 1u << 15);

    if ((pPaScWalkAlignState->hasHiSZ     != uint32(hasHiSZ)) ||
        (pPaScWalkAlignState->targetIn64K != uint32(targetIn64K)))
    {
        pPaScWalkAlignState->hasHiSZ     = hasHiSZ     ? 1 : 0;
        pPaScWalkAlignState->targetIn64K = targetIn64K ? 1 : 0;
        pPaScWalkAlignState->dirty       = 1;
    }

    memcpy(pCmdSpace, scissor, sizeof(scissor));
    pCmdSpace += sizeof(scissor) / sizeof(uint32);

    // Add reg pairs for CB Temporal Hint regs
    if (params.colorTargetCount > 0)
    {
        m_currentTargetsMetadata.pCbMemInfoPairsCmdSpace = pCmdSpace;

        static_assert(Util::CheckSequential({ mmCB_MEM0_INFO, mmCB_MEM1_INFO, mmCB_MEM2_INFO, mmCB_MEM3_INFO,
                                              mmCB_MEM4_INFO, mmCB_MEM5_INFO, mmCB_MEM6_INFO, mmCB_MEM7_INFO }),
                     "Unexpected offset of registers CB_MEMn_INFO.");

        regCB_MEM0_INFO cbMemInfo = {};

        cbMemInfo.bits.TEMPORAL_READ  = m_deviceConfig.gfx12TemporalHintsMrtRead;
        cbMemInfo.bits.TEMPORAL_WRITE = m_deviceConfig.gfx12TemporalHintsMrtWrite;

        // Add reg pairs to the command stream for each bound MRT slot.
        for (uint32 slot = 0; slot < params.colorTargetCount; slot++)
        {
            pCmdSpace[0] = mmCB_MEM0_INFO - CONTEXT_SPACE_START + slot;
            pCmdSpace[1] = cbMemInfo.u32All;
            pCmdSpace += 2;
        }
    }
    else
    {
        m_currentTargetsMetadata.pCbMemInfoPairsCmdSpace = nullptr;
    }

    // Go back and write the packet header now that we know how many RegPairs got added
    // (pSetPairsHeader + 1) not needed
    const uint32 numRegPairs = uint32(VoidPtrDiff(pCmdSpace, pSetPairsHeader) / sizeof(RegisterValuePair));
    void* pThrowAway;
    const size_t pktSize = CmdUtil::BuildSetContextPairsHeader(numRegPairs, &pThrowAway, pSetPairsHeader);
    PAL_ASSERT(pktSize == size_t(pCmdSpace - pSetPairsHeader));

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // End of SET_CONTEXT_REG_PAIRS pkt
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    const auto*const pOldDepthView = static_cast<const DepthStencilView*>(bindTargets.depthTarget.pDepthStencilView);
    const bool depthTargetChanged  = (pOldDepthView != nullptr) && (pOldDepthView->Equals(pNewDepthView) == false);

    if ((m_contextStatesPerBin > 1) && (colorTargetsChanged || depthTargetChanged))
    {
        // If the slice-index as programmed by the CB is changing, then we have to flush DFSM stuff. This isn't
        // necessary if DFSM is disabled.
        //
        // ("it" refers to the RT-index, the HW perspective of which slice is being rendered to. The RT-index is
        //  a combination of the CB registers and the GS output).
        //
        //  If the GS (HW VS) is changing it, then there is only one view, so no batch break is needed..  If any
        //  of the RT views are changing, the DFSM has no idea about it and there isn't any one single RT_index
        //  to keep track of since each RT may have a different view with different STARTs and SIZEs that can be
        //  independently changing.  The DB and Scan Converter also doesn't know about the CB's views changing.
        //  This is why there should be a batch break on RT view changes.  The other reason is that binning and
        //  deferred shading can't give any benefit when the bound RT views of consecutive contexts are not
        //  intersecting.  There is no way to increase cache hit ratios if there is no way to generate the same
        //  address between draws, so there is no reason to enable binning.
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(BREAK_BATCH, EngineTypeUniversal, pCmdSpace);
    }

    Extent2d pbbBinSizes = CalculatePbbBinSizes(m_deviceConfig.pbb.minBinSize,
                                                m_deviceConfig.pbb.maxBinSize,
                                                m_deviceConfig.pbb.colorBinSizeNumerator,
                                                m_deviceConfig.pbb.depthBinSizeNumerator,
                                                params);

    PAL_ASSERT(IsNested() == false); // Bind targets should never be called on a nested cmdbuffer.
    pCmdSpace = UpdateBatchBinnerState(BinningMode(m_gfxState.batchBinnerState.paScBinnerCntl0.bits.BINNING_MODE),
                                       BinSizeEnum(pbbBinSizes.width),
                                       BinSizeEnum(pbbBinSizes.height),
                                       pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);

    // We may have updated CB_COLOR0_INFO - invalidate it. We could update it's state here but it isn't really worth it.
    // BindTargets is pretty infrequent compared to BindPipeline. The need for the CB_COLOR0_INFO filter is for
    // avoiding redundant context rolls on pipeline changes during shadow passes when we apply the depth only opt.
    m_gfxState.validBits.cbColor0Info = 0;

    m_graphicsState.bindTargets.colorTargetCount              = updatedColorTargetCount;
    m_graphicsState.bindTargets.depthTarget.depthLayout       = params.depthTarget.depthLayout;
    m_graphicsState.bindTargets.depthTarget.stencilLayout     = params.depthTarget.stencilLayout;
    m_graphicsState.bindTargets.depthTarget.pDepthStencilView = StoreDepthStencilView(params);
    m_graphicsState.dirtyFlags.colorTargetView                = 1;
    m_graphicsState.dirtyFlags.depthStencilView               = 1;
    m_graphicsState.boundColorTargetMask                      = newColorTargetMask;
    m_graphicsState.targetExtent                              = targetsExtent;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::UpdateBatchBinnerState(
    BinningMode   binningMode,
    BinSizeExtend binSizeX,
    BinSizeExtend binSizeY,
    uint32*       pCmdSpace)
{
    PA_SC_BINNER_CNTL_0 paScBinnerCntl0 = {};

    paScBinnerCntl0.u32All = m_gfxState.batchBinnerState.paScBinnerCntl0.u32All;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 875
    if (m_deviceConfig.binningMode == DeferredBatchBinCustom)
#else
    if (m_deviceConfig.binningMode == DeferredBatchBinDisabled)
    {
        binningMode = BINNING_DISABLED;
    }
    else if (m_deviceConfig.binningMode == DeferredBatchBinCustom)
#endif
    {
        binSizeX = BinSizeExtend(m_deviceConfig.customBatchBinSize >> 16);
        binSizeY = BinSizeExtend(m_deviceConfig.customBatchBinSize & 0xFFFF);
    }

    // Update dynamic portions.
    paScBinnerCntl0.bits.BINNING_MODE = binningMode;

    if (binningMode == BINNING_DISABLED)
    {
        // If binning is disabled then the bin size fields are ignored by HW. We can set these fields to anything we
        // want. However, it's useful to normalize all "binning disabled" registers to one specific bin configuration.
        // This is the most simple way to guarantee that:
        // 1. We skip logically redundant register writes below, even if the register bits are not identical.
        // 2. m_gfxState's paScBinnerCntl0 is an exact match to the HW's current register value.
        //
        // Note that we only need separate binSizeX/Y state tracking because this code discards our most recent bin
        // size calculation when binning is disabled. If we removed this code, completely decoupling the BIN_SIZE and
        // BINNING_MODE fields then we could also remove binSizeX/Y. Currently it seems like the invariants listed
        // above are worth a bit of extra state tracking.
        paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = BIN_SIZE_128_PIXELS;
        paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = BIN_SIZE_128_PIXELS;
    }
    else
    {
        paScBinnerCntl0.bits.BIN_SIZE_X_EXTEND = binSizeX;
        paScBinnerCntl0.bits.BIN_SIZE_Y_EXTEND = binSizeY;
    }

    // Record the intended bin size even if we forced it to 128x128 above. This means we can recover the intended
    // bin size if we bind a new pipeline that enables binning.
    m_gfxState.batchBinnerState.binSizeX = binSizeX;
    m_gfxState.batchBinnerState.binSizeY = binSizeY;

    if ((paScBinnerCntl0.u32All != m_gfxState.batchBinnerState.paScBinnerCntl0.u32All) ||
        (m_gfxState.validBits.batchBinnerState == 0))
    {
        pCmdSpace = CmdStream::WriteSetOneContextReg(mmPA_SC_BINNER_CNTL_0, paScBinnerCntl0.u32All, pCmdSpace);

        // Update tracked state
        m_gfxState.batchBinnerState.paScBinnerCntl0.u32All = paScBinnerCntl0.u32All;
        m_gfxState.validBits.batchBinnerState = 1;
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Helper function which is responsible for making sure all user-data entries are written to either the spill table or
// to user-SGPR's, as well as making sure that all indirect user-data tables are up-to-date in GPU memory.  Part of
// Draw-time validation.  This version uses the CPU & embedded data for user-data table management.
// Additionally validates pipeline owned persistent state so that we can pack it in the same packet with user data.
template <bool HasPipelineChanged, bool Indirect>
uint32* UniversalCmdBuffer::ValidateGraphicsPersistentState(
    const ValidateDrawInfo& drawInfo,
    uint32*                 pCmdSpace)
{
    const GraphicsUserDataLayout* const pPrevGfxUserDataLayout = m_pPrevGfxUserDataLayoutValidatedWith;
    const auto* const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const GraphicsUserDataLayout* const pCurrentGfxUserDataLayout = pNewPipeline->UserDataLayout();

    PAL_ASSERT(pCurrentGfxUserDataLayout != nullptr);
    PAL_ASSERT(HasPipelineChanged || (pPrevGfxUserDataLayout != nullptr));

    m_nggTable.state.dirty |=
        (UpdateNggPrimCb(pNewPipeline, &m_gfxState.primShaderCullingCb) || (pPrevGfxUserDataLayout == nullptr));

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Insert a single packet for all persistent state registers
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Save off a location for a single SET_PAIRS header for all SH regs written for this bind
    uint32* const pSetPairsHeader = pCmdSpace;
    pCmdSpace += 1;

    // Step #1:
    // If the stream-out table or vertex buffer table were updated since the previous Draw, and are referenced by the
    // current pipeline, they must be relocated to a new location in GPU memory and re-uploaded by the CPU.
    const UserDataReg vertexBufTblUserDataReg = pCurrentGfxUserDataLayout->GetVertexBufferTable();
    if ((vertexBufTblUserDataReg.u32All != UserDataNotMapped) && (m_vbTable.watermarkInDwords > 0))
    {
        // NOTE: If the pipeline is changing and the previous pipeline's mapping for the VB table doesn't match the
        // current pipeline's, we need to re-write the GPU virtual address even if we don't re-upload the table.
        bool gpuAddrDirty = (HasPipelineChanged &&
            ((pPrevGfxUserDataLayout == nullptr) ||
             (pPrevGfxUserDataLayout->GetVertexBufferTable().u32All != vertexBufTblUserDataReg.u32All)));

        if (m_vbTable.gpuState.dirty)
        {
            m_vbTable.gpuState.sizeInDwords = m_vbTable.watermarkInDwords;

            UpdateUserDataTableCpu(&m_vbTable.gpuState,
                                   m_vbTable.watermarkInDwords,
                                   0,
                                   reinterpret_cast<const uint32*>(m_vbTable.srds));
            gpuAddrDirty = true;
        }

        if (gpuAddrDirty)
        {
            pCmdSpace[0] = vertexBufTblUserDataReg.regOffset;
            pCmdSpace[1] = LowPart(m_vbTable.gpuState.gpuVirtAddr);
            pCmdSpace += 2;
        }
    } // if vertex buffer table is mapped by current pipeline

    const UserDataReg soTableUserDataReg = pCurrentGfxUserDataLayout->GetStreamoutTable();

    if (soTableUserDataReg.u32All != UserDataNotMapped) [[unlikely]]
    {
        bool gpuAddrDirty = (HasPipelineChanged && ((pPrevGfxUserDataLayout == nullptr) ||
                            (pPrevGfxUserDataLayout->GetStreamoutTable().u32All != soTableUserDataReg.u32All)));
        if (m_streamOut.state.dirty)
        {
            m_streamOut.state.sizeInDwords = sizeof(m_streamOut.srd) / sizeof(uint32);

            UpdateUserDataTableCpu(&m_streamOut.state,
                                   sizeof(m_streamOut.srd) / sizeof(uint32),
                                   0,
                                   reinterpret_cast<const uint32*>(&m_streamOut.srd[0]));

            gpuAddrDirty = true;
        }

        if (gpuAddrDirty)
        {
            pCmdSpace[0] = soTableUserDataReg.regOffset;
            pCmdSpace[1] = LowPart(m_streamOut.state.gpuVirtAddr);
            pCmdSpace += 2;
        }
    }

    const UserDataReg soCtrlBufRegAddr = pCurrentGfxUserDataLayout->GetStreamoutCtrlBuf();

    if ((soCtrlBufRegAddr.u32All != UserDataNotMapped) && HasPipelineChanged) [[unlikely]]
    {
        PAL_ASSERT(m_streamoutCtrlBuf != 0);

        pCmdSpace[0] = soCtrlBufRegAddr.regOffset;
        pCmdSpace[1] = LowPart(m_streamoutCtrlBuf);
        pCmdSpace += 2;
    }

    const UserDataReg sampleInfoAddr = pCurrentGfxUserDataLayout->GetSampleInfo();

    if (sampleInfoAddr.u32All != UserDataNotMapped) [[unlikely]]
    {
        // We also need to update ApiSampleInfo in case the quadSamplePatternState changes between two draws.
        if (HasPipelineChanged || m_graphicsState.dirtyFlags.quadSamplePatternState)
        {
            Abi::ApiSampleInfo sampleInfo;
            sampleInfo.numSamples = m_graphicsState.numSamplesPerPixel;
            sampleInfo.samplePatternIdx = Log2(m_graphicsState.numSamplesPerPixel) * MaxMsaaRasterizerSamples;

            pCmdSpace[0] = sampleInfoAddr.regOffset;
            pCmdSpace[1] = sampleInfo.u32All;
            pCmdSpace += 2;
        }
    }

    const MultiUserDataReg compositeData = pCurrentGfxUserDataLayout->GetCompositeData();
    if (compositeData.u32All != UserDataNotMapped) [[unlikely]]
    {
        bool isDirty = m_graphicsState.dirtyFlags.quadSamplePatternState ||
                       m_graphicsState.dirtyFlags.inputAssemblyState ||
                       m_graphicsState.dirtyFlags.colorBlendState;
        if (HasPipelineChanged || isDirty)
        {
            Abi::ApiCompositeDataValue registerVal = {};
            const auto* const pGraPipeline =
                static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
            if (pGraPipeline->GetOutputNumVertices() != 0)
            {
                PAL_ASSERT(pGraPipeline->IsGsEnabled() || pGraPipeline->IsTessEnabled() ||
                    pGraPipeline->HasMeshShader());
                registerVal.primInfo = pGraPipeline->GetOutputNumVertices();
            }
            else
            {
                // only VS
                registerVal.primInfo = GfxDevice::VertsPerPrimitive(m_graphicsState.inputAssemblyState.topology);
            }
            registerVal.numSamples = m_graphicsState.numSamplesPerPixel;

            const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicState;

            registerVal.dynamicSourceBlend = dynamicState.enable.dualSourceBlendEnable &&
                                             dynamicState.dualSourceBlendEnable;

            for (uint32 compositeReg = compositeData.u32All; compositeReg != 0; compositeReg >>= 10)
            {
                const uint16 compositeRegAddr = compositeReg & 0x3FF;
                if (compositeRegAddr != UserDataNotMapped)
                {
                    pCmdSpace[0] = compositeRegAddr;
                    pCmdSpace[1] = registerVal.u32All;
                    pCmdSpace += 2;
                }
            }
        }
    }

    const UserDataReg colorExportAddr = pCurrentGfxUserDataLayout->GetColorExportAddr();

    if ((colorExportAddr.u32All != UserDataNotMapped) && HasPipelineChanged) [[unlikely]]
    {
        const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        const DynamicGraphicsState& dynamicState = m_graphicsState.dynamicState;

        const bool dynamicDualSourceBlendEnabled = dynamicState.dualSourceBlendEnable &&
                                                   dynamicState.enable.dualSourceBlendEnable;

        pCmdSpace[0] = colorExportAddr.regOffset;
        pCmdSpace[1] = LowPart(pPipeline->ColorExportGpuVa(dynamicDualSourceBlendEnabled ?
                                                           ColorExportShaderType::DualSourceBlendEnable :
                                                           ColorExportShaderType::Default));
        pCmdSpace += 2;
    }

    const UserDataReg primsNeededCnt = pCurrentGfxUserDataLayout->GetPrimNeededCnt();

    if ((primsNeededCnt.u32All != UserDataNotMapped) &&
        ((m_graphicsState.dirtyFlags.streamoutStatsQuery == 1) || HasPipelineChanged)) [[unlikely]]
    {
        uint32 queryActiveFlag = static_cast<uint32>(IsQueryActive(QueryPoolType::StreamoutStats));

        pCmdSpace[0] = primsNeededCnt.regOffset;
        pCmdSpace[1] = queryActiveFlag;
        pCmdSpace += 2;
    }

    if (HasPipelineChanged)
    {
        pCmdSpace = pNewPipeline->CopyShRegPairsToCmdSpace(m_graphicsState.dynamicGraphicsInfo,
                                                           pCmdSpace);
    }

    if (HasPipelineChanged || (Indirect == false) || (Indirect && (drawInfo.multiIndirectDraw == false)))
    {
        const UserDataReg vertexBaseReg       = pCurrentGfxUserDataLayout->GetVertexBase();
        const UserDataReg instanceBaseReg     = pCurrentGfxUserDataLayout->GetInstanceBase();
        const UserDataReg drawIndexReg        = pCurrentGfxUserDataLayout->GetDrawIndex();
        const UserDataReg meshDispatchDimsReg = pCurrentGfxUserDataLayout->GetMeshDispatchDims();
        const UserDataReg nggCullingDataReg   = pCurrentGfxUserDataLayout->GetNggCullingData();

        // The pipeline controls how various internal userdata values are mapped to HW regs.
        // We need to update our cached off HW reg offsets if the PSO changes and invalidate any filtering logic
        // if these mappings have changed.
        if (HasPipelineChanged)
        {
            // Cache this off as Indirect draws will need these.
            if (m_gfxState.vertexOffsetReg != vertexBaseReg.regOffset)
            {
                m_gfxState.vertexOffsetReg = vertexBaseReg.regOffset;
                m_gfxState.validBits.firstVertex   = 0;

                // Invalid firstInstance also because it immediately follows the vertex offset register
                m_gfxState.validBits.firstInstance = 0;
            }

            if (m_gfxState.drawIndexReg != drawIndexReg.regOffset)
            {
                m_gfxState.drawIndexReg = drawIndexReg.regOffset;
                m_gfxState.validBits.drawIndex = 0;
            }

            if (m_gfxState.nggCullingDataReg != nggCullingDataReg.regOffset)
            {
                m_gfxState.nggCullingDataReg = nggCullingDataReg.regOffset;
                m_nggTable.state.dirty       = 1;
            }

            if (m_gfxState.meshDispatchDimsReg != meshDispatchDimsReg.regOffset)
            {
                m_gfxState.meshDispatchDimsReg = meshDispatchDimsReg.regOffset;
                m_gfxState.validBits.meshDispatchDims = 0;
            }

            // There is no redundant filtering (or even valid bit) for viewIds so no need to invalidate if the mapping
            // changed. The way this is used it will effectively always be non-redundant for most use cases.
            m_gfxState.viewIdsReg = pCurrentGfxUserDataLayout->GetViewId();
        }

        if (Indirect == false)
        {
            if (vertexBaseReg.u32All != UserDataNotMapped)
            {
                if ((m_gfxState.validBits.firstVertex == 0) ||
                    (m_gfxState.drawArgs.firstVertex != drawInfo.firstVertex))
                {
                    pCmdSpace[0] = vertexBaseReg.regOffset;
                    pCmdSpace[1] = drawInfo.firstVertex;
                    pCmdSpace += 2;

                    m_gfxState.validBits.firstVertex = 1;
                    m_gfxState.drawArgs.firstVertex  = drawInfo.firstVertex;
                }
            }

            if (instanceBaseReg.u32All != UserDataNotMapped)
            {
                if ((m_gfxState.validBits.firstInstance == 0) ||
                    (m_gfxState.drawArgs.firstInstance != drawInfo.firstInstance))
                {
                    pCmdSpace[0] = instanceBaseReg.regOffset;
                    pCmdSpace[1] = drawInfo.firstInstance;
                    pCmdSpace += 2;

                    m_gfxState.validBits.firstInstance = 1;
                    m_gfxState.drawArgs.firstInstance  = drawInfo.firstInstance;
                }
            }

            if (meshDispatchDimsReg.u32All != UserDataNotMapped) [[unlikely]]
            {
                if ((m_gfxState.validBits.meshDispatchDims == 0) ||
                    (memcmp(&(m_gfxState.drawArgs.meshDispatchDims),
                            &(drawInfo.meshDispatchDims),
                            sizeof(DispatchDims)) != 0))
                {
                    pCmdSpace[0] = meshDispatchDimsReg.regOffset;
                    pCmdSpace[1] = drawInfo.meshDispatchDims.x;
                    pCmdSpace[2] = meshDispatchDimsReg.regOffset + 1;
                    pCmdSpace[3] = drawInfo.meshDispatchDims.y;
                    pCmdSpace[4] = meshDispatchDimsReg.regOffset + 2;
                    pCmdSpace[5] = drawInfo.meshDispatchDims.z;
                    pCmdSpace += 6;

                    m_gfxState.validBits.meshDispatchDims = 1;
                    m_gfxState.drawArgs.meshDispatchDims  = drawInfo.meshDispatchDims;
                }
            }
        }

        if ((drawIndexReg.u32All != UserDataNotMapped) &&
            ((Indirect == false) || (drawInfo.multiIndirectDraw == false))) [[unlikely]]
        {
            if ((m_gfxState.validBits.drawIndex == 0) ||
                (m_gfxState.drawArgs.drawIndex != drawInfo.drawIndex))
            {
                pCmdSpace[0] = drawIndexReg.regOffset;
                pCmdSpace[1] = drawInfo.drawIndex;
                pCmdSpace += 2;

                m_gfxState.validBits.drawIndex = 1;
                m_gfxState.drawArgs.drawIndex  = drawInfo.drawIndex;
            }
        }
    }

    // Mark all these states invalid since Indirect draws update them from GPU memory.
    if (Indirect)
    {
        m_gfxState.validBits.firstVertex      = 0;
        m_gfxState.validBits.firstInstance    = 0;
        m_gfxState.validBits.meshDispatchDims = 0;

        if (drawInfo.multiIndirectDraw)
        {
            m_gfxState.validBits.drawIndex = 0;
        }
    }

    // Step #2:
    // Validate user-data entries and map dirty entries to their mapped user SGPR's.
    pCmdSpace = ValidateGraphicsUserData<HasPipelineChanged
        >(pPrevGfxUserDataLayout,
        pCurrentGfxUserDataLayout,
        pCmdSpace);

    // (pSetPairsHeader + 1) not needed
    const uint32 numRegPairs = uint32(VoidPtrDiff(pCmdSpace, pSetPairsHeader) / sizeof(RegisterValuePair));
    if (numRegPairs > 0)
    {
        // Go back and write the packet header now that we know how many RegPairs got added
        void* pThrowAway;
        const size_t pktSize = CmdUtil::BuildSetShPairsHeader<ShaderGraphics>(numRegPairs,
                                                                              &pThrowAway,
                                                                              pSetPairsHeader);
        PAL_ASSERT(pktSize == uint32(pCmdSpace - pSetPairsHeader));
    }
    else
    {
        // Remove reserved space for header!
        pCmdSpace -= 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // End of SET_SH_REG_PAIRS pkt
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    m_pPrevGfxUserDataLayoutValidatedWith = pNewPipeline->UserDataLayout();

    return pCmdSpace;
}

// =====================================================================================================================
template <bool HasPipelineChanged
    >
uint32* UniversalCmdBuffer::ValidateGraphicsUserData(
    const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
    const GraphicsUserDataLayout* pCurrentGfxUserDataLayout,
    uint32*                       pCmdSpace)
{
    UserDataEntries& userDataEntries =
        m_graphicsState.gfxUserDataEntries;
    UserDataTableState& userDataTable =
        m_spillTable.stateGfx;

    const bool anyUserDataDirty = IsAnyUserDataDirty(&userDataEntries);

    if (HasPipelineChanged || anyUserDataDirty)
    {
        pCmdSpace = pCurrentGfxUserDataLayout->CopyUserDataPairsToCmdSpace<HasPipelineChanged>(
            pPrevGfxUserDataLayout,
            userDataEntries.dirty,
            userDataEntries.entries,
            pCmdSpace);

        MultiUserDataReg spillTableUserDataReg = pCurrentGfxUserDataLayout->GetSpillTable();

        if (spillTableUserDataReg.u32All != UserDataNotMapped)
        {
            bool         reUpload       = false;
            const uint16 spillThreshold = pCurrentGfxUserDataLayout->GetSpillThreshold();
            const uint32 userDataLimit  = pCurrentGfxUserDataLayout->GetUserDataLimit();

            userDataTable.sizeInDwords = userDataLimit;

            PAL_ASSERT(userDataLimit > 0);
            const uint16 lastUserData = (userDataLimit - 1);

            PAL_ASSERT(userDataTable.dirty == 0); // Not ever setting this today.

            if (HasPipelineChanged &&
                ((pPrevGfxUserDataLayout == nullptr) ||
                 (spillThreshold != pPrevGfxUserDataLayout->GetSpillThreshold()) ||
                 (userDataLimit > pPrevGfxUserDataLayout->GetUserDataLimit())))
            {
                // If the pipeline is changing and the start offset or size of the spilled region is changed, reupload.
                reUpload = true;
            }
            else if (anyUserDataDirty)
            {
                // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
                const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
                const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
                for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
                {
                    size_t dirtyMask = userDataEntries.dirty[maskId];
                    if (maskId == firstMaskId)
                    {
                        // Ignore the dirty bits for any entries below the spill threshold.
                        const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                        dirtyMask &= ~BitfieldGenMask(static_cast<size_t>(firstEntryInMask));
                    }
                    if (maskId == lastMaskId)
                    {
                        // Ignore the dirty bits for any entries beyond the user-data limit.
                        const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                        dirtyMask &= BitfieldGenMask(static_cast<size_t>(lastEntryInMask + 1));
                    }

                    if (dirtyMask != 0)
                    {
                        reUpload = true;
                        break; // We only care if *any* spill table contents change!
                    }
                } // for each wide-bitfield sub-mask
            }

            if (reUpload)
            {
                UpdateUserDataTableCpu(&userDataTable,
                                       (userDataLimit - spillThreshold),
                                       spillThreshold,
                                       &(userDataEntries.entries[0]));

            }

            if (HasPipelineChanged || reUpload)
            {
                const uint32 gpuVirtAddrLo = LowPart(userDataTable.gpuVirtAddr);

                while (spillTableUserDataReg.regOffset0 != 0)
                {
                    pCmdSpace[0] = spillTableUserDataReg.regOffset0;
                    pCmdSpace[1] = gpuVirtAddrLo;
                    pCmdSpace   += 2;

                    spillTableUserDataReg.u32All >>= 10;
                }
            }
        }

        // Clear dirty bits
        size_t* pDirtyMask = &(userDataEntries.dirty[0]);
        for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
        {
            pDirtyMask[i] = 0;
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WritePaScModeCntl1(
    uint32* pDeCmdSpace)
{
    const auto walkAlignState = m_gfxState.paScWalkAlignState;

    // WALK_ALIGNMENT and WALK_ALIGN8_PRIM_FITS_ST must be 0 if any of below condition is hit,
    // - If a VRS image is bound (if register bit VRS_SURFACE_ENABLE or RATE_HINT_WRITE_BACK_ENABLE is set)
    // - If a HiZ or HiS image is bound.
    // - If in "64K mode".

    const auto* const pMsaaState = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);
    regPA_SC_MODE_CNTL_1 paScModeCntl1 = { .u32All = (pMsaaState != nullptr) ? pMsaaState->PaScModeCntl1().u32All : 0 };

    {
        if (walkAlignState.hasHiSZ     ||
            walkAlignState.hasVrsImage ||
            (walkAlignState.targetIn64K                         &&
             m_deviceConfig.workarounds.walkAlign64kScreenSpace &&
             walkAlignState.globalScissorIn64K                  &&
             walkAlignState.scissorRectsIn64K))
        {
            paScModeCntl1.bits.WALK_ALIGNMENT           = 0;
            paScModeCntl1.bits.WALK_ALIGN8_PRIM_FITS_ST = 0;
        }

        if ((paScModeCntl1.u32All != m_gfxState.paScModeCntl1.u32All) ||
            (m_gfxState.validBits.paScModeCntl1 == 0))
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmPA_SC_MODE_CNTL_1, paScModeCntl1.u32All, pDeCmdSpace);

            m_gfxState.paScModeCntl1           = paScModeCntl1;
            m_gfxState.validBits.paScModeCntl1 = 1;
        }
    }

    m_gfxState.paScWalkAlignState.dirty = 0;

    return pDeCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WriteSpiPsInputEna(
    uint32* pDeCmdSpace)
{
    const auto* const pMsaaState = static_cast<const MsaaState*>(m_graphicsState.pMsaaState);

    if (pMsaaState != nullptr)
    {
        const auto* const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        const auto paScConservativeRasterizationCntl = pMsaaState->PaScConsRastCntl();

        const regSPI_PS_INPUT_ENA psoSpiPsInputEnable = pNewPipeline->SpiPsInputEna();

        regSPI_PS_INPUT_ENA validatedSpiPsInputEnable = psoSpiPsInputEnable;

        // If innerCoverage is used by the pipeline we can just check for it here to set COVERAGE_TO_SHADER_SELECT and
        // then assume that HW will set the required rasterization control bits.
        if (pNewPipeline->UsesInnerCoverage() ||
            ((paScConservativeRasterizationCntl.bits.UNDER_RAST_ENABLE == 1) &&
             (paScConservativeRasterizationCntl.bits.OVER_RAST_ENABLE == 0)))
        {
            validatedSpiPsInputEnable.bits.COVERAGE_TO_SHADER_SELECT = CovToShaderSel::INPUT_INNER_COVERAGE;
        }
        else if ((paScConservativeRasterizationCntl.bits.OVER_RAST_ENABLE == 1) &&
                 (paScConservativeRasterizationCntl.bits.UNDER_RAST_ENABLE == 0))
        {
            validatedSpiPsInputEnable.bits.COVERAGE_TO_SHADER_SELECT = CovToShaderSel::INPUT_COVERAGE;
        }

        if (psoSpiPsInputEnable.u32All != validatedSpiPsInputEnable.u32All)
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmSPI_PS_INPUT_ENA,
                                                              validatedSpiPsInputEnable.u32All,
                                                              pDeCmdSpace);

            // Mark PSO hash containing SPI_PS_INPUT_ENA as invalid
            m_gfxState.validBits.pipelineCtxHighHash = 0;
        }
    }

    return pDeCmdSpace;
}

// =====================================================================================================================
// Local helper which patches a previous CmdBindTarget bind's recorded CB Temporal Hint regs with new values.
// This generally is expected to happen when we detect scenarios where the pass should be resident in MALL.
// Examples of this include Blending which reads from the destination and producer/consumer scenarios where one pass
// reads the output of a previous passes color buffer.
static void PatchPassCbTemporalHints(
    TargetsMetadata*        pTargetMetadata,
    Gfx12TemporalHintsRead  readHint,
    Gfx12TemporalHintsWrite writeHint)
{
    PAL_ASSERT((pTargetMetadata->patchedAlready == false) &&
               ((pTargetMetadata->pCbMemInfoPairsCmdSpace != nullptr) || (pTargetMetadata->numMrtsBound == 0)));

    uint32* pCmdSpaceToPatch = pTargetMetadata->pCbMemInfoPairsCmdSpace;

    CB_MEM0_INFO cbMemInfo = {};

    cbMemInfo.bits.TEMPORAL_READ  = readHint;
    cbMemInfo.bits.TEMPORAL_WRITE = writeHint;

    for (uint32 slot = 0; slot < pTargetMetadata->numMrtsBound; slot++)
    {
        pCmdSpaceToPatch++;
        *pCmdSpaceToPatch = cbMemInfo.u32All;
        pCmdSpaceToPatch++;
    }

    pTargetMetadata->patchedAlready = true;
}

// =====================================================================================================================
bool UniversalCmdBuffer::DepthAndStencilEnabled(
    bool* pDepthWriteEn,
    bool* pStencilWriteEn
    ) const
{
    const DepthStencilState* pDsState = static_cast<const DepthStencilState*>(m_graphicsState.pDepthStencilState);
    const DepthStencilView*  pDsView  =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);

    DB_DEPTH_CONTROL   dbDepthControl   = {};
    DB_STENCIL_CONTROL dbStencilControl = m_gfxState.dbStencilControl;
    DB_STENCIL_INFO    dbStencilInfo    = {};
    DB_Z_INFO          dbZInfo          = {};
    DB_DEPTH_VIEW1     dbDepthView1     = {};
    DB_RENDER_CONTROL  dbRenderControl  = {};
    DB_RENDER_OVERRIDE dbRenderOverride = m_gfxState.dbRenderOverride;

    if (pDsState != nullptr)
    {
        dbDepthControl   = pDsState->DbDepthControl();
    }

    if (pDsView != nullptr)
    {
        dbStencilInfo    = pDsView->DbStencilInfo();
        dbZInfo          = pDsView->DbZInfo();
        dbDepthView1     = pDsView->DbDepthView1();
        dbRenderControl  = pDsView->DbRenderControl();
    }

    // ------------------------------------------------------------------------------------
    // Logic for zSurfEnable

    // --------------------------------------
    // Derive zTestReadEnable
    uint32 cullMode = uint32(m_graphicsState.triangleRasterState.cullMode);

    bool backfaceEnable =
        (dbDepthControl.bits.BACKFACE_ENABLE != 0) && (TestAnyFlagSet(cullMode, uint32(CullMode::Back)) == false);

    bool stencilExists =
        (dbDepthControl.bits.STENCIL_ENABLE != 0) && (dbStencilInfo.bits.FORMAT != STENCIL_INVALID);

    bool stencilFuncMayPassFf = (dbDepthControl.bits.STENCILFUNC != REF_NEVER);

    bool stencilFuncMayPassBf = (dbDepthControl.bits.STENCILFUNC_BF != REF_NEVER) && backfaceEnable;

    bool stencilMayPassFf = (stencilExists == false) || stencilFuncMayPassFf;
    bool stencilMayPassBf = (stencilExists == false) || stencilFuncMayPassBf;
    bool stencilMayPass   = stencilMayPassFf || stencilMayPassBf;

    bool formatHasZ = dbZInfo.bits.FORMAT != Z_INVALID;
    bool zEnableQ   = (dbDepthControl.bits.Z_ENABLE != 0);
    bool zExists    = formatHasZ && zEnableQ;

    bool zFuncMayPass = (dbDepthControl.bits.ZFUNC != FRAG_NEVER);
    bool zFuncMayFail = (dbDepthControl.bits.ZFUNC != FRAG_ALWAYS);

    bool zMayFail = zExists && stencilMayPass && zFuncMayFail;
    bool zMayPass = stencilMayPass && ((zExists == false) || zFuncMayPass);

    bool depthBoundsEnable = (dbDepthControl.bits.DEPTH_BOUNDS_ENABLE != 0) &&
                             (dbZInfo.bits.FORMAT != Z_INVALID);

    bool zTestReadEnable = (zMayPass && zMayFail) || depthBoundsEnable;
    // --------------------------------------

    // --------------------------------------
    // Derive zWriteEnable
    bool zFuncEqual   = (dbDepthControl.bits.ZFUNC == FRAG_EQUAL);
    bool zTestEnable  = zExists && stencilMayPass && zFuncMayFail;
    bool zReadOnlyQ   = (dbDepthView1.bits.Z_READ_ONLY != 0);

    bool zWriteEnableQ = dbDepthControl.bits.Z_WRITE_ENABLE != 0;

    bool zWriteEnable = zExists && zWriteEnableQ && zMayPass &&
                        ((zFuncEqual && zTestEnable) == false) && (zReadOnlyQ == false);
    // --------------------------------------

    // --------------------------------------
    // Derive noopsNeedZData
    bool zDecompressForce       = formatHasZ && (dbRenderControl.bits.DEPTH_COMPRESS_DISABLE == 0);
    bool zDecompressOnViolation = zEnableQ && formatHasZ && (dbRenderControl.bits.DECOMPRESS_ENABLE != 0);

    bool regbusForceZDirty = (dbRenderOverride.bits.FORCE_Z_DIRTY != 0) && formatHasZ;
    bool forceZValid       = ((dbRenderOverride.bits.FORCE_Z_VALID != 0) || regbusForceZDirty) && formatHasZ;

    bool noopsNeedZData = zDecompressForce || zDecompressOnViolation || forceZValid;
    // --------------------------------------

    bool zSurfEnable = zTestReadEnable || zWriteEnable || noopsNeedZData;
    // ------------------------------------------------------------------------------------

    // ------------------------------------------------------------------------------------
    // Logic for sSurfEnable

    // --------------------------------------
    // Derive stencilTestReadEnable
    bool stencilFuncMayFailFf = (dbDepthControl.bits.STENCILFUNC != REF_ALWAYS) && (backfaceEnable == false);
    bool stencilFuncMayFailBf = (dbDepthControl.bits.STENCILFUNC_BF != REF_ALWAYS) && backfaceEnable;

    bool stencilMayFailFf = stencilExists && stencilFuncMayFailFf;
    bool stencilMayFailBf = stencilExists && stencilFuncMayFailBf;

    bool stencilTestReadEnableFf = stencilMayPassFf && stencilMayFailFf;
    bool stencilTestReadEnableBf = stencilMayPassBf && stencilMayFailBf;
    bool stencilTestReadEnable = stencilTestReadEnableFf || stencilTestReadEnableBf;
    // --------------------------------------

    // --------------------------------------
    // Derive stencilWriteEnable
    bool stencilOpWritesFf =
        ((dbStencilControl.bits.STENCILFAIL  != STENCIL_KEEP) && stencilMayFailFf) ||
        ((dbStencilControl.bits.STENCILZFAIL != STENCIL_KEEP) && zMayFail)         ||
        ((dbStencilControl.bits.STENCILZPASS != STENCIL_KEEP) && zMayPass);

    bool stencilReadOnlyQ = (dbDepthView1.bits.STENCIL_READ_ONLY != 0);

    bool stencilWritePossibleFf = (stencilExists && stencilOpWritesFf && (stencilReadOnlyQ == false));
    bool stencilWriteEnableFf   = stencilWritePossibleFf && (m_gfxState.dbStencilWriteMask.bits.WRITEMASK != 0);

    bool stencilOpWritesBf = ((dbStencilControl.bits.STENCILFAIL_BF != STENCIL_KEEP) && stencilMayFailBf) ||
                             ((dbStencilControl.bits.STENCILZFAIL_BF != STENCIL_KEEP) && zMayFail)        ||
                             ((dbStencilControl.bits.STENCILZPASS_BF != STENCIL_KEEP) && zMayPass);

    bool stencilWritePossibleBf = stencilExists && stencilOpWritesBf && (stencilReadOnlyQ == false) && backfaceEnable;
    bool stencilWriteEnableBf   = stencilWritePossibleBf && (m_gfxState.dbStencilWriteMask.bits.WRITEMASK_BF != 0);

    bool stencilWriteEnable = stencilWriteEnableFf || stencilWriteEnableBf;
    // --------------------------------------

    // --------------------------------------
    // Derive noopsNeedStencilData
    bool formatHasStencil = (dbStencilInfo.bits.FORMAT != STENCIL_INVALID);
    bool regbusForceStencilDirty = (dbRenderOverride.bits.FORCE_STENCIL_DIRTY != 0);

    bool noopsNeedStencilData =
        ((dbRenderOverride.bits.FORCE_STENCIL_VALID != 0) || regbusForceStencilDirty) && formatHasStencil;
    // --------------------------------------

    bool sSurfEnable = stencilTestReadEnable || stencilWriteEnable || noopsNeedStencilData;
    // ------------------------------------------------------------------------------------

    *pDepthWriteEn   = zWriteEnable;
    *pStencilWriteEn = stencilWriteEnable;

    return zSurfEnable && sSurfEnable;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::ValidateHiZsWriteWa(
    const bool              depthAndStencilEn,
    const bool              depthWriteEn,
    const bool              stencilWriteEn,
    const bool              pipelineNoForceReZ,
    const DB_SHADER_CONTROL dbShaderControl,
    const DepthStencilView* pDepthStencilView,
    uint32*                 pDeCmdSpace)
{

    const Image& gfx12Image = *pDepthStencilView->GetImage();
    const SubresRange range = pDepthStencilView->ViewRange();

    const bool noForceReZ =
        (m_deviceConfig.workarounds.forceReZWhenHiZsDisabledWa == 0) || pipelineNoForceReZ;

    // Don't allow us to override the ZOrder mode to ReZ if that flavor of the workaround isn't active or
    // the client specifically told us not to.

    // Disable HiSZ for depth stencil view with both depth and stencil testing are enabled.
    // Re-enable on HiSZ state in GPU memory for all other cases.
    const uint32 pktSizeDwords = pDepthStencilView->OverrideHiZHiSEnable(false,
                                                                         dbShaderControl,
                                                                         noForceReZ,
                                                                         pDeCmdSpace);
    pDeCmdSpace += pktSizeDwords;

    if (depthAndStencilEn)
    {
        const HiSZ* pHiSZ = gfx12Image.GetHiSZ();

        // Need keep disabling HiSZ in this case.
        if ((depthWriteEn && pHiSZ->HiZEnabled()) || (stencilWriteEn && pHiSZ->HiSEnabled()))
        {
            // We only need to update the HiSZ metadata to indicate invalid data if writes are enabled for
            // Depth or Stencil.
            pDeCmdSpace = gfx12Image.UpdateHiSZStateMetaData(range, false, PacketPredicate(), GetEngineType(),
                                                             pDeCmdSpace);
        }
    }
    else
    {
        const gpusize stateAddr = gfx12Image.HiSZStateMetaDataAddr(range.startSubres.mipLevel);

        // COND_EXEC to see if can safe to re-enable HiSZ for the view.
        pDeCmdSpace += CmdUtil::BuildCondExec(stateAddr, pktSizeDwords, pDeCmdSpace);
        pDeCmdSpace += pDepthStencilView->OverrideHiZHiSEnable(true,
                                                               dbShaderControl,
                                                               noForceReZ,
                                                               pDeCmdSpace);
    }

    m_gfxState.dbShaderControl          = dbShaderControl;
    m_gfxState.noForceReZ               = noForceReZ;
    m_gfxState.validBits.hiszWorkaround = 1;
    return pDeCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::IssueHiSZWarEvent(
    uint32* pCmdSpace)
{
    if (m_deviceConfig.workarounds.hiszEventBasedWar == 0)
    {
        return pCmdSpace;
    }

    ReleaseMemGeneric releaseInfo = {};
    releaseInfo.vgtEvent    = BOTTOM_OF_PIPE_TS;
    releaseInfo.dataSel     = data_sel__me_release_mem__none;
    releaseInfo.noConfirmWr = true;

    pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
template <bool Indirect>
void UniversalCmdBuffer::ValidateDraw(
    const ValidateDrawInfo& drawInfo)
{
    // All of our dirty state will leak to the caller.
    m_graphicsState.leakFlags.u32All |= m_graphicsState.dirtyFlags.u32All;

#if PAL_DEVELOPER_BUILD
    uint32 startingCmdLen = GetUsedSize(CommandDataAlloc);
    uint32 userDataCmdLen = 0;
#endif

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (m_graphicsState.pipelineState.dirtyFlags.pipeline)
    {
        pDeCmdSpace = ValidateGraphicsPersistentState<true, Indirect>(drawInfo, pDeCmdSpace);
    }
    else
    {
        pDeCmdSpace = ValidateGraphicsPersistentState<false, Indirect>(drawInfo, pDeCmdSpace);
    }

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation != 0)
    {
        // GetUsedSize() is not accurate if we don't put the user-data validation and miscellaneous validation in
        // separate Reserve/Commit blocks.
        m_deCmdStream.CommitCommands(pDeCmdSpace);
        userDataCmdLen  = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        startingCmdLen += userDataCmdLen;
        pDeCmdSpace     = m_deCmdStream.ReserveCommands();
    }
#endif

    if (Indirect == false)
    {
        if ((m_gfxState.validBits.instanceCount == 0) ||
            (m_gfxState.drawArgs.instanceCount != drawInfo.instanceCount))
        {
            pDeCmdSpace += CmdUtil::BuildNumInstances(drawInfo.instanceCount, pDeCmdSpace);

            m_gfxState.validBits.instanceCount = 1;
            m_gfxState.drawArgs.instanceCount = drawInfo.instanceCount;
        }
    }
    else
    {
        m_gfxState.validBits.instanceCount = 0;

        const gpusize oldIndirectDrawArgsHi = m_gfxState.drawArgs.indirectDrawArgsHi;
        const gpusize newIndirectDrawArgsHi = drawInfo.indirectDrawArgsHi;
        if (((m_gfxState.validBits.indirectDrawArgsHi == 0) || (oldIndirectDrawArgsHi != newIndirectDrawArgsHi)) &&
            (drawInfo.isAdvancedIndirect == false))
        {
            pDeCmdSpace += CmdUtil::BuildSetBase<ShaderGraphics>((newIndirectDrawArgsHi << 32ull),
                                                                 base_index__pfp_set_base__patch_table_base,
                                                                 pDeCmdSpace);
            m_gfxState.drawArgs.indirectDrawArgsHi  = newIndirectDrawArgsHi;
            m_gfxState.validBits.indirectDrawArgsHi = 1;
        }
    }

    if ((m_gfxState.paScWalkAlignState.dirty != 0) || m_graphicsState.dirtyFlags.msaaState)
    {
        pDeCmdSpace = WritePaScModeCntl1(pDeCmdSpace);
    }

    // SPI_PS_INPUT_ENA can be very rarely impacted by conservative rasterization state. Since this scenario is very
    // rare and moving the register out of the PSO write would significantly impact the way packets are structured we
    // will just overwrite the value at draw-time and mark necessary filtering logic in the PSO invalid.
    if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_graphicsState.dirtyFlags.msaaState)
    {
        pDeCmdSpace = WriteSpiPsInputEna(pDeCmdSpace);
    }

    if (m_graphicsState.dirtyFlags.occlusionQueryActive)
    {
        pDeCmdSpace = UpdateDbCountControl(pDeCmdSpace);
    }

    if (Indirect)
    {
        // Index Base/Size are embedded in the draw packets for non-indirect draws. IndexType is handled at set-time.
        // Note that leakFlags.iaState implies an IB has been bound.
        if (m_gfxState.validBits.indexIndirectBuffer == 0)
        {
            m_gfxState.validBits.indexIndirectBuffer = 1;
            pDeCmdSpace += CmdUtil::BuildIndexBase(m_graphicsState.iaState.indexAddr, pDeCmdSpace);
            pDeCmdSpace += CmdUtil::BuildIndexBufferSize(m_graphicsState.iaState.indexCount, pDeCmdSpace);
        }
    }

    if (m_graphicsState.dirtyFlags.colorBlendState || m_graphicsState.dirtyFlags.colorTargetView)
    {
        if ((IsNested() == false) &&
            TestAnyFlagSet(m_deviceConfig.dynCbTemporalHints, Gfx12DynamicCbTemporalHintsBlendReadsDest))
        {
            if (m_currentTargetsMetadata.patchedAlready == false)
            {
                const auto* const pBlendState = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);
                if (pBlendState != nullptr)
                {
                    if (TestAnyFlagSet(pBlendState->BlendReadsDstPerformanceHeuristicMrtMask(),
                                       m_graphicsState.boundColorTargetMask))
                    {
                        PatchPassCbTemporalHints(&m_currentTargetsMetadata,
                                                 m_deviceConfig.gfx12TemporalHintsMrtReadBlendReadsDst,
                                                 m_deviceConfig.gfx12TemporalHintsMrtWriteBlendReadsDst);
                    }
                }
            }
        }
    }

    // Check alphaToCoverage at draw-time to determine sxMrt0BlendOpt.
    if (m_graphicsState.dirtyFlags.colorBlendState)
    {
        const auto* const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
        const auto* const pBlendState  = static_cast<const ColorBlendState*>(m_graphicsState.pColorBlendState);

        if (pBlendState != nullptr)
        {
            const bool alphaToCoverage = IsAlphaToCoverageEnabled(pNewPipeline, m_graphicsState.dynamicState);

            const uint32 sxMrt0BlendOptValue = alphaToCoverage ? 0 : pBlendState->SxMrt0BlendOpt().u32All;
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmSX_MRT0_BLEND_OPT, sxMrt0BlendOptValue, pDeCmdSpace);
        }
    }

    if (m_graphicsState.dirtyFlags.inputAssemblyState || m_graphicsState.pipelineState.dirtyFlags.pipeline)
    {
        const auto* const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

        regPA_SU_LINE_STIPPLE_CNTL paSuLineStippleCntl = {};

        if (pNewPipeline->IsLineStippleTexEnabled())
        {
            // Line stipple tex is only used by line stipple with wide antialiased line. so we need always
            // enable FRACTIONAL_ACCUM and EXPAND_FULL_LENGT.
            paSuLineStippleCntl.bits.LINE_STIPPLE_RESET =
                (m_graphicsState.inputAssemblyState.topology == PrimitiveTopology::LineList) ? 1 : 2;
            paSuLineStippleCntl.bits.FRACTIONAL_ACCUM   = 1;
            paSuLineStippleCntl.bits.EXPAND_FULL_LENGTH = 1;
        }

        if ((paSuLineStippleCntl.u32All != m_gfxState.paSuLineStippleCntl.u32All) ||
            (m_gfxState.validBits.paSuLineStippleCntl == 0))
        {
            pDeCmdSpace =
                m_deCmdStream.WriteSetOneContextReg(mmPA_SU_LINE_STIPPLE_CNTL, paSuLineStippleCntl.u32All, pDeCmdSpace);

            m_gfxState.paSuLineStippleCntl           = paSuLineStippleCntl;
            m_gfxState.validBits.paSuLineStippleCntl = 1;
        }
    }

    bool dbRenderOverrideUpdated = false;
    if ((m_deviceConfig.workarounds.waDbForceStencilValid != 0) &&
        (m_graphicsState.dirtyFlags.depthStencilView  ||
         m_graphicsState.dirtyFlags.depthStencilState ||
         (m_gfxState.validBits.dbRenderOverride == 0))          &&
        m_gfxState.szValid                                      &&
        (m_gfxState.dsLog2NumSamples > 0))
    {
        const DB_STENCIL_CONTROL dbStencilControl = m_gfxState.dbStencilControl;
        DB_RENDER_OVERRIDE       dbRenderOverride = m_gfxState.dbRenderOverride;

        if ((dbStencilControl.bits.STENCILZPASS    != dbStencilControl.bits.STENCILZFAIL) ||
            (dbStencilControl.bits.STENCILZPASS_BF != dbStencilControl.bits.STENCILZFAIL_BF))
        {
            dbRenderOverride.bits.FORCE_STENCIL_VALID = 1;
        }

        if ((dbRenderOverride.u32All != m_gfxState.dbRenderOverride.u32All) ||
            (m_gfxState.validBits.dbRenderOverride == 0))
        {
            pDeCmdSpace = m_deCmdStream.WriteSetOneContextReg(mmDB_RENDER_OVERRIDE, dbRenderOverride.u32All, pDeCmdSpace);

            m_gfxState.dbRenderOverride           = dbRenderOverride;
            m_gfxState.validBits.dbRenderOverride = 1;
            dbRenderOverrideUpdated               = true;
        }
    }

    const auto* pDepthStencilView =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    const auto* const pNewPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    const bool hiSZWarPossible =
        ((pDepthStencilView != nullptr)                          &&
         (pDepthStencilView->GetImage()->HasHiSZStateMetaData()) &&
         // We ignore internal blits (which will push graphics state) when considering if this workaround needs to be
         // active. While this could incur a risk, it is minimal and helps prevent corruption by properly writing the
         // HiSZ data during clears.
         (m_cmdBufState.flags.isGfxStatePushed == 0));

    const bool standardHiSZStateDirty =
        (m_graphicsState.dirtyFlags.depthStencilView    ||
         m_graphicsState.dirtyFlags.depthStencilState   ||
         m_graphicsState.dirtyFlags.stencilRefMaskState ||
         m_graphicsState.dirtyFlags.triangleRasterState ||
         dbRenderOverrideUpdated                        ||
         (m_gfxState.validBits.hiszWorkaround == 0));

    const bool forceReZStateDirty =
        (m_deviceConfig.workarounds.forceReZWhenHiZsDisabledWa &&
         m_graphicsState.pipelineState.dirtyFlags.pipeline     &&
         ((m_gfxState.dbShaderControl.u32All != pNewPipeline->DbShaderControl().u32All) ||
          (bool(m_gfxState.noForceReZ)       != pNewPipeline->NoForceReZ())));

    if (hiSZWarPossible && (standardHiSZStateDirty || forceReZStateDirty))
    {

        bool depthWriteEn   = false;
        bool stencilWriteEn = false;
        const bool depthAndStencilEn = DepthAndStencilEnabled(&depthWriteEn, &stencilWriteEn);

        pDeCmdSpace = ValidateHiZsWriteWa(
            depthAndStencilEn, depthWriteEn, stencilWriteEn, pNewPipeline->NoForceReZ(),
            pNewPipeline->DbShaderControl(), pDepthStencilView, pDeCmdSpace);
    }

    if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_graphicsState.dirtyFlags.colorTargetView)
    {
        pDeCmdSpace = ValidateDepthOnlyOpt(pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);

    // There are two reasons that viewports are validated at draw-time.
    // The first is so that we can limit how many VPs need to be written out to the command buffer. Most pipelines
    // can not access more than VP[0]. Most APIs are defined such that all 16 viewports are atomically set at set-time
    // and this is a significant amount of command buffer space (which is also why WriteViewports is using it's own
    // reserve/commit range).
    // The second is that there is a dependency on the depth clamp mode which is part of the pipeline state.
    if (m_graphicsState.dirtyFlags.viewports)
    {
        const uint32 viewportCount =
            (m_graphicsState.enableMultiViewport == 0) ? 1 : m_graphicsState.viewportState.count;

        WriteViewports(viewportCount);
    }

    // we need to wait all m_gfxState.primShaderCullingCb written, then to update NGG culling data constant buffer.
    if ((m_graphicsState.pipelineState.dirtyFlags.pipeline || m_graphicsState.dirtyFlags.u32All) &&
        m_nggTable.state.dirty &&
        (m_gfxState.nggCullingDataReg != UserDataNotMapped))
    {
        UpdateNggCullingDataBufferWithCpu();
    }

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation != 0)
    {
        const uint32 miscCmdLen = (GetUsedSize(CommandDataAlloc) - startingCmdLen);
        // Pipeline regs are not written during draw time validation in the GFX12 HWL
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    // Clear dirty flags
    m_graphicsState.dirtyFlags.u32All               = 0;
    m_graphicsState.pipelineState.dirtyFlags.u32All = 0;
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    ValidateDrawInfo drawInfo = {};
    drawInfo.vtxIdxCount      = vertexCount;
    drawInfo.instanceCount    = instanceCount;
    drawInfo.firstVertex      = firstVertex;
    drawInfo.firstInstance    = firstInstance;
    drawInfo.drawIndex        = drawId;
    drawInfo.isIndirect       = false;

    pThis->ValidateDraw<false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDraw);
    }

    if (IssueSqtt)
    {
        pThis->AddDrawSqttMarkers(drawInfo);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto* const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&       viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32            mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
                pCmdSpace += CmdUtil::BuildDrawIndexAuto(vertexCount,
                                                         false,
                                                         pThis->PacketPredicate(),
                                                         pCmdSpace);
                pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
            }
        }
    }
    else
    {
        pCmdSpace += CmdUtil::BuildDrawIndexAuto(vertexCount, false, pThis->PacketPredicate(), pCmdSpace);
        pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
    }

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize     streamOutFilledSizeVa,
    uint32      streamOutOffset,
    uint32      stride,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis      = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    auto* pValidBits = &pThis->m_gfxState.validBits;
    auto* pDrawArgs  = &pThis->m_gfxState.drawArgs;

    ValidateDrawInfo drawInfo = {};
    drawInfo.instanceCount    = instanceCount;
    drawInfo.firstInstance    = firstInstance;
    drawInfo.useOpaque        = true;
    drawInfo.isIndirect       = false;

    pThis->ValidateDraw<false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawOpaque);
    }

    if (IssueSqtt)
    {
        pThis->AddDrawSqttMarkers(drawInfo);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
    pCmdSpace += CmdUtil::BuildLoadContextRegsIndex(streamOutFilledSizeVa,
                                                    mmVGT_STRMOUT_DRAW_OPAQUE_BUFFER_FILLED_SIZE,
                                                    1,
                                                    pCmdSpace);
    pCmdSpace = pThis->m_deCmdStream.WriteSetOneContextReg(mmVGT_STRMOUT_DRAW_OPAQUE_OFFSET,
                                                           streamOutOffset,
                                                           pCmdSpace);
    pCmdSpace = pThis->m_deCmdStream.WriteSetOneContextReg(mmVGT_STRMOUT_DRAW_OPAQUE_VERTEX_STRIDE,
                                                           stride,
                                                           pCmdSpace);

    if (ViewInstancingEnable)
    {
        const auto* const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&       viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32            mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
                pCmdSpace += CmdUtil::BuildDrawIndexAuto(0,
                                                         true,
                                                         pThis->PacketPredicate(),
                                                         pCmdSpace);
                pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
            }
        }
    }
    else
    {
        pCmdSpace += CmdUtil::BuildDrawIndexAuto(0, true, pThis->PacketPredicate(), pCmdSpace);
        pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
    }

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    if (pThis->m_deviceConfig.workarounds.drawOpaqueSqNonEvents)
    {
        // We need to insert 3 SQ_NON_EVENTS after the end of every DRAW_OPAQUE packet. Otherwise, the
        // GE can determine an incorrect number of indices for back-to-back opaque draws if
        // the draw opaque registers are updated within 5 cycles on different states.
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(SQ_NON_EVENT, EngineTypeUniversal, pCmdSpace);
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(SQ_NON_EVENT, EngineTypeUniversal, pCmdSpace);
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(SQ_NON_EVENT, EngineTypeUniversal, pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount,
    uint32      drawId)
{
    auto*       pThis  = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const auto& idxBuf = pThis->m_graphicsState.iaState;

    ValidateDrawInfo drawInfo = {};
    drawInfo.vtxIdxCount      = indexCount;
    drawInfo.instanceCount    = instanceCount;
    drawInfo.firstVertex      = vertexOffset;
    drawInfo.firstInstance    = firstInstance;
    drawInfo.firstIndex       = firstIndex;
    drawInfo.drawIndex        = drawId;
    drawInfo.isIndirect       = false;
    drawInfo.isIndexed        = true;

    pThis->ValidateDraw<false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexed);
    }

    if (IssueSqtt)
    {
        pThis->AddDrawSqttMarkers(drawInfo);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    // The "validIndexCount" (set later in the code) will eventually be used to program the max_size
    // field in the draw packet, which is used to clamp how much of the index buffer can be read.
    //
    // For out-of-bounds index buffer fetches cases:
    // - the firstIndex parameter of the draw command is greater than the currently IB's indexCount
    // - Or binding a null IB (IB's indexCount = 0)
    // We consider validIndexCount = 0.
    // When validIndexCount == 0, the workaround HandleZeroIndexBuffer() is active,
    // we bind a one index sized index buffer with value 0 to conform to that requirement.
    const uint32 validIndexCount = (firstIndex >= idxBuf.indexCount) ? 0 : idxBuf.indexCount - firstIndex;

    // Compute the address of the IB. We must add the index offset specified by firstIndex into
    // our address because DRAW_INDEX_2 doesn't take an offset param.
    const uint32  indexSize   = 1 << static_cast<uint32>(idxBuf.indexType);
    const gpusize gpuVirtAddr = idxBuf.indexAddr + (indexSize * firstIndex);

    if (ViewInstancingEnable)
    {
        const GraphicsPipeline* pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&             viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32                  mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
                pCmdSpace += CmdUtil::BuildDrawIndex2(indexCount,
                                                      validIndexCount,
                                                      gpuVirtAddr,
                                                      pThis->PacketPredicate(),
                                                      pCmdSpace);
                pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
            }
        }
    }
    else
    {
        pCmdSpace += CmdUtil::BuildDrawIndex2(indexCount,
                                              validIndexCount,
                                              gpuVirtAddr,
                                              pThis->PacketPredicate(),
                                              pCmdSpace);
        pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
    }

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    bool multiPacketUsed = false;

    ValidateDrawInfo drawInfo  = {};
    drawInfo.multiIndirectDraw = (maximumCount > 1) || (countGpuAddr != 0uLL);
    drawInfo.isIndirect        = true;

    // To reduce the number of SET_BASE packets issued, we set the base address of the indirect draw arguments
    // to only the high-bits of the address. This should cover nearly all cases used by clients, so we should only
    // see a single SET_BASE per command buffer.
    drawInfo.indirectDrawArgsHi = HighPart(gpuVirtAddrAndStride.gpuVirtAddr);
    const gpusize offset        = LowPart(gpuVirtAddrAndStride.gpuVirtAddr);

    pThis->ValidateDraw<true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndirectMulti);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto*const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&      viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32           mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);

                if ((maximumCount == 1) && (countGpuAddr == 0uLL))
                {
                    pCmdSpace += CmdUtil::BuildDrawIndirect(offset,
                                                            pThis->GetVertexOffsetRegAddr(),
                                                            pThis->GetInstanceOffsetRegAddr(),
                                                            pThis->PacketPredicate(),
                                                            pCmdSpace);
                    pCmdSpace  = pThis->IssueHiSZWarEvent(pCmdSpace);
                }
                else
                {
                    multiPacketUsed = true;
                    pCmdSpace += CmdUtil::BuildDrawIndirectMulti(offset,
                                                                 pThis->GetVertexOffsetRegAddr(),
                                                                 pThis->GetInstanceOffsetRegAddr(),
                                                                 pThis->GetDrawIndexRegAddr(),
                                                                 gpuVirtAddrAndStride.stride,
                                                                 maximumCount,
                                                                 countGpuAddr,
                                                                 pThis->PacketPredicate(),
                                                                 IssueSqtt,
                                                                 pCmdSpace);
                    // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
                    pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
                }
            }
        }
    }
    else
    {
        if ((maximumCount == 1) && (countGpuAddr == 0uLL))
        {
            pCmdSpace += CmdUtil::BuildDrawIndirect(offset,
                                                    pThis->GetVertexOffsetRegAddr(),
                                                    pThis->GetInstanceOffsetRegAddr(),
                                                    pThis->PacketPredicate(),
                                                    pCmdSpace);
            pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
        }
        else
        {
            multiPacketUsed = true;
            pCmdSpace += CmdUtil::BuildDrawIndirectMulti(offset,
                                                         pThis->GetVertexOffsetRegAddr(),
                                                         pThis->GetInstanceOffsetRegAddr(),
                                                         pThis->GetDrawIndexRegAddr(),
                                                         gpuVirtAddrAndStride.stride,
                                                         maximumCount,
                                                         countGpuAddr,
                                                         pThis->PacketPredicate(),
                                                         IssueSqtt,
                                                         pCmdSpace);
            // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
            pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
        }
    }

    if (IssueSqtt
#if (PAL_BUILD_BRANCH >= 2410)
        && (multiPacketUsed == false)
#endif
       )
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    bool multiPacketUsed = false;

    ValidateDrawInfo drawInfo  = {};
    drawInfo.multiIndirectDraw = (maximumCount > 1) || (countGpuAddr != 0uLL);
    drawInfo.isIndirect        = true;
    drawInfo.isIndexed         = true;

    // To reduce the number of SET_BASE packets issued, we set the base address of the indirect draw arguments
    // to only the high-bits of the address. This should cover nearly all cases used by clients, so we should only
    // see a single SET_BASE per command buffer.
    drawInfo.indirectDrawArgsHi = HighPart(gpuVirtAddrAndStride.gpuVirtAddr);
    const gpusize offset        = LowPart(gpuVirtAddrAndStride.gpuVirtAddr);

    pThis->ValidateDraw<true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDrawIndexedIndirectMulti);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto* const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&       viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32            mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);

                if ((maximumCount == 1) && (countGpuAddr == 0uLL))
                {
                    pCmdSpace += CmdUtil::BuildDrawIndexIndirect(offset,
                                                                 pThis->GetVertexOffsetRegAddr(),
                                                                 pThis->GetInstanceOffsetRegAddr(),
                                                                 pThis->PacketPredicate(),
                                                                 pCmdSpace);
                    pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
                }
                else
                {
                    multiPacketUsed = true;
                    pCmdSpace += CmdUtil::BuildDrawIndexIndirectMulti(offset,
                                                                      pThis->GetVertexOffsetRegAddr(),
                                                                      pThis->GetInstanceOffsetRegAddr(),
                                                                      pThis->GetDrawIndexRegAddr(),
                                                                      gpuVirtAddrAndStride.stride,
                                                                      maximumCount,
                                                                      countGpuAddr,
                                                                      pThis->PacketPredicate(),
                                                                      IssueSqtt,
                                                                      pCmdSpace);
                    // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
                    pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
                }
            }
        }
    }
    else
    {
        if ((maximumCount == 1) && (countGpuAddr == 0uLL))
        {
            pCmdSpace += CmdUtil::BuildDrawIndexIndirect(offset,
                                                         pThis->GetVertexOffsetRegAddr(),
                                                         pThis->GetInstanceOffsetRegAddr(),
                                                         pThis->PacketPredicate(),
                                                         pCmdSpace);
            pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
        }
        else
        {
            multiPacketUsed = true;
            pCmdSpace += CmdUtil::BuildDrawIndexIndirectMulti(offset,
                                                              pThis->GetVertexOffsetRegAddr(),
                                                              pThis->GetInstanceOffsetRegAddr(),
                                                              pThis->GetDrawIndexRegAddr(),
                                                              gpuVirtAddrAndStride.stride,
                                                              maximumCount,
                                                              countGpuAddr,
                                                              pThis->PacketPredicate(),
                                                              IssueSqtt,
                                                              pCmdSpace);
            // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
            pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
        }
    }

    if (IssueSqtt
#if (PAL_BUILD_BRANCH >= 2410)
        && (multiPacketUsed == false)
#endif
       )
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool HasPipelineChanged>
uint32* UniversalCmdBuffer::ValidateComputeUserData(
    UserDataEntries*             pUserDataEntries,
    UserDataTableState*          pUserDataTable,
    const ComputeUserDataLayout* pCurrentComputeUserDataLayout,
    const ComputeUserDataLayout* pPrevComputeUserDataLayout,
    const DispatchDims*          pLogicalSize,
    gpusize                      indirectGpuVirtAddr,
    uint32*                      pCmdSpace)
{
    PAL_ASSERT(pCurrentComputeUserDataLayout != nullptr);

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Insert a single packet for all persistent state registers
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Save off a location for a single SET_PAIRS header for all SH regs written for this bind
    uint32* const pSetPairsHeader = pCmdSpace;
    pCmdSpace += 1;

    const UserDataReg workgroupReg = pCurrentComputeUserDataLayout->GetWorkgroup();
    if (workgroupReg.u32All != UserDataNotMapped)
    {
        PAL_ASSERT((pLogicalSize != nullptr) || (indirectGpuVirtAddr != 0));

        if (indirectGpuVirtAddr == 0)
        {
            *reinterpret_cast<DispatchDims*>(CmdAllocateEmbeddedData(3, 4, &indirectGpuVirtAddr)) = *pLogicalSize;
        }

        pCmdSpace[0] = workgroupReg.regOffset;
        pCmdSpace[1] = LowPart(indirectGpuVirtAddr);
        pCmdSpace[2] = workgroupReg.regOffset + 1;
        pCmdSpace[3] = HighPart(indirectGpuVirtAddr);
        pCmdSpace += 4;
    }

    const bool anyUserDataDirty = IsAnyUserDataDirty(pUserDataEntries);

    if (HasPipelineChanged || anyUserDataDirty)
    {
        pCmdSpace = pCurrentComputeUserDataLayout->CopyUserDataPairsToCmdSpace<HasPipelineChanged>(
            pPrevComputeUserDataLayout,
            pUserDataEntries->dirty,
            pUserDataEntries->entries,
            pCmdSpace);

        const UserDataReg spillTableUserDataReg = pCurrentComputeUserDataLayout->GetSpillTable();

        if ((spillTableUserDataReg.u32All != UserDataNotMapped) &&
            (pCurrentComputeUserDataLayout->GetSpillThreshold() != NoUserDataSpilling))
        {
            bool         reUpload       = false;
            const uint16 spillThreshold = pCurrentComputeUserDataLayout->GetSpillThreshold();
            const uint32 userDataLimit  = pCurrentComputeUserDataLayout->GetUserDataLimit();

            pUserDataTable->sizeInDwords = userDataLimit;

            PAL_ASSERT(userDataLimit > 0);
            const uint16 lastUserData = (userDataLimit - 1);

            PAL_ASSERT(pUserDataTable->dirty == 0); // Not ever setting this today.

            if (HasPipelineChanged &&
                ((pPrevComputeUserDataLayout == nullptr) ||
                 (spillThreshold != pPrevComputeUserDataLayout->GetSpillThreshold()) ||
                 (userDataLimit > pPrevComputeUserDataLayout->GetUserDataLimit())))
            {
                // If the pipeline is changing and the start offset or size of the spilled region is changed, reupload.
                reUpload = true;
            }
            else if (anyUserDataDirty)
            {
                // Otherwise, use the following loop to check if any of the spilled user-data entries are dirty.
                const uint32 firstMaskId = (spillThreshold / UserDataEntriesPerMask);
                const uint32 lastMaskId  = (lastUserData   / UserDataEntriesPerMask);
                for (uint32 maskId = firstMaskId; maskId <= lastMaskId; ++maskId)
                {
                    size_t dirtyMask = pUserDataEntries->dirty[maskId];
                    if (maskId == firstMaskId)
                    {
                        // Ignore the dirty bits for any entries below the spill threshold.
                        const uint32 firstEntryInMask = (spillThreshold & (UserDataEntriesPerMask - 1));
                        dirtyMask &= ~BitfieldGenMask(static_cast<size_t>(firstEntryInMask));
                    }
                    if (maskId == lastMaskId)
                    {
                        // Ignore the dirty bits for any entries beyond the user-data limit.
                        const uint32 lastEntryInMask = (lastUserData & (UserDataEntriesPerMask - 1));
                        dirtyMask &= BitfieldGenMask(static_cast<size_t>(lastEntryInMask + 1));
                    }

                    if (dirtyMask != 0)
                    {
                        reUpload = true;
                        break; // We only care if *any* spill table contents change!
                    }
                } // for each wide-bitfield sub-mask
            }

            if (reUpload)
            {
                UpdateUserDataTableCpu(pUserDataTable,
                                       (userDataLimit - spillThreshold),
                                       spillThreshold,
                                       &pUserDataEntries->entries[0]);
            }

            if (HasPipelineChanged || reUpload)
            {
                const uint32 gpuVirtAddrLo = LowPart(pUserDataTable->gpuVirtAddr);
                PAL_ASSERT(spillTableUserDataReg.regOffset != 0);

                pCmdSpace[0] = spillTableUserDataReg.regOffset;
                pCmdSpace[1] = gpuVirtAddrLo;
                pCmdSpace += 2;
            }
        }

        // Clear dirty bits
        size_t* pDirtyMask = &pUserDataEntries->dirty[0];
        for (uint32 i = 0; i < NumUserDataFlagsParts; i++)
        {
            pDirtyMask[i] = 0;
        }
    }

    // (pSetPairsHeader + 1) not needed
    const uint32 numRegPairs = uint32(VoidPtrDiff(pCmdSpace, pSetPairsHeader) / sizeof(RegisterValuePair));
    if (numRegPairs > 0)
    {
        // Go back and write the packet header now that we know how many RegPairs got added
        void* pThrowAway;
        const size_t pktSize = CmdUtil::BuildSetShPairsHeader<ShaderCompute>(numRegPairs,
                                                                             &pThrowAway,
                                                                             pSetPairsHeader);
        PAL_ASSERT(pktSize == size_t(pCmdSpace - pSetPairsHeader));
    }
    else
    {
        // Remove reserved space for header!
        pCmdSpace -= 1;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // End of SET_SH_REG_PAIRS pkt
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    return pCmdSpace;
}

// =====================================================================================================================
// ppPrevComputeUserDataLayout must not be nullptr and *ppPrevComputeUserDataLayout must be init (can be nullptr).
template <bool Indirect, bool IsAce>
uint32* UniversalCmdBuffer::ValidateDispatchPalAbi(
    uint32*                       pCmdSpace,
    ComputeState*                 pComputeState,
    UserDataTableState*           pUserDataTable,
    const ComputeUserDataLayout*  pCurrentComputeUserDataLayout,
    const ComputeUserDataLayout** ppPrevComputeUserDataLayout,
    const DispatchDims*           pLogicalSize,
    gpusize                       indirectAddr,
    bool                          allow2dDispatchInterleave,
    bool*                         pEnable2dDispatchInterleave)
{
#if PAL_DEVELOPER_BUILD
    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  userDataCmdLen    = 0;
#endif

    const ComputePipeline* const pPipeline =
        static_cast<const ComputePipeline*>(pComputeState->pipelineState.pPipeline);

    const ComputeUserDataLayout* const pPrevUserDataLayout = *ppPrevComputeUserDataLayout;

    if (pComputeState->pipelineState.dirtyFlags.pipeline)
    {
        pCmdSpace = ValidateComputeUserData<true>(&pComputeState->csUserDataEntries,
                                                  pUserDataTable,
                                                  pCurrentComputeUserDataLayout,
                                                  pPrevUserDataLayout,
                                                  pLogicalSize,
                                                  indirectAddr,
                                                  pCmdSpace);
    }
    else
    {
        pCmdSpace = ValidateComputeUserData<false>(&pComputeState->csUserDataEntries,
                                                   pUserDataTable,
                                                   pCurrentComputeUserDataLayout,
                                                   pPrevUserDataLayout,
                                                   pLogicalSize,
                                                   indirectAddr,
                                                   pCmdSpace);
    }
    *ppPrevComputeUserDataLayout = pPipeline->UserDataLayout();

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        userDataCmdLen    = (uint32(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        pStartingCmdSpace = pCmdSpace;
    }
#endif

    // IT_SET_BASE is not required for indirect dispatch sent to ACE.
    if (Indirect && (IsAce == false))
    {
        const gpusize indirectAddrHi = HighPart(indirectAddr);

        // ExecuteIndirectPacket() path passes indirectAddr value with zero even if Indirect = true.
        if (((indirectAddrHi != m_indirectDispatchArgsAddrHi) ||
             (m_indirectDispatchArgsValid == false)) &&
            (indirectAddr != 0))
        {
            // Only the base address is programmed into the SET_BASE.
            pCmdSpace += CmdUtil::BuildSetBase<ShaderCompute>((indirectAddrHi << 32ull),
                                                              base_index__pfp_set_base__patch_table_base,
                                                              pCmdSpace);
            m_indirectDispatchArgsValid  = true;
            m_indirectDispatchArgsAddrHi = indirectAddrHi;
        }
    }

    *pEnable2dDispatchInterleave = false;

    if (IsAce == false) // ACE doesn't support any kind (1D or 2D) of dispatch interleave.
    {
        uint32 dispatchInterleave = pPipeline->ComputeDispatchInterleave().u32All;

        if (pPipeline->Is2dDispatchInterleave())
        {
            *pEnable2dDispatchInterleave = true; // may become false below

            if (allow2dDispatchInterleave == false)
            {
                dispatchInterleave = mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT;
                *pEnable2dDispatchInterleave = false;
            }
            else if (pPipeline->IsDefaultDispatchInterleave())
            {
                if (Indirect)
                {
                    // Fall back 2D interleave to 1D interleave if not allowed.
                    if (m_deviceConfig.allow2dDispatchInterleaveOnIndirectDispatch == false)
                    {
                        dispatchInterleave = mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT;
                        *pEnable2dDispatchInterleave = false;
                    }
                }
                else
                {
                    // Fall back 2D interleave to 1D interleave on small dispatches.
                    // Note pLogicalSize isn't necessarily the actual number of groups dispatched for CmdDispatchOffset,
                    // but we use it as a proxy to reduce the number of parameters.
                    if ((pLogicalSize->x < m_deviceConfig.dispatchInterleaveSize2DMinX) ||
                        (pLogicalSize->y < m_deviceConfig.dispatchInterleaveSize2DMinY) ||
                        ((pLogicalSize->x * pLogicalSize->y) <= pPipeline->Get2dDispachInterleaveSize()))
                    {
                        dispatchInterleave = mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT;
                        *pEnable2dDispatchInterleave = false;
                    }
                }
            }
        }

        // Enhancement: if we'll set 2D_INTERLEAVE_EN=0 in the dispatch packet, we could only compare the low bits here,
        // as the high bits containing INTERLEAVE_2D_{X,Y}_SIZE will be ignored.
        if ((m_gfxState.computeDispatchInterleave.u32All != dispatchInterleave) ||
            (m_gfxState.validBits.computeDispatchInterleave == 0))
        {
            pCmdSpace += CmdUtil::BuildSetSeqShRegsIndex<ShaderCompute>(
                             mmCOMPUTE_DISPATCH_INTERLEAVE,
                             mmCOMPUTE_DISPATCH_INTERLEAVE,
                             index__pfp_set_sh_reg_index__compute_dispatch_interleave_shadow,
                             pCmdSpace);
            pCmdSpace[-1] = dispatchInterleave;

            m_gfxState.computeDispatchInterleave.u32All = dispatchInterleave;
            m_gfxState.validBits.computeDispatchInterleave = 1;
        }
    }

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (uint32(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    // Clear the dirty flags
    pComputeState->pipelineState.dirtyFlags.u32All = 0;

    return pCmdSpace;
}

// =====================================================================================================================
// Performs HSA ABI dispatch-time dirty state validation.
uint32* UniversalCmdBuffer::ValidateDispatchHsaAbi(
    DispatchDims        offset,
    const DispatchDims& logicalSize,
    uint32*             pCmdSpace)
{
#if PAL_DEVELOPER_BUILD
    uint32* pStartingCmdSpace = pCmdSpace;
    uint32  userDataCmdLen = 0;
#endif
    const auto* const pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);

    // PAL thinks in terms of threadgroups but the HSA ABI thinks in terms of global threads, we need to convert.
    const DispatchDims threads = pPipeline->ThreadsPerGroupXyz();
    offset *= threads;

    // Now we write the required SGPRs. These depend on per-dispatch state so we don't have dirty bit tracking.
    const HsaAbi::CodeObjectMetadata&        metadata    = pPipeline->HsaMetadata();
    const llvm::amdhsa::kernel_descriptor_t& desc        = pPipeline->KernelDescriptor();

    gpusize kernargsGpuVa = 0;
    uint32 ldsSize = metadata.GroupSegmentFixedSize();
    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        GfxCmdBuffer::CopyHsaKernelArgsToMem(offset, threads, logicalSize, &kernargsGpuVa, &ldsSize, metadata);
    }

    // If ldsBytesPerTg was specified then that's what LDS_SIZE was programmed to, otherwise we used the fixed size.
    const uint32 boundLdsSize =
        (m_computeState.dynamicCsInfo.ldsBytesPerTg > 0) ? m_computeState.dynamicCsInfo.ldsBytesPerTg
                                                         : metadata.GroupSegmentFixedSize();

    // If our computed total LDS size is larger than the previously bound size we must rewrite it.
    if (boundLdsSize < ldsSize)
    {
        pCmdSpace = pPipeline->WriteUpdatedLdsSize(pCmdSpace, ldsSize);

        // We've effectively rebound this state. Update its value so that we don't needlessly rewrite it on the
        // next dispatch call.
        m_computeState.dynamicCsInfo.ldsBytesPerTg = ldsSize;
    }

    uint32 startReg = mmCOMPUTE_USER_DATA_0;

    m_pPrevComputeUserDataLayoutValidatedWith = nullptr;
    // Many HSA ELFs request private segment buffer registers, but never actually use them. Space is reserved to
    // adhere to initialization order but will be unset as we do not support scratch space in this execution path.
    if (TestAnyFlagSet(desc.kernel_code_properties,
                       llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_PRIVATE_SEGMENT_BUFFER))
    {
        startReg += 4;
    }
    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_PTR))
    {
        const DispatchDims logicalSizeInWorkItems = logicalSize * threads;

        // Fake an AQL dispatch packet for the shader to read.
        gpusize aqlPacketGpu = 0;
        auto* const pAqlPacket = reinterpret_cast<hsa_kernel_dispatch_packet_t*>(
            CmdAllocateEmbeddedData(sizeof(hsa_kernel_dispatch_packet_t) / sizeof(uint32),
                1,
                &aqlPacketGpu));

        // Zero everything out then fill in certain fields the shader is likely to read.
        memset(pAqlPacket, 0, sizeof(hsa_kernel_dispatch_packet_t));

        pAqlPacket->workgroup_size_x     = static_cast<uint16>(threads.x);
        pAqlPacket->workgroup_size_y     = static_cast<uint16>(threads.y);
        pAqlPacket->workgroup_size_z     = static_cast<uint16>(threads.z);
        pAqlPacket->grid_size_x          = logicalSizeInWorkItems.x;
        pAqlPacket->grid_size_y          = logicalSizeInWorkItems.y;
        pAqlPacket->grid_size_z          = logicalSizeInWorkItems.z;
        pAqlPacket->private_segment_size = metadata.PrivateSegmentFixedSize();
        pAqlPacket->group_segment_size   = ldsSize;

        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg, (startReg + 1), &aqlPacketGpu, pCmdSpace);

        startReg += 2;
    }

    // When kernels request queue ptr, for COV4 (Code Object Version 4) and earlier, ENABLE_SGPR_QUEUE_PTR is set,
    // which means that the queue ptr is passed in two SGPRs, for COV5 and later, ENABLE_SGPR_QUEUE_PTR is deprecated
    // and HiddenQueuePtr is set, which means that the queue ptr is passed in hidden kernel arguments.
    // When there are indirect function call, such as virtual functions, HSA ABI compiler makes the optimization pass
    // unable to infer if queue ptr will be used or not. As a result, the pass has to assume the queue ptr
    // might be used, so HSA ELFs request queue ptrs but never actually use them. SGPR Space is reserved to adhere to
    // initialization order for COV4 when ENABLE_SGPR_QUEUE_PTR is set, but is unset as we can't support queue ptr.
    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_QUEUE_PTR))
    {
        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_KERNARG_SEGMENT_PTR))
    {
        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg, (startReg + 1), &kernargsGpuVa, pCmdSpace);

        startReg += 2;
    }

    if (TestAnyFlagSet(desc.kernel_code_properties, llvm::amdhsa::KERNEL_CODE_PROPERTY_ENABLE_SGPR_DISPATCH_ID))
    {
        // This feature may be enabled as a side effect of indirect calls.
        // However, the compiler team confirmed that the dispatch id itself is not used,
        // so safe to send 0 for each dispatch.
        constexpr uint32 DispatchId[2] = {};
        pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(startReg, (startReg + 1), &DispatchId, pCmdSpace);
        startReg += 2;
    }

#if PAL_ENABLE_PRINTS_ASSERTS
    regCOMPUTE_PGM_RSRC2 computePgmRsrc2 = {};
    computePgmRsrc2.u32All = desc.compute_pgm_rsrc2;

    PAL_ASSERT((startReg - mmCOMPUTE_USER_DATA_0) <= computePgmRsrc2.bitfields.USER_SGPR);
#endif

#if PAL_DEVELOPER_BUILD
    if (m_deviceConfig.enablePm4Instrumentation)
    {
        const uint32 miscCmdLen = (uint32(pCmdSpace - pStartingCmdSpace) * sizeof(uint32));
        m_device.DescribeDrawDispatchValidation(this, userDataCmdLen, miscCmdLen);
    }
#endif

    // Clear the dirty flags
    m_computeState.pipelineState.dirtyFlags.u32All = 0;

    return pCmdSpace;
}

// =====================================================================================================================
template <bool Indirect>
uint32* UniversalCmdBuffer::ValidateTaskDispatch(
    uint32*             pCmdSpace,
    const DispatchDims* pLogicalSize,
    gpusize             indirectGpuVirtAddr)
{
    const HybridGraphicsPipeline* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const ComputeUserDataLayout* pTaskUserDataLayout = pHybridPipeline->TaskUserDataLayout();

    // Copy the gfx user-data entries to ACE compute user-data entries.
    for (uint32 i = 0; i < pTaskUserDataLayout->GetUserDataLimit(); ++i)
    {
        if (m_pComputeStateAce->csUserDataEntries.entries[i] != m_graphicsState.gfxUserDataEntries.entries[i])
        {
            m_pComputeStateAce->csUserDataEntries.entries[i] = m_graphicsState.gfxUserDataEntries.entries[i];
            WideBitfieldSetBit(m_pComputeStateAce->csUserDataEntries.dirty, i);
        }
    }

    const ComputeUserDataLayout* pPrevUserDataLayout = nullptr;
    bool enable2dDispatchInterleave;
    pCmdSpace = ValidateDispatchPalAbi<Indirect, true>(
        pCmdSpace,
        m_pComputeStateAce,
        &m_spillTable.stateGfx,
        pTaskUserDataLayout,
        &pPrevUserDataLayout,
        pLogicalSize,
        indirectGpuVirtAddr,
        true, // allow2dDispatchInterleave
        &enable2dDispatchInterleave);

    if (Indirect == false)
    {
        uint32* const pSetPairsHeader = pCmdSpace;
        pCmdSpace += 1;

        // Initialize the taskDispatchIdx to 0 for direct dispatch
        const UserDataReg taskDispatchIdxReg = pTaskUserDataLayout->GetTaskDispatchIndex();
        if (taskDispatchIdxReg.u32All != UserDataNotMapped)
        {
            pCmdSpace[0] = taskDispatchIdxReg.regOffset;
            pCmdSpace[1] = 0;
            pCmdSpace += 2;
        }

        // Set dispatch dimensions for task shader
        const UserDataReg taskDispatchDimsReg = pTaskUserDataLayout->GetTaskDispatchDims();
        PAL_ASSERT((taskDispatchDimsReg.u32All != UserDataNotMapped) && (pLogicalSize != nullptr));
        pCmdSpace[0] = taskDispatchDimsReg.regOffset;
        pCmdSpace[1] = pLogicalSize->x;
        pCmdSpace[2] = taskDispatchDimsReg.regOffset + 1;
        pCmdSpace[3] = pLogicalSize->y;
        pCmdSpace[4] = taskDispatchDimsReg.regOffset + 2;
        pCmdSpace[5] = pLogicalSize->z;
        pCmdSpace   += 6;

        // Go back and write the packet header now that we know how many RegPairs got added
        // (pSetPairsHeader + 1) not needed
        const uint32 numRegPairs = uint32(VoidPtrDiff(pCmdSpace, pSetPairsHeader) / sizeof(RegisterValuePair));
        void* pThrowAway;
        const size_t pktSize =
            CmdUtil::BuildSetShPairsHeader<ShaderCompute>(numRegPairs, &pThrowAway, pSetPairsHeader);
        PAL_ASSERT(pktSize == uint32(pCmdSpace - pSetPairsHeader));
    }

    return pCmdSpace;
}

// =====================================================================================================================
bool UniversalCmdBuffer::GetDispatchPingPongEn()
{
    const ComputePipeline* pPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

    bool dispatchPingPongEn = false;

    if (m_deviceConfig.overrideCsDispatchPingPongMode == CsDispatchPingPongModeHonorClient)
    {
        // If the pipeline requested reverse walk order - use reverse order.
        if (pPipeline->PingPongEn())
        {
            dispatchPingPongEn = true;
        }
        // Else if the Cmdbuffer wants us to ping pong between forward and reverse do that.
        else if (DispatchPongPongWalk())
        {
            dispatchPingPongEn = (m_dispatchPingPongEn == false);
        }
    }
    else if (m_deviceConfig.overrideCsDispatchPingPongMode == CsDispatchPingPongModeForceOn)
    {
        dispatchPingPongEn = (m_dispatchPingPongEn == false);
    }
    else
    {
        PAL_ASSERT((m_deviceConfig.overrideCsDispatchPingPongMode == CsDispatchPingPongModeForceOff) &&
                   (dispatchPingPongEn == false));
    }

    // Save off the last used mode
    m_dispatchPingPongEn = dispatchPingPongEn;

    return dispatchPingPongEn;
}

// =====================================================================================================================
template <bool HsaAbi, bool IssueSqtt, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatch(
    ICmdBuffer*       pCmdBuffer,
    DispatchDims      size,
    DispatchInfoFlags infoFlags)
{
    auto*                  pThis     = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatch(Developer::DrawDispatchType::CmdDispatch, size, infoFlags);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    bool enable2dDispatchInterleave;
    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi(
            {},
            size,
            pCmdSpace);
        enable2dDispatchInterleave = false;
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi<false, false>(
            pCmdSpace,
            &pThis->m_computeState,
            &pThis->m_spillTable.stateCompute,
            pPipeline->UserDataLayout(),
            &pThis->m_pPrevComputeUserDataLayoutValidatedWith,
            &size,
            0,    // indirectGpuVirtAddr
            true, // allow2dDispatchInterleave
            &enable2dDispatchInterleave);
    }

    pCmdSpace += CmdUtil::BuildDispatchDirect<false, true>(size,
                                                           pThis->PacketPredicate(),
                                                           pPipeline->IsWave32(),
                                                           pThis->UsesDispatchTunneling(),
                                                           false,
                                                           pThis->GetDispatchPingPongEn(),
                                                           enable2dDispatchInterleave,
                                                           pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchIndirect(
    ICmdBuffer* pCmdBuffer,
    gpusize     gpuVirtAddr)
{
    auto*                  pThis          = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline      =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatchIndirect();
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();
    bool    enable2dDispatchInterleave;

    pCmdSpace = pThis->ValidateDispatchPalAbi<true, false>(
        pCmdSpace,
        &pThis->m_computeState,
        &pThis->m_spillTable.stateCompute,
        pPipeline->UserDataLayout(),
        &pThis->m_pPrevComputeUserDataLayoutValidatedWith,
        nullptr,
        gpuVirtAddr,
        true, // allow2dDispatchInterleave
        &enable2dDispatchInterleave);

    // To reduce the number of SET_BASE packets issued, we set the base address of the indirect dispatch arguments
    // to only the high-bits of the address. This should cover nearly all cases used by clients, so we should only
    // see a single SET_BASE per command buffer.
    const gpusize offset = LowPart(gpuVirtAddr);

    pCmdSpace += CmdUtil::BuildDispatchIndirectGfx(offset,
                                                   pThis->PacketPredicate(),
                                                   pPipeline->IsWave32(),
                                                   pThis->GetDispatchPingPongEn(),
                                                   enable2dDispatchInterleave,
                                                   pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool HsaAbi, bool IssueSqtt, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchOffset(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims offset,
    DispatchDims launchSize,
    DispatchDims logicalSize)
{
    auto*                  pThis     = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const ComputePipeline* pPipeline =
        static_cast<const ComputePipeline*>(pThis->m_computeState.pipelineState.pPipeline);

    if (DescribeDrawDispatch)
    {
        pThis->DescribeDispatchOffset(offset, launchSize, logicalSize);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    bool    enable2dDispatchInterleave;

    // ValidateDispatch should only change the interleave from this to a 1D interleave, if it does.
    const regCOMPUTE_DISPATCH_INTERLEAVE initialInterleave = pPipeline->ComputeDispatchInterleave();
    // Dispatch packets with 2D_INTERLEAVE_EN=1 do not support non-GOG-aligned dispatch offsets,
    // disallow 2D interleaves for such cases.
    const uint32 interleaveMaskX = (1 << initialInterleave.bits.INTERLEAVE_2D_X_SIZE) - 1;
    const uint32 interleaveMaskY = (1 << initialInterleave.bits.INTERLEAVE_2D_Y_SIZE) - 1;
    const bool allow2dDispatchInterleave = ((offset.x & interleaveMaskX) == 0) &&
                                           ((offset.y & interleaveMaskY) == 0);
    if (HsaAbi)
    {
        pCmdSpace = pThis->ValidateDispatchHsaAbi(
            offset,
            logicalSize,
            pCmdSpace);
        enable2dDispatchInterleave = false;
    }
    else
    {
        pCmdSpace = pThis->ValidateDispatchPalAbi<false, false>(
            pCmdSpace,
            &pThis->m_computeState,
            &pThis->m_spillTable.stateCompute,
            pPipeline->UserDataLayout(),
            &pThis->m_pPrevComputeUserDataLayoutValidatedWith,
            &logicalSize,
            0, // indirectGpuVirtAddr
            allow2dDispatchInterleave,
            &enable2dDispatchInterleave);
    }

    static_assert(Util::CheckSequential({ mmCOMPUTE_START_X, mmCOMPUTE_START_Y, mmCOMPUTE_START_Z, }),
                  "Unexpected offset of registers COMPUTE_START_*.");
    static_assert((offsetof(DispatchDims, x) == 0) &&
                  (offsetof(DispatchDims, y) == 4) &&
                  (offsetof(DispatchDims, z) == 8),
                  "Unexpected DispatchDims layout.");

    // For dispatch packets with 2D_INTERLEAVE_EN=1, COMPUTE_START_{X,Y} are in units of GOGs.
    DispatchDims start = offset;
    if (enable2dDispatchInterleave)
    {
        // Values here should be consistent with allow2dDispatchInterleave calculation before ValidateDispatch.
        PAL_ASSERT(initialInterleave.u32All == pThis->m_gfxState.computeDispatchInterleave.u32All);

        start.x >>= pThis->m_gfxState.computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE;
        start.y >>= pThis->m_gfxState.computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE;
    }
    pCmdSpace = CmdStream::WriteSetSeqShRegs<ShaderCompute>(mmCOMPUTE_START_X,
                                                            mmCOMPUTE_START_Z,
                                                            &start,
                                                            pCmdSpace);

    // Ping-pong is not supported when software uses the COMPUTE_START_* registers.
    // It interferes with preemption. The parameter 'pingPongEn' is set to false.
    pCmdSpace += CmdUtil::BuildDispatchDirect<false, false>(offset + launchSize,
                                                            pThis->PacketPredicate(),
                                                            pPipeline->IsWave32(),
                                                            pThis->UsesDispatchTunneling(),
                                                            false,
                                                            false,
                                                            enable2dDispatchInterleave,
                                                            pCmdSpace);

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMesh(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);

    ValidateDrawInfo drawInfo = {};
    drawInfo.instanceCount    = 1;
    drawInfo.meshDispatchDims = size;
    drawInfo.isIndirect       = false;

    pThis->ValidateDraw<false>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh);
    }

    if (IssueSqtt)
    {
        pThis->AddDrawSqttMarkers(drawInfo);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto* const pPipeline          =
            static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);
        const auto&       viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32            mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
                pCmdSpace += CmdUtil::BuildDispatchMeshDirect(size, pThis->PacketPredicate(), pCmdSpace);
                pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
            }
        }
    }
    else
    {
        pCmdSpace += CmdUtil::BuildDispatchMeshDirect(size, pThis->PacketPredicate(), pCmdSpace);
        pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
    }

    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void UniversalCmdBuffer::CmdDispatchMeshTaskAce(
    const DispatchDims& size)
{
    const HybridGraphicsPipeline* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const ComputeUserDataLayout* pTaskUserDataLayout = pHybridPipeline->TaskUserDataLayout();

    uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

    pAceCmdSpace = CmdAceWaitDe(pAceCmdSpace);

    pAceCmdSpace = ValidateTaskDispatch<false>(pAceCmdSpace, &size, 0);

    m_pAceCmdStream->CommitCommands(pAceCmdSpace);

    if (DescribeDrawDispatch)
    {
        DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh, true);
    }

    pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

    // Build the ACE direct dispatches.
    if (ViewInstancingEnable)
    {
        const auto& viewInstancingDesc = pHybridPipeline->GetViewInstancingDesc();
        uint32      mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pAceCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pAceCmdSpace);

                if ((PacketPredicate() == PredEnable) && (m_predGpuAddr != 0))
                {
                    pAceCmdSpace += CmdUtil::BuildCondExec(
                        m_predGpuAddr, CmdUtil::DispatchTaskMeshDirectMecSize, pAceCmdSpace);
                }

                pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshDirectMec(
                    size,
                    pTaskUserDataLayout->GetMeshTaskRingIndex().regOffset,
                    PacketPredicate(),
                    pHybridPipeline->IsTaskWave32(),
                    pAceCmdSpace);
            }
        }
    }
    else
    {
        if ((PacketPredicate() == PredEnable) && (m_predGpuAddr != 0))
        {
            pAceCmdSpace += CmdUtil::BuildCondExec(
                m_predGpuAddr, CmdUtil::DispatchTaskMeshDirectMecSize, pAceCmdSpace);
        }

        pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshDirectMec(
            size,
            pTaskUserDataLayout->GetMeshTaskRingIndex().regOffset,
            PacketPredicate(),
            pHybridPipeline->IsTaskWave32(),
            pAceCmdSpace);
    }

    if (IssueSqtt)
    {
        pAceCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                          EngineTypeCompute,
                                                          PacketPredicate(),
                                                          pAceCmdSpace);
    }

    m_pAceCmdStream->CommitCommands(pAceCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void UniversalCmdBuffer::CmdDispatchMeshTaskGfx()
{
    ValidateDrawInfo drawInfo   = {};
    drawInfo.isIndirect         = true;
    drawInfo.isAdvancedIndirect = true;
    ValidateDraw<true>(drawInfo);

    const HybridGraphicsPipeline* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const GraphicsUserDataLayout* pLayout = pHybridPipeline->UserDataLayout();

    // DescribeDraw is called when the ACE dispatch command is created
    // in CmdDispatchMeshTaskAce/CmdDispatchMeshIndirectMultiTaskAce,
    // not here.

    if (IssueSqtt)
    {
        AddDrawSqttMarkers(drawInfo);
    }

    uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto& viewInstancingDesc = pHybridPipeline->GetViewInstancingDesc();
        uint32      mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pDeCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pDeCmdSpace);

                pDeCmdSpace += CmdUtil::BuildDispatchTaskMeshGfx(
                    GetMeshDispatchDimRegAddr(),
                    pLayout->GetMeshRingIndex().regOffset,
                    PacketPredicate(),
                    IssueSqtt,
                    pHybridPipeline->IsLinearDispatch(),
                    pDeCmdSpace);
                // For now, issue the event here. We need CP FW to handle the TaskMesh case.
                pDeCmdSpace = IssueHiSZWarEvent(pDeCmdSpace);
            }
        }
    }
    else
    {
        pDeCmdSpace += CmdUtil::BuildDispatchTaskMeshGfx(
            GetMeshDispatchDimRegAddr(),
            pLayout->GetMeshRingIndex().regOffset,
            PacketPredicate(),
            IssueSqtt,
            pHybridPipeline->IsLinearDispatch(),
            pDeCmdSpace);
        // For now, issue the event here. We need CP FW to handle the TaskMesh case.
        pDeCmdSpace = IssueHiSZWarEvent(pDeCmdSpace);
    }

#if (PAL_BUILD_BRANCH < 2410)
    if (IssueSqtt)
    {
        pDeCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                         EngineTypeUniversal,
                                                         PacketPredicate(),
                                                         pDeCmdSpace);
    }
#endif

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshTask(
    ICmdBuffer*  pCmdBuffer,
    DispatchDims size)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    pThis->CmdDispatchMeshTaskAce<IssueSqtt, ViewInstancingEnable, DescribeDrawDispatch>(size);
    pThis->CmdDispatchMeshTaskGfx<IssueSqtt, ViewInstancingEnable, DescribeDrawDispatch>();
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshIndirectMulti(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto*                   pThis     = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    const GraphicsPipeline* pPipeline =
        static_cast<const GraphicsPipeline*>(pThis->m_graphicsState.pipelineState.pPipeline);

    ValidateDrawInfo drawInfo = {};
    drawInfo.isIndirect       = true;

    // To reduce the number of SET_BASE packets issued, we set the base address of the indirect draw arguments
    // to only the high-bits of the address. This should cover nearly all cases used by clients, so we should only
    // see a single SET_BASE per command buffer.
    drawInfo.indirectDrawArgsHi = HighPart(gpuVirtAddrAndStride.gpuVirtAddr);
    const gpusize offset        = LowPart(gpuVirtAddrAndStride.gpuVirtAddr);

    pThis->ValidateDraw<true>(drawInfo);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        pThis->DescribeDraw(Developer::DrawDispatchType::CmdDispatchMeshIndirectMulti);
    }

    uint32* pCmdSpace = pThis->m_deCmdStream.ReserveCommands();

    if (ViewInstancingEnable)
    {
        const auto&      viewInstancingDesc = pPipeline->GetViewInstancingDesc();
        uint32           mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= pThis->m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pCmdSpace  = pThis->BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
                pCmdSpace += CmdUtil::BuildDispatchMeshIndirectMulti(
                    offset,
                    pThis->GetMeshDispatchDimRegAddr(),
                    pThis->GetDrawIndexRegAddr(),
                    maximumCount,
                    gpuVirtAddrAndStride.stride,
                    countGpuAddr,
                    pThis->PacketPredicate(),
                    IssueSqtt,
                    pCmdSpace);
                // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
                pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
            }
        }
    }
    else
    {
        pCmdSpace += CmdUtil::BuildDispatchMeshIndirectMulti(
            offset,
            pThis->GetMeshDispatchDimRegAddr(),
            pThis->GetDrawIndexRegAddr(),
            maximumCount,
            gpuVirtAddrAndStride.stride,
            countGpuAddr,
            pThis->PacketPredicate(),
            IssueSqtt,
            pCmdSpace);
        // For now, issue the event here. We need CP FW to handle the IndirectMulti case.
        pCmdSpace = pThis->IssueHiSZWarEvent(pCmdSpace);
    }

#if (PAL_BUILD_BRANCH < 2410)
    if (IssueSqtt)
    {
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                       EngineTypeUniversal,
                                                       pThis->PacketPredicate(),
                                                       pCmdSpace);
    }
#endif

    pThis->m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void UniversalCmdBuffer::CmdDispatchMeshIndirectMultiTaskAce(
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    const gpusize indirectGpuAddr = gpuVirtAddrAndStride.gpuVirtAddr;

    PAL_ASSERT(IsPow2Aligned(indirectGpuAddr, sizeof(uint32)));

    const HybridGraphicsPipeline* pHybridPipeline =
        static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const ComputeUserDataLayout* pTaskUserDataLayout = pHybridPipeline->TaskUserDataLayout();

    uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

    pAceCmdSpace = CmdAceWaitDe(pAceCmdSpace);

    pAceCmdSpace = ValidateTaskDispatch<true>(pAceCmdSpace, nullptr, indirectGpuAddr);

    // Issue the DescribeDraw here, after ValidateDraw so that the user data locations are mapped, as they are
    // required for computations in DescribeDraw.
    if (DescribeDrawDispatch)
    {
        DescribeDraw(Developer::DrawDispatchType::CmdDispatchMesh, true);
    }

    if (ViewInstancingEnable)
    {
        const auto& viewInstancingDesc = pHybridPipeline->GetViewInstancingDesc();
        uint32      mask               = (1 << viewInstancingDesc.viewInstanceCount) - 1;

        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }

        for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
        {
            if (TestAnyFlagSet(mask, 1))
            {
                pAceCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pAceCmdSpace);

                if ((PacketPredicate() == PredEnable) && (m_predGpuAddr != 0))
                {
                    pAceCmdSpace += CmdUtil::BuildCondExec(
                        m_predGpuAddr, CmdUtil::DispatchTaskMeshIndirectMecSize, pAceCmdSpace);
                }

                pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshIndirectMultiMec(
                    indirectGpuAddr,
                    pTaskUserDataLayout->GetMeshTaskRingIndex().regOffset,
                    pTaskUserDataLayout->GetTaskDispatchDims().regOffset,
                    pTaskUserDataLayout->GetTaskDispatchIndex().regOffset,
                    maximumCount,
                    gpuVirtAddrAndStride.stride,
                    countGpuAddr,
                    pHybridPipeline->IsTaskWave32(),
                    PacketPredicate(),
                    IssueSqtt,
                    pAceCmdSpace);
            }
        }
    }
    else
    {
        if ((PacketPredicate() == PredEnable) && (m_predGpuAddr != 0))
        {
            pAceCmdSpace += CmdUtil::BuildCondExec(
                m_predGpuAddr, CmdUtil::DispatchTaskMeshIndirectMecSize, pAceCmdSpace);
        }

        pAceCmdSpace += CmdUtil::BuildDispatchTaskMeshIndirectMultiMec(
            indirectGpuAddr,
            pTaskUserDataLayout->GetMeshTaskRingIndex().regOffset,
            pTaskUserDataLayout->GetTaskDispatchDims().regOffset,
            pTaskUserDataLayout->GetTaskDispatchIndex().regOffset,
            maximumCount,
            gpuVirtAddrAndStride.stride,
            countGpuAddr,
            pHybridPipeline->IsTaskWave32(),
            PacketPredicate(),
            IssueSqtt,
            pAceCmdSpace);
    }

#if (PAL_BUILD_BRANCH < 2410)
    if (IssueSqtt)
    {
        pAceCmdSpace += CmdUtil::BuildNonSampleEventWrite(THREAD_TRACE_MARKER,
                                                          EngineTypeCompute,
                                                          PacketPredicate(),
                                                          pAceCmdSpace);
    }
#endif

    m_pAceCmdStream->CommitCommands(pAceCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool ViewInstancingEnable, bool DescribeDrawDispatch>
void PAL_STDCALL UniversalCmdBuffer::CmdDispatchMeshIndirectMultiTask(
    ICmdBuffer*          pCmdBuffer,
    GpuVirtAddrAndStride gpuVirtAddrAndStride,
    uint32               maximumCount,
    gpusize              countGpuAddr)
{
    auto* pThis = static_cast<UniversalCmdBuffer*>(pCmdBuffer);
    pThis->CmdDispatchMeshIndirectMultiTaskAce<IssueSqtt, ViewInstancingEnable, DescribeDrawDispatch>(
        gpuVirtAddrAndStride, maximumCount, countGpuAddr);
    pThis->CmdDispatchMeshTaskGfx<IssueSqtt, ViewInstancingEnable, DescribeDrawDispatch>();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindIndexData(
    gpusize        gpuAddr,
    uint32         indexCount,
    Pal::IndexType indexType)
{
    PAL_ASSERT(IsPow2Aligned(gpuAddr, (1ull << static_cast<uint64>(indexType))));
    PAL_ASSERT((indexType == IndexType::Idx8) || (indexType == IndexType::Idx16) || (indexType == IndexType::Idx32));

    // Index Base and Size are embedded in the draw packets for non-indirect draws

    // Index type is updated frequently per-draw by apps and is often redundant
    if ((indexType != m_graphicsState.iaState.indexType) ||
        (m_gfxState.validBits.indexType == 0))
    {
        constexpr uint32 IndexTypeTbl[] =
        {
            VGT_INDEX_8,   // Pal::IndexType::Uint8
            VGT_INDEX_16,  // Pal::IndexType::Uint16
            VGT_INDEX_32,  // Pal::IndexType::Uint32
        };
        PAL_ASSERT(static_cast<size_t>(indexType) < ArrayLen(IndexTypeTbl));

        static_assert(EnumSameVal(Gfx12TemporalHintsIbReadNormal,       VGT_TEMPORAL_NORMAL)        &&
                      EnumSameVal(Gfx12TemporalHintsIbReadStream,       VGT_TEMPORAL_STREAM)        &&
                      EnumSameVal(Gfx12TemporalHintsIbReadHighPriority, VGT_TEMPORAL_HIGH_PRIORITY) &&
                      EnumSameVal(Gfx12TemporalHintsIbReadDiscard,      VGT_TEMPORAL_DISCARD), "Definition mismatch!");

        VGT_DMA_INDEX_TYPE vgtDmaIndexType{};
        vgtDmaIndexType.bits.INDEX_TYPE = IndexTypeTbl[uint32(indexType)];
        vgtDmaIndexType.bits.TEMPORAL   = VGT_TEMPORAL(m_deviceConfig.temporalHintsIbRead);

        CmdUtil::BuildIndexType(vgtDmaIndexType.u32All, m_deCmdStream.AllocateCommands(CmdUtil::IndexTypeSizeDwords));

        m_gfxState.validBits.indexType    = 1;
        m_graphicsState.iaState.indexType = indexType;
    }

    // Update the currently active index buffer state.
    m_graphicsState.iaState.indexAddr        = gpuAddr;
    m_graphicsState.iaState.indexCount       = indexCount;
    m_graphicsState.dirtyFlags.iaState       = 1;
    m_gfxState.validBits.indexIndirectBuffer = 0;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetVertexBuffers(
    const VertexBufferViews& bufferViews)
{
    PAL_ASSERT(bufferViews.bufferCount > 0);
    PAL_ASSERT((bufferViews.firstBuffer + bufferViews.bufferCount) <= MaxVertexBuffers);
    PAL_ASSERT(bufferViews.pBufferViewInfos != nullptr);

    // Update the CPU copy of the current Vertex Buffers. The CPU copy will be updated at draw-time.
    if (bufferViews.offsetMode)
    {
        VertexBufferView* pViews = &(m_vbTable.bufferViews[bufferViews.firstBuffer]);
        memcpy(pViews, bufferViews.pVertexBufferViews, sizeof(VertexBufferView) * bufferViews.bufferCount);
    }
    else
    {
        GetDevice().CreateUntypedBufferViewSrds(bufferViews.bufferCount, bufferViews.pBufferViewInfos,
                                                &(m_vbTable.srds[bufferViews.firstBuffer]));
    }

    constexpr uint32 DwordsPerBufferView = Util::NumBytesToNumDwords(sizeof(VertexBufferView));
    static_assert(DwordsPerBufferSrd == DwordsPerBufferView);

    if ((DwordsPerBufferSrd * bufferViews.firstBuffer) < m_vbTable.watermarkInDwords)
    {
        // Only update the GPU side copy if VBs were updated that are visible to the current Pipeline.
        m_vbTable.gpuState.dirty = 1;
    }

    m_vbTable.modified = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    // NOTE: The hardware fundamentally does not support multiple border color palettes for compute as the register
    //       which controls the address of the palette is a config register. We need to support this for our clients,
    //       but it should not be considered a correct implementation. As a result we may see arbitrary hangs that
    //       do not reproduce easily. This setting (disableBorderColorPaletteBinds) should be set to TRUE in the event
    //       that one of these hangs is suspected. At that point we will need to come up with a more robust solution
    //       which may involve getting KMD support.
    if ((m_deviceConfig.disableBorderColorPaletteBinds == 0) || (pipelineBindPoint == PipelineBindPoint::Graphics))
    {
        const auto* pGfxPalette = static_cast<const BorderColorPalette*>(pPalette);

        {
            uint32* pCmdSpace = m_deCmdStream.ReserveCommands();
            pCmdSpace = pGfxPalette->WriteCommands(pipelineBindPoint, &m_deCmdStream, pCmdSpace);
            m_deCmdStream.CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
// Updates setting blend consts and manages dirty state
void UniversalCmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    // if (optimize GPU small batch disabled) - just emit commands without filtering.
    // else
    //     if (any consts changed) - emit commands
    //     else if (leak flag isn't set) - this call hasn't been recorded yet so emit commands.
    if ((m_buildFlags.optimizeGpuSmallBatch == 0) ||
        ((memcmp(&params, &(m_graphicsState.blendConstState), sizeof(params)) != 0) ||
         (m_graphicsState.leakFlags.blendConstState == 0)))
    {
        m_graphicsState.blendConstState            = params;
        m_graphicsState.dirtyFlags.blendConstState = 1;

        static_assert(Util::CheckSequential({ mmCB_BLEND_RED,
                                              mmCB_BLEND_GREEN,
                                              mmCB_BLEND_BLUE,
                                              mmCB_BLEND_ALPHA, }),
                      "BlendConst regs are not sequential!");
        static_assert((sizeof(BlendConstParams) == sizeof(uint32) * 4),
                      "BlendConstParams is expected to exactly match the HW regs order/def.");

        // HW reg layout and definition exactly matches the PAL layout and definition
        m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmCB_BLEND_RED, mmCB_BLEND_ALPHA, params.blendConst);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    PAL_ASSERT((numSamplesPerPixel > 0) && (numSamplesPerPixel <= MaxMsaaRasterizerSamples));

    struct PaScMsaaRegs
    {
        // MSAA centroid priorities
        PA_SC_CENTROID_PRIORITY_0          priority0;
        PA_SC_CENTROID_PRIORITY_0          priority1;
        // MSAA sample locations for pixel 0,0 in a 2x2 Quad
        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  x0y0[NumSampleQuadRegs];
        // MSAA sample locations for pixel 1,0 in a 2x2 Quad
        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  x1y0[NumSampleQuadRegs];
        // MSAA sample locations for pixel 0,1 in a 2x2 Quad
        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  x0y1[NumSampleQuadRegs];
        // MSAA sample locations for pixel 1,1 in a 2x2 Quad
        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0  x1y1[NumSampleQuadRegs];
    };

    PA_SC_SAMPLE_PROPERTIES sampleProperties = {};

    // Sort the samples based on distance from the center and calculate the maximum such distance.
    uint8 sortedIndices[MaxMsaaRasterizerSamples] = {};
    uint32 maxSampleDist = 0;

    MsaaState::SortSamples(numSamplesPerPixel, quadSamplePattern, &maxSampleDist, sortedIndices);

    m_graphicsState.quadSamplePatternState = quadSamplePattern;
    m_graphicsState.numSamplesPerPixel     = numSamplesPerPixel;

    const MsaaQuadSamplePattern& defaultSamplePattern = GfxDevice::DefaultSamplePattern[Log2(numSamplesPerPixel)];
    m_graphicsState.useCustomSamplePattern =
        (memcmp(&quadSamplePattern, &defaultSamplePattern, sizeof(MsaaQuadSamplePattern)) != 0);

    m_graphicsState.dirtyFlags.quadSamplePatternState = 1;
    m_nggTable.state.dirty                            = 1;

    sampleProperties.bits.MAX_SAMPLE_DIST = maxSampleDist;
    PaScMsaaRegs msaaRegs = {};
    // setting centroid priorities
    PAL_ASSERT((IsPowerOfTwo(numSamplesPerPixel) && numSamplesPerPixel <= 16));
    const uint32 sampleMask = numSamplesPerPixel - 1;
    // If using fewer than 16 samples, we must fill the extra distance fields by re-cycling through the samples in
    // order as many times as necessary to fill all fields.
    msaaRegs.priority0.u32All =
        (sortedIndices[0] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_0__SHIFT) |
        (sortedIndices[1 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_1__SHIFT) |
        (sortedIndices[2 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_2__SHIFT) |
        (sortedIndices[3 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_3__SHIFT) |
        (sortedIndices[4 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_4__SHIFT) |
        (sortedIndices[5 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_5__SHIFT) |
        (sortedIndices[6 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_6__SHIFT) |
        (sortedIndices[7 & sampleMask] << PA_SC_CENTROID_PRIORITY_0__DISTANCE_7__SHIFT);

    msaaRegs.priority1.u32All =
        (sortedIndices[8 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_8__SHIFT) |
        (sortedIndices[9 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_9__SHIFT) |
        (sortedIndices[10 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_10__SHIFT) |
        (sortedIndices[11 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_11__SHIFT) |
        (sortedIndices[12 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_12__SHIFT) |
        (sortedIndices[13 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_13__SHIFT) |
        (sortedIndices[14 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_14__SHIFT) |
        (sortedIndices[15 & sampleMask] << PA_SC_CENTROID_PRIORITY_1__DISTANCE_15__SHIFT);

    // setting quad pattern
    const SampleLocation* pSampleLocations = nullptr;

    constexpr size_t NumOfPixelsInQuad         = 4;
    constexpr size_t NumSamplesPerRegister     = 4;
    constexpr size_t BitsPerLocationCoordinate = 4;
    constexpr size_t BitMaskLocationCoordinate = 0xF;
    PaScMsaaRegs* pPaScMsaaRegs = &msaaRegs;
    for (uint32 pixIdx = 0; pixIdx < NumOfPixelsInQuad; ++pixIdx)
    {
        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0* pSampleQuadXY0 = nullptr;

        // The pixel coordinates within a sampling pattern (quad) are mapped to the registers as following:
        //    ------------------------------       ---------------
        //    | (topLeft)   | (topRight)   |       | X0Y0 | X1Y0 |
        //    ------------------------------  ==>  ---------------
        //    | (bottomLeft)| (bottomRight)|       | X0Y1 | X1Y1 |
        //    ------------------------------       ---------------
        //

        switch (pixIdx)
        {
        case 0:
            pSampleLocations = &quadSamplePattern.topLeft[0];
            pSampleQuadXY0   = &pPaScMsaaRegs->x0y0[0];
            break;
        case 1:
            pSampleLocations = &quadSamplePattern.topRight[0];
            pSampleQuadXY0   = &pPaScMsaaRegs->x1y0[0];
            break;
        case 2:
            pSampleLocations = &quadSamplePattern.bottomLeft[0];
            pSampleQuadXY0   = &pPaScMsaaRegs->x0y1[0];
            break;
        case 3:
            pSampleLocations = &quadSamplePattern.bottomRight[0];
            pSampleQuadXY0   = &pPaScMsaaRegs->x1y1[0];
            break;
        default:
            PAL_ASSERT_ALWAYS();
            break;
        }

        PA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0* pSampleQuad = pSampleQuadXY0;

        for (uint32 sampleIdx = 0; sampleIdx < numSamplesPerPixel; ++sampleIdx)
        {
            uint32 sampleRegisterIdx = sampleIdx / NumSamplesPerRegister;
            uint32 sampleLocationIdx = sampleIdx % NumSamplesPerRegister;

            pSampleQuad = pSampleQuadXY0 + sampleRegisterIdx;

            const size_t shiftX = (BitsPerLocationCoordinate * 2) * sampleLocationIdx;
            const size_t shiftY = (shiftX + BitsPerLocationCoordinate);

            pSampleQuad->u32All |= ((pSampleLocations[sampleIdx].x & BitMaskLocationCoordinate) << shiftX);
            pSampleQuad->u32All |= ((pSampleLocations[sampleIdx].y & BitMaskLocationCoordinate) << shiftY);
        }
    }
    // write the values into the register pairs
    static_assert(Util::CheckSequential({ mmPA_SC_CENTROID_PRIORITY_0,
                                          mmPA_SC_CENTROID_PRIORITY_1,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_0,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_1,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_2,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y0_3,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y0_0,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y0_1,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y0_2,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y0_3,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y1_0,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y1_1,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y1_2,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X0Y1_3,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_0,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_1,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_2,
                                          mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3, }),
                  "SampleLocRegs are not sequential!");
    static_assert((Util::CheckSequential({ offsetof(PaScMsaaRegs, priority0),
                                           offsetof(PaScMsaaRegs, priority1),
                                           offsetof(PaScMsaaRegs, x0y0) }, sizeof(uint32))) &&
                  (Util::CheckSequential({ offsetof(PaScMsaaRegs, x0y0),
                                           offsetof(PaScMsaaRegs, x1y0),
                                           offsetof(PaScMsaaRegs, x0y1),
                                           offsetof(PaScMsaaRegs, x1y1) }, sizeof(uint32) * NumSampleQuadRegs)),
                  "Storage order of PaScMsaaRegs is important!");

    constexpr uint32 TotalCmdDwords =
        (CmdUtil::SetSeqContextRegsSizeDwords(mmPA_SC_CENTROID_PRIORITY_0, mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3) +
         CmdUtil::SetOneContextRegSizeDwords);

    uint32* pDeCmdSpace = m_deCmdStream.AllocateCommands(TotalCmdDwords);
    pDeCmdSpace = CmdStream::WriteSetSeqContextRegs(mmPA_SC_CENTROID_PRIORITY_0,
                                                    mmPA_SC_AA_SAMPLE_LOCS_PIXEL_X1Y1_3,
                                                    &(pPaScMsaaRegs->priority0.u32All),
                                                    pDeCmdSpace);
    pDeCmdSpace = CmdStream::WriteSetOneContextReg(mmPA_SC_SAMPLE_PROPERTIES,
                                                   sampleProperties.u32All,
                                                   pDeCmdSpace);
}

// =====================================================================================================================
// Sets parameters controlling line stippling.
void UniversalCmdBuffer::CmdSetLineStippleState(
    const LineStippleStateParams& params)
{
    PA_SC_LINE_STIPPLE paScLineStipple = {};

    paScLineStipple.bits.LINE_PATTERN = params.lineStippleValue;
    paScLineStipple.bits.REPEAT_COUNT = params.lineStippleScale;
#if BIGENDIAN_CPU
    paScLineStipple.bits.PATTERN_BIT_ORDER = 1;
#endif

    m_deCmdStream.AllocateAndBuildSetOneContextReg(mmPA_SC_LINE_STIPPLE, paScLineStipple.u32All);

    m_graphicsState.lineStippleState            = params;
    m_graphicsState.dirtyFlags.lineStippleState = 1;
}

// =====================================================================================================================
// Sets parameters controlling point and line rasterization.
void UniversalCmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    m_graphicsState.pointLineRasterState            = params;
    m_graphicsState.dirtyFlags.pointLineRasterState = 1;

    struct PointLineRasterStateRegs
    {
        PA_SU_POINT_SIZE   paSuPointSize;
        PA_SU_POINT_MINMAX paSuPointMinMax;
        PA_SU_LINE_CNTL    paSuLineCntl;
    };

    static_assert(Util::CheckSequential({ mmPA_SU_POINT_SIZE,
                                          mmPA_SU_POINT_MINMAX,
                                          mmPA_SU_LINE_CNTL, }),
                  "PointLineRasterState regs are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(PointLineRasterStateRegs, paSuPointSize),
                                          offsetof(PointLineRasterStateRegs, paSuPointMinMax),
                                          offsetof(PointLineRasterStateRegs, paSuLineCntl) }, sizeof(uint32)),
                  "Storage order of PointLineRasterStateRegs is important!");

    PointLineRasterStateRegs regs = {};

    constexpr uint32 HalfSizeInSubPixels = 0x00000008; // The half size of sub pixels.
    constexpr uint32 MaxPointRadius      = 0x0000ffff; // The maximum radius of the point.
    constexpr uint32 MaxLineWidth        = 0x0000ffff; // The maximum width of the line.

    // Point radius and line width are in 4-bit sub-pixel precision
    const uint32 pointRadius    = Min(static_cast<uint32>(params.pointSize    * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 pointRadiusMin = Min(static_cast<uint32>(params.pointSizeMin * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 pointRadiusMax = Min(static_cast<uint32>(params.pointSizeMax * HalfSizeInSubPixels), MaxPointRadius);
    const uint32 lineWidthHalf  = Min(static_cast<uint32>(params.lineWidth    * HalfSizeInSubPixels), MaxLineWidth);

    regs.paSuPointSize.bits.WIDTH     = pointRadius;
    regs.paSuPointSize.bits.HEIGHT    = pointRadius;

    regs.paSuPointMinMax.bits.MIN_SIZE = pointRadiusMin;
    regs.paSuPointMinMax.bits.MAX_SIZE = pointRadiusMax;

    regs.paSuLineCntl.bits.WIDTH       = lineWidthHalf;

    m_deCmdStream.AllocateAndBuildSetSeqContextRegs(mmPA_SU_POINT_SIZE, mmPA_SU_LINE_CNTL, &regs);
}

constexpr uint32 StencilRefRegs[] =
{
    mmDB_STENCIL_REF,
};

constexpr uint32 StencilMaskRegs[] =
{
    mmDB_STENCIL_READ_MASK,
    mmDB_STENCIL_WRITE_MASK,
};

constexpr uint32 StencilOpValRegs[] =
{
    mmDB_STENCIL_OPVAL,
};

// =====================================================================================================================
// Sets bit-masks to be applied to stencil buffer reads and writes.
void UniversalCmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    if ((m_buildFlags.optimizeGpuSmallBatch == 0) ||
        ((params.flags.u8All != UINT8_MAX) ||
         (memcmp(&params, &(m_graphicsState.stencilRefMaskState), sizeof(params)) != 0) ||
         (m_graphicsState.leakFlags.stencilRefMaskState == 0)))
    {
        if (params.flags.u8All != 0)
        {
            SetStencilRefMasksState(params, &m_graphicsState.stencilRefMaskState);
            m_graphicsState.dirtyFlags.stencilRefMaskState = 1;

            using StencilRefs  = RegPairHandler<decltype(StencilRefRegs), StencilRefRegs>;
            using StencilMasks = RegPairHandler<decltype(StencilMaskRegs), StencilMaskRegs>;
            using StencilOpVal = RegPairHandler<decltype(StencilOpValRegs), StencilOpValRegs>;

            RegisterValuePair stencilRefs[StencilRefs::Size()];
            RegisterValuePair stencilMasks[StencilMasks::Size()];
            RegisterValuePair stencilOpVal[StencilOpVal::Size()];

            StencilRefs::Init(stencilRefs);
            StencilMasks::Init(stencilMasks);
            StencilOpVal::Init(stencilOpVal);

            uint32 numStencilRefRegs   = 0;
            uint32 numStencilMaskRegs  = 0;
            uint32 numStencilOpValRegs = 0;

            // NOTE: We are pulling values from m_graphicsState.stencilRefMaskState below instead of params!
            // This makes it so that if the client only updates partial state - we use previous state it set
            // for any fields which must be written which the client did not explicitly ask to be updated.
            // This allows us to avoid using RMW packets.
            // This means that we are assuming the client has updated all fields at some point in the command
            // recording.
            if (params.flags.updateFrontRef | params.flags.updateBackRef)
            {
                numStencilRefRegs = StencilRefs::Size();

                auto* pStencilRef = StencilRefs::Get<mmDB_STENCIL_REF, DB_STENCIL_REF>(stencilRefs);
                pStencilRef->bits.TESTVAL    = m_graphicsState.stencilRefMaskState.frontRef;
                pStencilRef->bits.TESTVAL_BF = m_graphicsState.stencilRefMaskState.backRef;
            }

            if (params.flags.updateFrontReadMask  | params.flags.updateBackReadMask  |
                params.flags.updateFrontWriteMask | params.flags.updateBackWriteMask)
            {
                numStencilMaskRegs = StencilMasks::Size();

                auto* pReadMask = StencilMasks::Get<mmDB_STENCIL_READ_MASK, DB_STENCIL_READ_MASK>(stencilMasks);
                pReadMask->bits.TESTMASK    = m_graphicsState.stencilRefMaskState.frontReadMask;
                pReadMask->bits.TESTMASK_BF = m_graphicsState.stencilRefMaskState.backReadMask;

                auto* pWriteMask = StencilMasks::Get<mmDB_STENCIL_WRITE_MASK, DB_STENCIL_WRITE_MASK>(stencilMasks);
                pWriteMask->bits.WRITEMASK    = m_graphicsState.stencilRefMaskState.frontWriteMask;
                pWriteMask->bits.WRITEMASK_BF = m_graphicsState.stencilRefMaskState.backWriteMask;
            }

            if (params.flags.updateFrontOpValue | params.flags.updateBackOpValue)
            {
                numStencilOpValRegs = StencilOpVal::Size();

                auto* pOpVal = StencilOpVal::Get<mmDB_STENCIL_OPVAL, DB_STENCIL_OPVAL>(stencilOpVal);
                pOpVal->bits.OPVAL    = m_graphicsState.stencilRefMaskState.frontOpValue;
                pOpVal->bits.OPVAL_BF = m_graphicsState.stencilRefMaskState.backOpValue;
            }

            m_gfxState.dbStencilWriteMask =
                StencilMasks::GetC<mmDB_STENCIL_WRITE_MASK, DB_STENCIL_WRITE_MASK>(stencilMasks);

            static_assert(StencilRefs::Size()  == StencilRefs::NumContext(),  "No other register types expected here.");
            static_assert(StencilMasks::Size() == StencilMasks::NumContext(), "No other register types expected here.");
            static_assert(StencilOpVal::Size() == StencilOpVal::NumContext(), "No other register types expected here.");

            m_deCmdStream.AllocateAndBuildSetContextPairGroups(
                            numStencilRefRegs + numStencilMaskRegs + numStencilOpValRegs,
                            stencilRefs,  numStencilRefRegs,
                            stencilMasks, numStencilMaskRegs,
                            stencilOpVal, numStencilOpValRegs);
        }
    }
}

constexpr uint32 ClipRectRegs[] =
{
    mmPA_SC_CLIPRECT_RULE,
    mmPA_SC_CLIPRECT_0_BR,
    mmPA_SC_CLIPRECT_0_TL,
    mmPA_SC_CLIPRECT_1_BR,
    mmPA_SC_CLIPRECT_1_TL,
    mmPA_SC_CLIPRECT_2_BR,
    mmPA_SC_CLIPRECT_2_TL,
    mmPA_SC_CLIPRECT_3_BR,
    mmPA_SC_CLIPRECT_3_TL,

    mmPA_SC_CLIPRECT_0_EXT,
    mmPA_SC_CLIPRECT_1_EXT,
    mmPA_SC_CLIPRECT_2_EXT,
    mmPA_SC_CLIPRECT_3_EXT,
};

// =====================================================================================================================
// Sets clip rects.
void UniversalCmdBuffer::CmdSetClipRects(
    uint16      clipRule,
    uint32      rectCount,
    const Rect* pRectList)
{
    PAL_ASSERT(rectCount <= MaxClipRects);

    m_graphicsState.clipRectsState.clipRule  = clipRule;
    m_graphicsState.clipRectsState.rectCount = rectCount;
    memcpy(m_graphicsState.clipRectsState.rectList, pRectList, sizeof(Rect) * rectCount);

    m_graphicsState.dirtyFlags.clipRectsState = 1;

    static_assert(Util::CheckSequential({
        mmPA_SC_CLIPRECT_0_EXT,
        mmPA_SC_CLIPRECT_1_EXT,
        mmPA_SC_CLIPRECT_2_EXT,
        mmPA_SC_CLIPRECT_3_EXT,
        }), "ClipRect_Ext are not sequential.");

    using ClipRects = RegPairHandler<decltype(ClipRectRegs), ClipRectRegs>;

    constexpr uint32 NumRegsPerClipRect = mmPA_SC_CLIPRECT_0_BR - mmPA_SC_CLIPRECT_0_TL + 1;

    RegisterValuePair regs[ClipRects::Size()];
    ClipRects::Init(regs);

    PAL_ASSERT(rectCount <= MaxClipRects);

    ClipRects::Get<mmPA_SC_CLIPRECT_RULE, PA_SC_CLIPRECT_RULE>(regs)->bits.CLIP_RULE = clipRule;

    for (uint32 i = 0; i < rectCount; i++)
    {
        const int32 x = pRectList[i].offset.x;
        const int32 y = pRectList[i].offset.y;

        // Top/left is inclusive and right/bottom is exclusive.
        const int32 left   = Clamp<int32>(x, 0, m_deviceConfig.maxScissorSize - 1);
        const int32 top    = Clamp<int32>(y, 0, m_deviceConfig.maxScissorSize - 1);

        // E.g., x = 0, width = 0xFFFFFFFF, int32 will be - 1 and clamp to 0, so use int64 clamp.
        const int32 right  = int32(Clamp<int64>(x + pRectList[i].extent.width,  0, m_deviceConfig.maxScissorSize));
        const int32 bottom = int32(Clamp<int64>(y + pRectList[i].extent.height, 0, m_deviceConfig.maxScissorSize));

        auto* pTl = ClipRects::Get<PA_SC_CLIPRECT_0_TL>(regs, mmPA_SC_CLIPRECT_0_TL + (i * NumRegsPerClipRect));
        auto* pBr = ClipRects::Get<PA_SC_CLIPRECT_0_BR>(regs, mmPA_SC_CLIPRECT_0_BR + (i * NumRegsPerClipRect));

        pTl->bits.TL_X = left;
        pTl->bits.TL_Y = top;
        pBr->bits.BR_X = right;
        pBr->bits.BR_Y = bottom;

        auto* pExt = ClipRects::Get<PA_SC_CLIPRECT_0_EXT>(regs, mmPA_SC_CLIPRECT_0_EXT + i);
        pExt->bits.BR_X_EXT  =
            (right  & ~(PA_SC_CLIPRECT_0_EXT__BR_X_EXT_MASK >> PA_SC_CLIPRECT_0_EXT__BR_X_EXT__SHIFT)) >>
            CountSetBits(PA_SC_CLIPRECT_0_EXT__BR_X_EXT_MASK);
        pExt->bits.BR_Y_EXT  =
            (bottom & ~(PA_SC_CLIPRECT_0_EXT__BR_Y_EXT_MASK >> PA_SC_CLIPRECT_0_EXT__BR_Y_EXT__SHIFT)) >>
            CountSetBits(PA_SC_CLIPRECT_0_EXT__BR_Y_EXT_MASK);
        pExt->bits.TL_X_EXT  =
            (left   & ~(PA_SC_CLIPRECT_0_EXT__TL_X_EXT_MASK >> PA_SC_CLIPRECT_0_EXT__TL_X_EXT__SHIFT)) >>
            CountSetBits(PA_SC_CLIPRECT_0_EXT__TL_X_EXT_MASK);
        pExt->bits.TL_Y_EXT  =
            (top    & ~(PA_SC_CLIPRECT_0_EXT__TL_Y_EXT_MASK >> PA_SC_CLIPRECT_0_EXT__TL_Y_EXT__SHIFT)) >>
            CountSetBits(PA_SC_CLIPRECT_0_EXT__TL_Y_EXT_MASK);
    }

    static_assert(ClipRects::Size() == ClipRects::NumContext(), "No other register types expected here.");
    static_assert(ClipRects::Index(mmPA_SC_CLIPRECT_RULE) == 0, "Unexpected index!");
    static_assert(ClipRects::Index(mmPA_SC_CLIPRECT_0_BR) == 1, "Unexpected index!");
    static_assert(ClipRects::Index(mmPA_SC_CLIPRECT_0_EXT) == (1 + (NumRegsPerClipRect * MaxClipRects)),
                  "Unexpected index!");

    const uint32 numCornerRegs = rectCount * NumRegsPerClipRect;
    const uint32 totalRegs     = 1 /* PA_SC_CLIPRECT_RULE */ + numCornerRegs + rectCount;

    m_deCmdStream.AllocateAndBuildSetContextPairGroups(totalRegs,
                    &regs[ClipRects::Index(mmPA_SC_CLIPRECT_RULE)],  1,
                    &regs[ClipRects::Index(mmPA_SC_CLIPRECT_0_BR)],  numCornerRegs,
                    &regs[ClipRects::Index(mmPA_SC_CLIPRECT_0_EXT)], rectCount);

}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    constexpr uint32 UserClipPlaneRegStride = mmPA_CL_UCP_1_X - mmPA_CL_UCP_0_X;

    // Maximum number of the used defined clip planes.
    constexpr uint32 MaxUserClipPlaneCount  = 0x00000006;

    static_assert(Util::CheckSequential({ mmPA_CL_UCP_0_X,
                                          mmPA_CL_UCP_0_Y,
                                          mmPA_CL_UCP_0_Z,
                                          mmPA_CL_UCP_0_W }) &&
                  ((UserClipPlaneRegStride * MaxUserClipPlaneCount) == ((mmPA_CL_UCP_5_W - mmPA_CL_UCP_0_X) + 1)) &&
                  (PA_CL_UCP_0_X__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_UCP_0_Y__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_UCP_0_Z__DATA_REGISTER_MASK == UINT32_MAX) &&
                  (PA_CL_UCP_0_W__DATA_REGISTER_MASK == UINT32_MAX),
                  "UserClipPlane layout is unexpected!");

    PAL_ASSERT((planeCount > 0) && (firstPlane + planeCount <= MaxUserClipPlaneCount));

    // Make sure that the layout of Pal::UserClipPlane is equivalent to the layout of the PA_CL_UCP_* registers.  This
    // lets us skip copying the data around an extra time.
    static_assert((offsetof(UserClipPlane, x) == 0) &&
                  (offsetof(UserClipPlane, y) == 4) &&
                  (offsetof(UserClipPlane, z) == 8) &&
                  (offsetof(UserClipPlane, w) == 12) &&
                  (sizeof(UserClipPlane) == sizeof(uint32) * 4),
                  "The layout of Pal::UserClipPlane must match the layout of the PA_CL_UCP* registers!");

    const uint32 startRegAddr = mmPA_CL_UCP_0_X + (firstPlane * UserClipPlaneRegStride);
    const uint32 endRegAddr   = mmPA_CL_UCP_0_W + ((firstPlane + planeCount - 1) * UserClipPlaneRegStride);

    m_deCmdStream.AllocateAndBuildSetSeqContextRegs(startRegAddr, endRegAddr, pPlanes);
}

// =====================================================================================================================
// Adds commands necessary to write "data" to the specified memory
void UniversalCmdBuffer::WriteEventCmd(
    const BoundGpuMemory& boundMemObj,
    uint32                stageMask,   // Bitmask of PipelineStageFlag
    uint32                data)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*    pDeCmdSpace         = m_deCmdStream.ReserveCommands();
    const bool issueReleaseMem     = TestAnyFlagSet(stageMask, EopWaitStageMask | VsPsCsWaitStageMask);
    bool       releaseMemWaitCpDma = false;
    bool       cpDmaWaited         = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_deviceConfig.enableReleaseMemWaitCpDma;
        if (releaseMemWaitCpDma == false)
        {
            pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        }
        SetCpBltState(false);
        cpDmaWaited = true;
    }

    // Now pick the packet that actually writes to the event. If multiple flags are set we must go down the path that
    // is most conservative (sets the event at the latest point). This is easiest to implement in this order:
    // 1. The EOS events can wait for one and only one stage. We should check for "only PS" or "only CS" first.
    // 2. Otherwise, all non-CP stages must fall back to an EOP timestamp. We'll go down this path if multiple EOS
    //    stages are specified in the same call and/or any stages that can only be waited on using an EOP timestamp.
    // 3. If no EOS or EOP stages were specified it must be safe to just to a direct write using the PFP or ME.
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if ((stageMask == PipelineStagePs) || (stageMask == PipelineStageCs))
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr   = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel   = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data      = data;
        releaseInfo.vgtEvent  = (stageMask == PipelineStagePs) ? PS_DONE : CS_DONE;
        releaseInfo.waitCpDma = releaseMemWaitCpDma;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pDeCmdSpace);
    }
    else if (issueReleaseMem)
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.dstAddr   = boundMemObj.GpuVirtAddr();
        releaseInfo.dataSel   = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.data      = data;
        releaseInfo.vgtEvent  = BOTTOM_OF_PIPE_TS;
        releaseInfo.waitCpDma = releaseMemWaitCpDma;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pDeCmdSpace);
    }
    else
    {
        const bool pfpWait = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);

        if (pfpWait && cpDmaWaited)
        {
            // The PFP write below must be synchronous with the DMA wait.
            pDeCmdSpace += CmdUtil::BuildPfpSyncMe(pDeCmdSpace);
        }

        WriteDataInfo writeData = {};
        writeData.engineType = GetEngineType();
        writeData.dstAddr    = boundMemObj.GpuVirtAddr();
        writeData.dstSel     = dst_sel__me_write_data__memory;
        writeData.engineSel  = pfpWait ? uint32(engine_sel__pfp_write_data__prefetch_parser)
                                       : uint32(engine_sel__me_write_data__micro_engine);

        pDeCmdSpace += CmdUtil::BuildWriteData(writeData, data, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Bind the last state set on the specified command buffer
void UniversalCmdBuffer::InheritStateFromCmdBuf(
    const GfxCmdBuffer* pCmdBuffer)
{
    GfxCmdBuffer::InheritStateFromCmdBuf(pCmdBuffer);

    if (pCmdBuffer->IsGraphicsSupported())
    {
        const auto*const pUniversalCmdBuffer = static_cast<const UniversalCmdBuffer*>(pCmdBuffer);

        SetGraphicsState(pUniversalCmdBuffer->GetGraphicsState());

        // Was "CmdSetVertexBuffers" ever called on the parent command buffer?
        if (pUniversalCmdBuffer->m_vbTable.modified == 1)
        {
            // Mark this buffer as also modified and copy over the modified SRD's
            m_vbTable.modified  = 1;
            m_vbTable.watermarkInDwords = pUniversalCmdBuffer->m_vbTable.watermarkInDwords;
            memcpy(m_vbTable.srds, pUniversalCmdBuffer->m_vbTable.srds, sizeof(m_vbTable.srds));

            // Set the "dirty" flag for "ValidateGraphicsUserData".
            m_vbTable.gpuState.dirty = 1;
        }
    }
}

// =====================================================================================================================
// Use the GPU's command processor to execute an atomic memory operation
void UniversalCmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    const gpusize address = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;

    CmdUtil::BuildAtomicMem(atomicOp, address, srcData, m_deCmdStream.AllocateCommands(CmdUtil::AtomicMemSizeDwords));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWriteTimestamp(
    uint32            stageMask,    // Bitmask of PipelineStageFlag
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*       pDeCmdSpace         = m_deCmdStream.ReserveCommands();
    const gpusize address             = dstGpuMemory.Desc().gpuVirtAddr + dstOffset;
    const bool    issueReleaseMem     = TestAnyFlagSet(stageMask, EopWaitStageMask | VsPsCsWaitStageMask);
    bool          releaseMemWaitCpDma = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_deviceConfig.enableReleaseMemWaitCpDma;
        if (releaseMemWaitCpDma == false)
        {
            pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        }
        SetCpBltState(false);
    }

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. All non-CP stages must fall back to an EOP timestamp.
    // 2. The CP stages can write the value directly using COPY_DATA in the ME. (PFP doesn't support gpu_clock_count?)
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if (issueReleaseMem)
    {
        ReleaseMemGeneric info = {};
        info.dstAddr     = address;
        info.dataSel     = data_sel__me_release_mem__send_gpu_clock_counter;
        info.vgtEvent    = BOTTOM_OF_PIPE_TS;
        info.waitCpDma   = releaseMemWaitCpDma;
        info.noConfirmWr = true;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(info, pDeCmdSpace);
    }
    else
    {
        CopyDataInfo info = {};
        info.engineType = EngineTypeUniversal;
        info.engineSel  = engine_sel__me_copy_data__micro_engine;
        info.dstSel     = dst_sel__me_copy_data__tc_l2;
        info.dstAddr    = address;
        info.srcSel     = src_sel__me_copy_data__gpu_clock_count;
        info.countSel   = count_sel__me_copy_data__64_bits_of_data;
        info.wrConfirm  = wr_confirm__me_copy_data__do_not_wait_for_confirmation;

        pDeCmdSpace += CmdUtil::BuildCopyData(info, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWriteImmediate(
    uint32             stageMask, // Bitmask of PipelineStageFlag
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    static_assert(uint32(dst_sel__pfp_copy_data__tc_l2) == uint32(dst_sel__me_copy_data__tc_l2),
                  "These are expected to be equal!");
    static_assert(uint32(src_sel__pfp_copy_data__immediate_data) == uint32(src_sel__me_copy_data__immediate_data),
                  "These are expected to be equal!");
    static_assert(uint32(count_sel__pfp_copy_data__32_bits_of_data) == uint32(count_sel__me_copy_data__32_bits_of_data),
                  "These are expected to be equal!");
    static_assert(uint32(count_sel__pfp_copy_data__64_bits_of_data) == uint32(count_sel__me_copy_data__64_bits_of_data),
                  "These are expected to be equal!");
    static_assert(uint32(wr_confirm__pfp_copy_data__wait_for_confirmation) ==
                  uint32(wr_confirm__me_copy_data__wait_for_confirmation),
                  "These are expected to be equal!");

    // This will replace PipelineStageBlt with a more specific set of flags if we haven't done any CP DMAs.
    m_barrierMgr.OptimizeStageMask(this, BarrierType::Global, &stageMask, nullptr);

    uint32*    pDeCmdSpace         = m_deCmdStream.ReserveCommands();
    const bool is32Bit             = (dataSize == ImmediateDataWidth::ImmediateData32Bit);
    const bool issueReleaseMem     = TestAnyFlagSet(stageMask, EopWaitStageMask | VsPsCsWaitStageMask);
    bool       releaseMemWaitCpDma = false;
    bool       cpDmaWaited         = false;

    // We must guarantee that all prior CP DMA accelerated blts have completed before we write this event because
    // this function expect that the prior blts have completed by the time the event is written to memory.
    // Given that our CP DMA blts are asynchronous to the pipeline stages the only way to satisfy this requirement
    // is to force the ME to stall until the CP DMAs are completed.
    if (GfxBarrierMgr::NeedWaitCpDma(this, stageMask))
    {
        releaseMemWaitCpDma = issueReleaseMem && m_deviceConfig.enableReleaseMemWaitCpDma;
        if (releaseMemWaitCpDma == false)
        {
            pDeCmdSpace += CmdUtil::BuildWaitDmaData(pDeCmdSpace);
        }
        SetCpBltState(false);
        cpDmaWaited = true;
    }

    // If multiple flags are set we must go down the path that is most conservative (writes at the latest point).
    // This is easiest to implement in this order:
    // 1. The EOS events can wait for one and only one stage. We should check for "only PS" or "only CS" first
    // 2. Otherwise, all non-CP stages must fall back to an EOP timestamp. We'll go down this path if multiple EOS
    //    stages are specified in the same call and/or any stages that can only be waited on using an EOP timestamp.
    // 3. The CP stages can write the value directly using COPY_DATA, taking care to select the PFP or ME.
    // Note that passing in a stageMask of zero will get you an ME write. It's not clear if that is even legal but
    // doing an ME write is probably the least impactful thing we could do in that case.
    if (issueReleaseMem)
    {
        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.vgtEvent  = TestAllFlagsSet(CsWaitStageMask, stageMask)                   ? CS_DONE :
                                TestAllFlagsSet(VsWaitStageMask | PsWaitStageMask, stageMask) ? PS_DONE :
                                BOTTOM_OF_PIPE_TS;
        releaseInfo.dstAddr   = address;
        releaseInfo.data      = data;
        releaseInfo.dataSel   = is32Bit ? data_sel__me_release_mem__send_32_bit_low
                                        : data_sel__me_release_mem__send_64_bit_data;
        releaseInfo.waitCpDma = releaseMemWaitCpDma;

        pDeCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pDeCmdSpace);
    }
    else
    {
        const bool pfpWait = TestAnyFlagSet(stageMask, PipelineStageTopOfPipe | PipelineStageFetchIndirectArgs);

        if (pfpWait && cpDmaWaited)
        {
            // The PFP write below must be synchronous with the DMA wait.
            pDeCmdSpace += CmdUtil::BuildPfpSyncMe(pDeCmdSpace);
        }

        CopyDataInfo info{};
        info.engineType = EngineTypeUniversal;
        info.engineSel  = pfpWait ? uint32(engine_sel__pfp_copy_data__prefetch_parser) :
                                    uint32(engine_sel__me_copy_data__micro_engine);
        info.dstSel     = dst_sel__pfp_copy_data__tc_l2;
        info.dstAddr    = address;
        info.srcSel     = src_sel__pfp_copy_data__immediate_data;
        info.srcAddr    = data;
        info.countSel   = is32Bit ? count_sel__pfp_copy_data__32_bits_of_data
                                  : count_sel__pfp_copy_data__64_bits_of_data;
        info.wrConfirm  = wr_confirm__pfp_copy_data__wait_for_confirmation;

        pDeCmdSpace += CmdUtil::BuildCopyData(info, pDeCmdSpace);
    }

    m_deCmdStream.CommitCommands(pDeCmdSpace);
}

// =====================================================================================================================
// Dumps this command buffer's command streams to the given file with an appropriate header.
void UniversalCmdBuffer::DumpCmdStreamsToFile(
    File*            pFile,
    CmdBufDumpFormat mode
    ) const
{
    m_deCmdStream.DumpCommands(pFile, "# Universal Queue - Command length = ", mode);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSaveGraphicsState()
{
    Pal::UniversalCmdBuffer::CmdSaveGraphicsState();

    CopyColorTargetViewStorage(m_colorTargetViewRestoreStorage, m_colorTargetViewStorage, &m_graphicsRestoreState);
    CopyDepthStencilViewStorage(&m_depthStencilViewRestoreStorage, &m_depthStencilViewStorage, &m_graphicsRestoreState);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdRestoreGraphicsStateInternal(
    bool trackBltActiveFlags)
{
    Pal::UniversalCmdBuffer::CmdRestoreGraphicsStateInternal(trackBltActiveFlags);

    CopyColorTargetViewStorage(m_colorTargetViewStorage, m_colorTargetViewRestoreStorage, &m_graphicsState);
    CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &m_depthStencilViewRestoreStorage, &m_graphicsState);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCommentString(
    const char* pComment)
{
    const struct
    {
        Pal::CmdStream* pStream;
        Pm4ShaderType   shaderType;
    } streams[] =
    {
        { &m_deCmdStream,  ShaderGraphics, },
        { m_pAceCmdStream, ShaderCompute,  },
    };

    for (uint32 i = 0; i < Util::ArrayLen(streams); i++)
    {
        Pal::CmdStream* pStream = streams[i].pStream;
        if (pStream != nullptr)
        {
            uint32* pCmdSpace = pStream->ReserveCommands();
            pCmdSpace += CmdUtil::BuildCommentString(pComment, streams[i].shaderType, pCmdSpace);
            pStream->CommitCommands(pCmdSpace);
        }
    }
}

// =====================================================================================================================
size_t UniversalCmdBuffer::BuildWriteToZero(
    gpusize       dstAddr,
    uint32        numDwords,
    const uint32* pZeros,
    uint32*       pCmdSpace
    ) const
{
    WriteDataInfo info = {};
    info.engineType = EngineTypeUniversal;
    info.engineSel  = uint32(engine_sel__me_write_data__micro_engine);
    info.dstAddr    = dstAddr;
    info.dstSel     = dst_sel__me_write_data__memory;

    return CmdUtil::BuildWriteData(info, numDwords, pZeros, pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdNop(
    const void* pPayload,
    uint32      payloadSize)
{
    CmdUtil::BuildNopPayload(pPayload, payloadSize,
                             m_deCmdStream.AllocateCommands(CmdUtil::NopPayloadSizeDwords(payloadSize)));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    const auto& pool = static_cast<const QueryPool&>(queryPool);

    if (pool.RequiresSamplingFromGangedAce() && (ImplicitGangedSubQueueCount() < 1))
    {
        // Some types of queries require using the ganged ACE stream _if_ work launched after the query has begun
        // ends up using the ACE.  However, we don't want to create the ganged ACE stream if no "real" work will
        // actually use it.  So track those queriess so that the begin operation can be applied if/when the ganged
        // ACE is initialized.
        if (m_deferredPipelineStatsQueries.PushBack(ActiveQueryState{&pool, slot}) != Result::Success)
        {
            NotifyAllocFailure();
        }
    }

    pool.Begin(this,
               &m_deCmdStream,
               ((ImplicitGangedSubQueueCount() >= 1) ? m_pAceCmdStream : nullptr),
               queryType,
               slot,
               flags);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    const auto& pool = static_cast<const QueryPool&>(queryPool);

    if (pool.RequiresSamplingFromGangedAce() && (ImplicitGangedSubQueueCount() < 1))
    {
        // If this query pool was tracked so that ganged ACE portions of its Begin() operation can be applied when
        // the ganged ACE was initialized, _and_ the ganged ACE never actually ended up being used, then we must
        // remove the pool from our tracking so that it doesn't get overwritten sometime later if the ACE is needed
        // later on in this command buffer.
        for (uint32 i = 0; i < m_deferredPipelineStatsQueries.NumElements(); ++i)
        {
            const ActiveQueryState& state = m_deferredPipelineStatsQueries.At(i);
            if ((state.pQueryPool == &pool) && (state.slot == slot))
            {
                m_deferredPipelineStatsQueries.Erase(i);
                break;
            }
        }
    }

    pool.End(this,
             &m_deCmdStream,
             ((ImplicitGangedSubQueueCount() >= 1) ? m_pAceCmdStream : nullptr),
             queryType,
             slot);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdResolveQuery(
    const IQueryPool&     queryPool,
    Pal::QueryResultFlags flags,
    QueryType             queryType,
    uint32                startQuery,
    uint32                queryCount,
    const IGpuMemory&     dstGpuMemory,
    gpusize               dstOffset,
    gpusize               dstStride)
{
    // Resolving a query is not supposed to honor predication.
    const uint32 packetPredicate = m_cmdBufState.flags.packetPredicate;
    m_cmdBufState.flags.packetPredicate = 0;

    const QueryPool& resolveQueryPool = static_cast<const QueryPool&>(queryPool);

    constexpr uint32 OptCaseWait64      = QueryResult64Bit | QueryResultWait;
    constexpr uint32 OptCaseWait64Accum = QueryResult64Bit | QueryResultWait | QueryResultAccumulate;

    // We can only use the cp packet to do the query resolve in graphics queue also it needs to be an occlusion query
    // with the two flags set. OCCLUSION_QUERY packet resolves a single occlusion query slot.
    // Does not work for BinaryOcclusion.
    // TODO: May use CP resolve path for more cases if CP Firmware support is updated.
    if ((queryType == QueryType::Occlusion) &&
        ((flags == OptCaseWait64) || (flags == OptCaseWait64Accum)))
    {
        // Condition above would be false due to the flags check for equality:
        PAL_ASSERT((flags & QueryResultPreferShaderPath) == 0);

        uint32*    pCmdSpace         = nullptr;
        uint32     remainingResolves = queryCount;
        const bool doAccumulate      = TestAnyFlagSet(flags, QueryResultAccumulate);
        uint32     queryIndex        = 0;

        if (doAccumulate == false)
        {
            // We are using PFP WriteData to zero out the memory so it will not accumulate. We need to make sure
            // PFP is not running ahead of previous commands.
            CmdUtil::BuildPfpSyncMe(m_deCmdStream.AllocateCommands(CmdUtil::PfpSyncMeSizeDwords));
        }

        // Resolve by CP goes through to mall directly and bypasses GL2.
        // Note that SetCpBltState() only applies to CP DMA so we don't need to call it here.
        if (remainingResolves > 0)
        {
            SetCpMemoryWriteL2CacheStaleState(true);
        }

        // If QueryResultAccumulate is not set, we need to write the result to 0 first.
        const uint64 zero             = 0;
        const uint32 writeDataSize    = NumBytesToNumDwords(sizeof(uint64));
        const uint32 writeDataPktSize = PM4_ME_WRITE_DATA_SIZEDW__CORE + writeDataSize;

        const uint32 resolvePerCommit = doAccumulate
            ? m_deCmdStream.ReserveLimit() / PM4_PFP_OCCLUSION_QUERY_SIZEDW__CORE
            : m_deCmdStream.ReserveLimit() / (PM4_PFP_OCCLUSION_QUERY_SIZEDW__CORE + writeDataPktSize);

        while (remainingResolves > 0)
        {
            // Write all of the queries or as many queries as we can fit in a reserve buffer.
            uint32  resolvesToWrite = Min(remainingResolves, resolvePerCommit);

            pCmdSpace          = m_deCmdStream.ReserveCommands();
            remainingResolves -= resolvesToWrite;

            while (resolvesToWrite-- > 0)
            {
                gpusize queryPoolAddr  = 0;
                gpusize resolveDstAddr = dstGpuMemory.Desc().gpuVirtAddr + dstOffset + queryIndex * dstStride;
                Result  result         = resolveQueryPool.GetQueryGpuAddress(queryIndex + startQuery, &queryPoolAddr);

                PAL_ASSERT(result == Result::Success);

                if (result == Result::Success)
                {
                    if (doAccumulate == false)
                    {
                        WriteDataInfo writeData = {};
                        writeData.engineType    = EngineTypeUniversal;
                        writeData.dstAddr       = resolveDstAddr;
                        writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
                        writeData.dstSel        = dst_sel__pfp_write_data__memory;

                        pCmdSpace += CmdUtil::BuildWriteData(writeData,
                                                             writeDataSize,
                                                             reinterpret_cast<const uint32*>(&zero),
                                                             pCmdSpace);
                    }

                    pCmdSpace += CmdUtil::BuildOcclusionQuery(queryPoolAddr, resolveDstAddr, pCmdSpace);
                }
                queryIndex++;
            }
            m_deCmdStream.CommitCommands(pCmdSpace);
        }
    }
    else
    {
        m_rsrcProcMgr.CmdResolveQuery(this,
                                      resolveQueryPool,
                                      flags,
                                      queryType,
                                      startQuery,
                                      queryCount,
                                      static_cast<const GpuMemory&>(dstGpuMemory),
                                      dstOffset,
                                      dstStride);
    }

    m_cmdBufState.flags.packetPredicate = packetPredicate;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    static_cast<const QueryPool&>(queryPool).DoGpuReset(this, &m_deCmdStream, startQuery, queryCount);
}

// =====================================================================================================================
// Updates the DB_COUNT_CONTROL register state based on the current occlusion query state.
uint32* UniversalCmdBuffer::UpdateDbCountControl(
    uint32* pDeCmdSpace)
{
    m_hasOcclusionQueryActive = IsQueryActive(QueryPoolType::Occlusion) &&
                                (NumActiveQueries(QueryPoolType::Occlusion) != 0);

    DB_COUNT_CONTROL dbCountControl                       = {0};
    dbCountControl.bits.DISABLE_CONSERVATIVE_ZPASS_COUNTS = 1;
    dbCountControl.bits.SLICE_EVEN_ENABLE                 = 1;
    dbCountControl.bits.SLICE_ODD_ENABLE                  = 1;

    if (m_hasOcclusionQueryActive)
    {
        // Since 8xx, the ZPass count controls have moved to a separate register call DB_COUNT_CONTROL.
        // PERFECT_ZPASS_COUNTS forces all partially covered tiles to be detail walked, not setting it will count
        // all HiZ passed tiles as 8x#samples worth of zpasses. Therefore in order for vis queries to get the right
        // zpass counts, PERFECT_ZPASS_COUNTS should be set to 1, but this will hurt performance when z passing
        // geometry does not actually write anything (ZFail Shadow volumes for example).

        // Hardware does not enable depth testing when issuing a depth only render pass with depth writes disabled.
        // Unfortunately this corner case prevents depth tiles from being generated and when setting
        // PERFECT_ZPASS_COUNTS = 0, the hardware relies on counting at the tile granularity for binary occlusion
        // queries. With the depth test disabled and PERFECT_ZPASS_COUNTS = 0, there will be 0 tiles generated which
        // will cause the binary occlusion test to always generate depth pass counts of 0.
        // Setting PERFECT_ZPASS_COUNTS = 1 forces tile generation and reliable binary occlusion query results.
        dbCountControl.bits.PERFECT_ZPASS_COUNTS = 1;
        dbCountControl.bits.ZPASS_ENABLE         = 1;
    }

    pDeCmdSpace = CmdStream::WriteSetOneContextReg(mmDB_COUNT_CONTROL, dbCountControl.u32All, pDeCmdSpace);

    return pDeCmdSpace;
}

// =====================================================================================================================
// Validate optimization of the CB export rate when no color is bound.
uint32* UniversalCmdBuffer::ValidateDepthOnlyOpt(
    uint32* pCmdSpace)
{
    // Check if slot 0 is null - this means we must write CB_COLOR0_INFO.
    if (m_graphicsState.bindTargets.colorTargets[0].pColorTargetView == nullptr)
    {
        if (m_deviceConfig.optimizeDepthOnlyFmt && (IsNested() == false))
        {
            const auto* const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
            CB_COLOR0_INFO    cbColor0Info = {};

            if ((pPipeline != nullptr) &&
                pPipeline->CanRbPlusOptimizeDepthOnly(&m_graphicsState.dynamicState) &&
                (m_graphicsState.bindTargets.colorTargetCount == 0))
            {
                cbColor0Info.bits.NUMBER_TYPE = Chip::NUMBER_FLOAT;
                cbColor0Info.bits.FORMAT      = Chip::COLOR_32;
            }

            if ((m_gfxState.validBits.cbColor0Info == 0) ||
                (m_gfxState.cbColor0Info.u32All != cbColor0Info.u32All))
            {
                pCmdSpace = CmdStream::WriteSetOneContextReg(mmCB_COLOR0_INFO, cbColor0Info.u32All, pCmdSpace);

                m_gfxState.validBits.cbColor0Info = 1;
                m_gfxState.cbColor0Info.u32All = cbColor0Info.u32All;
            }
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Enables the specified query type.
void UniversalCmdBuffer::ActivateQueryType(
    QueryPoolType queryPoolType)
{
    switch (queryPoolType)
    {
    case QueryPoolType::PipelineStats:
        CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_START, EngineTypeUniversal,
                                          m_deCmdStream.AllocateCommands(CmdUtil::NonSampleEventWriteSizeDwords));
        break;

    case QueryPoolType::StreamoutStats:
        // TODO: Specific handling for streamout stats query
        break;

    case QueryPoolType::Occlusion:
        m_graphicsState.dirtyFlags.occlusionQueryActive = (m_hasOcclusionQueryActive == 0);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base class function
    Pal::UniversalCmdBuffer::ActivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Disables the specified query type
void UniversalCmdBuffer::DeactivateQueryType(
    QueryPoolType queryPoolType)
{
    switch (queryPoolType)
    {
    case QueryPoolType::PipelineStats:
        CmdUtil::BuildNonSampleEventWrite(PIPELINESTAT_STOP, EngineTypeUniversal,
                                          m_deCmdStream.AllocateCommands(CmdUtil::NonSampleEventWriteSizeDwords));
        break;

    case QueryPoolType::StreamoutStats:
        // TODO: Specific handling for streamout stats query
        break;

    case QueryPoolType::Occlusion:
        m_graphicsState.dirtyFlags.occlusionQueryActive = m_hasOcclusionQueryActive;
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Call base class function
    Pal::UniversalCmdBuffer::DeactivateQueryType(queryPoolType);
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with starting a query
void UniversalCmdBuffer::AddQuery(
    QueryPoolType     queryType,
    QueryControlFlags flags)
{
    if (IsFirstQuery(queryType))
    {
        if (queryType == QueryPoolType::Occlusion)
        {
            // Activate queries on first AddQuery call
            ActivateQueryType(queryType);
        }
        else if (queryType == QueryPoolType::PipelineStats)
        {
            if (m_deviceConfig.enablePreamblePipelineStats == 0)
            {
                ActivateQueryType(queryType);
            }
            m_graphicsState.dirtyFlags.pipelineStatsQuery = 1;
        }
        else if (queryType == QueryPoolType::StreamoutStats)
        {
            m_graphicsState.dirtyFlags.streamoutStatsQuery = 1;
        }
        else
        {
            // What is this?
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
// Adds PM4 commands needed to write any registers associated with ending the last active query in this command buffer.
void UniversalCmdBuffer::RemoveQuery(
    QueryPoolType queryPoolType)
{
    if (IsLastActiveQuery(queryPoolType))
    {
        if (queryPoolType == QueryPoolType::Occlusion)
        {
            // Deactivate queries on last RemoveQuery call
            DeactivateQueryType(queryPoolType);
        }
        else if (queryPoolType == QueryPoolType::PipelineStats)
        {
            // We're not bothering with PIPELINE_STOP events, as leaving these counters running doesn't hurt anything
            m_graphicsState.dirtyFlags.pipelineStatsQuery = 1;
        }
        else if (queryPoolType == QueryPoolType::StreamoutStats)
        {
            m_graphicsState.dirtyFlags.streamoutStatsQuery = 1;
        }
        else
        {
            // What is this?
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::WriteBeginEndOcclusionQueryCmds(
    gpusize dstAddr)
{
    const size_t packetSize =
        CmdUtil::BuildSampleEventWrite(PIXEL_PIPE_STAT_DUMP,
                                       event_index__me_event_write__pixel_pipe_stat_control_or_dump,
                                       EngineTypeUniversal,
                                       samp_plst_cntr_mode__mec_event_write__legacy_mode,
                                       dstAddr,
                                       m_deCmdStream.AllocateCommands(CmdUtil::SampleEventWriteZpassSizeDwords));

    // This packet is tricky so make sure we used the right size.
    PAL_ASSERT(packetSize == size_t(CmdUtil::SampleEventWriteZpassSizeDwords));
}

struct OrderedIdPair
{
    uint32 orderedId;
    uint32 dwordsWritten;
};

#pragma pack(push, 1)
struct StreamoutCtrlBufLayout
{
    uint32        bufferOffset[MaxStreamOutTargets];
    uint64        primsNeeded[MaxStreamOutTargets];
    uint64        primsWritten[MaxStreamOutTargets];
    OrderedIdPair orderedIdPair[MaxStreamOutTargets];
};
#pragma pack(pop)

// =====================================================================================================================
// Verifies that the streamout control buffer address is non-zero. If the address is zero, we allocate the necessary
// memory and initialize the memory to zero.
uint32* UniversalCmdBuffer::VerifyStreamoutCtrlBuf(
    uint32* pCmdSpace)
{
    if (m_streamoutCtrlBuf == 0)
    {
        // The base address of the orderedId must be 64B aligned in memory to support "a single atomic with 4 lanes
        // enabled".
        constexpr uint32 OrderedIdAddrAlignmentBytes     = 64;
        // CP FW needs to send ACQUIRE_MEM packet to do range-based flush/invalidates on the streamout control buffer.
        // But the size granularity and the start of the surface for ACQUIRE_MEM packet both requires 128B aligned.
        constexpr uint32 StreamoutCtrlBufAllocAlignBytes = 128;
        constexpr uint32 OrderedIdOffset                 = offsetof(StreamoutCtrlBufLayout, orderedIdPair);
        // In order to achieve both alignment requirements, so we have to allocate a larger buffer and adjust the base
        // address of streamout control buffer.
        constexpr uint32 StreamoutCtrlBufAllocSize       = 256;

        gpusize    offset        = 0;
        GpuMemory* pGpuMem       = nullptr;
        gpusize    allocGpuMemVa = AllocateGpuScratchMem(StreamoutCtrlBufAllocSize / sizeof(uint32),
                                                         StreamoutCtrlBufAllocAlignBytes / sizeof(uint32),
                                                         &pGpuMem,
                                                         &offset);

        PAL_ASSERT(allocGpuMemVa != 0);

        m_streamoutCtrlBuf = Pow2Align(allocGpuMemVa + OrderedIdOffset, OrderedIdAddrAlignmentBytes) - OrderedIdOffset;

        PAL_ASSERT(IsPow2Aligned(m_streamoutCtrlBuf + OrderedIdOffset, OrderedIdAddrAlignmentBytes));

        // We need to initialize this buffer to all zeros to start.
        WriteDataInfo writeData = {};
        writeData.engineType    = EngineTypeUniversal;
        writeData.dstAddr       = m_streamoutCtrlBuf;
        writeData.engineSel     = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel        = dst_sel__pfp_write_data__memory;

        constexpr StreamoutCtrlBufLayout DummyControlBuffer = {};
        pCmdSpace              += CmdUtil::BuildWriteData(writeData,
                                                          sizeof(StreamoutCtrlBufLayout) / sizeof(uint32),
                                                          reinterpret_cast<const uint32*>(&DummyControlBuffer),
                                                          pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize(&gpuVirtAddr)[MaxStreamOutTargets])
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace  = VerifyStreamoutCtrlBuf(pCmdSpace);
    pCmdSpace += CmdUtil::BuildLoadBufferFilledSizes(m_streamoutCtrlBuf, &gpuVirtAddr[0], pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSetBufferFilledSize(
    uint32 bufferId,
    uint32 offset)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace  = VerifyStreamoutCtrlBuf(pCmdSpace);
    pCmdSpace += CmdUtil::BuildSetBufferFilledSize(m_streamoutCtrlBuf, bufferId, offset, pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize(&gpuVirtAddr)[MaxStreamOutTargets])
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    pCmdSpace  = VerifyStreamoutCtrlBuf(pCmdSpace);
    pCmdSpace += CmdUtil::BuildSaveBufferFilledSizes(m_streamoutCtrlBuf, &gpuVirtAddr[0], pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    bool areAnyNewOrPrevBuffersNonNull = false;

    const auto* const pPipeline = static_cast<const GraphicsPipeline* const>(GetGraphicsState().pipelineState.pPipeline);

    // Create SRDs on the stack based on the client-specified view info.
    BufferViewInfo viewInfos[MaxStreamOutTargets] = {};

    for (uint32 i = 0; i < MaxStreamOutTargets; i++)
    {
        const uint32 strideInBytes   = ((pPipeline == nullptr) ? 0 :
                                       pPipeline->StrmoutVtxStrideDw(i)) * sizeof(uint32);
        viewInfos[i].gpuAddr         = params.target[i].gpuVirtAddr;
        viewInfos[i].range           = params.target[i].size;
        viewInfos[i].stride          = (strideInBytes > 0) ? 1 : 0;
        viewInfos[i].swizzledFormat  = UndefinedSwizzledFormat;
        viewInfos[i].compressionMode = CompressionMode::ReadEnableWriteDisable;

        if ((params.target[i].gpuVirtAddr != 0) || (m_graphicsState.bindStreamOutTargets.target[i].gpuVirtAddr != 0))
        {
            areAnyNewOrPrevBuffersNonNull = true;
        }
    }

    if (areAnyNewOrPrevBuffersNonNull)
    {
        GetDevice().CreateUntypedBufferViewSrds(MaxStreamOutTargets, &viewInfos[0], &m_streamOut.srd[0]);

        uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

        pCmdSpace = VerifyStreamoutCtrlBuf(pCmdSpace);

        // We need to initialize or reset GE_GS_ORDERED_ID_BASE and orderedId to 0. If these values are
        // not reset, the system will be prone to hanging because the the streamout algorithm's critical section
        // relies on the values being equal.
        constexpr uint32        OrderedIdReset = 0;
        constexpr OrderedIdPair OrderedIdPairsReset[MaxStreamOutTargets] = {};

        // Perform a VS_PARTIAL_FLUSH before writing to GE_GS_ORDERED_ID_BASE or the streamout ctrl buf.
        pCmdSpace += CmdUtil::BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, m_engineType, pCmdSpace);

        // Reset all orderedIds in streamout ctrl buf.
        WriteDataInfo writeData = {};
        writeData.engineType    = m_engineType;
        writeData.dstAddr       = m_streamoutCtrlBuf + offsetof(StreamoutCtrlBufLayout, orderedIdPair);
        writeData.engineSel     = engine_sel__me_write_data__micro_engine;
        writeData.dstSel        = dst_sel__me_write_data__memory;

        pCmdSpace  = m_deCmdStream.WriteSetOneUConfigReg<true>(mmGE_GS_ORDERED_ID_BASE, OrderedIdReset, pCmdSpace);

        pCmdSpace += CmdUtil::BuildWriteData(writeData,
                                             sizeof(OrderedIdPairsReset) / sizeof(uint32),
                                             reinterpret_cast<const uint32*>(&OrderedIdPairsReset[0]),
                                             pCmdSpace);

        m_deCmdStream.CommitCommands(pCmdSpace);
    }
    m_streamOut.state.dirty = 1;

    m_graphicsState.bindStreamOutTargets        = params;
    m_graphicsState.dirtyFlags.streamOutTargets = 1;
}

// =====================================================================================================================
void UniversalCmdBuffer::CallNestedCmdBuffer(
    const UniversalCmdBuffer* pCallee)
{
    // Track the most recent OS paging fence value across all nested command buffers called from this one.
    m_lastPagingFence = Max(m_lastPagingFence, pCallee->LastPagingFence());

    // Track the lastest fence token across all nested command buffers called from this one.
    m_maxUploadFenceToken = Max(m_maxUploadFenceToken, pCallee->GetMaxUploadFenceToken());

    const bool exclusiveSubmit   = pCallee->IsExclusiveSubmit();
    const bool allowIb2Launch = ((IsNested() == false) &&
                                 (GetEngineType() == EngineTypeUniversal)) ? pCallee->AllowLaunchViaIb2() : false;

    m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_embeddedData.chunkList);
    m_deCmdStream.TrackNestedEmbeddedData(pCallee->m_gpuScratchMem.chunkList);
    m_deCmdStream.TrackNestedCommands(pCallee->m_deCmdStream);

    if ((pCallee->m_pAceCmdStream != nullptr) && (pCallee->m_pAceCmdStream->IsEmpty() == false))
    {
        TryInitAceGangedSubmitResources();
        m_pAceCmdStream->TrackNestedCommands(*(pCallee->m_pAceCmdStream));
        m_pAceCmdStream->Call(*(pCallee->m_pAceCmdStream), exclusiveSubmit, false);
    }

    m_deCmdStream.Call(pCallee->m_deCmdStream, exclusiveSubmit, allowIb2Launch);

    if (allowIb2Launch)
    {
        TrackIb2DumpInfoFromExecuteNestedCmds(pCallee->m_deCmdStream);

        if ((pCallee->m_pAceCmdStream != nullptr) && (pCallee->m_pAceCmdStream->IsEmpty() == false))
        {
            TrackIb2DumpInfoFromExecuteNestedCmds(*(pCallee->m_pAceCmdStream));
        }
    }

}

// =====================================================================================================================
void UniversalCmdBuffer::ValidateExecuteNestedCmdBuffer(
    )
{
    // In the event that occlusion queries have been started in the parent CmdBuffer but no draw has been executed,
    // we need to make sure to update DB_COUNT_CONTROL before the nested CmdBuffers are executed.
    if (m_graphicsState.dirtyFlags.occlusionQueryActive)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = UpdateDbCountControl(pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    if (m_graphicsState.pipelineState.dirtyFlags.pipeline || m_graphicsState.dirtyFlags.colorTargetView)
    {
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();
        pDeCmdSpace = ValidateDepthOnlyOpt(pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }

    // If the MSAA DS target workaround is active and we have a depth/stencil target bound, we need to consider the
    // possibility that any of the draws in the nested command buffer could trigger the bug.
    if ((m_deviceConfig.workarounds.waDbForceStencilValid != 0) &&
        m_gfxState.szValid                                      &&
        (m_gfxState.dsLog2NumSamples > 0))
    {
        DB_RENDER_OVERRIDE dbRenderOverride = m_gfxState.dbRenderOverride;

        // Assume that a depth-stencil state could be bound which could trigger the bug.
        dbRenderOverride.bits.FORCE_STENCIL_VALID = 1;

        if (dbRenderOverride.u32All != m_gfxState.dbRenderOverride.u32All)
        {
            m_deCmdStream.AllocateAndBuildSetOneContextReg(mmDB_RENDER_OVERRIDE, dbRenderOverride.u32All);

            m_gfxState.dbRenderOverride           = dbRenderOverride;
            m_gfxState.validBits.dbRenderOverride = 1;
        }
    }

    // If the HiSZ workaround is active and we have a depth/stencil target bound, we need to consider the possibility
    // that any of the draws in the nested command buffer could trigger the bug.
    // As a result, we need to disable HiZ/S for the currently bound range.
    // TODO: There may be an optimization here where we can track if certain pieces of state were ever seen in the
    //       nested command buffer (basically OR the register states), and then run it through the detection
    //       logic to see if we can leave it enabled still.
    const DepthStencilView* pDepthStencilView =
        static_cast<const DepthStencilView*>(m_graphicsState.bindTargets.depthTarget.pDepthStencilView);
    if ((pDepthStencilView != nullptr) &&
        pDepthStencilView->GetImage()->HasHiSZStateMetaData())
    {
        const SubresRange range = pDepthStencilView->ViewRange();
        uint32* pDeCmdSpace = m_deCmdStream.ReserveCommands();

        if (m_deviceConfig.workarounds.forceReZWhenHiZsDisabledWa &&
            (m_graphicsState.pipelineState.pPipeline != nullptr))
        {
            const GraphicsPipeline* pGfxPipeline =
                static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

            m_gfxState.dbShaderControl      = pGfxPipeline->DbShaderControl();
            m_gfxState.noForceReZ           = (m_deviceConfig.workarounds.forceReZWhenHiZsDisabledWa == 0) ||
                                              pGfxPipeline->NoForceReZ();
        }

        pDeCmdSpace += pDepthStencilView->OverrideHiZHiSEnable(false,
                                                               m_gfxState.dbShaderControl,
                                                               m_gfxState.noForceReZ,
                                                               pDeCmdSpace);

        pDeCmdSpace = pDepthStencilView->GetImage()->UpdateHiSZStateMetaData(range,
                                                                             false,
                                                                             PacketPredicate(),
                                                                             GetEngineType(),
                                                                             pDeCmdSpace);
        m_deCmdStream.CommitCommands(pDeCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32             cmdBufferCount,
    ICmdBuffer* const* ppCmdBuffers)
{
    ValidateExecuteNestedCmdBuffer(
        );

    for (uint32 buf = 0; buf < cmdBufferCount; ++buf)
    {
        auto* const pCallee = static_cast<Gfx12::UniversalCmdBuffer*>(ppCmdBuffers[buf]);
        PAL_ASSERT(pCallee != nullptr);

        if ((pCallee->m_inheritedPredGpuAddr != 0uLL) && (m_predGpuAddr != 0uLL))
        {
            const CopyDataInfo copyDataInfo =
            {
                .engineType = EngineTypeUniversal,
                .engineSel  = engine_sel__pfp_copy_data__prefetch_parser,
                .dstSel     = dst_sel__me_copy_data__tc_l2,
                .dstAddr    = pCallee->m_inheritedPredGpuAddr,
                .srcSel     = src_sel__me_copy_data__tc_l2,
                .srcAddr    = m_predGpuAddr,
                .countSel   = count_sel__me_copy_data__32_bits_of_data,
                .wrConfirm  = wr_confirm__me_copy_data__wait_for_confirmation
            };
            m_cmdUtil.BuildCopyData(copyDataInfo, m_deCmdStream.AllocateCommands(CmdUtil::CopyDataSizeDwords));
        }

        CallNestedCmdBuffer(pCallee);

        // Callee command buffers are also able to leak any changes they made to bound user-data entries and any other
        // state back to the caller.
        LeakNestedCmdBufferState(*pCallee);
    }
}

// =====================================================================================================================
// Helper method for handling the state "leakage" from a nested command buffer back to its caller. Since the callee has
// tracked its own state during the building phase, we can access the final state of the command buffer since its stored
// in the UniversalCmdBuffer object itself.
void UniversalCmdBuffer::LeakNestedCmdBufferState(
    const UniversalCmdBuffer& cmdBuffer)
{
    Pal::UniversalCmdBuffer::LeakNestedCmdBufferState(cmdBuffer);

    const auto* pNestedGfxState = &cmdBuffer.m_gfxState;
    auto*       pRootGfxState   = &m_gfxState;

    m_vbTable.gpuState.dirty     |= cmdBuffer.m_vbTable.modified;
    m_vbTable.watermarkInDwords   = cmdBuffer.m_vbTable.watermarkInDwords;

    // To be safe we assume that the nested command buffer clobbered all tracked state. We will set some of these back
    // below when we leak valid state from the nested command buffer to the caller. Note that all of these may still be
    // set to valid values on the GPU, we just can't know what that value is in our CPU-side redundancy filtering.
    pRootGfxState->validBits.u32All = 0;

    if (pNestedGfxState->validBits.firstVertex != 0)
    {
        pRootGfxState->drawArgs.firstVertex  = pNestedGfxState->drawArgs.firstVertex;
        pRootGfxState->validBits.firstVertex = 1;
    }

    if (pNestedGfxState->validBits.firstInstance != 0)
    {
        pRootGfxState->drawArgs.firstInstance  = pNestedGfxState->drawArgs.firstInstance;
        pRootGfxState->validBits.firstInstance = 1;
    }

    if (pNestedGfxState->validBits.instanceCount != 0)
    {
        pRootGfxState->drawArgs.instanceCount = pNestedGfxState->drawArgs.instanceCount;
        pRootGfxState->validBits.instanceCount = 1;
    }

    if (pNestedGfxState->validBits.drawIndex != 0)
    {
        pRootGfxState->drawArgs.drawIndex  = pNestedGfxState->drawArgs.drawIndex;
        pRootGfxState->validBits.drawIndex = 1;
    }

    if (pNestedGfxState->validBits.meshDispatchDims != 0)
    {
        pRootGfxState->drawArgs.meshDispatchDims  = pNestedGfxState->drawArgs.meshDispatchDims;
        pRootGfxState->validBits.meshDispatchDims = 1;
    }

    if (pNestedGfxState->validBits.indirectDrawArgsHi != 0)
    {
        pRootGfxState->drawArgs.indirectDrawArgsHi  = pNestedGfxState->drawArgs.indirectDrawArgsHi;
        pRootGfxState->validBits.indirectDrawArgsHi = 1;
    }

    if (pNestedGfxState->validBits.pipelineCtxLowHash != 0)
    {
        pRootGfxState->pipelineCtxLowPktHash        = pNestedGfxState->pipelineCtxLowPktHash;
        pRootGfxState->validBits.pipelineCtxLowHash = 1;
    }

    if (pNestedGfxState->validBits.pipelineCtxMedHash != 0)
    {
        pRootGfxState->pipelineCtxMedPktHash = pNestedGfxState->pipelineCtxMedPktHash;
        pRootGfxState->validBits.pipelineCtxMedHash = 1;
    }

    if (pNestedGfxState->validBits.pipelineCtxHighHash != 0)
    {
        pRootGfxState->pipelineCtxHighPktHash        = pNestedGfxState->pipelineCtxHighPktHash;
        pRootGfxState->validBits.pipelineCtxHighHash = 1;
    }

    if (pNestedGfxState->validBits.batchBinnerState != 0)
    {
        pRootGfxState->batchBinnerState.binSizeX        = pNestedGfxState->batchBinnerState.binSizeX;
        pRootGfxState->batchBinnerState.binSizeY        = pNestedGfxState->batchBinnerState.binSizeY;
        pRootGfxState->batchBinnerState.paScBinnerCntl0 = pNestedGfxState->batchBinnerState.paScBinnerCntl0;
        pRootGfxState->validBits.batchBinnerState = 1;
    }

    if (pNestedGfxState->validBits.paScModeCntl1 != 0)
    {
        pRootGfxState->paScModeCntl1 = pNestedGfxState->paScModeCntl1;
        pRootGfxState->validBits.paScModeCntl1 = 1;
    }

    if (pNestedGfxState->validBits.paSuLineStippleCntl != 0)
    {
        pRootGfxState->paSuLineStippleCntl           = pNestedGfxState->paSuLineStippleCntl;
        pRootGfxState->validBits.paSuLineStippleCntl = 1;
    }

    if (pNestedGfxState->validBits.indexIndirectBuffer != 0)
    {
        pRootGfxState->validBits.indexIndirectBuffer = 1;
    }

    if (pNestedGfxState->validBits.computeDispatchInterleave != 0)
    {
        pRootGfxState->computeDispatchInterleave           = pNestedGfxState->computeDispatchInterleave;
        pRootGfxState->validBits.computeDispatchInterleave = 1;
    }

    if (pNestedGfxState->validBits.dbRenderOverride != 0)
    {
        pRootGfxState->dbRenderOverride           = pNestedGfxState->dbRenderOverride;
        pRootGfxState->validBits.dbRenderOverride = 1;
    }

    memcpy(pRootGfxState->psInterpolants, pNestedGfxState->psInterpolants,
           pNestedGfxState->validBits.interpCount * sizeof(uint32));
    pRootGfxState->validBits.interpCount = Max(pRootGfxState->validBits.interpCount,
                                               pNestedGfxState->validBits.interpCount);

    if (pNestedGfxState->validBits.inputAssemblyCtxState != 0)
    {
        pRootGfxState->paScLineStippleReset    = pNestedGfxState->paScLineStippleReset;
        pRootGfxState->vgtMultiPrimIbResetIndx = pNestedGfxState->vgtMultiPrimIbResetIndx;
        pRootGfxState->validBits.inputAssemblyCtxState = 1;
    }

    if (pNestedGfxState->validBits.paClVrsCntl != 0)
    {
        pRootGfxState->paClVrsCntl           = pNestedGfxState->paClVrsCntl;
        pRootGfxState->validBits.paClVrsCntl = 1;
    }

    // Copying back cbColor0Info will never be valid on the nested cmd buffer, just leave it invalid.

    const GraphicsStateFlags& gfxLeakFlags       = cmdBuffer.m_graphicsState.leakFlags;
    auto* const               pSrcWalkAlignState = &pNestedGfxState->paScWalkAlignState;
    auto* const               pDstWalkAlignState = &pRootGfxState->paScWalkAlignState;

    if (gfxLeakFlags.globalScissorState != 0)
    {
        pDstWalkAlignState->globalScissorIn64K = pSrcWalkAlignState->globalScissorIn64K;
    }

    if (gfxLeakFlags.scissorRects != 0)
    {
        pDstWalkAlignState->scissorRectsIn64K = pSrcWalkAlignState->scissorRectsIn64K;
    }

    if ((gfxLeakFlags.colorTargetView != 0) || (gfxLeakFlags.depthStencilView != 0))
    {
        pDstWalkAlignState->targetIn64K = pSrcWalkAlignState->targetIn64K;
    }

    if (gfxLeakFlags.depthStencilView != 0)
    {
        pDstWalkAlignState->hasHiSZ     = pSrcWalkAlignState->hasHiSZ;
        pRootGfxState->dsLog2NumSamples = pNestedGfxState->dsLog2NumSamples;
        pRootGfxState->szValid          = pNestedGfxState->szValid;
    }

    if (gfxLeakFlags.depthStencilState != 0)
    {
        pRootGfxState->dbStencilControl = pNestedGfxState->dbStencilControl;
    }

    if (gfxLeakFlags.vrsImage != 0)
    {
        pDstWalkAlignState->hasVrsImage = pSrcWalkAlignState->hasVrsImage;
    }

    if (gfxLeakFlags.stencilRefMaskState != 0)
    {
        pRootGfxState->dbStencilWriteMask = pNestedGfxState->dbStencilWriteMask;
        pRootGfxState->validBits.hiszWorkaround = 0;
    }

    if (pSrcWalkAlignState->dirty != 0)
    {
        pDstWalkAlignState->dirty = 1;
    }

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        pRootGfxState->vertexOffsetReg      = pNestedGfxState->vertexOffsetReg;
        pRootGfxState->drawIndexReg         = pNestedGfxState->drawIndexReg;
        pRootGfxState->viewIdsReg           = pNestedGfxState->viewIdsReg;
        pRootGfxState->meshDispatchDimsReg  = pNestedGfxState->meshDispatchDimsReg;
        pRootGfxState->nggCullingDataReg    = pNestedGfxState->nggCullingDataReg;
        pRootGfxState->cbTargetMask         = pNestedGfxState->cbTargetMask;
        pRootGfxState->pipelinePsHash       = pNestedGfxState->pipelinePsHash;
        pRootGfxState->dbShaderControl      = pNestedGfxState->dbShaderControl;
        pRootGfxState->noForceReZ           = pNestedGfxState->noForceReZ;
        m_nggTable.numSamples               = cmdBuffer.m_nggTable.numSamples;
    }

    if (gfxLeakFlags.colorTargetView != 0)
    {
        CopyColorTargetViewStorage(m_colorTargetViewStorage, cmdBuffer.m_colorTargetViewStorage, &m_graphicsState);
    }

    if (gfxLeakFlags.depthStencilView != 0)
    {
        CopyDepthStencilViewStorage(&m_depthStencilViewStorage, &cmdBuffer.m_depthStencilViewStorage, &m_graphicsState);
    }

    m_dispatchPingPongEn = cmdBuffer.m_dispatchPingPongEn;

    if (cmdBuffer.m_indirectDispatchArgsValid)
    {
        m_indirectDispatchArgsValid  = cmdBuffer.m_indirectDispatchArgsValid;
        m_indirectDispatchArgsAddrHi = cmdBuffer.m_indirectDispatchArgsAddrHi;
    }

    if (cmdBuffer.m_graphicsState.pipelineState.pPipeline != nullptr)
    {
        m_pPrevGfxUserDataLayoutValidatedWith = cmdBuffer.m_pPrevGfxUserDataLayoutValidatedWith;
    }

    if (cmdBuffer.m_computeState.pipelineState.pPipeline != nullptr)
    {
        m_pPrevComputeUserDataLayoutValidatedWith = cmdBuffer.m_pPrevComputeUserDataLayoutValidatedWith;
    }

    if (cmdBuffer.m_pComputeStateAce != nullptr)
    {
        *m_pComputeStateAce = *cmdBuffer.m_pComputeStateAce;
    }

    m_nggTable.state.dirty |= cmdBuffer.m_nggTable.state.dirty;

    // Nested cmdbuffer always updates the CB/DB High bases even if CmdBindTargets isn't recorded since the Preamble
    // primes their state.
    m_writeCbDbHighBaseRegs = cmdBuffer.m_writeCbDbHighBaseRegs;

    SetShaderRingSize(cmdBuffer.m_ringSizes);

    // Reset any tracking related to m_previousTargetsMetadata/m_currentTargetsMetadata. Nested cmd buffers don't patch
    // and they shouldn't even call CmdBindTargets (except for gfx BLT cases).
    memset(&m_currentTargetsMetadata,  0, sizeof(m_currentTargetsMetadata));
    memset(&m_previousTargetsMetadata, 0, sizeof(m_previousTargetsMetadata));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    const DmaDataInfo dmaData =
    {
        .dstSel       = dst_sel__pfp_dma_data__dst_addr_using_das,
        .dstAddr      = dstGpuMemory.Desc().gpuVirtAddr + dstOffset,
        .dstAddrSpace = das__pfp_dma_data__memory,
        .srcSel       = src_sel__pfp_dma_data__src_addr_using_sas,
        .srcAddr      = srcRegisterOffset,
        .srcAddrSpace = sas__pfp_dma_data__register,
        .usePfp       = false,
        .sync         = true
    };
    CmdUtil::BuildDmaData<false>(dmaData, m_deCmdStream.AllocateCommands(CmdUtil::DmaDataSizeDwords));
}

// =====================================================================================================================
// Helper function for updating a command buffer's tracking of which user-data entries have known values after running
// an indirect-command generator and executing the generated commands.
static void CommandGeneratorTouchedUserData(
    const IndirectCmdGenerator& generator,
    size_t*                     pMask)
{
    // Mark any user-data entries which the command generator touched as "untouched" so that redundant user-data
    // filtering won't incorrectly reject subsequent user-data updates.
    for (uint32 idx = 0; idx < NumUserDataFlagsParts; ++idx)
    {
        pMask[idx] &= ~(generator.TouchedUserDataEntries()[idx]);
    }
}

// =====================================================================================================================
// Validation of the ExecuteIndirectOperation.
void UniversalCmdBuffer::ValidateExecuteIndirect(
    const IndirectCmdGenerator& gfx12Generator,
    const bool                  isGfx,
    const uint32                maximumCount,
    const gpusize               countGpuAddr,
    const bool                  isTaskEnabled,
    bool*                       pEnable2dDispatchInterleave)
{
    if (isGfx)
    {
        if (isTaskEnabled)
        {
            IssueGangedBarrierAceWaitDeIncr();
            uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();

            pAceCmdSpace = CmdAceWaitDe(pAceCmdSpace);

            // Just validate with arbitrary dispatch dims here as the real dims are in GPU memory which we don't
            // know at this point.
            constexpr DispatchDims LogicalSize = { .x = 1, .y = 1, .z = 1 };
            pAceCmdSpace = ValidateTaskDispatch<true>(pAceCmdSpace, &LogicalSize, 0);
            m_pAceCmdStream->CommitCommands(pAceCmdSpace);
        }
        ValidateDrawInfo drawInfo   = {};
        drawInfo.isIndirect         = true;
        drawInfo.isAdvancedIndirect = true;
        drawInfo.multiIndirectDraw  = ((maximumCount > 1) || (countGpuAddr != 0uLL)) &&
                                       (gfx12Generator.UseConstantDrawIndex() == false);
        drawInfo.isIndexed          = gfx12Generator.ContainIndexBuffer();

        ValidateDraw<true>(drawInfo);

        CommandGeneratorTouchedUserData(gfx12Generator, &m_graphicsState.gfxUserDataEntries.touched[0]);
    }
    else
    {
        const auto* pCsPipeline = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
        uint32*     pCmdSpace   = m_deCmdStream.ReserveCommands();

        // Just validate with arbitrary dispatch dims here as the real dims are in GPU memory which we don't
        // know at this point.
        constexpr DispatchDims LogicalSize = { .x = 1, .y = 1, .z = 1 };

        // This is Execute Indirect call here so indirect argument buffer shouldn't be passed for numWorkGroupReg.
        pCmdSpace = ValidateDispatchPalAbi<true, false>(
            pCmdSpace,
            &m_computeState,
            &m_spillTable.stateCompute,
            pCsPipeline->UserDataLayout(),
            &m_pPrevComputeUserDataLayoutValidatedWith,
            &LogicalSize,
            0,    // indirectGpuVirtAddr
            true, // allow2dDispatchInterleave
            pEnable2dDispatchInterleave);

        m_deCmdStream.CommitCommands(pCmdSpace);

        CommandGeneratorTouchedUserData(gfx12Generator, &m_computeState.csUserDataEntries.touched[0]);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::VbUserDataSpillTableHelper(
    const IndirectCmdGenerator& generator,
    const UserDataLayout*       pUserDataLayout,
    const uint32                vertexBufTableDwords,
    const bool                  isGfx,
    gpusize*                    pSpillTableAddress,
    uint32*                     pSpillTableStrideBytes)
{
    const GeneratorProperties& properties = generator.Properties();

    const uint32 spillThreshold = pUserDataLayout->GetSpillThreshold();
    const bool   userDataSpills = (spillThreshold != NoUserDataSpilling);

    const uint32 spillDwords = (pUserDataLayout->GetSpillThreshold() <= properties.userDataWatermark) ?
        properties.maxUserDataEntries : 0;

    *pSpillTableStrideBytes = Pow2Align(uint32((spillDwords + vertexBufTableDwords) * sizeof(uint32)),
                                        EiSpillTblStrideAlignmentBytes);
    uint32 spillTableStrideDwords = NumBytesToNumDwords(*pSpillTableStrideBytes);

    // UserData that spills over the assigned SGPRs is also modified by this generator and we will need to create
    // and handle SpillTable/s + VertexBuffer/s. We manage the VertexBuffer/SRD as part of the SpillTable Buffer.
    // Memory layout is [VertexBuffer + SpillTable].
    if (spillTableStrideDwords > 0)
    {
        // ExecuteIndirectV2 needs to maintain a single instance of UserData for the copy over to the queue
        // specific reserved memory buffer with the CP InitMemCpy operation. CP UpdateMemCpy operation will then
        // update UserData slots based on data from the Argument Buffer.
        // Allocate and populate Spill+VBTable Buffer with UserData. Each instance of the SpillTable and
        // VertexBuffer needs to be initialized with UserDataEntries of current context.
        uint32* pUserDataSpace = CmdAllocateEmbeddedData(spillTableStrideDwords,
                                                         EiSpillTblStrideAlignmentDwords,
                                                         pSpillTableAddress);

        PAL_ASSERT(pUserDataSpace != nullptr);

        if (isGfx)
        {
            if (vertexBufTableDwords != 0)
            {
                // MemCpy the VB entries needed by this pipeline.
                memcpy(pUserDataSpace, &m_vbTable.srds[0], sizeof(uint32) * vertexBufTableDwords);
            }
            if (spillDwords != 0)
            {

                memcpy(&pUserDataSpace[vertexBufTableDwords],
                       m_graphicsState.gfxUserDataEntries.entries,
                       sizeof(uint32) * spillDwords);
            }
        }
        // Compute
        else
        {
            if (spillDwords != 0)
            {
                memcpy(pUserDataSpace, m_computeState.csUserDataEntries.entries, sizeof(uint32) * spillDwords);
            }
        }
    }
}

// =====================================================================================================================
// Construct some portions of the ExecuteIndirect operation and fill the corresponding execute indirect packet info.
void UniversalCmdBuffer::PreprocessExecuteIndirect(
    const IndirectCmdGenerator& generator,
    const bool                  isGfx,
    const bool                  isTaskEnabled,
    const IPipeline*            pPipeline,
    ExecuteIndirectPacketInfo*  pPacketInfo,
    ExecuteIndirectMeta*        pMeta,
    const EiDispatchOptions&    options,
    const EiUserDataRegs&       regs)
{
    gpusize spillTableAddress      = 0;
    uint32  spillTableStrideBytes  = 0;

    ExecuteIndirectPacketInfo* pGfxPacketInfo = &pPacketInfo[EiEngineGfx];
    ExecuteIndirectMeta*       pGfxMeta       = &pMeta[EiEngineGfx];

    const UserDataLayout* pUserDataLayout = nullptr;

    const uint32                  cmdCount   = generator.ParameterCount();
    const IndirectParamData*const pParamData = generator.GetIndirectParamData();

    const GeneratorProperties& properties = generator.Properties();

    const uint32 vertexBufTableDwords = isGfx ? m_vbTable.watermarkInDwords : 0;

    // Graphics Pipeline (Indirect Draw)
    if (isGfx)
    {
        const auto* pGfxPipeline = static_cast<const GraphicsPipeline*>(pPipeline);
        pUserDataLayout          = pGfxPipeline->UserDataLayout();

        VbUserDataSpillTableHelper(generator,
                                   pUserDataLayout,
                                   vertexBufTableDwords,
                                   isGfx,
                                   &spillTableAddress,
                                   &spillTableStrideBytes);

        // Handle Task Shader case separately.
        if (isTaskEnabled)
        {
            ExecuteIndirectPacketInfo* pAcePacketInfo = &pPacketInfo[EiEngineAce];
            ExecuteIndirectMeta*       pAceMeta       = &pMeta[EiEngineAce];

            gpusize spillTableAddressAce     = 0;
            uint32  spillTableStrideAceBytes = 0;

            const auto*                  pHybridPipeline     = static_cast<const HybridGraphicsPipeline*>(pPipeline);
            const ComputeUserDataLayout* pTaskUserDataLayout = pHybridPipeline->TaskUserDataLayout();
            const UserDataLayout*        pAceUserDataLayout  = static_cast<const UserDataLayout*>(pTaskUserDataLayout);

            VbUserDataSpillTableHelper(generator,
                                       pAceUserDataLayout,
                                       0,                 // vertexBufTableDwords is 0 for Task in ACE.
                                       false,             // isGfx is false for ACE.
                                       &spillTableAddressAce,
                                       &spillTableStrideAceBytes);

            generator.PopulateExecuteIndirectParams(pPipeline,
                                                    isGfx,
                                                    true,           // CmdBuffer on ACE queue.
                                                    pAcePacketInfo,
                                                    pAceMeta,
                                                    0,              // vertexBufTableDwords is 0 for Task in ACE.
                                                    options,
                                                    regs);

            if (m_deviceConfig.issueSqttMarkerEvent)
            {
                pAceMeta->GetMetaData()->threadTraceEnable = isTaskEnabled;
            }

            // Fill in execute indirect packet info.
            pAcePacketInfo->spillTableAddr        = spillTableAddressAce;
            pAcePacketInfo->spillTableStrideBytes = spillTableStrideAceBytes;
            pAcePacketInfo->pUserDataLayout       = pAceUserDataLayout;
        }
    }

    // Compute Pipeline (Indirect Dispatch)
    else
    {
        const auto* pCsPipeline = static_cast<const ComputePipeline*>(pPipeline);
        pUserDataLayout         = pCsPipeline->UserDataLayout();

        VbUserDataSpillTableHelper(generator,
                                   pUserDataLayout,
                                   vertexBufTableDwords,
                                   isGfx,
                                   &spillTableAddress,
                                   &spillTableStrideBytes);
    }

    generator.PopulateExecuteIndirectParams(pPipeline,
                                            isGfx,
                                            false,         // CmdBuffer not on ACE queue.
                                            pGfxPacketInfo,
                                            pGfxMeta,
                                            vertexBufTableDwords,
                                            options,
                                            regs);

    pGfxMeta->GetMetaData()->threadTraceEnable |= m_deviceConfig.issueSqttMarkerEvent;

    // Fill in execute indirect packet info.
    pGfxPacketInfo->spillTableAddr        = spillTableAddress;
    pGfxPacketInfo->spillTableStrideBytes = spillTableStrideBytes;
    pGfxPacketInfo->pUserDataLayout       = pUserDataLayout;
}

// =====================================================================================================================
// This method helps create a CP packet to perform the ExecuteIndirect operation. We do this in 3 steps (1) Validate,
// (2) Pre-process and (3) Build PM4(s).
void UniversalCmdBuffer::ExecuteIndirectPacket(
    const IIndirectCmdGenerator& generator,
    const gpusize                gpuVirtAddr,
    const uint32                 maximumCount,
    const gpusize                countGpuAddr,
    const bool                   isTaskEnabled)
{
    const auto& gfx12Generator = static_cast<const IndirectCmdGenerator&>(generator);

    // The generation of indirect commands is determined by the currently-bound pipeline.
    const auto* pGfxPipeline    = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    const auto* pCsPipeline     = static_cast<const ComputePipeline*>(m_computeState.pipelineState.pPipeline);
    const auto* pHybridPipeline = static_cast<const HybridGraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    const bool       isGfx     = (gfx12Generator.Type() != GeneratorType::Dispatch);
    const IPipeline* pPipeline = (isGfx) ? static_cast<const IPipeline*>(pGfxPipeline) :
                                           static_cast<const IPipeline*>(pCsPipeline);

    uint32      mask         = 1;
    uint32*     pCmdSpace    = nullptr;
    uint32*     pAceCmdSpace = nullptr;

    if (isGfx && (pGfxPipeline->HwStereoRenderingEnabled() == false))
    {
        const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();
        mask = (1 << viewInstancingDesc.viewInstanceCount) - 1;
        if (viewInstancingDesc.enableMasking)
        {
            mask &= m_graphicsState.viewInstanceMask;
        }
    }

    bool enable2dDispatchInterleave = false;

    // This loop is for ViewInstancing.
    for (uint32 i = 0; mask != 0; ++i, mask >>= 1)
    {
        if (TestAnyFlagSet(mask, 1) == false)
        {
            continue;
        }

        // Step 1:-> Validate Draw/Dispatch Ops.
        ValidateExecuteIndirect(gfx12Generator,
                                isGfx,
                                maximumCount,
                                countGpuAddr,
                                isTaskEnabled,
                                &enable2dDispatchInterleave);

        // Step 2:-> Pre-process ExecuteIndirect.
        // Initialize the params required to program the EI V2 PM4. Although the ACE params will only be needed during
        // ganged submission for Task+Mesh case we still init them here.

        // From the UniversalCmdBuffer if we have a ganged submission the CmdBuffer could have 2 components one of
        // which is submitted to the GFX (PFP/ME) engine and another to the ACE (MEC) engine.

        // packetInfo[engineGfx] and packetInfo[engineAce]
        ExecuteIndirectPacketInfo packetInfo[EiEngineCount] = {};

        // For the EI V2 PM4 that will be submitted on Universal/Gfx queue.
        packetInfo[EiEngineGfx].argumentBufferAddr        = gpuVirtAddr;
        packetInfo[EiEngineGfx].countBufferAddr           = countGpuAddr;
        packetInfo[EiEngineGfx].argumentBufferStrideBytes = gfx12Generator.Properties().argBufStride;
        packetInfo[EiEngineGfx].maxCount                  = maximumCount;

        // For the EI V2 PM4 that will be submitted on the Compute/ACE queue in a gang submission when Task is enabled.
        packetInfo[EiEngineAce] = packetInfo[EiEngineGfx];

        // packetOp[engineGfx] and packetOp[engineAce]
        ExecuteIndirectOp   packetOp[EiEngineCount] = {};
        // meta[engineGfx] and meta[engineAce]
        ExecuteIndirectMeta meta[EiEngineCount]     = {};

        const EiDispatchOptions options =
        {
            .enable2dInterleave    = enable2dDispatchInterleave,
            .pingPongEnable        = (pCsPipeline != nullptr) ? GetDispatchPingPongEn() : false,
            .usesDispatchTunneling = UsesDispatchTunneling(),
            .isLinearDispatch      = ((m_deviceConfig.cpPfpVersion >= EiV2LinearDispatchFixPfpVersion) &&
                                      (pHybridPipeline != nullptr)) ? pHybridPipeline->IsLinearDispatch()
                                                                    : false,
            .isWave32              = (pCsPipeline != nullptr) ? pCsPipeline->IsWave32()
                                                              : pHybridPipeline->IsTaskWave32()
        };

        const GraphicsUserDataLayout* pGraphicsUserDataLayout = isGfx ? pGfxPipeline->UserDataLayout() :
                                                                        nullptr;

        const ComputeUserDataLayout* pComputeUserDataLayout = isGfx ? nullptr :
                                                                      pCsPipeline->UserDataLayout();

        const ComputeUserDataLayout* pTaskUserDataLayout = isTaskEnabled ? pHybridPipeline->TaskUserDataLayout() :
                                                                           nullptr;

        const EiUserDataRegs regs =
        {
            // Gfx
            .instOffsetReg           = GetInstanceOffsetRegAddr(),
            .vtxOffsetReg            = GetVertexOffsetRegAddr(),
            .vtxTableReg             = uint16((pGraphicsUserDataLayout != nullptr) ?
                                              pGraphicsUserDataLayout->GetVertexBufferTable().regOffset : 0u),
            .drawIndexReg            = uint8(GetDrawIndexRegAddr()),
            .meshDispatchDimsReg     = uint8(GetMeshDispatchDimRegAddr()),
            .meshRingIndexReg        = uint8((pGraphicsUserDataLayout != nullptr) ?
                                             pGraphicsUserDataLayout->GetMeshRingIndex().regOffset : 0u),
            .numWorkGroupReg         = uint16((pComputeUserDataLayout != nullptr) ?
                                              pComputeUserDataLayout->GetWorkgroup().regOffset : 0u),
            // Ace (Task + Mesh)
            .aceMeshTaskRingIndexReg = uint16((pTaskUserDataLayout != nullptr) ?
                                              pTaskUserDataLayout->GetMeshTaskRingIndex().regOffset : 0u),
            .aceTaskDispatchDimsReg  = uint16((pTaskUserDataLayout != nullptr) ?
                                              pTaskUserDataLayout->GetTaskDispatchDims().regOffset  : 0u),
            .aceTaskDispatchIndexReg = uint16((pTaskUserDataLayout != nullptr) ?
                                              pTaskUserDataLayout->GetTaskDispatchIndex().regOffset : 0u)
        };

        // The rest of the packet info needs to be filled based on the input param buffer.
        PreprocessExecuteIndirect(gfx12Generator,
                                  isGfx,
                                  isTaskEnabled,
                                  pPipeline,
                                  packetInfo,
                                  meta,
                                  options,
                                  regs);

        // Step3:-> Setup and Build PM4(s).

        // The GlobalSpillTable for EI V2 is only used when there will be updateMemCopy Ops (UserData SpillTable
        // changes between consecutive Draw/Dispatch Ops) or there is a buildSrd Op (VBTable). The FW expects the full
        // allocation for a HW workaround. So we allocate it every time.
        SetExecuteIndirectV2GlobalSpill(isTaskEnabled);

        pCmdSpace = m_deCmdStream.ReserveCommands();

        if (isGfx)
        {
            const auto& viewInstancingDesc = pGfxPipeline->GetViewInstancingDesc();
            pCmdSpace = BuildWriteViewId(viewInstancingDesc.viewId[i], pCmdSpace);
        }

        // We disable MCBP whenever there is an EI V2 PM4 in this CmdStream submission before the fix went in.
        if (m_deviceConfig.cpPfpVersion < EiV2McbpFixPfpVersion)
        {
            m_deCmdStream.DisablePreemption();
        }

        if (isTaskEnabled)
        {
            pAceCmdSpace = m_pAceCmdStream->ReserveCommands();
            pAceCmdSpace += CmdUtil::BuildExecuteIndirectV2Ace(PacketPredicate(),
                                                               packetInfo[EiEngineAce],
                                                               &meta[EiEngineAce],
                                                               pAceCmdSpace);
            m_pAceCmdStream->CommitCommands(pAceCmdSpace);
        }
        pCmdSpace += CmdUtil::BuildExecuteIndirectV2Gfx(PacketPredicate(),
                                                        isGfx,
                                                        packetInfo[EiEngineGfx],
                                                        &meta[EiEngineGfx],
                                                        pCmdSpace);

        // For now, issue the event here. We need CP FW to handle the ExecuteIndirect case.
        pCmdSpace = IssueHiSZWarEvent(pCmdSpace);

        m_deCmdStream.CommitCommands(pCmdSpace);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    gpusize                      gpuVirtAddr,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    const auto& gfx12Generator = static_cast<const IndirectCmdGenerator&>(generator);
    const auto* pGfxPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);

    if (m_deviceConfig.describeDrawDispatch)
    {
        DescribeExecuteIndirectCmds(this, static_cast<uint32>(gfx12Generator.Type()));
    }

    const bool isTaskEnabled = ((gfx12Generator.Type() == GeneratorType::DispatchMesh) &&
                                pGfxPipeline->HasTaskShader());
    if (isTaskEnabled)
    {
        // Update PayloadData Ring and TaskMeshRing sizes. This essentially marks them as being actively used in this
        // CmdBuffer and sets up future state as necessary in the Preamble.
        m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)] =
            Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::PayloadData)], 1);

        m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)] =
            Max<size_t>(m_ringSizes.itemSize[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)], 1);

        GetAceCmdStream();
        ReportHybridPipelineBind();
    }
    ExecuteIndirectPacket(generator, gpuVirtAddr, maximumCount, countGpuAddr, isTaskEnabled);

    // The ExecuteIndirectPacket in CP FW will overwrite the SET_BASE values for both draw and dispatch.
    // Mark them invalid so we are forced to rewrite them after.
    m_gfxState.validBits.indirectDrawArgsHi = 0;
    m_indirectDispatchArgsValid             = false;
}

// =====================================================================================================================
// Copy memory using the CP's DMA engine
void UniversalCmdBuffer::CopyMemoryCp(
    gpusize dstAddr,
    gpusize srcAddr,
    gpusize numBytes)
{
    DmaDataInfo dmaDataInfo =
    {
        .dstSel    = dst_sel__pfp_dma_data__dst_addr_using_l2,
        .dstAddr   = dstAddr,
        .srcSel    = src_sel__pfp_dma_data__src_addr_using_l2,
        .srcAddr   = srcAddr,
        .usePfp    = false,
        .sync      = false,
        .predicate = PacketPredicate()
    };

    while (numBytes > 0)
    {
        // The numBytes arg is a gpusize so we must upcast, clamp against MaxDmaDataByteCount, then safely downcast.
        dmaDataInfo.numBytes = uint32(Min(numBytes, gpusize(CmdUtil::MaxDmaDataByteCount)));

        CmdUtil::BuildDmaData<false>(dmaDataInfo, m_deCmdStream.AllocateCommands(CmdUtil::DmaDataSizeDwords));

        dmaDataInfo.dstAddr += dmaDataInfo.numBytes;
        dmaDataInfo.srcAddr += dmaDataInfo.numBytes;
        numBytes            -= dmaDataInfo.numBytes;
    }

    SetCpBltState(true);
    SetCpMemoryWriteL2CacheStaleState(true);

#if PAL_DEVELOPER_BUILD
    Developer::RpmBltData cbData = { .pCmdBuffer = this, .bltType = Developer::RpmBltType::CpDmaCopy };
    m_device.Parent()->DeveloperCb(Developer::CallbackType::RpmBlt, &cbData);
#endif
}

// =====================================================================================================================
bool UniversalCmdBuffer::IsPreemptable() const
{
    return m_deCmdStream.IsPreemptionEnabled();
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WriteWaitEop(
    WriteWaitEopInfo info,
    uint32*          pCmdSpace)
{
    SyncGlxFlags       glxSync   = SyncGlxFlags(info.hwGlxSync);
    const SyncRbFlags  rbSync    = SyncRbFlags(info.hwRbSync);
    const AcquirePoint acqPoint  = AcquirePoint(info.hwAcqPoint);
    const bool         waitCpDma = info.waitCpDma;

    bool waitAtPfpOrMe = false;

    if ((info.disablePws == false) && m_deviceConfig.pwsEnabled)
    {
        // We should always prefer a PWS sync over a wait for EOP timestamp because it avoids all TS memory accesses.
        // It can also push the wait point further down the graphics pipeline in some cases.
        pCmdSpace += m_cmdUtil.BuildWaitEopPws(acqPoint, waitCpDma, glxSync, rbSync, pCmdSpace);

        waitAtPfpOrMe = (acqPoint <= AcquirePointMe);
    }
    else
    {
        // Can optimize acquire at Eop case if hit it.
        PAL_ASSERT(acqPoint != AcquirePointEop);

        waitAtPfpOrMe = true;

        // Issue explicit waitCpDma packet if ReleaseMem doesn't support it.
        bool releaseMemWaitCpDma = waitCpDma;
        if (waitCpDma && (m_deviceConfig.enableReleaseMemWaitCpDma == false))
        {
            pCmdSpace += CmdUtil::BuildWaitDmaData(pCmdSpace);
            releaseMemWaitCpDma = false;
        }

        ReleaseMemGeneric releaseInfo = {};
        releaseInfo.cacheSync = CmdUtil::SelectReleaseMemCaches(&glxSync);
        releaseInfo.dataSel   = data_sel__me_release_mem__send_32_bit_low;
        releaseInfo.dstAddr   = GetWaitIdleTsGpuVa(&pCmdSpace);
        releaseInfo.data      = WaitIdleTsValue();
        releaseInfo.vgtEvent  = CmdUtil::SelectEopEvent(rbSync);
        releaseInfo.waitCpDma = releaseMemWaitCpDma;

        pCmdSpace += m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, pCmdSpace);

        pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                              mem_space__me_wait_reg_mem__memory_space,
                                              function__me_wait_reg_mem__equal_to_the_reference_value,
                                              engine_sel__me_wait_reg_mem__micro_engine,
                                              releaseInfo.dstAddr,
                                              uint32(releaseInfo.data),
                                              UINT32_MAX,
                                              pCmdSpace);

        // If we still have some caches to sync we require a final acquire_mem. It doesn't do any waiting, it just
        // immediately does some full-range cache flush and invalidates. The previous WRM packet is the real wait.
        if (glxSync != SyncGlxNone)
        {
            AcquireMemGeneric acquireMem = {};
            acquireMem.engineType = EngineTypeUniversal;
            acquireMem.cacheSync  = glxSync;
            pCmdSpace += m_cmdUtil.BuildAcquireMemGeneric(acquireMem, pCmdSpace);
        }

        if (acqPoint == AcquirePointPfp)
        {
            pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
        }
    }

    if (waitCpDma)
    {
        SetCpBltState(false);
    }

    if (waitAtPfpOrMe)
    {
        for (uint32 i = 0; i < ReleaseTokenCpDma; i++)
        {
            UpdateRetiredAcqRelFenceVal(ReleaseTokenType(i), GetCurAcqRelFenceVal(ReleaseTokenType(i)));
        }

        SetCsBltState(false);

        if ((GetCmdBufState().flags.csBltActive == 0) &&
            TestAllFlagsSet(glxSync, SyncGl2WbInv | SyncGlvInv | SyncGlkInv))
        {
            SetCsBltWriteCacheState(false);
        }

        // The previous EOP event and wait mean that anything prior to this point, including previous command
        // buffers on this queue, have completed.
        SetPrevCmdBufInactive();

        if ((GetCmdBufState().flags.cpBltActive == 0) &&
            TestAllFlagsSet(glxSync, SyncGl2Inv | SyncGlvInv | SyncGlkInv))
        {
            SetCpMemoryWriteL2CacheStaleState(false);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::WriteWaitCsIdle(
    uint32* pCmdSpace)
{
    pCmdSpace += CmdUtil::BuildNonSampleEventWrite(CS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);

    SetCsBltState(false);

    UpdateRetiredAcqRelFenceVal(ReleaseTokenCsDone, GetCurAcqRelFenceVal(ReleaseTokenCsDone));

    return pCmdSpace;
}

// =====================================================================================================================
// Enables or disables a flexible predication check which the CP uses to determine if a draw or dispatch can be skipped
// based on the results of prior GPU work.
// SEE: CmdUtil::BuildSetPredication(...) for more details on the meaning of this method's parameters.
void UniversalCmdBuffer::CmdSetPredication(
    IQueryPool*       pQueryPool,
    uint32            slot,
    const IGpuMemory* pGpuMemory,
    gpusize           offset,
    PredicateType     predType,
    bool              predPolarity,
    bool              waitResults,
    bool              accumulateData)
{
    PAL_ASSERT((pQueryPool == nullptr) || (pGpuMemory == nullptr));

    m_cmdBufState.flags.clientPredicate = ((pQueryPool != nullptr) || (pGpuMemory != nullptr)) ? 1 : 0;
    m_cmdBufState.flags.packetPredicate = m_cmdBufState.flags.clientPredicate;

    gpusize gpuVirtAddr = 0;
    if (pGpuMemory != nullptr)
    {
        gpuVirtAddr = pGpuMemory->Desc().gpuVirtAddr + offset;
    }

    if (pQueryPool != nullptr)
    {
        Result result = static_cast<QueryPool*>(pQueryPool)->GetQueryGpuAddress(slot, &gpuVirtAddr);
        PAL_ASSERT(result == Result::Success);
    }

    // Clear/disable predicate
    if ((pQueryPool == nullptr) && (gpuVirtAddr == 0))
    {
        predType = static_cast<PredicateType>(0);
    }

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    // If the predicate is 32-bits and the engine does not support that width natively, allocate a 64-bit
    // embedded predicate, zero it, emit a ME copy from the original to the lower 32-bits of the embedded
    // predicate, and update `gpuVirtAddr` and `predType`.
    if ((predType == PredicateType::Boolean32) && (m_deviceConfig.has32bitPredication == 0))
    {
        PAL_ASSERT(gpuVirtAddr != 0);
        constexpr size_t PredicateDwordSize  = sizeof(uint64) / sizeof(uint32);
        constexpr size_t PredicateDwordAlign = 16 / sizeof(uint32);
        gpusize predicateVirtAddr            = 0;
        uint32* pPredicate                   = CmdAllocateEmbeddedData(PredicateDwordSize,
                                                                       PredicateDwordAlign,
                                                                       &predicateVirtAddr);
        pPredicate[0] = 0;
        pPredicate[1] = 0;

        CopyDataInfo copyDataInfo = {};
        copyDataInfo.engineType   = EngineTypeUniversal;
        copyDataInfo.engineSel    = engine_sel__pfp_copy_data__prefetch_parser;
        copyDataInfo.dstSel       = dst_sel__me_copy_data__tc_l2;
        copyDataInfo.dstAddr      = predicateVirtAddr;
        copyDataInfo.srcSel       = src_sel__me_copy_data__tc_l2;
        copyDataInfo.srcAddr      = gpuVirtAddr;
        copyDataInfo.countSel     = count_sel__me_copy_data__32_bits_of_data;
        copyDataInfo.wrConfirm    = wr_confirm__me_copy_data__wait_for_confirmation;

        pCmdSpace += CmdUtil::BuildCopyData(copyDataInfo, pCmdSpace);

        pCmdSpace += CmdUtil::BuildPfpSyncMe(pCmdSpace);
        gpuVirtAddr = predicateVirtAddr;
        predType    = PredicateType::Boolean64;
    }

    pCmdSpace += CmdUtil::BuildSetPredication(gpuVirtAddr,
                                              predPolarity,
                                              waitResults,
                                              predType,
                                              accumulateData,
                                              pCmdSpace);

    // We need to save the result of the predicate into embedded data to use for:
    // - predicating indirect command generation in DX12,
    // - predicating compute workload discard when doing gang submit in Vulkan.
    if (gpuVirtAddr != 0)
    {
        const uint32 predCopyData = 1;
        uint32*      pPredCpuAddr = CmdAllocateEmbeddedData(1, 1, &m_predGpuAddr);
        (*pPredCpuAddr) = 0;

        WriteDataInfo writeData = {};
        writeData.engineType = EngineTypeUniversal;
        writeData.dstAddr    = m_predGpuAddr;
        writeData.engineSel  = engine_sel__pfp_write_data__prefetch_parser;
        writeData.dstSel     = dst_sel__pfp_write_data__memory;
        writeData.predicate  = PacketPredicate();

        pCmdSpace += CmdUtil::BuildWriteData(writeData, predCopyData, pCmdSpace);
    }
    else
    {
        m_predGpuAddr = 0;
    }

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdPrimeGpuCaches(
    uint32                    rangeCount,
    const PrimeGpuCacheRange* pRanges)
{
    PAL_ASSERT((rangeCount == 0) || (pRanges != nullptr));

    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    for (uint32 i = 0; i < rangeCount; ++i)
    {
        pCmdSpace += CmdUtil::BuildPrimeGpuCaches(pRanges[i],
                                                  m_deviceConfig.prefetchClampSize,
                                                  EngineTypeUniversal,
                                                  pCmdSpace);
    }

    m_deCmdStream.CommitCommands(pCmdSpace);
}

// =====================================================================================================================
template <bool IssueSqtt, bool DescribeDrawDispatch>
void UniversalCmdBuffer::SwitchDrawFunctionsInternal(
    bool viewInstancingEnable,
    bool hasTaskShader)
{
    if (viewInstancingEnable)
    {
        m_funcTable.pfnCmdDraw                      = CmdDraw<IssueSqtt, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawOpaque                = CmdDrawOpaque<IssueSqtt, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndirectMulti         = CmdDrawIndirectMulti<IssueSqtt, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndexed               = CmdDrawIndexed<IssueSqtt, true, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti  = CmdDrawIndexedIndirectMulti<IssueSqtt,
                                                                                  true,
                                                                                  DescribeDrawDispatch>;

        if (hasTaskShader)
        {
            m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMeshTask<IssueSqtt, true, DescribeDrawDispatch>;
            m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMultiTask<IssueSqtt,
                                                                                           true,
                                                                                           DescribeDrawDispatch>;
        }
        else
        {
            m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMesh<IssueSqtt, true, DescribeDrawDispatch>;
            m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMulti<IssueSqtt,
                                                                                       true,
                                                                                       DescribeDrawDispatch>;
        }
    }
    else
    {
        m_funcTable.pfnCmdDraw                      = CmdDraw<IssueSqtt, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawOpaque                = CmdDrawOpaque<IssueSqtt, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndirectMulti         = CmdDrawIndirectMulti<IssueSqtt, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndexed               = CmdDrawIndexed<IssueSqtt, false, DescribeDrawDispatch>;
        m_funcTable.pfnCmdDrawIndexedIndirectMulti  = CmdDrawIndexedIndirectMulti<IssueSqtt,
                                                                                  false,
                                                                                  DescribeDrawDispatch>;

        if (hasTaskShader)
        {
            m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMeshTask<IssueSqtt, false, DescribeDrawDispatch>;
            m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMultiTask<IssueSqtt,
                                                                                           false,
                                                                                           DescribeDrawDispatch>;
        }
        else
        {
            m_funcTable.pfnCmdDispatchMesh              = CmdDispatchMesh<IssueSqtt, false, DescribeDrawDispatch>;
            m_funcTable.pfnCmdDispatchMeshIndirectMulti = CmdDispatchMeshIndirectMulti<IssueSqtt,
                                                                                       false,
                                                                                       DescribeDrawDispatch>;
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::SwitchDrawFunctions(
    bool viewInstancingEnable,
    bool hasTaskShader)
{
    if (m_deviceConfig.issueSqttMarkerEvent)
    {
        SwitchDrawFunctionsInternal<true, true>(viewInstancingEnable, hasTaskShader);
    }
    else if (m_deviceConfig.describeDrawDispatch)
    {
        SwitchDrawFunctionsInternal<false, true>(viewInstancingEnable, hasTaskShader);
    }
    else
    {
        SwitchDrawFunctionsInternal<false, false>(viewInstancingEnable, hasTaskShader);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::SetDispatchFunctions(
    bool hsaAbi)
{
    if (hsaAbi)
    {
        if (m_deviceConfig.issueSqttMarkerEvent)
        {
            PAL_ASSERT(m_deviceConfig.describeDrawDispatch == 1);

            m_funcTable.pfnCmdDispatch         = CmdDispatch<true, true, true>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<true, true>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<true,true, true>;
        }
        else if (m_deviceConfig.describeDrawDispatch)
        {
            m_funcTable.pfnCmdDispatch         = CmdDispatch<true, false, true>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<false, true>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<true, false, true>;
        }
        else
        {
            m_funcTable.pfnCmdDispatch         = CmdDispatch<true, false, false>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<false, false>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<true, false, false>;
        }
    }
    else
    {
        if (m_deviceConfig.issueSqttMarkerEvent)
        {
            PAL_ASSERT(m_deviceConfig.describeDrawDispatch == 1);

            m_funcTable.pfnCmdDispatch         = CmdDispatch<false, true, true>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<true, true>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<false,true, true>;
        }
        else if (m_deviceConfig.describeDrawDispatch)
        {
            m_funcTable.pfnCmdDispatch         = CmdDispatch<false, false, true>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<false, true>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<false, false, true>;
        }
        else
        {
            m_funcTable.pfnCmdDispatch         = CmdDispatch<false, false, false>;
            m_funcTable.pfnCmdDispatchIndirect = CmdDispatchIndirect<false, false>;
            m_funcTable.pfnCmdDispatchOffset   = CmdDispatchOffset<false, false, false>;
        }
    }
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::BuildWriteViewId(
    uint32  viewId,
    uint32* pCmdSpace
    ) const
{
    constexpr uint32  NumRegs       = 3;
    uint32            regsToWrite   = 0;
    RegisterValuePair regs[NumRegs] = {};

    for (uint32 viewIdsReg = m_gfxState.viewIdsReg.u32All; viewIdsReg != 0; viewIdsReg >>= 10)
    {
        const uint16 viewIdRegAddr = viewIdsReg & 0x3FF;
        if (viewIdRegAddr != UserDataNotMapped)
        {
            regs[regsToWrite].offset = viewIdRegAddr;
            regs[regsToWrite].value  = viewId;
            regsToWrite++;
        }
    }

    if (regsToWrite > 0)
    {
        PAL_ASSERT(regsToWrite <= NumRegs);
        pCmdSpace += CmdUtil::BuildSetShPairs<ShaderGraphics>(regs, regsToWrite, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Updates the SQTT token mask for all SEs outside of a specific PerfExperiment. Used by GPA Session when targeting
// a single event for instruction level trace during command buffer building.
void UniversalCmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    PerfExperiment::UpdateSqttTokenMaskStatic(&m_deCmdStream, sqttTokenConfig);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    constexpr uint32 MarkerRegisters[] = { mmSQ_THREAD_TRACE_USERDATA_2,
                                           mmSQ_THREAD_TRACE_USERDATA_3,
                                           mmRLC_SPM_GLOBAL_USER_DATA_0,
                                           mmRLC_SPM_GLOBAL_USER_DATA_1,
                                           mmRLC_SPM_GLOBAL_USER_DATA_2,
                                           mmRLC_SPM_GLOBAL_USER_DATA_3 };
    static_assert(ArrayLen(MarkerRegisters) == uint32(PerfTraceMarkerType::Count),
                  "Array does not match expected length!");

    m_deCmdStream.AllocateAndBuildSetOneUConfigReg<true>(MarkerRegisters[uint32(markerType)], markerData);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdInsertRgpTraceMarker(
    RgpMarkerSubQueueFlags subQueueFlags,
    uint32                 numDwords,
    const void*            pData)
{
    PAL_ASSERT(subQueueFlags.u32All != 0);

    // The first dword of every RGP trace marker packet is written to SQ_THREAD_TRACE_USERDATA_2.  The second dword
    // is written to SQ_THREAD_TRACE_USERDATA_3.  For packets longer than 64-bits, continue alternating between
    // user data 2 and 3.
    static_assert(mmSQ_THREAD_TRACE_USERDATA_3 == mmSQ_THREAD_TRACE_USERDATA_2 + 1, "Registers not sequential!");

    const uint32* pDwordData = static_cast<const uint32*>(pData);
    while (numDwords > 0)
    {
        const uint32 dwordsToWrite = Min(numDwords, 2u);

        constexpr uint16 Start = mmSQ_THREAD_TRACE_USERDATA_2;
        const uint16     end   = (Start + dwordsToWrite - 1);

        // Reserve and commit command space inside this loop.  Some of the RGP packets are unbounded, like adding a
        // comment string, so it's not safe to assume the whole packet will fit under our reserve limit.
        if (subQueueFlags.includeMainSubQueue != 0)
        {
            m_deCmdStream.AllocateAndBuildSetSeqUConfigRegs<true>(Start, end, pDwordData);
        }

        if (subQueueFlags.includeGangedSubQueues != 0)
        {
            PAL_ASSERT(m_pAceCmdStream != nullptr);
            static_cast<CmdStream*>(m_pAceCmdStream)->AllocateAndBuildSetSeqUConfigRegs<true>(Start, end, pDwordData);
        }

        pDwordData += dwordsToWrite;
        numDwords  -= dwordsToWrite;
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                             mem_space__me_wait_reg_mem__register_space,
                             CmdUtil::WaitRegMemFunc(compareFunc),
                             engine_sel__me_wait_reg_mem__micro_engine,
                             registerOffset,
                             data,
                             mask,
                             m_deCmdStream.AllocateCommands(CmdUtil::WaitRegMemSizeDwords));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitMemoryValue(
    gpusize     gpuVirtAddr,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                             mem_space__me_wait_reg_mem__memory_space,
                             CmdUtil::WaitRegMemFunc(compareFunc),
                             engine_sel__me_wait_reg_mem__micro_engine,
                             gpuVirtAddr,
                             data,
                             mask,
                             m_deCmdStream.AllocateCommands(CmdUtil::WaitRegMemSizeDwords));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    const GpuMemory* pGpuMemory = static_cast<const GpuMemory*>(&gpuMemory);
    CmdWaitMemoryValue(pGpuMemory->GetBusAddrMarkerVa(), data, mask, compareFunc);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    const GpuMemory*    pGpuMemory = static_cast<const GpuMemory*>(&dstGpuMemory);
    const WriteDataInfo writeData  =
    {
        .engineType = GetEngineType(),
        .dstAddr    = pGpuMemory->GetBusAddrMarkerVa() + offset,
        .engineSel  = engine_sel__me_write_data__micro_engine,
        .dstSel     = dst_sel__me_write_data__memory
    };
    CmdUtil::BuildWriteData(writeData, value, m_deCmdStream.AllocateCommands(CmdUtil::WriteDataSizeDwords(1)));
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_deCmdStream.If(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdElse()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_deCmdStream.Else();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndIf()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_deCmdStream.EndIf();
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_deCmdStream.While(compareFunc, gpuMemory.Desc().gpuVirtAddr + offset, data, mask);
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdEndWhile()
{
    // Nested command buffers don't support control flow yet.
    PAL_ASSERT(IsNested() == false);

    m_deCmdStream.EndWhile();
}

// =====================================================================================================================
// When RB+ is enabled, pipelines are created per shader export format.  However, same export format possibly supports
// several down convert formats. For example, FP16_ABGR supports 8_8_8_8, 5_6_5, 1_5_5_5, 4_4_4_4, etc.  This updates
// the current RB+ PM4 image with the overridden values.
// Additionally, it is sometimes useful to redirect MRT0 to MRT1-7 for certain kinds of clears
// NOTE: This is expected to be called immediately after RPM binds a graphics pipeline! The pipeline must export to MRT0
void UniversalCmdBuffer::CmdOverwriteColorExportInfoForBlits(
    SwizzledFormat format,
    uint32         targetIndex)
{
    uint32* pCmdSpace = m_deCmdStream.ReserveCommands();

    const auto*const pPipeline = static_cast<const GraphicsPipeline*>(m_graphicsState.pipelineState.pPipeline);
    PAL_ASSERT(pPipeline != nullptr);

    // Always force update the pipeline state with no redundant check here.
    pCmdSpace = pPipeline->UpdateMrtSlotAndRbPlusFormatState(format,
                                                             targetIndex,
                                                             &m_gfxState.cbTargetMask,
                                                             pCmdSpace);

    m_deCmdStream.CommitCommands(pCmdSpace);

    m_gfxState.validBits.pipelineCtxHighHash = false;
}

// =====================================================================================================================
static bool CheckImageInTargetMetadata(
    const TargetsMetadata& metadata,
    const IImage*          pImage)
{
    bool found = false;

    if (pImage != nullptr)
    {
        for (uint32 x = 0; x < metadata.numMrtsBound; x++)
        {
            if (metadata.pImage[x] == pImage)
            {
                found = true;
                break;
            }
        }
    }

    return found;
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    GfxCmdBuffer::CmdBarrier(barrierInfo);

    if (ImplicitGangedSubQueueCount() >= 1)
    {
        IssueGangedBarrierAceWaitDeIncr();
    }

    if ((IsNested() == false) &&
        TestAnyFlagSet(m_deviceConfig.dynCbTemporalHints, Gfx12DynamicCbTemporalHintsReadAfterWrite))
    {
        for (uint32 x = 0; (x < barrierInfo.transitionCount) &&
                           ((m_currentTargetsMetadata.patchedAlready  == false) ||
                            (m_previousTargetsMetadata.patchedAlready == false)); x++)
        {
            if (TestAnyFlagSet(barrierInfo.pTransitions[x].imageInfo.oldLayout.usages, LayoutColorTarget) &&
                TestAnyFlagSet(barrierInfo.pTransitions[x].imageInfo.newLayout.usages,
                               (LayoutShaderRead | LayoutShaderWrite)))
            {
                if ((m_currentTargetsMetadata.patchedAlready == false) &&
                    CheckImageInTargetMetadata(m_currentTargetsMetadata, barrierInfo.pTransitions[x].imageInfo.pImage))
                {
                    PatchPassCbTemporalHints(&m_currentTargetsMetadata,
                                             m_deviceConfig.gfx12TemporalHintsMrtReadRaw,
                                             m_deviceConfig.gfx12TemporalHintsMrtWriteRaw);
                }
                if ((m_previousTargetsMetadata.patchedAlready == false) &&
                    CheckImageInTargetMetadata(m_previousTargetsMetadata, barrierInfo.pTransitions[x].imageInfo.pImage))
                {
                    PatchPassCbTemporalHints(&m_previousTargetsMetadata,
                                             m_deviceConfig.gfx12TemporalHintsMrtReadRaw,
                                             m_deviceConfig.gfx12TemporalHintsMrtWriteRaw);
                }
            }
        }
    }
}

// =====================================================================================================================
static bool ImgBarrierIsColorTargetToShaderReadOrWrite(
    const ImgBarrier& imgBarrier)
{
    return // Check layout
           ((imgBarrier.oldLayout.usages & LayoutColorTarget) &&
            (imgBarrier.newLayout.usages & (LayoutShaderRead | LayoutShaderWrite)));
}

// =====================================================================================================================
static void CheckAcquireReleaseInfoForCbTemporalHintPatch(
    const AcquireReleaseInfo& acquireInfo,
    TargetsMetadata*          pCurrentTargetMetadata,
    TargetsMetadata*          pPreviousTargetMetadata,
    Gfx12TemporalHintsRead    readHint,
    Gfx12TemporalHintsWrite   writeHint)
{
    for (uint32 x = 0; (x < acquireInfo.imageBarrierCount) &&
                       ((pCurrentTargetMetadata->patchedAlready == false) ||
                        (pPreviousTargetMetadata->patchedAlready == false)); x++)
    {
        if (ImgBarrierIsColorTargetToShaderReadOrWrite(acquireInfo.pImageBarriers[x]))
        {
            if ((pCurrentTargetMetadata->patchedAlready == false) &&
                CheckImageInTargetMetadata(*pCurrentTargetMetadata, acquireInfo.pImageBarriers[x].pImage))
            {
                PatchPassCbTemporalHints(pCurrentTargetMetadata, readHint, writeHint);
            }
            if ((pPreviousTargetMetadata->patchedAlready == false) &&
                CheckImageInTargetMetadata(*pPreviousTargetMetadata, acquireInfo.pImageBarriers[x].pImage))
            {
                PatchPassCbTemporalHints(pPreviousTargetMetadata, readHint, writeHint);
            }
        }
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    syncTokenCount,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 885
    const uint32*             pSyncTokens)
#else
    const ReleaseToken*       pSyncTokens)
#endif
{
    GfxCmdBuffer::CmdAcquire(acquireInfo, syncTokenCount, pSyncTokens);

    if (ImplicitGangedSubQueueCount() >= 1)
    {
        IssueGangedBarrierAceWaitDeIncr();
    }

    if ((IsNested() == false) &&
        TestAnyFlagSet(m_deviceConfig.dynCbTemporalHints, Gfx12DynamicCbTemporalHintsReadAfterWrite))
    {
        CheckAcquireReleaseInfoForCbTemporalHintPatch(acquireInfo,
                                                      &m_currentTargetsMetadata,
                                                      &m_previousTargetsMetadata,
                                                      m_deviceConfig.gfx12TemporalHintsMrtReadRaw,
                                                      m_deviceConfig.gfx12TemporalHintsMrtWriteRaw);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdAcquireEvent(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent* const*   ppGpuEvents)
{
    GfxCmdBuffer::CmdAcquireEvent(acquireInfo, gpuEventCount, ppGpuEvents);

    if (ImplicitGangedSubQueueCount() >= 1)
    {
        IssueGangedBarrierAceWaitDeIncr();
    }

    if ((IsNested() == false) &&
        TestAnyFlagSet(m_deviceConfig.dynCbTemporalHints, Gfx12DynamicCbTemporalHintsReadAfterWrite))
    {
        CheckAcquireReleaseInfoForCbTemporalHintPatch(acquireInfo,
                                                      &m_currentTargetsMetadata,
                                                      &m_previousTargetsMetadata,
                                                      m_deviceConfig.gfx12TemporalHintsMrtReadRaw,
                                                      m_deviceConfig.gfx12TemporalHintsMrtWriteRaw);
    }
}

// =====================================================================================================================
void UniversalCmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    GfxCmdBuffer::CmdReleaseThenAcquire(barrierInfo);

    if (ImplicitGangedSubQueueCount() >= 1)
    {
        IssueGangedBarrierAceWaitDeIncr();
    }

    if ((IsNested() == false) &&
        TestAnyFlagSet(m_deviceConfig.dynCbTemporalHints, Gfx12DynamicCbTemporalHintsReadAfterWrite))
    {
        CheckAcquireReleaseInfoForCbTemporalHintPatch(barrierInfo,
                                                      &m_currentTargetsMetadata,
                                                      &m_previousTargetsMetadata,
                                                      m_deviceConfig.gfx12TemporalHintsMrtReadRaw,
                                                      m_deviceConfig.gfx12TemporalHintsMrtWriteRaw);
    }
}

// =====================================================================================================================
// Returns the parent GfxCmdStream's ACE CmdStream as a Gfx9::CmdStream. Creates and initializes the ACE CmdStream if
// it is the first time this is called.
void UniversalCmdBuffer::InitAceCmdStream()
{
    PAL_ASSERT((m_pAceCmdStream == nullptr) == (m_pComputeStateAce == nullptr));

    if (m_pAceCmdStream == nullptr)
    {
        m_pComputeStateAce = PAL_NEW(ComputeState, m_device.GetPlatform(), AllocInternal);

        // This is the first time the ACE CmdStream is being used. So create and initialize the ACE CmdStream
        // and the associated GpuEvent object additionally.
        m_pAceCmdStream = PAL_NEW(CmdStream, m_device.GetPlatform(), AllocInternal)(
            static_cast<const Device&>(m_device),
            m_pCmdAllocator,
            EngineTypeCompute,
            SubEngineType::AsyncCompute,
            CmdStreamUsage::Workload,
            IsNested());

        Result result = Result::Success;

        if ((m_pAceCmdStream != nullptr) && (m_pComputeStateAce != nullptr))
        {
            memset(m_pComputeStateAce, 0, sizeof(ComputeState));
            result = m_pAceCmdStream->Init();
        }
        else
        {
            NotifyAllocFailure();
            result = Result::ErrorOutOfMemory;
        }

        if (result == Result::Success)
        {
            const PalSettings& coreSettings = m_device.Parent()->Settings();

            CmdStreamBeginFlags cmdStreamFlags = {};
            cmdStreamFlags.prefetchCommands    = m_buildFlags.prefetchCommands;
            cmdStreamFlags.optimizeCommands    = m_buildFlags.optimizeGpuSmallBatch;

            result = m_pAceCmdStream->Begin(cmdStreamFlags, m_pMemAllocator);
        }

        if (result == Result::Success)
        {
            ComputeCmdBufferDeviceConfig aceDeviceConfig   = {};
            aceDeviceConfig.disableBorderColorPaletteBinds = m_deviceConfig.disableBorderColorPaletteBinds;
            aceDeviceConfig.enablePreamblePipelineStats    = m_deviceConfig.enablePreamblePipelineStats;
            aceDeviceConfig.issueSqttMarkerEvent           = m_deviceConfig.issueSqttMarkerEvent;
            aceDeviceConfig.prefetchClampSize              = m_deviceConfig.prefetchClampSize;
            result = ComputeCmdBuffer::WritePreambleCommands(aceDeviceConfig, static_cast<CmdStream*>(m_pAceCmdStream));
        }

        // Creation of the Ace CmdStream failed.
        PAL_ASSERT(result == Result::Success);

        if (result != Result::Success)
        {
            SetCmdRecordingError(result);
        }
    }
}

// =====================================================================================================================
// Allocates memory for the command stream sync semaphore if not already allocated.
void UniversalCmdBuffer::AllocGangedCmdStreamSemaphore()
{
    PAL_ASSERT(m_gangSubmitState.cmdStreamSemAddr == 0);

    // Dword alignment is enough since the address is only used in WriteData/ReleaseMem/WaitRegMem packets.
    uint32* pData = CmdAllocateEmbeddedData(2, 1, &m_gangSubmitState.cmdStreamSemAddr);
    PAL_ASSERT(m_gangSubmitState.cmdStreamSemAddr != 0);

    // We need to memset this to handle a possible race condition with stale data.
    // If the memory contains any value, it is possible that, with the ACE running ahead, it could get a value
    // for this semaphore which is >= the number it is waiting for and then just continue ahead before GFX has
    // a chance to write it to 0.
    // To fix this, we use EmbeddedData and memset it on the CPU.
    // To handle the case where we reuse a command buffer entirely, we'll have to perform a GPU-side write of this
    // memory in the postamble.
    pData[0] = 0;
    pData[1] = 0;
}

// =====================================================================================================================
void UniversalCmdBuffer::IssueGangedBarrierDeWaitAceIncr()
{
    if (m_pAceCmdStream != nullptr)
    {
        PAL_ASSERT(m_gangSubmitState.cmdStreamSemAddr != 0);

        m_gangSubmitState.semCountDeWaitAce++;

        const ReleaseMemGeneric releaseInfo =
        {
            .vgtEvent = BOTTOM_OF_PIPE_TS,
            .dataSel  = data_sel__mec_release_mem__send_32_bit_low,
            .data     = m_gangSubmitState.semCountDeWaitAce,
            .dstAddr  = m_gangSubmitState.cmdStreamSemAddr + sizeof(uint32),
        };
        m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, m_pAceCmdStream->AllocateCommands(CmdUtil::ReleaseMemSizeDwords));
    }
}

// =====================================================================================================================
// For ganged-submit with ACE+GFX, we need to ensure that any stalls that occur on the GFX engine are properly stalled
// on the ACE engine and vice versa. To that end, when we detect when ganged-submit is active, we issue a
// bottom-of-pipe timestamp event which will write the current barrier count. Later, when the ACE engine is used, we'll
// issue a WAIT_REG_MEM to ensure that all prior events on the GFX engine have completed.
void UniversalCmdBuffer::IssueGangedBarrierAceWaitDeIncr()
{
    PAL_ASSERT(m_gangSubmitState.cmdStreamSemAddr != 0);

    m_gangSubmitState.semCountAceWaitDe++;

    const ReleaseMemGeneric releaseInfo =
    {
        .vgtEvent = BOTTOM_OF_PIPE_TS,
        .dataSel  = data_sel__me_release_mem__send_32_bit_low,
        .data     = m_gangSubmitState.semCountAceWaitDe,
        .dstAddr  = m_gangSubmitState.cmdStreamSemAddr
    };
    m_cmdUtil.BuildReleaseMemGeneric(releaseInfo, m_deCmdStream.AllocateCommands(CmdUtil::ReleaseMemSizeDwords));
}

// =====================================================================================================================
void UniversalCmdBuffer::TryInitAceGangedSubmitResources()
{
    if (ImplicitGangedSubQueueCount() < 1)
    {
        if (m_pAceCmdStream == nullptr)
        {
            InitAceCmdStream();
        }

        if (Util::IsErrorResult(m_status) == false)
        {
            EnableImplicitGangedSubQueueCount(1);
            AllocGangedCmdStreamSemaphore();

            // We need to properly issue a stall in case we're requesting the ACE CmdStream after a barrier call.
            IssueGangedBarrierAceWaitDeIncr();

            // We must always issue an AceWaitDe to synchronize for mesh/task related query
            uint32* pAceCmdSpace = m_pAceCmdStream->ReserveCommands();
            pAceCmdSpace = CmdAceWaitDe(pAceCmdSpace);

            if (m_deferredPipelineStatsQueries.IsEmpty() == false)
            {
                // Apply the deferred Begin() operation on any pipeline-stats queries we've accumulated before the
                // ganged ACE stream was initialized.
                for (const auto& state: m_deferredPipelineStatsQueries)
                {
                    PAL_ASSERT(state.pQueryPool != nullptr);
                    pAceCmdSpace = state.pQueryPool->DeferredBeginOnGangedAce(this, pAceCmdSpace, state.slot);
                }
                m_deferredPipelineStatsQueries.Clear();
            }

            m_pAceCmdStream->CommitCommands(pAceCmdSpace);

            // The above DE-side semaphore increment/wait is pipelined ACE work. Consequently increment the ACE-side
            // semaphore count so that a future DE postamble will correctly wait on it before resetting the fence.
            IssueGangedBarrierDeWaitAceIncr();
        }
    }
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::CmdAceWaitDe(
    uint32* pCmdSpace)
{
    PAL_ASSERT((m_pAceCmdStream != nullptr) && (m_gangSubmitState.cmdStreamSemAddr != 0));

    // We need to make sure that the ACE CmdStream properly waits for any barriers that may have occured
    // on the DE CmdStream. We've been incrementing a counter on the DE CmdStream, so all we need to do
    // on the ACE side is perform the wait.
    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeCompute,
                                          mem_space__mec_wait_reg_mem__memory_space,
                                          function__mec_wait_reg_mem__greater_than_or_equal_reference_value,
                                          0, // EngineSel enum does not exist in the MEC WAIT_REG_MEM packet.
                                          m_gangSubmitState.cmdStreamSemAddr,
                                          m_gangSubmitState.semCountAceWaitDe,
                                          0xFFFFFFFF,
                                          pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
uint32* UniversalCmdBuffer::CmdDeWaitAce(
    uint32* pCmdSpace)
{
    PAL_ASSERT((m_pAceCmdStream != nullptr) && (m_gangSubmitState.cmdStreamSemAddr != 0));

    // We need to make sure that the DE CmdStream properly waits for any barriers that may have occured
    // on the ACE CmdStream. We've been incrementing a counter on the ACE CmdStream, so all we need to do
    // on the DE side is perform the wait.
    pCmdSpace += CmdUtil::BuildWaitRegMem(EngineTypeUniversal,
                                          mem_space__pfp_wait_reg_mem__memory_space,
                                          function__pfp_wait_reg_mem__greater_than_or_equal_reference_value,
                                          engine_sel__pfp_wait_reg_mem__prefetch_parser,
                                          m_gangSubmitState.cmdStreamSemAddr + sizeof(uint32),
                                          m_gangSubmitState.semCountDeWaitAce,
                                          0xFFFFFFFF,
                                          pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
void UniversalCmdBuffer::SetShaderRingSize(
    const ShaderRingItemSizes& ringSizes)
{
    for (uint32 ring = 0; ring < static_cast<uint32>(ShaderRingType::NumUniversal); ++ring)
    {
        if (ringSizes.itemSize[ring] > m_ringSizes.itemSize[ring])
        {
            m_ringSizes.itemSize[ring] = ringSizes.itemSize[ring];
        }
    }
}

// =====================================================================================================================
bool UniversalCmdBuffer::UpdateNggPrimCb(
    const GraphicsPipeline*         pCurrentPipeline,
    Util::Abi::PrimShaderCullingCb* pPrimShaderCb
    ) const
{
    bool dirty = false;

    if ((pPrimShaderCb->paClVteCntl != pCurrentPipeline->PaClVteCntl().u32All) ||
        (pPrimShaderCb->paSuVtxCntl != pCurrentPipeline->PaSuVtxCntl().u32All))
    {
        dirty                      = true;
        pPrimShaderCb->paClVteCntl = pCurrentPipeline->PaClVteCntl().u32All;
        pPrimShaderCb->paSuVtxCntl = pCurrentPipeline->PaSuVtxCntl().u32All;
    }

    return dirty;
}

// =====================================================================================================================
static void UpdateMsaaForNggCullingCb(
    uint32                                     viewportCount,
    float                                      multiplier,
    const Abi::PrimShaderCullingCb::Viewports* pInputVportCb,
    Abi::PrimShaderCullingCb::Viewports*       pOutputVportCb)
{
    // Helper structure to convert uint32 to a float.
    union Uint32ToFloat
    {
        uint32 uValue;
        float  fValue;
    };

    // For small-primitive filter culling with NGG, the shader needs the viewport scale to premultiply
    // the number of samples into it.
    Uint32ToFloat uintToFloat = {};
    for (uint32 i = 0; i < viewportCount; i++)
    {
        uintToFloat.uValue  = pInputVportCb[i].paClVportXScale;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportXScale = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportXOffset;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportXOffset = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportYScale;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportYScale = uintToFloat.uValue;

        uintToFloat.uValue  = pInputVportCb[i].paClVportYOffset;
        uintToFloat.fValue *= multiplier;
        pOutputVportCb[i].paClVportYOffset = uintToFloat.uValue;
    }
}

// =====================================================================================================================
// This function updates the NGG culling data constant buffer which is needed for NGG culling operations to execute
// correctly.
void UniversalCmdBuffer::UpdateNggCullingDataBufferWithCpu()
{
    PAL_ASSERT(m_gfxState.nggCullingDataReg != UserDataNotMapped);

    constexpr uint32 NggStateDwords = (sizeof(Abi::PrimShaderCullingCb) / sizeof(uint32));
    Abi::PrimShaderCullingCb* pPrimShaderCullingCb = &m_gfxState.primShaderCullingCb;

    // If the clients have specified a default sample layout we can use the number of samples as a multiplier.
    // However, if custom sample positions are in use we need to assume the worst case sample count (16).
    const float multiplier = m_graphicsState.useCustomSamplePattern
                             ? 16.0f : static_cast<float>(m_nggTable.numSamples);

    // Make a local copy of the various shader state so that we can modify it as necessary.
    Abi::PrimShaderCullingCb localCb;
    if (multiplier > 1.0f)
    {
        memcpy(&localCb, &m_gfxState.primShaderCullingCb, NggStateDwords * sizeof(uint32));
        pPrimShaderCullingCb = &localCb;

        UpdateMsaaForNggCullingCb(m_graphicsState.viewportState.count,
                                  multiplier,
                                  &m_gfxState.primShaderCullingCb.viewports[0],
                                  &localCb.viewports[0]);
    }

    // Copy all of NGG state into embedded data, which is pointed to by nggTable.gpuVirtAddr
    UpdateUserDataTableCpu(&m_nggTable.state,
                           NggStateDwords, // size
                           0,              // offset
                           reinterpret_cast<const uint32*>(pPrimShaderCullingCb));

    const uint32 regOffset = m_gfxState.nggCullingDataReg + PERSISTENT_SPACE_START;
    m_deCmdStream.AllocateAndBuildSetSeqShRegs<ShaderGraphics>(regOffset, regOffset + 1, &m_nggTable.state.gpuVirtAddr);
}

// =====================================================================================================================
// Returns the parent GfxCmdStream's ACE CmdStream as a Gfx9::CmdStream. Creates and initializes the ACE CmdStream if
// it is the first time this is called.
CmdStream* UniversalCmdBuffer::GetAceCmdStream()
{
    if ((m_pAceCmdStream == nullptr) || (m_pAceCmdStream->IsEmpty() == true))
    {
        TryInitAceGangedSubmitResources();
    }  // If ACE command stream is yet to be created, or a previous use of this command buffer
       // reset the ACE stream, we need to re-initialize associated resources.

    return static_cast<CmdStream*>(m_pAceCmdStream);
}

}
}
