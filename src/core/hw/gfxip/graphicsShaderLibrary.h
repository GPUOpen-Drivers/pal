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

#pragma once

#include "core/device.h"
#include "core/hw/gfxip/pipeline.h"
#include "palMsgPack.h"
#include "palPipelineAbi.h"
#include "shaderLibrary.h"
#include "palVector.h"

namespace Pal
{
/// Properties of color export library
struct ColorExportProperty
{
    uint16 sgprCount;
    uint16 vgprCount;
    uint32 scratchMemorySize;
};

/// Properties of a graphics shader library.
struct GraphicsShaderLibraryInfo
{
    uint16 apiShaderMask;                    // ShaderType mask, include all present shader in the library
    uint16 hwShaderMask;                     // HardwareStage mask, include all present hw shader stage in the library
    ColorExportProperty colorExportProperty; // Color export shader special properties
    bool    isColorExport;                   // True if color export shader is included in the library
};

// =====================================================================================================================
// Hardware independent graphics shader library class. Implements all details of a graphics shader library that are
// common across all hardware types.
class GraphicsShaderLibrary : public ShaderLibrary
{
public:
    explicit GraphicsShaderLibrary(Device* pDevice)
        :
        ShaderLibrary(pDevice),
        m_gfxLibInfo{}
    {}

    virtual ~GraphicsShaderLibrary() {}

    virtual const GraphicsPipeline* GetPartialPipeline() const = 0 ;

    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const override;

    uint32 GetHwShaderMask() const { return m_gfxLibInfo.hwShaderMask; }
    uint32 GetApiShaderMask() const { return m_gfxLibInfo.apiShaderMask; }
    bool IsColorExportShader() const { return m_gfxLibInfo.isColorExport; }

    void GetColorExportProperty(ColorExportProperty* pProperty) const { *pProperty = m_gfxLibInfo.colorExportProperty; }

    virtual UploadFenceToken GetUploadFenceToken() const override;
    virtual uint64           GetPagingFenceVal() const override;

private:
    virtual Result PostInit(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        Util::MsgPackReader*                    pReader) override;

    GraphicsShaderLibraryInfo m_gfxLibInfo;

    PAL_DISALLOW_DEFAULT_CTOR(GraphicsShaderLibrary);
    PAL_DISALLOW_COPY_AND_ASSIGN(GraphicsShaderLibrary);
};

} // namespace Pal
