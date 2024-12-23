/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9Chip.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// At any time it's either 3 Gfx (PS, GS, HS) stages or 1 Compute stage (CS).
constexpr uint32 EiV2MaxStages = 3;

// Possible VBTable SRD update slots per ExecuteIndirect_V2 PM.
constexpr uint32 EiV2SrdSlots = 32;

// Number of MemCopies the CP can support with 1 ExecuteIndirect_V2 PM4.
constexpr uint32 EiV2MemCopySlots = 8;

// Number of possible entries/MemCopies at one time is limited to 256 that is the API max userdata spilled.
constexpr uint32 EiV2LutLength = 256;

// Struct for RegPacked format.
union ExecuteIndirectV2Packed
{
    uint8  u8bitComponents[EiV2MaxStages]; // Only used for GraphicsUserData reg with 3 stages.
    uint16 u16bitComponents[2];
    uint32 u32All;
};

// Struct for Draw components.
struct EiV2Draw
{
    uint32 dataOffset;
    struct
    {
        uint32 startVertex  : 8;
        uint32 startInst    : 8;
        uint32 commandIndex : 8;
        uint32 reserved     : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// Struct for DrawIndexed components.
struct EiV2DrawIndexed
{
    uint32 dataOffset;
    struct
    {
        uint32 baseVertex   : 8;
        uint32 startInst    : 8;
        uint32 commandIndex : 8;
        uint32 reserved     : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// Struct for Dispatch components.
struct EiV2Dispatch
{
    uint32 dataOffset;
    struct
    {
        uint32 reserved     : 16;
        uint32 commandIndex : 16;
    } locData;
    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator;
};

// Struct for DispatchMesh components.
struct EiV2DispatchMesh
{
    uint32 dataOffset;
    struct
    {
        uint32 xyzDim       : 8;
        uint32 reserved1    : 8;
        uint32 commandIndex : 8;
        uint32 reserved2    : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// All EIV2 operations are 3 DWORDs.
constexpr uint32 EiV2OpDwSize = 3;
static_assert((((sizeof(EiV2Draw)         / sizeof(uint32)) == EiV2OpDwSize) &&
               ((sizeof(EiV2DrawIndexed)  / sizeof(uint32)) == EiV2OpDwSize) &&
               ((sizeof(EiV2Dispatch)     / sizeof(uint32)) == EiV2OpDwSize) &&
               ((sizeof(EiV2DispatchMesh) / sizeof(uint32)) == EiV2OpDwSize)),
               "EiOpDwSize does not match some of the Ei Ops struct's size");

// Only one of these operations is valid at a time and ExecuteIndirectV2 will be programmed just for that.
union ExecuteIndirectV2Op
{
    EiV2Draw         draw;
    EiV2DrawIndexed  drawIndexed;
    EiV2Dispatch     dispatch;
    EiV2DispatchMesh dispatchMesh;
};

// MemCopy struct for offset of where to copy and size.
struct DynamicMemCopyEntry
{
    uint16  argBufferOffset;
    uint16  size;
};

struct BuildSrd
{
    uint32 count;
    uint32 srcOffsets[EiV2SrdSlots];
    uint32 dstOffsets[EiV2SrdSlots];
};

struct CpMemCopy
{
    uint32 count;
    uint32 srcOffsets[EiV2MemCopySlots];
    uint32 dstOffsets[EiV2MemCopySlots];
    uint32 sizes[EiV2MemCopySlots];
};

// Helper struct to help the ExecuteIndirectV2 PM4 perform tasks relevant for performing an Operation. They end up
// being part of the PM4 either directly or at an offset as MetaData.
struct ExecuteIndirectV2MetaData
{
    // opType here represents PFP_EXECUTE_INDIRECT_V2_operation_enum/MEC_EXECUTE_INDIRECT_V2_operation_enum
    uint32    opType;
    uint32    userDataDwCount;
    bool      commandIndexEnable;
    uint16    incConstReg[EiV2MaxStages];
    uint32    incConstRegCount;
    bool      fetchIndexAttributes;
    bool      vertexBoundsCheckEnable;
    uint32    indexAttributesOffset;
    uint32    userDataOffset;
    uint32    xyzDimLoc;
    uint32    userDataScatterMode;
    bool      threadTraceEnable;
    uint32    stageUsageCount;
    uint32    userData[NumUserDataRegisters * EiV2MaxStages];
    BuildSrd  buildSrd;
    CpMemCopy initMemCopy;
    CpMemCopy updateMemCopy;
};

// This class maintains the MetaData struct and other helper data variables and functions required for building the
// ExecuteIndirectV2 PM4.
class ExecuteIndirectV2Meta
{
public:

    ExecuteIndirectV2Meta();
    ~ExecuteIndirectV2Meta() { };

