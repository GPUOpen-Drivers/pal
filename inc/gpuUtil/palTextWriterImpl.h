/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "palDevice.h"
#include "palFormatInfo.h"
#include "palGpuMemory.h"
#include "palInlineFuncs.h"
#include "palPipeline.h"
#include "palSysMemory.h"
#include "palTextWriter.h"

#include "textWriter/g_textWriterComputePipelineInitImpl.h"

namespace GpuUtil
{

/// Enumerates the various types of text colors that the debug text draw supports.
enum TextColor : Pal::uint32
{
    WhiteColor = 0, ///< White
    BlackColor = 1, ///< Black
};

// =====================================================================================================================
template <typename Allocator>
TextWriter<Allocator>::TextWriter(
    Pal::IDevice* pDevice,
    Allocator*    pAllocator)
    :
    m_pDevice(pDevice),
    m_pAllocator(pAllocator),
    m_pPipeline(nullptr),
    m_pFontData(nullptr),
    m_maxSrdSize(0)
{
    memset(m_fontSrd, 0, sizeof(m_fontSrd));
    memset(&m_deviceProps, 0, sizeof(m_deviceProps));
    memset(m_memHeapProps, 0, sizeof(m_memHeapProps));
}

// =====================================================================================================================
template <typename Allocator>
TextWriter<Allocator>::~TextWriter()
{
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Destroy();
        PAL_SAFE_FREE(m_pPipeline, m_pAllocator);
    }

    if (m_pFontData != nullptr)
    {
        m_pFontData->Destroy();
        PAL_SAFE_FREE(m_pFontData, m_pAllocator);
    }
}

// =====================================================================================================================
// Initializes the TextWriter class:
//      - Stores the device and GPU memory heap properties for later reference.
//      - Create the pipeline and GPU memory for the draw text pipeline.
//      - Create the GPU memory for the constant binary font data.
//      - Add all GPU memory references to the device and mark them as always resident.
template <typename Allocator>
Pal::Result TextWriter<Allocator>::Init()
{
    Pal::Result result = m_pDevice->GetProperties(&m_deviceProps);

    if (result == Pal::Result::Success)
    {
        m_maxSrdSize = Util::Max(Util::Max(m_deviceProps.gfxipProperties.srdSizes.bufferView,
                                           m_deviceProps.gfxipProperties.srdSizes.imageView),
                                 Util::Max(m_deviceProps.gfxipProperties.srdSizes.fmaskView,
                                           m_deviceProps.gfxipProperties.srdSizes.sampler));

        result = m_pDevice->GetGpuMemoryHeapProperties(m_memHeapProps);
    }

    if (result == Pal::Result::Success)
    {
        result = TextWriterFont::CreateTextWriterComputePipelines(m_pDevice, m_pAllocator, &m_pPipeline);
    }

    if (result == Pal::Result::Success)
    {
        result = CreateDrawFontData();
    }

    // Make the pipeline and font GPU memories always resident.
    if (result == Pal::Result::Success)
    {
        Pal::GpuMemoryRef memRef = {};
        memRef.flags.readOnly = 1;
        memRef.pGpuMemory     = m_pFontData;

        result = m_pDevice->AddGpuMemoryReferences(1, &memRef, nullptr, Pal::GpuMemoryRefCantTrim);
    }

    return result;
}

// =====================================================================================================================
// Creates the draw text font GPU memory and uploads the data into it.
template <typename Allocator>
Pal::Result TextWriter<Allocator>::CreateDrawFontData()
{
    // Create the memory for the debug text.
    Pal::GpuMemoryRequirements memReqs = {};
    memReqs.size      = sizeof(TextWriterFont::FontData);
    memReqs.alignment = sizeof(Pal::uint32);
    memReqs.heapCount = 2;
    memReqs.heaps[0]  = Pal::GpuHeapLocal;
    memReqs.heaps[1]  = Pal::GpuHeapGartUswc;

    Pal::gpusize offset = 0;
    Pal::Result  result = CreateGpuMemory(&memReqs, &m_pFontData, &offset);
    Pal::uint8*  pData  = nullptr;

    // Copy the data for the debug font into the memory object.
    if (result == Pal::Result::Success)
    {
        result = m_pFontData->Map(reinterpret_cast<void**>(&pData));
    }

    if (result == Pal::Result::Success)
    {
        PAL_ASSERT(pData != nullptr);
        memcpy(&pData[offset], &TextWriterFont::FontData[0], sizeof(TextWriterFont::FontData));

        result = m_pFontData->Unmap();
    }

    // Create an SRD for reading the font data.
    if (result == Pal::Result::Success)
    {
        Pal::BufferViewInfo fontDataView = {};
        fontDataView.gpuAddr        = m_pFontData->Desc().gpuVirtAddr + offset;
        fontDataView.range          = memReqs.size;
        fontDataView.stride         = 1;
        fontDataView.swizzledFormat = Pal::UndefinedSwizzledFormat;

        m_pDevice->CreateUntypedBufferViewSrds(1, &fontDataView, m_fontSrd);
    }

    return result;
}

