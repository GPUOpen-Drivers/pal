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

#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12ComputeShaderLibrary.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12PipelineChunkCs.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "core/hw/gfxip/pipeline.h"

#include "core/imported/hsa/AMDHSAKernelDescriptor.h"
#include "core/imported/hsa/amd_hsa_kernel_code.h"
#include "palHsaAbiMetadata.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SH field of the COMPUTE_RESOURCE_LIMITS register.
static uint32 CalcMaxWavesPerSh(
    const GpuChipProperties& chipProps,
    float                    maxWavesPerCu)
{
    // The maximum number of waves per SE in "register units".
    // By default set the WaveLimit field to be unlimited.
    uint32 wavesPerSh = 0;

    if (maxWavesPerCu > 0)
    {
        // First calculate the maximum number of waves per SH/SA.
        const uint32 maxWavesPerShCompute = chipProps.gfx9.numSimdPerCu    *
                                            chipProps.gfx9.numWavesPerSimd *
                                            chipProps.gfx9.maxNumCuPerSh;

        // We assume no one is trying to use more than 100% of all waves.
        PAL_ASSERT(maxWavesPerCu <= (maxWavesPerShCompute / chipProps.gfx9.maxNumCuPerSh));

        const uint32 maxWavesPerSh = static_cast<uint32>(round(maxWavesPerCu * chipProps.gfx9.numCuPerSh));

        wavesPerSh = Min(maxWavesPerShCompute, maxWavesPerSh);
    }

    PAL_ASSERT(wavesPerSh <= 1023);

    return wavesPerSh;
}

// =====================================================================================================================
// Helper function to compute the WAVES_PER_SE field of the COMPUTE_RESOURCE_LIMITS register.
static uint32 CalcMaxWavesPerSe(
    const GpuChipProperties& chipProps,
    float                    maxWavesPerCu)
{
    // The maximum number of waves per SE in "register units".
    // By default set the WAVE_LIMIT field to be unlimited
    uint32 wavesPerSe = 0;

    if (maxWavesPerCu > 0)
    {
        wavesPerSe = CalcMaxWavesPerSh(chipProps, maxWavesPerCu) * chipProps.gfx9.numShaderArrays;
    }

    return wavesPerSe;
}

// =====================================================================================================================
static uint32 DivideAndRoundUpPow2(
    uint32 dividend,
    uint32 divisor)
{
    dividend = RoundUpToMultiple(dividend, divisor);

    return Pow2Pad(dividend / divisor);
}

