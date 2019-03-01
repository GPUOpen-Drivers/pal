/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/cmdBufferLogger/cmdBufferLoggerCmdBuffer.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerDevice.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerImage.h"
#include "core/layers/cmdBufferLogger/cmdBufferLoggerPlatform.h"
#include "core/g_palPlatformSettings.h"
#include <cinttypes>

// These includes are required because we need the definition of the D3D12DDI_PRESENT_0003 struct in order to make a
// copy of the data in it for the tokenization.

using namespace Util;

namespace Pal
{
namespace CmdBufferLogger
{

static constexpr size_t StringLength = 512;

// =====================================================================================================================
static const char* GetCmdBufCallIdString(
    CmdBufCallId id)
{
    return CmdBufCallIdStrings[static_cast<size_t>(id)];
}

// =====================================================================================================================
static const char* ImageAspectToString(
    ImageAspect aspect)
{
    const char* AspectNames[] =
    {
        "Color",
        "Depth",
        "Stencil",
        "Fmask",
        "Y",
        "CbCr",
        "Cb",
        "Cr",
        "YCbCr",
    };
    static_assert((ArrayLen(AspectNames) == static_cast<size_t>(ImageAspect::Count)), "");

    return AspectNames[static_cast<size_t>(aspect)];
}

// =====================================================================================================================
static void SubresIdToString(
    const SubresId& subresId,
    char            string[StringLength])
{
    const size_t currentLength = strlen(string);
    Snprintf(&string[0] + currentLength, StringLength - currentLength,
        "{ aspect: %s, mipLevel: 0x%x, arraySlice: 0x%x }",
        ImageAspectToString(subresId.aspect), subresId.mipLevel, subresId.arraySlice);
}

// =====================================================================================================================
static void ImageLayoutToString(
    const ImageLayout& imageLayout,
    char               string[StringLength])
{
    const size_t currentLength = strlen(string);
    Snprintf(&string[0] + currentLength,
             StringLength - currentLength,
             "[ usages: 0x%x, engines: 0x%x ]",
             imageLayout.usages,
             imageLayout.engines);
}

// =====================================================================================================================
static void SubresRangeToString(
    CmdBuffer*         pCmdBuffer,
    const SubresRange& subresRange,
    char               string[StringLength])
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "");
    SubresIdToString(subresRange.startSubres, pString);
    Snprintf(&string[0], StringLength, "{ startSubres: %s, numMips: 0x%x, numSlices: 0x%x }",
        pString, subresRange.numMips, subresRange.numSlices);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpFloat(
    CmdBuffer*  pCmdBuffer,
    const char* pTitle,
    float       data)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "%s = %f", pTitle, data);
    pCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
template <typename uint>
static void DumpUint(
    CmdBuffer*  pCmdBuffer,
    const char* pTitle,
    uint        data)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "%s = %x", pTitle, data);
    pCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DataToString(
    CmdBuffer*    pCmdBuffer,
    uint32        numEntries,
    const uint32* pEntryValues,
    const char*   pHeader)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    size_t currentIndex = 0;
    for (uint32 i = 0; i < numEntries; i++)
    {
        if ((i > 0) && ((i % 4) == 0))
        {
            pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
        }
        if ((i % 4) == 0)
        {
            currentIndex = Snprintf(pString, StringLength, "%s", pHeader);
        }
        currentIndex += Snprintf(&pString[currentIndex], StringLength - currentIndex, "0x%08X ", pEntryValues[i]);
    }

    if (currentIndex != 0)
    {
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const char* FormatToString(
    ChNumFormat format)
{
    const char* FormatStrings[] =
    {
        "Undefined",
        "X1_Unorm",
        "X1_Uscaled",
        "X4Y4_Unorm",
        "X4Y4_Uscaled",
        "L4A4_Unorm",
        "X4Y4Z4W4_Unorm",
        "X4Y4Z4W4_Uscaled",
        "X5Y6Z5_Unorm",
        "X5Y6Z5_Uscaled",
        "X5Y5Z5W1_Unorm",
        "X5Y5Z5W1_Uscaled",
        "X1Y5Z5W5_Unorm",
        "X1Y5Z5W5_Uscaled",
        "X8_Unorm",
        "X8_Snorm",
        "X8_Uscaled",
        "X8_Sscaled",
        "X8_Uint",
        "X8_Sint",
        "X8_Srgb",
        "A8_Unorm",
        "L8_Unorm",
        "P8_Uint",
        "X8Y8_Unorm",
        "X8Y8_Snorm",
        "X8Y8_Uscaled",
        "X8Y8_Sscaled",
        "X8Y8_Uint",
        "X8Y8_Sint",
        "X8Y8_Srgb",
        "L8A8_Unorm",
        "X8Y8Z8W8_Unorm",
        "X8Y8Z8W8_Snorm",
        "X8Y8Z8W8_Uscaled",
        "X8Y8Z8W8_Sscaled",
        "X8Y8Z8W8_Uint",
        "X8Y8Z8W8_Sint",
        "X8Y8Z8W8_Srgb",
        "U8V8_Snorm_L8W8_Unorm",
        "X10Y11Z11_Float",
        "X11Y11Z10_Float",
        "X10Y10Z10W2_Unorm",
        "X10Y10Z10W2_Snorm",
        "X10Y10Z10W2_Uscaled",
        "X10Y10Z10W2_Sscaled",
        "X10Y10Z10W2_Uint",
        "X10Y10Z10W2_Sint",
        "X10Y10Z10W2Bias_Unorm",
        "U10V10W10_Snorm_A2_Unorm",
        "X16_Unorm",
        "X16_Snorm",
        "X16_Uscaled",
        "X16_Sscaled",
        "X16_Uint",
        "X16_Sint",
        "X16_Float",
        "L16_Unorm",
        "X16Y16_Unorm",
        "X16Y16_Snorm",
        "X16Y16_Uscaled",
        "X16Y16_Sscaled",
        "X16Y16_Uint",
        "X16Y16_Sint",
        "X16Y16_Float",
        "X16Y16Z16W16_Unorm",
        "X16Y16Z16W16_Snorm",
        "X16Y16Z16W16_Uscaled",
        "X16Y16Z16W16_Sscaled",
        "X16Y16Z16W16_Uint",
        "X16Y16Z16W16_Sint",
        "X16Y16Z16W16_Float",
        "X32_Uint",
        "X32_Sint",
        "X32_Float",
        "X32Y32_Uint",
        "X32Y32_Sint",
        "X32Y32_Float",
        "X32Y32Z32_Uint",
        "X32Y32Z32_Sint",
        "X32Y32Z32_Float",
        "X32Y32Z32W32_Uint",
        "X32Y32Z32W32_Sint",
        "X32Y32Z32W32_Float",
        "D16_Unorm_S8_Uint",
        "D32_Float_S8_Uint",
        "X9Y9Z9E5_Float",
        "Bc1_Unorm",
        "Bc1_Srgb",
        "Bc2_Unorm",
        "Bc2_Srgb",
        "Bc3_Unorm",
        "Bc3_Srgb",
        "Bc4_Unorm",
        "Bc4_Snorm",
        "Bc5_Unorm",
        "Bc5_Snorm",
        "Bc6_Ufloat",
        "Bc6_Sfloat",
        "Bc7_Unorm",
        "Bc7_Srgb",
        "Etc2X8Y8Z8_Unorm",
        "Etc2X8Y8Z8_Srgb",
        "Etc2X8Y8Z8W1_Unorm",
        "Etc2X8Y8Z8W1_Srgb",
        "Etc2X8Y8Z8W8_Unorm",
        "Etc2X8Y8Z8W8_Srgb",
        "Etc2X11_Unorm",
        "Etc2X11_Snorm",
        "Etc2X11Y11_Unorm",
        "Etc2X11Y11_Snorm",
        "AstcLdr4x4_Unorm",
        "AstcLdr4x4_Srgb",
        "AstcLdr5x4_Unorm",
        "AstcLdr5x4_Srgb",
        "AstcLdr5x5_Unorm",
        "AstcLdr5x5_Srgb",
        "AstcLdr6x5_Unorm",
        "AstcLdr6x5_Srgb",
        "AstcLdr6x6_Unorm",
        "AstcLdr6x6_Srgb",
        "AstcLdr8x5_Unorm",
        "AstcLdr8x5_Srgb",
        "AstcLdr8x6_Unorm",
        "AstcLdr8x6_Srgb",
        "AstcLdr8x8_Unorm",
        "AstcLdr8x8_Srgb",
        "AstcLdr10x5_Unorm",
        "AstcLdr10x5_Srgb",
        "AstcLdr10x6_Unorm",
        "AstcLdr10x6_Srgb",
        "AstcLdr10x8_Unorm",
        "AstcLdr10x8_Srgb",
        "AstcLdr10x10_Unorm",
        "AstcLdr10x10_Srgb",
        "AstcLdr12x10_Unorm",
        "AstcLdr12x10_Srgb",
        "AstcLdr12x12_Unorm",
        "AstcLdr12x12_Srgb",
        "AstcHdr4x4_Float",
        "AstcHdr5x4_Float",
        "AstcHdr5x5_Float",
        "AstcHdr6x5_Float",
        "AstcHdr6x6_Float",
        "AstcHdr8x5_Float",
        "AstcHdr8x6_Float",
        "AstcHdr8x8_Float",
        "AstcHdr10x5_Float",
        "AstcHdr10x6_Float",
        "AstcHdr10x8_Float",
        "AstcHdr10x10_Float",
        "AstcHdr12x10_Float",
        "AstcHdr12x12_Float",
        "X8Y8_Z8Y8_Unorm",
        "X8Y8_Z8Y8_Uscaled",
        "Y8X8_Y8Z8_Unorm",
        "Y8X8_Y8Z8_Uscaled",
        "AYUV",
        "UYVY",
        "VYUY",
        "YUY2",
        "YVY2",
        "YV12",
        "NV11",
        "NV12",
        "NV21",
        "P016",
        "P010",
    };

    static_assert(ArrayLen(FormatStrings) == static_cast<size_t>(ChNumFormat::Count),
                  "The number of formats has changed!");

    return FormatStrings[static_cast<size_t>(format)];
}

// =====================================================================================================================
static void SwizzleToString(
    ChannelMapping swizzle,
    char*          pString)
{
    const char* SwizzleStrings[] =
    {
        "Zero",
        "One",
        "X",
        "Y",
        "Z",
        "W",
    };

    static_assert(ArrayLen(SwizzleStrings) == static_cast<size_t>(ChannelSwizzle::Count),
                  "The number of swizzles has changed!");

    const size_t currentLength = strlen(pString);

    Snprintf(pString + currentLength, StringLength - currentLength, "{ R = %s, G = %s, B = %s, A = %s }",
             SwizzleStrings[static_cast<size_t>(swizzle.r)],
             SwizzleStrings[static_cast<size_t>(swizzle.g)],
             SwizzleStrings[static_cast<size_t>(swizzle.b)],
             SwizzleStrings[static_cast<size_t>(swizzle.a)]);
}

// =====================================================================================================================
static void Offset2dToString(
    const Offset2d& offset,
    char            string[StringLength])
{
    const size_t currentLength = strlen(string);

    Snprintf(string + currentLength, StringLength - currentLength, "{ x = 0x%x, y = 0x%x }",
        offset.x, offset.y);
}

// =====================================================================================================================
static void Extent2dToString(
    const Extent2d& extent,
    char            string[StringLength])
{
    const size_t currentLength = strlen(string);

    Snprintf(string + currentLength, StringLength - currentLength, "{ width = 0x%x, height = 0x%x }",
        extent.width, extent.height);
}

// =====================================================================================================================
static void Offset3dToString(
    const Offset3d& offset,
    char            string[StringLength])
{
    const size_t currentLength = strlen(string);

    Snprintf(string + currentLength, StringLength - currentLength, "{ x = 0x%x, y = 0x%x, z = 0x%x }",
             offset.x, offset.y, offset.z);
}

// =====================================================================================================================
static void Extent3dToString(
    const Extent3d& extent,
    char            string[StringLength])
{
    const size_t currentLength = strlen(string);

    Snprintf(string + currentLength, StringLength - currentLength, "{ width = 0x%x, height = 0x%x, depth = 0x%x }",
             extent.width, extent.height, extent.depth);
}

// =====================================================================================================================
static const void DumpRanges(
    CmdBuffer*   pCmdBuffer,
    uint32       count,
    const Range* pRanges)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "rangeCount = %d", count);
    pNextCmdBuffer->CmdCommentString(pString);

    if ((count > 0) && (pRanges != nullptr))
    {
        Snprintf(pString, StringLength, "pRanges = {");
        pNextCmdBuffer->CmdCommentString(pString);

        for (uint32 i = 0; i < count; i++)
        {
            const auto& range = pRanges[i];

            Snprintf(pString, StringLength, "\tRange %d = { offset = 0x%08x, extent = 0x%08x }",
                     i, range.offset, range.extent);
            pNextCmdBuffer->CmdCommentString(pString);
        }

        Snprintf(pString, StringLength, "}");
        pNextCmdBuffer->CmdCommentString(pString);
}

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const void DumpSubresRanges(
    CmdBuffer*         pCmdBuffer,
    uint32             count,
    const SubresRange* pRanges)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "rangeCount = %d", count);
    pNextCmdBuffer->CmdCommentString(pString);

    if ((count > 0) && (pRanges != nullptr))
    {
        Snprintf(pString, StringLength, "pRanges = [");
        pNextCmdBuffer->CmdCommentString(pString);

        for (uint32 i = 0; i < count; i++)
        {
            char* pSubresRange = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

            const auto& range = pRanges[i];
            SubresRangeToString(pCmdBuffer, range, pSubresRange);

            Snprintf(pString, StringLength, "\tSubresRange %d = { %s }", i, pSubresRange);
            pNextCmdBuffer->CmdCommentString(pString);

            PAL_SAFE_DELETE_ARRAY(pSubresRange, &allocator);
        }

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const void DumpRects(
    CmdBuffer*  pCmdBuffer,
    uint32      count,
    const Rect* pRects)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "rectCount = %d", count);
    pNextCmdBuffer->CmdCommentString(pString);

    if ((count > 0) && (pRects != nullptr))
    {
        Snprintf(pString, StringLength, "pRects = {");
        pNextCmdBuffer->CmdCommentString(pString);

        for (uint32 i = 0; i < count; i++)
        {
            Snprintf(pString, StringLength, "\tRect %d = {", i);
            pNextCmdBuffer->CmdCommentString(pString);

            const auto& rect = pRects[i];

            Snprintf(pString, StringLength, "\t\t");
            Offset2dToString(rect.offset, pString);
            pNextCmdBuffer->CmdCommentString(pString);
            Snprintf(pString, StringLength, "\t\t");
            Extent2dToString(rect.extent, pString);
            pNextCmdBuffer->CmdCommentString(pString);

            Snprintf(pString, StringLength, "\t}", i);
            pNextCmdBuffer->CmdCommentString(pString);
        }

        Snprintf(pString, StringLength, "}");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const void DumpBoxes(
    CmdBuffer*  pCmdBuffer,
    uint32      count,
    const Box*  pBoxes)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "boxCount = %d", count);
    pNextCmdBuffer->CmdCommentString(pString);

    if ((count > 0) && (pBoxes != nullptr))
    {
        Snprintf(pString, StringLength, "pBoxes = [");
        pNextCmdBuffer->CmdCommentString(pString);

        for (uint32 i = 0; i < count; i++)
        {
            Snprintf(pString, StringLength, "\tBox %d = {", i);
            pNextCmdBuffer->CmdCommentString(pString);

            const auto& box = pBoxes[i];

            Snprintf(pString, StringLength, "\t\t");
            Offset3dToString(box.offset, pString);
            pNextCmdBuffer->CmdCommentString(pString);
            Snprintf(pString, StringLength, "\t\t");
            Extent3dToString(box.extent, pString);
            pNextCmdBuffer->CmdCommentString(pString);

            Snprintf(pString, StringLength, "\t}", i);
            pNextCmdBuffer->CmdCommentString(pString);
        }

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const void DumpClearColor(
    CmdBuffer*        pCmdBuffer,
    const ClearColor& color,
    const char*       pTitle)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    const char* ClearColorTypesStrings[] =
    {
        "Uint",
        "Sint",
        "Float",
    };

    Snprintf(pString, StringLength, "%s = {", pTitle);
    pNextCmdBuffer->CmdCommentString(pString);
    Snprintf(pString, StringLength, "\ttype = %s", ClearColorTypesStrings[static_cast<uint32>(color.type)]);
    pNextCmdBuffer->CmdCommentString(pString);

    if (color.type == ClearColorType::Float)
    {
        Snprintf(pString, StringLength, "\tR: %f, G: %f, B: %f, A: %f",
                 color.f32Color[0], color.f32Color[1], color.f32Color[2], color.f32Color[3]);
    }
    else
    {
        Snprintf(pString, StringLength, "\tR: 0x%08x, G: 0x%08x, B: 0x%08x, A: 0x%08x",
                 color.u32Color[0], color.u32Color[1], color.u32Color[2], color.u32Color[3]);
    }
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "}");
    pNextCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static const void PrintImageCreateInfo(
    CmdBuffer*             pCmdBuffer,
    const ImageCreateInfo& createInfo,
    char*                  pString,
    const char*            pPrefix)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    Snprintf(pString, StringLength, "%s ImageCreateInfo = [", pPrefix);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString,
             StringLength,
             "%s\t Image Format     = %s",
             pPrefix,
             FormatToString(createInfo.swizzledFormat.format));
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Image Swizzle    = ", pPrefix);
    SwizzleToString(createInfo.swizzledFormat.swizzle, pString);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Extent           = ", pPrefix);
    Extent3dToString(createInfo.extent, pString);
    pNextCmdBuffer->CmdCommentString(pString);

    const char* ImageTypeStrings[] =
    {
        "Tex1D",
        "Tex2D",
        "Tex3D",
    };
    static_assert(ArrayLen(ImageTypeStrings) == static_cast<size_t>(ImageType::Count),
                  "The number of ImageType's has changed!");

    Snprintf(pString, StringLength, "%s\t Image Type       = %s", pPrefix,
             ImageTypeStrings[static_cast<size_t>(createInfo.imageType)]);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Mip Levels       = %u", pPrefix, createInfo.mipLevels);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Array Size       = %u", pPrefix, createInfo.arraySize);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Samples          = %u", pPrefix, createInfo.samples);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t Fragments        = %u", pPrefix, createInfo.fragments);
    pNextCmdBuffer->CmdCommentString(pString);

    const char* ImageTilingStrings[] =
    {
        "Linear",
        "Optimal",
        "Standard64Kb",
    };
    static_assert(ArrayLen(ImageTilingStrings) == static_cast<size_t>(ImageTiling::Count),
                  "ImageTilingStrings struct is not the same size as the ImageTiling enum!");

    Snprintf(pString, StringLength, "%s\t Tiling           = %s", pPrefix,
             ImageTilingStrings[static_cast<size_t>(createInfo.tiling)]);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t ImageCreateFlags = 0x%08x", pPrefix, createInfo.flags.u32All);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t ImageUsageFlags  = 0x%08x", pPrefix, createInfo.usageFlags.u32All);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s ] // ImageCreateInfo", pPrefix);
    pNextCmdBuffer->CmdCommentString(pString);
}

