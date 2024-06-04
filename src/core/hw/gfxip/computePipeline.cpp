/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/computePipeline.h"
#include "palHsaAbiMetadata.h"
#include "palMetroHash.h"
#include "palPipelineAbi.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
ComputePipeline::ComputePipeline(
    Device* pDevice,
    bool    isInternal)  // True if this is a PAL-owned pipeline (i.e., an RPM pipeline).
    :
    Pipeline(pDevice, isInternal),
    m_pHsaMeta(nullptr),
    m_pKernelDescriptor(nullptr),
    m_threadsPerTg{},
    m_maxFunctionCallDepth(0),
    m_stackSizeInBytes(0)
{
    memset(&m_stageInfo, 0, sizeof(m_stageInfo));
    m_stageInfo.stageId = Abi::HardwareStage::Cs;
}

// =====================================================================================================================
ComputePipeline::~ComputePipeline()
{
    PAL_SAFE_DELETE(m_pHsaMeta, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Initialize this compute pipeline based on the provided creation info.
Result ComputePipeline::Init(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    Result result = Result::Success;

    m_maxFunctionCallDepth = createInfo.maxFunctionCallDepth;

    if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize != 0))
    {
        m_pipelineBinaryLen = createInfo.pipelineBinarySize;
        m_pPipelineBinary   = PAL_MALLOC(m_pipelineBinaryLen, m_pDevice->GetPlatform(), AllocInternal);
        if (m_pPipelineBinary == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
        else
        {
            memcpy(m_pPipelineBinary, createInfo.pPipelineBinary, m_pipelineBinaryLen);
        }
    }
    else
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        PAL_ASSERT((m_pPipelineBinary != nullptr) && (m_pipelineBinaryLen != 0));

        const uint8 abi = abiReader.GetOsAbi();

        if (abi == Abi::ElfOsAbiAmdgpuPal)
        {
            result = InitFromPalAbiBinary(createInfo, abiReader, metadata, pMetadataReader);
        }
        else if ((abi == Abi::ElfOsAbiAmdgpuHsa) && (m_pDevice->ChipProperties().gfxip.supportHsaAbi == 1))
        {
            result = InitFromHsaAbiBinary(createInfo, abiReader, pMetadataReader);
        }
        else
        {
            // You can end up here if this is an unknown ABI or if we don't support a known ABI on this device.
            result = Result::ErrorUnsupportedPipelineElfAbiVersion;
        }
    }

    if (result == Result::Success)
    {
        auto*const pEventProvider = m_pDevice->GetPlatform()->GetGpuMemoryEventProvider();

        ResourceDescriptionPipeline desc { };
        desc.pPipelineInfo = &GetInfo();
        desc.pCreateFlags  = &createInfo.flags;

        ResourceCreateEventData data { };
        data.type              = ResourceType::Pipeline;
        data.pResourceDescData = &desc;
        data.resourceDescSize  = sizeof(desc);
        data.pObj              = this;
        pEventProvider->LogGpuMemoryResourceCreateEvent(data);

        GpuMemoryResourceBindEventData bindData { };
        bindData.pObj               = this;
        bindData.pGpuMemory         = m_gpuMem.Memory();
        bindData.requiredGpuMemSize = m_gpuMemSize;
        bindData.offset             = m_gpuMem.Offset();
        pEventProvider->LogGpuMemoryResourceBindEvent(bindData);

        Developer::BindGpuMemoryData callbackData = {};
        callbackData.pObj               = bindData.pObj;
        callbackData.requiredGpuMemSize = bindData.requiredGpuMemSize;
        callbackData.pGpuMemory         = bindData.pGpuMemory;
        callbackData.offset             = bindData.offset;
        callbackData.isSystemMemory     = bindData.isSystemMemory;
        m_pDevice->DeveloperCb(Developer::CallbackType::BindGpuMemory, &callbackData);
    }

    PAL_ASSERT(Pipeline::DispatchInterleaveSizeIsValid(createInfo.interleaveSize, m_pDevice->ChipProperties()));

    return result;
}

// =====================================================================================================================
// Extracts PAL ABI metadata from the pipeline binary and initializes the pipeline from it.
Result ComputePipeline::InitFromPalAbiBinary(
    const ComputePipelineCreateInfo&  createInfo,
    const AbiReader&                  abiReader,
    const PalAbi::CodeObjectMetadata& metadata,
    MsgPackReader*                    pMetadataReader)
{
    ExtractPipelineInfo(metadata, ShaderType::Compute, ShaderType::Compute);

    DumpPipelineElf("PipelineCs", metadata.pipeline.name);

    const Elf::SymbolTableEntry* pSymbol =
        abiReader.GetPipelineSymbol(Abi::PipelineSymbolType::CsDisassembly);

    if (pSymbol != nullptr)
    {
        m_stageInfo.disassemblyLength = static_cast<size_t>(pSymbol->st_size);
    }

    const PalAbi::HardwareStageMetadata& csStageMetadata =
        metadata.pipeline.hardwareStage[static_cast<uint32>(Abi::HardwareStage::Cs)];

    if (csStageMetadata.hasEntry.backendStackSize != 0)
    {
        // Used by the new raytracing for Continuation, exported to Clients by IPipeline::GetStackSizes().
        m_cpsStackSizeInBytes.backendSize = csStageMetadata.backendStackSize;
    }

    if (csStageMetadata.hasEntry.frontendStackSize != 0)
    {
        // Used by the new raytracing for Continuation, exported to Clients by IPipeline::GetStackSizes().
        m_cpsStackSizeInBytes.frontendSize = csStageMetadata.frontendStackSize;
    }

    if (csStageMetadata.hasEntry.scratchMemorySize != 0)
    {
        // Used by the old client-raytracing way. It starts with launch kernel scratch size. Updated for the full
        // pipeline in LinkWithLibraries().
        m_stackSizeInBytes = csStageMetadata.scratchMemorySize;
    }

    Result result = HwlInit(createInfo, abiReader, metadata, pMetadataReader);

    return result;
}