// =====================================================================================================================
static COMPUTE_DISPATCH_INTERLEAVE GetComputeDispatchInterleave(
    DispatchInterleaveSize interleaveSize,
    const DispatchDims&    threadsPerTg,
#if PAL_BUILD_NAVI48
    bool                   isNavi48,
#endif
    bool*                  pIs2dDispatchInterleave)
{
    PAL_ASSERT(pIs2dDispatchInterleave != nullptr);

    COMPUTE_DISPATCH_INTERLEAVE computeDispatchInterleave;
    bool                        is2D = true;

    computeDispatchInterleave.u32All = mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT;

    switch (interleaveSize)
    {
    case DispatchInterleaveSize::Default:
        {
            uint32 seInterleaveWidth  = 1;
            uint32 seInterleaveHeight = 1;

            // 1D threadgroups
            if ((threadsPerTg.y == 1) && (threadsPerTg.z == 1))
            {
                seInterleaveWidth = Max(DivideAndRoundUpPow2(256u, threadsPerTg.x), 1u);
                seInterleaveWidth = Min(seInterleaveWidth, 16u);
            }
            else
            {
                // 2D threadgroups
                if (threadsPerTg.z == 1)
                {
                    uint32 dividend = 16u;

#if PAL_BUILD_NAVI48
                    if (isNavi48)
                    {
                        dividend = 32u;
                    }
#endif

                    seInterleaveWidth  = Max(DivideAndRoundUpPow2(dividend, threadsPerTg.x), 1u);
                    seInterleaveHeight = Max(DivideAndRoundUpPow2(dividend, threadsPerTg.y), 1u);
                }
                // 3D threadgroups
                else
                {
                    seInterleaveWidth  = Max(DivideAndRoundUpPow2(32u, (threadsPerTg.x * threadsPerTg.z)), 1u);
                    seInterleaveHeight = Max(DivideAndRoundUpPow2(32u, (threadsPerTg.y * threadsPerTg.z)), 1u);
                }

                if ((seInterleaveWidth * seInterleaveHeight) > 16)
                {
                    seInterleaveWidth  = 4;
                    seInterleaveHeight = 4;
                }
            }

            PAL_ASSERT(IsPowerOfTwo(seInterleaveWidth)  &&
                       IsPowerOfTwo(seInterleaveHeight) &&
                       ((seInterleaveWidth * seInterleaveHeight) != 0) &&
                       ((seInterleaveWidth * seInterleaveHeight) <= 16));

            computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = Log2(seInterleaveWidth);
            computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = Log2(seInterleaveHeight);
        }
        break;
    case DispatchInterleaveSize::Disable:
        computeDispatchInterleave.bits.INTERLEAVE_1D = 1;
        is2D = false;
        break;
    case DispatchInterleaveSize::_1D_64_Threads:
        computeDispatchInterleave.bits.INTERLEAVE_1D = 64;
        is2D = false;
        break;
    case DispatchInterleaveSize::_1D_128_Threads:
        computeDispatchInterleave.bits.INTERLEAVE_1D = 128;
        is2D = false;
        break;
    case DispatchInterleaveSize::_1D_256_Threads:
        computeDispatchInterleave.bits.INTERLEAVE_1D = 256;
        is2D = false;
        break;
    case DispatchInterleaveSize::_1D_512_Threads:
        computeDispatchInterleave.bits.INTERLEAVE_1D = 512;
        is2D = false;
        break;

    // INTERLEAVE_2D_X/Y_SIZE encoding - ([0-4] => [1,2,4,8,16])
    case DispatchInterleaveSize::_2D_1x1_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 0;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 0;
        break;
    case DispatchInterleaveSize::_2D_1x2_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 0;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 1;
        break;
    case DispatchInterleaveSize::_2D_1x4_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 0;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 2;
        break;
    case DispatchInterleaveSize::_2D_1x8_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 0;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 3;
        break;
    case DispatchInterleaveSize::_2D_1x16_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 0;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 4;
        break;

    case DispatchInterleaveSize::_2D_2x1_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 1;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 0;
        break;
    case DispatchInterleaveSize::_2D_2x2_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 1;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 1;
        break;
    case DispatchInterleaveSize::_2D_2x4_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 1;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 2;
        break;
    case DispatchInterleaveSize::_2D_2x8_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 1;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 3;
        break;

    case DispatchInterleaveSize::_2D_4x1_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 2;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 0;
        break;
    case DispatchInterleaveSize::_2D_4x2_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 2;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 1;
        break;
    case DispatchInterleaveSize::_2D_4x4_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 2;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 2;
        break;

    case DispatchInterleaveSize::_2D_8x1_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 3;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 0;
        break;
    case DispatchInterleaveSize::_2D_8x2_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 3;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 1;
        break;

    case DispatchInterleaveSize::_2D_16x1_ThreadGroups:
        computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE = 4;
        computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE = 0;
        break;
    default:
        PAL_ASSERT_ALWAYS();
        is2D = false;
        break;
    }

    if (is2D)
    {
        // 1D must be disabled if using 2D interleave (can't use both 1D and 2D)
        computeDispatchInterleave.bits.INTERLEAVE_1D = 1;
    }

    *pIs2dDispatchInterleave = is2D;

    return computeDispatchInterleave;
}