// =====================================================================================================================
static void DumpGpuMemoryInfo(
    CmdBuffer*        pCmdBuffer,
    const IGpuMemory* pGpuMemory,
    const char*       pTitle,
    const char*       pPrefix)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    const auto& desc = pGpuMemory->Desc();

    Snprintf(pString, StringLength, "%s %s = [", pPrefix, pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t GpuMemory Pointer = 0x%016" PRIXPTR, pPrefix, pGpuMemory);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\t GpuVirtAddr       = 0x%016llX", pPrefix, desc.gpuVirtAddr);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    Snprintf(pString, StringLength, "%s\t Size              = 0x%016llX", pPrefix, desc.size);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    Snprintf(pString, StringLength, "%s\t Alignment         = 0x%016llX", pPrefix, desc.alignment);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s ] // %s", pPrefix, pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpImageInfo(
    CmdBuffer*    pCmdBuffer,
    const IImage* pImage,
    const char*   pTitle,
    const char*   pPrefix)
{
    const Image* pLoggerImage = static_cast<const Image*>(pImage);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "%s%s = [", pPrefix, pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    const auto& imageCreateInfo = pImage->GetImageCreateInfo();
    Snprintf(pString, StringLength, "%s\t Image Pointer = 0x%016" PRIXPTR, pPrefix, pImage);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    char* pTotalPrefix = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);
    Snprintf(pTotalPrefix, StringLength, "%s\t", pPrefix);

    DumpGpuMemoryInfo(pCmdBuffer, pLoggerImage->GetBoundMemObject(), "Bound GpuMemory", pTotalPrefix);

    Snprintf(pString,
             StringLength,
             "%s\t Bound GpuMemory Offset  = 0x%016llX",
             pPrefix,
             pLoggerImage->GetBoundMemOffset());
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PrintImageCreateInfo(pCmdBuffer, imageCreateInfo, pString, pTotalPrefix);

    PAL_SAFE_DELETE_ARRAY(pTotalPrefix, &allocator);

    Snprintf(pString, StringLength, "%s] // %s", pPrefix, pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpImageLayout(
    CmdBuffer*         pCmdBuffer,
    const ImageLayout& layout,
    const char*        pTitle)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "%s ImageLayout = { usages = 0x%06X, engines = 0x%02X }",
        pTitle, layout.usages, layout.engines);

    pCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpImageViewSrd(
    CmdBuffer*   pCmdBuffer,
    const void*  pImageViewSrd,
    const char*  pTitle)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    const auto& props   = pCmdBuffer->LoggerDevice()->DeviceProps();

    Snprintf(pString, StringLength, "%s = {", pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    DataToString(pCmdBuffer,
                 (props.gfxipProperties.srdSizes.imageView / sizeof(uint32)),
                 static_cast<const uint32*>(pImageViewSrd),
                 "\t");

    Snprintf(pString, StringLength, "}", pTitle);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpBufferViewSrd(
    CmdBuffer*   pCmdBuffer,
    const void*  pBufferViewSrd,
    const char*  pTitle)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    const auto& props = pCmdBuffer->LoggerDevice()->DeviceProps();

    Snprintf(pString, StringLength, "%s = {", pTitle);
    pCmdBuffer->CmdCommentString(pString);

    DataToString(pCmdBuffer,
        (props.gfxipProperties.srdSizes.bufferView / sizeof(uint32)),
        static_cast<const uint32*>(pBufferViewSrd),
        "\t");

    Snprintf(pString, StringLength, "}", pTitle);
    pCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpClearColorImageFlags(
    CmdBuffer* pCmdBuffer,
    uint32     flags)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "flags = ");

    if (TestAnyFlagSet(flags, ClearColorImageFlags::ColorClearAutoSync))
    {
        const size_t currentStringLength = strlen(pString);
        Snprintf(pString + currentStringLength, StringLength - currentStringLength, "ColorClearAutoSync");
    }

    pCmdBuffer->CmdCommentString(pString);
    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpClearDepthStencilImageFlags(
    CmdBuffer* pCmdBuffer,
    uint32     flags)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "flags = ");

    if (TestAnyFlagSet(flags, ClearDepthStencilFlags::DsClearAutoSync))
    {
        const size_t currentStringLength = strlen(pString);
        Snprintf(pString + currentStringLength, StringLength - currentStringLength, "DsClearAutoSync");
    }

    pCmdBuffer->CmdCommentString(pString);
    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
CmdBuffer::CmdBuffer(
    ICmdBuffer*                pNextCmdBuffer,
    Device*                    pDevice,
    const CmdBufferCreateInfo& createInfo)
    :
    CmdBufferDecorator(pNextCmdBuffer, static_cast<DeviceDecorator*>(pDevice->GetNextLayer())),
    m_pDevice(pDevice),
    m_allocator(1 * 1024 * 1024),
    m_pTimestamp(nullptr),
    m_timestampAddr(0),
    m_counter(0)
{
    const auto& cmdBufferLoggerConfig = pDevice->GetPlatform()->PlatformSettings().cmdBufferLoggerConfig;
    m_annotations.u32All = cmdBufferLoggerConfig.cmdBufferLoggerAnnotations;
    m_singleStep.u32All  = cmdBufferLoggerConfig.cmdBufferLoggerSingleStep;

    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Compute)]  = &CmdBuffer::CmdSetUserDataCs;
    m_funcTable.pfnCmdSetUserData[static_cast<uint32>(PipelineBindPoint::Graphics)] = &CmdBuffer::CmdSetUserDataGfx;

    m_funcTable.pfnCmdDraw                     = CmdDraw;
    m_funcTable.pfnCmdDrawOpaque               = CmdDrawOpaque;
    m_funcTable.pfnCmdDrawIndexed              = CmdDrawIndexed;
    m_funcTable.pfnCmdDrawIndirectMulti        = CmdDrawIndirectMulti;
    m_funcTable.pfnCmdDrawIndexedIndirectMulti = CmdDrawIndexedIndirectMulti;
    m_funcTable.pfnCmdDispatch                 = CmdDispatch;
    m_funcTable.pfnCmdDispatchIndirect         = CmdDispatchIndirect;
    m_funcTable.pfnCmdDispatchOffset           = CmdDispatchOffset;
}

// =====================================================================================================================
Result CmdBuffer::Init()
{
    Result result = m_allocator.Init();

    if ((result == Result::Success) && IsTimestampingActive())
    {
        DeviceProperties deviceProps = {};
        result = m_pDevice->GetProperties(&deviceProps);

        GpuMemoryCreateInfo createInfo = {};
        if (result == Result::Success)
        {
            result = Result::ErrorOutOfMemory;

            const Pal::gpusize allocGranularity = deviceProps.gpuMemoryProperties.virtualMemAllocGranularity;

            createInfo.size               = Util::Pow2Align(sizeof(CmdBufferTimestampData), allocGranularity);
            createInfo.alignment          = Util::Pow2Align(sizeof(uint64), allocGranularity);
            createInfo.vaRange            = VaRange::Default;
            createInfo.priority           = GpuMemPriority::VeryLow;
            createInfo.heapCount          = 1;
            createInfo.heaps[0]           = GpuHeap::GpuHeapInvisible;
            createInfo.flags.virtualAlloc = 1;

            m_pTimestamp = static_cast<IGpuMemory*>(PAL_MALLOC(m_pDevice->GetGpuMemorySize(createInfo, &result),
                                                               m_pDevice->GetPlatform(),
                                                               AllocInternal));

            if (m_pTimestamp != nullptr)
            {
                result = m_pDevice->CreateGpuMemory(createInfo, static_cast<void*>(m_pTimestamp), &m_pTimestamp);
            }
            else
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            GpuMemoryRef memRef = {};
            memRef.pGpuMemory   = m_pTimestamp;
            result = m_pDevice->AddGpuMemoryReferences(1, &memRef, nullptr, GpuMemoryRefCantTrim);
        }

        if (result == Result::Success)
        {
            m_timestampAddr = m_pTimestamp->Desc().gpuVirtAddr;
        }
    }

    return result;
}

