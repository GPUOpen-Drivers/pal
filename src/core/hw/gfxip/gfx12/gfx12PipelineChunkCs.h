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

#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{
namespace Gfx12
{

class CmdStream;
class ComputeUserDataLayout;
class Device;
struct LibraryHwInfo;

// =====================================================================================================================
// Represents the chunk of a pipeline object which contains all of the registers which setup the hardware CS stage.
// This is sort of a PM4 "image" of the commands which write these registers, but with some intelligence so that the
// code used to setup the commands can be reused.
class PipelineChunkCs
{
public:
    PipelineChunkCs(const Device* pDevice);
    ~PipelineChunkCs();

    void Clone(const PipelineChunkCs& other);

    template<typename H>
    static void SetComputeShaderState(
        const Pal::Device*                      pDevice,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const ShaderLibStats*                   pLibStats,
        const CodeObjectUploader&               uploader,
        bool                                    glgEnabled,
        RegisterValuePair*                      pRegs,
        bool*                                   pIsWave32);

    template<typename H>
    static void SetComputeShaderState(
        const Pal::Device*                       pDevice,
        const Util::HsaAbi::CodeObjectMetadata&  metadata,
        const llvm::amdhsa::kernel_descriptor_t& desc,
        const uint32                             hash,
        Extent3d                                 groupSize,
        const CodeObjectUploader&                uploader,
        bool                                     glgEnabled,
        RegisterValuePair*                       pRegs,
        bool*                                    pIsWave32);

    static size_t ComputeDvgprExtraAceScratch(const Util::PalAbi::CodeObjectMetadata& metadata);

    Result HwlInit(
        const CodeObjectUploader&               uploader,
        const Util::PalAbi::CodeObjectMetadata& metadata,
        DispatchInterleaveSize                  interleaveSize,
        bool                                    glgEnabled);

    Result HwlInit(
        const CodeObjectUploader&                uploader,
        const Util::HsaAbi::CodeObjectMetadata&  metadata,
        const llvm::amdhsa::kernel_descriptor_t& desc,
        const uint32                             hash,
        Extent3d                                 groupSize,
        DispatchInterleaveSize                   interleaveSize,
        bool                                     glgEnabled);

    uint32* WriteCommands(
        const PipelineChunkCs*          pPrevChunkCs,
        const DynamicComputeShaderInfo& dynamicInfo,
        bool                            prefetch,
        uint32*                         pCmdSpace,
        CmdStream*                      pCmdStream) const;

    uint32* WriteShCommandsLdsSize(
        uint32*    pCmdSpace,
        uint32     ldsBytesPerTg) const;

    void UpdateAfterLibraryLink(const LibraryHwInfo& hwInfo);

    Result MergeUserDataLayout(const ComputeUserDataLayout& layout);

    template <uint32 RegOffset, typename R>
    const R& GetHwReg() const { return Regs::GetC<RegOffset, R>(m_regs); }

    bool IsWave32() const { return m_flags.isWave32 != 0; }
    bool Is2dDispatchInterleave() const { return m_flags.is2dDispatchInterleave != 0; }
    bool IsDefaultDispatchInterleave() const { return m_flags.isDefaultDispatchInterleave != 0; }

    regCOMPUTE_DISPATCH_INTERLEAVE ComputeDispatchInterleave() const { return m_computeDispatchInterleave; }

    uint32 Get2dDispachInterleaveSize() const
    {
        return 1u << (m_computeDispatchInterleave.bits.INTERLEAVE_2D_X_SIZE +
                      m_computeDispatchInterleave.bits.INTERLEAVE_2D_Y_SIZE);
    }
    const ComputeUserDataLayout* UserDataLayout() const { return m_pUserDataLayout; }

    size_t GetDvgprExtraAceScratch() const { return m_dvgprExtraAceScratch; }

private:
    static constexpr uint32 Registers[] =
    {
        Chip::mmCOMPUTE_PGM_LO,
        Chip::mmCOMPUTE_SHADER_CHKSUM,
        Chip::mmCOMPUTE_PGM_RSRC1,
        Chip::mmCOMPUTE_PGM_RSRC2,
        Chip::mmCOMPUTE_PGM_RSRC3,
        Chip::mmCOMPUTE_NUM_THREAD_X,
        Chip::mmCOMPUTE_NUM_THREAD_Y,
        Chip::mmCOMPUTE_NUM_THREAD_Z,
        Chip::mmCOMPUTE_RESOURCE_LIMITS,
        Chip::mmCOMPUTE_USER_DATA_1,
    };

    using Regs = RegPairHandler<decltype(Registers), Registers>;
    static_assert(Registers[Regs::Size() - 1] == Chip::mmCOMPUTE_USER_DATA_1,
                  "Expect mmCOMPUTE_USER_DATA_1 in the end of Regs.");
    static_assert(Regs::Size() == Regs::NumSh(), "Only SH regs expected.");

    void OverrideDynamicState(
        const DynamicComputeShaderInfo& input,
        RegisterValuePair               regs[Regs::Size()]) const;

    void SetDispatchInterleaveState(
#if PAL_BUILD_NAVI48
                                    bool                   isNavi48,
#endif
                                    DispatchInterleaveSize interleaveSize);

    const Device* m_pDevice;

    ComputeUserDataLayout* m_pUserDataLayout;
    Util::Mutex            m_userDataCombineMutex;    // Mutex guarding calls to CombineWith and Duplicate

    RegisterValuePair m_regs[Regs::Size()];

    // COMPUTE_DISPATCH_INTERLEAVE is not included in m_regs because it must be
    // set by IT_SET_SH_REG_INDEXED specially.
    regCOMPUTE_DISPATCH_INTERLEAVE m_computeDispatchInterleave;

    gpusize m_prefetchAddr;
    gpusize m_prefetchSize;

    union
    {
        struct
        {
            uint32 isWave32                    :  1;
            uint32 is2dDispatchInterleave      :  1;
            uint32 isDefaultDispatchInterleave :  1; // If this is default compute interleave size determined by PAL but
                                                     // not specified interleave size from clients or panel setting.
            uint32 reserved                    : 29;
        };

        uint32 u32All;
    } m_flags;

    size_t m_dvgprExtraAceScratch; // Additional scratch memory when dVGPRs are used in ACE compute queues.

    PAL_DISALLOW_DEFAULT_CTOR(PipelineChunkCs);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineChunkCs);
};

} // namespace Gfx12
} // namespace Pal