// =====================================================================================================================
PipelineChunkCs::PipelineChunkCs(
    const Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pUserDataLayout(nullptr),
    m_regs{},
    m_computeDispatchInterleave{mmCOMPUTE_DISPATCH_INTERLEAVE_DEFAULT},
    m_prefetchAddr(0),
    m_prefetchSize(0),
    m_flags{},
    m_dvgprExtraAceScratch(0)
{
    Regs::Init(m_regs);
    Regs::Get<mmCOMPUTE_USER_DATA_1, COMPUTE_USER_DATA_1>(m_regs)->u32All = InvalidUserDataInternalTable;
}

// =====================================================================================================================
PipelineChunkCs::~PipelineChunkCs()
{
    if (m_pUserDataLayout != nullptr)
    {
        m_pUserDataLayout->Destroy();
        m_pUserDataLayout = nullptr;
    }
}

// =====================================================================================================================
void PipelineChunkCs::Clone(
    const PipelineChunkCs& other)
{
    MutexAuto lock(&m_userDataCombineMutex);
    if (m_pUserDataLayout != nullptr)
    {
        m_pUserDataLayout->Destroy();
        m_pUserDataLayout = nullptr;
    }
    other.m_pUserDataLayout->Duplicate(*m_pDevice->Parent(), &m_pUserDataLayout);

    memcpy(m_regs, other.m_regs, sizeof(m_regs));

    m_computeDispatchInterleave = other.m_computeDispatchInterleave;
    m_prefetchAddr              = other.m_prefetchAddr;
    m_prefetchSize              = other.m_prefetchSize;
    m_flags                     = other.m_flags;
}

// =====================================================================================================================
template<typename H>
void PipelineChunkCs::SetComputeShaderState(
    const Pal::Device*                pDevice,
    const PalAbi::CodeObjectMetadata& metadata,
    const ShaderLibStats*             pLibStats,
    const CodeObjectUploader&         uploader,
    bool                              glgEnabled,
    RegisterValuePair*                pRegs,
    bool*                             pIsWave32)
{
    const auto& csMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];
    const Util::PalAbi::ComputeRegisterMetadata& computeRegisters = metadata.pipeline.computeRegister;
    const Device& device = static_cast<const Device&>(*pDevice->GetGfxDevice());

    const bool isWave32 = (csMetadata.wavefrontSize == 32);

    if (pIsWave32 != nullptr)
    {
        *pIsWave32 = isWave32;
    }

    if (pRegs != nullptr)
    {
        GpuSymbol          symbol       = {};
        const DispatchDims threadsPerTg = { csMetadata.threadgroupDimensions[0],
                                            csMetadata.threadgroupDimensions[1],
                                            csMetadata.threadgroupDimensions[2] };

        // For GEN_TWO wave, the sgpr count is always 128, so don't set it.
        uint32 vgprCount = 0;
        uint32 ldsSize   = 0;
        if ((csMetadata.vgprCount == 0) && (pLibStats != nullptr))
        {
            vgprCount = pLibStats->common.numUsedVgprs;
            ldsSize   = static_cast<uint32>(pLibStats->common.ldsUsageSizeInBytes);
        }
        else
        {
            vgprCount = csMetadata.vgprCount;
            ldsSize   = static_cast<uint32>(csMetadata.ldsSize);
        }

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC1))
        {
            auto* pRsrc1 = H::template Get<mmCOMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC1>(pRegs);
            pRsrc1->bits.VGPRS        = CalcNumVgprs(vgprCount, isWave32, computeRegisters.flags.dynamicVgprEn);
            pRsrc1->bits.FLOAT_MODE   = csMetadata.floatMode;
            pRsrc1->bits.WG_RR_EN     = csMetadata.flags.wgRoundRobin;
            pRsrc1->bits.DISABLE_PERF = 0; // TODO: Get this out of the metadata.
            pRsrc1->bits.FP16_OVFL    = csMetadata.flags.fp16Overflow;
            pRsrc1->bits.WGP_MODE     = csMetadata.flags.wgpMode;
            pRsrc1->bits.MEM_ORDERED  = csMetadata.flags.memOrdered;
            pRsrc1->bits.FWD_PROGRESS = csMetadata.flags.forwardProgress;
        }

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC2))
        {
            auto* pRsrc2 = H::template Get<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(pRegs);
            pRsrc2->bits.SCRATCH_EN     = csMetadata.flags.scratchEn;
            pRsrc2->bits.USER_SGPR      = csMetadata.userSgprs;
            pRsrc2->bits.DYNAMIC_VGPR   = computeRegisters.flags.dynamicVgprEn;
            pRsrc2->bits.TGID_X_EN      = computeRegisters.flags.tgidXEn;
            pRsrc2->bits.TGID_Y_EN      = computeRegisters.flags.tgidYEn;;
            pRsrc2->bits.TGID_Z_EN      = computeRegisters.flags.tgidZEn;;
            pRsrc2->bits.TG_SIZE_EN     = computeRegisters.flags.tgSizeEn;
            pRsrc2->bits.TIDIG_COMP_CNT = computeRegisters.tidigCompCnt;
            pRsrc2->bits.LDS_SIZE       = Pow2Align(ldsSize >> 2, LdsDwGranularity) / LdsDwGranularity;
        }

        COMPUTE_PGM_RSRC3* pRsrc3 = nullptr;
        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3))
        {
            pRsrc3 = H::template Get<mmCOMPUTE_PGM_RSRC3, COMPUTE_PGM_RSRC3>(pRegs);
            pRsrc3->bits.SHARED_VGPR_CNT = csMetadata.sharedVgprCnt;
            pRsrc3->bits.GLG_EN          = glgEnabled;

            // PWS+ only support PreShader/PrePs waits if the IMAGE_OP bit is set. Theoretically we only set it for
            // shaders that do an image operation. However that would mean that our use of the pre-shader PWS+ wait
            // is dependent on us only waiting on image resources, which we don't know in our interface. For now always
            // set the IMAGE_OP bit for corresponding shaders, making the PreShader/PrePs waits global.
            pRsrc3->bits.IMAGE_OP        = 1;
        }

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3) || H::Exist(mmCOMPUTE_PGM_LO))
        {
            if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
            {
                PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

                if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3))
                {
                    pRsrc3->bits.INST_PREF_SIZE = device.GetShaderPrefetchSize(symbol.size);
                }

                if constexpr (H::Exist(mmCOMPUTE_PGM_LO))
                {
                    auto* pPgmLo = H::template Get<mmCOMPUTE_PGM_LO, COMPUTE_PGM_LO>(pRegs);
                    pPgmLo->bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
                }
            }
        }

        if constexpr (H::Exist(mmCOMPUTE_SHADER_CHKSUM))
        {
            auto* pChksum = H::template Get<mmCOMPUTE_SHADER_CHKSUM, COMPUTE_SHADER_CHKSUM>(pRegs);
            pChksum->bits.CHECKSUM = csMetadata.checksumValue;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_X))
        {
            auto* pX = H::template Get<mmCOMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_X>(pRegs);
            pX->bits.NUM_THREAD_FULL   = threadsPerTg.x;
            pX->bits.INTERLEAVE_BITS_X = computeRegisters.xInterleave;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_Y))
        {
            auto* pY = H::template Get<mmCOMPUTE_NUM_THREAD_Y, COMPUTE_NUM_THREAD_Y>(pRegs);
            pY->bits.NUM_THREAD_FULL   = threadsPerTg.y;
            pY->bits.INTERLEAVE_BITS_Y = computeRegisters.yInterleave;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_Z))
        {
            auto* pZ = H::template Get<mmCOMPUTE_NUM_THREAD_Z, COMPUTE_NUM_THREAD_Z>(pRegs);
            pZ->bits.NUM_THREAD_FULL = threadsPerTg.z;
        }

        if constexpr (H::Exist(mmCOMPUTE_USER_DATA_1))
        {
            if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
            {
                H::template Get<mmCOMPUTE_USER_DATA_1, COMPUTE_USER_DATA_1>(pRegs)->u32All = symbol.gpuVirtAddr;
            }
        }

        if constexpr (H::Exist(mmCOMPUTE_RESOURCE_LIMITS))
        {
            const uint32 threadsPerGroup = (threadsPerTg.x *
                                            threadsPerTg.y *
                                            threadsPerTg.z);
            const uint32 wavesPerGroup   = RoundUpQuotient(threadsPerGroup, csMetadata.wavefrontSize);

            auto* pRsrcLimits = H::template Get<mmCOMPUTE_RESOURCE_LIMITS, COMPUTE_RESOURCE_LIMITS>(pRegs);
            pRsrcLimits->bits.WAVES_PER_SH = csMetadata.wavesPerSe;
            pRsrcLimits->bits.LOCK_THRESHOLD = 0; // TODO: Get this out of the metadata.

            // SimdDestCntl: Controls which SIMDs thread groups get scheduled on.  If the number of
            // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
            pRsrcLimits->bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;
            pRsrcLimits->bits.CU_GROUP_COUNT = 0; // TODO: Get this out of the metadata.

            // Force even distribution on all SIMDs in CU for workgroup size is 64
            // This has shown some good improvements if #CU per SE not a multiple of 4
            const GpuChipProperties& chipProps = pDevice->ChipProperties();
            if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
            {
                pRsrcLimits->bits.FORCE_SIMD_DIST = 1;
            }
        }
    }
}