// =====================================================================================================================
void CmdBuffer::Destroy()
{
    if (IsTimestampingActive() && (m_pTimestamp != nullptr))
    {
        m_pDevice->RemoveGpuMemoryReferences(1, &m_pTimestamp, nullptr);
        m_pTimestamp->Destroy();
        PAL_SAFE_FREE(m_pTimestamp, m_pDevice->GetPlatform());
    }

    ICmdBuffer* pNextLayer = m_pNextLayer;
    this->~CmdBuffer();
    pNextLayer->Destroy();
}

// =====================================================================================================================
void CmdBuffer::AddTimestamp()
{
    m_counter++;

    char desc[256] = {};
    Snprintf(&desc[0], sizeof(desc), "Incrementing counter for the next event with counter value 0x%08x.", m_counter);
    GetNextLayer()->CmdCommentString(&desc[0]);

    GetNextLayer()->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                      m_counter,
                                      ImmediateDataWidth::ImmediateData32Bit,
                                      m_timestampAddr + offsetof(CmdBufferTimestampData, counter));
}

// =====================================================================================================================
void CmdBuffer::AddSingleStepBarrier()
{
    BarrierInfo barrier = {};
    barrier.waitPoint   = HwPipePoint::HwPipeTop;

    constexpr HwPipePoint PipePoints[] =
    {
        HwPipePoint::HwPipeBottom,
        HwPipePoint::HwPipePostCs
    };
    barrier.pPipePoints        = &PipePoints[0];
    barrier.pipePointWaitCount = static_cast<uint32>(ArrayLen(PipePoints));

    char desc[256] = {};
    Snprintf(&desc[0], sizeof(desc), "Waiting for the previous event with counter value 0x%08x.", m_counter);
    GetNextLayer()->CmdCommentString(&desc[0]);

    GetNextLayer()->CmdBarrier(barrier);
}

// =====================================================================================================================
Result CmdBuffer::Begin(
    const CmdBufferBuildInfo& info)
{
    m_counter = 0;

    Result result = GetNextLayer()->Begin(NextCmdBufferBuildInfo(info));

    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::Begin));
    }

    if (IsTimestampingActive())
    {
        char buffer[256] = {};
        Snprintf(&buffer[0], sizeof(buffer), "Updating CmdBuffer Hash to 0x%016llX.", reinterpret_cast<uint64>(this));
        GetNextLayer()->CmdCommentString(&buffer[0]);
        Snprintf(&buffer[0], sizeof(buffer), "Resetting counter to 0.");
        GetNextLayer()->CmdCommentString(&buffer[0]);

        GetNextLayer()->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                          reinterpret_cast<uint64>(this),
                                          ImmediateDataWidth::ImmediateData64Bit,
                                          m_timestampAddr + offsetof(CmdBufferTimestampData, cmdBufferHash));
        GetNextLayer()->CmdWriteImmediate(HwPipePoint::HwPipeTop,
                                          0,
                                          ImmediateDataWidth::ImmediateData32Bit,
                                          m_timestampAddr + offsetof(CmdBufferTimestampData, counter));
    }

    return result;
}

// =====================================================================================================================
Result CmdBuffer::End()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::End));
    }

    return GetNextLayer()->End();
}

// =====================================================================================================================
Result CmdBuffer::Reset(
    ICmdAllocator* pCmdAllocator,
    bool           returnGpuMemory)
{
    m_counter = 0;
    return GetNextLayer()->Reset(NextCmdAllocator(pCmdAllocator), returnGpuMemory);
}

// =====================================================================================================================
static void CmdBindPipelineToString(
    CmdBuffer*                pCmdBuffer,
    const PipelineBindParams& params)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "PipelineBindPoint = %s",
             (params.pipelineBindPoint == PipelineBindPoint::Compute) ? "PipelineBindPoint::Compute" :
                                                                        "PipelineBindPoint::Graphics");
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    if (params.pPipeline != nullptr)
    {
        const auto& info = params.pPipeline->GetInfo();

        Snprintf(pString, StringLength, "PipelineStableHash      = 0x%016llX", info.internalPipelineHash.stable);
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

        Snprintf(pString, StringLength, "PipelineUniqueHash      = 0x%016llX", info.internalPipelineHash.unique);
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

        Snprintf(pString, StringLength, "PipelinePalRuntimeHash  = 0x%016llX", info.palRuntimeHash);
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    }
    else
    {
        Snprintf(pString, StringLength, "Pipeline = Null");
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdBindPipeline(
    const PipelineBindParams& params)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindPipeline));

        CmdBindPipelineToString(this, params);
    }

    GetNextLayer()->CmdBindPipeline(NextPipelineBindParams(params));
}

// =====================================================================================================================
void CmdBuffer::CmdBindMsaaState(
    const IMsaaState* pMsaaState)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindMsaaState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBindMsaaState(NextMsaaState(pMsaaState));
}

// =====================================================================================================================
void CmdBuffer::CmdBindColorBlendState(
    const IColorBlendState* pColorBlendState)
{
    if (m_annotations.logCmdBinds)
    {
       GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindColorBlendState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBindColorBlendState(NextColorBlendState(pColorBlendState));
}

// =====================================================================================================================
void CmdBuffer::CmdBindDepthStencilState(
    const IDepthStencilState* pDepthStencilState)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindDepthStencilState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBindDepthStencilState(NextDepthStencilState(pDepthStencilState));
}

// =====================================================================================================================
void CmdBuffer::CmdBindIndexData(
    gpusize   gpuAddr,
    uint32    indexCount,
    IndexType indexType)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindIndexData));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBindIndexData(gpuAddr, indexCount, indexType);
}

// =====================================================================================================================
void DumpColorTargetViewInfo(
    CmdBuffer*                       pCmdBuffer,
    const ColorTargetViewDecorator*  pView)
{
    if (pView != nullptr)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
        char*       pString        = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);
        ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
        const auto& viewCreateInfo = pView->GetCreateInfo();

        Snprintf(pString,
                 StringLength,
                 "\t\t\tView Format      = %s",
                 FormatToString(viewCreateInfo.swizzledFormat.format));
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t\t\tImage Swizzle    = ");
        SwizzleToString(viewCreateInfo.swizzledFormat.swizzle, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        if (viewCreateInfo.flags.isBufferView)
        {
            const auto&  bufferInfo = viewCreateInfo.bufferInfo;
            const auto*  pMemory    = bufferInfo.pGpuMemory;

            DumpGpuMemoryInfo(pCmdBuffer, pMemory, "", "\t\t");
            Snprintf(pString, StringLength, "\t\t\t\t{ offset = %d, extent = %d }",
                     bufferInfo.offset,
                     bufferInfo.extent);
            pNextCmdBuffer->CmdCommentString(pString);
        }
        else
        {
            const auto* pImage = viewCreateInfo.imageInfo.pImage;
            char        string[StringLength];

            Snprintf(pString, StringLength, "%s\t\t\tImage Pointer    = 0x%016" PRIXPTR, "", pImage);
            pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

            Snprintf(pString, StringLength, "");
            SubresIdToString(viewCreateInfo.imageInfo.baseSubRes, pString);
            Snprintf(&string[0], StringLength, "\t\t\t\t{ startSubres: %s, numSlices: 0x%x }",
                     pString, viewCreateInfo.imageInfo.arraySize);
            pNextCmdBuffer->CmdCommentString(&string[0]);

            if (pImage != nullptr)
            {
                const auto&  imageCreateInfo = pImage->GetImageCreateInfo();

                if ((imageCreateInfo.imageType == ImageType::Tex3d) &&
                    viewCreateInfo.flags.zRangeValid)
                {
                    Snprintf(&string[0],
                             StringLength,
                             "\t\t\t\t{ zRange: start:  %d, count: %d }",
                             viewCreateInfo.zRange.offset,
                             viewCreateInfo.zRange.extent);
                    pNextCmdBuffer->CmdCommentString(&string[0]);
                }
            }
        }

        PAL_SAFE_DELETE_ARRAY(pString, &allocator);
    }
}

// =====================================================================================================================
void DumpBindTargetParams(
    CmdBuffer*              pCmdBuffer,
    const BindTargetParams& params)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "params = [");
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\tcolorTargetCount = %d", params.colorTargetCount);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\tcolorTargets = {");
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        Snprintf(pString, StringLength, "\t\tColorTarget #%d = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        const auto& colorTarget = params.colorTargets[i];
        const auto* pView       = static_cast<const ColorTargetViewDecorator*>(colorTarget.pColorTargetView);

        Snprintf(pString, StringLength, "\t\t\tpColorTargetView = 0x%016" PRIXPTR, pView);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t\t\timageLayout      = ");
        ImageLayoutToString(colorTarget.imageLayout, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        DumpColorTargetViewInfo(pCmdBuffer, pView);

        Snprintf(pString, StringLength, "\t\t] // ColorTarget #%d", i);
        pNextCmdBuffer->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "\t } // colorTargets");
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\tdepthTarget = {");
    pNextCmdBuffer->CmdCommentString(pString);

    const auto& depthTarget = params.depthTarget;

    Snprintf(pString, StringLength, "\t\tpDepthStencilView = 0x%016" PRIXPTR, depthTarget.pDepthStencilView);;
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\t\tdepthLayout       = ");
    ImageLayoutToString(depthTarget.depthLayout, pString);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\t\tstencilLayout     = ");
    ImageLayoutToString(depthTarget.stencilLayout, pString);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "\t } // depthTarget");
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "] // params");
    pNextCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdBindTargets(
    const BindTargetParams& params)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindTargets));
        DumpBindTargetParams(this, params);
    }

    BindTargetParams nextParams = params;

    for (uint32 i = 0; i < params.colorTargetCount; i++)
    {
        nextParams.colorTargets[i].pColorTargetView = NextColorTargetView(params.colorTargets[i].pColorTargetView);
    }

    nextParams.depthTarget.pDepthStencilView = NextDepthStencilView(params.depthTarget.pDepthStencilView);

    GetNextLayer()->CmdBindTargets(nextParams);
}

// =====================================================================================================================
void CmdBuffer::CmdBindStreamOutTargets(
    const BindStreamOutTargetParams& params)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindStreamOutTargets));
    }

    // TODO: Add comment string.

    GetNextLayer()->CmdBindStreamOutTargets(params);
}

// =====================================================================================================================
static void CmdBindBorderColorPaletteToString(
    CmdBuffer*                 pCmdBuffer,
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    const char* pFormat = "PipelineBindPoint = %s";

    Snprintf(pString,
             StringLength,
             pFormat,
             (pipelineBindPoint == PipelineBindPoint::Compute) ? "PipelineBindPoint::Compute" :
                                                                 "PipelineBindPoint::Graphics");
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdBindBorderColorPalette(
    PipelineBindPoint          pipelineBindPoint,
    const IBorderColorPalette* pPalette)
{
    if (m_annotations.logCmdBinds)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBindBorderColorPalette));

        CmdBindBorderColorPaletteToString(this, pipelineBindPoint, pPalette);
    }

    GetNextLayer()->CmdBindBorderColorPalette(pipelineBindPoint, NextBorderColorPalette(pPalette));
}

// =====================================================================================================================
static void UserDataEntriesToString(
    CmdBuffer*        pCmdBuffer,
    uint32            entryCount,
    const uint32*     pEntryValues)
{
    pCmdBuffer->GetNextLayer()->CmdCommentString("Entries:");
    DataToString(pCmdBuffer, entryCount, pEntryValues, "\t");
}

// =====================================================================================================================
static void CmdSetUserDataToString(
    CmdBuffer*        pCmdBuffer,
    PipelineBindPoint userDataType,
    uint32            firstEntry,
    uint32            entryCount,
    const uint32*     pEntryValues)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "User Data Type = %s",
             (userDataType == PipelineBindPoint::Compute) ? "Compute" : "Graphics");
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "First Entry    = %u", firstEntry);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "Entry Count    = %u", entryCount);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);

    UserDataEntriesToString(pCmdBuffer, entryCount, pEntryValues);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataCs(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pCmdBuf->Annotations().logCmdSetUserData)
    {
        pCmdBuf->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetUserData));

        CmdSetUserDataToString(pCmdBuf, PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
    }

    pCmdBuf->GetNextLayer()->CmdSetUserData(PipelineBindPoint::Compute, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdSetUserDataGfx(
    ICmdBuffer*   pCmdBuffer,
    uint32        firstEntry,
    uint32        entryCount,
    const uint32* pEntryValues)
{
    auto* pCmdBuf = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pCmdBuf->Annotations().logCmdSetUserData)
    {
        pCmdBuf->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetUserData));

        CmdSetUserDataToString(pCmdBuf, PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
    }

    pCmdBuf->GetNextLayer()->CmdSetUserData(PipelineBindPoint::Graphics, firstEntry, entryCount, pEntryValues);
}

