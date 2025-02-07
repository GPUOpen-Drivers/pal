/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "pal.h"
#include "palImage.h"
#include "util/imported/dds/dds.h"

namespace Util
{

#pragma pack(push,1)

// The entire direct draw surface header. The "headerExt" portion may not be required depending on the format
struct DdsHeaderFull
{
    uint32                      ddsMagic;
    DirectX::DDS_HEADER         headerBase;
    DirectX::DDS_HEADER_DXT10   headerExt;
};
#pragma pack(pop)

// Get the Dds pixel format for the input SwizzledFormat. A dxgi format may also be necessary to represent the surface
Result GetDdsPixelFormat(
    DirectX::DDS_PIXELFORMAT*   pDdspf,
    uint32_t*                   pDxgiFormat,
    Pal::SwizzledFormat         palFormat);

// Get the dds header and header size for a surface
Result GetDdsHeader(
    DdsHeaderFull*       pHeader,
    size_t*              pActualHeaderSize,
    Pal::ImageType       imageType,
    Pal::SwizzledFormat  palFormat,
    uint32               arraySize,
    Pal::SubresLayout*   pSubresLayout);

} // Util

#endif