template void PipelineChunkCs::SetComputeShaderState<PipelineChunkCs::Regs> (
    const Pal::Device*                pDevice,
    const PalAbi::CodeObjectMetadata& metadata,
    const ShaderLibStats*             pLibStats,
    const CodeObjectUploader&         uploader,
    bool                              glgEnabled,
    RegisterValuePair*                pRegs,
    bool*                             pIsWave32);

template void PipelineChunkCs::SetComputeShaderState<ComputeShaderLibrary::Regs>(
    const Pal::Device*                pDevice,
    const PalAbi::CodeObjectMetadata& metadata,
    const ShaderLibStats*             pLibStats,
    const CodeObjectUploader&         uploader,
    bool                              glgEnabled,
    RegisterValuePair*                pRegs,
    bool*                             pIsWave32);

// =====================================================================================================================
template<typename H>
void PipelineChunkCs::SetComputeShaderState(
    const Pal::Device*                       pDevice,
    const Util::HsaAbi::CodeObjectMetadata&  metadata,
    const llvm::amdhsa::kernel_descriptor_t& desc,
    const uint32                             hash,
    Extent3d                                 groupSize,
    const CodeObjectUploader&                uploader,
    bool                                     glgEnabled,
    RegisterValuePair*                       pRegs,
    bool*                                    pIsWave32)
{
    const Device& device = static_cast<const Device&>(*pDevice->GetGfxDevice());
    const bool isWave32 = metadata.WavefrontSize() == 32;

    if (pIsWave32 != nullptr)
    {
        *pIsWave32 = isWave32;
    }

    if (pRegs != nullptr)
    {
        GpuSymbol symbol = {};

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC1))
        {
            auto* pRsrc1 = H::template Get<mmCOMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC1>(pRegs);
            pRsrc1->u32All = desc.compute_pgm_rsrc1;
        }

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC2))
        {
            auto* pRsrc2 = H::template Get<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(pRegs);
            pRsrc2->u32All        = desc.compute_pgm_rsrc2;
            pRsrc2->bits.LDS_SIZE =
                RoundUpQuotient(NumBytesToNumDwords(desc.group_segment_fixed_size), LdsDwGranularity);
        }

        COMPUTE_PGM_RSRC3* pRsrc3 = nullptr;
        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3))
        {
            pRsrc3 = H::template Get<mmCOMPUTE_PGM_RSRC3, COMPUTE_PGM_RSRC3>(pRegs);
            pRsrc3->u32All = desc.compute_pgm_rsrc3;

            // HSA has this defined in metadata, but we will override to be consistent with PAL runtime state.
            pRsrc3->bits.GLG_EN = glgEnabled;

            // PWS+ only support PreShader/PrePs waits if the IMAGE_OP bit is set. Theoretically we only set it for
            // shaders that do an image operation. However that would mean that our use of the pre-shader PWS+ wait
            // is dependent on us only waiting on image resources, which we don't know in our interface. For now always
            // set the IMAGE_OP bit for corresponding shaders, making the PreShader/PrePs waits global.
            pRsrc3->bits.IMAGE_OP = 1;
        }

        if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3) || H::Exist(mmCOMPUTE_PGM_LO))
        {
            if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::CsMainEntry, &symbol) == Result::Success)
            {
                PAL_ASSERT(IsPow2Aligned(symbol.gpuVirtAddr, 256));

                if constexpr (H::Exist(mmCOMPUTE_PGM_RSRC3))
                {
                    pRsrc3->bits.INST_PREF_SIZE = device.GetShaderPrefetchSize(symbol.size);
                }

                if constexpr (H::Exist(mmCOMPUTE_PGM_LO))
                {
                    auto* pPgmLo = H::template Get<mmCOMPUTE_PGM_LO, COMPUTE_PGM_LO>(pRegs);
                    pPgmLo->bits.DATA = Get256BAddrLo(symbol.gpuVirtAddr);
                }
            }
        }

        if constexpr (H::Exist(mmCOMPUTE_SHADER_CHKSUM))
        {
            auto* checksum = H::template Get<mmCOMPUTE_SHADER_CHKSUM, COMPUTE_SHADER_CHKSUM>(pRegs);

            checksum->u32All = hash;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_X))
        {
            auto* pX = H::template Get<mmCOMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_X>(pRegs);
            pX->bits.NUM_THREAD_FULL = groupSize.width;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_Y))
        {
            auto* pY = H::template Get<mmCOMPUTE_NUM_THREAD_Y, COMPUTE_NUM_THREAD_Y>(pRegs);
            pY->bits.NUM_THREAD_FULL = groupSize.height;
        }

        if constexpr (H::Exist(mmCOMPUTE_NUM_THREAD_Z))
        {
            auto* pZ = H::template Get<mmCOMPUTE_NUM_THREAD_Z, COMPUTE_NUM_THREAD_Z>(pRegs);
            pZ->bits.NUM_THREAD_FULL = groupSize.depth;
        }

        if constexpr (H::Exist(mmCOMPUTE_USER_DATA_1))
        {
            if (uploader.GetGpuSymbol(Abi::PipelineSymbolType::CsShdrIntrlTblPtr, &symbol) == Result::Success)
            {
                H::template Get<mmCOMPUTE_USER_DATA_1, COMPUTE_USER_DATA_1>(pRegs)->u32All = symbol.gpuVirtAddr;
            }
        }

        if constexpr (H::Exist(mmCOMPUTE_RESOURCE_LIMITS))
        {
            const uint32 threadsPerGroup = (groupSize.width *
                                            groupSize.height *
                                            groupSize.depth);
            const uint32 wavesPerGroup = RoundUpQuotient(threadsPerGroup, metadata.WavefrontSize());

            auto* pRsrcLimits = H::template Get<mmCOMPUTE_RESOURCE_LIMITS, COMPUTE_RESOURCE_LIMITS>(pRegs);

            pRsrcLimits->bits.WAVES_PER_SH = 0;

            pRsrcLimits->bits.LOCK_THRESHOLD = 0; // TODO: Get this out of the metadata.

            // SimdDestCntl: Controls which SIMDs thread groups get scheduled on.  If the number of
            // waves-per-TG is a multiple of 4, this should be 1, otherwise 0.
            pRsrcLimits->bits.SIMD_DEST_CNTL = ((wavesPerGroup % 4) == 0) ? 1 : 0;
            pRsrcLimits->bits.CU_GROUP_COUNT = 0; // TODO: Get this out of the metadata.

            // Force even distribution on all SIMDs in CU for workgroup size is 64
            // This has shown some good improvements if #CU per SE not a multiple of 4
            const GpuChipProperties& chipProps = pDevice->ChipProperties();
            if (((chipProps.gfx9.numShaderArrays * chipProps.gfx9.numCuPerSh) & 0x3) && (wavesPerGroup == 1))
            {
                pRsrcLimits->bits.FORCE_SIMD_DIST = 1;
            }
        }
    }
}