// =====================================================================================================================
static void CmdSetIndirectUserDataToString(
    CmdBuffer*  pCmdBuffer,
    uint16      tableId,
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "Table Id     = %hu", tableId);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "Dword Offset = %u", dwordOffset);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "Dword Size   = %u", dwordSize);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);

    UserDataEntriesToString(pCmdBuffer, dwordSize, static_cast<const uint32*>(pSrcData));
}

// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserData(
    uint16      tableId,
    uint32      dwordOffset,
    uint32      dwordSize,
    const void* pSrcData)
{
    if (m_annotations.logCmdSetUserData)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetIndirectUserData));

        CmdSetIndirectUserDataToString(this, tableId, dwordOffset, dwordSize, pSrcData);
    }

    GetNextLayer()->CmdSetIndirectUserData(tableId, dwordOffset, dwordSize, pSrcData);
}

// =====================================================================================================================
static void CmdSetIndirectUserDataWatermarkToString(
    CmdBuffer* pCmdBuffer,
    uint16     tableId,
    uint32     dwordLimit)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "Table Id     = %hu", tableId);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "Dword Limit  = %u", dwordLimit);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdSetIndirectUserDataWatermark(
    uint16 tableId,
    uint32 dwordLimit)
{
    if (m_annotations.logCmdSetUserData)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetIndirectUserDataWatermark));

        CmdSetIndirectUserDataWatermarkToString(this, tableId, dwordLimit);
    }

    GetNextLayer()->CmdSetIndirectUserDataWatermark(tableId, dwordLimit);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBlendConst(
    const BlendConstParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetBlendConst));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetBlendConst(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetInputAssemblyState(
    const InputAssemblyStateParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetInputAssemblyState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetInputAssemblyState(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetTriangleRasterState(
    const TriangleRasterStateParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetTriangleRasterState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetTriangleRasterState(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPointLineRasterState(
    const PointLineRasterStateParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetPointLineRasterState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetPointLineRasterState(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBiasState(
    const DepthBiasParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetDepthBiasState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetDepthBiasState(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetDepthBounds(
    const DepthBoundsParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetDepthBounds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetDepthBounds(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetStencilRefMasks(
    const StencilRefMaskParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetStencilRefMasks));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetStencilRefMasks(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetMsaaQuadSamplePattern(
    uint32                       numSamplesPerPixel,
    const MsaaQuadSamplePattern& quadSamplePattern)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetMsaaQuadSamplePattern));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetMsaaQuadSamplePattern(numSamplesPerPixel, quadSamplePattern);
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewports(
    const ViewportParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetViewports));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetViewports(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetScissorRects(
    const ScissorRectParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetScissorRects));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetScissorRects(params);
}

// =====================================================================================================================
void CmdBuffer::CmdSetGlobalScissor(
    const GlobalScissorParams& params)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetGlobalScissor));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetGlobalScissor(params);
}

// =====================================================================================================================
static const char* HwPipePointToString(
    HwPipePoint pipePoint)
{
    const char* pString = "";

    switch (pipePoint)
    {
    case HwPipeTop:
        pString = "HwPipeTop";
        break;

    // HwPipePostIndexFetch == HwPipePreCs == HwPipePreBlt
    case HwPipePostIndexFetch:
        pString = "HwPipePreCs || HwPipePreBlt || HwPipePostIndexFetch";
        break;
    case HwPipePreRasterization:
        pString = "HwPipePreRasterization";
        break;
    case HwPipePostPs:
        pString = "HwPipePostPs";
        break;
    case HwPipeBottom:
        pString = "HwPipeBottom";
        break;
    case HwPipePostCs:
        pString = "HwPipePostCs";
        break;
    case HwPipePostBlt:
        pString = "HwPipePostBlt";
        break;
    }

    static_assert(((HwPipePostIndexFetch == HwPipePreCs) && (HwPipePostIndexFetch == HwPipePreBlt)), "");

    return pString;
}

// =====================================================================================================================
static void DumpMsaaQuadSamplePattern(
    CmdBuffer*                   pCmdBuffer,
    const MsaaQuadSamplePattern& quadSamplePattern,
    const char*                  pTitle,
    const char*                  pHeader)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "%s%s = [", pHeader, pTitle);
    pCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\ttopLeft = [", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < MaxMsaaRasterizerSamples; i++)
    {
        Snprintf(pString, StringLength, "%s\t\t Pattern %d = ", pHeader, i);
        Offset2dToString(quadSamplePattern.topLeft[i], pString);
        pCmdBuffer->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "%s\t]", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\ttopRight = [", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < MaxMsaaRasterizerSamples; i++)
    {
        Snprintf(pString, StringLength, "%s\t\t Pattern %d = ", pHeader, i);
        Offset2dToString(quadSamplePattern.topRight[i], pString);
        pCmdBuffer->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "%s\t]", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\tbottomLeft = [", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < MaxMsaaRasterizerSamples; i++)
    {
        Snprintf(pString, StringLength, "%s\t\t Pattern %d = ", pHeader, i);
        Offset2dToString(quadSamplePattern.bottomLeft[i], pString);
        pCmdBuffer->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "%s\t]", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s\tbottomRight = [", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < MaxMsaaRasterizerSamples; i++)
    {
        Snprintf(pString, StringLength, "%s\t\t Pattern %d = ", pHeader, i);
        Offset2dToString(quadSamplePattern.bottomRight[i], pString);
        pCmdBuffer->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "%s\t]", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "%s]", pHeader);
    pCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void BarrierTransitionToString(
    CmdBuffer*               pCmdBuffer,
    uint32                   index,
    const BarrierTransition& transition,
    char                     string[StringLength])
{
    Snprintf(&string[0], StringLength, "barrierInfo.pTransitions[%u] = {", index);
    pCmdBuffer->CmdCommentString(&string[0]);

    Snprintf(&string[0], StringLength, "\tsrcCacheMask = 0x%08X", transition.srcCacheMask);
    pCmdBuffer->CmdCommentString(&string[0]);
    Snprintf(&string[0], StringLength, "\tdstCacheMask = 0x%08X", transition.dstCacheMask);
    pCmdBuffer->CmdCommentString(&string[0]);

    pCmdBuffer->CmdCommentString("\timageInfo = [");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    if (transition.imageInfo.pImage != nullptr)
    {
        DumpImageInfo(pCmdBuffer, transition.imageInfo.pImage, "pImage", "\t\t");

        SubresRangeToString(pCmdBuffer, transition.imageInfo.subresRange, pString);
        Snprintf(&string[0], StringLength, "\t\tsubresRange = %s", pString);
        pCmdBuffer->CmdCommentString(&string[0]);

        Snprintf(&string[0], StringLength, "\t\toldLayout = ");
        ImageLayoutToString(transition.imageInfo.oldLayout, &string[0]);
        pCmdBuffer->CmdCommentString(&string[0]);

        Snprintf(&string[0], StringLength, "\t\tnewLayout = ");
        ImageLayoutToString(transition.imageInfo.newLayout, &string[0]);
        pCmdBuffer->CmdCommentString(&string[0]);

        if (transition.imageInfo.pQuadSamplePattern != nullptr)
        {
            DumpMsaaQuadSamplePattern(
                pCmdBuffer, *transition.imageInfo.pQuadSamplePattern, "pQuadSamplePattern", "\t\t");
        }
    }
    else
    {
        Snprintf(&string[0], StringLength, "\t\tpImage = 0x%016" PRIXPTR, transition.imageInfo.pImage);
        pCmdBuffer->CmdCommentString(&string[0]);
    }

    pCmdBuffer->CmdCommentString("\t]");
    pCmdBuffer->CmdCommentString("}");

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void CmdBarrierToString(
    CmdBuffer*         pCmdBuffer,
    const BarrierInfo& barrierInfo)
{
    pCmdBuffer->GetNextLayer()->CmdCommentString("BarrierInfo:");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "barrierInfo.flags = 0x%0X", barrierInfo.flags);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.waitPoint = %s", HwPipePointToString(barrierInfo.waitPoint));
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.pipePointWaitCount = %u", barrierInfo.pipePointWaitCount);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.pipePointWaitCount; i++)
    {
        Snprintf(pString, StringLength,
                 "barrierInfo.pPipePoints[%u] = %s", i, HwPipePointToString(barrierInfo.pPipePoints[i]));
        pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    }

    Snprintf(pString, StringLength, "barrierInfo.gpuEventWaitCount = %u", barrierInfo.gpuEventWaitCount);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength,
             "barrierInfo.rangeCheckedTargetWaitCount = %u", barrierInfo.rangeCheckedTargetWaitCount);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.transitionCount = %u", barrierInfo.transitionCount);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
    {
        BarrierTransitionToString(pCmdBuffer, i, barrierInfo.pTransitions[i], pString);
    }

    Snprintf(pString, StringLength,
             "barrierInfo.pSplitBarrierGpuEvent = 0x%016" PRIXPTR, barrierInfo.pSplitBarrierGpuEvent);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdBarrier(
    const BarrierInfo& barrierInfo)
{
    if (m_annotations.logCmdBarrier)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBarrier));

        CmdBarrierToString(this, barrierInfo);
    }

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(&m_allocator, false);
    BarrierInfo        nextBarrierInfo = barrierInfo;
    const IGpuEvent**  ppGpuEvents     = nullptr;
    const IImage**     ppTargets       = nullptr;
    BarrierTransition* pTransitions    = nullptr;

    if (barrierInfo.gpuEventWaitCount > 0)
    {
        ppGpuEvents = PAL_NEW_ARRAY(const IGpuEvent*, barrierInfo.gpuEventWaitCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < barrierInfo.gpuEventWaitCount; i++)
        {
            ppGpuEvents[i] = NextGpuEvent(barrierInfo.ppGpuEvents[i]);
        }

        nextBarrierInfo.ppGpuEvents = ppGpuEvents;
    }

    if (barrierInfo.rangeCheckedTargetWaitCount > 0)
    {
        ppTargets =
            PAL_NEW_ARRAY(const IImage*, barrierInfo.rangeCheckedTargetWaitCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < barrierInfo.rangeCheckedTargetWaitCount; i++)
        {
            ppTargets[i] = NextImage(barrierInfo.ppTargets[i]);
        }

        nextBarrierInfo.ppTargets = ppTargets;
    }

    if (barrierInfo.transitionCount > 0)
    {
        pTransitions = PAL_NEW_ARRAY(BarrierTransition, barrierInfo.transitionCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < barrierInfo.transitionCount; i++)
        {
            pTransitions[i]                  = barrierInfo.pTransitions[i];
            pTransitions[i].imageInfo.pImage = NextImage(barrierInfo.pTransitions[i].imageInfo.pImage);
        }

        nextBarrierInfo.pTransitions = pTransitions;
    }

    nextBarrierInfo.pSplitBarrierGpuEvent = NextGpuEvent(barrierInfo.pSplitBarrierGpuEvent);

    GetNextLayer()->CmdBarrier(nextBarrierInfo);

    if (m_singleStep.waitIdleDispatches)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBarriers)
    {
        AddTimestamp();
    }

    PAL_SAFE_DELETE_ARRAY(ppGpuEvents, &allocator);
    PAL_SAFE_DELETE_ARRAY(ppTargets, &allocator);
    PAL_SAFE_DELETE_ARRAY(pTransitions, &allocator);
}

// =====================================================================================================================
// Called because of a callback informing this layer about a barrier within a lower layer. Annotates the command buffer
// before the specifics of this barrier with a comment describing this barrier.
void CmdBuffer::DescribeBarrier(
    const Developer::BarrierData* pData)
{
    if (pData->hasTransition)
    {
        LinearAllocatorAuto<VirtualLinearAllocator> allocator(Allocator(), false);

        const auto& imageInfo = pData->transition.imageInfo.pImage->GetImageCreateInfo();
        char*       pString   = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

        Snprintf(&pString[0], StringLength,
            "ImageInfo: %ux%u %s - %s",
            imageInfo.extent.width, imageInfo.extent.height,
            FormatToString(imageInfo.swizzledFormat.format),
            ImageAspectToString(pData->transition.imageInfo.subresRange.startSubres.aspect));

        GetNextLayer()->CmdCommentString(pString);

        PAL_SAFE_DELETE_ARRAY(pString, &allocator);
    }

    GetNextLayer()->CmdCommentString("PipelineStalls = {");

    if (pData->operations.pipelineStalls.waitOnEopTsBottomOfPipe)
    {
        GetNextLayer()->CmdCommentString("\twaitOnEopTsBottomOfPipe");
    }

    if (pData->operations.pipelineStalls.vsPartialFlush)
    {
        GetNextLayer()->CmdCommentString("\tvsPartialFlush");
    }

    if (pData->operations.pipelineStalls.psPartialFlush)
    {
        GetNextLayer()->CmdCommentString("\tpsPartialFlush");
    }

    if (pData->operations.pipelineStalls.csPartialFlush)
    {
        GetNextLayer()->CmdCommentString("\tcsPartialFlush");
    }

    if (pData->operations.pipelineStalls.pfpSyncMe)
    {
        GetNextLayer()->CmdCommentString("\tpfpSyncMe");
    }

    if (pData->operations.pipelineStalls.syncCpDma)
    {
        GetNextLayer()->CmdCommentString("\tsyncCpDma");
    }

    GetNextLayer()->CmdCommentString("}");

    GetNextLayer()->CmdCommentString("LayoutTransitions = {");

    if (pData->operations.layoutTransitions.depthStencilExpand)
    {
        GetNextLayer()->CmdCommentString("\tdepthStencilExpand");
    }

    if (pData->operations.layoutTransitions.htileHiZRangeExpand)
    {
        GetNextLayer()->CmdCommentString("\thtileHiZRangeExpand");
    }

    if (pData->operations.layoutTransitions.depthStencilResummarize)
    {
        GetNextLayer()->CmdCommentString("\tdepthStencilResummarize");
    }

    if (pData->operations.layoutTransitions.dccDecompress)
    {
        GetNextLayer()->CmdCommentString("\tdccDecompress");
    }

    if (pData->operations.layoutTransitions.fmaskDecompress)
    {
        GetNextLayer()->CmdCommentString("\tfmaskDecompress");
    }

    if (pData->operations.layoutTransitions.fastClearEliminate)
    {
        GetNextLayer()->CmdCommentString("\tfastClearEliminate");
    }

    if (pData->operations.layoutTransitions.fmaskColorExpand)
    {
        GetNextLayer()->CmdCommentString("\tfmaskColorExpand");
    }

    if (pData->operations.layoutTransitions.initMaskRam)
    {
        GetNextLayer()->CmdCommentString("\tinitMaskRam");
    }

    GetNextLayer()->CmdCommentString("}");

    GetNextLayer()->CmdCommentString("Caches = {");

    if (pData->operations.caches.invalTcp)
    {
        GetNextLayer()->CmdCommentString("\tinvalTcp");
    }

    if (pData->operations.caches.invalSqI$)
    {
        GetNextLayer()->CmdCommentString("\tinvalSqI$");
    }

    if (pData->operations.caches.invalSqK$)
    {
        GetNextLayer()->CmdCommentString("\tinvalSqK$");
    }

    if (pData->operations.caches.flushTcc)
    {
        GetNextLayer()->CmdCommentString("\tflushTcc");
    }

    if (pData->operations.caches.invalTcc)
    {
        GetNextLayer()->CmdCommentString("\tinvalTcc");
    }

    if (pData->operations.caches.flushCb)
    {
        GetNextLayer()->CmdCommentString("\tflushCb");
    }

    if (pData->operations.caches.invalCb)
    {
        GetNextLayer()->CmdCommentString("\tinvalCb");
    }

    if (pData->operations.caches.flushDb)
    {
        GetNextLayer()->CmdCommentString("\tflushDb");
    }

    if (pData->operations.caches.invalDb)
    {
        GetNextLayer()->CmdCommentString("\tinvalDb");
    }

    if (pData->operations.caches.invalCbMetadata)
    {
        GetNextLayer()->CmdCommentString("\tinvalCbMetadata");
    }

    if (pData->operations.caches.flushCbMetadata)
    {
        GetNextLayer()->CmdCommentString("\tflushCbMetadata");
    }

    if (pData->operations.caches.invalDbMetadata)
    {
        GetNextLayer()->CmdCommentString("\tinvalDbMetadata");
    }

    if (pData->operations.caches.flushDbMetadata)
    {
        GetNextLayer()->CmdCommentString("\tflushDbMetadata");
    }

    GetNextLayer()->CmdCommentString("}");
}

// =====================================================================================================================
// Adds single-step and timestamp logic for any internal draws/dispatches that the internal PAL core might do.
void CmdBuffer::HandleDrawDispatch(
    bool isDraw)
{
    const bool timestampEvent = (isDraw) ? m_singleStep.timestampDraws : m_singleStep.timestampDispatches;
    const bool waitIdleEvent  = (isDraw) ? m_singleStep.waitIdleDraws  : m_singleStep.waitIdleDispatches;

    if (waitIdleEvent)
    {
        AddSingleStepBarrier();
    }

    if (timestampEvent)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
static void MemoryBarrierTransitionToString(
    CmdBuffer*        pCmdBuffer,
    uint32            index,
    const MemBarrier& transition,
    char              string[StringLength])
{
    Snprintf(&string[0], StringLength, "barrierInfo.pMemoryBarriers[%u] = {", index);
    pCmdBuffer->CmdCommentString(&string[0]);

    pCmdBuffer->CmdCommentString("\tmemory = [");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    DumpGpuMemoryInfo(pCmdBuffer, transition.memory.pGpuMemory, "Bound GpuMemory", "\t\t");

    Snprintf(pString, StringLength, "%s\t offset = 0x%016llX", "Bound GpuMemory", transition.memory.offset);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);
    Snprintf(pString, StringLength, "%s\t Size   = 0x%016llX", "Bound GpuMemory", transition.memory.size);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    pCmdBuffer->CmdCommentString("\t]");

    Snprintf(pString, StringLength, "srcAccessMask = 0x%0X", transition.srcAccessMask);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    Snprintf(pString, StringLength, "dstAccessMask = 0x%0X", transition.dstAccessMask);
    pCmdBuffer->GetNextLayer()->CmdCommentString(pString);

    pCmdBuffer->CmdCommentString("}");

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void ImageBarrierTransitionToString(
    CmdBuffer*        pCmdBuffer,
    uint32            index,
    const ImgBarrier& transition,
    char              string[StringLength])
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    Snprintf(&string[0], StringLength, "barrierInfo.pImageBarriers[%u] = {", index);
    pNextCmdBuffer->CmdCommentString(&string[0]);

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    if (transition.pImage != nullptr)
    {
        DumpImageInfo(pCmdBuffer, transition.pImage, "pImage", "\t\t");

        SubresRangeToString(pCmdBuffer, transition.subresRange, pString);
        Snprintf(&string[0], StringLength, "\t\tsubresRange = %s", pString);
        pNextCmdBuffer->CmdCommentString(&string[0]);

        pNextCmdBuffer->CmdCommentString("\tBox = {");

        Snprintf(pString, StringLength, "\t\t");
        Offset3dToString(transition.box.offset, pString);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t\t");
        Extent3dToString(transition.box.extent, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        pNextCmdBuffer->CmdCommentString("\t}");

        Snprintf(pString, StringLength, "srcAccessMask = 0x%0X", transition.srcAccessMask);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "dstAccessMask = 0x%0X", transition.dstAccessMask);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(&string[0], StringLength, "\t\toldLayout = ");
        ImageLayoutToString(transition.oldLayout, &string[0]);
        pNextCmdBuffer->CmdCommentString(&string[0]);

        Snprintf(&string[0], StringLength, "\t\tnewLayout = ");
        ImageLayoutToString(transition.newLayout, &string[0]);
        pNextCmdBuffer->CmdCommentString(&string[0]);

        if (transition.pQuadSamplePattern != nullptr)
        {
            DumpMsaaQuadSamplePattern(
                pCmdBuffer, *transition.pQuadSamplePattern, "pQuadSamplePattern", "\t\t");
        }
    }
    else
    {
        Snprintf(&string[0], StringLength, "\t\tpImage = 0x%016" PRIXPTR, transition.pImage);
        pNextCmdBuffer->CmdCommentString(&string[0]);
    }

    pNextCmdBuffer->CmdCommentString("}");

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void CmdReleaseToString(
    CmdBuffer*                pCmdBuffer,
    const AcquireReleaseInfo& barrierInfo,
    const IGpuEvent*          pGpuEvent)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    pNextCmdBuffer->CmdCommentString("AcquireReleaseInfo:");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "acquireReleaseInfo.srcStageMask = 0x%0X", barrierInfo.srcStageMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "acquireReleaseInfo.srcGlobalAccessMask = 0x%0X", barrierInfo.srcGlobalAccessMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.memoryBarrierCount = %u", barrierInfo.memoryBarrierCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        MemoryBarrierTransitionToString(pCmdBuffer, i, barrierInfo.pMemoryBarriers[i], pString);
    }

    Snprintf(pString, StringLength, "barrierInfo.imageBarrierCount = %u", barrierInfo.imageBarrierCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        ImageBarrierTransitionToString(pCmdBuffer, i, barrierInfo.pImageBarriers[i], pString);
    }

    pNextCmdBuffer->CmdCommentString("IGpuEvent:");
    Snprintf(pString, StringLength,
        "pGpuEvent = 0x%016" PRIXPTR, pGpuEvent);
    pNextCmdBuffer->CmdCommentString(pString);

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void CmdAcquireToString(
    CmdBuffer*                pCmdBuffer,
    const AcquireReleaseInfo& barrierInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    pNextCmdBuffer->CmdCommentString("AcquireReleaseInfo:");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "acquireReleaseInfo.dstStageMask = 0x%0X", barrierInfo.dstStageMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "acquireReleaseInfo.dstGlobalAccessMask = 0x%0X", barrierInfo.dstGlobalAccessMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.memoryBarrierCount = %u", barrierInfo.memoryBarrierCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        MemoryBarrierTransitionToString(pCmdBuffer, i, barrierInfo.pMemoryBarriers[i], pString);
    }

    Snprintf(pString, StringLength, "gpuEventCount = %u", gpuEventCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < gpuEventCount; i++)
    {
        pNextCmdBuffer->CmdCommentString("IGpuEvent:");
        Snprintf(pString, StringLength,
            "pGpuEvent = 0x%016" PRIXPTR, ppGpuEvents[i]);
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void CmdAcquireReleaseToString(
    CmdBuffer*                pCmdBuffer,
    const AcquireReleaseInfo& barrierInfo)
{
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();
    pNextCmdBuffer->CmdCommentString("AcquireReleaseInfo:");

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    Snprintf(pString, StringLength, "acquireReleaseInfo.srcStageMask = 0x%0X", barrierInfo.srcStageMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "acquireReleaseInfo.dstStageMask = 0x%0X", barrierInfo.dstStageMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "acquireReleaseInfo.srcGlobalAccessMask = 0x%0X", barrierInfo.srcGlobalAccessMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "acquireReleaseInfo.dstGlobalAccessMask = 0x%0X", barrierInfo.dstGlobalAccessMask);
    pNextCmdBuffer->CmdCommentString(pString);

    Snprintf(pString, StringLength, "barrierInfo.memoryBarrierCount = %u", barrierInfo.memoryBarrierCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
    {
        MemoryBarrierTransitionToString(pCmdBuffer, i, barrierInfo.pMemoryBarriers[i], pString);
    }

    Snprintf(pString, StringLength, "barrierInfo.imageBarrierCount = %u", barrierInfo.imageBarrierCount);
    pNextCmdBuffer->CmdCommentString(pString);

    for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
    {
        ImageBarrierTransitionToString(pCmdBuffer, i, barrierInfo.pImageBarriers[i], pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdRelease(
    const AcquireReleaseInfo& releaseInfo,
    const IGpuEvent*          pGpuEvent)
{
    if (m_annotations.logCmdBarrier)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdRelease));

        CmdReleaseToString(this, releaseInfo, pGpuEvent);
    }

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(&m_allocator, false);
    AcquireReleaseInfo nextReleaseInfo = releaseInfo;
    MemBarrier*        pMemoryBarriers = nullptr;
    ImgBarrier*        pImageBarriers  = nullptr;
    const IGpuEvent*   pNextGpuEvent   = NextGpuEvent(pGpuEvent);

    if (releaseInfo.memoryBarrierCount > 0)
    {
        pMemoryBarriers = PAL_NEW_ARRAY(MemBarrier, releaseInfo.memoryBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < releaseInfo.memoryBarrierCount; i++)
        {
            pMemoryBarriers[i] = releaseInfo.pMemoryBarriers[i];
            pMemoryBarriers[i].memory.pGpuMemory = NextGpuMemory(releaseInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }

        nextReleaseInfo.pMemoryBarriers = pMemoryBarriers;
    }

    if (releaseInfo.imageBarrierCount > 0)
    {
        pImageBarriers = PAL_NEW_ARRAY(ImgBarrier, releaseInfo.imageBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < releaseInfo.imageBarrierCount; i++)
        {
            pImageBarriers[i] = releaseInfo.pImageBarriers[i];
            pImageBarriers[i].pImage = NextImage(releaseInfo.pImageBarriers[i].pImage);
        }

        nextReleaseInfo.pImageBarriers = pImageBarriers;
    }

    GetNextLayer()->CmdRelease(nextReleaseInfo, pNextGpuEvent);

    PAL_SAFE_DELETE_ARRAY(pMemoryBarriers, &allocator);
    PAL_SAFE_DELETE_ARRAY(pImageBarriers, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdAcquire(
    const AcquireReleaseInfo& acquireInfo,
    uint32                    gpuEventCount,
    const IGpuEvent*const*    ppGpuEvents)
{
    if (m_annotations.logCmdBarrier)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdAcquire));

        CmdAcquireToString(this, acquireInfo, gpuEventCount, ppGpuEvents);
    }

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(&m_allocator, false);
    AcquireReleaseInfo     nextAcquireInfo = acquireInfo;
    MemBarrier*            pMemoryBarriers = nullptr;
    ImgBarrier*            pImageBarriers  = nullptr;
    const IGpuEvent** ppNextGpuEvents = nullptr;

    if (acquireInfo.memoryBarrierCount > 0)
    {
        pMemoryBarriers = PAL_NEW_ARRAY(MemBarrier, acquireInfo.memoryBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < acquireInfo.memoryBarrierCount; i++)
        {
            pMemoryBarriers[i] = acquireInfo.pMemoryBarriers[i];
            pMemoryBarriers[i].memory.pGpuMemory = NextGpuMemory(acquireInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }

        nextAcquireInfo.pMemoryBarriers = pMemoryBarriers;
    }

    if (acquireInfo.imageBarrierCount > 0)
    {
        pImageBarriers = PAL_NEW_ARRAY(ImgBarrier, acquireInfo.imageBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < acquireInfo.imageBarrierCount; i++)
        {
            pImageBarriers[i] = acquireInfo.pImageBarriers[i];
            pImageBarriers[i].pImage = NextImage(acquireInfo.pImageBarriers[i].pImage);
        }

        nextAcquireInfo.pImageBarriers = pImageBarriers;
    }

    if (gpuEventCount > 0)
    {
        ppNextGpuEvents = PAL_NEW_ARRAY(const IGpuEvent*, gpuEventCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < gpuEventCount; i++)
        {
            ppNextGpuEvents[i] = NextGpuEvent(ppGpuEvents[i]);
        }

        nextAcquireInfo.pImageBarriers = pImageBarriers;
    }

    GetNextLayer()->CmdAcquire(nextAcquireInfo, gpuEventCount, ppNextGpuEvents);

    PAL_SAFE_DELETE_ARRAY(pMemoryBarriers, &allocator);
    PAL_SAFE_DELETE_ARRAY(pImageBarriers, &allocator);
    PAL_SAFE_DELETE_ARRAY(ppNextGpuEvents, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdReleaseThenAcquire(
    const AcquireReleaseInfo& barrierInfo)
{
    if (m_annotations.logCmdBarrier)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdReleaseThenAcquire));

        CmdAcquireReleaseToString(this, barrierInfo);
    }

    LinearAllocatorAuto<VirtualLinearAllocator> allocator(&m_allocator, false);
    AcquireReleaseInfo nextBarrierInfo = barrierInfo;
    MemBarrier*        pMemoryBarriers = nullptr;
    ImgBarrier*        pImageBarriers  = nullptr;

    if (barrierInfo.memoryBarrierCount > 0)
    {
        pMemoryBarriers = PAL_NEW_ARRAY(MemBarrier, barrierInfo.memoryBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < barrierInfo.memoryBarrierCount; i++)
        {
            pMemoryBarriers[i] = barrierInfo.pMemoryBarriers[i];
            pMemoryBarriers[i].memory.pGpuMemory = NextGpuMemory(barrierInfo.pMemoryBarriers[i].memory.pGpuMemory);
        }

        nextBarrierInfo.pMemoryBarriers = pMemoryBarriers;
    }

    if (barrierInfo.imageBarrierCount > 0)
    {
        pImageBarriers = PAL_NEW_ARRAY(ImgBarrier, barrierInfo.imageBarrierCount, &allocator, AllocInternalTemp);

        for (uint32 i = 0; i < barrierInfo.imageBarrierCount; i++)
        {
            pImageBarriers[i] = barrierInfo.pImageBarriers[i];
            pImageBarriers[i].pImage = NextImage(barrierInfo.pImageBarriers[i].pImage);
        }

        nextBarrierInfo.pImageBarriers = pImageBarriers;
    }

    GetNextLayer()->CmdReleaseThenAcquire(nextBarrierInfo);

    PAL_SAFE_DELETE_ARRAY(pMemoryBarriers, &allocator);
    PAL_SAFE_DELETE_ARRAY(pImageBarriers, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitRegisterValue(
    uint32      registerOffset,
    uint32      data,
    uint32      mask,
    CompareFunc compareFunc)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWaitRegisterValue));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWaitRegisterValue(registerOffset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitMemoryValue(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWaitMemoryValue));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWaitMemoryValue(*NextGpuMemory(&gpuMemory), offset, data, mask, compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdWaitBusAddressableMemoryMarker(
    const IGpuMemory& gpuMemory,
    uint32            data,
    uint32            mask,
    CompareFunc       compareFunc)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWaitBusAddressableMemoryMarker));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWaitBusAddressableMemoryMarker(*NextGpuMemory(&gpuMemory), data, mask, compareFunc);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDraw(
    ICmdBuffer* pCmdBuffer,
    uint32      firstVertex,
    uint32      vertexCount,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDraws)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDraw));

        LinearAllocatorAuto<VirtualLinearAllocator> allocator(pThis->Allocator(), false);
        char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

        Snprintf(pString, StringLength, "First Vertex   = 0x%08x", firstVertex);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Vertex Count   = 0x%08x", vertexCount);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "First Instance = 0x%08x", firstInstance);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Instance Count = 0x%08x", instanceCount);
        pThis->GetNextLayer()->CmdCommentString(pString);

        PAL_SAFE_DELETE_ARRAY(pString, &allocator);
    }

    pThis->GetNextLayer()->CmdDraw(firstVertex, vertexCount, firstInstance, instanceCount);

    pThis->HandleDrawDispatch(true);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawOpaque(
    ICmdBuffer* pCmdBuffer,
    gpusize streamOutFilledSizeVa,
    uint32  streamOutOffset,
    uint32  stride,
    uint32  firstInstance,
    uint32  instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDraws)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDrawOpaque));
        // TODO: Add comment string.
    }

    pThis->GetNextLayer()->CmdDrawOpaque(streamOutFilledSizeVa, streamOutOffset, stride, firstInstance, instanceCount);

    pThis->HandleDrawDispatch(true);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexed(
    ICmdBuffer* pCmdBuffer,
    uint32      firstIndex,
    uint32      indexCount,
    int32       vertexOffset,
    uint32      firstInstance,
    uint32      instanceCount)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDraws)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDrawIndexed));

        LinearAllocatorAuto<VirtualLinearAllocator> allocator(pThis->Allocator(), false);
        char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

        Snprintf(pString, StringLength, "First Index    = 0x%08x", firstIndex);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Index Count    = 0x%08x", indexCount);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Vertex Offset  = 0x%08x", vertexOffset);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "First Instance = 0x%08x", firstInstance);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Instance Count = 0x%08x", instanceCount);
        pThis->GetNextLayer()->CmdCommentString(pString);

        PAL_SAFE_DELETE_ARRAY(pString, &allocator);
    }

    pThis->GetNextLayer()->CmdDrawIndexed(firstIndex, indexCount, vertexOffset, firstInstance, instanceCount);

    pThis->HandleDrawDispatch(true);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDraws)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDrawIndirectMulti));

        // TODO: Add comment string.
    }

    pThis->GetNextLayer()->CmdDrawIndirectMulti(*NextGpuMemory(&gpuMemory), offset, stride, maximumCount, countGpuAddr);

    pThis->HandleDrawDispatch(true);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDrawIndexedIndirectMulti(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint32            stride,
    uint32            maximumCount,
    gpusize           countGpuAddr)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDraws)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDrawIndexedIndirectMulti));

        // TODO: Add comment string.
    }

    pThis->GetNextLayer()->CmdDrawIndexedIndirectMulti(*NextGpuMemory(&gpuMemory),
                                                       offset,
                                                       stride,
                                                       maximumCount,
                                                       countGpuAddr);

    pThis->HandleDrawDispatch(true);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatch(
    ICmdBuffer* pCmdBuffer,
    uint32      x,
    uint32      y,
    uint32      z)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDispatchs)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDispatch));

        LinearAllocatorAuto<VirtualLinearAllocator> allocator(pThis->Allocator(), false);
        char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

        Snprintf(pString, StringLength, "X = 0x%08x", x);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Y = 0x%08x", y);
        pThis->GetNextLayer()->CmdCommentString(pString);
        Snprintf(pString, StringLength, "Z = 0x%08x", z);
        pThis->GetNextLayer()->CmdCommentString(pString);

        PAL_SAFE_DELETE_ARRAY(pString, &allocator);
    }

    pThis->GetNextLayer()->CmdDispatch(x, y, z);

    pThis->HandleDrawDispatch(false);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchIndirect(
    ICmdBuffer*       pCmdBuffer,
    const IGpuMemory& gpuMemory,
    gpusize           offset)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDispatchs)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDispatchIndirect));

        // TODO: Add comment string.
    }

    pThis->GetNextLayer()->CmdDispatchIndirect(*NextGpuMemory(&gpuMemory), offset);

    pThis->HandleDrawDispatch(false);
}

