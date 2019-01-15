/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/gpuMemory.h"

namespace Pal
{
namespace Gfx6
{

class CmdStream;
class Device;
class ShaderRing;

// Enumerates the types of Shader Rings available.
enum class ShaderRingType : uint32
{
    ComputeScratch = 0,                // Scratch Ring for compute pipelines
    SamplePos,                         // Sample position buffer
    GfxScratch,                        // Scratch Ring for graphics pipelines
    EsGs,                              // Ring for passing vertex data between the ES & GS stage
    GsVs,                              // Ring for passing vertex data between the GS & VS stage
    TfBuffer,                          // Tess-Factor Buffer
    OffChipLds,                        // Off-Chip Tessellation LDS buffers
    NumUniversal,                      // Number of Rings in a RingSet associated with a universal Queue
    NumCompute = (SamplePos + 1),      // Number of Rings in a RingSet associated with a compute Queue
};

// Enumerates the SRD's used in the per-RingSet internal table.
enum class ShaderRingSrd : uint32
{
    ScratchGraphics = 0,                // Graphics Scratch Ring
    ScratchCompute,                     // Compute Scratch Ring
    EsGsWrite,                          // ES/GS Ring Write Access
    EsGsRead,                           // ES/GS Ring Read Access
    GsVsWrite0,                         // GS/VS Ring Write Access (Offset 0)
    GsVsWrite1,                         // GS/VS Ring Write Access (Offset 1)
    GsVsWrite2,                         // GS/VS Ring Write Access (Offset 2)
    GsVsWrite3,                         // GS/VS Ring Write Access (Offset 3)
    GsVsRead,                           // GS/VS Ring Read Access
    TessFactorBuffer,                   // Tessellation Factor Buffer
    OffChipLdsBuffer,                   // Off-Chip Tessellation LDS buffer
    OffChipParamCache,                  // Off-Chip parameter cache, doing nothing but reserve SRD slot
    SamplePosBuffer,                    // Sample position buffer
    NumUniversal,                       // Number of Ring SRD's in a RingSet associated with a universal Queue
    NumCompute = (SamplePosBuffer + 1), // Number of Ring SRD's in a RingSet associated with a compute Queue
};

// Contains the largest required item-size for each Shader Ring. Note that there is one item size tracker for each ring
// in a Universal Queue's RingSet. This works because the Compute RingSet is a subset of the Universal RingSet.
struct ShaderRingItemSizes
{
    size_t itemSize[static_cast<size_t>(ShaderRingType::NumUniversal)];
    static_assert(ShaderRingType::NumUniversal >= ShaderRingType::NumCompute,
                  "The compute ring set must be a subset of the universal ring set.");
};

// =====================================================================================================================
// A ShaderRingSet object contains all of the shader Rings used by command buffers which run on a particular Queue.
// Additionally, each Ring Set also manages the PM4 image of commands which write the ring state to hardware.
class ShaderRingSet
{
public:
    virtual ~ShaderRingSet();

    virtual Result Init();
    virtual Result Validate(const ShaderRingItemSizes&  ringSizes,
                            const SamplePatternPalette& samplePatternPalette);

    // Writes the per-Ring-Set register state into the specified command stream.
    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;

    // Write affected registers not in the Rlc save/restore list.
    virtual uint32* WriteNonRlcRestoredRegs(CmdStream* pCmdStream, uint32* pCmdSpace) const = 0;

    size_t NumRings() const { return m_numRings; }

    size_t SrdTableSize() const { return (sizeof(BufferSrd) * m_numSrds); }
    size_t TotalMemSize() const { return SrdTableSize(); }

protected:
    ShaderRingSet(Device* pDevice, size_t numRings, size_t numSrds);

    Device*const  m_pDevice;
    const size_t  m_numRings;       // Number of shader rings contained in the set
    const size_t  m_numSrds;        // Number of SRD's in this set's table

    ShaderRing**  m_ppRings;
    BufferSrd*    m_pSrdTable;

    BoundGpuMemory  m_srdTableMem;

private:
    PAL_DISALLOW_DEFAULT_CTOR(ShaderRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderRingSet);
};

// Represents an "image" of the PM4 commands necessary to write a UniversalRingSet to hardware. The required register
// writes are grouped into sets based on sequential register addresses, so that we can minimize the amount of PM4 space
// needed by setting several reg's in each packet.
struct UniversalRingSetPm4Img
{
    PM4CMDEVENTWRITE              vsPartialFlush;
    PM4CMDEVENTWRITE              vgtFlush;