template void PipelineChunkCs::SetComputeShaderState<PipelineChunkCs::Regs> (
    const Pal::Device*                       pDevice,
    const Util::HsaAbi::CodeObjectMetadata&  metadata,
    const llvm::amdhsa::kernel_descriptor_t& desc,
    const uint32                             hash,
    Extent3d                                 groupSize,
    const CodeObjectUploader&                uploader,
    bool                                     glgEnabled,
    RegisterValuePair*                       pRegs,
    bool*                                    pIsWave32);

template void PipelineChunkCs::SetComputeShaderState<ComputeShaderLibrary::Regs>(
    const Pal::Device*                       pDevice,
    const Util::HsaAbi::CodeObjectMetadata&  metadata,
    const llvm::amdhsa::kernel_descriptor_t& desc,
    const uint32                             hash,
    Extent3d                                 groupSize,
    const CodeObjectUploader&                uploader,
    bool                                     glgEnabled,
    RegisterValuePair*                       pRegs,
    bool*                                    pIsWave32);

// =====================================================================================================================
size_t PipelineChunkCs::ComputeDvgprExtraAceScratch(
    const PalAbi::CodeObjectMetadata& metadata)
{
    // VGPRS[0:15] are stored as fixed allocations. Additional scratch memory needs to be allocatted for
    // VGPRs 16 and above.
    const bool   dynamicVgprEn   = metadata.pipeline.computeRegister.flags.dynamicVgprEn;
    const auto&  csStageMetadata = metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];

    size_t dvgprExtraAceScratch = 0;

    if (dynamicVgprEn && (csStageMetadata.wavefrontSize == 32))
    {
        dvgprExtraAceScratch = csStageMetadata.dynamicVgprSavedCount * sizeof(uint32);
    }

    return dvgprExtraAceScratch;
}