    // This helper function for writing UserData Entries into Registers, VBTable SRD and the MemCopy structs which help
    // the CP copy SpilledUserData in 'RegPacked' format. This is what the pOut array looks looks like for relevant
    // values of bitsPerComponent and countInDwords: here { } represents a uint32 packed value
    // With updateMemCopyCount == 2 and bitsPerComponent == 16
    // pOut = [ {pIn1[1] | pIn1[0]},
    //          {pIn2[1] | pIn2[0]},
    //          {pIn3[1] | pIn3[0]} ]
    // With updateMemCopyCount == 3 and bitsPerComponent == 16
    // pOut = [ {pIn1[1] | pIn1[0]},
    //          {pIn2[1] | pIn2[0]},
    //          {pIn3[1] | pIn3[0]},
    //          {0       | pIn1[2]},
    //          {0       | pIn2[2]},
    //          {0       | pIn3[2]} ]
    // With updateMemCopyCount == 4 and bitsPerComponent == 8
    // pOut = [ {pIn1[3] | pIn1[2] | pIn1[1] | pIn1[0]},
    //          {pIn2[3] | pIn2[2] | pIn2[1] | pIn2[0]},
    //          {pIn3[3] | pIn3[2] | pIn3[1] | pIn3[0]}  ]
    static uint32 ExecuteIndirectV2WritePacked(uint32* pOut,
                                               const uint32  bitsPerComponent,
                                               const uint32  countInDwords,
                                               const uint32* pIn1,
                                               const uint32* pIn2 = nullptr,
                                               const uint32* pIn3 = nullptr);

    // Initialize the Look-up Table for all possible MemCpy's for the spilled UserData entries. Slots between exStart
    // and exEnd are (typically) not suposed to be touched.
    inline void InitLut()
    {
        memset(m_computeMemCopiesLutFlags, 0, sizeof(m_computeMemCopiesLutFlags));
    }

    inline void SetMemCpyRange(uint32 exStart, uint32 exEnd)
    {
        m_excludeStart = exStart;
        m_excludeEnd   = exEnd;
    }

    // Unset the flag/bit corresponding to idx in the Look-up Table.
    inline void ClearLut(uint32 idx)
    {
        WideBitfieldClearBit(m_computeMemCopiesLutFlags, idx);
    }

    // Add info for the MemCopy in the Look-up Table and set the corresponding flag/bit at idx.
    inline void SetLut(uint32 idx, uint32 argBufferDwIdx, uint32 size)
    {
        m_computeMemCopiesLut[idx].argBufferOffset = static_cast<uint16>(argBufferDwIdx);
        m_computeMemCopiesLut[idx].size            = static_cast<uint16>(size);

        WideBitfieldSetBit(m_computeMemCopiesLutFlags, idx);
    }

    // Helper to check for next MemCopy to be done. Also, clears that MemCopy from the Look-up table.
    bool NextUpdate(const uint32 vbSpillTableWatermark, uint32* pNextIdx, DynamicMemCopyEntry* pEntry);

    // CP performs a MemCpy as part of the ExecuteIndirectV2 Packet function for the SpilledUserData. This computes
    // what to copy. The vbSpillTableWatermark here refers to the last entry to be updated in the VBTable+UserDataSpill
    // buffer. InitMemCpy and UpdateMemCpy structs are both required for the CP to do its job.
    void ComputeMemCopyStructures(const uint32 vbSpillTableWatermark, uint32* pInitCount, uint32* pUpdateCount);

    // If in dynamicSpillMode, CP will allocate and use global spilled table intead of local spilled table. In this
    // case, if there are VB SRDs that are updated from CPU side, need issue InitMemCopy to copy it from local spilled
    // table to global spilled table. vbSlotMask is the VB slots that need the copy.
    void ComputeVbSrdInitMemCopy(uint32 vbSlotMask);

    // Helper for InitMemCopy
    void ProcessInitMemCopy(const uint32 vbSpillTableWatermark, uint32* pInitCount, uint32 currentIdx, uint32 nextIdx);

    //Helper for UpdateMemCopy
    void ProcessUpdateMemCopy(
        const uint32         vbSpillTableWatermark,
        uint32*              pUpdateCount,
        uint32*              pCurrentIdx,
        uint32*              pNextIdx,
        DynamicMemCopyEntry* pEntry,
        bool*                pValidUpdate);

    // Helper for CommandIndex
    uint16 ProcessCommandIndex(const uint16 drawIndexRegAddr, const bool useConstantDrawIndex, const bool useEightBitMask);

    ExecuteIndirectV2MetaData* GetMetaData() { return &m_metaData; }

private:
    ExecuteIndirectV2MetaData m_metaData;

    // We set up a Look-up Table to help with updating the data in the buffer for Spilled UserData in this
    // ExecuteIndirect Op.
    DynamicMemCopyEntry m_computeMemCopiesLut[EiV2LutLength];

    // A bit for each of the 256 (EiV2LutLength) DynamicMemCopyEntrys
    uint64 m_computeMemCopiesLutFlags[EiV2LutLength / (sizeof(uint64) << 3)];

    // ExcludeStart and ExcludeEnd are the part of the VB+SpillBuffer which would contain unchanging (for the
    // UpdateMemCopy function) VBTable and register mapped UserDataEntries.
    uint32 m_excludeStart;
    uint32 m_excludeEnd;
};

} // Gfx9
} // Pal
