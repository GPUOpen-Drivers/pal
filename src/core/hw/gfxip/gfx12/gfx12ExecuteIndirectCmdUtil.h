/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// At any time it's either 3 Gfx (PS, GS, HS) stages or 1 Compute stage (CS).
constexpr uint32 EiMaxStages = 3;

// Possible VBTable SRD update slots per ExecuteIndirect_V2 PM.
constexpr uint32 EiSrdSlots = 32;

// Number of MemCopies the CP can support with 1 ExecuteIndirect_V2 PM4.
constexpr uint32 EiMemCopySlots = 8;

// Number of possible entries/MemCopies at one time is limited to 256 that is the API max userdata spilled.
constexpr uint32 EiLutLength = 256;

// PFP version after which SetBase of ExecuteIndirectV2 can be saved and restored, therefore we can enable MCBP.
constexpr uint32 EiV2McbpFixPfpVersion = 2550;

// PFP version after which linear dispatch optimization can be enabled.
constexpr uint32 EiV2LinearDispatchFixPfpVersion = 2710;

// PFP version after which offset mode vertex binding can be enabled.
constexpr uint32 EiV2OffsetModeVertexBindingFixPfpVersion = 2720;

// PFP version after which HS HW stage register support can be enabled.
constexpr uint32 EiV2HsHwRegFixPfpVersion = 2740;

// PFP version after which WorkGroup register supoort can be enabled.
constexpr uint32 EiV2WorkGroupRegFixPfpVersion = 2810;

// As the EI V2 PM4 is defined on both PFP/ME and MEC this enum is for indicating the engine.
enum EiEngine : uint8
{
    EiEngineGfx,
    EiEngineAce,
    EiEngineCount
};

// Struct for RegPacked format.
union ExecuteIndirectPacked
{
    uint8  u8bitComponents[EiMaxStages]; // Only used for GraphicsUserData reg with 3 stages.
    uint16 u16bitComponents[2];
    uint32 u32All;
};

// Struct to help populate COMPUTE_DISPATCH_INITIATOR register.
struct EiDispatchOptions
{
    uint8 enable2dInterleave    : 1;
    uint8 pingPongEnable        : 1;
    uint8 usesDispatchTunneling : 1;
    uint8 isLinearDispatch      : 1;
    uint8 isWave32              : 1;
    uint8 reserved              : 3;
};

// Struct for UserDataRegs (offsets to regs) marked as used in this EI V2 submission.
// They are at an offset to PERSISTENT_SPACE_START.
struct EiUserDataRegs
{
    // These mark reg offsets for the GFX EI V2 PM4
    // Translation to mmSPI_SHADER_USER_DATA_HS_0-based offset is required only if HS HW stage is enabled
    // Otherwise, PERSISTENT_SPACE_START-based offset is required here
    uint16 instOffsetReg;       // GS or HS HW stage
    uint16 vtxOffsetReg;        // GS or HS HW stage
    uint16 vtxTableReg;         // GS or HS HW stage
    uint8  drawIndexReg;        // GS or HS HW stage (To do item)
    uint8  meshDispatchDimsReg; // GS HW stage only
    uint8  meshRingIndexReg;    // GS HW stage only
    uint16 numWorkGroupReg;     // CS HW stage only
    // These mark reg offsets for the ACE EI V2 PM4
    // Translation to COMPUTE_USER_DATA_0-based offset is required before filling into EiDispatchTaskMesh
    uint16 aceMeshTaskRingIndexReg;
    uint16 aceTaskDispatchDimsReg;
    uint16 aceTaskDispatchIndexReg;
};

// Struct for Draw components.
struct EiDraw
{
    uint32 dataOffset;
    struct
    {
        uint32 startVertex  : 8;
        uint32 startInst    : 8;
        uint32 commandIndex : 8;
        uint32 reserved1    : 6;
        uint32 drawRegsInHs : 1;
        uint32 reserved2    : 1;
    } locData;
    VGT_DRAW_INITIATOR drawInitiator;
};

// Struct for DrawIndexed components.
struct EiDrawIndexed
{
    uint32 dataOffset;
    struct
    {
        uint32 startVertex  : 8;
        uint32 startInst    : 8;
        uint32 commandIndex : 8;
        uint32 reserved1    : 6;
        uint32 drawRegsInHs : 1;
        uint32 reserved2    : 1;
    } locData;
    VGT_DRAW_INITIATOR drawInitiator;
};

// Struct for Dispatch components.
struct EiDispatch
{
    uint32 dataOffset;
    struct
    {
        uint32 numWorkGroup       : 10;
        uint32 numWorkGroupEnable : 1;
        uint32 reserved           : 5;
        uint32 commandIndex       : 16;
    } locData;
    COMPUTE_DISPATCH_INITIATOR dispatchInitiator;
};

// Struct for DispatchTaskMesh components.
struct EiDispatchTaskMesh
{
    uint32 dataOffset;
    struct
    {
        uint32 xyzDim               : 8;
        uint32 ringEntry            : 8;
        uint32 commandIndex         : 8;
        uint32 xyzDimEnable         : 1;
        uint32 linearDispatchEnable : 1;
        uint32 reserved             : 6;
    } locData;

    union
    {
        COMPUTE_DISPATCH_INITIATOR dispatchInitiator; // For task shader on ACE queue.
        VGT_DRAW_INITIATOR         drawInitiator;     // For mesh shader on universal queue.
    };
};