// =====================================================================================================================
void PipelineChunkCs::SetDispatchInterleaveState(
#if PAL_BUILD_NAVI48
    bool                   isNavi48,
#endif
    DispatchInterleaveSize interleaveSize)
{
    CsDispatchInterleaveSize overrideSetting   = m_pDevice->Settings().gfx12CsDispatchInterleaveSize;
    DispatchInterleaveSize   interleaveSizeLcl = interleaveSize;

    if (overrideSetting != CsDispatchInterleaveSizeHonorClient)
    {
        static_assert((uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_128) + 1  ==
                       uint32(DispatchInterleaveSize::_1D_128_Threads)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_256) + 1  ==
                       uint32(DispatchInterleaveSize::_1D_256_Threads)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_512) + 1  ==
                       uint32(DispatchInterleaveSize::_1D_512_Threads)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x1) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_1x1_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x2) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_1x2_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x4) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_1x4_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x8) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_1x8_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x16) + 1 ==
                       uint32(DispatchInterleaveSize::_2D_1x16_ThreadGroups)) &&

                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x1) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_2x1_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x2) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_2x2_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x4) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_2x4_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x8) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_2x8_ThreadGroups)) &&

                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x1) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_4x1_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x2) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_4x2_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x4) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_4x4_ThreadGroups)) &&

                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_8x1) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_8x1_ThreadGroups)) &&
                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_8x2) + 1  ==
                       uint32(DispatchInterleaveSize::_2D_8x2_ThreadGroups)) &&

                      (uint32(CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_16x1) + 1 ==
                       uint32(DispatchInterleaveSize::_2D_16x1_ThreadGroups)),
                      "Mismatch in some enums of CsDispatchInterleaveSize and DispatchInterleaveSize!");

        switch (overrideSetting)
        {
        case CsDispatchInterleaveSize::CsDispatchInterleaveSizeDisabled:
            interleaveSizeLcl = DispatchInterleaveSize::Disable;
            break;
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_64:
            interleaveSizeLcl = DispatchInterleaveSize::_1D_64_Threads;
            break;
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_128:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_256:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize1D_512:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x1:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x2:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x4:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x8:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_1x16:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x1:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x2:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x4:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_2x8:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x1:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x2:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_4x4:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_8x1:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_8x2:
        case CsDispatchInterleaveSize::CsDispatchInterleaveSize2D_16x1:
            interleaveSizeLcl = static_cast<DispatchInterleaveSize>(overrideSetting + 1);
            break;

        default:
            PAL_ASSERT_ALWAYS();
            break;
        }
    }

    DispatchDims threadsPerTg;

    threadsPerTg.x = GetHwReg<mmCOMPUTE_NUM_THREAD_X, COMPUTE_NUM_THREAD_X>().bits.NUM_THREAD_FULL;
    threadsPerTg.y = GetHwReg<mmCOMPUTE_NUM_THREAD_Y, COMPUTE_NUM_THREAD_Y>().bits.NUM_THREAD_FULL;
    threadsPerTg.z = GetHwReg<mmCOMPUTE_NUM_THREAD_Z, COMPUTE_NUM_THREAD_Z>().bits.NUM_THREAD_FULL;

    bool is2dDispatchInterleave = false;
    m_computeDispatchInterleave = GetComputeDispatchInterleave(interleaveSizeLcl,
                                                               threadsPerTg,
#if PAL_BUILD_NAVI48
                                                               isNavi48,
#endif
                                                               &is2dDispatchInterleave);

    m_flags.is2dDispatchInterleave = is2dDispatchInterleave;
}