// =====================================================================================================================
void PAL_STDCALL CmdBuffer::CmdDispatchOffset(
    ICmdBuffer* pCmdBuffer,
    uint32      xOffset,
    uint32      yOffset,
    uint32      zOffset,
    uint32      xDim,
    uint32      yDim,
    uint32      zDim)
{
    auto* pThis = static_cast<CmdBuffer*>(pCmdBuffer);

    if (pThis->m_annotations.logCmdDispatchs)
    {
        pThis->GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDispatchOffset));

        // TODO: Add comment string.
    }

    pThis->GetNextLayer()->CmdDispatchOffset(xOffset, yOffset, zOffset, xDim, yDim, zDim);

    pThis->HandleDrawDispatch(false);
}

// =====================================================================================================================
void CmdBuffer::CmdStartGpuProfilerLogging()
{
    if (m_annotations.logCmdDispatchs)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdStartGpuProfilerLogging));
    }

    GetNextLayer()->CmdStartGpuProfilerLogging();
}

// =====================================================================================================================
void CmdBuffer::CmdStopGpuProfilerLogging()
{
    if (m_annotations.logCmdDispatchs)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdStopGpuProfilerLogging));
    }

    GetNextLayer()->CmdStopGpuProfilerLogging();
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dataSize,
    const uint32*     pData)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdUpdateMemory));
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdUpdateMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, dataSize, pData);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateBusAddressableMemoryMarker(
    const IGpuMemory& dstGpuMemory,
    gpusize           offset,
    uint32            value)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdUpdateBusAddressableMemoryMarker));
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdUpdateBusAddressableMemoryMarker(*NextGpuMemory(&dstGpuMemory), offset, value);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdFillMemory(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           fillSize,
    uint32            data)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdFillMemory));
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdFillMemory(*NextGpuMemory(&dstGpuMemory), dstOffset, fillSize, data);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTypedBuffer(
    const IGpuMemory&            srcGpuMemory,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const TypedBufferCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyTypedBuffer));
        DumpGpuMemoryInfo(this, &srcGpuMemory, "srcGpuMemory", "");
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyTypedBuffer(*NextGpuMemory(&srcGpuMemory),
                                       *NextGpuMemory(&dstGpuMemory),
                                       regionCount,
                                       pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyRegisterToMemory(
    uint32            srcRegisterOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyRegisterToMemory));
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyRegisterToMemory(srcRegisterOffset, *NextGpuMemory(&dstGpuMemory), dstOffset);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
static void DumpImageCopyRegion(
    CmdBuffer*             pCmdBuffer,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    for (uint32 i = 0; i < regionCount; i++)
    {
        const auto& region = pRegions[i];

        Snprintf(pString, StringLength, "Region %u = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcSubres  = ");
        SubresIdToString(region.srcSubres, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcOffset  = ");
        Offset3dToString(region.srcOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstSubres  = ");
        SubresIdToString(region.dstSubres, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstOffset  = ");
        Offset3dToString(region.dstOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t extent     = ");
        Extent3dToString(region.extent, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t numSlices  = %u",   region.numSlices);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpImageResolveRegion(
    CmdBuffer*                pCmdBuffer,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    for (uint32 i = 0; i < regionCount; i++)
    {
        const auto& region = pRegions[i];

        Snprintf(pString, StringLength, "Region %u = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcAspect  = %s", ImageAspectToString(region.srcAspect));
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcSlice   = 0x%x", region.srcSlice);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcOffset  = ");
        Offset3dToString(region.srcOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstAspect  = %s", ImageAspectToString(region.dstAspect));
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstSlice   = 0x%x", region.dstSlice);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstOffset  = ");
        Offset3dToString(region.dstOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t extent     = ");
        Extent3dToString(region.extent, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t numSlices  = %u", region.numSlices);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t swizzledFormat = { format = %s, swizzle = ",
                 FormatToString(region.swizzledFormat.format));
        SwizzleToString(region.swizzledFormat.swizzle, pString);

        const size_t currentLength = strlen(pString);
        Snprintf(pString + currentLength, StringLength - currentLength, " }");
        pNextCmdBuffer->CmdCommentString(pString);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 406
        if (region.pQuadSamplePattern != nullptr)
        {
            DumpMsaaQuadSamplePattern(pCmdBuffer, *region.pQuadSamplePattern, "pQuadSamplePattern", "\t\t");
        }
#endif

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpResolveMode(
    CmdBuffer*  pCmdBuffer,
    ResolveMode resolveMode)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);
    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    switch (resolveMode)
    {
    case ResolveMode::Average:
        Snprintf(pString, StringLength, "ResolveMode: Average");
        break;
    case ResolveMode::Minimum:
        Snprintf(pString, StringLength, "ResolveMode: Min");
        break;
    case ResolveMode::Maximum:
        Snprintf(pString, StringLength, "ResolveMode: Max");
        break;
    default:
        Snprintf(pString, StringLength, "ResolveMode: Unknown");
        PAL_NEVER_CALLED();
        break;
    }
    pNextCmdBuffer->CmdCommentString(pString);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImage(
    const IImage&          srcImage,
    ImageLayout            srcImageLayout,
    const IImage&          dstImage,
    ImageLayout            dstImageLayout,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    uint32                 flags)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyImage));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageLayout(this, srcImageLayout, "srcImageLayout");
        DumpImageInfo(this, &dstImage, "dstImage", "");
        DumpImageLayout(this, dstImageLayout, "dstImageLayout");
        DumpImageCopyRegion(this, regionCount, pRegions);

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyImage(*NextImage(&srcImage),
                              srcImageLayout,
                              *NextImage(&dstImage),
                              dstImageLayout,
                              regionCount,
                              pRegions,
                              flags);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdScaledCopyImage(
    const ScaledCopyInfo&        copyInfo)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdScaledCopyImage));
        DumpImageInfo(this, copyInfo.pSrcImage, "srcImage", "");
        DumpImageLayout(this, copyInfo.srcImageLayout, "srcImageLayout");
        DumpImageInfo(this, copyInfo.pDstImage, "dstImage", "");
        DumpImageLayout(this, copyInfo.dstImageLayout, "dstImageLayout");

        // TODO: Add comment string.
    }

    ScaledCopyInfo nextCopyInfo = {};

    nextCopyInfo.pSrcImage      = NextImage(copyInfo.pSrcImage);
    nextCopyInfo.srcImageLayout = copyInfo.srcImageLayout;
    nextCopyInfo.pDstImage      = NextImage(copyInfo.pDstImage);
    nextCopyInfo.dstImageLayout = copyInfo.dstImageLayout;
    nextCopyInfo.regionCount    = copyInfo.regionCount;
    nextCopyInfo.pRegions       = copyInfo.pRegions;
    nextCopyInfo.filter         = copyInfo.filter;
    nextCopyInfo.rotation       = copyInfo.rotation;
    nextCopyInfo.pColorKey      = copyInfo.pColorKey;
    nextCopyInfo.flags          = copyInfo.flags;

    GetNextLayer()->CmdScaledCopyImage(nextCopyInfo);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdColorSpaceConversionCopy(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const ColorSpaceConversionRegion* pRegions,
    TexFilter                         filter,
    const ColorSpaceConversionTable&  cscTable)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdColorSpaceConversionCopy));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageLayout(this, srcImageLayout, "srcImageLayout");
        DumpImageInfo(this, &dstImage, "dstImage", "");
        DumpImageLayout(this, dstImageLayout, "dstImageLayout");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdColorSpaceConversionCopy(*NextImage(&srcImage),
                                                srcImageLayout,
                                                *NextImage(&dstImage),
                                                dstImageLayout,
                                                regionCount,
                                                pRegions,
                                                filter,
                                                cscTable);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCloneImageData(
    const IImage& srcImage,
    const IImage& dstImage)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCloneImageData));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageInfo(this, &dstImage, "dstImage", "");

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCloneImageData(*NextImage(&srcImage), *NextImage(&dstImage));

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
static void DumpMemoryCopyRegion(
    CmdBuffer*              pCmdBuffer,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    for (uint32 i = 0; i < regionCount; i++)
    {
        const auto& region = pRegions[i];

        Snprintf(pString, StringLength, "Region %u = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t srcOffset = 0x%016llX", region.srcOffset);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t dstOffset = 0x%016llX", region.dstOffset);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t copySize  = 0x%016llX", region.copySize);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
static void DumpMemoryImageCopyRegion(
    CmdBuffer*                   pCmdBuffer,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    for (uint32 i = 0; i < regionCount; i++)
    {
        const auto& region = pRegions[i];

        Snprintf(pString, StringLength, "Region %u = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageSubres         = ");
        SubresIdToString(region.imageSubres, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageOffset         = ");
        Offset3dToString(region.imageOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageExtent         = ");
        Extent3dToString(region.imageExtent, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t numSlices           = %u",   region.numSlices);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryOffset     = 0x%016llX", region.gpuMemoryOffset);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryRowPitch   = 0x%016llX", region.gpuMemoryRowPitch);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryDepthPitch = 0x%016llX", region.gpuMemoryDepthPitch);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToImage(
    const IGpuMemory&            srcGpuMemory,
    const IImage&                dstImage,
    ImageLayout                  dstImageLayout,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyMemoryToImage));
        DumpGpuMemoryInfo(this, &srcGpuMemory, "srcGpuMemory", "");
        DumpImageInfo(this, &dstImage, "dstImage", "");
        DumpImageLayout(this, dstImageLayout, "dstImageLayout");
        DumpMemoryImageCopyRegion(this, regionCount, pRegions);
        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyMemoryToImage(*NextGpuMemory(&srcGpuMemory),
                                      *NextImage(&dstImage),
                                      dstImageLayout,
                                      regionCount,
                                      pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToMemory(
    const IImage&                srcImage,
    ImageLayout                  srcImageLayout,
    const IGpuMemory&            dstGpuMemory,
    uint32                       regionCount,
    const MemoryImageCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyImageToMemory));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageLayout(this, srcImageLayout, "srcImageLayout");
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");
        DumpMemoryImageCopyRegion(this, regionCount, pRegions);
        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyImageToMemory(*NextImage(&srcImage),
                                      srcImageLayout,
                                      *NextGpuMemory(&dstGpuMemory),
                                      regionCount,
                                      pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemory(
    const IGpuMemory&       srcGpuMemory,
    const IGpuMemory&       dstGpuMemory,
    uint32                  regionCount,
    const MemoryCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyMemory));
        DumpGpuMemoryInfo(this, &srcGpuMemory, "srcGpuMemory", "");
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");
        DumpMemoryCopyRegion(this, regionCount, pRegions);
    }

    GetNextLayer()->CmdCopyMemory(*NextGpuMemory(&srcGpuMemory), *NextGpuMemory(&dstGpuMemory), regionCount, pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
static void DumpMemoryTiledImageCopyRegion(
    CmdBuffer*                        pCmdBuffer,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    LinearAllocatorAuto<VirtualLinearAllocator> allocator(pCmdBuffer->Allocator(), false);
    char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

    ICmdBuffer* pNextCmdBuffer = pCmdBuffer->GetNextLayer();

    for (uint32 i = 0; i < regionCount; i++)
    {
        const auto& region = pRegions[i];

        Snprintf(pString, StringLength, "Region %u = [", i);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageSubres         = ");
        SubresIdToString(region.imageSubres, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageOffset         = ");
        Offset3dToString(region.imageOffset, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t imageExtent         = ");
        Extent3dToString(region.imageExtent, pString);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "\t numSlices           = %u",   region.numSlices);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryOffset     = 0x%016llX", region.gpuMemoryOffset);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryRowPitch   = 0x%016llX", region.gpuMemoryRowPitch);
        pNextCmdBuffer->CmdCommentString(pString);
        Snprintf(pString, StringLength, "\t gpuMemoryDepthPitch = 0x%016llX", region.gpuMemoryDepthPitch);
        pNextCmdBuffer->CmdCommentString(pString);

        Snprintf(pString, StringLength, "]");
        pNextCmdBuffer->CmdCommentString(pString);
    }

    PAL_SAFE_DELETE_ARRAY(pString, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdCopyMemoryToTiledImage(
    const IGpuMemory&                 srcGpuMemory,
    const IImage&                     dstImage,
    ImageLayout                       dstImageLayout,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyMemoryToTiledImage));
        DumpGpuMemoryInfo(this, &srcGpuMemory, "srcGpuMemory", "");
        DumpImageInfo(this, &dstImage, "dstImage", "");
        DumpImageLayout(this, dstImageLayout, "dstImageLayout");
        DumpMemoryTiledImageCopyRegion(this, regionCount, pRegions);

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyMemoryToTiledImage(*NextGpuMemory(&srcGpuMemory),
                                              *NextImage(&dstImage),
                                              dstImageLayout,
                                              regionCount,
                                              pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyTiledImageToMemory(
    const IImage&                     srcImage,
    ImageLayout                       srcImageLayout,
    const IGpuMemory&                 dstGpuMemory,
    uint32                            regionCount,
    const MemoryTiledImageCopyRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyTiledImageToMemory));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageLayout(this, srcImageLayout, "srcImageLayout");
        DumpGpuMemoryInfo(this, &dstGpuMemory, "dstGpuMemory", "");
        DumpMemoryTiledImageCopyRegion(this, regionCount, pRegions);

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyTiledImageToMemory(*NextImage(&srcImage),
                                              srcImageLayout,
                                              *NextGpuMemory(&dstGpuMemory),
                                              regionCount,
                                              pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdCopyImageToPackedPixelImage(
    const IImage&          srcImage,
    const IImage&          dstImage,
    uint32                 regionCount,
    const ImageCopyRegion* pRegions,
    Pal::PackedPixelType   packPixelType)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdCopyImageToPackedPixelImage));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdCopyImageToPackedPixelImage(srcImage,
                                                   dstImage,
                                                   regionCount,
                                                   pRegions,
                                                   packPixelType);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorBuffer(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    SwizzledFormat    bufferFormat,
    uint32            bufferOffset,
    uint32            bufferExtent,
    uint32            rangeCount,
    const Range*      pRanges)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearColorBuffer));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdClearColorBuffer(*NextGpuMemory(&gpuMemory),
                                     color,
                                     bufferFormat,
                                     bufferOffset,
                                     bufferExtent,
                                     rangeCount,
                                     pRanges);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundColorTargets(
    uint32                          colorTargetCount,
    const BoundColorTarget*         pBoundColorTargets,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearBoundColorTargets));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdClearBoundColorTargets(colorTargetCount,
                                           pBoundColorTargets,
                                           regionCount,
                                           pClearRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearColorImage(
    const IImage&      image,
    ImageLayout        imageLayout,
    const ClearColor&  color,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             boxCount,
    const Box*         pBoxes,
    uint32             flags)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearColorImage));
        DumpImageInfo(this, &image, "image", "");
        DumpImageLayout(this, imageLayout, "imageLayout");
        DumpClearColor(this, color, "color");
        DumpSubresRanges(this, rangeCount, pRanges);
        DumpBoxes(this, boxCount, pBoxes);
        DumpClearColorImageFlags(this, flags);
    }

    GetNextLayer()->CmdClearColorImage(*NextImage(&image),
                                    imageLayout,
                                    color,
                                    rangeCount,
                                    pRanges,
                                    boxCount,
                                    pBoxes,
                                    flags);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearBoundDepthStencilTargets(
    float                           depth,
    uint8                           stencil,
    uint32                          samples,
    uint32                          fragments,
    DepthStencilSelectFlags         flag,
    uint32                          regionCount,
    const ClearBoundTargetRegion*   pClearRegions)
{
    if (m_annotations.logCmdBlts)
    {
        CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearBoundDepthStencilTargets));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdClearBoundDepthStencilTargets(
        depth, stencil, samples, fragments, flag, regionCount, pClearRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearDepthStencil(
    const IImage&      image,
    ImageLayout        depthLayout,
    ImageLayout        stencilLayout,
    float              depth,
    uint8              stencil,
    uint32             rangeCount,
    const SubresRange* pRanges,
    uint32             rectCount,
    const Rect*        pRects,
    uint32             flags)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearDepthStencil));
        DumpImageInfo(this, &image, "image", "");
        DumpImageLayout(this, depthLayout, "depthLayout");
        DumpImageLayout(this, stencilLayout, "stencilLayout");
        DumpFloat(this, "depth", depth);
        DumpUint(this, "stencil", stencil);
        DumpSubresRanges(this, rangeCount, pRanges);
        DumpRects(this, rectCount, pRects);
        DumpClearDepthStencilImageFlags(this, flags);
    }

    GetNextLayer()->CmdClearDepthStencil(*NextImage(&image),
                                         depthLayout,
                                         stencilLayout,
                                         depth,
                                         stencil,
                                         rangeCount,
                                         pRanges,
                                         rectCount,
                                         pRects,
                                         flags);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearBufferView(
    const IGpuMemory& gpuMemory,
    const ClearColor& color,
    const void*       pBufferViewSrd,
    uint32            rangeCount,
    const Range*      pRanges)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearBufferView));
        DumpGpuMemoryInfo(this, &gpuMemory, "gpuMemory", "");
        DumpClearColor(this, color, "color");
        DumpBufferViewSrd(this, pBufferViewSrd, "pBufferViewSrd");
        DumpRanges(this, rangeCount, pRanges);
    }

    GetNextLayer()->CmdClearBufferView(*NextGpuMemory(&gpuMemory),
                                    color,
                                    pBufferViewSrd,
                                    rangeCount,
                                    pRanges);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdClearImageView(
    const IImage&     image,
    ImageLayout       imageLayout,
    const ClearColor& color,
    const void*       pImageViewSrd,
    uint32            rectCount,
    const Rect*       pRects)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdClearImageView));
        DumpImageInfo(this, &image, "image", "");
        DumpImageLayout(this, imageLayout, "imageLayout");
        DumpClearColor(this, color, "color");
        DumpImageViewSrd(this, pImageViewSrd, "pImageViewSrd");
        DumpRects(this, rectCount, pRects);
    }

    GetNextLayer()->CmdClearImageView(*NextImage(&image),
                                   imageLayout,
                                   color,
                                   pImageViewSrd,
                                   rectCount,
                                   pRects);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdResolveImage(
    const IImage&             srcImage,
    ImageLayout               srcImageLayout,
    const IImage&             dstImage,
    ImageLayout               dstImageLayout,
    ResolveMode               resolveMode,
    uint32                    regionCount,
    const ImageResolveRegion* pRegions)
{
    if (m_annotations.logCmdBlts)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdResolveImage));
        DumpImageInfo(this, &srcImage, "srcImage", "");
        DumpImageLayout(this, srcImageLayout, "srcImageLayout");
        DumpImageInfo(this, &dstImage, "dstImage", "");
        DumpImageLayout(this, dstImageLayout, "dstImageLayout");
        DumpResolveMode(this, resolveMode);
        DumpImageResolveRegion(this, regionCount, pRegions);
    }

    GetNextLayer()->CmdResolveImage(*NextImage(&srcImage),
                                    srcImageLayout,
                                    *NextImage(&dstImage),
                                    dstImageLayout,
                                    resolveMode,
                                    regionCount,
                                    pRegions);

    if (m_singleStep.waitIdleBlts)
    {
        AddSingleStepBarrier();
    }

    if (m_singleStep.timestampBlts)
    {
        AddTimestamp();
    }
}