// All EIV2 operations are 3 DWORDs.
constexpr uint32 EiOpDwSize = 3;
static_assert((((sizeof(EiDraw) / sizeof(uint32))             == EiOpDwSize) &&
               ((sizeof(EiDrawIndexed) / sizeof(uint32))      == EiOpDwSize) &&
               ((sizeof(EiDispatch) / sizeof(uint32))         == EiOpDwSize) &&
               ((sizeof(EiDispatchTaskMesh) / sizeof(uint32)) == EiOpDwSize)),
              "EiOpDwSize does not match some of the Ei Ops struct's size");

// Only one of these operations is valid at a time and ExecuteIndirectV2 will be programmed just for that.
union ExecuteIndirectOp
{
    EiDraw             draw;
    EiDrawIndexed      drawIndexed;
    EiDispatch         dispatch;
    EiDispatchTaskMesh dispatchTaskMesh;
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
    uint32 srcOffsets[EiSrdSlots];
    uint32 dstOffsets[EiSrdSlots];
};

struct CpMemCopy
{
    uint32 count;
    uint32 srcOffsets[EiMemCopySlots];
    uint32 dstOffsets[EiMemCopySlots];
    uint32 sizes[EiMemCopySlots];
};

// Helper struct to help the ExecuteIndirectV2 PM4 perform tasks relevant for performing an Operation. They end up
// being part of the PM4 either directly or at an offset as MetaData.
struct ExecuteIndirectMetaData
{
    // opType here represents PFP_EXECUTE_INDIRECT_V2_operation_enum/MEC_EXECUTE_INDIRECT_V2_operation_enum
    uint32    opType;
    uint32    userDataDwCount;
    bool      commandIndexEnable;
    uint16    incConstReg[EiMaxStages];
    uint32    incConstRegCount;
    bool      fetchIndexAttributes;
    bool      vertexOffsetModeEnable;
    bool      vertexBoundsCheckEnable;
    uint32    indexAttributesOffset;
    uint32    userDataOffset;
    uint32    xyzDimLoc;
    uint32    userDataScatterMode;
    bool      threadTraceEnable;
    uint32    stageUsageCount;
    uint32    userData[NumUserDataRegisters * EiMaxStages];
    BuildSrd  buildSrd;
    CpMemCopy initMemCopy;
    CpMemCopy updateMemCopy;
};

// This class maintains the MetaData struct and other helper data variables and functions required for building the
// ExecuteIndirectV2 PM4.
class ExecuteIndirectMeta
{
public:

    ExecuteIndirectMeta();
    ~ExecuteIndirectMeta() { };

    // This helper function for writing UserData Entries into Registers, VBTable SRD and the MemCopy structs which help
    // the CP copy SpilledUserData in 'RegPacked' format. This is what the pOut array looks looks like for relevant
    // values of bitsPerComponent and countInDwords: here { } represents a uint32 packed value
    // With countInDwords == 2 and bitsPerComponent == 16
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
    // Returns count of dwords appended to the base PM4 for the InitMemCpy, UpdateMemCpy, BuildSRD and UserData
    // read/copy Op.
    static uint32 ExecuteIndirectWritePacked(uint32*       pOut,
                                             const uint32  bitsPerComponent,
                                             const uint32  countInDwords,
                                             const uint32* pIn1,
                                             const uint32* pIn2 = nullptr,
                                             const uint32* pIn3 = nullptr);

    // There was an optimization for how the UserDataRegisters could be read in the MEC as there is a guarantee of only
    // 1 shader stage i.e. CS. So we use this instead of ExecuteIndirectWritePacked function for UserDataOp for Compute
    // CmdBuffers. This effectively reduces the number of dwords to be appended to the main PM4 if we have a long
    // contiguous range of registers.
    // Example output for this function, if we have CS UserDataReg in pIn -> [0x244,0x245,0x246,0x250,0x251,0x254] then
    // pOut -> [0x02440003, 0x02500002, 0x02540001]
    // Returns count of dwords appended to the base PM4 for the MEC UserData read/copy Op.
    static uint32 AppendUserDataMec(uint32*       pPackedUserData,
                                    const uint32  userDataCount,
                                    const uint32* pUserData);

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

    //Helper for InitMemCopy
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
    uint16 ProcessCommandIndex(const uint8 drawIndexRegOffset, const bool useConstantDrawIndex, const bool useEightBitMask);

    ExecuteIndirectMetaData* GetMetaData() { return &m_metaData; }
    ExecuteIndirectOp*       GetOp() {return &m_op;}

private:
    ExecuteIndirectMetaData m_metaData;
    ExecuteIndirectOp       m_op;

    // We set up a Look-up Table to help with updating the data in the buffer for Spilled UserData in this
    // ExecuteIndirect Op.
    DynamicMemCopyEntry m_computeMemCopiesLut[EiLutLength];

    // A bit for each of the 256 (EiLutLength) DynamicMemCopyEntrys
    uint64 m_computeMemCopiesLutFlags[EiLutLength / (sizeof(uint64) << 3)];

    // ExcludeStart and ExcludeEnd are the part of the VB+SpillBuffer which would contain unchanging (for the
    // UpdateMemCopy function) VBTable and register mapped UserDataEntries. This range is inclusive and ExcludeEnd
    // marks the last unchanging entry.
    uint32 m_excludeStart;
    uint32 m_excludeEnd;
};

} // Gfx12
} // Pal