// =====================================================================================================================
Result PipelineChunkCs::HwlInit(
    const CodeObjectUploader&               uploader,
    const Util::PalAbi::CodeObjectMetadata& metadata,
    DispatchInterleaveSize                  interleaveSize,
    bool                                    glgEnabled)
{
    const Pal::Device* const pDevice = m_pDevice->Parent();

    Result result = ComputeUserDataLayout::Create(*pDevice, metadata.pipeline, &m_pUserDataLayout);

    if (result == Result::Success)
    {
        if (pDevice->Settings().pipelinePrefetchEnable)
        {
            m_prefetchAddr = uploader.PrefetchAddr();
            m_prefetchSize = uploader.PrefetchSize();
        }

        bool isWave32 = false;
        SetComputeShaderState<Regs>(
            pDevice, metadata, nullptr, uploader, glgEnabled, m_regs, &isWave32);

        m_flags.isWave32                    = isWave32;
        m_flags.isDefaultDispatchInterleave =
            (m_pDevice->Settings().gfx12CsDispatchInterleaveSize == CsDispatchInterleaveSizeHonorClient) &&
            (interleaveSize == DispatchInterleaveSize::Default);

        SetDispatchInterleaveState(
#if PAL_BUILD_NAVI48
                                   IsNavi48(*pDevice),
#endif
                                   interleaveSize);

        m_dvgprExtraAceScratch = ComputeDvgprExtraAceScratch(metadata);
    }

    return result;
}

// =====================================================================================================================
Result PipelineChunkCs::HwlInit(
    const CodeObjectUploader&                uploader,
    const Util::HsaAbi::CodeObjectMetadata&  metadata,
    const llvm::amdhsa::kernel_descriptor_t& desc,
    const uint32                             hash,
    Extent3d                                 groupSize,
    DispatchInterleaveSize                   interleaveSize,
    bool                                     glgEnabled)
{
    const Pal::Device* const pDevice = m_pDevice->Parent();

    if (pDevice->Settings().pipelinePrefetchEnable)
    {
        m_prefetchAddr = uploader.PrefetchAddr();
        m_prefetchSize = uploader.PrefetchSize();
    }
    bool isWave32 = false;
    SetComputeShaderState<Regs>(pDevice, metadata, desc, hash, groupSize, uploader, glgEnabled, m_regs, &isWave32);

    m_flags.isWave32 = isWave32;
    m_flags.isDefaultDispatchInterleave =
        (m_pDevice->Settings().gfx12CsDispatchInterleaveSize == CsDispatchInterleaveSizeHonorClient) &&
        (interleaveSize == DispatchInterleaveSize::Default);

    SetDispatchInterleaveState(
#if PAL_BUILD_NAVI48
        IsNavi48(*pDevice),
#endif
        interleaveSize);

    return Result::Success;
}

// =====================================================================================================================
uint32* PipelineChunkCs::WriteCommands(
    const PipelineChunkCs*          pPrevChunkCs,
    const DynamicComputeShaderInfo& dynamicInfo,
    bool                            prefetch,
    uint32*                         pCmdSpace,
    CmdStream*                      pCmdStream
    ) const
{
    const EngineType engine = pCmdStream->GetEngineType();

    constexpr DynamicComputeShaderInfo NullDynamicInfo = { };
    const uint32 csRegSize =
        (Regs::GetC<mmCOMPUTE_USER_DATA_1, COMPUTE_USER_DATA_1>(m_regs).u32All == InvalidUserDataInternalTable) ?
        (Regs::Size() - 1) : Regs::Size();
    if (memcmp(&dynamicInfo, &NullDynamicInfo, sizeof(DynamicComputeShaderInfo)) == 0)
    {
        static_assert(Regs::Size() == Regs::NumSh(), "Only SH registers expected!");
        pCmdSpace += CmdUtil::BuildSetShPairs<ShaderCompute>(m_regs, csRegSize, pCmdSpace);
    }
    else
    {
        RegisterValuePair regs[Regs::Size()];
        memcpy(regs, m_regs, sizeof(m_regs));

        OverrideDynamicState(dynamicInfo, regs);

        pCmdSpace += CmdUtil::BuildSetShPairs<ShaderCompute>(regs, csRegSize, pCmdSpace);
    }

    if (prefetch && (m_prefetchAddr != 0))
    {
        const PrefetchMethod method = (engine == EngineTypeCompute) ?
            m_pDevice->Settings().shaderPrefetchMethodAce : m_pDevice->Settings().shaderPrefetchMethodGfx;

        if (method != PrefetchDisabled)
        {
            PrimeGpuCacheRange cacheInfo;
            cacheInfo.gpuVirtAddr         = m_prefetchAddr;
            cacheInfo.size                = m_prefetchSize;
            cacheInfo.usageMask           = CoherShaderRead;
            cacheInfo.addrTranslationOnly = (method == PrefetchPrimeUtcL2);

            pCmdSpace += CmdUtil::BuildPrimeGpuCaches(cacheInfo,
                                                      m_pDevice->Parent()->Settings().prefetchClampSize,
                                                      engine,
                                                      pCmdSpace);
        }
    }

    return pCmdSpace;
}