// =====================================================================================================================
void CmdBuffer::CmdSetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      setPoint)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetEvent));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetEvent(*NextGpuEvent(&gpuEvent), setPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdResetEvent(
    const IGpuEvent& gpuEvent,
    HwPipePoint      resetPoint)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdResetEvent));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdResetEvent(*NextGpuEvent(&gpuEvent), resetPoint);
}

// =====================================================================================================================
void CmdBuffer::CmdPredicateEvent(
    const IGpuEvent& gpuEvent)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdPredicateEvent));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdPredicateEvent(*NextGpuEvent(&gpuEvent));
}

// =====================================================================================================================
void CmdBuffer::CmdMemoryAtomic(
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    uint64            srcData,
    AtomicOp          atomicOp)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdMemoryAtomic));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdMemoryAtomic(*NextGpuMemory(&dstGpuMemory), dstOffset, srcData, atomicOp);
}

// =====================================================================================================================
void CmdBuffer::CmdResetQueryPool(
    const IQueryPool& queryPool,
    uint32            startQuery,
    uint32            queryCount)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdResetQueryPool));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdResetQueryPool(*NextQueryPool(&queryPool), startQuery, queryCount);
}

// =====================================================================================================================
void CmdBuffer::CmdBeginQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot,
    QueryControlFlags flags)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBeginQuery));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBeginQuery(*NextQueryPool(&queryPool), queryType, slot, flags);
}