// =====================================================================================================================
// Extracts HSA ABI metadata from the pipeline binary and initializes the pipeline from it.
Result ComputePipeline::InitFromHsaAbiBinary(
    const ComputePipelineCreateInfo& createInfo,
    const AbiReader&                 abiReader,
    MsgPackReader*                   pMetadataReader)
{
    Result result = Result::ErrorOutOfMemory;

    PAL_ASSERT(m_pHsaMeta == nullptr);
    m_pHsaMeta = PAL_NEW(HsaAbi::CodeObjectMetadata, m_pDevice->GetPlatform(), AllocInternal)(m_pDevice->GetPlatform());

    if (m_pHsaMeta != nullptr)
    {
        result = abiReader.GetMetadata(pMetadataReader, m_pHsaMeta, createInfo.pKernelName);
    }

    if (result == Result::Success)
    {
        // The metadata gives the name of our kernel's launch descriptor object. Look it up in the ELF binary and
        // cache a pointer to it for future reference. Note that we don't make a new copy, it's just a pointer.
        // It's a required symbol so it should never be nullptr.
        const Elf::SymbolTableEntry* pSymbol = abiReader.GetGenericSymbol(m_pHsaMeta->KernelDescriptorSymbol());
        PAL_ASSERT(pSymbol != nullptr);

        const void* pData = nullptr;
        result = abiReader.GetElfReader().GetSymbol(*pSymbol, &pData);

        if (result == Result::Success)
        {
            m_pKernelDescriptor = static_cast<const llvm::amdhsa::kernel_descriptor_t*>(pData);
        }
    }

    if (result == Result::Success)
    {
        // Hash the entire ELF to get a "good enough" pipeline and shader hash. There's no difference between
        // the stable hash and the unique hash so they get set to the same compacted 64-bit value.
        MetroHash128 hasher;
        hasher.Update(static_cast<const uint8*>(m_pPipelineBinary), m_pipelineBinaryLen);

        MetroHash::Hash hashedBin = {};
        hasher.Finalize(hashedBin.bytes);

        const uint64     hash64 = MetroHash::Compact64(&hashedBin);
        constexpr uint32 CsIdx  = static_cast<uint32>(Abi::ApiShaderType::Cs);

        m_info.flags.hsaAbi              = 1;
        m_info.internalPipelineHash      = { hash64, hash64 };
        m_info.shader[CsIdx].hash        = { hashedBin.qwords[0], hashedBin.qwords[1] };
        m_apiHwMapping.apiShaders[CsIdx] = CsIdx;

        // It's not clear if this is correct or if it should be zero (no expected stack support).
        m_stackSizeInBytes = m_pHsaMeta->PrivateSegmentFixedSize();

        DumpPipelineElf("PipelineCs", m_pHsaMeta->KernelName());

        result = HwlInit(createInfo, abiReader, *m_pHsaMeta, pMetadataReader);
    }

    return result;
}

// =====================================================================================================================
const HsaAbi::CodeObjectMetadata& ComputePipeline::HsaMetadata() const
{
    PAL_ASSERT((m_info.flags.hsaAbi == 1) && (m_pHsaMeta != nullptr));

    return *m_pHsaMeta;
}

// =====================================================================================================================
const llvm::amdhsa::kernel_descriptor_t& ComputePipeline::KernelDescriptor() const
{
    PAL_ASSERT((m_info.flags.hsaAbi == 1) && (m_pKernelDescriptor != nullptr));

    return *m_pKernelDescriptor;
}

// =====================================================================================================================
const HsaAbi::KernelArgument* ComputePipeline::GetKernelArgument(
    uint32 index
    ) const
{
    return ((m_pHsaMeta != nullptr) && (index < m_pHsaMeta->NumArguments()))
                ? (m_pHsaMeta->Arguments() + index)
                : nullptr;
}

// =====================================================================================================================
// Get the size of the stack managed by the compiler, including the backend and the frontend.
Result ComputePipeline::GetStackSizes(
    CompilerStackSizes* pSizes
    ) const
{
    Result result = Result::ErrorInvalidPointer;

    if (pSizes != nullptr)
    {
        *pSizes = m_cpsStackSizeInBytes;
        result = Result::Success;
    }

    return result;
}

} // Pal
