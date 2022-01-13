/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

class Device;

// =====================================================================================================================
// Base class for all shader-ring types. Provides defaults for computing the ring video memory size as well as handling
// the memory allocation.
class ShaderRing
{
public:
    virtual ~ShaderRing();

    Result Validate(size_t itemSize, ShaderRingMemory* pDeferredMem);

    bool IsMemoryValid() const { return m_ringMem.IsBound(); }

    gpusize GpuVirtAddr() const { return m_ringMem.GpuVirtAddr(); }

    gpusize MemorySizeBytes() const { return m_allocSize; }
    gpusize MemorySizeDwords() const { return (m_allocSize / sizeof(uint32)); }

    // Returns the shader Ring's maximum supported Item Size. The units and meaning of this value depends on which ring
    // you are referring to.
    size_t ItemSizeMax() const { return m_itemSizeMax; }

protected:
    ShaderRing(Device* pDevice, BufferSrd* pSrdTable, bool isTmz, ShaderRingType type);

    virtual gpusize ComputeAllocationSize() const;

    // Informs the Shader Ring to update its SRD's.
    virtual void UpdateSrds() const = 0;

    Device*const     m_pDevice;
    BufferSrd*const  m_pSrdTable;   // Pointer to the parent ring-set's SRD table
    const bool       m_tmzEnabled;  // Indicate this shader ring is tmz or not.

    BoundGpuMemory  m_ringMem;     // Shader-ring video memory allocation

    gpusize  m_allocSize;   // Current "real" video memory size (in bytes)
    size_t   m_numMaxWaves; // Max. number of waves allowed to execute in parallel
    size_t   m_itemSizeMax; // Highest item size this Ring has needed so far

    const ShaderRingType m_ringType;
    const GfxIpLevel     m_gfxLevel;

private:
    Result AllocateVideoMemory(gpusize memorySizeBytes, ShaderRingMemory* pDeferredMem);

    PAL_DISALLOW_DEFAULT_CTOR(ShaderRing);
    PAL_DISALLOW_COPY_AND_ASSIGN(ShaderRing);
};

// =====================================================================================================================
// Implements shader-ring functionality specific for shader scratch memory.
class ScratchRing final : public ShaderRing
{
public:
    ScratchRing(Device* pDevice, BufferSrd* pSrdTable, PM4ShaderType shaderType, bool isTmz);
    virtual ~ScratchRing() {}

    size_t CalculateWaves() const;
    size_t CalculateWaveSize() const;

protected:
    virtual gpusize ComputeAllocationSize() const override;
    virtual void UpdateSrds() const override;

private:
    const PM4ShaderType m_shaderType;
    uint32              m_numTotalCus;

    PAL_DISALLOW_DEFAULT_CTOR(ScratchRing);
    PAL_DISALLOW_COPY_AND_ASSIGN(ScratchRing);
};

// =====================================================================================================================
// Implements shader-ring functionality specific to the ES/GS shader ring required to support normal (i.e. off-chip) GS.
class EsGsRing final : public ShaderRing
{
public:
    EsGsRing(Device* pDevice, BufferSrd* pSrdTable, bool isTmz);
    virtual ~EsGsRing() {}

protected:
    virtual void UpdateSrds() const override;

private:
    // Total number of SRD's referenced by the ES/GS ring: one for write, one for read.
    static constexpr size_t TotalSrds = 2;

    PAL_DISALLOW_DEFAULT_CTOR(EsGsRing);
    PAL_DISALLOW_COPY_AND_ASSIGN(EsGsRing);
};

// =====================================================================================================================
// Implements shader-ring functionality specific to the GS/VS shader ring required to support normal (i.e. off-chip) GS.
class GsVsRing final : public ShaderRing
{
public:
    GsVsRing(Device* pDevice, BufferSrd* pSrdTable, bool isTmz);
    virtual ~GsVsRing() {}

protected:
    virtual void UpdateSrds() const override;

private:
    // Number of SRD's written to by the GS/VS ring.
    static constexpr size_t WriteSrds = 4;
    // Total number of SRD's referenced by the GS/VS ring: four for write, one for read.
    static constexpr size_t TotalSrds = (WriteSrds + 1);

    // Fixed number of records for the GS/VS write SRD's:
    static constexpr uint32 NumRecordsWrite = 64;

    PAL_DISALLOW_DEFAULT_CTOR(GsVsRing);
    PAL_DISALLOW_COPY_AND_ASSIGN(GsVsRing);
};

// =====================================================================================================================
// Implements shader-ring functionality specific to the Tess-Factor Buffer required to support tessellation.
class TessFactorBuffer final : public ShaderRing
{
public:
    TessFactorBuffer(Device* pDevice, BufferSrd* pSrdTable, bool isTmz);
    virtual ~TessFactorBuffer() {}

protected:
    virtual gpusize ComputeAllocationSize() const override;
    virtual void UpdateSrds() const override;

private:
    PAL_DISALLOW_DEFAULT_CTOR(TessFactorBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(TessFactorBuffer);
};

// =====================================================================================================================
// Implements shader-ring functionality specific to the Offchip LDS Buffers required for offchip tessellation.
class OffchipLdsBuffer final : public ShaderRing
{
public:
    OffchipLdsBuffer(Device* pDevice, BufferSrd* pSrdTable, bool isTmz);
    virtual ~OffchipLdsBuffer() {}

protected:
    virtual gpusize ComputeAllocationSize() const override;
    virtual void UpdateSrds() const override;

private:
    PAL_DISALLOW_DEFAULT_CTOR(OffchipLdsBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(OffchipLdsBuffer);
};

// =====================================================================================================================
// Implements shader-ring functionality specific to the sample position buffer required for AMDIL samplepos.
class SamplePosBuffer final : public ShaderRing
{
public:
    SamplePosBuffer(Device* pDevice, BufferSrd* pSrdTable, bool isTmz);
    virtual ~SamplePosBuffer() {}

    void UploadSamplePatternPalette(const SamplePatternPalette& samplePatternPalette);

protected:
    virtual gpusize ComputeAllocationSize() const override;
    virtual void UpdateSrds() const override;

private:
    PAL_DISALLOW_DEFAULT_CTOR(SamplePosBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(SamplePosBuffer);
};

} // Gfx6
} // Pal