// =====================================================================================================================
// Creates a GpuMemory object using the given memory requirements.
template <typename Allocator>
Pal::Result TextWriter<Allocator>::CreateGpuMemory(
    Pal::GpuMemoryRequirements* pMemReqs,
    Pal::IGpuMemory**           ppGpuMemory,
    Pal::gpusize*               pOffset)
{
    // Translate the memory requirements into a GpuMemory create info.
    Pal::GpuMemoryCreateInfo createInfo = {};
    createInfo.size      = pMemReqs->size;
    createInfo.alignment = pMemReqs->alignment;
    createInfo.vaRange   = Pal::VaRange::Default;
    createInfo.priority  = Pal::GpuMemPriority::VeryLow;
    createInfo.heapCount = pMemReqs->heapCount;

    for (Pal::uint32 i = 0; i < createInfo.heapCount; i++)
    {
        createInfo.heaps[i] = pMemReqs->heaps[i];
    }

    Pal::Result  result     = Pal::Result::Success;
    const size_t objectSize = m_pDevice->GetGpuMemorySize(createInfo, &result);

    if (result == Pal::Result::Success)
    {
        void* pMemory = PAL_MALLOC(objectSize, m_pAllocator, Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            result = m_pDevice->CreateGpuMemory(createInfo, pMemory, reinterpret_cast<Pal::IGpuMemory**>(ppGpuMemory));
            if (result != Pal::Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_pAllocator);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
// Executes a text draw (using a dispatch) onto the destination image.
template <typename Allocator>
void TextWriter<Allocator>::DrawDebugText(
    const Pal::IImage& dstImage,    // Desintation image.
    Pal::ICmdBuffer*   pCmdBuffer,  // Command buffer for drawing text
    const char*        pText,       // Text to draw
    Pal::uint32        x,           // X drawing offset
    Pal::uint32        y            // Y drawing offset
    ) const
{
    const Pal::uint32 stringLen = static_cast<Pal::uint32>(strlen(pText));

    if (stringLen > 0)
    {
        TextDrawShaderInfo info = {};
        info.startX = x;
        info.startY = y;

        // Pack the raw draw colors into the destination format.
        const Pal::SwizzledFormat imgFormat = dstImage.GetImageCreateInfo().swizzledFormat;
        Pal::uint32 foregroundColor[4] = {};
        Pal::uint32 backgroundColor[4] = {};

        // Convert the raw color into the destination format.
        if (Pal::Formats::IsUnorm(imgFormat.format)   || Pal::Formats::IsSnorm(imgFormat.format)   ||
            Pal::Formats::IsUscaled(imgFormat.format) || Pal::Formats::IsSscaled(imgFormat.format) ||
            Pal::Formats::IsFloat(imgFormat.format)   || Pal::Formats::IsSrgb(imgFormat.format))
        {
            constexpr float ColorTable[][4] =
            {
                { 1.0f, 1.0f, 1.0f, 1.0f },     // White
                { 0.0f, 0.0f, 0.0f, 1.0f },     // Black
            };

            Pal::Formats::ConvertColor(imgFormat, &ColorTable[WhiteColor][0], &foregroundColor[0]);
            Pal::Formats::ConvertColor(imgFormat, &ColorTable[BlackColor][0], &backgroundColor[0]);
        }
        else if (Pal::Formats::IsSint(imgFormat.format))
        {
            constexpr Pal::uint32 ColorTable[][4] =
            {
                { 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF },     // White
                { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },     // Black
            };
            memcpy(&foregroundColor[0], &ColorTable[WhiteColor][0], sizeof(foregroundColor));
            memcpy(&backgroundColor[0], &ColorTable[BlackColor][0], sizeof(backgroundColor));
        }
        else
        {
            PAL_ASSERT(Pal::Formats::IsUint(imgFormat.format));

            constexpr Pal::uint32 ColorTable[][4] =
            {
                { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },     // White
                { 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },     // Black
            };

            memcpy(&foregroundColor[0], &ColorTable[WhiteColor][0], sizeof(foregroundColor));
            memcpy(&backgroundColor[0], &ColorTable[BlackColor][0], sizeof(backgroundColor));
        }

        Pal::uint32 swizzledForegroundColor[4] = {};
        Pal::uint32 swizzledBackgroundColor[4] = {};

        Pal::Formats::SwizzleColor(imgFormat, foregroundColor, swizzledForegroundColor);
        Pal::Formats::SwizzleColor(imgFormat, backgroundColor, swizzledBackgroundColor);

        Pal::Formats::PackRawClearColor(imgFormat, swizzledForegroundColor, &info.foregroundColor[0]);
        Pal::Formats::PackRawClearColor(imgFormat, swizzledBackgroundColor, &info.backgroundColor[0]);

        // Get enough embedded space to store the text draw info struct and the string.
        Pal::gpusize      dataAddr   = 0;
        const Pal::uint32 dataDwords = (sizeof(TextDrawShaderInfo) / sizeof(Pal::uint32)) + stringLen;
        Pal::uint32*      pData      = pCmdBuffer->CmdAllocateEmbeddedData(dataDwords, 1, &dataAddr);

        // Copy the info struct into the embedded space.
        memcpy(pData, &info, sizeof(TextDrawShaderInfo));

        // NOTE: The string data immediately follows the info struct in the buffer. The shader's thread group is
        //       responsible for one letter. Each uint32 contains the ASCII value of the character, which is how
        //       the shader determines the offset into the font data buffer.
        pData += (sizeof(TextDrawShaderInfo) / sizeof(Pal::uint32));
        for (Pal::uint32 index = 0; index < stringLen; index++)
        {
            pData[index] = pText[index];
        }

        // Construct an embedded descriptor table. The first entry is a buffer view for the font data and the second is
        // an image view for the target image.
        const Pal::uint32 srdDwords = m_maxSrdSize / sizeof(Pal::uint32);

        Pal::gpusize      tableGpuAddr = 0;
        Pal::uint32*const pTable       = pCmdBuffer->CmdAllocateEmbeddedData(2 * srdDwords, 1, &tableGpuAddr);

        memcpy(pTable, m_fontSrd, sizeof(m_fontSrd));
        CreateImageView(&dstImage, pTable + srdDwords);

        // Bind that descriptor table to user data #0.
        const Pal::uint32 tableGpuAddrLo = Util::LowPart(tableGpuAddr);
        pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 0, 1, &tableGpuAddrLo);

        // Bind a buffer view for the embedded info struct and string in user data #1-4.
        Pal::BufferViewInfo dynamicViewInfo = {};
        dynamicViewInfo.gpuAddr        = dataAddr;
        dynamicViewInfo.range          = dataDwords * sizeof(Pal::uint32);
        dynamicViewInfo.stride         = 1;
        dynamicViewInfo.swizzledFormat = Pal::UndefinedSwizzledFormat;

        Pal::uint32 dynamicViewSrd[4] = {};
        m_pDevice->CreateUntypedBufferViewSrds(1, &dynamicViewInfo, &dynamicViewSrd[0]);
        pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, 4, &dynamicViewSrd[0]);

        // Bind the pipeline and issue one thread group per letter.
        pCmdBuffer->CmdBindPipeline({ Pal::PipelineBindPoint::Compute, m_pPipeline, Pal::InternalApiPsoHash, });
        pCmdBuffer->CmdDispatch(stringLen, 1, 1);
    }
}

// =====================================================================================================================
// Creates an internal image view for the provided pImage.
template <typename Allocator>
void TextWriter<Allocator>::CreateImageView(
    const Pal::IImage* pImage,
    void*              pOut
    ) const
{
    const auto& createInfo = pImage->GetImageCreateInfo();

    Pal::ImageViewInfo imgViewInfo = {};
    imgViewInfo.pImage     = pImage;
    imgViewInfo.viewType   = Pal::ImageViewType::Tex2d;

    imgViewInfo.swizzledFormat = GetRawFormat(createInfo.swizzledFormat.format);

    // This is only used in a compute shader write, but will probably be immediately followed by a present
    imgViewInfo.possibleLayouts.engines = Pal::EngineTypeUniversal   | Pal::EngineTypeCompute;
    imgViewInfo.possibleLayouts.usages  = Pal::LayoutShaderWrite     | Pal::LayoutShaderRead |
                                          Pal::LayoutPresentWindowed | Pal::LayoutPresentFullscreen;

    imgViewInfo.subresRange.startSubres.aspect     = Pal::ImageAspect::Color;
    imgViewInfo.subresRange.startSubres.arraySlice = 0;
    imgViewInfo.subresRange.startSubres.mipLevel   = 0;
    imgViewInfo.subresRange.numSlices              = createInfo.arraySize;
    imgViewInfo.subresRange.numMips                = createInfo.mipLevels;

    m_pDevice->CreateImageViewSrds(1, &imgViewInfo, pOut);
}

// =====================================================================================================================
// Gets a raw Uint format that matches the bit depth of the provided format.
template <typename Allocator>
Pal::SwizzledFormat TextWriter<Allocator>::GetRawFormat(
    Pal::ChNumFormat oldFmt)
{
    Pal::SwizzledFormat retFmt = Pal::UndefinedSwizzledFormat;

    switch(Pal::Formats::BitsPerPixel(oldFmt))
    {
    case 8:
        retFmt.format  = Pal::ChNumFormat::X8_Uint;
        retFmt.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 16:
        retFmt.format  = Pal::ChNumFormat::X16_Uint;
        retFmt.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 32:
        retFmt.format  = Pal::ChNumFormat::X32_Uint;
        retFmt.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 64:
        retFmt.format  = Pal::ChNumFormat::X32Y32_Uint;
        retFmt.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Zero, Pal::ChannelSwizzle::One };
        break;
    case 128:
        retFmt.format  = Pal::ChNumFormat::X32Y32Z32W32_Uint;
        retFmt.swizzle =
            { Pal::ChannelSwizzle::X, Pal::ChannelSwizzle::Y,    Pal::ChannelSwizzle::Z,    Pal::ChannelSwizzle::W   };
        break;
    default:
        retFmt = Pal::UndefinedSwizzledFormat;
        break;
    }

    return retFmt;
}

} // GpuUtil