// =====================================================================================================================
uint32* PipelineChunkCs::WriteShCommandsLdsSize(
    uint32*    pCmdSpace,
    uint32     ldsBytesPerTg
    ) const
{
    // If ldsBytesPerTg is zero, which means there is no dynamic LDS, keep LDS_SIZE register as static LDS size.
    if (ldsBytesPerTg > 0)
    {
        COMPUTE_PGM_RSRC2 computePgmRsrc2 = Regs::GetC<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(m_regs);

        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LdsSize field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        computePgmRsrc2.bits.LDS_SIZE =
            Pow2Align((ldsBytesPerTg / sizeof(uint32)), LdsDwGranularity) >> LdsDwGranularityShift;

        pCmdSpace = CmdStream::WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_PGM_RSRC2,
                                                               computePgmRsrc2.u32All,
                                                               pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
void PipelineChunkCs::UpdateAfterLibraryLink(
    const LibraryHwInfo& hwInfo)
{
    *Regs::Get<mmCOMPUTE_PGM_RSRC1, COMPUTE_PGM_RSRC1>(m_regs) = hwInfo.libRegs.computePgmRsrc1;
    *Regs::Get<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(m_regs) = hwInfo.libRegs.computePgmRsrc2;
    *Regs::Get<mmCOMPUTE_PGM_RSRC3, COMPUTE_PGM_RSRC3>(m_regs) = hwInfo.libRegs.computePgmRsrc3;
}

// =====================================================================================================================
Result PipelineChunkCs::MergeUserDataLayout(
    const ComputeUserDataLayout& layout)
{
    MutexAuto lock(&m_userDataCombineMutex);
    return layout.CombineWith(*m_pDevice->Parent(), &m_pUserDataLayout);
}

// =====================================================================================================================
void PipelineChunkCs::OverrideDynamicState(
    const DynamicComputeShaderInfo& input,
    RegisterValuePair               regs[Regs::Size()]
    ) const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    auto* pRsrcLimits = Regs::Get<mmCOMPUTE_RESOURCE_LIMITS, COMPUTE_RESOURCE_LIMITS>(regs);

    constexpr uint32 MaxTgPerCu = 15;

    // CS threadgroup limit per CU. Range is 1 to 15, 0 disables the limit.
    pRsrcLimits->bits.TG_PER_CU = Min(input.maxThreadGroupsPerCu, MaxTgPerCu);

    if (input.maxWavesPerCu > 0)
    {
        // 1 means 1 wave, 1023 means 1023, and 0 disables the limit.
        // This is actually WAVES_PER_SE
        pRsrcLimits->bits.WAVES_PER_SH = CalcMaxWavesPerSe(chipProps, input.maxWavesPerCu);
    }

    // CuGroupCount: Sets the number of CS threadgroups to attempt to send to a single CU before moving to the next CU.
    // Range is 1 to 8, 0 disables the limit.
    constexpr uint32 MaxCuGroupCount = 8;
    if (input.tgScheduleCountPerCu > 0)
    {
        // Number of threadgroups to attempt to send to a CU before moving on to the next CU.
        // 0 = 1 threadgroup, 7 = 8 threadgroups.
        pRsrcLimits->bits.CU_GROUP_COUNT = Min(input.tgScheduleCountPerCu, MaxCuGroupCount) - 1;
    }

    if (input.ldsBytesPerTg > 0)
    {
        auto* pComputePgmRsrc2 = Regs::Get<mmCOMPUTE_PGM_RSRC2, COMPUTE_PGM_RSRC2>(regs);

        // Round to nearest multiple of the LDS granularity, then convert to the register value.
        // NOTE: Granularity for the LdsSize field is 128, range is 0->128 which allocates 0 to 16K DWORDs.
        pComputePgmRsrc2->bits.LDS_SIZE =
            Pow2Align((input.ldsBytesPerTg / sizeof(uint32)), LdsDwGranularity) >> LdsDwGranularityShift;
    }
}

} // namespace Gfx12
} // namespace Pal