// =====================================================================================================================
void CmdBuffer::CmdEndQuery(
    const IQueryPool& queryPool,
    QueryType         queryType,
    uint32            slot)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdEndQuery));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdEndQuery(*NextQueryPool(&queryPool), queryType, slot);
}

// =====================================================================================================================
void CmdBuffer::CmdResolveQuery(
    const IQueryPool& queryPool,
    QueryResultFlags  flags,
    QueryType         queryType,
    uint32            startQuery,
    uint32            queryCount,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset,
    gpusize           dstStride)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdResolveQuery));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdResolveQuery(*NextQueryPool(&queryPool),
                                 flags,
                                 queryType,
                                 startQuery,
                                 queryCount,
                                 *NextGpuMemory(&dstGpuMemory),
                                 dstOffset,
                                 dstStride);
}

// =====================================================================================================================
void CmdBuffer::CmdSetPredication(
    IQueryPool*       pQueryPool,
    uint32            slot,
    const IGpuMemory* pGpuMemory,
    gpusize           offset,
    PredicateType     predType,
    bool              predPolarity,
    bool              waitResults,
    bool              accumulateData)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetPredication));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetPredication(NextQueryPool(pQueryPool),
                                   slot,
                                   pGpuMemory,
                                   offset,
                                   predType,
                                   predPolarity,
                                   waitResults,
                                   accumulateData);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteTimestamp(
    HwPipePoint       pipePoint,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstOffset)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWriteTimestamp));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWriteTimestamp(pipePoint, *NextGpuMemory(&dstGpuMemory), dstOffset);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteImmediate(
    HwPipePoint        pipePoint,
    uint64             data,
    ImmediateDataWidth dataSize,
    gpusize            address)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWriteImmediate));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWriteImmediate(pipePoint, data, dataSize, address);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadGds(
    HwPipePoint       pipePoint,
    uint32            dstGdsOffset,
    const IGpuMemory& srcGpuMemory,
    gpusize           srcMemOffset,
    uint32            size)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdLoadGds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdLoadGds(pipePoint,
                               dstGdsOffset,
                               *NextGpuMemory(&srcGpuMemory),
                               srcMemOffset,
                               size);
}

