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

#include "core/device.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12GraphicsPipeline.h"
#include "core/hw/gfxip/gfx12/gfx12UserDataLayout.h"
#include "core/hw/gfxip/gfx12/gfx12UniversalCmdBuffer.h"
#include "g_palPipelineAbiMetadata.h"

#include "palAutoBuffer.h"
#include "palHsaAbiMetadata.h"
#include "palIterator.h"
#include "palSysMemory.h"
#include <cstring>

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Returns a count of how many physical registers this MultiUserDataReg broadcasts to (0 to 3).
static uint32 MultiUserDataRegCount(
    const MultiUserDataReg& map)
{
    uint32 count = (map.regOffset0 != 0) +
                   (map.regOffset1 != 0) +
                   (map.regOffset2 != 0);

    return count;
}

// =====================================================================================================================
UserDataLayout::UserDataLayout(
    const Pal::Device& device,
    uint32*            pMap,
    uint32             numMapWords,
    uint32             spillThreshold)
    :
    m_device(device),
    m_hash(0),
    m_pMap(pMap),
    m_numMapWords(numMapWords),
    m_spillThreshold(spillThreshold)
{
}

// =====================================================================================================================
void UserDataLayout::Destroy()
{
    const Pal::Device& device = m_device;

    this->~UserDataLayout();
    PAL_FREE(this, device.GetPlatform());
}

// =====================================================================================================================
// Examines a prior bound user data layout vs. the current to minimize the amount of state that needs to be re-sent to
// the HW. Returns true if any delta is found, false if they are identical.
bool UserDataLayout::ComputeLayoutDelta(
    const UserDataLayout* pPrevLayout,
    LayoutDelta*          pOut
    ) const
{
    bool ret = false;

    if ((pPrevLayout == nullptr) || (m_hash != pPrevLayout->m_hash))
    {
        // Found at least some delta.
        ret = true;

        // If the previous layout is unknown, assume the whole map is stale.
        pOut->firstStaleMapWord = 0;
        pOut->numStaleMapWords  = m_numMapWords;

        // If the previous layout is unknown, assume nothing was spilled. I.e., if the new layout spills, all user
        // data from the new threshold up has to be re-set.
        uint32 oldSpillThreshold = MaxUserDataEntries;

        if (pPrevLayout != nullptr)
        {
            // Search through map and identify first point of divergence between the old and new layouts.
            const uint32 overlappingUserData = Min(pPrevLayout->m_numMapWords, m_numMapWords);
            while (pOut->firstStaleMapWord < overlappingUserData)
            {
                if (pPrevLayout->m_pMap[pOut->firstStaleMapWord] != m_pMap[pOut->firstStaleMapWord])
                {
                    break;
                }
                pOut->firstStaleMapWord++;
            }

            pOut->numStaleMapWords -= pOut->firstStaleMapWord;

            oldSpillThreshold = pPrevLayout->m_spillThreshold;
        }

        // If this layout lowers the spill threshold, we must re-set all user data that is freshly spilled.
        const uint32 numStaleSpillEntries = Max(int32(oldSpillThreshold) - int32(m_spillThreshold), 0);

        // Compute firstStaleEntry/numStaleEntries which report which user data entry values need to be re-set based
        // on the newly bound layout.
        if ((pOut->numStaleMapWords == 0) && (numStaleSpillEntries == 0))
        {
            pOut->numStaleEntries = 0;
        }
        else if (numStaleSpillEntries == 0)
        {
            pOut->firstStaleEntry = pOut->firstStaleMapWord;
            pOut->numStaleEntries = pOut->numStaleMapWords;
        }
        else if (pOut->numStaleMapWords == 0)
        {
            pOut->firstStaleEntry = m_spillThreshold;
            pOut->numStaleEntries = numStaleSpillEntries;
        }
        else
        {
            pOut->firstStaleEntry = Min(pOut->firstStaleMapWord, m_spillThreshold);
            pOut->numStaleEntries = Max(pOut->firstStaleMapWord + pOut->numStaleMapWords,
                                        m_spillThreshold + numStaleSpillEntries) - pOut->firstStaleEntry;
        }
    }

    return ret;
}

