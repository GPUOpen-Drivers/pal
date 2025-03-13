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

#include "core/gpuMemory.h"
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"
#include "palDeque.h"

namespace Pal
{

class Platform;
class SubmissionContext;

namespace Gfx12
{

class CmdStream;
class Device;
class ShaderRing;
struct ShaderRingItemSizes;

// Enumerates the SRD's used in the per-RingSet internal table.
enum class ShaderRingSrd : uint32
{
    ScratchGraphics = 0, // Graphics Scratch Ring
    ScratchCompute,      // Compute Scratch Ring
    Reserved2,         // Reserved for future use.
    Reserved3,         // Reserved for future use.
    Reserved4,         // Reserved for future use.
    Reserved5,         // Reserved for future use.
    Reserved6,         // Reserved for future use.
    Reserved7,         // Reserved for future use.
    Reserved8,         // Reserved for future use.
    TfBuffer,          // Tessellation Factor Buffer
    OffChipLds,        // Off-chip Tessellation LDS buffer
    VertexAttributes,  // Ring for passing vertex and primitive attributes from the HW GS to the PS
    SamplePosBuffer,   // Sample position palette constant buffer
    PayloadDataRing,   // Task -> GFX payload data.
    DrawDataRing,      // IndirectDraw parameters from task shader.
    MeshScratch,       // Mesh shader scratch ring, accessible by whole threadgroup.
    Reserved16,        // Reserved for future use.
    NumUniversal,      // Number of Ring SRD's in a RingSet associated with a universal Queue
    NumCompute = (SamplePosBuffer + 1), // Number of Ring SRD's in a RingSet associated with a compute Queue
};

// Struct to track shader ring memory to be defer-freed.
struct ShaderRingMemory
{
    GpuMemory*  pGpuMemory;
    gpusize     offset;
    uint64      timestamp;   // last submitted timestamp value
};

typedef Util::Deque<ShaderRingMemory, Platform> ShaderRingMemDeque;

// =====================================================================================================================
// A ShaderRingSet object contains all of the shader Rings used by command buffers which run on a particular Queue.
// Additionally, each Ring Set also manages the PM4 image of commands which write the ring state to hardware.
class ShaderRingSet
{
public:
    virtual ~ShaderRingSet();

    virtual Result Init();
    Result Validate(
        const ShaderRingItemSizes&  ringSizes,
        const SamplePatternPalette& samplePatternPalette,
        const uint64                lastTimeStamp,
        uint32*                     pReallocatedRings);

    // Writes the per-Ring-Set register state into the specified command stream.
    virtual uint32* WriteCommands(uint32* pCmdSpace) const = 0;

    const ShaderRing* const* GetRings() const { return m_ppRings; }
    size_t NumRings() const { return m_numRings; }

    size_t SrdTableSize() const { return (sizeof(sq_buf_rsrc_t) * m_numSrds); }
    size_t TotalMemSize() const { return SrdTableSize(); }

    void ClearDeferredFreeMemory(SubmissionContext* pSubmissionCtx);
    Result UpdateSrdTable(bool deferFreeSrdTable, uint64 lastTimestamp);
    void   CopySrdTableEntry(ShaderRingSrd entry, sq_buf_rsrc_t* pSrdTable);

protected:
    ShaderRingSet(Device* pDevice, size_t numRings, size_t numSrds, bool tmzEnabled);

    Result AllocateSrdTableMem();

    Device*const      m_pDevice;
    const size_t      m_numRings;       // Number of shader rings contained in the set
    const size_t      m_numSrds;        // Number of SRDs in this set's table
    const bool        m_tmzEnabled;
    ShaderRing**      m_ppRings;
    sq_buf_rsrc_t*    m_pSrdTable;
    BoundGpuMemory    m_srdTableMem;

    ShaderRingMemDeque m_deferredFreeMemDeque;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ShaderRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderRingSet);
};

static constexpr uint32 ComputeRingSetRegs[] =
{
    mmCOMPUTE_USER_DATA_0,
    mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO,
    mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI,
    mmCOMPUTE_TMPRING_SIZE,
};
using CsRingSet = RegPairHandler<decltype(ComputeRingSetRegs), ComputeRingSetRegs>;
static_assert(CsRingSet::Size() == CsRingSet::NumSh(), "Non-SH registers in ComputeRingSet!");

// =====================================================================================================================
// Implements a ShaderRingSet for a Compute-only Queue.
class ComputeRingSet final : public ShaderRingSet
{
public:
    explicit ComputeRingSet(
        Device* pDevice,
        bool    isTmz,
        size_t  numSrds = size_t(ShaderRingSrd::NumCompute));
    virtual ~ComputeRingSet() {}

    virtual Result Init() override;
    Result Validate(
        const ShaderRingItemSizes&  ringSizes,
        const SamplePatternPalette& samplePatternPalette,
        uint64                      lastTimeStamp);

    virtual uint32* WriteCommands(uint32* pCmdSpace) const override;

private:
    RegisterValuePair m_csRing[CsRingSet::Size()];

    PAL_DISALLOW_DEFAULT_CTOR(ComputeRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeRingSet);
};

// =====================================================================================================================
// Implements a ShaderRingSet for a Universal Queue.
class UniversalRingSet final : public ShaderRingSet
{
public:
    explicit UniversalRingSet(Device* pDevice, bool tmzEnabled);
    virtual ~UniversalRingSet();

    virtual Result Init() override;
    Result Validate(
        const ShaderRingItemSizes&  ringSizes,
        const SamplePatternPalette& samplePatternPalette,
        uint64                      lastTimeStamp,
        bool                        hasAce);

    virtual uint32* WriteCommands(uint32* pCmdSpace) const override;

    uint32* WriteComputeCommands(uint32* pCmdSpace) const;

    bool HasAceRingSet() const { return (m_pAceRingSet != nullptr); }

    ComputeRingSet* GetAceRingSet() const { return m_pAceRingSet; }

private:
    static constexpr uint32 GraphicsRingSetRegs[] =
    {
        // SH
        mmSPI_SHADER_USER_DATA_HS_0,
        mmSPI_SHADER_USER_DATA_GS_0,
        mmSPI_SHADER_USER_DATA_PS_0,

        // Context
        mmSPI_GFX_SCRATCH_BASE_LO,
        mmSPI_GFX_SCRATCH_BASE_HI,
        mmSPI_TMPRING_SIZE,

        // UConfig
        mmSPI_ATTRIBUTE_RING_BASE,
        mmSPI_ATTRIBUTE_RING_SIZE,
        mmVGT_TF_MEMORY_BASE,
        mmVGT_TF_MEMORY_BASE_HI,
        mmVGT_TF_RING_SIZE,
        mmVGT_HS_OFFCHIP_PARAM,
        mmGE_PRIM_RING_BASE,
        mmGE_PRIM_RING_SIZE,
        mmGE_POS_RING_BASE,
        mmGE_POS_RING_SIZE,
    };
    using GfxRingSet = RegPairHandler<decltype(GraphicsRingSetRegs), GraphicsRingSetRegs>;

    RegisterValuePair m_gfxRing[GfxRingSet::Size()];
    RegisterValuePair m_csRing[CsRingSet::Size()];
    ComputeRingSet*   m_pAceRingSet;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalRingSet);
};

} // namespace Gfx12
} // namespace Pal