// =====================================================================================================================
void CmdBuffer::CmdStoreGds(
    HwPipePoint       pipePoint,
    uint32            srcGdsOffset,
    const IGpuMemory& dstGpuMemory,
    gpusize           dstMemOffset,
    uint32            size,
    bool              waitForWC)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdStoreGds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdStoreGds(pipePoint,
                                srcGdsOffset,
                                *NextGpuMemory(&dstGpuMemory),
                                dstMemOffset,
                                size,
                                waitForWC);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            dataSize,
    const uint32*     pData)
{
    PAL_ASSERT(pData != nullptr);

    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdUpdateGds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdUpdateGds(pipePoint,
                                 gdsOffset,
                                 dataSize,
                                 pData);
}

// =====================================================================================================================
void CmdBuffer::CmdFillGds(
    HwPipePoint       pipePoint,
    uint32            gdsOffset,
    uint32            fillSize,
    uint32            data)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdFillGds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdFillGds(pipePoint,
                               gdsOffset,
                               fillSize,
                               data);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdLoadBufferFilledSizes));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdLoadBufferFilledSizes(gpuVirtAddr);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveBufferFilledSizes(
    const gpusize (&gpuVirtAddr)[MaxStreamOutTargets])
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSaveBufferFilledSizes));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSaveBufferFilledSizes(gpuVirtAddr);
}

// =====================================================================================================================
void CmdBuffer::CmdSetBufferFilledSize(
    uint32  bufferId,
    uint32  offset)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetBufferFilledSize));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetBufferFilledSize(bufferId, offset);
}

// =====================================================================================================================
void CmdBuffer::CmdLoadCeRam(
    const IGpuMemory& srcGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdLoadCeRam));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdLoadCeRam(*NextGpuMemory(&srcGpuMemory), memOffset, ramOffset, dwordSize);
}

// =====================================================================================================================
void CmdBuffer::CmdWriteCeRam(
    const void* pSrcData,
    uint32      ramOffset,
    uint32      dwordSize)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWriteCeRam));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWriteCeRam(pSrcData, ramOffset, dwordSize);
}

// =====================================================================================================================
void CmdBuffer::CmdDumpCeRam(
    const IGpuMemory& dstGpuMemory,
    gpusize           memOffset,
    uint32            ramOffset,
    uint32            dwordSize,
    uint32            currRingPos,
    uint32            ringSize)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdDumpCeRam));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdDumpCeRam(*NextGpuMemory(&dstGpuMemory),
                              memOffset,
                              ramOffset,
                              dwordSize,
                              currRingPos,
                              ringSize);
}

// =====================================================================================================================
uint32 CmdBuffer::GetEmbeddedDataLimit() const
{
    return GetNextLayer()->GetEmbeddedDataLimit();
}

// =====================================================================================================================
uint32* CmdBuffer::CmdAllocateEmbeddedData(
    uint32   sizeInDwords,
    uint32   alignmentInDwords,
    gpusize* pGpuAddress)
{
    return GetNextLayer()->CmdAllocateEmbeddedData(sizeInDwords, alignmentInDwords, pGpuAddress);
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteNestedCmdBuffers(
    uint32            cmdBufferCount,
    ICmdBuffer*const* ppCmdBuffers)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdExecuteNestedCmdBuffers));

        // TODO: Add comment string.
    }

    Util::LinearAllocatorAuto<Util::VirtualLinearAllocator> allocator(&m_allocator, false);

    ICmdBuffer** ppNextCmdBuffers = PAL_NEW_ARRAY(ICmdBuffer*, cmdBufferCount, &allocator, AllocInternalTemp);

    for (uint32 i = 0; i < cmdBufferCount; i++)
    {
        ppNextCmdBuffers[i] = static_cast<CmdBuffer*>(ppCmdBuffers[i])->GetNextLayer();
    }
    GetNextLayer()->CmdExecuteNestedCmdBuffers(cmdBufferCount, ppNextCmdBuffers);

    PAL_SAFE_DELETE_ARRAY(ppNextCmdBuffers, &allocator);
}

// =====================================================================================================================
void CmdBuffer::CmdExecuteIndirectCmds(
    const IIndirectCmdGenerator& generator,
    const IGpuMemory&            gpuMemory,
    gpusize                      offset,
    uint32                       maximumCount,
    gpusize                      countGpuAddr)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdExecuteIndirectCmds));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdExecuteIndirectCmds(*NextIndirectCmdGenerator(&generator),
                                        *NextGpuMemory(&gpuMemory),
                                        offset,
                                        maximumCount,
                                        countGpuAddr);
}

// =====================================================================================================================
void CmdBuffer::CmdIf(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdIf));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdIf(*NextGpuMemory(&gpuMemory),
                       offset,
                       data,
                       mask,
                       compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdElse()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdElse));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdElse();
}

// =====================================================================================================================
void CmdBuffer::CmdEndIf()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdEndIf));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdEndIf();
}

// =====================================================================================================================
void CmdBuffer::CmdWhile(
    const IGpuMemory& gpuMemory,
    gpusize           offset,
    uint64            data,
    uint64            mask,
    CompareFunc       compareFunc)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdWhile));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdWhile(*NextGpuMemory(&gpuMemory),
                          offset,
                          data,
                          mask,
                          compareFunc);
}

// =====================================================================================================================
void CmdBuffer::CmdEndWhile()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdEndWhile));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdEndWhile();
}

// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState0(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetHiSCompareState0));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetHiSCompareState0(compFunc, compMask, compValue, enable);
}

// =====================================================================================================================
void CmdBuffer::CmdSetHiSCompareState1(
    CompareFunc compFunc,
    uint32      compMask,
    uint32      compValue,
    bool        enable)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetHiSCompareState1));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetHiSCompareState1(compFunc, compMask, compValue, enable);
}

// =====================================================================================================================
void CmdBuffer::CmdFlglSync()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdFlglSync));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdFlglSync();
}

// =====================================================================================================================
void CmdBuffer::CmdFlglEnable()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdFlglEnable));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdFlglEnable();
}

// =====================================================================================================================
void CmdBuffer::CmdFlglDisable()
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdFlglDisable));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdFlglDisable();
}

// =====================================================================================================================
void CmdBuffer::CmdBeginPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdBeginPerfExperiment));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdBeginPerfExperiment(NextPerfExperiment(pPerfExperiment));
}

// =====================================================================================================================
void CmdBuffer::CmdUpdatePerfExperimentSqttTokenMask(
    IPerfExperiment*              pPerfExperiment,
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdUpdatePerfExperimentSqttTokenMask));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdUpdatePerfExperimentSqttTokenMask(NextPerfExperiment(pPerfExperiment), sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::CmdUpdateSqttTokenMask(
    const ThreadTraceTokenConfig& sqttTokenConfig)
{
    ICmdBuffer* pNext = GetNextLayer();
    if (m_annotations.logMiscellaneous)
    {
        pNext->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetUserData));

        LinearAllocatorAuto<VirtualLinearAllocator> allocator(Allocator(), false);
        char* pString = PAL_NEW_ARRAY(char, StringLength, &allocator, AllocInternalTemp);

        pNext->CmdCommentString("SqttTokenConfig:");
        Snprintf(pString, StringLength, "TokenMask   = %04x", sqttTokenConfig.tokenMask);
        pNext->CmdCommentString(pString);
        Snprintf(pString, StringLength, "RegMask     = %04x", sqttTokenConfig.regMask);
        pNext->CmdCommentString(pString);
    }

    pNext->CmdUpdateSqttTokenMask(sqttTokenConfig);
}

// =====================================================================================================================
void CmdBuffer::CmdEndPerfExperiment(
    IPerfExperiment* pPerfExperiment)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdEndPerfExperiment));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdEndPerfExperiment(NextPerfExperiment(pPerfExperiment));
}

// =====================================================================================================================
void CmdBuffer::CmdInsertTraceMarker(
    PerfTraceMarkerType markerType,
    uint32              markerData)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdInsertTraceMarker));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdInsertTraceMarker(markerType, markerData);
}

// =====================================================================================================================
void CmdBuffer::CmdInsertRgpTraceMarker(
    uint32      numDwords,
    const void* pData)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdInsertRgpTraceMarker));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdInsertRgpTraceMarker(numDwords, pData);
}

// =====================================================================================================================
void CmdBuffer::CmdSaveComputeState(
    uint32 stateFlags)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSaveComputeState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSaveComputeState(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::CmdRestoreComputeState(
    uint32 stateFlags)
{
    if (m_annotations.logMiscellaneous)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdRestoreComputeState));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdRestoreComputeState(stateFlags);
}

// =====================================================================================================================
void CmdBuffer::CmdCommentString(
    const char* pComment)
{
    GetNextLayer()->CmdCommentString(pComment);
}

// =====================================================================================================================
void CmdBuffer::CmdSetUserClipPlanes(
    uint32               firstPlane,
    uint32               planeCount,
    const UserClipPlane* pPlanes)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetUserClipPlanes));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetUserClipPlanes(firstPlane, planeCount, pPlanes);
}

// =====================================================================================================================
void CmdBuffer::CmdXdmaWaitFlipPending()
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdXdmaWaitFlipPending));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdXdmaWaitFlipPending();
}

// =====================================================================================================================
void CmdBuffer::CmdSetViewInstanceMask(
    uint32 mask)
{
    if (m_annotations.logCmdSets)
    {
        GetNextLayer()->CmdCommentString(GetCmdBufCallIdString(CmdBufCallId::CmdSetViewInstanceMask));

        // TODO: Add comment string.
    }

    GetNextLayer()->CmdSetViewInstanceMask(mask);
}

} // CmdBufferLogger
} // Pal
