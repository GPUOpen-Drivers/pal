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
static constexpr uint32 EIV2MaxStages = 3;

// Possible VBTable SRD update slots per ExecuteIndirect_V2 PM.
static constexpr uint32 EIV2SrdSlots = 32;

// Number of MemCopies the CP can support with 1 ExecuteIndirect_V2 PM4.
static constexpr uint32 EIV2InitMemCopySlots   = 8;
static constexpr uint32 EIV2UpdateMemCopySlots = 8;

// Gfx Regs require 8-bits while Cs (Compute) Regs require 16-bits.
constexpr uint32 BitsGraphics = 8;
constexpr uint32 BitsCompute  = 16;

// To be used in ExecuteIndirectV2Packed::pInputs for writing UserData Entries in Packed format. This was chosen based
// on what it is used for and the most number of input was 3 for all cases for eg. for Gfx UserData it represents
// the at most 3 active supported HwShaderStgs at 1 point. Compute will require only 1 input for CS.
constexpr uint32 EiV2NumInputsWritePacked = 3;

// Number of possible entries/MemCopies at one time is limited to 256 that is the API max userdata spilled.
static constexpr uint32 EiV2LutLength = 256;

// Struct for RegPacked format.
union ExecuteIndirectV2Packed
{
    uint8  graphicsRegs[4];
    uint16 computeRegs[2];
    uint32 u32All;
};

// Struct for Draw components.
struct EIV2Draw
{
    uint32 dataOffset;
    struct
    {
        uint32 startVertexLoc  : 8;
        uint32 startInstLoc    : 8;
        uint32 commandIndexLoc : 8;
        uint32 reserved        : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// Struct for DrawIndexed components.
struct EIV2DrawIndexed
{
    uint32 dataOffset;
    struct
    {
        uint32 baseVertexLoc   : 8;
        uint32 startInstLoc    : 8;
        uint32 commandIndexLoc : 8;
        uint32 reserved        : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// Struct for Dispatch components.
struct EIV2Dispatch
{
    uint32 dataOffset;
    struct
    {
        uint32 reserved        : 16;
        uint32 commandIndexLoc : 16;
    } locData;
    regCOMPUTE_DISPATCH_INITIATOR dispatchInitiator;
};

// Struct for DispatchMesh components.
struct EIV2DispatchMesh
{
    uint32 dataOffset;
    struct
    {
        uint32 xyzDimLoc       : 8;
        uint32 reserved1       : 8;
        uint32 commandIndexLoc : 8;
        uint32 reserved2       : 8;
    } locData;
    regVGT_DRAW_INITIATOR drawInitiator;
};

// Only one of these operations is valid at a time and ExecuteIndirectV2 will be programmed just for that.
union ExecuteIndirectV2Op
{
    EIV2Draw         draw;
    EIV2DrawIndexed  drawIndexed;
    EIV2Dispatch     dispatch;
    EIV2DispatchMesh dispatchMesh;
};

// MemCopy struct for offset of where to copy and size.
struct DynamicMemCopyEntry
{
    uint16  argBufferOffset;
    uint16  size;
};

// Helper struct to help the ExecuteIndirectV2 PM4 perform tasks relevant for performing an Operation. They end up
// being part of the PM4 either directly or at an offset as MetaData.
struct ExecuteIndirectV2MetaData
{
    PFP_EXECUTE_INDIRECT_V2_operation_enum opType;
    uint32 initMemCopyCount;
    uint32 updateMemCopyCount;
    uint32 buildSrdCount;
    uint32 userDataDwCount;
    bool   commandIndexEnable;
    bool   fetchIndexAttributes;
    bool   vertexBoundsCheckEnable;
    uint32 indexAttributesOffset;
    uint32 userDataOffset;
    uint32 xyzDimLoc;
    PFP_EXECUTE_INDIRECT_V2_REG_SCATTER_MODE_function_enum userDataScatterMode;
    bool   threadTraceEnable;
    uint32 stageUsageCount;
    uint32 userData[NumUserDataRegisters * EIV2MaxStages];
    uint32 buildSrdSrcOffsets[EIV2SrdSlots];
    uint32 buildSrdDstOffsets[EIV2SrdSlots];
    uint32 copyInitSrcOffsets[EIV2InitMemCopySlots];
    uint32 copyInitDstOffsets[EIV2InitMemCopySlots];
    uint32 copyInitSizes[EIV2InitMemCopySlots];
    uint32 copyUpdateSrcOffsets[EIV2UpdateMemCopySlots];
    uint32 copyUpdateDstOffsets[EIV2UpdateMemCopySlots];
    uint32 copyUpdateSizes[EIV2UpdateMemCopySlots];
};

// This class maintains the MetaData struct and other helper data variables and functions required for building the
// ExecuteIndirectV2 PM4.
class ExecuteIndirectV2Meta
{
public:

    ExecuteIndirectV2Meta()
        :
        m_metaData{},
        m_excludeStart(0),
        m_excludeEnd(0),
        m_computeMemCopiesLut{ 0 },
        m_computeMemCopiesLutFlags{ 0 }
    {
    };

    ~ExecuteIndirectV2Meta() { };

    // This helper function for writing UserData Entries into Registers, VBTable SRD and the MemCopy structs which help
    // the CP copy SpilledUserData in 'RegPacked' format.
    static uint32 ExecuteIndirectV2WritePacked(uint32* pOut,
                                               const uint32  bitsPerComponent,
                                               const uint32  countInDwords,
                                               const uint32* pIn1,
                                               const uint32* pIn2 = nullptr,
                                               const uint32* pIn3 = nullptr);

    // Initialize the Look-up Table for all possible MemCpy's for the spilled UserData entries. Slots between exStart
    // and exEnd are (typically) not suposed to be touched.
    inline void InitLut(uint32 exStart, uint32 exEnd)
    {
        memset(m_computeMemCopiesLutFlags, 0, sizeof(m_computeMemCopiesLutFlags));
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
    bool NextUpdate(const uint32 vbSpillTableWatermark, uint32* nextIdx, DynamicMemCopyEntry* entry);

    // CP performs a MemCpy as part of the ExecuteIndirectV2 Packet function for the SpilledUserData. This computes
    // what to copy. The vbSpillTableWatermark here refers to the last entry to be updated in the VBTable+UserDataSpill
    // buffer. InitMemCpy and UpdateMemCpy structs are both required for the CP to do its job.
    void ComputeMemCopyStructures(const uint32 vbSpillTableWatermark, uint32* pInitCount, uint32* pUpdateCount);

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