// =====================================================================================================================
// Allocates memory for and initializes a GraphicsUseDataLayout object.
Result GraphicsUserDataLayout::Create(
    const Pal::Device&              device,
    const PalAbi::PipelineMetadata& metadata,
    GraphicsUserDataLayout**        ppObject)
{
    PAL_ASSERT(ppObject != nullptr);

    MultiUserDataReg map[MaxUserDataEntries] = { };

    CreateInfo createInfo = { };

    // Ensure UserDataReg and MultUserDataReg fields are properly initialized to NotMapped.
    PAL_ASSERT((createInfo.esGsLdsSize.regOffset == UserDataNotMapped) &&
               (createInfo.viewId.regOffset0     == UserDataNotMapped) &&
               (createInfo.viewId.regOffset1     == UserDataNotMapped) &&
               (createInfo.viewId.regOffset2     == UserDataNotMapped));

    createInfo.spillThreshold = metadata.hasEntry.spillThreshold ? Min(metadata.spillThreshold, MaxUserDataEntries) :
                                                                   MaxUserDataEntries;
    createInfo.pMap           = &map[0];

    static constexpr Abi::HardwareStage AbiHwStage[] =
    {
        Abi::HardwareStage::Hs,
        Abi::HardwareStage::Gs,
        Abi::HardwareStage::Ps,
    };

    for (const auto& hwStage : AbiHwStage)
    {
        const auto& hwShader = metadata.hardwareStage[uint32(hwStage)];

        if (hwShader.hasEntry.userDataRegMap == 0)
        {
            continue;
        }

        const uint32 startingUserDataReg = StartingUserDataOffset[uint32(hwStage)] - PERSISTENT_SPACE_START;
        PAL_ASSERT(startingUserDataReg != UserDataNotMapped);

        for (uint16 offset = 0; offset < 32; offset++)
        {
            const uint32 value     = hwShader.userDataRegMap[offset];
            const uint32 regOffset = startingUserDataReg + offset;
            if (value != uint32(Abi::UserDataMapping::NotMapped))
            {
                if (value < MaxUserDataEntries)
                {
                    map[value].u32All <<= 10;

                    map[value].regOffset0 = regOffset;

                    createInfo.numMapWords = Max(createInfo.numMapWords, value + 1);
                }
                else
                {
                    switch (Abi::UserDataMapping(value))
                    {
                    case Abi::UserDataMapping::GlobalTable:
                        PAL_ASSERT(regOffset == startingUserDataReg + InternalTblStartReg);
                        break;
                    case Abi::UserDataMapping::BaseVertex:
                        createInfo.baseVertex.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::BaseInstance:
                        createInfo.baseInstance.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::DrawIndex:
                        createInfo.drawIndex.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::VertexBufferTable:
                        createInfo.vertexBufferTable.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::StreamOutTable:
                        createInfo.streamoutTable.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::StreamOutControlBuf:
                        createInfo.streamoutCtrlBuf.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::EsGsLdsSize:
                        createInfo.esGsLdsSize.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::MeshTaskDispatchDims:
                        createInfo.meshDispatchDims.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::MeshTaskRingIndex:
                        createInfo.meshRingIndex.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::SampleInfo:
                        createInfo.sampleInfo.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::ColorExportAddr:
                        createInfo.colorExportAddr.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::EnPrimsNeededCnt:
                        createInfo.primsNeededCnt.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::NggCullingData:
                        createInfo.nggCullingData.regOffset = regOffset;
                        break;
                    case Abi::UserDataMapping::ViewId:
                        createInfo.viewId.u32All <<= 10;

                        createInfo.viewId.regOffset0 = regOffset;
                        break;
                    case Abi::UserDataMapping::CompositeData:
                        createInfo.compositeData.u32All <<= 10;

                        createInfo.compositeData.regOffset0 = regOffset;
                        break;
                    case Abi::UserDataMapping::SpillTable:
                        createInfo.spillTable.u32All <<= 10;

                        createInfo.spillTable.regOffset0 = regOffset;
                        break;
                    default:
                        PAL_NOT_IMPLEMENTED_MSG("Encountered unimplemented graphics user data type.");
                        break;
                    }
                }
            }
        }

        if ((hwStage == Abi::HardwareStage::Gs)                     &&
            (createInfo.nggCullingData.u32All == UserDataNotMapped) &&
            (metadata.graphicsRegister.hasEntry.nggCullingDataReg != 0))
        {
            createInfo.nggCullingData.regOffset = metadata.graphicsRegister.nggCullingDataReg;
        }
    }

    if (metadata.hasEntry.userDataLimit != 0)
    {
        createInfo.userDataLimit = metadata.userDataLimit;
    }

    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppObject = PAL_PLACEMENT_NEW(pMem) GraphicsUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Allocates memory for and initializes a GraphicsUseDataLayout object from partial graphics user data layout
Result GraphicsUserDataLayout::Create(
    const Pal::Device&              device,
    const GraphicsUserDataLayout&   preRasterLayout,
    const GraphicsUserDataLayout&   psLayout,
    GraphicsUserDataLayout**        ppObject)
{
    PAL_ASSERT(ppObject != nullptr);

    MultiUserDataReg map[MaxUserDataEntries];

    CreateInfo createInfo = { };
    // Merge built-in layout
    createInfo.baseVertex            = preRasterLayout.GetVertexBase();
    createInfo.baseInstance          = preRasterLayout.GetInstanceBase();
    createInfo.drawIndex             = preRasterLayout.GetDrawIndex();
    createInfo.vertexBufferTable     = preRasterLayout.GetVertexBufferTable();
    createInfo.streamoutTable        = preRasterLayout.GetStreamoutTable();
    createInfo.streamoutCtrlBuf      = preRasterLayout.GetStreamoutCtrlBuf();
    createInfo.esGsLdsSize.regOffset = preRasterLayout.EsGsLdsSizeRegOffset();
    createInfo.meshDispatchDims      = preRasterLayout.GetMeshDispatchDims();
    createInfo.meshRingIndex         = preRasterLayout.GetMeshRingIndex();
    createInfo.viewId                = preRasterLayout.GetViewId();
    createInfo.primsNeededCnt        = preRasterLayout.GetPrimNeededCnt();
    createInfo.sampleInfo            = psLayout.GetSampleInfo();
    createInfo.colorExportAddr       = psLayout.GetColorExportAddr();
    createInfo.nggCullingData        = preRasterLayout.GetNggCullingData();

    createInfo.compositeData         = preRasterLayout.GetCompositeData();
    if (psLayout.GetCompositeData().u32All > 0)
    {
        createInfo.compositeData.u32All <<= 10;
        createInfo.compositeData.regOffset0 = psLayout.GetCompositeData().regOffset0;
    }

    if (psLayout.GetViewId().u32All > 0)
    {
        createInfo.viewId.u32All <<= 10;
        createInfo.viewId.regOffset0 = psLayout.GetViewId().regOffset0;
    }

    // Merge general user data layout
    createInfo.spillThreshold = Min(preRasterLayout.GetSpillThreshold(), psLayout.GetSpillThreshold());
    createInfo.userDataLimit  = Max(preRasterLayout.GetUserDataLimit(), psLayout.GetUserDataLimit());
    createInfo.spillTable     = preRasterLayout.GetSpillTable();
    if (psLayout.GetSpillTable().u32All > 0)
    {
        createInfo.spillTable.u32All <<= 10;
        createInfo.spillTable.regOffset0 = psLayout.GetSpillTable().regOffset0;
    }

    createInfo.numMapWords = Max(preRasterLayout.GetNumMapWords(), psLayout.GetNumMapWords());
    createInfo.pMap = &map[0];
    memcpy(map, preRasterLayout.GetMapping(), sizeof(MultiUserDataReg) * preRasterLayout.GetNumMapWords());
    for (uint32 i = preRasterLayout.GetNumMapWords(); i < psLayout.GetNumMapWords(); i++)
    {
        map[i].u32All = 0;
    }

    const uint32* pPsMap = psLayout.GetMapping();
    for (uint32 i = 0; i < psLayout.GetNumMapWords(); i++)
    {
        if (pPsMap[i] != 0)
        {
            map[i].u32All <<= 10;
            map[i].regOffset0 = pPsMap[i];
        }
    }

    // Create UserDataLayout object
    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppObject = PAL_PLACEMENT_NEW(pMem) GraphicsUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Determines size in bytes required for the specified user data layout object.
size_t GraphicsUserDataLayout::Size(
    const CreateInfo& createInfo)
{
    const size_t extraSize = createInfo.numMapWords * sizeof(MultiUserDataReg);

    // Contents of the packet are stored after the object itself.
    return sizeof(GraphicsUserDataLayout) + extraSize;
}

// =====================================================================================================================
GraphicsUserDataLayout::GraphicsUserDataLayout(
    const Pal::Device& device,
    const CreateInfo&  createInfo)
    :
    UserDataLayout(device,
                   reinterpret_cast<uint32*>(this + 1),
                   createInfo.numMapWords,
                   createInfo.spillThreshold),
    m_baseVertex(createInfo.baseVertex),
    m_baseInstance(createInfo.baseInstance),
    m_drawIndex(createInfo.drawIndex),
    m_vertexBufferTable(createInfo.vertexBufferTable),
    m_streamoutCtrlBuf(createInfo.streamoutCtrlBuf),
    m_streamoutTable(createInfo.streamoutTable),
    m_esGsLdsSize(createInfo.esGsLdsSize),
    m_meshDispatchDims(createInfo.meshDispatchDims),
    m_meshRingIndex(createInfo.meshRingIndex),
    m_sampleInfo(createInfo.sampleInfo),
    m_colorExportAddr(createInfo.colorExportAddr),
    m_primsNeededCnt(createInfo.primsNeededCnt),
    m_nggCullingData(createInfo.nggCullingData),
    m_viewId(createInfo.viewId),
    m_compositeData(createInfo.compositeData),
    m_spillTable(createInfo.spillTable),
    m_userDataLimit(createInfo.userDataLimit)
{
    for (uint32 i = 0; i < m_numMapWords; i++)
    {
        const MultiUserDataReg map = createInfo.pMap[i];

        m_pMap[i] = map.u32All;
    }

    MetroHash64 hash;
    hash.Update(reinterpret_cast<const uint8*>(&createInfo), offsetof(CreateInfo, pMap));
    hash.Update(reinterpret_cast<const uint8*>(createInfo.pMap), m_numMapWords * sizeof(MultiUserDataReg));
    hash.Finalize(reinterpret_cast<uint8*>(&m_hash));

}

// =====================================================================================================================
template <bool PipelineSwitch>
uint32* GraphicsUserDataLayout::CopyUserDataPairsToCmdSpace(
    const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
    const UserDataFlags&          dirty,
    const uint32*                 pUserData,
    uint32*                       pCmdSpace
    ) const
{
    Pal::UserDataFlags localDirty;
    memcpy(&localDirty, &dirty, sizeof(localDirty));

    if (PipelineSwitch)
    {
        LayoutDelta delta;

        if (ComputeLayoutDelta(pPrevGfxUserDataLayout, &delta))
        {
            for (uint32 i = delta.firstStaleEntry;
                (i < delta.firstStaleEntry + delta.numStaleEntries) &&
                (i < m_numMapWords);
                i++)
            {
                const auto& origUserData = reinterpret_cast<const MultiUserDataReg&>(m_pMap[i]);

                MultiUserDataReg userData = origUserData;
                while (userData.regOffset0 != 0)
                {
                    pCmdSpace[0] = userData.regOffset0;
                    pCmdSpace[1] = pUserData[i];
                    pCmdSpace += 2;

                    WideBitfieldClearBit(localDirty, i);

                    userData.u32All >>= 10;
                }
            }
        }
    }

    if (WideBitfieldIsAnyBitSet(localDirty))
    {
        WideBitIter<size_t, NumUserDataFlagsParts> validIter(localDirty);

        if (m_numMapWords > 0)
        {
            while (validIter.IsValid())
            {
                const uint32 index = validIter.Get();

                if (index < m_numMapWords)
                {
                    const MultiUserDataReg& userData = reinterpret_cast<const MultiUserDataReg&>(m_pMap[index]);
                    MultiUserDataReg data = userData;
                    while (data.u32All != 0)
                    {
                        pCmdSpace[0] = data.regOffset0;
                        pCmdSpace[1] = pUserData[index];
                        pCmdSpace += 2;

                        data.u32All >>= 10;
                    }
                }

                validIter.Next();
            }
        }
    }

    return pCmdSpace;
}

template uint32* GraphicsUserDataLayout::CopyUserDataPairsToCmdSpace<true>(
    const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
    const Pal::UserDataFlags&     dirty,
    const uint32*                 pUserData,
    uint32*                       pCmdSpace) const;
template uint32* GraphicsUserDataLayout::CopyUserDataPairsToCmdSpace<false>(
    const GraphicsUserDataLayout* pPrevGfxUserDataLayout,
    const Pal::UserDataFlags&     dirty,
    const uint32*                 pUserData,
    uint32*                       pCmdSpace) const;

// =====================================================================================================================
Result GraphicsUserDataLayout::Duplicate(
    const Pal::Device&       device,
    GraphicsUserDataLayout** ppOther) const
{
    PAL_ASSERT((ppOther != nullptr) && (*ppOther == nullptr));

    CreateInfo createInfo { };
    createInfo.baseVertex             = m_baseVertex;
    createInfo.baseInstance           = m_baseInstance;
    createInfo.drawIndex              = m_drawIndex;
    createInfo.vertexBufferTable      = m_vertexBufferTable;
    createInfo.streamoutCtrlBuf       = m_streamoutCtrlBuf;
    createInfo.streamoutTable         = m_streamoutTable;
    createInfo.esGsLdsSize            = m_esGsLdsSize;
    createInfo.meshDispatchDims       = m_meshDispatchDims;
    createInfo.meshRingIndex          = m_meshRingIndex;
    createInfo.sampleInfo             = m_sampleInfo;
    createInfo.colorExportAddr        = m_colorExportAddr;
    createInfo.primsNeededCnt         = m_primsNeededCnt;
    createInfo.nggCullingData         = m_nggCullingData;
    createInfo.viewId                 = m_viewId;
    createInfo.compositeData          = m_compositeData;
    createInfo.spillThreshold         = m_spillThreshold;
    createInfo.spillTable             = m_spillTable;
    createInfo.numMapWords            = m_numMapWords;
    createInfo.pMap                   = reinterpret_cast<MultiUserDataReg*>(m_pMap);
    createInfo.userDataLimit          = m_userDataLimit;

    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppOther = PAL_PLACEMENT_NEW(pMem) GraphicsUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Allocates memory for and initializes a ComputeUseDataLayout object.
Result ComputeUserDataLayout::Create(
    const Pal::Device&              device,
    const PalAbi::PipelineMetadata& metadata,
    ComputeUserDataLayout**         ppObject)
{
    PAL_ASSERT(ppObject != nullptr);

    UserDataReg map[MaxUserDataEntries] = { };

    ComputeUserDataLayoutCreateInfo createInfo = { };
    if ((metadata.hasEntry.spillThreshold != 0) && (metadata.spillThreshold != NoUserDataSpilling))
    {
        createInfo.spillThreshold = Min(metadata.spillThreshold, MaxUserDataEntries);
    }
    else
    {
        createInfo.spillThreshold = NoUserDataSpilling;
    }

    createInfo.pMap = &map[0];

    const Util::PalAbi::HardwareStageMetadata& hwCs = metadata.hardwareStage[uint32(Abi::HardwareStage::Cs)];
    PAL_ASSERT(hwCs.userSgprs <= 16);

    const uint32 startingRegOffset = StartingUserDataOffset[uint32(Abi::HardwareStage::Cs)] - PERSISTENT_SPACE_START;

    for (uint16 sgprIdx = 0; sgprIdx < 16; sgprIdx++)
    {
        uint32 value = 0;
        if (hwCs.hasEntry.userDataRegMap)
        {
            value = hwCs.userDataRegMap[sgprIdx];

            // value is not mapped, move on to the next entry
            if (value == uint32(Abi::UserDataMapping::NotMapped))
            {
                continue;
            }

            const uint32 regOffset = startingRegOffset + sgprIdx;

            if (value < MaxUserDataEntries)
            {
                map[value].regOffset = regOffset;

                createInfo.numMapWords = Max(createInfo.numMapWords, value + 1);
            }
            else
            {
                switch (Abi::UserDataMapping(value))
                {
                case Abi::UserDataMapping::GlobalTable:
                    PAL_ASSERT(regOffset == startingRegOffset + InternalTblStartReg);
                    break;
                case Abi::UserDataMapping::Workgroup:
                    createInfo.workgroup.regOffset = regOffset;
                    break;
                case Abi::UserDataMapping::SpillTable:
                    createInfo.spillTable.regOffset = regOffset;
                    break;
                case Abi::UserDataMapping::MeshTaskDispatchDims:
                    createInfo.meshTaskDispatchDims.regOffset = regOffset;
                    break;
                case Abi::UserDataMapping::MeshTaskRingIndex:
                    createInfo.meshTaskRingIndex.regOffset = regOffset;
                    break;
                case Abi::UserDataMapping::TaskDispatchIndex:
                    createInfo.taskDispatchIndex.regOffset = regOffset;
                    break;
                default:
                    PAL_NOT_IMPLEMENTED_MSG("Encountered unimplemented compute user data type.");
                    break;
                }
            }
        }
    }

    if (metadata.hasEntry.userDataLimit != 0)
    {
        createInfo.userDataLimit = metadata.userDataLimit;
    }

    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppObject = PAL_PLACEMENT_NEW(pMem) ComputeUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Allocates memory for and initializes a ComputeUseDataLayout object.
Result ComputeUserDataLayout::Create(
    const Pal::Device&                      device,
    const Util::HsaAbi::CodeObjectMetadata& metadata,
    ComputeUserDataLayout**                 ppObject)
{
    PAL_ASSERT(ppObject != nullptr);

    UserDataReg map[MaxUserDataEntries] = { };

    ComputeUserDataLayoutCreateInfo createInfo = { };

    createInfo.spillThreshold = 0xFFFF;
    createInfo.userDataLimit = 0;
    createInfo.pMap = &map[0];

    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppObject = PAL_PLACEMENT_NEW(pMem) ComputeUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Determines size in bytes required for the specified user data layout object.
size_t ComputeUserDataLayout::Size(
    const ComputeUserDataLayoutCreateInfo& createInfo)
{
    const size_t extraSize = createInfo.numMapWords * sizeof(UserDataReg);

    // Contents of the packet are stored after the object itself.
    return sizeof(ComputeUserDataLayout) + extraSize;
}

// =====================================================================================================================
ComputeUserDataLayout::ComputeUserDataLayout(
    const Pal::Device&                     device,
    const ComputeUserDataLayoutCreateInfo& createInfo)
    :
    UserDataLayout(device,
                   reinterpret_cast<uint32*>(this + 1),
                   createInfo.numMapWords,
                   createInfo.spillThreshold),
    m_workgroup(createInfo.workgroup),
    m_taskDispatchDims(createInfo.meshTaskDispatchDims),
    m_meshTaskRingIndex(createInfo.meshTaskRingIndex),
    m_taskDispatchIndex(createInfo.taskDispatchIndex),
    m_spillTable(createInfo.spillTable),
    m_userDataLimit(createInfo.userDataLimit)
{
    for (uint32 i = 0; i < m_numMapWords; i++)
    {
        m_pMap[i] = createInfo.pMap[i].u32All;
    }

    MetroHash64 hash;
    hash.Update(reinterpret_cast<const uint8*>(&createInfo), offsetof(ComputeUserDataLayoutCreateInfo, pMap));
    hash.Update(reinterpret_cast<const uint8*>(createInfo.pMap), m_numMapWords * sizeof(UserDataReg));
    hash.Finalize(reinterpret_cast<uint8*>(&m_hash));
}

// =====================================================================================================================
// Duplicates this user-data layout by making a deep copy and creating a new object.
Result ComputeUserDataLayout::Duplicate(
    const Pal::Device&      device,
    ComputeUserDataLayout** ppOther
    ) const
{
    PAL_ASSERT((ppOther != nullptr) && (*ppOther == nullptr));

    ComputeUserDataLayoutCreateInfo createInfo { };
    createInfo.workgroup            = m_workgroup;
    createInfo.spillThreshold       = m_spillThreshold;
    createInfo.spillTable           = m_spillTable;
    createInfo.meshTaskDispatchDims = m_taskDispatchDims;
    createInfo.meshTaskRingIndex    = m_meshTaskRingIndex;
    createInfo.taskDispatchIndex    = m_taskDispatchIndex;
    createInfo.numMapWords          = m_numMapWords;
    createInfo.userDataLimit        = m_userDataLimit;
    createInfo.pMap                 = reinterpret_cast<UserDataReg*>(m_pMap);

    Result result = Result::ErrorOutOfMemory;
    void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);

    if (pMem != nullptr)
    {
        *ppOther = PAL_PLACEMENT_NEW(pMem) ComputeUserDataLayout(device, createInfo);
        result = Result::Success;
    }

    return result;
}

// =====================================================================================================================
// Helper function which merges a user-data register entry between two signatures.
// Returns true if the register was updated.
bool CombineUserDataReg(
    Result*      pResult,
    UserDataReg* pDest,
    UserDataReg  source)
{
    bool ret = false;

    if ((*pResult == Result::Success) && (source.regOffset != UserDataNotMapped))
    {
        if ((pDest->u32All != source.u32All) && (pDest->regOffset != UserDataNotMapped))
        {
            PAL_ASSERT_ALWAYS_MSG("Compute User-SGPR mapping not compatible with other nodes in the graph!");
            *pResult = Result::ErrorIncompatibleLibrary;
        }
        else
        {
            pDest->u32All = source.u32All;
            ret = true;
        }
    }

    return ret;
}

// =====================================================================================================================
// Checks that this user-data layout is compatible with the given one.  If they are compatible and identical, the given
// layout is left alone.  If they are compatible and not identical, the given layout is destroyed and re-created such
// that it represents the "union" of this layout and the original one.
// Warning: the owner of ppOther must guard this call with a mutex!
Result ComputeUserDataLayout::CombineWith(
    const Pal::Device&      device,
    ComputeUserDataLayout** ppOther
    ) const
{
    PAL_ASSERT((ppOther != nullptr) && (*ppOther != nullptr));

    Result result = Result::Success;

    const ComputeUserDataLayout& other = **ppOther;
    if ((m_hash != other.m_hash) ||
        (m_userDataLimit != other.m_userDataLimit))
    {
        UserDataReg map[MaxUserDataEntries] = { };
        for (uint32 i = 0;  i < other.m_numMapWords; ++i)
        {
            map[i].u32All = other.m_pMap[i];
        }

        ComputeUserDataLayoutCreateInfo createInfo { };
        createInfo.pMap       = map;
        createInfo.workgroup  = other.m_workgroup;
        createInfo.spillTable = other.m_spillTable;
        createInfo.meshTaskDispatchDims = other.m_taskDispatchDims;
        createInfo.meshTaskRingIndex    = other.m_meshTaskRingIndex;
        createInfo.taskDispatchIndex    = other.m_taskDispatchIndex;

        // The | operator is used here instead of || because each function call must happen and
        // || will short circuit. | will not short circuit.
        bool updated =
            CombineUserDataReg(&result, &createInfo.workgroup,  m_workgroup)  |
            CombineUserDataReg(&result, &createInfo.spillTable, m_spillTable) |
            CombineUserDataReg(&result, &createInfo.meshTaskDispatchDims, m_taskDispatchDims)  |
            CombineUserDataReg(&result, &createInfo.meshTaskRingIndex,    m_meshTaskRingIndex) |
            CombineUserDataReg(&result, &createInfo.taskDispatchIndex,    m_taskDispatchIndex);

        for (uint32 i = 0; (i < MaxUserDataEntries) && (result == Result::Success); ++i)
        {
            uint32 thisData = (i < m_numMapWords) ? m_pMap[i] : 0;
            updated |= CombineUserDataReg(&result, &map[i], UserDataReg{ .u32All = thisData });
            if (map[i].regOffset != 0)
            {
                createInfo.numMapWords = (i + 1);
            }
        }

        if (updated)
        {
            // As long as user-SGPR mappings between the two signatures are compatible, we can expand the region
            // of spilled user-data entries without trouble.
            createInfo.userDataLimit  = Max(m_userDataLimit,  other.m_userDataLimit);
            createInfo.spillThreshold = Min(m_spillThreshold, other.m_spillThreshold);

            void* pMem = PAL_MALLOC(Size(createInfo), device.GetPlatform(), SystemAllocType::AllocObject);
            if (pMem != nullptr)
            {
                (*ppOther)->Destroy();
                *ppOther = PAL_PLACEMENT_NEW(pMem) ComputeUserDataLayout(device, createInfo);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }
    } // ...if hashes don't match

    return result;
}

// =====================================================================================================================
template <bool PipelineSwitch>
uint32* ComputeUserDataLayout::CopyUserDataPairsToCmdSpace(
    const ComputeUserDataLayout* pPrevComputeUserDataLayout,
    const Pal::UserDataFlags&    dirty,
    const uint32*                pUserData,
    uint32*                      pCmdSpace
    ) const
{
    Pal::UserDataFlags localDirty;
    memcpy(&localDirty, &dirty, sizeof(localDirty));

    if (PipelineSwitch)
    {
        LayoutDelta delta;

        if (ComputeLayoutDelta(pPrevComputeUserDataLayout, &delta))
        {
            for (uint32 i = delta.firstStaleEntry;
                (i < delta.firstStaleEntry + delta.numStaleEntries) &&
                (i < m_numMapWords);
                i++)
            {
                const auto& userData = reinterpret_cast<const UserDataReg&>(m_pMap[i]);

                if (userData.regOffset != 0)
                {
                    pCmdSpace[0] = userData.regOffset;
                    pCmdSpace[1] = pUserData[i];
                    pCmdSpace += 2;

                    WideBitfieldClearBit(localDirty, i);
                }
            }
        }
    }

    WideBitIter<size_t, NumUserDataFlagsParts> validIter(localDirty);

    if ((WideBitfieldIsAnyBitSet(localDirty)) && (m_numMapWords > 0))
    {
        while (validIter.IsValid())
        {
            const uint32 index = validIter.Get();

            if (index < m_numMapWords)
            {
                const UserDataReg& userData = reinterpret_cast<const UserDataReg&>(m_pMap[index]);
                if (userData.regOffset != 0)
                {
                    pCmdSpace[0] = userData.regOffset;
                    pCmdSpace[1] = pUserData[index];
                    pCmdSpace += 2;
                }
            }

            validIter.Next();
        }
    }

    return pCmdSpace;
}

template uint32* ComputeUserDataLayout::CopyUserDataPairsToCmdSpace<true>(
    const ComputeUserDataLayout* pPrevComputeUserDataLayout,
    const Pal::UserDataFlags&    dirty,
    const uint32*                pUserData,
    uint32*                      pCmdSpace) const;
template uint32* ComputeUserDataLayout::CopyUserDataPairsToCmdSpace<false>(
    const ComputeUserDataLayout* pPrevComputeUserDataLayout,
    const Pal::UserDataFlags&    dirty,
    const uint32*                pUserData,
    uint32*                      pCmdSpace) const;

} // namespace Gfx12
} // namespace Pal