    PM4CMDSETDATA                 hdrEsGsGsVsRingSize;
    regVGT_ESGS_RING_SIZE         vgtEsGsRingSize;
    regVGT_GSVS_RING_SIZE         vgtGsVsRingSize;

    PM4CMDSETDATA                 hdrTfMemoryBase;
    regVGT_TF_MEMORY_BASE         vgtTfMemoryBase;

    PM4CMDSETDATA                 hdrTfRingSize;
    regVGT_TF_RING_SIZE           vgtTfRingSize;

    PM4CMDSETDATA                 hdrHsOffchipParam;
    regVGT_HS_OFFCHIP_PARAM       vgtHsOffchipParam;

    // Command space needed, in DWORDs.  This field must always be last in the structure to not interfere w/ the
    // actual commands contained within.
    size_t                        spaceNeeded;
};

// Represents an "image" of the PM4 commands necessary to write a UniversalRingSet to hardware. Only include affected
// registers not in Rlc save/restore list.
struct UniversalRingSetNonRlcRestoredPm4Img
{
    PM4CMDSETDATA                 hdrLsUserData;
    regSPI_SHADER_USER_DATA_LS_0  lsUserData0;

    PM4CMDSETDATA                 hdrHsUserData;
    regSPI_SHADER_USER_DATA_HS_0  hsUserData0;

    PM4CMDSETDATA                 hdrEsUserData;
    regSPI_SHADER_USER_DATA_ES_0  esUserData0;

    PM4CMDSETDATA                 hdrGsUserData;
    regSPI_SHADER_USER_DATA_GS_0  gsUserData0;

    PM4CMDSETDATA                 hdrVsUserData;
    regSPI_SHADER_USER_DATA_VS_0  vsUserData0;

    PM4CMDSETDATA                 hdrPsUserData;
    regSPI_SHADER_USER_DATA_PS_0  psUserData0;

    PM4CMDSETDATA                 hdrComputeUserData;
    regCOMPUTE_USER_DATA_0        computeUserData0;

    PM4CMDSETDATA                 hdrGfxScratchRingSize;
    regSPI_TMPRING_SIZE           gfxScratchRingSize;

    PM4CMDSETDATA                 hdrComputeScratchRingSize;
    regCOMPUTE_TMPRING_SIZE       computeScratchRingSize;
};

// =====================================================================================================================
// Implements a ShaderRingSet for a Universal Queue.
class UniversalRingSet : public ShaderRingSet
{
public:
    explicit UniversalRingSet(Device* pDevice);
    virtual ~UniversalRingSet() {}

    virtual Result Init() override;
    virtual Result Validate(const ShaderRingItemSizes&  ringSizes,
                            const SamplePatternPalette& samplePatternPalette) override;

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const override;
    virtual uint32* WriteNonRlcRestoredRegs(CmdStream* pCmdStream, uint32* pCmdSpace) const override;

private:
    void BuildPm4Headers();

    // Command images used to write this Ring-Set's state to hardware.
    UniversalRingSetPm4Img               m_pm4Commands;
    UniversalRingSetNonRlcRestoredPm4Img m_pm4NonRlcRestoredCommands;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalRingSet);
};

// Represents an "image" of the PM4 commands necessary to write a ComputeRingSet to hardware. The required register
// writes are grouped into sets based on sequential register addresses, so that we can minimize the amount of PM4 space
// needed by setting several reg's in each packet.
struct ComputeRingSetPm4Img
{
    PM4CMDSETDATA           hdrComputeUserData;
    regCOMPUTE_USER_DATA_0  computeUserData0;

    PM4CMDSETDATA           hdrComputeScratchRingSize;
    regCOMPUTE_TMPRING_SIZE computeScratchRingSize;
};

// =====================================================================================================================
// Implements a ShaderRingSet for a Compute-only Queue.
class ComputeRingSet : public ShaderRingSet
{
public:
    explicit ComputeRingSet(Device* pDevice);
    virtual ~ComputeRingSet() {}

    virtual Result Init() override;
    virtual Result Validate(const ShaderRingItemSizes&  ringSizes,
                            const SamplePatternPalette& samplePatternPalette) override;

    virtual uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const override;

    // This should be never called for ComputeRingSet.
    virtual uint32* WriteNonRlcRestoredRegs(CmdStream* pCmdStream, uint32* pCmdSpace) const override
        { PAL_NEVER_CALLED(); return pCmdSpace; }

private:
    void BuildPm4Headers();

    ComputeRingSetPm4Img m_pm4Commands; // Image of the PM4 commands used to write this Ring-Set's state to hardware.

    PAL_DISALLOW_DEFAULT_CTOR(ComputeRingSet);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeRingSet);
};

} // Gfx6
} // Pal
